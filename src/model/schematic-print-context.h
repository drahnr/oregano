/*
 * schematic.c
 *
 * Authors:
 *  Ricardo Markiewicz <rmarkie@fi.uba.ar>
 *
 * Web page: http://oregano.lug.fi.uba.ar/
 *
 * Copyright (C) 1999-2001  Richard Hult
 * Copyright (C) 2003,2006  Ricardo Markiewicz
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
#ifndef _SCHEMATIC_PRINT_CONTEXT_
#define _SCHEMATIC_PRINT_CONTEXT_

#include <gdk/gdk.h>

typedef struct _SchematicColors {
	GdkColor components;
	GdkColor labels;
	GdkColor wires;
	GdkColor text;
	GdkColor background;
} SchematicColors;

typedef struct _SchematicPrintContext {
	SchematicColors colors;
} SchematicPrintContext;

#endif

