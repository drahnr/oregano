/*
 * sheet.c
 *
 *
 * Authors:
 *  Richard Hult <rhult@hem.passagen.se>
 *  Ricardo Markiewicz <rmarkie@fi.uba.ar>
 *  Andres de Barbara <adebarbara@fi.uba.ar>
 *  Marc Lorber <lorber.marc@wanadoo.fr>
 
 * Web page: https://github.com/marc-lorber/oregano
 *
 * Copyright (C) 1999-2001  Richard Hult
 * Copyright (C) 2003,2006  Ricardo Markiewicz
 * Copyright (C) 2009-2012  Marc Lorber
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

#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>
#include <math.h>
#include <goocanvas.h>
#include <goocanvasutils.h>

#include "sheet-private.h"
#include "sheet-item.h"
#include "node-store.h"
#include "node-item.h"
#include "wire-item.h"
#include "part-item.h"
#include "grid.h"
#include "sheet-item-factory.h"
#include "schematic-view.h"

static void 	sheet_class_init (SheetClass *klass);
static void 	sheet_init (Sheet *sheet);
static void 	sheet_set_property (GObject *object, guint prop_id,
					const GValue *value, GParamSpec *spec);
static void 	sheet_get_property (GObject *object, guint prop_id,
					GValue *value, GParamSpec *spec);
static void 	sheet_set_zoom (const Sheet *sheet, double zoom);
static GList *	sheet_preserve_selection (Sheet *sheet);
static void		rotate_items (Sheet *sheet, GList *items);
static void		flip_items (Sheet *sheet, GList *items, gboolean horizontal);
static void 	node_dot_added_callback (Schematic *schematic, SheetPos *pos, 
					Sheet *sheet);
static void 	node_dot_removed_callback (Schematic *schematic, SheetPos *pos, 
    				Sheet *sheet);
static void		sheet_finalize (GObject *object);
static int 		dot_equal (gconstpointer a, gconstpointer b);
static guint 	dot_hash (gconstpointer key);


#define ZOOM_MIN 0.35
#define ZOOM_MAX 3

#define NG_DEBUG(s) if (0) g_print ("%s\n", s)

enum {
	SELECTION_CHANGED,
	BUTTON_PRESS,
	CONTEXT_CLICK,
	CANCEL,
	LAST_SIGNAL
};
static guint signals[LAST_SIGNAL] = { 0 };

enum {
	ARG_0,
	ARG_ZOOM
};

G_DEFINE_TYPE (Sheet, sheet, GOO_TYPE_CANVAS)

static void
sheet_class_init (SheetClass *sheet_class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (sheet_class);

	object_class->set_property = sheet_set_property;
	object_class->get_property = sheet_get_property;
	object_class->finalize = sheet_finalize;
	sheet_parent_class = g_type_class_peek (GOO_TYPE_CANVAS);

	g_object_class_install_property (object_class, ARG_ZOOM,
			g_param_spec_double ("zoom", "Sheet::zoom", "the zoom factor",
								0.01f, 10.0f, 1.0f, G_PARAM_READWRITE));
	
	// Signals.
	signals[SELECTION_CHANGED] = g_signal_new ("selection_changed",
				G_TYPE_FROM_CLASS (object_class),
				G_SIGNAL_RUN_FIRST,
				G_STRUCT_OFFSET (SheetClass, selection_changed),
				NULL,
				NULL,
				g_cclosure_marshal_VOID__VOID,
				G_TYPE_NONE, 
	            0);
	
	signals[BUTTON_PRESS] = g_signal_new ("button_press",
				G_TYPE_FROM_CLASS (object_class),
				G_SIGNAL_RUN_FIRST,
				G_STRUCT_OFFSET (SheetClass, button_press),
				NULL,
				NULL,
				g_cclosure_marshal_VOID__POINTER,
				G_TYPE_NONE, 
	            1, 
	            GDK_TYPE_EVENT);
	
	signals[CONTEXT_CLICK] = g_signal_new ("context_click",
				G_TYPE_FROM_CLASS (object_class),
				G_SIGNAL_RUN_FIRST,
				G_STRUCT_OFFSET (SheetClass, context_click),
				NULL,
				NULL,
				g_cclosure_marshal_VOID__POINTER,
				G_TYPE_NONE, 
	            2, 
	            G_TYPE_STRING, 
	            G_TYPE_POINTER);
	
	signals[CONTEXT_CLICK] = g_signal_new ("cancel",
				G_TYPE_FROM_CLASS (object_class),
				G_SIGNAL_RUN_FIRST,
				G_STRUCT_OFFSET (SheetClass, cancel),
				NULL,
				NULL,
				g_cclosure_marshal_VOID__VOID,
				G_TYPE_NONE, 
	            0);
}

static 
cairo_pattern_t*
create_stipple (const char *color_name, guchar stipple_data[16])
{
  cairo_surface_t *surface;
  cairo_pattern_t *pattern;
  GdkColor color;

  gdk_color_parse (color_name, &color);
  stipple_data[2] = stipple_data[14] = color.red >> 8;
  stipple_data[1] = stipple_data[13] = color.green >> 8;
  stipple_data[0] = stipple_data[12] = color.blue >> 8;
  surface = cairo_image_surface_create_for_data (stipple_data,
						 CAIRO_FORMAT_ARGB32,
						 2, 2, 8);
  pattern = cairo_pattern_create_for_surface (surface);
  cairo_surface_destroy (surface);
  cairo_pattern_set_extend (pattern, CAIRO_EXTEND_REPEAT);

  return pattern;
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
	
	sheet->priv->items = NULL;
	sheet->priv->rubberband = g_new0 (RubberbandInfo, 1);
	sheet->priv->rubberband->state = RUBBER_NO;
	sheet->priv->preserve_selection_items = NULL;
	sheet->priv->sheet_parent_class = g_type_class_ref (GOO_TYPE_CANVAS);
	sheet->priv->voltmeter_items = NULL;
	sheet->priv->voltmeter_nodes = g_hash_table_new_full (g_str_hash,
			g_str_equal, g_free, g_free);

	sheet->state = SHEET_STATE_NONE;
}

static void
sheet_finalize (GObject *object)
{	
	Sheet *sheet = SHEET (object);

	if (sheet->priv) {
		g_hash_table_destroy (sheet->priv->node_dots);
		g_free (sheet->priv);
	}
	if (G_OBJECT_CLASS (sheet_parent_class)->finalize)
		(* G_OBJECT_CLASS (sheet_parent_class)->finalize) (object);
}

void		
sheet_get_pointer (Sheet *sheet, gdouble *x, gdouble *y)
{
	GtkWidget     *widget;
	GtkAdjustment *hadjustment;
	GtkAdjustment *vadjustment;
	gdouble value, x1, y1;
	gint _x, _y;

	gtk_widget_get_pointer (GTK_WIDGET (sheet), &_x, &_y);
	x1 = (gdouble) _x;
	y1 = (gdouble) _y;
	
	widget = gtk_widget_get_parent (GTK_WIDGET (sheet));
	hadjustment =  gtk_scrolled_window_get_hadjustment (
	                 GTK_SCROLLED_WINDOW (widget));
	value = gtk_adjustment_get_value (hadjustment);

	x1 += value;
	vadjustment =  gtk_scrolled_window_get_vadjustment (
	                 GTK_SCROLLED_WINDOW (widget));
	value = gtk_adjustment_get_value (vadjustment);
	y1 += value;
	*x = x1;
	*y = y1;
	goo_canvas_convert_from_pixels (GOO_CANVAS (sheet), x, y);
	snap_to_grid (sheet->grid, x, y);
}

void
sheet_get_zoom (const Sheet *sheet, gdouble *zoom)
{
	*zoom = sheet->priv->zoom;
}

static void
sheet_set_zoom (const Sheet *sheet, double zoom)
{
	goo_canvas_set_scale (GOO_CANVAS (sheet), zoom);
	sheet->priv->zoom = zoom;
}

void
sheet_change_zoom (const Sheet *sheet, gdouble rate)
{
	gdouble scale;
	
	sheet->priv->zoom *= rate;
	scale = goo_canvas_get_scale (GOO_CANVAS (sheet));
	scale = scale * rate;
	goo_canvas_set_scale (GOO_CANVAS (sheet), scale);
}

// This function defines the drawing sheet on which schematic will be drawn 
GtkWidget *
sheet_new (int width, int height)
{
	GooCanvas *sheet_canvas;
	GooCanvasGroup *sheet_group;
	GooCanvasPoints *points;
	Sheet *sheet;
	GtkWidget *sheet_widget;
  	GooCanvasItem *root;
	
	// Creation of the Canvas
	sheet = SHEET (g_object_new (TYPE_SHEET, NULL));

	sheet_canvas = GOO_CANVAS (sheet);
	g_object_set (G_OBJECT (sheet_canvas), 
                  "bounds-from-origin", FALSE,
                  "bounds-padding", 4.0,
	              "background-color-rgb", 0xFFFFFF,
	              NULL);

  	root = goo_canvas_get_root_item (sheet_canvas);
	
	sheet_group = GOO_CANVAS_GROUP (goo_canvas_group_new (
	                                root,
	                                NULL));
	sheet_widget = GTK_WIDGET (sheet);

	goo_canvas_set_bounds (GOO_CANVAS (sheet_canvas), 0, 0, 
	    width + 20, height + 20);

	// Define vicinity around GooCanvasItem
	//sheet_canvas->close_enough = 6.0;

	sheet->priv->width = width;
	sheet->priv->height = height;

	// Create the dot grid.
	sheet->grid = grid_create (GOO_CANVAS_ITEM (sheet_group),
	                           width,
	                           height);

	// Everything outside the sheet should be gray.
	// top //
	goo_canvas_rect_new (GOO_CANVAS_ITEM (sheet_group), 
	                     0.0, 
	                     0.0, 
	                     (double) width + 20.0, 
	                     20.0, 
		                 "fill_color", "gray", 
	                     "line-width", 0.0, 
	                     NULL);

	goo_canvas_rect_new (GOO_CANVAS_ITEM (sheet_group), 
	                     0.0, 
	                     (double) height, 
	                     (double) width + 20.0, 
	                     (double) height + 20.0, 
	                     "fill_color", "gray", 
	                     "line-width", 0.0, 
	                     NULL);                

	// right //
	goo_canvas_rect_new (GOO_CANVAS_ITEM (sheet_group), 
	                     0.0, 
	                     0.0, 
	                     20.0, 
	                     (double) height + 20.0, 
	                     "fill_color", "gray", 
	                     "line-width", 0.0, 
	                     NULL);                  

	goo_canvas_rect_new (GOO_CANVAS_ITEM (sheet_group), 
	                     (double) width, 
	                     0.0, 
	                     (double) width + 20.0, 
	                     (double) height + 20.0, 
	                     "fill_color", "gray", 
	                     "line-width", 0.0, 
	                     NULL);                 

	//  Draw a thin black border around the sheet.
	points = goo_canvas_points_new (5);
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
	
	goo_canvas_polyline_new (GOO_CANVAS_ITEM (sheet_group),
	      FALSE, 0,
	      "line-width", 1.0, 
	      "points", points, 
	      NULL);

	goo_canvas_points_unref (points);

	// Finally, create the object group that holds all objects.
	sheet->object_group = GOO_CANVAS_GROUP (goo_canvas_group_new (
	                      root,
	     				  "x", 0.0, 
	                      "y", 0.0,
	                      NULL));

	sheet->priv->selected_group = GOO_CANVAS_GROUP (goo_canvas_group_new (
	     GOO_CANVAS_ITEM (sheet->object_group), 
	     "x", 0.0, 
	     "y", 0.0,
	     NULL));

	sheet->priv->floating_group = GOO_CANVAS_GROUP (goo_canvas_group_new (
	     GOO_CANVAS_ITEM (sheet->object_group), 
	     "x", 0.0, 
	     "y", 0.0, 
	     NULL));
	
	// Hash table that keeps maps coordinate to a specific dot.
	sheet->priv->node_dots = g_hash_table_new_full (dot_hash, dot_equal, g_free, 
	                                                NULL);
	
	return sheet_widget;
}

static void
sheet_set_property (GObject *object,
					guint prop_id, const GValue *value, GParamSpec *spec)
{
	const Sheet *sheet = SHEET (object);

	switch (prop_id) {
	case ARG_ZOOM:
		sheet_set_zoom (sheet, g_value_get_double (value));
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
		g_value_set_double (value, sheet->priv->zoom);
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (sheet, prop_id, spec);
		break;
	}
}

void
sheet_scroll (const Sheet *sheet, int delta_x, int delta_y)
{
	GtkAdjustment *hadj, *vadj;
	GtkAllocation allocation;
	gfloat vnew, hnew;
	gfloat hmax, vmax;
	const SheetPriv *priv = sheet->priv;

	hadj = gtk_container_get_focus_hadjustment (GTK_CONTAINER (sheet));
	vadj = gtk_container_get_focus_vadjustment (GTK_CONTAINER (sheet));

	gtk_widget_get_allocation (GTK_WIDGET (sheet), &allocation);

	if (priv->width > allocation.width)
		hmax = (gfloat) (priv->width - allocation.width);
	else
		hmax = 0.0;

	if (priv->height > allocation.height)
		vmax = (gfloat) (priv->height -  allocation.height);
	else
		vmax = 0.0;

	hnew = CLAMP (gtk_adjustment_get_value (hadj) + (gfloat) delta_x, 0.0, hmax);
	vnew = CLAMP (gtk_adjustment_get_value (vadj) + (gfloat) delta_y, 0.0, vmax);

	if (hnew != gtk_adjustment_get_value (hadj)) {
		gtk_adjustment_set_value (hadj, hnew);
		g_signal_emit_by_name (G_OBJECT (hadj), "value_changed");
	}
	if (vnew != gtk_adjustment_get_value (vadj)) {
		gtk_adjustment_set_value (vadj, vnew);
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
sheet_remove_selected_object (const Sheet *sheet, SheetItem *item)
{
	sheet->priv->selected_objects = 
		g_list_remove (sheet->priv->selected_objects, item);
}

void 	   
sheet_prepend_selected_object (Sheet *sheet, SheetItem *item)
{
	sheet->priv->selected_objects =
		g_list_prepend (sheet->priv->selected_objects, item);
}

void 	   
sheet_remove_floating_object (const Sheet *sheet, SheetItem *item)
{
	sheet->priv->floating_objects = 
		g_list_remove (sheet->priv->floating_objects, item);
}

void 	  
sheet_prepend_floating_object (Sheet *sheet, SheetItem *item)
{
	sheet->priv->floating_objects = 
		g_list_prepend (sheet->priv->floating_objects, item);
}

void
sheet_connect_part_item_to_floating_group (Sheet *sheet, gpointer *sv)
{
	g_return_if_fail (sheet != NULL);
	g_return_if_fail (IS_SHEET (sheet));
	g_return_if_fail (sv != NULL);
	g_return_if_fail (IS_SCHEMATIC_VIEW (sv));

	sheet->state = SHEET_STATE_FLOAT_START;

	if (sheet->priv->float_handler_id != 0)
		return;

	sheet->priv->float_handler_id = g_signal_connect(G_OBJECT (sheet), 
	        "event", G_CALLBACK (sheet_item_floating_event), 
	        sv);
}

void
sheet_show_node_labels (Sheet *sheet, gboolean show)
{
	g_return_if_fail (sheet != NULL);
	g_return_if_fail (IS_SHEET (sheet));

	GList *item;

	for (item = sheet->priv->items; item; item=item->next) {
    	if (IS_PART_ITEM (item->data))
			if (part_get_num_pins (PART (sheet_item_get_data (SHEET_ITEM(item->data))))==1)
				part_item_show_node_labels (PART_ITEM (item->data), show);
        }
	g_list_free_full (item, g_object_unref);
}

void
sheet_add_item (Sheet *sheet, SheetItem *item)
{
	g_return_if_fail (sheet != NULL);
	g_return_if_fail (IS_SHEET (sheet));
	g_return_if_fail (item != NULL);
	g_return_if_fail (IS_SHEET_ITEM (item));

	sheet->priv->items = g_list_prepend (sheet->priv->items, item);
}

int    
sheet_rubberband_timeout_cb (Sheet *sheet)
{
	static double width_old = 0, height_old = 0;
    double x, y;
	double height, width;
	
    double dx, dy;
    GList *list;
    SheetPos p1, p2;

    // Obtains the current pointer position and modifier state.
    // The position is given in coordinates relative to window.
	sheet_get_pointer (sheet, &x, &y);

	if (x < sheet->priv->rubberband->start_x) {
		width = sheet->priv->rubberband->start_x - x;
	}
	else {
		double tmp = x;
		x = sheet->priv->rubberband->start_x;
		width = tmp - sheet->priv->rubberband->start_x;
	}

	if (y < sheet->priv->rubberband->start_y) {
		height = sheet->priv->rubberband->start_y - y;
	}
	else {
		double tmp = y;
		y = sheet->priv->rubberband->start_y;
		height = tmp - sheet->priv->rubberband->start_y;
	}

	p1.x = x;
	p1.y = y;
	p2.x = x + width;
	p2.y = y + height;

	// Scroll the sheet if needed.
	// Need FIX
	/*{
		int width, height;
		int dx = 0, dy = 0;
		GtkAllocation allocation;
		
		sheet_get_pointer (sheet, &x, &y);

		gtk_widget_get_allocation (GTK_WIDGET (sheet), &allocation);
		width = allocation.width;
		height = allocation.height;

		if (_x < 0)
			dx = -1;
		else if (_x > width)
			dx = 1;

		if (_y < 0)
			dy = -1;
		else if (_y > height)
			dy = 1;

		if (!(_x > 0 && _x < width && _y > 0 && _y < height))
			sheet_scroll (sheet, dx * 5, dy * 5);
	}*/

	// Modify the rubberband rectangle if needed
	dx = fabs (width - width_old);
	dy = fabs (height - height_old);
	if (dx > 1.0 || dy > 1.0) {
		// Save old state
		width_old = width;
		height_old = height;

		for (list = sheet->priv->items; list; list = list->next) {
			sheet_item_select_in_area (list->data, &p1, &p2);
		}

		g_object_set (sheet->priv->rubberband->rectangle,
		              "x", (double) x, 
		              "y", (double) y,
		              "width", width, 
		              "height", height,
		              NULL);
	}
	g_list_free_full (list, g_object_unref);
	return TRUE;
}

