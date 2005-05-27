/*
 * settings.c
 *
 *
 * Authors:
 *  Richard Hult <rhult@hem.passagen.se>
 *  Ricardo Markiewicz <rmarkie@fi.uba.ar>
 *  Andres de Barbara <adebarbara@fi.uba.ar>
 *
 * Web page: http://arrakis.lug.fi.uba.ar/
 *
 * Copyright (C) 1999-2001  Richard Hult
 * Copyright (C) 2003,2005  LUGFI
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

#include <config.h>
#include <unistd.h>
#include <glib.h>
#include <gnome.h>
#include <glade/glade.h>
#include <stdio.h>
#include "main.h"
#include "string.h"
#include "settings.h"
#include "schematic.h"
#include "schematic-view.h"
#include "dialogs.h"
#include "oregano-utils.h"
#include "oregano-config.h"

typedef struct {
	Schematic *sm;
	GtkWidget *pbox; /* Property box */

	GtkWidget *ok_bt;
	GtkWidget *cancel_bt;

	GtkWidget *w_show_splash;
	GtkWidget *w_show_log;

	GtkWidget *w_engine_path;
	GtkWidget *w_compress_files;
	GtkWidget *w_engine;
} Settings;
#define SETTINGS(x) ((Settings*)(x))

static void
remove_item_cb ( GtkWidget *widget, GnomeFileEntry *entry) {
/*
   GtkList *lst;
   GList   *sel;
   gchar   *txt;

   txt = (gchar*) gtk_entry_get_text ( GTK_ENTRY (gnome_entry_gtk_entry (entry)));
   lst   =  GTK_LIST ( entry->combo.list );
   if ( lst ) {
      sel = g_list_copy (lst->selection);
      if ( sel ) {
	 gtk_list_remove_items(lst,sel);
	 g_list_free (sel);
#ifdef GNOME_CONVERSION_COMPLETE
	 sel = entry->items;
	 for ( ;sel;sel = sel->next) {
		 if ( ! strcmp(txt,sel->data) )
			 g_list_remove (entry->items,sel->data);
	 }
#endif
      }
   }*/
}

static void
apply_callback (GtkWidget *w, Settings *s)
{
	oregano.simtype = g_strdup ((gchar*) gtk_widget_get_name ( GTK_WIDGET( s->w_engine )));
	oregano.simexec = gtk_editable_get_chars (									 			GTK_EDITABLE (gnome_file_entry_gtk_entry (GNOME_FILE_ENTRY (s->w_engine_path))), 0, -1);
	oregano.compress_files = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (s->w_compress_files));
	oregano.show_log = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (s->w_show_log ));
	oregano.show_splash = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (s->w_show_splash ));

	oregano_config_save ();

	gtk_widget_destroy (s->pbox);
	s->pbox = NULL;
}

static void
set_engine_name( GtkWidget *w, Settings *s)
{
	const gchar * type = g_strdup (gtk_widget_get_name (GTK_WIDGET (w)));
	s->w_engine = w;
	oregano_config_save ();
}

static gint
close_callback (GtkWidget *widget, Settings *s)
{
	gtk_widget_destroy (s->pbox);
	s->pbox = NULL;

	return FALSE;
}

