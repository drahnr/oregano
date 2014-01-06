/*
 * wire-item.c
 *
 *
 * Authors:
 *  Richard Hult <rhult@hem.passagen.se>
 *  Ricardo Markiewicz <rmarkie@fi.uba.ar>
 *  Andres de Barbara <adebarbara@fi.uba.ar>
 *  Marc Lorber <lorber.marc@wanadoo.fr>
 *
 * Web page: https://srctwig.com/oregano
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

#include <gtk/gtk.h>
#include <string.h>

#include "cursors.h"
#include "coords.h"
#include "wire-item.h"
#include "node-store.h"
#include "wire.h"
#include "wire-private.h"
#include "schematic.h"
#include "schematic-view.h"
#include "options.h"

#define NORMAL_COLOR "blue"
#define SELECTED_COLOR "green"
#define HIGHLIGHT_COLOR "yellow"

#define RESIZER_SIZE 4.0f

static void 		wire_item_class_init (WireItemClass *klass);
static void 		wire_item_init (WireItem *item);
static void 		wire_item_dispose (GObject *object);
static void 		wire_item_finalize (GObject *object);
static void 		wire_item_moved (SheetItem *object);
static void 		wire_rotated_callback (ItemData *data, int angle,
						SheetItem *sheet_item);
static void 		wire_flipped_callback (ItemData *data, IDFlip horizontal,
						SheetItem *sheet_item);
static void 		wire_moved_callback (ItemData *data, Coords *pos,
						SheetItem *item);
static void 		wire_changed_callback (Wire *, WireItem *item);
static void 		wire_delete_callback (Wire *, WireItem *item);
static void 		wire_item_paste (Sheet *sheet, ItemData *data);
static void 		selection_changed (WireItem *item, gboolean select,
						gpointer user_data);
static int 			select_idle_callback (WireItem *item);
static int 			deselect_idle_callback (WireItem *item);
static gboolean 	is_in_area (SheetItem *object, Coords *p1, Coords *p2);
inline static void 	get_boundingbox (WireItem *item, Coords *p1, Coords *p2);
static void 		mouse_over_wire_callback (WireItem *item, Sheet *sheet);
static void 		highlight_wire_callback (Wire *wire, WireItem *item);
static int 			unhighlight_wire (WireItem *item);
static void 		wire_item_place (SheetItem *item, Sheet *sheet);
static void 		wire_item_place_ghost (SheetItem *item, Sheet *sheet);
static void         wire_item_get_property (GObject *object, guint prop_id,
                    	GValue *value, GParamSpec *spec);
static void         wire_item_set_property (GObject *object, guint prop_id,
                    	const GValue *value, GParamSpec *spec);

#include "debug.h"

enum {
	WIRE_RESIZER_NONE,
	WIRE_RESIZER_1,
	WIRE_RESIZER_2
};

struct _WireItemPriv {
	guint 				cache_valid : 1;
	guint 				resize_state;
	guint 				highlight : 1;
	WireDir 			direction;	// Direction of the wire.

	GooCanvasPolyline *	line;
	GooCanvasRect	  *	resize1;	// Resize box of the wire
	GooCanvasRect	  *	resize2;	// Resize box of the wire

	// Cached bounding box. This is used to make
	// the rubberband selection a bit faster.
	Coords 			bbox_start;
	Coords 			bbox_end;
};

G_DEFINE_TYPE (WireItem, wire_item, TYPE_SHEET_ITEM)

static void
wire_item_class_init (WireItemClass *wire_item_class)
{
	GObjectClass *object_class;
	SheetItemClass *sheet_item_class;

	object_class = G_OBJECT_CLASS (wire_item_class);
	sheet_item_class = SHEET_ITEM_CLASS (wire_item_class);
	wire_item_parent_class = g_type_class_peek_parent (wire_item_class);

	object_class->finalize = wire_item_finalize;
	object_class->dispose = wire_item_dispose;
	object_class->set_property = wire_item_set_property;
    object_class->get_property = wire_item_get_property;

	sheet_item_class->moved = wire_item_moved;
	sheet_item_class->paste = wire_item_paste;
	sheet_item_class->is_in_area = is_in_area;
	sheet_item_class->selection_changed = selection_changed;
	sheet_item_class->place = wire_item_place;
	sheet_item_class->place_ghost = wire_item_place_ghost;
}

static void
wire_item_set_property (GObject *object, guint property_id, const GValue *value,
        GParamSpec *pspec)
{
        g_return_if_fail (object != NULL);
        g_return_if_fail (IS_WIRE_ITEM(object));

        switch (property_id) {
        default:
        	g_warning ("PartItem: Invalid argument.\n");
        }
}

static void
wire_item_get_property (GObject *object, guint property_id, GValue *value,
        GParamSpec *pspec)
{
        g_return_if_fail (object != NULL);
        g_return_if_fail (IS_WIRE_ITEM (object));

        switch (property_id) {
        default:
        	pspec->value_type = G_TYPE_INVALID;
            break;
        }
}

static void
wire_item_init (WireItem *item)
{
	WireItemPriv *priv;

	priv = g_new0 (WireItemPriv, 1);

	priv->direction = WIRE_DIR_NONE;
	priv->highlight = FALSE;
	priv->cache_valid = FALSE;
	priv->line = NULL;
	priv->resize1 = NULL;
	priv->resize2 = NULL;

	item->priv = priv;
}

static void
wire_item_dispose (GObject *object)
{
	WireItemPriv *priv;

	priv = WIRE_ITEM (object)->priv;

	g_clear_object (&(priv->line));
	g_clear_object (&(priv->resize1));
	g_clear_object (&(priv->resize2));
	G_OBJECT_CLASS (wire_item_parent_class)->dispose (object);
}


static void
wire_item_finalize (GObject *object)
{
	WireItemPriv *priv;

	priv = WIRE_ITEM (object)->priv;

	if (priv != NULL) {
		g_free (priv);
	}

	G_OBJECT_CLASS (wire_item_parent_class)->finalize (object);
}

// TODO get rid of this
static void
wire_item_moved (SheetItem *object)
{
//	NG_DEBUG ("wire MOVED callback called - LEGACY");
}

WireItem *
wire_item_new (Sheet *sheet, Wire *wire)
{
	GooCanvasItem *item;
	WireItem *wire_item;
	ItemData *item_data;
	GooCanvasPoints *points;
	WireItemPriv *priv;
	Coords start_pos, length;

	g_return_val_if_fail (sheet != NULL, NULL);
	g_return_val_if_fail (IS_SHEET (sheet), NULL);

	wire_get_pos_and_length (wire, &start_pos, &length);

	item = g_object_new (TYPE_WIRE_ITEM, NULL);

	g_object_set (item,
	              "parent", sheet->object_group,
	              NULL);

	wire_item = WIRE_ITEM (item);
	g_object_set (wire_item,
	              "data", wire,
	              NULL);

	priv = wire_item->priv;


	const int random_color_count = 9;
	const char *random_color[] = {"blue", "red", "green"/*, "yellow"*/, "orange", "brown", "purple", "pink", "lightblue", "lightgreen"};

	priv->resize1 = GOO_CANVAS_RECT (goo_canvas_rect_new (
	           	GOO_CANVAS_ITEM (wire_item),
			   	-RESIZER_SIZE,
	            -RESIZER_SIZE,
	            2 * RESIZER_SIZE,
	            2 * RESIZER_SIZE,
	                "stroke-color", opts.debug.wires ? random_color[g_random_int_range(0,random_color_count-1)] : "blue",
	            "fill-color", "green",
	    		"line-width", 1.0,
	    		NULL));
	g_object_set (priv->resize1,
				  "visibility", GOO_CANVAS_ITEM_INVISIBLE,
	              NULL);

	priv->resize2 = GOO_CANVAS_RECT (goo_canvas_rect_new (
				GOO_CANVAS_ITEM (wire_item),
				length.x - RESIZER_SIZE,
	     		length.y - RESIZER_SIZE,
	            2 * RESIZER_SIZE,
	    		2 * RESIZER_SIZE,
	                "stroke-color", opts.debug.wires ? random_color[g_random_int_range(0,random_color_count-1)] : "blue",
	            "fill-color", "green",
	    		"line-width", 1.0,
	    		NULL));
	g_object_set (priv->resize2,
				  "visibility", GOO_CANVAS_ITEM_INVISIBLE,
	              NULL);

	points = goo_canvas_points_new (2);
	points->coords[0] = 0;
	points->coords[1] = 0;
	points->coords[2] = length.x;
	points->coords[3] = length.y;

	priv->line = GOO_CANVAS_POLYLINE (goo_canvas_polyline_new (
	    GOO_CANVAS_ITEM (wire_item),
		FALSE, 0,
	    "points", points,
	    "stroke-color", opts.debug.wires ? random_color[g_random_int_range(0,random_color_count-1)] : "blue",
	    "line-width", 1.0,
	    "start-arrow", opts.debug.wires ? TRUE : FALSE,
	    "end-arrow", opts.debug.wires ? TRUE : FALSE,
	    NULL));

	goo_canvas_points_unref (points);


	item_data = ITEM_DATA (wire);
	item_data->rotated_handler_id = g_signal_connect_object (G_OBJECT (wire),
	                                "rotated",
	                                G_CALLBACK (wire_rotated_callback),
	                                G_OBJECT (wire_item), 0);
	item_data->flipped_handler_id = g_signal_connect_object (G_OBJECT (wire),
	                                "flipped",
	                                G_CALLBACK (wire_flipped_callback),
	                                G_OBJECT (wire_item), 0);
	item_data->moved_handler_id = g_signal_connect_object (G_OBJECT (wire),
	                                "moved",
	                                G_CALLBACK (wire_moved_callback),
	                                G_OBJECT (wire_item), 0);
	item_data->changed_handler_id = g_signal_connect_object (G_OBJECT (wire),
	                                "changed",
	                                G_CALLBACK (wire_changed_callback),
	                                G_OBJECT (wire_item), 0);

	g_signal_connect (wire, "delete",
	    G_CALLBACK (wire_delete_callback), wire_item);

	wire_update_bbox (wire);

	return wire_item;
}