void
sheet_stop_rubberband (Sheet *sheet, GdkEventButton *event)
{
	GList *list;

	g_source_remove (sheet->priv->rubberband->timeout_id);
	sheet->priv->rubberband->state = RUBBER_NO;

	if (sheet->priv->preserve_selection_items != NULL) {
		for (list = sheet->priv->preserve_selection_items; list; list = list->next)
			sheet_item_set_preserve_selection (SHEET_ITEM (list->data), FALSE);
		
		g_list_free (sheet->priv->preserve_selection_items);
		sheet->priv->preserve_selection_items = NULL;
	}

	goo_canvas_pointer_ungrab (GOO_CANVAS (sheet),
	                           GOO_CANVAS_ITEM (sheet->grid), event->time);
	
	goo_canvas_item_remove (GOO_CANVAS_ITEM (sheet->priv->rubberband->rectangle));
	g_list_free_full (list, g_object_unref);
}

void 
sheet_setup_rubberband (Sheet *sheet, GdkEventButton *event)
{
	double x, y;
	cairo_pattern_t *pattern;
	static guchar stipple_data[16] = 
	{0, 0, 0, 255,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 255 };

	x = event->x; //the x coordinate of the pointer relative to the window.
	y = event->y; //the y coordinate of the pointer relative to the window.
	goo_canvas_convert_from_pixels (GOO_CANVAS (sheet), &x, &y);

	sheet->priv->rubberband->start_x = x;
	sheet->priv->rubberband->start_y = y;

	sheet->priv->rubberband->state = RUBBER_YES;
	sheet->priv->rubberband->click_start_state = event->state;

	pattern = create_stipple ("lightgrey", stipple_data);

	sheet->priv->rubberband->rectangle = goo_canvas_rect_new (
		GOO_CANVAS_ITEM (sheet->object_group), 
	    x, y, 0.0, 0.0, 
	    "stroke-color", "black",
	    "line-width", 0.2,
	    "fill-pattern", pattern,
	    NULL);

	goo_canvas_pointer_grab (GOO_CANVAS (sheet), GOO_CANVAS_ITEM (sheet->grid), 
		(GDK_POINTER_MOTION_MASK | GDK_BUTTON_RELEASE_MASK),
	    NULL, event->time);
	
	// Mark all the selected objects to preserve their selected state
	// if SHIFT is pressed while rubberbanding.
	if (event->state & GDK_SHIFT_MASK) {
		sheet->priv->preserve_selection_items = 
			g_list_copy (sheet_preserve_selection (sheet));
	}

	sheet->priv->rubberband->timeout_id = g_timeout_add (
			10,
			(gpointer) sheet_rubberband_timeout_cb,
			(gpointer) sheet);
}

