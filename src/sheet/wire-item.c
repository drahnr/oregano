/*
 * wire-item.c
 *
 *
 * Authors:
 *  Richard Hult <rhult@hem.passagen.se>
 *  Ricardo Markiewicz <rmarkie@fi.uba.ar>
 *  Andres de Barbara <adebarbara@fi.uba.ar>
 *
 * Web page: http://arrakis.lug.fi.uba.ar/
 *
 * Copyright (C) 1999-2001  Richard Hult
 * Copyright (C) 2003,2006  Ricardo Markiewicz
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
#include <gnome.h>
#include "cursors.h"
#include "sheet-private.h"
#include "sheet-pos.h"
#include "wire-item.h"
#include "node-store.h"
#include "wire.h"
#include "wire-private.h"

#define NORMAL_COLOR "blue"
#define SELECTED_COLOR "green"
#define HIGHLIGHT_COLOR "yellow"

#define RESIZER_SIZE 4.0f

static void wire_item_class_init (WireItemClass *klass);
static void wire_item_init (WireItem *item);
static void wire_item_set_arg (GtkObject *object, GtkArg *arg, guint arg_id);
static void wire_item_get_arg (GtkObject *object, GtkArg *arg, guint arg_id);
static void wire_item_destroy (GtkObject *object);
static void wire_item_moved (SheetItem *object);

static void wire_rotated_callback (ItemData *data, int angle,
	SheetItem *sheet_item);
static void wire_flipped_callback (ItemData *data, gboolean horizontal,
	SheetItem *sheet_item);
static void wire_moved_callback (ItemData *data, SheetPos *pos,
	SheetItem *item);
static void wire_changed_callback (Wire *, WireItem *item);
static void wire_delete_callback (Wire *, WireItem *item);

static void wire_item_paste (SchematicView *sv, ItemData *data);
static void selection_changed (WireItem *item, gboolean select,
	gpointer user_data);
static int select_idle_callback (WireItem *item);
static int deselect_idle_callback (WireItem *item);
static gboolean is_in_area (SheetItem *object, SheetPos *p1, SheetPos *p2);
inline static void get_bbox (WireItem *item, SheetPos *p1, SheetPos *p2);

static void mouse_over_wire_cb (WireItem *item, SchematicView *sv);
static void highlight_wire_cb (Wire *wire, WireItem *item);
static int unhighlight_wire (WireItem *item);

static void wire_item_place (SheetItem *item, SchematicView *sv);
static void wire_item_place_ghost (SheetItem *item, SchematicView *sv);

static SheetItemClass *wire_item_parent_class = NULL;

enum {
	WIRE_ITEM_ARG_0,
	WIRE_ITEM_ARG_NAME
};

enum {
	WIRE_RESIZER_NONE,
	WIRE_RESIZER_1,
	WIRE_RESIZER_2
};

struct _WireItemPriv {
	guint cache_valid : 1;
	guint resize_state;
	guint highlight : 1;
	WireDir direction;	   /* Direction of the wire. */

	GnomeCanvasLine *line;
	GnomeCanvasRect *resize1;
	GnomeCanvasRect *resize2;

	/*
	 * Cached bounding box. This is used to make
	 * the rubberband selection a bit faster.
	 */
	SheetPos bbox_start;
	SheetPos bbox_end;
};

GType
wire_item_get_type ()
{
	static GType wire_item_type = 0;

	if (!wire_item_type) {
		static const GTypeInfo wire_item_info = {
			sizeof (WireItemClass),
			NULL,
			NULL,
			(GClassInitFunc)wire_item_class_init,
			NULL,
			NULL,
			sizeof (WireItem),
			0,
			(GInstanceInitFunc)wire_item_init,
			NULL
		};

		wire_item_type = g_type_register_static(TYPE_SHEET_ITEM,
			"WireItem", &wire_item_info, 0);
	}
	return wire_item_type;
}

static void
wire_item_class_init (WireItemClass *wire_item_class)
{
	GObjectClass *object_class;
	GtkObjectClass *gtk_object_class;
	SheetItemClass *sheet_item_class;

	object_class = G_OBJECT_CLASS(wire_item_class);
	gtk_object_class = GTK_OBJECT_CLASS(wire_item_class);
	sheet_item_class = SHEET_ITEM_CLASS(wire_item_class);
	wire_item_parent_class = g_type_class_peek(TYPE_SHEET_ITEM);

	gtk_object_class->destroy = wire_item_destroy;

	sheet_item_class->moved = wire_item_moved;
	sheet_item_class->paste = wire_item_paste;
	sheet_item_class->is_in_area = is_in_area;
	sheet_item_class->selection_changed = (gpointer) selection_changed;
	sheet_item_class->place = wire_item_place;
	sheet_item_class->place_ghost = wire_item_place_ghost;
}

