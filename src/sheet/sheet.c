/*
 * sheet.c
 *
 *
 * Authors:
 *  Richard Hult <rhult@hem.passagen.se>
 *  Ricardo Markiewicz <rmarkie@fi.uba.ar>
 *  Andres de Barbara <adebarbara@fi.uba.ar>
 *
 * Web page: http://arrakis.lug.fi.uba.ar/
 *
 * Copyright (C) 1999-2001  Richard Hult
 * Copyright (C) 2003,2006  Ricardo Markiewicz
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

#include <gdk/gdkprivate.h>
#include "sheet-private.h"
#include "sheet-item.h"
#include "node-store.h"
#include "wire-item.h"
#include "part-item.h"
#include "grid.h"

static void sheet_class_init (SheetClass *klass);
static void sheet_init (Sheet *sheet);
static void sheet_set_property (GObject *object, guint prop_id,
								const GValue *value, GParamSpec *spec);
static void sheet_get_property (GObject *object, guint prop_id,
								GValue *value, GParamSpec *spec);
static void sheet_destroy (GtkObject *object);
static void sheet_set_zoom (const Sheet *sheet, double zoom);
static void sheet_realized (GtkWidget *widget, gpointer data);

enum {
	SELECTION_CHANGED,
	BUTTON_PRESS,
	CONTEXT_CLICK,
	CANCEL,
	RESET_TOOL,
	LAST_SIGNAL
};
static guint signals[LAST_SIGNAL] = { 0 };

GnomeCanvasClass *sheet_parent_class = NULL;

enum {
	ARG_0,
	ARG_ZOOM
};

GType
sheet_get_type ()
{
	static GType sheet_type = 0;

	if (!sheet_type) {
		static const GTypeInfo sheet_info = {
			sizeof(SheetClass),
			NULL,
			NULL,
			(GClassInitFunc)sheet_class_init,
			NULL,
			NULL,
			sizeof(Sheet),
			0,
			(GInstanceInitFunc)sheet_init,
			NULL
		};

		sheet_type = g_type_register_static(GNOME_TYPE_CANVAS,
											"Sheet", &sheet_info, 0);
	}
	return sheet_type;
}

static void
sheet_class_init (SheetClass *sheet_class)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;

	object_class = G_OBJECT_CLASS (sheet_class);
	widget_class = GTK_WIDGET_CLASS (sheet_class);
	sheet_parent_class = g_type_class_peek(GNOME_TYPE_CANVAS);

	object_class->set_property = sheet_set_property;
	object_class->get_property = sheet_get_property;

	g_object_class_install_property(
			object_class,
			ARG_ZOOM,
			g_param_spec_double("zoom", "Sheet::zoom", "the zoom factor",
								0.01f, 10.0f, 1.0f, G_PARAM_READWRITE)
	);

	//object_class->destroy = sheet_destroy;

	/* Signals.  */
	signals[SELECTION_CHANGED] = g_signal_new ("selection_changed",
											   G_TYPE_FROM_CLASS(object_class),
											   G_SIGNAL_RUN_FIRST,
											   G_STRUCT_OFFSET (SheetClass, selection_changed),
											   NULL,
											   NULL,
											   g_cclosure_marshal_VOID__VOID,
											   G_TYPE_NONE, 0);
	signals[BUTTON_PRESS] = g_signal_new ("button_press",
										  G_TYPE_FROM_CLASS(object_class),
										  G_SIGNAL_RUN_FIRST,
										  G_STRUCT_OFFSET(SheetClass, button_press),
										  NULL,
										  NULL,
										  g_cclosure_marshal_VOID__POINTER,
										  /* TODO deberia ser boolean? */
										  G_TYPE_NONE, 1, GDK_TYPE_EVENT);
	signals[CONTEXT_CLICK] = g_signal_new ("context_click",
										   G_TYPE_FROM_CLASS(object_class),
										   G_SIGNAL_RUN_FIRST,
										   G_STRUCT_OFFSET(SheetClass, context_click),
										   NULL,
										   NULL,
										   g_cclosure_marshal_VOID__POINTER,
										   G_TYPE_NONE, 2, G_TYPE_STRING, G_TYPE_POINTER);
	signals[CONTEXT_CLICK] = g_signal_new ("cancel",
										   G_TYPE_FROM_CLASS(object_class),
										   G_SIGNAL_RUN_FIRST,
										   G_STRUCT_OFFSET(SheetClass, cancel),
										   NULL,
										   NULL,
										   g_cclosure_marshal_VOID__VOID,
										   G_TYPE_NONE, 0);
	signals[RESET_TOOL] = g_signal_new ("reset_tool",
										G_TYPE_FROM_CLASS(object_class),
										G_SIGNAL_RUN_FIRST,
										G_STRUCT_OFFSET(SheetClass, reset_tool),
										NULL,
										NULL,
										g_cclosure_marshal_VOID__VOID,
										G_TYPE_NONE, 0);
}

