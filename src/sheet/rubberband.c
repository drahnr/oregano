/*
 * rubberband.c
 *
 *
 * Authors:
 *  Richard Hult <rhult@hem.passagen.se>
 *  Ricardo Markiewicz <rmarkie@fi.uba.ar>
 *  Andres de Barbara <adebarbara@fi.uba.ar>
 *  Marc Lorber <lorber.marc@wanadoo.fr>
 *  Bernhard Schuster <bernhard@ahoi.io>
 *  Guido Trentalancia <guido@trentalancia.com>
 *
 * Description: Handles the user interaction when doing area/rubberband
 *selections.
 *
 * Web page: https://ahoi.io/project/oregano
 *
 * Copyright (C) 1999-2001  Richard Hult
 * Copyright (C) 2003,2006  Ricardo Markiewicz
 * Copyright (C) 2009-2012  Marc Lorber
 * Copyright (C) 2013       Bernhard Schuster
 * Copyright (C) 2017       Guido Trentalancia
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
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include <math.h>

#include <gdk/gdkkeysyms.h>

#include "rubberband.h"
#include "sheet-private.h"

#include "debug.h"

inline static cairo_pattern_t *create_stipple (const char *color_name, guchar stipple_data[])
{
	cairo_surface_t *surface;
	cairo_pattern_t *pattern;
	GdkRGBA color;
	int stride;
	const int width = 8;
	const int height = 8;

	gdk_rgba_parse (&color, color_name);
	/*	stipple_data[2] = stipple_data[14] = color.red >> 8;
	        stipple_data[1] = stipple_data[13] = color.green >> 8;
	        stipple_data[0] = stipple_data[12] = color.blue >> 8;
	*/
	stride = cairo_format_stride_for_width (CAIRO_FORMAT_ARGB32, width);
	g_assert (stride > 0);
	NG_DEBUG ("stride = %i", stride);
	surface = cairo_image_surface_create_for_data (stipple_data, CAIRO_FORMAT_ARGB32, width, height,
	                                               stride);
	pattern = cairo_pattern_create_for_surface (surface);
	cairo_surface_destroy (surface);
	cairo_pattern_set_extend (pattern, CAIRO_EXTEND_REPEAT);

	return pattern;
}

#define COLOR_A 0x3093BA52
#define COLOR_B 0x30FFFFFF
#define PREMULTIPLY(argb)                                                                          \
	((argb & 0xFF << 24) |                                                                         \
	 ((((argb & 0xFF << 16) >> 16) * ((argb & 0xFF << 24) >> 24) / 0xFF) << 16) |                  \
	 ((((argb & 0xFF << 8) >> 8) * ((argb & 0xFF << 24) >> 24) / 0xFF) << 8) |                     \
	 ((((argb & 0xFF << 0) >> 0) * ((argb & 0xFF << 24) >> 24) / 0xFF) << 0))
#define COLOR_A_PRE PREMULTIPLY (COLOR_A)
#define COLOR_B_PRE PREMULTIPLY (COLOR_B)

