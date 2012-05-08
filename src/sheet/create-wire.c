/*
 * create-wire.c
 *
 * Authors:
 * Richard Hult <rhult@hem.passagen.se>
 * Ricardo Markiewicz <rmarkie@fi.uba.ar>
 * Andres de Barbara <adebarbara@fi.uba.ar>
 * Marc Lorber <lorber.marc@wanadoo.fr>
 *
 * Description: Handles the user interaction when creating wires.
 * The name is not really right. This part handles creation of wires and
 * acts as glue between NodeStore/Wire and Sheet/WireItem.
 *
 * Web page: https://github.com/marc-lorber/oregano
 *
 * Copyright (C) 1999-2001  Richard Hult
 * Copyright (C) 2003,2006  Ricardo Markiewicz
 * Copyright (C) 2009-2012  Marc Lorber
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
#include <stdlib.h>

#include "cursors.h"
#include "sheet-pos.h"
#include "node-store.h"
#include "wire-item.h"
#include "create-wire.h"
#include "wire.h"

#define NG_DEBUG(s) if (0) g_print ("%s\n", s)

struct _CreateWireContext {
	guint 				active : 1;
	gint 				start_handler_id;
	gint 				draw_handler_id;
	gint 				create_wire_cancel_id;
	gdouble 			old_x, old_y;
	Sheet *				sheet;
	GooCanvasItem	*	dot_item;
	CreateWire *		create_wire;
};

static int 		create_wire_event (Sheet *sheet, const GdkEvent *event,
					CreateWireContext *cwc);
static int 		create_wire_pre_create_event (Sheet *sheet, const GdkEvent *event,
					CreateWireContext *cwc);
static int 		create_wire_cancel (Sheet *sheet, CreateWireContext *cwc);
static void 	fixate_wire (CreateWireContext *cwc, gboolean always_fixate_both,
					gdouble x, gdouble y);
static Wire *	create_wire_and_place_item (Sheet *sheet, SheetPos start_pos,
					SheetPos end_pos);
static void 	cancel_wire (CreateWireContext *cwc);
static void 	exit_wire_mode (CreateWireContext *cwc);

// Initiates wire creation by disconnecting the signal handler that
// starts the wire-drawing and then connecting the drawing handler.
// This is an event handler.
//
// @param sheet a sheet (the canvas)
// @param event the event
// @param cwc   context
static int
create_wire_pre_create_event (Sheet *sheet, const GdkEvent *event,
	CreateWireContext *cwc)
{
	GooCanvasPoints	*points;
	GooCanvasLineDash *dash;
	CreateWire *wire;
	double new_x, new_y;

	if (cwc->active)
		return FALSE;

	if (event->button.button == 2 || event->button.button > 3)
		return FALSE;

	if (event->type == GDK_2BUTTON_PRESS ||
		event->type == GDK_3BUTTON_PRESS)
		return FALSE;

	g_signal_stop_emission_by_name (G_OBJECT (sheet), "event");

	if (event->type != GDK_BUTTON_PRESS) 
		return FALSE;

	// Button 3 resets the sheet mode.
	if (event->button.button == 3) {
		exit_wire_mode (cwc);
		return TRUE;
	}

	// Button 1 starts a new wire. Start by deselecting all objects.
	sheet_select_all (sheet, FALSE);

	sheet_get_pointer (sheet, &new_x, &new_y);

	points = goo_canvas_points_new (3);
	points->coords[0] = new_x;
	points->coords[1] = new_y;
	points->coords[2] = new_x;
	points->coords[3] = new_y;
	points->coords[4] = new_x;
	points->coords[5] = new_y;

	wire = g_new0 (CreateWire, 1);
	cwc->create_wire = wire;
	cwc->old_x = new_x;
	cwc->old_y = new_y;

	dash = goo_canvas_line_dash_new (2, 5.0, 5.0);
	
	wire->line = GOO_CANVAS_POLYLINE (
		goo_canvas_polyline_new (GOO_CANVAS_ITEM (sheet->object_group),
	        FALSE, 0, 
		    "points", points,
		    "stroke-color", "red",
		    "line-dash", dash,
			"line-width", 1.0,
			NULL));
	
	goo_canvas_line_dash_unref (dash);        

	wire->points = points;
	wire->direction = WIRE_DIR_NONE;

	cwc->draw_handler_id = g_signal_connect (sheet, "event",
		G_CALLBACK (create_wire_event), cwc);

	cwc->active = TRUE;

	return TRUE;
}

// This needs to be called in order to start a wire creation.
// It sets up the initial event handler that basically just
// takes care of the first button-1 press that starts the
// drawing mode. */
CreateWireContext *
create_wire_initiate (Sheet *sheet)
{
	CreateWireContext *cwc;

	g_return_val_if_fail (sheet != NULL, NULL);
	g_return_val_if_fail (IS_SHEET (sheet), NULL);

	cwc = g_new0 (CreateWireContext, 1);
	cwc->sheet = sheet;
	cwc->active = FALSE;
	cwc->dot_item = NULL;

	cwc->start_handler_id = g_signal_connect (sheet, "event",
	    G_CALLBACK (create_wire_pre_create_event), cwc);

	cwc->create_wire_cancel_id = g_signal_connect (sheet, "cancel",
		G_CALLBACK (create_wire_cancel), cwc);

	return cwc;
}

