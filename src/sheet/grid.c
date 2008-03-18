/*
 * grid.c
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
 * Copyright (C) 2003,2006  Ricardo Markiewicz
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

#include <math.h>
#include "grid.h"
#include "sheet-private.h"

#define ROUND(x) (floor((x)+0.5))

enum {
	ARG_0,
	ARG_COLOR,
	ARG_SPACING,
	ARG_SNAP
};

typedef struct {
	guint snap : 1;
	guint visible : 1;
	GdkColor color;
	gdouble spacing;
	gdouble cached_zoom;
	GdkGC *gc;
} GridPriv;

static void grid_class_init (GridClass *class);
static void grid_init (Grid *grid);
static void grid_destroy (GtkObject *object);
static void grid_set_property (GObject *object, guint prop_id,
	const GValue *value, GParamSpec *spec);
static void grid_get_property (GObject *object, guint prop_id, GValue *value,
	GParamSpec *spec);
static void grid_update (GnomeCanvasItem *item, gdouble *affine,
	ArtSVP *clip_path, gint flags);
static void grid_realize (GnomeCanvasItem *item);
static void grid_unrealize (GnomeCanvasItem *item);
static void grid_draw (GnomeCanvasItem *grid, GdkDrawable *drawable,
	gint x, gint y, gint width, gint height);
static double grid_point (GnomeCanvasItem *item, gdouble x, gdouble y, gint cx,
	gint cy, GnomeCanvasItem **actual_item);

static GnomeCanvasItemClass *grid_parent_class;

GType
grid_get_type (void)
{
	static GType grid_type = 0;

	if (!grid_type) {
		static const GTypeInfo grid_info = {
			sizeof (GridClass),
			NULL,
			NULL,
			(GClassInitFunc) grid_class_init,
			NULL,
			NULL,
			sizeof (Grid),
			0,
			(GInstanceInitFunc) grid_init,
			NULL
		};

		grid_type = g_type_register_static(GNOME_TYPE_CANVAS_ITEM,
			"Grid", &grid_info, 0);
	}

	return grid_type;
}

static void
grid_class_init (GridClass *class)
{
	GObjectClass *object_class;
	GtkObjectClass *gtk_object_class;
	GnomeCanvasItemClass *item_class;

	object_class = G_OBJECT_CLASS (class);
	gtk_object_class = GTK_OBJECT_CLASS(class);
	item_class = GNOME_CANVAS_ITEM_CLASS(class);
	
	gtk_object_class->destroy = grid_destroy;

	grid_parent_class = g_type_class_peek_parent (class);

	object_class->set_property = grid_set_property;
	object_class->get_property = grid_get_property;

	g_object_class_install_property(
		object_class,
		ARG_COLOR,
		g_param_spec_string("color", "Grid::color", "the color",
			"black", G_PARAM_WRITABLE)
		);
	g_object_class_install_property(
		object_class,
		ARG_SPACING,
		g_param_spec_double("spacing", "Grid::spacing",
			"the grid spacing", 0.0f, 100.0f, 10.0f,
			G_PARAM_READWRITE)
		);
	g_object_class_install_property(
		object_class,
		ARG_SNAP,
		g_param_spec_boolean("snap", "Grid::snap", "snap to grid?",
			TRUE,G_PARAM_READWRITE)
		);

	item_class->update = grid_update;
	item_class->realize = grid_realize;
	item_class->unrealize = grid_unrealize;
	item_class->draw = grid_draw;
	item_class->point = grid_point;
}

static void
grid_init (Grid *grid)
{
	GridPriv *priv;

	priv = g_new0 (GridPriv, 1);

	grid->priv = priv;

	priv->spacing = 10.0;
	priv->snap = TRUE;
	priv->visible = TRUE;
}

static void
grid_destroy (GtkObject *object)
{
	Grid *grid;
	GridPriv *priv;

	grid = GRID (object);
	priv = grid->priv;
	
	if (GTK_OBJECT_CLASS(grid_parent_class)->destroy) {
		GTK_OBJECT_CLASS(grid_parent_class)->destroy(object);
	}
}

inline void
snap_to_grid (Grid *grid, gdouble *x, gdouble *y)
{
	GridPriv *priv;
	gdouble spacing;

	priv = grid->priv;
	spacing = priv->spacing;

	if (priv->snap) {
		if (x) *x = ROUND((*x) / spacing) * spacing;
		if (y) *y = ROUND((*y) / spacing) * spacing;
	}
}

static void
grid_set_property (GObject *object, guint prop_id, const GValue *value,
	GParamSpec *spec)
{
	GnomeCanvasItem *item;
	Grid *grid;
	GridPriv *priv;
	GdkColor color;

	item = GNOME_CANVAS_ITEM (object);
	grid = GRID (object);
	priv = grid->priv;

	switch (prop_id){
	case ARG_COLOR:
		if (gnome_canvas_get_color (item->canvas,
			    g_value_get_string(value), &color)) {
			priv->color = color;
		} else {
			color.pixel = 0;
			priv->color = color;
		}
		if (priv->gc)
			gdk_gc_set_foreground (priv->gc, &color);
		break;

	case ARG_SPACING:
		priv->spacing = g_value_get_double(value);
		break;

	case ARG_SNAP:
		priv->snap = g_value_get_boolean (value);
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

	switch (prop_id){
	case ARG_SPACING:
		g_value_set_double(value, priv->spacing);
		break;
	case ARG_SNAP:
		g_value_set_boolean(value, priv->snap);
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(grid, prop_id, spec);
		break;
	}
}

void
grid_show (Grid *grid, gboolean show)
{
	GridPriv *priv;

	priv = grid->priv;

	priv->visible = show;
	gnome_canvas_item_request_update (GNOME_CANVAS_ITEM (grid));
}

gint
grid_is_snap (Grid *grid)
{
	GridPriv *priv;

	priv = grid->priv;

	return priv->snap;
}

gint
grid_is_show (Grid *grid)
{
	GridPriv *priv;

	priv = grid->priv;

	return priv->visible;
}

void
grid_snap (Grid *grid, gboolean snap)
{
	GridPriv *priv;

	priv = grid->priv;

	priv->snap = snap;
}

static void
grid_update (GnomeCanvasItem *item, gdouble *affine, ArtSVP *clip_path,
	gint flags)
{
	Grid *grid;
	GridPriv *priv;

	grid = GRID (item);

	priv = grid->priv;

	priv->cached_zoom = SHEET(item->canvas)->priv->zoom;

	if (grid_parent_class->update)
		(* grid_parent_class->update) (item, affine, clip_path, flags);

	gdk_gc_set_foreground (priv->gc, &priv->color);
	gnome_canvas_update_bbox (item, 0, 0, INT_MAX, INT_MAX);
}

static void
grid_realize (GnomeCanvasItem *item)
{
	Grid *grid;
	GridPriv *priv;

	grid = GRID (item);

	priv = grid->priv;

	if (grid_parent_class->realize)
		(* grid_parent_class->realize) (item);

	priv->gc = gdk_gc_new (item->canvas->layout.bin_window);
}

static void
grid_unrealize (GnomeCanvasItem *item)
{
	Grid *grid;
	GridPriv *priv;

	grid = GRID (item);
	priv = grid->priv;

	g_object_unref (priv->gc);

	if (grid_parent_class->unrealize)
		(* grid_parent_class->unrealize) (item);
}

inline static gint
start_coord (long c, long g)
{
	long m;

	if (c > 0){
		m = c % g;
		if (m == 0)
			return m;
		else
			return g - m;
	} else
		return (-c) % g;
}

static void
grid_draw (GnomeCanvasItem *canvas, GdkDrawable *drawable, gint x, gint y,
	gint width, gint height)
{
	GridPriv *priv;
	Grid *grid;
	GdkGC *gc;
	gdouble gx, gy, sgx, sgy;
	gdouble spacing;

	grid = GRID(canvas);
	priv = grid->priv;

	if (!priv->visible)
		return;

	spacing = priv->spacing * priv->cached_zoom * 10000;
	gc = priv->gc;

	sgx = start_coord (x * 10000, spacing) - spacing;
	sgy = start_coord (y * 10000, spacing) - spacing;

	for (gx = sgx; gx <= width * 10000; gx += spacing)
		for (gy = sgy; gy <= height * 10000; gy += spacing) {
			gdk_draw_point (drawable, gc, rint (gx/10000 + 0.45),
				rint (gy/10000 + 0.45));
		}
}

static double
grid_point (GnomeCanvasItem *item, gdouble x, gdouble y, gint cx, gint cy,
	GnomeCanvasItem **actual_item)
{
	/*
	 * The grid is everywhere. (That's a bug).
	 */
	*actual_item = item;
	return 0.0;
}

