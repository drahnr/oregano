/*
 * gtkcairoplotview.c
 *
 * 
 * Author: 
 *  Ricardo Markiewicz <rmarkie@fi.uba.ar>
 * 
 *  http://www.fi.uba.ar/~rmarkie/
 * 
 * Copyright (C) 2004-2005  Ricardo Markiewicz
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

#include <math.h>
#include <cairo.h>
#include <gdk/gdkx.h>
#include "gtkcairoplotview.h"
#include <stdio.h>

enum {
	ARG_0,
};

struct _GtkCairoPlotViewPriv {
	GList *items;
	gint width, height;
	GtkCairoPlotViewItem *attached;
	gboolean scaled;
};

static void gtk_cairo_plot_view_class_init (GtkCairoPlotViewClass *class);
static void gtk_cairo_plot_view_init (GtkCairoPlotView *view);
static void gtk_cairo_plot_view_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *spec);
static void gtk_cairo_plot_view_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *spec);
static void gtk_cairo_plot_view_generic_draw (GtkCairoPlotView *view, GtkCairoPlotModel *model, cairo_t *cr);

static GObjectClass *parent_class;

GType
gtk_cairo_plot_view_get_type (void)
{
  static GType view_type = 0;
  
  if (!view_type) {
	  static const GTypeInfo view_info = {
		  sizeof (GtkCairoPlotViewClass),
			NULL,
			NULL,
		  (GClassInitFunc) gtk_cairo_plot_view_class_init,
			NULL,
			NULL,
		  sizeof (GtkCairoPlotView),
			0,
		  (GInstanceInitFunc) gtk_cairo_plot_view_init,
		  NULL
	  };
	 
	  view_type = g_type_register_static (G_TYPE_OBJECT, "GtkCairoPlotView", &view_info, 0);
  }

	return view_type;
}

static void 
gtk_cairo_plot_view_class_init (GtkCairoPlotViewClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	
	parent_class = g_type_class_peek_parent (class);
	
	object_class->set_property = gtk_cairo_plot_view_set_property;
	object_class->get_property = gtk_cairo_plot_view_get_property;

/*	g_object_class_install_property(
		object_class,
		ARG_TITLE,
		g_param_spec_string("title", "GtkCairoPlorModel::title", "the title", "Title", G_PARAM_READWRITE)
	);
	g_object_class_install_property(
		object_class,
		ARG_X_TITLE,
		g_param_spec_string("x-title", "GtkCairoPlorModel::x-title", "x axis title", "X axis Title", G_PARAM_WRITABLE)
	);
	g_object_class_install_property(
		object_class,
		ARG_Y_TITLE,
		g_param_spec_string("y-title", "GtkCairoPlorModel::y-title", "y axis title", "Y axis title", G_PARAM_WRITABLE)
	);

	g_object_class_install_property(
			object_class,
			ARG_SPACING,
			g_param_spec_double("spacing", "Grid::spacing", "the grid spacing", 0.0f, 100.0f, 10.0f, G_PARAM_READWRITE)
	);
	g_object_class_install_property(
			object_class,
			ARG_SNAP,
			g_param_spec_boolean("snap", "Grid::snap", "snap to grid?", TRUE, G_PARAM_READWRITE)
	);
*/	
}

static void
gtk_cairo_plot_view_init (GtkCairoPlotView *view)
{
	GtkCairoPlotViewPriv *priv;

	priv = g_new0 (GtkCairoPlotViewPriv, 1);

	view->priv = priv;

	/* Init private data */
	view->in_zoom = FALSE;
	priv->items = NULL;
	priv->attached = NULL;
	view->priv->width = 300;
	view->priv->height = 300;
}

static void
gtk_cairo_plot_view_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *spec)
{
	GtkCairoPlotView *view;
	GtkCairoPlotViewPriv *priv;

	view = GTK_CAIRO_PLOT_VIEW (object);
	priv = view->priv;
	
/*	switch (prop_id){
			priv->y_title = g_strdup (g_value_get_string (value));
	}
*/
}

void
gtk_cairo_plot_view_attach (GtkCairoPlotView *view, GtkCairoPlotViewItem *item)
{
	view->priv->attached = item;
}

static void
gtk_cairo_plot_view_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *spec)
{
	GtkCairoPlotView *view;
	GtkCairoPlotViewPriv *priv;

	view = GTK_CAIRO_PLOT_VIEW (object);
	priv = view->priv;
	
/*	switch (prop_id){
		case ARG_TITLE:
			g_value_set_string (value, priv->title);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(model, prop_id, spec);
	}*/
}

