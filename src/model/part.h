/*
 * part.h
 *
 *
 * Author:
 *  Richard Hult <rhult@hem.passagen.se>
 *  Ricardo Markiewicz <rmarkie@fi.uba.ar>
 *  Andres de Barbara <adebarbara@fi.uba.ar>
 *  Marc Lorber <lorber.marc@wanadoo.fr>
 *  Bernhard Schuster <schuster.bernhard@gmail.com>
 *
 * Web page: https://srctwig.com/oregano
 *
 * Copyright (C) 1999-2001  Richard Hult
 * Copyright (C) 2003,2004  Ricardo Markiewicz
 * Copyright (C) 2009-2012  Marc Lorber
 * Copyright (C) 2013-2014  Bernhard Schuster
 *
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

#ifndef __PART_H
#define __PART_H

#include <gtk/gtk.h>

#include "coords.h"
#include "grid.h"
#include "clipboard.h"
#include "load-common.h"

#define TYPE_PART			 (part_get_type())
#define PART(obj)			 (G_TYPE_CHECK_INSTANCE_CAST ((obj), TYPE_PART, Part))
#define PART_CLASS(klass)	 (G_TYPE_CHECK_CLASS_CAST ((klass), TYPE_PART, PartClass))
#define IS_PART(obj)		 (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TYPE_PART))
#define IS_PART_CLASS(klass) (G_TYPE_INSTANCE_GET_CLASS ((klass), TYPE_PART, PartClass))

typedef struct _Part Part;
typedef struct _PartClass PartClass;
typedef struct _Pin Pin;
typedef struct _PartPriv PartPriv;

#include "item-data.h"

struct _Pin {
	Coords offset;
	guint pin_nr;
	gint node_nr; // Node number into the netlist
	Part *part;
};

#define PIN(x) ((Pin *)(x))

struct _Part {
	ItemData 	parent;
	PartPriv *	priv;
};

struct _PartClass
{
	ItemDataClass parent_class;

	void (*changed) ();
};

GType  		part_get_type (void);
Part *		part_new (Grid *grid);
Part *		part_new_from_library_part (LibraryPart *library_part, Grid *grid);
gint		part_get_num_pins (Part *part);
Pin *		part_get_pins (Part *part);
gboolean   	part_set_pins (Part *part, GSList *connections);
gboolean   	part_get_rotation (Part *part);
IDFlip 		part_get_flip (Part *part);
void   		part_labels_rotate (Part *part, int rotation);
char *		part_get_property (Part *part, char *name);
GSList *	part_get_properties (Part *part);
GSList *	part_get_labels (Part *part);

ClipboardData *	part_clipboard_dup (Part *part);

#endif
