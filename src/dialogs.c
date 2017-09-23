/*
 * Author:
 *  Richard Hult <rhult@codefactory.se>
 *  Ricardo Markiewicz <rmarkie@fi.uba.ar>
 *  Andres de Barbara <adebarbara@fi.uba.ar>
 *  Marc Lorber <lorber.marc@wanadoo.fr>
 *  Bernhard Schuster <bernhard@ahoi.io>
 *  Guido Trentalancia <guido@trentalancia.com>
 *
 * Web page: https://ahoi.io/project/oregano
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

#include <sys/syscall.h>

#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "dialogs.h"
#include "oregano.h"

#include "pixmaps/logo.xpm"

/*
 * Schedule a call to the oregano_error() function so that it is
 * called whenever there are no higher priority events pending.
 *
 * The caller should schedule this function call using the GLib
 * function g_idle_add_full().
 */
gboolean oregano_schedule_error (gchar *msg) { oregano_error_with_title (msg, NULL); return G_SOURCE_REMOVE; }

/*
 * Schedule a call to the oregano_error_with_title() function
 * so that it is called whenever there are no higher priority
 * events pending.
 *
 * The caller should schedule this function call using the GLib
 * function g_idle_add_full().
 */
gboolean oregano_schedule_error_with_title (OreganoTitleMsg *tm)
{
	oregano_error_with_title (tm->title, tm->msg);
	g_free (tm->title);
	g_free (tm->msg);
	g_free (tm);

	return G_SOURCE_REMOVE;
}

void oregano_error (gchar *msg) { oregano_error_with_title (msg, NULL); }

void oregano_error_with_title (gchar *title, gchar *msg)
{
	GtkWidget *dialog;
	GString *span_msg;
	pid_t tid = 0;

#ifdef SYS_gettid
	tid = syscall(SYS_gettid);
#endif
	// make sure that this is running in the main thread
	if (tid && (getpid() != tid)) {
		OreganoTitleMsg *tm = g_malloc (sizeof (OreganoTitleMsg));
		g_assert (tm != NULL);
		tm->title = g_strdup (title);
		tm->msg = g_strdup (msg);
		g_idle_add_full (G_PRIORITY_DEFAULT_IDLE, (GSourceFunc) oregano_schedule_error_with_title, tm, NULL);
		return;
	}

	span_msg = g_string_new ("<span weight=\"bold\" size=\"large\">");
	span_msg = g_string_append (span_msg, title);
	span_msg = g_string_append (span_msg, "</span>");

	if (msg && msg[0] != '\0') {
		span_msg = g_string_append (span_msg, "\n\n");
		span_msg = g_string_append (span_msg, msg);
	}

	dialog = gtk_message_dialog_new (NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR,
					 GTK_BUTTONS_CLOSE, NULL);

	gtk_message_dialog_set_markup (GTK_MESSAGE_DIALOG (dialog), span_msg->str);

	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_CLOSE);

	gtk_dialog_run (GTK_DIALOG (dialog));

	g_string_free (span_msg, TRUE);

	gtk_widget_destroy (dialog);
}

/*
 * Schedule a call to the oregano_warning() function so that it is
 * called whenever there are no higher priority events pending.
 *
 * The caller should schedule this function call using the GLib
 * function g_idle_add_full().
 */
gboolean oregano_schedule_warning (gchar *msg) { oregano_warning_with_title (msg, NULL); return G_SOURCE_REMOVE; }

/*
 * Schedule a call to the oregano_warning_with_title() function
 * so that it is called whenever there are no higher priority
 * events pending.
 *
 * The caller should schedule this function call using the GLib
 * function g_idle_add_full().
 */
gboolean oregano_schedule_warning_with_title (OreganoTitleMsg *tm)
{
	oregano_warning_with_title (tm->title, tm->msg);
	g_free (tm->title);
	g_free (tm->msg);
	g_free (tm);

	return G_SOURCE_REMOVE;
}

void oregano_warning (gchar *msg) { oregano_warning_with_title (msg, NULL); }

