/*
 * xml-helper.c
 *
 *
 * Authors:
 *  Richard Hult <rhult@hem.passagen.se>
 *  Ricardo Markiewicz <rmarkie@fi.uba.ar>
 *  Andres de Barbara <adebarbara@fi.uba.ar>
 *
 * Web page: http://arrakis.lug.fi.uba.ar/
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
#include <gnome.h>

#include "xml-compat.h"
//#include <gnome-xml/parser.h>
//#include <gnome-xml/parserInternals.h>

#include "xml-helper.h"

/**
 * A modified version of XmlSAXParseFile in gnome-xml. This one lets us set
 * the user_data that is passed to the various callbacks, to make it possible
 * to avoid lots of global variables.
 */
int oreganoXmlSAXParseFile (xmlSAXHandlerPtr sax,
	gpointer user_data, const gchar *filename)
{
	int ret = 0;
	xmlParserCtxtPtr ctxt;

	ctxt = xmlCreateFileParserCtxt (filename);
	if (ctxt == NULL) return -1;
	ctxt->sax = sax;
	ctxt->userData = user_data;

#if defined(LIBXML_VERSION) && LIBXML_VERSION >= 20000
	xmlKeepBlanksDefault(0);
#endif
	xmlParseDocument (ctxt);

	if (ctxt->wellFormed)
		ret = 0;
	else
		ret = -1;
	if (sax != NULL)
		ctxt->sax = NULL;
	xmlFreeParserCtxt (ctxt);

	return ret;
}


/**
 * Set coodinate for a node, carried as the content of a child.
 */
void
xmlSetCoordinate (xmlNodePtr node, const char *name, double x, double y)
{
	xmlNodePtr child;
	gchar *str;
	xmlChar *ret;

	str = g_strdup_printf ("(%g %g)", x, y);
	ret = xmlGetProp (node, BAD_CAST name);
	if (ret != NULL){
		xmlSetProp (node, BAD_CAST name, BAD_CAST str);
		g_free (str);
		return;
	}
	child = node->childs;
	while (child != NULL){
		if (!xmlStrcmp (child->name, BAD_CAST name)){
			xmlNodeSetContent (child, BAD_CAST str);
			return;
		}
		child = child->next;
	}
	xmlSetProp (node, BAD_CAST name, BAD_CAST str);
	g_free (str);
}



/**
 * Set coodinates for a node, carried as the content of a child.
 */
void
xmlSetCoordinates (xmlNodePtr node, const char *name,
	double x1, double y1, double x2, double y2)
{
	xmlNodePtr child;
	gchar *str;
	xmlChar *ret;

	str = g_strdup_printf ("(%g %g)(%g %g)", x1, y1, x2, y2);
	ret = xmlGetProp (node, BAD_CAST name);
	if (ret != NULL){
		xmlSetProp (node, BAD_CAST name, BAD_CAST str);
		g_free (str);
		return;
	}
	child = node->childs;
	while (child != NULL){
		if (!xmlStrcmp (child->name, BAD_CAST name)){
			xmlNodeSetContent (child, BAD_CAST str);
			return;
		}
		child = child->next;
	}
	xmlSetProp (node, BAD_CAST name, BAD_CAST str);
	g_free (str);
}


/**
 * Set a string value for a node either carried as an attibute or as
 * the content of a child.
 */
void
xmlSetGnomeCanvasPoints (xmlNodePtr node, const char *name,
			 GnomeCanvasPoints *val)
{
	xmlNodePtr child;
	char *str, *base;
	int i;

	if (val == NULL)
		return;
	if ((val->num_points < 0) || (val->num_points > 5000))
		return;
	base = str = g_malloc (val->num_points * 30 * sizeof (char));
	if (str == NULL)
		return;
	for (i = 0; i < val->num_points; i++){
		str += sprintf (str, "(%g %g)", val->coords[2 * i],
				val->coords[2 * i + 1]);
	}

	child = node->childs;
	while (child != NULL){
		if (!xmlStrcmp (child->name, BAD_CAST name)){
			xmlNodeSetContent (child, BAD_CAST base);
			free (base);
			return;
		}
		child = child->next;
	}
	xmlNewChild (node, NULL, BAD_CAST name,
		xmlEncodeEntitiesReentrant(node->doc, BAD_CAST base));
	g_free (base);
}


/**
 * Set a string value for a node either carried as an attibute or as
 * the content of a child.
 */
void
xmlSetValue (xmlNodePtr node, const char *name, const char *val)
{
	xmlChar *ret;
	xmlNodePtr child;

	ret = xmlGetProp (node, BAD_CAST name);
	if (ret != NULL){
		xmlSetProp (node, BAD_CAST name, BAD_CAST val);
		return;
	}
	child = node->childs;
	while (child != NULL){
		if (!xmlStrcmp (child->name, BAD_CAST name)){
			xmlNodeSetContent (child, BAD_CAST val);
			return;
		}
		child = child->next;
	}
	xmlSetProp (node, BAD_CAST name, BAD_CAST val);
}

/**
 * Set an integer value for a node either carried as an attibute or as
 * the content of a child.
 */
void
xmlSetIntValue (xmlNodePtr node, const char *name, int val)
{
	xmlChar *ret;
	xmlNodePtr child;
	char str[101];

	snprintf (str, 100, "%d", val);
	ret = xmlGetProp (node,BAD_CAST name);
	if (ret != NULL){
		xmlSetProp (node, BAD_CAST name, BAD_CAST str);
		return;
	}
	child = node->childs;
	while (child != NULL){
		if (!xmlStrcmp (child->name, BAD_CAST name)){
			xmlNodeSetContent (child, BAD_CAST str);
			return;
		}
		child = child->next;
	}
	xmlSetProp (node, BAD_CAST name, BAD_CAST str);
}


/**
 * Set a double value for a node either carried as an attibute or as
 * the content of a child.
 */
void
xmlSetDoubleValue (xmlNodePtr node, const char *name, double val)
{
	xmlChar *ret;
	xmlNodePtr child;
	char str[101];

	snprintf (str, 100, "%g", (float) val);
	ret = xmlGetProp (node, BAD_CAST name);
	if (ret != NULL){
		xmlSetProp (node, BAD_CAST name, BAD_CAST str);
		return;
	}
	child = node->childs;
	while (child != NULL){
		if (!xmlStrcmp (child->name, BAD_CAST name)){
			xmlNodeSetContent (child, BAD_CAST str);
			return;
		}
		child = child->next;
	}
	xmlSetProp (node, BAD_CAST name, BAD_CAST str);
}

