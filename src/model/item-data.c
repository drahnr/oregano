/*
 * item-data.c
 *
 * ItemData object: part and wire model superclass.
 *
 * Author:
 *  Richard Hult <rhult@hem.passagen.se>
 *  Ricardo Markiewicz <rmarkie@fi.uba.ar>
 *  Andres de Barbara <adebarbara@fi.uba.ar>
 *  Marc Lorber <lorber.marc@wanadoo.fr>
 *  Bernhard Schuster <bernhard@ahoi.io>
 *
 * Web page: https://ahoi.io/project/oregano
 *
 * Copyright (C) 1999-2001  Richard Hult
 * Copyright (C) 2003,2006  Ricardo Markiewicz
 * Copyright (C) 2009-2012  Marc Lorber
 * Copyright (C) 2013       Bernhard Schuster
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

#include <glib.h>
#include <goocanvas.h>

#include "item-data.h"
#include "node-store.h"

#include "debug.h"

static void item_data_class_init (ItemDataClass *klass);
static void item_data_init (ItemData *item_data);
static void item_data_set_gproperty (GObject *object, guint prop_id, const GValue *value,
                                     GParamSpec *spec);
static void item_data_get_gproperty (GObject *object, guint prop_id, GValue *value,
                                     GParamSpec *spec);
static void item_data_copy (ItemData *dest, ItemData *src);

enum { ARG_0, ARG_STORE, ARG_POS };

enum {
	MOVED,
	ROTATED,
	FLIPPED,
	CHANGED, // used to notify the view to reset and recalc the transformation
	HIGHLIGHT,
	LAST_SIGNAL
};

struct _ItemDataPriv
{
	NodeStore *store;

	// modificator matrices
	cairo_matrix_t translate;
	cairo_matrix_t rotate;
	cairo_matrix_t flip;

	// Bounding box
	GooCanvasBounds bounds;
};

G_DEFINE_TYPE (ItemData, item_data, G_TYPE_OBJECT);

static guint item_data_signals[LAST_SIGNAL] = {0};

static void item_data_init (ItemData *item_data)
{
	ItemDataPriv *priv;

	priv = g_slice_new0 (ItemDataPriv);

	priv->bounds.x1 = priv->bounds.x2 = priv->bounds.y1 = priv->bounds.y2 = 0;

	cairo_matrix_init_identity (&(priv->translate));
	cairo_matrix_init_identity (&(priv->rotate));
	cairo_matrix_init_identity (&(priv->flip));

	item_data->priv = priv;
}

static void item_data_dispose (GObject *object)
{
	ItemDataPriv *priv = ITEM_DATA (object)->priv;
	// Remove the item from the sheet node store if there.
	if (priv->store) {
		item_data_unregister (ITEM_DATA (object));
	}
	g_slice_free (ItemDataPriv, priv);
	G_OBJECT_CLASS (item_data_parent_class)->dispose (object);
}

static void item_data_finalize (GObject *object)
{
	g_return_if_fail (object != NULL);
	G_OBJECT_CLASS (item_data_parent_class)->finalize (object);
}

static void item_data_class_init (ItemDataClass *klass)
{
	GObjectClass *object_class;

	item_data_parent_class = g_type_class_peek_parent (klass);

	object_class = G_OBJECT_CLASS (klass);

	// This assignment must be  performed before the call
	// to g_object_class_install_property
	object_class->set_property = item_data_set_gproperty;
	object_class->get_property = item_data_get_gproperty;

	g_object_class_install_property (
	    object_class, ARG_STORE,
	    g_param_spec_pointer ("store", "ItemData::store", "the store data", G_PARAM_READWRITE));

	g_object_class_install_property (
	    object_class, ARG_POS,
	    g_param_spec_pointer ("pos", "ItemData::pos", "the pos data", G_PARAM_READWRITE));

	object_class->dispose = item_data_dispose;
	object_class->finalize = item_data_finalize;
	item_data_signals[MOVED] =
	    g_signal_new ("moved", G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_FIRST,
	                  G_STRUCT_OFFSET (ItemDataClass, moved), NULL, NULL,
	                  g_cclosure_marshal_VOID__POINTER, G_TYPE_NONE, 1, G_TYPE_POINTER);

	item_data_signals[ROTATED] =
	    g_signal_new ("rotated", G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_FIRST, 0, NULL,
	                  NULL, g_cclosure_marshal_VOID__INT, G_TYPE_NONE, 1, G_TYPE_INT);

	item_data_signals[FLIPPED] =
	    g_signal_new ("flipped", G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_FIRST, 0, NULL,
	                  NULL, g_cclosure_marshal_VOID__INT, G_TYPE_NONE, 1, G_TYPE_INT);

	item_data_signals[CHANGED] =
	    g_signal_new ("changed", G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_FIRST, 0, NULL,
	                  NULL, g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

	item_data_signals[HIGHLIGHT] =
	    g_signal_new ("highlight", G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_FIRST, 0, NULL,
	                  NULL, g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

	// Methods.
	klass->clone = NULL;
	klass->copy = item_data_copy;
	klass->rotate = NULL;
	klass->flip = NULL;
	klass->reg = NULL;
	klass->unreg = NULL;
	klass->changed = NULL;

	// Signals.
	klass->moved = NULL;
}

////////////////////////////////////////////////////////////////////////////////
// END BOILER PLATE
////////////////////////////////////////////////////////////////////////////////

ItemData *item_data_new (void)
{
	ItemData *item_data;

	item_data = ITEM_DATA (g_object_new (item_data_get_type (), NULL));

	return item_data;
}

static void item_data_set_gproperty (GObject *object, guint prop_id, const GValue *value,
                                     GParamSpec *spec)
{
	ItemData *item_data = ITEM_DATA (object);

	switch (prop_id) {
	case ARG_STORE:
		item_data->priv->store = g_value_get_pointer (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (item_data, prop_id, spec);
	}
}

static void item_data_get_gproperty (GObject *object, guint prop_id, GValue *value,
                                     GParamSpec *spec)
{
	ItemData *item_data = ITEM_DATA (object);

	switch (prop_id) {
	case ARG_STORE:
		g_value_set_pointer (value, item_data->priv->store);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (item_data, prop_id, spec);
	}
}

/**
 * returns the top left corner of the item
 */