gboolean
wire_item_event (WireItem *wire_item,
		 GooCanvasItem *sheet_target_item,
         GdkEvent *event, Sheet *sheet)
{
	Coords start_pos, length;
	Wire *wire;
	GooCanvas *canvas;
	static double last_x, last_y;
	double dx, dy, zoom;
	// The selected group's bounding box in window resp. canvas coordinates.
	double snapped_x, snapped_y;
	Coords pos;


	canvas = GOO_CANVAS (sheet);
	g_object_get (G_OBJECT (wire_item),
	              "data", &wire,
	              NULL);

	wire_get_pos_and_length (WIRE (wire), &start_pos, &length);
	sheet_get_zoom (sheet, &zoom);

	switch (event->type) {
		case GDK_BUTTON_PRESS:
			switch (event->button.button) {
				case 1: {
					double x, y;
					g_signal_stop_emission_by_name (wire_item,
					                                "button_press_event");
					sheet_get_pointer_snapped (sheet, &x, &y);
					x = x - start_pos.x;
					y = y - start_pos.y;
					if ((x > -RESIZER_SIZE) && (x < RESIZER_SIZE)  &&
						(y > -RESIZER_SIZE) && (y < RESIZER_SIZE)) {
						gtk_widget_grab_focus (GTK_WIDGET (sheet));
						sheet->state = SHEET_STATE_DRAG_START;
						wire_item->priv->resize_state = WIRE_RESIZER_1;

						sheet_get_pointer_snapped (sheet, &x, &y);
						last_x = x;
						last_y = y;
						item_data_unregister (ITEM_DATA (wire));
						return TRUE;
					}
                    if ((x > (length.x-RESIZER_SIZE)) &&
                        (x < (length.x+RESIZER_SIZE)) &&
						(y > (length.y-RESIZER_SIZE)) &&
                        (y < (length.y+RESIZER_SIZE))) {
						gtk_widget_grab_focus (GTK_WIDGET (sheet));
						sheet->state = SHEET_STATE_DRAG_START;
						wire_item->priv->resize_state = WIRE_RESIZER_2;

						sheet_get_pointer_snapped (sheet, &x, &y);
						last_x = x;
						last_y = y;
						item_data_unregister (ITEM_DATA (wire));
						return TRUE;
					}
				}
				break;
			}
			break;

		case GDK_MOTION_NOTIFY:
            if (sheet->state != SHEET_STATE_DRAG &&
				sheet->state != SHEET_STATE_DRAG_START)
				break;

			if (wire_item->priv->resize_state == WIRE_RESIZER_NONE)
				break;

			if (sheet->state == SHEET_STATE_DRAG_START ||
			    sheet->state == SHEET_STATE_DRAG) 	   {

				g_signal_stop_emission_by_name (wire_item,
				                                "motion-notify-event");

				sheet->state = SHEET_STATE_DRAG;

				sheet_get_pointer_snapped (sheet, &snapped_x, &snapped_y);

				dx = snapped_x - last_x;
				dy = snapped_y - last_y;

				last_x = snapped_x;
				last_y = snapped_y;

				wire_get_pos_and_length (wire, &pos, &length);

				if (wire_item->priv->resize_state == WIRE_RESIZER_1) {
					switch (wire->priv->direction) {
						case WIRE_DIR_VERT:
							/* Vertical Wire */
							pos.y = last_y;
							length.y -= dy;
						break;
						case WIRE_DIR_HORIZ:
							/* Horizontal Wire */
							pos.x = last_x;
							length.x -= dx;
						break;
						default:
							pos.y = last_y;
							length.y -= dy;
							pos.x = last_x;
							length.x -= dx;
					}
				}
				else {
					switch (wire->priv->direction) {
						case WIRE_DIR_VERT:
							/* Vertical Wire */
							length.y += dy;
						break;
						case WIRE_DIR_HORIZ:
							/* Horizontal Wire */
							length.x += dx;
						break;
						default:
							length.y += dy;
							length.x += dx;
					}
				}
				snap_to_grid (sheet->grid, &length.x, &length.y);
				item_data_set_pos (sheet_item_get_data (SHEET_ITEM (wire_item)),
				                   &pos);

				wire_set_length (wire, &length);
				return TRUE;
			}
			break;
		case GDK_BUTTON_RELEASE:
			switch (event->button.button) {
			case 1:
				if (sheet->state != SHEET_STATE_DRAG &&
					sheet->state != SHEET_STATE_DRAG_START) {
					break;
				}
				if (wire_item->priv->resize_state == WIRE_RESIZER_NONE) {
					break;
				}

				g_signal_stop_emission_by_name (wire_item,
				                                "button-release-event");

				goo_canvas_pointer_ungrab (canvas, GOO_CANVAS_ITEM (wire_item),
					event->button.time);

				wire_item->priv->resize_state = WIRE_RESIZER_NONE;
				sheet->state = SHEET_STATE_NONE;
				item_data_register (ITEM_DATA (wire));
				return TRUE;
			}
			break;
		default:
			return sheet_item_event (GOO_CANVAS_ITEM (wire_item),
			                         GOO_CANVAS_ITEM (wire_item), event, sheet);
	}
	return sheet_item_event (GOO_CANVAS_ITEM (wire_item),
	                         GOO_CANVAS_ITEM (wire_item), event, sheet);
}

