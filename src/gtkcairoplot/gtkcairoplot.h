/*
 * gtkcairoplot.h
 *
 * 
 * Author: 
 *  Ricardo Markiewicz <rmarkie@fi.uba.ar>
 * 
 *  http://www.fi.uba.ar/~rmarkie/
 * 
 * Copyright (C) 2004  Ricardo Markiewicz
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

#ifndef _GTK_CAIRO_PLOT_H_
#define _GTK_CAIRO_PLOT_H_

#include <gdk/gdk.h>
#include <gtk/gtkwidget.h>
#include <glib.h>

#include "gtkcairoplotmodel.h"
#include "gtkcairoplotview.h"

#define GTK_CAIRO_PLOT(obj)          G_TYPE_CHECK_INSTANCE_CAST(obj, gtk_cairo_plot_get_type(), GtkCairoPlot)
#define GTK_CAIRO_PLOT_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST(klass, gtk_cairo_plot_get_type(), GtkCairoPlotClass)
#define IS_GTK_CAIRO_PLOT(obj)             G_TYPE_CHECK_INSTANCE_TYPE(obj, gtk_cairo_plot_get_type())

typedef struct _GtkCairoPlot GtkCairoPlot;
typedef struct _GtkCairoPlotClass GtkCairoPlotClass;
typedef struct _GtkCairoPlotPriv GtkCairoPlotPriv;

struct _GtkCairoPlot {
	GtkLayout parent;
	guint width;
	guint height;

	GtkCairoPlotPriv *priv;
};

struct _GtkCairoPlotClass {
	GtkLayoutClass parent_class;
};

GType      gtk_cairo_plot_get_type (void);
GtkWidget *gtk_cairo_plot_new (void);
void       gtk_cairo_plot_set_model (GtkCairoPlot *plot, GtkCairoPlotModel *model);
void       gtk_cairo_plot_set_view (GtkCairoPlot *plot, GtkCairoPlotView *view);

#endif

