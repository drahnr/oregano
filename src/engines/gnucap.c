/*
 * gnucap.c
 *
 * Authors:
 *  Ricardo Markiewicz <rmarkie@fi.uba.ar>
 *  Marc Lorber <lorber.marc@wanadoo.fr>
 *
 * Web page: https://ahoi.io/project/oregano
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
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include <glib.h>
#include <glib/gprintf.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <ctype.h>
#include <glib/gi18n.h>

#include "gnucap.h"
#include "netlist-helper.h"
#include "dialogs.h"
#include "engine-internal.h"

// TODO Move analysis data and result to another file
#include "simulation.h"

struct analysis_tag
{
	gchar *name;
	guint len;
};

static struct analysis_tag analysis_tags[] = {
	{"#", 1},     /* OP */
	{"#Time", 5}, /* Transient */
	{"#DC", 3},   /* DC */
	{"#Freq", 5}, /* AC */
};

#define IS_THIS_ITEM(str, item) (!strncmp (str, item.name, item.len))
#define GNUCAP_TITLE '#'
#define TAGS_COUNT (sizeof(analysis_tags) / sizeof(struct analysis_tag))

// Parser STATUS
struct _OreganoGnuCapPriv
{
	GPid child_pid;
	gint child_stdout;
	GIOChannel *child_iochannel;
	gint child_iochannel_watch;
	Schematic *schematic;

	gboolean aborted;

	GList *analysis;
	gint num_analysis;
	SimulationData *current;
	double progress;
	gboolean char_last_newline;
	guint status;
	guint buf_count;
};

#include "debug.h"

static void gnucap_class_init (OreganoGnuCapClass *klass);
static void gnucap_finalize (GObject *object);
static void gnucap_dispose (GObject *object);
static void gnucap_instance_init (GTypeInstance *instance, gpointer g_class);
static void gnucap_interface_init (gpointer g_iface, gpointer iface_data);
static gboolean gnucap_child_stdout_cb (GIOChannel *source, GIOCondition condition,
                                        OreganoGnuCap *gnucap);
static void gnucap_parse (gchar *raw, gint len, OreganoGnuCap *gnucap);
static gdouble strtofloat (char *s);

static GObjectClass *parent_class = NULL;

GType oregano_gnucap_get_type (void)
{
	static GType type = 0;
	if (type == 0) {
		static const GTypeInfo info = {sizeof(OreganoGnuCapClass),
		                               NULL,                              // base_init
		                               NULL,                              // base_finalize
		                               (GClassInitFunc)gnucap_class_init, // class_init
		                               NULL,                              // class_finalize
		                               NULL,                              // class_data
		                               sizeof(OreganoGnuCap),
		                               0,                                       // n_preallocs
		                               (GInstanceInitFunc)gnucap_instance_init, // instance_init
		                               NULL};

		static const GInterfaceInfo gnucap_info = {
		    (GInterfaceInitFunc)gnucap_interface_init, // interface_init
		    NULL,                                      // interface_finalize
		    NULL                                       // interface_data
		};

		type = g_type_register_static (G_TYPE_OBJECT, "OreganoGnuCap", &info, 0);
		g_type_add_interface_static (type, OREGANO_TYPE_ENGINE, &gnucap_info);
	}
	return type;
}

static void gnucap_class_init (OreganoGnuCapClass *klass)
{
	GObjectClass *object_class;

	parent_class = g_type_class_peek_parent (klass);

	object_class = G_OBJECT_CLASS (klass);

	object_class->dispose = gnucap_dispose;
	object_class->finalize = gnucap_finalize;
}

