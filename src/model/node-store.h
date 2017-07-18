/*
 * node-store.h
 *
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
 * Copyright (C) 2003,2004  Ricardo Markiewicz
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
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */
#ifndef __NODE_STORE_H
#define __NODE_STORE_H

#include <glib.h>
#include <glib-object.h>

#include "coords.h"

#define TYPE_NODE_STORE node_store_get_type ()
#define NODE_STORE(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), TYPE_NODE_STORE, NodeStore))
#define NODE_STORE_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), TYPE_NODE_STORE, NodeStoreClass))
#define IS_NODE_STORE(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TYPE_NODE_STORE))
#define NODE_STORE_GET_CLASS(obj)                                                                  \
	(G_TYPE_INSTANCE_GET_CLASS ((obj), TYPE_NODE_STORE, NodeStoreClass))

typedef struct _NodeStore NodeStore;
typedef struct _NodeStoreClass NodeStoreClass;
typedef struct _NodeRect NodeRect;

#include "schematic-print-context.h"
#include "node.h"
#include "wire.h"
#include "part.h"
#include "textbox.h"

struct _NodeStore
{
	GObject parent;

	GHashTable *nodes;
	GList *items;
	GList *wires;
	GList *parts;
	GList *textbox;
};

struct _NodeStoreClass
{
	GObjectClass parent_class;

	// signals

	void (*node_dot_added)(NodeStore *);
	void (*node_dot_removed)(NodeStore *);
};

struct _NodeRect
{
	double x0, y0, x1, y1;
};

GType node_store_get_type (void);
NodeStore *node_store_new (void);
Node *node_store_get_node (NodeStore *store, Coords pos);
gboolean node_store_add_part (NodeStore *store, Part *part);
gboolean node_store_remove_part (NodeStore *store, Part *part);
void node_store_remove_overlapping_wires (NodeStore *store, Wire *wire);
gboolean node_store_add_wire (NodeStore *store, Wire *wire);
gboolean node_store_remove_wire (NodeStore *store, Wire *wire);
gboolean node_store_add_textbox (NodeStore *self, Textbox *text);
gboolean node_store_remove_textbox (NodeStore *self, Textbox *text);
void node_store_node_foreach (NodeStore *store, GHFunc *func, gpointer user_data);
gboolean node_store_is_wire_at_pos (NodeStore *store, Coords pos);
gboolean node_store_is_pin_at_pos (NodeStore *store, Coords pos);
GList *node_store_get_parts (NodeStore *store);
GList *node_store_get_wires (NodeStore *store);
GList *node_store_get_items (NodeStore *store);
GList *node_store_get_node_positions (NodeStore *store);
GList *node_store_get_nodes (NodeStore *store);
void node_store_dump_wires (NodeStore *store);
void node_store_get_bounds (NodeStore *store, NodeRect *rect);
gint node_store_count_items (NodeStore *store, NodeRect *rect);
void node_store_print_items (NodeStore *store, cairo_t *opc, SchematicPrintContext *ctx);
Node *node_store_get_or_create_node (NodeStore *store, Coords pos);

#endif
