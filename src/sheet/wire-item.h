/*
 * wire-item.h
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
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef __WIRE_ITEM_H
#define __WIRE_ITEM_H

#include <gtk/gtk.h>

#include "sheet.h"
#include "sheet-item.h"
#include "wire.h"

#define TYPE_WIRE_ITEM (wire_item_get_type ())
#define WIRE_ITEM(obj) (G_TYPE_CHECK_INSTANCE_CAST (obj, wire_item_get_type (), WireItem))
#define WIRE_ITEM_CLASS(klass)                                                                     \
	(G_TYPE_CHECK_CLASS_CAST (klass, wire_item_get_type (), WireItemClass))
#define IS_WIRE_ITEM(obj) (G_TYPE_CHECK_INSTANCE_TYPE (obj, wire_item_get_type ()))

typedef struct _WireItemPriv WireItemPriv;

typedef struct
{
	SheetItem parent_object;
	WireItemPriv *priv;
} WireItem;

typedef struct
{
	SheetItemClass parent_class;
} WireItemClass;

GType wire_item_get_type (void);
WireItem *wire_item_new (Sheet *sheet, Wire *wire);
void wire_item_initiate (Sheet *sheet);
void wire_item_get_start_pos (WireItem *item, Coords *pos);
void wire_item_get_length (WireItem *item, Coords *pos);

#endif