void oregano_warning_with_title (gchar *title, gchar *msg)
{
	GtkWidget *dialog;
	GString *span_msg;
	pid_t tid = 0;

#ifdef SYS_gettid
	tid = syscall(SYS_gettid);
#endif
	// make sure that this is running in the main thread
	if (tid && (getpid() != tid)) {
		OreganoTitleMsg *tm = g_malloc (sizeof (OreganoTitleMsg));
		g_assert (tm != NULL);
		tm->title = g_strdup (title);
		tm->msg = g_strdup (msg);
		g_idle_add_full (G_PRIORITY_DEFAULT_IDLE, (GSourceFunc) oregano_schedule_warning_with_title, tm, NULL);
		return;
	}

	span_msg = g_string_new ("<span weight=\"bold\" size=\"large\">");
	span_msg = g_string_append (span_msg, title);
	span_msg = g_string_append (span_msg, "</span>");

	if (msg && msg[0] != '\0') {
		span_msg = g_string_append (span_msg, "\n\n");
		span_msg = g_string_append (span_msg, msg);
	}

	dialog = gtk_message_dialog_new (NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_WARNING,
					 GTK_BUTTONS_CLOSE, NULL);

	gtk_message_dialog_set_markup (GTK_MESSAGE_DIALOG (dialog), span_msg->str);

	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_CLOSE);

	gtk_dialog_run (GTK_DIALOG (dialog));

	g_string_free (span_msg, TRUE);

	gtk_widget_destroy (dialog);
}

/*
 * Schedule a call to the oregano_question() function so that it is
 * called whenever there are no higher priority events pending.
 *
 * The caller should schedule this function call using the GLib
 * function g_idle_add_full().
 */
gboolean oregano_schedule_question (OreganoQuestionAnswer *qa) {
	qa->ans = oregano_question (qa->msg);
	g_free (qa->msg);

	return G_SOURCE_REMOVE;
}

gint oregano_question (gchar *msg)
{
	GtkWidget *dialog;
	gint ans;

	dialog = gtk_message_dialog_new (NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_QUESTION,
					 GTK_BUTTONS_YES_NO, NULL);

	gtk_message_dialog_set_markup (GTK_MESSAGE_DIALOG (dialog), msg);

	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_CANCEL);

	ans = gtk_dialog_run (GTK_DIALOG (dialog));

	gtk_widget_destroy (dialog);

	return (ans == GTK_RESPONSE_YES);
}

void dialog_about (void)
{
	static GtkWidget *about = NULL;
	GdkPixbuf *logo;

	const gchar *authors[] = {"Richard Hult",       "Margarita Manterola", "Andres de Barbara",
	                          "Gustavo M. Pereyra", "Maximiliano Curia",   "Ricardo Markiewicz",
	                          "Marc Lorber",        "Bernhard Schuster",   "Guido Trentalancia",
				  NULL};

	const char *docs[] = {"Ricardo Markiewicz <rmarkie@fi.uba.ar> (es)",
	                      "Jordi Mallach <tradgnome@softcatala.net> (ca)",
	                      "Marc Lorber <lorber.marc@wanadoo.fr> (en)",
	                      "Bernhard Schuster <bernhard@ahoi.io> (de)",
                              NULL};

	const gchar *copy = _ ("(c) 2017 Guido Trentalancia\n"
	                       "(c) 2012-2017 Bernhard Schuster\n"
	                       "(c) 2009-2012 Marc Lorber\n"
	                       "(c) 2003-2006 LUGFi\n"
	                       "(c) 1999-2001 Richard Hult");

	logo = gdk_pixbuf_new_from_xpm_data ((const char **)logo_xpm);
	about = gtk_about_dialog_new ();
	gtk_about_dialog_set_program_name (GTK_ABOUT_DIALOG (about), "Oregano");
	gtk_about_dialog_set_version (GTK_ABOUT_DIALOG (about), VERSION);
	gtk_about_dialog_set_copyright (GTK_ABOUT_DIALOG (about), copy);
	gtk_about_dialog_set_comments (GTK_ABOUT_DIALOG (about),
	                               _ ("Schematic capture and circuit simulation.\n"));
	gtk_about_dialog_set_license (GTK_ABOUT_DIALOG (about), "GNU General Public License");
	gtk_about_dialog_set_website (GTK_ABOUT_DIALOG (about), "https://ahoi.io/project/oregano");
	gtk_about_dialog_set_authors (GTK_ABOUT_DIALOG (about), authors);
	gtk_about_dialog_set_documenters (GTK_ABOUT_DIALOG (about), docs);
	gtk_about_dialog_set_logo (GTK_ABOUT_DIALOG (about), logo);
	gtk_dialog_run (GTK_DIALOG (about));
	gtk_widget_destroy (about);
	about = NULL;
}
