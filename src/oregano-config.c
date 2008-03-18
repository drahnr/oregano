/*
 * oregano-config.c
 *
 *
 * Author:
 *  Richard Hult <rhult@hem.passagen.se>
 *  Ricardo Markiewicz <rmarkie@fi.uba.ar>
 *  Andres de Barbara <adebarbara@fi.uba.ar>
 *
 * Web page: http://arrakis.lug.fi.uba.ar/
 *
 * Copyright (C) 1999-2001  Richard Hult
 * Copyright (C) 2003,2006  Ricardo Markiewicz
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
#include <gconf/gconf-client.h>
#include <string.h>

#include "main.h"
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
	GConfClient * gconf;

	gconf = gconf_client_get_default ();

	oregano.engine = gconf_client_get_int (gconf, "/apps/oregano/engine",
		NULL);
	oregano.compress_files = gconf_client_get_bool (gconf, "/apps/oregano/compress_files",
		NULL);
	oregano.show_log = gconf_client_get_bool (gconf, "/apps/oregano/show_log",
		NULL);
	oregano.show_splash = gconf_client_get_bool (gconf, "/apps/oregano/show_splash",
		NULL);

	g_object_unref (gconf);

	/* Let's deal with first use -I don't like this- */

	if ((oregano.engine < 0) || (oregano.engine >= OREGANO_ENGINE_COUNT))
		oregano.engine = 0;
}

void
oregano_config_save (void)
{
	GConfClient * gconf;

	gconf = gconf_client_get_default ();

	gconf_client_set_int (gconf, "/apps/oregano/engine", oregano.engine,
		NULL);
	gconf_client_set_bool (gconf, "/apps/oregano/compress_files", oregano.compress_files,
		NULL);
	gconf_client_set_bool (gconf, "/apps/oregano/show_log", oregano.show_log,
		NULL);
	gconf_client_set_bool (gconf, "/apps/oregano/show_splash", oregano.show_splash,
		NULL);

	g_object_unref (gconf);
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

			/* do the following only if splash is enabled */
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

			g_free(fname);
		}
	}
	closedir (libdir);
}

/*
 * Helpers
 */

static gboolean
is_oregano_library_name (gchar *name)
{
	gchar *dot;
	dot = strchr (name, '.');
	if (dot == NULL || dot[1] == '\0')
		return FALSE;

	dot++;	/* Points to the extension. */

	if (strcmp (dot, OREGLIB_EXT) == 0)
		return TRUE;

	return FALSE;
}

static void
load_library_error (gchar *name)
{
	gchar *title, *desc;
		title = g_strdup_printf (_("Could not read the parts library: %s "), name);
		desc = g_strdup_printf (_(
			 "The file is probably corrupt. Please reinstall the parts\n"
			 "library or Oregano and try again."));
	oregano_error_with_title (title, desc);
	g_free (title);
	g_free (desc);
}
