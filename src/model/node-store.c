/*
 * node-store.c
 *
 *
 * Store nodes, wires to hashtables.
 * Generate new wires, merge new wire create requests where possible with already existing ones
 *
 *
 * Authors:
 *  Richard Hult <rhult@hem.passagen.se>
 *  Ricardo Markiewicz <rmarkie@fi.uba.ar>
 *  Andres de Barbara <adebarbara@fi.uba.ar>
 *  Marc Lorber <lorber.marc@wanadoo.fr>
 *  Bernhard Schuster <schuster.bernhard@gmail.com>
 *
 * Web page: https://srctwig.com/oregano
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
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <glib.h>
#include <glib-object.h>
#include <gtk/gtk.h>
#include <math.h>


#define NODE_EPSILON 1e-10
#define HASH_EPSILON 1e-3
#include "node-store.h"
#include "node-store-private.h"
#include "node.h"
#include "part.h"
#include "wire.h"
#include "wire-private.h"

#include "debug.h"


/* NODE_EPSILON is used to check for intersection. */
/* HASH_EPSILON is used in the hash equality check function. */

/* Share an endpoint? */
#define SEP(p1x,p1y,p2x,p2y) (IS_EQ(p1x, p2x) && IS_EQ(p1y, p2y))

/* Equals? */
#define IS_EQ(a,b) (fabs ((a) - (b)) < NODE_EPSILON)

#define ON_THE_WIRE(p1,start,end) (fabs((end.y-start.y)*(p1.x-start.x)-(end.x-start.x)*(p1.y-start.y))<NODE_EPSILON )

static void		node_store_class_init (NodeStoreClass *klass);
static void		node_store_init (NodeStore *store);
static void		node_store_finalize (GObject *self);
static void		node_store_dispose (GObject *self);

enum {
	NODE_DOT_ADDED,
	NODE_DOT_REMOVED,
	LAST_SIGNAL
};

G_DEFINE_TYPE (NodeStore, node_store, G_TYPE_OBJECT)

static guint node_store_signals [LAST_SIGNAL] = { 0 };

static void
node_store_dispose (GObject *self)
{
	G_OBJECT_CLASS (node_store_parent_class)->dispose (self);
}

static void
node_store_finalize (GObject *object)
{
	NodeStore *self = NODE_STORE (object);

	if (self->nodes) {
		g_hash_table_destroy (self->nodes);
		self->nodes = NULL;
	}

	if (self->wires) {
		g_list_free (self->wires);
		self->wires = NULL;
	}
	if (self->parts) {
		g_list_free (self->parts);
		self->parts = NULL;
	}
	if (self->items) {
		g_list_free (self->items);
		self->items = NULL;
	}

	G_OBJECT_CLASS (node_store_parent_class)->finalize (object);
}

static void
node_store_class_init (NodeStoreClass *klass)
{
	GObjectClass *gobject_class;

	gobject_class = G_OBJECT_CLASS (klass);
	node_store_parent_class = g_type_class_peek_parent (klass);

	gobject_class->finalize = node_store_finalize;
	gobject_class->dispose = node_store_dispose;

	node_store_signals [NODE_DOT_ADDED] =
		g_signal_new ("node_dot_added",
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (NodeStoreClass, node_dot_added),
		  NULL,
		  NULL,
		  g_cclosure_marshal_VOID__POINTER,
		  G_TYPE_NONE,
		  1, G_TYPE_POINTER);

	node_store_signals [NODE_DOT_REMOVED] =
		g_signal_new ("node_dot_removed",
		  TYPE_NODE_STORE,
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (NodeStoreClass, node_dot_removed),
		  NULL,
		  NULL,
		  g_cclosure_marshal_VOID__POINTER,
		  G_TYPE_NONE,
		  1, G_TYPE_POINTER);
}

static void
node_store_init (NodeStore *self)
{
	self->nodes = g_hash_table_new (node_hash, node_equal);
	self->wires = NULL;
	self->parts = NULL;
	self->items = NULL;
	self->textbox = NULL;
}

////////////////////////////////////////////////////////////////////////////////
//         END BOILERPLATE
////////////////////////////////////////////////////////////////////////////////

NodeStore *
node_store_new (void)
{
	return NODE_STORE(g_object_new (TYPE_NODE_STORE, NULL));
}