RubberbandInfo *rubberband_info_new (Sheet *sheet)
{
	RubberbandInfo *rubberband_info;
	cairo_pattern_t *pattern;
	cairo_matrix_t matrix;

	NG_DEBUG ("0x%x A", COLOR_A);
	NG_DEBUG ("0x%x B", COLOR_B);
	NG_DEBUG ("0x%x A PRE", COLOR_A_PRE);
	NG_DEBUG ("0x%x B PRE", COLOR_B_PRE);
	static guint32 stipple_data[8 * 8] = {
	    COLOR_A_PRE, COLOR_A_PRE, COLOR_A_PRE, COLOR_B_PRE, COLOR_B_PRE, COLOR_B_PRE, COLOR_B_PRE,
	    COLOR_A_PRE, COLOR_A_PRE, COLOR_A_PRE, COLOR_B_PRE, COLOR_B_PRE, COLOR_B_PRE, COLOR_B_PRE,
	    COLOR_A_PRE, COLOR_A_PRE, COLOR_A_PRE, COLOR_B_PRE, COLOR_B_PRE, COLOR_B_PRE, COLOR_B_PRE,
	    COLOR_A_PRE, COLOR_A_PRE, COLOR_A_PRE, COLOR_B_PRE, COLOR_B_PRE, COLOR_B_PRE, COLOR_B_PRE,
	    COLOR_A_PRE, COLOR_A_PRE, COLOR_A_PRE, COLOR_A_PRE,

	    COLOR_B_PRE, COLOR_B_PRE, COLOR_B_PRE, COLOR_A_PRE, COLOR_A_PRE, COLOR_A_PRE, COLOR_A_PRE,
	    COLOR_B_PRE, COLOR_B_PRE, COLOR_B_PRE, COLOR_A_PRE, COLOR_A_PRE, COLOR_A_PRE, COLOR_A_PRE,
	    COLOR_B_PRE, COLOR_B_PRE, COLOR_B_PRE, COLOR_A_PRE, COLOR_A_PRE, COLOR_A_PRE, COLOR_A_PRE,
	    COLOR_B_PRE, COLOR_B_PRE, COLOR_B_PRE, COLOR_A_PRE, COLOR_A_PRE, COLOR_A_PRE, COLOR_A_PRE,
	    COLOR_B_PRE, COLOR_B_PRE, COLOR_B_PRE, COLOR_B_PRE};

	/* the stipple patten should look like that
	 *	    1 1 1 0  0 0 0 1
	 *	    1 1 0 0  0 0 1 1
	 *	    1 0 0 0  0 1 1 1
	 *	    0 0 0 0  1 1 1 1
	 *
	 *	    0 0 0 1  1 1 1 0
	 *	    0 0 1 1  1 1 0 0
	 *	    0 1 1 1  1 0 0 0
	 *	    1 1 1 1  0 0 0 0
	 */
	rubberband_info = g_new (RubberbandInfo, 1);
	rubberband_info->state = RUBBERBAND_START;

	pattern = create_stipple ("lightgrey", (guchar *)stipple_data);

	// scale 5x, see
	// http://cairographics.org/manual/cairo-cairo-pattern-t.html#cairo-pattern-t
	cairo_matrix_init_scale (&matrix, 1.0, 1.0);
	cairo_pattern_set_matrix (pattern, &matrix);

	rubberband_info->rectangle = GOO_CANVAS_RECT (goo_canvas_rect_new (
	    GOO_CANVAS_ITEM (sheet->object_group), 10.0, 10.0, 10.0, 10.0, "stroke-color", "black",
	    "line-width", 0.2, "fill-pattern", pattern, "visibility", GOO_CANVAS_ITEM_INVISIBLE, NULL));
	cairo_pattern_destroy (pattern);
	return rubberband_info;
}

void rubberband_info_destroy (RubberbandInfo *rubberband_info)
{
	g_return_if_fail (rubberband_info != NULL);
	goo_canvas_item_remove (GOO_CANVAS_ITEM (rubberband_info->rectangle));
	g_free (rubberband_info);
}

gboolean rubberband_start (Sheet *sheet, GdkEvent *event)
{
	GList *list;
	double x, y;
	RubberbandInfo *rubberband_info;

	g_assert (event->type == GDK_BUTTON_PRESS);
	x = event->button.x;
	y = event->button.y;
	goo_canvas_convert_from_pixels (GOO_CANVAS (sheet), &x, &y);

	rubberband_info = sheet->priv->rubberband_info;
	rubberband_info->start.x = x;
	rubberband_info->start.y = y;
	rubberband_info->end.x = x;
	rubberband_info->end.y = y;
	rubberband_info->state = RUBBERBAND_ACTIVE;

	// FIXME TODO recheck
	g_assert (rubberband_info->rectangle != NULL);
	g_object_set (rubberband_info->rectangle, "x", x, "y", y, "width", 0., "height", 0.,
	              "visibility", GOO_CANVAS_ITEM_VISIBLE, NULL);
#if 1
	// Mark all the selected objects to preserve their selected state
	// if SHIFT is pressed while rubberbanding.
	if (event->button.state & GDK_SHIFT_MASK) {
		for (list = sheet->priv->selected_objects; list; list = list->next)
			sheet_item_set_preserve_selection (SHEET_ITEM (list->data), TRUE);

		sheet->priv->preserve_selection_items = g_list_copy (sheet->priv->selected_objects);
	}
#endif

	sheet_pointer_grab (sheet, event);
	return TRUE;
}

