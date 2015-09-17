/*
 * schematic-print-context.h
 *
 *
 * Authors:
 *  Ricardo Markiewicz <rmarkie@fi.uba.ar>
 *  Marc Lorber <lorber.marc@wanadoo.fr>
 *  Bernhard Schuster <bernhard@ahoi.io>
 *
 * Web page: https://ahoi.io/project/oregano
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
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */
#ifndef _SCHEMATIC_PRINT_CONTEXT_
#define _SCHEMATIC_PRINT_CONTEXT_

#include <gdk/gdk.h>

typedef struct _SchematicColors
{
	GdkRGBA components;
	GdkRGBA labels;
	GdkRGBA wires;
	GdkRGBA text;
	GdkRGBA background;
} SchematicColors;

typedef struct _SchematicPrintContext
{
	SchematicColors colors;
} SchematicPrintContext;

#endif
