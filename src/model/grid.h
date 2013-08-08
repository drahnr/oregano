/*
 * grid.h
 *
 *
 * Authors:
 *  Bernhard Schuster <schuster.bernhard@gmail.com>
 *
 * Web page: https://srctwig.com/oregano
 *
 * Copyright (C) 2013       Bernhard Schuster
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

#include <gtk/gtk.h>

#include "coords.h"
#include "clipboard.h"
#include "item-data.h"

#define TYPE_GRID            (grid_get_type ())
#define GRID(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), TYPE_GRID, Grid))
#define GRID_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), TYPE_GRID, GridClass))
#define IS_GRID(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TYPE_GRID))
#define IS_GRID_CLASS(klass) (G_TYPE_INSTANCE_GET_CLASS ((klass), TYPE_GRID))

typedef struct _Grid Grid;
typedef struct _GridClass GridClass;
typedef struct _GridPriv GridPriv;


struct _Grid {
	GObject parent;
	GridPriv *priv;
};

struct _GridClass
{
	GObjectClass parent_class;

	void (*changed) ();
	void (*delete) ();
};

GType 		grid_get_type (void);
Grid *		grid_new (gdouble height, gdouble width);
#if 0 //TODO
void 		grid_set_step (Grid *grid, gdouble step_delta);
void 		grid_set_color (Grid *grid, guint color_grid);
void		grid_set_background_color (Grid *grid, guint color_bg);
#endif

#define grid_show(x,b) grid_set_visible(x,b)
void 		grid_set_visible (Grid *grid, gboolean show);
gboolean	grid_is_visible (Grid *grid);
void 		grid_set_spacing (Grid *grid, gdouble spacing);
gdouble		grid_get_spacing (Grid *grid);
void		grid_get_size (Grid *grid, gdouble *height, gdouble *width);
#define grid_snap(x,b) grid_set_snap(x,b)
void		grid_set_snap (Grid *grid, gboolean snap);
gboolean	grid_is_snap (Grid *grid);
void		snap_to_grid (Grid *grid, gdouble *x, gdouble *y);

#endif
