/*
 * sheet-item-factory.c
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

#include "sheet-item-factory.h"
#include "wire-item.h"
#include "part-item.h"
#include "textbox-item.h"

#include "debug.h"

// Create a SheetItem from an ItemData object. This is a bit ugly.
// It could be beautified by having a method that creates the item.
// E.g. sheet_item->new_from_data (data);
SheetItem *sheet_item_factory_create_sheet_item (Sheet *sheet, ItemData *data)
{
	SheetItem *item;

	g_return_val_if_fail (data != NULL, NULL);
	g_return_val_if_fail (IS_ITEM_DATA (data), NULL);
	g_return_val_if_fail (sheet != NULL, NULL);
	g_return_val_if_fail (IS_SHEET (sheet), NULL);

	item = NULL;

	// Pick the right model.
	if (IS_PART (data)) {
		item = SHEET_ITEM (part_item_new (sheet, PART (data)));
		NG_DEBUG ("part %p", item);
	} else if (IS_WIRE (data)) {
		item = SHEET_ITEM (wire_item_new (sheet, WIRE (data)));
		NG_DEBUG ("wire %p", item);
	} else if (IS_TEXTBOX (data)) {
		item = SHEET_ITEM (textbox_item_new (sheet, TEXTBOX (data)));
		NG_DEBUG ("text %p", item);
	} else {
		g_warning ("Unknown Item type.");
	}

	return item;
}
