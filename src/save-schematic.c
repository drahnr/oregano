/*
 * save-schematic.c
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
#include <math.h>
#include "xml-compat.h"
#include "main.h"
#include "schematic.h"
#include "sheet-private.h"
#include "sheet-pos.h"
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

typedef struct {
	xmlDocPtr  doc;		 /* Xml document. */
	xmlNsPtr   ns;		 /* Main namespace. */

	xmlNodePtr node_symbols; /* For saving of symbols. */
	xmlNodePtr node_parts;	 /* For saving of parts. */
	xmlNodePtr node_props;	 /* For saving of properties. */
	xmlNodePtr node_labels;	 /* For saving of labels. */
	xmlNodePtr node_wires;	 /* For saving of wires. */
	xmlNodePtr node_textboxes;
} parseXmlContext;

static void
write_xml_sim_settings (xmlNodePtr cur, parseXmlContext *ctxt, Schematic *sm)
{
	xmlNodePtr sim_settings_node, child,analysis,options;
	gchar *str;
	SimSettings *s;
	SimOption *so;
	GList *list;

	s = schematic_get_sim_settings (sm);

	sim_settings_node = xmlNewChild (cur, ctxt->ns,
		BAD_CAST"simulation-settings", NULL);
	if (!sim_settings_node){
		g_warning ("Failed during save of simulation settings.\n");
		return;
	}

	/* Transient analysis    */
	analysis = xmlNewChild (sim_settings_node, ctxt->ns, BAD_CAST "transient",
		NULL);
	if (!analysis){
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

	if (sim_settings_get_trans_init_cond(s))
		str = "true";
	else
		str = "false";
	child = xmlNewChild (analysis, ctxt->ns, BAD_CAST "init-conditions", BAD_CAST str);

	/*  AC analysis   */
	analysis =  xmlNewChild (sim_settings_node, ctxt->ns, BAD_CAST "ac", NULL);
	if (!analysis){
		g_warning ("Failed during save of AC analysis settings.\n");
		return;
	}
	child = xmlNewChild (analysis, ctxt->ns, BAD_CAST "enabled",
		BAD_CAST (sim_settings_get_ac (s) ? "true" : "false") );

	str = g_strdup_printf ("%d", sim_settings_get_ac_npoints (s));
	child = xmlNewChild (analysis, ctxt->ns, BAD_CAST "npoints", BAD_CAST str);
	g_free (str);

	str = g_strdup_printf ("%g", sim_settings_get_ac_start (s));
	child = xmlNewChild (analysis, ctxt->ns, BAD_CAST "start", BAD_CAST str);
	g_free (str);

	str = g_strdup_printf ("%g", sim_settings_get_ac_stop (s));
	child = xmlNewChild (analysis, ctxt->ns, BAD_CAST "stop", BAD_CAST str);
	g_free (str);

	/*  DC analysis   */
	analysis =  xmlNewChild (sim_settings_node, ctxt->ns, BAD_CAST "dc-sweep",
		NULL);
	if (!analysis){
		g_warning ("Failed during save of DC sweep analysis settings.\n");
		return;
	}
	child = xmlNewChild (analysis, ctxt->ns, BAD_CAST "enabled",
		BAD_CAST (sim_settings_get_dc (s) ? "true" : "false") );

	child = xmlNewChild (analysis, ctxt->ns, BAD_CAST "vsrc1",
		BAD_CAST sim_settings_get_dc_vsrc(s,0));

	str = g_strdup_printf ("%g", sim_settings_get_dc_start (s,0));
	child = xmlNewChild (analysis, ctxt->ns, BAD_CAST "start1", BAD_CAST str);
	g_free (str);

	str = g_strdup_printf ("%g", sim_settings_get_dc_stop (s,0));
	child = xmlNewChild (analysis, ctxt->ns, BAD_CAST "stop1", BAD_CAST str);
	g_free (str);

	str = g_strdup_printf ("%g", sim_settings_get_dc_step (s,0));
	child = xmlNewChild (analysis, ctxt->ns, BAD_CAST "step1", BAD_CAST str);
	g_free (str);

	child = xmlNewChild (analysis, ctxt->ns, BAD_CAST "vsrc2",
		BAD_CAST sim_settings_get_dc_vsrc(s,1));

	str = g_strdup_printf ("%g", sim_settings_get_dc_start (s,1));
	child = xmlNewChild (analysis, ctxt->ns, BAD_CAST "start2", BAD_CAST str);
	g_free (str);

	str = g_strdup_printf ("%g", sim_settings_get_dc_stop (s,1));
	child = xmlNewChild (analysis, ctxt->ns, BAD_CAST "stop2", BAD_CAST str);
	g_free (str);

	str = g_strdup_printf ("%g", sim_settings_get_dc_step (s,1));
	child = xmlNewChild (analysis, ctxt->ns, BAD_CAST "step2", BAD_CAST str);
	g_free (str);


	/* Save the options */
	list = sim_settings_get_options (s);
	if ( list ) {
		options =  xmlNewChild (sim_settings_node, ctxt->ns, BAD_CAST "options",
			NULL);
		if (!options){
			g_warning ("Failed during save of sim engine options.\n");
			return;
		}

		while ( list ) {
			so  = list->data;
			child=xmlNewChild (options, ctxt->ns, BAD_CAST "option", NULL);
			xmlNewChild (child  , ctxt->ns, BAD_CAST "name", BAD_CAST so->name);
			xmlNewChild (child  , ctxt->ns, BAD_CAST "value", BAD_CAST so->value);
			list = list->next;
		}
	}
}

static void
write_xml_property (Property *prop, parseXmlContext *ctxt)
{
	xmlNodePtr node_property;

	/*
	 * Create a node for the property.
	 */
	node_property = xmlNewChild (ctxt->node_props, ctxt->ns, BAD_CAST "property",
		NULL);
	if (!node_property){
		g_warning ("Failed during save of property %s.\n", prop->name);
		return;
	}

	/*
	 * Store the name.
	 */
	xmlNewChild (node_property, ctxt->ns, BAD_CAST "name",
		xmlEncodeEntitiesReentrant (ctxt->doc, BAD_CAST prop->name));

	/*
	 * Store the value.
	 */
	xmlNewChild (node_property, ctxt->ns, BAD_CAST "value",
		xmlEncodeEntitiesReentrant (ctxt->doc, BAD_CAST prop->value));
}

static void
write_xml_label (PartLabel *label, parseXmlContext *ctxt)
{
	xmlNodePtr node_label;
	gchar *str;

	/*
	 * Create a node for the property.
	 */
	node_label = xmlNewChild (ctxt->node_labels, ctxt->ns, BAD_CAST "label", NULL);
	if (!node_label){
		g_warning ("Failed during save of label %s.\n", label->name);
		return;
	}

	/*
	 * Store the name.
	 */
	xmlNewChild (node_label, ctxt->ns, BAD_CAST "name",
		xmlEncodeEntitiesReentrant (ctxt->doc, BAD_CAST label->name));

	/*
	 * Store the value.
	 */
	xmlNewChild (node_label, ctxt->ns, BAD_CAST "text",
		xmlEncodeEntitiesReentrant (ctxt->doc, BAD_CAST label->text));

	str = g_strdup_printf ("(%g %g)",  label->pos.x, label->pos.y);
	xmlNewChild (node_label, ctxt->ns, BAD_CAST "position", BAD_CAST str);
	g_free (str);

	/*
	 * Editable or not?
	 */
/*	xmlNewChild (node_property, ctxt->ns, "editable", prop->edit ? "true" : "false");*/
}

static void
write_xml_part (Part *part, parseXmlContext *ctxt)
{
	PartPriv *priv;
	xmlNodePtr node_part;
	xmlNodePtr node_pos;
	gchar *str;
	SheetPos pos;

	priv = part->priv;

	/*
	 * Create a node for the part.
	 */
	node_part = xmlNewChild (ctxt->node_parts, ctxt->ns, BAD_CAST "part", NULL);
	if (!node_part){
		g_warning ("Failed during save of part %s.\n", priv->name);
		return;
	}

	str = g_strdup_printf ("%d", priv->rotation);
	xmlNewChild (node_part, ctxt->ns, BAD_CAST "rotation",
		xmlEncodeEntitiesReentrant (ctxt->doc, BAD_CAST str));
	g_free (str);

	if (priv->flip & ID_FLIP_HORIZ)
		xmlNewChild (node_part, ctxt->ns, BAD_CAST "flip",
			xmlEncodeEntitiesReentrant (ctxt->doc, BAD_CAST "horizontal"));

	if (priv->flip & ID_FLIP_VERT)
		xmlNewChild (node_part, ctxt->ns, BAD_CAST "flip",
			xmlEncodeEntitiesReentrant (ctxt->doc, BAD_CAST "vertical"));

	/*
	 * Store the name.
	 */
	xmlNewChild (node_part, ctxt->ns, BAD_CAST "name",
		xmlEncodeEntitiesReentrant (ctxt->doc, BAD_CAST priv->name));

	/*
	 * Store the name of the library the part resides in.
	 */
	xmlNewChild (node_part, ctxt->ns, BAD_CAST "library",
		xmlEncodeEntitiesReentrant (ctxt->doc, BAD_CAST priv->library->name));

	/*
	 * Which symbol to use.
	 */
	xmlNewChild (node_part, ctxt->ns, BAD_CAST "symbol",
		xmlEncodeEntitiesReentrant (ctxt->doc, BAD_CAST priv->symbol_name));

	/*
	 * Position.
	 */
	item_data_get_pos (ITEM_DATA (part), &pos);
	str = g_strdup_printf ("(%g %g)", pos.x, pos.y);
	node_pos = xmlNewChild (node_part, ctxt->ns, BAD_CAST "position", BAD_CAST str);
	g_free (str);

	/*
	 * Create a node for the properties.
	 */
	ctxt->node_props = xmlNewChild (node_part, ctxt->ns, BAD_CAST "properties",
		NULL);
	if (!ctxt->node_props){
		g_warning ("Failed during save of part %s.\n", priv->name);
		return;
	}
	else{
		g_slist_foreach (priv->properties, (GFunc) write_xml_property,
			ctxt);
	}

	/*
	 * Create a node for the labels.
	 */
	ctxt->node_labels = xmlNewChild (node_part, ctxt->ns, BAD_CAST "labels", NULL);
	if (!ctxt->node_labels){
		g_warning ("Failed during save of part %s.\n", priv->name);
		return;
	}
	else{
		g_slist_foreach (priv->labels, (GFunc) write_xml_label, ctxt);
	}
}

static void
write_xml_wire (Wire *wire, parseXmlContext *ctxt)
{
	xmlNodePtr node_wire;
	gchar *str;
	SheetPos start_pos, end_pos;

	g_return_if_fail (wire != NULL);
	g_return_if_fail (IS_WIRE (wire));

	/*
	 * Create a node for the wire.
	 */
	node_wire = xmlNewChild (ctxt->node_wires, ctxt->ns, BAD_CAST "wire", NULL);
	if (!node_wire){
		g_warning ("Failed during save of wire.\n");
		return;
	}

	wire_get_start_pos (wire, &start_pos);
	wire_get_end_pos (wire, &end_pos);

	str = g_strdup_printf ("(%g %g)(%g %g)",
		start_pos.x, start_pos.y, end_pos.x, end_pos.y);
	xmlNewChild (node_wire, ctxt->ns, BAD_CAST "points", BAD_CAST str);
	g_free (str);
}

static void
write_xml_textbox (Textbox *textbox, parseXmlContext *ctxt)
{
	xmlNodePtr node_textbox;
	gchar *str;
	SheetPos pos;

	g_return_if_fail (textbox != NULL);

	/*
	 * FIXME: just a quick hack to get this working.
	 */
	if (!IS_TEXTBOX (textbox))
		return;

	g_return_if_fail (IS_TEXTBOX (textbox));

	/*
	 * Create a node for the textbox.
	 */
	node_textbox = xmlNewChild (ctxt->node_textboxes, ctxt->ns,
		BAD_CAST "textbox", NULL);
	if (!node_textbox){
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

/*
 * Create an XML subtree of doc equivalent to the given Schematic.
 */
static xmlNodePtr
write_xml_schematic (parseXmlContext *ctxt, Schematic *sm, GError **error)
{
	xmlNodePtr cur;
	xmlNodePtr grid;
	xmlNsPtr ogo;
	gchar *str;
	/*
	 * Unused variables
	double zoom;
	*/

	cur = xmlNewDocNode (ctxt->doc, ctxt->ns, BAD_CAST "schematic", NULL);
	if (cur == NULL) {
		printf("%s:%d NULL que no debe ser NULL!\n", __FILE__,
			__LINE__);
		return NULL;
	}

	if (ctxt->ns == NULL) {
		ogo = xmlNewNs (cur,
			BAD_CAST "http://www.dtek.chalmers.se/~d4hult/oregano/v1",
			BAD_CAST "ogo");
		xmlSetNs (cur,ogo);
		ctxt->ns = ogo;
	}

	/*
	 * General information about the Schematic.
	 */
	str = g_strdup_printf ("%s", schematic_get_author (sm));
	xmlNewChild (cur, ctxt->ns, BAD_CAST "author", xmlEncodeEntitiesReentrant (ctxt->doc, BAD_CAST str));
	g_free (str);

	str = g_strdup_printf ("%s", schematic_get_title (sm));
	xmlNewChild (cur, ctxt->ns, BAD_CAST "title", xmlEncodeEntitiesReentrant (ctxt->doc, BAD_CAST str));
	g_free (str);

	str = g_strdup_printf ("%s", schematic_get_comments (sm));
	xmlNewChild (cur, ctxt->ns, BAD_CAST "comments", xmlEncodeEntitiesReentrant (ctxt->doc, BAD_CAST str));
	g_free (str);

	/*
	 * Zoom.
	 */
//	sheet_get_zoom (sm->sheet, &zoom);
//	str = g_strdup_printf ("%g", zoom);
//	xmlNewChild (cur, ctxt->ns, "zoom", str);
//	g_free (str);

	/*
	 * Grid.
	 */
	grid = xmlNewChild (cur, ctxt->ns, BAD_CAST "grid", NULL);
	xmlNewChild (grid, ctxt->ns, BAD_CAST "visible", BAD_CAST "true");
	xmlNewChild (grid, ctxt->ns, BAD_CAST "snap", BAD_CAST "true");
	/*
	 * Simulation settings.
	 */
	write_xml_sim_settings (cur, ctxt, sm);

	/*
	 * Parts.
	 */
	ctxt->node_parts = xmlNewChild (cur, ctxt->ns, BAD_CAST "parts", NULL);
	schematic_parts_foreach (sm, (gpointer) write_xml_part, ctxt);

	/*
	 * Wires.
	 */
	ctxt->node_wires = xmlNewChild (cur, ctxt->ns, BAD_CAST "wires", NULL);
	schematic_wires_foreach (sm, (gpointer) write_xml_wire, ctxt);

	/*
	 * Text boxes.
	 */
	ctxt->node_textboxes = xmlNewChild (cur, ctxt->ns, BAD_CAST "textboxes", NULL);
	schematic_items_foreach (sm, (gpointer) write_xml_textbox, ctxt);

	return cur;
}

/*
 * schematic_write_xml
 *
 * Save a Sheet to an XML file.
 */

gint
schematic_write_xml (Schematic *sm, GError **error)
{
	int ret = -1;
	xmlDocPtr xml;
	parseXmlContext ctxt;
	GError *internal_error = NULL;

	g_return_val_if_fail (sm != NULL, FALSE);

	/*
	 * Create the tree.
	 */
	xml = xmlNewDoc (BAD_CAST "1.0");
	if (xml == NULL){
		return FALSE;
	}

	ctxt.ns = NULL;
	ctxt.doc = xml;

	xmlDocSetRootElement(xml, write_xml_schematic(&ctxt, sm, &internal_error));

	if (internal_error) {
		g_propagate_error (error, internal_error);
		return FALSE;
	}

	/*
	 * Dump the tree.
	 */
	xmlSetDocCompressMode (xml, oregano.compress_files ? 9 : 0);

	/* FIXME: check for != NULL. */
	{
		char *s =schematic_get_filename (sm);
		if (s != NULL) {
			xmlIndentTreeOutput;
			ret = xmlSaveFormatFile (s, xml, 1);
		} else {
			g_warning("Schematic has no filename!!\n");
		}
	}

	if (xml != NULL) {
		xmlFreeDoc (xml);
	} else {
		g_warning("XML object is NULL\n");
	}

	if (ret < 0)
		return FALSE;
	return TRUE;
}


