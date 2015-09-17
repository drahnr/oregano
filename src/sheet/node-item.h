/*
 *
 * node-item.h
 *
 *
 * Authors:
 *  Richard Hult <rhult@hem.passagen.se>
 *  Ricardo Markiewicz <rmarkie@fi.uba.ar>
 *  Andres de Barbara <adebarbara@fi.uba.ar>
 *  Marc Lorber <lorber.marc@wanadoo.fr>
 *
 * Web page: https://ahoi.io/project/oregano
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
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */
#ifndef __NODE_ITEM_H__
#define __NODE_ITEM_H__

#include <gtk/gtk.h>
#include <goocanvas.h>

#define TYPE_NODE_ITEM (node_item_get_type ())
#define NODE_ITEM(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), TYPE_NODE_ITEM, NodeItem))
#define NODE_ITEM_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), TYPE_NODE_ITEM, NodeItemClass))
#define IS_NODE_ITEM(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TYPE_NODE_ITEM))
#define IS_NODE_ITEM_CLASS(klass) (G_TYPE_INSTANCE_GET_CLASS ((obj), TYPE_NODE_ITEM, NodeItemClass))

typedef struct _NodeItem NodeItem;
typedef struct _NodeItemClass NodeItemClass;
typedef struct _NodeItemPriv NodeItemPriv;

struct _NodeItem
{
	GooCanvasGroup parent;
	NodeItemPriv *priv;
};

struct _NodeItemClass
{
	GooCanvasGroupClass parent_class;
};

GType node_item_get_type (void);
GtkWidget *node_item_new (void);
void node_item_show_dot (NodeItem *item, gboolean show);

#endif /* __NODE_ITEM_H__ */
