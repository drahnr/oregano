/*
 * file.c
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
#include <gnome.h>
#include <string.h>
#include "file.h"
#include "schematic.h"
#include "schematic-view.h"
#include "dialogs.h"
#include "save-schematic.h"

#if (GTK_MINOR_VERSION >= 4)

char *
dialog_open_file (SchematicView *sv)
{
	GtkWidget *dialog;
	GtkFileFilter *allfilter, *orefilter;
	char *name;

	allfilter = gtk_file_filter_new ();
	orefilter = gtk_file_filter_new ();
	
	gtk_file_filter_set_name (orefilter, _("Oregano Files"));
	gtk_file_filter_add_pattern (orefilter, "*.oregano");
	gtk_file_filter_set_name (allfilter, _("All Files"));
	gtk_file_filter_add_pattern (allfilter, "*");
	
	dialog = gtk_file_chooser_dialog_new (_("Open File"),
		NULL,
		GTK_FILE_CHOOSER_ACTION_OPEN,
		GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
		NULL);
	
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (dialog), orefilter);
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (dialog), allfilter);

	if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT) {
		name = gtk_file_chooser_get_filename (
			GTK_FILE_CHOOSER (dialog));
		if (name[strlen (name)-1] == '/')
			name = NULL;
		else
			name = g_strdup (name);
	} else
		name = NULL;

	gtk_widget_destroy (dialog);
	return name;
}

void
dialog_save_as (SchematicView *sv)
{
	GtkWidget *dialog;
	GtkFileFilter *orefilter, *allfilter;
	char *name;
	Schematic *sm;
	GError *error = NULL;

	orefilter = gtk_file_filter_new ();
	allfilter = gtk_file_filter_new ();
	gtk_file_filter_set_name (orefilter, _("Oregano Files"));
	gtk_file_filter_add_pattern (orefilter, "*.oregano");
	gtk_file_filter_set_name (allfilter, _("All Files"));
	gtk_file_filter_add_pattern (allfilter, "*");
		
	dialog = gtk_file_chooser_dialog_new (_("Save File"),
		NULL,
		GTK_FILE_CHOOSER_ACTION_SAVE,
		GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
		NULL);

	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (dialog), orefilter);
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (dialog), allfilter);

	if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT) {

		name = gtk_file_chooser_get_filename (
			GTK_FILE_CHOOSER (dialog));
		if (name [strlen (name) - 1] != '/') {
			gchar *tmp;
			const gchar *base = g_basename (name);

			if (strchr (base, '.') == NULL){
				tmp = g_strconcat (name, ".oregano", NULL);
			} else {
				tmp = g_strdup (name);
			}

			sm = schematic_view_get_schematic (sv);

			schematic_set_filename (sm, tmp);
			// schematic_set_title (sm, (gchar *) g_basename (tmp));

			if (!schematic_save_file (sm, &error)){
				char *msg = g_strdup_printf (
					"Could not save Schematic file %s\n",
					tmp);
				oregano_error (msg);
				g_free (msg);
				g_free (name);
			}
			g_free (tmp);
		}
	}

	gtk_widget_destroy (dialog);
}

char *
dialog_netlist_file (SchematicView *sv)
{
	GtkWidget *dialog;
	char *name = NULL;

	dialog = gtk_file_chooser_dialog_new (_("Netlist File"),
		NULL,
		GTK_FILE_CHOOSER_ACTION_SAVE,
		GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
		NULL);

	if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT) {
		name = gtk_file_chooser_get_filename (
			GTK_FILE_CHOOSER (dialog));

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

char *
dialog_file_open (const gchar *title)
{
	GtkWidget *dialog;
	char *name = NULL;

	dialog = gtk_file_chooser_dialog_new (title,
		NULL,
		GTK_FILE_CHOOSER_ACTION_SAVE,
		GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
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

#else
static void
set_ok (GtkWidget *widget, gboolean *dialog_result)
{
	*dialog_result = TRUE;
	gtk_main_quit ();
}

static guint
file_dialog_delete_event (GtkWidget *widget, GdkEventAny *event)
{
	gtk_main_quit ();
	return TRUE;
}

char *
dialog_open_file (SchematicView *sv)
{
	GtkFileSelection *fsel;
	gboolean accepted = FALSE;
	char *result;

	g_return_val_if_fail (sv != NULL, NULL);
	g_return_val_if_fail (IS_SCHEMATIC_VIEW (sv), NULL);

	fsel = GTK_FILE_SELECTION (gtk_file_selection_new (_("Open file")));
	gtk_window_set_modal (GTK_WINDOW (fsel), TRUE);

	gtk_window_set_transient_for (GTK_WINDOW (fsel),
		GTK_WINDOW (sv->toplevel));

	/* Connect the signals for Ok and Cancel. */
	g_signal_connect (G_OBJECT (fsel->ok_button), "clicked",
		GTK_SIGNAL_FUNC (set_ok), &accepted);
	g_signal_connect (G_OBJECT (fsel->cancel_button), "clicked",
		GTK_SIGNAL_FUNC (gtk_main_quit), NULL);
	gtk_window_set_position (GTK_WINDOW (fsel), GTK_WIN_POS_MOUSE);

	/* Make sure that we quit the main loop if the window is destroyed. */
	g_signal_connect (G_OBJECT (fsel), "delete_event",
		GTK_SIGNAL_FUNC (file_dialog_delete_event), NULL);

	gtk_widget_show (GTK_WIDGET (fsel));
	gtk_grab_add (GTK_WIDGET (fsel));
	gtk_main ();

	if (accepted){
		const gchar *name = gtk_file_selection_get_filename (fsel);

		if (name[strlen (name)-1] == '/')
			result = NULL;
		else
			result = g_strdup (name);
	} else
		result = NULL;

	gtk_widget_destroy (GTK_WIDGET (fsel));

	return result;
}

