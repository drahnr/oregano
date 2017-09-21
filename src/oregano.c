/*
 * oregano.c
 *
 *
 * Authors:
 *  Richard Hult <rhult@hem.passagen.se>
 *  Ricardo Markiewicz <rmarkie@fi.uba.ar>
 *  Andres de Barbara <adebarbara@fi.uba.ar>
 *  Marc Lorber <lorber.marc@wanadoo.fr>
 *  Bernhard Schuster <bernhard@ahoi.io>
 *  Guido Trentalancia <guido@trentalancia.com>
 *
 * Web page: https://ahoi.io/project/oregano
 *
 * Oregano, a tool for schematic capture and simulation of electronic
 * circuits.
 *
 * Copyright (C) 1999-2001  Richard Hult
 * Copyright (C) 2003,2006  Ricardo Markiewicz
 * Copyright (C) 2009-2012  Marc Lorber
 * Copyright (C) 2013       Bernhard Schuster
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

#define ENABLE_NLS 1
#define HAVE_BIND_TEXTDOMAIN_CODESET 1
#define HAVE_GETTEXT 1
#define HAVE_LC_MESSAGES 1

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <locale.h>
#include <signal.h>
#include <gio/gdesktopappinfo.h>

#include "dialogs.h"
#include "engine.h"
#include "schematic.h"
#include "schematic-view.h"
#include "cursors.h"
#include "load-library.h"
#include "load-schematic.h"
#include "load-common.h"
#include "oregano-config.h"
#include "stock.h"
#include "oregano.h"
#include "splash.h"

#include <libintl.h>

G_DEFINE_TYPE (Oregano, oregano, GTK_TYPE_APPLICATION);

OreganoApp oregano;
Oregano *app;
int oregano_debugging;

static void oregano_application (GApplication *app, GFile *file);
static void oregano_activate (GApplication *application);
static void oregano_open (GApplication *application, GFile **files, gint n_files,
                          const gchar *hint);

static void oregano_finalize (GObject *object)
{
	cursors_shutdown ();
	G_OBJECT_CLASS (oregano_parent_class)->finalize (object);
}

static void oregano_class_init (OreganoClass *klass)
{
	G_APPLICATION_CLASS (klass)->activate = oregano_activate;
	G_APPLICATION_CLASS (klass)->open = oregano_open;
	G_OBJECT_CLASS (klass)->finalize = oregano_finalize;
}

static void oregano_init (Oregano *object)
{
	guint i;
	gchar *engine_name;

	cursors_init ();
	stock_init ();

	oregano_config_load ();

	// check if the engine loaded from the configuration exists...
	// otherwise pick up the next one !
	engine_name = oregano_engine_get_engine_name_by_index (oregano.engine);
	if (g_find_program_in_path (engine_name) == NULL) {
		oregano.engine = OREGANO_ENGINE_COUNT;
		for (i = 0; i < OREGANO_ENGINE_COUNT; i++) {
			g_free (engine_name);
			engine_name = oregano_engine_get_engine_name_by_index (i);
			if (g_find_program_in_path (engine_name) != NULL) {
				oregano.engine = i;
			}
		}
	}
	g_free (engine_name);

	// simulation cannot run, disable log
	if (oregano.engine < 0 || oregano.engine >= OREGANO_ENGINE_COUNT)
		oregano.show_log = FALSE;
}

Oregano *oregano_new (void)
{
	return g_object_new (oregano_get_type (), "application-id", "org.gnome.oregano", "flags",
	                     G_APPLICATION_HANDLES_OPEN, NULL);
}

// GApplication implementation
static void oregano_activate (GApplication *application)
{
	oregano_application (application, NULL);
}

static void oregano_open (GApplication *application, GFile **files, gint n_files, const gchar *hint)
{
	gint i;

	for (i = 0; i < n_files; i++)
		oregano_application (application, files[i]);
}

static gboolean quit_hook (GSignalInvocationHint *ihint, guint n_param_values,
                           const GValue *param_values, gpointer data)
{
	return FALSE;
}

static void oregano_application (GApplication *app, GFile *file)
{
	Schematic *schematic = NULL;
	SchematicView *schematic_view = NULL;

	gchar *msg;
	Splash *splash = NULL;
	GError *error = NULL;

	// Keep non localized input for ngspice
	setlocale (LC_NUMERIC, "C");

	if (oregano.show_splash) {
		splash = oregano_splash_new (&error);
		if (error) {
			msg = g_strdup_printf (_ ("Failed to spawn splash-screen \'%s\''. Code %i - %s"),
			                       OREGANO_UIDIR "splash.ui", error->code, error->message);
			oregano_error (msg);
			g_free (msg);
			g_clear_error (&error);
			// non fatal issue
		}
	}
	// splash == NULL if showing splash is disabled
	oregano_lookup_libraries (splash);

	if (oregano.libraries == NULL) {
		oregano_error (_ ("Could not find a parts library.\n\n"
		                  "Supposed to be in " OREGANO_LIBRARYDIR));
		return;
	}

	oregano.clipboard = NULL;

	schematic = NULL;

	if (file) {
		GError *error = NULL;
		char *fname = g_file_get_parse_name (file);

		schematic = schematic_read (fname, &error);
		if (schematic) {
			schematic_view = schematic_view_new (schematic);

			gtk_widget_show_all (schematic_view_get_toplevel (schematic_view));
			schematic_set_filename (schematic, fname);
			schematic_set_title (schematic, g_path_get_basename (fname));
		}
	} else {
		schematic = schematic_new ();
		schematic_view = schematic_view_new (schematic);
		gtk_widget_show_all (schematic_view_get_toplevel (schematic_view));
	}

	g_signal_add_emission_hook (g_signal_lookup ("last_schematic_destroyed", TYPE_SCHEMATIC), 0,
	                            quit_hook, NULL, NULL);

	if (oregano.show_splash && splash)
		oregano_splash_done (splash, _ ("Welcome to Oregano"));
}

void oregano_deallocate_memory (void)
{
	GList *iter;
	Library *l;

	g_object_unref (oregano.settings);

        // Free the memory used by the parts libraries
        for (iter = oregano.libraries; iter; iter = iter->next) {
                l = (Library *) iter->data;
                g_free (l->name);
                g_free (l->author);
                g_free (l->version);
        }
        g_list_free_full (oregano.libraries, g_free);

	clipboard_empty ();
}
