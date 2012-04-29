/*
 * file-manager.h
 *
 *
 * Authors:
 *  Ricardo Markiewicz <rmarkie@fi.uba.ar>
 *  Andres de Barbara <adebarbara@fi.uba.ar>
 *  Marc Lorber <lorber.marc@wanadoo.fr>
 *
 * Web page: https://github.com/marc-lorber/oregano
 *
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

#ifndef _FILE_MANAGER_H_
#define _FILE_MANAGER_H_ 1

#include <gtk/gtk.h>
#include <string.h>

#include "schematic.h"
#include "load-schematic.h"
#include "save-schematic.h"

#define FILE_TYPE(a,b,c,d) {a, b, c, d}

typedef struct _file_manager_ext_ {
	gchar *extension;
	gchar *description;
	int (*load_func)(Schematic *schematic, const gchar *filename, GError **error);
	int (*save_func)(Schematic *schematic, GError **error);
} FileType;

FileType *file_manager_get_handler (const gchar *fname);

#endif
