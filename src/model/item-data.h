/*
 * item-data.h
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
#ifndef __ITEM_DATA_H
#define __ITEM_DATA_H

// Base class for schematic model.

#include <cairo/cairo.h>

#include "sheet-pos.h"
#include "schematic-print-context.h"

#define TYPE_ITEM_DATA			  (item_data_get_type ())
#define ITEM_DATA(obj)			  (G_TYPE_CHECK_INSTANCE_CAST ((obj), TYPE_ITEM_DATA, ItemData))
#define ITEM_DATA_CLASS(klass)	  (G_TYPE_CHECK_CLASS_CAST ((klass), TYPE_ITEM_DATA, ItemDataClass))
#define IS_ITEM_DATA(obj)		  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TYPE_ITEM_DATA))
#define IS_ITEM_DATA_CLASS(klass) (G_TYPE_CHECK_INSTANCE_GET_CLASS ((klass), TYPE_ITEM_DATA, ItemDataClass))

typedef struct _ItemData ItemData;
typedef struct _ItemDataClass ItemDataClass;
typedef struct _ItemDataPriv ItemDataPriv;

typedef enum {
	ID_FLIP_NONE 	= 0,
	ID_FLIP_HORIZ 	= 1 << 0,
	ID_FLIP_VERT 	= 1 << 1
} IDFlip;

struct _ItemData {
	GObject 		parent;
	gulong			moved_handler_id;
	gulong			rotated_handler_id;
	gulong			flipped_handler_id;
	ItemDataPriv *	priv;
};

struct _ItemDataClass
{
	GObjectClass parent_class;

	// Signals.
	void 		(*moved) 				(ItemData *data, SheetPos *delta);

	// Methods.
	ItemData *	(*clone) 				(ItemData *src);
	void 		(*copy) 				(ItemData *dest, ItemData *src);
	void 		(*rotate)				(ItemData *data, int angle, 
							             SheetPos *center);
	void 		(*flip) 				(ItemData *data, gboolean horizontal, 
				            			 SheetPos *center);
	void 		(*unreg)				(ItemData *data);
	int 		(*reg)		 			(ItemData *data);

	char* 		(*get_refdes_prefix) 	(ItemData *data);
	void 		(*set_property) 		(ItemData *data, char *property, 
						                 char *value);
	gboolean 	(*has_properties) 		(ItemData *data);

	void 		(*print) 				(ItemData *data, cairo_t *cr, 
			               				 SchematicPrintContext *ctx);
};

GType	  item_data_get_type (void);
// Create a new ItemData object
ItemData *item_data_new (void);

// Clone an ItemData
//    param src A valid ItemData
ItemData *	item_data_clone (ItemData *src);

// Get Item position
void 		item_data_get_pos (ItemData *item_data, SheetPos *pos);

// Set Item position
void 		item_data_set_pos (ItemData *item_data, SheetPos *pos);

//  Move an ItemData
//  \param delta offset to move the item
void 		item_data_move (ItemData *item_data, SheetPos *delta);

// Get the bounding box of an item data 
//Retrieve the bounding box of the item relative to position
//  \param p1 Where to store the upper-left point
//  \param p2 Where to store the lower-right point
void 		item_data_get_relative_bbox (ItemData *data, SheetPos *p1, 
		                                 SheetPos *p2);

// Set the relative bounding box
void 		item_data_set_relative_bbox (ItemData *data, SheetPos *p1, 
		                                 SheetPos *p2);
// Get absolute bounding box
//  This function is like item_data_get_relative_bbox but it add
//  the item position to the bbox vertex
void 		item_data_get_absolute_bbox (ItemData *data, SheetPos *p1, 
		                                 SheetPos *p2);

// Get the absolute bounding box of a list of items
//  This return a bbox that enclose all item in a list
void 		item_data_list_get_absolute_bbox (GList *item_data_list, 
		                                      SheetPos *p1, SheetPos *p2);

// Rotate an item
void 		item_data_rotate (ItemData *data, int angle, SheetPos *center);

// Flip an item
void 		item_data_flip (ItemData *data, gboolean horizontal, 
		                    SheetPos *center);

// Get the Store associated for this item
//  Store is a class that hold all items in a schematic
gpointer 	item_data_get_store (ItemData *item_data);

//  Unregister item in its Store
void 		item_data_unregister (ItemData *data);

//  Register item in its Store
int 		item_data_register (ItemData *data);

//  Get the prefix of a part reference
char *		item_data_get_refdes_prefix (ItemData *data);

gboolean 	item_data_has_properties (ItemData *date);

//  Set property
void 		item_data_set_property (ItemData *data, char *property, 
		                            char *value);
//  Print Item data
// This method implement the Cairo stuff for schematic print of an item.
void 		item_data_print (ItemData *data, cairo_t *cr, 
		                     SchematicPrintContext *ctx);


#endif
