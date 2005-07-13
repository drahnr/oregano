/*
 * gtkcairoplotmodel.c
 *
 * 
 * Author: 
 *  Ricardo Markiewicz <rmarkie@fi.uba.ar>
 * 
 *  http://www.fi.uba.ar/~rmarkie/
 * 
 * Copyright (C) 2004-2005  Ricardo Markiewicz
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

#include <stdio.h>
#include "gtkcairoplotmodel.h"

/* Properties */
enum {
	ARG_0,
	ARG_TITLE,
	ARG_X_UNIT,
	ARG_Y_UNIT,
	ARG_LOGX
};
/* Signals */
enum {
	CHANGED,
	LAST_SIGNAL
};

static guint model_signals [LAST_SIGNAL] = { 0 };

struct _GtkCairoPlotModelPriv {
	gchar *title;
	gchar *x_title;
	gchar *y_title;
	gboolean logx;

	/* Each function is store with a "name" as keyword and a
	 * GArray for the points data
	 */
	GHashTable *functions;
	gdouble x_max, x_min;
	gdouble y_max, y_min;
};

static void gtk_cairo_plot_model_class_init (GtkCairoPlotModelClass *class);
static void gtk_cairo_plot_model_init (GtkCairoPlotModel *grid);
static void gtk_cairo_plot_model_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *spec);
static void gtk_cairo_plot_model_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *spec);

static GObjectClass *parent_class;

GType
gtk_cairo_plot_model_get_type (void)
{
  static GType plot_type = 0;
  
  if (!plot_type) {
	  static const GTypeInfo plot_info = {
		  sizeof (GtkCairoPlotModelClass),
			NULL,
			NULL,
		  (GClassInitFunc) gtk_cairo_plot_model_class_init,
			NULL,
			NULL,
		  sizeof (GtkCairoPlotModel),
			0,
		  (GInstanceInitFunc) gtk_cairo_plot_model_init,
		  NULL
	  };
	 
	  plot_type = g_type_register_static (G_TYPE_OBJECT, "GtkCairoPlotModel", &plot_info, 0);
  }

	return plot_type;
}

static void 
gtk_cairo_plot_model_class_init (GtkCairoPlotModelClass *class)
{
	GObjectClass *object_class;
	
	object_class = G_OBJECT_CLASS (class);
	
	parent_class = g_type_class_peek_parent (class);
	
	object_class->set_property = gtk_cairo_plot_model_set_property;
	object_class->get_property = gtk_cairo_plot_model_get_property;

	g_object_class_install_property(
		object_class,
		ARG_TITLE,
		g_param_spec_string("title", "GtkCairoPlorModel::title", "the title", "Title", G_PARAM_READWRITE)
	);
	g_object_class_install_property(
		object_class,
		ARG_X_UNIT,
		g_param_spec_string("x-unit", "GtkCairoPlorModel::x-unit", "x axis unit", "X axis unit", G_PARAM_READWRITE)
	);
	g_object_class_install_property(
		object_class,
		ARG_Y_UNIT,
		g_param_spec_string("y-unit", "GtkCairoPlorModel::y-unit", "y axis unit", "Y axis unit", G_PARAM_READWRITE)
	);
	g_object_class_install_property(
		object_class,
		ARG_LOGX,
		g_param_spec_boolean("logx", "GtkCairoPlorModel::logx", "use logx", FALSE, G_PARAM_READWRITE)
	);

	model_signals [CHANGED] =
		g_signal_new ("changed", G_TYPE_FROM_CLASS(object_class),
			G_SIGNAL_RUN_FIRST,
			G_STRUCT_OFFSET (GtkCairoPlotModelClass, changed),
			NULL,
			NULL,
			g_cclosure_marshal_VOID__VOID,
			G_TYPE_NONE,
			0);
}

static void
gtk_cairo_plot_model_init (GtkCairoPlotModel *model)
{
	GtkCairoPlotModelPriv *priv;

	priv = g_new0 (GtkCairoPlotModelPriv, 1);

	model->priv = priv;

	/* Init private data */
	priv->title = NULL;
	priv->x_title = NULL;
	priv->y_title = NULL;
	/* TODO : Provide destroy functions for key and value */
	priv->functions = g_hash_table_new (g_str_hash, g_str_equal);
	priv->x_max = -G_MAXDOUBLE;
	priv->x_min = G_MAXDOUBLE;
	priv->y_max = -G_MAXDOUBLE;
	priv->y_min = G_MAXDOUBLE;
}

static void
gtk_cairo_plot_model_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *spec)
{
	GtkCairoPlotModel *model;
	GtkCairoPlotModelPriv *priv;

	model = GTK_CAIRO_PLOT_MODEL (object);
	priv = model->priv;
	
	switch (prop_id){
		case ARG_TITLE:
			if (priv->title) g_free (priv->title);
			priv->title = g_strdup (g_value_get_string (value));
			g_signal_emit_by_name (G_OBJECT (model), "changed");
		break;
		case ARG_X_UNIT:
			if (priv->x_title) g_free (priv->x_title);
			priv->x_title = g_strdup (g_value_get_string (value));
			g_signal_emit_by_name (G_OBJECT (model), "changed");
		break;
		case ARG_Y_UNIT:
			if (priv->y_title) g_free (priv->y_title);
			priv->y_title = g_strdup (g_value_get_string (value));
			g_signal_emit_by_name (G_OBJECT (model), "changed");
		break;
		case ARG_LOGX:
			priv->logx = g_value_get_boolean (value);
	}
}

