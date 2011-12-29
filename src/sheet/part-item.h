/*
 * part-item.h
 *
 *
 * Author:
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
#ifndef __PART_ITEM_H
#define __PART_ITEM_H

typedef struct _PartItem PartItem;
typedef struct _PartItemClass PartItemClass;
typedef struct _PartItemPriv PartItemPriv;

#include "sheet-item.h"
#include "load-library.h"
#include "load-common.h"
#include "part.h"

#define TYPE_PART_ITEM		(part_item_get_type ())
#define PART_ITEM(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), TYPE_PART_ITEM, PartItem))
#define PART_ITEM_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass), TYPE_PART_ITEM, PartItemClass))
#define IS_PART_ITEM(obj)	(G_TYPE_CHECK_INSTANCE_TYPE ((obj), TYPE_PART_ITEM))
#define PART_ITEM_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), TYPE_PART_ITEM, PartItemClass))

#include "schematic-view.h"

struct _PartItem {
	SheetItem	 parent_object;
	PartItemPriv *priv;
};

struct _PartItemClass {
	SheetItemClass parent_class;
};

GType	  part_item_get_type (void);
PartItem *part_item_new (Sheet *sheet, Part *part);
PartItem *part_item_new_from_part (Sheet *sheet, Part *part);
void	  part_item_signal_connect_floating (PartItem *item);
void	  part_item_signal_connect_floating_group (Sheet *sheet,
	SchematicView *schematic_view);
void	  part_item_create_canvas_items_for_preview (GnomeCanvasGroup *group,
	LibraryPart *library_part);
void	  part_item_update_node_label (PartItem *part);
void    part_item_show_node_labels (PartItem *part, gboolean b);

#endif
