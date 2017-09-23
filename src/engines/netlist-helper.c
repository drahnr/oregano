/*
 * netlist-helper.c
 *
 *
 * Author:
 *  Richard Hult <rhult@hem.passagen.se>
 *  Ricardo Markiewicz <rmarkie@fi.uba.ar>
 *  Andres de Barbara <adebarbara@fi.uba.ar>
 *  Marc Lorber <Lorber.Marc@wanadoo.fr>
 *  Bernhard Schuster <bernhard@ahoi.io>
 *  Guido Trentalancia <guido@trentalancia.com>
 *
 * Web page: https://ahoi.io/project/oregano
 *
 * Copyright (C) 1999-2001  Richard Hult
 * Copyright (C) 2003,2006  Ricardo Markiewicz
 * Copyright (C) 2009-2012  Marc Lorber
 * Copyright (C) 2014       Bernhard Schuster
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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include "oregano.h"
#include "schematic.h"
#include "node-store.h"
#include "node.h"
#include "wire.h"
#include "part-private.h"
#include "part-property.h"
#include "netlist-helper.h"
#include "errors.h"
#include "dialogs.h"

#include "debug.h"

static void netlist_helper_node_foreach_reset (gpointer key, gpointer value, gpointer user_data);
static void netlist_helper_wire_traverse (Wire *wire, NetlistData *data);
static void netlist_helper_node_traverse (Node *node, NetlistData *data);
static void netlist_helper_node_foreach_traverse (gpointer key, gpointer value, NetlistData *data);
static gboolean netlist_helper_foreach_model_free (gpointer key, gpointer model,
                                                   gpointer user_data);
static gboolean netlist_helper_foreach_model_save (gpointer key, gpointer model,
                                                   gpointer user_data);
static char *netlist_helper_linebreak (char *str);
static void netlist_helper_nl_node_traverse (Node *node, GSList **lst);

static void netlist_helper_nl_wire_traverse (Wire *wire, GSList **lst)
{
	GSList *iter;
	GSList *iter2;

	g_return_if_fail (wire != NULL);
	g_return_if_fail (IS_WIRE (wire));

	if (wire_is_visited (wire))
		return;

	wire_set_visited (wire, TRUE);

	for (iter = wire_get_nodes (wire); iter; iter = iter->next) {
		Node *node = iter->data;

		for (iter2 = node->pins; iter2; iter2 = iter2->next) {
			Pin *pin = iter2->data;
			Part *part = PART (pin->part);

			char *template, *tmp;
			char **template_split;

			tmp = part_get_property (part, "template");

			if (!tmp)
				continue;

			template = part_property_expand_macros (part, tmp);

			template_split = g_strsplit (template, " ", 0);

			if (template_split[0] != NULL)
				(*lst) = g_slist_prepend (*lst, g_strdup (template_split[0]));

			g_strfreev (template_split);
			g_free (tmp);
			g_free (template);
		}

		netlist_helper_nl_node_traverse (node, lst);
	}
}

static void netlist_helper_nl_node_traverse (Node *node, GSList **lst)
{
	GSList *iter;

	g_return_if_fail (node != NULL);
	g_return_if_fail (IS_NODE (node));

	if (node_is_visited (node))
		return;

	node_set_visited (node, TRUE);

	for (iter = node->wires; iter; iter = iter->next) {
		Wire *wire = iter->data;
		netlist_helper_nl_wire_traverse (wire, lst);
	}
}

/**
 * traverses the netlist and picks up all clamp type parts
 *
 * TODO this is part of the "logic" that determines which "nodes" (or V levels)
 * TODO will be plotted (or: not discarded)
 *
 * @returns a list of parts whose type is clamp
 */
static GSList *netlist_helper_get_clamp_parts (NodeStore *store, Node *node)
{
	GList *iter;
	GSList *iter2;
	Wire *wire;
	GSList *ret = NULL;

	if (!node)
		return NULL;

	node_store_node_foreach (store, (GHFunc *)netlist_helper_node_foreach_reset, NULL);

	for (iter = store->wires; iter; iter = iter->next) {
		wire = iter->data;
		wire_set_visited (wire, FALSE);
	}

	for (iter2 = node->wires; iter2; iter2 = iter2->next) {
		wire = iter2->data;
		netlist_helper_nl_wire_traverse (wire, &ret);
	}

	return ret;
}

