/*
 * cursors.c
 *
 *
 * Author:
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

#include "cursors.h"
#include "debug.h"

OreganoCursor oregano_cursors[] = {
    {NULL, GDK_LEFT_PTR}, {NULL, GDK_TCROSS}, {NULL, GDK_PENCIL}, {NULL, GDK_XTERM}, {NULL, -1}};

void cursors_init (void)
{
	int i;
	GdkDisplay *display;

	display = gdk_display_get_default ();

	for (i = 0; oregano_cursors[i].type != -1; i++) {
		oregano_cursors[i].cursor = gdk_cursor_new_for_display (display, oregano_cursors[i].type);
	}
}

void cursors_shutdown (void)
{
	int i;

	for (i = 0; oregano_cursors[i].type != -1; i++) {
		if (oregano_cursors[i].cursor)
			g_object_unref (oregano_cursors[i].cursor);
	}
}

void cursor_set_widget (GtkWidget *w, int name)
{
	if (gtk_widget_get_window (w))
		gdk_window_set_cursor (gtk_widget_get_window (w), oregano_cursors[name].cursor);
}