static void
gtk_cairo_plot_model_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *spec)
{
	GtkCairoPlotModel *model;
	GtkCairoPlotModelPriv *priv;

	model = GTK_CAIRO_PLOT_MODEL (object);
	priv = model->priv;
	
	switch (prop_id){
		case ARG_TITLE:
			g_value_set_string (value, priv->title);
		break;
		case ARG_X_UNIT:
			g_value_set_string (value, priv->x_title);
		break;
		case ARG_Y_UNIT:
			g_value_set_string (value, priv->y_title);
		break;
		case ARG_LOGX:
			g_value_set_boolean (value, priv->logx);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(model, prop_id, spec);
	}
}

GtkCairoPlotModel *
gtk_cairo_plot_model_new (void)
{
	GtkCairoPlotModel *model;

	model = GTK_CAIRO_PLOT_MODEL (g_object_new (GTK_CAIRO_PLOT_MODEL_TYPE, NULL));

	return model;
}

void
gtk_cairo_plot_model_add_point(GtkCairoPlotModel *model, const gchar *func_name, Point p)
{
	GArray *func_data;

	g_return_if_fail (IS_GTK_CAIRO_PLOT_MODEL (model));

	func_data = g_hash_table_lookup (model->priv->functions, func_name);

	if (func_data == NULL) {
		/* This is the first point, we need to create fisrt */
		func_data = g_array_new (FALSE, FALSE, sizeof (Point));

		g_return_if_fail (func_data != NULL);
	
		g_hash_table_insert (model->priv->functions, (gchar *)func_name, func_data);
	}

	g_array_append_val (func_data, p);

	if (p.x > model->priv->x_max) model->priv->x_max = p.x;
	if (p.x < model->priv->x_min) model->priv->x_min = p.x;
	if (p.y > model->priv->y_max) model->priv->y_max = p.y;
	if (p.y < model->priv->y_min) model->priv->y_min = p.y;

	g_signal_emit_by_name (G_OBJECT (model), "changed");
}

guint
gtk_cairo_plot_model_get_num_points (GtkCairoPlotModel *model, const gchar *func_name)
{
	GArray *func_data;
	g_return_val_if_fail (IS_GTK_CAIRO_PLOT_MODEL (model), -1);

	func_data = g_hash_table_lookup (model->priv->functions, func_name);

	g_return_val_if_fail (func_data != NULL, -1);

	return func_data->len;
}

Point *
gtk_cairo_plot_model_get_point (GtkCairoPlotModel *model, const gchar *func_name, guint pos)
{
	GArray *func_data;

	func_data = g_hash_table_lookup (model->priv->functions, func_name);

	g_return_val_if_fail (func_data != NULL, NULL);
	g_return_val_if_fail (pos < func_data->len, NULL);

	return &g_array_index (func_data, Point, pos);
}

void
gtk_cairo_plot_model_get_x_minmax (GtkCairoPlotModel *model, gdouble *min, gdouble *max)
{
	(*min) = model->priv->x_min;
	(*max) = model->priv->x_max;
}

void
gtk_cairo_plot_model_get_y_minmax (GtkCairoPlotModel *model, gdouble *min, gdouble *max)
{
	(*min) = model->priv->y_min;
	(*max) = model->priv->y_max;
}

void
gtk_cairo_plot_model_load_from_file (GtkCairoPlotModel *model, const gchar *filename, const gchar *func_name)
{
	char line[255];
	FILE *fp;
	gchar **p, *fin;
	Point point;

	fp = fopen (filename, "rt");
	g_return_if_fail (fp != NULL);

	while (!feof(fp)) {
		fscanf (fp, "%[^\n]", line);
		p = g_strsplit (line, ",", 2);
		point.x = g_strtod(p[0], &fin);
		point.y = g_strtod(p[1], &fin);

		gtk_cairo_plot_model_add_point (model, func_name, point);
		fgetc(fp); /* Read \n from file */
	}
	fclose(fp);
}

static void
_add_to_list (gpointer key, gpointer value, gpointer data)
{
	GList *list = *((GList **)data);

	list = g_list_append (list, key);

	*((GList **)data) = list;
}

GList*
gtk_cairo_plot_model_get_functions (GtkCairoPlotModel *model)
{
	GList *lst = NULL;

	g_hash_table_foreach (model->priv->functions, _add_to_list, &lst);

	return lst;
}

static void
_clear_func (gpointer key, gpointer value, gpointer data)
{
	GArray *array = (GArray *)value;

	g_array_free (array, TRUE);
}

void
gtk_cairo_plot_model_clear (GtkCairoPlotModel *model)
{

	g_return_if_fail (model != NULL);
	g_return_if_fail (IS_GTK_CAIRO_PLOT_MODEL (model));

	g_hash_table_foreach (model->priv->functions, _clear_func, model);

	g_hash_table_destroy (model->priv->functions);
	model->priv->functions = g_hash_table_new (g_str_hash, g_str_equal);
	model->priv->x_max = -G_MAXDOUBLE;
	model->priv->x_min = G_MAXDOUBLE;
	model->priv->y_max = -G_MAXDOUBLE;
	model->priv->y_min = G_MAXDOUBLE;
}