void
wire_item_signal_connect_placed (WireItem *wire_item, Sheet *sheet)
{
	ItemData *item;

	item = sheet_item_get_data (SHEET_ITEM (wire_item));

	g_signal_connect (wire_item, "button-press-event",
	    G_CALLBACK (wire_item_event), sheet);

	g_signal_connect (wire_item, "button-release-event",
	    G_CALLBACK (wire_item_event), sheet);

	g_signal_connect (wire_item, "motion-notify-event",
	    G_CALLBACK (wire_item_event), sheet);

	g_signal_connect (wire_item, "mouse_over",
		G_CALLBACK (mouse_over_wire_callback), sheet);

	g_signal_connect (item, "highlight",
		G_CALLBACK (highlight_wire_callback), wire_item);
}

static void
wire_rotated_callback (ItemData *data, int angle, SheetItem *sheet_item)
{
	WireItem *wire_item;
	GooCanvasPoints *points;
	Coords start_pos, length;

	g_return_if_fail (sheet_item != NULL);
	g_return_if_fail (IS_WIRE_ITEM (sheet_item));

	wire_item = WIRE_ITEM (sheet_item);

	wire_get_pos_and_length (WIRE (data), &start_pos, &length);

	points = goo_canvas_points_new (2);
	points->coords[0] = 0;
	points->coords[1] = 0;
	points->coords[2] = length.x;
	points->coords[3] = length.y;

	g_object_set (wire_item->priv->line,
			      "points", points,
		          NULL);
	goo_canvas_points_unref (points);

	g_object_set (wire_item,
				  "x", start_pos.x,
		          "y", start_pos.y,
		          NULL);

	g_object_set (wire_item-> priv->resize2,
		          "x", length.x-RESIZER_SIZE,
	              "y", length.y-RESIZER_SIZE,
	              NULL);

	//Invalidate the bounding box cache.
	wire_item->priv->cache_valid = FALSE;
}

