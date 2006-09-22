/**
 * @file   main.c
 * @author Richard Hult <rhult@hem.passagen.se>
 * @author Ricardo Markiewicz <rmarkie@fi.uba.ar>
 * @author Andres de Barbara <adebarbara@fi.uba.ar>
 *
 * @date   1999-2001,2003-2006
 *
 * @brief Oregano, a tool for schematic capture and simulation of electrical
 * circuits.
 *
 * @link Web page: http://arrakis.lug.fi.uba.ar/
 */
/*
 * Copyright (C) 1999-2001  Richard Hult
 * Copyright (C) 2003,2006  LUGFI
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

#define ENABLE_NLS 1
#define HAVE_BIND_TEXTDOMAIN_CODESET 1
#define HAVE_GETTEXT 1
#define HAVE_LC_MESSAGES 1

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <glade/glade.h>
#include <libbonobo-2.0/libbonobo.h>
#include <locale.h>
#include <signal.h>


#include "dialogs.h"
#include "schematic.h"
#include "schematic-view.h"
#include "cursors.h"
#include "load-library.h"
#include "load-schematic.h"
#include "load-common.h"
#include "oregano-config.h"
#include "stock.h"
#include "main.h"
#include "splash.h"

#include <libintl.h>
OreganoApp oregano;
static char **startup_files = NULL;
int oregano_debugging;

static void
quit (void)
{
	printf("! GTK_MAIN_QUIT \n");
	gtk_main_quit ();
}

/* I have no idea if this is right. */
static gboolean
quit_hook (GSignalInvocationHint *ihint,
	guint n_param_values,
	const GValue *param_values,
	gpointer data)
{
	bonobo_main_quit ();
	return FALSE;
}

static void
session_die (void)
{
	/* FIXME: The session is ending. Save unsaved work etc. */
	quit ();
}

int
main (int argc, char **argv)
{
	GnomeProgram *OreProgram = NULL;
	GnomeClient *client = NULL;
	GOptionContext *context;

	Schematic *schematic = NULL ;
	SchematicView *schematic_view = NULL;

	gchar *msg;
	gint i;
	Splash *splash=NULL;

	bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	setlocale (LC_ALL, "");

	context = g_option_context_new ("oregano");

	OreProgram = gnome_program_init (PACKAGE, VERSION, LIBGNOMEUI_MODULE,
		argc, argv, GNOME_PARAM_GOPTION_CONTEXT, context,
		GNOME_PARAM_APP_DATADIR, DATADIR, GNOME_PARAM_NONE);

	cursors_init ();
	stock_init ();

	oregano_config_load ();

	if (!g_file_test (OREGANO_GLADEDIR "/sim-settings.glade2",
		G_FILE_TEST_EXISTS)) {
		msg = g_strdup_printf (
			_("You seem to be running Oregano without\n"
				"having it installed properly on your system.\n\n"
				"Please install Oregano and try again."));

		oregano_error (msg);
		g_free (msg);
		return 1;
	}

	setlocale (LC_NUMERIC, "C");

	/* Connect to session manager. */
	client = gnome_master_client ();
	g_signal_connect (G_OBJECT (client), "die", G_CALLBACK(session_die), NULL);

	if (oregano.show_splash){
		splash = oregano_splash_new ();
	}
	/* splash == NULL if showing splash is disabled */
	oregano_lookup_libraries(splash);

	if (oregano.libraries == NULL) {
		oregano_error (
			_("Could not find a parts library.\n\n"
				"This is probably due to a faulty installation\n"
				"of Oregano. Please check your installation."));
		return 1;
	}

	oregano.clipboard = NULL;

	schematic = NULL;

	startup_files = argv;
	if (startup_files) {
		GError *error = NULL;

		for (i = 1; i < argc; i++) {
			Schematic *new_schematic;
			char *fname = startup_files[i];

			new_schematic = schematic_read (fname, &error);
			if (new_schematic) {
				schematic_view = schematic_view_new (new_schematic);

				gtk_widget_show_all (schematic_view->toplevel);
				schematic_set_filename (new_schematic, fname);
				schematic_set_title (new_schematic, g_path_get_basename(fname));

				 /* Show something */
				while (gtk_events_pending ())
					gtk_main_iteration ();
				schematic = new_schematic;
			}
		}
	}

	g_option_context_free (context);

	if (schematic == NULL){
		schematic = schematic_new ();
		schematic_view = schematic_view_new (schematic);
		gtk_widget_show_all (schematic_view->toplevel);
	}

	g_signal_add_emission_hook (
		g_signal_lookup("last_schematic_destroyed", TYPE_SCHEMATIC),
		0,
		quit_hook,
		NULL,
		NULL);

	if (oregano.show_splash)
		oregano_splash_done (splash, _("Welcome to Oregano"));

	bonobo_main ();
	cursors_shutdown ();
	gnome_config_drop_all ();
	return 0;
}
