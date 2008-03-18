/*
 * node-store.h
 *
 *
 * Author:
 *  Richard Hult <rhult@hem.passagen.se>
 *  Ricardo Markiewicz <rmarkie@fi.uba.ar>
 *  Andres de Barbara <adebarbara@fi.uba.ar>
 *
 * Web page: http://arrakis.lug.fi.uba.ar/
 *
 * Copyright (C) 1999-2001  Richard Hult
 * Copyright (C) 2003,2004  Ricardo Markiewicz
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
#ifndef __NODE_STORE_H
#define __NODE_STORE_H

#include <glib.h>
#include <glib-object.h>
#include "sheet-pos.h"

#define TYPE_NODE_STORE			  node_store_get_type ()
#define NODE_STORE(obj)			  (G_TYPE_CHECK_INSTANCE_CAST ((obj), TYPE_NODE_STORE, NodeStore))
#define NODE_STORE_CLASS(klass)	  (G_TYPE_CHECK_CLASS_CAST ((klass), TYPE_NODE_STORE, NodeStoreClass))
#define IS_NODE_STORE(obj)		  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TYPE_NODE_STORE))
#define NODE_STORE_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), TYPE_NODE_STORE, NodeStoreClass))


typedef struct _NodeStore NodeStore;
typedef struct _NodeStoreClass NodeStoreClass;

#include "schematic-print-context.h"
#include "node.h"
#include "wire.h"
#include "part.h"
#include "textbox.h"

struct _NodeStore {
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

	/* signals */

	void (*dot_added) (NodeStore*);
	void (*dot_removed) (NodeStore*);
};

GType  node_store_get_type (void);
NodeStore	*node_store_new (void);
Node		*node_store_get_node (NodeStore *store, SheetPos pos);
int		 node_store_add_part (NodeStore *store, Part *part);
int		 node_store_remove_part (NodeStore *store, Part *part);
int		 node_store_add_wire (NodeStore *store, Wire *wire);
int		 node_store_remove_wire (NodeStore *store, Wire *wire);
int    node_store_add_textbox (NodeStore *self, Textbox *text);
int    node_store_remove_textbox (NodeStore *self, Textbox *text);
void	 node_store_node_foreach (NodeStore *store, GHFunc *func,
	gpointer user_data);
int		 node_store_is_wire_at_pos (NodeStore *store, SheetPos pos);
int    node_store_is_pin_at_pos (NodeStore *store, SheetPos pos);
GList	*node_store_get_parts (NodeStore *store);
GList	*node_store_get_wires (NodeStore *store);
GList	*node_store_get_items (NodeStore *store);
GList	*node_store_get_node_positions (NodeStore *store);
GList	*node_store_get_nodes (NodeStore *store);
void	 node_store_dump_wires (NodeStore *store);
void	 node_store_get_bounds (NodeStore *store, ArtDRect *rect);
gint	 node_store_count_items (NodeStore *store, ArtDRect *rect);
void	 node_store_print_items (NodeStore *store, cairo_t *opc, SchematicPrintContext *ctx);
void	 node_store_print_labels (NodeStore *store,
	cairo_t *opc, ArtDRect *rect);
Node	 *node_store_get_or_create_node (NodeStore *store, SheetPos pos);

#endif
