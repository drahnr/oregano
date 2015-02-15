/*
 * rubberband.h
 *
 *
 * Authors:
 *  Richard Hult <rhult@hem.passagen.se>
 *  Ricardo Markiewicz <rmarkie@fi.uba.ar>
 *  Andres de Barbara <adebarbara@fi.uba.ar>
 *  Marc Lorber <lorber.marc@wanadoo.fr>
 *  Bernhard Schuster <schuster.bernhard@gmail.com>
 *
 * Description: Handles the user interaction when doing area/rubberband
 *selections.
 *
 * Web page: https://srctwig.com/oregano
 *
 * Copyright (C) 1999-2001  Richard Hult
 * Copyright (C) 2003,2006  Ricardo Markiewicz
 * Copyright (C) 2009-2012  Marc Lorber
 * Copyright (C) 2013       Bernhard Schuster
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

#ifndef __INPUT_CONTEXT_RUBBERBAND_H
#define __INPUT_CONTEXT_RUBBERBAND_H

#include <goocanvas.h>
#include <glib.h>

#include "coords.h"

typedef enum { RUBBERBAND_DISABLED, RUBBERBAND_START, RUBBERBAND_ACTIVE } RubberbandState;

typedef struct _RubberbandInfo RubberbandInfo;

#include "sheet.h"

struct _RubberbandInfo
{
	RubberbandState state;
	Coords start;
	Coords end;
	GooCanvasRect *rectangle;
};

RubberbandInfo *rubberband_info_new (Sheet *sheet);
void rubberband_info_destroy (RubberbandInfo *rubberband);
gboolean rubberband_start (Sheet *sheet, GdkEvent *event);
gboolean rubberband_update (Sheet *sheet, GdkEvent *event);
gboolean rubberband_finish (Sheet *sheet, GdkEvent *event);

#endif /* __INPUT_CONTEXT_RUBBERBAND_H */