static void gnucap_finalize (GObject *object)
{
	SimulationData *data;
	OreganoGnuCap *gnucap;
	GList *iter;
	int i;

	gnucap = OREGANO_GNUCAP (object);

	iter = gnucap->priv->analysis;
	for (; iter; iter = iter->next) {
		data = SIM_DATA (iter->data);
		for (i = 0; i < data->n_variables; i++) {
			g_free (data->var_names[i]);
			g_free (data->var_units[i]);
		}
		g_free (data->var_names);
		g_free (data->var_units);
		for (i = 0; i < data->n_variables; i++)
			g_array_free (data->data[i], TRUE);
		g_free (data->min_data);
		g_free (data->max_data);

		g_free (iter->data);
	}
	g_list_free (gnucap->priv->analysis);
	gnucap->priv->analysis = NULL;

	parent_class->finalize (object);
}

static void gnucap_dispose (GObject *object) { parent_class->dispose (object); }

static gboolean gnucap_has_warnings (OreganoEngine *self) { return FALSE; }

static gboolean gnucap_is_available (OreganoEngine *self)
{
	gchar *exe;
	exe = g_find_program_in_path ("gnucap");

	if (!exe)
		return FALSE; // gnucap not found

	g_free (exe);
	return TRUE;
}

static gboolean gnucap_generate_netlist (OreganoEngine *engine, const gchar *filename,
                                         GError **error)
{
	OreganoGnuCap *gnucap;
	Netlist output;
	SimOption *so;
	GList *iter;
	FILE *file;
	GError *local_error = NULL;

	gnucap = OREGANO_GNUCAP (engine);

	netlist_helper_create (gnucap->priv->schematic, &output, &local_error);
	if (local_error != NULL) {
		g_propagate_error (error, local_error);
		return FALSE;
	}

	file = fopen (filename, "w");
	if (!file) {
		oregano_error (g_strdup_printf ("Creation of file %s not possible\n", filename));
		return FALSE;
	}

	// Prints title
	fputs (output.title ? output.title : "Title: <unset>", file);
	fputs ("\n"
	       "*----------------------------------------------"
	       "\n"
	       "*\tGNUCAP - NETLIST"
	       "\n",
	       file);
	// Prints Options
	fputs (".options OUT=120 ", file);

	iter = sim_settings_get_options (output.settings);
	for (; iter; iter = iter->next) {
		so = iter->data;
		// Prevent send NULL text
		if (so->value) {
			if (strlen (so->value) > 0) {
				g_fprintf (file, "%s=%s ", so->name, so->value);
			}
		}
	}
	fputc ('\n', file);

	// Include of subckt models
	fputs ("*------------- Models -------------------------\n", file);
	for (iter = output.models; iter; iter = iter->next) {
		gchar *model;
		model = (gchar *)iter->data;
		g_fprintf (file, ".include %s/%s.model\n", OREGANO_MODELDIR, model);
	}

	// Prints template parts
	fputs ("*------------- Circuit Description-------------\n", file);
	fputs (output.template->str, file);
	fputs ("\n*----------------------------------------------\n", file);

	// Prints Transient Analysis
	if (sim_settings_get_trans (output.settings)) {
		gchar *tmp_str = netlist_helper_create_analysis_string (output.store, FALSE);
		g_fprintf (file, ".print tran %s\n", tmp_str);
		g_free (tmp_str);

		g_fprintf (file, ".tran %g %g ", sim_settings_get_trans_start (output.settings),
		           sim_settings_get_trans_stop (output.settings));
		if (!sim_settings_get_trans_step_enable (output.settings))
			g_fprintf (file, "%g", (sim_settings_get_trans_stop (output.settings) -
			                        sim_settings_get_trans_start (output.settings)) /
			                           100);
		else
			g_fprintf (file, "%g", sim_settings_get_trans_step (output.settings));

		if (sim_settings_get_trans_init_cond (output.settings)) {
			fputs (" UIC\n", file);
		} else {
			fputs ("\n", file);
		}
	}

	// Print DC Analysis
	if (sim_settings_get_dc (output.settings)) {
		g_fprintf (file, ".print dc %s\n",
		           netlist_helper_create_analysis_string (output.store, FALSE));
		fputs (".dc ", file);

		// GNUCAP don't support nesting so the first or the second
		// Maybe an error message must be show if both are on
		g_fprintf (file, "%s %g %g %g", sim_settings_get_dc_vsrc (output.settings),
		           sim_settings_get_dc_start (output.settings),
		           sim_settings_get_dc_stop (output.settings),
		           sim_settings_get_dc_step (output.settings));
		fputc ('\n', file);
	}

	// Prints AC Analysis
	if (sim_settings_get_ac (output.settings)) {
		double ac_start, ac_stop, ac_step;
		// GNUCAP dont support OCT or DEC: Maybe an error message
		// must be shown if the netlist is set in that way.
		ac_start = sim_settings_get_ac_start (output.settings);
		ac_stop = sim_settings_get_ac_stop (output.settings);
		ac_step = (ac_stop - ac_start) / sim_settings_get_ac_npoints (output.settings);
		g_fprintf (file, ".print ac %s\n",
		           netlist_helper_create_analysis_string (output.store, TRUE));
		// AC format : ac start stop step_size
		g_fprintf (file, ".ac %g %g %g\n", ac_start, ac_stop, ac_step);
	}

	// Prints analysis using a Fourier transform
	/* if (sim_settings_get_fourier (output.settings)) {
	        gchar *tmp_str = netlist_helper_create_analysis_string (output.store,
	FALSE);
	        g_fprintf (file, ".four %d %s\n",
	                sim_settings_get_fourier_frequency (output.settings),
	tmp_str);
	        g_free (tmp_str);
	}*/

	// Debug op analysis.
	fputs (".print op v(nodes)\n", file);
	fputs (".op\n", file);
	fputs (".end\n", file);
	fclose (file);
	return TRUE;
}

