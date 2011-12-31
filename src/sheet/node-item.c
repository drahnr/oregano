/*
 * node-item.c
 * Authors:
 *  Richard Hult <rhult@hem.passagen.se>
 *  Ricardo Markiewicz <rmarkie@fi.uba.ar>
 *  Andres de Barbara <adebarbara@fi.uba.ar>
 *
 * Web page: http://arrakis.lug.fi.uba.ar/
 *
 * Copyright (C) 1999-2001  Richard Hult
 * Copyright (C) 2003,2006  Ricardo Markiewicz
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
#include <gnome.h>
#include "node-item.h"

static void node_item_init		(NodeItem		 *item);
static void node_item_class_init	(NodeItemClass	 *klass);

struct _NodeItemPriv {
	GnomeCanvasItem *dot_item;
};

static GnomeCanvasGroupClass *parent_class = NULL;


GType
node_item_get_type (void)
{
	static GType item_type = 0;

	if (!item_type)
	{
		static const GTypeInfo item_info =
		{
			sizeof (NodeItemClass),
			NULL,
			NULL,
			(GClassInitFunc) node_item_class_init,
			NULL,
			NULL,
			sizeof (NodeItem),
			0,
			(GInstanceInitFunc) node_item_init,
			NULL
		};

		item_type = g_type_register_static(GNOME_TYPE_CANVAS_GROUP, "NodeItem",
										   &item_info, 0);
	}

	return item_type;
}

static void
node_item_class_init (NodeItemClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS(klass);

	parent_class = g_type_class_peek_parent(klass);
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
			item->priv->dot_item = gnome_canvas_item_new (
				GNOME_CANVAS_GROUP (item),
				gnome_canvas_ellipse_get_type (),
				"x1", -2.0,
				"y1", -2.0,
				"x2", 2.0,
				"y2", 2.0,
				"fill_color", "black",
				NULL);
		}

		gnome_canvas_item_show (item->priv->dot_item);
	} else {
		if (item->priv->dot_item != NULL)
			gnome_canvas_item_hide (item->priv->dot_item);
	}
}