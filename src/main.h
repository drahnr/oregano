/*
 * main.h
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
 * Copyright (C) 2003,2004  Ricardo Markiewicz
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

#include <libgnome/libgnome.h>

typedef struct {
	GList *libraries;
	GSList *clipboard;

	/**
	 * list for library paths
	 */
	GList *lib_path;

	/**
	 * list for model paths
	 */
	GList *model_path;

	/**
	 * hash table with model names as keys and paths as data
	 */
	GHashTable *models;

	gint engine;
	gboolean compress_files;
	gboolean show_log;
	gboolean show_splash;
} OreganoApp;

extern OreganoApp oregano;
extern int oregano_debugging;

#endif
