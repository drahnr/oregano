/*
 * grid.h
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

#ifndef __GRID_H
#define __GRID_H

#include <glib.h>
#include <gtk/gtk.h>
#include <goocanvas.h>

#define TYPE_GRID		   (grid_get_type())
#define GRID(obj)		   (G_TYPE_CHECK_INSTANCE_CAST (obj, grid_get_type (), Grid))
#define GRID_CLASS(klass)  (G_TYPE_CHECK_CLASS_CAST (klass, grid_get_type (), GridClass))
#define IS_GRID(obj)	   (G_TYPE_CHECK_INSTANCE_TYPE (obj, grid_get_type ()))

typedef struct _Grid Grid;
typedef struct _GridClass GridClass;
typedef struct _GridPriv GridPriv;


struct _Grid {
	GooCanvasGroup canvas_group;
	gdouble 	   width;
	gdouble        height;
	gdouble        x;
	gdouble		   y;
	GridPriv *	   priv;
};

struct _GridClass {
	GooCanvasGroupClass	parent_class;
};

Grid *grid_create (GooCanvasItem *root, gdouble width, gdouble height);
GType grid_get_type (void);
void  snap_to_grid (Grid *grid, double *x, double *y);
void  grid_show (Grid *grid, gboolean snap);
void  grid_snap (Grid *grid, gboolean snap);

#endif