static void
wire_item_init (WireItem *item)
{
	WireItemPriv *priv;

	priv = g_new0 (WireItemPriv, 1);

	priv->direction = WIRE_DIR_NONE;
	priv->highlight = FALSE;
	priv->cache_valid = FALSE;

	item->priv = priv;
}

static void
wire_item_set_arg (GtkObject *object, GtkArg *arg, guint arg_id)
{
	WireItem *wire_item = WIRE_ITEM (object);

	wire_item = WIRE_ITEM (object);

	switch (arg_id) {
	case WIRE_ITEM_ARG_NAME:
		break;
	}
}

static void
wire_item_get_arg (GtkObject *object, GtkArg *arg, guint arg_id)
{
	WireItem *wire_item = WIRE_ITEM (object);

	wire_item = WIRE_ITEM (object);

	switch (arg_id) {
	case WIRE_ITEM_ARG_NAME:
		break;
	default:
		//arg->type = G_TYPE_INVALID;
		break;
	}
}

static void
wire_item_destroy (GtkObject *object)
{
	WireItem *wire;
	WireItemPriv *priv;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_WIRE_ITEM (object));

	wire = WIRE_ITEM (object);
	priv = wire->priv;

	if (priv) {
		if (priv->line) {
			/* TODO Check if destroy or unref have to be used for
			 * GnomeCanvasItem */
			gtk_object_destroy (GTK_OBJECT (priv->line));
			priv->line = NULL;
		}
		g_free (priv);
		wire->priv = NULL;
	}

	if (GTK_OBJECT_CLASS(wire_item_parent_class)->destroy){
		GTK_OBJECT_CLASS(wire_item_parent_class)->destroy(object);
	}
}

/**
 * "moved" signal handler. Invalidates the bounding box cache.
 */
static void
wire_item_moved (SheetItem *object)
{
	WireItem *item;
	WireItemPriv *priv;

	item = WIRE_ITEM (object);
	priv = item->priv;

	priv->cache_valid = FALSE;
}

WireItem *
wire_item_new (Sheet *sheet, Wire *wire)
{
	WireItem *item;
	GnomeCanvasPoints *points;
	WireItemPriv *priv;
	SheetPos start_pos, length;

	g_return_val_if_fail (sheet != NULL, NULL);
	g_return_val_if_fail (IS_SHEET (sheet), NULL);

	//g_object_ref (G_OBJECT(wire));
	/* XXX Ver si hay equivalente gtk_object_sink (GTK_OBJECT (wire)); */

	wire_get_pos_and_length (wire, &start_pos, &length);

	/*
	 * Because of the GnomeCanvasGroup inheritance, a small hack is needed
	 * here. The group starts at the startpoint of the wire, and the line
	 * goes from (0,0) to (length.x, length.y).
	 */
	item = WIRE_ITEM (gnome_canvas_item_new (
		sheet->object_group,
		wire_item_get_type (),
		"data", wire,
		"x", (double) start_pos.x,
		"y", (double) start_pos.y,
		NULL));

	priv = item->priv;

	priv->resize1 = GNOME_CANVAS_RECT (gnome_canvas_item_new (
		GNOME_CANVAS_GROUP (item),
		gnome_canvas_rect_get_type (),
		"x1", -RESIZER_SIZE,
		"y1", -RESIZER_SIZE,
		"x2", RESIZER_SIZE,
		"y2", RESIZER_SIZE,
		"fill_color", "red",
		"fill_color_rgba", 0x3cb37180,
		"outline_color", "blue",
		"width_pixels", 1,
		NULL));

	priv->resize2 = GNOME_CANVAS_RECT (gnome_canvas_item_new (
		GNOME_CANVAS_GROUP (item),
		gnome_canvas_rect_get_type (),
		"x1", length.x-RESIZER_SIZE,
		"y1", length.y-RESIZER_SIZE,
		"x2", length.x+RESIZER_SIZE,
		"y2", length.y+RESIZER_SIZE,
		"fill_color", "red",
		"fill_color_rgba", 0x3cb37180,
		"outline_color", "blue",
		"width_pixels", 1,
		NULL));
	gnome_canvas_item_hide (GNOME_CANVAS_ITEM (priv->resize1));
	gnome_canvas_item_hide (GNOME_CANVAS_ITEM (priv->resize2));

	points = gnome_canvas_points_new (2);
	points->coords[0] = 0;
	points->coords[1] = 0;
	points->coords[2] = length.x;
	points->coords[3] = length.y;

	priv->line = GNOME_CANVAS_LINE (gnome_canvas_item_new (
		GNOME_CANVAS_GROUP (item),
		gnome_canvas_line_get_type (),
		"points", points,
		"fill_color", "blue",
		"width_pixels", 1,
		NULL));

	gnome_canvas_points_free (points);

	g_signal_connect_object(G_OBJECT(wire),	"rotated",
		G_CALLBACK(wire_rotated_callback), G_OBJECT(item), 0);
	g_signal_connect_object(G_OBJECT(wire), "flipped",
		G_CALLBACK(wire_flipped_callback), G_OBJECT(item), 0);
	g_signal_connect_object(G_OBJECT(wire), "moved",
		G_CALLBACK(wire_moved_callback),  G_OBJECT(item), 0);

	g_signal_connect (G_OBJECT (wire), "changed", G_CALLBACK (wire_changed_callback), item);
	g_signal_connect (G_OBJECT (wire), "delete", G_CALLBACK (wire_delete_callback), item);
	wire_update_bbox (wire);

	return item;
}

