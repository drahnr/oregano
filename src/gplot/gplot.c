/*
 * gplot.c
 *
 * Authors:
 *  Ricardo Markiewicz <rmarkie@fi.uba.ar>
 *
 * Copyright (C) 1999-2001  Richard Hult
 * Copyright (C) 2003,2004  LUGFI
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <math.h>
#include "gplot.h"

#define BORDER_SIZE 35

static void g_plot_class_init (GPlotClass* class);
static void g_plot_init (GPlot* plot);
static gint g_plot_expose (GtkWidget* widget, GdkEventExpose* event);
static cairo_t* g_plot_create_cairo (GPlot *);
static void _calculate_step (gdouble min, gdouble max, gdouble *step);
static gboolean g_plot_motion_cb (GtkWidget *, GdkEventMotion *, GPlot *);
static gboolean g_plot_button_press_cb (GtkWidget *, GdkEventButton *, GPlot *);
static gboolean g_plot_button_release_cb (GtkWidget *, GdkEventButton *, GPlot *);
static void g_plot_update_bbox (GPlot *);
static void g_plot_finalize (GObject *object);
static void g_plot_dispose (GObject *object);

enum {
	ACTION_NONE,
	ACTION_STARTING_PAN,
	ACTION_PAN,
	ACTION_STARTING_REGION,
	ACTION_REGION
};

static GtkLayoutClass* parent_class = NULL;
	
struct _GPlotPriv {
	GList *functions;

	gchar *xlabel;
	gchar *ylabel;

	gdouble left_border;
	gdouble right_border;
	gdouble top_border;
	gdouble bottom_border;

	guint zoom_mode;
	guint action;
	gdouble zoom;
	gdouble offset_x;
	gdouble offset_y;
	gdouble last_x;
	gdouble last_y;

	/* Window->Viewport * Transformation Matrix */
	cairo_matrix_t matrix;

	gboolean window_valid;
	GPlotFunctionBBox window_bbox;
	GPlotFunctionBBox viewport_bbox;

	GPlotFunctionBBox rubberband;
};

GType
g_plot_get_type ()
{
	static GType g_plot_type = 0;
    
	if (!g_plot_type) {
		static const GTypeInfo g_plot_info = {
			sizeof (GPlotClass),
			NULL,
			NULL,
			(GClassInitFunc) g_plot_class_init,
			NULL,
			NULL,
			sizeof (GPlot),
			0,
			(GInstanceInitFunc) g_plot_init,
			NULL
		};
		g_plot_type = g_type_register_static (GTK_TYPE_LAYOUT, "GPlot", &g_plot_info, 0);
	}
	return g_plot_type;
}

static void
g_plot_class_init (GPlotClass* class)
{
	GObjectClass* object_class;
	GtkWidgetClass* widget_class;

	object_class = G_OBJECT_CLASS (class);
	widget_class = GTK_WIDGET_CLASS (class);
	parent_class = g_type_class_peek_parent (class);

	widget_class->expose_event = g_plot_expose;

	object_class->dispose = g_plot_dispose;
	object_class->finalize = g_plot_finalize;
}

static cairo_t* 
g_plot_create_cairo (GPlot *p)
{
	cairo_t *cr;

	cr = gdk_cairo_create (GTK_LAYOUT (p)->bin_window);

	return cr;
}

