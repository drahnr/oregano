/*
 * oregano-config.c
 *
 *
 * Author:
 *  Richard Hult <rhult@hem.passagen.se>
 *  Ricardo Markiewicz <rmarkie@fi.uba.ar>
 *  Andres de Barbara <adebarbara@fi.uba.ar>
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

#include <sys/types.h>
#include <dirent.h>
#include <glib.h>
#include <string.h>
#include <glib/gi18n.h>

#include "oregano.h"
#include "oregano-config.h"
#include "load-library.h"
#include "dialogs.h"
#include "engine.h"

#define OREGLIB_EXT	"oreglib"

static gboolean is_oregano_library_name (gchar* name);
static void load_library_error (gchar *name);

void
oregano_config_load (void)
{
	oregano.settings = g_settings_new ("apps.oregano");
	oregano.engine = g_settings_get_int (oregano.settings, 
			"engine");
	oregano.compress_files = g_settings_get_boolean (oregano.settings, 
			"compress-files");
	oregano.show_log = g_settings_get_boolean (oregano.settings, 
			"show-log");
	oregano.show_splash = g_settings_get_boolean (oregano.settings, 
			"show-splash");

	// Let's deal with first use -I don't like this-
	if ((oregano.engine < 0) || (oregano.engine >= OREGANO_ENGINE_COUNT))
		oregano.engine = 0;
}

void
oregano_config_save (void)
{
	g_settings_set_int (oregano.settings, "engine", 
			oregano.engine);
	g_settings_set_boolean (oregano.settings, "compress-files", 
			oregano.compress_files);
	g_settings_set_boolean (oregano.settings, "show-log", 
			oregano.show_log);
	g_settings_set_boolean (oregano.settings, "show-splash", 
			oregano.show_splash);
}

void
oregano_lookup_libraries (Splash *sp)
{
	gchar *fname;
	DIR *libdir;
	struct dirent *libentry;
	Library *library;

	oregano.libraries = NULL;
	libdir = opendir (OREGANO_LIBRARYDIR);

	if (libdir == NULL) return;

	if (oregano.libraries != NULL) {
		closedir (libdir);
		return;
	}

	fname = g_build_filename (OREGANO_LIBRARYDIR, "default.oreglib", NULL);

	if (g_file_test (fname, G_FILE_TEST_EXISTS)) {
		library = library_parse_xml_file (fname);
		oregano.libraries = g_list_append (oregano.libraries, library);
	}
	g_free (fname);

	while ((libentry=readdir (libdir)) != NULL) {
		if (is_oregano_library_name (libentry->d_name) &&
			strcmp (libentry->d_name,"default.oreglib")) {
			fname = g_build_filename (OREGANO_LIBRARYDIR, libentry->d_name, NULL);

			// do the following only if splash is enabled
			if (sp) {
				char txt[50];
				sprintf (txt, _("Loading %s ..."), libentry->d_name);

				oregano_splash_step (sp, txt);
			}

			library = library_parse_xml_file (fname);

			if (library)
				oregano.libraries = g_list_append ( oregano.libraries, library);
			else
				load_library_error (fname);

			g_free (fname);
		}
	}
	closedir (libdir);
}

static gboolean
is_oregano_library_name (gchar *name)
{
	gchar *dot;
	dot = strchr (name, '.');
	if (dot == NULL || dot[1] == '\0')
		return FALSE;

	dot++;	// Points to the extension.

	if (strcmp (dot, OREGLIB_EXT) == 0)
		return TRUE;

	return FALSE;
}

static void
load_library_error (gchar *name)
{
	gchar *title, *desc;
	title = g_strdup_printf (_("Could not read the parts library: %s "), 
	                         name);
	desc = g_strdup_printf (_("The file is probably corrupt. Please "
		                      "reinstall the parts library or Oregano "
	                          "and try again."));
	oregano_error_with_title (title, desc);
	g_free (title);
	g_free (desc);
}
