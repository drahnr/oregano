/*
 * gplotlines.h
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

#ifndef _GPLOT_LINES_H_
#define _GPLOT_LINES_H_

#include "gplotfunction.h" 

#define TYPE_GPLOT_LINES            (g_plot_lines_get_type())
#define GPLOT_LINES(obj)            G_TYPE_CHECK_INSTANCE_CAST(obj, TYPE_GPLOT_LINES, GPlotLines)
#define GPLOT_LINES_CLASS(klass)    G_TYPE_CHECK_CLASS_CAST(klass, TYPE_GPLOT_LINES, GPlotLinesClass)
#define IS_GPLOT_LINES(obj)        G_TYPE_CHECK_INSTANCE_TYPE(obj, TYPE_GPLOT_LINES)

typedef struct _GPlotLines GPlotLines;
typedef struct _GPlotLinesPriv GPlotLinesPriv;
typedef struct _GPlotLinesClass GPlotLinesClass;

struct _GPlotLines {
	GObject parent;

	GPlotLinesPriv *priv;
};

struct _GPlotLinesClass {
	GObjectClass parent;
};

GType          g_plot_lines_get_type ();
GPlotFunction* g_plot_lines_new (gdouble *x, gdouble *y, guint points);

#endif