static
int wire_item_event (WireItem *wire_item, const GdkEvent *event, SchematicView *sv)
{
	SheetPos start_pos, length;
	Wire *wire;
	Sheet *sheet;
	GnomeCanvas *canvas;
	static double last_x, last_y;
	double dx, dy, zoom;
	/* The selected group's bounding box in window resp. canvas coordinates. */
	double x1, y1, x2, y2;
	static double bb_x1, bb_y1, bb_x2, bb_y2;
	int cx1, cy1, cx2, cy2;
	double snapped_x, snapped_y;
	int sheet_width, sheet_height;
	SheetPos pos;

	sheet = schematic_view_get_sheet (sv);
	canvas = GNOME_CANVAS (sheet);
	g_object_get (G_OBJECT (wire_item), "data", &wire, NULL);

	wire_get_pos_and_length (WIRE (wire), &start_pos, &length);
	sheet_get_zoom (sheet, &zoom);

	switch (event->type) {
		case GDK_BUTTON_PRESS:
			switch (event->button.button) {
				case 1: {
					g_signal_stop_emission_by_name (G_OBJECT (sheet), "event");
					double x, y;
					x = event->button.x - start_pos.x;
					y = event->button.y - start_pos.y;
					if ((x > -RESIZER_SIZE) && (x < RESIZER_SIZE) &&
						 (y > -RESIZER_SIZE) && (y < RESIZER_SIZE)) {
						gtk_widget_grab_focus (GTK_WIDGET (sheet));
						sheet->state = SHEET_STATE_DRAG_START;
						wire_item->priv->resize_state = WIRE_RESIZER_1;

						last_x = event->button.x;
						last_y = event->button.y;
						item_data_unregister (ITEM_DATA (wire));
						return TRUE;
					}
					if ((x > (length.x-RESIZER_SIZE)) && (x < (length.x+RESIZER_SIZE)) &&
						 (y > (length.y-RESIZER_SIZE)) && (y < (length.y+RESIZER_SIZE))) {
						gtk_widget_grab_focus (GTK_WIDGET (sheet));
						sheet->state = SHEET_STATE_DRAG_START;
						wire_item->priv->resize_state = WIRE_RESIZER_2;

						last_x = event->button.x;
						last_y = event->button.y;
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

			if (sheet->state == SHEET_STATE_DRAG_START || sheet->state == SHEET_STATE_DRAG) {
				sheet->state = SHEET_STATE_DRAG;
		
				snapped_x = event->motion.x;
				snapped_y = event->motion.y;
				snap_to_grid (sheet->grid, &snapped_x, &snapped_y);
		
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
				} else {
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
				item_data_set_pos (sheet_item_get_data (SHEET_ITEM (wire_item)), &pos);
				wire_set_length (wire, &length);
				return TRUE;
			}
		break;
		case GDK_BUTTON_RELEASE:
			switch (event->button.button) {
			case 1:
				if (sheet->state != SHEET_STATE_DRAG &&
					sheet->state != SHEET_STATE_DRAG_START)
					break;
				if (wire_item->priv->resize_state == WIRE_RESIZER_NONE)
					break;

				g_signal_stop_emission_by_name (G_OBJECT (wire_item), "event");

				//gtk_timeout_remove (priv->scroll_timeout_id); // Esto no esta bien.

				sheet->state = SHEET_STATE_NONE;
				gnome_canvas_item_ungrab (GNOME_CANVAS_ITEM (wire_item), event->button.time);

				wire_item->priv->resize_state = WIRE_RESIZER_NONE;
				sheet->state = SHEET_STATE_NONE;
				item_data_register (ITEM_DATA (wire));
				return TRUE;
			}
			break;
		default:
			return sheet_item_event (SHEET_ITEM (wire_item), event, sv);
	}
	return sheet_item_event (SHEET_ITEM (wire_item), event, sv);
}

void
wire_item_signal_connect_placed (WireItem *wire, SchematicView *sv)
{
	g_signal_connect (
		G_OBJECT (wire),
		"event",
		G_CALLBACK(wire_item_event),
		sv);

	g_signal_connect (
		G_OBJECT (wire),
		"mouse_over",
		G_CALLBACK(mouse_over_wire_cb),
		sv);

	g_signal_connect (
		G_OBJECT (sheet_item_get_data (SHEET_ITEM (wire))),
		"highlight",
		G_CALLBACK(highlight_wire_cb),
		wire);
}

static void
wire_rotated_callback (ItemData *data, int angle, SheetItem *sheet_item)
{
	WireItem *wire_item;
	GnomeCanvasPoints *points;
	SheetPos start_pos, length;

	g_return_if_fail (sheet_item != NULL);
	g_return_if_fail (IS_WIRE_ITEM (sheet_item));

	wire_item = WIRE_ITEM (sheet_item);

	wire_get_pos_and_length (WIRE (data), &start_pos, &length);

	points = gnome_canvas_points_new (2);
	points->coords[0] = 0;
	points->coords[1] = 0;
	points->coords[2] = length.x;
	points->coords[3] = length.y;

	gnome_canvas_item_set (GNOME_CANVAS_ITEM (wire_item->priv->line),
		"points", points,
		NULL);
	gnome_canvas_points_unref (points);

	gnome_canvas_item_set (GNOME_CANVAS_ITEM (wire_item),
		"x", start_pos.x,
		"y", start_pos.y,
		NULL);

	gnome_canvas_item_set (
		GNOME_CANVAS_ITEM (wire_item-> priv->resize2),
		"x1", length.x-RESIZER_SIZE,
		"y1", length.y-RESIZER_SIZE,
		"x2", length.x+RESIZER_SIZE,
		"y2", length.y+RESIZER_SIZE,
		NULL
	);

	/*
	 * Invalidate the bounding box cache.
	 */
	wire_item->priv->cache_valid = FALSE;
}

static void
wire_flipped_callback (ItemData *data,
	gboolean horizontal, SheetItem *sheet_item)
{
	GnomeCanvasPoints *points;
	WireItem *item;
	WireItemPriv *priv;
	SheetPos start_pos, length;

	g_return_if_fail (sheet_item != NULL);
	g_return_if_fail (IS_WIRE_ITEM (sheet_item));

	item = WIRE_ITEM (sheet_item);
	priv = item->priv;

	wire_get_pos_and_length (WIRE (data), &start_pos, &length);

	points = gnome_canvas_points_new (2);
	points->coords[0] = 0;
	points->coords[1] = 0;
	points->coords[2] = length.x;
	points->coords[3] = length.y;

	gnome_canvas_item_set (GNOME_CANVAS_ITEM (item->priv->line),
			       "points", points,
			       NULL);
	gnome_canvas_points_unref (points);

	gnome_canvas_item_set (GNOME_CANVAS_ITEM (item),
			       "x", start_pos.x,
			       "y", start_pos.y,
			       NULL);

	/*
	 * Invalidate the bounding box cache.
	 */
	priv->cache_valid = FALSE;
}

static int
select_idle_callback (WireItem *item)
{
	WireItemPriv *priv = item->priv;

	gnome_canvas_item_set (GNOME_CANVAS_ITEM (priv->line),
		"fill_color", SELECTED_COLOR, NULL);

	priv->highlight = TRUE;

	g_object_unref (G_OBJECT (item));
	return FALSE;
}

static int
deselect_idle_callback (WireItem *item)
{
	WireItemPriv *priv = item->priv;

	gnome_canvas_item_set (GNOME_CANVAS_ITEM (priv->line),
		"fill_color", NORMAL_COLOR, NULL);

	priv->highlight = FALSE;

	g_object_unref(G_OBJECT (item));
	return FALSE;
}

static void
selection_changed(WireItem *item, gboolean select, gpointer user_data)
{
	g_object_ref(G_OBJECT(item));
	if (select) {
		gtk_idle_add ((gpointer) select_idle_callback, item);
		gnome_canvas_item_show (GNOME_CANVAS_ITEM (item->priv->resize1));
		gnome_canvas_item_show (GNOME_CANVAS_ITEM (item->priv->resize2));
	} else {
		gtk_idle_add ((gpointer) deselect_idle_callback, item);
		gnome_canvas_item_hide (GNOME_CANVAS_ITEM (item->priv->resize1));
		gnome_canvas_item_hide (GNOME_CANVAS_ITEM (item->priv->resize2));
	}
}

/**
 * This function returns the position of the canvas item. It has
 * nothing to do with where the wire is stored in the sheet node store.
 */
void
wire_item_get_start_pos (WireItem *item, SheetPos *pos)
{
	g_return_if_fail (item != NULL);
	g_return_if_fail (IS_WIRE_ITEM (item));
	g_return_if_fail (pos != NULL);

	g_object_get(G_OBJECT(item), "x", &pos->x, "y", &pos->y, NULL);
}

/**
 * This function returns the length of the canvas item.
 */
void
wire_item_get_length (WireItem *item, SheetPos *pos)
{
	WireItemPriv *priv;
	GnomeCanvasPoints *points;

	g_return_if_fail (item != NULL);
	g_return_if_fail (IS_WIRE_ITEM (item));
	g_return_if_fail (pos != NULL);

	priv = item->priv;

	g_object_get(G_OBJECT(priv->line), "points", &points, NULL);

	/*
	 * This is not strictly neccessary, since the first point is always
	 * (0,0) but it's more correct and good to have if this changes in the
	 * future.
	 */
	pos->x = points->coords[2] - points->coords[0];
	pos->y = points->coords[3] - points->coords[1];
	gnome_canvas_points_free (points);
}

static gboolean
is_in_area (SheetItem *object, SheetPos *p1, SheetPos *p2)
{
	WireItem *item;
	SheetPos bbox_start, bbox_end;

	item = WIRE_ITEM (object);

	get_bbox (item, &bbox_start, &bbox_end);

	if (p1->x < bbox_start.x &&
	    p2->x > bbox_end.x &&
	    p1->y < bbox_start.y &&
	    p2->y > bbox_end.y)
		return TRUE;

	return FALSE;
}

/**
 * Retrieves the bounding box. We use a caching scheme for this
 * since it's too expensive to calculate it every time we need it.
 */
inline static void
get_bbox (WireItem *item, SheetPos *p1, SheetPos *p2)
{
	WireItemPriv *priv;
	priv = item->priv;

	if (!priv->cache_valid) {
		SheetPos start_pos, end_pos;

		wire_item_get_start_pos (item, &start_pos);
		wire_item_get_length (item, &end_pos);
		end_pos.x += start_pos.x;
		end_pos.y += start_pos.y;

		priv->bbox_start.x = MIN (start_pos.x, end_pos.x);
		priv->bbox_start.y = MIN (start_pos.y, end_pos.y);
		priv->bbox_end.x = MAX (start_pos.x, end_pos.x);
		priv->bbox_end.y = MAX (start_pos.y, end_pos.y);
		priv->cache_valid = TRUE;
	}

	memcpy (p1, &priv->bbox_start, sizeof (SheetPos));
	memcpy (p2, &priv->bbox_end, sizeof (SheetPos));
}

static void
wire_item_paste (SchematicView *sv, ItemData *data)
{
	g_return_if_fail (sv != NULL);
	g_return_if_fail (IS_SCHEMATIC_VIEW (sv));
	g_return_if_fail (data != NULL);
	g_return_if_fail (IS_WIRE (data));

	schematic_view_add_ghost_item (sv, data);
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

	g_signal_emit_by_name(G_OBJECT (wire), "highlight");

	for (nodes = wire_get_nodes (wire); nodes; nodes = nodes->next) {
		Node *node = nodes->data;

		node_traverse (node);
	}
}

static void
node_foreach_reset (gpointer key, gpointer value, gpointer user_data)
{
	Node *node = value;

	node_set_visited (node, FALSE);
}

static void
mouse_over_wire_cb (WireItem *item, SchematicView *sv)
{
	GList *wires;
	Wire *wire;
	NodeStore *store;
	Sheet *sheet;

	sheet = schematic_view_get_sheet (sv);

	if (sheet->state != SHEET_STATE_NONE)
		return;

	store = schematic_get_store (schematic_view_get_schematic (sv));

	node_store_node_foreach (store, (GHFunc *) node_foreach_reset, NULL);
	for (wires = store->wires; wires; wires = wires->next) {
		wire = wires->data;
		wire_set_visited (wire, FALSE);
	}

	wire = WIRE (sheet_item_get_data (SHEET_ITEM (item)));
	wire_traverse (wire);
}

static void
highlight_wire_cb (Wire *wire, WireItem *item)
{
	WireItemPriv *priv = item->priv;

	gnome_canvas_item_set (GNOME_CANVAS_ITEM (priv->line),
		"fill_color", HIGHLIGHT_COLOR, NULL);

	/*
	 * Guard against removal during the highlighting.
	 */
	g_object_ref(G_OBJECT (item));

	gtk_timeout_add (1000, (gpointer) unhighlight_wire, item);
}

static int
unhighlight_wire (WireItem *item)
{
	char *color;
	WireItemPriv *priv = item->priv;

	color = sheet_item_get_selected (SHEET_ITEM (item)) ?
		SELECTED_COLOR : NORMAL_COLOR;

	gnome_canvas_item_set (GNOME_CANVAS_ITEM (priv->line),
		"fill_color", color, NULL);

	g_object_unref (G_OBJECT (item));

	return FALSE;
}

/**
 * This is called when the wire data was moved. Update the view accordingly.
 */
static void
wire_moved_callback (ItemData *data, SheetPos *pos, SheetItem *item)
{
	WireItem *wire_item;

	g_return_if_fail (data != NULL);
	g_return_if_fail (IS_ITEM_DATA (data));
	g_return_if_fail (item != NULL);
	g_return_if_fail (IS_WIRE_ITEM (item));

	if (pos == NULL)
		return;

	wire_item = WIRE_ITEM (item);

	/*
	 * Move the canvas item and invalidate the bbox cache.
	 */
	gnome_canvas_item_move (GNOME_CANVAS_ITEM (item), pos->x, pos->y);
	wire_item->priv->cache_valid = FALSE;
}

static void
wire_item_place (SheetItem *item, SchematicView *sv)
{
	wire_item_signal_connect_placed (WIRE_ITEM (item), sv);
}

static void
wire_item_place_ghost (SheetItem *item, SchematicView *sv)
{
//	wire_item_signal_connect_placed (WIRE_ITEM (item));
}


static void
wire_changed_callback (Wire *wire, WireItem *item)
{
	SheetPos start_pos, length;
	GnomeCanvasPoints *points;

	wire_get_pos_and_length (wire, &start_pos, &length);

	points = gnome_canvas_points_new (2);
	points->coords[0] = 0;
	points->coords[1] = 0;
	points->coords[2] = length.x;
	points->coords[3] = length.y;

	gnome_canvas_item_set (GNOME_CANVAS_ITEM (item->priv->line),
		"points", points,
		NULL);
	gnome_canvas_points_unref (points);

	gnome_canvas_item_set (GNOME_CANVAS_ITEM (item->priv->resize1),
		"x1", -RESIZER_SIZE,
		"y1", -RESIZER_SIZE,
		"x2", RESIZER_SIZE,
		"y2", RESIZER_SIZE,
		NULL);

	gnome_canvas_item_set (GNOME_CANVAS_ITEM (item->priv->resize2),
		"x1", length.x-RESIZER_SIZE,
		"y1", length.y-RESIZER_SIZE,
		"x2", length.x+RESIZER_SIZE,
		"y2", length.y+RESIZER_SIZE,
		NULL);
}

static void
wire_delete_callback (Wire *wire, WireItem *item)
{
	gtk_object_destroy (GTK_OBJECT (item));
}