void item_data_get_pos (ItemData *item_data, Coords *pos)
{
	g_return_if_fail (item_data != NULL);
	g_return_if_fail (IS_ITEM_DATA (item_data));
	g_return_if_fail (pos != NULL);
	ItemDataPriv *priv;
	priv = item_data->priv;
	pos->x = priv->translate.x0;
	pos->y = priv->translate.y0;
}

void item_data_set_pos (ItemData *item_data, Coords *pos)
{
	ItemDataPriv *priv;
	gboolean handler_connected;

	g_return_if_fail (pos);
	g_return_if_fail (item_data);
	g_return_if_fail (IS_ITEM_DATA (item_data));

	priv = item_data->priv;

	cairo_matrix_init_translate (&(priv->translate), pos->x, pos->y);

	handler_connected =
	    g_signal_handler_is_connected (G_OBJECT (item_data), item_data->moved_handler_id);
	if (handler_connected) {
		g_signal_emit_by_name (G_OBJECT (item_data), "moved", pos);
	}
	handler_connected =
	    g_signal_handler_is_connected (G_OBJECT (item_data), item_data->changed_handler_id);
	if (handler_connected) {
		g_signal_emit_by_name (G_OBJECT (item_data), "changed");
	}
}

void item_data_move (ItemData *item_data, const Coords *delta)
{
	ItemDataPriv *priv;

	g_return_if_fail (item_data != NULL);
	g_return_if_fail (IS_ITEM_DATA (item_data));

	if (delta == NULL)
		return;

	if (fabs (delta->x) < 1e-2 && fabs (delta->y) < 1e-2)
		return;

	priv = item_data->priv;
	cairo_matrix_translate (&(priv->translate), delta->x, delta->y);

	g_signal_emit_by_name (G_OBJECT (item_data), "changed");
}