static void gnucap_progress (OreganoEngine *self, double *d)
{
	OreganoGnuCap *gnucap = OREGANO_GNUCAP (self);

	gnucap->priv->progress += 0.1;
	(*d) = gnucap->priv->progress;
}

static void gnucap_stop (OreganoEngine *self)
{
	OreganoGnuCap *gnucap = OREGANO_GNUCAP (self);
	g_io_channel_shutdown (gnucap->priv->child_iochannel, TRUE, NULL);
	g_source_remove (gnucap->priv->child_iochannel_watch);
	g_spawn_close_pid (gnucap->priv->child_pid);
	g_spawn_close_pid (gnucap->priv->child_stdout);
}

static void gnucap_watch_cb (GPid pid, gint status, OreganoGnuCap *gnucap)
{
	// check for status, see man waitpid(2)
	if (WIFEXITED (status)) {
		gchar *line;
		gsize len;
		g_io_channel_read_to_end (gnucap->priv->child_iochannel, &line, &len, NULL);
		if (len > 0)
			gnucap_parse (line, len, gnucap);
		g_free (line);

		// Free stuff
		g_io_channel_shutdown (gnucap->priv->child_iochannel, TRUE, NULL);
		g_source_remove (gnucap->priv->child_iochannel_watch);
		g_spawn_close_pid (gnucap->priv->child_pid);
		g_spawn_close_pid (gnucap->priv->child_stdout);

		gnucap->priv->current = NULL;

		if (gnucap->priv->num_analysis == 0) {
			schematic_log_append_error (gnucap->priv->schematic,
			                            _ ("### Too few or none analysis found ###\n"));
			gnucap->priv->aborted = TRUE;
			g_signal_emit_by_name (G_OBJECT (gnucap), "aborted");
		} else
			g_signal_emit_by_name (G_OBJECT (gnucap), "done");
	}
}

static gboolean gnucap_child_stdout_cb (GIOChannel *source, GIOCondition condition,
                                        OreganoGnuCap *gnucap)
{
	gchar *line;
	gsize len, terminator;
	GIOStatus status;
	GError *error = NULL;

	status = g_io_channel_read_line (source, &line, &len, &terminator, &error);
	if ((status & G_IO_STATUS_NORMAL) && (len > 0)) {
		gnucap_parse (line, len, gnucap);
		g_free (line);
	}

	// Let UI update
	g_main_context_iteration (NULL, FALSE);
	return TRUE;
}

