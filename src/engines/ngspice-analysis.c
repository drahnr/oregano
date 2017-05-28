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

static gchar *analysis_names[] = {
    "Operating Point",  "Transient Analysis", "DC Transfer Characteristic",
    "AC Analysis",      "Transfer Function",  "Distortion Analysis",
    "Noise Analysis",   "Pole-Zero Analysis", "Sensitivity Analysis",
    "Fourier Analysis", "Unknown Analysis",   NULL};

#define SP_TITLE "oregano\n"
#define CPU_TIME "CPU time since last call:"

#define TAGS_COUNT (sizeof(analysis_tags) / sizeof(struct analysis_tag))
#include "debug.h"
#define IS_THIS_ITEM(str, item) (!strncmp (str, item, strlen (item)))
#define DEBUG_THIS 1

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

void parse_dc_analysis (OreganoNgSpice *ngspice, gchar *tmp)
{
	static SimulationData *sdata;
	static Analysis *data;
	OreganoNgSpicePriv *priv = ngspice->priv;
	SimSettings *sim_settings;
	static gchar buf[256];
	gboolean found = FALSE;
	gchar **variables;
	gint i, n = 0, index = 0;
	gdouble val[10];
	gdouble np1;

	NG_DEBUG ("DC: result str\n>>>\n%s<<<\n", tmp);

	sim_settings = (SimSettings *)schematic_get_sim_settings (priv->schematic);
	data = g_new0 (Analysis, 1);
	priv->current = sdata = SIM_DATA (data);
	priv->analysis = g_list_append (priv->analysis, sdata);
	priv->num_analysis = 1;
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

typedef struct {
	GString *name;
	GString *unit;
	GArray *data;
	gdouble min;
	gdouble max;
} NgspiceColumn;

typedef struct {
	GPtrArray *ngspice_columns;
	GPtrArray *current_variables;
} NgspiceTable;

static NgspiceColumn *ngspice_column_new(const gchar *name, guint predicted_size) {
	NgspiceColumn *ret_val = malloc(sizeof(NgspiceColumn));

	ret_val->name = g_string_new(name);

	if (g_str_has_prefix(name, "Index"))
		ret_val->unit = g_string_new("none");
	else if (g_str_has_prefix(name, "time"))
		ret_val->unit = g_string_new("time");
	else if (g_str_has_prefix(name, "V") || g_str_has_prefix(name, "v"))
		ret_val->unit = g_string_new("voltage");
	else if (g_str_has_suffix(name, "#branch"))
		ret_val->unit = g_string_new("current");
	else
		ret_val->unit = g_string_new("unknown");

	ret_val->data = g_array_sized_new(TRUE, TRUE, sizeof(gdouble), predicted_size);
	ret_val->min = G_MAXDOUBLE;
	ret_val->max = -G_MAXDOUBLE;

	return ret_val;
}

static void asdf(gpointer data) {
	g_free(data);
}

static void jkloe(gpointer ptr) {
	NgspiceColumn *column = (NgspiceColumn *)ptr;
	if (column == NULL)
		return;
	if (column->name != NULL)
		g_string_free(column->name, TRUE);
	if (column->unit != NULL)
		g_string_free(column->unit, TRUE);
	g_free(column);
}

static NgspiceTable *ngspice_table_new() {
	NgspiceTable *ret_val = malloc(sizeof(NgspiceTable));
	ret_val->ngspice_columns = g_ptr_array_new_with_free_func(jkloe);
	ret_val->current_variables = g_ptr_array_new_with_free_func(asdf);
	return ret_val;
}

static void ngspice_table_destroy(NgspiceTable *ngspice_table) {
	if (ngspice_table->current_variables != NULL)
		g_ptr_array_free(ngspice_table->current_variables, TRUE);
	if (ngspice_table->ngspice_columns != NULL)
		g_ptr_array_free(ngspice_table->ngspice_columns, TRUE);
	g_free(ngspice_table);
}

static void convert_variable_name(gchar **variable) {
	gchar **splitted = g_regex_split_simple("\\#branch", *variable, 0, 0);
	g_free(*variable);
	*variable = g_strdup_printf("I(%s)", *splitted);
	g_strfreev(splitted);
}

static void ngspice_table_new_columns(NgspiceTable *ngspice_table, gchar **variables) {
	guint predicted_size = 0;
	if (ngspice_table->ngspice_columns->len > 0) {
		NgspiceColumn *column = (NgspiceColumn *)ngspice_table->ngspice_columns->pdata[0];
		predicted_size = column->data->len;
	}

	g_ptr_array_free(ngspice_table->current_variables, TRUE);
	ngspice_table->current_variables = g_ptr_array_new_with_free_func(asdf);

	for (gchar **variable = variables; *variable != NULL && **variable != '\n' && **variable != 0; variable++) {
		if (g_str_has_suffix(*variable, "#branch"))
			convert_variable_name(variable);
		int i;
		for (i = 0; i < ngspice_table->ngspice_columns->len; i++) {
			if (!strcmp(*variable, ((NgspiceColumn *)ngspice_table->ngspice_columns->pdata[i])->name->str))
				break;
		}
		if (i == ngspice_table->ngspice_columns->len) {
			NgspiceColumn *new_column = ngspice_column_new(*variable, predicted_size);
			g_ptr_array_add(ngspice_table->ngspice_columns, new_column);
		}
		int *new_int = malloc(sizeof(int));
		*new_int = i;
		g_ptr_array_add(ngspice_table->current_variables, new_int);
	}
}

static void ngspice_table_add_data(NgspiceTable *ngspice_table, gchar **data) {
	g_return_if_fail(data != NULL);
	g_return_if_fail(*data != NULL);
	if (**data == 0)
		return;
	g_return_if_fail(ngspice_table != NULL);
	guint64 index = g_ascii_strtoull(*data, NULL, 10);
	for (int i = 0; data[i] != NULL && data[i][0] != 0 && data[i][0] != '\n' && i < ngspice_table->current_variables->len; i++) {
		int column_index = *((int *)(ngspice_table->current_variables->pdata[i]));
		NgspiceColumn *column = (NgspiceColumn*)(ngspice_table->ngspice_columns->pdata[column_index]);
		if (column->data->len > index) {
			continue;//assert equal
		}
		gdouble new_content = g_ascii_strtod(data[i], NULL);
		g_array_append_val(column->data, new_content);
		if (new_content < column->min)
			column->min = new_content;
		if (new_content > column->max)
			column->max = new_content;
	}
}

void parse_transient_analysis (OreganoNgSpice *ngspice, gchar *tmp)
{
	enum STATE {
		NGSPICE_ANALYSIS_STATE_READ_DATA,
		NGSPICE_ANALYSIS_STATE_DATA_SMALL_BLOCK_END,
		NGSPICE_ANALYSIS_STATE_DATA_LARGE_BLOCK_END,
		NGSPICE_ANALYSIS_STATE_DATA_END,
		NGSPICE_ANALYSIS_STATE_READ_VARIABLES_NEW,
		NGSPICE_ANALYSIS_STATE_READ_VARIABLES_OLD
	};
	static gchar buf[256];

	memcpy(buf, tmp, (strlen(tmp) + 1) * sizeof(char));
	NgspiceTable *ngspice_table = ngspice_table_new();

	enum STATE state = NGSPICE_ANALYSIS_STATE_DATA_LARGE_BLOCK_END;

	do {
		switch (state) {
			case NGSPICE_ANALYSIS_STATE_READ_VARIABLES_NEW:
			{
				gchar **splitted_line = g_regex_split_simple(" +", buf, 0, 0);

				ngspice_table_new_columns(ngspice_table, splitted_line);

				g_strfreev(splitted_line);

				state = NGSPICE_ANALYSIS_STATE_READ_VARIABLES_OLD;
				break;
			}
			case NGSPICE_ANALYSIS_STATE_READ_DATA:
			{
				gchar **splitted_line = g_regex_split_simple("\\t+|-{2,}", buf, 0, 0);

				ngspice_table_add_data(ngspice_table, splitted_line);

				g_strfreev(splitted_line);
				fgets(buf, 255, ngspice->priv->inputfp);
				switch (buf[0]) {
				case '\f':
					state = NGSPICE_ANALYSIS_STATE_DATA_SMALL_BLOCK_END;
					break;
				case '\n':
					state = NGSPICE_ANALYSIS_STATE_DATA_END;
					break;
				}
				break;
			}
			case NGSPICE_ANALYSIS_STATE_READ_VARIABLES_OLD:
			{
				fgets(buf, 255, ngspice->priv->inputfp);
				if (buf[0] != '-')
					state = NGSPICE_ANALYSIS_STATE_READ_DATA;
				break;
			}
			case NGSPICE_ANALYSIS_STATE_DATA_SMALL_BLOCK_END:
			{
				fgets(buf, 255, ngspice->priv->inputfp);
				switch (buf[0]) {
				case 'I':
					state = NGSPICE_ANALYSIS_STATE_READ_VARIABLES_OLD;
					break;
				case ' ':
					state = NGSPICE_ANALYSIS_STATE_DATA_LARGE_BLOCK_END;
					break;
				}
				break;
			}
			case NGSPICE_ANALYSIS_STATE_DATA_LARGE_BLOCK_END:
			{
				fgets(buf, 255, ngspice->priv->inputfp);
				if (buf[0] == 'I')
					state = NGSPICE_ANALYSIS_STATE_READ_VARIABLES_NEW;
				break;
			}
			case NGSPICE_ANALYSIS_STATE_DATA_END:
				break;
		}
	} while (state != NGSPICE_ANALYSIS_STATE_DATA_END);


	OreganoNgSpicePriv *priv = ngspice->priv;
	SimSettings *sim_settings;
	SimulationData *sdata;
	Analysis *data;
	gint nodes_nb = ngspice_table->ngspice_columns->len - 2;

	NG_DEBUG ("TRANSIENT: result str\n>>>\n%s<<<\n", tmp);

	sim_settings = (SimSettings *)schematic_get_sim_settings (priv->schematic);
	data = g_new0 (Analysis, 1);
	priv->current = sdata = SIM_DATA (data);
	priv->analysis = g_list_append (priv->analysis, sdata);
	priv->num_analysis = 1;
	sdata->type = ANALYSIS_UNKNOWN;
	sdata->functions = NULL;

	g_strchug (tmp);
	for (int i = 0; analysis_names[i]; i++)
		if (g_str_has_prefix (tmp, analysis_names[i]))
			sdata->type = i;

	ANALYSIS (sdata)->transient.sim_length =
	    sim_settings_get_trans_stop (sim_settings) - sim_settings_get_trans_start (sim_settings);
	ANALYSIS (sdata)->transient.step_size = sim_settings_get_trans_step (sim_settings);

	sdata->var_names = g_new0 (gchar *, nodes_nb + 1);
	sdata->var_units = g_new0 (gchar *, nodes_nb + 1);
	sdata->data = g_new0 (GArray *, nodes_nb + 1);
	sdata->min_data = g_new (gdouble, nodes_nb + 1);
	sdata->max_data = g_new (gdouble, nodes_nb + 1);

	for (int i = 0; i < nodes_nb + 1; i++) {
		NgspiceColumn *column = ngspice_table->ngspice_columns->pdata[i+1];
		sdata->var_names[i] = g_strdup(column->name->str);
		sdata->var_units[i] = g_strdup(column->unit->str);
		sdata->data[i] = column->data;
		sdata->min_data[i] = column->min;
		sdata->max_data[i] = column->max;
	}
	sdata->n_variables = nodes_nb + 1;
	NgspiceColumn *column = ngspice_table->ngspice_columns->pdata[0];
	sdata->got_points = column->data->len;
	sdata->got_var = nodes_nb + 1;

	g_array_free(column->data, TRUE);
	ngspice_table_destroy(ngspice_table);
}

void parse_fourier_analysis (OreganoNgSpice *ngspice, gchar *tmp)
{
	static SimulationData *sdata;
	static Analysis *data;
	OreganoNgSpicePriv *priv = ngspice->priv;
	SimSettings *sim_settings;
	static gchar buf[256];
	gchar **variables;
	gint i, n = 0, freq, j, k;
	gdouble val[10], mag[10][10];
	gchar **node_ids;
	gchar *vout;

	NG_DEBUG ("F{}: result str\n>>>\n%s<<<\n", tmp);

	sim_settings = (SimSettings *)schematic_get_sim_settings (priv->schematic);

	// New analysis
	data = g_new0 (Analysis, 1);
	sdata = SIM_DATA (data);
	priv->current = sdata;
	priv->analysis = g_list_append (priv->analysis, sdata);
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
		NG_DEBUG ("ngspice-analysis: mag[%d][0]=%lf\tmag[%d][1]=%lf\n", j, mag[j][0], j, mag[j][1]);
	}
	return;
}

void ngspice_parse (OreganoNgSpice *ngspice)
{
	OreganoNgSpicePriv *priv = ngspice->priv;
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
		if (g_str_has_suffix (buf, SP_TITLE)) {
			found = TRUE;
		}
	}
	tmp = &buf[0];
	tmp = g_strchug (tmp);

	for (i = 0; analysis_names[i]; i++) {
		if (g_str_has_prefix (tmp, analysis_names[i])) {
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
	NG_DEBUG ("1 buf = %s\n", buf);
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
	NG_DEBUG ("2 buf = %s\n", buf);
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
