/*
 * settings.c
 *
 *
 * Authors:
 *  Richard Hult <rhult@hem.passagen.se>
 *  Ricardo Markiewicz <rmarkie@fi.uba.ar>
 *  Andres de Barbara <adebarbara@fi.uba.ar>
 *  Marc Lorber <lorber.marc@wanadoo.fr>
 *
 * Web page: https://github.com/marc-lorber/oregano
 *
 * Copyright (C) 1999-2001  Richard Hult
 * Copyright (C) 2003,2006  Ricardo Markiewicz
 * Copyright (C) 2008-2012  Marc Lorber
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

#include <unistd.h>
#include <glib.h>
#include <stdio.h>
#include <glib/gi18n.h>

#include "oregano.h"
#include "string.h"
#include "settings.h"
#include "schematic.h"
#include "schematic-view.h"
#include "dialogs.h"
#include "oregano-utils.h"
#include "oregano-config.h"

// Engines Types
static const gchar*
engine[] = {
	"gnucap",
	"ngspice"
};

typedef struct {
	Schematic *sm;
	GtkWidget *pbox; // Property box

	GtkWidget *w_show_splash;
	GtkWidget *w_show_log;

	GtkWidget *w_compress_files;
	GtkWidget *w_engine;
} Settings;

#define SETTINGS(x) ((Settings*)(x))

GtkWidget *engine_path;	
GtkWidget *button[2];


static void
apply_callback (GtkWidget *w, Settings *s)
{
	oregano.engine = GPOINTER_TO_INT (
	                  g_object_get_data (G_OBJECT (s->w_engine), "id"));
	oregano.compress_files = gtk_toggle_button_get_active (
	                  GTK_TOGGLE_BUTTON (s->w_compress_files));
	oregano.show_log = gtk_toggle_button_get_active (
	                  GTK_TOGGLE_BUTTON (s->w_show_log ));
	oregano.show_splash = gtk_toggle_button_get_active (
	                  GTK_TOGGLE_BUTTON (s->w_show_splash ));

	oregano_config_save ();

	gtk_widget_destroy (s->pbox);
	s->pbox = NULL;
}

static void
delete_event_callback (GtkWidget *w, GdkEvent *event, Settings *s)
{
    apply_callback(w, s);
}

static void
set_engine_name (GtkWidget *w, Settings *s)
{
	int engine_id;

	s->w_engine = w; 
	engine_id = GPOINTER_TO_INT (
	               g_object_get_data (G_OBJECT (s->w_engine), "id"));
	if (g_find_program_in_path (engine[engine_id]) == NULL) {
		if (gtk_toggle_button_get_active (
		                GTK_TOGGLE_BUTTON (button[engine_id]))) {
			GString *msg = g_string_new (
			               _("Engine <span weight=\"bold\" size=\"large\">"));
			msg = g_string_append (msg, engine[engine_id]);
			msg = g_string_append (msg, 
			              _("</span> not found\nThe engine is unable to locate "
			                "the external program."));
			oregano_warning_with_title (_("Warning"), msg->str);
			g_string_free (msg, TRUE);
			engine_id = (engine_id +1) % 2;
			gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button[engine_id]),
			                              TRUE);
		}	
		else
			gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button[engine_id]),
		                                  FALSE);
	}
}

gpointer
settings_new (Schematic *sm)
{
	Settings *s;

	s = g_new0 (Settings, 1);
	s->sm = sm;

	return s;
}

void
settings_show (GtkWidget *widget, SchematicView *sv)
{
	gint i;
	GtkWidget *engine_group = NULL;
	GtkWidget *w, *pbox, *toplevel;
	GtkBuilder *gui;
	GError *perror = NULL;
	gchar *msg;
	Settings *s;
	Schematic *sm;

	g_return_if_fail (sv != NULL);
	
	// If no engine available, stop oregano
	if ((g_find_program_in_path (engine[0]) == NULL) &&
	    (g_find_program_in_path (engine[1]) == NULL)) {
		gchar *msg;
		msg = g_strdup_printf (
		_("No engine allowing analysis is available.\n"
		  "You might install one, at least! \n"
		  "Either ngspice or gnucap."));
		oregano_error_with_title (_("Could not create settings dialog"), msg);
		g_free (msg);
		return;
	}
	
	g_return_if_fail (sv != NULL);

	if ((gui = gtk_builder_new ()) == NULL) {
		oregano_error (_("Could not create settings dialog"));
		return;
	} 
	else 
		gtk_builder_set_translation_domain (gui, NULL);

	sm = schematic_view_get_schematic (sv);
	s = schematic_get_settings (sm);

	// Only allow one instance of the property box per schematic.
	if (GTK_WIDGET (SETTINGS (s)->pbox)) {
		gdk_window_raise (gtk_widget_get_window (
		                   GTK_WIDGET (SETTINGS (s)->pbox)));
		return;
	}
	
	if (!g_file_test (OREGANO_UIDIR "/settings.ui", G_FILE_TEST_EXISTS)) {
		msg = g_strdup_printf (
			_("The file %s could not be found. "
			  "You might need to reinstall Oregano to fix this."),
			OREGANO_UIDIR "/settings.ui");
		oregano_error_with_title (_("Could not create settings dialog"), msg);
		g_free (msg);
		return;
	}
	
	if (gtk_builder_add_from_file (gui, OREGANO_UIDIR "/settings.ui", 
	    &perror) <= 0) {
		msg = perror->message;
		oregano_error_with_title (_("Could not create settings dialog"), msg);
		g_error_free (perror);
		return;
	}	

	w = toplevel = GTK_WIDGET (gtk_builder_get_object (gui, "toplevel"));
	if (!w) {
		oregano_error (_("Could not create settings dialog"));
		return;
	}
	g_signal_connect (G_OBJECT (w), "delete_event",
			  G_CALLBACK (delete_event_callback), s);

	pbox = toplevel;
	s->pbox = GTK_WIDGET (pbox);

	w = GTK_WIDGET (gtk_builder_get_object (gui, "close_bt"));
	g_signal_connect (G_OBJECT (w), "clicked",
		G_CALLBACK (apply_callback), s);

	w = GTK_WIDGET (gtk_builder_get_object (gui, "splash-enable"));
	s->w_show_splash = w;
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w), oregano.show_splash);

	w = GTK_WIDGET (gtk_builder_get_object (gui, "compress-enable"));
	s->w_compress_files = w;
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w),
								  oregano.compress_files);
	w = GTK_WIDGET (gtk_builder_get_object (gui, "log-enable"));
	s->w_show_log = w;
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w), oregano.show_log);

	w = GTK_WIDGET (gtk_builder_get_object (gui, "grid-size"));
	gtk_widget_set_sensitive (w, FALSE);
	w = GTK_WIDGET (gtk_builder_get_object (gui, "realtime-enable"));
	gtk_widget_set_sensitive (w, FALSE);

	w = GTK_WIDGET (gtk_builder_get_object (gui, "engine_table"));
	for (i = 0; i < OREGANO_ENGINE_COUNT; i++) {
		if (engine_group)
			button[i] = gtk_radio_button_new_with_label_from_widget (
			      GTK_RADIO_BUTTON (engine_group), engine[i]);
		else
			button[i] = engine_group = 
		          gtk_radio_button_new_with_label_from_widget (NULL, engine[i]);

		g_object_set_data (G_OBJECT (button[i]), "id", GUINT_TO_POINTER (i));

		gtk_table_attach (GTK_TABLE (w), button[i], 0, 1, i, i+1, 
						  GTK_EXPAND|GTK_FILL, GTK_SHRINK, 6, 6);
		g_signal_connect (G_OBJECT (button[i]), "clicked", 
						  G_CALLBACK (set_engine_name), s);

	}
	
	// Is the engine available?
	// In that case the button is active
	if (g_find_program_in_path (engine[oregano.engine]) != NULL)
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button[oregano.engine]), 
		                              TRUE);
	// Otherwise the button is inactive
	else
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button[oregano.engine]), 
		                              FALSE);

	// If no engine available, stop oregano
	if ((g_find_program_in_path (engine[0]) == NULL) &&
	    (g_find_program_in_path (engine[1]) == NULL)) {
			gchar *msg;
			msg = g_strdup_printf (
			_("No engine allowing analysis is available.\n"
			  "You might install one, at least! \n"
			  "Either ngspice or gnucap."));
			oregano_error_with_title (_("Could not create settings dialog"), msg);
			g_free (msg);
	}

	gtk_widget_show_all (toplevel);
}
