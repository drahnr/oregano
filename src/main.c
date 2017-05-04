/*
 * main.c
 *
 *
 * Authors:
 *  Richard Hult <rhult@hem.passagen.se>
 *  Ricardo Markiewicz <rmarkie@fi.uba.ar>
 *  Andres de Barbara <adebarbara@fi.uba.ar>
 *  Marc Lorber <lorber.marc@wanadoo.fr>
 *
 * Web page: https://ahoi.io/project/oregano
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
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include <glib.h>
#include <glib/gprintf.h>

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include "oregano.h"
#include "options.h"
#include "schematic.h"

int main (int argc, char *argv[])
{
	// keep in mind the app is a subclass of GtkApplication
	// which is a subclass of GApplication
	// so casts from g_application_get_default to
	// GtkApplication as well as GApplication or Oregano
	// are explicitly allowed
	Oregano *app;
	GError *error = NULL;
	int status;
	gpointer class = NULL;

#ifdef ENABLE_NLS
	bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);
#endif

	oregano_options_parse (&argc, &argv, &error);
	if (error) {
		g_warning ("Failed to parse commandline arguments: %i - %s", error->code, error->message);
		g_clear_error (&error);
		return 1;
	}
	if (oregano_options_version()) {
		g_printf("Oregano "VERSION"\n"
			 " Website: https://ahoi.io/projects/oregano\n"
			 " License: GPLv2/GPLv3\n"
			 " Main Developer: Bernhard Schuster\n");
		return 0;
	}

	// required?
	gtk_init (&argc, &argv);

	// required, as we possibly need signal
	// information within oregano.c _before_ the
	// first Schematic instance exists
	class = g_type_class_ref (TYPE_SCHEMATIC);
	app = oregano_new ();

	status = g_application_run (G_APPLICATION (app), argc, argv);

	g_object_unref (app);
	g_type_class_unref (class);

	return status;
}
