/*
 * part-item.h
 *
 *
 * Author:
 *  Richard Hult <rhult@hem.passagen.se>
 *  Ricardo Markiewicz <rmarkie@fi.uba.ar>
 *  Andres de Barbara <adebarbara@fi.uba.ar>
 *  Marc Lorber <lorber.marc@wanadoo.fr>
 *  Bernhard Schuster <bernhard@ahoi.io>
 *  Daniel Dwek <todovirtual15@gmail.com>
 *
 * Web page: https://ahoi.io/project/oregano
 *
 * Copyright (C) 1999-2001  Richard Hult
 * Copyright (C) 2003,2004  Ricardo Markiewicz
 * Copyright (C) 2009-2012  Marc Lorber
 * Copyright (C) 2013-2014  Bernhard Schuster
 * Copyright (C) 2022-2023  Daniel Dwek
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

#ifndef __PART_ITEM_H
#define __PART_ITEM_H

typedef struct _PartItem PartItem;
typedef struct _PartItemClass PartItemClass;
typedef struct _PartItemPriv PartItemPriv;

#include "sheet-item.h"
#include "load-library.h"
#include "load-common.h"
#include "part.h"

#define TYPE_PART_ITEM (part_item_get_type ())
#define PART_ITEM(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), TYPE_PART_ITEM, PartItem))
#define PART_ITEM_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), TYPE_PART_ITEM, PartItemClass))
#define IS_PART_ITEM(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TYPE_PART_ITEM))
#define PART_ITEM_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), TYPE_PART_ITEM, PartItemClass))

struct _PartItem
{
	SheetItem parent_object;
	PartItemPriv *priv;
};

struct _PartItemClass
{
	SheetItemClass parent_class;
};

GType part_item_get_type (void);
PartItem *part_item_new (Sheet *sheet, Part *part);
GooCanvasItem *part_item_canvas_draw (GooCanvasGroup *group, SymbolObject *object);
void part_item_create_canvas_items_for_preview (GooCanvasGroup *group, LibraryPart *library_part);
void part_item_update_node_label (PartItem *part);
void part_item_show_node_labels (PartItem *part, gboolean b);
void show_labels (SheetItem *sheet_item, gboolean show);

#endif
