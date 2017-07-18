/*
 * ngspice-analysis.c
 *
 * Authors:
 *  Marc Lorber <Lorber.Marc@wanadoo.fr>
 *  Bernhard Schuster <bernhard@ahoi.io>
 *  Guido Trentalancia <guido@trentalancia.com>
 *
 * Web page: https://ahoi.io/project/oregano
 *
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
#include "../tools/thread-pipe.h"
#include "../tools/cancel-info.h"

#define SP_TITLE "oregano\n"
#define CPU_TIME "CPU time since last call:"

#define TAGS_COUNT (sizeof(analysis_tags) / sizeof(struct analysis_tag))
#include "debug.h"
#define IS_THIS_ITEM(str, item) (!strncmp (str, item, strlen (item)))
#ifdef DEBUG_THIS
#undef DEBUG_THIS
#endif
#define DEBUG_THIS 1

/**
 * \brief extract the resulting variables from ngspice output
 *
 * In ngspice a number can terminate only with a space,
 * while in spice3 a number can also terminate with a
 * comma.
 *
 * In the Fourier analysis the name of the output ends
 * with a colon.
 *
 * Tested function.
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
		if (isspace (*end) || *end == ',' || *end == ':') {
			// number ended, designate as such and replace the string
			tmp[i] = g_strndup (start, (gsize)(end - start));
			i++;
			start = end;
			while (isspace (*start) || *start == ',' || *start == ':')
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

/**
 * @resources: caller frees
 */
