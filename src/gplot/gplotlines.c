/*
 * gplotlines.c
 *
 * Authors:
 *  Ricardo Markiewicz <rmarkie@fi.uba.ar>
 *
 * Copyright (C) 1999-2001  Richard Hult
 * Copyright (C) 2003,2004  Ricardo Markiewicz
 *
 * This library is free software; you can redistribute it and/or
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "gplotlines.h"
#include <string.h>

static void g_plot_lines_class_init (GPlotLinesClass* class);
static void g_plot_lines_init (GPlotLines* plot);
static void g_plot_lines_draw (GPlotFunction *, cairo_t *, GPlotFunctionBBox *);
static void g_plot_lines_get_bbox (GPlotFunction *, GPlotFunctionBBox *);
static void g_plot_lines_function_init (GPlotFunctionClass *iface);
static void g_plot_lines_set_property(GObject *object, guint prop_id, const GValue *value, GParamSpec *spec);
static void g_plot_lines_get_property(GObject *object, guint prop_id, GValue *value, GParamSpec *spec);

static GObjectClass* parent_class = NULL;
	
enum {
	ARG_0,
	ARG_WIDTH,
	ARG_COLOR,
	ARG_COLOR_GDKCOLOR,
	ARG_VISIBLE,
};

struct _GPlotLinesPriv {
	gboolean bbox_valid;
	GPlotFunctionBBox bbox;
	gdouble *x;
	gdouble *y;
	gdouble points;
	gboolean visible;

	/** Line width */
	gdouble width;

	/** Line Color */
	gchar *color_string;
	GdkColor color;
};

G_DEFINE_TYPE_WITH_CODE (GPlotLines, g_plot_lines, G_TYPE_OBJECT,
	G_IMPLEMENT_INTERFACE (TYPE_GPLOT_FUNCTION,
	g_plot_lines_function_init));

static void
g_plot_lines_function_init (GPlotFunctionClass *iface)
{
	iface->draw = g_plot_lines_draw;
	iface->get_bbox = g_plot_lines_get_bbox;
}

static void
g_plot_lines_dispose(GObject *object)
{
	G_OBJECT_CLASS(parent_class)->dispose(object);
}

static void
g_plot_lines_finalize(GObject *object)
{
	GPlotLines *lines;

	lines = GPLOT_LINES (object);

	if (lines->priv) {
		g_free (lines->priv->x);
		g_free (lines->priv->y);
		g_free (lines->priv->color_string);
		g_free (lines->priv);
	}

	G_OBJECT_CLASS(parent_class)->finalize(object);
}

static void
g_plot_lines_class_init (GPlotLinesClass* class)
{
	GObjectClass* object_class;

	object_class = G_OBJECT_CLASS (class);
	parent_class = g_type_class_peek_parent (class);
	
	object_class->set_property = g_plot_lines_set_property;
	object_class->get_property = g_plot_lines_get_property;
	object_class->dispose = g_plot_lines_dispose;
	object_class->finalize = g_plot_lines_finalize;

	g_object_class_override_property (object_class, ARG_VISIBLE, "visible");

	g_object_class_install_property(
		object_class,
		ARG_WIDTH,
		g_param_spec_double ("width", "GPlotLines::width",
		"the line width", 0.1, 5.0, 1.0, G_PARAM_READWRITE));

	g_object_class_install_property(
		object_class,
		ARG_COLOR,
		g_param_spec_string ("color", "GPlotLines::color",
		"the string color line", "white", G_PARAM_READWRITE));

	g_object_class_install_property(
		object_class,
		ARG_COLOR_GDKCOLOR,
		g_param_spec_pointer ("color-rgb", "GPlotLines::color-rgb",
		"the GdkColor of the line", G_PARAM_READWRITE));
}

static void
g_plot_lines_init (GPlotLines* plot)
{
	GPlotLinesPriv* priv = g_new0 (GPlotLinesPriv, 1);

	priv->bbox_valid = FALSE;

	plot->priv = priv;

	priv->width = 1.0;
	priv->visible = TRUE;
	priv->color_string = g_strdup ("white");
	memset (&priv->color, 0xFF, sizeof (GdkColor));
}

