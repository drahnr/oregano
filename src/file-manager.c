/*
 * file-manager.c
 *
 *
 * Authors:
 *  Ricardo Markiewicz <rmarkie@fi.uba.ar>
 *  Andres de Barbara <adebarbara@fi.uba.ar>
 *
 * Web page: http://arrakis.lug.fi.uba.ar/
 *
 * Copyright (C) 2003,2006 Ricardo Markiewicz
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

#include "file-manager.h"

FileType file_types[] = {
	FILE_TYPE("oregano", "Oregano Schematic File", schematic_parse_xml_file, schematic_write_xml)
};

#define FILE_TYPES_COUNT (sizeof(file_types)/sizeof(FileType))


FileType *file_manager_get_handler (const gchar *fname)
{
	int i;
	gchar *ext, *ptr;
	FileType *ft = NULL; 

	g_return_val_if_fail (fname != NULL, NULL);

	ptr = ext = (gchar *)fname;

	/* Search for file extension */
	while (*ptr != '\0') {
		if (*ptr == '.') {
			ext = ptr + 1;
		}
		ptr++;
	}

	for (i=0; i<FILE_TYPES_COUNT; i++)
		if (!strcmp (file_types[i].extension, ext)) {
			ft = &file_types[i];
			break;
		}

	return ft;
}

