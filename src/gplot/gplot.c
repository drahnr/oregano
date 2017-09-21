/*
 * gplot.c
 *
 * Authors:
 *  Ricardo Markiewicz <rmarkie@fi.uba.ar>
 *  Marc Lorber <lorber.marc@wanadoo.fr>
 *  Guido Trentalancia <guido@trentalancia.com>
 *
 * Web page: https://ahoi.io/project/oregano
 *
 * Copyright (C) 1999-2001  Richard Hult
 * Copyright (C) 2003,2004  Ricardo Markiewicz
 * Copyright (C) 2009-2012  Marc Lorber
 * Copyright (C) 2017       Guido Trentalancia
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
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include <math.h>

#include "gplot-internal.h"
#include "gplot.h"

#define BORDER_SIZE 50

static void g_plot_class_init (GPlotClass *class);
static void g_plot_init (GPlot *plot);
static gboolean g_plot_draw (GtkWidget *widget, cairo_t *cr);
static cairo_t *g_plot_create_cairo (GPlot *);
static gboolean g_plot_motion_cb (GtkWidget *, GdkEventMotion *, GPlot *);
static gboolean g_plot_button_press_cb (GtkWidget *, GdkEventButton *, GPlot *);
static gboolean g_plot_button_release_cb (GtkWidget *, GdkEventButton *, GPlot *);
static void g_plot_update_bbox (GPlot *);
static void g_plot_finalize (GObject *object);
static void g_plot_dispose (GObject *object);

static void get_order_of_magnitude (gdouble val, gdouble *man, gdouble *pw);

enum { ACTION_NONE, ACTION_STARTING_PAN, ACTION_PAN, ACTION_STARTING_REGION, ACTION_REGION };

static GtkLayoutClass *parent_class = NULL;

struct _GPlotPriv
{
	GList *functions;

	gchar *xlabel;
	gchar *xlabel_unit;
	gchar *ylabel;
	gchar *ylabel_unit;

	gdouble left_border;
	gdouble right_border;
	gdouble top_border;
	gdouble bottom_border;

	guint zoom_mode;
	guint action;
	gdouble zoom;
	gdouble offset_x;
	gdouble offset_y;
	gdouble press_x;
	gdouble press_y;

	// Window->Viewport * Transformation Matrix
	cairo_matrix_t matrix;

	gboolean window_valid;
	GPlotFunctionBBox window_bbox;
	GPlotFunctionBBox viewport_bbox;

	GPlotFunctionBBox rubberband;

#if GTK_CHECK_VERSION(3,22,0)
	GdkDrawingContext *gdk_ctx;

	cairo_region_t *region;
#endif
};

GType g_plot_get_type ()
{
	static GType g_plot_type = 0;

	if (!g_plot_type) {
		static const GTypeInfo g_plot_info = {
		    sizeof(GPlotClass), NULL,          NULL, (GClassInitFunc)g_plot_class_init, NULL,
		    NULL,               sizeof(GPlot), 0,    (GInstanceInitFunc)g_plot_init,    NULL};
		g_plot_type = g_type_register_static (GTK_TYPE_LAYOUT, "GPlot", &g_plot_info, 0);
	}
	return g_plot_type;
}

static void g_plot_class_init (GPlotClass *class)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;

	object_class = G_OBJECT_CLASS (class);
	widget_class = GTK_WIDGET_CLASS (class);
	parent_class = g_type_class_peek_parent (class);

	widget_class->draw = g_plot_draw;

	object_class->dispose = g_plot_dispose;
	object_class->finalize = g_plot_finalize;
}

static cairo_t *g_plot_create_cairo (GPlot *p)
{
	GdkWindow *window;
	cairo_t *cr;

	window = gtk_layout_get_bin_window (GTK_LAYOUT (p));

#if GTK_CHECK_VERSION(3,22,0)
	p->priv->region = gdk_window_get_clip_region (window);
	p->priv->gdk_ctx = gdk_window_begin_draw_frame (window, p->priv->region);
	cr = gdk_drawing_context_get_cairo_context (p->priv->gdk_ctx);
#else
	cr = gdk_cairo_create (window);
#endif

	return cr;
}

static void g_plot_finalize (GObject *object)
{
	GPlot *p = GPLOT (object);

	g_free (p->priv->xlabel);
	g_free (p->priv->ylabel);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void g_plot_dispose (GObject *object)
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
	g_list_free_full (lst, g_object_unref);

	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static int get_best_exponent (int div)
{
	// http://en.wikipedia.org/wiki/Micro
	switch (div) {
	case -24:
	case -21:
	case -18:
	case -15:
	case -12:
	case -9:
	case -6:
	case -3:
	case 0:
	case 3:
	case 6:
	case 9:
	case 12:
	case 15:
	case 18:
	case 21:
	case 24:
		return div;
	}
	if (div == -1)
		return -3;

	if ((div - 1) % 3 == 0)
		return div - 1;
	return div + 1;
}

void draw_axis (cairo_t *cr, GPlotFunctionBBox *bbox, gdouble min, gdouble max, gboolean vertical,
                gint *div)
{
	gchar *label;
	cairo_text_extents_t extents;
	gdouble i, j;
	gdouble x1, y1, x2, y2, x3, y3;
	gdouble step = (max - min) / 10.0;
	gdouble s;
	gdouble divisor;
	gdouble man1, pw1, man2, pw2;

	get_order_of_magnitude (min, &man1, &pw1);
	get_order_of_magnitude (max, &man2, &pw2);
	(*div) = get_best_exponent ((pw2 + pw1) / 2.0 + 0.5);
	if ((*div) == 0)
		divisor = 1;
	else
		divisor = pow (10, *div);

	if (vertical)
		s = (bbox->ymax - bbox->ymin) / 10.0;
	else
		s = (bbox->xmax - bbox->xmin) / 10.0;

	for (i = (vertical ? bbox->ymin : bbox->xmin), j = max;
	     i <= (vertical ? bbox->ymax : bbox->xmax) + 0.5; i += s, j -= step) {
		label = g_strdup_printf ("%.*f", MAX(0, (int)(2 - floor(log10(fabs((max - min)/divisor))))), j / divisor);
		cairo_text_extents (cr, label, &extents);

		if (vertical) {
			x1 = bbox->xmin - 4;
			y1 = i;
			x2 = bbox->xmin + 4;
			y2 = i;
			x3 = bbox->xmin - extents.width - 6;
			y3 = i + extents.height / 2.0;
		} else {
			x1 = i;
			y1 = bbox->ymax - 4;
			x2 = i;
			y2 = bbox->ymax + 4;
			x3 = i;
			y3 = bbox->ymax + extents.height * 1.5;
		}

		cairo_move_to (cr, x1, y1);
		cairo_line_to (cr, x2, y2);
		cairo_move_to (cr, x3, y3);
		if (!vertical && extents.width > s - 5) {
			cairo_save(cr);
			cairo_rotate(cr, M_PI/10);
		}
		cairo_show_text (cr, label);
		if (!vertical && extents.width > s - 5) {
			cairo_restore(cr);
		}
		g_free (label);
	}
	cairo_stroke (cr);
}

static gchar *get_unit_text (int div)
{
	// http://en.wikipedia.org/wiki/Micro
	switch (div) {
	case -24:
		return g_strdup ("y");
	case -21:
		return g_strdup ("z");
	case -18:
		return g_strdup ("a");
	case -15:
		return g_strdup ("f");
	case -12:
		return g_strdup ("p");
	case -9:
		return g_strdup ("n");
	case -6:
		return g_strdup ("\302\265");
	case -3:
		return g_strdup ("m");
	case 0:
		return g_strdup ("");
	case 3:
		return g_strdup ("k");
	case 6:
		return g_strdup ("M");
	case 9:
		return g_strdup ("G");
	case 12:
		return g_strdup ("T");
	case 15:
		return g_strdup ("P");
	case 18:
		return g_strdup ("E");
	case 21:
		return g_strdup ("Z");
	case 24:
		return g_strdup ("Y");
	}

	return g_strdup_printf ("10e%02d", div);
}

static gboolean g_plot_draw (GtkWidget *widget, cairo_t *cr)
{
	static double dashes[] = {3,  // ink
	                          3,  // skip
	                          3,  // ink
	                          3}; // skip
	static int ndash = sizeof(dashes) / sizeof(dashes[0]);
	static double offset = -0.2;

	GPlot *plot;
	GPlotPriv *priv;
	guint width;
	guint height;
	guint graph_width;
	guint graph_height;
	GPlotFunction *f;
	GList *lst;
	gdouble aX, bX, aY, bY;
	gint div;
	cairo_text_extents_t extents;

	plot = GPLOT (widget);
	priv = plot->priv;

	if (!priv->window_valid) {
		g_plot_update_bbox (plot);
	}

	width = gtk_widget_get_allocated_width (widget);
	height = gtk_widget_get_allocated_height (widget);

	graph_width = width - priv->left_border - priv->right_border;
	graph_height = height - priv->top_border - priv->bottom_border;

	// Paint background
	cairo_save (cr);
	cairo_set_source_rgb (cr, 0.0, 0.0, 0.0);
	cairo_rectangle (cr, 0, 0, width, height);
	cairo_fill (cr);
	cairo_restore (cr);

	// Plot Border
	cairo_save (cr);
	cairo_set_line_width (cr, 0.5);
	cairo_set_source_rgb (cr, 1.0, 1.0, 1.0);
	cairo_rectangle (cr, priv->left_border, priv->right_border, graph_width, graph_height);
	cairo_stroke (cr);
	cairo_restore (cr);

	priv->viewport_bbox.xmax = width - priv->right_border;
	priv->viewport_bbox.xmin = priv->left_border;
	priv->viewport_bbox.ymax = height - priv->bottom_border;
	priv->viewport_bbox.ymin = priv->top_border;

	// Calculating Window to Viewport matrix
	aX = (priv->viewport_bbox.xmax - priv->viewport_bbox.xmin) /
	     (priv->window_bbox.xmax - priv->window_bbox.xmin);
	bX = -aX * priv->window_bbox.xmin + priv->viewport_bbox.xmin;
	aY = (priv->viewport_bbox.ymax - priv->viewport_bbox.ymin) /
	     (priv->window_bbox.ymin - priv->window_bbox.ymax);
	bY = -aY * priv->window_bbox.ymax + priv->viewport_bbox.ymin;

	cairo_matrix_init (&priv->matrix, aX, 0, 0, aY, bX, bY);

	//plot functions
	cairo_save (cr);
	cairo_rectangle (cr, priv->left_border, priv->right_border, graph_width, graph_height);
	cairo_clip (cr);

	cairo_set_source_rgb (cr, 1.0, 1.0, 1.0);
	cairo_transform(cr, &priv->matrix);
	lst = plot->priv->functions;
	while (lst) {
		f = (GPlotFunction *)lst->data;
		g_plot_function_draw (f, cr, &priv->window_bbox);
		lst = lst->next;
	}
	cairo_restore (cr);

	//plot red axis
	cairo_save (cr);
	{
		cairo_rectangle (cr, priv->left_border, priv->right_border, graph_width, graph_height);
		cairo_clip (cr);
		//plot x axis
		cairo_save (cr);
		{
			cairo_transform(cr, &priv->matrix);
			cairo_move_to (cr, priv->window_bbox.xmin, 0.0);
			cairo_line_to (cr, priv->window_bbox.xmax, 0.0);
		}
		cairo_restore (cr);
		cairo_save (cr);
		{
			cairo_set_source_rgb (cr, 1.0, 0.0, 0.0);
			cairo_set_line_width (cr, 2);
			cairo_stroke (cr);
		}
		cairo_restore (cr);

		//plot y axis
		cairo_save (cr);
		{
			cairo_transform(cr, &priv->matrix);
			cairo_move_to (cr, 0.0, priv->window_bbox.ymin);
			cairo_line_to (cr, 0.0, priv->window_bbox.ymax);
		}
		cairo_restore (cr);
		cairo_save (cr);
		{
			cairo_set_source_rgb (cr, 1.0, 0.0, 0.0);
			cairo_set_line_width (cr, 2);
			cairo_stroke (cr);
		}
		cairo_restore (cr);
	}
	cairo_restore (cr);

	//plot axis ticks
	cairo_save (cr);
	cairo_set_source_rgb (cr, 1.0, 1.0, 1.0);
	cairo_set_line_width (cr, 1);

	draw_axis (cr, &priv->viewport_bbox, priv->window_bbox.ymin, priv->window_bbox.ymax, TRUE,
	           &div);
	if (priv->ylabel_unit) {
		g_free (priv->ylabel_unit);
		priv->ylabel_unit = NULL;
	}
	if (div != 1)
		priv->ylabel_unit = get_unit_text (div);
	draw_axis (cr, &priv->viewport_bbox, priv->window_bbox.xmax, priv->window_bbox.xmin, FALSE,
	           &div);
	if (priv->xlabel_unit) {
		g_free (priv->xlabel_unit);
		priv->xlabel_unit = NULL;
	}
	if (div != 1)
		priv->xlabel_unit = get_unit_text (div);
	cairo_restore (cr);

	// Axis x Label
	if (priv->xlabel) {
		char *txt;
		if (priv->xlabel_unit == NULL)
			txt = g_strdup (priv->xlabel);
		else
			txt = g_strdup_printf ("%s %s", priv->xlabel_unit, priv->xlabel);

		cairo_save (cr);
		cairo_set_source_rgb (cr, 1.0, 1.0, 1.0);
		cairo_set_font_size (cr, 14);
		cairo_text_extents (cr, txt, &extents);
		cairo_move_to (cr, width / 2.0 - extents.width / 2.0, height - extents.height / 2.0);
		cairo_show_text (cr, txt);
		cairo_stroke (cr);
		cairo_restore (cr);
		g_free (txt);
	}

	//axis y label
	if (priv->ylabel) {
		char *txt;
		if (priv->ylabel_unit == NULL)
			txt = g_strdup (priv->ylabel);
		else
			txt = g_strdup_printf ("%s %s", priv->ylabel_unit, priv->ylabel);

		cairo_save (cr);
		cairo_set_source_rgb (cr, 1.0, 1.0, 1.0);
		cairo_set_font_size (cr, 14);
		cairo_text_extents (cr, txt, &extents);
		cairo_move_to (cr, extents.height, height / 2.0 + extents.width / 2.0);
		cairo_rotate (cr, 4.7124);
		cairo_show_text (cr, txt);
		cairo_stroke (cr);
		cairo_restore (cr);
	}

	//plot rubberband (zoom-in-rectangle)
	if (priv->action == ACTION_REGION) {
		gdouble x, y, w, h;
		x = priv->rubberband.xmin;
		y = priv->rubberband.ymin;
		w = priv->rubberband.xmax;
		h = priv->rubberband.ymax;
		cairo_save (cr);
		cairo_set_dash (cr, dashes, ndash, offset);

		cairo_set_line_width (cr, 2);
		cairo_set_source_rgb (cr, 0.0, 1.0, 0.0);
		cairo_rectangle (cr, x, y, w - x, h - y);
		cairo_stroke (cr);
		cairo_restore (cr);
	}
	g_list_free_full (lst, g_object_unref);

	return FALSE;
}

static void g_plot_init (GPlot *plot)
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
	plot->priv->xlabel_unit = NULL;
	plot->priv->ylabel = NULL;
	plot->priv->ylabel_unit = NULL;
}

GtkWidget *g_plot_new ()
{
	GPlot *plot;

	plot = GPLOT (g_object_new (TYPE_GPLOT, NULL));

	gtk_widget_add_events (GTK_WIDGET (plot), GDK_POINTER_MOTION_MASK | GDK_BUTTON_PRESS_MASK |
	                                              GDK_BUTTON_RELEASE_MASK | GDK_BUTTON_MOTION_MASK |
	                                              GDK_BUTTON1_MOTION_MASK |
	                                              GDK_BUTTON2_MOTION_MASK |
	                                              GDK_BUTTON3_MOTION_MASK);

	g_signal_connect (G_OBJECT (plot), "motion-notify-event", G_CALLBACK (g_plot_motion_cb), plot);
	g_signal_connect (G_OBJECT (plot), "button-press-event", G_CALLBACK (g_plot_button_press_cb),
	                  plot);
	g_signal_connect (G_OBJECT (plot), "button-release-event",
	                  G_CALLBACK (g_plot_button_release_cb), plot);

	return GTK_WIDGET (plot);
}

int g_plot_add_function (GPlot *plot, GPlotFunction *func)
{
	g_return_val_if_fail (IS_GPLOT (plot), -1);

	plot->priv->functions = g_list_append (plot->priv->functions, func);
	plot->priv->window_valid = FALSE;
	return 0;
}

static gboolean g_plot_motion_cb (GtkWidget *w, GdkEventMotion *e, GPlot *p)
{
#if GTK_CHECK_VERSION (3,16,0)
	GdkDisplay *display;
	display = gtk_widget_get_display (w);
#endif

	switch (p->priv->zoom_mode) {
	case GPLOT_ZOOM_INOUT:
		if ((p->priv->action == ACTION_STARTING_PAN) || (p->priv->action == ACTION_PAN)) {
			gdouble dx, dy;
			cairo_matrix_t t = p->priv->matrix;
#if GTK_CHECK_VERSION (3,16,0)
			GdkCursor *cursor = gdk_cursor_new_for_display (display, GDK_FLEUR);
#else
			GdkCursor *cursor = gdk_cursor_new (GDK_FLEUR);
#endif
			gdk_window_set_cursor (gtk_widget_get_window (w), cursor);
			gdk_flush ();

			dx = p->priv->press_x - e->x;
			dy = p->priv->press_y - e->y;

			cairo_matrix_invert (&t);
			cairo_matrix_transform_distance (&t, &dx, &dy);

			p->priv->window_bbox.xmin -= dx;
			p->priv->window_bbox.xmax -= dx;
			p->priv->window_bbox.ymin -= dy;
			p->priv->window_bbox.ymax -= dy;

			p->priv->press_x = e->x;
			p->priv->press_y = e->y;
			p->priv->action = ACTION_PAN;
			gtk_widget_queue_draw (w);
		}
		break;
	case GPLOT_ZOOM_REGION:
		if ((p->priv->action == ACTION_STARTING_REGION) || (p->priv->action == ACTION_REGION)) {
#if GTK_CHECK_VERSION (3,16,0)
			GdkCursor *cursor = gdk_cursor_new_for_display (display, GDK_CROSS);
#else
			GdkCursor *cursor = gdk_cursor_new (GDK_CROSS);
#endif
			gdk_window_set_cursor (gtk_widget_get_window (w), cursor);
			gdk_flush ();

			p->priv->rubberband.xmin = MIN(e->x, p->priv->press_x);
			p->priv->rubberband.xmax = MAX(e->x, p->priv->press_x);

			p->priv->rubberband.ymin = MIN(e->y, p->priv->press_y);
			p->priv->rubberband.ymax = MAX(e->y, p->priv->press_y);

			p->priv->action = ACTION_REGION;
			gtk_widget_queue_draw (w);
		}
	}

	return FALSE;
}

static gboolean g_plot_button_press_cb (GtkWidget *w, GdkEventButton *e, GPlot *p)
{
	g_return_val_if_fail (IS_GPLOT (p), TRUE);

	if (e->type == GDK_2BUTTON_PRESS) {
		/* TODO : Check function below cursor and open a property dialog :) */
	} else {
		switch (p->priv->zoom_mode) {
		case GPLOT_ZOOM_INOUT:
			if (e->button == 1) {
				p->priv->action = ACTION_STARTING_PAN;
				p->priv->press_x = e->x;
				p->priv->press_y = e->y;
			}
			break;
		case GPLOT_ZOOM_REGION:
			if (e->button == 1) {
				p->priv->action = ACTION_STARTING_REGION;
				p->priv->rubberband.xmin = e->x;
				p->priv->rubberband.ymin = e->y;
				p->priv->rubberband.xmax = e->x;
				p->priv->rubberband.ymax = e->y;
				p->priv->press_x = e->x;
				p->priv->press_y = e->y;
			}
		}
	}

	return TRUE;
}

