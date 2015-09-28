/*
 * ngspice-analysis.c
 *
 * Authors:
 *  Marc Lorber <Lorber.Marc@wanadoo.fr>
 *  Bernhard Schuster <bernhard@ahoi.io>
 *
 * Web page: https://ahoi.io/project/oregano
 *
 * Copyright (C) 2009-2012  Marc Lorber
 * Copyright (C) 2014       Bernhard Schuster
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

#include <glib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <ctype.h>
#include <stdlib.h>
#include <glib/gi18n.h>

#include "ngspice.h"
#include "netlist-helper.h"
#include "dialogs.h"
#include "engine-internal.h"
#include "ngspice-analysis.h"

typedef enum { STATE_IDLE, IN_VARIABLES, IN_VALUES, STATE_ABORT } ParseDataState;

static gchar *analysis_names[] = {
    "Operating Point",  "Transient Analysis", "DC Transfer Characteristic",
    "AC Analysis",      "Transfer Function",  "Distortion Analysis",
    "Noise Analysis",   "Pole-Zero Analysis", "Sensitivity Analysis",
    "Fourier Analysis", "Unknown Analysis",   NULL};

#define SP_TITLE "oregano\n"
#define CPU_TIME "CPU time since last call:"

#define TAGS_COUNT (sizeof(analysis_tags) / sizeof(struct analysis_tag))
#define IS_THIS_ITEM(str, item) (!strncmp (str, item, strlen (item)))

/**
 * \brief extract the resulting variables from ngspice output
 *
 * @returns a GArray filled up doubles
 */
gchar **get_variables (const gchar *str, gint *count)
{
	g_return_val_if_fail (str, NULL);

	gchar **out;
	static gchar *tmp[100];
	const gchar *start, *end;
	gint i = 0;

	start = str;
	while (isspace (*start))
		start++;
	end = start;
	while (*end != '\0') {
		if (isspace (*end)) {
			// number ended, designate as such and replace the string
			tmp[i] = g_strndup (start, (gsize)(end - start));
			i++;
			start = end;
			while (isspace (*start))
				start++;
			end = start;
		} else {
			end++;
		}
	}
	if (end > start) {
		tmp[i] = g_strndup (start, (gsize)(end - start));
		i++;
	}

	if (i == 0) {
		g_warning ("NO COLUMNS FOUND\n");
		return NULL;
	}

	// append an extra NUL slot to allow using g_strfreev
	out = g_new0 (gchar *, i + 1);
	(*count) = i;
	memcpy (out, tmp, sizeof(gchar *) * i);
	out[i] = NULL;
	return out;
}