GList *
sheet_preserve_selection (Sheet *sheet)
{
	g_return_val_if_fail (sheet != NULL, FALSE);
	g_return_val_if_fail (IS_SHEET (sheet), FALSE);
	
	GList *list;
	for (list = sheet->priv->selected_objects; list; list = list->next) {
		sheet_item_set_preserve_selection (SHEET_ITEM (list->data), TRUE);
	}
	// Return the list so that we can remove the preserve_selection
	// flags later.
	g_list_free_full (list, g_object_unref);
	return sheet->priv->selected_objects;
}

int
sheet_event_callback (GtkWidget *widget, GdkEvent *event, Sheet *sheet)
{
	switch (event->type) {
		case GDK_3BUTTON_PRESS:
			// We don't not care about triple clicks on the sheet.
			return FALSE;
		case GDK_2BUTTON_PRESS:
			// The sheet does not care about double clicks, but invoke the
		 	// canvas event handler and see if an item picks up the event.
			if ((*GTK_WIDGET_CLASS (sheet->priv->sheet_parent_class)
			     ->button_press_event) (widget, (GdkEventButton *)event))
			return TRUE;
		else
			return FALSE;
		case GDK_BUTTON_PRESS:
			// If we are in the middle of something else, don't interfere
		 	// with that.
			if (sheet->state != SHEET_STATE_NONE) {
				return FALSE;
			}

			if ((* GTK_WIDGET_CLASS (sheet->priv->sheet_parent_class)
		         ->button_press_event) (widget, (GdkEventButton *) event)) {
				return TRUE;
			}

			if (event->button.button == 3) {
				run_context_menu (
					schematic_view_get_schematicview_from_sheet (sheet), 
			    	(GdkEventButton *) event);
				return TRUE;
			}

			if (event->button.button == 1) {
				if (!(event->button.state & GDK_SHIFT_MASK))
					sheet_select_all (sheet, FALSE);

				sheet_setup_rubberband (sheet, (GdkEventButton *) event);
				return TRUE;
			}
			break;
		case GDK_BUTTON_RELEASE:
			if (event->button.button == 4 || event->button.button == 5)
				return TRUE;

			if (event->button.button == 1 					&&
		        sheet->priv->rubberband->state == RUBBER_YES) {
				sheet_stop_rubberband (sheet, (GdkEventButton *) event);
				return TRUE;
			}

			if (GTK_WIDGET_CLASS (sheet->priv->sheet_parent_class)->button_release_event != NULL) {
				return GTK_WIDGET_CLASS (sheet->priv->sheet_parent_class)
					->button_release_event (widget, (GdkEventButton *) event);
			}

			break;

		case GDK_SCROLL:
			if (((GdkEventScroll *)event)->direction == GDK_SCROLL_UP) {
				double zoom;
				sheet_get_zoom (sheet, &zoom);
				if (zoom < ZOOM_MAX)
					sheet_change_zoom (sheet, 1.1);
				} 
				else if (((GdkEventScroll *)event)->direction == GDK_SCROLL_DOWN) {
					double zoom;
				sheet_get_zoom (sheet, &zoom);
				if (zoom > ZOOM_MIN)
					sheet_change_zoom (sheet, 0.9);
				}
				break;
		case GDK_MOTION_NOTIFY:
			if (GTK_WIDGET_CLASS (sheet->priv->sheet_parent_class)
			    ->motion_notify_event != NULL) {
				return GTK_WIDGET_CLASS (sheet->priv->sheet_parent_class)
						->motion_notify_event (widget, (GdkEventMotion *) event);		
			}
		case GDK_ENTER_NOTIFY:
			GTK_WIDGET_CLASS (sheet->priv->sheet_parent_class)
						->enter_notify_event (widget, (GdkEventCrossing *) event);
		case GDK_KEY_PRESS:
			switch (event->key.keyval) {
				case GDK_KEY_R:
				case GDK_KEY_r:
					if (sheet->state == SHEET_STATE_NONE)
						sheet_rotate_selection (sheet);
					break;
				case GDK_KEY_Home:
				case GDK_KEY_End:
					break;
				case GDK_KEY_Left:
					if (event->key.state & GDK_MOD1_MASK)
						sheet_scroll (sheet, -20, 0);
					break;
				case GDK_KEY_Up:
					if (event->key.state & GDK_MOD1_MASK)
						sheet_scroll (sheet, 0, -20);
					break;
				case GDK_KEY_Right:
					if (event->key.state & GDK_MOD1_MASK)
						sheet_scroll (sheet, 20, 0);
					break;
				case GDK_KEY_Down:
					if (event->key.state & GDK_MOD1_MASK)
						sheet_scroll (sheet, 0, 20);
					break;
				case GDK_KEY_Page_Up:
					if (event->key.state & GDK_MOD1_MASK)
						sheet_scroll (sheet, 0, -120);
					break;
				case GDK_KEY_Page_Down:
					if (event->key.state & GDK_MOD1_MASK)
						sheet_scroll (sheet, 0, 120);
					break;
				case GDK_KEY_Escape:
					g_signal_emit_by_name (G_OBJECT (sheet), "cancel");
					break;
				case GDK_KEY_Delete:
					sheet_delete_selection (sheet);
					break;
				default:
					return FALSE;
			}
		default:
			return FALSE;
	}

	return TRUE;
}