void netlist_helper_init_data (NetlistData *data)
{
	data->pins = g_hash_table_new (g_direct_hash, g_direct_equal);
	data->models = g_hash_table_new (g_str_hash, g_str_equal);
	data->node_nr = 1;
	data->gnd_list = NULL;
	data->clamp_list = NULL;
	data->mark_list = NULL;
	data->node_and_number_list = NULL;
}

void netlist_helper_node_foreach_reset (gpointer key, gpointer value, gpointer user_data)
{
	Node *node = value;

	node_set_visited (node, FALSE);
}

void netlist_helper_wire_traverse (Wire *wire, NetlistData *data)
{
	GSList *iter;

	g_return_if_fail (wire != NULL);
	g_return_if_fail (IS_WIRE (wire));

	if (wire_is_visited (wire))
		return;

	wire_set_visited (wire, TRUE);

	for (iter = wire_get_nodes (wire); iter; iter = iter->next) {
		Node *node = iter->data;

		netlist_helper_node_traverse (node, data);
	}
}

void netlist_helper_node_traverse (Node *node, NetlistData *data)
{
	GSList *iter;
	gchar *prop;
	NodeAndNumber *nan;

	g_return_if_fail (node != NULL);
	g_return_if_fail (IS_NODE (node));
	g_return_if_fail (data != NULL);

	if (node_is_visited (node))
		return;

	node_set_visited (node, TRUE);

	// Keep track of netlist nr <---> Node.
	nan = g_new0 (NodeAndNumber, 1);
	nan->node_nr = data->node_nr;
	nan->node = node;
	data->node_and_number_list = g_list_prepend (data->node_and_number_list, nan);

	// Traverse the pins at this node.
	for (iter = node->pins; iter; iter = iter->next) {
		Pin *pin = iter->data;

		// First see if the pin belongs to an "internal", special part.
		prop = part_get_property (pin->part, "internal");
		if (prop) {
			if (!g_ascii_strcasecmp (prop, "marker")) {
				Marker *marker;
				gchar *name, *value;

				name = part_get_property (pin->part, "name");

				if (!name) {
					g_free (prop);
					continue;
				}

				value = part_property_expand_macros (pin->part, name);
				g_free (name);

				if (!value)
					continue;

				marker = g_new0 (Marker, 1);
				marker->node_nr = data->node_nr;
				marker->name = value;
				data->mark_list = g_slist_prepend (data->mark_list, marker);

			} else if (g_ascii_strcasecmp (prop, "ground") == 0) {
				data->gnd_list = g_slist_prepend (data->gnd_list, GINT_TO_POINTER (data->node_nr));

			} else if (g_ascii_strcasecmp (prop, "clamp") == 0) {
				data->clamp_list =
				    g_slist_prepend (data->clamp_list, GINT_TO_POINTER (data->node_nr));
			}
			g_free (prop);
		}

		// Keep track of models to include. Needs to be freed when the
		// hash table is no longer needed.
		prop = part_get_property (pin->part, "model");
		if (prop) {
			if (!g_hash_table_lookup (data->models, prop))
				g_hash_table_insert (data->models, prop, NULL);
		}

		g_hash_table_insert (data->pins, pin, GINT_TO_POINTER (data->node_nr));
	}

	// Traverse the wires at this node.
	for (iter = node->wires; iter; iter = iter->next) {
		Wire *wire = iter->data;
		netlist_helper_wire_traverse (wire, data);
	}
}

void netlist_helper_node_foreach_traverse (gpointer key, gpointer value, NetlistData *data)
{
	Node *node = value;

	g_return_if_fail (data != NULL);

	// Only visit nodes that are not already visited.
	if (node_is_visited (node))
		return;

	netlist_helper_node_traverse (node, data);

	data->node_nr++;
}

gint compare_marker (gconstpointer a, gconstpointer b)
{
	const Marker *ma;
	gint node_nr;

	ma = a;
	node_nr = GPOINTER_TO_INT (b);

	if (ma->node_nr == node_nr)
		return 0;
	else
		return 1;
}

gboolean netlist_helper_foreach_model_save (gpointer key, gpointer model, gpointer user_data)
{
	GList **l = user_data;

	(*l) = g_list_prepend (*l, g_strdup ((gchar *)key));

	return TRUE;
}

gboolean netlist_helper_foreach_model_free (gpointer key, gpointer model, gpointer user_data)
{
	g_free (key);

	return FALSE;
}

