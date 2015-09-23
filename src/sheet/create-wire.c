/*
 * create-wire.c
 *
 *
 * Authors:
 * Richard Hult <rhult@hem.passagen.se>
 * Ricardo Markiewicz <rmarkie@fi.uba.ar>
 * Andres de Barbara <adebarbara@fi.uba.ar>
 * Marc Lorber <lorber.marc@wanadoo.fr>
 * Bernhard Schuster <bernhard@ahoi.io>
 *
 * Description: Handles the user interaction when creating wires.
 * The name is not really right. This part handles creation of wires and
 * acts as glue between NodeStore/Wire and Sheet/WireItem.
 *
 * Web page: https://ahoi.io/project/oregano
 *
 * Copyright (C) 1999-2001  Richard Hult
 * Copyright (C) 2003,2006  Ricardo Markiewicz
 * Copyright (C) 2009-2012  Marc Lorber
 * Copyright (C) 2012-2013  Bernhard Schuster
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
#include <stdlib.h>

#include "cursors.h"
#include "coords.h"
#include "node-store.h"
#include "wire-item.h"
#include "create-wire.h"
#include "wire.h"
#include "sheet-private.h"

#include "debug.h"

CreateWireInfo *create_wire_info_new (Sheet *sheet)
{
	CreateWireInfo *create_wire_info;
	GooCanvasLineDash *dash;

	create_wire_info = g_new0 (CreateWireInfo, 1);

	create_wire_info->state = WIRE_DISABLED;

	create_wire_info->direction = WIRE_DIR_HORIZ;
	//	create_wire_info->direction_auto_toggle =
	// WIRE_DIRECTION_AUTO_TOGGLE_OFF;

	create_wire_info->points = goo_canvas_points_new (3);
	create_wire_info->points->coords[0] = 0.;
	create_wire_info->points->coords[1] = 0.;
	create_wire_info->points->coords[2] = 0.;
	create_wire_info->points->coords[3] = 0.;
	create_wire_info->points->coords[4] = 0.;
	create_wire_info->points->coords[5] = 0.;

	dash = goo_canvas_line_dash_new (2, 5.0, 5.0);

	create_wire_info->line = GOO_CANVAS_POLYLINE (goo_canvas_polyline_new (
	    GOO_CANVAS_ITEM (sheet->grid), FALSE, 0, "points", create_wire_info->points,
	    "stroke-color-rgba", 0x92BA52C3, "line-dash", dash, "line-width", 2.0, "visibility",
	    GOO_CANVAS_ITEM_INVISIBLE, NULL));
	goo_canvas_line_dash_unref (dash);

	create_wire_info->dot = GOO_CANVAS_ELLIPSE (goo_canvas_ellipse_new (
	    GOO_CANVAS_ITEM (sheet->object_group), -3.0, -3.0, 6.0, 6.0, "fill-color", "red",
	    "visibility", GOO_CANVAS_ITEM_INVISIBLE, NULL));

	return create_wire_info;
}

void create_wire_info_destroy (CreateWireInfo *create_wire_info)
{
	g_return_if_fail (create_wire_info);

	goo_canvas_item_remove (GOO_CANVAS_ITEM (create_wire_info->dot));
	goo_canvas_item_remove (GOO_CANVAS_ITEM (create_wire_info->line));
	goo_canvas_points_unref (create_wire_info->points);
	g_free (create_wire_info);
}

inline static gboolean create_wire_start (Sheet *sheet, GdkEvent *event)
{
	double x, y;
	GooCanvasPoints *points;
	GooCanvasPolyline *line;
	CreateWireInfo *create_wire_info;

	// g_signal_stop_emission_by_name (sheet, "event");

	create_wire_info = sheet->priv->create_wire_info;
#if 0
	//TODO save and restore the selection?
	sheet_select_all (sheet, FALSE);
#endif
	points = create_wire_info->points;
	line = create_wire_info->line;

	g_assert (points);
	g_assert (line);

	x = event->button.x;
	y = event->button.y;

	goo_canvas_convert_from_pixels (GOO_CANVAS (sheet), &x, &y);
	snap_to_grid (sheet->grid, &x, &y);

#if 0
	Coords p;
	sheet_get_pointer (sheet, &p.x, &p.y);
	NG_DEBUG ("diff_x=%lf; diff_y=%lf;", p.x-x, p.y-y);
#endif

	// start point
	points->coords[0] = x;
	points->coords[1] = y;
	// mid point
	points->coords[2] = x;
	points->coords[3] = y;
	// end point
	points->coords[4] = x;
	points->coords[5] = y;

	g_object_set (G_OBJECT (line), "points", points, "visibility", GOO_CANVAS_ITEM_VISIBLE, NULL);

	goo_canvas_item_raise (GOO_CANVAS_ITEM (create_wire_info->line), NULL);
	goo_canvas_item_raise (GOO_CANVAS_ITEM (create_wire_info->dot), NULL);

	create_wire_info->state = WIRE_ACTIVE;
	sheet_pointer_grab (sheet, event);
	sheet_keyboard_grab (sheet, event);
	return TRUE;
}

inline static gboolean create_wire_update (Sheet *sheet, GdkEvent *event)
{
	CreateWireInfo *create_wire_info;
	double new_x, new_y, x1, y1;
	gint32 snapped_x, snapped_y;
	Coords pos;

	Schematic *schematic;
	NodeStore *store;

	// g_signal_stop_emission_by_name (sheet, "event");

	create_wire_info = sheet->priv->create_wire_info;

	schematic = schematic_view_get_schematic_from_sheet (sheet);
	g_assert (schematic);

	store = schematic_get_store (schematic);
	g_assert (store);

	new_x = event->button.x;
	new_y = event->button.y;

	goo_canvas_convert_from_pixels (GOO_CANVAS (sheet), &new_x, &new_y);
	snap_to_grid (sheet->grid, &new_x, &new_y);

#if 0
	Coords p;
	sheet_get_pointer (sheet, &p.x, &p.y);
	NG_DEBUG ("diff_x=%lf; diff_y=%lf;", p.x-new_x, p.y-new_y);
#endif

	snapped_x = (gint32)new_x;
	snapped_y = (gint32)new_y;

	/* start pos (do not update,
	 * was fixed in _start func and will not change
	 * until _discard or _fixate)
	 */
	x1 = create_wire_info->points->coords[0];
	y1 = create_wire_info->points->coords[1];

	// mid pos
	if (create_wire_info->direction == WIRE_DIR_VERT) {
		create_wire_info->points->coords[2] = x1;
		create_wire_info->points->coords[3] = snapped_y;
	} else if (create_wire_info->direction == WIRE_DIR_HORIZ) {
		create_wire_info->points->coords[2] = snapped_x;
		create_wire_info->points->coords[3] = y1;
	} else {
		create_wire_info->points->coords[2] = x1;
		create_wire_info->points->coords[3] = y1;
	}
	NG_DEBUG ("update ~._.~ start=(%lf,%lf) → end=(%i,%i)", x1, y1, snapped_x, snapped_y);
	// end pos
	create_wire_info->points->coords[4] = snapped_x;
	create_wire_info->points->coords[5] = snapped_y;

	g_assert (create_wire_info->line);
	// required to trigger goocanvas update of the line object
	g_object_set (G_OBJECT (create_wire_info->line), "points", create_wire_info->points, NULL);
	pos.x = (gdouble)snapped_x;
	pos.y = (gdouble)snapped_y;
	/* Check if the pre-wire intersect another wire, and
	 * draw a small red circle to indicate the connection
	 */
	g_assert (create_wire_info->dot);

	const guint8 is_pin = node_store_is_pin_at_pos (store, pos);
	const guint8 is_wire = node_store_is_wire_at_pos (store, pos);

	if (is_pin || is_wire) {
		g_object_set (G_OBJECT (create_wire_info->dot), "x", new_x - 3.0, "y", new_y - 3.0, "width",
		              6.0, "height", 6.0, "visibility", GOO_CANVAS_ITEM_VISIBLE, NULL);
	} else {
		g_object_set (G_OBJECT (create_wire_info->dot), "visibility", GOO_CANVAS_ITEM_INVISIBLE,
		              NULL);
	}
	goo_canvas_item_raise (GOO_CANVAS_ITEM (create_wire_info->line), NULL);
	return TRUE;
}

