/*
 * save-schematic.c
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

#include "xml-compat.h"
#include "oregano.h"
#include "schematic.h"
#include "coords.h"
#include "load-library.h"
#include "part.h"
#include "textbox.h"
#include "part-private.h"
#include "part-label.h"
#include "wire.h"
#include "sim-settings.h"
#include "node-store.h"
#include "save-schematic.h"
#include "xml-helper.h"

#include "debug.h"

typedef struct
{
	xmlDocPtr doc; // Xml document.
	xmlNsPtr ns;   // Main namespace.

	xmlNodePtr node_symbols; // For saving of symbols.
	xmlNodePtr node_parts;   // For saving of parts.
	xmlNodePtr node_props;   // For saving of properties.
	xmlNodePtr node_labels;  // For saving of labels.
	xmlNodePtr node_wires;   // For saving of wires.
	xmlNodePtr node_textboxes;
} parseXmlContext;

static void write_xml_sim_settings (xmlNodePtr cur, parseXmlContext *ctxt, Schematic *sm)
{
	xmlNodePtr sim_settings_node, child, analysis, options;
	gchar *str;
	SimSettings *s;
	SimOption *so;
	GList *list;

	s = schematic_get_sim_settings (sm);

	sim_settings_node = xmlNewChild (cur, ctxt->ns, BAD_CAST "simulation-settings", NULL);
	if (!sim_settings_node) {
		g_warning ("Failed during save of simulation settings.\n");
		return;
	}

	// Transient analysis
	analysis = xmlNewChild (sim_settings_node, ctxt->ns, BAD_CAST "transient", NULL);
	if (!analysis) {
		g_warning ("Failed during save of transient analysis settings.\n");
		return;
	}

	if (sim_settings_get_trans (s))
		str = "true";
	else
		str = "false";
	child = xmlNewChild (analysis, ctxt->ns, BAD_CAST "enabled", BAD_CAST str);

	str = g_strdup_printf ("%g", sim_settings_get_trans_start (s));
	child = xmlNewChild (analysis, ctxt->ns, BAD_CAST "start", BAD_CAST str);
	g_free (str);

	str = g_strdup_printf ("%g", sim_settings_get_trans_stop (s));
	child = xmlNewChild (analysis, ctxt->ns, BAD_CAST "stop", BAD_CAST str);
	g_free (str);

	str = g_strdup_printf ("%g", sim_settings_get_trans_step (s));
	child = xmlNewChild (analysis, ctxt->ns, BAD_CAST "step", BAD_CAST str);
	g_free (str);

	if (sim_settings_get_trans_step_enable (s))
		str = "true";
	else
		str = "false";
	child = xmlNewChild (analysis, ctxt->ns, BAD_CAST "step-enabled", BAD_CAST str);

	if (sim_settings_get_trans_init_cond (s))
		str = "true";
	else
		str = "false";
	child = xmlNewChild (analysis, ctxt->ns, BAD_CAST "init-conditions", BAD_CAST str);

	if (sim_settings_get_trans_analyze_all(s))
		str = "true";
	else
		str = "false";
	child = xmlNewChild (analysis, ctxt->ns, BAD_CAST "analyze-all", BAD_CAST str);

	//  AC analysis
	analysis = xmlNewChild (sim_settings_node, ctxt->ns, BAD_CAST "ac", NULL);
	if (!analysis) {
		g_warning ("Failed during save of AC analysis settings.\n");
		return;
	}
	child = xmlNewChild (analysis, ctxt->ns, BAD_CAST "enabled",
	                     BAD_CAST (sim_settings_get_ac (s) ? "true" : "false"));

	child =
	    xmlNewChild (analysis, ctxt->ns, BAD_CAST "vout1", BAD_CAST sim_settings_get_ac_vout (s));

	child =
	    xmlNewChild (analysis, ctxt->ns, BAD_CAST "type", BAD_CAST sim_settings_get_ac_type (s));

	str = g_strdup_printf ("%d", sim_settings_get_ac_npoints (s));
	child = xmlNewChild (analysis, ctxt->ns, BAD_CAST "npoints", BAD_CAST str);
	g_free (str);

	str = g_strdup_printf ("%g", sim_settings_get_ac_start (s));
	child = xmlNewChild (analysis, ctxt->ns, BAD_CAST "start", BAD_CAST str);
	g_free (str);

	str = g_strdup_printf ("%g", sim_settings_get_ac_stop (s));
	child = xmlNewChild (analysis, ctxt->ns, BAD_CAST "stop", BAD_CAST str);
	g_free (str);

	//  DC analysis
	analysis = xmlNewChild (sim_settings_node, ctxt->ns, BAD_CAST "dc-sweep", NULL);
	if (!analysis) {
		g_warning ("Failed during save of DC sweep analysis settings.\n");
		return;
	}
	child = xmlNewChild (analysis, ctxt->ns, BAD_CAST "enabled",
	                     BAD_CAST (sim_settings_get_dc (s) ? "true" : "false"));

	child =
	    xmlNewChild (analysis, ctxt->ns, BAD_CAST "vsrc1", BAD_CAST sim_settings_get_dc_vsrc (s));

	child =
	    xmlNewChild (analysis, ctxt->ns, BAD_CAST "vout1", BAD_CAST sim_settings_get_dc_vout (s));

	str = g_strdup_printf ("%g", sim_settings_get_dc_start (s));
	child = xmlNewChild (analysis, ctxt->ns, BAD_CAST "start1", BAD_CAST str);
	g_free (str);

	str = g_strdup_printf ("%g", sim_settings_get_dc_stop (s));
	child = xmlNewChild (analysis, ctxt->ns, BAD_CAST "stop1", BAD_CAST str);
	g_free (str);

	str = g_strdup_printf ("%g", sim_settings_get_dc_step (s));
	child = xmlNewChild (analysis, ctxt->ns, BAD_CAST "step1", BAD_CAST str);
	g_free (str);

	//  Fourier analysis
	analysis = xmlNewChild (sim_settings_node, ctxt->ns, BAD_CAST "fourier", NULL);
	if (!analysis) {
		g_warning ("Failed during save of Fourier analysis settings.\n");
		return;
	}
	child = xmlNewChild (analysis, ctxt->ns, BAD_CAST "enabled",
	                     BAD_CAST (sim_settings_get_fourier (s) ? "true" : "false"));

	str = g_strdup_printf ("%.3f", sim_settings_get_fourier_frequency (s));
	child = xmlNewChild (analysis, ctxt->ns, BAD_CAST "freq", BAD_CAST str);
	g_free (str);

	str = g_strdup_printf ("%s", sim_settings_get_fourier_vout (s));
	child = xmlNewChild (analysis, ctxt->ns, BAD_CAST "vout", BAD_CAST str);
	g_free (str);

	//  Noise analysis
	analysis = xmlNewChild (sim_settings_node, ctxt->ns, BAD_CAST "noise", NULL);
	if (!analysis) {
		g_warning ("Failed during save of Noise analysis settings.\n");
		return;
	}
	child = xmlNewChild (analysis, ctxt->ns, BAD_CAST "enabled",
	                     BAD_CAST (sim_settings_get_noise (s) ? "true" : "false"));

	child =
	    xmlNewChild (analysis, ctxt->ns, BAD_CAST "vsrc1", BAD_CAST sim_settings_get_noise_vsrc (s));

	child =
	    xmlNewChild (analysis, ctxt->ns, BAD_CAST "vout1", BAD_CAST sim_settings_get_noise_vout (s));

	child =
	    xmlNewChild (analysis, ctxt->ns, BAD_CAST "type", BAD_CAST sim_settings_get_noise_type (s));

	str = g_strdup_printf ("%d", sim_settings_get_noise_npoints (s));
	child = xmlNewChild (analysis, ctxt->ns, BAD_CAST "npoints", BAD_CAST str);
	g_free (str);

	str = g_strdup_printf ("%g", sim_settings_get_noise_start (s));
	child = xmlNewChild (analysis, ctxt->ns, BAD_CAST "start", BAD_CAST str);
	g_free (str);

	str = g_strdup_printf ("%g", sim_settings_get_noise_stop (s));
	child = xmlNewChild (analysis, ctxt->ns, BAD_CAST "stop", BAD_CAST str);
	g_free (str);

	// Save the options
	list = sim_settings_get_options (s);
	if (list) {
		options = xmlNewChild (sim_settings_node, ctxt->ns, BAD_CAST "options", NULL);
		if (!options) {
			g_warning ("Failed during save of sim engine options.\n");
			return;
		}

		while (list) {
			so = list->data;
			child = xmlNewChild (options, ctxt->ns, BAD_CAST "option", NULL);
			xmlNewChild (child, ctxt->ns, BAD_CAST "name", BAD_CAST so->name);
			xmlNewChild (child, ctxt->ns, BAD_CAST "value", BAD_CAST so->value);
			list = list->next;
		}
	}
}

static void write_xml_property (Property *prop, parseXmlContext *ctxt)
{
	xmlNodePtr node_property;

	// Create a node for the property.
	node_property = xmlNewChild (ctxt->node_props, ctxt->ns, BAD_CAST "property", NULL);
	if (!node_property) {
		g_warning ("Failed during save of property %s.\n", prop->name);
		return;
	}

	// Store the name.
	xmlNewChild (node_property, ctxt->ns, BAD_CAST "name",
	             xmlEncodeEntitiesReentrant (ctxt->doc, BAD_CAST prop->name));

	// Store the value.
	xmlNewChild (node_property, ctxt->ns, BAD_CAST "value",
	             xmlEncodeEntitiesReentrant (ctxt->doc, BAD_CAST prop->value));
}

static void write_xml_label (PartLabel *label, parseXmlContext *ctxt)
{
	xmlNodePtr node_label;
	gchar *str;

	// Create a node for the property.
	node_label = xmlNewChild (ctxt->node_labels, ctxt->ns, BAD_CAST "label", NULL);
	if (!node_label) {
		g_warning ("Failed during save of label %s.\n", label->name);
		return;
	}

	// Store the name.
	xmlNewChild (node_label, ctxt->ns, BAD_CAST "name",
	             xmlEncodeEntitiesReentrant (ctxt->doc, BAD_CAST label->name));

	// Store the value.
	xmlNewChild (node_label, ctxt->ns, BAD_CAST "text",
	             xmlEncodeEntitiesReentrant (ctxt->doc, BAD_CAST label->text));

	str = g_strdup_printf ("(%g %g)", label->pos.x, label->pos.y);
	xmlNewChild (node_label, ctxt->ns, BAD_CAST "position", BAD_CAST str);
	g_free (str);
}

static void write_xml_part (Part *part, parseXmlContext *ctxt)
{
	PartPriv *priv;
	xmlNodePtr node_part;
	gchar *str;
	Coords pos;

	g_return_if_fail (part != NULL);
	g_return_if_fail (IS_PART (part));

	priv = part->priv;

	// Create a node for the part.
	node_part = xmlNewChild (ctxt->node_parts, ctxt->ns, BAD_CAST "part", NULL);
	if (!node_part) {
		g_warning ("Failed during save of part %s.\n", priv->name);
		return;
	}

	str = g_strdup_printf ("%d", part_get_rotation (part));
	xmlNewChild (node_part, ctxt->ns, BAD_CAST "rotation",
	             xmlEncodeEntitiesReentrant (ctxt->doc, BAD_CAST str));
	g_free (str);

	if (priv->flip & ID_FLIP_HORIZ)
		xmlNewChild (node_part, ctxt->ns, BAD_CAST "flip",
		             xmlEncodeEntitiesReentrant (ctxt->doc, BAD_CAST "horizontal"));

	if (priv->flip & ID_FLIP_VERT)
		xmlNewChild (node_part, ctxt->ns, BAD_CAST "flip",
		             xmlEncodeEntitiesReentrant (ctxt->doc, BAD_CAST "vertical"));

	// Store the name.
	xmlNewChild (node_part, ctxt->ns, BAD_CAST "name",
	             xmlEncodeEntitiesReentrant (ctxt->doc, BAD_CAST priv->name));

	// Store the name of the library the part resides in.
	xmlNewChild (node_part, ctxt->ns, BAD_CAST "library",
	             xmlEncodeEntitiesReentrant (ctxt->doc, BAD_CAST priv->library->name));

	// Which symbol to use.
	xmlNewChild (node_part, ctxt->ns, BAD_CAST "symbol",
	             xmlEncodeEntitiesReentrant (ctxt->doc, BAD_CAST priv->symbol_name));

	// Position.
	item_data_get_pos (ITEM_DATA (part), &pos);
	str = g_strdup_printf ("(%g %g)", pos.x, pos.y);
	xmlNewChild (node_part, ctxt->ns, BAD_CAST "position", BAD_CAST str);
	g_free (str);

	// Create a node for the properties.
	ctxt->node_props = xmlNewChild (node_part, ctxt->ns, BAD_CAST "properties", NULL);
	if (!ctxt->node_props) {
		g_warning ("Failed during save of part %s.\n", priv->name);
		return;
	} else {
		g_slist_foreach (priv->properties, (GFunc)write_xml_property, ctxt);
	}

	// Create a node for the labels.
	ctxt->node_labels = xmlNewChild (node_part, ctxt->ns, BAD_CAST "labels", NULL);
	if (!ctxt->node_labels) {
		g_warning ("Failed during save of part %s.\n", priv->name);
		return;
	} else {
		g_slist_foreach (priv->labels, (GFunc)write_xml_label, ctxt);
	}
}

static gint cmp_nodes (gconstpointer a, gconstpointer b)
{
	return coords_compare (&(((const Node *)a)->key), &(((const Node *)b)->key));
}

static void write_xml_wire (Wire *wire, parseXmlContext *ctxt)
{
	xmlNodePtr node_wire;
	gchar *str;
	Coords start_pos, end_pos;

	g_return_if_fail (wire != NULL);
	g_return_if_fail (IS_WIRE (wire));

	// Create a node for the wire.
	node_wire = xmlNewChild (ctxt->node_wires, ctxt->ns, BAD_CAST "wire", NULL);
	if (!node_wire) {
		g_warning ("Failed during save of wire.\n");
		return;
	}

	wire_get_start_pos (wire, &start_pos);
	wire_get_end_pos (wire, &end_pos);

	Node *node;
	Coords last, current, tmp;
	GSList *iter, *copy;

	copy = g_slist_sort (g_slist_copy (wire_get_nodes (wire)), cmp_nodes);
	current = last = start_pos;

	for (iter = copy; iter; iter = iter->next) {
		node = iter->data;
		if (node == NULL) {
			g_warning ("Node of wire did not exist [%p].", node);
			continue;
		}

		tmp = node->key;
		if (coords_equal (&tmp, &start_pos))
			continue;
		if (coords_equal (&tmp, &end_pos))
			continue;

		last = current;
		current = tmp;

		str = g_strdup_printf ("(%g %g)(%g %g)", last.x, last.y, current.x, current.y);

		xmlNewChild (node_wire, ctxt->ns, BAD_CAST "points", BAD_CAST str);
		g_free (str);
	}
	last = current;
	current = end_pos;
	str = g_strdup_printf ("(%g %g)(%g %g)", last.x, last.y, current.x, current.y);

	xmlNewChild (node_wire, ctxt->ns, BAD_CAST "points", BAD_CAST str);
	g_free (str);

	g_slist_free (copy);
}

static void write_xml_textbox (Textbox *textbox, parseXmlContext *ctxt)
{
	xmlNodePtr node_textbox;
	gchar *str;
	Coords pos;

	g_return_if_fail (textbox != NULL);
	if (!IS_TEXTBOX (textbox))
		return;

	// Create a node for the textbox.
	node_textbox = xmlNewChild (ctxt->node_textboxes, ctxt->ns, BAD_CAST "textbox", NULL);
	if (!node_textbox) {
		g_warning ("Failed during save of text box.\n");
		return;
	}

	item_data_get_pos (ITEM_DATA (textbox), &pos);

	str = g_strdup_printf ("(%g %g)", pos.x, pos.y);
	xmlNewChild (node_textbox, ctxt->ns, BAD_CAST "position", BAD_CAST str);
	g_free (str);

	str = textbox_get_text (textbox);
	xmlNewChild (node_textbox, ctxt->ns, BAD_CAST "text", BAD_CAST str);
}

// Create an XML subtree of doc equivalent to the given Schematic.
static xmlNodePtr write_xml_schematic (parseXmlContext *ctxt, Schematic *sm, GError **error)
{
	xmlNodePtr cur;
	xmlNodePtr grid;
	xmlNsPtr ogo;
	gchar *str;

	cur = xmlNewDocNode (ctxt->doc, ctxt->ns, BAD_CAST "schematic", NULL);
	if (cur == NULL) {
		printf ("%s:%d NULL that shall be not NULL!\n", __FILE__, __LINE__);
		return NULL;
	}

	if (ctxt->ns == NULL) {
		ogo = xmlNewNs (cur, BAD_CAST "https://beerbach.me/project/oregano/ns/v1", BAD_CAST "ogo");
		xmlSetNs (cur, ogo);
		ctxt->ns = ogo;
	}

	// General information about the Schematic.
	str = g_strdup_printf ("%s", schematic_get_author (sm));
	xmlNewChild (cur, ctxt->ns, BAD_CAST "author",
	             xmlEncodeEntitiesReentrant (ctxt->doc, BAD_CAST str));
	g_free (str);

	str = g_strdup_printf ("%s", schematic_get_title (sm));
	xmlNewChild (cur, ctxt->ns, BAD_CAST "title",
	             xmlEncodeEntitiesReentrant (ctxt->doc, BAD_CAST str));
	g_free (str);

	str = g_strdup_printf ("%s", schematic_get_version (sm));
	xmlNewChild (cur, ctxt->ns, BAD_CAST "version",
	             xmlEncodeEntitiesReentrant (ctxt->doc, BAD_CAST str));
	g_free (str);

	str = g_strdup_printf ("%s", schematic_get_comments (sm));
	xmlNewChild (cur, ctxt->ns, BAD_CAST "comments",
	             xmlEncodeEntitiesReentrant (ctxt->doc, BAD_CAST str));
	g_free (str);

	// Grid.
	grid = xmlNewChild (cur, ctxt->ns, BAD_CAST "grid", NULL);
	xmlNewChild (grid, ctxt->ns, BAD_CAST "visible", BAD_CAST "true");
	xmlNewChild (grid, ctxt->ns, BAD_CAST "snap", BAD_CAST "true");

	// Simulation settings.
	write_xml_sim_settings (cur, ctxt, sm);

	// Parts.
	ctxt->node_parts = xmlNewChild (cur, ctxt->ns, BAD_CAST "parts", NULL);
	schematic_parts_foreach (sm, (gpointer)write_xml_part, ctxt);

	// Wires.
	ctxt->node_wires = xmlNewChild (cur, ctxt->ns, BAD_CAST "wires", NULL);
	schematic_wires_foreach (sm, (gpointer)write_xml_wire, ctxt);

	// Text boxes.
	ctxt->node_textboxes = xmlNewChild (cur, ctxt->ns, BAD_CAST "textboxes", NULL);
	schematic_items_foreach (sm, (gpointer)write_xml_textbox, ctxt);

	return cur;
}

// schematic_write_xml
//
// Save a Sheet to an XML file.
gboolean schematic_write_xml (Schematic *sm, GError **error)
{
	int ret = -1;
	xmlDocPtr xml;
	parseXmlContext ctxt;
	GError *err = NULL;

	g_return_val_if_fail (sm != NULL, FALSE);

	// Create the tree.
	xml = xmlNewDoc (BAD_CAST "1.0");
	if (xml == NULL) {
		return FALSE;
	}

	ctxt.ns = NULL;
	ctxt.doc = xml;

	xmlDocSetRootElement (xml, write_xml_schematic (&ctxt, sm, &err));

	if (err) {
		g_propagate_error (error, err);
		return FALSE;
	}

	// Dump the tree.
	xmlSetDocCompressMode (xml, oregano.compress_files ? 9 : 0);

	{
		char *s = schematic_get_filename (sm);
		if (s != NULL) {
			ret = xmlSaveFormatFile (s, xml, 1);
		} else {
			g_warning ("Schematic has no filename!!\n");
		}
	}

	if (xml != NULL) {
		xmlFreeDoc (xml);
	} else {
		g_warning ("XML object is NULL\n");
	}

	if (ret < 0)
		return FALSE;

	return TRUE;
}