void parse_dc_analysis (NgSpice *ngspice, gchar *tmp)
{
	static SimulationData *sdata;
	static Analysis *data;
	NgSpicePrivate *priv = ngspice->priv;
	SimSettings *sim_settings;
	static gchar buf[256];
	gboolean found = FALSE;
	gchar **variables;
	gint i, n = 0, index = 0;
	gdouble val[10];
	gdouble np1;

	oregano_echo ("DC: result str\n>>>\n%s<<<\n", tmp);

	sim_settings = (SimSettings *)schematic_get_sim_settings (priv->schematic);
	data = g_new0 (Analysis, 1);
	priv->current = sdata = SIM_DATA (data);
	priv->analysis = g_list_append (priv->analysis, sdata);
	priv->num_analysis = 1;
	sdata->state = IN_VALUES;
	sdata->type = DC_TRANSFER;
	sdata->functions = NULL;

	np1 = 1.;
	ANALYSIS (sdata)->dc.start = sim_settings_get_dc_start (sim_settings);
	ANALYSIS (sdata)->dc.stop = sim_settings_get_dc_stop (sim_settings);
	ANALYSIS (sdata)->dc.step = sim_settings_get_dc_step (sim_settings);

	np1 = (ANALYSIS (sdata)->dc.stop - ANALYSIS (sdata)->dc.start) / ANALYSIS (sdata)->dc.step;
	ANALYSIS (sdata)->dc.sim_length = np1;

	fgets (buf, 255, priv->inputfp);
	fgets (buf, 255, priv->inputfp);

	// Calculates the number of variables
	variables = get_variables (buf, &n);

	n = n - 1;
	sdata->var_names = (char **)g_new0 (gpointer, n);
	sdata->var_units = (char **)g_new0 (gpointer, n);
	sdata->var_names[0] = g_strdup ("Voltage sweep");
	sdata->var_units[0] = g_strdup (_ ("voltage"));
	sdata->var_names[1] = g_strdup (variables[2]);
	sdata->var_units[1] = g_strdup (_ ("voltage"));

	sdata->state = IN_VALUES;
	sdata->n_variables = 2;
	sdata->got_points = 0;
	sdata->got_var = 0;
	sdata->data = (GArray **)g_new0 (gpointer, 2);

	fgets (buf, 255, priv->inputfp);

	for (i = 0; i < 2; i++)
		sdata->data[i] = g_array_new (TRUE, TRUE, sizeof(double));

	sdata->min_data = g_new (double, n);
	sdata->max_data = g_new (double, n);

	// Read the data
	for (i = 0; i < 2; i++) {
		sdata->min_data[i] = G_MAXDOUBLE;
		sdata->max_data[i] = -G_MAXDOUBLE;
	}
	found = FALSE;
	while ((fgets (buf, 255, priv->inputfp) != NULL) && !found) {
		if (strlen (buf) <= 2) {
			fgets (buf, 255, priv->inputfp);
			fgets (buf, 255, priv->inputfp);
			fgets (buf, 255, priv->inputfp);
		}
		tmp = &buf[0];
		variables = get_variables (tmp, &i);
		index = atoi (variables[0]);
		for (i = 0; i < n; i++) {
			val[i] = g_ascii_strtod (variables[i + 1], NULL);
			sdata->data[i] = g_array_append_val (sdata->data[i], val[i]);
			if (val[i] < sdata->min_data[i])
				sdata->min_data[i] = val[i];
			if (val[i] > sdata->max_data[i])
				sdata->max_data[i] = val[i];
		}
		sdata->got_points++;
		sdata->got_var = 2;
		if (index >= ANALYSIS (sdata)->dc.sim_length)
			found = TRUE;
	}
	return;
}

