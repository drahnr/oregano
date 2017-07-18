/*
 * ngspice.c
 *
 * Authors:
 *  Ricardo Markiewicz <rmarkie@fi.uba.ar>
 *  Marc Lorber <lorber.marc@wanadoo.fr>
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

#include <glib.h>
#include <glib/gprintf.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <ctype.h>
#include <glib/gi18n.h>

#include "ngspice.h"
#include "netlist-helper.h"
#include "dialogs.h"
#include "engine-internal.h"
#include "ngspice-analysis.h"
#include "errors.h"

#include "ngspice-watcher.h"

static void ngspice_class_init (OreganoNgSpiceClass *klass);
static void ngspice_finalize (GObject *object);
static void ngspice_dispose (GObject *object);
static void ngspice_instance_init (GTypeInstance *instance, gpointer g_class);
static void ngspice_interface_init (gpointer g_iface, gpointer iface_data);

static GObjectClass *parent_class = NULL;

GType oregano_ngspice_get_type (void)
{
	static GType type = 0;
	if (type == 0) {
		static const GTypeInfo info = {sizeof(OreganoNgSpiceClass),
		                               NULL,                               // base_init
		                               NULL,                               // base_finalize
		                               (GClassInitFunc)ngspice_class_init, // class_init
		                               NULL,                               // class_finalize
		                               NULL,                               // class_data
		                               sizeof(OreganoNgSpice),
		                               0,                                        // n_preallocs
		                               (GInstanceInitFunc)ngspice_instance_init, // instance_init
		                               NULL};

		static const GInterfaceInfo ngspice_info = {
		    (GInterfaceInitFunc)ngspice_interface_init, // interface_init
		    NULL,                                       // interface_finalize
		    NULL                                        // interface_data
		};

		type = g_type_register_static (G_TYPE_OBJECT, "OreganoNgSpice", &info, 0);
		g_type_add_interface_static (type, OREGANO_TYPE_ENGINE, &ngspice_info);
	}
	return type;
}

static void ngspice_class_init (OreganoNgSpiceClass *klass)
{
	GObjectClass *object_class;

	parent_class = g_type_class_peek_parent (klass);

	object_class = G_OBJECT_CLASS (klass);

	object_class->dispose = ngspice_dispose;
	object_class->finalize = ngspice_finalize;
}

static void ngspice_finalize (GObject *object)
{
	OreganoNgSpice *ngspice;
	GList *iter;
	int i;

	ngspice = OREGANO_NGSPICE (object);
	iter = ngspice->priv->analysis;
	for (; iter; iter = iter->next) {
		SimulationData *data = SIM_DATA (iter->data);
		for (i = 0; i < data->n_variables; i++) {
			g_free(data->var_names[i]);
			g_free(data->var_units[i]);
			g_array_free (data->data[i], TRUE);
		}
		g_free (data->var_names);
		g_free (data->var_units);
		g_free (data->data);
		g_free (data->min_data);
		g_free (data->max_data);

		g_free (data);
	}
	g_list_free (ngspice->priv->analysis);
	g_mutex_clear(&ngspice->priv->progress_ngspice.progress_mutex);
	g_mutex_clear(&ngspice->priv->progress_reader.progress_mutex);
	g_mutex_clear(&ngspice->priv->current.mutex);
	cancel_info_unsubscribe(ngspice->priv->cancel_info);
	g_free(ngspice->priv);

	parent_class->finalize (object);
}

static void ngspice_dispose (GObject *object) { parent_class->dispose (object); }

static gboolean ngspice_has_warnings (OreganoEngine *self) { return FALSE; }

static gboolean ngspice_is_available (OreganoEngine *self)
{
	gboolean is_vanilla;
	gchar *exe;
	OreganoNgSpice *ngspice = OREGANO_NGSPICE (self);

	is_vanilla = ngspice->priv->is_vanilla;

	if (is_vanilla)
		exe = g_find_program_in_path (SPICE_EXE);
	else
		exe = g_find_program_in_path (NGSPICE_EXE);

	if (!exe)
		return FALSE; // ngspice not found

	g_free (exe);
	return TRUE;
}

/**
 * \brief create a netlist buffer from the engine inernals
 *
 * @engine
 * @error [allow-none]
 */
