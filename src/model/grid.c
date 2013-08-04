/*
 * grid.c
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

#include <goocanvas.h>
#include <math.h>

#include "grid.h"
#include "clipboard.h"
#include "schematic-print-context.h"
#include "oregano-utils.h"
#include "speedy.h"

static void      grid_class_init (GridClass *klass);
static void      grid_init (Grid *grid);
//static ItemData *grid_clone (ItemData *src);
static gboolean  grid_has_properties (GObject *data);
static void      grid_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *spec);
static void      grid_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *spec);
static void      grid_print (GObject *data, cairo_t *cr, SchematicPrintContext *ctx);
static void      grid_changed (GObject *data);


#include "debug.h"

enum {
	ARG_0,
	ARG_COLOR,
	ARG_SPACING,
	ARG_SNAP,
	ARG_WIDTH,
	ARG_HEIGHT
};

enum {
	CHANGED,
	DELETE,
	LAST_SIGNAL
};

struct _GridPriv {
	gboolean visible;
	gboolean snap;
	gdouble spacing;
	gint height;
	gint width;
};

G_DEFINE_TYPE (Grid, grid, G_TYPE_OBJECT)

static guint grid_signals [LAST_SIGNAL] = { 0 };
static ItemDataClass *parent_class = NULL;

static void
grid_finalize (GObject *object)
{
	Grid *grid = GRID (object);
	GridPriv *priv = grid->priv;

	if (priv) {
		g_free (priv);
		grid->priv = NULL;
	}

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
grid_dispose (GObject *object)
{
	G_OBJECT_CLASS (parent_class)->dispose (object);
}


static void
grid_class_init (GridClass *klass)
{
	GObjectClass *object_class;

	parent_class = g_type_class_peek (G_TYPE_OBJECT);
	object_class = G_OBJECT_CLASS (klass);

	object_class->dispose = grid_dispose;
	object_class->finalize = grid_finalize;

	grid_signals [CHANGED] = g_signal_new ("changed", 
	                G_TYPE_FROM_CLASS (object_class),
	                G_SIGNAL_RUN_FIRST,
	                G_STRUCT_OFFSET (GridClass, changed),
	                NULL,
	                NULL,
	                g_cclosure_marshal_VOID__VOID,
	                G_TYPE_NONE,
	                0);

	grid_signals [DELETE] = g_signal_new ("delete", 
	                G_TYPE_FROM_CLASS (object_class),
	                G_SIGNAL_RUN_FIRST,
	                G_STRUCT_OFFSET (GridClass, delete),
	                NULL,
	                NULL,
	                g_cclosure_marshal_VOID__VOID,
	                G_TYPE_NONE,
	                0);

	object_class->set_property = grid_set_property;
	object_class->get_property = grid_get_property;

	g_object_class_install_property (object_class, ARG_COLOR,
		   g_param_spec_string ("color", "Grid::color", "the color",
	                                "black",
	                                G_PARAM_WRITABLE));
	
	g_object_class_install_property (object_class, ARG_SPACING,
	           g_param_spec_double ("spacing", "Grid::spacing", "the grid spacing",
	                                0.0f, 100.0f, 10.0f,
	                                G_PARAM_READWRITE));
	
	g_object_class_install_property (object_class, ARG_SNAP,
	           g_param_spec_boolean ("snap", "Grid::snap", "snap to grid?",
		                         TRUE,
	                                 G_PARAM_READWRITE));
}

static void
grid_init (Grid *grid)
{
	GridPriv *priv = g_new0 (GridPriv, 1);

	//FIXME load from stored settings, maybe cache within sheet?
	priv->visible = TRUE;
	priv->snap = TRUE;
	priv->spacing = 10.;

	grid->priv = priv;
}


static void
grid_set_property (GObject *object, guint prop_id, const GValue *value,
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
	case ARG_HEIGHT:
		priv->height = g_value_get_int (value);
		break;
	case ARG_WIDTH:
		priv->width = g_value_get_int (value);
		break;
	case ARG_COLOR:
//		priv->color = g_value_get_string (value);
// better use 4 bytes ARGB or RGBA
		break;

	default:
		break;
	}
}

static void
grid_get_property (GObject *object, guint prop_id, GValue *value,
	GParamSpec *spec)
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
	case ARG_COLOR:
		break; //TODO
	case ARG_HEIGHT:
		g_value_set_boolean (value, priv->height);
		break;
	case ARG_WIDTH:
		g_value_set_boolean (value, priv->width);
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (grid, prop_id, spec);
		break;
	}
}




Grid *
grid_new (gdouble height, gdouble width)
{
	Grid *grid;
	GridPriv *priv; 

	grid = GRID (g_object_new (TYPE_GRID, NULL));
	g_assert (grid);
	priv = grid->priv;
	priv->height = height;
	priv->width = width;
	return grid;
}


gboolean
grid_is_visible (Grid *grid)
{
	GridPriv *priv;

	g_return_val_if_fail (grid != NULL, FALSE);
	g_return_val_if_fail (IS_GRID (grid), FALSE);

	priv = grid->priv;

	return priv->visible;
}

void
grid_set_visible (Grid *grid, gboolean show)
{
	GridPriv *priv;

	g_return_if_fail (grid != NULL);
	g_return_if_fail (IS_GRID (grid));

	priv = grid->priv;

	priv->visible = show;

	g_signal_emit_by_name ((GObject *)grid, "changed");
}


gdouble
grid_get_spacing (Grid *grid)
{
	GridPriv *priv;

	g_return_val_if_fail (grid != NULL, FALSE);
	g_return_val_if_fail (IS_GRID (grid), FALSE);

	priv = grid->priv;

	return priv->spacing;
}

void
grid_set_spacing (Grid *grid, gdouble spacing)
{
	GridPriv *priv;

	g_return_if_fail (grid != NULL);
	g_return_if_fail (IS_GRID (grid));

	priv = grid->priv;

	priv->spacing = spacing;

	g_signal_emit_by_name ((GObject *)grid, "changed");
}


gboolean
grid_is_snap (Grid *grid)
{
	GridPriv *priv;

	g_return_val_if_fail (grid != NULL, FALSE);
	g_return_val_if_fail (IS_GRID (grid), FALSE);

	priv = grid->priv;

	return priv->snap;
}

void
grid_set_snap (Grid *grid, gboolean snap)
{
	GridPriv *priv;

	g_return_if_fail (grid != NULL);
	g_return_if_fail (IS_GRID (grid));

	priv = grid->priv;

	priv->snap = snap;

	g_signal_emit_by_name ((GObject *)grid, "changed");
}


void
grid_get_size (Grid *grid, gint *height, gint *width)
{
	GridPriv *priv;

	g_return_if_fail (grid != NULL);
	g_return_if_fail (IS_GRID (grid));

	priv = grid->priv;

	if (height)
		*height = priv->height;
	if (width)
		*width = priv->width;
}

#define ROUND(x) (floor((x)+0.5))

inline void
snap_to_grid (Grid *grid, gdouble *x, gdouble *y)
{
	GridPriv *priv;
	gdouble spacing;

	priv = grid->priv;
	spacing = priv->spacing;

	if (__likely(priv->snap)) {
		if (__likely(x)) *x = ROUND ((*x) / spacing) * spacing;
		if (__likely(y)) *y = ROUND ((*y) / spacing) * spacing;
	}
}

typedef enum {
	GRID_BIAS_NONE = 0,
	GRID_BIAS_NORTH = 1,
	GRID_BIAS_SOUTH = 2,
	GRID_BIAS_WEST = 4,
	GRID_BIAS_EAST = 8
} GridRoundingBias;
typedef guint8 GridRoundingBiasFlags;

inline void
snap_to_grid_with_bias (Grid *grid, gdouble *x, gdouble *y, GridRoundingBiasFlags bias)
{
	GridPriv *priv;
	gdouble spacing;


	priv = grid->priv;
	spacing = priv->spacing;

	if (__likely(priv->snap)) {
		gdouble xcorr, ycorr;
		if (bias != GRID_BIAS_NONE) {
			if (bias & GRID_BIAS_NORTH & ~GRID_BIAS_SOUTH) {
				xcorr = -0.25;
			} else if (bias & ~GRID_BIAS_NORTH & GRID_BIAS_SOUTH) {
				xcorr = +0.25;
			} else {
				xcorr = 0.;
			}
			if (bias & GRID_BIAS_WEST & ~GRID_BIAS_EAST) {
				ycorr = -0.25;
			} else if (bias & ~GRID_BIAS_WEST & GRID_BIAS_EAST) {
				ycorr = +0.25;
			} else {
				ycorr = 0.;
			}
			if (xcorr == 0. && ycorr == 0.) {
				g_warning ("Invalid GRID_BIAS_* value, ignoring (with value %i)", bias);
			}
		} else {
			xcorr = ycorr = 0.;
		}
		if (__likely(x)) *x = ROUND (((*x)+xcorr) / spacing) * spacing;
		if (__likely(y)) *y = ROUND (((*y)+ycorr) / spacing) * spacing;
	}
}


static gboolean
grid_has_properties (GObject *data)
{
	return FALSE;
}

static void
grid_changed (GObject *data)
{
	g_return_if_fail (IS_GRID (data));

	g_signal_emit_by_name ((GObject *)data, "changed");
}


static void
grid_print (GObject *data, cairo_t *cr, SchematicPrintContext *ctx)
{
	Coords start_pos, end_pos;
	Grid *grid;
	UNUSED (start_pos);
	UNUSED (end_pos);
	UNUSED (grid);

	g_return_if_fail (data != NULL);
	g_return_if_fail (IS_GRID (data));

	grid = GRID (data);

	cairo_save (cr);
		//FIXME geometry? just white? or nothing?
		cairo_stroke (cr);
	cairo_restore (cr);
}

void grid_delete (Grid *grid)
{
	g_return_if_fail (IS_GRID (grid));

	g_signal_emit_by_name (G_OBJECT (grid), "delete");
}