void parse_transient_analysis (NgSpice *ngspice, gchar *tmp)
{
	static SimulationData *sdata;
	static Analysis *data;
	NgSpicePrivate *priv = ngspice->priv;
	SimSettings *sim_settings;
	static gchar buf[256];
	gboolean found = FALSE;
	gchar **variables;
	gint i, n = 0, m = 0, p = 0;
	gdouble val[10];
	GSList *nodes_list = NULL;
	GError *error = NULL;
	gint nodes_nb = 0;
	GArray **val_tmp1;
	GArray **val_tmp2;
	GArray **val_tmp3;

	oregano_echo ("TRANSIENT: result str\n>>>\n%s<<<\n", tmp);

	sim_settings = (SimSettings *)schematic_get_sim_settings (priv->schematic);
	data = g_new0 (Analysis, 1);
	priv->current = sdata = SIM_DATA (data);
	priv->analysis = g_list_append (priv->analysis, sdata);
	priv->num_analysis = 1;
	sdata->state = IN_VALUES;
	sdata->type = ANALYSIS_UNKNOWN;
	sdata->functions = NULL;

	// Identify the number of analysis from the number of clamp
	// ASCII format of ngspice only returns 3 columns at a time
	nodes_list = netlist_helper_get_voltmeters_list (priv->schematic, &error);
	for (; nodes_list; nodes_list = nodes_list->next) {
		nodes_nb++;
	}

	if (nodes_nb > 9) {
		oregano_warning (_ ("Only the 9 first nodes will be considered"));
		nodes_nb = 9;
	}

	tmp = g_strchug (tmp);
	for (i = 0; analysis_names[i]; i++) {
		if (g_str_has_prefix (g_strdup (tmp), analysis_names[i])) {
			sdata->type = i;
		}
	}

	ANALYSIS (sdata)->transient.sim_length =
	    sim_settings_get_trans_stop (sim_settings) - sim_settings_get_trans_start (sim_settings);
	ANALYSIS (sdata)->transient.step_size = sim_settings_get_trans_step (sim_settings);

	fgets (buf, 255, priv->inputfp);
	fgets (buf, 255, priv->inputfp);

	sdata->var_names = (char **)g_new0 (gpointer, nodes_nb + 1);
	sdata->var_units = (char **)g_new0 (gpointer, nodes_nb + 1);
	variables = get_variables (buf, &n);
	n = n - 1;
	sdata->var_names[0] = g_strdup (_ ("Time"));
	sdata->var_units[0] = g_strdup (_ ("time"));
	for (i = 1; i < n; i++) {
		sdata->var_names[i] = g_strdup (variables[i + 1]);
		sdata->var_units[i] = g_strdup (_ ("voltage"));
	}
	sdata->state = IN_VALUES;
	sdata->n_variables = nodes_nb + 1;
	sdata->got_points = 0;
	sdata->got_var = 0;
	sdata->data = (GArray **)g_new0 (gpointer, nodes_nb + 1);

	fgets (buf, 255, priv->inputfp);
	fgets (buf, 255, priv->inputfp);

	for (i = 0; i < nodes_nb + 1; i++)
		sdata->data[i] = g_array_new (TRUE, TRUE, sizeof(double));

	val_tmp1 = (GArray **)g_new0 (gpointer, 4);
	val_tmp2 = (GArray **)g_new0 (gpointer, 4);
	val_tmp3 = (GArray **)g_new0 (gpointer, 4);
	for (i = 0; i < 4; i++) {
		val_tmp1[i] = g_array_new (TRUE, TRUE, sizeof(double));
		val_tmp2[i] = g_array_new (TRUE, TRUE, sizeof(double));
		val_tmp3[i] = g_array_new (TRUE, TRUE, sizeof(double));
	}

	sdata->min_data = g_new (double, nodes_nb + 1);
	sdata->max_data = g_new (double, nodes_nb + 1);

	// Read the data
	for (i = 0; i < nodes_nb + 1; i++) {
		sdata->min_data[i] = G_MAXDOUBLE;
		sdata->max_data[i] = -G_MAXDOUBLE;
	}
	found = FALSE;
	while ((fgets (buf, 255, priv->inputfp) != NULL) && !found) {
		gint k = 0;
		if (strlen (buf) <= 2) {
			if (priv->status != 0 && strcmp(buf, "\n") == 0) {
				ANALYSIS (sdata)->transient.sim_length = val[0];
				break;
			}
			fgets (buf, 255, priv->inputfp);
			fgets (buf, 255, priv->inputfp);
			fgets (buf, 255, priv->inputfp);
		}
		tmp = &buf[0];
		variables = get_variables (tmp, &k);
		for (i = 0; i < n; i++) {
			val[i] = g_ascii_strtod (variables[i + 1], NULL);
			val_tmp1[i] = g_array_append_val (val_tmp1[i], val[i]);
			if (val[i] < sdata->min_data[i])
				sdata->min_data[i] = val[i];
			if (val[i] > sdata->max_data[i])
				sdata->max_data[i] = val[i];
		}
		if (val[0] >= ANALYSIS (sdata)->transient.sim_length)
			found = TRUE;
	}

	if (n < nodes_nb + 1) {
		fgets (buf, 255, priv->inputfp);
		fgets (buf, 255, priv->inputfp);
		fgets (buf, 255, priv->inputfp);
		fgets (buf, 255, priv->inputfp);
		variables = get_variables (buf, &m);
		m = m - 1;

		for (i = 1; i < m; i++) {
			sdata->var_names[i + n - 1] = g_strdup (variables[i + 1]);
			sdata->var_units[i + n - 1] = g_strdup (_ ("voltage"));
		}
		fgets (buf, 255, priv->inputfp);
		found = FALSE;
		while ((fgets (buf, 255, priv->inputfp) != NULL) && !found) {
			gint k = 0;
			if (strlen (buf) <= 2) {
				fgets (buf, 255, priv->inputfp);
				fgets (buf, 255, priv->inputfp);
				fgets (buf, 255, priv->inputfp);
			}
			tmp = &buf[0];
			variables = get_variables (tmp, &k);
			for (i = 0; i < m; i++) {
				val[i] = g_ascii_strtod (variables[i + 1], NULL);
				val_tmp2[i] = g_array_append_val (val_tmp2[i], val[i]);
				if (val[i] < sdata->min_data[i + n])
					sdata->min_data[i + n - 1] = val[i];
				if (val[i] > sdata->max_data[i + n])
					sdata->max_data[i + n - 1] = val[i];
			}
			if (val[0] >= ANALYSIS (sdata)->transient.sim_length)
				found = TRUE;
		}
		if (n + m - 1 < nodes_nb + 1) {
			fgets (buf, 255, priv->inputfp);
			fgets (buf, 255, priv->inputfp);
			fgets (buf, 255, priv->inputfp);
			fgets (buf, 255, priv->inputfp);
			variables = get_variables (buf, &p);
			p = p - 1;

			for (i = 1; i < p; i++) {
				sdata->var_names[i + n + m - 2] = g_strdup (variables[i + 1]);
				sdata->var_units[i + n + m - 2] = g_strdup (_ ("voltage"));
			}
			fgets (buf, 255, priv->inputfp);
			found = FALSE;
			while ((fgets (buf, 255, priv->inputfp) != NULL) && !found) {
				gint k = 0;
				if (strlen (buf) <= 2) {
					fgets (buf, 255, priv->inputfp);
					fgets (buf, 255, priv->inputfp);
					fgets (buf, 255, priv->inputfp);
				}
				tmp = &buf[0];
				variables = get_variables (tmp, &k);
				if (variables) {
					for (i = 0; i < p; i++) {
						val[i] = g_ascii_strtod (variables[i + 1], NULL);
						val_tmp3[i] = g_array_append_val (val_tmp3[i], val[i]);
						if (val[i] < sdata->min_data[i + n + m - 3])
							sdata->min_data[i + n + m - 3] = val[i];
						if (val[i] > sdata->max_data[i + n + m - 3])
							sdata->max_data[i + n + m - 3] = val[i];
					}
					if (val[0] >= ANALYSIS (sdata)->transient.sim_length)
						found = TRUE;
				}
			}
		}
	}
	found = FALSE;
	while (!found) {
		gdouble val0 = 0.;
		for (i = 0; i < nodes_nb + 1; i++) {
			// 0 = time
			// From node 1 to node 3
			if (i < n) {
				gdouble val = 0;
				val = g_array_index (val_tmp1[i], gdouble, sdata->got_points);
				if (i == 0)
					val0 = val;
				sdata->data[i] = g_array_append_val (sdata->data[i], val);
			}
			// From node 4 to node 6
			else if (i < n + m - 1) {
				gdouble val = 0;
				val = g_array_index (val_tmp2[i - n + 1], gdouble, sdata->got_points);
				sdata->data[i] = g_array_append_val (sdata->data[i], val);
			}
			// From node 7 to node 9
			else {
				gdouble val = 0;
				val = g_array_index (val_tmp3[i - n - m + 2], gdouble, sdata->got_points);
				sdata->data[i] = g_array_append_val (sdata->data[i], val);
			}
		}
		sdata->got_points++;
		sdata->got_var = nodes_nb + 1;
		if (val0 >= ANALYSIS (sdata)->transient.sim_length)
			found = TRUE;
	}
}