static GString *ngspice_generate_netlist_buffer (OreganoEngine *engine, GError **error)
{
	OreganoNgSpice *ngspice;
	Netlist output;
	GList *iter;
	GError *e = NULL;
	GString *buffer = NULL;

	ngspice = OREGANO_NGSPICE (engine);

	netlist_helper_create (ngspice->priv->schematic, &output, &e);
	if (e) {
		g_propagate_error (error, e);
		return NULL;
	}

	buffer = g_string_sized_new (500);
	if (!buffer) {
		g_set_error_literal (&e, OREGANO_ERROR, OREGANO_OOM,
		                     "Failed to allocate intermediate buffer.");
		g_propagate_error (error, e);
		return NULL;
	}
	// Prints title
	g_string_append (buffer, "* ");
	g_string_append (buffer, output.title ? output.title : "Title: <unset>");
	g_string_append (buffer, "\n"
	                         "*----------------------------------------------"
	                         "\n"
	                         "*\tngspice - NETLIST"
	                         "\n");

	// Prints Options
	g_string_append (buffer, ".options OUT=120 ");

	iter = sim_settings_get_options (output.settings);
	for (; iter; iter = iter->next) {
		const SimOption *so = iter->data;
		// Prevent send NULL text
		if (so->value) {
			if (strlen (so->value) > 0) {
				g_string_append_printf (buffer, "%s=%s ", so->name, so->value);
			}
		}
	}
	g_string_append_c (buffer, '\n');

	// Include of subckt models
	g_string_append (buffer, "*------------- Models -------------------------\n");
	for (iter = output.models; iter; iter = iter->next) {
		const gchar *model = iter->data;
		gchar *model_with_ext = g_strdup_printf ("%s.model", model);
		gchar *model_path = g_build_filename (OREGANO_MODELDIR, model_with_ext, NULL);
		g_string_append_printf (buffer, ".include %s\n", model_path);
		g_free (model_path);
		g_free (model_with_ext);
	}

	// Prints template parts
	g_string_append (buffer, "*------------- Circuit Description-------------\n");
	g_string_append (buffer, output.template->str);
	g_string_append (buffer, "\n*----------------------------------------------\n");

	// Prints Transient Analysis
	if (sim_settings_get_trans (output.settings)) {
		gdouble st = 0;
		gdouble start = sim_settings_get_trans_start (output.settings);
		gdouble stop = sim_settings_get_trans_stop (output.settings);

		if (sim_settings_get_trans_step_enable (output.settings))
			st = sim_settings_get_trans_step (output.settings);
		else
			st = (stop - start) / 50;

		if ((stop - start) <= 0) {
			// FIXME ask for swapping or cancel simulation
			oregano_error (_ ("Transient: Start time is after Stop time - fix this."
			                  "stop figure\n"));
			g_string_free (buffer, TRUE);
			return NULL;
		}

		g_string_append_printf (buffer, ".tran %e %e %e", st, stop, start);
		if (sim_settings_get_trans_init_cond (output.settings)) {
			g_string_append_printf (buffer, " uic");
		}
		g_string_append_printf (buffer, "\n");

		if (sim_settings_get_trans_analyze_all(output.settings)) {
			g_string_append_printf (buffer, ".print tran all\n");
		} else {
			gchar *tmp_str = netlist_helper_create_analysis_string (output.store, FALSE);
			g_string_append_printf (buffer, ".print tran %s\n", tmp_str);
			g_free (tmp_str);
		}
		g_string_append_c (buffer, '\n');
	}

	// Prints DC Analysis
	if (sim_settings_get_dc (output.settings)) {
		g_string_append (buffer, ".dc ");
		if (sim_settings_get_dc_vsrc (output.settings)) {
			g_string_append_printf (buffer, "V_%s %g %g %g\n",
			                        sim_settings_get_dc_vsrc (output.settings),
			                        sim_settings_get_dc_start (output.settings),
			                        sim_settings_get_dc_stop (output.settings),
			                        sim_settings_get_dc_step (output.settings));
			g_string_append_printf (buffer, ".print dc V(%s)\n",
			                        sim_settings_get_dc_vout (output.settings));
		}
	}

	// Prints AC Analysis
	if (sim_settings_get_ac (output.settings)) {
		if (sim_settings_get_ac_vout (output.settings)) {
			g_string_append_printf (buffer, ".ac %s %d %g %g\n",
		                        sim_settings_get_ac_type (output.settings),
		                        sim_settings_get_ac_npoints (output.settings),
		                        sim_settings_get_ac_start (output.settings),
		                        sim_settings_get_ac_stop (output.settings));
	                g_string_append_printf (buffer, ".print ac %s\n",
	                                        sim_settings_get_ac_vout (output.settings));
		}
	}

	// Prints analysis using a Fourier transform
	if (sim_settings_get_fourier (output.settings)) {
		if (sim_settings_get_fourier_frequency (output.settings) &&
		    sim_settings_get_fourier_nodes (output.settings)) {
			g_string_append_printf (buffer, ".four %.3f %s\n",
			                        sim_settings_get_fourier_frequency (output.settings),
			                        sim_settings_get_fourier_nodes (output.settings));
		}
	}

	// Prints Noise Analysis
	if (sim_settings_get_noise (output.settings)) {
		if (sim_settings_get_noise_vout (output.settings)) {
			g_string_append_printf (buffer, ".noise V(%s) V_%s %s %d %g %g\n",
						sim_settings_get_noise_vout (output.settings),
						sim_settings_get_noise_vsrc (output.settings),
						sim_settings_get_noise_type (output.settings),
						sim_settings_get_noise_npoints (output.settings),
						sim_settings_get_noise_start (output.settings),
						sim_settings_get_noise_stop (output.settings));
			g_string_append (buffer, ".print noise inoise_spectrum onoise_spectrum\n");
		}
	}

	g_string_append (buffer, ".op\n\n.END\n");

	return buffer;
}

