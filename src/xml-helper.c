/*
 * xml-helper.c
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
 * Copyright (C) 2003,2006  Ricardo Markiewicz
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
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "xml-compat.h"
#include "xml-helper.h"

// A modified version of XmlSAXParseFile in gnome-xml. This one lets us set
// the user_data that is passed to the various callbacks, to make it possible
// to avoid lots of global variables.
gboolean oreganoXmlSAXParseFile (xmlSAXHandlerPtr sax, gpointer user_data, const gchar *filename)
{
	g_return_val_if_fail (filename != NULL, FALSE);

	gboolean parser_failed, ret = TRUE;
	xmlParserCtxtPtr ctxt;

	ctxt = xmlCreateFileParserCtxt (filename);
	if (ctxt == NULL)
		return FALSE;

	ctxt->sax = sax;
	ctxt->userData = user_data;

#if defined(LIBXML_VERSION) && LIBXML_VERSION >= 20000
	xmlKeepBlanksDefault (0);
#endif
	parser_failed = FALSE;
	if (xmlParseDocument (ctxt) < 0) {
		// FIXME post a message to the log buffer with as much details as possible
		g_message ("Failed to parse \"%s\"", filename);
		ret = FALSE;
		parser_failed = TRUE;
	} else {
		ret = ctxt->wellFormed ? TRUE : FALSE;
		if (sax != NULL)
			ctxt->sax = NULL;
	}

	if (!parser_failed)
		xmlFreeParserCtxt (ctxt);

	return ret;
}

// Set coodinate for a node, carried as the content of a child.
void xmlSetCoordinate (xmlNodePtr node, const char *name, double x, double y)
{
	xmlNodePtr child;
	gchar *str;
	xmlChar *ret;

	str = g_strdup_printf ("(%g %g)", x, y);
	ret = xmlGetProp (node, BAD_CAST name);
	if (ret != NULL) {
		xmlSetProp (node, BAD_CAST name, BAD_CAST str);
		g_free (str);
		return;
	}
	child = node->childs;
	while (child != NULL) {
		if (!xmlStrcmp (child->name, BAD_CAST name)) {
			xmlNodeSetContent (child, BAD_CAST str);
			return;
		}
		child = child->next;
	}
	xmlSetProp (node, BAD_CAST name, BAD_CAST str);
	g_free (str);
}

// Set coodinates for a node, carried as the content of a child.
void xmlSetCoordinates (xmlNodePtr node, const char *name, double x1, double y1, double x2,
                        double y2)
{
	xmlNodePtr child;
	gchar *str;
	xmlChar *ret;

	str = g_strdup_printf ("(%g %g)(%g %g)", x1, y1, x2, y2);
	ret = xmlGetProp (node, BAD_CAST name);
	if (ret != NULL) {
		xmlSetProp (node, BAD_CAST name, BAD_CAST str);
		g_free (str);
		return;
	}
	child = node->childs;
	while (child != NULL) {
		if (!xmlStrcmp (child->name, BAD_CAST name)) {
			xmlNodeSetContent (child, BAD_CAST str);
			return;
		}
		child = child->next;
	}
	xmlSetProp (node, BAD_CAST name, BAD_CAST str);
	g_free (str);
}

// Set a string value for a node either carried as an attibute or as the
// content of a child.
void xmlSetValue (xmlNodePtr node, const char *name, const char *val)
{
	xmlChar *ret;
	xmlNodePtr child;

	ret = xmlGetProp (node, BAD_CAST name);
	if (ret != NULL) {
		xmlSetProp (node, BAD_CAST name, BAD_CAST val);
		return;
	}
	child = node->childs;
	while (child != NULL) {
		if (!xmlStrcmp (child->name, BAD_CAST name)) {
			xmlNodeSetContent (child, BAD_CAST val);
			return;
		}
		child = child->next;
	}
	xmlSetProp (node, BAD_CAST name, BAD_CAST val);
}

// Set an integer value for a node either carried as an attibute or as
// the content of a child.
void xmlSetIntValue (xmlNodePtr node, const char *name, int val)
{
	xmlChar *ret;
	xmlNodePtr child;
	char str[101];

	snprintf (str, 100, "%d", val);
	ret = xmlGetProp (node, BAD_CAST name);
	if (ret != NULL) {
		xmlSetProp (node, BAD_CAST name, BAD_CAST str);
		return;
	}
	child = node->childs;
	while (child != NULL) {
		if (!xmlStrcmp (child->name, BAD_CAST name)) {
			xmlNodeSetContent (child, BAD_CAST str);
			return;
		}
		child = child->next;
	}
	xmlSetProp (node, BAD_CAST name, BAD_CAST str);
}

// Set a double value for a node either carried as an attibute or as
// the content of a child.
void xmlSetDoubleValue (xmlNodePtr node, const char *name, double val)
{
	xmlChar *ret;
	xmlNodePtr child;
	char str[101];

	snprintf (str, 100, "%g", (float)val);
	ret = xmlGetProp (node, BAD_CAST name);
	if (ret != NULL) {
		xmlSetProp (node, BAD_CAST name, BAD_CAST str);
		return;
	}
	child = node->childs;
	while (child != NULL) {
		if (!xmlStrcmp (child->name, BAD_CAST name)) {
			xmlNodeSetContent (child, BAD_CAST str);
			return;
		}
		child = child->next;
	}
	xmlSetProp (node, BAD_CAST name, BAD_CAST str);
}
