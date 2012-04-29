/*
 * simulation.c
 *
 *
 * Authors:
 *	Richard Hult <rhult@hem.passagen.se>
 *	Ricardo Markiewicz <rmarkie@fi.uba.ar>
 *	Andres de Barbara <adebarbara@fi.uba.ar>
 *  Marc Lorber <lorber.marc@wanadoo.fr>
 *
 * Web page: https://github.com/marc-lorber/oregano
 *
 * Copyright (C) 1999-2001	Richard Hult
 * Copyright (C) 2003,2006	Ricardo Markiewicz
 * Copyright (C) 2009-2012  Marc Lorber
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
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
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

typedef struct {
	Schematic 		*sm;
	SchematicView 	*sv;
	GtkDialog 		*dialog;
	OreganoEngine 	*engine;
	GtkProgressBar 	*progress;
	GtkLabel 		*progress_label;
	int 			 progress_timeout_id;
} Simulation;

static int progress_bar_timeout_cb (Simulation *s);
static void cancel_cb (GtkWidget *widget, gint arg1, Simulation *s);
static void engine_done_cb (OreganoEngine *engine, Simulation *s);
static void engine_aborted_cb (OreganoEngine *engine, Simulation *s);
static gboolean simulate_cmd (Simulation *s);

static int
delete_event_cb (GtkWidget *widget, GdkEvent *event, gpointer data)
{
	return FALSE;
}

gpointer
simulation_new (Schematic *sm)
{
	Simulation *s;

	s = g_new0 (Simulation, 1);
	s->sm = sm;
	s->sv = NULL;

	return s;
}

void
simulation_show (GtkWidget *widget, SchematicView *sv)
{
	GtkWidget *w;
	GtkBuilder *gui;
	GError *perror = NULL;
	Simulation *s;
	Schematic *sm;

	g_return_if_fail (sv != NULL);

	if ((gui = gtk_builder_new ()) == NULL) {
		oregano_error (_("Could not create simulation dialog"));
		return;
	} 
	else gtk_builder_set_translation_domain (gui, NULL);
	sm = schematic_view_get_schematic (sv);
	s = schematic_get_simulation (sm);

	// Only allow one instance of the dialog box per schematic.
	if (s->dialog) {
		gdk_window_raise (gtk_widget_get_window (GTK_WIDGET (s->dialog)));
		return;
	}

	if (!g_file_test (OREGANO_UIDIR "/simulation.ui",
		G_FILE_TEST_EXISTS)) {
		oregano_error (_("Could not create simulation dialog"));
		return;
	}

	if (gtk_builder_add_from_file (gui, OREGANO_UIDIR "/simulation.ui", 
	    &perror) <= 0) {
			gchar *msg;
		msg = perror->message;
		oregano_error_with_title (_("Could not create simulation dialog"), msg);
		g_error_free (perror);
		return;
	}

	w = GTK_WIDGET (gtk_builder_get_object (gui, "toplevel"));
	if (!w) {
		oregano_error (_("Could not create simulation dialog"));
		return;
	}

	s->dialog = GTK_DIALOG (w);
	g_signal_connect (G_OBJECT (w), "delete_event", 
		G_CALLBACK (delete_event_cb), s);

	w = GTK_WIDGET (gtk_builder_get_object (gui, "progressbar"));
	s->progress = GTK_PROGRESS_BAR (w);
	gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (s->progress), 0.0);

	w = GTK_WIDGET (gtk_builder_get_object (gui, "progress_label"));
	s->progress_label = GTK_LABEL (w);

	g_signal_connect (G_OBJECT (s->dialog), "response", 
		G_CALLBACK (cancel_cb), s);

	gtk_widget_show_all (GTK_WIDGET (s->dialog));

	s->sv = sv;
	simulate_cmd (s);
}

static int
progress_bar_timeout_cb (Simulation *s)
{
	gchar *str;
	double p;
	g_return_val_if_fail (s != NULL, FALSE);

	oregano_engine_get_progress (s->engine, &p);

	if (p >= 1) p = 0;

	gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (s->progress), p);

	str = g_strdup_printf (_("Progress: <b>%s</b>"),
		oregano_engine_get_current_operation (s->engine));
	gtk_label_set_markup (s->progress_label, str);
	g_free (str);

	return TRUE;
}

static void
engine_done_cb (OreganoEngine *engine, Simulation *s)
{
	if (s->progress_timeout_id != 0) {
		g_source_remove (s->progress_timeout_id);
		s->progress_timeout_id = 0;

		// Make sure that the progress bar is completed, just for good looks.
		gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (s->progress), 1.0);
	}

	gtk_widget_destroy (GTK_WIDGET (s->dialog));
	s->dialog = NULL;

	plot_show (s->engine);

	if (oregano_engine_has_warnings (s->engine))
		schematic_view_log_show (s->sv, FALSE);

	sheet_clear_op_values (schematic_view_get_sheet (s->sv));

	// I don't need the engine anymore. The plot window owns its reference to 
	// the engine
	g_object_unref (s->engine);
	s->engine = NULL;

}

static void
engine_aborted_cb (OreganoEngine *engine, Simulation *s)
{
	GtkWidget *dialog;
	int answer;

	if (s->progress_timeout_id != 0) {
		g_source_remove (s->progress_timeout_id);
		s->progress_timeout_id = 0;
	}

	gtk_widget_destroy (GTK_WIDGET (s->dialog));
	s->dialog = NULL;

	if (!schematic_view_get_log_window_exists (s->sv)) {
		dialog = gtk_message_dialog_new_with_markup (
		    GTK_WINDOW (schematic_view_get_toplevel (s->sv)),
			GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_MESSAGE_ERROR,
			GTK_BUTTONS_YES_NO,
			_("<span weight=\"bold\" size=\"large\">The simulation was aborted"
			" due to an error.</span>\n\nWould you like to view the error log?"));

		answer = gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);

		if (answer == GTK_RESPONSE_YES) {
			schematic_view_log_show (s->sv, TRUE);
		}
	} 
	else {
		oregano_error (_("The simulation was aborted due to an error"));

		schematic_view_log_show (s->sv, FALSE);
	}
}

static void
cancel_cb (GtkWidget *widget, gint arg1, Simulation *s)
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
}

static gboolean
simulate_cmd (Simulation *s)
{
	OreganoEngine *engine;

	if (s->engine != NULL) {
		g_object_unref (G_OBJECT (s->engine));
		s->engine = NULL;
	}

	engine = oregano_engine_factory_create_engine (oregano.engine, s->sm);
	s->engine = engine;

	s->progress_timeout_id = g_timeout_add (100,
	                                   (GSourceFunc)progress_bar_timeout_cb, s);

	g_signal_connect (G_OBJECT (engine), "done", 
		G_CALLBACK (engine_done_cb), s);
	g_signal_connect (G_OBJECT (engine), "aborted", 
	    G_CALLBACK (engine_aborted_cb), s);

	oregano_engine_start (engine);

	return TRUE;
}