static void
sheet_init (Sheet *sheet)
{
	sheet->priv = g_new0 (SheetPriv, 1);
	sheet->priv->zoom = 1.0;
	sheet->priv->selected_objects = NULL;
	sheet->priv->selected_group = NULL;
	sheet->priv->floating_group = NULL;
	sheet->priv->wire_handler_id = 0;
	sheet->priv->float_handler_id = 0;

	sheet->state = SHEET_STATE_NONE;

/*	GNOME_CANVAS (sheet)->aa = TRUE;*/
}

void
sheet_get_zoom (const Sheet *sheet, gdouble *zoom)
{
	*zoom = sheet->priv->zoom;
}

static void
sheet_set_zoom (const Sheet *sheet, double zoom)
{
	gnome_canvas_set_pixels_per_unit (GNOME_CANVAS (sheet), zoom);
	sheet->priv->zoom = zoom;
}

void
sheet_change_zoom (const Sheet *sheet, gdouble rate)
{
	sheet->priv->zoom *= rate;

	gnome_canvas_set_pixels_per_unit (GNOME_CANVAS (sheet), sheet->priv->zoom);
}

static void
sheet_realized (GtkWidget *widget, gpointer data)
{
	GdkWindow *window;
	GdkColormap *colormap;
	GtkStyle  *style;

	/*
	 * We set the background pixmap to NULL so that X won't clear
	 * exposed areas and thus be faster.
	 */
	gdk_window_set_back_pixmap (GTK_LAYOUT (widget)->bin_window, NULL, FALSE);

	window = widget->window;

	/*
	 * Set the background to white.
	 */
	style = gtk_style_copy (widget->style);
	colormap = gtk_widget_get_colormap (widget);
	gdk_color_white (colormap, &style->bg[GTK_STATE_NORMAL]);
	gtk_widget_set_style (widget, style);
	gtk_style_unref (style);
}

GtkWidget *
sheet_new (int width, int height)
{
	GnomeCanvas *sheet_canvas;
	GnomeCanvasGroup *sheet_group;
	GnomeCanvasPoints *points;
	Sheet *sheet;
	GtkWidget *sheet_widget;

	/* Creo el Canvas con Anti-aliasing */
	sheet = SHEET(g_object_new(TYPE_SHEET, NULL));

	g_signal_connect(G_OBJECT(sheet),
					 "realize", G_CALLBACK(sheet_realized), 0);

	sheet_canvas = GNOME_CANVAS(sheet);
	sheet_group = GNOME_CANVAS_GROUP(sheet_canvas->root);
	sheet_widget = GTK_WIDGET(sheet);

	gnome_canvas_set_scroll_region (sheet_canvas,
									0, 0, width + 20, height + 20);

	/* Since there is no API for this (yet), tweak it directly. */
	sheet_canvas->close_enough = 6.0;

	sheet->priv->width = width;
	sheet->priv->height = height;

	/* Create the dot grid. */
	sheet->grid = GRID (gnome_canvas_item_new (
		sheet_group,
		grid_get_type (),
		"color", "dark gray",
		"spacing", 10.0,
		"snap", TRUE,
		NULL));

	grid_show (sheet->grid, TRUE);

	/* Everything outside the sheet should be gray. */
	/* top */
	gnome_canvas_item_new (
		sheet_group,
		gnome_canvas_rect_get_type (),
		"fill_color", "gray",
		"outline_color", NULL,
		"width_pixels", (int)1,
		"x1", 0.0,
		"y1", 0.0,
		"x2", (double)width + 20,
		"y2", 20.0,
		NULL);

	gnome_canvas_item_new (
		sheet_group,
		gnome_canvas_rect_get_type (),
		"fill_color", "gray",
		"outline_color", NULL,
		"width_pixels", (int)1,
		"x1", 0.0,
		"y1", (double)height,
		"x2", (double)width + 20,
		"y2", (double)height + 20,
		NULL);

	/* right */
	gnome_canvas_item_new (
		sheet_group,
		gnome_canvas_rect_get_type (),
		"fill_color", "gray",
		"outline_color", NULL,
		"width_pixels", (int)1,
		"x1", 0.0,
		"y1", 0.0,
		"x2", 20.0,
		"y2", (double)height + 20,
		NULL);

	gnome_canvas_item_new (
		sheet_group,
		gnome_canvas_rect_get_type (),
		"fill_color", "gray",
		"outline_color", NULL,
		"width_pixels", (int)1,
		"x1", (double)width,
		"y1", 0.0,
		"x2", (double)width + 20,
		"y2", (double)height + 20,
		NULL);

	/*  Draw a thin black border around the sheet. */
	points = gnome_canvas_points_new (5);
	points->coords[0] = 20.0;
	points->coords[1] = 20.0;
	points->coords[2] = width;
	points->coords[3] = 20.0;
	points->coords[4] = width;
	points->coords[5] = height;
	points->coords[6] = 20.0;
	points->coords[7] = height;
	points->coords[8] = 20.0;
	points->coords[9] = 20.0;
	gnome_canvas_item_new (
		sheet_group,
		gnome_canvas_line_get_type (),
		"fill_color", "black",
		"width_pixels", (int)1,
		"points", points,
		NULL);

	gnome_canvas_points_free (points);

	/* Finally, create the object group that holds all objects. */
	sheet->object_group = GNOME_CANVAS_GROUP(gnome_canvas_item_new (
		gnome_canvas_root(GNOME_CANVAS(sheet_canvas)),
		GNOME_TYPE_CANVAS_GROUP,
		"x", 0.0,
		"y", 0.0,
		NULL));

	sheet->priv->selected_group = GNOME_CANVAS_GROUP (gnome_canvas_item_new (
		sheet->object_group,
		GNOME_TYPE_CANVAS_GROUP,
		"x", 0.0,
		"y", 0.0,
		NULL));

	sheet->priv->floating_group = GNOME_CANVAS_GROUP (gnome_canvas_item_new (
		sheet->object_group,
		gnome_canvas_group_get_type (),
		"x", 0.0,
		"y", 0.0,
		NULL));

	return sheet_widget;
}

