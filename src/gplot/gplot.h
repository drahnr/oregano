/*
 * gplot.h
 *
 * Authors:
 *  Ricardo Markiewicz <rmarkie@fi.uba.ar>
 *  Marc Lorber <lorber.marc@wanadoo.fr>
 * 
 * Web page: https://github.com/marc-lorber/oregano
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
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef _GPLOT_H_
#define _GPLOT_H_

#include <gdk/gdk.h>
#include <gtk/gtk.h>
#include <glib.h>

#define GPLOT(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, TYPE_GPLOT, GPlot)
#define IS_GPLOT(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, TYPE_GPLOT)
#define TYPE_GPLOT			(g_plot_get_type())

typedef struct _GPlot GPlot;
typedef struct _GPlotFunction {} GPlotFunction;

enum {
	GPLOT_ZOOM_INOUT,
	GPLOT_ZOOM_REGION
};

GType      g_plot_get_type ();
GtkWidget* g_plot_new ();
void       g_plot_clear (GPlot *);
int        g_plot_add_function (GPlot *, GPlotFunction *);
void       g_plot_set_zoom_mode (GPlot *, guint);
guint      g_plot_get_zoom_mode (GPlot *);
void       g_plot_reset_zoom (GPlot *);
void       g_plot_set_axis_labels (GPlot *, gchar *, gchar *);
void       g_plot_window_to_device (GPlot *, double *x, double *y);

#endif
