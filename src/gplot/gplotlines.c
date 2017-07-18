/*
 * gplotlines.c
 *
 * Authors:
 *  Ricardo Markiewicz <rmarkie@fi.uba.ar>
 *  Marc Lorber <lorber.marc@wanadoo.fr>
 *
 * Web page: https://ahoi.io/project/oregano
 *
 * Copyright (C) 1999-2001  Richard Hult
 * Copyright (C) 2003,2004  Ricardo Markiewicz
 * Copyright (C) 2009-2012  Marc Lorber
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
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include <string.h>

#include "gplot-internal.h"
#include "gplotlines.h"

typedef struct _GPlotLines GPlotLines;
typedef struct _GPlotLinesPriv GPlotLinesPriv;
typedef struct _GPlotLinesClass GPlotLinesClass;

struct _GPlotLines
{
	GObject parent;

	GPlotLinesPriv *priv;
};

struct _GPlotLinesClass
{
	GObjectClass parent;
};

GType g_plot_function_get_type ();

static void g_plot_lines_class_init (GPlotLinesClass *class);
static void g_plot_lines_init (GPlotLines *plot);
static void g_plot_lines_draw (GPlotFunction *, cairo_t *, GPlotFunctionBBox *);
static void g_plot_lines_get_bbox (GPlotFunction *, GPlotFunctionBBox *);
static void g_plot_lines_function_init (GPlotFunctionClass *iface);
static void g_plot_lines_set_property (GObject *object, guint prop_id, const GValue *value,
                                       GParamSpec *spec);
static void g_plot_lines_get_property (GObject *object, guint prop_id, GValue *value,
                                       GParamSpec *spec);

static GObjectClass *parent_class = NULL;

enum { ARG_0, ARG_WIDTH, ARG_COLOR, ARG_COLOR_GDKRGBA, ARG_VISIBLE, ARG_GRAPH_TYPE, ARG_SHIFT };

struct _GPlotLinesPriv
{
	gboolean bbox_valid;
	GPlotFunctionBBox bbox;
	gdouble *x;
	gdouble *y;
	gdouble points;
	gboolean visible;

	// Line width
	gdouble width;

	// Line Color
	gchar *color_string;
	GdkRGBA color;

	// Graphic type
	GraphicType graphic_type;

	// Shift for pulse drawings
	gdouble shift;
};

#define TYPE_GPLOT_LINES (g_plot_lines_get_type ())
#define TYPE_GPLOT_GRAPHIC_TYPE (g_plot_lines_graphic_get_type ())
#include "debug.h"

G_DEFINE_TYPE_WITH_CODE (GPlotLines, g_plot_lines, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (TYPE_GPLOT_FUNCTION, g_plot_lines_function_init));

GType g_plot_lines_graphic_get_type (void)
{
	static GType etype = 0;
	if (etype == 0) {
		static const GEnumValue values[] = {{FUNCTIONAL_CURVE, "FUNCTIONAL_CURVE", "funct-curve"},
		                                    {FREQUENCY_PULSE, "FREQUENCY_PULSE", "freq-pulse"},
		                                    {0, NULL, NULL}};
		etype = g_enum_register_static ("GraphicType", values);
	}
	return etype;
}

static void g_plot_lines_function_init (GPlotFunctionClass *iface)
{
	iface->draw = g_plot_lines_draw;
	iface->get_bbox = g_plot_lines_get_bbox;
}

static void g_plot_lines_dispose (GObject *object)
{
	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void g_plot_lines_finalize (GObject *object)
{
	GPlotLines *lines;

	lines = GPLOT_LINES (object);

	if (lines->priv) {
		g_free (lines->priv->x);
		g_free (lines->priv->y);
		g_free (lines->priv->color_string);
		g_free (lines->priv);
	}

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void g_plot_lines_class_init (GPlotLinesClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	parent_class = g_type_class_peek_parent (class);

	object_class->set_property = g_plot_lines_set_property;
	object_class->get_property = g_plot_lines_get_property;
	object_class->dispose = g_plot_lines_dispose;
	object_class->finalize = g_plot_lines_finalize;

	g_object_class_override_property (object_class, ARG_VISIBLE, "visible");

	g_object_class_install_property (object_class, ARG_WIDTH,
	                                 g_param_spec_double ("width", "GPlotLines::width",
	                                                      "the line width", 0.1, 5.0, 1.0,
	                                                      G_PARAM_READWRITE));

	g_object_class_install_property (object_class, ARG_COLOR,
	                                 g_param_spec_string ("color", "GPlotLines::color",
	                                                      "the string color line", "white",
	                                                      G_PARAM_READWRITE));

	g_object_class_install_property (object_class, ARG_COLOR_GDKRGBA,
	                                 g_param_spec_pointer ("color-rgb", "GPlotLines::color-rgb",
	                                                       "the GdkRGBA of the line",
	                                                       G_PARAM_READWRITE));

	g_object_class_install_property (
	    object_class, ARG_GRAPH_TYPE,
	    g_param_spec_enum ("graph-type", "GPlotLines::graph-type", "the type of graphic",
	                       TYPE_GPLOT_GRAPHIC_TYPE, FUNCTIONAL_CURVE, G_PARAM_READWRITE));

	g_object_class_install_property (object_class, ARG_SHIFT,
	                                 g_param_spec_double ("shift", "GPlotLines::shift",
	                                                      "the shift for multiple pulses", 0.0,
	                                                      15e+10, 50.0, G_PARAM_READWRITE));
}

static void g_plot_lines_init (GPlotLines *plot)
{
	GPlotLinesPriv *priv = g_new0 (GPlotLinesPriv, 1);

	priv->bbox_valid = FALSE;

	plot->priv = priv;

	priv->width = 1.0;
	priv->visible = TRUE;
	priv->color_string = g_strdup ("white");
	memset (&priv->color, 0xFF, sizeof(GdkRGBA));
}

static void g_plot_lines_set_property (GObject *object, guint prop_id, const GValue *value,
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
		gdk_rgba_parse (&plot->priv->color, plot->priv->color_string);
		break;
	case ARG_GRAPH_TYPE:
		plot->priv->graphic_type = g_value_get_enum (value);
		break;
	case ARG_SHIFT:
		plot->priv->shift = g_value_get_double (value);
		break;
	default:
		break;
	}
}

static void g_plot_lines_get_property (GObject *object, guint prop_id, GValue *value,
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
	case ARG_GRAPH_TYPE:
		g_value_set_enum (value, plot->priv->graphic_type);
		break;
	case ARG_SHIFT:
		g_value_set_double (value, plot->priv->shift);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (plot, prop_id, spec);
	}
}

GPlotFunction *g_plot_lines_new (gdouble *x, gdouble *y, guint points)
{
	GPlotLines *plot;

	plot = GPLOT_LINES (g_object_new (TYPE_GPLOT_LINES, NULL));
	plot->priv->x = x;
	plot->priv->y = y;
	plot->priv->points = points;

	return GPLOT_FUNCTION (plot);
}

static void g_plot_lines_get_bbox (GPlotFunction *f, GPlotFunctionBBox *bbox)
{
	GPlotLines *plot;

	g_return_if_fail (IS_GPLOT_LINES (f));

	plot = GPLOT_LINES (f);
	if (!plot->priv->bbox_valid) {
		// Update bbox
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
			plot->priv->bbox.xmin = MIN (plot->priv->bbox.xmin, x[point]);
			plot->priv->bbox.ymin = MIN (plot->priv->bbox.ymin, y[point]);
			plot->priv->bbox.xmax = MAX (plot->priv->bbox.xmax, x[point]);
			plot->priv->bbox.ymax = MAX (plot->priv->bbox.ymax, y[point]);
		}
		plot->priv->bbox_valid = TRUE;
	}

	(*bbox) = plot->priv->bbox;
}

// This procedure is in charge to link points with ligne.
// Modified to only draw spectral ray for Fourier analysis.
static void g_plot_lines_draw (GPlotFunction *f, cairo_t *cr, GPlotFunctionBBox *bbox)
{
	gboolean first_point = TRUE;
	guint point;
	gdouble *x, x1;
	gdouble *y, y1;
	guint points;
	GPlotLines *plot;

	g_return_if_fail (IS_GPLOT_LINES (f));

	plot = GPLOT_LINES (f);

	if (!plot->priv->visible)
		return;

	points = plot->priv->points;
	x = plot->priv->x;
	y = plot->priv->y;

	if (plot->priv->graphic_type == FUNCTIONAL_CURVE) {
		for (point = 1; point < points; point++) {
			if ((x[point] >= bbox->xmin) && (x[point] <= bbox->xmax) && (y[point] >= bbox->ymin) &&
			    (y[point] <= bbox->ymax)) {

				if (first_point) {
					cairo_move_to (cr, x[point - 1], y[point - 1]);
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
	} else {
		for (point = 1; point < points; point++) {
			x1 = x[point - 1] + plot->priv->shift;
			y1 = 0;
			NG_DEBUG ("gplotlines: x= %lf\ty= %lf\n", x1, y1);
			cairo_line_to (cr, x1, y1);
			cairo_move_to (cr, x[point] + plot->priv->shift, y[point]);
		}
	}
	if (point < points) {
		cairo_line_to (cr, x[point], y[point]);
	}

	cairo_save (cr);
	cairo_set_source_rgb (cr, plot->priv->color.red, plot->priv->color.green,
	                      plot->priv->color.blue);
	cairo_identity_matrix (cr);
	cairo_set_line_width (cr, plot->priv->width);
	cairo_stroke (cr);
	cairo_restore (cr);
}