static void
wire_flipped_callback (ItemData *data,
	IDFlip direction, SheetItem *sheet_item)
{
	GooCanvasPoints *points;
	WireItem *item;
	WireItemPriv *priv;
	Coords start_pos, length;

	g_return_if_fail (sheet_item != NULL);
	g_return_if_fail (IS_WIRE_ITEM (sheet_item));

	item = WIRE_ITEM (sheet_item);
	priv = item->priv;

	wire_get_pos_and_length (WIRE (data), &start_pos, &length);

	points = goo_canvas_points_new (2);
	points->coords[0] = 0;
	points->coords[1] = 0;
	points->coords[2] = length.x;
	points->coords[3] = length.y;

	g_object_set (item->priv->line,
	              "points", points,
	              NULL);
	goo_canvas_points_unref (points);

	g_object_set (item,
	              "x", start_pos.x,
	              "y", start_pos.y,
	              NULL);

	// Invalidate the bounding box cache.
	priv->cache_valid = FALSE;
}

static int
select_idle_callback (WireItem *item)
{
	WireItemPriv *priv = item->priv;

	g_object_set (priv->line,
	              "stroke-color", SELECTED_COLOR,
	              NULL);
	g_object_set (item->priv->resize1,
				  "visibility", GOO_CANVAS_ITEM_VISIBLE,
	              NULL);
	g_object_set (item->priv->resize2,
				  "visibility", GOO_CANVAS_ITEM_VISIBLE,
	              NULL);

	priv->highlight = TRUE;

	g_object_unref (G_OBJECT (item));
	return FALSE;
}