void
dialog_save_as (SchematicView *sv)
{
	Schematic *sm;
	GtkFileSelection *fsel;
	gboolean accepted = FALSE;
	char *filename;

	g_return_if_fail (sv != NULL);
	g_return_if_fail (IS_SCHEMATIC_VIEW (sv));

	sm = schematic_view_get_schematic (sv);

	fsel = GTK_FILE_SELECTION (
		gtk_file_selection_new (_("Save schematic as")));

	gtk_window_set_modal (GTK_WINDOW (fsel), TRUE);
	filename = schematic_get_filename (sm);
	if (filename)
		gtk_file_selection_set_filename (fsel, filename);

	/* Connect the signals for Ok and Cancel. */
	g_signal_connect (G_OBJECT (fsel->ok_button), "clicked",
		GTK_SIGNAL_FUNC (set_ok), &accepted);
	g_signal_connect (G_OBJECT (fsel->cancel_button), "clicked",
		GTK_SIGNAL_FUNC (gtk_main_quit), NULL);
	gtk_window_set_position (GTK_WINDOW (fsel), GTK_WIN_POS_MOUSE);

	/* Make sure that we quit the main loop if the window is destroyed. */
	g_signal_connect (G_OBJECT (fsel), "delete_event",
		GTK_SIGNAL_FUNC (file_dialog_delete_event), NULL);

	gtk_widget_show (GTK_WIDGET (fsel));
	gtk_grab_add (GTK_WIDGET (fsel));
	gtk_main ();

	if (accepted){
		const gchar *name = gtk_file_selection_get_filename (fsel);

		if (name [strlen (name) - 1] != '/') {
			gchar *tmp;
			const gchar *base = g_basename (name);

			if (strchr (base, '.') == NULL){
				tmp = g_strconcat (name, ".oregano", NULL);
			} else
				tmp = g_strdup (name);

			schematic_set_filename (sm, tmp);
			// schematic_set_title (sm, (gchar *) g_basename (tmp));

			if (!schematic_save_file (sm)){
				char *msg = g_strdup_printf (
					"Could not save Schematic file %s\n",
					tmp);

				oregano_error (msg);
				g_free (msg);
			} else {
				schematic_set_dirty (schematic_view_get_schematic (sv), FALSE);
			}
			g_free (tmp);
		}
	}

	gtk_widget_destroy (GTK_WIDGET (fsel));
}

