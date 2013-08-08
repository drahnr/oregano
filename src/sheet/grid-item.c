/*
 * grid-item.c
 *
 *
 * Authors:
 *  Richard Hult <rhult@hem.passagen.se>
 *  Ricardo Markiewicz <rmarkie@fi.uba.ar>
 *  Andres de Barbara <adebarbara@fi.uba.ar>
 *  Marc Lorber <lorber.marc@wanadoo.fr>
 *
 * Web page: https://srctwig.com/oregano
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
#include <string.h>

#include "coords.h"
#include "grid-item.h"
#include "grid.h"
#include "grid-item-priv.h"


#define RESIZER_SIZE 4.0f

static void 		grid_item_class_init (GridItemClass *klass);
static void 		grid_item_init (GridItem*item);
static void 		grid_item_finalize (GObject *object);
//static void 		grid_item_moved (SheetItem *object);
static void 		grid_changed_callback (Grid *grid, GridItem*item);
static void 		grid_delete_callback (Grid *grid, GridItem*item);
static void         grid_item_get_property (GObject *object, guint prop_id,
                    	GValue *value, GParamSpec *spec);
static void         grid_item_set_property (GObject *object, guint prop_id,
                    	const GValue *value, GParamSpec *spec);

#include "debug.h"


G_DEFINE_TYPE (GridItem, grid_item, GOO_TYPE_CANVAS_GROUP)

static void
grid_item_class_init (GridItemClass *grid_item_class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (grid_item_class);
	grid_item_parent_class = g_type_class_peek_parent (grid_item_class);
	
	object_class->finalize = grid_item_finalize;
	object_class->set_property = grid_item_set_property;
	object_class->get_property = grid_item_get_property;
}

static void
grid_item_set_property (GObject *object, guint property_id, const GValue *value,
        GParamSpec *pspec)
{
        g_return_if_fail (object != NULL);
        g_return_if_fail (IS_GRID_ITEM(object));

        switch (property_id) {
        default:
        	g_warning ("PartItem: Invalid argument.\n");
        }
}

static void
grid_item_get_property (GObject *object, guint property_id, GValue *value,
        GParamSpec *pspec)
{
        g_return_if_fail (object != NULL);
        g_return_if_fail (IS_GRID_ITEM (object));

        switch (property_id) {
        default:
        	pspec->value_type = G_TYPE_INVALID;
            break;
        }
}

static void
grid_item_init (GridItem*item)
{
	GridItemPriv *priv;

	priv = g_new0 (GridItemPriv, 1);
	item->priv = priv;
}

static void
grid_item_finalize (GObject *object)
{
	GridItemPriv *priv;

	priv = GRID_ITEM (object)->priv;

	if (priv != NULL) {
		g_free (priv);
	}
	
	G_OBJECT_CLASS (grid_item_parent_class)->finalize (object);
}



GridItem *
grid_item_new (GooCanvasItem *root, Grid *grid)
{
	GridItem *grid_item;
	GridItemPriv *priv;

	g_return_val_if_fail (root, NULL);
	g_return_val_if_fail (GOO_IS_CANVAS_ITEM (root), NULL);
	g_return_val_if_fail (grid, NULL);
	g_return_val_if_fail (IS_GRID (grid), NULL);

	grid_item = GRID_ITEM (g_object_new (TYPE_GRID_ITEM,
	                       "parent", root,
	                       NULL));

	priv = grid_item->priv;

	gdouble height = -1.,  width = -1.;
	grid_get_size (grid, &height, &width);
	g_printf ("%s xxx %lf x %lf\n", __FUNCTION__, height, width);
	// we make the canvas_grid child of the grid_item
	priv->canvas_grid = GOO_CANVAS_GRID (
	                    goo_canvas_grid_new (
	                            GOO_CANVAS_ITEM (grid_item),
	                            0.0,
	                            0.0,
	                            width,
	                            height,
	                            10.0,
	                            10.0,
	                            0.0,
	                            0.0,
	                            "horz-grid-line-width", 0.05,
	                            "horz-grid-line-color", "dark gray",
	                            "vert-grid-line-width", 0.05,
	                            "vert-grid-line-color", "dark gray",
	                            NULL)
	                     );

	g_signal_connect (grid, "changed", 
		G_CALLBACK (grid_changed_callback), grid_item);

	g_signal_connect (grid, "delete", 
	    G_CALLBACK (grid_delete_callback), grid_item);
	                                   
	return grid_item;
}


static void
grid_changed_callback (Grid *grid, GridItem *item)
{

	const gdouble spacing = grid_get_spacing (grid);
	gdouble height = -1., width = -1.;

	grid_get_size (grid, &height, &width);

	g_object_set (item->priv->canvas_grid,
	              "x-step", spacing,
	              "y-step", spacing,
	              "height", height,
	              "width", width,
	              "visibility", grid_is_visible (grid) ? GOO_CANVAS_ITEM_VISIBLE : GOO_CANVAS_ITEM_INVISIBLE,
	              NULL);

	goo_canvas_item_request_update (GOO_CANVAS_ITEM (item->priv->canvas_grid));
}

static void
grid_delete_callback (Grid *grid, GridItem *item)
{
	g_object_unref (G_OBJECT (item));
}