static ThreadPipe *parse_dc_analysis (NgspiceAnalysisResources *resources)
{
	ThreadPipe *pipe = resources->pipe;
	gchar **buf = &resources->buf;
	const SimSettings* const sim_settings = resources->sim_settings;
	GList **analysis = resources->analysis;
	guint *num_analysis = resources->num_analysis;

	static SimulationData *sdata;
	static Analysis *data;
	gsize size;
	gboolean found = FALSE;
	gchar **variables;
	gint i, n = 0, index = 0;
	gdouble val[10];
	gdouble np1;

	NG_DEBUG ("DC: result str\n>>>\n%s\n<<<", *buf);

	data = g_new0 (Analysis, 1);
	sdata = SIM_DATA (data);
	sdata->type = ANALYSIS_TYPE_DC_TRANSFER;
	sdata->functions = NULL;

	np1 = 1.;
	ANALYSIS (sdata)->dc.start = sim_settings_get_dc_start (sim_settings);
	ANALYSIS (sdata)->dc.stop = sim_settings_get_dc_stop (sim_settings);
	ANALYSIS (sdata)->dc.step = sim_settings_get_dc_step (sim_settings);

	np1 = (ANALYSIS (sdata)->dc.stop - ANALYSIS (sdata)->dc.start) / ANALYSIS (sdata)->dc.step;
	ANALYSIS (sdata)->dc.sim_length = np1;

	pipe = thread_pipe_pop(pipe, (gpointer *)buf, &size);
	pipe = thread_pipe_pop(pipe, (gpointer *)buf, &size);

	// Calculates the number of variables
	variables = get_variables (*buf, &n);
	if (!variables)
		return pipe;

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

	pipe = thread_pipe_pop(pipe, (gpointer *)buf, &size);

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
	while (((pipe = thread_pipe_pop(pipe, (gpointer *)buf, &size)) != 0) && !found) {
		if (strlen (*buf) <= 2) {
			pipe = thread_pipe_pop(pipe, (gpointer *)buf, &size);
			pipe = thread_pipe_pop(pipe, (gpointer *)buf, &size);
			pipe = thread_pipe_pop(pipe, (gpointer *)buf, &size);
		}
		variables = get_variables (*buf, &i);
		if (!variables)
			return pipe;
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

	*analysis = g_list_append (*analysis, sdata);
	(*num_analysis)++;

	return pipe;
}

/**
 * @resources: caller frees
 */
static ThreadPipe *parse_ac_analysis (NgspiceAnalysisResources *resources)
{
	ThreadPipe *pipe = resources->pipe;
	gboolean is_vanilla = resources->is_vanilla;
	gchar *scale, **variables, **buf = &resources->buf;
	const SimSettings* const sim_settings = resources->sim_settings;
	GList **analysis = resources->analysis;
	guint *num_analysis = resources->num_analysis;
	static SimulationData *sdata;
	static Analysis *data;
	gsize size;
	gboolean found = FALSE;
	gint i, n = 0, index = 0;
	gdouble fstart, fstop, val[10];

	NG_DEBUG ("AC: result str\n>>>\n%s\n<<<", *buf);

	data = g_new0 (Analysis, 1);
	sdata = SIM_DATA (data);
	sdata->type = ANALYSIS_TYPE_AC;
	sdata->functions = NULL;

	ANALYSIS (sdata)->ac.sim_length = 1.;

	ANALYSIS (sdata)->ac.start = fstart = sim_settings_get_ac_start (sim_settings);
	ANALYSIS (sdata)->ac.stop = fstop = sim_settings_get_ac_stop (sim_settings);

        scale = sim_settings_get_ac_type (sim_settings);
        if (!g_ascii_strcasecmp (scale, "LIN")) {
                ANALYSIS (sdata)->ac.sim_length = (double) sim_settings_get_ac_npoints (sim_settings);
        } else if (!g_ascii_strcasecmp (scale, "DEC")) {
                ANALYSIS (sdata)->ac.sim_length = (double) sim_settings_get_ac_npoints (sim_settings) * log10 (fstop / fstart);
        } else if (!g_ascii_strcasecmp (scale, "OCT")) {
                ANALYSIS (sdata)->ac.sim_length = (double) sim_settings_get_ac_npoints (sim_settings) * log10 (fstop / fstart) / log10 (2);
        }

	pipe = thread_pipe_pop(pipe, (gpointer *)buf, &size);
	pipe = thread_pipe_pop(pipe, (gpointer *)buf, &size);

	// Calculates the number of variables
	variables = get_variables (*buf, &n);
	if (!variables)
		return pipe;

	n = n - 1;
	sdata->var_names = (char **)g_new0 (gpointer, n);
	sdata->var_units = (char **)g_new0 (gpointer, n);
	sdata->var_names[0] = g_strdup ("Frequency");
	sdata->var_units[0] = g_strdup (_ ("frequency"));
	sdata->var_names[1] = g_strdup (variables[2]);
	sdata->var_units[1] = g_strdup (_ ("voltage"));

	sdata->n_variables = 2;
	sdata->got_points = 0;
	sdata->got_var = 0;
	sdata->data = (GArray **)g_new0 (gpointer, 2);

	pipe = thread_pipe_pop(pipe, (gpointer *)buf, &size);

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
	while (((pipe = thread_pipe_pop(pipe, (gpointer *)buf, &size)) != 0) && !found) {
		if (strlen (*buf) <= 2) {
			pipe = thread_pipe_pop(pipe, (gpointer *)buf, &size);
			pipe = thread_pipe_pop(pipe, (gpointer *)buf, &size);
			pipe = thread_pipe_pop(pipe, (gpointer *)buf, &size);
		}

		variables = get_variables (*buf, &i);
		if (!variables)
			return pipe;

		index = atoi (variables[0]);
		for (i = 0; i < n; i++) {
			if (i == 0)
				val[i] = g_ascii_strtod (variables[i + 1], NULL);
			if (is_vanilla && i > 0)
				val[i] = g_ascii_strtod (variables[i + 2], NULL);
			else
				val[i] = g_ascii_strtod (variables[i + 1], NULL);
			sdata->data[i] = g_array_append_val (sdata->data[i], val[i]);
			if (val[i] < sdata->min_data[i])
				sdata->min_data[i] = val[i];
			if (val[i] > sdata->max_data[i])
				sdata->max_data[i] = val[i];
		}
		sdata->got_points++;
		sdata->got_var = 2;
		if (index >= ANALYSIS (sdata)->ac.sim_length - 1)
			found = TRUE;
	}

	*analysis = g_list_append (*analysis, sdata);
	(*num_analysis)++;

	return pipe;
}

/**
 * Structure that helps parsing the output of ngspice.
 *
 * @name: The name (handled like an ID) of the column.
 * @unit: The physical unit. it can have one of the following values:
 * "none", "time", "voltage", "current", "unknown"
 * @data: The data.
 * @min: The min value of the data.
 * @max: The max value of the data.
 */
typedef struct {
	GString *name;
	GString *unit;
	GArray *data;
	gdouble min;
	gdouble max;
} NgspiceColumn;

/**
 * Structure that helps parsing the output of ngspice.
 *
 * @ngspice_columns: The columns.
 * @current_variables: New columns are appended to the end
 * of the columns array. Because the index- and time-column
 * are displayed repeatedly with every 3 new columns, there
 * is needed an information how to interpret the incoming
 * data to append it to the right columns. The current_variables
 * array maps the position of the input data in the file
 * to the position of the related column position in the table.
 */
typedef struct {
	GPtrArray *ngspice_columns;
	GArray *current_variables;
} NgspiceTable;

/**
 * Allocates memory.
 *
 * @name: The name (ID) of the new column
 * @predicted_size: The predicted end size of the new column.
 */
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

/**
 * Frees memory.
 */
static void ngspice_column_destroy(gpointer ptr) {
	NgspiceColumn *column = (NgspiceColumn *)ptr;
	if (column == NULL)
		return;
	if (column->name != NULL)
		g_string_free(column->name, TRUE);
	if (column->unit != NULL)
		g_string_free(column->unit, TRUE);
	g_free(column);
}

/**
 * Allocates memory.
 */
static NgspiceTable *ngspice_table_new() {
	NgspiceTable *ret_val = malloc(sizeof(NgspiceTable));
	ret_val->ngspice_columns = g_ptr_array_new_with_free_func(ngspice_column_destroy);
	ret_val->current_variables = g_array_new(TRUE, TRUE, sizeof(guint));
	return ret_val;
}

/**
 * Frees the memory.
 */
static void ngspice_table_destroy(NgspiceTable *ngspice_table) {

	if (ngspice_table->ngspice_columns != NULL) {
		guint len = ngspice_table->ngspice_columns->len;

		for (int i = 0; i < len; i++) {
			NgspiceColumn *column = ngspice_table->ngspice_columns->pdata[i];
			if (column != NULL)
				g_array_free(column->data, TRUE);
		}

		g_ptr_array_free(ngspice_table->ngspice_columns, TRUE);
	}

	if (ngspice_table->current_variables != NULL)
		g_array_free(ngspice_table->current_variables, TRUE);

	g_free(ngspice_table);
}

/**
 * Converts "x...x#branch" to "I(x...x)".
 *
 * Let "x...x" be the name of a part in the ngspice netlist,
 * then the current variable (current, that flows through
 * that part) of that part is named
 * "x...x#branch" in ngspice. To shorten the name and make
 * it prettier, the name will be displayed as "I(x...x)"
 * in Oregano (analogous to V(node_nr)).
 */
static void convert_variable_name(gchar **variable) {
	gchar **splitted = g_regex_split_simple("\\#branch", *variable, 0, 0);
	g_free(*variable);
	*variable = g_strdup_printf("I(%s)", *splitted);
	g_strfreev(splitted);
}

/**
 * Creates and appends new columns to the table, if there are new variables.
 * The referenced columns are now the newly added columns.
 *
 * @predicted_size: The predicted end size of the new columns. It is assumed
 * that the new columns will have the same size.
 */
static void ngspice_table_new_columns(NgspiceTable *ngspice_table, gchar **variables, guint predicted_size) {
	if (ngspice_table->ngspice_columns->len > 0) {
		NgspiceColumn *column = (NgspiceColumn *)ngspice_table->ngspice_columns->pdata[0];
		predicted_size = column->data->len;
	}

	g_array_free(ngspice_table->current_variables, TRUE);
	ngspice_table->current_variables = g_array_new(TRUE, TRUE, sizeof(guint));

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
		g_array_append_val(ngspice_table->current_variables, i);
	}
}