char *
dialog_netlist_file (SchematicView *sv)
{
	GtkFileSelection *fsel;
	gboolean accepted = FALSE;
	char *result = NULL;

	g_return_val_if_fail (sv != NULL, NULL);
	g_return_val_if_fail (IS_SCHEMATIC_VIEW (sv), NULL);

	fsel = GTK_FILE_SELECTION (
		gtk_file_selection_new (_("Netlist filename")));
	gtk_window_set_modal (GTK_WINDOW (fsel), TRUE);

	gtk_window_set_transient_for (GTK_WINDOW (fsel),
		GTK_WINDOW (sv->toplevel));

	/* Connect the signals for Ok and Cancel. */
	g_signal_connect (G_OBJECT (fsel->ok_button), "clicked",
		GTK_SIGNAL_FUNC (set_ok), &accepted);
	g_signal_connect (G_OBJECT (fsel->cancel_button), "clicked",
		GTK_SIGNAL_FUNC (gtk_main_quit), NULL);
	gtk_window_set_position (GTK_WINDOW (fsel), GTK_WIN_POS_MOUSE);

	/* Make sure that we quit the main loop if the window is destroyed. */
	g_signal_connect (G_OBJECT (fsel), "delete_event",
		GTK_SIGNAL_FUNC (file_dialog_delete_event), NULL);

	gtk_widget_show (GTK_WIDGET (fsel));
	gtk_grab_add (GTK_WIDGET (fsel));
	gtk_main ();

	if (accepted) {
		const gchar *name = gtk_file_selection_get_filename (fsel);

		if (name[strlen (name) - 1] == '/')
			result = NULL;
		else
			result = g_strdup (name);
	} else
		result = NULL;

	gtk_widget_destroy (GTK_WIDGET (fsel));

	return result;
}

char *
dialog_file_open (const gchar *title)
{
	GtkFileSelection *fsel;
	gboolean accepted = FALSE;
	char *result = NULL;

	fsel = GTK_FILE_SELECTION (
		gtk_file_selection_new (title));
	gtk_window_set_modal (GTK_WINDOW (fsel), TRUE);

	gtk_window_set_transient_for (GTK_WINDOW (fsel),
		GTK_WINDOW (sv->toplevel));

	/* Connect the signals for Ok and Cancel. */
	g_signal_connect (G_OBJECT (fsel->ok_button), "clicked",
		GTK_SIGNAL_FUNC (set_ok), &accepted);
	g_signal_connect (G_OBJECT (fsel->cancel_button), "clicked",
		GTK_SIGNAL_FUNC (gtk_main_quit), NULL);
	gtk_window_set_position (GTK_WINDOW (fsel), GTK_WIN_POS_MOUSE);

	/* Make sure that we quit the main loop if the window is destroyed. */
	g_signal_connect (G_OBJECT (fsel), "delete_event",
		GTK_SIGNAL_FUNC (file_dialog_delete_event), NULL);

	gtk_widget_show (GTK_WIDGET (fsel));
	gtk_grab_add (GTK_WIDGET (fsel));
	gtk_main ();

	if (accepted) {
		const gchar *name = gtk_file_selection_get_filename (fsel);

		if (name[strlen (name) - 1] == '/')
			result = NULL;
		else
			result = g_strdup (name);
	} else
		result = NULL;

	gtk_widget_destroy (GTK_WIDGET (fsel));

	return result;
}
#endif
