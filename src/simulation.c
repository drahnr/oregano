/*
 * simulation.c
 *
 *
 * Authors:
 *  Richard Hult <rhult@hem.passagen.se>
 *  Ricardo Markiewicz <rmarkie@fi.uba.ar>
 *  Andres de Barbara <adebarbara@fi.uba.ar>
 *  Marc Lorber <lorber.marc@wanadoo.fr>
 *  Bernhard Schuster <bernhard@ahoi.io>
 *
 * Web page: https://ahoi.io/project/oregano
 *
 * Copyright (C) 1999-2001  Richard Hult
 * Copyright (C) 2003,2006  Ricardo Markiewicz
 * Copyright (C) 2009-2012  Marc Lorber
 * Copyright (C) 2013-2014  Bernhard Schuster
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include <gtk/gtk.h>
#include <locale.h>
#include <glib/gi18n.h>

#include "oregano.h"
#include "simulation.h"
#include "schematic.h"
#include "schematic-view.h"
#include "dialogs.h"
#include "oregano-utils.h"
#include "oregano-config.h"
#include "plot.h"
#include "gnucap.h"
#include "log.h"

//NULL terminated
const char *SimulationFunctionTypeString[] = {
		"Subtraction",
		"Division",
		NULL
};

typedef struct
{
	Schematic *sm;
	SchematicView *sv;
	GtkDialog *dialog;
	OreganoEngine *engine;
	GtkProgressBar *progress_solver;
	GtkLabel *progress_label_solver;
	GtkProgressBar *progress_reader;
	GtkLabel *progress_label_reader;
	int progress_timeout_id;
	Log *logstore;
} Simulation;

static int progress_bar_timeout_cb (Simulation *s);
static void cancel_cb (GtkWidget *widget, gint arg1, Simulation *s);
static void engine_done_cb (OreganoEngine *engine, Simulation *s);
static void engine_aborted_cb (OreganoEngine *engine, Simulation *s);
static gboolean simulate_cmd (Simulation *s);

static int delete_event_cb (GtkWidget *widget, GdkEvent *event, gpointer data) { return FALSE; }

gpointer simulation_new (Schematic *sm, Log *logstore)
{
	Simulation *s;

	s = g_new0 (Simulation, 1);
	s->sm = sm;
	s->sv = NULL;
	s->logstore = logstore;

	return s;
}

void simulation_show_progress_bar (GtkWidget *widget, SchematicView *sv)
{
	GtkWidget *w;
	GtkBuilder *gui;
	GError *e = NULL;
	Simulation *s;
	Schematic *sm;

	g_return_if_fail (sv != NULL);

	if (oregano.engine < 0 || oregano.engine >= OREGANO_ENGINE_COUNT)
		return;

	sm = schematic_view_get_schematic (sv);
	s = schematic_get_simulation (sm);

	if ((gui = gtk_builder_new ()) == NULL) {
		log_append (s->logstore, _ ("Simulation"),
		            _ ("Could not create simulation dialog - Builder creation failed."));
		return;
	}
	gtk_builder_set_translation_domain (gui, NULL);

	// Only allow one instance of the dialog box per schematic.
	if (s->dialog) {
		gdk_window_raise (gtk_widget_get_window (GTK_WIDGET (s->dialog)));
		return;
	}

	if (gtk_builder_add_from_file (gui, OREGANO_UIDIR "/simulation.ui", &e) <= 0) {
		log_append_error (s->logstore, _ ("Simulation"), _ ("Could not create simulation dialog"),
		                  e);
		g_clear_error (&e);
		return;
	}

	w = GTK_WIDGET (gtk_builder_get_object (gui, "toplevel"));
	if (!w) {
		log_append (s->logstore, _ ("Simulation"),
		            _ ("Could not create simulation dialog - .ui file lacks widget "
		               "called \"toplevel\"."));
		return;
	}

	s->dialog = GTK_DIALOG (w);
	g_signal_connect (G_OBJECT (w), "delete_event", G_CALLBACK (delete_event_cb), s);

	/**
	 * progress bars and progress labels
	 */
	w = GTK_WIDGET (gtk_builder_get_object (gui, "progressbar_solver"));
	s->progress_solver = GTK_PROGRESS_BAR (w);
	gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (s->progress_solver), 0.0);

	w = GTK_WIDGET (gtk_builder_get_object (gui, "progress_label_solver"));
	s->progress_label_solver = GTK_LABEL (w);

	w = GTK_WIDGET (gtk_builder_get_object (gui, "progressbar_reader"));
	s->progress_reader = GTK_PROGRESS_BAR (w);
	gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (s->progress_reader), 0.0);

	w = GTK_WIDGET (gtk_builder_get_object (gui, "progress_label_reader"));
	s->progress_label_reader = GTK_LABEL (w);

	g_signal_connect (G_OBJECT (s->dialog), "response", G_CALLBACK (cancel_cb), s);

	gtk_widget_show_all (GTK_WIDGET (s->dialog));

	s->sv = sv;

	simulate_cmd (s);
}

