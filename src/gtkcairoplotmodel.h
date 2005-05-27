/*
 * gtkcairoplotmodel.h 
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

#ifndef _GTK_CAIRO_PLOT_MODEL_
#define _GTK_CAIRO_PLOT_MODEL_

#include <glib.h>
#include <glib-object.h>

#define GTK_CAIRO_PLOT_MODEL_TYPE           (gtk_cairo_plot_model_get_type ())
#define GTK_CAIRO_PLOT_MODEL(obj)          (G_TYPE_CHECK_INSTANCE_CAST(obj, GTK_CAIRO_PLOT_MODEL_TYPE, GtkCairoPlotModel))
#define GTK_CAIRO_PLOT_MODEL_CLASS(klass)  (G_TYPE_CHECK_CLASS_CAST(klass, GTK_CAIRO_PLOT_MODEL_TYPE, GtkCairoPlotModelClass))
#define IS_GTK_CAIRO_PLOT_MODEL(obj)       (G_TYPE_CHECK_INSTANCE_TYPE(obj, GTK_CAIRO_PLOT_MODEL_TYPE))

typedef struct _GtkCairoPlotModel GtkCairoPlotModel;
typedef struct _GtkCairoPlotModelClass GtkCairoPlotModelClass;
typedef struct _GtkCairoPlotModelPriv GtkCairoPlotModelPriv;

/* TODO : Pensar en una forma mas flexible de guardar los puntos */
typedef struct _point_ {
	gdouble x;
	gdouble y;
} Point;

struct _GtkCairoPlotModel {
	GObject item;

	GtkCairoPlotModelPriv *priv;
};

struct _GtkCairoPlotModelClass {
	GObjectClass parent_class;

	void (*changed) ();
};

GType              gtk_cairo_plot_model_get_type (void);
GtkCairoPlotModel *gtk_cairo_plot_model_new (void);
void               gtk_cairo_plot_model_add_point (GtkCairoPlotModel *model, const gchar *func_name, Point p);
guint              gtk_cairo_plot_model_get_num_points (GtkCairoPlotModel *model, const gchar *func_name);
Point             *gtk_cairo_plot_model_get_point (GtkCairoPlotModel *model, const gchar *func_name, guint pos);
void               gtk_cairo_plot_model_get_x_minmax (GtkCairoPlotModel *model, gdouble *min, gdouble *max);
void               gtk_cairo_plot_model_get_y_minmax (GtkCairoPlotModel *model, gdouble *min, gdouble *max);
void               gtk_cairo_plot_model_load_from_file (GtkCairoPlotModel *model, const gchar *filename, const gchar *func_name);
GList             *gtk_cairo_plot_model_get_functions (GtkCairoPlotModel *model);
void               gtk_cairo_plot_model_clear (GtkCairoPlotModel *model);

#endif

