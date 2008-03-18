/*
 * create-wire.h
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#ifndef __CREATE_WIRE_H
#define __CREATE_WIRE_H

#include <gnome.h>

#include "sheet.h"
#include "wire.h"
#include "wire-item.h"
#include "schematic-view.h"

typedef struct _CreateWireContext CreateWireContext;

typedef struct {
	GnomeCanvasLine *line;
	GnomeCanvasPoints *points;

	WireDir direction;	   /* Direction of the first wire segment. */
} CreateWire;

CreateWireContext *create_wire_initiate (SchematicView *sv);

void create_wire_from_file (SchematicView *sv, SheetPos start_pos,
							SheetPos end_pos);
void create_wire_exit (CreateWireContext *cwc);
void paste_wire_item_from_wire (SchematicView *schematic_view,
								Wire *wire);

#endif
