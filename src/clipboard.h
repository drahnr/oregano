/*
 * clipboard.h
 *
 *
 * Authors:
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#ifndef __CLIPBOARD_H
#define __CLIPBOARD_H

typedef struct _ClipboardData ClipboardData;

typedef void (*ClipBoardFunction) (ClipboardData *data, gpointer user_data);

void		   clipboard_empty (void);
gboolean	   clipboard_is_empty (void);
void		   clipboard_foreach (ClipBoardFunction callback,
								  gpointer user_data);
void		   clipboard_add_object (GObject *item);
GObjectClass   *clipboard_data_get_item_class (ClipboardData *data);
GObject		   *clipboard_data_get_item_data (ClipboardData *data);

#endif