static int
deselect_idle_callback (WireItem *item)
{
	WireItemPriv *priv = item->priv;

	g_object_set (priv->line,
	              "stroke_color", NORMAL_COLOR,
	              NULL);
	g_object_set (item->priv->resize1,
				  "visibility", GOO_CANVAS_ITEM_INVISIBLE,
	              NULL);
	g_object_set (item->priv->resize2,
				  "visibility", GOO_CANVAS_ITEM_INVISIBLE,
	              NULL);

	priv->highlight = FALSE;

	g_object_unref(G_OBJECT (item));
	return FALSE;
}

static void
selection_changed ( WireItem *item, gboolean select, gpointer user)
{
	g_object_ref (G_OBJECT (item));
	if (select) {
		g_idle_add ((gpointer) select_idle_callback, item);
	}
	else {
		g_idle_add ((gpointer) deselect_idle_callback, item);
	}
}

// This function returns the position of the canvas item. It has
// nothing to do with where the wire is stored in the sheet node store.
void
wire_item_get_start_pos (WireItem *item, Coords *pos)
{
	g_return_if_fail (item != NULL);
	g_return_if_fail (IS_WIRE_ITEM (item));
	g_return_if_fail (pos != NULL);

	g_object_get (G_OBJECT (item),
	              "x", &pos->x,
	              "y", &pos->y,
	              NULL);
}

