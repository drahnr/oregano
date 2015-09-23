/*
 * clipboard.c
 *
 *
 * Author:
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
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include <glib.h>

#include "oregano.h"
#include "sheet-item.h"
#include "clipboard.h"

struct _ClipboardData
{
	ItemData *item_data;
	SheetItemClass *item_class;
};

void clipboard_empty (void)
{
	GSList *list;
	ClipboardData *cb_data;

	if (oregano.clipboard == NULL)
		return;

	for (list = oregano.clipboard; list; list = list->next) {
		cb_data = list->data;

		g_object_unref (G_OBJECT (cb_data->item_data));
		g_free (cb_data);
	}

	g_slist_free (oregano.clipboard);
	oregano.clipboard = NULL;
}

gboolean clipboard_is_empty (void)
{
	if (oregano.clipboard == NULL)
		return TRUE;

	return g_slist_length (oregano.clipboard) > 0 ? FALSE : TRUE;
}

void clipboard_foreach (ClipBoardFunction callback, gpointer user_data)
{
	GSList *list;
	ClipboardData *data;

	if (!oregano.clipboard)
		return;

	for (list = oregano.clipboard; list; list = list->next) {
		data = list->data;
		callback (data, user_data);
	}
}

void clipboard_add_object (GObject *item)
{
	ItemDataClass *id_class;
	ItemData *item_data, *clone;
	ClipboardData *cb_data;

	g_return_if_fail (item != NULL);

	item_data = sheet_item_get_data (SHEET_ITEM (item));
	g_return_if_fail (item_data != NULL);

	id_class = ITEM_DATA_CLASS (G_OBJECT_GET_CLASS (item_data));
	if (id_class->clone == NULL)
		return;

	// Duplicate the data for the object and add to the clipboard.
	clone = id_class->clone (item_data);

	cb_data = g_new0 (ClipboardData, 1);
	cb_data->item_data = clone;
	cb_data->item_class = SHEET_ITEM_CLASS (G_OBJECT_GET_CLASS (item));

	oregano.clipboard = g_slist_prepend (oregano.clipboard, cb_data);
}

GObject *clipboard_data_get_item_data (ClipboardData *data)
{
	g_return_val_if_fail (data != NULL, NULL);

	return G_OBJECT (data->item_data);
}

GObjectClass *clipboard_data_get_item_class (ClipboardData *data)
{
	g_return_val_if_fail (data != NULL, NULL);

	return G_OBJECT_CLASS (data->item_class);
}
