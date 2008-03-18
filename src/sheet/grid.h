/*
 * grid.h
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
#ifndef __GRID_H
#define __GRID_H

#include <libgnomecanvas/gnome-canvas.h>

#define GRID(obj)		   (G_TYPE_CHECK_INSTANCE_CAST (obj, grid_get_type (), Grid))
#define GRID_CLASS(klass)  (G_TYPE_CHECK_CLASS_CAST (klass, grid_get_type (), GridClass))
#define IS_GRID(obj)	   (G_TYPE_CHECK_INSTANCE_TYPE (obj, grid_get_type ()))

typedef struct _Grid Grid;
typedef struct _GridClass GridClass;

struct _Grid {
	GnomeCanvasItem item;

	gpointer priv;
};

struct _GridClass {
	GnomeCanvasItemClass parent_class;
};

GType grid_get_type (void);
void snap_to_grid(Grid *grid, double *x, double *y);

void grid_show (Grid *grid, gboolean snap);
void grid_snap (Grid *grid, gboolean snap);
gint grid_is_show (Grid *grid);
gint grid_is_snap (Grid *grid);

#endif