static gboolean g_plot_button_release_cb (GtkWidget *w, GdkEventButton *e, GPlot *p)
{
	gdouble zoom = 0.0;

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
			gdk_window_set_cursor (gtk_widget_get_window (w), NULL);
			gdk_flush ();
		}
		break;
	case GPLOT_ZOOM_REGION:
		if ((e->button == 1) && (p->priv->action == ACTION_REGION)) {
			gdk_window_set_cursor (gtk_widget_get_window (w), NULL);
			gdk_flush ();

			if (e->x < p->priv->press_x || e->y < p->priv->press_y) {
				g_plot_reset_zoom(p);
			} else {
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
			gdk_window_set_cursor (gtk_widget_get_window (w), NULL);
			gdk_flush ();
		}
		gtk_widget_queue_draw (w);
	}

	p->priv->action = ACTION_NONE;
	return TRUE;
}

void g_plot_set_zoom_mode (GPlot *p, guint mode)
{
	g_return_if_fail (IS_GPLOT (p));

	p->priv->zoom_mode = mode;
}

guint g_plot_get_zoom_mode (GPlot *p)
{
	g_return_val_if_fail (IS_GPLOT (p), 0);

	return p->priv->zoom_mode;
}

static void g_plot_update_bbox (GPlot *p)
{
	GPlotFunction *f;
	GPlotFunctionBBox bbox;
	GPlotPriv *priv;
	GList *lst;

	priv = p->priv;

	// Get functions bbox
	priv->window_bbox.xmax = -9999999;
	priv->window_bbox.xmin = 9999999;
	priv->window_bbox.ymax = -9999999;
	priv->window_bbox.ymin = 9999999;
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
	g_list_free_full (lst, g_object_unref);

	if (priv->window_bbox.xmin == priv->window_bbox.xmax)
		priv->window_bbox.xmax += 1;

	if (priv->window_bbox.ymin == priv->window_bbox.ymax)
		priv->window_bbox.ymax += 1;

	priv->window_bbox.ymin *= 1.1;
	priv->window_bbox.ymax *= 1.1;

	priv->window_valid = TRUE;
}

