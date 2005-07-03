/*
 * gtkcairoplotitems.c
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
#include "gtkcairoplotitems.h"

/* Fill cairo surface with transparent color */
static void cr_clear(cairo_t *cr, guint w, guint h)
{
	return;
	cairo_save (cr);
		cairo_set_operator (cr, CAIRO_OPERATOR_SRC);
		cairo_set_rgb_color (cr, 1, 1, 1);
		cairo_set_alpha (cr, 0);
		cairo_rectangle (cr, 0, 0, w, h);
		cairo_fill (cr);
		cairo_stroke (cr);
	cairo_restore (cr);
}

/* Buscar una manera de hacerlo mas "lindo" */
typedef struct _rgb_ {
	guint r, g, b;
} RGB;

RGB func_colors[] = {
	{0, 0, 0},
	{0,0,1},
	{1,0,1},
	{0.5,1,0.5},
	{0,1,0}
};

#define NUMCOLORS (sizeof(func_colors)/sizeof(RGB))
#define DEGREE_TO_RADIANS 0.017453293

static void
get_order_of_magnitude (double val, double *man, double *pw)
{
	gdouble b;
	gdouble sx;

	b = (val != 0.0 ? log10 (fabs (val)) / 3.0 : 0.0);
	b  = 3.0 * rint (b);
	sx = (b != 0.0 ? pow (10.0, b) : 1.0);
	*man = val / sx;
	*pw  = b;
}

static void gtk_cairo_plot_item_draw_class_init (GtkCairoPlotItemDrawClass *class);
static void gtk_cairo_plot_item_draw_init (GtkCairoPlotItemDraw *view);
static void gtk_cairo_plot_item_draw_update (GtkCairoPlotViewItem *item, GtkCairoPlotModel *model, cairo_t *cr);
static void gtk_cairo_plot_item_draw_get_point (GtkCairoPlotViewItem *item, GtkCairoPlotModel *model, gdouble x, gdouble y, gdouble *x1, gdouble *y1);

static GtkCairoPlotViewItem *draw_parent_class;

GType
gtk_cairo_plot_item_draw_get_type (void)
{
  static GType view_type = 0;
  
  if (!view_type) {
	  static const GTypeInfo view_info = {
		  sizeof (GtkCairoPlotItemDrawClass),
			NULL,
			NULL,
		  (GClassInitFunc) gtk_cairo_plot_item_draw_class_init,
			NULL,
			NULL,
		  sizeof (GtkCairoPlotItemDraw),
			0,
		  (GInstanceInitFunc) gtk_cairo_plot_item_draw_init,
		  NULL
	  };
	 
	  view_type = g_type_register_static (
			GTK_CAIRO_PLOT_VIEW_ITEM_TYPE,
			"GtkCairoPlotItemDraw", &view_info, 0);
  }

	return view_type;
}

static void 
gtk_cairo_plot_item_draw_class_init (GtkCairoPlotItemDrawClass *class)
{
	GObjectClass *object_class;
	GtkCairoPlotViewItemClass *item_class;

	object_class = G_OBJECT_CLASS (class);
	item_class = GTK_CAIRO_PLOT_VIEW_ITEM_CLASS (class);

	draw_parent_class = g_type_class_peek_parent (class);

	item_class->update = gtk_cairo_plot_item_draw_update;
	item_class->get_point = gtk_cairo_plot_item_draw_get_point;
}

static void
gtk_cairo_plot_item_draw_init (GtkCairoPlotItemDraw *item)
{
}

void
gtk_cairo_plot_item_draw_set_scroll_region (GtkCairoPlotItemDraw *item, gdouble x1, gdouble x2, gdouble y1, gdouble y2)
{
	g_return_if_fail (IS_GTK_CAIRO_PLOT_ITEM_DRAW (item));

	item->x1 = x1;
	item->x2 = x2,
	item->y1 = y1;
	item->y2 = y2;
}