static void
node_dot_added_callback (Node *node, Coords *pos, NodeStore *store)
{
	g_return_if_fail (store != NULL);
	g_return_if_fail (IS_NODE_STORE (store));

	g_signal_emit_by_name (G_OBJECT (store), "node_dot_added", pos);
}

static void
node_dot_removed_callback (Node *node, Coords *pos, NodeStore *store)
{
	g_return_if_fail (store);
	g_return_if_fail (IS_NODE_STORE (store));

	g_signal_emit_by_name (G_OBJECT (store), "node_dot_removed", pos);
}

/**
 * lookup if a node at @pos exists, if so return it, otherwise
 * create one, add it to the nodestore and return it
 */
Node *
node_store_get_or_create_node (NodeStore *self, Coords pos)
{
	Node *node;

	g_return_val_if_fail (self, NULL);
	g_return_val_if_fail (IS_NODE_STORE (self), NULL);

	node = g_hash_table_lookup (self->nodes, &pos);

	if (!node) {
		// Create a node at (x, y) and return it.
		node = node_new (pos);

		g_signal_connect_object (G_OBJECT (node), "node_dot_added",
			G_CALLBACK (node_dot_added_callback), G_OBJECT (self), 0);

		g_signal_connect_object (G_OBJECT (node), "node_dot_removed",
			G_CALLBACK (node_dot_removed_callback), G_OBJECT (self), 0);

		g_hash_table_insert (self->nodes, &node->key, node);
	}

	return node;
}

/**
 * register a part to the nodestore
 */
gboolean
node_store_add_part (NodeStore *self, Part *part)
{
	NG_DEBUG ("-0-");
	g_return_val_if_fail (self, FALSE);
	g_return_val_if_fail (IS_NODE_STORE (self), FALSE);
	g_return_val_if_fail (part, FALSE);
	g_return_val_if_fail (IS_PART (part), FALSE);

	GSList *wire_list, *list;
	Node *node;
	Coords lookup_key;
	Coords part_pos;
	gdouble x, y;
	int i, num_pins;
	Pin *pins;

	num_pins = part_get_num_pins (part);
	pins = part_get_pins (part);

	item_data_get_pos (ITEM_DATA (part), &part_pos);

	for (i = 0; i < num_pins; i++) {
		x = part_pos.x + pins[i].offset.x;
		y = part_pos.y + pins[i].offset.y;

		//Use the position of the pin as hash key.
		lookup_key.x = x;
		lookup_key.y = y;

		// Retrieve a node for this position.
		node = node_store_get_or_create_node (self, lookup_key);

		// Add all the wires that intersect this pin to the node store.
		wire_list = get_wires_at_pos (self, lookup_key);

		for (list = wire_list; list; list = list->next) {
			Wire *wire = list->data;

			NG_DEBUG ("Add pin (node) %p to wire %p.\n", node, wire);

			node_add_wire (node, wire);
			wire_add_node (wire, node);
		}

		g_slist_free (wire_list);

		node_add_pin (node, &pins[i]);
	}

	g_object_set (G_OBJECT (part), "store", self, NULL);
	self->parts = g_list_prepend (self->parts, part);
	self->items = g_list_prepend (self->items, part);

	return TRUE;
}

/**
 * remove/unregister a part from the nodestore
 * this does _not_ free the part!
 */
gboolean
node_store_remove_part (NodeStore *self, Part *part)
{
	Node *node;
	Coords lookup_key;
	Coords pos;
	int i, num_pins;
	Pin *pins;

	g_return_val_if_fail (self, FALSE);
	g_return_val_if_fail (IS_NODE_STORE (self), FALSE);
	g_return_val_if_fail (part, FALSE);
	g_return_val_if_fail (IS_PART (part), FALSE);

	self->parts = g_list_remove (self->parts, part);
	self->items = g_list_remove (self->items, part);

	num_pins = part_get_num_pins (part);
	item_data_get_pos (ITEM_DATA (part), &pos);

	pins = part_get_pins (part);
	for (i = 0; i < num_pins; i++) {
		lookup_key.x = pos.x + pins[i].offset.x;
		lookup_key.y = pos.y + pins[i].offset.y;

		node = g_hash_table_lookup (self->nodes, &lookup_key);
		if (node) {
			if (!node_remove_pin (node, &pins[i])) {
				g_warning ("Could not remove pin[%i] from node %p.", i, node);
				return FALSE;
			}

			// If the node is empty after removing the pin,
			// remove the node as well.
			if (node_is_empty (node)) {
				g_hash_table_remove (self->nodes, &lookup_key);
				g_object_unref (G_OBJECT (node));
			}
		} else {
			return FALSE;
		}
	}

	return TRUE;
}

