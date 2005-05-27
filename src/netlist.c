/*
 * netlist.c
 *
 *
 * Author:
 *  Richard Hult <rhult@hem.passagen.se>
 *  Ricardo Markiewicz <rmarkie@fi.uba.ar>
 *  Andres de Barbara <adebarbara@fi.uba.ar>
 *
 * Web page: http://arrakis.lug.fi.uba.ar/
 *
 * Copyright (C) 1999-2001  Richard Hult
 * Copyright (C) 2003,2005  LUGFI
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

#include <config.h>
#include <gnome.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "main.h"
#include "schematic.h"
#include "sim-settings.h"
#include "sheet-private.h"
#include "node-store.h"
#include "node.h"
#include "wire.h"
#include "part-private.h"
#include "part-property.h"
#include "simulate.h"
#include "netlist.h"


void
node_foreach_reset (gpointer key, gpointer value, gpointer user_data)
{
	Node *node = value;

	node_set_visited (node, FALSE);
}

void
wire_traverse (Wire *wire, NetlistData *data)
{
	GSList *nodes;

	g_return_if_fail (wire != NULL);
	g_return_if_fail (IS_WIRE (wire));

	if (wire_is_visited (wire))
		return;

	wire_set_visited (wire, TRUE);

	for (nodes = wire_get_nodes (wire); nodes; nodes = nodes->next) {
		Node *node = nodes->data;

		node_traverse (node, data);
	}
}

void
node_traverse (Node *node, NetlistData *data)
{
	GSList *wires, *pins;
	gchar *prop;
	NodeAndNumber *nan;

	g_return_if_fail (node != NULL);
	g_return_if_fail (IS_NODE (node));

	if (node_is_visited (node))
		return;

	node_set_visited (node, TRUE);

	/*
	 * Keep track of netlist nr <---> Node.
	 */
	nan = g_new0 (NodeAndNumber, 1);
	nan->node_nr = data->node_nr;
	nan->node = node;
	data->node_and_number_list = g_list_prepend (data->node_and_number_list,
		nan);

	/*
	 * Traverse the pins at this node.
	 */
	for (pins = node->pins; pins; pins = pins->next) {
		Pin *pin = pins->data;

		/*
		 * First see if the pin belongs to an "internal", special part.
		 */
		prop = part_get_property (pin->part, "internal");
		if (prop) {
			if (!g_strcasecmp (prop, "marker")) {
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
			} else if (!g_strcasecmp (prop, "ground")) {
				data->gnd_list = g_slist_prepend (data->gnd_list, GINT_TO_POINTER (data->node_nr));
			} else if (!g_strcasecmp (prop, "punta")) {
				data->clamp_list = g_slist_prepend (data->clamp_list, GINT_TO_POINTER (data->node_nr));
			} else if (!g_strncasecmp (prop, "jumper", 5)) {
				/* Either jumper2 or jumper4. */
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
				}
#endif
				node_traverse (opposite_node, data);
			}

			if (g_strcasecmp (prop, "punta")) {
				g_free (prop);
				continue;
			}
			g_free(prop);
		}

		/*
		 * Keep track of models to include. Needs to be freed when the
		 * hash table is no longer needed.
		 */
		prop = part_get_property (pin->part, "model");
		if (prop) {
			if (!g_hash_table_lookup (data->models, prop))
				g_hash_table_insert (data->models, prop, NULL);
		}

		g_hash_table_insert (data->pins, pin, GINT_TO_POINTER (data->node_nr));
	}

	/*
	 * Traverse the wires at this node.
	 */
	for (wires = node->wires; wires; wires = wires->next) {
		Wire *wire = wires->data;
		wire_traverse (wire, data);
	}
}

void
node_foreach_traverse (gpointer key, gpointer value, NetlistData *data)
{
	Node *node = value;

	/*
	 * Only visit nodes that are not already visited.
	 */
	if (!node_is_visited (node))
		node_traverse (node, data);
	else
		return;

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

void
foreach_model_write (char *model, gpointer user_data, FILE *f)
{
	char *str, *tmp1, *tmp2;

	tmp1 = g_build_filename (OREGANO_MODELDIR, model, NULL);
	tmp2 = g_strconcat (tmp1, ".model", NULL);

	str = g_strdup_printf (".include %s\n", tmp2);
	fputs (str, f);

	g_free (tmp1);
	g_free (tmp2);
	g_free (str);
}

void
foreach_model_free (gpointer key, char *model, gpointer user_data)
{
	g_free (key);
}

char *
linebreak (char *str)
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
	g_string_free (out, FALSE); /* Don't free the string data. */
	return tmp;
}

