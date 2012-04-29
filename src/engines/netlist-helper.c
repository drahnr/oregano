/*
 * netlist-helper.c
 *
 *
 * Author:
 *  Richard Hult <rhult@hem.passagen.se>
 *  Ricardo Markiewicz <rmarkie@fi.uba.ar>
 *  Andres de Barbara <adebarbara@fi.uba.ar>
 *  Marc Lorber <Lorber.Marc@wanadoo.fr>
 *
 * Web page: https://github.com/marc-lorber/oregano
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
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
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

#define NG_DEBUG(s) if (0) g_print ("NG: %s\n", s)

static void 	netlist_helper_node_foreach_reset (gpointer key, gpointer value, 
    				gpointer user_data);
static void 	netlist_helper_wire_traverse (Wire *wire, NetlistData *data);
static void 	netlist_helper_node_traverse (Node *node, NetlistData *data);
static void 	netlist_helper_node_foreach_traverse (gpointer key, 
    				gpointer value, NetlistData *data);
static gboolean netlist_helper_foreach_model_free (gpointer key, gpointer model, 
    				gpointer user_data);
static gboolean netlist_helper_foreach_model_save (gpointer key, gpointer model, 
    					gpointer user_data);
static char *	netlist_helper_linebreak (char *str);
static void 	netlist_helper_nl_node_traverse (Node *node, GSList **lst);

static void
netlist_helper_nl_wire_traverse (Wire *wire, GSList **lst)
{
	GSList *nodes;

	g_return_if_fail (wire != NULL);
	g_return_if_fail (IS_WIRE (wire));

	if (wire_is_visited (wire))
		return;

	wire_set_visited (wire, TRUE);

	for (nodes = wire_get_nodes (wire); nodes; nodes = nodes->next) {
		GSList *pins;
		Part *part;
		Node *node = nodes->data;

		for (pins=node->pins; pins; pins=pins->next) {
			char *template, *tmp;
			char **template_split;

			part = PART (((Pin *)pins->data)->part);

			tmp = part_get_property (part, "template");

			if (!tmp) continue;

			template = part_property_expand_macros (part, tmp);

			template_split = g_strsplit (template, " ", 0);

			(*lst) = g_slist_prepend (*lst, g_strdup (template_split[0]));

			g_strfreev (template_split);
			g_free (tmp);
			g_free (template);
		}

		netlist_helper_nl_node_traverse (node, lst);
	}
	g_slist_free_full (nodes, g_object_unref);
}

static void
netlist_helper_nl_node_traverse (Node *node, GSList **lst)
{
	GSList *wires;

	g_return_if_fail (node != NULL);
	g_return_if_fail (IS_NODE (node));

	if (node_is_visited (node))
		return;

	node_set_visited (node, TRUE);

	for (wires = node->wires; wires; wires = wires->next) {
		Wire *wire = wires->data;
		netlist_helper_nl_wire_traverse (wire, lst);
	}
	g_slist_free_full (wires, g_object_unref);
}

static GSList * 
netlist_helper_get_clamp_parts (NodeStore *store, Node *node)
{
	GList *wires;
	GSList *list;
	Wire *wire;
	GSList *ret = NULL;

	if (!node) return NULL;

	node_store_node_foreach (store, (GHFunc *) netlist_helper_node_foreach_reset, 
	                         NULL);
	for (wires = store->wires; wires; wires = wires->next) {
		wire = wires->data;
		wire_set_visited (wire, FALSE);
	}

	list = node->wires;
	while (list) {
		netlist_helper_nl_wire_traverse (list->data, &ret);
		list = list->next;
	}

	g_list_free_full (wires, g_object_unref);
	g_slist_free_full (list, g_object_unref);

	return ret;
}

void
netlist_helper_init_data (NetlistData *data)
{
	data->pins = g_hash_table_new (g_direct_hash, g_direct_equal);
	data->models = g_hash_table_new (g_str_hash, g_str_equal);
	data->node_nr = 1;
	data->gnd_list = NULL;
	data->clamp_list = NULL;
	data->mark_list = NULL;
	data->node_and_number_list = NULL;
}

void
netlist_helper_node_foreach_reset (gpointer key, gpointer value, gpointer user_data)
{
	Node *node = value;

	node_set_visited (node, FALSE);
}

void
netlist_helper_wire_traverse (Wire *wire, NetlistData *data)
{
	GSList *nodes;

	g_return_if_fail (wire != NULL);
	g_return_if_fail (IS_WIRE (wire));

	if (wire_is_visited (wire))
		return;

	wire_set_visited (wire, TRUE);

	for (nodes = wire_get_nodes (wire); nodes; nodes = nodes->next) {
		Node *node = nodes->data;

		netlist_helper_node_traverse (node, data);
	}
	g_slist_free_full (nodes, g_object_unref);
}

void
netlist_helper_node_traverse (Node *node, NetlistData *data)
{
	GSList *wires, *pins;
	gchar *prop;
	NodeAndNumber *nan;

	g_return_if_fail (node != NULL);
	g_return_if_fail (IS_NODE (node));

	if (node_is_visited (node))
		return;

	node_set_visited (node, TRUE);

	// Keep track of netlist nr <---> Node.
	nan = g_new0 (NodeAndNumber, 1);
	nan->node_nr = data->node_nr;
	nan->node = node;
	data->node_and_number_list = g_list_prepend (data->node_and_number_list,
		nan);

	// Traverse the pins at this node.
	for (pins = node->pins; pins; pins = pins->next) {
		Pin *pin = pins->data;

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
			} 
			else if (!g_ascii_strcasecmp (prop, "ground")) {
				data->gnd_list = g_slist_prepend (data->gnd_list, GINT_TO_POINTER (data->node_nr));
			} 
			else if (!g_ascii_strcasecmp (prop, "point")) {
				data->clamp_list = g_slist_prepend (data->clamp_list, GINT_TO_POINTER (data->node_nr));
			} 
			else if (!g_ascii_strncasecmp (prop, "jumper", 5)) {
				// Either jumper2 or jumper4.
				Node *opposite_node;
				Pin opposite_pin;
				gint pin_nr, opposite_pin_nr;
				SheetPos pos;
				Pin *jumper_pins;
				gint num_pins;

				opposite_pin_nr = -1;
				num_pins = part_get_num_pins (pin->part);
				jumper_pins = part_get_pins (pin->part);
				for (pin_nr = 0; pin_nr < num_pins; pin_nr++) {
					if (&jumper_pins[pin_nr] == pin) {
						opposite_pin_nr = pin_nr;
						break;
					}
				}

				switch (opposite_pin_nr) {
				case 0:
					opposite_pin_nr = 1;
					break;
				case 1:
					opposite_pin_nr = 0;
					break;
				case 2:
					opposite_pin_nr = 3;
					break;
				case 3:
					opposite_pin_nr = 2;
					break;
				default:
					g_assert (TRUE);
					break;
				}

				opposite_pin = jumper_pins[opposite_pin_nr];

				item_data_get_pos (ITEM_DATA (pin->part), &pos);
				pos.x += opposite_pin.offset.x;
				pos.y += opposite_pin.offset.y;

				opposite_node = node_store_get_node (data->store, pos);

#if 0
				if (node_is_visited (opposite_node)) {
					GList *list;

					/* Set the node name on the current node to the same as the
					   already  visited node. */
					for (list = data->node_and_number_list; list; list = list->next) {
						NodeAndNumber *opposite_nan = list->data;

						if (opposite_nan->node == opposite_node)
							nan->node_nr = opposite_nan->node_nr;
					}
					g_list_free_full (list, g_object_unref);
				}