void
sheet_select_all (Sheet *sheet, gboolean select)
{
	GList *list;

	g_return_if_fail (sheet != NULL);
	g_return_if_fail (IS_SHEET (sheet));

	for (list = sheet->priv->items; list; list = list->next)
		sheet_item_select (SHEET_ITEM (list->data), select);

	if (!select)
		sheet_release_selected_objects (sheet);
	g_list_free_full (list, g_object_unref);
}

void
sheet_rotate_selection (Sheet *sheet)
{
	g_return_if_fail (sheet != NULL);
	g_return_if_fail (IS_SHEET (sheet));

	if (sheet->priv->selected_objects != NULL)
		rotate_items (sheet, sheet->priv->selected_objects);
}

void
sheet_rotate_ghosts (Sheet *sheet)
{
	g_return_if_fail (sheet != NULL);
	g_return_if_fail (IS_SHEET (sheet));

	if (sheet->priv->floating_objects != NULL)
		rotate_items (sheet, sheet->priv->floating_objects);
}

static void
rotate_items (Sheet *sheet, GList *items)
{
	GList *list, *item_data_list;
	SheetPos center, b1, b2;

	item_data_list = NULL;
	for (list = items; list; list = list->next) {
		item_data_list = g_list_prepend (item_data_list,
			sheet_item_get_data (list->data));
	}

	item_data_list_get_absolute_bbox (item_data_list, &b1, &b2);

	center.x = (b2.x - b1.x) / 2 + b1.x;
	center.y = (b2.y - b1.y) / 2 + b1.y;

	snap_to_grid (sheet->grid, &center.x, &center.y);

	for (list = item_data_list; list; list = list->next) {
		ItemData *item_data = list->data;

		if (sheet->state == SHEET_STATE_NONE)
			item_data_unregister (item_data);

		item_data_rotate (item_data, 90, &center);

		if (sheet->state == SHEET_STATE_NONE)
			item_data_register (item_data);
	}

	g_list_free (item_data_list);
	g_list_free_full (list, g_object_unref);
}

