/*
 * Author:
 *  Richard Hult <rhult@codefactory.se>
 *  Ricardo Markiewicz <rmarkie@fi.uba.ar>
 *  Andres de Barbara <adebarbara@fi.uba.ar>
 *  Marc Lorber <lorber.marc@wanadoo.fr>
 *  Bernhard Schuster <schuster.bernhard@gmail.com>
 *
 * Web page: https://srctwig.com/oregano
 *
 * Copyright (C) 1999-2001  Richard Hult
 * Copyright (C) 2003,2006  Ricardo Markiewicz
 * Copyright (C) 2009-2012  Marc Lorber
 * Copyright (C) 2013       Bernhard Schuster
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

#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "dialogs.h"
#include "oregano.h"

#include "pixmaps/logo.xpm"

void
oregano_error (gchar *msg)
{
    oregano_error_with_title (msg, NULL);
}

void
oregano_error_with_title (gchar *title, gchar *desc)
{
	GtkWidget *dialog;
	GString* span_msg;

	span_msg = g_string_new ("<span weight=\"bold\" size=\"large\">");
	span_msg = g_string_append (span_msg, title);
	span_msg = g_string_append (span_msg,"</span>");

	if (desc && desc[0] != '\0') {
		span_msg = g_string_append (span_msg,"\n\n");
		span_msg = g_string_append (span_msg, desc);
	}

	dialog = gtk_message_dialog_new_with_markup (
		NULL,
		GTK_DIALOG_MODAL,
		GTK_MESSAGE_ERROR,
		GTK_BUTTONS_OK,
		"%s",
		span_msg->str);

	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);

	gtk_dialog_run (GTK_DIALOG (dialog));

	g_string_free (span_msg, TRUE);
	gtk_widget_destroy (dialog);
}

void
oregano_warning (gchar *msg)
{
    oregano_warning_with_title (msg, NULL);
}

void
oregano_warning_with_title (gchar *title, gchar *desc)
{
	GtkWidget *dialog;
	GString* span_msg;

	span_msg = g_string_new ("<span weight=\"bold\" size=\"large\">");
	span_msg = g_string_append (span_msg, title);
	span_msg = g_string_append (span_msg,"</span>");

	if (desc && desc[0] != '\0') {
		span_msg = g_string_append (span_msg,"\n\n");
		span_msg = g_string_append (span_msg, desc);
	}

	dialog = gtk_message_dialog_new_with_markup (
		NULL,
		GTK_DIALOG_MODAL,
		GTK_MESSAGE_WARNING,
		GTK_BUTTONS_OK,
		"%s", span_msg->str);

	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);

	gtk_dialog_run (GTK_DIALOG (dialog));

	g_string_free (span_msg, TRUE);
	gtk_widget_destroy (dialog);
}

gint
oregano_question (gchar *msg)
{
	GtkWidget *dialog;
	gint ans;

	dialog = gtk_message_dialog_new_with_markup (
		NULL,
		GTK_MESSAGE_QUESTION,
		GTK_BUTTONS_OK,
		GTK_BUTTONS_CANCEL,
		"%s",
		msg);

	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_CANCEL);

	ans = gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);
	return (ans == GTK_RESPONSE_ACCEPT);
}

void
dialog_about (void)
{
	static GtkWidget *about = NULL;
	GdkPixbuf *logo;

	const gchar *authors[] = {
		"Richard Hult",
		"Margarita Manterola",
		"Andres de Barbara",
		"Gustavo M. Pereyra",
		"Maximiliano Curia",
		"Ricardo Markiewicz",
		"Marc Lorber",
		"Bernhard Schuster",
		NULL
	};

	const char *docs[] = {
		"Ricardo Markiewicz <rmarkie@fi.uba.ar> (es)",
		"Jordi Mallach <tradgnome@softcatala.net> (ca)",
		"Marc Lorber <lorber.marc@wanadoo.fr> (en)",
		NULL
	};

	const gchar *copy = _(
	    "(c) 2012-2013 Bernhard Schuster\n"
	    "(c) 2009-2012 Marc Lorber\n"
	    "(c) 2003-2006 LUGFi\n"
	    "(c) 1999-2001 Richard Hult");

	// Allow only one about box at a time.
	if (about) {
		gdk_window_raise (gtk_widget_get_window (about));
		return;
	}

	logo = gdk_pixbuf_new_from_xpm_data ((const char **) logo_xpm);
	about = gtk_about_dialog_new ();
	gtk_about_dialog_set_program_name (GTK_ABOUT_DIALOG (about), "Oregano");
	gtk_about_dialog_set_version (GTK_ABOUT_DIALOG (about), VERSION);
	gtk_about_dialog_set_copyright (GTK_ABOUT_DIALOG (about), copy);
	gtk_about_dialog_set_comments (GTK_ABOUT_DIALOG (about), 
		_("Schematic capture and circuit simulation.\n"));
	gtk_about_dialog_set_license (GTK_ABOUT_DIALOG (about), 
		"GNU General Public License");
	gtk_about_dialog_set_website (GTK_ABOUT_DIALOG (about), 
	    "https://srctwig.com/oregano");
	gtk_about_dialog_set_authors (GTK_ABOUT_DIALOG (about), authors);
	gtk_about_dialog_set_documenters (GTK_ABOUT_DIALOG (about), docs);
	gtk_about_dialog_set_logo (GTK_ABOUT_DIALOG (about), logo);
	gtk_dialog_run (GTK_DIALOG (about));
	gtk_widget_destroy (about);
	about = NULL;
}