void
gtk_cairo_plot_view_set_size (GtkCairoPlotView *view, int width, int height)
{
	view->priv->width = width;
	view->priv->height = height;
	if (view->priv->attached) {
		g_object_set (G_OBJECT (view->priv->attached), "width", width, "height", height, NULL);
	}
}

GtkCairoPlotView *
gtk_cairo_plot_view_new (void)
{
	GtkCairoPlotView *view;

	view = GTK_CAIRO_PLOT_VIEW (g_object_new (GTK_CAIRO_PLOT_VIEW_TYPE, NULL));

	return view;
}

void 
gtk_cairo_plot_view_set_scroll_region (GtkCairoPlotView *item, gdouble x1, gdouble x2, gdouble y1, gdouble y2)
{
	if (item->priv->attached) {
		gtk_cairo_plot_item_draw_set_scroll_region (
			item->priv->attached, x1, x2, y1, y2);
	}
}

void
gtk_cairo_plot_view_draw (GtkCairoPlotView *view, GtkCairoPlotModel *model, GdkWindow *window)
{
	cairo_t *cr;
	cairo_surface_t *target;
	GdkDrawable* real_drawable;
	Display* dpy;
	Drawable drawable;
	gint x_off, y_off;
	gint width, height;

	g_return_if_fail (IS_GTK_CAIRO_PLOT_VIEW (view));
	g_return_if_fail (IS_GTK_CAIRO_PLOT_MODEL (model));

	gdk_window_get_internal_paint_info (window, &real_drawable, &x_off, &y_off);
	dpy = gdk_x11_drawable_get_xdisplay (real_drawable);
	drawable = gdk_x11_drawable_get_xid (real_drawable);
	gdk_drawable_get_size (real_drawable, &width, &height);

	view->priv->width = width;
	view->priv->height = height;

	target = cairo_xlib_surface_create (dpy, drawable,
			gdk_x11_visual_get_xvisual (gdk_drawable_get_visual (real_drawable)),
			width,
			height);
	
	cr = cairo_create (target);

	view->priv->scaled = FALSE;
	gtk_cairo_plot_view_generic_draw (view, model, cr);
	
	cairo_destroy(cr);
}

static void 
gtk_cairo_plot_view_generic_draw (GtkCairoPlotView *view, GtkCairoPlotModel *model, cairo_t *cr)
{
	GList *it;
	GtkCairoPlotViewItem *item;
	cairo_surface_t *surface;
	guint x, y, w, h;
	guint width, height;

	g_return_if_fail (IS_GTK_CAIRO_PLOT_VIEW (view));
	g_return_if_fail (IS_GTK_CAIRO_PLOT_MODEL (model));

	cairo_save (cr);
		cairo_set_source_rgb (cr, 1, 1, 1);
		cairo_rectangle (cr, 0, 0, view->priv->width, view->priv->height);
		cairo_fill (cr);
		cairo_stroke (cr);
	cairo_restore (cr);

	it = view->priv->items;
	while (it) {
		item = it->data;

		cairo_save (cr);
			g_object_get (G_OBJECT (item), "x", &x, "y", &y,
				"width", &w, "height", &h,
				"surface", &surface, NULL);

			if (item != view->priv->attached)
				cairo_translate (cr, x, y);

			gtk_cairo_plot_view_item_update (item, model, cr);
		cairo_restore (cr);

		/* Draw item in the window */
		it = it->next;
	}
}

void gtk_cairo_plot_view_get_point (GtkCairoPlotView *view, GtkCairoPlotModel *model, gdouble x, gdouble y, gdouble *x1, gdouble *y1)
{
	g_return_if_fail (view != NULL);
	g_return_if_fail (IS_GTK_CAIRO_PLOT_VIEW (view));

	if (view->priv->attached) {
		gtk_cairo_plot_view_item_get_point (
			view->priv->attached, model, x, y, x1, y1);
	} else {
		// TODO : Search FRONT item ar (x,y) and request coords
		*x1 = x;
		*y1 = y;
	}
}

void 
gtk_cairo_plot_view_add_item (GtkCairoPlotView *view, GtkCairoPlotViewItem *item, guint pos)
{
	GtkCairoPlotViewPriv *priv;

	g_return_if_fail (IS_GTK_CAIRO_PLOT_VIEW (view));
	g_return_if_fail (IS_GTK_CAIRO_PLOT_VIEW_ITEM (item));

	priv = view->priv;

	switch (pos) {
		case ITEM_POS_FRONT:
			priv->items = g_list_append (priv->items, item);
		break;
		case ITEM_POS_BACK:
			priv->items = g_list_prepend (priv->items, item);
		break;
		default:
			g_warning ("ItemPos unknow!!\n");
	}

}

