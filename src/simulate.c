/*
 * Copyright (C) 2003  Andres de Barbara
 *
 * Based on Richard Hult software
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
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include "main.h"
#include "schematic.h"
#include "sim-settings.h"
#include "sheet-private.h"
#include "node-store.h"
#include "node.h"
#include "wire.h"
#include "part-private.h"
#include "part-property.h"
#include "netlist.h"
#include "simulate.h"
#include "part-private.h"

typedef struct {
	gchar *cmd;
	gchar *title;
	GString *template;
	SimSettings *settings;
	NodeStore *store;
} net_out;

static void nl_node_traverse (Node *node, GSList **);
static GSList* nl_get_clamp_parts (NodeStore *store, Node *node);
static void nl_node_traverse (Node *node, GSList **);


GQuark
oregano_simulate_error_quark (void) 
{
	static GQuark err = 0;
	if (!err) {
		err = g_quark_from_static_string ("oregano-simulate-error");
	}
	return err;
}

static void
nl_wire_traverse (Wire *wire, GSList **lst)
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

		for(pins=node->pins; pins; pins=pins->next) {
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

		nl_node_traverse (node, lst);
	}
}

static void
nl_node_traverse (Node *node, GSList **lst)
{
	GSList *wires;

	g_return_if_fail (node != NULL);
	g_return_if_fail (IS_NODE (node));

	if (node_is_visited (node))
		return;

	node_set_visited (node, TRUE);

	for (wires = node->wires; wires; wires = wires->next) {
		Wire *wire = wires->data;
		nl_wire_traverse (wire, lst);
	}
}

static GSList * 
nl_get_clamp_parts (NodeStore *store, Node *node)
{
	GList *wires;
	GSList *lst;
	Wire *wire;
	GSList *ret=NULL;

	if (!node) return;

	node_store_node_foreach (store, (GHFunc *) node_foreach_reset, NULL);
	for (wires = store->wires; wires; wires = wires->next) {
		wire = wires->data;
		wire_set_visited (wire, FALSE);
	}

	lst = node->wires;
	while (lst) {
		nl_wire_traverse (lst->data, &ret);
		lst = lst->next;
	}

	return ret;
}


gint
open_nl_file (gchar **name, FILE **f)
{
	gchar *tmp_filename = NULL;
	gint fd;

	/* If no name was given, a temporary filename is created */
	if ((*name) == NULL) {
		tmp_filename = g_build_filename (g_get_tmp_dir(), "oregXXXXXX", NULL);
		fd = mkstemp (tmp_filename);
		if (fd == -1) {
			g_warning(N_("Could't generate temp file!!\n"));
			g_free(tmp_filename);
			return FALSE;
		}
		*f = fdopen (fd, "w");
		/* Maybe this file should be unlinked */
		*name = g_strdup(tmp_filename);
	} else {
		*f = fopen (*name, "w");
	}

	/* If the file couldn't be opened, return false */
	if (*f == NULL) {
		return FALSE;
	}

	/* If everything went ok, return true*/
	return TRUE;
}

void
initialize_nl_data (NetlistData **data, NodeStore *store)
{
	*data = g_new0 (NetlistData, 1);
	(*data)->pins = g_hash_table_new (g_direct_hash, g_direct_equal);
	(*data)->models = g_hash_table_new (g_str_hash, g_str_equal);
	(*data)->node_nr = 1;
	(*data)->gnd_list = NULL;
	(*data)->mark_list = NULL;
	(*data)->node_and_number_list = NULL;
	(*data)->store = store;
}
	