void g_plot_reset_zoom (GPlot *p)
{
	g_return_if_fail (IS_GPLOT (p));

	p->priv->window_valid = FALSE;
	p->priv->zoom = 1.0;
	gtk_widget_queue_draw (GTK_WIDGET (p));
}

void g_plot_set_axis_labels (GPlot *p, gchar *x, gchar *y)
{
	cairo_text_extents_t extents;

	g_return_if_fail (IS_GPLOT (p));

	if (x) {
		cairo_t *cr = g_plot_create_cairo (p);
		cairo_set_source_rgb (cr, 1.0, 1.0, 1.0);
		cairo_set_font_size (cr, 14);
		cairo_text_extents (cr, x, &extents);
#if GTK_CHECK_VERSION(3,22,0)
		gdk_window_end_draw_frame (gtk_layout_get_bin_window (GTK_LAYOUT (p)) , p->priv->gdk_ctx);
		cairo_region_destroy (p->priv->region);
#else
		cairo_destroy (cr);
#endif

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
#if GTK_CHECK_VERSION(3,22,0)
		gdk_window_end_draw_frame (gtk_layout_get_bin_window (GTK_LAYOUT (p)) , p->priv->gdk_ctx);
		cairo_region_destroy (p->priv->region);
#else
		cairo_destroy (cr);
#endif

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

void g_plot_clear (GPlot *plot)
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
	g_list_free (lst);
}

void g_plot_window_to_device (GPlot *plot, double *x, double *y)
{
	cairo_t *cr;

	g_return_if_fail (plot != NULL);
	g_return_if_fail (IS_GPLOT (plot));

	cr = g_plot_create_cairo (plot);
	cairo_set_matrix (cr, &plot->priv->matrix);
	cairo_device_to_user (cr, x, y);

#if GTK_CHECK_VERSION(3,22,0)
	gdk_window_end_draw_frame (gtk_layout_get_bin_window (GTK_LAYOUT (plot)) , plot->priv->gdk_ctx);
	cairo_region_destroy (plot->priv->region);
#else
	cairo_destroy (cr);
#endif
}

static void get_order_of_magnitude (gdouble val, gdouble *man, gdouble *pw)
{
	gdouble b;
	gdouble sx;

	b = (val != 0.0 ? log10 (fabs (val)) / 3.0 : 0.0);
	b = 3.0 * rint (b);
	sx = (b != 0.0 ? pow (10.0, b) : 1.0);
	*man = val / sx;
	*pw = b;
}