static int
create_wire_event (Sheet *sheet, const GdkEvent *event, CreateWireContext *cwc)
{
	int x1, y1, x2, y2;
	gdouble snapped_x, snapped_y;
	gboolean diagonal;
	CreateWire *wire = cwc->create_wire;
	Schematic *s;
	NodeStore *store;
	SheetPos pos;
	int intersect;

	s = schematic_view_get_schematic_from_sheet (cwc->sheet);
	store = schematic_get_store (s);

	if (event->type == GDK_2BUTTON_PRESS ||	
		event->type == GDK_3BUTTON_PRESS) {
		return FALSE;
	}

	switch (event->type) {
		case GDK_BUTTON_PRESS:
			switch (event->button.button) {
				case 2:
				case 4:
				case 5:
					// Don't care about middle button or mouse wheel.
					return FALSE;
					break;

				case 1:

					// Button-1 fixates the first segment of the wire,
					// letting the user continue drawing wires, with the
					// former second segment as the first segment of the
					// new wire. There are a few exceptions though; if the
					// button press happens at another wire or a pin of a
					// part, then we fixate the wire and cancel the drawing
					// mode.
					g_signal_stop_emission_by_name (sheet, "event");
					sheet_get_pointer (sheet, &snapped_x, &snapped_y);
					fixate_wire (cwc, FALSE, snapped_x, snapped_y);
					break;

				case 3:
					g_signal_stop_emission_by_name (sheet, "event");
					cancel_wire (cwc);
					break;
			}
			break;

		case GDK_BUTTON_RELEASE:
			g_signal_stop_emission_by_name (sheet, "event");		

			switch (event->button.button) {
				case 1:
				case 2:
				case 4:
				case 5:
					// Don't care about middle button or mouse wheel.
					return FALSE;
					break;
			}
			return TRUE;

		case GDK_MOTION_NOTIFY:
			g_signal_stop_emission_by_name (sheet, "event");

			diagonal = event->button.state & GDK_SHIFT_MASK;
			if (!diagonal && wire->direction == WIRE_DIR_DIAG)
				wire->direction = WIRE_DIR_NONE;

			sheet_get_pointer (sheet, &snapped_x, &snapped_y);

			x1 = wire->points->coords[0];
			y1 = wire->points->coords[1];

			if (x1 == snapped_x && y1 == snapped_y) {
				wire->direction = WIRE_DIR_NONE;
			} 
			else {
				if (wire->direction == WIRE_DIR_NONE) {
					if (abs (y1 - snapped_y) < abs (x1 - snapped_x)) {
						wire->direction = WIRE_DIR_HORIZ;
					} 
					else {
						wire->direction = WIRE_DIR_VERT;
					}
				}
			}

			x2 = snapped_x;
			y2 = snapped_y;

			if (diagonal) {
				wire->direction = WIRE_DIR_DIAG;
				x2 = x1;
				y2 = y1;
			}

			if (wire->direction == WIRE_DIR_HORIZ) {
				y2 = y1;
			} 
			else if (wire->direction == WIRE_DIR_VERT) {
				x2 = x1;
			}

			if (wire->direction == WIRE_DIR_HORIZ && x2 == x1) {
				x2 = snapped_x;
				y2 = snapped_y;
			} 
			else if (wire->direction == WIRE_DIR_VERT && y2 == y1) {
				x2 = snapped_x;
				y2 = snapped_y;
			}
			
			wire->points->coords[2] = x2;
			wire->points->coords[3] = y2;
			wire->points->coords[4] = snapped_x;
			wire->points->coords[5] = snapped_y;

			g_object_set (G_OBJECT (wire->line),
						  "points", wire->points, 
			              NULL);

			pos.x = snapped_x;
			pos.y = snapped_y;
			// Check if the pre-wire intersect another wire, and
			// draw a small red circle to indicate the connection
			intersect = 0;
			intersect = node_store_is_wire_at_pos (store, pos);
			intersect += node_store_is_pin_at_pos (store, pos);
			if (intersect) {
				if (cwc->dot_item) {
					g_object_set (G_OBJECT (cwc->dot_item),
				   				  "x", -3.0 + snapped_x,
				    			  "y", -3.0 + snapped_y,
				    			  "width", 6.0,
				    			  "height", 6.0,
								  NULL);

					g_object_set (cwc->dot_item, 
				                  "visibility", GOO_CANVAS_ITEM_VISIBLE, 
					              NULL);
				} 
				else {
					cwc->dot_item = goo_canvas_ellipse_new (
				    	GOO_CANVAS_ITEM (sheet->object_group),
				    	snapped_x, snapped_y, 3.0, 3.0, 
				    	"fill_color", "red", 
					    NULL);
				    
					g_object_set (cwc->dot_item, 
				                  "visibility", GOO_CANVAS_ITEM_VISIBLE, 
					              NULL);
				}
			} 
			else {
				if (cwc->dot_item) 
					g_object_set (cwc->dot_item, 
				                  "visibility", GOO_CANVAS_ITEM_INVISIBLE, 
					              NULL);
			}

			cwc->old_x = snapped_x;
			cwc->old_y = snapped_y;
			break;

		default:
			return FALSE;
		}
		return TRUE;
}

