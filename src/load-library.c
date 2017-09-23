/*
 * load-library.c
 *
 *
 * Authors:
 *  Richard Hult <rhult@hem.passagen.se>
 *  Ricardo Markiewicz <rmarkie@fi.uba.ar>
 *  Andres de Barbara <adebarbara@fi.uba.ar>
 *  Marc Lorber <lorber.marc@wanadoo.fr>
 *
 * Web page: https://ahoi.io/project/oregano
 *
 * Copyright (C) 1999-2001  Richard Hult
 * Copyright (C) 2003,2006  Ricardo Markiewicz
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
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include <goocanvas.h>
#include <string.h>
#include <glib/gi18n.h>

#include "xml-compat.h"
#include "oregano.h"
#include "xml-helper.h"
#include "load-common.h"
#include "load-library.h"
#include "part-label.h"

typedef enum {
	PARSE_START,
	PARSE_LIBRARY,
	PARSE_NAME,
	PARSE_AUTHOR,
	PARSE_VERSION,
	PARSE_SYMBOLS,
	PARSE_SYMBOL,
	PARSE_SYMBOL_NAME,
	PARSE_SYMBOL_OBJECTS,
	PARSE_SYMBOL_LINE,
	PARSE_SYMBOL_ARC,
	PARSE_SYMBOL_TEXT,
	PARSE_SYMBOL_CONNECTIONS,
	PARSE_SYMBOL_CONNECTION,
	PARSE_PARTS,
	PARSE_PART,
	PARSE_PART_NAME,
	PARSE_PART_DESCRIPTION,
	PARSE_PART_USESYMBOL,
	PARSE_PART_LABELS,
	PARSE_PART_LABEL,
	PARSE_PART_LABEL_NAME,
	PARSE_PART_LABEL_TEXT,
	PARSE_PART_LABEL_POS,
	PARSE_PART_PROPERTIES,
	PARSE_PART_PROPERTY,
	PARSE_PART_PROPERTY_NAME,
	PARSE_PART_PROPERTY_VALUE,
	PARSE_FINISH,
	PARSE_UNKNOWN,
	PARSE_ERROR
} State;

typedef struct
{
	Library *library;

	State state;
	State prev_state;
	gint unknown_depth;
	GString *content;

	// Temporary placeholder for part
	LibraryPart *part;
	PartLabel *label;
	Property *property;

	// Temporary placeholder for symbol
	LibrarySymbol *symbol;
	Connection *connection;
	SymbolObject *object;
} ParseState;

static xmlEntityPtr get_entity (void *user_data, const xmlChar *name);
static void start_document (ParseState *state);
static void end_document (ParseState *state);
static void start_element (ParseState *state, const xmlChar *name, const xmlChar **attrs);
static void end_element (ParseState *state, const xmlChar *name);

static void my_characters (ParseState *state, const xmlChar *chars, int len);
static void my_warning (void *user_data, const char *msg, ...);
static void my_error (void *user_data, const char *msg, ...);
static void my_fatal_error (void *user_data, const char *msg, ...);

static xmlSAXHandler oreganoSAXParser = {
    0,                                    // internalSubset
    0,                                    // isStandalone
    0,                                    // hasInternalSubset
    0,                                    // hasExternalSubset
    0,                                    // resolveEntity
    (getEntitySAXFunc)get_entity,         // getEntity
    0,                                    // entityDecl
    0,                                    // notationDecl
    0,                                    // attributeDecl
    0,                                    // elementDecl
    0,                                    // unparsedEntityDecl
    0,                                    // setDocumentLocator
    (startDocumentSAXFunc)start_document, // startDocument
    (endDocumentSAXFunc)end_document,     // endDocument
    (startElementSAXFunc)start_element,   // startElement
    (endElementSAXFunc)end_element,       // endElement
    0,                                    // reference
    (charactersSAXFunc)my_characters,     // characters
    0,                                    // ignorableWhitespace
    0,                                    // processingInstruction
    0,                                    // (commentSAXFunc)0,	 comment
    (warningSAXFunc)my_warning,           // warning
    (errorSAXFunc)my_error,               // error
    (fatalErrorSAXFunc)my_fatal_error,    // fatalError
};

LibrarySymbol *library_get_symbol (const gchar *symbol_name)
{
	LibrarySymbol *symbol;
	Library *library;
	GList *iter;

	g_return_val_if_fail (symbol_name != NULL, NULL);

	symbol = NULL;
	for (iter = oregano.libraries; iter; iter = iter->next) {
		library = iter->data;
		symbol = g_hash_table_lookup (library->symbol_hash, symbol_name);
		if (symbol)
			break;
	}

	if (symbol == NULL) {
		g_message (_ ("Could not find the requested symbol: %s\n"), symbol_name);
	}

	return symbol;
}

LibraryPart *library_get_part (Library *library, const gchar *part_name)
{
	LibraryPart *part;

	g_return_val_if_fail (library != NULL, NULL);
	g_return_val_if_fail (part_name != NULL, NULL);

	part = g_hash_table_lookup (library->part_hash, part_name);
	if (part == NULL) {
		g_message (_ ("Could not find the requested part: %s\n"), part_name);
	}
	return part;
}

Library *library_parse_xml_file (const gchar *filename)
{
	Library *library;
	ParseState state;

	if (!oreganoXmlSAXParseFile (&oreganoSAXParser, &state, filename)) {
		g_warning ("Library '%s' not well formed!", filename);
	}

	if (state.state == PARSE_ERROR) {
		library = NULL;
	} else {
		library = state.library;
	}

	return library;
}

static void start_document (ParseState *state)
{
	state->state = PARSE_START;
	state->unknown_depth = 0;
	state->prev_state = PARSE_UNKNOWN;

	state->content = g_string_sized_new (128);
	state->part = NULL;
	state->symbol = NULL;
	state->connection = NULL;
	state->label = NULL;
	state->property = NULL;

	state->library = g_new0 (Library, 1);
	state->library->name = NULL;
	state->library->author = NULL;
	state->library->version = NULL;

	state->library->part_hash = g_hash_table_new (g_str_hash, g_str_equal);
	state->library->symbol_hash = g_hash_table_new (g_str_hash, g_str_equal);
}

static void end_document (ParseState *state)
{
	if (state->unknown_depth != 0)
		g_warning ("unknown_depth != 0 (%d)", state->unknown_depth);
}

static void start_element (ParseState *state, const xmlChar *xml_name, const xmlChar **attrs)
{
	const char *name = (const char *)xml_name;

	switch (state->state) {
	case PARSE_START:
		if (strcmp (name, "ogo:library")) {
			g_warning ("Expecting 'ogo:library'.  Got '%s'", name);
			state->state = PARSE_ERROR;
		} else
			state->state = PARSE_LIBRARY;
		break;

	case PARSE_LIBRARY:
		if (!strcmp (name, "ogo:author")) {
			state->state = PARSE_AUTHOR;
			g_string_truncate (state->content, 0);
		} else if (!strcmp (name, "ogo:name")) {
			state->state = PARSE_NAME;
			g_string_truncate (state->content, 0);
		} else if (!strcmp (name, "ogo:version")) {
			state->state = PARSE_VERSION;
			g_string_truncate (state->content, 0);
		} else if (!strcmp (name, "ogo:symbols")) {
			state->state = PARSE_SYMBOLS;
		} else if (!strcmp (name, "ogo:parts")) {
			state->state = PARSE_PARTS;
		} else {
			state->prev_state = state->state;
			state->state = PARSE_UNKNOWN;
			state->unknown_depth++;
		}
		break;

	case PARSE_SYMBOLS:
		if (!strcmp (name, "ogo:symbol")) {
			state->state = PARSE_SYMBOL;
			state->symbol = g_new0 (LibrarySymbol, 1);
		} else {
			state->prev_state = state->state;
			state->state = PARSE_UNKNOWN;
			state->unknown_depth++;
		}
		break;

	case PARSE_SYMBOL:
		if (!strcmp (name, "ogo:name")) {
			state->state = PARSE_SYMBOL_NAME;
			g_string_truncate (state->content, 0);
		} else if (!strcmp (name, "ogo:objects")) {
			state->state = PARSE_SYMBOL_OBJECTS;
		} else if (!strcmp (name, "ogo:connections")) {
			state->state = PARSE_SYMBOL_CONNECTIONS;
		} else {
			state->prev_state = state->state;
			state->state = PARSE_UNKNOWN;
			state->unknown_depth++;
		}
		break;
	case PARSE_SYMBOL_OBJECTS:
		if (!strcmp (name, "ogo:line")) {
			state->object = g_new0 (SymbolObject, 1);
			state->object->type = SYMBOL_OBJECT_LINE;
			state->state = PARSE_SYMBOL_LINE;
			g_string_truncate (state->content, 0);
		} else if (!strcmp (name, "ogo:arc")) {
			state->object = g_new0 (SymbolObject, 1);
			state->object->type = SYMBOL_OBJECT_ARC;
			state->state = PARSE_SYMBOL_ARC;
			g_string_truncate (state->content, 0);
		} else if (!strcmp (name, "ogo:text")) {
			state->object = g_new0 (SymbolObject, 1);
			state->object->type = SYMBOL_OBJECT_TEXT;
			state->state = PARSE_SYMBOL_TEXT;
			g_string_truncate (state->content, 0);
		} else {
			state->prev_state = state->state;
			state->state = PARSE_UNKNOWN;
			state->unknown_depth++;
		}
		break;

	case PARSE_SYMBOL_CONNECTIONS:
		if (!strcmp (name, "ogo:connection")) {
			state->state = PARSE_SYMBOL_CONNECTION;
			state->connection = g_new0 (Connection, 1);
			g_string_truncate (state->content, 0);
		} else {
			state->prev_state = state->state;
			state->state = PARSE_UNKNOWN;
			state->unknown_depth++;
		}
		break;

	case PARSE_PARTS:
		if (!strcmp (name, "ogo:part")) {
			state->state = PARSE_PART;
			state->part = g_new0 (LibraryPart, 1);
			state->part->library = state->library;
		} else {
			state->prev_state = state->state;
			state->state = PARSE_UNKNOWN;
			state->unknown_depth++;
		}
		break;
	case PARSE_PART:
		if (!strcmp (name, "ogo:name")) {
			state->state = PARSE_PART_NAME;
			g_string_truncate (state->content, 0);
		} else if (!strcmp (name, "ogo:description")) {
			state->state = PARSE_PART_DESCRIPTION;
			g_string_truncate (state->content, 0);
		} else if (!strcmp (name, "ogo:symbol")) {
			state->state = PARSE_PART_USESYMBOL;
			g_string_truncate (state->content, 0);
		} else if (!strcmp (name, "ogo:labels")) {
			state->state = PARSE_PART_LABELS;
		} else if (!strcmp (name, "ogo:properties")) {
			state->state = PARSE_PART_PROPERTIES;
		} else {
			state->prev_state = state->state;
			state->state = PARSE_UNKNOWN;
			state->unknown_depth++;
		}
		break;
	case PARSE_PART_LABELS:
		if (!strcmp (name, "ogo:label")) {
			state->state = PARSE_PART_LABEL;
			state->label = g_new0 (PartLabel, 1);
		} else {
			state->prev_state = state->state;
			state->state = PARSE_UNKNOWN;
			state->unknown_depth++;
		}
		break;
	case PARSE_PART_LABEL:
		if (!strcmp (name, "ogo:name")) {
			state->state = PARSE_PART_LABEL_NAME;
			g_string_truncate (state->content, 0);
		} else if (!strcmp (name, "ogo:text")) {
			state->state = PARSE_PART_LABEL_TEXT;
			g_string_truncate (state->content, 0);
		} else if (!strcmp (name, "ogo:position")) {
			state->state = PARSE_PART_LABEL_POS;
			g_string_truncate (state->content, 0);
		} else {
			state->prev_state = state->state;
			state->state = PARSE_UNKNOWN;
			state->unknown_depth++;
		}
		break;

	case PARSE_PART_PROPERTIES:
		if (!strcmp (name, "ogo:property")) {
			state->state = PARSE_PART_PROPERTY;
			state->property = g_new0 (Property, 1);
		} else {
			state->prev_state = state->state;
			state->state = PARSE_UNKNOWN;
			state->unknown_depth++;
		}
		break;
	case PARSE_PART_PROPERTY:
		if (!strcmp (name, "ogo:name")) {
			state->state = PARSE_PART_PROPERTY_NAME;
			g_string_truncate (state->content, 0);
		} else if (!strcmp (name, "ogo:value")) {
			state->state = PARSE_PART_PROPERTY_VALUE;
			g_string_truncate (state->content, 0);
		} else {
			state->prev_state = state->state;
			state->state = PARSE_UNKNOWN;
			state->unknown_depth++;
		}
		break;

	case PARSE_SYMBOL_NAME:
	case PARSE_SYMBOL_LINE:
	case PARSE_SYMBOL_ARC:
	case PARSE_SYMBOL_TEXT:
	case PARSE_SYMBOL_CONNECTION:
	case PARSE_PART_NAME:
	case PARSE_PART_DESCRIPTION:
	case PARSE_PART_USESYMBOL:
	case PARSE_PART_LABEL_NAME:
	case PARSE_PART_LABEL_TEXT:
	case PARSE_PART_LABEL_POS:
	case PARSE_PART_PROPERTY_NAME:
	case PARSE_PART_PROPERTY_VALUE:
	case PARSE_NAME:
	case PARSE_AUTHOR:
	case PARSE_VERSION:
		// there should be no tags inside these types of tags
		g_message ("*** '%s' tag found", name);
		state->prev_state = state->state;
		state->state = PARSE_UNKNOWN;
		state->unknown_depth++;
		break;

	case PARSE_ERROR:
		break;
	case PARSE_UNKNOWN:
		state->unknown_depth++;
		break;
	case PARSE_FINISH:
		// should not start new elements in this state
		g_assert_not_reached ();
		break;
	}
	// g_message("Start element %s (state %s)", name, states[state->state]);
}

static void end_element (ParseState *state, const xmlChar *name)
{
	switch (state->state) {
	case PARSE_UNKNOWN:
		state->unknown_depth--;
		if (state->unknown_depth == 0)
			state->state = state->prev_state;
		break;
	case PARSE_AUTHOR:
		state->library->author = g_strdup (state->content->str);
		state->state = PARSE_LIBRARY;
		break;
	case PARSE_NAME:
		state->library->name = g_strdup (state->content->str);
		state->state = PARSE_LIBRARY;
		break;
	case PARSE_VERSION:
		state->library->version = g_strdup (state->content->str);
		state->state = PARSE_LIBRARY;
		break;
	case PARSE_SYMBOLS:
		state->state = PARSE_LIBRARY;
		break;
	case PARSE_SYMBOL:
		g_hash_table_insert (state->library->symbol_hash, state->symbol->name, state->symbol);
		state->state = PARSE_SYMBOLS;
		break;
	case PARSE_SYMBOL_NAME:
		state->symbol->name = g_strdup (state->content->str);
		state->state = PARSE_SYMBOL;
		break;
	case PARSE_SYMBOL_OBJECTS:
		state->state = PARSE_SYMBOL;
		break;
	case PARSE_SYMBOL_LINE: {
		int i, j;
		gchar *ptr;
		gchar **points;

		i = sscanf (state->content->str, "%d %n", &state->object->u.uline.spline, &j);
		if (i)
			ptr = state->content->str + j;
		else {
			state->object->u.uline.spline = FALSE;
			ptr = state->content->str;
		}

		points = g_strsplit (ptr, "(", 0);

		i = 0;
		// Count the points.
		while (points[i] != NULL) {
			i++;
		}

		// Do not count the first string, which simply is a (.
		i--;

		// Construct goo canvas points.
		state->object->u.uline.line = goo_canvas_points_new (i);
		for (j = 0; j < i; j++) {
			double x, y;

			sscanf (points[j + 1], "%lf %lf)", &x, &y);

			state->object->u.uline.line->coords[2 * j] = x;
			state->object->u.uline.line->coords[2 * j + 1] = y;
		}

		g_strfreev (points);
		state->symbol->symbol_objects =
		    g_slist_prepend (state->symbol->symbol_objects, state->object);
		state->state = PARSE_SYMBOL_OBJECTS;
	} break;
	case PARSE_SYMBOL_ARC:
		sscanf (state->content->str, "(%lf %lf)(%lf %lf)", &state->object->u.arc.x1,
		        &state->object->u.arc.y1, &state->object->u.arc.x2, &state->object->u.arc.y2);
		state->symbol->symbol_objects =
		    g_slist_prepend (state->symbol->symbol_objects, state->object);
		state->state = PARSE_SYMBOL_OBJECTS;
		break;

	case PARSE_SYMBOL_TEXT:
		sscanf (state->content->str, "(%d %d)%s", &state->object->u.text.x,
		        &state->object->u.text.y, state->object->u.text.str);
		state->symbol->symbol_objects =
		    g_slist_prepend (state->symbol->symbol_objects, state->object);
		state->state = PARSE_SYMBOL_OBJECTS;
		break;

	case PARSE_SYMBOL_CONNECTIONS:
		state->state = PARSE_SYMBOL;
		state->symbol->connections = g_slist_reverse (state->symbol->connections);
		break;
	case PARSE_SYMBOL_CONNECTION:
		sscanf (state->content->str, "(%lf %lf)", &state->connection->pos.x,
		        &state->connection->pos.y);
		state->symbol->connections =
		    g_slist_prepend (state->symbol->connections, state->connection);
		state->state = PARSE_SYMBOL_CONNECTIONS;
		break;

	case PARSE_PARTS:
		state->state = PARSE_LIBRARY;
		break;
	case PARSE_PART:
		g_hash_table_insert (state->library->part_hash, state->part->name, state->part);
		state->state = PARSE_PARTS;
		break;
	case PARSE_PART_NAME:
		state->part->name = g_strdup (state->content->str);
		state->state = PARSE_PART;
		break;
	case PARSE_PART_DESCRIPTION:
		state->part->description = g_strdup (state->content->str);
		state->state = PARSE_PART;
		break;
	case PARSE_PART_USESYMBOL:
		state->part->symbol_name = g_strdup (state->content->str);
		state->state = PARSE_PART;
		break;
	case PARSE_PART_LABELS:
		state->state = PARSE_PART;
		break;
	case PARSE_PART_LABEL:
		state->state = PARSE_PART_LABELS;
		state->part->labels = g_slist_prepend (state->part->labels, state->label);
		break;
	case PARSE_PART_LABEL_NAME:
		state->label->name = g_strdup (state->content->str);
		state->state = PARSE_PART_LABEL;
		break;
	case PARSE_PART_LABEL_TEXT:
		state->label->text = g_strdup (state->content->str);
		state->state = PARSE_PART_LABEL;
		break;
	case PARSE_PART_LABEL_POS:
		sscanf (state->content->str, "(%lf %lf)", &state->label->pos.x, &state->label->pos.y);
		state->state = PARSE_PART_LABEL;
		break;
	case PARSE_PART_PROPERTIES:
		state->state = PARSE_PART;
		break;
	case PARSE_PART_PROPERTY:
		state->state = PARSE_PART_PROPERTIES;
		state->part->properties = g_slist_prepend (state->part->properties, state->property);
		break;
	case PARSE_PART_PROPERTY_NAME:
		state->property->name = g_strdup (state->content->str);
		state->state = PARSE_PART_PROPERTY;
		break;
	case PARSE_PART_PROPERTY_VALUE:
		state->property->value = g_strdup (state->content->str);
		state->state = PARSE_PART_PROPERTY;
		break;

	case PARSE_LIBRARY:
		// The end of the file.
		state->state = PARSE_FINISH;
		break;
	case PARSE_ERROR:
		break;
	case PARSE_START:
	case PARSE_FINISH:
		// There should not be a closing tag in this state.
		g_assert_not_reached ();
		break;
	}
	// g_message("End element %s (state %s)", name, states[state->state]);
}

static void my_characters (ParseState *state, const xmlChar *chars, int len)
{
	int i;

	if (state->state == PARSE_FINISH || state->state == PARSE_START ||
	    state->state == PARSE_PARTS || state->state == PARSE_PART)
		return;

	for (i = 0; i < len; i++)
		g_string_append_c (state->content, chars[i]);
}

static xmlEntityPtr get_entity (void *user_data, const xmlChar *name)
{
	return xmlGetPredefinedEntity (name);
}

static void my_warning (void *user_data, const char *msg, ...)
{
	va_list args;

	va_start (args, msg);
	g_logv ("XML", G_LOG_LEVEL_WARNING, msg, args);
	va_end (args);
}

static void my_error (void *user_data, const char *msg, ...)
{
	va_list args;

	va_start (args, msg);
	g_logv ("XML", G_LOG_LEVEL_CRITICAL, msg, args);
	va_end (args);
}

static void my_fatal_error (void *user_data, const char *msg, ...)
{
	va_list args;

	va_start (args, msg);
	g_logv ("XML", G_LOG_LEVEL_ERROR, msg, args);
	va_end (args);
}
