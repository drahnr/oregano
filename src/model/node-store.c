/*
 * node-store.c
 *
 * This is where the circuit elements (pins/wires) are stored.
 *
 * Authors:
 *  Richard Hult <rhult@hem.passagen.se>
 *  Ricardo Markiewicz <rmarkie@fi.uba.ar>
 *  Andres de Barbara <adebarbara@fi.uba.ar>
 *  Marc Lorber <lorber.marc@wanadoo.fr>
 *
 * Web page: https://github.com/marc-lorber/oregano
 *
 * Copyright (C) 1999-2001  Richard Hult
 * Copyright (C) 2003,2006 Ricardo Markiewicz
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

#include <glib.h>
#include <glib-object.h>
#include <gtk/gtk.h>
#include <math.h>

#include "node-store.h"
#include "node.h"
#include "part.h"
#include "wire.h"
#include "wire-private.h"

// NODE_EPSILON is used to check for intersection.
#define NODE_EPSILON 1e-10
// HASH_EPSILON is used in the hash equality check function.
#define HASH_EPSILON 1e-3
// Share an endpoint?
#define SEP(p1x,p1y,p2x,p2y) (IS_EQ(p1x, p2x) && IS_EQ(p1y, p2y))
// Equals?
#define IS_EQ(a,b) (fabs ((a) - (b)) < NODE_EPSILON)

#define ON_THE_WIRE(p1,start,end) (fabs((end.y-start.y)*(p1.x-start.x)-(end.x-start.x)*(p1.y-start.y))<NODE_EPSILON )

#define NG_DEBUG(s) if (0) g_print ("NG: %s\n", s)

static void	   node_store_class_init (NodeStoreClass *klass);
static void	   node_store_init (NodeStore *store);
static guint   node_hash (gconstpointer key);
static int	   node_equal (gconstpointer a, gconstpointer b);
static GSList *wires_intersect (NodeStore *store, double x1, double y1,
					double x2, double y2);
static GSList *wire_intersect_parts (NodeStore *store, Wire *wire);
static int     is_wire_at_pos (double x1, double y1, double x2, double y2,
					SheetPos pos);
static GSList *wires_at_pos (NodeStore *store, SheetPos pos);
static int	   do_wires_intersect (double Ax, double Ay, double Bx, double By,
					double Cx, double Cy, double Dx, double Dy, SheetPos *pos);
static void	   node_store_finalize (GObject *self);
static void	   node_store_dispose (GObject *self);

typedef struct {
	Wire *wire;
	SheetPos pos;
} IntersectionPoint;

enum {
	NODE_DOT_ADDED,
	NODE_DOT_REMOVED,
	LAST_SIGNAL
};

G_DEFINE_TYPE (NodeStore, node_store, G_TYPE_OBJECT)

static guint node_store_signals [LAST_SIGNAL] = { 0 };

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
node_store_dispose (GObject *self)
{
	G_OBJECT_CLASS (node_store_parent_class)->dispose (self);
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

NodeStore *
node_store_new (void)
{
	return NODE_STORE(g_object_new (TYPE_NODE_STORE, NULL));
}

static void
node_dot_added_callback (Node *node, SheetPos *pos, NodeStore *store)
{
	g_return_if_fail (store != NULL);
	g_return_if_fail (IS_NODE_STORE (store));

	g_signal_emit_by_name (G_OBJECT (store), "node_dot_added", pos);
}

static void
node_dot_removed_callback (Node *node, SheetPos *pos, NodeStore *store)
{
	g_return_if_fail (store != NULL);
	g_return_if_fail (IS_NODE_STORE (store));

	g_signal_emit_by_name (G_OBJECT (store), "node_dot_removed", pos);
}

Node *
node_store_get_or_create_node (NodeStore *self, SheetPos pos)
{
	Node *node;

	g_return_val_if_fail (self != NULL, NULL);
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

	// If there was a previously stored node here, just
	// return that node.
	return node;
}

int
node_store_add_part (NodeStore *self, Part *part)
{
	GSList *wire_list, *list;
	Node *node;
	SheetPos lookup_key;
	SheetPos part_pos;
	gdouble x, y;
	int i, num_pins;

	g_return_val_if_fail (self != NULL, FALSE);
	g_return_val_if_fail (IS_NODE_STORE (self), FALSE);
	g_return_val_if_fail (part != NULL, FALSE);
	g_return_val_if_fail (IS_PART (part), FALSE);

	num_pins = part_get_num_pins (part);

	item_data_get_pos (ITEM_DATA (part), &part_pos);

	for (i = 0; i < num_pins; i++) {
		Pin *pins;

		pins = part_get_pins (part);
		x = part_pos.x + pins[i].offset.x;
		y = part_pos.y + pins[i].offset.y;

		//Use the position of the pin as hash key.
		lookup_key.x = x;
		lookup_key.y = y;

		// Retrieve a node for this position.
		node = node_store_get_or_create_node (self, lookup_key);

		// Add all the wires that intersect this pin to the node store.
		wire_list = wires_at_pos (self, lookup_key);

		for (list = wire_list; list; list = list->next) {
			Wire *wire = list->data;

		    NG_DEBUG ("Add pin to wire.\n");

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

int
node_store_remove_part (NodeStore *self, Part *part)
{
	Node *node;
	SheetPos lookup_key;
	SheetPos pos;
	gdouble x, y;
	int i, num_pins;
	Pin *pins;

	g_return_val_if_fail (self != NULL, FALSE);
	g_return_val_if_fail (IS_NODE_STORE (self), FALSE);
	g_return_val_if_fail (part != NULL, FALSE);
	g_return_val_if_fail (IS_PART (part), FALSE);

	self->parts = g_list_remove (self->parts, part);
	self->items = g_list_remove (self->items, part);

	num_pins = part_get_num_pins (part);
	item_data_get_pos (ITEM_DATA (part), &pos);

	pins = part_get_pins (part);
	for (i = 0; i < num_pins; i++) {
		x = pos.x + pins[i].offset.x;
		y = pos.y + pins[i].offset.y;

		// Use the position of the pin as lookup key.
		lookup_key.x = x;
		lookup_key.y = y;

		node = g_hash_table_lookup (self->nodes, &lookup_key);
		if (node) {
			if (!node_remove_pin (node, &pins[i])) {
				g_warning ("Couldn't remove pin.");
				return FALSE;
			}

			// If the node is empty after removing the pin,
			// remove the node as well.
			if (node_is_empty (node)) {
				g_hash_table_remove (self->nodes, &lookup_key);
				g_object_unref (G_OBJECT (node));
			}
		} 
		else {
			return FALSE;
		}
	}

	return TRUE;
}

int
node_store_add_textbox (NodeStore *self, Textbox *text)
{
	g_object_set (G_OBJECT (text), "store", self, NULL);
	self->textbox = g_list_prepend (self->textbox, text);

	return TRUE;
}

int
node_store_remove_textbox (NodeStore *self, Textbox *text)
{
	self->textbox = g_list_remove (self->textbox, text);

	return TRUE;
}

int
node_store_add_wire (NodeStore *store, Wire *wire)
{
	gdouble x1, y1, x2, y2;
	GSList *ip_list, *list;
	IntersectionPoint *ipoint;
	Node *node;
	SheetPos pos, length;

	g_return_val_if_fail (store != NULL, FALSE);
	g_return_val_if_fail (IS_NODE_STORE (store), FALSE);
	g_return_val_if_fail (wire != NULL, FALSE);
	g_return_val_if_fail (IS_WIRE (wire), FALSE);

	wire_get_pos_and_length (wire, &pos, &length);

	x1 = pos.x;
	y1 = pos.y;
	x2 = x1 + length.x;
	y2 = y1 + length.y;

	// Check for intersection with other wires.
	ip_list = wires_intersect (store, x1, y1, x2, y2);

	for (list = ip_list; list; list = list->next) {
		ipoint = list->data;

		if (IS_EQ (x1, x2) && ((ipoint->pos.y == y1) || (ipoint->pos.y == y2))) {
			SheetPos w_pos, w_length;
			gboolean can_join;
			GSList *nodes;

			wire_get_pos_and_length (ipoint->wire, &w_pos, &w_length);
			gdouble _x1, _x2, _y1, _y2;

			_x1 = w_pos.x;
			_y1 = w_pos.y;
			_x2 = _x1 + w_length.x;
			_y2 = _y1 + w_length.y;

			can_join = TRUE;
			nodes = wire_get_nodes (wire);
			for (; nodes; nodes = nodes->next) {
				SheetPos p1;
				Node *node = (Node *)nodes->data;

				p1.x = _x1;
				p1.y = _y1;
				if ((fabs (node->key.x - p1.x) < 1e-3) && 
				    (fabs (node->key.y - p1.y) < 1e-3)){
					can_join = FALSE;
					break;
				}
				p1.x = _x2;
				p1.y = _y2;
				if ((fabs (node->key.x - p1.x) < 1e-3) && 
				    (fabs (node->key.y - p1.y) < 1e-3)){    
					can_join = FALSE;
					break;
				}
			}

			if (IS_EQ(_x1, _x2) && can_join) {
				if (w_pos.x < pos.x) pos.x = w_pos.x;
				if (w_pos.y < pos.y) pos.y = w_pos.y;
				length.x += w_length.x;
				length.y += w_length.y;

				// Update the new size and pos of the wire
				item_data_unregister (ITEM_DATA (ipoint->wire));
				wire_set_length (ipoint->wire, &length);
				item_data_set_pos (ITEM_DATA (ipoint->wire), &pos);
				wire_update_bbox (ipoint->wire);
				item_data_register (ITEM_DATA (ipoint->wire));

				// Done!, return -1 so wire is deleted
				return -1;
			}
		}
		else if (IS_EQ (y1, y2)          && 
		         ((ipoint->pos.x == x1)  || 
				  (ipoint->pos.x == x2))) {
			SheetPos w_pos, w_length;
			gboolean can_join;
			GSList *nodes;

			wire_get_pos_and_length (ipoint->wire, &w_pos, &w_length);
			gdouble _x1, _x2, _y1, _y2;

			_x1 = w_pos.x;
			_y1 = w_pos.y;
			_x2 = _x1 + w_length.x;
			_y2 = _y1 + w_length.y;

			can_join = TRUE;
			nodes = wire_get_nodes (wire);
			for (; nodes; nodes = nodes->next) {
				SheetPos p;
				Node *node = (Node *)nodes->data;

				p.x = _x1;
				p.y = _y1;
				if ((fabs (node->key.x - p.x) < 1e-3) && 
				    (fabs (node->key.y - p.y) < 1e-3)){  
					can_join = FALSE;
					break;
				}
				p.x = _x2;
				p.y = _y2;
				if ((fabs (node->key.x - p.x) < 1e-3) && 
				    (fabs (node->key.y - p.y) < 1e-3)){  
					can_join = FALSE;
					break;
				}
			}

			if (IS_EQ(_y1, _y2) && can_join) {
				if (w_pos.x < pos.x) pos.x = w_pos.x;
				if (w_pos.y < pos.y) pos.y = w_pos.y;
				length.x += w_length.x;
				length.y += w_length.y;

				// Update the new size and pos of the wire
				item_data_unregister (ITEM_DATA (ipoint->wire));
				wire_set_length (ipoint->wire, &length);
				item_data_set_pos (ITEM_DATA (ipoint->wire), &pos);
				wire_update_bbox (ipoint->wire);
				item_data_register (ITEM_DATA (ipoint->wire));

				// Done!, return -1 so wire si deleted
				return -1;
			}
		}

		node = node_store_get_or_create_node (store, ipoint->pos);

		// Add the wire, and also the wire that is intersected.
		node_add_wire (node, wire);
		node_add_wire (node, ipoint->wire);

		wire_add_node (wire, node);
		wire_add_node (ipoint->wire, node);

		NG_DEBUG ("Add wire to wire.\n");

		g_free (ipoint);
	}
	g_slist_free (ip_list);

	// Check for intersection with parts (pins).
	ip_list = wire_intersect_parts (store, wire);

	for (list = ip_list; list; list = list->next) {
		node = list->data;

		// Add the wire to the node (pin) that it intersected.
		node_add_wire (node, wire);
		wire_add_node (wire, node);

		NG_DEBUG ("Add wire to pin.\n");
	}

	g_slist_free (ip_list);

	g_object_set (G_OBJECT (wire), "store", store, NULL);
	store->wires = g_list_prepend (store->wires, wire);
	store->items = g_list_prepend (store->items, wire);

	return TRUE;
}

int
node_store_remove_wire (NodeStore *store, Wire *wire)
{
	GSList *list;
	SheetPos lookup_key, pos, length;

	g_return_val_if_fail (store != NULL, FALSE);
	g_return_val_if_fail (IS_NODE_STORE (store), FALSE);
	g_return_val_if_fail (wire != NULL, FALSE);
	g_return_val_if_fail (IS_WIRE (wire), FALSE);

	if (item_data_get_store (ITEM_DATA (wire)) == NULL) {
		g_warning ("Trying to remove non-stored wire.");
		return FALSE;
	}

	wire_get_pos_and_length (wire, &pos, &length);

	store->wires = g_list_remove (store->wires, wire);
	store->items = g_list_remove (store->items, wire);

	// If the nodes that this wire passes through will be
	// empty when the wire is removed, remove the node as well.

	// We must work on a copy of the nodes list, since it
	// changes as we remove nodes.
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

static GSList *
wire_intersect_parts (NodeStore *store, Wire *wire)
{
	GList *list;
	GSList *ip_list;
	Node *node;
	SheetPos lookup_pos;
	SheetPos part_pos, wire_pos, wire_length;
	Part *part;
	double x, y, wire_x1, wire_y1, wire_x2, wire_y2;
	int i, num_pins;

	g_return_val_if_fail (store != NULL, FALSE);
	g_return_val_if_fail (IS_NODE_STORE (store), FALSE);
	g_return_val_if_fail (wire != NULL, FALSE);
	g_return_val_if_fail (IS_WIRE (wire), FALSE);

	ip_list = NULL;

	wire_get_pos_and_length (wire, &wire_pos, &wire_length);

	wire_x1 = wire_pos.x;
	wire_x2 = wire_pos.x + wire_length.x;
	wire_y1 = wire_pos.y;
	wire_y2 = wire_pos.y + wire_length.y;

	// Go through all the parts and see which of their
	// pins that intersect the wire.
	for (list = store->parts; list; list = list->next) {
		part = list->data;

		num_pins = part_get_num_pins (part);
		item_data_get_pos (ITEM_DATA (part), &part_pos);

		for (i = 0; i < num_pins; i++) {
			Pin *pins;

			pins = part_get_pins (part);
			x = part_pos.x + pins[i].offset.x;
			y = part_pos.y + pins[i].offset.y;

			lookup_pos.x = x;
			lookup_pos.y = y;

			// If there is a wire at this pin's position,
			// add it to the return list.
			if (is_wire_at_pos (wire_x1, wire_y1, wire_x2, wire_y2, lookup_pos)) {
				node = node_store_get_node (store, lookup_pos);

				if (node != NULL)
					ip_list = g_slist_prepend (ip_list, node);
			}
		}
	}
	g_list_free_full (list, g_object_unref);

	return ip_list;
}

static GSList *
wires_at_pos (NodeStore *store, SheetPos pos)
{
	GList *list;
	GSList *wire_list;
	Wire *wire;
	SheetPos wire_pos, wire_length;
	double x1, y1, x2, y2;

	g_return_val_if_fail (store != NULL, FALSE);
	g_return_val_if_fail (IS_NODE_STORE (store), FALSE);

	wire_list = NULL;

	for (list = store->wires; list; list = list->next) {
		wire = list->data;

		wire_get_pos_and_length (wire, &wire_pos, &wire_length);
		x1 = wire_pos.x;
		y1 = wire_pos.y;
		x2 = x1 + wire_length.x;
		y2 = y1 + wire_length.y;

		if (is_wire_at_pos (x1, y1, x2, y2, pos))
			wire_list = g_slist_prepend (wire_list, wire);
	}
	g_list_free_full (list, g_object_unref);

	return wire_list;
}

int
node_store_is_wire_at_pos (NodeStore *store, SheetPos pos)
{
	GList *list;
	Wire *wire;
	SheetPos wire_pos, wire_length;
	double x1, y1, x2, y2;

	g_return_val_if_fail (store != NULL, FALSE);
	g_return_val_if_fail (IS_NODE_STORE (store), FALSE);

	for (list = store->wires; list; list = list->next) {
		wire = list->data;

		wire_get_pos_and_length (wire, &wire_pos, &wire_length);
		x1 = wire_pos.x;
		y1 = wire_pos.y;
		x2 = x1 + wire_length.x;
		y2 = y1 + wire_length.y;

		if (is_wire_at_pos (x1, y1, x2, y2, pos))
			return TRUE;
	}
	g_list_free_full (list, g_object_unref);

	return FALSE;
}

static GSList *
wires_intersect (NodeStore *store, double x1, double y1, double x2, double y2)
{
	GList *list;
	GSList *ip_list;
	Wire *wire;
	SheetPos pos, wire_pos, wire_length;
	double wire_x1, wire_y1, wire_x2, wire_y2;

	g_return_val_if_fail (store != NULL, FALSE);
	g_return_val_if_fail (IS_NODE_STORE (store), FALSE);

	// Search through all the wires. Is there a better way?
	ip_list = NULL;
	for (list = store->wires; list; list = list->next) {
		wire = list->data;

		wire_get_pos_and_length (wire, &wire_pos, &wire_length);
		wire_x1 = wire_pos.x;
		wire_y1 = wire_pos.y;
		wire_x2 = wire_x1 + wire_length.x;
		wire_y2 = wire_y1 + wire_length.y;

		if (do_wires_intersect (x1, y1, x2, y2, wire_x1, wire_y1,
								wire_x2, wire_y2, &pos)) {
			IntersectionPoint *ip;

			ip = g_new0 (IntersectionPoint, 1);

			ip->wire = wire;
			ip->pos = pos;
			ip_list = g_slist_prepend (ip_list, ip);
		}
	}
	g_list_free_full (list, g_object_unref);

	return ip_list;
}

// node_store_get_node
 
// Find the node that has an element at a certain position.
Node *
node_store_get_node (NodeStore *store, SheetPos pos)
{
	Node *node;

	g_return_val_if_fail (store != NULL, NULL);
	g_return_val_if_fail (IS_NODE_STORE (store), NULL);

	node = g_hash_table_lookup (store->nodes, &pos);

	if (!node) {
		NG_DEBUG (g_strdup_printf ("No node at (%g, %g)\n", pos.x, pos.y));}
	else {
		NG_DEBUG (g_strdup_printf ("Found node at (%g, %g)\n", pos.x, pos.y));}
	return node;
}

static guint
node_hash (gconstpointer key)
{
	SheetPos *sp = (SheetPos *) key;
	int x, y;

	// Hash on any other node?

	x = (int)rint (sp->x) % 256;
	y = (int)rint (sp->y) % 256;

	return (y << 8) | x;
}

static int
node_equal (gconstpointer a, gconstpointer b)
{
	SheetPos *spa, *spb;

	spa = (SheetPos *) a;
	spb = (SheetPos *) b;

	if (fabs (spa->y - spb->y) > HASH_EPSILON) {
		if (fabs (spa->y - spb->y) < 2.0)
			NG_DEBUG ("A neighbour of B in Y\n");

		return 0;
	}

	if (fabs (spa->x - spb->x) > HASH_EPSILON) {
		if (fabs (spa->x - spb->x) < 5.0)
			NG_DEBUG ("A neighbour of B in X\n\n");

		return 0;
	}

	return 1;
}

void
node_store_node_foreach (NodeStore *store, GHFunc *func, gpointer user_data)
{
	g_return_if_fail (store != NULL);
	g_return_if_fail (IS_NODE_STORE (store));

	g_hash_table_foreach (store->nodes, (gpointer)func, user_data);
}

static int
is_wire_at_pos (double x1, double y1, double x2, double y2, SheetPos pos)
{
	double k, x0, y0;

	x0 = pos.x;
	y0 = pos.y;

	if (!IS_EQ (x1, x2) && !IS_EQ (y1, y2)) {
		k = ((y2 - y1)) / ((x2 - x1));
		if (IS_EQ (y0, (y1 + k * (x0 - x1)))) {
			return TRUE;
		}
	} 
	else if (IS_EQ (x2, x1) && IS_EQ (x1, x0)) {
		if (y0 >= y1 && y0 <= y2) {
			return TRUE;
		}
	} 
	else if (IS_EQ (y2, y1) && IS_EQ (y1, y0)) {
		if (x0 >= x1 && x0 <= x2) {
			return TRUE;
		}
	}

	NG_DEBUG (g_strdup_printf ("no match: (%g %g) -> (%g %g), (%g %g)\n", x1, y1, x2, y2, pos.x, pos.y));

	return FALSE;
}

// Decides if two wires intersect. Note that wires that share an
// endpoint are considered intersecting each other. Intersection point
// is returned in pos.
static int
do_wires_intersect (double Ax, double Ay, double Bx, double By, double Cx,
					double Cy, double Dx, double Dy, SheetPos *pos)
{
	double r, s, d;

	// Wires don't intersect if they share an endpoint. NOTE: they do here...
	if (SEP (Ax, Ay, Cx, Cy)) { // same starting point
		pos->x = Ax;
		pos->y = Ay;
		return TRUE;
	} 
	else if (SEP (Ax, Ay, Dx, Dy)) { // 1st start == 2nd end
		pos->x = Ax;
		pos->y = Ay;
		return TRUE;
	} 
	else if (SEP (Bx, By, Cx, Cy)) { // 1st end == 2nd start
		pos->x = Bx;
		pos->y = By;
		return TRUE;
	} 
	else if (SEP (Bx, By, Dx, Dy)) { // 1st end == 2nd end
		pos->x = Bx;
		pos->y = By;
		return TRUE;
	}

	// Calculate the denominator.
	d = ((Bx - Ax) * (Dy - Cy) - (By - Ay) * (Dx - Cx));

	// We have two parallell wires if d = 0.
	if (fabs (d) < NODE_EPSILON) {
		return FALSE;
	}

	r = ((Ay - Cy) * (Dx - Cx) - (Ax - Cx) * (Dy - Cy));
	r = r / d;
	s = ((Ay - Cy) * (Bx - Ax) - (Ax - Cx) * (By - Ay)) / d;

	// Check for intersection, which we have for values of
	// r and s in [0, 1].
	if (r >= 0                  &&
	   (r - 1.0) < NODE_EPSILON &&
		s >= 0                  &&
	   (s - 1.0) < NODE_EPSILON) {

		// Calculate the intersection point.
		pos->x = Ax + r * (Bx - Ax);
		pos->y = Ay + r * (By - Ay);

		// to be accepted only if it coincides with the start or end
		// of any of the wires
		if ( SEP (pos->x,pos->y,Ax,Ay) ||
			 SEP (pos->x,pos->y,Bx,By) ||
			 SEP (pos->x,pos->y,Cx,Cy) ||
			 SEP (pos->x,pos->y,Dx,Dy) )
			return TRUE;
		else
		   	return FALSE;
	}

	return FALSE;
}

GList *
node_store_get_parts (NodeStore *store)
{
	g_return_val_if_fail (store != NULL, NULL);
	g_return_val_if_fail (IS_NODE_STORE (store), NULL);

	return store->parts;
}

GList *
node_store_get_wires (NodeStore *store)
{
	g_return_val_if_fail (store != NULL, NULL);
	g_return_val_if_fail (IS_NODE_STORE (store), NULL);

	return store->wires;
}

GList *
node_store_get_items (NodeStore *store)
{
	g_return_val_if_fail (store != NULL, NULL);
	g_return_val_if_fail (IS_NODE_STORE (store), NULL);

	return store->items;
}

static void
add_node (gpointer key, Node *node, GList **list)
{
	*list = g_list_prepend (*list, node);
}

static void
add_node_position (gpointer key, Node *node, GList **list)
{
	if (node_needs_dot (node))
		*list = g_list_prepend (*list, key);
}

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
	SheetPos p1, p2;

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
	g_list_free_full (list, g_object_unref);
}

gint
node_store_count_items (NodeStore *store, NodeRect *rect)
{
	GList *list;
	SheetPos p1, p2;
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
	g_list_free_full (list, g_object_unref);

	return n;
}

static void
draw_dot (SheetPos *key, Node *value, cairo_t *cr)
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
	g_list_free_full (list, g_object_unref);

	g_hash_table_foreach (store->nodes, (GHFunc)draw_dot, cr);
}

int
node_store_is_pin_at_pos (NodeStore *store, SheetPos pos)
{
	int num_pins;
	SheetPos part_pos;
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

			if ((x == pos.x) && (y == pos.y)) {
				return 1;
			}
		}
	}
	g_list_free_full (p, g_object_unref);
	return 0;
}
