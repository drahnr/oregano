/*
 * splash.c
 *
 *
 * Author:
 *  Ricardo Markiewicz <rmarkie@fi.uba.ar>
 *  Marc Lorber <lorber.marc@wanadoo.fr>
 * 
 * Web page: https://github.com/marc-lorber/oregano
 *
 * Copyright (C) 1999-2001	Richard Hult
 * Copyright (C) 2003,2006	Ricardo Markiewicz
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


#include <unistd.h>
#include <glib/gi18n.h>

#include "splash.h"
#include "dialogs.h"

static void
oregano_splash_destroy (GtkWidget *w, GdkEvent *event, Splash *sp)
{
	if ((event->type == GDK_BUTTON_PRESS) && (event->button.button == 1)) {
		if (sp->can_destroy)
			gtk_widget_destroy (GTK_WIDGET (sp->win));
	}
}

Splash *
oregano_splash_new ()
{
	GtkBuilder *gui;
	GError *perror = NULL;
	Splash *sp;
	GtkEventBox *event;
	gchar *msg;
	
	if ((gui = gtk_builder_new ()) == NULL) {
		oregano_error (_("Could not create splash message."));
		return NULL;
	} 
	else gtk_builder_set_translation_domain (gui, NULL);
	
	if (!g_file_test (OREGANO_UIDIR "/splash.ui", G_FILE_TEST_EXISTS) ||
	     !g_file_test (OREGANO_UIDIR "/splash.xpm", G_FILE_TEST_EXISTS)) {
		msg = g_strdup_printf (
			_("The files %s or %s could not be found. You might need to reinstall Oregano to fix this."),
			OREGANO_UIDIR "/splash.ui",  OREGANO_UIDIR "/splash.xpm");
		oregano_error_with_title (_("Could not create splash message."), msg);
		g_free (msg);
		return NULL;
	}
	
	if (gtk_builder_add_from_file (gui, OREGANO_UIDIR "/splash.ui", 
	    &perror) <= 0) {
		msg = perror->message;
		oregano_error_with_title (_("Could not create splash message."), msg);
		g_error_free (perror);
		return NULL;
	}

	sp = g_new0 (Splash, 1);
	sp->can_destroy = FALSE;

	sp->win = GTK_WINDOW (gtk_builder_get_object (gui, "splash"));
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

gboolean
oregano_splash_free (Splash *sp)
{
	/* Need to disconnect the EventBox Widget! */
	g_signal_handlers_disconnect_by_func (sp->event, oregano_splash_destroy, sp);
	gtk_widget_destroy (GTK_WIDGET (sp->win));
	g_free (sp);
	return FALSE;
}

void
oregano_splash_step (Splash *sp, char *s)
{
	gtk_label_set_text (sp->lbl, s);
	gtk_progress_bar_pulse (GTK_PROGRESS_BAR (sp->progress));
	while (gtk_events_pending ())
		gtk_main_iteration ();
}


void
oregano_splash_done (Splash *sp, char *s)
{
	gtk_label_set_text (sp->lbl, s);
	sp->can_destroy = TRUE;
	g_timeout_add (2000, (GSourceFunc)(oregano_splash_free), sp);
}