inline static gboolean create_wire_discard (Sheet *sheet, GdkEvent *event)
{
	CreateWireInfo *create_wire_info;

	// g_signal_stop_emission_by_name (sheet, "event");
	NG_DEBUG ("wire got discarded");
	sheet_keyboard_ungrab (sheet, event);
	sheet_pointer_ungrab (sheet, event);

	create_wire_info = sheet->priv->create_wire_info;
	g_object_set (G_OBJECT (create_wire_info->dot), "visibility", GOO_CANVAS_ITEM_INVISIBLE, NULL);

	g_object_set (G_OBJECT (create_wire_info->line), "visibility", GOO_CANVAS_ITEM_INVISIBLE, NULL);

	create_wire_info->state = WIRE_START;

	return TRUE;
}

inline static Wire *create_wire_spawn (Sheet *sheet, Coords start_pos, Coords end_pos)
{
	Wire *wire = NULL;
	Coords length;

	NG_DEBUG ("=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=- spawning...");
	g_assert (sheet);
	g_assert (IS_SHEET (sheet));

	wire = wire_new (sheet->grid);

	length.x = end_pos.x - start_pos.x;
	length.y = end_pos.y - start_pos.y;
	wire_set_length (wire, &length);

	item_data_set_pos (ITEM_DATA (wire), &start_pos);
	schematic_add_item (schematic_view_get_schematic_from_sheet (sheet), ITEM_DATA (wire));

	NG_DEBUG ("=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=- spawning wire %p", wire);

	return wire;
}

