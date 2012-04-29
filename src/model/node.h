/*
 * node.h
 *
 *
 * Author:
 *  Richard Hult <rhult@hem.passagen.se>
 *  Ricardo Markiewicz <rmarkie@fi.uba.ar>
 *  Andres de Barbara <adebarbara@fi.uba.ar>
 *  Marc Lorber <lorber.marc@wanadoo.fr>
 *
 * Web page: https://github.com/marc-lorber/oregano
 *
 * Copyright (C) 1999-2001  Richard Hult
 * Copyright (C) 2003,2004  Ricardo Markiewicz
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
#ifndef __NODE_H
#define __NODE_H

#include <gtk/gtk.h>

#include "sheet-pos.h"
#include "part.h"

#define TYPE_NODE			 (node_get_type ())
#define NODE(obj)			 (G_TYPE_CHECK_INSTANCE_CAST ((obj), TYPE_NODE, Node))
#define NODE_CLASS(klass)	 (G_TYPE_CHECK_CLASS_CAST ((klass), TYPE_NODE, NodeClass))
#define IS_NODE(obj)		 (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TYPE_NODE))
#define IS_NODE_CLASS(klass) (G_TYPE_INSTANCE_GET_CLASS ((klass), TYPE_NODE, NodeClass))

typedef struct _Node Node;
typedef struct _NodeClass NodeClass;

#include "wire.h"

struct _Node {
	GObject parent;

	// Used for traversing all nodes in the netlist generation.
	guint visited : 1;

	char *netlist_node_name;

	// The number of wires and pins in this node.
	guint16 pin_count;
	guint16 wire_count;

	GSList *pins;
	GSList *wires;

	SheetPos key;
};

struct _NodeClass
{
	GObjectClass parent_class;
};

GType node_get_type (void);
Node *node_new (SheetPos pos);
gint node_is_empty (Node *node);

gint node_add_pin (Node *node, Pin *pin);
gint node_remove_pin (Node *node, Pin *pin);

gint node_add_wire (Node *node, Wire *wire);
gint node_remove_wire (Node *node, Wire *wire);

gint node_is_visited (Node *node);
void node_set_visited (Node *node, gboolean is_visited);

gboolean node_needs_dot (Node *node);

#endif