void parse_fourier_analysis (NgSpice *ngspice, gchar *tmp)
{
	static SimulationData *sdata;
	static Analysis *data;
	NgSpicePrivate *priv = ngspice->priv;
	SimSettings *sim_settings;
	static gchar buf[256];
	gchar **variables;
	gint i, n = 0, freq, j, k;
	gdouble val[10], mag[10][10];
	gchar **node_ids;
	gchar *vout;

	oregano_echo ("F{}: result str\n>>>\n%s<<<\n", tmp);

	sim_settings = (SimSettings *)schematic_get_sim_settings (priv->schematic);

	// New analysis
	data = g_new0 (Analysis, 1);
	sdata = SIM_DATA (data);
	priv->current = sdata;
	priv->analysis = g_list_append (priv->analysis, sdata);
	sdata->state = IN_VALUES;
	sdata->functions = NULL;
	priv->num_analysis++;
	sdata->type = FOURIER;

	tmp = g_strchug (tmp);
	ANALYSIS (sdata)->fourier.freq = sim_settings_get_fourier_frequency (sim_settings);
	vout = sim_settings_get_fourier_vout (sim_settings);
	node_ids = g_strsplit (g_strdup (vout), " ", 0);
	for (i = 0; node_ids[i] != NULL; i++) {
	}
	ANALYSIS (sdata)->fourier.nb_var = i + 1;
	g_free (vout);

	n = ANALYSIS (sdata)->fourier.nb_var;
	sdata->n_variables = n;
	sdata->var_names = (char **)g_new0 (gpointer, n);
	sdata->var_units = (char **)g_new0 (gpointer, n);
	variables = get_variables (tmp, &k);
	sdata->var_names[0] = _ ("Frequency");
	sdata->var_names[1] = g_strdup (variables[3]);
	for (i = 0; i < n; i++) {
		if (i == 0)
			sdata->var_units[i] = g_strdup (_ ("frequency"));
		else
			sdata->var_units[i] = g_strdup (_ ("voltage"));
	}
	sdata->state = IN_VALUES;
	sdata->got_points = 0;
	sdata->got_var = 0;
	sdata->data = (GArray **)g_new0 (gpointer, n);

	fgets (buf, 255, priv->inputfp);
	fgets (buf, 255, priv->inputfp);
	fgets (buf, 255, priv->inputfp);
	fgets (buf, 255, priv->inputfp);

	for (i = 0; i < n; i++)
		sdata->data[i] = g_array_new (TRUE, TRUE, sizeof(double));

	sdata->min_data = g_new (double, n);
	sdata->max_data = g_new (double, n);

	// Read the data
	for (i = 0; i < n; i++) {
		sdata->min_data[i] = G_MAXDOUBLE;
		;
		sdata->max_data[i] = -G_MAXDOUBLE;
	}

	for (j = 0; j < 10; j++) {
		fgets (buf, 255, priv->inputfp);
		sscanf (buf, "\t%d\t%d\t%lf\t%lf", &i, &freq, &val[0], &val[1]);
		mag[j][0] = (gdouble)freq;
		mag[j][1] = val[0];
	}

	for (j = 2; j < n; j++) {
		fgets (buf, 255, priv->inputfp);
		fgets (buf, 255, priv->inputfp);
		tmp = &buf[0];
		variables = get_variables (tmp, &k);
		sdata->var_names[j] = g_strdup (variables[3]);
		fgets (buf, 255, priv->inputfp);
		fgets (buf, 255, priv->inputfp);
		fgets (buf, 255, priv->inputfp);
		fgets (buf, 255, priv->inputfp);

		for (k = 0; k < 10; k++) {
			fgets (buf, 255, priv->inputfp);
			sscanf (buf, "\t%d\t%d\t%lf\t%lf", &i, &freq, &val[0], &val[1]);
			mag[k][j] = val[0];
		}
	}

	for (i = 0; i < n; i++) {
		sdata->min_data[i] = G_MAXDOUBLE;
		sdata->max_data[i] = -G_MAXDOUBLE;
	}

	for (j = 0; j < 10; j++) {
		for (i = 0; i < n; i++) {
			sdata->data[i] = g_array_append_val (sdata->data[i], mag[j][i]);
			if (mag[j][i] < sdata->min_data[i])
				sdata->min_data[i] = mag[j][i];
			if (mag[j][i] > sdata->max_data[i])
				sdata->max_data[i] = mag[j][i];
			sdata->got_points++;
			sdata->got_var = n;
		}
		oregano_echo ("ngspice-analysis: mag[%d][0]=%lf\tmag[%d][1]=%lf\n", j, mag[j][0], j, mag[j][1]);
	}
	return;
}

