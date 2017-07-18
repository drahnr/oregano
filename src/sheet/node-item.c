/*
 * node-item.c
 * Authors:
 *  Richard Hult <rhult@hem.passagen.se>
 *  Ricardo Markiewicz <rmarkie@fi.uba.ar>
 *  Andres de Barbara <adebarbara@fi.uba.ar>
 *  Marc Lorber <lorber.marc@wanadoo.fr>
 *
 * Web page: https://ahoi.io/project/oregano
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
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include <gtk/gtk.h>
#include "options.h"
#include "node-item.h"

static void node_item_init (NodeItem *item);
static void node_item_class_init (NodeItemClass *klass);

struct _NodeItemPriv
{
	GooCanvasItem *dot_item;
	GooCanvasItem *circle_item; // debug
};

G_DEFINE_TYPE (NodeItem, node_item, GOO_TYPE_CANVAS_GROUP);

static void node_item_dispose (GObject *object)
{
	NodeItem *item = NODE_ITEM (object);
	NodeItemPriv *priv = item->priv;

	g_clear_object (&(priv->dot_item));
	g_clear_object (&(priv->circle_item));
	G_OBJECT_CLASS (node_item_parent_class)->dispose (object);
}

static void node_item_finalize (GObject *object)
{
	G_OBJECT_CLASS (node_item_parent_class)->finalize (object);
}

static void node_item_class_init (NodeItemClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = node_item_finalize;
	object_class->dispose = node_item_dispose;
}

static void node_item_init (NodeItem *item)
{
	item->priv = g_new0 (NodeItemPriv, 1);
	item->priv->dot_item =
	    goo_canvas_ellipse_new (GOO_CANVAS_ITEM (item), 0.0, 0.0, 2.0, 2.0, "fill-color", "black",
	                            "visibility", GOO_CANVAS_ITEM_INVISIBLE, NULL);
	item->priv->circle_item = goo_canvas_ellipse_new (
	    GOO_CANVAS_ITEM (item), 0.0, 0.0, 3.0, 3.0, "stroke-color-rgba", 0x3399FFFF, "line-width",
	    1.0, "visibility",
	    oregano_options_debug_dots () ? GOO_CANVAS_ITEM_VISIBLE : GOO_CANVAS_ITEM_INVISIBLE, NULL);
}

void node_item_show_dot (NodeItem *item, gboolean show)
{
	g_return_if_fail (item != NULL);
	g_return_if_fail (IS_NODE_ITEM (item));

	g_object_set (item->priv->dot_item, "visibility",
	              show ? GOO_CANVAS_ITEM_VISIBLE : GOO_CANVAS_ITEM_INVISIBLE, NULL);
}