/**
 * \brief generate a netlist and write to file
 *
 * @engine engine to extract schematic and settings from
 * @filename target netlist file, user selected
 * @error [allow-none]
 */
static gboolean ngspice_generate_netlist (OreganoEngine *engine, const gchar *filename,
                                          GError **error)
{
	GError *e = NULL;
	GString *buffer;
	gboolean success = FALSE;

	buffer = ngspice_generate_netlist_buffer (engine, &e);
	if (!buffer) {
		oregano_error (g_strdup_printf ("Failed generate netlist buffer\n"));
		g_propagate_error (error, e);
		return FALSE;
	}

	success = g_file_set_contents (filename, buffer->str, buffer->len, &e);
	g_string_free (buffer, TRUE);

	if (!success) {
		g_propagate_error (error, e);
		oregano_error (g_strdup_printf ("Failed to open file \"%s\" in 'w' mode.\n", filename));
		return FALSE;
	}
	return TRUE;
}

static void ngspice_progress (OreganoEngine *self, double *d)
{
	OreganoNgSpice *ngspice = OREGANO_NGSPICE (self);

	g_mutex_lock(&ngspice->priv->progress_ngspice.progress_mutex);
	*d = ngspice->priv->progress_ngspice.progress;
	g_mutex_unlock(&ngspice->priv->progress_ngspice.progress_mutex);
}

static void reader_progress (OreganoEngine *self, double *d)
{
	OreganoNgSpice *ngspice = OREGANO_NGSPICE (self);

	g_mutex_lock(&ngspice->priv->progress_reader.progress_mutex);
	*d = ngspice->priv->progress_reader.progress;
	g_mutex_unlock(&ngspice->priv->progress_reader.progress_mutex);
}

static void ngspice_stop (OreganoEngine *self)
{
	OreganoNgSpice *ngspice = OREGANO_NGSPICE (self);
	cancel_info_set_cancel(ngspice->priv->cancel_info);
	GPid child_pid = ngspice->priv->child_pid;
	if (child_pid != 0) {
		// CTRL+C (Terminal quit signal.)
		kill(child_pid, SIGINT);
		ngspice->priv->child_pid = 0;
	}
}

