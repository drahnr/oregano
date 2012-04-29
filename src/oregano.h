/*
 * oregano.h
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
 * Copyright (C) 2003,2004  Ricardo Markiewicz
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
#ifndef __MAIN_H
#define __MAIN_H

#undef GTK_ENABLE_BROKEN

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define OREGANO_TYPE_APPLICATION             (oregano_get_type ())
#define OREGANO_APPLICATION(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), OREGANO_TYPE_APPLICATION, Oregano))
#define OREGANO_APPLICATION_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), OREGANO_TYPE_APPLICATION, OreganoClass))
#define OREGANO_IS_APPLICATION(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), OREGANO_TYPE_APPLICATION))
#define OREGANO_IS_APPLICATION_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), OREGANO_TYPE_APPLICATION))
#define OREGANO_APPLICATION_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), OREGANO_TYPE_APPLICATION, OreganoClass))

typedef struct {
	GtkApplication parent_instance;
} Oregano;

typedef struct {
	GtkApplicationClass parent_class;
} OreganoClass;

typedef struct {
	GList *libraries;
	GSList *clipboard;

	// list for library paths
	GList *lib_path;

	// list for model paths
	GList *model_path;

	// hash table with model names as keys and paths as data
	GHashTable *models;

	GSettings *settings;
	gint engine;
	gboolean compress_files;
	gboolean show_log;
	gboolean show_splash;
} OreganoApp;

extern OreganoApp oregano;
extern int oregano_debugging;


Oregano *oregano_new (void);

G_END_DECLS

#endif