void ngspice_parse (NgSpice *ngspice)
{
	NgSpicePrivate *priv = ngspice->priv;
	SimSettings *sim_settings;
	static gchar buf[256];
	gchar *tmp = NULL;
	gboolean found = FALSE;
	gint i, analysis_type = 11;
	gboolean transient_enabled = FALSE;
	gboolean fourier_enabled = FALSE;
	gboolean dc_enabled = FALSE;
	// gboolean ac_enabled = FALSE;

	sim_settings = (SimSettings *)schematic_get_sim_settings (priv->schematic);

	transient_enabled = sim_settings_get_trans (sim_settings);
	fourier_enabled = sim_settings_get_fourier (sim_settings);
	// ac_enabled = sim_settings_get_ac (sim_settings);
	dc_enabled = sim_settings_get_dc (sim_settings);

	priv->inputfp = fopen ("/tmp/netlist.lst", "r");

	fgets (buf, 255, priv->inputfp);
	fgets (buf, 255, priv->inputfp);
	while ((fgets (buf, 255, priv->inputfp) != NULL) && !found) {
		if (g_str_has_suffix (g_strdup (buf), SP_TITLE)) {
			found = TRUE;
		}
	}
	tmp = &buf[0];
	tmp = g_strchug (tmp);

	for (i = 0; analysis_names[i]; i++) {
		if (g_str_has_prefix (g_strdup (tmp), analysis_names[i])) {
			analysis_type = i;
		}
	}
	if (!analysis_names[analysis_type]) {
		oregano_warning (_ ("No analysis found"));
		return;
	}

	if (transient_enabled) {
		if (g_str_has_prefix (analysis_names[analysis_type], "Transient Analysis")) {
			parse_transient_analysis (ngspice, buf);
		} else {
			oregano_warning (_ ("Transient analysis expected but not found"));
		}
	}

	fgets (buf, 255, priv->inputfp);
	oregano_echo ("1 buf = %s\n", buf);
	tmp = &buf[0];
	tmp = g_strchug (tmp);

	analysis_type = ANALYSIS_UNKNOWN;
	for (i = 0; analysis_names[i]; i++) {
		if (g_str_has_prefix (g_strdup (tmp), analysis_names[i])) {
			analysis_type = i;
		}
	}
	if (fourier_enabled) {
		if (g_str_has_prefix (analysis_names[analysis_type], "Fourier analysis")) {
			parse_fourier_analysis (ngspice, buf);
		} else {
			oregano_warning (_ ("Fourier analysis expected but not found"));
		}
	}
	fgets (buf, 255, priv->inputfp);
	oregano_echo ("2 buf = %s\n", buf);
	tmp = &buf[0];
	tmp = g_strchug (tmp);

	analysis_type = ANALYSIS_UNKNOWN;
	for (i = 0; analysis_names[i]; i++) {
		if (g_str_has_prefix (g_strdup (tmp), analysis_names[i])) {
			analysis_type = i;
		}
	}
	if (dc_enabled) {
		if (g_str_has_prefix (analysis_names[analysis_type], "DC transfer characteristic")) {
			parse_dc_analysis (ngspice, buf);
		} else {
			oregano_warning (_ ("DC Sweep expected but not found"));
		}
	}

	fclose (priv->inputfp);
}
