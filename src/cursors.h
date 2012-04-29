/*
 * cursors.h
 *
 *
 * Author:
 *  Richard Hult <rhult@hem.passagen.se>
 *  Ricardo Markiewicz <rmarkie@fi.uba.ar>
 *  Andres de Barbara <adebarbara@fi.uba.ar>
 *  Marc Lorber <lorber.marc@wanadoo.fr>
 *
 * Web page: https://github.com/marc-lorber/oregano
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
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#ifndef __CURSORS_H
#define __CURSORS_H

#include <gtk/gtk.h>

#define	OREGANO_CURSOR_LEFT_PTR 0
#define	OREGANO_CURSOR_CROSS 1
#define	OREGANO_CURSOR_PENCIL 2
#define	OREGANO_CURSOR_CARET 3

typedef struct {
	GdkCursor *cursor;
	GdkCursorType type;
} OreganoCursor;

extern OreganoCursor oregano_cursors[];

void cursors_init	   (void);
void cursors_shutdown  (void);
void cursor_set_widget (GtkWidget *w, int name);

#endif