static void gnucap_start (OreganoEngine *self)
{
	OreganoGnuCap *gnucap;
	GError *error = NULL;
	char *argv[] = {"gnucap", "-b", "/tmp/netlist.tmp", NULL};

	gnucap = OREGANO_GNUCAP (self);
	oregano_engine_generate_netlist (self, "/tmp/netlist.tmp", &error);
	if (error != NULL) {
		gnucap->priv->aborted = TRUE;
		schematic_log_append_error (gnucap->priv->schematic, error->message);
		g_signal_emit_by_name (G_OBJECT (gnucap), "aborted");
		g_error_free (error);
		return;
	}

	error = NULL;
	if (g_spawn_async_with_pipes (NULL, // Working directory
	                              argv, NULL, G_SPAWN_DO_NOT_REAP_CHILD | G_SPAWN_SEARCH_PATH, NULL,
	                              NULL, &gnucap->priv->child_pid,
	                              NULL,                        // STDIN
	                              &gnucap->priv->child_stdout, // STDOUT
	                              NULL,                        // STDERR
	                              &error)) {
		// Add a watch for process status
		g_child_watch_add (gnucap->priv->child_pid, (GChildWatchFunc)gnucap_watch_cb, gnucap);
		// Add a GIOChannel to read from process stdout
		gnucap->priv->child_iochannel = g_io_channel_unix_new (gnucap->priv->child_stdout);
		// Watch the I/O Channel to read child strout
		gnucap->priv->child_iochannel_watch = g_io_add_watch (
		    gnucap->priv->child_iochannel, G_IO_IN | G_IO_PRI | G_IO_HUP | G_IO_NVAL,
		    (GIOFunc)gnucap_child_stdout_cb, gnucap);
	} else {
		gnucap->priv->aborted = TRUE;
		schematic_log_append_error (gnucap->priv->schematic, _ ("Unable to execute GnuCap."));
		g_signal_emit_by_name (G_OBJECT (gnucap), "aborted");
	}
}

static GList *gnucap_get_results (OreganoEngine *self)
{
	return OREGANO_GNUCAP (self)->priv->analysis;
}

static gchar *gnucap_get_operation (OreganoEngine *self)
{
	OreganoGnuCapPriv *priv = OREGANO_GNUCAP (self)->priv;

	if (priv->current == NULL)
		return g_strdup(_("None"));

	return oregano_engine_get_analysis_name (priv->current);
}

static void gnucap_interface_init (gpointer g_iface, gpointer iface_data)
{
	OreganoEngineClass *klass = (OreganoEngineClass *)g_iface;
	klass->start = gnucap_start;
	klass->stop = gnucap_stop;
	klass->progress_solver = gnucap_progress;
	klass->get_netlist = gnucap_generate_netlist;
	klass->has_warnings = gnucap_has_warnings;
	klass->get_results = gnucap_get_results;
	klass->get_operation_solver = gnucap_get_operation;
	klass->is_available = gnucap_is_available;
}

static void gnucap_instance_init (GTypeInstance *instance, gpointer g_class)
{
	OreganoGnuCap *self = OREGANO_GNUCAP (instance);

	self->priv = g_new0 (OreganoGnuCapPriv, 1);
	self->priv->progress = 0.0;
	self->priv->char_last_newline = TRUE;
	self->priv->status = 0;
	self->priv->buf_count = 0;
	self->priv->num_analysis = 0;
	self->priv->analysis = NULL;
	self->priv->current = NULL;
	self->priv->aborted = FALSE;
}

OreganoEngine *oregano_gnucap_new (Schematic *sc)
{
	OreganoGnuCap *gnucap;

	gnucap = OREGANO_GNUCAP (g_object_new (OREGANO_TYPE_GNUCAP, NULL));
	gnucap->priv->schematic = sc;

	return OREGANO_ENGINE (gnucap);
}

typedef struct
{
	gchar *name;
} GCap_Variable;