#endif
				netlist_helper_node_traverse (opposite_node, data);
			}

			if (g_ascii_strcasecmp (prop, "point")) {
				g_free (prop);
				continue;
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
	for (wires = node->wires; wires; wires = wires->next) {
		Wire *wire = wires->data;
		netlist_helper_wire_traverse (wire, data);
	}
	g_slist_free_full (wires, g_object_unref);
	g_slist_free_full (pins, g_object_unref);
}

void
netlist_helper_node_foreach_traverse (gpointer key, gpointer value, NetlistData *data)
{
	Node *node = value;

	// Only visit nodes that are not already visited.
	if (node_is_visited (node))
		return;

	netlist_helper_node_traverse (node, data);

	data->node_nr++;
}

gint
compare_marker (gconstpointer a, gconstpointer b)
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

gboolean
netlist_helper_foreach_model_save (gpointer key, gpointer model, 
                                   gpointer user_data)
{
	GList **l = (GList **)user_data;

	(*l) = g_list_prepend (*l, g_strdup ((gchar *)key));

	return TRUE;
}

gboolean
netlist_helper_foreach_model_free (gpointer key, gpointer model, 
                                   gpointer user_data)
{
	g_free (key);

	return FALSE;
}

char *
netlist_helper_linebreak (char *str)
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
		} 
		else {
			out = g_string_append (out, split[i]);
		}

		i++;
	}

	g_strfreev (split);
	tmp = out->str;
	g_string_free (out, FALSE); // Don't free the string data.
	return tmp;
}