gboolean
node_store_add_textbox (NodeStore *self, Textbox *text)
{
	g_object_set (G_OBJECT (text), "store", self, NULL);
	self->textbox = g_list_prepend (self->textbox, text);

	return TRUE;
}

gboolean
node_store_remove_textbox (NodeStore *self, Textbox *text)
{
	self->textbox = g_list_remove (self->textbox, text);

	return TRUE;
}



/**
 * add/register the wire to the nodestore
 */
gboolean
node_store_add_wire (NodeStore *store, Wire *wire)
{
	GList *list;
	Node *node;
	int i=0;

	g_return_val_if_fail (store, FALSE);
	g_return_val_if_fail (IS_NODE_STORE (store), FALSE);
	g_return_val_if_fail (wire, FALSE);
	g_return_val_if_fail (IS_WIRE (wire), FALSE);



	// Check for intersection with other wires.
	for (list = store->wires; list;	list = list->next) {
		Coords where = {-77.77, -77.77};
		Wire *other = list->data;
		if (other==NULL)
			continue;
		if (do_wires_intersect (wire, other, &where)) {
			if (is_t_crossing (wire, other, &where) || is_t_crossing (other, wire, &where)) {

				node = node_store_get_or_create_node (store, where);

				node_add_wire (node, wire);
				node_add_wire (node, other);

				wire_add_node (wire, node);
				wire_add_node (other, node);

				NG_DEBUG ("Add wire %p to wire %p @ %lf,%lf.\n", wire, other, where.x, where.y);
			} else {
				// magic node removal if a x crossing is overlapped with another wire
				node = node_store_get_node (store, where);
				NG_DEBUG ("Nuke that node [ %p ] at coords inbetween", node);
				if (node) {
					Coords c[4];
					wire_get_start_and_end_pos (other, c+0, c+1);
					wire_get_start_and_end_pos (wire, c+2, c+3);
					if (!coords_equal (&where, c+0) &&
					    !coords_equal (&where, c+1) &&
					    !coords_equal (&where, c+2) &&
					    !coords_equal (&where, c+3)) {

						wire_remove_node (wire, node);
						wire_remove_node (other, node);
						node_remove_wire (node, wire);
						node_remove_wire (node, other);
					}
				}
			}
		}
	}

	// Check for overlapping with other wires.
	for (list = store->wires; list;	list = list->next) {
		Wire *other = list->data;
		Coords so, eo;
		const gboolean overlap = do_wires_overlap (wire, other, &so, &eo);
		NG_DEBUG ("overlap [ %p] and [ %p ] -- %s", wire, other, overlap==TRUE ? "YES" : "NO" );
		if (overlap) {
			Node *sn = node_store_get_node (store, eo);
			Node *en = node_store_get_node (store, so);
			#if 1
			wire = vulcanize_wire (store, wire, other, &so, &eo);
			NG_DEBUG ("overlapping of %p with %p ", wire, other);
			#else
			if (!sn && !en) {
				wire = vulcanize_wire (store, wire, other, &so, &eo);
			} else if (!sn) {
				NG_DEBUG ("do_something(TM) : %p sn==NULL ", other);
			} else if (!en) {
				NG_DEBUG ("do_something(TM) : %p en==NULL ", other);
			} else {
				NG_DEBUG ("do_something(TM) : %p else ", other);
			}
			#endif
		} else {
			NG_DEBUG ("not of %p with %p ", wire, other);
		}
	}

	// Check for intersection with parts (pins).
	for (list = store->parts; list; list = list->next) {
		Coords part_pos;
		Part *part;
		gint num_pins = -1;

		part = list->data;

		num_pins = part_get_num_pins (part);
		item_data_get_pos (ITEM_DATA (part), &part_pos);

		// Go through all the parts and see which of their
		// pins that intersect the wire.
		for (i = 0; i < num_pins; i++) {
			Pin *pins;
			Coords lookup_pos;

			pins = part_get_pins (part);
			lookup_pos.x = part_pos.x + pins[i].offset.x;
			lookup_pos.y = part_pos.y + pins[i].offset.y;

			// If there is a wire at this pin's position,
			// add it to the return list.
			if (is_wire_at_coords (wire, &lookup_pos)) {
				Node *node;
				node = node_store_get_node (store, lookup_pos);

				if (node != NULL) {
					// Add the wire to the node (pin) that it intersected.
					node_add_wire (node, wire);
					wire_add_node (wire, node);
					NG_DEBUG ("Add wire %p to pin (node) %p.\n", wire, node);
				} else {
					g_warning ("Bug: Found no node at pin at (%g %g).\n",
					           lookup_pos.x, lookup_pos.y);

				}
			}
		}
	}

	g_object_set (G_OBJECT (wire), "store", store, NULL);
	store->wires = g_list_prepend (store->wires, wire);
	store->items = g_list_prepend (store->items, wire);

	return TRUE;
}