static int progress_bar_timeout_cb (Simulation *s)
{
	g_return_val_if_fail (s != NULL, FALSE);

	double p = 0;
	oregano_engine_get_progress_solver (s->engine, &p);

	gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (s->progress_solver), p);

	gchar *current_operation = oregano_engine_get_current_operation_solver(s->engine);
	gchar *str = g_strdup_printf (_ ("Progress: <b>%s</b>"), current_operation);
	g_free(current_operation);

	gtk_label_set_markup (s->progress_label_solver, str);
	g_free (str);

	p = 0;
	oregano_engine_get_progress_reader (s->engine, &p);

	gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (s->progress_reader), p);

	str = g_strdup_printf (_ ("Progress: <b>%s</b>"),
	                       oregano_engine_get_current_operation_reader (s->engine));
	gtk_label_set_markup (s->progress_label_reader, str);
	g_free (str);

	return TRUE;
}

static void engine_done_cb (OreganoEngine *engine, Simulation *s)
{
	if (s->progress_timeout_id != 0) {
		g_source_remove (s->progress_timeout_id);
		s->progress_timeout_id = 0;

		// Make sure that the progress bar is completed, just for good looks.
		gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (s->progress_solver), 1.0);
		gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (s->progress_reader), 1.0);
	}

	gtk_widget_destroy (GTK_WIDGET (s->dialog));
	s->dialog = NULL;

	plot_show (s->engine);

	if (oregano_engine_has_warnings (s->engine)) {
		log_append (s->logstore, _ ("Simulation"),
		            _ ("Finished with warnings:")); // FIXME add actual warnings
	} else {
		log_append (s->logstore, _ ("Simulation"), _ ("Finished."));
	}

	// show log window if this is enabled in preferences
	schematic_view_log_show (s->sv, FALSE);

	sheet_clear_op_values (schematic_view_get_sheet (s->sv));

	// I don't need the engine anymore. The plot window owns its reference to
	// the engine
	g_object_unref (s->engine);
	s->engine = NULL;
}

static void engine_aborted_cb (OreganoEngine *engine, Simulation *s)
{
	if (s->progress_timeout_id != 0) {
		g_source_remove (s->progress_timeout_id);
		s->progress_timeout_id = 0;
	}

	//	g_clear_object (&(s->dialog));
	if (s->dialog != NULL) {
		gtk_widget_destroy (GTK_WIDGET (s->dialog));
		s->dialog = NULL;
	}

	log_append (s->logstore, _ ("Simulation"), _ ("Aborted. See lines below for details."));
	if (s->sv != NULL)
		schematic_view_log_show (s->sv, TRUE);

	g_object_unref (s->engine);
	s->engine = NULL;
}

static void cancel_cb (GtkWidget *widget, gint arg1, Simulation *s)
{
	g_return_if_fail (s != NULL);

	if (s->progress_timeout_id != 0) {
		g_source_remove (s->progress_timeout_id);
		s->progress_timeout_id = 0;
	}

	if (s->engine)
		oregano_engine_stop (s->engine);

	gtk_widget_destroy (GTK_WIDGET (s->dialog));
	s->dialog = NULL;
	s->sv = NULL;

	log_append (s->logstore, _ ("Simulation"), _ ("Canceled."));
}

static gboolean simulate_cmd (Simulation *s)
{
	OreganoEngine *engine;

	if (s->engine != NULL) {
		g_clear_object (&(s->engine));
	}

	engine = oregano_engine_factory_create_engine (oregano.engine, s->sm);
	if (!engine)
		return FALSE;

	s->engine = engine;

	s->progress_timeout_id = g_timeout_add (250, (GSourceFunc)progress_bar_timeout_cb, s);

	g_signal_connect (G_OBJECT (engine), "done", G_CALLBACK (engine_done_cb), s);
	g_signal_connect (G_OBJECT (engine), "aborted", G_CALLBACK (engine_aborted_cb), s);

	oregano_engine_start (engine);

	return TRUE;
}
