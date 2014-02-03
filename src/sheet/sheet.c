/*
 * sheet.c
 *
 *
 * Authors:
 *  Richard Hult <rhult@hem.passagen.se>
 *  Ricardo Markiewicz <rmarkie@fi.uba.ar>
 *  Andres de Barbara <adebarbara@fi.uba.ar>
 *  Marc Lorber <lorber.marc@wanadoo.fr>
 *  Bernhard Schuster <schuster.bernhard@gmail.com>
 *
 * Web page: https://srctwig.com/oregano
 *
 * Copyright (C) 1999-2001  Richard Hult
 * Copyright (C) 2003,2006  Ricardo Markiewicz
 * Copyright (C) 2009-2012  Marc Lorber
 * Copyright (C) 2013       Bernhard Schuster
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

#include "speedy.h"
#include "sheet-private.h"
#include "sheet-item.h"
#include "node-store.h"
#include "node-item.h"
#include "wire-item.h"
#include "part-item.h"
#include "grid.h"
#include "sheet-item-factory.h"
#include "schematic-view.h"

#include "rubberband.h"
#include "create-wire.h"

static void 	sheet_class_init (SheetClass *klass);
static void 	sheet_init (Sheet *sheet);
static void 	sheet_set_property (GObject *object, guint prop_id,
					const GValue *value, GParamSpec *spec);
static void 	sheet_get_property (GObject *object, guint prop_id,
					GValue *value, GParamSpec *spec);
static void 	sheet_set_zoom (const Sheet *sheet, double zoom);
static GList *	sheet_preserve_selection (Sheet *sheet);
static void	rotate_items (Sheet *sheet, GList *items);
static void	flip_items (Sheet *sheet, GList *items, IDFlip direction);
static void 	node_dot_added_callback (Schematic *schematic, Coords *pos, Sheet *sheet);
static void 	node_dot_removed_callback (Schematic *schematic, Coords *pos, Sheet *sheet);
static void	sheet_finalize (GObject *object);
static int 	dot_equal (gconstpointer a, gconstpointer b);
static guint 	dot_hash (gconstpointer key);


#define ZOOM_MIN 0.35
#define ZOOM_MAX 3

#include "debug.h"

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


static void
sheet_init (Sheet *sheet)
{
	sheet->priv = g_new0 (SheetPriv, 1);
	sheet->priv->zoom = 1.0;
	sheet->priv->selected_group = NULL;
	sheet->priv->floating_group = NULL;
	sheet->priv->floating_objects = NULL;
	sheet->priv->selected_objects = NULL;
	sheet->priv->wire_handler_id = 0;
	sheet->priv->float_handler_id = 0;

	sheet->priv->items = NULL;
	sheet->priv->rubberband_info = NULL;
	sheet->priv->create_wire_info = NULL;
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
		if (sheet->priv->node_dots)
			g_hash_table_destroy (sheet->priv->node_dots);
		g_free (sheet->priv);
	}
	if (G_OBJECT_CLASS (sheet_parent_class)->finalize)
		(* G_OBJECT_CLASS (sheet_parent_class)->finalize) (object);
}


/*
 * position within the sheet in pixel coordinates
 * coordinates are clamped to grid if grid is enabled
 * see snap_to_grid
 * zero point : top left corner of the window (not widget!)
 * x : horizontal, left to right
 * y : vertical, top to bottom
 * returns wether the position could be detected properly
 */
