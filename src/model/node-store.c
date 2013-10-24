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

#include "speedy.h"
#include "node-store.h"
#include "node.h"
#include "part.h"
#include "wire.h"
#include "wire-private.h"

#include "debug.h"


/* NODE_EPSILON is used to check for intersection. */
/* HASH_EPSILON is used in the hash equality check function. */
#define NODE_EPSILON 1e-10
#define HASH_EPSILON 1e-3

/* Share an endpoint? */
#define SEP(p1x,p1y,p2x,p2y) (IS_EQ(p1x, p2x) && IS_EQ(p1y, p2y))

/* Equals? */
#define IS_EQ(a,b) (fabs ((a) - (b)) < NODE_EPSILON)

#define ON_THE_WIRE(p1,start,end) (fabs((end.y-start.y)*(p1.x-start.x)-(end.x-start.x)*(p1.y-start.y))<NODE_EPSILON )

static void		node_store_class_init (NodeStoreClass *klass);
static void		node_store_init (NodeStore *store);
static guint		node_hash (gconstpointer key);
static gboolean		node_equal (gconstpointer a, gconstpointer b);
static GSList		*wire_intersect_parts (NodeStore *store, Wire *wire);
static gboolean		is_wire_at_pos (double x1, double y1, double x2, double y2, Coords pos, gboolean endpoints);
static gboolean		is_wire_at_coords(Wire *w, Coords *coo, gboolean endpoints);
static GSList *		wires_at_pos (NodeStore *store, Coords pos);
static gboolean	do_wires_intersect (Wire *a, Wire *b, Coords *where);
static gboolean 	do_wires_overlap (Wire *a, Wire *b);
static gboolean 	do_wires_have_common_endpoint (Wire *a, Wire *b);

static void		node_store_finalize (GObject *self);
static void		node_store_dispose (GObject *self);