// This function returns the length of the canvas item.
void
wire_item_get_length (WireItem *item, Coords *pos)
{
	WireItemPriv *priv;
	GooCanvasPoints *points;

	g_return_if_fail (item != NULL);
	g_return_if_fail (IS_WIRE_ITEM (item));
	g_return_if_fail (pos != NULL);

	priv = item->priv;

	g_object_get (G_OBJECT (priv->line),
	              "points", &points,
	              NULL);

	// This is not strictly neccessary, since the first point is always
	// (0,0) but it's more correct and good to have if this changes in the
	// future.
	pos->x = points->coords[2] - points->coords[0];
	pos->y = points->coords[3] - points->coords[1];
	goo_canvas_points_unref (points);
}

static gboolean
is_in_area (SheetItem *object, Coords *p1, Coords *p2)
{
	WireItem *item;
	Coords bbox_start, bbox_end;

	item = WIRE_ITEM (object);

	get_boundingbox (item, &bbox_start, &bbox_end);

	if (p1->x < bbox_start.x &&
	    p2->x > bbox_end.x &&
	    p1->y < bbox_start.y &&
	    p2->y > bbox_end.y) {
			return TRUE;
	}
	return FALSE;
}

// Retrieves the bounding box. We use a caching scheme for this
// since it's too expensive to calculate it every time we need it.
inline static void
get_boundingbox (WireItem *item, Coords *p1, Coords *p2)
{
	WireItemPriv *priv;
	priv = item->priv;

	if (!priv->cache_valid) {
		Coords start_pos, end_pos;
		GooCanvasBounds bounds; //, canvas_bounds;

		goo_canvas_item_get_bounds (GOO_CANVAS_ITEM (item), &bounds);
		start_pos.x = bounds.x1;
		start_pos.y = bounds.y1;
		end_pos.x = bounds.x2;
		end_pos.y = bounds.y2;

		priv->bbox_start = start_pos;
		priv->bbox_end = end_pos;
		priv->cache_valid = TRUE;
	}

	memcpy (p1, &priv->bbox_start, sizeof (Coords));
	memcpy (p2, &priv->bbox_end, sizeof (Coords));
}

static void
wire_item_paste (Sheet *sheet, ItemData *data)
{
	g_return_if_fail (sheet != NULL);
	g_return_if_fail (IS_SHEET (sheet));
	g_return_if_fail (data != NULL);
	g_return_if_fail (IS_WIRE (data));

	sheet_add_ghost_item (sheet, data);
}

static void wire_traverse (Wire *wire);

static void
node_traverse (Node *node)
{
	GSList *wires;

	g_return_if_fail (node != NULL);
	g_return_if_fail (IS_NODE (node));

	if (node_is_visited (node))
		return;

	node_set_visited (node, TRUE);

	for (wires = node->wires; wires; wires = wires->next) {
		Wire *wire = wires->data;
		wire_traverse (wire);
	}
	g_slist_free_full (wires, g_object_unref);
}

static void
wire_traverse (Wire *wire)
{
	GSList *nodes;

	g_return_if_fail (wire != NULL);
	g_return_if_fail (IS_WIRE (wire));

	if (wire_is_visited (wire))
		return;

	wire_set_visited (wire, TRUE);

	g_signal_emit_by_name (wire, "highlight");

	for (nodes = wire_get_nodes (wire); nodes; nodes = nodes->next) {
		Node *node = nodes->data;

		node_traverse (node);
	}
	g_slist_free_full (nodes, g_object_unref);
}

static void
node_foreach_reset (gpointer key, gpointer value, gpointer user_data)
{
	Node *node = value;

	node_set_visited (node, FALSE);
}

