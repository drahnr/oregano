/*
 * grid-item.h
 *
 *
 * Authors:
 *  Bernhard Schuster <schuster.bernhard@gmail.com>
 *
 * Web page: https://srctwig.com/oregano
 *
 * Copyright (C) 2013      Bernhard Schuster
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

#ifndef __GRID_ITEM_H__
#define __GRID_ITEM_H__

#include <gtk/gtk.h>
#include <goocanvas.h>

#include "grid.h"
#include "grid-item-priv.h"


#define TYPE_GRID_ITEM         (grid_item_get_type ())
#define GRID_ITEM(obj)         (G_TYPE_CHECK_INSTANCE_CAST (obj, grid_item_get_type (), GridItem))
#define GRID_ITEM_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST (klass, grid_item_get_type (), GridItemClass))
#define IS_GRID_ITEM(obj)      (G_TYPE_CHECK_INSTANCE_TYPE (obj, grid_item_get_type ()))

typedef struct _GridItemPriv GridItemPriv;
typedef struct _GridItem GridItem;
typedef struct _GridItemClass GridItemClass;

struct _GridItem {
	GooCanvasGroup parent;
	GridItemPriv *	priv;
};

struct _GridItemClass {
	GooCanvasGroupClass parent_class;
};

GType 		grid_item_get_type (void);
GridItem *	grid_item_new (GooCanvasItem *root, Grid *grid);

#endif
