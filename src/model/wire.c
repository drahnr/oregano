/*
 * wire.c
 *
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

#include <goocanvas.h>
#include <math.h>

#include "node-store.h"
#include "node.h"
#include "wire.h"
#include "wire-private.h"
#include "clipboard.h"
#include "schematic-print-context.h"

static void      wire_class_init (WireClass *klass);
static void      wire_init (Wire *wire);
static void      wire_copy (ItemData *dest, ItemData *src);
static ItemData *wire_clone (ItemData *src);
static void      wire_rotate (ItemData *data, int angle, SheetPos *center);
static void      wire_flip (ItemData *data, gboolean horizontal,
					SheetPos *center);
static void      wire_unregister (ItemData *data);
static int       wire_register (ItemData *data);
static gboolean  wire_has_properties (ItemData *data);
static void      wire_print (ItemData *data, cairo_t *cr,
                    SchematicPrintContext *ctx);

#define NG_DEBUG(s) if (0) g_print ("%s\n", s)

enum {
	ARG_0,
	ARG_CHANGED,
	ARG_DELETE,
	ARG_LAST_SIGNAL
};

G_DEFINE_TYPE (Wire, wire, TYPE_ITEM_DATA)

static guint wire_signals [ARG_LAST_SIGNAL] = { 0 };
static ItemDataClass *parent_class = NULL;

static void
wire_finalize (GObject *object)
{
	Wire *wire = WIRE (object);
	WirePriv *priv = wire->priv;

	if (priv) {
		g_slist_free (priv->nodes);
		g_free (priv);
		wire->priv = NULL;
	}

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
wire_dispose (GObject *object)
{
	G_OBJECT_CLASS (parent_class)->dispose (object);
}


static void
wire_class_init (WireClass *klass)
{
	GObjectClass *object_class;
	ItemDataClass *item_data_class;

	parent_class = g_type_class_peek (TYPE_ITEM_DATA);
	item_data_class = ITEM_DATA_CLASS (klass);
	object_class = G_OBJECT_CLASS (klass);

	object_class->dispose = wire_dispose;
	object_class->finalize = wire_finalize;

	wire_signals [ARG_CHANGED] = g_signal_new ("changed", 
		    G_TYPE_FROM_CLASS (object_class),
			G_SIGNAL_RUN_FIRST,
			G_STRUCT_OFFSET (WireClass, changed),
			NULL,
			NULL,
			g_cclosure_marshal_VOID__VOID,
			G_TYPE_NONE,
			0);

	wire_signals [ARG_DELETE] = g_signal_new ("delete", 
		    G_TYPE_FROM_CLASS (object_class),
			G_SIGNAL_RUN_FIRST,
			G_STRUCT_OFFSET (WireClass, delete),
			NULL,
			NULL,
			g_cclosure_marshal_VOID__VOID,
			G_TYPE_NONE,
			0);

	item_data_class->clone = wire_clone;
	item_data_class->copy = wire_copy;
	item_data_class->rotate = wire_rotate;
	item_data_class->flip = wire_flip;
	item_data_class->unreg = wire_unregister;
	item_data_class->reg = wire_register;
	item_data_class->has_properties = wire_has_properties;
	item_data_class->print = wire_print;
}

static void
wire_init (Wire *wire)
{
	WirePriv *priv = g_new0 (WirePriv, 1);

	// For debugging purposes.
	priv->length.x = -1;
	priv->length.y = -1;

	priv->nodes = NULL;
	priv->visited = FALSE;
	priv->direction = WIRE_DIR_NONE;

	wire->priv = priv;
}

Wire *
wire_new (void)
{
	return WIRE (g_object_new (TYPE_WIRE, NULL));
}

gint
wire_add_node (Wire *wire, Node *node)
{
	WirePriv *priv;

	g_return_val_if_fail (wire != NULL, FALSE);
	g_return_val_if_fail (IS_WIRE (wire), FALSE);
	g_return_val_if_fail (node != NULL, FALSE);
	g_return_val_if_fail (IS_NODE (node), FALSE);

	priv = wire->priv;

	if (g_slist_find (priv->nodes, node)) {
		return FALSE;
	}

	priv->nodes = g_slist_prepend (priv->nodes, node);
	return TRUE;
}

gint
wire_remove_node (Wire *wire, Node *node)
{
	WirePriv *priv;

	g_return_val_if_fail (wire != NULL, FALSE);
	g_return_val_if_fail (IS_WIRE (wire), FALSE);
	g_return_val_if_fail (node != NULL, FALSE);
	g_return_val_if_fail (IS_NODE (node), FALSE);

	priv = wire->priv;

	if (!g_slist_find (priv->nodes, node)) {
		return FALSE;
	}

	priv->nodes = g_slist_remove (priv->nodes, node);

	return TRUE;
}

GSList *
wire_get_nodes (Wire *wire)
{
	WirePriv *priv;

	g_return_val_if_fail (wire != NULL, FALSE);
	g_return_val_if_fail (IS_WIRE (wire), FALSE);

	priv = wire->priv;

	return priv->nodes;
}

void
wire_get_start_pos (Wire *wire, SheetPos *pos)
{
	g_return_if_fail (wire != NULL);
	g_return_if_fail (IS_WIRE (wire));
	g_return_if_fail (pos != NULL);

	item_data_get_pos (ITEM_DATA (wire), pos);
}

void
wire_get_end_pos (Wire *wire, SheetPos *pos)
{
	WirePriv *priv;

	g_return_if_fail (wire != NULL);
	g_return_if_fail (IS_WIRE (wire));
	g_return_if_fail (pos != NULL);

	priv = wire->priv;

	item_data_get_pos (ITEM_DATA (wire), pos);

	pos->x += priv->length.x;
	pos->y += priv->length.y;
}

void
wire_get_pos_and_length (Wire *wire, SheetPos *pos, SheetPos *length)
{
	WirePriv *priv;

	g_return_if_fail (wire != NULL);
	g_return_if_fail (IS_WIRE (wire));
	g_return_if_fail (pos != NULL);

	priv = wire->priv;

	item_data_get_pos (ITEM_DATA (wire), pos);
	*length = priv->length;
}

void
wire_set_length (Wire *wire, SheetPos *length)
{
	WirePriv *priv;

	g_return_if_fail (wire != NULL);
	g_return_if_fail (IS_WIRE (wire));

	priv = wire->priv;

	priv->length = *length;

	if (length->x == 0) {
		wire->priv->direction = WIRE_DIR_VERT;
	} 
	else if (length->y == 0) {
		wire->priv->direction = WIRE_DIR_HORIZ;
	} 
	else {
		wire->priv->direction = WIRE_DIR_DIAG;
	}

	g_signal_emit_by_name (G_OBJECT (wire), 
	                       "changed");
}

gint
wire_is_visited (Wire *wire)
{
	WirePriv *priv;

	g_return_val_if_fail (wire != NULL, FALSE);
	g_return_val_if_fail (IS_WIRE (wire), FALSE);

	priv = wire->priv;

	return priv->visited;
}

void
wire_set_visited (Wire *wire, gboolean is_visited)
{
	WirePriv *priv;

	g_return_if_fail (wire != NULL);
	g_return_if_fail (IS_WIRE (wire));

	priv = wire->priv;

	priv->visited = is_visited;
}

static ItemData *
wire_clone (ItemData *src)
{
	Wire *new_wire;
	ItemDataClass *id_class;

	g_return_val_if_fail (src != NULL, NULL);
	g_return_val_if_fail (IS_WIRE (src), NULL);

	id_class = ITEM_DATA_CLASS (G_OBJECT_GET_CLASS(src));
	if (id_class->copy == NULL)
		return NULL;

	new_wire = g_object_new (TYPE_WIRE, NULL);
	id_class->copy (ITEM_DATA (new_wire), src);

	return ITEM_DATA (new_wire);
}

static void
wire_copy (ItemData *dest, ItemData *src)
{
	Wire *dest_wire, *src_wire;

	g_return_if_fail (dest != NULL);
	g_return_if_fail (IS_WIRE (dest));
	g_return_if_fail (src != NULL);
	g_return_if_fail (IS_WIRE (src));

	if (parent_class->copy != NULL)
		parent_class->copy (dest, src);

	dest_wire = WIRE (dest);
	src_wire = WIRE (src);

	dest_wire->priv->nodes = NULL;
	dest_wire->priv->length = src_wire->priv->length;
}

static void
wire_rotate (ItemData *data, int angle, SheetPos *center_pos)
{
	cairo_matrix_t affine;
	double dx, dy, x, y;
	Wire *wire;
	WirePriv *priv;
	SheetPos b1, b2;
	SheetPos wire_center_before, wire_center_after, delta;

	g_return_if_fail (data != NULL);
	g_return_if_fail (IS_WIRE (data));

	if (angle == 0)
		return;

	wire = WIRE (data);

	if (center_pos) {
		item_data_get_absolute_bbox (ITEM_DATA (wire), &b1, &b2);
		wire_center_before.x = (b1.x + b2.x) / 2;
		wire_center_before.y = (b1.y + b2.y) / 2;
	}

	priv = wire->priv;

	if (priv->direction == WIRE_DIR_VERT) {
		priv->direction = WIRE_DIR_HORIZ;
	} 
	else if (priv->direction == WIRE_DIR_HORIZ) {
		priv->direction = WIRE_DIR_VERT;
	}

	cairo_matrix_init_rotate (&affine, (double) angle * M_PI / 180.0);

	// Rotate the wire's end point.
	x = priv->length.x;
	y = priv->length.y;
	
	cairo_matrix_transform_point (&affine, &x, &y);

	if (fabs (x) < 1e-2)
		x = 0.0;
	if (fabs (y) < 1e-2)
		y = 0.0;

	// 'Normalize'.
	if (y < 0 || (y == 0 && x < 0)) {
		priv->length.x = -x;
		priv->length.y = -y;
		delta.x = -x;
		delta.y = -y;

		item_data_move (ITEM_DATA (wire), &delta);
	} 
	else {
		priv->length.x = x;
		priv->length.y = y;
	}

	// Let the views (canvas items) know about the rotation.
	g_signal_emit_by_name (G_OBJECT (wire), "rotated", angle);

	// Update bounding box.
	wire_update_bbox (wire);

	if (center_pos) {
		SheetPos wire_pos;

		item_data_get_absolute_bbox (ITEM_DATA (wire), &b1, &b2);

		wire_center_after.x = (b1.x + b2.x) / 2;
		wire_center_after.y = (b1.y + b2.y) / 2;

		dx = wire_center_before.x - wire_center_after.x;
		dy = wire_center_before.y - wire_center_after.y;

		item_data_get_pos (ITEM_DATA (wire), &wire_pos);

		x = wire_center_before.x - center_pos->x;
		y = wire_center_before.y - center_pos->y;
		dx = dx - x;
		dy = dy - y;
		cairo_matrix_transform_point (&affine, &x, &y);

		delta.x = dx + x + b1.x; 
		delta.y = dy + y + b1.y; 

		item_data_move (ITEM_DATA (wire), &delta);
	}
}

static void
wire_flip (ItemData *data, gboolean horizontal, SheetPos *center)
{
	// Do nothing!	
	return;
}

void
wire_update_bbox (Wire *wire)
{
	SheetPos b1, b2, pos, length;

	wire_get_pos_and_length (wire, &pos, &length);

	b1.x = b1.y = 0.0;
	b2 = length;

	item_data_set_relative_bbox (ITEM_DATA (wire), &b1, &b2);
}

static gboolean
wire_has_properties (ItemData *data)
{
	return FALSE;
}

static void
wire_unregister (ItemData *data)
{
	NodeStore *store;

	g_return_if_fail (IS_WIRE (data));

	store = item_data_get_store (data);
	node_store_remove_wire (store, WIRE (data));
}

static int 
wire_register (ItemData *data)
{
	NodeStore *store;

	g_return_val_if_fail (IS_WIRE (data), -1);

	store = item_data_get_store (data);
	return node_store_add_wire (store, WIRE (data));
}

static void
wire_print (ItemData *data, cairo_t *cr, SchematicPrintContext *ctx)
{
	SheetPos start_pos, end_pos;
	Wire *wire;

	g_return_if_fail (data != NULL);
	g_return_if_fail (IS_WIRE (data));

	wire = WIRE (data);

	wire_get_start_pos (wire, &start_pos);
	wire_get_end_pos (wire, &end_pos);

	cairo_save (cr);
		gdk_cairo_set_source_color (cr, &ctx->colors.wires);
		cairo_move_to (cr, start_pos.x, start_pos.y);
		cairo_line_to (cr, end_pos.x, end_pos.y);
		cairo_stroke (cr);
	cairo_restore (cr);
}

void wire_delete (Wire *wire)
{
	g_return_if_fail (IS_WIRE (wire));

	g_signal_emit_by_name (G_OBJECT (wire), "delete");
}
