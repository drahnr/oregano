/*
 * ngspice.c
 *
 * Authors:
 *  Ricardo Markiewicz <rmarkie@fi.uba.ar>
 *  Marc Lorber <lorber.marc@wanadoo.fr>
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

#define NG_DEBUG(s) if (0) g_print ("%s\n", s)

static void ngspice_class_init (OreganoNgSpiceClass *klass);
static void ngspice_finalize (GObject *object);
static void ngspice_dispose (GObject *object);
static void ngspice_instance_init (GTypeInstance *instance, gpointer g_class);
static void ngspice_interface_init (gpointer g_iface, gpointer iface_data);
static gboolean ngspice_child_stdout_cb (GIOChannel *source, 
    GIOCondition condition, OreganoNgSpice *ngspice);
static gboolean ngspice_child_stderr_cb (GIOChannel *source, 
    GIOCondition condition, OreganoNgSpice *ngspice);

static GObjectClass *parent_class = NULL;

GType 
oregano_ngspice_get_type (void)
{
	static GType type = 0;
	if (type == 0) {
		static const GTypeInfo info = {
			sizeof (OreganoNgSpiceClass),
			NULL,   									// base_init
			NULL,   									// base_finalize
			(GClassInitFunc) ngspice_class_init,   		// class_init
			NULL,   									// class_finalize
			NULL,   									// class_data
			sizeof (OreganoNgSpice),
			0,      									// n_preallocs
			(GInstanceInitFunc) ngspice_instance_init,  // instance_init
			NULL
		};

		static const GInterfaceInfo ngspice_info = {
			(GInterfaceInitFunc) ngspice_interface_init,// interface_init
			NULL,               						// interface_finalize
			NULL                						// interface_data
		};

		type = g_type_register_static (G_TYPE_OBJECT, "OreganoNgSpice", &info, 0);
		g_type_add_interface_static (type, OREGANO_TYPE_ENGINE, &ngspice_info);
	}
	return type;
}

static void
ngspice_class_init (OreganoNgSpiceClass *klass)
{
	GObjectClass *object_class;

	parent_class = g_type_class_peek_parent (klass);

	object_class = G_OBJECT_CLASS (klass);

	object_class->dispose = ngspice_dispose;
	object_class->finalize = ngspice_finalize;
}

static void
ngspice_finalize (GObject *object)
{
	SimulationData *data;
	OreganoNgSpice *ngspice;
	GList *list;
	int i;

	ngspice = OREGANO_NGSPICE (object);
	list = ngspice->priv->analysis;
	while (list) {
		data = SIM_DATA (list->data);
		g_free (data->var_names);
		g_free (data->var_units);
		for (i=0; i<data->n_variables; i++)
			g_array_free (data->data[i], TRUE);

		g_free (data->min_data);
		g_free (data->max_data);
		
		g_free (list->data);
		list = list->next;
	}
	g_list_free (ngspice->priv->analysis);
	ngspice->priv->analysis = NULL;
	g_list_free_full (list, g_object_unref);

	parent_class->finalize (object);
}

static void
ngspice_dispose (GObject *object)
{
	parent_class->dispose (object);
}

static gboolean
ngspice_has_warnings (OreganoEngine *self)
{
	return FALSE;
}

static gboolean
ngspice_is_available (OreganoEngine *self)
{
	gchar *exe;
	exe = g_find_program_in_path ("ngspice");

	if (!exe) return FALSE; // ngspice not found

	g_free (exe);
	return TRUE;
}

static void
ngspice_generate_netlist (OreganoEngine *engine, const gchar *filename, 
                          GError **error)
{
	OreganoNgSpice *ngspice;
	Netlist output;
	SimOption *so;
	GList *list;
	FILE *file;
	GError *local_error = NULL;

	ngspice = OREGANO_NGSPICE (engine);

	netlist_helper_create (ngspice->priv->schematic, &output, &local_error);
	if (local_error != NULL) {
		g_propagate_error (error, local_error);
		return;
	}

	file = fopen (filename, "w");
	if (!file) {
		oregano_error (g_strdup_printf ("Creation of %s not possible\n", filename));
		return;
	}
	
	list = sim_settings_get_options (output.settings);
		
	// Prints title	
	fputs ("* ",file);
	fputs (output.title ? output.title : "Title: <unset>", file);
	fputs ("\n"
		"*----------------------------------------------"
		"\n"
		"*\tngspice - NETLIST"
		"\n", file);

	// Prints Options
	fputs (".options OUT=120 ",file);

	while (list) {
		so = list->data;
		// Prevent send NULL text
		if (so->value) {
			if (strlen(so->value) > 0) {
				g_fprintf (file,"%s=%s ",so->name,so->value);
			}
		}
		list = list->next;
	}
	fputc ('\n',file);

	// Include of subckt models
	fputs ("*------------- Models -------------------------\n",file);
	list = output.models;
	while (list) {
		gchar *model;
		model = (gchar *)list->data;
		g_fprintf (file,".include %s/%s.model\n", OREGANO_MODELDIR, model);
		list = list->next;
	}

	// Prints template parts 
	fputs ("*------------- Circuit Description-------------\n",file);
	fputs (output.template->str, file);
	fputs ("\n*----------------------------------------------\n",file);

	// Prints Transient Analysis
	if (sim_settings_get_trans (output.settings)) {
		gdouble st = 0;
		gdouble start = sim_settings_get_trans_start (output.settings);
		gdouble stop  = sim_settings_get_trans_stop (output.settings);

		if (sim_settings_get_trans_step_enable (output.settings))
			st = sim_settings_get_trans_step (output.settings);
		else
			st = (stop-start) / 50;

		if ((stop-start) <= 0) {
			oregano_error (_("Your transient analysis settings present a "
			          "weakness... the start figure is greater than "
			           "stop figure\n"));
			return;
		}

		g_fprintf (file, ".tran %lf %lf %lf\n", st, stop, start);
		{
			gchar *tmp_str = netlist_helper_create_analysis_string (output.store, FALSE);
			g_fprintf (file, ".print tran %s\n", tmp_str);
			g_free (tmp_str);
			
		}
		fputs ("\n", file);
	}

	//	Print DC Analysis
	if (sim_settings_get_dc (output.settings)) {
		fputs (".dc ",file);
		if (sim_settings_get_dc_vsrc (output.settings)) {
			g_fprintf (file, "V_V%s %g %g %g\n",
				sim_settings_get_dc_vsrc (output.settings),
				sim_settings_get_dc_start (output.settings),
				sim_settings_get_dc_stop (output.settings),
				sim_settings_get_dc_step (output.settings));
			g_fprintf (file, ".print dc V(%s)\n", sim_settings_get_dc_vsrc (output.settings));
		}
	}

	// Prints AC Analysis
	if (sim_settings_get_ac (output.settings)) {
		g_fprintf (file, ".ac %s %d %g %g\n",
			sim_settings_get_ac_type (output.settings),
			sim_settings_get_ac_npoints (output.settings),
			sim_settings_get_ac_start (output.settings),
			sim_settings_get_ac_stop (output.settings));
	}
	
	// Prints analysis using a Fourier transform
	if (sim_settings_get_fourier (output.settings)) {	
		g_fprintf (file, ".four %d %s\n",
		    sim_settings_get_fourier_frequency (output.settings), 
		    sim_settings_get_fourier_nodes (output.settings));
	}

	g_list_free_full (list, g_object_unref);
	
	// Debug op analysis.
	fputs (".op\n", file);
	fputs ("\n.END\n", file);
	fclose (file);
}

static void
ngspice_progress (OreganoEngine *self, double *d)
{
	OreganoNgSpice *ngspice = OREGANO_NGSPICE (self);

	ngspice->priv->progress += 0.1;
	(*d) = ngspice->priv->progress;
}

static void
ngspice_stop (OreganoEngine *self)
{
	OreganoNgSpice *ngspice = OREGANO_NGSPICE (self);
	g_io_channel_shutdown (ngspice->priv->child_iochannel, TRUE, NULL);
	g_source_remove (ngspice->priv->child_iochannel_watch);
	g_spawn_close_pid (ngspice->priv->child_pid);
	g_spawn_close_pid (ngspice->priv->child_stdout);
}

static void
ngspice_watch_cb (GPid pid, gint status, OreganoNgSpice *ngspice)
{
	// check for status, see man waitpid(2)
	if (WIFEXITED (status)) {
		gchar *line;
		gsize len;
		g_io_channel_read_to_end (ngspice->priv->child_iochannel, &line, &len, NULL);
		if (len > 0) {
			fprintf (ngspice->priv->inputfp, "%s", line); 
		}
		g_free (line);

		// Free stuff
		g_io_channel_shutdown (ngspice->priv->child_iochannel, TRUE, NULL);
		g_source_remove (ngspice->priv->child_iochannel_watch);
		g_spawn_close_pid (ngspice->priv->child_pid);
		g_spawn_close_pid (ngspice->priv->child_stdout);
		g_io_channel_shutdown (ngspice->priv->child_ioerror, TRUE, NULL);
		g_source_remove (ngspice->priv->child_ioerror_watch);
		g_spawn_close_pid (ngspice->priv->child_error);

		ngspice->priv->current = NULL;
		fclose (ngspice->priv->inputfp);
		ngspice_parse (ngspice);

		if (ngspice->priv->num_analysis == 0) {
			schematic_log_append_error (ngspice->priv->schematic, 
			    _("### Too few or none analysis found ###\n"));
			ngspice->priv->aborted = TRUE;
			g_signal_emit_by_name (G_OBJECT (ngspice), "aborted");
		}
		else {
			g_signal_emit_by_name (G_OBJECT (ngspice), "done");
		}
	}
}

static gboolean
ngspice_child_stdout_cb (GIOChannel *source, GIOCondition condition, OreganoNgSpice *ngspice)
{
	gchar *line;
	gsize len, terminator;
	GIOStatus status;
	GError *error = NULL;

	status = g_io_channel_read_line (source, &line, &len, &terminator, &error);
	if ((status & G_IO_STATUS_NORMAL) && (len > 0)) {
		fprintf (ngspice->priv->inputfp, "%s", line); 
	}
	g_free (line);
	
	// Let UI update
	return g_main_context_iteration (NULL, FALSE);
}

static gboolean
ngspice_child_stderr_cb (GIOChannel *source, GIOCondition condition, OreganoNgSpice *ngspice)
{
	gchar *line;
	gsize len, terminator;
	GIOStatus status;
	GError *error = NULL;

	status = g_io_channel_read_line (source, &line, &len, &terminator, &error);
	if ((status & G_IO_STATUS_NORMAL) && (len > 0)) {
		schematic_log_append_error (ngspice->priv->schematic, line);
	}
	g_free (line);
	
	// Let UI update
	g_main_context_iteration (NULL, FALSE);
	return TRUE;
}

static void
ngspice_start (OreganoEngine *self)
{
	OreganoNgSpice *ngspice;
	GError *error = NULL;
	char *argv[] = {"ngspice", "-b", "/tmp/netlist.tmp", NULL};

	ngspice = OREGANO_NGSPICE (self);
	oregano_engine_generate_netlist (self, "/tmp/netlist.tmp", &error);
	if (error != NULL) {
		ngspice->priv->aborted = TRUE;
		schematic_log_append_error (ngspice->priv->schematic, error->message);
		g_signal_emit_by_name (G_OBJECT (ngspice), "aborted");
		g_error_free (error);
		return;
	}
	
	// Open the file storing the output of ngspice
	ngspice->priv->inputfp = fopen ("/tmp/netlist.lst", "w");
	
	error = NULL;
	if (g_spawn_async_with_pipes (
			NULL, // Working directory
			argv,
			NULL,
			G_SPAWN_DO_NOT_REAP_CHILD | G_SPAWN_SEARCH_PATH,
			NULL,
			NULL,
			&ngspice->priv->child_pid,
			NULL, // STDIN
			&ngspice->priv->child_stdout, // STDOUT
			&ngspice->priv->child_error,  // STDERR
			&error)) {
		// Add a watch for process status
		g_child_watch_add (ngspice->priv->child_pid, (GChildWatchFunc)ngspice_watch_cb, ngspice);
		// Add a GIOChannel to read from process stdout
		ngspice->priv->child_iochannel = g_io_channel_unix_new (ngspice->priv->child_stdout);
		// Watch the I/O Channel to read child strout
		ngspice->priv->child_iochannel_watch = g_io_add_watch (ngspice->priv->child_iochannel,
			G_IO_IN|G_IO_PRI|G_IO_HUP|G_IO_NVAL, (GIOFunc)ngspice_child_stdout_cb, ngspice);
		// Add a GIOChannel to read from process stderr
		ngspice->priv->child_ioerror = g_io_channel_unix_new (ngspice->priv->child_error);
		// Watch the I/O error channel to read the child sterr
		ngspice->priv->child_ioerror_watch = g_io_add_watch (ngspice->priv->child_ioerror,
			G_IO_IN|G_IO_PRI|G_IO_HUP|G_IO_NVAL, (GIOFunc)ngspice_child_stderr_cb, ngspice);
		
	} 
	else {
		ngspice->priv->aborted = TRUE;
		schematic_log_append_error (ngspice->priv->schematic, _("Unable to execute NgSpice."));
		g_signal_emit_by_name (G_OBJECT (ngspice), "aborted");
	}
}

static GList*
ngspice_get_results (OreganoEngine *self)
{
	if (OREGANO_NGSPICE (self)->priv->analysis == NULL)
		printf ("pas d'analyse\n");
	return OREGANO_NGSPICE (self)->priv->analysis;
}

static gchar *
ngspice_get_operation (OreganoEngine *self)
{
	OreganoNgSpicePriv *priv = OREGANO_NGSPICE (self)->priv;

	if (priv->current == NULL)
		return _("None");

	return oregano_engine_get_analysis_name (priv->current);
}

static void
ngspice_interface_init (gpointer g_iface, gpointer iface_data)
{
	OreganoEngineClass *klass = (OreganoEngineClass *)g_iface;
	klass->start = ngspice_start;
	klass->stop = ngspice_stop;
	klass->progress = ngspice_progress;
	klass->get_netlist = ngspice_generate_netlist;
	klass->has_warnings = ngspice_has_warnings;
	klass->get_results = ngspice_get_results;
	klass->get_operation = ngspice_get_operation;
	klass->is_available = ngspice_is_available;
}

static void
ngspice_instance_init (GTypeInstance *instance, gpointer g_class)
{
	OreganoNgSpice *self = OREGANO_NGSPICE (instance);

	self->priv = g_new0 (OreganoNgSpicePriv, 1);
	self->priv->progress = 0.0;
	self->priv->char_last_newline = TRUE;
	self->priv->status = 0;
	self->priv->buf_count = 0;
	self->priv->num_analysis = 0;
	self->priv->analysis = NULL;
	self->priv->current = NULL;
	self->priv->aborted = FALSE;
}

OreganoEngine*
oregano_ngspice_new (Schematic *sc)
{
	OreganoNgSpice *ngspice;

	ngspice = OREGANO_NGSPICE (g_object_new (OREGANO_TYPE_NGSPICE, NULL));
	ngspice->priv->schematic = sc;

	return OREGANO_ENGINE (ngspice);
}
