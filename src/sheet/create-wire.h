/*
 * create-wire.h
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef __CREATE_WIRE_H
#define __CREATE_WIRE_H

#include <goocanvas.h>

#include "sheet.h"
#include "wire.h"
#include "wire-item.h"
#include "schematic-view.h"

typedef enum { WIRE_START, WIRE_ACTIVE, WIRE_DISABLED } WireState;

typedef struct _CreateWireInfo CreateWireInfo;

struct _CreateWireInfo
{
	WireState state;
	GooCanvasPolyline *line;
	GooCanvasPoints *points;
	GooCanvasEllipse *dot;
	WireDir direction;
	gulong event_handler_id;
	//	gulong			 cancel_handler_id;
};

CreateWireInfo *create_wire_info_new (Sheet *sheet);
void create_wire_info_destroy (CreateWireInfo *wire_info);
gboolean create_wire_setup (Sheet *sheet);
gboolean create_wire_orientationtoggle (Sheet *sheet);
gboolean create_wire_event (Sheet *sheet, GdkEvent *event, gpointer data);
gboolean create_wire_cleanup (Sheet *sheet);

#endif