gboolean
sheet_get_pointer_pixel (Sheet *sheet, gdouble *x, gdouble *y)
{
	GtkAdjustment *hadj, *vadj;
	gdouble x1, y1;
	gint _x, _y;
	GdkDeviceManager *device_manager;
	GdkDevice *device_pointer;
	GdkRectangle allocation;
	GdkDisplay *display;
	GdkWindow *window;

	// deprecated gtk_widget_get_pointer (GTK_WIDGET (sheet), &_x, &_y);
	// replaced by a code copied from evince

	if (G_UNLIKELY (!sheet || !gtk_widget_get_realized (GTK_WIDGET (sheet)))) {
		NG_DEBUG ("Widget is not realized.");
		return FALSE;
	}

	display = gtk_widget_get_display (GTK_WIDGET (sheet));
	device_manager = gdk_display_get_device_manager (display);
	// gdk_device_manager_get_client_pointer
	// shall not be used within events
	device_pointer = gdk_device_manager_get_client_pointer (device_manager);
	window = gtk_widget_get_window (GTK_WIDGET (sheet));

	if (!window) {
		NG_DEBUG ("Window is not realized.");
		return FALSE;
	}
	// even though above is all defined the below will always return NUL for
	// unknown reason and _x and _y are populated as expected
	gdk_window_get_device_position (
	                window,
					device_pointer,
					&_x, &_y, NULL);
#if 0
	if (!window) { //fails always
		NG_DEBUG ("Window does not seem to be realized yet?");
		return FALSE;
	}
#else
	NG_DEBUG ("\n%p %p %p %p %i %i\n\n",display, device_manager, device_pointer, window, _x, _y);
#endif


	gtk_widget_get_allocation (GTK_WIDGET (sheet), &allocation);

	_x -= allocation.x;
	_y -= allocation.y;

	x1 = (gdouble) _x;
	y1 = (gdouble) _y;

	if (!sheet_get_adjustments (sheet, &hadj, &vadj))
	      return FALSE;

	x1 += gtk_adjustment_get_value (hadj);
	y1 += gtk_adjustment_get_value (vadj);

	*x = x1;
	*y = y1;
	return TRUE;
}

gboolean
sheet_get_pointer (Sheet *sheet, gdouble *x, gdouble *y)
{
	if (!sheet_get_pointer_pixel (sheet, x, y))
		return FALSE;
	goo_canvas_convert_from_pixels (GOO_CANVAS (sheet), x, y);
	return TRUE;
}

