/*
 *
 * node-item.h
 *
 *
 * Authors:
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
#ifndef __NODE_ITEM_H__
#define __NODE_ITEM_H__

#include <gtk/gtk.h>
#include <gnome.h>

#define TYPE_NODE_ITEM	 (node_item_get_type ())
#define NODE_ITEM(obj)			  (G_TYPE_CHECK_INSTANCE_CAST ((obj), TYPE_NODE_ITEM, NodeItem))
#define NODE_ITEM_CLASS(klass)	  (G_TYPE_CHECK_CLASS_CAST ((klass), TYPE_NODE_ITEM, NodeItemClass))
#define IS_NODE_ITEM(obj)		  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TYPE_NODE_ITEM))
#define IS_NODE_ITEM_CLASS(klass) (G_TYPE_INSTANCE_GET_CLASS ((obj), TYPE_NODE_ITEM, NodeItemClass))


typedef struct _NodeItem	  NodeItem;
typedef struct _NodeItemClass NodeItemClass;
typedef struct _NodeItemPriv  NodeItemPriv;

struct _NodeItem
{
	GnomeCanvasGroup parent;
	NodeItemPriv *priv;
};

struct _NodeItemClass
{
	GnomeCanvasGroupClass parent_class;
};


GType	   node_item_get_type (void);
GtkWidget *node_item_new	  (void);
void	   node_item_show_dot (NodeItem *item, gboolean show);

#endif /* __NODE_ITEM_H__ */