// NodeStore *
gpointer item_data_get_store (ItemData *item_data)
{
	g_return_val_if_fail (item_data != NULL, NULL);
	g_return_val_if_fail (IS_ITEM_DATA (item_data), NULL);

	return item_data->priv->store;
}

ItemData *item_data_clone (ItemData *src)
{
	ItemDataClass *id_class;

	g_return_val_if_fail (src != NULL, NULL);
	g_return_val_if_fail (IS_ITEM_DATA (src), NULL);

	id_class = ITEM_DATA_CLASS (G_OBJECT_GET_CLASS (src));
	if (id_class->clone == NULL)
		return NULL;

	return id_class->clone (src);
}

static void item_data_copy (ItemData *dest, ItemData *src)
{
	g_return_if_fail (dest != NULL);
	g_return_if_fail (IS_ITEM_DATA (dest));
	g_return_if_fail (src != NULL);
	g_return_if_fail (IS_ITEM_DATA (src));

	dest->priv->translate = src->priv->translate;
	dest->priv->rotate = src->priv->rotate;
	dest->priv->flip = src->priv->flip;
	dest->priv->store = NULL;
}

void item_data_get_relative_bbox (ItemData *data, Coords *p1, Coords *p2)
{
	g_return_if_fail (data != NULL);
	g_return_if_fail (IS_ITEM_DATA (data));

	if (p1) {
		p1->x = data->priv->bounds.x1;
		p1->y = data->priv->bounds.y1;
	}

	if (p2) {
		p2->x = data->priv->bounds.x2;
		p2->y = data->priv->bounds.y2;
	}
}

void item_data_get_absolute_bbox (ItemData *data, Coords *p1, Coords *p2)
{
	g_return_if_fail (data != NULL);
	g_return_if_fail (IS_ITEM_DATA (data));

	ItemDataPriv *priv;

	item_data_get_relative_bbox (data, p1, p2);
	priv = data->priv;

	if (p1) {
		p1->x += priv->translate.x0;
		p1->y += priv->translate.y0;
	}

	if (p2) {
		p2->x += priv->translate.x0;
		p2->y += priv->translate.y0;
	}
}

void item_data_set_relative_bbox (ItemData *data, Coords *p1, Coords *p2)
{
	g_return_if_fail (data != NULL);
	g_return_if_fail (IS_ITEM_DATA (data));

	if (p1) {
		data->priv->bounds.x1 = p1->x;
		data->priv->bounds.y1 = p1->y;
	}

	if (p2) {
		data->priv->bounds.x2 = p2->x;
		data->priv->bounds.y2 = p2->y;
	}
}

void item_data_list_get_absolute_bbox (GList *item_data_list, Coords *p1, Coords *p2)
{
	GList *iter;
	Coords b1, b2;

	if (item_data_list == NULL)
		return;

	item_data_get_absolute_bbox (item_data_list->data, p1, p2);

	for (iter = item_data_list; iter; iter = iter->next) {
		if (G_UNLIKELY (iter->data == NULL))
			continue;
		item_data_get_absolute_bbox (iter->data, &b1, &b2);

		if (p1) {
			p1->x = MIN (p1->x, b1.x);
			p1->y = MIN (p1->y, b1.y);
		}

		if (p2) {
			p2->x = MAX (p2->x, b2.x);
			p2->y = MAX (p2->y, b2.y);
		}
	}
}

void item_data_rotate (ItemData *data, int angle, Coords *center)
{
	ItemDataClass *id_class;

	g_return_if_fail (data != NULL);
	g_return_if_fail (IS_ITEM_DATA (data));

	id_class = ITEM_DATA_CLASS (G_OBJECT_GET_CLASS (data));
	if (id_class->rotate) {
		id_class->rotate (data, angle, center);
	}
}