gboolean rubberband_update (Sheet *sheet, GdkEvent *event)
{
	GList *iter;
	Coords cur, cmin, cmax;
	double dx, dy; // TODO maybe keep track of subpixel changes, make em
	               // global/part of the rubberband_info struct and reset on
	               // finish
	double width, height, width_ng, height_ng;
	RubberbandInfo *rubberband_info;

	rubberband_info = sheet->priv->rubberband_info;

	g_assert (event->type == GDK_MOTION_NOTIFY);
	cur.x = event->motion.x;
	cur.y = event->motion.y;
	goo_canvas_convert_from_pixels (GOO_CANVAS (sheet), &cur.x, &cur.y);

	width = fabs (rubberband_info->end.x - rubberband_info->start.x);
	height = fabs (rubberband_info->end.y - rubberband_info->start.y);

	width_ng = fabs (cur.x - rubberband_info->start.x);
	height_ng = fabs (cur.y - rubberband_info->start.y);

	dx = fabs (width_ng - width);
	dy = fabs (height_ng - height);
	NG_DEBUG ("motion :: dx=%lf, dy=%lf :: x=%lf, y=%lf :: w_ng=%lf, h_ng=%lf", dx, dy, cur.x,
	          cur.y, width_ng, height_ng);

	// TODO FIXME scroll window if needed (use
	// http://developer.gnome.org/goocanvas/stable/GooCanvas.html#goo-canvas-scroll-to)

	if (dx > 0.1 || dy > 0.1) { // a 0.1 change in pixel coords would be the least
		                        // visible, silently ignore everything else
		rubberband_info->end.x = cur.x;
		rubberband_info->end.y = cur.y;
		cmin.x = MIN (rubberband_info->start.x, rubberband_info->end.x);
		cmin.y = MIN (rubberband_info->start.y, rubberband_info->end.y);
		cmax.x = cmin.x + width_ng;
		cmax.y = cmin.y + height_ng;
#if 1
		for (iter = sheet->priv->items; iter; iter = iter->next) {
			sheet_item_select_in_area (iter->data, &cmin, &cmax);
		}
#endif

		g_object_set (GOO_CANVAS_ITEM (rubberband_info->rectangle), "x", cmin.x, "y", cmin.y,
		              "width", width_ng, "height", height_ng, "visibility", GOO_CANVAS_ITEM_VISIBLE,
		              NULL);
		goo_canvas_item_raise (GOO_CANVAS_ITEM (rubberband_info->rectangle), NULL);
	}
	return TRUE;
}

gboolean rubberband_finish (Sheet *sheet, GdkEvent *event)
{
	RubberbandInfo *rubberband_info;

	rubberband_info = sheet->priv->rubberband_info;
#if 1
	GList *iter = NULL;
	if (sheet->priv->preserve_selection_items) {
		for (iter = sheet->priv->preserve_selection_items; iter; iter = iter->next)
			sheet_item_set_preserve_selection (SHEET_ITEM (iter->data), FALSE);

		g_list_free (sheet->priv->preserve_selection_items);
		sheet->priv->preserve_selection_items = NULL;
	}
#endif

	sheet_pointer_ungrab (sheet, event);

	g_object_set (rubberband_info->rectangle, "visibility", GOO_CANVAS_ITEM_INVISIBLE, NULL);

	rubberband_info->state = RUBBERBAND_START;
	return TRUE;
}