static void ngspice_start (OreganoEngine *self)
{
	OreganoNgSpice *ngspice = OREGANO_NGSPICE (self);
	OreganoNgSpicePriv *priv = ngspice->priv;

	GError *e = NULL;
	if (!oregano_engine_generate_netlist (self, "/tmp/netlist.tmp", &e)) {
		priv->aborted = TRUE;
		if (e)
			schematic_log_append_error (priv->schematic, e->message);
		else
			schematic_log_append_error(priv->schematic, "Error at netlist generation.");
		g_signal_emit_by_name (G_OBJECT (ngspice), "aborted");
		g_clear_error (&e);
		return;
	}

	NgspiceWatcherBuildAndLaunchResources *resources = ngspice_watcher_build_and_launch_resources_new(ngspice);
	ngspice_watcher_build_and_launch(resources);
	ngspice_watcher_build_and_launch_resources_finalize(resources);
}

static GList *ngspice_get_results (OreganoEngine *self)
{
	if (OREGANO_NGSPICE (self)->priv->analysis == NULL)
		printf ("pas d'analyse\n");
	return OREGANO_NGSPICE (self)->priv->analysis;
}

static gchar *ngspice_get_operation_ngspice (OreganoEngine *self)
{
	OreganoNgSpicePriv *priv = OREGANO_NGSPICE (self)->priv;

	g_mutex_lock(&priv->progress_ngspice.progress_mutex);
	gint64 old_time = priv->progress_ngspice.time;
	g_mutex_unlock(&priv->progress_ngspice.progress_mutex);

	gint64 new_time = g_get_monotonic_time();
	if (new_time - old_time >= 1000000)
		return g_strdup("ngspice not responding");
	return g_strdup(_("ngspice solving"));
}

static gchar *ngspice_get_operation_reader (OreganoEngine *self)
{
	OreganoNgSpicePriv *priv = OREGANO_NGSPICE (self)->priv;

	g_mutex_lock(&priv->current.mutex);
	AnalysisType type = priv->current.type;
	g_mutex_unlock(&priv->current.mutex);

	return oregano_engine_get_analysis_name_by_type(type);
}

static void ngspice_interface_init (gpointer g_iface, gpointer iface_data)
{
	OreganoEngineClass *klass = (OreganoEngineClass *)g_iface;
	klass->start = ngspice_start;
	klass->stop = ngspice_stop;
	klass->progress_solver = ngspice_progress;
	klass->progress_reader = reader_progress;
	klass->get_netlist = ngspice_generate_netlist;
	klass->has_warnings = ngspice_has_warnings;
	klass->get_results = ngspice_get_results;
	klass->get_operation_solver = ngspice_get_operation_ngspice;
	klass->get_operation_reader = ngspice_get_operation_reader;
	klass->is_available = ngspice_is_available;
}

static void ngspice_instance_init (GTypeInstance *instance, gpointer g_class)
{
	OreganoNgSpice *self = OREGANO_NGSPICE (instance);

	self->priv = g_new0 (OreganoNgSpicePriv, 1);
	self->priv->progress_ngspice.progress = 0.0;
	self->priv->progress_ngspice.time = g_get_monotonic_time();
	g_mutex_init(&self->priv->progress_ngspice.progress_mutex);
	self->priv->progress_reader.progress = 0.0;
	self->priv->progress_reader.time = g_get_monotonic_time();
	g_mutex_init(&self->priv->progress_reader.progress_mutex);
	self->priv->current.type = ANALYSIS_TYPE_NONE;
	g_mutex_init(&self->priv->current.mutex);
	self->priv->num_analysis = 0;
	self->priv->analysis = NULL;
	self->priv->aborted = FALSE;

	self->priv->cancel_info = cancel_info_new();
}

/*
 * Set "is_vanilla" to TRUE if using the original spice3 from
 * UC Berkeley.
 */
OreganoEngine *oregano_spice_new (Schematic *sc, gboolean is_vanilla)
{
	OreganoNgSpice *ngspice;

	ngspice = OREGANO_NGSPICE (g_object_new (OREGANO_TYPE_NGSPICE, NULL));
	ngspice->priv->schematic = sc;
	ngspice->priv->is_vanilla = is_vanilla;

	return OREGANO_ENGINE (ngspice);
}