/**
 * removes/unregisters a wire from the nodestore
 * this does _not_ free the wire itself!
 */
gboolean
node_store_remove_wire (NodeStore *store, Wire *wire)
{
	GSList *list;
	Coords lookup_key;

	g_return_val_if_fail (store, FALSE);
	g_return_val_if_fail (IS_NODE_STORE (store), FALSE);
	g_return_val_if_fail (wire, FALSE);
	g_return_val_if_fail (IS_WIRE (wire), FALSE);

	if (item_data_get_store (ITEM_DATA (wire)) == NULL) {
		g_warning ("Trying to remove not-stored wire %p.", wire);
		return FALSE;
	}

	store->wires = g_list_remove (store->wires, wire);
	store->items = g_list_remove (store->items, wire);

	// If the nodes that this wire passes through will be
	// empty when the wire is removed, remove the node as well.

	// FIXME if done properly, a list copy is _not_ necessary
	list = g_slist_copy (wire_get_nodes (wire));
	for (; list; list = list->next) {
		Node *node = list->data;

		lookup_key = node->key;

		node_remove_wire (node, wire);
		wire_remove_node (wire, node);

		if (node_is_empty (node))
			g_hash_table_remove (store->nodes, &lookup_key);
	}

	g_slist_free (list);

	return TRUE;
}







/**
 * @returns if there exists a wire at the target position which is registered to the nodestore
 */
gboolean
node_store_is_wire_at_pos (NodeStore *store, Coords pos)
{
	GList *list;
	Wire *wire;
	Coords start, end;

	g_return_val_if_fail (store, FALSE);
	g_return_val_if_fail (IS_NODE_STORE (store), FALSE);

	for (list = store->wires; list; list = list->next) {
		wire = list->data;

		wire_get_start_pos (wire, &start);
		wire_get_end_pos (wire, &end);

		if (is_line_segment_at_pos (&start, &end, &pos))
			return TRUE;
	}
	return FALSE;
}


/**
 * Find the node that has an element at a certain position.
 */
Node *
node_store_get_node (NodeStore *store, Coords pos)
{
	Node *node;

	g_return_val_if_fail (store != NULL, NULL);
	g_return_val_if_fail (IS_NODE_STORE (store), NULL);

	node = g_hash_table_lookup (store->nodes, &pos);

	if (!node){
		NG_DEBUG ("No node at (%g, %g)", pos.x, pos.y);
	} else {
		NG_DEBUG ("Found node at (%g, %g)", pos.x, pos.y);
	}
	return node;
}



/**
 * Call GHFunc for each node in the nodestore
 */
void
node_store_node_foreach (NodeStore *store, GHFunc *func, gpointer user_data)
{
	g_return_if_fail (store != NULL);
	g_return_if_fail (IS_NODE_STORE (store));

	g_hash_table_foreach (store->nodes, (gpointer)func, user_data);
}




/**
 * [transfer-none]
 */
GList *
node_store_get_parts (NodeStore *store)
{
	g_return_val_if_fail (store != NULL, NULL);
	g_return_val_if_fail (IS_NODE_STORE (store), NULL);

	return store->parts;
}

/**
 * [transfer-none]
 */
GList *
node_store_get_wires (NodeStore *store)
{
	g_return_val_if_fail (store != NULL, NULL);
	g_return_val_if_fail (IS_NODE_STORE (store), NULL);

	return store->wires;
}

/**
 * [transfer-none]
 */
GList *
node_store_get_items (NodeStore *store)
{
	g_return_val_if_fail (store != NULL, NULL);
	g_return_val_if_fail (IS_NODE_STORE (store), NULL);

	return store->items;
}