void item_data_flip (ItemData *data, IDFlip direction, Coords *center)
{
	ItemDataClass *id_class;

	g_return_if_fail (data != NULL);
	g_return_if_fail (IS_ITEM_DATA (data));

	id_class = ITEM_DATA_CLASS (G_OBJECT_GET_CLASS (data));
	if (id_class->flip) {
		id_class->flip (data, direction, center);
	}
}

void item_data_unregister (ItemData *data)
{
	ItemDataClass *id_class;

	g_return_if_fail (data != NULL);
	g_return_if_fail (IS_ITEM_DATA (data));

	id_class = ITEM_DATA_CLASS (G_OBJECT_GET_CLASS (data));
	if (id_class->unreg) {
		id_class->unreg (data);
	}
}

gboolean item_data_register (ItemData *data)
{
	ItemDataClass *id_class;

	g_return_val_if_fail (data != NULL, FALSE);
	g_return_val_if_fail (IS_ITEM_DATA (data), FALSE);

	id_class = ITEM_DATA_CLASS (G_OBJECT_GET_CLASS (data));
	if (id_class->reg) {
		return id_class->reg (data);
	}
	return FALSE;
}

char *item_data_get_refdes_prefix (ItemData *data)
{
	ItemDataClass *id_class;

	g_return_val_if_fail (data != NULL, NULL);
	g_return_val_if_fail (IS_ITEM_DATA (data), NULL);

	id_class = ITEM_DATA_CLASS (G_OBJECT_GET_CLASS (data));
	if (id_class->get_refdes_prefix) {
		return id_class->get_refdes_prefix (data);
	}

	return NULL;
}

void item_data_set_property (ItemData *data, char *property, char *value)
{
	ItemDataClass *id_class;

	g_return_if_fail (data != NULL);
	g_return_if_fail (IS_ITEM_DATA (data));

	id_class = ITEM_DATA_CLASS (G_OBJECT_GET_CLASS (data));
	if (id_class->set_property) {
		id_class->set_property (data, property, value);
		return;
	}
}

gboolean item_data_has_properties (ItemData *data)
{
	ItemDataClass *id_class;

	g_return_val_if_fail (data != NULL, FALSE);
	g_return_val_if_fail (IS_ITEM_DATA (data), FALSE);

	id_class = ITEM_DATA_CLASS (G_OBJECT_GET_CLASS (data));
	if (id_class->has_properties) {
		return id_class->has_properties (data);
	}
	return FALSE;
}

void item_data_print (ItemData *data, cairo_t *cr, SchematicPrintContext *ctx)
{
	ItemDataClass *id_class;

	g_return_if_fail (data != NULL);
	g_return_if_fail (IS_ITEM_DATA (data));
	g_return_if_fail (cr != NULL);

	id_class = ITEM_DATA_CLASS (G_OBJECT_GET_CLASS (data));
	if (id_class->print) {
		id_class->print (data, cr, ctx);
	}
}

/**
 * \brief changed, forcefully emits a changed signal to recalculate the morph
 *matrix
 *
 * @param data determines which item to refresh
 *
 * \note this function does _not_ request a redraw
 */
void item_data_changed (ItemData *data)
{
	ItemDataClass *id_class;

	g_return_if_fail (data != NULL);
	g_return_if_fail (IS_ITEM_DATA (data));

	id_class = ITEM_DATA_CLASS (G_OBJECT_GET_CLASS (data));
	if (id_class->changed == NULL)
		return;

	return id_class->changed (data);
}

/**
 * @param data
 * @returns [transfer-none] pointer to cairo matrix which only includes the
 * translation
 */
cairo_matrix_t *item_data_get_translate (ItemData *data)
{
	g_return_val_if_fail (data != NULL, NULL);
	g_return_val_if_fail (IS_ITEM_DATA (data), NULL);
	return &(data->priv->translate);
}

/**
 * @param data
 * @returns [transfer-none] pointer to cairo matrix which only includes the
 * rotation
 */
cairo_matrix_t *item_data_get_rotate (ItemData *data)
{
	g_return_val_if_fail (data != NULL, NULL);
	g_return_val_if_fail (IS_ITEM_DATA (data), NULL);
	return &(data->priv->rotate);
}
