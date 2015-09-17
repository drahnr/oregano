/*
 * xml-helper.h
 *
 *
 * Authors:
 *  Richard Hult <rhult@hem.passagen.se>
 *  Ricardo Markiewicz <rmarkie@fi.uba.ar>
 *  Andres de Barbara <adebarbara@fi.uba.ar>
 *  Marc Lorber <lorber.marc@wanadoo.fr>
 *  Bernhard Schuster <bernhard@ahoi.io>
 *
 * Web page: https://ahoi.io/project/oregano
 *
 * Copyright (C) 1999-2001  Richard Hult
 * Copyright (C) 2003,2004  Ricardo Markiewicz
 * Copyright (C) 2009-2012  Marc Lorber
 * Copyright (C) 2014       Bernhard Schuster
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
#ifndef __XML_HELPER_H
#define __XML_HELPER_H

#include <glib.h>

#include "xml-compat.h"

gboolean oreganoXmlSAXParseFile (xmlSAXHandlerPtr sax, gpointer user_data, const gchar *filename);

void xmlSetValue (xmlNodePtr node, const char *name, const char *val);

void xmlSetIntValue (xmlNodePtr node, const char *name, int val);

void xmlSetDoubleValue (xmlNodePtr node, const char *name, double val);

void xmlSetCoordinate (xmlNodePtr node, const char *name, double x, double y);

void xmlSetCoordinates (xmlNodePtr node, const char *name, double x1, double y1, double x2,
                        double y2);

#endif