static void 
gtk_cairo_plot_item_draw_get_point (GtkCairoPlotViewItem *item, GtkCairoPlotModel *model, gdouble x, gdouble y, gdouble *x1, gdouble *y1)
{
	GtkCairoPlotItemDraw *draw;
	guint offset_x, offset_y;
	guint w, h;
	guint plot_w, plot_h;
	gdouble xmin, xmax;
	gdouble ymin, ymax;
	gdouble aX, bX, aY, bY;

	draw = GTK_CAIRO_PLOT_ITEM_DRAW (item);

	gtk_cairo_plot_model_get_x_minmax (model, &xmin, &xmax);
	gtk_cairo_plot_model_get_y_minmax (model, &ymin, &ymax);

	g_object_get (
		G_OBJECT (item), 
		"width", &w, 
		"height", &h, 
		NULL);

	offset_x = 50;
	offset_y = 50;
	plot_w = w-2*offset_x;
	plot_h = h-2*offset_y;

	/* Calculate Window-ViewPort Transformation coef */
	aX = plot_w / (draw->x2 - draw->x1);
	bX = -aX * draw->x1;
	aY = (0.0 - plot_h) / (draw->y2 - draw->y1);
	bY = -aY * draw->y1 + plot_h;

	/* xv = a*xw + b + 50 */
	/* xw = (xv - 50 - b)/a */
	*x1 = (x - 50 - bX)/aX;
	*y1 = (y - 50 - bY)/aY;
}

#define X(x) (offset_x + (aX * x + bX))
#define Y(y) (offset_y + (aY * y + bY))

/* Main Drawing widget!!
 *
 * TODO : some important things 
 *   * Clipping
 *   * Modularize axis drawing!
 *
 */