static GCap_Variable *_get_variables (gchar *str, gint *count)
{
	GCap_Variable *out;
	gchar *tmp[100];
	gchar *ini, *fin;
	gint i = 0;

	i = 0;
	ini = str;
	// Put blank in advance
	while (isspace (*ini))
		ini++;
	fin = ini;
	while (*fin != '\0') {
		if (isspace (*fin)) {
			*fin = '\0';
			tmp[i] = g_strdup (ini);
			*fin = ' ';
			i++;
			ini = fin;
			while (isspace (*ini))
				ini++;
			fin = ini;
		} else
			fin++;
	}

	if (i == 0) {
		g_warning ("NO COLUMNS FOUND\n");
		return NULL;
	}

	out = g_new0 (GCap_Variable, i);
	(*count) = i;
	for (i = 0; i < (*count); i++) {
		out[i].name = tmp[i];
	}
	return out;
}

static void _free_variables (GCap_Variable *v, gint count)
{
	int i;
	for (i = 0; i < count; i++)
		g_free (v[i].name);
	g_free (v);
}

gdouble strtofloat (gchar *s)
{
	gdouble val;
	gchar *error;

	val = g_strtod (s, &error);
	// If the value looks like : 100.u, adjust it
	// We need this because GNU Cap's or ngSpice float notation
	switch (error[0]) {
	case 'u':
		val /= 1000000;
		break;
	case 'n':
		val /= 1000000;
		val /= 1000;
		break;
	case 'p':
		val /= 1000000;
		val /= 1000000;
		break;
	case 'f':
		val /= 100;
		break;
	case 'K':
		val *= 1000;
		break;
	default:
		if (strcmp (error, "Meg") == 0)
			val *= 1000000;
	}

	return val;
}