typedef struct {
	Wire *wire;
	Coords pos;
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
int
node_store_add_part (NodeStore *self, Part *part)
{
	GSList *wire_list, *list;
	Node *node;
	Coords lookup_key;
	Coords part_pos;
	gdouble x, y;
	int i, num_pins;
	Pin *pins;

	NG_DEBUG ("-0-");
	g_return_val_if_fail (self, FALSE);
	g_return_val_if_fail (IS_NODE_STORE (self), FALSE);
	g_return_val_if_fail (part, FALSE);
	g_return_val_if_fail (IS_PART (part), FALSE);

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
		wire_list = wires_at_pos (self, lookup_key);

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
int
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
				g_warning ("Couldn't remove pin from node %p.", node);
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

int
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
 * attention: only ever call this for two parallel and overlapping wires!
 */
static Wire *
vulcanize_wire (NodeStore *store, Wire *a, Wire *b)
{
	Coords starta, enda;
	Coords startb, endb;
	GSList *list;

g_print ("Merge these 2...\n");
wire_dbg_print (a);
wire_dbg_print (b);
g_print ("... into ...\n");
	wire_get_start_pos (a, &starta);
	wire_get_end_pos (a, &enda);
	wire_get_start_pos (b, &startb);
	wire_get_end_pos (b, &endb);

	Coords start, end, len;
	start.x = MIN(MIN(starta.x, startb.x),MIN(enda.x,endb.x));
	start.y = MIN(MIN(starta.y, startb.y),MIN(enda.y,endb.y));
	end.x = MAX(MAX(starta.x, startb.x),MAX(enda.x,endb.x));
	end.y = MAX(MAX(starta.y, startb.y),MAX(enda.y,endb.y));
	len.x = end.x - start.x;
	len.y = end.y - start.y;
	g_print ("len=%lf,%lf\n", len.x, len.y);
	g_assert (fabs(len.x) < NODE_EPSILON || fabs(len.y) < NODE_EPSILON);
//FIXME register and unregister to new position
#define CREATE_NEW_WIRE 0
	// always null, or schematic_add_item in create_wire
	// will return pure bogus (and potentially crash!)
#if CREATE_NEW_WIRE
	Wire *w = wire_new (item_data_get_grid (ITEM_DATA (a)));
	if (!w)
		return NULL;
#else
	Wire *w = a;
#endif
	item_data_set_pos (ITEM_DATA (w), &start);
	wire_set_length (w, &len);

	for (list = wire_get_nodes(b); list;) {
		Node *n = list->data;
		list = list->next;
		if (!IS_NODE (n))
			g_warning ("Found bogus node entry in wire %p, ignored.", b);
		wire_add_node (w, n);
		node_add_wire (n, w);
	}
#if CREATE_NEW_WIRE
	node_store_remove_wire (store, a);//equiv wire_unregister
#endif
	node_store_remove_wire (store, b);//equiv wire_unregister
	g_print ("... this vulcanized: \n");
	wire_dbg_print (w);
	return w;
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



	// Check for overlapping with other wires.
	for (list = store->wires; list;	) {
		Wire *other = list->data;
		list = list->next;
		if (do_wires_overlap (wire, other)) {
			wire = vulcanize_wire (store, wire, other);
			if (wire) {
				list = store->wires; //round'n'round
			}
		}
	}

	// Check for intersection with other wires.
	for (list = store->wires; list;	list = list->next) {
		Coords where = {0., 0.};
		Wire *other = list->data;
		if (do_wires_intersect (wire, other, &where)) {
			node = node_store_get_or_create_node (store, where);

			node_add_wire (node, wire);
			node_add_wire (node, other);

			wire_add_node (wire, node);
			wire_add_node (other, node);

			g_print ("Add wire %p to wire %p @ %lf,%lf.\n", wire, other, where.x, where.y);
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
			if (is_wire_at_coords (wire, &lookup_pos, TRUE)) {
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



/*
 * returns a list of wire at position p (including endpoints)
 */
static GSList *
wires_at_pos (NodeStore *store, Coords pos)
{
	GList *list;
	GSList *wire_list;
	Wire *wire;
	Coords start, end;

	g_return_val_if_fail (store, FALSE);
	g_return_val_if_fail (IS_NODE_STORE (store), FALSE);

	wire_list = NULL;

	for (list = store->wires; list; list = list->next) {
		wire = list->data;

		if (is_wire_at_coords (wire, &pos, TRUE))
			wire_list = g_slist_prepend (wire_list, wire);
	}

	return wire_list;
}

/**
 * check if the two given wires have any incomon endpoints
 */
static gboolean
do_wires_have_common_endpoint (Wire *a, Wire *b)
{
	g_assert (a);
	g_assert (b);
	Coords delta1, start1, end1;
	Coords delta2, start2, end2;

	wire_get_start_pos (a, &start1);
	wire_get_end_pos (a, &end1);

	wire_get_start_pos (a, &start2);
	wire_get_end_pos (a, &end2);

	return coords_equal (&start1, &start2) ||
		   coords_equal (&start1, &end2) ||
		   coords_equal (&end1, &start2) ||
		   coords_equal (&end1, &end2);
}

/**
 * check if the two given wires overlap
 */
static gboolean
do_wires_overlap (Wire *a, Wire *b)
{
	g_assert (a);
	g_assert (b);
	Coords delta1, start1, end1;
	Coords delta2, start2, end2;

	wire_get_start_pos (a, &start1);
	wire_get_end_pos (a, &end1);

	wire_get_start_pos (b, &start2);
	wire_get_end_pos (b, &end2);

	delta1 = coords_sub (&end1, &start1);
	delta2 = coords_sub (&end2, &start2);

	// parallel check
	Coords n1;
	n1.x = +delta1.y;
	n1.y = -delta1.x;
	const gdouble dot = fabs(coords_dot (&n1,&delta2));
#if 0
	g_print ("Should be 0 if colinear %lf\n", dot);
	g_print ("%4.8lf,%4.8lf\n", delta1.x, delta1.y);
	g_print ("%4.8lf,%4.8lf\n", delta2.x, delta2.y);
#endif
	if (dot > NODE_EPSILON) {
		// they could intersect, but not overlap in a parallel fashion
		return FALSE;
	}


	// project onto wire, at least one has to succed
	// to prove real parallism
	// also the distance has to be 0
	const gdouble l1 = coords_euclid2 (&delta1);
	const gdouble l2 = coords_euclid2 (&delta2);
	gdouble lambda, d;
	Coords q, f;

#if 1
	//get distance from start2 ontop of wire *a
	q = coords_sub (&start2, &start1);
	lambda = coords_dot (&q, &delta1) / l1;
	f.x = start1.x + lambda * delta1.x - start2.x;
	f.y = start1.y + lambda * delta1.y - start2.y;
	d = coords_euclid2(&f);
	if (lambda >= -NODE_EPSILON &&
	    lambda-NODE_EPSILON <= 1. &&
	    d < NODE_EPSILON) {
		g_print ("###1 lambda=%lf ... d=%lf  ....  %lf,%lf\n", lambda, d, f.x, f.y);
		return TRUE;
	}

	//get distance from end2 ontop of wire *a
	q = coords_sub (&end2, &start1);
	lambda = coords_dot (&q, &delta1) / l1;
	f.x = start1.x + lambda * delta1.x - end2.y;
	f.y = start1.y + lambda * delta1.y - end2.y;
	d = coords_euclid2(&f);
	if (lambda >= -NODE_EPSILON &&
	    lambda-NODE_EPSILON <= 1. &&
	    d < NODE_EPSILON) {
		g_print ("###2 lambda=%lf ... d=%lf  ....  %lf,%lf\n", lambda, d, f.x, f.y);
		return TRUE;
	}

	//get distance from start1 ontop of wire *b
	q = coords_sub (&start1, &start2);
	lambda = coords_dot (&q, &delta2) / l2;
	f.x = start2.x + lambda * delta2.x - start1.x;
	f.y = start2.y + lambda * delta2.y - start1.y;
	d = coords_euclid2(&f);
	if (lambda >= -NODE_EPSILON &&
	    lambda-NODE_EPSILON <= 1. &&
	    d < NODE_EPSILON) {
		g_print ("###3 lambda=%lf ... d=%lf  ....  %lf,%lf\n", lambda, d, f.x, f.y);
		return TRUE;
	}

	//get distance from end1 ontop of wire *b
	q = coords_sub (&end1, &start2);
	lambda = coords_dot (&q, &delta2) / l2;
	f.x = start2.x + lambda * delta2.x - end1.x;
	f.y = start2.y + lambda * delta2.y - end1.y;
	d = coords_euclid2(&f);
	if (lambda >= -NODE_EPSILON &&
	    lambda-NODE_EPSILON <= 1. &&
	    d < NODE_EPSILON) {
		g_print ("###4 lambda=%lf ... d=%lf  ....  %lf,%lf\n", lambda, d, f.x, f.y);
		return TRUE;
	}
#endif
	return FALSE;
}

/*
 * check if 2 wires intersect
 */
gboolean
do_wires_intersect (Wire *a, Wire *b, Coords *where)
{
	g_assert (a);
	g_assert (b);
	Coords delta1, start1;
	Coords delta2, start2;
	wire_get_pos_and_length (a, &start1, &delta1);
	wire_get_pos_and_length (b, &start2, &delta2);

	// parallel check
	const gdouble d = coords_cross (&delta1, &delta2);
	wire_dbg_print (a);
	wire_dbg_print (b);
	if (fabs(d) < NODE_EPSILON) {
	    g_print ("XXXXXX do_wires_intersect(%p,%p): NO! d=%lf\n", a,b, d);
		return FALSE;
	}

	// implemented according to
	// http://stackoverflow.com/questions/563198/how-do-you-detect-where-two-line-segments-intersect
	const Coords qminusp = coords_sub (&start2, &start1);

	// p = start1, q = start2, r = delta1, s = delta2
	const gdouble t = coords_cross (&qminusp, &delta1) / d;
	const gdouble u = coords_cross (&qminusp, &delta2) / d;

	if (t >= -NODE_EPSILON && t-NODE_EPSILON <= 1.f &&
	    u >= -NODE_EPSILON && u-NODE_EPSILON <= 1.f) {
	    g_print ("XXXXXX do_wires_intersect(%p,%p): YES! t,u = %lf,%lf\n",a,b, t, u);
	    if (where) {
			where->x = start1.x + u * delta1.x;
			where->y = start1.y + u * delta1.y;
	    }
	    return TRUE;
	}
    g_print ("XXXXXX do_wires_intersect(%p,%p): NO! t,u = %lf,%lf\n",a,b, t, u);
	return FALSE;
}


/*
 * returns if there exists a wire at the target position
 * endpoints determines if endpoints are allowed and treated as positive
 */
gboolean
node_store_is_wire_at_pos_check_endpoints (NodeStore *store, Coords pos, gboolean endpoints)
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

		if (is_wire_at_pos (start.x, start.y, end.x, end.y, pos, endpoints))
			return TRUE;
	}
	return FALSE;
}


/*
 * convenience helper for the above
 * XXX to be removed
 */
inline gboolean
node_store_is_wire_at_pos (NodeStore *store, Coords pos)
{
	return node_store_is_wire_at_pos_check_endpoints (store, pos, TRUE);
}



/*
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

static inline guint
node_hash (gconstpointer key)
{
	Coords *sp = (Coords *) key;
	register int x, y;

	// Hash on any other node?

	x = (int)rint (sp->x) % 256;
	y = (int)rint (sp->y) % 256;

	return (y << 8) | x;
}

static int
node_equal (gconstpointer a, gconstpointer b)
{
	Coords *spa, *spb;

	spa = (Coords *) a;
	spb = (Coords *) b;

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

static inline gboolean
is_wire_at_coords(Wire *w, Coords *coo, gboolean endpoints)
{
	Coords p1, p2, len;
	wire_get_pos_and_length (w, &p1, &len);
	p2 = p1;
	coords_add (&p2, &len);
	return is_wire_at_pos (p1.x, p1.y, p2.x, p2.y, *coo, endpoints);
}

/**
 * evaluates if x1,y1 to x2,y2 contain pos.x/pos.y if connected by a birds eye line
 */
static inline gboolean
is_wire_at_pos (double x1, double y1, double x2, double y2, Coords pos, gboolean endpoints)
{
	//calculate the hessenormalform and check the results vs eps
	// 0 = (vx - vp0) dot (n)
	double d;
	Coords a, n;

	a.x = x2 - x1;
	a.y = y2 - y1;
	n.x = -a.y;
	n.y = +a.x;

	d = (pos.x - x1) * n.x + (pos.y - y1) * n.y;

	if (fabs(d) > NODE_EPSILON)
		return FALSE;

	if (endpoints) {
		if (pos.x<MIN(x1,x2) || pos.x>MAX(x1,x2) || pos.y<MIN(y1,y2) || pos.y>MAX(y1,y2))
			return FALSE;

	} else {
		if (pos.x<=MIN(x1,x2) || pos.x>=MAX(x1,x2) || pos.y<=MIN(y1,y2) || pos.y>=MAX(y1,y2))
			return FALSE;

	}

	NG_DEBUG ("on wire (DIST=%g): linear start:(%g %g); end:(%g %g); point:(%g %g)", d, x1, y1, x2, y2, pos.x, pos.y);

	return TRUE;
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