void
sheet_delete_selection (Sheet *sheet)
{
	GList *list, *copy;

	g_return_if_fail (sheet != NULL);
	g_return_if_fail (IS_SHEET (sheet));

	if (sheet->state != SHEET_STATE_NONE)
		return;

	copy = g_list_copy (sheet->priv->selected_objects);
	for (list = copy; list; list = list->next) {
		sheet_remove_item_in_sheet (SHEET_ITEM (list->data), sheet);
		goo_canvas_item_remove (GOO_CANVAS_ITEM (list->data));
	}

	g_list_free (sheet->priv->selected_objects);
	sheet->priv->selected_objects = NULL;
	
	g_list_free (copy);
	g_list_free_full (list, g_object_unref);
}

void
sheet_release_selected_objects (Sheet *sheet)
{
	GList *list, *copy;
	GooCanvasGroup *group;
	gint   item_nbr;
	
	g_return_if_fail (sheet != NULL);
	g_return_if_fail (IS_SHEET (sheet));

	group = sheet->priv->selected_group;
	copy = g_list_copy (sheet->priv->selected_objects);
	
	// Remove all the selected_objects into selected_group
	for (list = copy; list; list = list->next) {
		item_nbr = goo_canvas_item_find_child (GOO_CANVAS_ITEM (group),
		                                      GOO_CANVAS_ITEM (list->data));
		goo_canvas_item_remove_child (GOO_CANVAS_ITEM (group),
		                              item_nbr);
	}
	g_list_free (copy);
		
	g_list_free (sheet->priv->selected_objects);
	g_list_free_full (list, g_object_unref);
	sheet->priv->selected_objects = NULL;
}