char *netlist_helper_linebreak (char *str)
{
	char **split, *tmp;
	GString *out;
	int i;

	split = g_strsplit (str, "\\", 0);

	out = g_string_new ("");

	i = 0;
	while (split[i] != NULL) {
		if (split[i][0] == 'n') {
			if (strlen (split[i]) > 1) {
				out = g_string_append_c (out, '\n');
				out = g_string_append (out, split[i] + 1);
			}
		} else {
			out = g_string_append (out, split[i]);
		}

		i++;
	}

	g_strfreev (split);
	tmp = out->str;
	g_string_free (out, FALSE); // Don't free the string data.
	return tmp;
}

void update_schematic(Schematic *sm) {
	char *version = schematic_get_version(sm);
	if (version == NULL) {
		NodeStore *store = schematic_get_store(sm);

		for (GList *iter = store->parts; iter; iter = iter->next) {
			int node_ctr = 1;
			Part *part = iter->data;
			update_connection_designators(part, part_get_property_ref(part, "template"), &node_ctr);
		}
	}
	schematic_set_version(sm, VERSION);

	return;
}

// FIXME this one piece of ugly+bad code
void netlist_helper_create (Schematic *sm, Netlist *out, GError **error)
{
	NetlistData data;
	GList *iter;
	Part *part;
	gint pin_nr, num_nodes, num_gnd_nodes, i, j, num_clamps;
	Pin *pins;
	gchar *template, **template_split;
	NodeStore *store;
	gchar **node2real;

	out->models = NULL;
	out->title = schematic_get_filename (sm);
	out->settings = schematic_get_sim_settings (sm);
	store = schematic_get_store (sm);
	out->store = store;

	node_store_node_foreach (store, (GHFunc *)netlist_helper_node_foreach_reset, NULL);

	for (iter = store->wires; iter; iter = iter->next) {
		Wire *wire = iter->data;
		wire_set_visited (wire, FALSE);
	}

	netlist_helper_init_data (&data);
	data.store = store;

	node_store_node_foreach (store, (GHFunc *)netlist_helper_node_foreach_traverse, &data);
	num_gnd_nodes = g_slist_length (data.gnd_list);
	num_clamps = g_slist_length (data.clamp_list);

	// Check if there is a Ground node
	if (num_gnd_nodes == 0) {
		g_set_error (error, OREGANO_ERROR, OREGANO_SIMULATE_ERROR_NO_GND,
		             _ ("At least one GND is required. Add at least one and try again."));
	}

	else if (num_clamps == 0) {
		// FIXME put a V/I clamp on each and every subtree
		// FIXME and let the user toggle visibility in the plot window
		// FIXME see also TODO/FIXME above
		g_set_error (error, OREGANO_ERROR, OREGANO_SIMULATE_ERROR_NO_CLAMP,
		             _ ("No test clamps found. Add at least one and try again."));
	}

	else {

		num_nodes = data.node_nr - 1;

		// Build an array for going from node nr to "real node nr",
		// where gnd nodes are nr 0 and the rest of the nodes are
		// 1, 2, 3, ...
		node2real = g_new0 (gchar *, num_nodes + 1 + 1);
		node2real[num_nodes + 1] = NULL; // so we can use g_strfreev

		for (i = 1, j = 1; i <= num_nodes; i++) {
			GSList *mlist;

			if (g_slist_find (data.gnd_list, GINT_TO_POINTER (i))) {
				node2real[i] = g_strdup ("0");
			} else if ((mlist = g_slist_find_custom (data.mark_list, GINT_TO_POINTER (i),
			                                         compare_marker))) {
				Marker *marker = mlist->data;
				node2real[i] = g_strdup (marker->name);
			} else {
				node2real[i] = g_strdup_printf ("%d", j++);
			}
		}

		// Fill in the netlist node names for all the used nodes.
		for (iter = data.node_and_number_list; iter; iter = iter->next) {
			NodeAndNumber *nan = iter->data;
			if (nan->node_nr > 0) {
				g_free (nan->node->netlist_node_name);
				nan->node->netlist_node_name = g_strdup (node2real[nan->node_nr]);
			}
		}

		// Initialize out->template
		out->template = g_string_new ("");
		for (iter = store->parts; iter; iter = iter->next) {
			part = iter->data;

			gchar *tmp, *internal;
			GString *str;

			internal = part_get_property (part, "internal");
			if (internal != NULL) {
				gint node_nr;
				Pin *pins;
				if (g_ascii_strcasecmp (internal, "clamp") != 0) {
					g_free (internal);
					continue;
				}

				// Got a clamp!, set node number
				pins = part_get_pins (part);
				node_nr = GPOINTER_TO_INT (g_hash_table_lookup (data.pins, &pins[0]));
				if (!node_nr) {
					g_warning ("Couldn't find part, pin_nr %d.", 0);
				} else {
					// need to substrac 1, netlist starts in 0, and node_nr in 1
					pins[0].node_nr = atoi (node2real[node_nr]);
				}
				g_free (internal);
				continue;
			}

			tmp = part_get_property (part, "template");
			if (!tmp) {
				continue;
			}

			template = part_property_expand_macros (part, tmp);
			NG_DEBUG ("Template: '%s'\n"
			          "macro   : '%s'\n",
			          tmp, template);

			g_free (tmp);
			tmp = netlist_helper_linebreak (template);

			g_free (template);
			template = tmp;

			pins = part_get_pins (part);




			GRegex *regex;
			GMatchInfo *match_info;
			GError *error = NULL;

			regex = g_regex_new("%\\d*", 0, 0, NULL);
			g_regex_match_full(regex, template, -1, 0, 0, &match_info, &error);
			template_split = g_regex_split(regex, template, 0);

			str = g_string_new ("");

			NG_DEBUG ("Reading pins.\n)");

			int i;
			for (i = 0; g_match_info_matches(match_info); i++) {
				g_string_append (str, template_split[i]);

				gchar *word = g_match_info_fetch(match_info, 0);


				pin_nr = g_ascii_strtoll(word + 1, NULL, 10) - 1;
				gint node_nr = 0;
				node_nr = GPOINTER_TO_INT (g_hash_table_lookup (data.pins, &pins[pin_nr]));
				if (!node_nr) {
					g_set_error (&error, OREGANO_ERROR, OREGANO_SIMULATE_ERROR_NO_SUCH_PART,
					             _ ("Could not find part in library, pin #%d."), pin_nr);
					return; // FIXME wtf?? this leaks like hell and did for ages!
				} else {
					gchar *tmp;
					tmp = node2real[node_nr];

					// need to substrac 1, netlist starts in 0, and node_nr in 1
					pins[pin_nr].node_nr = atoi (node2real[node_nr]);
					g_string_append (str, tmp);
					NG_DEBUG ("str: %s\n", str->str);
				}

				g_free(word);
				g_match_info_next(match_info, NULL);
			}
			g_match_info_free(match_info);
			g_free (template);
			template = NULL;
			g_regex_unref(regex);
			if (template_split[i] != NULL) {
				g_string_append (str, template_split[i]);
			}
			g_strfreev (template_split);
			if (error != NULL) {
				g_printerr("Error while matching: %s\n", error->message);
				g_error_free(error);
			}

			NG_DEBUG ("Done with pins, i = %d\n", i);

			NG_DEBUG ("str: %s\n", str->str);
			out->template = g_string_append (out->template, str->str);
			out->template = g_string_append_c (out->template, '\n');
			g_string_free (str, TRUE);
		}

		g_strfreev (node2real);

		g_hash_table_foreach (data.models, (GHFunc)netlist_helper_foreach_model_save, &out->models);
		return;
	}

	g_hash_table_foreach (data.models, (GHFunc)netlist_helper_foreach_model_free, NULL);
	g_hash_table_destroy (data.models);
	g_hash_table_destroy (data.pins);
	g_list_free_full (data.node_and_number_list, (GDestroyNotify)g_free);
}