static void
fixate_wire (CreateWireContext *cwc, gboolean always_fixate_both, 
             gdouble x, gdouble y)
{
	CreateWire *create_wire = cwc->create_wire;
	SheetPos p1, p2, start_pos, end_pos, start_pos2, end_pos2;
	gboolean cancel = FALSE;
	NodeStore *store;
	Schematic *schematic;

	g_return_if_fail (cwc != NULL);
	g_return_if_fail (create_wire != NULL);

	schematic = schematic_view_get_schematic_from_sheet (cwc->sheet);
	store = schematic_get_store (schematic);

	p1.x = create_wire->points->coords[0];
	p1.y = create_wire->points->coords[1];
	p2.x = create_wire->points->coords[2];
	p2.y = create_wire->points->coords[3];

	if (create_wire->direction == WIRE_DIR_DIAG) {
		p1.x = p2.x;
		p1.y = p2.y;
		p2.x = create_wire->points->coords[4];
		p2.y = create_wire->points->coords[5];
		create_wire->points->coords[2] = p2.x;
		create_wire->points->coords[3] = p2.y;
		create_wire->points->num_points = 2;
	}

	// If the user clicks when wire length is zero, cancel the wire.
	if (p1.x == p2.x && p1.y == p2.y) {
		cancel_wire (cwc);
		return;
	}

	if (create_wire->direction != WIRE_DIR_DIAG) {
		start_pos.x = MIN (p1.x, p2.x);
		start_pos.y = MIN (p1.y, p2.y);
		end_pos.x = MAX (p1.x, p2.x);
		end_pos.y = MAX (p1.y, p2.y);
	} 
	else {
		start_pos.x = p1.x;
		start_pos.y = p1.y;
		end_pos.x = p2.x;
		end_pos.y = p2.y;
	}

	// If the wire connects to something, then fixate
	// the second segment too and exit wire drawing mode.
	// Also fixate both segments when explicitly asked to. */
	p2.x = x;
	p2.y = y;
	if (always_fixate_both 						||
		node_store_get_node (store, p2) 		||
		node_store_is_wire_at_pos (store, p2)) {
		if (create_wire->points->num_points == 3) {
			p1.x = create_wire->points->coords[2];
			p1.y = create_wire->points->coords[3];

			if (create_wire->direction != WIRE_DIR_DIAG) {
				start_pos2.x = MIN (p1.x, p2.x);
				start_pos2.y = MIN (p1.y, p2.y);
				end_pos2.x = MAX (p1.x, p2.x);
				end_pos2.y = MAX (p1.y, p2.y);
			} 
			else {
				start_pos2.x = p1.x;
				start_pos2.y = p1.y;
				end_pos2.x = p2.x;
				end_pos2.y = p2.y;
			}

			create_wire_and_place_item (cwc->sheet,
				start_pos2, end_pos2);
		}
		cancel_wire (cwc);
		cancel = TRUE;
	}

	create_wire_and_place_item (cwc->sheet, start_pos, end_pos);

	if (cancel)
		return;

	// Start a new "floating" wire, using the same CreateWire that was used
	// for the old wire. 
	create_wire->points->coords[0] = create_wire->points->coords[2];
	create_wire->points->coords[1] = create_wire->points->coords[3];
	create_wire->points->coords[2] = x;
	create_wire->points->coords[3] = y;
	create_wire->points->num_points = 2;

	g_object_set (G_OBJECT (create_wire->line),
	              "points", create_wire->points,
	              NULL);

	// Raise the floting wire so that we can see it when it is overlapping
	// other wires.
	goo_canvas_item_raise (GOO_CANVAS_ITEM (create_wire->line), NULL);
}

