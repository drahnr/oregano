/*
 * file.c
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

#include <string.h>
#include <glib/gi18n.h>

#include "file.h"
#include "schematic.h"
#include "schematic-view.h"
#include "dialogs.h"
#include "save-schematic.h"

char *dialog_open_file (SchematicView *sv)
{
	GtkWidget *dialog;
	GtkFileFilter *allfilter, *orefilter;
	char *name;

	allfilter = gtk_file_filter_new ();
	orefilter = gtk_file_filter_new ();

	gtk_file_filter_set_name (orefilter, _ ("Oregano Files"));
	gtk_file_filter_add_pattern (orefilter, "*.oregano");
	gtk_file_filter_set_name (allfilter, _ ("All Files"));
	gtk_file_filter_add_pattern (allfilter, "*");

	dialog = gtk_file_chooser_dialog_new (_ ("Open File"),
	                                      NULL,
	                                      GTK_FILE_CHOOSER_ACTION_OPEN,
	                                      _("_Cancel"),
	                                      GTK_RESPONSE_CANCEL,
	                                      _("_Open"),
	                                      GTK_RESPONSE_ACCEPT,
	                                      NULL);

	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (dialog), orefilter);
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (dialog), allfilter);

	if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT) {
		name = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));
		if (name[strlen (name) - 1] == '/') {
			g_free (name);
			name = NULL;
		}
	} else
		name = NULL;

	gtk_widget_destroy (dialog);
	return name;
}

void dialog_save_as (SchematicView *sv)
{
	GtkWidget *dialog;
	GtkFileFilter *orefilter, *allfilter;
	char *name;
	Schematic *sm;
	GError *error = NULL;

	orefilter = gtk_file_filter_new ();
	allfilter = gtk_file_filter_new ();
	gtk_file_filter_set_name (orefilter, _ ("Oregano Files"));
	gtk_file_filter_add_pattern (orefilter, "*.oregano");
	gtk_file_filter_set_name (allfilter, _ ("All Files"));
	gtk_file_filter_add_pattern (allfilter, "*");

	dialog = gtk_file_chooser_dialog_new (_ ("Save File"),
	                                      NULL,
	                                      GTK_FILE_CHOOSER_ACTION_SAVE,
	                                      _("_Cancel"),
	                                      GTK_RESPONSE_CANCEL,
	                                      _("_Save"),
	                                      GTK_RESPONSE_ACCEPT,
	                                      NULL);

	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (dialog), orefilter);
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (dialog), allfilter);

	if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT) {

		name = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));
		if (name[strlen (name) - 1] != '/') {
			gchar *tmp;
			const gchar *base = g_path_get_basename (name);

			if (strchr (base, '.') == NULL) {
				tmp = g_strconcat (name, ".oregano", NULL);
			} else {
				tmp = g_strdup (name);
			}

			sm = schematic_view_get_schematic (sv);

			schematic_set_filename (sm, tmp);

			if (!schematic_save_file (sm, &error)) {
				char *msg = g_strdup_printf ("Could not save Schematic file %s\n", tmp);
				oregano_error (msg);
				g_free (msg);
				g_free (name);
			}
			g_free (tmp);
		}
	}

	gtk_widget_destroy (dialog);
}

char *dialog_netlist_file (SchematicView *sv)
{
	GtkWidget *dialog;
	char *name = NULL;

	dialog = gtk_file_chooser_dialog_new (_ ("Netlist File"),
	                                      NULL,
	                                      GTK_FILE_CHOOSER_ACTION_SAVE,
	                                      _("_Cancel"),
	                                      GTK_RESPONSE_CANCEL,
	                                      _("_Save"),
	                                      GTK_RESPONSE_ACCEPT,
	                                      NULL);
	gtk_file_chooser_set_local_only (GTK_FILE_CHOOSER (dialog), TRUE);
	gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (dialog), TRUE);

	if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT) {
		name = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));

		if (name[strlen (name) - 1] == '/') {
			name = NULL;
		} else {
			name = g_strdup (name);
		}
	} else
		name = NULL;

	gtk_widget_destroy (dialog);
	return name;
}

char *dialog_file_open (const gchar *title)
{
	GtkWidget *dialog;
	char *name = NULL;

	dialog = gtk_file_chooser_dialog_new (title,
	                                      NULL,
	                                      GTK_FILE_CHOOSER_ACTION_SAVE,
	                                      _("_Cancel"),
	                                      GTK_RESPONSE_CANCEL,
	                                      _("_Save"),
	                                      GTK_RESPONSE_ACCEPT,
	                                      NULL);

	if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT) {
		name = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));

		if (name[strlen (name) - 1] == '/') {
			g_free (name);
			name = NULL;
		} else
			name = g_strdup (name);
	} else
		name = NULL;

	gtk_widget_destroy (dialog);
	return name;
}