char *
nl_create_analisys_string (NodeStore *store, gboolean do_ac)
{
	GList *parts;
	GList *p;
	gchar *prop, *type, *ac;
	parts = node_store_get_parts (store);
	GString *out;
	gchar *ret;

	out = g_string_new ("");

	for(p=parts; p; p = p->next) {
		prop = part_get_property (p->data, "internal");
		if (prop) {
			if (!g_strcasecmp (prop, "punta")) {
				Pin *pins = part_get_pins (p->data);
				g_free (prop);

				prop = part_get_property (p->data, "type");
			
				if (!g_strcasecmp (prop, "v")) {
					if (!do_ac) {
						g_string_append_printf (out, " %s(%d)", prop, pins[0].node_nr);
					} else {
						gchar *ac_type, *ac_db;
						ac_type = part_get_property (p->data, "ac_type");
						ac_db = part_get_property (p->data, "ac_db");
	
						if (!g_strcasecmp (ac_db, "true"))
							g_string_append_printf (out, " %s%sdb(%d)", prop, ac_type, pins[0].node_nr);
						else
							g_string_append_printf (out, " %s%s(%d)", prop, ac_type, pins[0].node_nr);
					}
				} else {
					Node *node;
					SheetPos lookup_key;
					SheetPos part_pos;
	
					item_data_get_pos (ITEM_DATA (p->data), &part_pos);

					lookup_key.x = part_pos.x + pins[0].offset.x;
					lookup_key.y = part_pos.y + pins[0].offset.y;

					node = node_store_get_or_create_node (store, lookup_key);

					if (node) {
						GSList *lst, *it;
						it = lst = nl_get_clamp_parts (store, node);
						while (it) {
							g_string_append_printf (out, " i(%s)", it->data);
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
	return ret;
}


void nl_generate_spice (net_out * , FILE *, NetlistData *data);
void nl_generate_gnucap (net_out *, FILE *, NetlistData * data);

gchar *
nl_generate (Schematic *sm, gchar *fn, GError **error)
{
	net_out out;
	gchar *name;
	int iii = 0;
	NetlistData *data;
	GList *parts, *wires, *list;
	Part *part;
	gint pin_nr, num_pins, num_nodes, num_gnd_nodes, i, j, num_clamps;
	Pin *pins;
	gchar *template, **template_split;
	NodeStore *store;
	gchar **node2real;
	FILE *f;
	SimSettings *sim_settings;
	int engine_type=0;

	if (fn != NULL) {
		name = g_strdup (fn);
	} else {
		name = NULL;
	}

	schematic_log_clear (sm);

	/* Try to open the file.  Return NULL if it fails*/
	if (open_nl_file(&name, &f) == FALSE ) {
		g_set_error (error,
			OREGANO_SIMULATE_ERROR,
			OREGANO_SIMULATE_ERROR_IO_ERROR,
				_("Possibly due an I/O error.")
			);
		return NULL;
	}

	out.title = schematic_get_filename (sm);
	out.settings = schematic_get_sim_settings (sm);
	store = schematic_get_store (sm);
	out.store = store;
	node_store_node_foreach (store, (GHFunc *) node_foreach_reset, NULL);
	for (wires = store->wires; wires; wires = wires->next) {
		Wire *wire = wires->data;
		wire_set_visited (wire, FALSE);
	}

	initialize_nl_data(&data, store);

	node_store_node_foreach (store, (GHFunc *) node_foreach_traverse, data);

	num_gnd_nodes = g_slist_length (data->gnd_list);
	num_clamps = g_slist_length (data->clamp_list);

	/* Check if there is a Ground node */
	if (num_gnd_nodes == 0) {
		schematic_log_append (sm, _("No ground node. Aborting.\n"));
		schematic_log_show (sm);
		g_set_error (error,
			OREGANO_SIMULATE_ERROR,
			OREGANO_SIMULATE_ERROR_NO_GND,
				_("Possibly due to a faulty circuit schematic. Please check that\n"
				"you have a Ground node and try again.")
			);

		/* No file will be generated. */
		fclose (f);
		unlink (name);
		g_free (name);
		name = NULL;

		/* FIXME!!! */
		goto bail_out;
	}

	if (num_clamps == 0) {
		schematic_log_append (sm, _("No test clamps found. Aborting.\n"));
		schematic_log_show (sm);
		g_set_error (error,
			OREGANO_SIMULATE_ERROR,
			OREGANO_SIMULATE_ERROR_NO_CLAMP,
				_("Possibly due to a faulty circuit schematic. Please check that\n"
					"you have one o more test clamps and try again.")
			);

		fclose (f);
		unlink (name);
		g_free (name);
		name = NULL;

		goto bail_out;
	}

	num_nodes = data->node_nr - 1;

	/*
	 * Build an array for going from node nr to "real node nr",
	 * where gnd nodes are nr 0 and the rest of the nodes are
	 * 1, 2, 3, ...
	 */
	node2real = g_new0 (gchar*, num_nodes + 1);

	for (i = 1, j = 1; i <= num_nodes; i++) {
		GSList *mlist;

		if (g_slist_find (data->gnd_list, GINT_TO_POINTER (i)))
			node2real[i] = g_strdup ("0");
		else if ((mlist = g_slist_find_custom (data->mark_list,
			GINT_TO_POINTER (i),
			compare_marker))) {
			Marker *marker = mlist->data;
			node2real[i] = g_strdup (marker->name);
		}
		else node2real[i] = g_strdup_printf ("%d", j++);
	}


	/*
	 * Fill in the netlist node names for all the used nodes.
	 */
	for (list = data->node_and_number_list; list; list = list->next) {
		NodeAndNumber *nan = list->data;
		if (nan->node->netlist_node_name != NULL)
			g_free (nan->node->netlist_node_name);
		if (nan->node_nr != 0)
			nan->node->netlist_node_name = g_strdup (node2real[nan->node_nr]);
	}

	/* Initialize out.template */
	out.template = g_string_new("");
	for (parts = store->parts; parts; parts = parts->next) {
		gchar *tmp, *internal;
		GString *str;

		part = parts->data;
		internal = part_get_property (part, "internal");
		if (internal != NULL) {
			gint node_nr;
			Pin *pins;
			if (g_strcasecmp (internal, "punta") != 0) {
				g_free (internal);
				continue;
			}
			
			/* Got a clamp!, set node number */
			pins = part_get_pins (part);
			node_nr = GPOINTER_TO_INT (g_hash_table_lookup (data->pins, &pins[0]));
			if (!node_nr) {
				g_warning ("Couln't find part, pin_nr %d.", 0);
			} else {
				gchar *tmp;
				tmp = node2real[node_nr];

				/* need to substrac 1, netlist starts in 0, and node_nr in 1 */
				pins[0].node_nr = atoi(node2real[node_nr]);
			}
			g_free (internal);
			continue;
		}

		tmp = part_get_property (part, "template");
		if (!tmp) {
			continue;
		}

		template = part_property_expand_macros (part, tmp);
		//g_print ("Template: '%s'\n" "macro   : '%s'\n", tmp, template);
		if (tmp != NULL) g_free (tmp);

		tmp = linebreak (template);

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
			//g_print ("str: %s\n", str->str);
		}

		//g_print ("Reading %d pins.\n)", num_pins);

		for (pin_nr = 0; pin_nr < num_pins; pin_nr++) {
			gint node_nr;
			node_nr = GPOINTER_TO_INT (g_hash_table_lookup (data->pins,
				&pins[pin_nr]));
			if (!node_nr) {
				g_warning ("Couln't find part, pin_nr %d.", pin_nr);
			} else {
				gchar *tmp;
				gchar *_tmp;
				tmp = node2real[node_nr];

				/* need to substrac 1, netlist starts in 0, and node_nr in 1 */
				pins[pin_nr].node_nr = atoi(node2real[node_nr]);
				g_string_append (str, tmp);
				g_string_append_c (str, ' ');
				/*g_print ("str: %s\n", str->str);*/
				i++;
			}

			while (template_split[i] != NULL) {
				if (template_split[i][0] == '%')
					break;

				g_string_append (str, template_split[i]);
				g_string_append_c (str, ' ');
				//g_print ("str: %s\n", str->str);
				i++;
			}
		}

		//g_print ("Done with pins, i = %d\n", i);

		while (template_split[i] != NULL) {
			g_string_append (str, template_split[i]);
			g_string_append_c (str, ' ');
			//g_print ("str: %s\n", str->str);
			i++;
		}

		g_strfreev (template_split);
		//g_print ("str: %s\n", str->str);
		out.template = g_string_append(out.template, str->str);
		out.template = g_string_append_c(out.template, '\n');
		g_string_free (str, TRUE);
	}

	for (i = 0; i < num_nodes + 1; i++) {
		g_free (node2real[i]);
	}
	g_free (node2real);

	if (strcmp (oregano.simtype,"gnucap") == 0 )
		nl_generate_gnucap (&out, f, data);
	else if (strcmp (oregano.simtype,"ngspice") == 0)
		nl_generate_spice (&out, f, data);
	else
		printf ("Simulator %s is not avaible.", oregano.simtype );
	fclose (f);

	return name;

 bail_out:
	g_hash_table_foreach (data->models, (GHFunc)foreach_model_free, NULL);
	g_hash_table_destroy (data->models);
	g_hash_table_destroy (data->pins);
	for (list = data->node_and_number_list; list; list = list->next) {
		NodeAndNumber *nan = list->data;
		if (nan != NULL) g_free (nan);
	}
	g_list_free (data->node_and_number_list);
	if (data != NULL) g_free (data);
	return name;
}


void nl_generate_spice (net_out * output, FILE *file, NetlistData * data)
{

	GList *list = sim_settings_get_options (output->settings);
	SimOption *so;

	/* Prints title */
	fputs ("* ",file);
	fputs (output->title ? output->title : "Title: <unset>", file);
	fputs ("\n"
		   "*----------------------------------------------"
		   "\n"
		   "*\tNGSPICE - NETLIST"
		   "\n", file);
	/* Prints Options */
	fputs (".options NOPAGE\n",file);

	while (list) {
		so = list->data;
		/* Prevent send NULL text */
		if (so->value) {
			if (strlen(so->value) > 0)
				fprintf (file,"+ %s=%s\n",so->name,so->value);
		}
		list = list->next;
	}
		
	/* Inclusion of complex part */
	g_hash_table_foreach (data->models, (GHFunc) foreach_model_write, file);

	/* Prints template parts */

	fputs ("\n*----------------------------------------------\n\n",file);
	fputs (output->template->str,file);
	fputs ("\n*----------------------------------------------\n\n",file);

	/* Prints Transient Analisis */
	if (sim_settings_get_trans(output->settings)) {
		fprintf (file, ".tran %g %g %g %s\n",
			sim_settings_get_trans_step(output->settings),
			sim_settings_get_trans_stop(output->settings),
			sim_settings_get_trans_start(output->settings),
			sim_settings_get_trans_init_cond(output->settings)?"UIC":"");
		fprintf(file, ".print tran %s\n", nl_create_analisys_string (output->store, FALSE));
	}

	/*	Print dc Analysis */
	if (sim_settings_get_dc (output->settings)) {
		fputs(".dc ",file);
		if (sim_settings_get_dc_vsrc(output->settings,0) ) {
			fprintf (file, "%s %g %g %g\n",
					sim_settings_get_dc_vsrc(output->settings,0),
					sim_settings_get_dc_start(output->settings,0),
					sim_settings_get_dc_stop(output->settings,0),
					sim_settings_get_dc_step(output->settings,0) );
		}
		if ( sim_settings_get_dc_vsrc(output->settings,1) ) {
			fprintf (file, "%s %g %g %g\n",
					sim_settings_get_dc_vsrc(output->settings,1),
					sim_settings_get_dc_start(output->settings,1),
					sim_settings_get_dc_stop(output->settings,1),
					sim_settings_get_dc_step(output->settings,1) );
		}
		fprintf(file, ".print dc  %s\n", nl_create_analisys_string (output->store, FALSE));
	}

	/* Prints ac Analysis*/
	if ( sim_settings_get_ac (output->settings) ) {
		fprintf (file, ".ac %s %d %g %g\n",
				sim_settings_get_ac_type(output->settings),
				sim_settings_get_ac_npoints(output->settings),
				sim_settings_get_ac_start(output->settings),
				sim_settings_get_ac_stop(output->settings)
				);
		fprintf(file, ".print ac %s\n", nl_create_analisys_string (output->store, TRUE));
	}

	/* Debug op analysis. */
	fputs (".op\n", file);
	fputs ("\n.end\n", file);
}

void nl_generate_gnucap (net_out * output, FILE *file, NetlistData * data)
{
	GList *list = sim_settings_get_options ( output->settings );
	SimOption *so;

	/* Prints title */
	fputs (output->title ? output->title : "Title: <unset>", file);
	fputs ("\n"
		   "*----------------------------------------------"
		   "\n"
		   "*\tGNUCAP - NETLIST"
		   "\n", file);
	/* Prints Options */
	fputs (".options OUT=120",file);

	while (list) {
		so = list->data;
		/* Prevent send NULL text */
		if (so->value) {
			if (strlen(so->value) > 0) {
				fprintf (file,"%s=%s ",so->name,so->value);

			}
		}
		list = list->next;
	}

	fputc ('\n',file);
	/* Prints template parts */

	/* Inclusion of complex part */
	/* TODO : add GNU Cap model inclusion */
	/* g_hash_table_foreach (data->models, (GHFunc) foreach_model_write_gnucap, file); */

	fputs ("*----------------------------------------------\n",file);
	fputs (output->template->str,file);
	fputs ("*----------------------------------------------\n",file);

	/* Prints Transient Analisis */
	if (sim_settings_get_trans (output->settings)) {
		fprintf(file, ".print tran %s\n", nl_create_analisys_string (output->store, FALSE));
		fprintf (file, ".tran %g %g ",
			sim_settings_get_trans_start(output->settings),
			sim_settings_get_trans_stop(output->settings));
			if (!sim_settings_get_trans_step_enable(output->settings))
				/* FIXME Do something to get the right resolution */
				fprintf(file,"%g",
						(sim_settings_get_trans_stop(output->settings)-
						sim_settings_get_trans_start(output->settings))/100);
			else
				fprintf(file,"%g", sim_settings_get_trans_step(output->settings));

			if (sim_settings_get_trans_init_cond(output->settings)) {
				fputs(" UIC\n", file);
			} else {
				fputs("\n", file);
			}
	}

	/*	Print dc Analysis */
	if (sim_settings_get_dc (output->settings)) {
		fprintf(file, ".print dc %s\n", nl_create_analisys_string (output->store, FALSE));
		fputs(".dc ",file);

		/* GNUCAP don t support nesting so the first or the second */
		/* Maybe a error message must be show if both are on	   */

		if ( sim_settings_get_dc_vsrc (output->settings,0) ) {
			fprintf (file,"%s %g %g %g",
					sim_settings_get_dc_vsrc(output->settings,0),
					sim_settings_get_dc_start (output->settings,0),
					sim_settings_get_dc_stop (output->settings,0),
					sim_settings_get_dc_step (output->settings,0) );
		}

		else if ( sim_settings_get_dc_vsrc (output->settings,1) ) {
			fprintf (file,"%s %g %g %g",
					sim_settings_get_dc_vsrc(output->settings,1),
					sim_settings_get_dc_start (output->settings,1),
					sim_settings_get_dc_stop (output->settings,1),
					sim_settings_get_dc_step (output->settings,1) );
		};

		fputc ('\n',file);
	}

	/* Prints ac Analysis*/
	if ( sim_settings_get_ac (output->settings) ) {
		double ac_start, ac_stop, ac_step;
		/* GNUCAP dont support OCT or DEC */
		/* Maybe a error message must be show if is set in that way */
		ac_start = sim_settings_get_ac_start(output->settings) ;
		ac_stop = sim_settings_get_ac_stop(output->settings);
		ac_step = (ac_stop - ac_start) / sim_settings_get_ac_npoints(output->settings);
		fprintf(file, ".print ac %s\n", nl_create_analisys_string (output->store, TRUE));
		/* AC format : ac start stop step_size */
		fprintf (file, ".ac %g %g %g\n", ac_start, ac_stop, ac_step);
	}

	/* Debug op analysis. */
	fputs(".print op v(nodes)\n", file);
	fputs(".op\n", file);
	fputs(".end\n", file);
}