// Main method. Here we'll transform GnuCap output
// into SimulationResults!
static void gnucap_parse (gchar *raw, gint len, OreganoGnuCap *gnucap)
{
	static SimulationData *sdata;
	static Analysis *data;
	static char buf[1024];
	GCap_Variable *variables;
	OreganoGnuCapPriv *priv = gnucap->priv;
	gint i, j, n;
	gdouble val;
	gchar *s;

	for (j = 0; j < len; j++) {
		if (raw[j] != '\n') {
			buf[priv->buf_count++] = raw[j];
			continue;
		}
		buf[priv->buf_count] = '\0';

		// Got a complete line
		s = buf;
		NG_DEBUG ("%s", s);
		if (s[0] == GNUCAP_TITLE) {
			SimSettings *sim_settings;
			gdouble np1;

			sim_settings = schematic_get_sim_settings (priv->schematic);

			data = g_new0 (Analysis, 1);
			priv->current = sdata = SIM_DATA (data);
			priv->analysis = g_list_append (priv->analysis, sdata);
			priv->num_analysis++;
			sdata->type = ANALYSIS_TYPE_UNKNOWN;
			sdata->functions = NULL;

			// Calculates the quantity of variables
			variables = _get_variables (s, &n);

			for (i = 0; i < TAGS_COUNT; i++)
				if (IS_THIS_ITEM (variables[0].name, analysis_tags[i]))
					sdata->type = i + 1;

			sdata->n_variables = n;
			sdata->got_points = 0;
			sdata->got_var = 0;
			sdata->var_names = (char **)g_new0 (gpointer, n);
			sdata->var_units = (char **)g_new0 (gpointer, n);
			sdata->data = (GArray **)g_new0 (gpointer, n);

			for (i = 0; i < n; i++)
				sdata->data[i] = g_array_new (TRUE, TRUE, sizeof(double));
			sdata->min_data = g_new (double, n);
			sdata->max_data = g_new (double, n);

			for (i = 0; i < n; i++) {
				sdata->min_data[i] = G_MAXDOUBLE;
				sdata->max_data[i] = -G_MAXDOUBLE;
			}
			for (i = 0; i < n; i++) {
				sdata->var_names[i] = g_strdup (variables[i].name);
				switch (sdata->type) {
				case ANALYSIS_TYPE_TRANSIENT:
					if (i == 0)
						sdata->var_units[i] = g_strdup (_ ("time"));
					else {
						if (strstr (sdata->var_names[i], "db") != NULL) {
							sdata->var_units[i] = g_strdup ("db");
						} else
							sdata->var_units[i] = g_strdup (_ ("voltage"));
					}
					break;
				case ANALYSIS_TYPE_AC:
					if (i == 0)
						sdata->var_units[i] = g_strdup (_ ("frequency"));
					else {
						if (strstr (sdata->var_names[i], "db") != NULL) {
							sdata->var_units[i] = g_strdup ("db");
						} else
							sdata->var_units[i] = g_strdup (_ ("voltage"));
					}
					break;
				default:
					sdata->var_units[i] = g_strdup ("");
				}
			}
			sdata->n_variables = n;

			switch (sdata->type) {
			case ANALYSIS_TYPE_TRANSIENT:
				data->transient.sim_length = sim_settings_get_trans_stop (sim_settings) -
				                             sim_settings_get_trans_start (sim_settings);
				data->transient.step_size = sim_settings_get_trans_step (sim_settings);
				break;
			case ANALYSIS_TYPE_AC:
				data->ac.start = sim_settings_get_ac_start (sim_settings);
				data->ac.stop = sim_settings_get_ac_stop (sim_settings);
				data->ac.sim_length = sim_settings_get_ac_npoints (sim_settings);
				break;
			case ANALYSIS_TYPE_OP_POINT:
				break;
			case ANALYSIS_TYPE_DC_TRANSFER:
				np1 = 1.;
				data->dc.start = sim_settings_get_dc_start (sim_settings);
				data->dc.stop = sim_settings_get_dc_stop (sim_settings);
				data->dc.step = sim_settings_get_dc_step (sim_settings);
				np1 = (data->dc.stop - data->dc.start) / data->dc.step;

				data->dc.sim_length = np1;
				break;
			case ANALYSIS_TYPE_TRANSFER:
			case ANALYSIS_TYPE_DISTORTION:
			case ANALYSIS_TYPE_NOISE:
			case ANALYSIS_TYPE_POLE_ZERO:
			case ANALYSIS_TYPE_SENSITIVITY:
			case ANALYSIS_TYPE_FOURIER:
				break;
			case ANALYSIS_TYPE_NONE:
				g_error (_ ("No analysis"));
				break;
			case ANALYSIS_TYPE_UNKNOWN:
				g_error (_ ("Unknown analysis"));
				break;
			}
		} else {
			if ((priv->analysis == NULL) || (isalpha (s[0]))) {
				if (priv->buf_count > 1) {
					schematic_log_append (priv->schematic, s);
					schematic_log_append (priv->schematic, "\n");
				}
				priv->buf_count = 0;
				continue;
			}

			val = strtofloat (s);
			switch (sdata->type) {
			case ANALYSIS_TYPE_TRANSIENT:
				priv->progress = val / data->transient.sim_length;
				break;
			case ANALYSIS_TYPE_AC:
				priv->progress = (val - data->ac.start) / data->ac.sim_length;
				break;
			case ANALYSIS_TYPE_DC_TRANSFER:
				priv->progress = val / data->ac.sim_length;
				break;
			default:
				break;
			}
			if (priv->progress > 1.0)
				priv->progress = 1.0;

			variables = _get_variables (s, &n);
			for (i = 0; i < n; i++) {
				val = strtofloat (variables[i].name);
				sdata->data[i] = g_array_append_val (sdata->data[i], val);

				// Update the minimum and maximum values so far.
				if (val < sdata->min_data[i])
					sdata->min_data[i] = val;
				if (val > sdata->max_data[i])
					sdata->max_data[i] = val;
			}

			_free_variables (variables, n);
			sdata->got_points++;
			sdata->got_var = n;
		}
		priv->buf_count = 0;
	}
}
