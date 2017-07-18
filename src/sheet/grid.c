/*
 * grid.c
 *
 * Authors:
 *  Richard Hult <rhult@hem.passagen.se>
 *  Ricardo Markiewicz <rmarkie@fi.uba.ar>
 *  Andres de Barbara <adebarbara@fi.uba.ar>
 *  Marc Lorber <lorber.marc@wanadoo.fr>
 *  Bernhard Schuster <bernhard@ahoi.io>
 *  Guido Trentalancia <guido@trentalancia.com>
 *
 * Web page: https://github.com/marc-lorber/oregano
 *
 * Copyright (C) 1999-2001  Richard Hult
 * Copyright (C) 2003,2006  Ricardo Markiewicz
 * Copyright (C) 2009-2012  Marc Lorber
 * Copyright (C) 2013-2014  Bernhard Schuster
 * Copyright (C) 2017       Guido Trentalancia
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
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include <math.h>

#include "grid.h"

#define ROUND(x) (floor ((x)+0.5))
#include "debug.h"

enum { ARG_0, ARG_COLOR, ARG_SPACING, ARG_SNAP };

struct _GridPriv
{
	GooCanvasItem *canvas_grid;
	guint snap;
	GdkRGBA color;
	gdouble spacing;
	gdouble cached_zoom;
	cairo_t *cairo;
};

G_DEFINE_TYPE (Grid, grid, GOO_TYPE_CANVAS_GROUP)

static void grid_class_init (GridClass *class);
static void grid_init (Grid *grid);
static void grid_finalize (GObject *object);
static void grid_set_property (GObject *object, guint prop_id, const GValue *value,
                               GParamSpec *spec);
static void grid_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *spec);
static void grid_dispose (GObject *object);

static void grid_class_init (GridClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	grid_parent_class = g_type_class_peek_parent (class);

	object_class->dispose = grid_dispose;
	object_class->finalize = grid_finalize;

	object_class->set_property = grid_set_property;
	object_class->get_property = grid_get_property;

	g_object_class_install_property (
	    object_class, ARG_COLOR,
	    g_param_spec_string ("color", "Grid::color", "the color", "black", G_PARAM_WRITABLE));

	g_object_class_install_property (object_class, ARG_SPACING,
	                                 g_param_spec_double ("spacing", "Grid::spacing",
	                                                      "the grid spacing", 0.0f, 100.0f, 10.0f,
	                                                      G_PARAM_READWRITE));

	g_object_class_install_property (
	    object_class, ARG_SNAP,
	    g_param_spec_boolean ("snap", "Grid::snap", "snap to grid?", TRUE, G_PARAM_READWRITE));
}

static void grid_init (Grid *grid)
{
	GridPriv *priv;

	priv = g_new0 (GridPriv, 1);

	grid->priv = priv;

	priv->spacing = 10.0;
	priv->snap = TRUE;
}

static void grid_dispose (GObject *object) { G_OBJECT_CLASS (grid_parent_class)->dispose (object); }

static void grid_finalize (GObject *object)
{
	Grid *grid;

	grid = GRID (object);
	grid->priv = NULL;

	G_OBJECT_CLASS (grid_parent_class)->finalize (object);
}

static void grid_set_property (GObject *object, guint prop_id, const GValue *value,
                               GParamSpec *spec)
{
	Grid *grid;
	GridPriv *priv;
	grid = GRID (object);
	priv = grid->priv;

	switch (prop_id) {

	case ARG_SPACING:
		priv->spacing = g_value_get_double (value);
		break;

	case ARG_SNAP:
		priv->snap = g_value_get_boolean (value);
		break;

	default:
		break;
	}
}

static void grid_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *spec)
{
	Grid *grid;
	GridPriv *priv;

	grid = GRID (object);
	priv = grid->priv;
	switch (prop_id) {
	case ARG_SPACING:
		g_value_set_double (value, priv->spacing);
		break;
	case ARG_SNAP:
		g_value_set_boolean (value, priv->snap);
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (grid, prop_id, spec);
		break;
	}
}

Grid *grid_new (GooCanvasItem *root, gdouble width, gdouble height)
{
	Grid *grid = NULL;

	grid = g_object_new (TYPE_GRID, NULL);

	g_object_set (G_OBJECT (grid), "parent", root, NULL);

	grid->priv->canvas_grid = goo_canvas_grid_new (
	    GOO_CANVAS_ITEM (grid), 0.0, 0.0, width, height, 10.0, 10.0, 0.0, 0.0,
	    "horz-grid-line-width", 0.05, "horz-grid-line-color", "dark gray", "vert-grid-line-width",
	    0.05, "vert-grid-line-color", "dark gray", NULL);

	grid_show (grid, TRUE);
	grid_snap (grid, TRUE);
	return grid;
}

inline gboolean snap_to_grid (Grid *grid, gdouble *x, gdouble *y)
{
	GridPriv *priv;
	gdouble spacing;

	priv = grid->priv;
	spacing = priv->spacing;

	Coords old = {0., 0.};
	gboolean moved = FALSE;

	if (G_LIKELY (priv->snap)) {
		if (G_LIKELY (x)) {
			old.x = *x;
			*x = ROUND ((*x) / spacing) * spacing;
			moved = moved || (fabs ((*x) - old.x) > COORDS_DELTA);
		}
		if (G_LIKELY (y)) {
			old.y = *y;
			*y = ROUND ((*y) / spacing) * spacing;
			moved = moved || (fabs ((*y) - old.y) > COORDS_DELTA);
		}
	}
	return moved;
}

void grid_show (Grid *grid, gboolean show)
{
	if (show)
		g_object_set (G_OBJECT (grid), "visibility", GOO_CANVAS_ITEM_VISIBLE, NULL);
	else
		g_object_set (G_OBJECT (grid), "visibility", GOO_CANVAS_ITEM_INVISIBLE, NULL);
}

void grid_snap (Grid *grid, gboolean snap)
{
	GridPriv *priv;

	priv = grid->priv;
	priv->snap = snap;
}
