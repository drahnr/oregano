/*
 * gplot-internal.h
 *
 * Authors:
 *  Marc Lorber <lorber.marc@wanadoo.fr>
 * 
 * Web page: https://github.com/marc-lorber/oregano
 *
 * Copyright (C) 1999-2001  Richard Hult
 * Copyright (C) 2003,2006  Ricardo Markiewicz
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
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef _GPLOT_INTERNAL_H_
#define _GPLOT_INTERNAL_H_

#include <gdk/gdk.h>
#include <gtk/gtk.h>
#include <glib.h>

#include "gplot.h"
#include "gplotfunction.h"

// Internal definitions associated to gplot.h

typedef struct _GPlotClass GPlotClass;
typedef struct _GPlotPriv GPlotPriv;


struct _GPlot {
	GtkLayout parent;

	GPlotPriv *priv;
};
struct _GPlotClass {
	GtkLayoutClass parent_class;
};


// Internal definitions associated to gplotfunction.h

#define TYPE_GPLOT_FUNCTION            (g_plot_function_get_type ())
#define GPLOT_FUNCTION_CLASS(klass)     G_TYPE_CHECK_CLASS_CAST (klass, TYPE_GPLOT_FUNCTION, GPlotFunctionClass)
#define GPLOT_FUNCTION_GET_CLASS(inst) (G_TYPE_INSTANCE_GET_INTERFACE ((inst), TYPE_GPLOT_FUNCTION, GPlotFunctionClass))

typedef struct _GPlotFunctionClass GPlotFunctionClass;

struct _GPlotFunctionClass {
	GTypeInterface parent;

	void (*draw)     (GPlotFunction *, cairo_t *, GPlotFunctionBBox *);
	void (*get_bbox) (GPlotFunction *, GPlotFunctionBBox *);
};

#endif
