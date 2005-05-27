/*
 * gtkcairoplotitems.h 
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

#ifndef _GTK_CAIRO_PLOT_ITEMS_
#define _GTK_CAIRO_PLOT_ITEMS_

#include <glib.h>
#include <glib-object.h>
#include <gtk/gtk.h>
#include "gtkcairoplotview.h"

#define GTK_CAIRO_PLOT_ITEM_DRAW_TYPE          (gtk_cairo_plot_item_draw_get_type ())
#define GTK_CAIRO_PLOT_ITEM_DRAW(obj)          (G_TYPE_CHECK_INSTANCE_CAST(obj, GTK_CAIRO_PLOT_ITEM_DRAW_TYPE, GtkCairoPlotItemDraw))
#define GTK_CAIRO_PLOT_ITEM_DRAW_CLASS(klass)  (G_TYPE_CHECK_CLASS_CAST(klass, GTK_CAIRO_PLOT_ITEM_DRAW_TYPE, GtkCairoPlotItemDrawClass))
#define IS_GTK_CAIRO_PLOT_ITEM_DRAW(obj)       (G_TYPE_CHECK_INSTANCE_TYPE(obj, GTK_CAIRO_PLOT_ITEM_DRAW_TYPE))

/* The View */
typedef struct _GtkCairoPlotItemDraw GtkCairoPlotItemDraw;
typedef struct _GtkCairoPlotItemDrawClass GtkCairoPlotItemDrawClass;

struct _GtkCairoPlotItemDraw {
	GtkCairoPlotViewItem parent;

	/* Window coordinates */
	gdouble x1, x2;
	gdouble y1, y2;
};

struct _GtkCairoPlotItemDrawClass {
	GtkCairoPlotViewItemClass parent_class;
};

GType gtk_cairo_plot_item_draw_get_type (void);
void  gtk_cairo_plot_item_draw_set_scroll_region (GtkCairoPlotItemDraw *item, gdouble x1, gdouble x2, gdouble y1, gdouble y2);

#define GTK_CAIRO_PLOT_ITEM_TITLE_TYPE          (gtk_cairo_plot_item_title_get_type ())
#define GTK_CAIRO_PLOT_ITEM_TITLE(obj)          (G_TYPE_CHECK_INSTANCE_CAST(obj, GTK_CAIRO_PLOT_ITEM_TITLE_TYPE, GtkCairoPlotItemTitle))
#define GTK_CAIRO_PLOT_ITEM_TITLE_CLASS(klass)  (G_TYPE_CHECK_CLASS_CAST(klass, GTK_CAIRO_PLOT_ITEM_TITLE_TYPE, GtkCairoPlotItemTitleClass))
#define IS_GTK_CAIRO_PLOT_TITLE_DRAW(obj)       (G_TYPE_CHECK_INSTANCE_TYPE(obj, GTK_CAIRO_PLOT_ITEM_TITLE_TYPE))

/* The View */
typedef struct _GtkCairoPlotItemDraw GtkCairoPlotItemTitle;
typedef struct _GtkCairoPlotItemDrawClass GtkCairoPlotItemTitleClass;

struct _GtkCairoPlotItemTitle {
	GtkCairoPlotViewItem parent;
};

struct _GtkCairoPlotItemTitleClass {
	GtkCairoPlotViewItemClass parent_class;
};

GType gtk_cairo_plot_item_title_get_type (void);

/* Rectangle */
#define GTK_CAIRO_PLOT_ITEM_RECT_TYPE          (gtk_cairo_plot_item_rect_get_type ())
#define GTK_CAIRO_PLOT_ITEM_RECT(obj)          (G_TYPE_CHECK_INSTANCE_CAST(obj, GTK_CAIRO_PLOT_ITEM_RECT_TYPE, GtkCairoPlotItemRect))
#define GTK_CAIRO_PLOT_ITEM_RECT_CLASS(klass)  (G_TYPE_CHECK_CLASS_CAST(klass, GTK_CAIRO_PLOT_ITEM_RECT_TYPE, GtkCairoPlotItemRectClass))
#define IS_GTK_CAIRO_PLOT_ITEM_RECT(obj)       (G_TYPE_CHECK_INSTANCE_TYPE(obj, GTK_CAIRO_PLOT_ITEM_RECT_TYPE))

typedef struct _GtkCairoPlotItemRect GtkCairoPlotItemRect;
typedef struct _GtkCairoPlotItemRectClass GtkCairoPlotItemRectClass;

struct _GtkCairoPlotItemRect {
	GtkCairoPlotViewItem parent;

	/* box coordinates */
	gdouble x1, x2;
	gdouble y1, y2;
};

struct _GtkCairoPlotItemRectClass {
	GtkCairoPlotViewItemClass parent_class;
};

GType gtk_cairo_plot_item_rect_get_type (void);

#endif

