/*
 * gplotfunction.h
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

#ifndef _GPLOT_FUNCTION_H_
#define _GPLOT_FUNCTION_H_

#include <gdk/gdk.h>
#include <gtk/gtk.h>
#include <glib.h>

#define TYPE_GPLOT_FUNCTION            (g_plot_function_get_type())
#define GPLOT_FUNCTION(obj)            G_TYPE_CHECK_INSTANCE_CAST(obj, TYPE_GPLOT_FUNCTION, GPlotFunction)
#define GPLOT_FUNCTION_CLASS(klass)    G_TYPE_CHECK_CLASS_CAST(klass, TYPE_GPLOT_FUNCTION, GPlotFunctionClass)
#define IS_GPLOT_FUNCTION (obj)        G_TYPE_CHECK_INSTANCE_TYPE(obj, TYPE_GPLOT_FUNCTION)
#define GPLOT_FUNCTION_GET_CLASS(inst) (G_TYPE_INSTANCE_GET_INTERFACE ((inst), TYPE_GPLOT_FUNCTION, GPlotFunctionClass))
/*
#define MAX(a,b) ((a)>(b)?(a):(b))
#define MIN(a,b) ((a)<(b)?(a):(b))
*/

typedef struct _GPlotFunctionBBox {
	gdouble xmin;
	gdouble ymin;
	gdouble xmax;
	gdouble ymax;
} GPlotFunctionBBox;

typedef struct _GPlotFunction {} GPlotFunction;
typedef struct _GPlotFunctionClass GPlotFunctionClass;

struct _GPlotFunctionClass {
	GTypeInterface parent;

	void (*draw)(GPlotFunction *, cairo_t *, GPlotFunctionBBox *);
	void (*get_bbox)(GPlotFunction *, GPlotFunctionBBox *);
};


GType    g_plot_function_get_type ();
void     g_plot_function_draw (GPlotFunction *, cairo_t *, GPlotFunctionBBox *);
void     g_plot_function_get_bbox (GPlotFunction *, GPlotFunctionBBox *);
void     g_plot_function_set_visible (GPlotFunction *, gboolean);
gboolean g_plot_function_get_visible (GPlotFunction *);

#endif


