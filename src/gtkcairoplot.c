/*
 * gtkcairoplot.c
 *
 * 
 * Author: 
 *  Ricardo Markiewicz <rmarkie@fi.uba.ar>
 * 
 *  http://www.fi.uba.ar/~rmarkie/
 * 
 * Copyright (C) 2005  Ricardo Markiewicz
 * 
 * This program is free software; you can redistribute it and/or 
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
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <gdk/gdkx.h>
#include <cairo.h>
#include <math.h>
#include <gdk/gdkkeysyms.h>
#include "gtkcairoplot.h"
#include "gtkcairoplotitems.h"

static GtkLayoutClass* parent_class = NULL;

static void gtk_cairo_plot_class_init (GtkCairoPlotClass* class);
static void gtk_cairo_plot_init (GtkCairoPlot* ge);
static void gtk_cairo_plot_size_allocate (GtkWidget* widget, GtkAllocation* allocation);
static gint gtk_cairo_plot_expose (GtkWidget* widget, GdkEventExpose* event);
static gint gtk_cairo_plot_event_callback (GtkWidget *widget, GdkEvent *event, gpointer data);
static gint rubberband_timeout_cb (GtkCairoPlot *plot);

struct _GtkCairoPlotPriv {
	GtkCairoPlotModel *model;
	GtkCairoPlotView *view;

	gboolean in_zoom;
	int timeout_id;
	guint start_x, start_y;
	GtkCairoPlotViewItem *rubberband;
};

				
GType
gtk_cairo_plot_get_type (void)
{
	static GType gtk_plot_type = 0;
    
	if (!gtk_plot_type) {
		static const GTypeInfo gtk_plot_info = {
			sizeof (GtkCairoPlotClass),
			NULL,
			NULL,
			(GClassInitFunc) gtk_cairo_plot_class_init,
			NULL,
			NULL,
			sizeof (GtkCairoPlot),
			0,
			(GInstanceInitFunc) gtk_cairo_plot_init,
			NULL
		};
		gtk_plot_type = g_type_register_static (GTK_TYPE_LAYOUT, "GtkCairoPlot", &gtk_plot_info, 0);
	}
	return gtk_plot_type;
}

GtkWidget*
gtk_cairo_plot_new (void)
{
	GtkCairoPlot *cw;
	GtkWidget *widget;

	cw = GTK_CAIRO_PLOT (g_object_new (gtk_cairo_plot_get_type (), NULL));
	widget = GTK_WIDGET (cw);
	widget->style = gtk_style_new ();

	gtk_widget_add_events (widget,
		GDK_POINTER_MOTION_MASK|GDK_BUTTON_PRESS_MASK|
		GDK_BUTTON_RELEASE_MASK|GDK_BUTTON_MOTION_MASK|
		GDK_BUTTON1_MOTION_MASK|GDK_BUTTON2_MOTION_MASK|
		GDK_BUTTON3_MOTION_MASK);

	g_signal_connect (G_OBJECT (widget), "event", G_CALLBACK(gtk_cairo_plot_event_callback), NULL);

	return widget;
}

static void
gtk_cairo_plot_class_init (GtkCairoPlotClass* class)
{
	GObjectClass* object_class;
	GtkWidgetClass* widget_class;

	object_class = G_OBJECT_CLASS (class);
	widget_class = GTK_WIDGET_CLASS (class);
	parent_class = g_type_class_peek_parent (class);

	widget_class->size_allocate = gtk_cairo_plot_size_allocate;
	widget_class->expose_event = gtk_cairo_plot_expose;
}

static void
gtk_cairo_plot_set_size (GtkCairoPlot *cw, int width, int height)
{
	cw->width = width;
	cw->height = height;
	gtk_cairo_plot_view_set_size (cw->priv->view, width, height);
}

static void
gtk_cairo_plot_init (GtkCairoPlot* cw)
{
	cw->width = 100;
	cw->height = 100;

	cw->priv = g_new0 (GtkCairoPlotPriv, 1);

	cw->priv->model = NULL;
	cw->priv->view = NULL;
}

static void
gtk_cairo_plot_size_allocate (GtkWidget* widget, GtkAllocation* allocation)
{
	GtkCairoPlot *cw;

	g_return_if_fail (widget != NULL);
	g_return_if_fail (IS_GTK_CAIRO_PLOT (widget));
	g_return_if_fail (allocation != NULL);

	cw = GTK_CAIRO_PLOT (widget);
	widget->allocation = *allocation;
	if (GTK_WIDGET_REALIZED (widget)) {
		gdk_window_move_resize (
			GTK_LAYOUT (widget)->bin_window, 
			0, 0, //allocation->x, allocation->y,
			allocation->width, allocation->height
		);
	}

	gtk_cairo_plot_set_size (cw, widget->allocation.width, widget->allocation.height);

	GTK_WIDGET_CLASS (parent_class)->size_allocate (widget, allocation);
}

static gint
gtk_cairo_plot_expose (GtkWidget* widget, GdkEventExpose* event)
{
	GtkCairoPlot *plot;

	g_return_val_if_fail (IS_GTK_CAIRO_PLOT (widget), FALSE);
	plot = GTK_CAIRO_PLOT (widget);

	g_return_val_if_fail (plot->priv->model != NULL, FALSE);
	g_return_val_if_fail (plot->priv->view != NULL, FALSE);

	gtk_cairo_plot_view_draw (
		plot->priv->view,
		plot->priv->model,
		GDK_WINDOW (GTK_LAYOUT (widget)->bin_window)
	);

	return FALSE;
}

static void
model_chaged (GtkCairoPlotModel *model, GtkCairoPlot *plot)
{
	gtk_widget_queue_draw (GTK_WIDGET (plot));
}

void
gtk_cairo_plot_set_model (GtkCairoPlot *plot, GtkCairoPlotModel *model)
{
	g_return_if_fail (IS_GTK_CAIRO_PLOT (plot));

	if (plot->priv->model) {
		g_object_unref (plot->priv->model);
		g_signal_handlers_disconnect_by_func (
			G_OBJECT (plot),
			G_CALLBACK (model_chaged), plot);
	}
	plot->priv->model = model;
	if (plot->priv->model) {
		g_object_ref (plot->priv->model);
		g_signal_connect (
			G_OBJECT (plot->priv->model), "changed",
			G_CALLBACK (model_chaged), plot);
	}
}

void
gtk_cairo_plot_set_view (GtkCairoPlot *plot, GtkCairoPlotView *view)
{
	g_return_if_fail (IS_GTK_CAIRO_PLOT (plot));

	if (plot->priv->view)
		g_object_unref (plot->priv->view);
	plot->priv->view = view;
	if (plot->priv->view)
		g_object_ref (plot->priv->view);
}

static gint
gtk_cairo_plot_event_callback (GtkWidget *widget, GdkEvent *event, gpointer data)
{
	GtkCairoPlotPriv *priv;

	priv = GTK_CAIRO_PLOT (widget)->priv;

	switch (event->type) {
		case GDK_BUTTON_PRESS:
			if (event->button.button == 1) {
				priv->in_zoom = TRUE;
				priv->start_x = ((GdkEventButton *)event)->x;
				priv->start_y = ((GdkEventButton *)event)->y;
				priv->view->sx = ((GdkEventButton *)event)->x;
				priv->view->sy = ((GdkEventButton *)event)->y;
				priv->rubberband = gtk_cairo_plot_view_item_new (
					GTK_CAIRO_PLOT_ITEM_RECT_TYPE,
					priv->start_x, priv->start_y, 1, 1);

				gtk_cairo_plot_view_add_item (priv->view, priv->rubberband, ITEM_POS_FRONT);

				priv->timeout_id = g_timeout_add (
					15,
					(gpointer)rubberband_timeout_cb,
					 GTK_CAIRO_PLOT (widget)
				);
			}
			if (event->button.button == 3) {
				gdouble x1, x2, y1, y2;
				priv->view->in_zoom = FALSE;
				gtk_cairo_plot_model_get_x_minmax (priv->model, &x1, &x2);
				gtk_cairo_plot_model_get_y_minmax (priv->model, &y1, &y2);
				gtk_cairo_plot_view_set_scroll_region (
					priv->view, x1, x2, y1, y2);
				gtk_widget_queue_draw (widget);
			}
		break;
		case GDK_BUTTON_RELEASE:
			if (event->button.button == 1) {
				gdouble x, y;
				gdouble x1, x2, y1, y2;
				priv->in_zoom = FALSE;
				gtk_idle_remove (priv->timeout_id);
				gtk_cairo_plot_view_remove_item (priv->view, priv->rubberband);
				
				x1 = priv->start_x;
				x2 = ((GdkEventButton *)event)->x;
				y1 = priv->start_y;
				y2 = ((GdkEventButton *)event)->y;
				if (x1 > x2) {
					x = x1;
					x1 = x2;
					x2 = x;
				}
				if (y2 > y1) {
					y = y1;
					y1 = y2;
					y2 = y;
				}

				gtk_cairo_plot_view_get_point (priv->view, priv->model,
					x1, y1, &x, &y);
				x1 = x;
				y1 = y;
				gtk_cairo_plot_view_get_point (priv->view, priv->model,
					x2, y2, &x, &y);
				x2 = x;
				y2 = y;

				gtk_cairo_plot_view_set_scroll_region (
					priv->view, x1, x2, y1, y2);
				gtk_widget_queue_draw (widget);
			}
		break;
		case GDK_KEY_PRESS:
			if (event->key.keyval == GDK_1) {
			}
		break;
		default:
			return FALSE;
	}
	
	return TRUE; 
}

static gint
rubberband_timeout_cb (GtkCairoPlot *plot)
{
	static guint old_x, old_y;
	static guint old_area_x=0, old_area_y=0, old_area_w=500, old_area_h=500;
	guint x, y;
	guint x1, y1, x2, y2;
	GtkCairoPlotPriv *priv;
	int swap;
	gboolean changed;

	priv = plot->priv;

	gdk_window_get_pointer (GTK_WIDGET (plot)->window, &x, &y, NULL);

	x1 = priv->start_x;
	y1 = priv->start_y;
	x2 = x;
	y2 = y;

	if (x1 > x2) {
		swap = x1;
		x1 = x2;
		x2 = swap;
	}
	if (y1 > y2) {
		swap = y1;
		y1 = y2;
		y2 = swap;
	}

	if (x1 < 0) x1 = 0;
	if (y1 < 0) y1 = 0;

	if ((old_x != x) || (old_y != y)) {
		guint valid_x, valid_y, valid_w, valid_h;
		g_object_set (G_OBJECT (priv->rubberband),
			"x", x1, "y", y1,
			"width", x2-x1, "height", y2-y1, NULL);

		valid_x = MIN(x1, old_area_x);
		valid_y = MIN(y1, old_area_y);
		valid_w = MAX(x2-x1, old_area_w);
		valid_h = MAX(y2-y1, old_area_h);

		gtk_widget_queue_draw_area (GTK_WIDGET (plot),
			valid_x, valid_y,
			valid_w, valid_h);

		old_area_x = valid_x;
		old_area_y = valid_y;
		old_area_w = valid_w;
		old_area_h = valid_h;
	}

	old_x = x;
	old_y = y;
	return TRUE;
}

