/*
 * node-item.c
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

#include <gtk/gtk.h>

#include "node-item.h"

static void node_item_init		(NodeItem		 *item);
static void node_item_class_init	(NodeItemClass	 *klass);

struct _NodeItemPriv {
	GooCanvasItem *dot_item;
};

G_DEFINE_TYPE (NodeItem, node_item, GOO_TYPE_CANVAS_GROUP)

static void
node_item_class_init (NodeItemClass *klass)
{
	node_item_parent_class = g_type_class_peek_parent (klass);
}

static void
node_item_init (NodeItem *item)
{
	item->priv = g_new0 (NodeItemPriv, 1);
}

void
node_item_show_dot (NodeItem *item, gboolean show)
{
	g_return_if_fail (item != NULL);
	g_return_if_fail (IS_NODE_ITEM (item));

	if (show) {
		if (item->priv->dot_item == NULL) {
			item->priv->dot_item = goo_canvas_ellipse_new (
			     	GOO_CANVAS_ITEM (item),
			   		0.0, 0.0, 2.0, 2.0, 
			   		"fill_color", "black", 
			        NULL);
		}
		g_object_set (item->priv->dot_item, 
				      "visibility", GOO_CANVAS_ITEM_VISIBLE, NULL);
	} 
	else if (item->priv->dot_item != NULL)
		g_object_set (item->priv->dot_item, 
				      "visibility", GOO_CANVAS_ITEM_INVISIBLE, NULL);
}
