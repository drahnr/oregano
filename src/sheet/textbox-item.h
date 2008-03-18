/*
 * textbox-item.h
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
#ifndef __TEXTBOX_ITEM_H
#define __TEXTBOX_ITEM_H

#include <gtk/gtk.h>
#include "schematic-view.h"
#include "sheet-item.h"
#include "textbox.h"

#define TEXTBOX_ITEM(obj)	   G_TYPE_CHECK_INSTANCE_CAST (obj, textbox_item_get_type (), TextboxItem)
#define TEXTBOX_ITEM_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, textbox_item_get_type (), TextboxItemClass)
#define IS_TEXTBOX_ITEM(obj)	   G_TYPE_CHECK_INSTANCE_TYPE (obj, textbox_item_get_type ())

typedef struct _TextboxItemPriv TextboxItemPriv;

typedef enum {
	TEXTBOX_DIR_NONE = 0,
	TEXTBOX_DIR_HORIZ = 1,
	TEXTBOX_DIR_VERT = 2,
	TEXTBOX_DIR_DIAG = 3
} TextboxDir;

typedef struct {
	SheetItem parent_object;
	TextboxItemPriv *priv;
} TextboxItem;

typedef struct {
	SheetItemClass parent_class;
} TextboxItemClass;

GType        textbox_item_get_type (void);
TextboxItem *textbox_item_new (Sheet *sheet, Textbox *textbox);
void         textbox_item_signal_connect_placed (TextboxItem *textbox_item, SchematicView *sv);
void         textbox_item_cancel_listen (SchematicView *sv);
void         textbox_item_listen (SchematicView *sv);


#endif
