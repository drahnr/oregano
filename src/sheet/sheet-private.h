/*
 * sheet-private.h
 *
 *
 * Authors:
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

#ifndef __SHEET_PRIVATE_H
#define __SHEET_PRIVATE_H

#include <gtk/gtk.h>
#include <goocanvas.h>

#include "sheet.h"
#include "create-wire.h"

typedef enum {
	RUBBER_NO = 0,
	RUBBER_YES,
	RUBBER_START
} RubberState;

typedef struct {
	RubberState		 state;
	int 			 timeout_id;
	int 			 click_start_state;
	GooCanvasItem	*rectangle;
	double	 		 start_x, start_y;
} RubberbandInfo;
	
struct _SheetPriv {
	// Keeps the current signal handler for wire creation.	
	int 				 wire_handler_id;
	// Keeps the signal handler for floating objects.
	int 				 float_handler_id;

	double 				 zoom;
	gulong 				 width;
	gulong  			 height;

	GooCanvasGroup		*selected_group;
	GooCanvasGroup		*floating_group;
	GList				*selected_objects;
	GList				*floating_objects;

	GList				*items;
	RubberbandInfo		*rubberband;
	GList 				*preserve_selection_items;
	GooCanvasClass		*sheet_parent_class;

	GList 				*voltmeter_items; // List of GooCanvasItem
	GHashTable 			*voltmeter_nodes;

	CreateWireContext 	*create_wire_context; // Wire context for each schematic

	GHashTable 			*node_dots;
};

#endif