static void
gtk_cairo_plot_item_draw_update (GtkCairoPlotViewItem *item, GtkCairoPlotModel *model, cairo_t *cr)
{
	GtkCairoPlotItemDraw *draw;
	int i;
	guint points;
	Point *p;
	gdouble xmin, xmax;
	gdouble ymin, ymax;
	guint w, h, cant_y_vals;
	guint plot_w, plot_h;
	guint offset_x, offset_y;
	cairo_text_extents_t extend;
	gdouble y, paso, x;
	GList *funcs, *node;
	cairo_surface_t *surface;
	guint current_color;
	gdouble manx, expx, sx;
	gdouble many, expy, sy;
	gchar *txt;
	gboolean logx;
	/* Window-View Port Transformation variables */
	gdouble aX, bX, aY, bY, xaxis_y, yaxis_x;

	draw = GTK_CAIRO_PLOT_ITEM_DRAW (item);

	funcs = gtk_cairo_plot_model_get_functions (model);
	g_object_get (G_OBJECT (model), "logx", &logx, NULL);
	
	g_object_get (
		G_OBJECT (item), 
		"width", &w, 
		"height", &h, 
		"surface", &surface, 
		NULL);

	if (funcs == NULL) {
		return;
	}

	offset_x = 50;
	offset_y = 50;
	plot_w = w-2*offset_x;
	plot_h = h-2*offset_y;

	gtk_cairo_plot_model_get_x_minmax (model, &xmin, &xmax);
	gtk_cairo_plot_model_get_y_minmax (model, &ymin, &ymax);

	/* Calculate Window-ViewPort Transformation coef */
	aX = plot_w / (draw->x2 - draw->x1);
	bX = -aX * draw->x1;
	aY = (0.0 - plot_h) / (draw->y2 - draw->y1);
	bY = -aY * draw->y1 + plot_h;

	get_order_of_magnitude((logx)?pow(10,xmax):xmax, &manx, &expx);
	if (expx != 0.0) {
		sx = pow(10.0, expx);
		// Label para la unidad
		//g_print ("%.1e\n", pow(10.0, expx));
	} else {
		sx = 1.0;
	}
	get_order_of_magnitude(ymax, &many, &expy);
	if (expy != 0.0) {
		sy = pow(10.0, expy);
		// Label para la unidad
		//g_print ("%.1e\n", pow(10.0, expx));
	} else {
		sy = 1.0;
	}

	cairo_save (cr);

	/* Y Axis */
	yaxis_x = 0;
	if (xmin > 0)
		yaxis_x = xmin;
	if (xmax < 0)
		yaxis_x = xmax;

	cairo_set_rgb_color (cr, 1, 0, 0);
	cairo_move_to (cr, X(yaxis_x), Y(ymin));
	cairo_line_to (cr, X(yaxis_x), Y(ymax));
	cairo_stroke (cr);
	cairo_set_rgb_color (cr, 0, 0, 0);
		
	cairo_select_font (cr, "sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
	cairo_scale_font (cr, 10);

	cairo_text_extents (cr, "1234567890", &extend);
	cant_y_vals = 10;

	paso = (ymax - ymin)/cant_y_vals;

	/* Prevent infinite loop */
	if (paso < 1e-2) paso = 1e-2;

	/* We draw from 0 to YMax and from 0 to YMin, to be sure
	 * that 0 is draw and is in the center
	 */

	/* Draw postives values */
	for(y=0; y <= ceil(ymax); y+=paso) {
		txt = g_strdup_printf ("%.2f", y);
		cairo_text_extents (cr, txt, &extend);

		cairo_move_to (cr, X(yaxis_x) - extend.width-6, Y(y));
		cairo_show_text (cr, txt);

		cairo_save (cr);
			cairo_set_line_width (cr, 0.5);
			cairo_move_to (cr, X(yaxis_x)-5, Y(y));
			cairo_line_to (cr, X(yaxis_x)+5, Y(y));
			cairo_stroke (cr);
		cairo_restore (cr);
		g_free (txt);
	}
	/* Draw negatives values */
	for(y=0; y >= floor(ymin); y-=paso) {
		txt = g_strdup_printf ("%.2f", y);
		cairo_text_extents (cr, txt, &extend);

		cairo_move_to (cr, X(yaxis_x) - extend.width-6, Y(y));
		cairo_show_text (cr, txt);

		cairo_save (cr);
			cairo_set_line_width (cr, 0.5);
			cairo_move_to (cr, X(yaxis_x)-5, Y(y));
			cairo_line_to (cr, X(yaxis_x)+5, Y(y));
			cairo_stroke (cr);
		cairo_restore (cr);
		cairo_stroke (cr);
		g_free (txt);
	}
	g_object_get (G_OBJECT (model), "y-unit", &txt, NULL);
	if (txt) {
		char *y_title;
		if (fabs(sy) != 1.0) {
			y_title = g_strdup_printf ("%.2e %s", sy, txt);
		} else {
			y_title = g_strdup_printf ("%s", txt);
		}
		cairo_text_extents (cr, y_title, &extend);

		cairo_save (cr);
			cairo_move_to (cr, extend.height*1.3, h/2+extend.width/2);
			cairo_rotate (cr, -90*DEGREE_TO_RADIANS);
			cairo_show_text (cr, y_title);
		cairo_restore (cr);

		g_free (y_title);
		g_free (txt);
	}

	/* X Axis */
	xaxis_y = 0;
	if (ymin > 0)
		xaxis_y = ymin;
	if (ymax < 0)
		xaxis_y = ymax;

	cairo_set_rgb_color (cr, 1, 0, 0);
	cairo_move_to (cr, X(xmin), Y(xaxis_y));
	cairo_line_to (cr, X(xmax), Y(xaxis_y));
	cairo_stroke (cr);
	
	paso = (xmax - xmin)/cant_y_vals;

	/* Prevent infinite loop */
	if (paso < 1e-8) paso = 1e-8;

	cairo_set_rgb_color (cr, 0, 0, 0);
	for (x=xmin; x <= xmax; x+=paso) {
		gdouble xreal;

		xreal = (logx)?pow(10,x):(x);
		if (xreal < 9e3)
			txt = g_strdup_printf ("%.3f", xreal);
		else
			txt = g_strdup_printf ("%.1e", xreal);
		cairo_save (cr);
			cairo_move_to (cr, X(x), Y(xaxis_y)+7);
			cairo_rotate (cr, 45*DEGREE_TO_RADIANS);
			cairo_show_text (cr, txt);
		cairo_restore (cr);
		g_free (txt);

		cairo_save (cr);
			cairo_set_line_width (cr, 0.5);
			cairo_move_to (cr, X(x), Y(xaxis_y)-5);
			cairo_line_to (cr, X(x), Y(xaxis_y)+5);
			cairo_stroke (cr);
		cairo_restore (cr);
	}
	g_object_get (G_OBJECT (model), "x-unit", &txt, NULL);
	if (txt) {
		char *x_title;
		if (sx != 1.0) {
			x_title = g_strdup_printf ("%.2e %s", sx, txt);
		} else {
			x_title = g_strdup_printf ("%s", txt);
		}
		cairo_text_extents (cr, x_title, &extend);

		cairo_save (cr);
			cairo_move_to (cr, w/2-extend.width, h-extend.height);
			cairo_show_text (cr, x_title);
		cairo_restore (cr);

		g_free (x_title);
		g_free (txt);
	}

	cairo_save (cr); /* Draw Lines */
	cairo_set_rgb_color (cr, 0, 0, 0);
	cairo_set_alpha (cr, 1);

	current_color = 0;
	for (node = funcs; node; node = node->next) {
		if (current_color == NUMCOLORS) {
			current_color = 0;
		}
	
		cairo_save (cr);
		cairo_set_rgb_color (cr, 
			func_colors[current_color].r,
			func_colors[current_color].g,
			func_colors[current_color].b
		);
		current_color++;

		points = gtk_cairo_plot_model_get_num_points (model, (gchar *)node->data);
		p = gtk_cairo_plot_model_get_point (model, (gchar *)node->data, 0);
		cairo_move_to (cr, X(p->x), Y(p->y));
		for(i=1; i<points; i++) {
			p = gtk_cairo_plot_model_get_point (model, (gchar *)node->data, i);
			cairo_line_to (cr, X(p->x), Y(p->y));
		}
		cairo_stroke (cr);
		cairo_restore (cr);
	}
	cairo_restore (cr); /* Draw Lines */
}

#undef X
#undef Y

/* ========== Title Item ========== */
static void gtk_cairo_plot_item_title_class_init (GtkCairoPlotItemTitleClass *class);
static void gtk_cairo_plot_item_title_init (GtkCairoPlotItemTitle *view);
static void gtk_cairo_plot_item_title_update (GtkCairoPlotViewItem *item, GtkCairoPlotModel *model, cairo_t *cr);

static GtkCairoPlotViewItem *title_parent_class;

GType
gtk_cairo_plot_item_title_get_type (void)
{
  static GType view_type = 0;
  
  if (!view_type) {
	  static const GTypeInfo view_info = {
		  sizeof (GtkCairoPlotItemTitleClass),
			NULL,
			NULL,
		  (GClassInitFunc) gtk_cairo_plot_item_title_class_init,
			NULL,
			NULL,
		  sizeof (GtkCairoPlotItemTitle),
			0,
		  (GInstanceInitFunc) gtk_cairo_plot_item_title_init,
		  NULL
	  };
	 
	  view_type = g_type_register_static (
			GTK_CAIRO_PLOT_VIEW_ITEM_TYPE,
			"GtkCairoPlotItemTitle", &view_info, 0);
  }

	return view_type;
}

static void 
gtk_cairo_plot_item_title_class_init (GtkCairoPlotItemTitleClass *class)
{
	GObjectClass *object_class;
	GtkCairoPlotViewItemClass *item_class;

	object_class = G_OBJECT_CLASS (class);
	item_class = GTK_CAIRO_PLOT_VIEW_ITEM_CLASS (class);

	title_parent_class = g_type_class_peek_parent (class);

	item_class->update = gtk_cairo_plot_item_title_update;
	item_class->get_point = NULL;
}

static void
gtk_cairo_plot_item_title_init (GtkCairoPlotItemTitle *item)
{
}

static void
gtk_cairo_plot_item_title_update (GtkCairoPlotViewItem *item, GtkCairoPlotModel *model, cairo_t *cr)
{
	char *title;
	cairo_text_extents_t extend;
	cairo_surface_t *surface;
	guint w, h;

	g_object_get (G_OBJECT (item), "width", &w, "height", &h, "surface", &surface, NULL);

	cr_clear (cr, w, h);

	g_object_get (G_OBJECT (model), "title", &title, NULL);
	if (title) {
		cairo_save (cr);
			cairo_set_rgb_color (cr, 0, 0, 0);
			cairo_select_font (cr, "sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
			cairo_scale_font (cr, 18);
	
			cairo_text_extents (cr, "1234567890", &extend);

			cairo_translate (cr, 0, extend.height);
			cairo_show_text (cr, title);
			cairo_stroke (cr);
		cairo_restore (cr);

		g_free (title);
	}

	cairo_stroke (cr);
}


/* ==== Rectangle ==== */
static void gtk_cairo_plot_item_rect_class_init (GtkCairoPlotItemRectClass *class);
static void gtk_cairo_plot_item_rect_init (GtkCairoPlotItemRect *view);
static void gtk_cairo_plot_item_rect_update (GtkCairoPlotViewItem *item, GtkCairoPlotModel *model, cairo_t *cr);

static GtkCairoPlotViewItem *rect_parent_class;

GType
gtk_cairo_plot_item_rect_get_type (void)
{
  static GType rect_type = 0;
  
  if (!rect_type) {
	  static const GTypeInfo view_info = {
		  sizeof (GtkCairoPlotItemRectClass),
			NULL,
			NULL,
		  (GClassInitFunc) gtk_cairo_plot_item_rect_class_init,
			NULL,
			NULL,
		  sizeof (GtkCairoPlotItemRect),
			0,
		  (GInstanceInitFunc) gtk_cairo_plot_item_rect_init,
		  NULL
	  };
	 
	  rect_type = g_type_register_static (
			GTK_CAIRO_PLOT_VIEW_ITEM_TYPE,
			"GtkCairoPlotItemRect", &view_info, 0);
  }

	return rect_type;
}

static void 
gtk_cairo_plot_item_rect_class_init (GtkCairoPlotItemRectClass *class)
{
	GObjectClass *object_class;
	GtkCairoPlotViewItemClass *item_class;

	object_class = G_OBJECT_CLASS (class);
	item_class = GTK_CAIRO_PLOT_VIEW_ITEM_CLASS (class);

	draw_parent_class = g_type_class_peek_parent (class);

	item_class->update = gtk_cairo_plot_item_rect_update;
}

static void
gtk_cairo_plot_item_rect_init (GtkCairoPlotItemRect *item)
{
}

static void
gtk_cairo_plot_item_rect_update (GtkCairoPlotViewItem *item, GtkCairoPlotModel *model, cairo_t *cr)
{
	cairo_surface_t *surface;
	guint w, h;
	
	g_object_get (G_OBJECT (item), "width", &w, "height", &h, "surface", &surface, NULL);

	/* Redimensionar (es decir, eliminar y volver a crear) la surface para
	 * este item no es util. Hay que reveer como hacerlo de mejor manera
	 */
	cairo_set_rgb_color (cr, 0, 0, 0);
	cairo_rectangle (cr, 0, 0, w, h);
	cairo_save (cr);
		cairo_set_alpha (cr, 0.2);
		cairo_set_rgb_color (cr, 0.2, 0.7, 0.2);
		cairo_fill (cr);
	cairo_restore (cr);
	cairo_stroke (cr);
}

