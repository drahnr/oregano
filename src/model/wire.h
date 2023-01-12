/*
 * wire.h
 *
 *
 * Authors:
 *  Richard Hult <rhult@hem.passagen.se>
 *  Ricardo Markiewicz <rmarkie@fi.uba.ar>
 *  Andres de Barbara <adebarbara@fi.uba.ar>
 *  Marc Lorber <lorber.marc@wanadoo.fr>
 *  Bernhard Schuster <bernhard@ahoi.io>
 *  Daniel Dwek <todovirtual15@gmail.com>
 *
 * Web page: https://ahoi.io/project/oregano
 *
 * Copyright (C) 1999-2001  Richard Hult
 * Copyright (C) 2003,2004  Ricardo Markiewicz
 * Copyright (C) 2009-2012  Marc Lorber
 * Copyright (C) 2013-2014  Bernhard Schuster
 * Copyright (C) 2022-2023  Daniel Dwek
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

#ifndef __WIRE_H
#define __WIRE_H

typedef struct _Wire Wire;
typedef struct _WireClass WireClass;
typedef struct _WirePriv WirePriv;

typedef enum {
	WIRE_DIR_NONE = 0,
	WIRE_DIR_HORIZ = 1,
	WIRE_DIR_VERT = 2,
	WIRE_DIR_DIAG = 3
} WireDir;

#include <gtk/gtk.h>
#include "item-data.h"
#include "node-store.h"
#include "node.h"
#include "grid.h"

#include "coords.h"
#include "clipboard.h"

#define TYPE_WIRE (wire_get_type ())
#define WIRE(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), TYPE_WIRE, Wire))
#define WIRE_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), TYPE_WIRE, WireClass))
#define IS_WIRE(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TYPE_WIRE))
#define IS_WIRE_CLASS(klass) (G_TYPE_INSTANCE_GET_CLASS ((klass), TYPE_WIRE))

struct _Wire
{
	ItemData parent;
	WirePriv *priv;
};

struct _WireClass
{
	ItemDataClass parent_class;

	Wire *(*dup)(Wire *wire);
	void (*changed)();
	void (*delete)();
};

GType wire_get_type (void);
Wire *wire_new (void);
NodeStore *wire_get_store (Wire *wire);
gint wire_set_store (Wire *wire, NodeStore *store);
gint wire_add_node (Wire *wire, Node *node);
gint wire_remove_node (Wire *wire, Node *node);
GSList *wire_get_nodes (Wire *wire);
void wire_get_start_pos (Wire *wire, Coords *pos);
void wire_get_end_pos (Wire *wire, Coords *pos);
void wire_get_start_and_end_pos (Wire *wire, Coords *start, Coords *end);
void wire_get_pos_and_length (Wire *wire, Coords *pos, Coords *length);
void wire_set_pos (Wire *wire, Coords *pos);
void wire_set_length (Wire *wire, Coords *length);
gint wire_is_visited (Wire *wire);
void wire_set_visited (Wire *wire, gboolean is_visited);
void wire_delete (Wire *wire);
void wire_update_bbox (Wire *wire);

void wire_dbg_print (Wire *w);
#endif