#define FINISH_ON_WIRE_CLICK 0
inline static gboolean create_wire_fixate (Sheet *sheet, GdkEvent *event)
{
	NodeStore *store;
	Schematic *schematic;

	Coords p1, p2, start_pos, end_pos, mid_pos;
	CreateWireInfo *create_wire_info;
	Wire *wire = NULL;
	gboolean b_start_eq_mid, b_mid_eq_end;
#if FINISH_ON_WIRE_CLICK
	gboolean b_finish;
#endif
	double x, y;

	// g_signal_stop_emission_by_name (sheet, "event");

	x = event->button.x;
	y = event->button.y;
	goo_canvas_convert_from_pixels (GOO_CANVAS (sheet), &x, &y);
	snap_to_grid (sheet->grid, &x, &y);

	create_wire_info = sheet->priv->create_wire_info;
	g_assert (create_wire_info);
	g_assert (create_wire_info->points);
	g_assert (create_wire_info->line);

	p1.x = create_wire_info->points->coords[0];
	p1.y = create_wire_info->points->coords[1];

	p2.x = x;
	p2.y = y;

	NG_DEBUG ("x: %lf ?= %lf | y: %lf ?= %lf", p1.x, p2.x, p1.y, p2.y);

	// if we are back at the starting point of our wire,
	// and the user tries to fixate, just ignore it
	// and mark the event as handled
	if (coords_equal (&p1, &p2))
		return TRUE;

	schematic = schematic_view_get_schematic_from_sheet (sheet);
	g_assert (schematic);
	store = schematic_get_store (schematic);
	g_assert (store);

	start_pos.x = create_wire_info->points->coords[0];
	start_pos.y = create_wire_info->points->coords[1];

	mid_pos.x = create_wire_info->points->coords[2];
	mid_pos.y = create_wire_info->points->coords[3];

	end_pos.x = create_wire_info->points->coords[4];
	end_pos.y = create_wire_info->points->coords[5];

	NG_DEBUG ("A(%g, %g) B(%g, %g) -> same = %i", start_pos.x, start_pos.y, mid_pos.x, mid_pos.y,
	          coords_equal (&start_pos, &mid_pos));
	NG_DEBUG ("A(%g, %g) B(%g, %g) -> same = %i", mid_pos.x, mid_pos.y, end_pos.x, end_pos.y,
	          coords_equal (&mid_pos, &end_pos));

#if FINISH_ON_WIRE_CLICK
	// check for wires _before_ spawning wires
	// otherwise we will end up with 1 anyways
	b_finish = node_store_get_node (store, end_pos) || node_store_is_wire_at_pos (store, end_pos);
#endif
	b_start_eq_mid = coords_equal (&start_pos, &mid_pos);
	b_mid_eq_end = coords_equal (&mid_pos, &end_pos);

	if (!b_mid_eq_end && !b_start_eq_mid) {
		NG_DEBUG ("we should get exactly 2 wires");
	}

	if (!b_start_eq_mid) {
		wire = create_wire_spawn (sheet, start_pos, mid_pos);
		g_assert (wire);
		g_assert (IS_WIRE (wire));
	} else {
		NG_DEBUG ("looks like start == midpos");
	}

	if (!b_mid_eq_end) {
		wire = create_wire_spawn (sheet, mid_pos, end_pos);
		g_assert (wire);
		g_assert (IS_WIRE (wire));
	} else {
		NG_DEBUG ("looks like midpos == endpos");
	}

/* check if target location is either wire or node,
 * if so,
 * set state to START and fixate both ends of the current wire
 */
#if FINISH_ON_WIRE_CLICK
	/*
	 * auto-finish if the target is a wire or node
	 */
	if (b_finish)
		return create_wire_discard (sheet, event);
#endif
	/*
	 * update all position/cursor data for the next wire
	 * (consider this is a implicit START call, with less bloat)
	 */
	x = create_wire_info->points->coords[4];
	y = create_wire_info->points->coords[5];
	create_wire_info->points->coords[0] = x;
	create_wire_info->points->coords[1] = y;
	create_wire_info->points->coords[2] = x;
	create_wire_info->points->coords[3] = y;

	// required to trigger goocanvas update of the line object
	g_object_set (G_OBJECT (create_wire_info->line), "points", create_wire_info->points, NULL);

	// toggle wire direction
	if (create_wire_info->direction == WIRE_DIR_VERT)
		create_wire_info->direction = WIRE_DIR_HORIZ;
	else
		create_wire_info->direction = WIRE_DIR_VERT;

	goo_canvas_item_raise (GOO_CANVAS_ITEM (create_wire_info->line), NULL);

	/*
	 * do NOT ungrab sheet here, as we are still creating another wire
	 * this should be changed if on fixate means that we are back in state
	 * WIRE_START
	 */
	return TRUE;
}

