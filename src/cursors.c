/*
 * cursors.c
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
#include <gnome.h>
#include "cursors.h"

OreganoCursor oregano_cursors[] = {
	{ NULL, GDK_LEFT_PTR },
	{ NULL, GDK_TCROSS },
	{ NULL, GDK_PENCIL },
	{ NULL, GDK_XTERM },
	{ NULL, -1 }
};

void
cursors_init (void)
{
	int i;

	for (i = 0; oregano_cursors[i].type != -1; i++){
		oregano_cursors[i].cursor = gdk_cursor_new (oregano_cursors[i].type);
	}
}

void
cursors_shutdown (void)
{
	int i;

	for (i = 0; oregano_cursors[i].type != -1; i++)
		gdk_cursor_unref(oregano_cursors[i].cursor);
}

void
cursor_set_widget (GtkWidget *w, int name)
{
	if (w->window)
		gdk_window_set_cursor (w->window, oregano_cursors[name].cursor);
}