char *netlist_helper_create_analysis_string (NodeStore *store, gboolean do_ac)
{
	GList *iter;
	Part *part;
	gchar *prop;
	GString *out;
	gchar *ret;

	out = g_string_new ("");

	for (iter = node_store_get_parts (store); iter; iter = iter->next) {
		part = iter->data;
		prop = part_get_property (part, "internal");
		if (prop) {
			if (!g_ascii_strcasecmp (prop, "clamp")) {
				Pin *pins = part_get_pins (part);

				prop = part_get_property (part, "type");

				if (!g_ascii_strcasecmp (prop, "v")) {
					if (!do_ac) {
						g_string_append_printf (out, " %s(%d)", prop, pins[0].node_nr);
					} else {
						gchar *ac_type, *ac_db;
						ac_type = part_get_property (part, "ac_type");
						ac_db = part_get_property (part, "ac_db");

						if (!g_ascii_strcasecmp (ac_db, "true"))
							g_string_append_printf (out, " %s%sdb(%d)", prop, ac_type,
							                        pins[0].node_nr);
						else
							g_string_append_printf (out, " %s%s(%d)", prop, ac_type,
							                        pins[0].node_nr);
					}
				} else {
					Node *node;
					Coords lookup_key;
					Coords part_pos;

					item_data_get_pos (ITEM_DATA (part), &part_pos);

					lookup_key.x = part_pos.x + pins[0].offset.x;
					lookup_key.y = part_pos.y + pins[0].offset.y;

					node = node_store_get_or_create_node (store, lookup_key);

					if (node) {
						GSList *lst, *iter;
						iter = lst = netlist_helper_get_clamp_parts (store, node);

						for (; iter; iter = iter->next) {
							g_string_append_printf (out, " i(%s)", (char *)iter->data);
						}
						g_slist_free_full (lst, g_free); // need to free this, we are owning it!
					}
				}
			}
			g_free (prop);
		}
	}

	ret = out->str;
	g_string_free (out, FALSE);
	return ret;
}