static void
mouse_over_wire_callback (WireItem *item, Sheet *sheet)
{
	GList *wires;
	Wire *wire;
	NodeStore *store;

	if (sheet->state != SHEET_STATE_NONE)
		return;

	store = schematic_get_store (
	        	schematic_view_get_schematic_from_sheet (sheet));

	node_store_node_foreach (store, (GHFunc *) node_foreach_reset, NULL);
	for (wires = store->wires; wires; wires = wires->next) {
		wire = wires->data;
		wire_set_visited (wire, FALSE);
	}

	wire = WIRE (sheet_item_get_data (SHEET_ITEM (item)));
	wire_traverse (wire);
	g_list_free_full (wires, g_object_unref);
}

static void
highlight_wire_callback (Wire *wire, WireItem *item)
{
	WireItemPriv *priv = item->priv;

	g_object_set (priv->line,
	              "stroke-color", HIGHLIGHT_COLOR,
	              NULL);

	// Guard against removal during the highlighting.
	g_object_ref (G_OBJECT (item));

	g_timeout_add (1000, (gpointer) unhighlight_wire, item);
}

static int
unhighlight_wire (WireItem *item)
{
	char *color;
	WireItemPriv *priv = item->priv;

	color = sheet_item_get_selected (SHEET_ITEM (item)) ?
		SELECTED_COLOR : NORMAL_COLOR;

	g_object_set (priv->line,
	              "stroke-color", color,
	              NULL);

	g_object_unref (G_OBJECT (item));

	return FALSE;
}



//FIXME get rid of
static void
wire_moved_callback (ItemData *data, Coords *pos, SheetItem *item)
{
	//NG_DEBUG ("wire MOVED callback called - LEGACY");
}

static void
wire_item_place (SheetItem *item, Sheet *sheet)
{
	wire_item_signal_connect_placed (WIRE_ITEM (item), sheet);
}

//FIXME get rid of
static void
wire_item_place_ghost (SheetItem *item, Sheet *sheet)
{
//	wire_item_signal_connect_placed (WIRE_ITEM (item));
}


static void
wire_changed_callback (Wire *wire, WireItem *item)
{
	Coords start_pos, length;
	GooCanvasPoints *points;

	g_return_if_fail (wire != NULL);
	g_return_if_fail (IS_ITEM_DATA (wire));
	g_return_if_fail (item != NULL);
	g_return_if_fail (IS_WIRE_ITEM (item));


	wire_get_pos_and_length (wire, &start_pos, &length);

	// Move the canvas item and invalidate the bbox cache.
	goo_canvas_item_set_simple_transform (GOO_CANVAS_ITEM (item),
	                                      start_pos.x,
	                                      start_pos.y,
	                                      1.0,
	                                      0.0);
	item->priv->cache_valid = FALSE;

	points = goo_canvas_points_new (2);
	points->coords[0] = 0;
	points->coords[1] = 0;
	points->coords[2] = length.x;
	points->coords[3] = length.y;

	g_object_set (item->priv->line,
	              "points", points,
	              NULL);
	goo_canvas_points_unref (points);

	g_object_set (item->priv->resize1,
	              "x", -RESIZER_SIZE,
	              "y", -RESIZER_SIZE,
	              "width", 2 * RESIZER_SIZE,
	              "height", 2 * RESIZER_SIZE,
	              NULL);

	g_object_set (item->priv->resize2,
	              "x", length.x-RESIZER_SIZE,
	              "y", length.y-RESIZER_SIZE,
	              "width", 2 * RESIZER_SIZE,
	              "height", 2 * RESIZER_SIZE,
	              NULL);

	goo_canvas_item_request_update (GOO_CANVAS_ITEM (item->priv->line));
}

static void
wire_delete_callback (Wire *wire, WireItem *item)
{
// no clue why does work but canvas item does not disappear
//	g_clear_object (&item);
//	g_assert (item==NULL);
	goo_canvas_item_remove (GOO_CANVAS_ITEM (item));

}
