/*
 * sim-settings.c
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

#include <stdlib.h>
#include <string.h>
#include <glib.h>

#include "oregano-utils.h"
#include "sim-settings.h"

SimSettings *sim_settings_new ()
{
	SimSettings *sim_settings;

	sim_settings = g_new0 (SimSettings, 1);

	// Set some default settings.
	// transient
	sim_settings->trans_enable = TRUE;
	sim_settings->trans_start = g_strdup ("0 s");
	sim_settings->trans_stop = g_strdup ("5 ms");
	sim_settings->trans_step = g_strdup ("0.1 ms");
	sim_settings->trans_step_enable = FALSE;

	//  AC
	sim_settings->ac_enable = FALSE;
	sim_settings->ac_vout = g_strdup ("");
	sim_settings->ac_type = g_strdup ("DEC");
	sim_settings->ac_npoints = g_strdup ("50");
	sim_settings->ac_start = g_strdup ("1");
	sim_settings->ac_stop = g_strdup ("1 Meg");

	// DC
	sim_settings->dc_enable = FALSE;
	sim_settings->dc_vin = g_strdup ("");
	sim_settings->dc_vout = g_strdup ("");
	sim_settings->dc_start = g_strdup ("");
	sim_settings->dc_stop = g_strdup ("");
	sim_settings->dc_step = g_strdup ("");

	// Fourier
	sim_settings->fourier_enable = FALSE;
	sim_settings->fourier_frequency = g_strdup ("");
	sim_settings->fourier_vout = NULL;

	sim_settings->options = NULL;

	return sim_settings;
}

static void sim_option_finalize(SimOption *option) {
	g_free(option->name);
	g_free(option->value);
	g_free(option);
}

void sim_settings_finalize(SimSettings *sim_settings) {

	// Set some default settings.
	// transient
	g_free(sim_settings->trans_start);
	g_free(sim_settings->trans_stop);
	g_free(sim_settings->trans_step);

	//  AC
	g_free(sim_settings->ac_vout);
	g_free(sim_settings->ac_type);
	g_free(sim_settings->ac_npoints);
	g_free(sim_settings->ac_start);
	g_free(sim_settings->ac_stop);

	// DC
	g_free(sim_settings->dc_vin);
	g_free(sim_settings->dc_vout);
	g_free(sim_settings->dc_start);
	g_free(sim_settings->dc_stop);
	g_free(sim_settings->dc_step);

	// Fourier
	sim_settings->fourier_enable = FALSE;
	g_free(sim_settings->fourier_frequency);
	if (sim_settings->fourier_vout != NULL)
		g_slist_free_full(sim_settings->fourier_vout, g_free);

	if (sim_settings->options != NULL)
		g_list_free_full(sim_settings->options, (GDestroyNotify)sim_option_finalize);

	g_free(sim_settings);
}

gchar *fourier_add_vout(SimSettings *sim_settings, gboolean result, guint i) {
	gchar *ret_val = NULL;

	// Is the node identifier already available?
	GSList *node_slist = g_slist_copy (sim_settings->fourier_vout);
	while (node_slist) {
		if ((i - 1) == atoi (node_slist->data)) {
			result = TRUE;
		}
		node_slist = node_slist->next;
	}
	g_slist_free (node_slist);
	if (!result) {
		GSList *node_slist;
		gchar *text = NULL;

		// Add Node (i-1) at the end of fourier_vout
		text = g_strdup_printf ("%d", i - 1);
		sim_settings->fourier_vout =
			g_slist_append (sim_settings->fourier_vout, g_strdup_printf ("%d", i - 1));

		// Update the fourier_vout widget
		node_slist = g_slist_copy (sim_settings->fourier_vout);
		if (node_slist->data)
			text = g_strdup_printf ("V(%d)", atoi (node_slist->data));
		node_slist = node_slist->next;
		while (node_slist) {
			if (node_slist->data)
				text = g_strdup_printf ("%s V(%d)", text, atoi (node_slist->data));
			node_slist = node_slist->next;
		}

		if (text)
			ret_val = text;
		else
			ret_val = g_strdup("");
		g_slist_free (node_slist);
	}

	return ret_val;
}

gboolean sim_settings_get_trans (const SimSettings *sim_settings)
{
	return sim_settings->trans_enable;
}

gboolean sim_settings_get_trans_init_cond (const SimSettings *sim_settings)
{
	return sim_settings->trans_init_cond;
}

gboolean sim_settings_get_trans_analyze_all (const SimSettings *sim_settings)
{
	return sim_settings->trans_analyze_all;
}

gdouble sim_settings_get_trans_start (const SimSettings *sim_settings)
{
	gchar *text = sim_settings->trans_start;
	return oregano_strtod (text, 's');
}

gdouble sim_settings_get_trans_stop (const SimSettings *sim_settings)
{
	gchar *text = sim_settings->trans_stop;
	return oregano_strtod (text, 's');
}

gdouble sim_settings_get_trans_step (const SimSettings *sim_settings)
{
	gchar *text = sim_settings->trans_step;
	return oregano_strtod (text, 's');
}

gdouble sim_settings_get_trans_step_enable (const SimSettings *sim_settings)
{
	return sim_settings->trans_step_enable;
}

void sim_settings_set_trans (SimSettings *sim_settings, gboolean enable)
{
	sim_settings->trans_enable = enable;
}

void sim_settings_set_trans_start (SimSettings *sim_settings, gchar *str)
{
	if (sim_settings->trans_start)
		g_strdup (sim_settings->trans_start);
	sim_settings->trans_start = g_strdup (str);
}

void sim_settings_set_trans_init_cond (SimSettings *sim_settings, gboolean enable)
{
	sim_settings->trans_init_cond = enable;
}

void sim_settings_set_trans_analyze_all (SimSettings *sim_settings, gboolean enable)
{
	sim_settings->trans_analyze_all = enable;
}

void sim_settings_set_trans_stop (SimSettings *sim_settings, gchar *str)
{
	if (sim_settings->trans_stop)
		g_strdup (sim_settings->trans_stop);
	sim_settings->trans_stop = g_strdup (str);
}

void sim_settings_set_trans_step (SimSettings *sim_settings, gchar *str)
{
	if (sim_settings->trans_step)
		g_strdup (sim_settings->trans_step);
	sim_settings->trans_step = g_strdup (str);
}

void sim_settings_set_trans_step_enable (SimSettings *sim_settings, gboolean enable)
{
	sim_settings->trans_step_enable = enable;
}

gboolean sim_settings_get_ac (const SimSettings *sim_settings) { return sim_settings->ac_enable; }

gchar *sim_settings_get_ac_vout (const SimSettings *sim_settings) { return sim_settings->ac_vout; }

gchar *sim_settings_get_ac_type (const SimSettings *sim_settings) { return sim_settings->ac_type; }

gint sim_settings_get_ac_npoints (const SimSettings *sim_settings)
{
	return atoi (sim_settings->ac_npoints);
}

gdouble sim_settings_get_ac_start (const SimSettings *sim_settings)
{
	return oregano_strtod (sim_settings->ac_start, 's');
}

gdouble sim_settings_get_ac_stop (const SimSettings *sim_settings)
{
	return oregano_strtod (sim_settings->ac_stop, 's');
}

void sim_settings_set_ac (SimSettings *sim_settings, gboolean enable)
{
	sim_settings->ac_enable = enable;
}

void sim_settings_set_ac_vout (SimSettings *sim_settings, gchar *str)
{
	g_free (sim_settings->ac_vout);
	sim_settings->ac_vout = g_strdup (str);
}

void sim_settings_set_ac_type (SimSettings *sim_settings, gchar *str)
{
	g_free (sim_settings->ac_type);
	sim_settings->ac_type = g_strdup (str);
}

void sim_settings_set_ac_npoints (SimSettings *sim_settings, gchar *str)
{
	g_free (sim_settings->ac_npoints);
	sim_settings->ac_npoints = g_strdup (str);
}

void sim_settings_set_ac_start (SimSettings *sim_settings, gchar *str)
{
	g_free (sim_settings->ac_start);
	sim_settings->ac_start = g_strdup (str);
}

void sim_settings_set_ac_stop (SimSettings *sim_settings, gchar *str)
{
	g_free (sim_settings->ac_stop);
	sim_settings->ac_stop = g_strdup (str);
}

gboolean sim_settings_get_dc (const SimSettings *sim_settings) { return sim_settings->dc_enable; }

gchar *sim_settings_get_dc_vsrc (const SimSettings *sim_settings) { return sim_settings->dc_vin; }

gchar *sim_settings_get_dc_vout (const SimSettings *sim_settings) { return sim_settings->dc_vout; }

gdouble sim_settings_get_dc_start (const SimSettings *sim_settings)
{
	return oregano_strtod (sim_settings->dc_start, 's');
}

gdouble sim_settings_get_dc_stop (const SimSettings *sim_settings)
{
	return oregano_strtod (sim_settings->dc_stop, 's');
}

gdouble sim_settings_get_dc_step (const SimSettings *sim_settings)
{
	return oregano_strtod (sim_settings->dc_step, 's');
}

void sim_settings_set_dc (SimSettings *sim_settings, gboolean enable)
{
	sim_settings->dc_enable = enable;
}

void sim_settings_set_dc_vsrc (SimSettings *sim_settings, gchar *str)
{
	g_free (sim_settings->dc_vin);
	sim_settings->dc_vin = g_strdup (str);
}

void sim_settings_set_dc_vout (SimSettings *sim_settings, gchar *str)
{
	g_free (sim_settings->dc_vout);
	sim_settings->dc_vout = g_strdup (str);
}

void sim_settings_set_dc_start (SimSettings *sim_settings, gchar *str)
{
	g_free (sim_settings->dc_start);
	sim_settings->dc_start = g_strdup (str);
}

void sim_settings_set_dc_stop (SimSettings *sim_settings, gchar *str)
{
	g_free (sim_settings->dc_stop);
	sim_settings->dc_stop = g_strdup (str);
}

void sim_settings_set_dc_step (SimSettings *sim_settings, gchar *str)
{
	g_free (sim_settings->dc_step);
	sim_settings->dc_step = g_strdup (str);
}

void sim_settings_set_fourier (SimSettings *sim_settings, gboolean enable)
{
	sim_settings->fourier_enable = enable;
}

void sim_settings_set_fourier_frequency (SimSettings *sim_settings, gchar *str)
{
	g_free (sim_settings->fourier_frequency);
	sim_settings->fourier_frequency = g_strdup (str);
}

void sim_settings_set_fourier_vout (SimSettings *sim_settings, gchar *str)
{
	gchar **node_ids = NULL;
	gint i;
	g_free (sim_settings->fourier_vout);
	node_ids = g_strsplit (g_strdup (str), " ", 0);
	for (i = 0; node_ids[i] != NULL; i++) {
		if (node_ids[i])
			sim_settings->fourier_vout =
			    g_slist_append (sim_settings->fourier_vout, g_strdup (node_ids[i]));
	}
}

gboolean sim_settings_get_fourier (const SimSettings *sim_settings)
{
	return sim_settings->fourier_enable;
}

gint sim_settings_get_fourier_frequency (const SimSettings *sim_settings)
{
	return atoi (sim_settings->fourier_frequency);
}

gchar *sim_settings_get_fourier_vout (const SimSettings *sim_settings)
{
	GSList *node_slist;
	gchar *text = NULL;

	node_slist = g_slist_copy (sim_settings->fourier_vout);
	if (node_slist)
		text = g_strdup_printf ("%d", atoi (node_slist->data));
	if (node_slist)
		node_slist = node_slist->next;
	while (node_slist) {
		if (node_slist->data)
			text = g_strdup_printf ("%s %d", text, atoi (node_slist->data));
		node_slist = node_slist->next;
	}
	g_slist_free (node_slist);
	return text;
}

gchar *sim_settings_get_fourier_nodes (const SimSettings *sim_settings)
{
	GSList *node_slist;
	gchar *text = NULL;

	node_slist = g_slist_copy (sim_settings->fourier_vout);
	if (node_slist->data)
		text = g_strdup_printf ("V(%d)", atoi (node_slist->data));
	if (node_slist)
		node_slist = node_slist->next;
	while (node_slist) {
		if (node_slist->data)
			text = g_strdup_printf ("%s V(%d)", text, atoi (node_slist->data));
		node_slist = node_slist->next;
	}
	g_slist_free (node_slist);
	return text;
}

GList *sim_settings_get_options (const SimSettings *sim_settings)
{
	g_return_val_if_fail (sim_settings != NULL, NULL);
	return sim_settings->options;
}

void sim_settings_add_option (SimSettings *sim_settings, SimOption *opt)
{
	GList *iter;
	// Remove the option if already in the list.
	for (iter = sim_settings->options; iter; iter = iter->next) {
		SimOption *so = iter->data;
		if (so && !strcmp (opt->name, so->name)) {
			g_free (so->name);
			g_free (so->value);
			sim_settings->options = g_list_remove (sim_settings->options, so);
			sim_option_finalize(so);
		}
	}
	sim_settings->options = g_list_append (sim_settings->options, opt);
}
