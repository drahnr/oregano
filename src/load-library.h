/*
 * load-library.h
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
#ifndef __LOAD_LIBRARY_H
#define __LOAD_LIBRARY_H

#include <gnome.h>
#include "sheet.h"

typedef struct _SymbolObject SymbolObject;
typedef struct _LibrarySymbol LibrarySymbol;

#include "load-common.h"

struct _LibrarySymbol {
	char  *name;
	GSList *connections;
	GSList *symbol_objects;
};

typedef enum {
	SYMBOL_OBJECT_LINE,
	SYMBOL_OBJECT_ARC,
	SYMBOL_OBJECT_TEXT
} SymbolObjectType;

struct _SymbolObject {
	SymbolObjectType type;

	union {
		struct {
			gboolean spline;
			GnomeCanvasPoints *line;
		} uline;
		struct {
			double x1;
			double y1;
			double x2;
			double y2;
		} arc;
		struct {
			gint x,y;
			gchar str[256];
		} text;
	} u;
};

Library			*library_parse_xml_file (const gchar *filename);
LibrarySymbol	*library_get_symbol (const gchar *symbol_name);
LibraryPart		*library_get_part (Library *library, const gchar *part_name);

#endif
