/*
 * splash.c
 *
 *
 * Authors:
 *  Ricardo Markiewicz <rmarkie@fi.uba.ar>
 *  Marc Lorber <lorber.marc@wanadoo.fr>
 *  Bernhard Schuster <bernhard@ahoi.io>
 *
 * Web page: https://ahoi.io/project/oregano
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
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include <unistd.h>
#include <glib/gi18n.h>

#include "splash.h"
#include "dialogs.h"
#include "errors.h"

static void oregano_splash_destroy (GtkWidget *w, GdkEvent *event, Splash *sp)
{
	if ((event->type == GDK_BUTTON_PRESS) && (event->button.button == 1)) {
		if (sp->can_destroy)
			gtk_widget_destroy (GTK_WIDGET (sp->win));
	}
}

Splash *oregano_splash_new (GError **error)
{
	GtkBuilder *gui;
	Splash *sp;
	GtkEventBox *event;

	if ((gui = gtk_builder_new ()) == NULL) {
		g_set_error_literal (error, OREGANO_ERROR, OREGANO_UI_ERROR_NO_BUILDER,
		                     _ ("Failed to spawn builder"));
		return NULL;
	}
	gtk_builder_set_translation_domain (gui, NULL);

	if (gtk_builder_add_from_file (gui, OREGANO_UIDIR "/splash.ui", error) <= 0) {
		return NULL;
	}

	sp = g_new0 (Splash, 1);
	sp->can_destroy = FALSE;

	sp->win = GTK_WINDOW (gtk_builder_get_object (gui, "splash"));
	gtk_window_set_keep_above (sp->win, TRUE);
	sp->lbl = GTK_LABEL (gtk_builder_get_object (gui, "label"));
	sp->progress = GTK_WIDGET (gtk_builder_get_object (gui, "pbar"));

	event = GTK_EVENT_BOX (gtk_builder_get_object (gui, "event"));
	sp->event = GTK_WIDGET (event);

	gtk_progress_bar_set_pulse_step (GTK_PROGRESS_BAR (sp->progress), 0.07);
	gtk_widget_show_all (GTK_WIDGET (sp->win));

	while (gtk_events_pending ())
		gtk_main_iteration ();
	return sp;
}

gboolean oregano_splash_free (Splash *sp)
{
	/* Need to disconnect the EventBox Widget! */
	g_signal_handlers_disconnect_by_func (sp->event, oregano_splash_destroy, sp);
	gtk_widget_destroy (GTK_WIDGET (sp->win));
	g_free (sp);
	return FALSE;
}

void oregano_splash_step (Splash *sp, char *s)
{
	gtk_label_set_text (sp->lbl, s);
	gtk_progress_bar_pulse (GTK_PROGRESS_BAR (sp->progress));
	while (gtk_events_pending ())
		gtk_main_iteration ();
}

void oregano_splash_done (Splash *sp, char *s)
{
	gtk_label_set_text (sp->lbl, s);
	sp->can_destroy = TRUE;
	g_timeout_add (2000, (GSourceFunc)(oregano_splash_free), sp);
}