GList *
sheet_get_selection (Sheet *sheet)
{
	g_return_val_if_fail (sheet != NULL, NULL);
	g_return_val_if_fail (IS_SHEET (sheet), NULL);

	return sheet->priv->selected_objects;
}

void
sheet_update_parts (Sheet *sheet)
{
	GList *list;

	g_return_if_fail (sheet != NULL);
	g_return_if_fail (IS_SHEET (sheet));

	for (list = sheet->priv->items; list; list = list->next) {
		if (IS_PART_ITEM (list->data))
			part_item_update_node_label (PART_ITEM (list->data));
	}
	g_list_free_full (list, g_object_unref);
}

void
sheet_flip_selection (Sheet *sheet, gboolean horizontal)
{
	g_return_if_fail (sheet != NULL);
	g_return_if_fail (IS_SHEET (sheet));

	if (sheet->priv->selected_objects != NULL)
		flip_items (sheet, sheet->priv->selected_objects, horizontal);
}

void
sheet_flip_ghosts (Sheet *sheet, gboolean horizontal)
{
	g_return_if_fail (sheet != NULL);
	g_return_if_fail (IS_SHEET (sheet));

	if (sheet->priv->floating_objects != NULL)
		flip_items (sheet, sheet->priv->floating_objects, horizontal);
}

