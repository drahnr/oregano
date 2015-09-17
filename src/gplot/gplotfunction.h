/*
 * gplotfunction.h
 *
 * Authors:
 *  Ricardo Markiewicz <rmarkie@fi.uba.ar>
 *  Marc Lorber <lorber.marc@wanadoo.fr>
 *
 * Web page: https://ahoi.io/project/oregano
 *
 * Copyright (C) 1999-2001  Richard Hult
 * Copyright (C) 2003,2004  Ricardo Markiewicz
 * Copyright (C) 2009-2010  Marc Lorber
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
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef _GPLOT_FUNCTION_H_
#define _GPLOT_FUNCTION_H_

#include <gtk/gtk.h>

#define GPLOT_FUNCTION(obj) G_TYPE_CHECK_INSTANCE_CAST (obj, TYPE_GPLOT_FUNCTION, GPlotFunction)
#define IS_GPLOT_FUNCTION(obj) G_TYPE_CHECK_INSTANCE_TYPE (obj, TYPE_GPLOT_FUNCTION)

typedef struct _GPlotFunctionBBox
{
	gdouble xmin;
	gdouble ymin;
	gdouble xmax;
	gdouble ymax;
} GPlotFunctionBBox;

void g_plot_function_draw (GPlotFunction *, cairo_t *, GPlotFunctionBBox *);
void g_plot_function_get_bbox (GPlotFunction *, GPlotFunctionBBox *);
void g_plot_function_set_visible (GPlotFunction *, gboolean);
gboolean g_plot_function_get_visible (GPlotFunction *);

#endif