void
netlist_helper_create (Schematic *sm, Netlist *out, GError **error)
{
	NetlistData data;
	GList *parts, *wires, *list;
	Part *part;
	gint pin_nr, num_pins, num_nodes, num_gnd_nodes, i, j, num_clamps;
	Pin *pins;
	gchar *template, **template_split;
	NodeStore *store;
	gchar **node2real;

	schematic_log_clear (sm);

	out->models = NULL;
	out->title = schematic_get_filename (sm);
	out->settings = schematic_get_sim_settings (sm);
	store = schematic_get_store (sm);
	out->store = store;
	node_store_node_foreach (store, (GHFunc *)netlist_helper_node_foreach_reset, NULL);
	for (wires = store->wires; wires; wires = wires->next) {
		Wire *wire = wires->data;
		wire_set_visited (wire, FALSE);
	}

	netlist_helper_init_data (&data);
	data.store = store;

	node_store_node_foreach (store, (GHFunc *)netlist_helper_node_foreach_traverse, &data);
	num_gnd_nodes = g_slist_length (data.gnd_list);
	num_clamps = g_slist_length (data.clamp_list);

	// Check if there is a Ground node
	if (num_gnd_nodes == 0) {
		schematic_log_append (sm, _("No ground node. Aborting.\n"));
		schematic_log_show (sm);
		g_set_error (error,
			OREGANO_ERROR,
			OREGANO_SIMULATE_ERROR_NO_GND,
			_("Possibly due to a faulty circuit schematic. Please check that\n"
			  "you have a ground node and try again."));
	}

	else if (num_clamps == 0) {
		schematic_log_append (sm, _("No test clamps found. Aborting.\n"));
		schematic_log_show (sm);
		g_set_error (error,
			OREGANO_ERROR,
			OREGANO_SIMULATE_ERROR_NO_CLAMP,
			_("Possibly due to a faulty circuit schematic. Please check that\n"
			  "you have one o more test clamps and try again."));
	}

	else {

		num_nodes = data.node_nr - 1;
		
	 	// Build an array for going from node nr to "real node nr",
	 	// where gnd nodes are nr 0 and the rest of the nodes are
	 	// 1, 2, 3, ...
		node2real = g_new0 (gchar*, num_nodes + 1);
		
		for (i = 1, j = 1; i <= num_nodes; i++) {
			GSList *mlist;

			if (g_slist_find (data.gnd_list, GINT_TO_POINTER (i))) {
				node2real[i] = g_strdup ("0");
			}
			else if ((mlist = g_slist_find_custom (data.mark_list,
				GINT_TO_POINTER (i), compare_marker))) {
				Marker *marker = mlist->data;
				node2real[i] = g_strdup (marker->name);
			}
			else {
				node2real[i] = g_strdup_printf ("%d", j++);
			}
		}

	 	// Fill in the netlist node names for all the used nodes.
		for (list = data.node_and_number_list; list; list = list->next) {
			NodeAndNumber *nan = list->data;
			if (nan->node->netlist_node_name != NULL)
				g_free (nan->node->netlist_node_name);
			if (nan->node_nr != 0)
				nan->node->netlist_node_name = g_strdup (node2real[nan->node_nr]);
		}

		// Initialize out->template
		out->template = g_string_new ("");
		for (parts = store->parts; parts; parts = parts->next) {
			gchar *tmp, *internal;
			GString *str;
	
			part = parts->data;
			internal = part_get_property (part, "internal");
			if (internal != NULL) {
				gint node_nr;
				Pin *pins;
				if (g_ascii_strcasecmp (internal, "point") != 0) {
					g_free (internal);
					continue;
				}
			
				// Got a clamp!, set node number
				pins = part_get_pins (part);
				node_nr = GPOINTER_TO_INT (g_hash_table_lookup (data.pins, &pins[0]));
				if (!node_nr) {
					g_warning ("Couln't find part, pin_nr %d.", 0);
				} 
				else {
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
			NG_DEBUG (g_strdup_printf ("Template: '%s'\n" "macro   : '%s'\n", tmp, template));
			if (tmp != NULL) g_free (tmp);

			tmp = netlist_helper_linebreak (template);

			if (template != NULL) g_free (template);
			template = tmp;

			num_pins = part_get_num_pins (part);
			pins = part_get_pins (part);

			template_split = g_strsplit (template, " ", 0);
			if (template != NULL) g_free (template);
			template = NULL;

			str = g_string_new ("");

			i = 0;
			while (template_split[i] != NULL && template_split[i][0] != '%') {
				g_string_append (str, template_split[i++]);
				g_string_append_c (str , ' ');
				NG_DEBUG (g_strdup_printf ("str: %s\n", str->str));
			}

			NG_DEBUG (g_strdup_printf ("Reading %d pins.\n)", num_pins));

			for (pin_nr = 0; pin_nr < num_pins; pin_nr++) {
				gint node_nr = 0;
				node_nr = GPOINTER_TO_INT (g_hash_table_lookup (data.pins, &pins[pin_nr]));
				if (!node_nr) {
					gchar * tmp = g_strdup_printf (_("Couldn't find part, pin_nr %d."), pin_nr);
					oregano_error_with_title (_("Problem of library:"), tmp);
					return;
				} 
				else {
					gchar *tmp;
					tmp = node2real[node_nr];

					// need to substrac 1, netlist starts in 0, and node_nr in 1
					pins[pin_nr].node_nr = atoi (node2real[node_nr]);
					g_string_append (str, tmp);
					g_string_append_c (str, ' ');
					NG_DEBUG (g_strdup_printf ("str: %s\n", str->str));
					i++;
				}

				while (template_split[i] != NULL) {
					if (template_split[i][0] == '%')
						break;

					g_string_append (str, template_split[i]);
					g_string_append_c (str, ' ');
					NG_DEBUG (g_strdup_printf ("str: %s\n", str->str));
					i++;
				}
			}

			NG_DEBUG (g_strdup_printf ("Done with pins, i = %d\n", i));

			while (template_split[i] != NULL) {
				if (template_split[i][0] == '%')
					break;
				g_string_append (str, template_split[i]);
				g_string_append_c (str, ' ');
				NG_DEBUG (g_strdup_printf ("str: %s\n", str->str));
				i++;
			}

			g_strfreev (template_split);
			NG_DEBUG (g_strdup_printf ("str: %s\n", str->str));
			out->template = g_string_append (out->template, str->str);
			out->template = g_string_append_c (out->template, '\n');
			g_string_free (str, TRUE);
		}

		for (i = 0; i < num_nodes + 1; i++) {
			g_free (node2real[i]);
		}
		g_free (node2real);

		g_hash_table_foreach (data.models, (GHFunc)netlist_helper_foreach_model_save,
	    	&out->models);
		return;
	}

	g_hash_table_foreach (data.models, (GHFunc)netlist_helper_foreach_model_free, NULL);
	g_hash_table_destroy (data.models);
	g_hash_table_destroy (data.pins);
	for (list = data.node_and_number_list; list; list = list->next) {
		NodeAndNumber *nan = list->data;
		if (nan != NULL) g_free (nan);
	}
	g_list_free (data.node_and_number_list);
	
	g_list_free_full (wires, g_object_unref);
	g_list_free_full (list, g_object_unref);
}

char *
netlist_helper_create_analysis_string (NodeStore *store, gboolean do_ac)
{
	GList *p;
	gchar *prop;
	GString *out;
	gchar *ret;

	out = g_string_new ("");

	for (p = node_store_get_parts (store); p; p = p->next) {
		prop = part_get_property (p->data, "internal");
		if (prop) {
			if (!g_ascii_strcasecmp (prop, "point")) {
				Pin *pins = part_get_pins (p->data);
				g_free (prop);

				prop = part_get_property (p->data, "type");
			
				if (!g_ascii_strcasecmp (prop, "v")) {
					if (!do_ac) {
						g_string_append_printf (out, " %s(%d)", prop, pins[0].node_nr);
					} 
					else {
						gchar *ac_type, *ac_db;
						ac_type = part_get_property (p->data, "ac_type");
						ac_db = part_get_property (p->data, "ac_db");
	
						if (!g_ascii_strcasecmp (ac_db, "true"))
							g_string_append_printf (out, " %s%sdb(%d)", prop, ac_type, pins[0].node_nr);
						else
							g_string_append_printf (out, " %s%s(%d)", prop, ac_type, pins[0].node_nr);
					}
				} 
				else {
					Node *node;
					SheetPos lookup_key;
					SheetPos part_pos;
	
					item_data_get_pos (ITEM_DATA (p->data), &part_pos);

					lookup_key.x = part_pos.x + pins[0].offset.x;
					lookup_key.y = part_pos.y + pins[0].offset.y;

					node = node_store_get_or_create_node (store, lookup_key);

					if (node) {
						GSList *lst, *it;
						it = lst = netlist_helper_get_clamp_parts (store, node);
						while (it) {
							g_string_append_printf (out, " i(%s)", (char *)it->data);
							it = it->next;
						}
						g_slist_free (lst);
					}
				}
			}
		}
	}

	ret = out->str;
	g_string_free (out, FALSE);
	g_list_free_full (p, g_object_unref);
	return ret;
}

GSList 	*	
netlist_helper_get_voltmeters_list (Schematic *sm, GError **error)
{
	Netlist netlist_data;
	GError *local_error = 0;
	GSList *clamp_list = NULL;
	NodeStore *node_store;
	GList *p;
	gchar *prop;

	netlist_helper_create (sm, &netlist_data, &local_error);
	error = &local_error;

	node_store = netlist_data.store;

	for (p = node_store_get_parts (node_store); p; p = p->next) {
		prop = part_get_property (p->data, "internal");
		if (prop) {
			if (!g_ascii_strcasecmp (prop, "point")) {
				Pin *pins = part_get_pins (p->data);
				g_free (prop);
				prop = part_get_property (p->data, "type");
			
				if (!g_ascii_strcasecmp (prop, "v")) {
					gchar * tmp;
					tmp = g_strdup_printf ("%d", pins[0].node_nr);
					clamp_list = g_slist_prepend (clamp_list, tmp);
					if (0) printf ("clamp_list = %s\n", tmp);
				} 
			}
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
		while (slist)
		{
			text = g_strdup_printf ("%s V(%d)", text, atoi (slist->data));
			slist = slist->next;
		}
		if (0) printf ("clamp_list = %s\n", text);
	}
	g_list_free_full (p, g_object_unref);
	
	return clamp_list;
}