static void
sheet_set_property (GObject *object,
					guint prop_id, const GValue *value, GParamSpec *spec)
{
	const Sheet *sheet = SHEET (object);

	switch (prop_id) {
	case ARG_ZOOM:
		sheet_set_zoom (sheet, g_value_get_double(value));
		break;
	}
}

static void
sheet_get_property (GObject *object,
					guint prop_id, GValue *value, GParamSpec *spec)
{
	const Sheet *sheet = SHEET (object);

	switch (prop_id) {
	case ARG_ZOOM:
		g_value_set_double(value, sheet->priv->zoom);
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(sheet, prop_id, spec);
		break;
	}
}

static void
sheet_destroy (GtkObject *object)
{
	Sheet *sheet;
	SheetPriv *priv;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_SHEET (object));

	sheet = SHEET (object);
	priv = sheet->priv;

	if (priv) {
		g_list_free (priv->selected_objects);
		g_list_free (priv->floating_objects);

		/*
		 * We need to destroy this first so that things get destroyed in the
		 * right order. Label groups are destroyed in their parent parts'
		 * destroy handler, so when the sheet gets destroyed, label groups
		 * will screw things up if this is not done.
		 */
		gtk_object_destroy (GTK_OBJECT (sheet->object_group));

		g_free (priv);
		sheet->priv = NULL;
	}

	if (GTK_OBJECT_CLASS (sheet_parent_class)->destroy)
		(* GTK_OBJECT_CLASS (sheet_parent_class)->destroy) (object);
}

void
sheet_scroll (const Sheet *sheet, int delta_x, int delta_y)
{
	GtkAdjustment *hadj, *vadj;
	GtkAllocation *allocation;
	gfloat vnew, hnew;
	gfloat hmax, vmax;
	const SheetPriv *priv = sheet->priv;

	hadj = GTK_LAYOUT (sheet)->hadjustment;
	vadj = GTK_LAYOUT (sheet)->vadjustment;

	allocation = &GTK_WIDGET (sheet)->allocation;

	if (priv->width > allocation->width)
		hmax = (gfloat) (priv->width - allocation->width);
	else
		hmax = 0.0;

	if (priv->height > allocation->height)
		vmax = (gfloat) (priv->height - allocation->height);
	else
		vmax = 0.0;

	hnew = CLAMP (hadj->value + (gfloat) delta_x, 0.0, hmax);
	vnew = CLAMP (vadj->value + (gfloat) delta_y, 0.0, vmax);

	if (hnew != hadj->value) {
		hadj->value = hnew;
		g_signal_emit_by_name (G_OBJECT (hadj), "value_changed");
	}
	if (vnew != vadj->value) {
		vadj->value = vnew;
		g_signal_emit_by_name (G_OBJECT (vadj), "value_changed");
	}
}

void
sheet_get_size_pixels (const Sheet *sheet, guint *width, guint *height)
{
	*width = sheet->priv->width * sheet->priv->zoom;
	*height = sheet->priv->height * sheet->priv->zoom;
}

void
sheet_dialog_set_parent (const Sheet *sheet, GtkDialog *dialog)
{
/*	gtk_window_set_transient_for (
		GTK_WINDOW (dialog),
		GTK_WINDOW (SCHEMATIC (sheet->schematic)->toplevel)
	);*/
}