void 
gtk_cairo_plot_view_remove_item (GtkCairoPlotView *view, GtkCairoPlotViewItem *item)
{
	view->priv->items = g_list_remove (view->priv->items, item);
}


/* ========= Item ===========*/

enum {
	ARG_ITEM_0,
	ARG_ITEM_WIDTH,
	ARG_ITEM_HEIGHT,
	ARG_ITEM_POS_X,
	ARG_ITEM_POS_Y,
	ARG_ITEM_SURFACE
};

struct _GtkCairoPlotViewItemPriv {
	/* Al operations will be done here */
	cairo_surface_t *surface;
	
	/* Size */
	guint w;
	guint h;

	/* Item pos in the parent */
	guint pos_x, pos_y;

	/* Unit Sizes */
	gdouble x_min, x_max;
	gdouble y_min, y_max;

	gboolean need_update;
};

static void gtk_cairo_plot_view_item_class_init (GtkCairoPlotViewItemClass *class);
static void gtk_cairo_plot_view_item_init (GtkCairoPlotViewItem *view);
static void gtk_cairo_plot_view_item_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *spec);
static void gtk_cairo_plot_view_item_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *spec);

static GObjectClass *item_parent_class;

GType
gtk_cairo_plot_view_item_get_type (void)
{
  static GType view_type = 0;
  
  if (!view_type) {
	  static const GTypeInfo view_info = {
		  sizeof (GtkCairoPlotViewItemClass),
			NULL,
			NULL,
		  (GClassInitFunc) gtk_cairo_plot_view_item_class_init,
			NULL,
			NULL,
		  sizeof (GtkCairoPlotViewItem),
			0,
		  (GInstanceInitFunc) gtk_cairo_plot_view_item_init,
		  NULL
	  };
	 
	  view_type = g_type_register_static (G_TYPE_OBJECT, "GtkCairoPlotViewItem", &view_info, G_TYPE_FLAG_ABSTRACT);
  }

	return view_type;
}

static void 
gtk_cairo_plot_view_item_class_init (GtkCairoPlotViewItemClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	
	item_parent_class = g_type_class_peek_parent (class);
	
	object_class->set_property = gtk_cairo_plot_view_item_set_property;
	object_class->get_property = gtk_cairo_plot_view_item_get_property;

	class->update = NULL;

	g_object_class_install_property (
		object_class,
		ARG_ITEM_WIDTH,
		g_param_spec_uint (
			"width", "GtkCairoPlotViewItem::width",
			"the width", 0, G_MAXUINT, 0, G_PARAM_READWRITE)
	);
	g_object_class_install_property(
		object_class,
		ARG_ITEM_HEIGHT,
		g_param_spec_uint (
			"height", "GtkCairoPloyViewItem::height", 
			"the height", 0, G_MAXUINT, 0, G_PARAM_READWRITE)
	);
	g_object_class_install_property(
		object_class,
		ARG_ITEM_POS_X,
		g_param_spec_uint (
			"x", "GtkCairoPloyViewItem::x", 
			"the X pos", 0, G_MAXUINT, 0, G_PARAM_READWRITE)
	);
	g_object_class_install_property(
		object_class,
		ARG_ITEM_POS_Y,
		g_param_spec_uint (
			"y", "GtkCairoPloyViewItem::y", 
			"the Y pos", 0, G_MAXUINT, 0, G_PARAM_READWRITE)
	);
	g_object_class_install_property(
		object_class,
		ARG_ITEM_SURFACE,
		g_param_spec_pointer (
			"surface", "GtkCairoPloyViewItem::surface", 
			"the surface", G_PARAM_READABLE)
	);
}

static void
gtk_cairo_plot_view_item_init (GtkCairoPlotViewItem *view)
{
	GtkCairoPlotViewItemPriv *priv;

	priv = g_new0 (GtkCairoPlotViewItemPriv, 1);

	view->priv = priv;

	/* Init private stuff here */
	priv->need_update = TRUE;
}

static void
gtk_cairo_plot_view_item_set_property (
	GObject *object,
	guint prop_id,
	const GValue *value,
	GParamSpec *spec)
{
	gboolean size_change = FALSE;
	GtkCairoPlotViewItem *view;
	GtkCairoPlotViewItemPriv *priv;
	guint new_val;

	view = GTK_CAIRO_PLOT_VIEW_ITEM (object);
	priv = view->priv;
	
	switch (prop_id){
		case ARG_ITEM_WIDTH:
			new_val = g_value_get_uint (value);
			if (new_val < 10) new_val = 10;
			if (priv->w != new_val) {
				priv->w = new_val;
				size_change = TRUE;
			}
		break;
		case ARG_ITEM_HEIGHT:
			new_val = g_value_get_uint (value);
			if (new_val < 10) new_val = 10;
			if (new_val != priv->h) {
				priv->h = new_val,
				size_change = TRUE;
			}
		break;
		case ARG_ITEM_POS_X:
			priv->pos_x = g_value_get_uint (value);
		break;
		case ARG_ITEM_POS_Y:
			priv->pos_y = g_value_get_uint (value);
	}

	if (size_change) {
		cairo_surface_destroy (priv->surface);
		priv->surface = cairo_image_surface_create (CAIRO_FORMAT_RGB24, priv->w, priv->h);
		priv->need_update = TRUE;
	}
}

