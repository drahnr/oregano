/*
 * gtkcairoplotview.h 
 *
 * 
 * Author: 
 *  Ricardo Markiewicz <rmarkie@fi.uba.ar>
 * 
 *  http://www.fi.uba.ar/~rmarkie/
 * 
 * Copyright (C) 2004  Ricardo Markiewicz
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

#ifndef _GTK_CAIRO_PLOT_VIEW_
#define _GTK_CAIRO_PLOT_VIEW_

#include <glib.h>
#include <glib-object.h>
#include <gtk/gtk.h>
#include <cairo/cairo.h>
#include "gtkcairoplotmodel.h"

#define GTK_CAIRO_PLOT_VIEW_TYPE          (gtk_cairo_plot_view_get_type ())
#define GTK_CAIRO_PLOT_VIEW(obj)          (G_TYPE_CHECK_INSTANCE_CAST(obj, GTK_CAIRO_PLOT_VIEW_TYPE, GtkCairoPlotView))
#define GTK_CAIRO_PLOT_VIEW_CLASS(klass)  (G_TYPE_CHECK_CLASS_CAST(klass, GTK_CAIRO_PLOT_VIEW_TYPE, GtkCairoPlotViewClass))
#define IS_GTK_CAIRO_PLOT_VIEW(obj)       (G_TYPE_CHECK_INSTANCE_TYPE(obj, GTK_CAIRO_PLOT_VIEW_TYPE))

/* The View */
typedef struct _GtkCairoPlotView GtkCairoPlotView;
typedef struct _GtkCairoPlotViewClass GtkCairoPlotViewClass;
typedef struct _GtkCairoPlotViewPriv GtkCairoPlotViewPriv;

/* The Items */
typedef struct _GtkCairoPlotViewItem GtkCairoPlotViewItem;
typedef struct _GtkCairoPlotViewItemClass GtkCairoPlotViewItemClass;
typedef struct _GtkCairoPlotViewItemPriv GtkCairoPlotViewItemPriv;

struct _GtkCairoPlotView{
	GObject parent;

	GtkCairoPlotViewPriv *priv;
	
	gboolean in_zoom;
	double sx, sy, fx, fy;
};

struct _GtkCairoPlotViewClass {
	GObjectClass parent_class;
};

enum {
	ITEM_POS_FRONT,
	ITEM_POS_BACK,
};

enum {
	GTK_CAIRO_PLOT_VIEW_SAVE_PNG,
	GTK_CAIRO_PLOT_VIEW_SAVE_PS
};

GType gtk_cairo_plot_view_get_type (void);
GtkCairoPlotView *gtk_cairo_plot_view_new (void);
void gtk_cairo_plot_view_get_point (GtkCairoPlotView *view, GtkCairoPlotModel *model, gdouble x, gdouble y, gdouble *x1, gdouble *y1);
void gtk_cairo_plot_view_add_item (GtkCairoPlotView *view, GtkCairoPlotViewItem *item, guint pos);
void gtk_cairo_plot_view_remove_item (GtkCairoPlotView *view, GtkCairoPlotViewItem *item);
void gtk_cairo_plot_view_save (GtkCairoPlotView *view, GtkCairoPlotModel *model, gint w, gint h, gchar *fn, int format);

/* TODO AGREGAR METODO DE CLASE Y QUE ESTA SEA ABSTRACTA Y HEREDAR XXX */
void gtk_cairo_plot_view_draw (GtkCairoPlotView *view, GtkCairoPlotModel *model, GdkWindow *window);
void gtk_cairo_plot_view_set_size (GtkCairoPlotView *view, int widget, int height);
void gtk_cairo_plot_view_attach (GtkCairoPlotView *view, GtkCairoPlotViewItem *item);
void gtk_cairo_plot_view_set_scroll_region (GtkCairoPlotView *item, gdouble x1, gdouble x2, gdouble y1, gdouble y2);

/* === Item related funcions === */
/* Items are placed over a GtkCairoPlotView to generate de complete view.
 * You can combine as many as you want (or can) to reproduce nicely 
 * end user experience ;-)
 */
#define GTK_CAIRO_PLOT_VIEW_ITEM_TYPE          (gtk_cairo_plot_view_item_get_type ())
#define GTK_CAIRO_PLOT_VIEW_ITEM(obj)          (G_TYPE_CHECK_INSTANCE_CAST(obj, GTK_CAIRO_PLOT_VIEW_ITEM_TYPE, GtkCairoPlotViewItem))
#define GTK_CAIRO_PLOT_VIEW_ITEM_CLASS(klass)  (G_TYPE_CHECK_CLASS_CAST(klass, GTK_CAIRO_PLOT_VIEW_ITEM_TYPE, GtkCairoPlotViewItemClass))
#define IS_GTK_CAIRO_PLOT_VIEW_ITEM(obj)       (G_TYPE_CHECK_INSTANCE_TYPE(obj, GTK_CAIRO_PLOT_VIEW_ITEM_TYPE))


struct _GtkCairoPlotViewItem {
	GObject item;

	GtkCairoPlotViewItemPriv *priv;
};

struct _GtkCairoPlotViewItemClass {
	GObjectClass parent_class;
	
	void (*update) (GtkCairoPlotViewItem *view, GtkCairoPlotModel *model, cairo_t *cr);
	void (*get_point) (GtkCairoPlotViewItem *view, GtkCairoPlotModel *model, gdouble x, gdouble y, gdouble *x1, gdouble *y1);
};

GType gtk_cairo_plot_view_item_get_type (void);
GtkCairoPlotViewItem *gtk_cairo_plot_view_item_new (GType type, guint x, guint y, guint w, guint h);
void gtk_cairo_plot_view_item_get_point (GtkCairoPlotViewItem *view, GtkCairoPlotModel *model, gdouble x, gdouble y, gdouble *x1, gdouble *y1);
void gtk_cairo_plot_view_item_update (GtkCairoPlotViewItem *item, GtkCairoPlotModel *model, cairo_t *cr);

#endif