Wire *
create_wire_and_place_item (Sheet *sheet, SheetPos start_pos,
	SheetPos end_pos)
{
	Wire *wire;
	SheetPos length;

	g_return_val_if_fail (sheet != NULL, NULL);
	g_return_val_if_fail (IS_SHEET (sheet), NULL);

	wire = wire_new ();
	item_data_set_pos (ITEM_DATA (wire), &start_pos);

	length.x = end_pos.x - start_pos.x;
	length.y = end_pos.y - start_pos.y;
	wire_set_length (wire, &length);

	schematic_add_item (schematic_view_get_schematic_from_sheet (sheet), 
	                    ITEM_DATA (wire));

	return wire;
}

static void
cancel_wire (CreateWireContext *cwc)
{
	CreateWire *create_wire;

	create_wire = cwc->create_wire;

	g_return_if_fail (create_wire != NULL);

	g_signal_handler_disconnect (G_OBJECT (cwc->sheet), cwc->draw_handler_id);
	cwc->draw_handler_id = 0;
	cwc->active = FALSE;

	goo_canvas_points_unref (create_wire->points);

	if (cwc->dot_item) {
		goo_canvas_item_remove (GOO_CANVAS_ITEM (cwc->dot_item));
		cwc->dot_item = NULL;
	}
	goo_canvas_item_remove (GOO_CANVAS_ITEM (create_wire->line));

	g_free (create_wire);

	// Setup the sheet for a new wire creation process.
}

static void
exit_wire_mode (CreateWireContext *cwc)
{
	if (cwc->draw_handler_id != 0)
		cancel_wire (cwc);

	if (cwc->start_handler_id != 0) {
		g_signal_handler_disconnect (G_OBJECT (cwc->sheet),
			cwc->start_handler_id);
		cwc->start_handler_id = 0;
	}

	if (cwc->create_wire_cancel_id != 0) {
		g_signal_handler_disconnect (G_OBJECT (cwc->sheet),
			cwc->create_wire_cancel_id);
		cwc->create_wire_cancel_id = 0;
	}
	schematic_view_reset_tool (
	          schematic_view_get_schematicview_from_sheet (cwc->sheet));

	g_free (cwc);
}

void
create_wire_exit (CreateWireContext *cwc)
{
	if (cwc->draw_handler_id != 0)
		cancel_wire (cwc);

	if (cwc->start_handler_id != 0) {
		g_signal_handler_disconnect (G_OBJECT (cwc->sheet),
			cwc->start_handler_id);
		cwc->start_handler_id = 0;
	}
}

// Signal handler for the "cancel" signal that the sheet emits
// when <escape> is pressed.
static int
create_wire_cancel (Sheet *sheet, CreateWireContext *cwc)
{
	g_return_val_if_fail (sheet != NULL, FALSE);
	g_return_val_if_fail (IS_SHEET (sheet), FALSE);

	if (cwc->active)
		cancel_wire (cwc);
	else
		exit_wire_mode (cwc);

	return TRUE;
}