static void
gtk_cairo_plot_view_item_get_property (
	GObject *object, guint prop_id,
	GValue *value, GParamSpec *spec)
{
	GtkCairoPlotViewItem *view;
	GtkCairoPlotViewItemPriv *priv;

	view = GTK_CAIRO_PLOT_VIEW_ITEM (object);
	priv = view->priv;
	
	switch (prop_id){
		case ARG_ITEM_WIDTH:
			g_value_set_uint (value, priv->w);
		break;
		case ARG_ITEM_HEIGHT:
			g_value_set_uint (value, priv->h);
		break;
		case ARG_ITEM_POS_X:
			g_value_set_uint (value, priv->pos_x);
		break;
		case ARG_ITEM_POS_Y:
			g_value_set_uint (value, priv->pos_y);
		break;
		case ARG_ITEM_SURFACE:
			g_value_set_pointer (value, priv->surface);
		break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(view, prop_id, spec);
	}
}

GtkCairoPlotViewItem *
gtk_cairo_plot_view_item_new (GType type, guint x, guint y, guint w, guint h)
{
	GtkCairoPlotViewItem *view;

	view = GTK_CAIRO_PLOT_VIEW_ITEM (
		g_object_new (type,
			"width", w,
			"height", h,
			"x", x,
			"y", y,
			NULL)
		);

	view->priv->surface = cairo_image_surface_create (CAIRO_FORMAT_RGB24, w, h);
	return view;
}

void
gtk_cairo_plot_view_item_update (GtkCairoPlotViewItem *item, GtkCairoPlotModel *model, cairo_t *cr)
{
	GtkCairoPlotViewItemClass *class;

	g_return_if_fail (item != NULL);
	g_return_if_fail (IS_GTK_CAIRO_PLOT_VIEW_ITEM (item));

	/* Item is up2date!, don't redraw it */
	//if (!item->priv->need_update) return;

	/* this is buggy, fail with null exception pointer ¿?!! */
	class = GTK_CAIRO_PLOT_VIEW_ITEM_CLASS (G_OBJECT_GET_CLASS (item));
	if (class->update)
		class->update (item, model, cr);

	item->priv->need_update = TRUE;
}

void
gtk_cairo_plot_view_item_get_point (GtkCairoPlotViewItem *view, GtkCairoPlotModel *model, gdouble x, gdouble y, gdouble *x1, gdouble *y1)
{
	GtkCairoPlotViewItemClass *class;

	g_return_if_fail (view != NULL);
	g_return_if_fail (IS_GTK_CAIRO_PLOT_VIEW_ITEM (view));

	class = GTK_CAIRO_PLOT_VIEW_ITEM_CLASS (G_OBJECT_GET_CLASS (view));
	if (class->get_point)
		class->get_point (view, model, x, y, x1, y1);
	else {
		*x1 = x;
		*y1 = y;
	}
}

void 
gtk_cairo_plot_view_save (
	GtkCairoPlotView *view, GtkCairoPlotModel *model, gint w, gint h, gchar *fn, int format)
{
	/*cairo_t *cr;
	FILE *fp;
	gdouble sx, sy;*/

	/*fp = fopen (fn, "w");
	cr = cairo_create ();
	switch (format) {
		case GTK_CAIRO_PLOT_VIEW_SAVE_PNG:
			cairo_set_target_png (cr, fp, CAIRO_FORMAT_ARGB32, w, h);
			sx = w/(gdouble)view->priv->width;
			sy = h/(gdouble)view->priv->height;
		break;
		case GTK_CAIRO_PLOT_VIEW_SAVE_PS:
			cairo_set_target_ps (cr, fp, w/96.0f, h/96.0f, 300, 300);
			sx = w/(gdouble)view->priv->width;
			sy = h/(gdouble)view->priv->height;
	}
	view->priv->scaled = TRUE;
	cairo_save (cr);
		cairo_scale (cr, sx, sy);
		gtk_cairo_plot_view_generic_draw (view, model, cr);
	cairo_restore (cr);
	view->priv->scaled = FALSE;

	cairo_show_page (cr);
	cairo_destroy (cr);
	fclose(fp);
	*/
}