gboolean create_wire_setup (Sheet *sheet)
{
	CreateWireInfo *create_wire_info;

	g_return_val_if_fail (sheet, FALSE);
	g_return_val_if_fail (IS_SHEET (sheet), FALSE);

	create_wire_info = sheet->priv->create_wire_info;
	g_return_val_if_fail (create_wire_info, FALSE);

	if (create_wire_info->state == WIRE_DISABLED) {
		create_wire_info->event_handler_id =
		    g_signal_connect (sheet, "event", G_CALLBACK (create_wire_event), NULL);
		// this is also handled by event
		//		create_wire_info->cancel_handler_id = g_signal_connect (sheet,
		//"cancel",
		//		                                                 G_CALLBACK
		//(create_wire_discard), NULL);
	}

	create_wire_info->state = WIRE_START;
	return TRUE;
}

gboolean create_wire_orientationtoggle (Sheet *sheet)
{
	CreateWireInfo *create_wire_info;
	GooCanvasPoints *points;
	g_return_val_if_fail (sheet, FALSE);
	g_return_val_if_fail (IS_SHEET (sheet), FALSE);

	NG_DEBUG ("toggle orientation")
	create_wire_info = sheet->priv->create_wire_info;
	g_return_val_if_fail (create_wire_info, FALSE);

	points = create_wire_info->points;

	switch (create_wire_info->direction) {
	case WIRE_DIR_HORIZ:
		create_wire_info->direction = WIRE_DIR_VERT;
		points->coords[2] = points->coords[0];
		points->coords[3] = points->coords[5];
		g_object_set (G_OBJECT (create_wire_info->line), "points", points, NULL);
		break;
	case WIRE_DIR_VERT:
		create_wire_info->direction = WIRE_DIR_HORIZ;
		points->coords[2] = points->coords[4];
		points->coords[3] = points->coords[1];
		g_object_set (G_OBJECT (create_wire_info->line), "points", points, NULL);
		break;
	default:
		break;
	}
	return TRUE;
}

gboolean create_wire_event (Sheet *sheet, GdkEvent *event, gpointer data)
{
	CreateWireInfo *create_wire_info;
	g_return_val_if_fail (sheet, FALSE);
	g_return_val_if_fail (IS_SHEET (sheet), FALSE);

	create_wire_info = sheet->priv->create_wire_info;
	g_return_val_if_fail (create_wire_info, FALSE);

	switch (event->type) {
	case GDK_3BUTTON_PRESS:
	case GDK_2BUTTON_PRESS:
	case GDK_BUTTON_RELEASE:
		break;
	case GDK_BUTTON_PRESS:
		switch (event->button.button) {
		case 1:
			if (create_wire_info->state == WIRE_START)
				return create_wire_start (sheet, event);
			else if (create_wire_info->state == WIRE_ACTIVE)
				return create_wire_fixate (sheet, event);
			break;
		case 3:
			if (create_wire_info->state == WIRE_ACTIVE)
				return create_wire_discard (sheet, event);
			break;
		default:
			break;
		}
		break;
	case GDK_MOTION_NOTIFY:
		if (create_wire_info->state == WIRE_ACTIVE)
			return create_wire_update (sheet, event);
		break;
	case GDK_KEY_PRESS:
		NG_DEBUG ("keypress 0");
		if (create_wire_info->state != WIRE_ACTIVE)
			return FALSE;
		NG_DEBUG ("keypress 1");
		switch (event->key.keyval) {
		case GDK_KEY_Escape:
			return create_wire_discard (sheet, event);
		case GDK_KEY_R:
		case GDK_KEY_r:
			return create_wire_orientationtoggle (sheet);
		default:
			break;
		}
	default:
		break;
	}
	return FALSE;
}

gboolean create_wire_cleanup (Sheet *sheet)
{
	CreateWireInfo *create_wire_info;

	create_wire_info = sheet->priv->create_wire_info;
	if (create_wire_info && create_wire_info->state != WIRE_DISABLED) {
		g_object_set (G_OBJECT (create_wire_info->line), "visibility", GOO_CANVAS_ITEM_INVISIBLE,
		              NULL);
		create_wire_info->state = WIRE_DISABLED;
		g_signal_handler_disconnect (G_OBJECT (sheet), create_wire_info->event_handler_id);
		//		g_signal_handler_disconnect (G_OBJECT (sheet),
		// create_wire_info->cancel_handler_id);
	}

	return TRUE;
}