/**
 * Appends a split line to the end of the current referenced columns.
 */
static void ngspice_table_add_data(NgspiceTable *ngspice_table, gchar **data) {
	g_return_if_fail(data != NULL);
	g_return_if_fail(*data != NULL);
	if (**data == 0)
		return;
	g_return_if_fail(ngspice_table != NULL);

	guint64 index = g_ascii_strtoull(*data, NULL, 10);
	for (int i = 0; data[i] != NULL && data[i][0] != 0 && data[i][0] != '\n' && i < ngspice_table->current_variables->len; i++) {
		guint column_index = g_array_index(ngspice_table->current_variables, guint, i);
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

/**
 * Counts the number of different guints. If a certain
 * guint occurs more than once, it is counted as one.
 */
static guint get_real_len(GArray *array_guint) {
	guint ret_val = 0;
	for (guint i = 0; i < array_guint->len; i++) {
		ret_val++;
		for (guint j = 0; j < i; j++) {
			if (g_array_index(array_guint, guint, i) == g_array_index(array_guint, guint, j)) {
				ret_val--;
				break;
			}
		}
	}

	return ret_val;
}

/**
 * Returns the index that is currently parsed.
 *
 * The first referenced column is the string-index column,
 * the second referenced column is the time. The index and
 * the time column can already be filled up by earlier
 * iterations, so the length of the third referenced column
 * represents the index that is currently parsed.
 */
static guint get_current_index(NgspiceTable *ngspice_table) {
	guint column_nr = g_array_index(ngspice_table->current_variables, guint, 2);
	NgspiceColumn *column = g_ptr_array_index(ngspice_table->ngspice_columns, column_nr);
	return column->data->len;
}

typedef struct {
	NgspiceTable *table;
	ThreadPipe *pipe;
	gboolean is_cancel;
} ParseTransientAnalysisReturnResources;

/**
 * @resources: caller frees
 */
static ParseTransientAnalysisReturnResources parse_transient_analysis_resources (NgspiceAnalysisResources *resources)
{
	gchar **buf = &resources->buf;
	const SimSettings *sim_settings = resources->sim_settings;
	guint *num_analysis = resources->num_analysis;
	ProgressResources *progress_reader = resources->progress_reader;
	guint64 no_of_data_rows = resources->no_of_data_rows_transient;
	guint no_of_variables = resources->no_of_variables;


	enum STATE {
		NGSPICE_ANALYSIS_STATE_READ_DATA,
		NGSPICE_ANALYSIS_STATE_DATA_SMALL_BLOCK_END,
		NGSPICE_ANALYSIS_STATE_DATA_LARGE_BLOCK_END,
		NGSPICE_ANALYSIS_STATE_DATA_END,
		NGSPICE_ANALYSIS_STATE_READ_VARIABLES_NEW,
		NGSPICE_ANALYSIS_STATE_READ_VARIABLES_OLD
	};

	g_mutex_lock(&progress_reader->progress_mutex);
	progress_reader->progress = 0;
	progress_reader->time = g_get_monotonic_time();
	g_mutex_unlock(&progress_reader->progress_mutex);

	gsize size;

	NgspiceTable *ngspice_table = ngspice_table_new();

	ParseTransientAnalysisReturnResources ret_val;
	ret_val.table = ngspice_table;
	ret_val.pipe = resources->pipe;
	ret_val.is_cancel = TRUE;

	enum STATE state = NGSPICE_ANALYSIS_STATE_DATA_LARGE_BLOCK_END;

	guint i = 0;

	do {
		if (i % 50 == 0 && cancel_info_is_cancel(resources->cancel_info))
			return ret_val;

		switch (state) {
			case NGSPICE_ANALYSIS_STATE_READ_VARIABLES_NEW:
			{
				gchar **splitted_line = g_regex_split_simple(" +", *buf, 0, 0);

				ngspice_table_new_columns(ngspice_table, splitted_line, no_of_data_rows);

				g_strfreev(splitted_line);

				state = NGSPICE_ANALYSIS_STATE_READ_VARIABLES_OLD;
				break;
			}
			case NGSPICE_ANALYSIS_STATE_READ_DATA:
			{
				gchar **splitted_line = g_regex_split_simple("\\t+|-{2,}", *buf, 0, 0);

				ngspice_table_add_data(ngspice_table, splitted_line);

				g_strfreev(splitted_line);

				if ((ret_val.pipe = thread_pipe_pop(ret_val.pipe, (gpointer *)buf, &size)) == NULL)
					return ret_val;

				switch (*buf[0]) {
				case '\f':{
					// estimate progress begin
					guint len_of_current_variables = get_real_len(ngspice_table->current_variables) - 2;
					guint len_of_current_columns = ngspice_table->ngspice_columns->len - 2;
					guint count_of_variables_already_finished = len_of_current_columns - len_of_current_variables;
					g_mutex_lock(&progress_reader->progress_mutex);
					progress_reader->progress = (double)count_of_variables_already_finished / (double)no_of_variables +
							(double)len_of_current_variables / (double)no_of_variables *
							(double)get_current_index(ngspice_table) / (double)no_of_data_rows;
					progress_reader->time = g_get_monotonic_time();
					g_mutex_unlock(&progress_reader->progress_mutex);
					// estimate progress end

					state = NGSPICE_ANALYSIS_STATE_DATA_SMALL_BLOCK_END;
					break;}
				case '\n':
					state = NGSPICE_ANALYSIS_STATE_DATA_END;
					break;
				}
				break;
			}
			case NGSPICE_ANALYSIS_STATE_READ_VARIABLES_OLD:
			{
				if ((ret_val.pipe = thread_pipe_pop(ret_val.pipe, (gpointer *)buf, &size)) == NULL)
					return ret_val;

				if (*buf[0] != '-')
					state = NGSPICE_ANALYSIS_STATE_READ_DATA;
				break;
			}
			case NGSPICE_ANALYSIS_STATE_DATA_SMALL_BLOCK_END:
			{
				if ((ret_val.pipe = thread_pipe_pop(ret_val.pipe, (gpointer *)buf, &size)) == NULL)
					return ret_val;

				switch (*buf[0]) {
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
				if ((ret_val.pipe = thread_pipe_pop(ret_val.pipe, (gpointer *)buf, &size)) == NULL)
					return ret_val;

				if (*buf[0] == 'I')
					state = NGSPICE_ANALYSIS_STATE_READ_VARIABLES_NEW;
				break;
			}
			case NGSPICE_ANALYSIS_STATE_DATA_END:
				break;
		}
	} while (state != NGSPICE_ANALYSIS_STATE_DATA_END);

	ret_val.is_cancel = FALSE;

	SimulationData *sdata = SIM_DATA (g_new0 (Analysis, 1));
	sdata->type = ANALYSIS_TYPE_TRANSIENT;
	sdata->functions = NULL;

	gint nodes_nb = ngspice_table->ngspice_columns->len - 2;

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

		ngspice_table->ngspice_columns->pdata[i+1] = NULL;
	}
	sdata->n_variables = nodes_nb + 1;
	NgspiceColumn *column = ngspice_table->ngspice_columns->pdata[0];
	sdata->got_points = column->data->len;
	sdata->got_var = nodes_nb + 1;

	(*num_analysis)++;
	*resources->analysis = g_list_append (*resources->analysis, sdata);

	return ret_val;
}

static ThreadPipe *parse_transient_analysis (NgspiceAnalysisResources *resources) {
	ParseTransientAnalysisReturnResources ret_res = parse_transient_analysis_resources(resources);

	ngspice_table_destroy(ret_res.table);

	if (ret_res.is_cancel && ret_res.pipe) {
		thread_pipe_set_read_eof(ret_res.pipe);
		ret_res.pipe = NULL;
	}

	return ret_res.pipe;
}

/**
 * @resources: caller frees
 */
static ThreadPipe *parse_fourier_analysis (NgspiceAnalysisResources *resources)
{
	ThreadPipe *pipe = resources->pipe;
	gchar **buf = &resources->buf;
	const SimSettings *sim_settings = resources->sim_settings;
	GList **analysis = resources->analysis;
	guint *num_analysis = resources->num_analysis;

	static SimulationData *sdata;
	static Analysis *data;
	gsize size;
	gchar **variables;
	gint i, n = 0, j, k;
	gdouble val[3];
	gchar **node_ids;
	gchar *vout;

	NG_DEBUG ("FOURIER: result str\n>>>\n%s\n<<<", *buf);

	data = g_new0 (Analysis, 1);
	sdata = SIM_DATA (data);
	sdata->type = ANALYSIS_TYPE_FOURIER;
	sdata->functions = NULL;

	g_strchug (*buf);
	ANALYSIS (sdata)->fourier.freq = sim_settings_get_fourier_frequency (sim_settings);

	vout = sim_settings_get_fourier_vout (sim_settings);
	node_ids = g_strsplit (vout, " ", 0);
	for (i = 0; node_ids[i] != NULL; i++) {
	}
	g_strfreev (node_ids);
	g_free (vout);
	ANALYSIS (sdata)->fourier.nb_var = i + 1;
	n = ANALYSIS (sdata)->fourier.nb_var;
	sdata->n_variables = n;

	sdata->var_names = (char **)g_new0 (gpointer, n);
	sdata->var_units = (char **)g_new0 (gpointer, n);
	sdata->var_names[0] = g_strdup ("Frequency");
	sdata->var_units[0] = g_strdup (_ ("frequency"));

	sdata->got_points = 0;
	sdata->got_var = 0;

	sdata->data = (GArray **)g_new0 (gpointer, n);
	for (i = 0; i < n; i++)
		sdata->data[i] = g_array_new (TRUE, TRUE, sizeof(double));

	sdata->min_data = g_new (double, n);
	sdata->max_data = g_new (double, n);

	for (i = 0; i < n; i++) {
		sdata->min_data[i] = G_MAXDOUBLE;
		sdata->max_data[i] = -G_MAXDOUBLE;
	}

	// For each output voltage (plus the frequency for the x-axis),
	// scan its data set
	for (k = 1; k < n; k++) {
		variables = get_variables (*buf, &i);
		if (!variables)
			return pipe;

		// Skip data set header (4 lines)
		for (i = 0; i < 4; i++)
			pipe = thread_pipe_pop(pipe, (gpointer *)buf, &size);

		sdata->var_names[k] = g_strdup_printf ("mag(%s)", variables[3]);
		sdata->var_units[k] = g_strdup (_ ("voltage"));

		// Scan data set for 10 harmonics
		for (j = 0; j < 10; j++) {
			pipe = thread_pipe_pop(pipe, (gpointer *)buf, &size);
			if (!pipe)
				return pipe;

			sscanf (*buf, "\t%d\t%lf\t%lf\t%lf", &i, &val[0], &val[1], &val[2]);
			if (k == 1) {
				sdata->data[0] = g_array_append_val (sdata->data[0], val[0]);
				if (val[0] < sdata->min_data[0])
					sdata->min_data[0] = val[0];
				if (val[0] > sdata->max_data[0])
					sdata->max_data[0] = val[0];
				sdata->data[1] = g_array_append_val (sdata->data[1], val[1]);
				if (val[1] < sdata->min_data[1])
					sdata->min_data[1] = val[1];
				if (val[1] > sdata->max_data[1])
					sdata->max_data[1] = val[1];
				sdata->got_points = sdata->got_points + 2;
			} else {
				sdata->data[k] = g_array_append_val (sdata->data[k], val[1]);
				if (val[1] < sdata->min_data[k])
					sdata->min_data[k] = val[1];
				if (val[1] > sdata->max_data[k])
					sdata->max_data[k] = val[1];
				sdata->got_points++;
			}
		}
		pipe = thread_pipe_pop(pipe, (gpointer *)buf, &size);
		if (!pipe)
			return pipe;

		pipe = thread_pipe_pop(pipe, (gpointer *)buf, &size);
		if (!pipe)
			return pipe;

	}

	sdata->got_var = n;

	*analysis = g_list_append (*analysis, sdata);
	(*num_analysis)++;

	return pipe;
}

/**
 * @resources: caller frees
 */
static ThreadPipe *parse_noise_analysis (NgspiceAnalysisResources *resources)
{
	ThreadPipe *pipe = resources->pipe;
	gboolean is_vanilla = resources->is_vanilla;
	gchar *scale, **variables, **buf = &resources->buf;
	const SimSettings* const sim_settings = resources->sim_settings;
	GList **analysis = resources->analysis;
	guint *num_analysis = resources->num_analysis;
	static SimulationData *sdata;
	static Analysis *data;
	gsize size;
	gboolean found = FALSE;
	gint i, n = 0, index = 0;
	gdouble fstart, fstop, val[10];

	NG_DEBUG ("NOISE: result str\n>>>\n%s\n<<<", *buf);

	data = g_new0 (Analysis, 1);
	sdata = SIM_DATA (data);
	sdata->type = ANALYSIS_TYPE_NOISE;
	sdata->functions = NULL;

	ANALYSIS (sdata)->noise.sim_length = 1.;

	ANALYSIS (sdata)->noise.start = fstart = sim_settings_get_noise_start (sim_settings);
	ANALYSIS (sdata)->noise.stop = fstop = sim_settings_get_noise_stop (sim_settings);

	scale = sim_settings_get_noise_type (sim_settings);
	if (!g_ascii_strcasecmp (scale, "LIN")) {
		ANALYSIS (sdata)->noise.sim_length = (double) sim_settings_get_noise_npoints (sim_settings);
	} else if (!g_ascii_strcasecmp (scale, "DEC")) {
		ANALYSIS (sdata)->noise.sim_length = (double) sim_settings_get_noise_npoints (sim_settings) * log10 (fstop / fstart);
	} else if (!g_ascii_strcasecmp (scale, "OCT")) {
		ANALYSIS (sdata)->noise.sim_length = (double) sim_settings_get_noise_npoints (sim_settings) * log10 (fstop / fstart) / log10 (2);
	}

	pipe = thread_pipe_pop(pipe, (gpointer *)buf, &size);
	pipe = thread_pipe_pop(pipe, (gpointer *)buf, &size);

	// Calculates the number of variables
	variables = get_variables (*buf, &n);
	if (!variables)
		return pipe;

	n = n - 1;
	sdata->var_names = (char **)g_new0 (gpointer, 3);
	sdata->var_units = (char **)g_new0 (gpointer, 3);
	sdata->var_names[0] = g_strdup ("Frequency");
	sdata->var_units[0] = g_strdup (_ ("frequency"));
	sdata->var_names[1] = g_strdup ("Input Noise Spectrum");
	sdata->var_units[1] = g_strdup (_ ("psd"));
	sdata->var_names[2] = g_strdup ("Output Noise Spectrum");
	sdata->var_units[2] = g_strdup (_ ("psd"));

	sdata->n_variables = 3;
	sdata->got_points = 0;
	sdata->got_var = 0;
	sdata->data = (GArray **)g_new0 (gpointer, 3);

	pipe = thread_pipe_pop(pipe, (gpointer *)buf, &size);

	for (i = 0; i < 3; i++)
		sdata->data[i] = g_array_new (TRUE, TRUE, sizeof(double));

	sdata->min_data = g_new (double, 3);
	sdata->max_data = g_new (double, 3);

	// Read the data
	for (i = 0; i < 3; i++) {
		sdata->min_data[i] = G_MAXDOUBLE;
		sdata->max_data[i] = -G_MAXDOUBLE;
	}
	found = FALSE;
	while (!found && ((pipe = thread_pipe_pop(pipe, (gpointer *)buf, &size)) != 0)) {
		if (strlen (*buf) <= 2) {
			pipe = thread_pipe_pop(pipe, (gpointer *)buf, &size);
			pipe = thread_pipe_pop(pipe, (gpointer *)buf, &size);
			pipe = thread_pipe_pop(pipe, (gpointer *)buf, &size);
		}

		variables = get_variables (*buf, &i);
		if (!variables)
			return pipe;

		index = atoi (variables[0]);
		for (i = 0; i < 3; i++) {
			val[i] = g_ascii_strtod (variables[i + 1], NULL);
			sdata->data[i] = g_array_append_val (sdata->data[i], val[i]);
			if (val[i] < sdata->min_data[i])
				sdata->min_data[i] = val[i];
			if (val[i] > sdata->max_data[i])
				sdata->max_data[i] = val[i];
		}
		sdata->got_points++;

		if (index >= ANALYSIS (sdata)->noise.sim_length - 1)
			found = TRUE;
	}

	// Spice 3f5 is affected by a sort of bug and prints extra data
	if (is_vanilla) {
		while (((pipe = thread_pipe_pop(pipe, (gpointer *)buf, &size)) != 0)) {
			if (strlen (*buf) < 2)
				break;
		}
	}

	sdata->got_var = 3;

	*analysis = g_list_append (*analysis, sdata);
	(*num_analysis)++;

	return pipe;
}

/**
 * Stores all the data coming through the given pipe to the given file.
 * This is needed because in case of failure, ngspice prints additional
 * error information to stdout.
 *
 * Maybe the user wants to analyze the ngspice output by external software.
 */
void ngspice_save (const gchar *path_to_file, ThreadPipe *pipe, CancelInfo *cancel_info)
{
	FILE *file = fopen(path_to_file, "w");
	gpointer buf = NULL;
	gsize size;

	for (int i = 0; (pipe = thread_pipe_pop(pipe, &buf, &size)) != NULL; i++) {
		if (size != 0)
			fwrite(buf, 1, size, file);
		/**
		 * cancel_info uses mutex operations, so it shouldn't be
		 * called to often.
		 */
		if (i % 50 == 0 && cancel_info_is_cancel(cancel_info))
			break;
	}
	fclose(file);
	if (pipe != NULL)
		thread_pipe_set_read_eof(pipe);
}

static gboolean get_analysis_type(gchar *buf_in, AnalysisType *type_out) {

	int i = 0;
	gchar *analysis_name = oregano_engine_get_analysis_name_by_type(i);
	while (analysis_name) {
		if (g_str_has_prefix (buf_in, analysis_name)) {
			*type_out = i;
			g_free(analysis_name);
			return TRUE;
		}
		g_free(analysis_name);

		i++;
		analysis_name = oregano_engine_get_analysis_name_by_type(i);
	}
	return FALSE;
}

static guint64 parse_no_of_data_rows(gchar *line) {
	gchar **splitted = g_regex_split_simple("No\\. of Data Rows \\: \\D*(\\d+)\\n", line, 0, 0);
	guint64 no_of_data_rows = g_ascii_strtoull(splitted[1], NULL, 10);
	g_strfreev(splitted);

	return no_of_data_rows;
}

static guint parse_no_of_variables(ThreadPipe *pipe, gchar **buf) {
	gsize size;

	for (int i = 0; i < 5; i++)
		pipe = thread_pipe_pop(pipe, (gpointer *)buf, &size);

	guint no_of_variables = 0;
	while (**buf != '\n') {
		no_of_variables++;
		pipe = thread_pipe_pop(pipe, (gpointer *)buf, &size);
	}

	return no_of_variables;
}

/**
 * @resources: caller frees
 */
void ngspice_analysis (NgspiceAnalysisResources *resources)
{
	ThreadPipe *pipe = resources->pipe;
	const SimSettings *sim_settings = resources->sim_settings;
	AnalysisTypeShared *current = resources->current;
	gboolean is_vanilla = resources->is_vanilla;
	gchar **buf = &resources->buf;
	gsize size;
	gboolean end_of_output;
	gboolean transient_enabled = sim_settings_get_trans (sim_settings);
	gboolean fourier_enabled = sim_settings_get_fourier (sim_settings);
	gboolean dc_enabled = sim_settings_get_dc (sim_settings);
	gboolean ac_enabled = sim_settings_get_ac (sim_settings);
	gboolean noise_enabled = sim_settings_get_noise (sim_settings);

	if (thread_pipe_pop(pipe, (gpointer *)buf, &size) == NULL)
		return;

	if (!is_vanilla) {
		// Get the number of AC Analysis data rows
		while (ac_enabled && !g_str_has_prefix (*buf, "No. of Data Rows : ")) {
			if (thread_pipe_pop(pipe, (gpointer *)buf, &size) == NULL)
				return;
		}
		if (ac_enabled && g_str_has_prefix(*buf, "No. of Data Rows : "))
			resources->no_of_data_rows_ac = parse_no_of_data_rows(*buf);

		if (ac_enabled && thread_pipe_pop(pipe, (gpointer *)buf, &size) == NULL)
			return;

		// Get the number of DC Analysis data rows
		while (dc_enabled && !g_str_has_prefix (*buf, "No. of Data Rows : ")) {
			if (thread_pipe_pop(pipe, (gpointer *)buf, &size) == NULL)
				return;
		}
		if (dc_enabled && g_str_has_prefix(*buf, "No. of Data Rows : "))
			resources->no_of_data_rows_dc = parse_no_of_data_rows(*buf);

		if (dc_enabled && thread_pipe_pop(pipe, (gpointer *)buf, &size) == NULL)
			return;

		// Get the number of Operating Point Analysis data rows
		while (!g_str_has_prefix (*buf, "No. of Data Rows : ")) {
			if (thread_pipe_pop(pipe, (gpointer *)buf, &size) == NULL)
				return;
		}
		if (g_str_has_prefix(*buf, "No. of Data Rows : "))
			resources->no_of_data_rows_op = parse_no_of_data_rows(*buf);

		if (thread_pipe_pop(pipe, (gpointer *)buf, &size) == NULL)
			return;

		// Get the number of Transient Analysis variables
		while (transient_enabled && !g_str_has_prefix (*buf, "Initial Transient Solution")) {
			if (thread_pipe_pop(pipe, (gpointer *)buf, &size) == NULL)
				return;
		}
		if (transient_enabled && g_str_has_prefix(*buf, "Initial Transient Solution"))
			resources->no_of_variables = parse_no_of_variables(pipe, buf);

		if (transient_enabled && thread_pipe_pop(pipe, (gpointer *)buf, &size) == NULL)
			return;

		// Get the number of Transient Analysis data rows
		while (transient_enabled && !g_str_has_prefix (*buf, "No. of Data Rows : ")) {
			if (thread_pipe_pop(pipe, (gpointer *)buf, &size) == NULL)
				return;
		}
		if (transient_enabled && g_str_has_prefix(*buf, "No. of Data Rows : "))
			resources->no_of_data_rows_transient = parse_no_of_data_rows(*buf);

		if (transient_enabled && thread_pipe_pop(pipe, (gpointer *)buf, &size) == NULL)
			return;

		// Get the number of Noise Analysis data rows
		while (noise_enabled && !g_str_has_prefix (*buf, "No. of Data Rows : ")) {
			if (thread_pipe_pop(pipe, (gpointer *)buf, &size) == NULL)
				return;
		}
		if (noise_enabled && g_str_has_prefix(*buf, "No. of Data Rows : "))
			resources->no_of_data_rows_noise = parse_no_of_data_rows(*buf);
	} else {
		while (!g_str_has_prefix (*buf, "Operating point information:")) {
			if (thread_pipe_pop(pipe, (gpointer *)buf, &size) == NULL)
				return;
		}
	}

	while (!g_str_has_suffix (*buf, SP_TITLE)) {
		if (thread_pipe_pop(pipe, (gpointer *)buf, &size) == NULL)
			return;
	}

	end_of_output = FALSE;
	for (int i = 0; transient_enabled || fourier_enabled || dc_enabled || ac_enabled || noise_enabled; i++) {

		AnalysisType analysis_type = ANALYSIS_TYPE_NONE;

		do {
			pipe = thread_pipe_pop(pipe, (gpointer *)buf, &size);
			g_strstrip (*buf);
			if (!g_ascii_strncasecmp (*buf, "CPU time", 8))
				end_of_output = TRUE;
			NG_DEBUG ("%d buf = %s", i, *buf);
		} while (pipe != NULL && !end_of_output && (*buf[0] == '*' || *buf[0] == '\0'));

		// The simulation has finished: no more analysis to parse
		if (end_of_output)
			break;

		if (!get_analysis_type(*buf, &analysis_type) && i == 0) {
			oregano_warning ("No analysis found");
			break;
		}

		gboolean unexpected_analysis_found = FALSE;

		g_mutex_lock(&current->mutex);
		current->type = analysis_type;
		g_mutex_unlock(&current->mutex);

		switch (analysis_type) {
		case ANALYSIS_TYPE_TRANSIENT:
			pipe = parse_transient_analysis (resources);
			transient_enabled = FALSE;
			break;
		case ANALYSIS_TYPE_FOURIER:
			pipe = parse_fourier_analysis (resources);
			fourier_enabled = FALSE;
			break;
		case ANALYSIS_TYPE_DC_TRANSFER:
			pipe = parse_dc_analysis (resources);
			dc_enabled = FALSE;
			break;
		case ANALYSIS_TYPE_AC:
			pipe = parse_ac_analysis (resources);
			ac_enabled = FALSE;
			break;
		case ANALYSIS_TYPE_NOISE:
			pipe = parse_noise_analysis (resources);
			noise_enabled = FALSE;
			break;
		default:
			oregano_warning ("Unexpected analysis found");
			unexpected_analysis_found = TRUE;
			break;
		}

		g_mutex_lock(&current->mutex);
		current->type = ANALYSIS_TYPE_NONE;
		g_mutex_unlock(&current->mutex);

		if (unexpected_analysis_found || pipe == NULL)
			break;
	}

	if (pipe != NULL)
		thread_pipe_set_read_eof(pipe);

	resources->pipe = NULL;
}