GSList *netlist_helper_get_voltmeters_list (Schematic *sm, GError **error, gboolean with_type)
{
	Netlist netlist_data;
	GError *e = NULL;
	GSList *clamp_list = NULL;
	NodeStore *node_store;
	GList *iter;
	gchar *prop;
	Part *part;
	Pin *pins;
	gint i;
	gchar *ac_type, *ac_db, *tmp;

	netlist_helper_create (sm, &netlist_data, &e);
	if (e) {
		g_propagate_error (error, e);
		return NULL;
	}

	node_store = netlist_data.store;

	for (iter = node_store_get_parts (node_store); iter; iter = iter->next) {
		part = iter->data;
		prop = part_get_property (part, "internal");
		if (prop) {
			if (!g_ascii_strcasecmp (prop, "clamp")) {
				g_free (prop);
				prop = part_get_property (part, "type");

				if (!g_ascii_strcasecmp (prop, "v")) {
					pins = part_get_pins (part);
					if (with_type) {
						ac_type = part_get_property(part, "ac_type");
						ac_db = part_get_property(part, "ac_db");
						i = 0;
						while (ac_type[i]) {
							ac_type[i] = g_ascii_toupper(ac_type[i]);
							i++;
						}
						if (!g_strcmp0(ac_db, "true"))
							tmp = g_strdup_printf ("VDB(%d)", pins[0].node_nr);
						else
							tmp = g_strdup_printf ("V%s(%d)", ac_type, pins[0].node_nr);
						g_free(ac_type);
						g_free(ac_db);
					} else {
						tmp = g_strdup_printf ("%d", pins[0].node_nr);
					}
					clamp_list = g_slist_prepend (clamp_list, tmp);
					if (0)
						printf ("clamp_list = %s\n", tmp);
				}
			}
			g_free(prop);
		}
	}
	if (0) {
		clamp_list = g_slist_prepend (clamp_list, "3");
		clamp_list = g_slist_prepend (clamp_list, "2");
		clamp_list = g_slist_prepend (clamp_list, "1");
	}
	if (0) {
		GSList *slist = NULL;
		gchar *text = NULL;

		slist = g_slist_copy (clamp_list);
		text = g_strdup_printf ("V(%d)", atoi (slist->data));
		slist = slist->next;
		while (slist) {
			text = g_strdup_printf ("%s V(%d)", text, atoi (slist->data));
			slist = slist->next;
		}
		if (0)
			printf ("clamp_list = %s\n", text);
	}

	return clamp_list;
}

GSList *netlist_helper_get_voltage_sources_list (Schematic *sm, GError **error, gboolean ac_only)
{
	Netlist netlist_data;
	GError *e = NULL;
	GSList *sources_list = NULL;
	NodeStore *node_store;
	GList *iter;
	gchar *prop;
	Part *part;

	netlist_helper_create (sm, &netlist_data, &e);
	if (e) {
		g_propagate_error (error, e);
		return NULL;
	}

	node_store = netlist_data.store;

	for (iter = node_store_get_parts (node_store); iter; iter = iter->next) {
		part = iter->data;
		prop = part_get_property (part, "Refdes");
		if (prop) {
			if (prop[0] == 'V' && (prop[1] >= '0' && prop[1] <= '9')) {
				gchar *tmp;
				tmp = g_strdup (&prop[1]);
				if (ac_only) {
					g_free(prop);
					prop = part_get_property (part, "Frequency");
					if (prop)
						sources_list = g_slist_append (sources_list, tmp);
				} else {
					sources_list = g_slist_append (sources_list, tmp);
				}
				if (0)
					printf ("sources_list = %s\n", tmp);
			}
			g_free(prop);
		}
	}

	return sources_list;
}