/**
 * the caller has to free the list himself, but not the actual data items!
 */
GList *
node_store_get_node_positions (NodeStore *store)
{
	GList *result;

	g_return_val_if_fail (store != NULL, NULL);
	g_return_val_if_fail (IS_NODE_STORE (store), NULL);

	result = NULL;
	g_hash_table_foreach (store->nodes, (GHFunc) add_node_position, &result);

	return result;
}

/**
 * the caller has to free the list himself, but not the actual data items!
 */
GList *
node_store_get_nodes (NodeStore *store)
{
	GList *result;

	g_return_val_if_fail (store != NULL, NULL);
	g_return_val_if_fail (IS_NODE_STORE (store), NULL);

	result = NULL;
	g_hash_table_foreach (store->nodes, (GHFunc) add_node, &result);

	return result;
}

void
node_store_get_bounds (NodeStore *store, NodeRect *rect)
{
	GList *list;
	Coords p1, p2;

	g_return_if_fail (store != NULL);
	g_return_if_fail (IS_NODE_STORE (store));
	g_return_if_fail (rect != NULL);

	rect->x0 = G_MAXDOUBLE;
	rect->y0 = G_MAXDOUBLE;
	rect->x1 = -G_MAXDOUBLE;
	rect->y1 = -G_MAXDOUBLE;

	for (list = store->items; list; list = list->next) {
		item_data_get_absolute_bbox (ITEM_DATA (list->data), &p1, &p2);

		rect->x0 = MIN (rect->x0, p1.x);
		rect->y0 = MIN (rect->y0, p1.y);
		rect->x1 = MAX (rect->x1, p2.x);
		rect->y1 = MAX (rect->y1, p2.y);
	}
}

gint
node_store_count_items (NodeStore *store, NodeRect *rect)
{
	GList *list;
	Coords p1, p2;
	ItemData *data;
	gint n;

	g_return_val_if_fail (store != NULL, 0);
	g_return_val_if_fail (IS_NODE_STORE (store), 0);

	if (rect == NULL)
		return g_list_length (store->items);

	for (list = store->items, n = 0; list; list = list->next) {
		data = ITEM_DATA (list->data);
		item_data_get_absolute_bbox (data, &p1, &p2);
		if (p1.x <= rect->x1 && p1.y <= rect->y1 &&
			p2.x >= rect->x0 && p2.y >= rect->y0) {
			n++;
		}
	}

	return n;
}

static void
draw_dot (Coords *key, Node *value, cairo_t *cr)
{
	if (node_needs_dot (value)) {
		cairo_save (cr);
		cairo_set_source_rgb (cr, 0.0, 0.0, 0.0);
		cairo_translate (cr, key->x, key->y);
		cairo_scale (cr, 1.0, 1.0);
		cairo_arc (cr, 0.0, 0.0, 1.0, 0.0, 2 * M_PI);
		cairo_fill (cr);
		cairo_restore (cr);
	}
}

void
node_store_print_items (NodeStore *store, cairo_t *cr, SchematicPrintContext *ctx)
{
	GList *list;
	ItemData *data;

	g_return_if_fail (store != NULL);
	g_return_if_fail (IS_NODE_STORE (store));

	cairo_set_line_join (cr, CAIRO_LINE_JOIN_ROUND);
	for (list = store->items; list; list = list->next) {
		data = ITEM_DATA (list->data);
		item_data_print (data, cr, ctx);
	}

	g_hash_table_foreach (store->nodes, (GHFunc)draw_dot, cr);
}


gboolean
node_store_is_pin_at_pos (NodeStore *store, Coords pos)
{
	int num_pins;
	Coords part_pos;
	GList *p;
	Part *part;
	Pin *pins;
	int i;
	gdouble x, y;

	for (p = store->parts; p; p = p->next) {
		part = PART (p->data);

		num_pins = part_get_num_pins (part);

		item_data_get_pos (ITEM_DATA (part), &part_pos);

		pins = part_get_pins (part);
		for (i = 0; i < num_pins; i++) {
			x = part_pos.x + pins[i].offset.x;
			y = part_pos.y + pins[i].offset.y;

			if (fabs(x-pos.x)<NODE_EPSILON && fabs(y-pos.y)<NODE_EPSILON)
				return TRUE;
		}
	}
	return FALSE;
}