static void
g_plot_finalize (GObject *object)
{
	GPlot *p = GPLOT (object);

	if (p->priv->xlabel)
		g_free (p->priv->xlabel);

	if (p->priv->ylabel)
		g_free (p->priv->ylabel);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
g_plot_dispose (GObject *object)
{
	GList *lst;
	GPlot *plot;

	plot = GPLOT (object);

	lst = plot->priv->functions;
	while (lst) {
		GPlotFunction *f = (GPlotFunction *)lst->data;
		g_object_unref (G_OBJECT (f));
		lst = lst->next;
	}
	g_list_free (plot->priv->functions);
	plot->priv->functions = NULL;

	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static gint 
g_plot_expose (GtkWidget* widget, GdkEventExpose* event)
{
	static double dashes[] = {3,  /* ink */
		3,  /* skip */
		3,  /* ink */
		3   /* skip*/ };
	static int ndash  = sizeof (dashes)/sizeof(dashes[0]);
	static double offset = -0.2;

	GPlot *plot;
	GPlotPriv *priv;
	cairo_t* cr;
	guint width;
	guint height;
	guint graph_width;
	guint graph_height;
	GPlotFunction *f;
	GList *lst;
	GPlotFunctionBBox bbox;
	gdouble aX, bX, aY, bY;
	gdouble v, x, y, step;
	int i = 0;
	cairo_text_extents_t extents;
	gchar *label;

	plot = GPLOT (widget);
	priv = plot->priv;

	if (!priv->window_valid) {
		g_plot_update_bbox (plot);
	}

	width = widget->allocation.width;
	height = widget->allocation.height;

	graph_width = width - priv->left_border - priv->right_border;
	graph_height = height - priv->top_border - priv->bottom_border;

	cr = g_plot_create_cairo (plot);

	/* Set a clip region for the expose event */
	/* TODO :This is useful if we use gtk_widget_queue_draw_area */
	cairo_rectangle (cr, event->area.x, event->area.y, event->area.width, event->area.height);
	cairo_clip (cr);

	/* Paint background */
	cairo_save (cr);
		cairo_set_source_rgb (cr, 0.0, 0.0, 0.0);
		cairo_rectangle (cr, 0, 0, width, height);
		cairo_fill (cr);
	cairo_restore (cr);

	/* Plot Border */
	cairo_save (cr);
		cairo_set_line_width (cr, 0.5);
		cairo_set_source_rgb (cr, 1.0, 1.0, 1.0);
		cairo_rectangle (cr, priv->left_border, priv->right_border, graph_width, graph_height);
		cairo_stroke (cr);
	cairo_restore (cr);

	/* Axis Labels */
	if (priv->xlabel) {
		cairo_save (cr);
			cairo_set_source_rgb (cr, 1.0, 1.0, 1.0);
			cairo_set_font_size (cr, 14);
			cairo_text_extents (cr, priv->xlabel, &extents);
			cairo_move_to (cr, width/2.0 - extents.width/2.0, height-extents.height/2.0);
			cairo_show_text (cr, priv->xlabel);
			cairo_stroke (cr);
		cairo_restore (cr);
	}

	if (priv->ylabel) {
		cairo_save (cr);
			cairo_set_source_rgb (cr, 1.0, 1.0, 1.0);
			cairo_set_font_size (cr, 14);
			cairo_text_extents (cr, priv->ylabel, &extents);
			cairo_move_to (cr, extents.height, height/2.0 + extents.width/2.0);
			cairo_rotate (cr, 4.7124);
			cairo_show_text (cr, priv->ylabel);
			cairo_stroke (cr);
		cairo_restore (cr);
	}

	/* TODO : Move this to SizeAllocation functions */
	priv->viewport_bbox.xmax = widget->allocation.width - priv->right_border;
	priv->viewport_bbox.xmin = priv->left_border;
	priv->viewport_bbox.ymax = widget->allocation.height - priv->bottom_border;
	priv->viewport_bbox.ymin = priv->top_border;

	/* Save real bbox */
	bbox = priv->window_bbox;

	/* Calculating Window to Viewport matrix */
	aX = (priv->viewport_bbox.xmax - priv->viewport_bbox.xmin)
		/ (priv->window_bbox.xmax - priv->window_bbox.xmin);
	bX = -aX * priv->window_bbox.xmin + priv->viewport_bbox.xmin;
	aY = (priv->viewport_bbox.ymax - priv->viewport_bbox.ymin)
		/ (priv->window_bbox.ymin - priv->window_bbox.ymax);
	bY = -aY * priv->window_bbox.ymax + priv->viewport_bbox.ymin;

	cairo_matrix_init (&priv->matrix, aX, 0, 0, aY, bX, bY);

	cairo_save (cr);
		cairo_rectangle (cr, priv->left_border, priv->right_border, graph_width, graph_height);
		cairo_clip (cr);

		cairo_set_source_rgb (cr, 1.0, 1.0, 1.0);
		cairo_set_matrix (cr, &priv->matrix);
		lst = plot->priv->functions;
		while (lst) {
			f = (GPlotFunction *)lst->data;
			g_plot_function_draw (f, cr, &priv->window_bbox);
			lst = lst->next;
		}
	cairo_restore (cr);

	cairo_save (cr);
		cairo_rectangle (cr, priv->left_border, priv->right_border, graph_width, graph_height);
		cairo_clip (cr);
		cairo_save (cr);
			cairo_set_matrix (cr, &priv->matrix);
			cairo_move_to (cr, priv->window_bbox.xmin, 0.0);
			cairo_line_to (cr, priv->window_bbox.xmax, 0.0);
		cairo_restore (cr);
		cairo_save (cr);
			cairo_set_source_rgb (cr, 1.0, 0.0, 0.0);
			cairo_set_line_width (cr, 2);
			cairo_stroke (cr);
		cairo_restore (cr);

		cairo_save (cr);
			cairo_set_matrix (cr, &priv->matrix);
			cairo_move_to (cr, 0.0, priv->window_bbox.ymin);
			cairo_line_to (cr, 0.0, priv->window_bbox.ymax);
		cairo_restore (cr);
		cairo_save (cr);
			cairo_set_source_rgb (cr, 1.0, 0.0, 0.0);
			cairo_set_line_width (cr, 2);
			cairo_stroke (cr);
		cairo_restore (cr);
	cairo_restore (cr);

	cairo_save (cr);
		i = 0;
		cairo_set_source_rgb (cr, 1.0, 1.0, 1.0);
		cairo_set_line_width (cr, 0.3);
		_calculate_step (priv->window_bbox.xmin, priv->window_bbox.xmax, &step);
		for (v=priv->window_bbox.xmin; v <= priv->window_bbox.xmax; v += step) {
			x = v;
			y = priv->window_bbox.ymin;
			cairo_matrix_transform_point (&priv->matrix, &x, &y);
			cairo_move_to (cr, x, y-5);
			cairo_line_to (cr, x, y);
			cairo_stroke (cr);
		
			cairo_save (cr);
				label = g_strdup_printf ("%.2f", v);
				cairo_text_extents (cr, label, &extents);
				cairo_move_to (cr, x, y+extents.height);
				cairo_rotate (cr, 0.404);
				cairo_show_text (cr, label);
				cairo_stroke (cr);
				g_free (label);
			cairo_restore (cr);

			x = v;
			y = priv->window_bbox.ymax;
			cairo_matrix_transform_point (&priv->matrix, &x, &y);
			cairo_move_to (cr, x, y+5);
			cairo_line_to (cr, x, y);
			cairo_stroke (cr);
			i++;
		}
		_calculate_step (priv->window_bbox.ymin, priv->window_bbox.ymax, &step);
		for (v=priv->window_bbox.ymin; v <= priv->window_bbox.ymax; v += step) {
			x = priv->window_bbox.xmin;
			y = v;
			cairo_matrix_transform_point (&priv->matrix, &x, &y);
			cairo_move_to (cr, x+5, y);
			cairo_line_to (cr, x, y);
			cairo_stroke (cr);
			
			cairo_save (cr);
				label = g_strdup_printf ("%.2f", v);
				cairo_text_extents (cr, label, &extents);
				cairo_move_to (cr, x-extents.width-5, y);
				cairo_show_text (cr, label);
				cairo_stroke (cr);
				g_free (label);
			cairo_restore (cr);

			x = priv->window_bbox.xmax;
			y = v;
			cairo_matrix_transform_point (&priv->matrix, &x, &y);
			cairo_move_to (cr, x, y);
			cairo_line_to (cr, x-5, y);
			cairo_stroke (cr);
			i++;
		}
	cairo_restore (cr);

	if (priv->action == ACTION_REGION) {
		gdouble x, y, w, h;
		x = priv->rubberband.xmin;
		y = priv->rubberband.ymin;
		w = priv->rubberband.xmax;
		h = priv->rubberband.ymax;
		cairo_save (cr);
			cairo_identity_matrix (cr);
			cairo_set_dash (cr, dashes, ndash, offset);

			cairo_set_line_width (cr, 2);
			cairo_set_source_rgb (cr, 0.0, 1.0, 0.0);
			cairo_rectangle (cr, x, y, w-x, h-y);
			cairo_stroke (cr);
		cairo_restore (cr);
	}

	cairo_destroy (cr);

	return FALSE;
}

static void
g_plot_init (GPlot* plot)
{
	plot->priv = g_new0 (GPlotPriv, 1);

	plot->priv->zoom_mode = GPLOT_ZOOM_REGION;
	plot->priv->functions = NULL; 
	plot->priv->action = ACTION_NONE;
	plot->priv->zoom = 1.0;
	plot->priv->offset_x = 0.0;
	plot->priv->offset_y = 0.0;
	plot->priv->left_border = BORDER_SIZE;
	plot->priv->right_border = BORDER_SIZE;
	plot->priv->top_border = BORDER_SIZE;
	plot->priv->bottom_border = BORDER_SIZE;
	plot->priv->xlabel = NULL;
	plot->priv->ylabel = NULL;
}

GtkWidget*
g_plot_new ()
{
	GPlot *plot;

	plot = GPLOT (g_object_new (TYPE_GPLOT, NULL));

	gtk_widget_add_events (GTK_WIDGET (plot),
		GDK_POINTER_MOTION_MASK|GDK_BUTTON_PRESS_MASK|
		GDK_BUTTON_RELEASE_MASK|GDK_BUTTON_MOTION_MASK|
		GDK_BUTTON1_MOTION_MASK|GDK_BUTTON2_MOTION_MASK|
		GDK_BUTTON3_MOTION_MASK);

	g_signal_connect (G_OBJECT (plot), "motion-notify-event", G_CALLBACK(g_plot_motion_cb), plot);
	g_signal_connect (G_OBJECT (plot), "button-press-event", G_CALLBACK(g_plot_button_press_cb), plot);
	g_signal_connect (G_OBJECT (plot), "button-release-event", G_CALLBACK(g_plot_button_release_cb), plot);

	return GTK_WIDGET (plot);
}

int
g_plot_add_function (GPlot *plot, GPlotFunction *func)
{
	g_return_val_if_fail (IS_GPLOT (plot), -1);

	plot->priv->functions = g_list_append (plot->priv->functions, func);
	plot->priv->window_valid = FALSE;
	return 0;
}

static void
_calculate_step (gdouble min, gdouble max, gdouble *step)
{
	max = ceil (max);

	(*step) = (max - min) / 10;

	if ((*step) < 1e-2)
		(*step) = 1e-2;
}

static gboolean
g_plot_motion_cb (GtkWidget *w, GdkEventMotion *e, GPlot *p)
{
	switch (p->priv->zoom_mode) {
		case GPLOT_ZOOM_INOUT:
			if ((p->priv->action == ACTION_STARTING_PAN) || (p->priv->action == ACTION_PAN)) {
				gdouble dx, dy;
				cairo_matrix_t t = p->priv->matrix;
				GdkCursor *cursor = gdk_cursor_new (GDK_FLEUR);
				gdk_window_set_cursor (w->window, cursor);
				gdk_cursor_destroy (cursor);
				gdk_flush ();

				dx = p->priv->last_x - e->x;
				dy = p->priv->last_y - e->y;

				cairo_matrix_invert (&t);
				cairo_matrix_transform_distance (&t, &dx, &dy);

				p->priv->window_bbox.xmin -= dx;
				p->priv->window_bbox.xmax -= dx;
				p->priv->window_bbox.ymin -= dy;
				p->priv->window_bbox.ymax -= dy;

				p->priv->last_x = e->x;
				p->priv->last_y = e->y;
				p->priv->action = ACTION_PAN;
				gtk_widget_queue_draw (w);
			}
		break;
		case GPLOT_ZOOM_REGION:
			if ((p->priv->action == ACTION_STARTING_REGION) || (p->priv->action == ACTION_REGION)) {
				gdouble dx, dy;
				GdkCursor *cursor = gdk_cursor_new (GDK_CROSS);
				gdk_window_set_cursor (w->window, cursor);
				gdk_cursor_destroy (cursor);
				gdk_flush ();

				/* dx < 0 == moving to the left */
				dx = e->x - p->priv->last_x;
				dy = e->y - p->priv->last_y;

				if (dx < 0) {
					p->priv->rubberband.xmin = e->x;
				} else {
					p->priv->rubberband.xmax = e->x;
				}
				if (dy < 0) {
					p->priv->rubberband.ymin = e->y;
				} else {
					p->priv->rubberband.ymax = e->y;
				}

				p->priv->action = ACTION_REGION;
				gtk_widget_queue_draw (w);
			}
	}

	return FALSE;
}

static gboolean
g_plot_button_press_cb (GtkWidget *w, GdkEventButton *e, GPlot *p)
{
	g_return_val_if_fail (IS_GPLOT (p), TRUE);

	if (e->type == GDK_2BUTTON_PRESS) {
		/* TODO : Chekck function below cursor and open a property dialog :) */
	} else {
		switch (p->priv->zoom_mode) {
			case GPLOT_ZOOM_INOUT:
				if (e->button == 1) {
					p->priv->action = ACTION_STARTING_PAN;
					p->priv->last_x = e->x;
					p->priv->last_y = e->y;
				}
			break;
			case GPLOT_ZOOM_REGION:
				if ((e->button == 1)) {
					p->priv->action = ACTION_STARTING_REGION;
					p->priv->rubberband.xmin = e->x;
					p->priv->rubberband.ymin = e->y;
					p->priv->rubberband.xmax = e->x;
					p->priv->rubberband.ymax = e->y;
					p->priv->last_x = e->x;
					p->priv->last_y = e->y;
				}
		}
	}

	return TRUE;
}

static gboolean
g_plot_button_release_cb (GtkWidget *w, GdkEventButton *e, GPlot *p)
{
	gdouble zoom;

	g_return_val_if_fail (IS_GPLOT (p), TRUE);

	switch (p->priv->zoom_mode) {
		case GPLOT_ZOOM_INOUT:
			if (p->priv->action != ACTION_PAN) {
				switch (e->button) {
					case 1:
						p->priv->zoom += 0.1;
						zoom = 1.1;
					break;
					case 3:
						p->priv->zoom -= 0.1;
						zoom = 0.9;
				}
				p->priv->window_bbox.xmin /= zoom;
				p->priv->window_bbox.xmax /= zoom;
				p->priv->window_bbox.ymin /= zoom;
				p->priv->window_bbox.ymax /= zoom;
				gtk_widget_queue_draw (w);
			} else {
				gdk_window_set_cursor (w->window, NULL);
				gdk_flush ();
			}
		break;
		case GPLOT_ZOOM_REGION:
			if ((e->button == 1) && (p->priv->action == ACTION_REGION)) {
				gdk_window_set_cursor (w->window, NULL);
				gdk_flush ();
				{
					gdouble x1, y1, x2, y2;
					cairo_matrix_t t = p->priv->matrix;
					cairo_matrix_invert (&t);

					x1 = p->priv->rubberband.xmin;
					y1 = p->priv->rubberband.ymin;
					x2 = p->priv->rubberband.xmax;
					y2 = p->priv->rubberband.ymax;

					cairo_matrix_transform_point (&t, &x1, &y1);
					cairo_matrix_transform_point (&t, &x2, &y2);

					p->priv->window_bbox.xmin = x1;
					p->priv->window_bbox.xmax = x2;
					p->priv->window_bbox.ymin = y2;
					p->priv->window_bbox.ymax = y1;
				}
			} else if (e->button == 3) {
				gdk_window_set_cursor (w->window, NULL);
				gdk_flush ();
			}
			gtk_widget_queue_draw (w);
	}

	p->priv->action = ACTION_NONE;
	return TRUE;
}

void
g_plot_set_zoom_mode (GPlot *p, guint mode)
{
	g_return_if_fail (IS_GPLOT (p));

	p->priv->zoom_mode = mode;
}

guint
g_plot_get_zoom_mode (GPlot *p)
{
	g_return_val_if_fail (IS_GPLOT (p), 0);

	return p->priv->zoom_mode;
}

static void
g_plot_update_bbox (GPlot *p)
{
	GPlotFunction *f;
	GPlotFunctionBBox bbox;
	GPlotPriv *priv;
	GList *lst;

	priv = p->priv;

	/* Get functions bbox */
	priv->window_bbox.xmax = -9999;
	priv->window_bbox.xmin = 9999;
	priv->window_bbox.ymax = -9999;
	priv->window_bbox.ymin = 9999;
	lst = priv->functions;
	while (lst) {
		f = (GPlotFunction *)lst->data;
		g_plot_function_get_bbox (f, &bbox);
		priv->window_bbox.xmax = MAX (priv->window_bbox.xmax, bbox.xmax);
		priv->window_bbox.xmin = MIN (priv->window_bbox.xmin, bbox.xmin);
		priv->window_bbox.ymax = MAX (priv->window_bbox.ymax, bbox.ymax);
		priv->window_bbox.ymin = MIN (priv->window_bbox.ymin, bbox.ymin);
		lst = lst->next;
	}

	if (priv->window_bbox.xmin == priv->window_bbox.xmax)
		priv->window_bbox.xmax += 1;

	if (priv->window_bbox.ymin == priv->window_bbox.ymax)
		priv->window_bbox.ymax += 1;

	priv->window_bbox.ymin *= 1.1;
	priv->window_bbox.ymax *= 1.1;

	priv->window_valid = TRUE;
}

void
g_plot_reset_zoom (GPlot *p)
{
	g_return_if_fail (IS_GPLOT (p));

	p->priv->window_valid = FALSE;
	p->priv->zoom = 1.0;
	gtk_widget_queue_draw (GTK_WIDGET (p));
}

void
g_plot_set_axis_labels (GPlot *p, gchar *x, gchar *y)
{
	cairo_text_extents_t extents;

	g_return_if_fail (IS_GPLOT (p));

	if (x) {
		cairo_t *cr = g_plot_create_cairo (p);
		cairo_set_source_rgb (cr, 1.0, 1.0, 1.0);
		cairo_set_font_size (cr, 14);
		cairo_text_extents (cr, x, &extents);
		cairo_destroy (cr);

		p->priv->xlabel = g_strdup (x);
		p->priv->bottom_border = BORDER_SIZE + extents.height;
	} else {
		p->priv->bottom_border = BORDER_SIZE;
		if (p->priv->xlabel) {
			g_free (p->priv->xlabel);
			p->priv->xlabel = NULL;
		}
	}

	if (y) {
		cairo_t *cr = g_plot_create_cairo (p);
		cairo_set_source_rgb (cr, 1.0, 1.0, 1.0);
		cairo_set_font_size (cr, 14);
		cairo_text_extents (cr, y, &extents);
		cairo_destroy (cr);

		p->priv->xlabel = g_strdup (x);
		p->priv->left_border = BORDER_SIZE + extents.height;

		p->priv->ylabel = g_strdup (y);
	} else {
		p->priv->left_border = BORDER_SIZE;
		if (p->priv->ylabel) {
			g_free (p->priv->ylabel);
			p->priv->ylabel = NULL;
		}
	}
}

void
g_plot_clear (GPlot *plot)
{
	GList *lst;

	g_return_if_fail (plot != NULL);
	g_return_if_fail (IS_GPLOT (plot));

	lst = plot->priv->functions;

	while (lst) {
		g_object_unref (G_OBJECT (lst->data));
		lst = lst->next;
	}
	g_list_free (plot->priv->functions);
	plot->priv->functions = NULL;
	plot->priv->window_valid = FALSE;
}

void
g_plot_window_to_device (GPlot *plot, double *x, double *y)
{
	cairo_t *cr;

	g_return_if_fail (plot != NULL);
	g_return_if_fail (IS_GPLOT (plot));

	cr = g_plot_create_cairo (plot);
	cairo_set_matrix (cr, &plot->priv->matrix);
	cairo_device_to_user (cr, x, y);
	cairo_destroy (cr);
}