static void
g_plot_lines_set_property(GObject *object, guint prop_id, const GValue *value,
	GParamSpec *spec)
{
	GPlotLines *plot = GPLOT_LINES (object);

	switch (prop_id) {
		case ARG_VISIBLE:
			plot->priv->visible = g_value_get_boolean (value);
			break;
		case ARG_WIDTH:
			plot->priv->width = g_value_get_double (value);
			break;
		case ARG_COLOR:
			g_free (plot->priv->color_string);
			plot->priv->color_string = g_strdup (g_value_get_string (value));
			gdk_color_parse (plot->priv->color_string, &plot->priv->color);
			break;
		case ARG_COLOR_GDKCOLOR:
			//s = g_value_get_string (value)
			//gdk_color_parse (s, &plot->priv->color);
			break;
		default:
			break;
	}
}


static void
g_plot_lines_get_property(GObject *object, guint prop_id, GValue *value,
	GParamSpec *spec)
{
	GPlotLines *plot = GPLOT_LINES (object);

	switch (prop_id) {
	case ARG_WIDTH:
		g_value_set_double (value, plot->priv->width);
		break;
	case ARG_VISIBLE:
		g_value_set_boolean (value, plot->priv->visible);
		break;
	case ARG_COLOR:
		g_value_set_string (value, plot->priv->color_string);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (plot, prop_id, spec);
	}
}

GPlotFunction*
g_plot_lines_new (gdouble *x, gdouble *y, guint points)
{
	GPlotLines *plot;

	plot = GPLOT_LINES (g_object_new (TYPE_GPLOT_LINES, NULL));
	plot->priv->x = x;
	plot->priv->y = y;
	plot->priv->points = points;

	return GPLOT_FUNCTION (plot);
}

static void
g_plot_lines_get_bbox (GPlotFunction *f, GPlotFunctionBBox *bbox)
{
	GPlotLines *plot;

	g_return_if_fail (IS_GPLOT_LINES (f));

	plot = GPLOT_LINES (f);
	if (!plot->priv->bbox_valid) {
		/* Update bbox */
		guint point;
		gdouble *x;
		gdouble *y;
		guint points;

		points = plot->priv->points;
		x = plot->priv->x;
		y = plot->priv->y;
		plot->priv->bbox.xmin = 99999999;
		plot->priv->bbox.xmax = -99999999;
		plot->priv->bbox.ymin = 99999999;
		plot->priv->bbox.ymax = -99999999;
		for (point = 0; point < points; point++) {
			plot->priv->bbox.xmin = MIN(plot->priv->bbox.xmin, x[point]);
			plot->priv->bbox.ymin = MIN(plot->priv->bbox.ymin, y[point]);
			plot->priv->bbox.xmax = MAX(plot->priv->bbox.xmax, x[point]);
			plot->priv->bbox.ymax = MAX(plot->priv->bbox.ymax, y[point]);
		}
		plot->priv->bbox_valid = TRUE;
	}

	(*bbox) = plot->priv->bbox;
}

static void
g_plot_lines_draw (GPlotFunction *f, cairo_t *cr, GPlotFunctionBBox *bbox)
{
	gboolean first_point = TRUE;
	guint point;
	gdouble *x;
	gdouble *y;
	guint points;
	GPlotLines *plot;

	g_return_if_fail (IS_GPLOT_LINES (f));

	plot = GPLOT_LINES (f);

	if (!plot->priv->visible) return;

	points = plot->priv->points;
	x = plot->priv->x;
	y = plot->priv->y;

	for (point = 1; point < points; point++) {
		if ((x[point] >= bbox->xmin) && (x[point] <= bbox->xmax) &&
			 (y[point] >= bbox->ymin) && (y[point] <= bbox->ymax)) {

			if (first_point) {
				cairo_move_to (cr, x[point-1], y[point-1]);
				first_point = FALSE;
			}

			cairo_line_to (cr, x[point], y[point]);
		} else {
			if (!first_point) {
				cairo_line_to (cr, x[point], y[point]);
				first_point = TRUE;
			}
		}
	}
	
	if (point < points) {
		cairo_line_to (cr, x[point], y[point]);
	}

	cairo_save (cr);
		cairo_set_source_rgb (cr,
			plot->priv->color.red / 65535.0,
			plot->priv->color.green / 65535.0,
			plot->priv->color.blue / 65535.0);
		cairo_identity_matrix (cr);
		cairo_set_line_width (cr, plot->priv->width);
		cairo_stroke (cr);
	cairo_restore (cr);
}