static void
flip_items (Sheet *sheet, GList *items, gboolean horizontal)
{
	GList *list, *item_data_list;
	SheetPos center, b1, b2;
	SheetPos after;

	item_data_list = NULL;
	for (list = items; list; list = list->next) {
		item_data_list = g_list_prepend (item_data_list,
					sheet_item_get_data (list->data));
	}

	item_data_list_get_absolute_bbox (item_data_list, &b1, &b2);

	center.x = (b2.x + b1.x) / 2;
	center.y = (b2.y + b1.y) / 2;

	for (list = item_data_list; list; list = list->next) {
		ItemData *item_data = list->data;

		if (sheet->state == SHEET_STATE_NONE)
			item_data_unregister (item_data);

		item_data_flip (item_data, horizontal, &center);

		// Make sure we snap to grid.
		item_data_get_pos (item_data, &after);

		snap_to_grid (sheet->grid, &after.x, &after.y);

		item_data_move (item_data, &after);

		if (sheet->state == SHEET_STATE_NONE)
			item_data_register (item_data);
	}

	g_list_free (item_data_list);
	g_list_free_full (list, g_object_unref);
}

void
sheet_clear_op_values (Sheet *sheet)
{
	GList *list;

	g_return_if_fail (sheet != NULL);
	g_return_if_fail (IS_SHEET (sheet));

	for (list = sheet->priv->voltmeter_items; list; list = list->next) {
		g_object_unref (G_OBJECT (list->data));
	}

	g_list_free (sheet->priv->voltmeter_items);
	sheet->priv->voltmeter_items = NULL;

	g_hash_table_destroy (sheet->priv->voltmeter_nodes);
	sheet->priv->voltmeter_nodes = g_hash_table_new_full (g_str_hash,
			g_str_equal, g_free, NULL);
	g_list_free_full (list, g_object_unref);
}

void
sheet_provide_object_properties (Sheet *sheet)
{
	g_return_if_fail (sheet != NULL);
	g_return_if_fail (IS_SHEET (sheet));

	if (g_list_length (sheet->priv->selected_objects) == 1)
		sheet_item_edit_properties (sheet->priv->selected_objects->data);
}

void
sheet_clear_ghosts (Sheet *sheet)
{
	GList *copy, *list;
	
	g_return_if_fail (sheet != NULL);
	g_return_if_fail (IS_SHEET (sheet));

	if (sheet->priv->floating_objects == NULL) return;

	g_assert (sheet->state != SHEET_STATE_FLOAT);
	copy = g_list_copy (sheet->priv->floating_objects);

	for (list = copy; list; list = list->next) {
		g_object_force_floating (G_OBJECT (list->data));
		g_object_unref (G_OBJECT (list->data));
	}

	g_list_free (copy);
	
	sheet->priv->floating_objects = NULL;
	g_list_free_full (list, g_object_unref);
}

guint
sheet_get_selected_objects_length (Sheet *sheet)
{
	g_return_val_if_fail ((sheet != NULL), 0);
	g_return_val_if_fail (IS_SHEET (sheet), 0);

	return g_list_length (sheet->priv->selected_objects);
}

GList	*
sheet_get_floating_objects (Sheet *sheet)
{
	g_return_val_if_fail (sheet != NULL, NULL);
	g_return_val_if_fail (IS_SHEET (sheet), NULL);

	return g_list_copy (sheet->priv->floating_objects);
}