static gint
cancel_event_cb (GtkWidget *widget, GdkEvent *event, gpointer data)
{
	return FALSE;
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
	GtkWidget *w, *pbox, *toplevel;
	GtkWidget *w0;
	GladeXML *gui = NULL;
	Settings *s;
	Schematic *sm;

	g_return_if_fail (sv != NULL);

	sm = schematic_view_get_schematic (sv);
	s = schematic_get_settings (sm);

	/* Only allow one instance of the property box per schematic. */
	if (GTK_WIDGET(SETTINGS (s)->pbox)){
		gdk_window_raise (GTK_WIDGET (SETTINGS (s)->pbox)->window);
		return;
	}

	if (!g_file_test (OREGANO_GLADEDIR "/settings.glade2", G_FILE_TEST_EXISTS)) {
		oregano_error (_("<span size=\"x-large\" weight=\"bold\">Could not create settings dialog.</span>"));
		return;
	}

	gui = glade_xml_new (OREGANO_GLADEDIR "/settings.glade2", NULL, "toplevel");
	if (!gui) {
		oregano_error (_("<span size=\"x-large\" weight=\"bold\">Could not create settings dialog.</span>"));
		return;
	}

	w = toplevel = glade_xml_get_widget (gui, "toplevel");
	if (!w) {
		oregano_error (_("<span size=\"x-large\" weight=\"bold\">Could not create settings dialog.</span>"));
		return;
	}

	pbox = toplevel;
	s->pbox = GTK_WIDGET (pbox);

	w = glade_xml_get_widget (gui, "ok_bt");
	g_signal_connect (G_OBJECT (w), "clicked",
		G_CALLBACK (apply_callback), s);

	w = glade_xml_get_widget (gui, "cancel_bt");
	g_signal_connect (G_OBJECT (w), "clicked",
		G_CALLBACK (close_callback), s);

	/* Setup callbacks. */
#if (GTK_MINOR_VERSION >= 4)
	//w = gnome_file_entry_new ("EnginePath", "Engine path");
	w = glade_xml_get_widget (gui, "engine-path-entry");
	gtk_entry_set_text (GTK_ENTRY (gnome_file_entry_gtk_entry (GNOME_FILE_ENTRY (w))), oregano.simexec);
#else
	w = glade_xml_get_widget (gui, "engine-path-entry");
	gnome_file_entry_set_title (GNOME_FILE_ENTRY (w),
								"Path to simulation engine executable");
	gtk_entry_set_text (GTK_ENTRY (gnome_file_entry_gtk_entry (GNOME_FILE_ENTRY (w))),oregano.simexec);
#endif
	s->w_engine_path = w;

	w = glade_xml_get_widget (gui, "splash-enable");
	s->w_show_splash = w;
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w), oregano.show_splash);

	w = glade_xml_get_widget (gui, "compress-enable");
	s->w_compress_files = w;
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w),
								  oregano.compress_files);

	w = glade_xml_get_widget (gui, "log-enable");
	s->w_show_log = w;
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w), oregano.show_log);

	w = glade_xml_get_widget (gui, "grid-size");
	gtk_widget_set_sensitive (w, FALSE);
	w = glade_xml_get_widget (gui, "realtime-enable");
	gtk_widget_set_sensitive (w, FALSE);

#if (GTK_MINOR_VERSION >= 4)
	w = glade_xml_get_widget (gui, "library-path-entry");
//	w = gnome_file_entry_new ("LibrariesPath", "Library path");
	w0 = gnome_file_entry_gtk_entry (GNOME_FILE_ENTRY (w));
#else
	w = glade_xml_get_widget (gui, "library-path-entry");
	gnome_file_entry_gtk_entry (GNOME_FILE_ENTRY (w));
	w0 = gnome_file_entry_gnome_entry (GNOME_FILE_ENTRY (w));
	gnome_entry_set_history_id (GNOME_FILE_ENTRY (w0),"LibrariesPath");
	gnome_entry_set_max_saved (GNOME_FILE_ENTRY (w0), 100 );
#endif
	gtk_widget_set_sensitive (w, FALSE);

	w = glade_xml_get_widget (gui, "btn_remove_lib_path");
	gtk_widget_set_sensitive (w, FALSE);
	g_signal_connect (G_OBJECT (w), "clicked", G_CALLBACK (remove_item_cb), w0);

#if (GTK_MINOR_VERSION >= 4)
	w = glade_xml_get_widget (gui, "model-path-entry");
//	w = gnome_file_entry_new ("ModelsPath", "Model path");
	w0 = gnome_file_entry_gtk_entry (GNOME_FILE_ENTRY (w));
#else
	w = glade_xml_get_widget (gui, "model-path-entry");
	w0 = gnome_file_entry_gnome_entry (GNOME_FILE_ENTRY (w));
	gnome_entry_set_history_id(GNOME_FILE_ENTRY (w0),"Models Path");
	gnome_entry_set_max_saved (GNOME_FILE_ENTRY (w0), 100 );
#endif
	gtk_widget_set_sensitive (w, FALSE);

	w = glade_xml_get_widget (gui, "btn_remove_model_path");
	gtk_widget_set_sensitive (w, FALSE);
	g_signal_connect (G_OBJECT (w), "clicked", G_CALLBACK (remove_item_cb), w0);

	w = glade_xml_get_widget (gui, "gnucap");
	g_signal_connect (G_OBJECT (w), "clicked", G_CALLBACK (set_engine_name), s);

	if (!strcmp (oregano.simtype,"gnucap")) {
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w), TRUE);
		s->w_engine = w;
	}

	w = glade_xml_get_widget (gui, "ngspice");
	g_signal_connect (G_OBJECT (w), "clicked", G_CALLBACK (set_engine_name), s);

	if (!strcmp (oregano.simtype,"ngspice")) {
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w), TRUE);
		s->w_engine = w;
	}
	
	gtk_widget_show_all (toplevel);
}