gboolean
sheet_get_pointer_snapped (Sheet *sheet, gdouble *x, gdouble *y)
{
	if (!sheet_get_pointer_pixel (sheet, x, y))
		return FALSE;
	goo_canvas_convert_from_pixels (GOO_CANVAS (sheet), x, y);
	snap_to_grid (sheet->grid, x, y);
	return TRUE;
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


/*
 * gets the sheets parent adjustments
 * returns TRUE on success
 */
gboolean
sheet_get_adjustments (const Sheet *sheet, GtkAdjustment **hadj, GtkAdjustment **vadj)
{
	GtkWidget *parent;
	GtkScrolledWindow *scrolled;

	if (__unlikely (!sheet))
		return FALSE;
	if (__unlikely (!vadj || !hadj))
		return FALSE;

	parent = gtk_widget_get_parent (GTK_WIDGET (sheet));
	if (__unlikely (!parent || !GTK_IS_SCROLLED_WINDOW (parent)))
		return FALSE;
	scrolled = GTK_SCROLLED_WINDOW (parent);

	*hadj = gtk_scrolled_window_get_hadjustment (scrolled);
	if (__unlikely (!*hadj || !GTK_IS_ADJUSTMENT (*hadj)))
		return FALSE;

	*vadj = gtk_scrolled_window_get_vadjustment (scrolled);
	if (__unlikely (!*vadj || !GTK_IS_ADJUSTMENT (*vadj)))
		return FALSE;

	return TRUE;
}


/*
 * change the zoom by factor <rate>
 * zoom origin when zooming in is the cursor
 * zoom origin when zooming out is the center of the current viewport
 * sane <rate> values are in range of [0.5 .. 2]
 */
void
sheet_change_zoom (Sheet *sheet, gdouble rate)
{
	g_return_if_fail (sheet);
	g_return_if_fail (IS_SHEET (sheet));
//////////////////////////////////////////////7

	gdouble x, y;
	gdouble rx, ry;
	gdouble px, py;
	gdouble dx, dy;
	gdouble cx, cy;
	gdouble dcx, dcy;
	GtkAdjustment *hadj, *vadj;
	GooCanvas *canvas;

	canvas = GOO_CANVAS (sheet);

	// if we scroll out, just scroll to the center
	if (rate < 1.) {
		goo_canvas_set_scale (canvas, rate * goo_canvas_get_scale (canvas));
		return;
	}

	// top left corner in pixels
	if (sheet_get_adjustments (sheet, &hadj, &vadj)) {
		x = gtk_adjustment_get_value (hadj);
		y = gtk_adjustment_get_value (vadj);
	} else {
		x = y = 0.;
	}

	// get pointer position in pixels
	sheet_get_pointer_pixel (sheet, &px, &py);

	// get the page size in pixels
	dx = gtk_adjustment_get_page_size (hadj);
	dy = gtk_adjustment_get_page_size (vadj);
	// calculate the center of the widget in pixels
	cx = x + dx/2;
	cy = y + dy/2;

	// calculate the delta between the center and the pointer in pixels
	// this is required as the center is the zoom target
	dcx = px - cx;
	dcy = py - cy;

	// increase the top left position in pixels by our calculated delta
	x += dcx;
	y += dcy;

	//convert to canvas coords
	goo_canvas_convert_from_pixels (canvas, &x, &y);

	//the center of the canvas is now our cursor position
	goo_canvas_scroll_to (canvas, x, y);

	//calculate a correction term
	//for the case that we can not scroll the pane far enough to
	//compensate the whole off-due-to-wrong-center-error
	rx = gtk_adjustment_get_value (hadj);
	ry = gtk_adjustment_get_value (vadj);
	goo_canvas_convert_from_pixels (canvas, &rx, &ry);
	//the correction term in goo coordinates, to be subtracted from the backscroll distance
	rx -= x;
	ry -= y;

	// no the center is our cursor position and we can safely call scale
	goo_canvas_set_scale (canvas, rate * goo_canvas_get_scale (canvas));

	// top left corner in pixels after scaling
	if (sheet_get_adjustments (sheet, &hadj, &vadj)) {
		x = gtk_adjustment_get_value (hadj);
		y = gtk_adjustment_get_value (vadj);
	} else {
		x = y = 0.;
	}
	// not sure if the below part is required, could be zer0
	NG_DEBUG ("rx %lf\n", rx);
	NG_DEBUG ("ry %lf\n", ry);
	NG_DEBUG ("dcx %lf\n", dcx);
	NG_DEBUG ("dcy %lf\n", dcy);
	NG_DEBUG ("\n\n");
	// gtk_adjustment_get_page_size is constant
	x -= (dcx) / sheet->priv->zoom;
	y -= (dcy) / sheet->priv->zoom;
	goo_canvas_convert_from_pixels (canvas, &x, &y);

	goo_canvas_scroll_to (canvas, x-rx, y-ry);

	gtk_widget_queue_draw (GTK_WIDGET (canvas));
}


/* This function defines the drawing sheet on which schematic will be drawn
 * @param grid the grid (not a GtkGrid but a Grid struct)
 */
GtkWidget *
sheet_new (Grid *grid)
{
	g_return_val_if_fail (grid, NULL);
	g_return_val_if_fail (IS_GRID (grid), NULL);

	GooCanvas *sheet_canvas;
	GooCanvasGroup *sheet_group;
	GooCanvasPoints *points;
	Sheet *sheet;
	GtkWidget *sheet_widget;
	GooCanvasItem *root;

	gdouble height=-1., width=-1.;
	g_object_get (grid,
	              "height", &height,
	              "width", &width,
	              NULL);
	g_printf ("%s xxx %lf x %lf\n", __FUNCTION__, height, width);

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

	goo_canvas_set_bounds (GOO_CANVAS (sheet_canvas),
	                       0., 0.,
	                       width + 20., height + 20.);

	// Define vicinity around GooCanvasItem
	//sheet_canvas->close_enough = 6.0;

	sheet->priv->width = width;
	sheet->priv->height = height;

	// Create the dot grid.
	sheet->grid = grid;
	sheet->grid_item = grid_item_new (GOO_CANVAS_ITEM (sheet_group), sheet->grid);

	// Everything outside the sheet should be gray.
	// top //
	goo_canvas_rect_new (GOO_CANVAS_ITEM (sheet_group),
	                     0.0,
	                     0.0,
	                     width + 20.0,
	                     20.0,
	                     "fill_color", "gray",
	                     "line-width", 0.0,
	                     NULL);

	goo_canvas_rect_new (GOO_CANVAS_ITEM (sheet_group),
	                     0.0,
	                     height,
	                     width + 20.0,
	                     height + 20.0,
	                     "fill_color", "gray",
	                     "line-width", 0.0,
	                     NULL);

	// right //
	goo_canvas_rect_new (GOO_CANVAS_ITEM (sheet_group),
	                     0.0,
	                     0.0,
	                     20.0,
	                     height + 20.0,
	                     "fill_color", "gray",
	                     "line-width", 0.0,
	                     NULL);

	goo_canvas_rect_new (GOO_CANVAS_ITEM (sheet_group),
	                     width,
	                     0.0,
	                     width + 20.0,
	                     height + 20.0,
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
	NG_DEBUG ("root group %p", sheet->object_group);

	sheet->priv->selected_group = GOO_CANVAS_GROUP (goo_canvas_group_new (
	     GOO_CANVAS_ITEM (sheet->object_group),
	     "x", 0.0,
	     "y", 0.0,
	     NULL));
	NG_DEBUG ("selected group %p", sheet->priv->selected_group);

	sheet->priv->floating_group = GOO_CANVAS_GROUP (goo_canvas_group_new (
	     GOO_CANVAS_ITEM (sheet->object_group),
	     "x", 0.0,
	     "y", 0.0,
	     NULL));
	NG_DEBUG ("floating group %p", sheet->priv->floating_group);

	// Hash table that maps coordinates to a specific dot.
	sheet->priv->node_dots = g_hash_table_new_full (dot_hash, dot_equal, g_free, NULL);

	//this requires object_group to be setup properly
	sheet->priv->rubberband_info = rubberband_info_new (sheet);
	sheet->priv->create_wire_info = create_wire_info_new (sheet);

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

/*
 * scroll to <dx,dy> in pixels relative to the current coords
 * note that pixels are _not_ affected by zoom
 */
void
sheet_scroll_pixel (const Sheet *sheet, int delta_x, int delta_y)
{
	GtkAdjustment *hadj, *vadj;
	GtkAllocation allocation;
	gfloat vnew, hnew;
	gfloat hmax, vmax;
	gfloat x1, y1;
	const SheetPriv *priv = sheet->priv;

	if (sheet_get_adjustments (sheet, &hadj, &vadj)) {
		x1 = gtk_adjustment_get_value (hadj);
		y1 = gtk_adjustment_get_value (vadj);
	} else {
		x1 = y1 = 0.f;
	}

	gtk_widget_get_allocation (GTK_WIDGET (sheet), &allocation);

	if (priv->width > allocation.width)
		hmax = (gfloat) (priv->width - allocation.width);
	else
		hmax = 0.f;

	if (priv->height > allocation.height)
		vmax = (gfloat) (priv->height -  allocation.height);
	else
		vmax = 0.f;

	hnew = CLAMP (x1 + (gfloat) delta_x, 0.f, hmax);
	vnew = CLAMP (y1 + (gfloat) delta_y, 0.f, vmax);

	if (hnew != x1) {
		gtk_adjustment_set_value (hadj, hnew);
		g_signal_emit_by_name (G_OBJECT (hadj), "value_changed");
	}
	if (vnew != y1) {
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
	g_return_if_fail (sheet);
	g_return_if_fail (IS_SHEET (sheet));
	g_return_if_fail (sv);
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

	GList *item = NULL;

	for (item = sheet->priv->items; item; item=item->next) {
		if (IS_PART_ITEM (item->data))
			if (part_get_num_pins (PART (sheet_item_get_data (SHEET_ITEM(item->data))))==1)
				part_item_show_node_labels (PART_ITEM (item->data), show);
	}
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

/**
 * save the selection
 * @attention not stackable
 */
GList *
sheet_preserve_selection (Sheet *sheet)
{
	g_return_val_if_fail (sheet != NULL, FALSE);
	g_return_val_if_fail (IS_SHEET (sheet), FALSE);

	GList *list = NULL;
	for (list = sheet->priv->selected_objects; list; list = list->next) {
		sheet_item_set_preserve_selection (SHEET_ITEM (list->data), TRUE);
	}
	// Return the list so that we can remove the preserve_selection
	// flags later.
	return sheet->priv->selected_objects;
}


int
sheet_event_callback (GtkWidget *widget, GdkEvent *event, Sheet *sheet)
{
	GtkWidgetClass *wklass = GTK_WIDGET_CLASS (sheet->priv->sheet_parent_class);
	switch (event->type) {
		case GDK_3BUTTON_PRESS:
			// We do not care about triple clicks on the sheet.
			return FALSE;
		case GDK_2BUTTON_PRESS:
			// The sheet does not care about double clicks, but invoke the
		 	// canvas event handler and see if an item picks up the event.
			return wklass->button_press_event(widget, (GdkEventButton *) event);
		case GDK_BUTTON_PRESS:
			// If we are in the middle of something else, don't interfere
		 	// with that.
			if (sheet->state != SHEET_STATE_NONE) {
				return FALSE;
			}

			if (wklass->button_press_event(widget, (GdkEventButton *) event)) {
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

				rubberband_start (sheet, event);
				return TRUE;
			}
			break;
		case GDK_BUTTON_RELEASE:
			if (event->button.button == 4 || event->button.button == 5)
				return TRUE;

			if (event->button.button == 1 && sheet->priv->rubberband_info->state == RUBBERBAND_ACTIVE) {
				rubberband_finish (sheet, event);
				return TRUE;
			}

			if (wklass->button_release_event != NULL) {
				return wklass->button_release_event (widget, (GdkEventButton *) event);
			}

			break;

		case GDK_SCROLL:
			{
				GdkEventScroll *scr_event = (GdkEventScroll*) event;
				if (scr_event->state & GDK_SHIFT_MASK) {
					// Scroll horizontally
					if (scr_event->direction == GDK_SCROLL_UP)
						sheet_scroll_pixel (sheet, -30, 0);
					else if (scr_event->direction == GDK_SCROLL_DOWN)
						sheet_scroll_pixel (sheet, 30, 0);

				} else if (scr_event->state & GDK_CONTROL_MASK) {
					// Scroll vertically
					if (scr_event->direction == GDK_SCROLL_UP)
						sheet_scroll_pixel (sheet, 0,-30);
					else if (scr_event->direction == GDK_SCROLL_DOWN)
						sheet_scroll_pixel (sheet, 0, 30);

				} else {
					// Zoom
					if (scr_event->direction == GDK_SCROLL_UP) {
						double zoom;
						sheet_get_zoom (sheet, &zoom);
						if (zoom < ZOOM_MAX)
							sheet_change_zoom (sheet, 1.1);
					}
					else if (scr_event->direction == GDK_SCROLL_DOWN) {
						double zoom;
						sheet_get_zoom (sheet, &zoom);
						if (zoom > ZOOM_MIN)
							sheet_change_zoom (sheet, 0.9);
					}
				}
			}
			break;
		case GDK_MOTION_NOTIFY:
			if (sheet->priv->rubberband_info->state == RUBBERBAND_ACTIVE) {
				rubberband_update (sheet, event);
				return TRUE;
			}
			if (wklass->motion_notify_event != NULL) {
				return wklass->motion_notify_event (widget, (GdkEventMotion *) event);
			}
		case GDK_ENTER_NOTIFY:
			wklass->enter_notify_event (widget, (GdkEventCrossing *) event);
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
						sheet_scroll_pixel (sheet, -20, 0);
					break;
				case GDK_KEY_Up:
					if (event->key.state & GDK_MOD1_MASK)
						sheet_scroll_pixel (sheet, 0, -20);
					break;
				case GDK_KEY_Right:
					if (event->key.state & GDK_MOD1_MASK)
						sheet_scroll_pixel (sheet, 20, 0);
					break;
				case GDK_KEY_Down:
					if (event->key.state & GDK_MOD1_MASK)
						sheet_scroll_pixel (sheet, 0, 20);
					break;
				case GDK_KEY_Page_Up:
					if (event->key.state & GDK_MOD1_MASK)
						sheet_scroll_pixel (sheet, 0, -120);
					break;
				case GDK_KEY_Page_Down:
					if (event->key.state & GDK_MOD1_MASK)
						sheet_scroll_pixel (sheet, 0, 120);
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
}

void
sheet_rotate_selection (Sheet *sheet)
{
	g_return_if_fail (sheet != NULL);
	g_return_if_fail (IS_SHEET (sheet));

	if (sheet->priv->selected_objects != NULL)
		rotate_items (sheet, sheet->priv->selected_objects);
}

/**
 * rotate floating items
 */
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
	Coords center, b1, b2;

	item_data_list = NULL;
	for (list = items; list; list = list->next) {
		item_data_list = g_list_prepend (item_data_list,
			sheet_item_get_data (list->data));
	}

	item_data_list_get_absolute_bbox (item_data_list, &b1, &b2);

	center = coords_average(&b1, &b2);

	snap_to_grid (sheet->grid, &center.x, &center.y);

	for (list = item_data_list; list; list = list->next) {
		ItemData *item_data = list->data;
		if (item_data==NULL)
			continue;

		if (sheet->state == SHEET_STATE_NONE)
			item_data_unregister (item_data);

		item_data_rotate (item_data, 90, &center);

		if (sheet->state == SHEET_STATE_NONE)
			item_data_register (item_data);
	}

	g_list_free (item_data_list);
}


/**
 * remove the currently selected items from the sheet
 * (especially their goocanvas representators)
 */
void
sheet_delete_selection (Sheet *sheet)
{
	GList *copy, *iter;

	g_return_if_fail (sheet != NULL);
	g_return_if_fail (IS_SHEET (sheet));

	if (sheet->state != SHEET_STATE_NONE)
		return;

	copy = g_list_copy (sheet->priv->selected_objects);

	for (iter = copy; iter; iter = iter->next) {
		sheet_remove_item_in_sheet (SHEET_ITEM (iter->data), sheet);
		goo_canvas_item_remove (GOO_CANVAS_ITEM (iter->data));
	}
	g_list_free (copy);

	// we need to it like this as <sheet_remove_item_in_sheet>
	// requires selected_objects, items, floating_objects
	// to be not NULL!
	g_list_free (sheet->priv->selected_objects);
	sheet->priv->selected_objects = NULL;
}


/**
 * removes all canvas items in the selected canvas group from their parents
 * but does NOT delete them
 */
void
sheet_release_selected_objects (Sheet *sheet)
{
	GList *list, *copy;
	GooCanvasGroup *group;
	gint item_nbr;

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
	sheet->priv->selected_objects = NULL;
}

/**
 * @returns [transfer-none] the list of selected objects
 */
GList *
sheet_get_selection (Sheet *sheet)
{
	g_return_val_if_fail (sheet != NULL, NULL);
	g_return_val_if_fail (IS_SHEET (sheet), NULL);

	return sheet->priv->selected_objects;
}

/**
 * update the node lables of all parts in the current sheet
 */
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
}

/**
 * flip currently selected items
 */
void
sheet_flip_selection (Sheet *sheet, IDFlip direction)
{
	g_return_if_fail (sheet != NULL);
	g_return_if_fail (IS_SHEET (sheet));

	if (sheet->priv->selected_objects != NULL)
		flip_items (sheet, sheet->priv->selected_objects, direction);
}

/**
 * flip currently floating items
 */
void
sheet_flip_ghosts (Sheet *sheet, IDFlip direction)
{
	g_return_if_fail (sheet != NULL);
	g_return_if_fail (IS_SHEET (sheet));

	if (sheet->priv->floating_objects != NULL)
		flip_items (sheet, sheet->priv->floating_objects, direction);
}

static void
flip_items (Sheet *sheet, GList *items, IDFlip direction)
{
	GList *iter, *item_data_list;
	Coords center, b1, b2;
	Coords after;

	item_data_list = NULL;
	for (iter = items; iter; iter = iter->next) {
		item_data_list = g_list_prepend (item_data_list,
					sheet_item_get_data (iter->data));
	}

	item_data_list_get_absolute_bbox (item_data_list, &b1, &b2);

	// FIXME center is currently not used by item_data_flip (part.c implentation)
	center.x = (b2.x + b1.x) / 2;
	center.y = (b2.y + b1.y) / 2;

	// FIXME - registering an item after flipping it still creates an offset as the position is still 0
	for (iter = item_data_list; iter; iter = iter->next) {
		ItemData *item_data = iter->data;

		if (sheet->state == SHEET_STATE_NONE)
			item_data_unregister (item_data);

		item_data_flip (item_data, direction, &center);

		// Make sure we snap to grid.
		item_data_get_pos (item_data, &after);

		snap_to_grid (sheet->grid, &after.x, &after.y);

		item_data_set_pos (item_data, &after);

		if (sheet->state == SHEET_STATE_NONE)
			item_data_register (item_data);
	}

	g_list_free (item_data_list);
}

void
sheet_clear_op_values (Sheet *sheet)
{
	GList *iter;

	g_return_if_fail (sheet != NULL);
	g_return_if_fail (IS_SHEET (sheet));

	for (iter = sheet->priv->voltmeter_items; iter; iter = iter->next) {
		g_object_unref (G_OBJECT (iter->data));
	}

	g_list_free (sheet->priv->voltmeter_items);
	sheet->priv->voltmeter_items = NULL;

	g_hash_table_destroy (sheet->priv->voltmeter_nodes);
	sheet->priv->voltmeter_nodes = g_hash_table_new_full (g_str_hash,
			g_str_equal, g_free, NULL);
}

void
sheet_provide_object_properties (Sheet *sheet)
{
	g_return_if_fail (sheet != NULL);
	g_return_if_fail (IS_SHEET (sheet));

	if (g_list_length (sheet->priv->selected_objects) == 1)
		sheet_item_edit_properties (sheet->priv->selected_objects->data);
}

/**
 * get rid of floating items (delete them)
 */
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
}

/**
 * count selected objects O(n)
 */
guint
sheet_get_selected_objects_length (Sheet *sheet)
{
	g_return_val_if_fail ((sheet != NULL), 0);
	g_return_val_if_fail (IS_SHEET (sheet), 0);

	return g_list_length (sheet->priv->selected_objects);
}

/**
 * @returns a shallow copy of the floating items list, free with `g_list_free`
 */
GList	*
sheet_get_floating_objects (Sheet *sheet)
{
	g_return_val_if_fail (sheet != NULL, NULL);
	g_return_val_if_fail (IS_SHEET (sheet), NULL);

	return g_list_copy (sheet->priv->floating_objects);
}

/**
 * add a recently created item (no view representation yet!)
 * to the group of floating items and add a SheetItem via
 * `sheet_item_factory_create_sheet_item()`
 */
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

	create_wire_cleanup (sheet);
}

void
sheet_initiate_create_wire (Sheet *sheet)
{
	g_return_if_fail (sheet != NULL);
	g_return_if_fail (IS_SHEET (sheet));

	create_wire_setup (sheet);
}

static void
node_dot_added_callback (Schematic *schematic, Coords *pos, Sheet *sheet)
{
	NodeItem *node_item;
	Coords *key;

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
	key = g_new0 (Coords, 1);
	key->x = pos->x;
	key->y = pos->y;

	g_hash_table_insert (sheet->priv->node_dots, key, node_item);
}

static void
node_dot_removed_callback (Schematic *schematic, Coords *pos, Sheet *sheet)
{
	GooCanvasItem *node_item;
	Coords * orig_key;
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

/**
 * hash function for dots (for their position actually)
 * good enough to get some spread and very lightweight
 */
static guint
dot_hash (gconstpointer key)
{
	Coords *sp = (Coords *) key;
	int x, y;

	x = (int)rint (sp->x) % 256;
	y = (int)rint (sp->y) % 256;

	return (y << 8) | x;
}

#define HASH_EPSILON 1e-2

static int
dot_equal (gconstpointer a, gconstpointer b)
{
	Coords *spa, *spb;

	g_return_val_if_fail (a!=NULL, 0);
	g_return_val_if_fail (b!=NULL, 0);

	spa = (Coords *) a;
	spb = (Coords *) b;

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

}

/**
 * remove a single item from the sheet
 */
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

	ItemData *data = sheet_item_get_data (item);
	g_return_if_fail (data != NULL);
	g_return_if_fail (IS_ITEM_DATA (data));

	// properly unregister the itemdata from the sheet
	item_data_unregister (data);
	// Destroy the item-data (model) associated to the sheet-item
	g_object_unref (data);
}


inline static guint32
extract_time (GdkEvent *event)
{
	if (event) {
		switch (event->type) {
		/* only added relevant events */
		case GDK_MOTION_NOTIFY:
			return ((GdkEventMotion *)event)->time;
		case GDK_3BUTTON_PRESS:
		case GDK_2BUTTON_PRESS:
		case GDK_BUTTON_RELEASE:
		case GDK_BUTTON_PRESS:
			return ((GdkEventButton *)event)->time;
		case GDK_KEY_PRESS:
			return ((GdkEventKey *)event)->time;
		case GDK_ENTER_NOTIFY:
		case GDK_LEAVE_NOTIFY:
			return ((GdkEventCrossing *)event)->time;
		case GDK_PROPERTY_NOTIFY:
			return ((GdkEventProperty *)event)->time;
		case GDK_DRAG_ENTER:
		case GDK_DRAG_LEAVE:
		case GDK_DRAG_MOTION:
		case GDK_DRAG_STATUS:
		case GDK_DROP_START:
		case GDK_DROP_FINISHED:
			return ((GdkEventDND *)event)->time;
		default:
			return 0;
		}
	}
	return 0;
}

/**
 * helpful for debugging to not grab all input forever
 * if oregano segfaults while running inside
 * a gdb session
 */
//#define DEBUG_DISABLE_GRABBING

gboolean
sheet_pointer_grab (Sheet *sheet, GdkEvent *event)
{
	g_return_val_if_fail (sheet, FALSE);
	g_return_val_if_fail (IS_SHEET (sheet), FALSE);
#ifndef DEBUG_DISABLE_GRABBING
	if (sheet->priv->pointer_grabbed==0 &&
	    goo_canvas_pointer_grab (GOO_CANVAS (sheet),
	                             GOO_CANVAS_ITEM (sheet->grid),
	                             GDK_POINTER_MOTION_MASK | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK,
	                         NULL,
	                         extract_time (event))==GDK_GRAB_SUCCESS) {
		sheet->priv->pointer_grabbed = 1;
	}
	return (sheet->priv->pointer_grabbed == 1);
#else
	return TRUE;
#endif
}


void
sheet_pointer_ungrab (Sheet *sheet, GdkEvent *event)
{
	g_return_if_fail (sheet);
	g_return_if_fail (IS_SHEET (sheet));
#ifndef DEBUG_DISABLE_GRABBING
	if (sheet->priv->pointer_grabbed) {
		sheet->priv->pointer_grabbed = 0;
		goo_canvas_pointer_ungrab (GOO_CANVAS (sheet),
		                           GOO_CANVAS_ITEM (sheet->grid),
		                           extract_time (event));
	}
#endif
}


gboolean
sheet_keyboard_grab (Sheet *sheet, GdkEvent *event)
{
	g_return_val_if_fail (sheet, FALSE);
	g_return_val_if_fail (IS_SHEET (sheet), FALSE);
#ifndef DEBUG_DISABLE_GRABBING
	if (sheet->priv->keyboard_grabbed==0 &&
	    goo_canvas_keyboard_grab (GOO_CANVAS (sheet),
		                          GOO_CANVAS_ITEM (sheet->grid),
	                              TRUE, /*do not reroute signals through sheet->grid*/
		                      extract_time (event))==GDK_GRAB_SUCCESS) {
		sheet->priv->keyboard_grabbed = 1;
	}
	return (sheet->priv->keyboard_grabbed == 1);
#else
	return TRUE;
#endif
}


void
sheet_keyboard_ungrab (Sheet *sheet, GdkEvent *event)
{
	g_return_if_fail (sheet);
	g_return_if_fail (IS_SHEET (sheet));
#ifndef DEBUG_DISABLE_GRABBING
	if (sheet->priv->keyboard_grabbed) {
		sheet->priv->keyboard_grabbed = 0;
		goo_canvas_keyboard_ungrab (GOO_CANVAS (sheet),
		                            GOO_CANVAS_ITEM (sheet->grid),
		                            extract_time (event));
	}
#endif
}
