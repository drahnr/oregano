/*
 * splash.h
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

#ifndef _OREGANO_SPLASH_H_
#define _OREGANO_SPLASH_H_

#include <gtk/gtk.h>

typedef struct _Splash Splash;

struct _Splash {
	GtkWindow *win;
	GtkWidget *progress;
	GtkWidget *event;
	GtkLabel  *lbl;
	gboolean can_destroy;
};

Splash *oregano_splash_new ();
gboolean oregano_splash_free (Splash *);
void oregano_splash_step (Splash *, char *s);
void oregano_splash_done (Splash *, char *s);

#endif //_OREGANO_SPLASH_H_
