/*
 * load-schematic.c
 *
 *
 * Authors:
 *  Richard Hult <rhult@hem.passagen.se>
 *  Ricardo Markiewicz <rmarkie@fi.uba.ar>
 *  Andres de Barbara <adebarbara@fi.uba.ar>
 *  Marc Lorber <lorber.marc@wanadoo.fr>
 *  Bernhard Schuster <bernhard@ahoi.io>
 *  Guido Trentalancia <guido@trentalancia.com>
 *
 * Web page: https://ahoi.io/project/oregano
 *
 * Copyright (C) 1999-2001  Richard Hult
 * Copyright (C) 2003,2006  Ricardo Markiewicz
 * Copyright (C) 2009-2012  Marc Lorber
 * Copyright (C) 2013-2014  Bernhard Schuster
 * Copyright (C) 2017       Guido Trentalancia
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

#include <string.h>
#include <glib/gi18n.h>

#include "xml-compat.h"
#include "oregano.h"
#include "xml-helper.h"
#include "load-common.h"
#include "load-schematic.h"
#include "coords.h"
#include "part-label.h"
#include "schematic.h"
#include "sim-settings.h"
#include "textbox.h"
#include "errors.h"
#include "engines/netlist-helper.h"

#include "debug.h"

typedef enum {
	PARSE_START,
	PARSE_SCHEMATIC,
	PARSE_TITLE,
	PARSE_AUTHOR,
	PARSE_VERSION,
	PARSE_COMMENTS,

	PARSE_SIMULATION_SETTINGS,
	PARSE_TRANSIENT_SETTINGS,
	PARSE_TRANSIENT_ENABLED,
	PARSE_TRANSIENT_START,
	PARSE_TRANSIENT_STOP,
	PARSE_TRANSIENT_STEP,
	PARSE_TRANSIENT_STEP_ENABLE,
	PARSE_TRANSIENT_INIT_COND,
	PARSE_TRANSIENT_ANALYZE_ALL,

	PARSE_AC_SETTINGS,
	PARSE_AC_ENABLED,
	PARSE_AC_VOUT,
	PARSE_AC_TYPE,
	PARSE_AC_NPOINTS,
	PARSE_AC_START,
	PARSE_AC_STOP,

	PARSE_DC_SETTINGS,
	PARSE_DC_ENABLED,
	PARSE_DC_VSRC,
	PARSE_DC_VOUT,
	PARSE_DC_START,
	PARSE_DC_STOP,
	PARSE_DC_STEP,

	PARSE_FOURIER_SETTINGS,
	PARSE_FOURIER_ENABLED,
	PARSE_FOURIER_FREQ,
	PARSE_FOURIER_VOUT,

	PARSE_NOISE_SETTINGS,
	PARSE_NOISE_ENABLED,
	PARSE_NOISE_VSRC,
	PARSE_NOISE_VOUT,
	PARSE_NOISE_TYPE,
	PARSE_NOISE_NPOINTS,
	PARSE_NOISE_START,
	PARSE_NOISE_STOP,

	PARSE_OPTION_LIST,
	PARSE_OPTION,
	PARSE_OPTION_NAME,
	PARSE_OPTION_VALUE,

	PARSE_ZOOM,

	PARSE_PARTS,
	PARSE_PART,
	PARSE_PART_NAME,
	PARSE_PART_LIBNAME,
	PARSE_PART_REFDES,
	PARSE_PART_POSITION,
	PARSE_PART_ROTATION,
	PARSE_PART_FLIP,
	PARSE_PART_SYMNAME,
	PARSE_PART_TEMPLATE,
	PARSE_PART_MODEL,
	PARSE_PART_LABELS,
	PARSE_PART_LABEL,
	PARSE_PART_LABEL_NAME,
	PARSE_PART_LABEL_TEXT,
	PARSE_PART_LABEL_POS,
	PARSE_PART_PROPERTIES,
	PARSE_PART_PROPERTY,
	PARSE_PART_PROPERTY_NAME,
	PARSE_PART_PROPERTY_VALUE,
	PARSE_WIRES,
	PARSE_WIRE,
	PARSE_WIRE_POINTS,
	//	PARSE_WIRE_LABEL,
	PARSE_TEXTBOXES,
	PARSE_TEXTBOX,
	PARSE_TEXTBOX_TEXT,
	PARSE_TEXTBOX_POSITION,
	PARSE_FINISH,
	PARSE_UNKNOWN,
	PARSE_ERROR
} State;

typedef struct
{
	State state;
	State prev_state;
	int unknown_depth;
	GString *content;
	Schematic *schematic;
	SimSettings *sim_settings;

	char *author;
	char *title;
	char *oregano_version;
	char *comments;

	char *textbox_text;

	// Temporary place holder for a wire
	Coords wire_start;
	Coords wire_end;

	// Temporary place holder for a part
	LibraryPart *part;
	PartLabel *label;
	Property *property;
	Coords pos;
	int rotation;
	IDFlip flip;

	// Temporary place holder for an option
	SimOption *option;
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

static void create_wire (ParseState *state);
static void create_part (ParseState *state);

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
    0,                                    //(commentSAXFunc)0,	 comment
    (warningSAXFunc)my_warning,           // warning
    (errorSAXFunc)my_error,               // error
    (fatalErrorSAXFunc)my_fatal_error,    // fatalError
};

static void create_textbox (ParseState *state)
{
	Textbox *textbox;

	textbox = textbox_new (NULL);
	textbox_set_text (textbox, state->textbox_text);
	item_data_set_pos (ITEM_DATA (textbox), &state->pos);
	schematic_add_item (state->schematic, ITEM_DATA (textbox));
}

static void create_wire (ParseState *state)
{
	Coords start_pos, length;
	Wire *wire;

	start_pos.x = state->wire_start.x;
	start_pos.y = state->wire_start.y;
	length.x = state->wire_end.x - start_pos.x;
	length.y = state->wire_end.y - start_pos.y;

	wire = wire_new ();
	wire_set_length (wire, &length);

	item_data_set_pos (ITEM_DATA (wire), &state->wire_start);
	schematic_add_item (state->schematic, ITEM_DATA (wire));
}

static void create_part (ParseState *state)
{
	Part *part;
	LibraryPart *library_part = state->part;

	part = part_new_from_library_part (library_part);
	if (!part) {
		g_warning ("Failed to create Part from LibraryPart");
		return;
	}

	item_data_set_pos (ITEM_DATA (part), &state->pos);
	item_data_rotate (ITEM_DATA (part), state->rotation, NULL);
	if (state->flip & ID_FLIP_HORIZ)
		item_data_flip (ITEM_DATA (part), ID_FLIP_HORIZ, NULL);
	if (state->flip & ID_FLIP_VERT)
		item_data_flip (ITEM_DATA (part), ID_FLIP_VERT, NULL);

	schematic_add_item (state->schematic, ITEM_DATA (part));
}

int schematic_parse_xml_file (Schematic *sm, const char *filename, GError **error)
{
	ParseState state;
	int retval = 0;

	state.schematic = sm;
	state.sim_settings = schematic_get_sim_settings (sm);
	state.author = NULL;
	state.title = NULL;
	state.oregano_version = NULL;
	state.comments = NULL;

	if (!oreganoXmlSAXParseFile (&oreganoSAXParser, &state, filename)) {
		g_warning ("Document not well formed!");
		if (error != NULL) {
			g_set_error (error, OREGANO_ERROR, OREGANO_SCHEMATIC_BAD_FILE_FORMAT,
			             _ ("Bad file format."));
		}
		retval = -1;
	}

	if (state.state == PARSE_ERROR) {
		if (error != NULL) {
			g_set_error (error, OREGANO_ERROR, OREGANO_SCHEMATIC_BAD_FILE_FORMAT,
			             _ ("Unknown parser error."));
		}
		retval = -2;
	}

	schematic_set_filename(sm, filename);
	schematic_set_author (sm, state.author);
	schematic_set_title (sm, state.title);
	schematic_set_version(sm, state.oregano_version);
	schematic_set_comments (sm, state.comments);

	g_free(state.author);
	g_free(state.title);
	g_free(state.oregano_version);
	g_free(state.comments);

	update_schematic(sm);

	return retval;
}

static void start_document (ParseState *state)
{
	state->state = PARSE_START;
	state->unknown_depth = 0;
	state->prev_state = PARSE_UNKNOWN;

	state->content = g_string_sized_new (128);
	state->part = NULL;
}

static void end_document (ParseState *state)
{

	if (state->unknown_depth != 0)
		g_warning ("unknown_depth != 0 (%d)", state->unknown_depth);
}

static void start_element (ParseState *state, const xmlChar *name, const xmlChar **attrs)
{
	switch (state->state) {
	case PARSE_START:
		if (xmlStrcmp (BAD_CAST name, BAD_CAST "ogo:schematic")) {
			g_warning ("Expecting 'ogo:schematic'.  Got '%s'", name);
			state->state = PARSE_ERROR;
		} else
			state->state = PARSE_SCHEMATIC;
		break;

	case PARSE_SCHEMATIC:
		if (!xmlStrcmp (BAD_CAST name, BAD_CAST "ogo:author")) {
			state->state = PARSE_AUTHOR;
			g_string_truncate (state->content, 0);
		} else if (!xmlStrcmp (BAD_CAST name, BAD_CAST "ogo:title")) {
			state->state = PARSE_TITLE;
			g_string_truncate (state->content, 0);
		} else if (!xmlStrcmp (BAD_CAST name, BAD_CAST "ogo:version")) {
			state->state = PARSE_VERSION;
			g_string_truncate (state->content, 0);
		} else if (!xmlStrcmp (BAD_CAST name, BAD_CAST "ogo:comments")) {
			state->state = PARSE_COMMENTS;
			g_string_truncate (state->content, 0);
		} else if (!xmlStrcmp (BAD_CAST name, BAD_CAST "ogo:zoom")) {
			state->state = PARSE_ZOOM;
			g_string_truncate (state->content, 0);
		} else if (!xmlStrcmp (BAD_CAST name, BAD_CAST "ogo:simulation-settings")) {
			state->state = PARSE_SIMULATION_SETTINGS;
		} else if (!xmlStrcmp (BAD_CAST name, BAD_CAST "ogo:parts")) {
			state->state = PARSE_PARTS;
		} else if (!xmlStrcmp (BAD_CAST name, BAD_CAST "ogo:wires")) {
			state->state = PARSE_WIRES;
		} else if (!xmlStrcmp (BAD_CAST name, BAD_CAST "ogo:textboxes")) {
			state->state = PARSE_TEXTBOXES;
		} else {
			state->prev_state = state->state;
			state->state = PARSE_UNKNOWN;
			state->unknown_depth++;
		}
		break;

	case PARSE_SIMULATION_SETTINGS:
		if (!xmlStrcmp (BAD_CAST name, BAD_CAST "ogo:transient")) {
			state->state = PARSE_TRANSIENT_SETTINGS;
		} else if (!xmlStrcmp (BAD_CAST name, BAD_CAST "ogo:ac")) {
			state->state = PARSE_AC_SETTINGS;
		} else if (!xmlStrcmp (BAD_CAST name, BAD_CAST "ogo:dc-sweep")) {
			state->state = PARSE_DC_SETTINGS;
		} else if (!xmlStrcmp (BAD_CAST name, BAD_CAST "ogo:fourier")) {
			state->state = PARSE_FOURIER_SETTINGS;
		} else if (!xmlStrcmp (BAD_CAST name, BAD_CAST "ogo:noise")) {
			state->state = PARSE_NOISE_SETTINGS;
		} else if (!xmlStrcmp (BAD_CAST name, BAD_CAST "ogo:options")) {
			state->state = PARSE_OPTION_LIST;
		} else {
			state->prev_state = state->state;
			state->state = PARSE_UNKNOWN;
			state->unknown_depth++;
		}
		break;

	case PARSE_TRANSIENT_SETTINGS:
		if (!xmlStrcmp (BAD_CAST name, BAD_CAST "ogo:enabled")) {
			state->state = PARSE_TRANSIENT_ENABLED;
			g_string_truncate (state->content, 0);
		} else if (!xmlStrcmp (BAD_CAST name, BAD_CAST "ogo:start")) {
			state->state = PARSE_TRANSIENT_START;
			g_string_truncate (state->content, 0);
		} else if (!xmlStrcmp (BAD_CAST name, BAD_CAST "ogo:stop")) {
			state->state = PARSE_TRANSIENT_STOP;
			g_string_truncate (state->content, 0);
		} else if (!xmlStrcmp (BAD_CAST name, BAD_CAST "ogo:step")) {
			state->state = PARSE_TRANSIENT_STEP;
			g_string_truncate (state->content, 0);
		} else if (!xmlStrcmp (BAD_CAST name, BAD_CAST "ogo:step-enabled")) {
			state->state = PARSE_TRANSIENT_STEP_ENABLE;
			g_string_truncate (state->content, 0);
		} else if (!xmlStrcmp (BAD_CAST name, BAD_CAST "ogo:init-conditions")) {
			state->state = PARSE_TRANSIENT_INIT_COND;
			g_string_truncate (state->content, 0);
		} else if (!xmlStrcmp (BAD_CAST name, BAD_CAST "ogo:analyze-all")) {
			state->state = PARSE_TRANSIENT_ANALYZE_ALL;
			g_string_truncate (state->content, 0);
		} else {
			state->prev_state = state->state;
			state->state = PARSE_UNKNOWN;
			state->unknown_depth++;
		}
		break;

	case PARSE_AC_SETTINGS:
		if (!xmlStrcmp (BAD_CAST name, BAD_CAST "ogo:enabled")) {
			state->state = PARSE_AC_ENABLED;
			g_string_truncate (state->content, 0);
		} else if (!xmlStrcmp (BAD_CAST name, BAD_CAST "ogo:vout1")) {
			state->state = PARSE_AC_VOUT;
			g_string_truncate (state->content, 0);
		} else if (!xmlStrcmp (BAD_CAST name, BAD_CAST "ogo:type")) {
			state->state = PARSE_AC_TYPE;
			g_string_truncate (state->content, 0);
		} else if (!xmlStrcmp (BAD_CAST name, BAD_CAST "ogo:npoints")) {
			state->state = PARSE_AC_NPOINTS;
			g_string_truncate (state->content, 0);
		} else if (!xmlStrcmp (BAD_CAST name, BAD_CAST "ogo:start")) {
			state->state = PARSE_AC_START;
			g_string_truncate (state->content, 0);
		} else if (!xmlStrcmp (BAD_CAST name, BAD_CAST "ogo:stop")) {
			state->state = PARSE_AC_STOP;
			g_string_truncate (state->content, 0);
		} else {
			state->prev_state = state->state;
			state->state = PARSE_UNKNOWN;
			state->unknown_depth++;
		}
		break;

	case PARSE_DC_SETTINGS:
		if (!xmlStrcmp (BAD_CAST name, BAD_CAST "ogo:enabled")) {
			state->state = PARSE_DC_ENABLED;
			g_string_truncate (state->content, 0);
		} else if (!xmlStrcmp (BAD_CAST name, BAD_CAST "ogo:vsrc1")) {
			state->state = PARSE_DC_VSRC;
			g_string_truncate (state->content, 0);
		} else if (!xmlStrcmp (BAD_CAST name, BAD_CAST "ogo:vout1")) {
			state->state = PARSE_DC_VOUT;
			g_string_truncate (state->content, 0);
		} else if (!xmlStrcmp (BAD_CAST name, BAD_CAST "ogo:start1")) {
			state->state = PARSE_DC_START;
			g_string_truncate (state->content, 0);
		} else if (!xmlStrcmp (BAD_CAST name, BAD_CAST "ogo:stop1")) {
			state->state = PARSE_DC_STOP;
			g_string_truncate (state->content, 0);
		} else if (!xmlStrcmp (BAD_CAST name, BAD_CAST "ogo:step1")) {
			state->state = PARSE_DC_STEP;
			g_string_truncate (state->content, 0);
		} else {
			state->prev_state = state->state;
			state->state = PARSE_UNKNOWN;
			state->unknown_depth++;
		}
		break;

	case PARSE_FOURIER_SETTINGS:
		if (!xmlStrcmp (BAD_CAST name, BAD_CAST "ogo:enabled")) {
			state->state = PARSE_FOURIER_ENABLED;
			g_string_truncate (state->content, 0);
		} else if (!xmlStrcmp (BAD_CAST name, BAD_CAST "ogo:freq")) {
			state->state = PARSE_FOURIER_FREQ;
			g_string_truncate (state->content, 0);
		} else if (!xmlStrcmp (BAD_CAST name, BAD_CAST "ogo:vout")) {
			state->state = PARSE_FOURIER_VOUT;
			g_string_truncate (state->content, 0);
		} else {
			state->prev_state = state->state;
			state->state = PARSE_UNKNOWN;
			state->unknown_depth++;
		}
		break;

	case PARSE_NOISE_SETTINGS:
		if (!xmlStrcmp (BAD_CAST name, BAD_CAST "ogo:enabled")) {
			state->state = PARSE_NOISE_ENABLED;
			g_string_truncate (state->content, 0);
		} else if (!xmlStrcmp (BAD_CAST name, BAD_CAST "ogo:vsrc1")) {
			state->state = PARSE_NOISE_VSRC;
			g_string_truncate (state->content, 0);
		} else if (!xmlStrcmp (BAD_CAST name, BAD_CAST "ogo:vout1")) {
			state->state = PARSE_NOISE_VOUT;
			g_string_truncate (state->content, 0);
		} else if (!xmlStrcmp (BAD_CAST name, BAD_CAST "ogo:type")) {
			state->state = PARSE_NOISE_TYPE;
			g_string_truncate (state->content, 0);
		} else if (!xmlStrcmp (BAD_CAST name, BAD_CAST "ogo:npoints")) {
			state->state = PARSE_NOISE_NPOINTS;
			g_string_truncate (state->content, 0);
		} else if (!xmlStrcmp (BAD_CAST name, BAD_CAST "ogo:start")) {
			state->state = PARSE_NOISE_START;
			g_string_truncate (state->content, 0);
		} else if (!xmlStrcmp (BAD_CAST name, BAD_CAST "ogo:stop")) {
			state->state = PARSE_NOISE_STOP;
			g_string_truncate (state->content, 0);
		} else {
			state->prev_state = state->state;
			state->state = PARSE_UNKNOWN;
			state->unknown_depth++;
		}
		break;

	case PARSE_OPTION_LIST:
		if (!xmlStrcmp (BAD_CAST name, BAD_CAST "ogo:option")) {
			state->state = PARSE_OPTION;
			state->option = g_new0 (SimOption, 1);
			g_string_truncate (state->content, 0);
		} else {
			state->prev_state = state->state;
			state->state = PARSE_UNKNOWN;
			state->unknown_depth++;
		}
		break;
	case PARSE_OPTION:
		if (!xmlStrcmp (BAD_CAST name, BAD_CAST "ogo:name")) {
			state->state = PARSE_OPTION_NAME;
			g_string_truncate (state->content, 0);
		} else if (!xmlStrcmp (BAD_CAST name, BAD_CAST "ogo:value")) {
			state->state = PARSE_OPTION_VALUE;
			g_string_truncate (state->content, 0);
		} else {
			state->prev_state = state->state;
			state->state = PARSE_UNKNOWN;
			state->unknown_depth++;
		}

		break;
	case PARSE_PARTS:
		if (!xmlStrcmp (BAD_CAST name, BAD_CAST "ogo:part")) {
			state->state = PARSE_PART;
			state->part = g_new0 (LibraryPart, 1);
			state->rotation = 0;
			state->flip = ID_FLIP_NONE;
			state->label = NULL;
			state->property = NULL;
		} else {
			state->prev_state = state->state;
			state->state = PARSE_UNKNOWN;
			state->unknown_depth++;
		}
		break;
	case PARSE_PART:
		if (!xmlStrcmp (BAD_CAST name, BAD_CAST "ogo:name")) {
			state->state = PARSE_PART_NAME;
			g_string_truncate (state->content, 0);
		} else if (!xmlStrcmp (BAD_CAST name, BAD_CAST "ogo:library")) {
			state->state = PARSE_PART_LIBNAME;
			g_string_truncate (state->content, 0);
		} else if (!xmlStrcmp (BAD_CAST name, BAD_CAST "ogo:refdes")) {
			state->state = PARSE_PART_REFDES;
			g_string_truncate (state->content, 0);
		} else if (!xmlStrcmp (BAD_CAST name, BAD_CAST "ogo:position")) {
			state->state = PARSE_PART_POSITION;
			g_string_truncate (state->content, 0);
		} else if (!xmlStrcmp (BAD_CAST name, BAD_CAST "ogo:rotation")) {
			state->state = PARSE_PART_ROTATION;
			g_string_truncate (state->content, 0);
		} else if (!xmlStrcmp (BAD_CAST name, BAD_CAST "ogo:flip")) {
			state->state = PARSE_PART_FLIP;
			g_string_truncate (state->content, 0);
		} else if (!xmlStrcmp (BAD_CAST name, BAD_CAST "ogo:template")) {
			state->state = PARSE_PART_TEMPLATE;
			g_string_truncate (state->content, 0);
		} else if (!xmlStrcmp (BAD_CAST name, BAD_CAST "ogo:model")) {
			state->state = PARSE_PART_MODEL;
			g_string_truncate (state->content, 0);
		} else if (!xmlStrcmp (BAD_CAST name, BAD_CAST "ogo:symbol")) {
			state->state = PARSE_PART_SYMNAME;
			g_string_truncate (state->content, 0);
		} else if (!xmlStrcmp (BAD_CAST name, BAD_CAST "ogo:labels")) {
			state->state = PARSE_PART_LABELS;
		} else if (!xmlStrcmp (BAD_CAST name, BAD_CAST "ogo:properties")) {
			state->state = PARSE_PART_PROPERTIES;
		} else {
			state->prev_state = state->state;
			state->state = PARSE_UNKNOWN;
			state->unknown_depth++;
		}
		break;
	case PARSE_PART_LABELS:
		if (!xmlStrcmp (BAD_CAST name, BAD_CAST "ogo:label")) {
			state->state = PARSE_PART_LABEL;
			state->label = g_new0 (PartLabel, 1);
		} else {
			state->prev_state = state->state;
			state->state = PARSE_UNKNOWN;
			state->unknown_depth++;
		}
		break;
	case PARSE_PART_LABEL:
		if (!xmlStrcmp (BAD_CAST name, BAD_CAST "ogo:name")) {
			state->state = PARSE_PART_LABEL_NAME;
			g_string_truncate (state->content, 0);
		} else if (!xmlStrcmp (BAD_CAST name, BAD_CAST "ogo:text")) {
			state->state = PARSE_PART_LABEL_TEXT;
			g_string_truncate (state->content, 0);
		} else if (!xmlStrcmp (BAD_CAST name, BAD_CAST "ogo:position")) {
			state->state = PARSE_PART_LABEL_POS;
			g_string_truncate (state->content, 0);
		} else {
			state->prev_state = state->state;
			state->state = PARSE_UNKNOWN;
			state->unknown_depth++;
		}
		break;

	case PARSE_PART_PROPERTIES:
		if (!xmlStrcmp (BAD_CAST name, BAD_CAST "ogo:property")) {
			state->state = PARSE_PART_PROPERTY;
			state->property = g_new0 (Property, 1);
		} else {
			state->prev_state = state->state;
			state->state = PARSE_UNKNOWN;
			state->unknown_depth++;
		}
		break;
	case PARSE_PART_PROPERTY:
		if (!xmlStrcmp (BAD_CAST name, BAD_CAST "ogo:name")) {
			state->state = PARSE_PART_PROPERTY_NAME;
			g_string_truncate (state->content, 0);
		} else if (!xmlStrcmp (BAD_CAST name, BAD_CAST "ogo:value")) {
			state->state = PARSE_PART_PROPERTY_VALUE;
			g_string_truncate (state->content, 0);
		} else {
			state->prev_state = state->state;
			state->state = PARSE_UNKNOWN;
			state->unknown_depth++;
		}
		break;

	case PARSE_WIRES:
		if (!xmlStrcmp (BAD_CAST name, BAD_CAST "ogo:wire")) {
			state->state = PARSE_WIRE;
		} else {
			state->prev_state = state->state;
			state->state = PARSE_UNKNOWN;
			state->unknown_depth++;
		}
		break;
	case PARSE_WIRE:
		if (!xmlStrcmp (BAD_CAST name, BAD_CAST "ogo:points")) {
			state->state = PARSE_WIRE_POINTS;
			g_string_truncate (state->content, 0);
		} else {
			state->prev_state = state->state;
			state->state = PARSE_UNKNOWN;
			state->unknown_depth++;
		}
		break;

	case PARSE_TEXTBOXES:
		if (!xmlStrcmp (BAD_CAST name, BAD_CAST "ogo:textbox")) {
			state->state = PARSE_TEXTBOX;
		} else {
			state->prev_state = state->state;
			state->state = PARSE_UNKNOWN;
			state->unknown_depth++;
		}
		break;
	case PARSE_TEXTBOX:
		if (!xmlStrcmp (BAD_CAST name, BAD_CAST "ogo:position")) {
			state->state = PARSE_TEXTBOX_POSITION;
			g_string_truncate (state->content, 0);
		} else if (!xmlStrcmp (BAD_CAST name, BAD_CAST "ogo:text")) {
			state->state = PARSE_TEXTBOX_TEXT;
			g_string_truncate (state->content, 0);
		} else {
			state->prev_state = state->state;
			state->state = PARSE_UNKNOWN;
			state->unknown_depth++;
		}
		break;

	case PARSE_TRANSIENT_ENABLED:
	case PARSE_TRANSIENT_START:
	case PARSE_TRANSIENT_STOP:
	case PARSE_TRANSIENT_STEP:
	case PARSE_TRANSIENT_STEP_ENABLE:

	case PARSE_AC_ENABLED:
	case PARSE_AC_VOUT:
	case PARSE_AC_TYPE:
	case PARSE_AC_NPOINTS:
	case PARSE_AC_START:
	case PARSE_AC_STOP:

	case PARSE_DC_ENABLED:
	case PARSE_DC_VSRC:
	case PARSE_DC_VOUT:
	case PARSE_DC_START:
	case PARSE_DC_STOP:
	case PARSE_DC_STEP:

	case PARSE_FOURIER_ENABLED:
	case PARSE_FOURIER_FREQ:
	case PARSE_FOURIER_VOUT:

	case PARSE_NOISE_ENABLED:
	case PARSE_NOISE_VSRC:
	case PARSE_NOISE_VOUT:
	case PARSE_NOISE_TYPE:
	case PARSE_NOISE_NPOINTS:
	case PARSE_NOISE_START:
	case PARSE_NOISE_STOP:

	case PARSE_WIRE_POINTS:
	case PARSE_OPTION_NAME:
	case PARSE_OPTION_VALUE:
	case PARSE_PART_NAME:
	case PARSE_PART_LIBNAME:
	case PARSE_PART_TEMPLATE:
	case PARSE_PART_MODEL:
	case PARSE_PART_REFDES:
	case PARSE_PART_POSITION:
	case PARSE_PART_ROTATION:
	case PARSE_PART_FLIP:
	case PARSE_PART_SYMNAME:
	case PARSE_PART_LABEL_NAME:
	case PARSE_PART_LABEL_TEXT:
	case PARSE_PART_LABEL_POS:
	case PARSE_PART_PROPERTY_NAME:
	case PARSE_PART_PROPERTY_VALUE:
	case PARSE_TEXTBOX_POSITION:
	case PARSE_TEXTBOX_TEXT:
	case PARSE_ZOOM:
	case PARSE_TITLE:
	case PARSE_AUTHOR:
	case PARSE_VERSION:
	case PARSE_COMMENTS:
	case PARSE_TRANSIENT_INIT_COND:
	case PARSE_TRANSIENT_ANALYZE_ALL:
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
	GList *libs;
	Schematic *schematic = state->schematic;

	switch (state->state) {
	case PARSE_UNKNOWN:
		state->unknown_depth--;
		if (state->unknown_depth == 0)
			state->state = state->prev_state;
		break;
	case PARSE_AUTHOR:
		state->author = g_strdup (state->content->str);
		state->state = PARSE_SCHEMATIC;
		break;
	case PARSE_TITLE:
		state->title = g_strdup (state->content->str);
		state->state = PARSE_SCHEMATIC;
		break;
	case PARSE_VERSION:
		state->oregano_version = g_strdup (state->content->str);
		state->state = PARSE_SCHEMATIC;
		break;
	case PARSE_COMMENTS:
		state->comments = g_strdup (state->content->str);
		state->state = PARSE_SCHEMATIC;
		break;
	case PARSE_ZOOM: {
		double zoom;

		zoom = g_strtod (state->content->str, NULL);
		schematic_set_zoom (state->schematic, zoom);
		state->state = PARSE_SCHEMATIC;

		break;
	}

	case PARSE_SIMULATION_SETTINGS:
		state->state = PARSE_SCHEMATIC;
		break;

	case PARSE_TRANSIENT_SETTINGS:
		state->state = PARSE_SIMULATION_SETTINGS;
		break;
	case PARSE_TRANSIENT_ENABLED:
		sim_settings_set_trans (state->sim_settings,
		                        !g_ascii_strcasecmp (state->content->str, "true"));
		state->state = PARSE_TRANSIENT_SETTINGS;
		break;
	case PARSE_TRANSIENT_START:
		sim_settings_set_trans_start (state->sim_settings, state->content->str);
		state->state = PARSE_TRANSIENT_SETTINGS;
		break;
	case PARSE_TRANSIENT_STOP:
		sim_settings_set_trans_stop (state->sim_settings, state->content->str);
		state->state = PARSE_TRANSIENT_SETTINGS;
		break;
	case PARSE_TRANSIENT_STEP:
		sim_settings_set_trans_step (state->sim_settings, state->content->str);
		state->state = PARSE_TRANSIENT_SETTINGS;
		break;
	case PARSE_TRANSIENT_STEP_ENABLE:
		sim_settings_set_trans_step_enable (state->sim_settings,
		                                    !g_ascii_strcasecmp (state->content->str, "true"));
		state->state = PARSE_TRANSIENT_SETTINGS;
		break;
	case PARSE_TRANSIENT_INIT_COND:
		sim_settings_set_trans_init_cond (state->sim_settings,
		                                  !g_ascii_strcasecmp (state->content->str, "true"));
		state->state = PARSE_TRANSIENT_SETTINGS;
		break;
	case PARSE_TRANSIENT_ANALYZE_ALL:
		sim_settings_set_trans_analyze_all(state->sim_settings,
		                                !g_ascii_strcasecmp (state->content->str, "true"));
		state->state = PARSE_TRANSIENT_SETTINGS;
		break;

	case PARSE_AC_SETTINGS:
		state->state = PARSE_SIMULATION_SETTINGS;
		break;
	case PARSE_AC_ENABLED:
		sim_settings_set_ac (state->sim_settings,
		                     !g_ascii_strcasecmp (state->content->str, "true"));
		state->state = PARSE_AC_SETTINGS;
		break;
	case PARSE_AC_VOUT:
		sim_settings_set_ac_vout (state->sim_settings, state->content->str);
		state->state = PARSE_AC_SETTINGS;
		break;
	case PARSE_AC_TYPE:
		sim_settings_set_ac_type (state->sim_settings, state->content->str);
		state->state = PARSE_AC_SETTINGS;
		break;
	case PARSE_AC_NPOINTS:
		sim_settings_set_ac_npoints (state->sim_settings, state->content->str);
		state->state = PARSE_AC_SETTINGS;
		break;
	case PARSE_AC_START:
		sim_settings_set_ac_start (state->sim_settings, state->content->str);
		state->state = PARSE_AC_SETTINGS;
		break;
	case PARSE_AC_STOP:
		sim_settings_set_ac_stop (state->sim_settings, state->content->str);
		state->state = PARSE_AC_SETTINGS;
		break;

	case PARSE_DC_SETTINGS:
		state->state = PARSE_SIMULATION_SETTINGS;
		break;
	case PARSE_DC_ENABLED:
		sim_settings_set_dc (state->sim_settings,
		                     !g_ascii_strcasecmp (state->content->str, "true"));
		state->state = PARSE_DC_SETTINGS;
		break;
	case PARSE_DC_VSRC:
		sim_settings_set_dc_vsrc (state->sim_settings, state->content->str);
		state->state = PARSE_DC_SETTINGS;
		break;
	case PARSE_DC_VOUT:
		sim_settings_set_dc_vout (state->sim_settings, state->content->str);
		state->state = PARSE_DC_SETTINGS;
		break;
	case PARSE_DC_START:
		sim_settings_set_dc_start (state->sim_settings, state->content->str);
		state->state = PARSE_DC_SETTINGS;
		break;
	case PARSE_DC_STOP:
		sim_settings_set_dc_stop (state->sim_settings, state->content->str);
		state->state = PARSE_DC_SETTINGS;
		break;
	case PARSE_DC_STEP:
		sim_settings_set_dc_step (state->sim_settings, state->content->str);
		state->state = PARSE_DC_SETTINGS;
		break;

	case PARSE_FOURIER_SETTINGS:
		state->state = PARSE_SIMULATION_SETTINGS;
		break;
	case PARSE_FOURIER_ENABLED:
		sim_settings_set_fourier (state->sim_settings,
		                          !g_ascii_strcasecmp (state->content->str, "true"));
		state->state = PARSE_FOURIER_SETTINGS;
		break;
	case PARSE_FOURIER_FREQ:
		sim_settings_set_fourier_frequency (state->sim_settings, state->content->str);
		state->state = PARSE_FOURIER_SETTINGS;
		break;
	case PARSE_FOURIER_VOUT:
		sim_settings_set_fourier_vout (state->sim_settings, state->content->str);
		state->state = PARSE_FOURIER_SETTINGS;
		break;

	case PARSE_NOISE_SETTINGS:
		state->state = PARSE_SIMULATION_SETTINGS;
		break;
	case PARSE_NOISE_ENABLED:
		sim_settings_set_noise (state->sim_settings,
		                     !g_ascii_strcasecmp (state->content->str, "true"));
		state->state = PARSE_NOISE_SETTINGS;
		break;
	case PARSE_NOISE_VSRC:
		sim_settings_set_noise_vsrc (state->sim_settings, state->content->str);
		state->state = PARSE_NOISE_SETTINGS;
		break;
	case PARSE_NOISE_VOUT:
		sim_settings_set_noise_vout (state->sim_settings, state->content->str);
		state->state = PARSE_NOISE_SETTINGS;
		break;
	case PARSE_NOISE_TYPE:
		sim_settings_set_noise_type (state->sim_settings, state->content->str);
		state->state = PARSE_NOISE_SETTINGS;
		break;
	case PARSE_NOISE_NPOINTS:
		sim_settings_set_noise_npoints (state->sim_settings, state->content->str);
		state->state = PARSE_NOISE_SETTINGS;
		break;
	case PARSE_NOISE_START:
		sim_settings_set_noise_start (state->sim_settings, state->content->str);
		state->state = PARSE_NOISE_SETTINGS;
		break;
	case PARSE_NOISE_STOP:
		sim_settings_set_noise_stop (state->sim_settings, state->content->str);
		state->state = PARSE_NOISE_SETTINGS;
		break;

	case PARSE_OPTION_LIST:
		state->state = PARSE_SIMULATION_SETTINGS;
		break;
	case PARSE_OPTION:
		state->state = PARSE_OPTION_LIST;
		sim_settings_add_option (state->sim_settings, state->option);
		state->option = g_new0 (SimOption, 1);
		break;
	case PARSE_OPTION_NAME:
		state->option->name = g_strdup (state->content->str);
		state->state = PARSE_OPTION;
		break;

	case PARSE_OPTION_VALUE:
		state->option->value = g_strdup (state->content->str);
		state->state = PARSE_OPTION;
		break;

	case PARSE_PARTS:
		state->state = PARSE_SCHEMATIC;
		break;
	case PARSE_PART:
		create_part (state);
		state->state = PARSE_PARTS;
		break;
	case PARSE_PART_NAME:
		state->part->name = g_strdup (state->content->str);
		state->state = PARSE_PART;
		break;
	case PARSE_PART_LIBNAME:
		state->state = PARSE_PART;
		state->part->library = NULL;
		libs = oregano.libraries;
		while (libs) {
			Library *lib = (Library *)libs->data;
			if (g_ascii_strcasecmp (state->content->str, lib->name) == 0) {
				state->part->library = lib;
				break;
			}
			libs = libs->next;
		}
		break;
	case PARSE_PART_POSITION:
		sscanf (state->content->str, "(%lf %lf)", &state->pos.x, &state->pos.y);
		// Try to fix invalid positions
		if (state->pos.x < 0)
			state->pos.x = -state->pos.x;
		if (state->pos.y < 0)
			state->pos.y = -state->pos.y;
		// Determine the maximum parts' coordinates to be used during sheet creation
		if (state->pos.x > schematic_get_width(schematic))
			schematic_set_width(schematic, (guint) state->pos.x);
		if (state->pos.y > schematic_get_height(schematic))
			schematic_set_height(schematic, (guint) state->pos.y);
		state->state = PARSE_PART;
		break;
	case PARSE_PART_ROTATION:
		sscanf (state->content->str, "%d", &state->rotation);
		state->state = PARSE_PART;
		break;
	case PARSE_PART_FLIP:
		if (g_ascii_strcasecmp (state->content->str, "horizontal") == 0)
			state->flip = state->flip | ID_FLIP_HORIZ;
		else if (g_ascii_strcasecmp (state->content->str, "vertical") == 0)
			state->flip = state->flip | ID_FLIP_VERT;
		state->state = PARSE_PART;
		break;
	case PARSE_PART_REFDES:
		state->part->refdes = g_strdup (state->content->str);
		state->state = PARSE_PART;
		break;
	case PARSE_PART_TEMPLATE:
		state->part->template = g_strdup (state->content->str);
		state->state = PARSE_PART;
		break;
	case PARSE_PART_MODEL:
		state->part->model = g_strdup (state->content->str);
		state->state = PARSE_PART;
		break;
	case PARSE_PART_SYMNAME:
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

	case PARSE_WIRES:
		state->state = PARSE_SCHEMATIC;
		break;
	case PARSE_WIRE:
		state->state = PARSE_WIRES;
		break;
	case PARSE_WIRE_POINTS:
		sscanf (state->content->str, "(%lf %lf)(%lf %lf)", &state->wire_start.x,
		        &state->wire_start.y, &state->wire_end.x, &state->wire_end.y);
		create_wire (state);
		state->state = PARSE_WIRE;
		break;

	case PARSE_TEXTBOXES:
		state->state = PARSE_SCHEMATIC;
		break;
	case PARSE_TEXTBOX:
		create_textbox (state);
		state->state = PARSE_TEXTBOXES;
		break;
	case PARSE_TEXTBOX_POSITION:
		sscanf (state->content->str, "(%lf %lf)", &state->pos.x, &state->pos.y);
		state->state = PARSE_TEXTBOX;
		break;
	case PARSE_TEXTBOX_TEXT:
		state->textbox_text = g_strdup (state->content->str);
		state->state = PARSE_TEXTBOX;
		break;
	case PARSE_SCHEMATIC:
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

// FIXME this should not be critical but forward to the oregano log buffer
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