void
sheet_add_ghost_item (Sheet *sheet, ItemData *data)
{
	SheetItem *item;

	g_return_if_fail (sheet != NULL);
	g_return_if_fail (IS_SHEET (sheet));

	item = sheet_item_factory_create_sheet_item (sheet, data);

	g_object_set (G_OBJECT (item),
	              "visibility", GOO_CANVAS_ITEM_INVISIBLE,
	              NULL);

	sheet_prepend_floating_object (sheet, item);
}

void
sheet_stop_create_wire (Sheet *sheet)
{
	g_return_if_fail (sheet != NULL);
	g_return_if_fail (IS_SHEET (sheet));
	
	if (sheet->priv->create_wire_context) {
		create_wire_exit (sheet->priv->create_wire_context);
		sheet->priv->create_wire_context = NULL;
	}
}

void
sheet_initiate_create_wire (Sheet *sheet)
{
	g_return_if_fail (sheet != NULL);
	g_return_if_fail (IS_SHEET (sheet));

	sheet->priv->create_wire_context = create_wire_initiate (sheet);
}

static void
node_dot_added_callback (Schematic *schematic, SheetPos *pos, Sheet *sheet)
{
	NodeItem *node_item;

	SheetPos *key;

	g_return_if_fail (sheet != NULL);
	g_return_if_fail (IS_SHEET (sheet));

	node_item = g_hash_table_lookup (sheet->priv->node_dots, pos);
	if (node_item == NULL) {
		node_item = NODE_ITEM (g_object_new (TYPE_NODE_ITEM, NULL));	
		g_object_set (node_item, 
		              "parent", goo_canvas_get_root_item (GOO_CANVAS (sheet)),
		              "x", pos->x,
					  "y", pos->y,
					  NULL);
	}

	node_item_show_dot (node_item, TRUE);
	key = g_new0 (SheetPos, 1);
	key->x = pos->x;
	key->y = pos->y;

	g_hash_table_insert (sheet->priv->node_dots, key, node_item);
}

static void
node_dot_removed_callback (Schematic *schematic, SheetPos *pos, Sheet *sheet)
{
	GooCanvasItem *node_item;
	SheetPos * orig_key;
	gboolean found;

	g_return_if_fail (sheet != NULL);
	g_return_if_fail (IS_SHEET (sheet));

	found = g_hash_table_lookup_extended (
			sheet->priv->node_dots,
			pos,
			(gpointer) &orig_key,
			(gpointer) &node_item);

	if (found) {
		goo_canvas_item_remove (GOO_CANVAS_ITEM (node_item));
		g_hash_table_remove (sheet->priv->node_dots, pos);
	} 
	else
		g_warning ("No dot to remove!");
}

static guint
dot_hash (gconstpointer key)
{
	SheetPos *sp = (SheetPos *) key;
	int x, y;

	x = (int)rint (sp->x) % 256;
	y = (int)rint (sp->y) % 256;

	return (y << 8) | x;
}

#define HASH_EPSILON 1e-2

static int
dot_equal (gconstpointer a, gconstpointer b)
{
	SheetPos *spa, *spb;

	g_return_val_if_fail (a!=NULL, 0);
	g_return_val_if_fail (b!=NULL, 0);

	spa = (SheetPos *) a;
	spb = (SheetPos *) b;

	if (fabs (spa->y - spb->y) > HASH_EPSILON)
		return 0;

	if (fabs (spa->x - spb->x) > HASH_EPSILON)
		return 0;

	return 1;
}

void
sheet_connect_node_dots_to_signals (Sheet *sheet)
{
	g_return_if_fail (sheet != NULL);
	g_return_if_fail (IS_SHEET (sheet));

	GList *list;	
	Schematic *sm;

	sm = schematic_view_get_schematic_from_sheet (sheet);
	list = schematic_get_items (sm);

	g_signal_connect_object (G_OBJECT (sm), "node_dot_added",
			G_CALLBACK (node_dot_added_callback), G_OBJECT (sheet), 0);
	g_signal_connect_object (G_OBJECT (sm), "node_dot_removed",
			G_CALLBACK (node_dot_removed_callback), G_OBJECT (sheet), 0);

	list = node_store_get_node_positions (schematic_get_store (sm));
	for (; list; list = list->next)
		node_dot_added_callback (sm, list->data, sheet);

	g_list_free_full (list, g_object_unref);
}

void
sheet_remove_item_in_sheet (SheetItem *item, Sheet *sheet)
{
	g_return_if_fail (sheet != NULL);
    g_return_if_fail (IS_SHEET (sheet));
    g_return_if_fail (item != NULL);
    g_return_if_fail (IS_SHEET_ITEM (item));

    sheet->priv->items = g_list_remove (sheet->priv->items, item);

    //  Remove the object from the selected-list before destroying.
    sheet_remove_selected_object (sheet, item);
    sheet_remove_floating_object (sheet, item);

	// Destroy the item-data (model) associated to the sheet-item
	g_object_unref (sheet_item_get_data (item));
}
