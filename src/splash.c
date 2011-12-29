/*
 * splash.c
 *
 *
 * Author:
 *  Ricardo Markiewicz <rmarkie@fi.uba.ar>
 *
 * Copyright (C) 2003-2008  Ricardo Markiewicz
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
#include <glade/glade.h>
#include "splash.h"
#include "splash.xpm"


/* TODO : If we support this, we need to know how to stop the g_timeout :-/ */
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
	GladeXML *gui;
	Splash *sp;
	GtkImage *img;
	GtkEventBox *event;
	GdkPixbuf *logo;
	
	gui = glade_xml_new (OREGANO_GLADEDIR "/splash.glade2", NULL, NULL);

	sp = g_new0 (Splash, 1);
	sp->can_destroy = FALSE;

	sp->win = GTK_WINDOW (glade_xml_get_widget (gui, "splash"));
	sp->lbl = GTK_LABEL (glade_xml_get_widget (gui, "label"));
	sp->progress = glade_xml_get_widget (gui, "pbar");

	event = GTK_EVENT_BOX (glade_xml_get_widget (gui, "event"));
	sp->event = GTK_WIDGET (event);

	// Replaced with TimeOut!
	//g_signal_connect (G_OBJECT (event), "button_press_event", G_CALLBACK (oregano_splash_destroy), sp);

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
	int i;
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

