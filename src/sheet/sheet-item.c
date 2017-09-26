/*
 * sheet-item.c
 *
 *
 * Authors:
 *  Richard Hult <rhult@hem.passagen.se>
 *  Ricardo Markiewicz <rmarkie@fi.uba.ar>
 *  Andres de Barbara <adebarbara@fi.uba.ar>
 *  Marc Lorber <lorber.marc@wanadoo.fr>
 *  Bernhard Schuster <bernhard@ahoi.io>
 *  Guido Trentalancia <guido@trentalancia.com>
 *
 * Web page: https://ahoi.io/project/oregano
 *
 * Copyright (C) 1999-2001  Richard Hult
 * Copyright (C) 2003,2006  Ricardo Markiewicz
 * Copyright (C) 2009-2012  Marc Lorber
 * Copyright (C) 2013-2014  Bernhard Schuster
 * Copyright (C) 2017       Guido Trentalancia
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
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

// FIXME
#define FIXME_INCREMENTAL_MOVMENT_DOES_NOT_WORK 1

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <math.h>

#include "oregano.h"
#include "sheet-private.h"
#include "sheet-item.h"
#include "stock.h"
#include "clipboard.h"
#include "options.h"

static void sheet_item_class_init (SheetItemClass *klass);
static void sheet_item_init (SheetItem *item);
static void sheet_item_set_property (GObject *object, guint prop_id, const GValue *value,
                                     GParamSpec *spec);
static void sheet_item_get_property (GObject *object, guint prop_id, GValue *value,
                                     GParamSpec *spec);
static void sheet_item_finalize (GObject *object);
static void sheet_item_run_menu (SheetItem *item, Sheet *sheet, GdkEventButton *event);
static void sheet_item_reparent (SheetItem *item, GooCanvasGroup *group);
static void sheet_item_dispose (GObject *object);

#include "debug.h"

struct _SheetItemPriv
{
	guint selected : 1;
	guint preserve_selection : 1;
	ItemData *data;
	GtkActionGroup *action_group;
	GtkUIManager *ui_manager;
};

enum {
	ARG_0,
	// GOOGANVASGROUP properties
	ARG_X,
	ARG_Y,
	ARG_WIDTH,
	ARG_HEIGHT,
	// Sheet item properties
	ARG_DATA,
	ARG_SHEET,
	ARG_ACTION_GROUP
};

enum { MOVED, PLACED, SELECTION_CHANGED, MOUSE_OVER, DOUBLE_CLICKED, LAST_SIGNAL };

static guint so_signals[LAST_SIGNAL] = {0};

// This is the upper part of the object popup menu. It contains actions
// that are the same for all objects, e.g. parts and wires.
static const char *sheet_item_context_menu = "<ui>"
                                             "  <popup name='ItemMenu'>"
                                             "    <menuitem action='Copy'/>"
                                             "    <menuitem action='Cut'/>"
                                             "    <menuitem action='Delete'/>"
                                             "    <separator/>"
                                             "    <menuitem action='Rotate'/>"
                                             "    <menuitem action='FlipH'/>"
                                             "    <menuitem action='FlipV'/>"
                                             "    <separator/>"
                                             "  </popup>"
                                             "</ui>";

static GtkActionEntry action_entries[] = {
    {"Copy", GTK_STOCK_COPY, N_ ("_Copy"), "<control>C", NULL, NULL},
    {"Cut", GTK_STOCK_CUT, N_ ("C_ut"), "<control>X", NULL, NULL},
    {"Delete", GTK_STOCK_DELETE, N_ ("_Delete"), "<control>D", N_ ("Delete the selection"), NULL},
    {"Rotate", STOCK_PIXMAP_ROTATE, N_ ("_Rotate"), "<control>R",
     N_ ("Rotate the selection clockwise"), NULL},
    {"FlipH", NULL, N_ ("Flip _horizontally"), "<control>F", N_ ("Flip the selection horizontally"),
     NULL},
    {"FlipV", NULL, N_ ("Flip _vertically"), "<control><shift>F",
     N_ ("Flip the selection vertically"), NULL}};

G_DEFINE_TYPE (SheetItem, sheet_item, GOO_TYPE_CANVAS_GROUP)

static void sheet_item_dispose (GObject *object)
{
	G_OBJECT_CLASS (sheet_item_parent_class)->dispose (object);
}

static void sheet_item_class_init (SheetItemClass *sheet_item_class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (sheet_item_class);
	object_class->finalize = sheet_item_finalize;
	object_class->dispose = sheet_item_dispose;
	object_class->set_property = sheet_item_set_property;
	object_class->get_property = sheet_item_get_property;

	sheet_item_parent_class = g_type_class_peek_parent (sheet_item_class);

	// Override from GooCanvasGroup
	g_object_class_override_property (object_class, ARG_X, "x");
	g_object_class_override_property (object_class, ARG_Y, "y");
	g_object_class_override_property (object_class, ARG_WIDTH, "width");
	g_object_class_override_property (object_class, ARG_HEIGHT, "height");

	g_object_class_install_property (
	    object_class, ARG_DATA,
	    g_param_spec_pointer ("data", "SheetItem::data", "the data", G_PARAM_READWRITE));

	g_object_class_install_property (
	    object_class, ARG_SHEET,
	    g_param_spec_pointer ("sheet", "SheetItem::sheet", "the sheet", G_PARAM_READABLE));

	g_object_class_install_property (object_class, ARG_ACTION_GROUP,
	                                 g_param_spec_pointer ("action_group",
	                                                       "SheetItem::action_group",
	                                                       "action group", G_PARAM_READWRITE));

	sheet_item_class->is_in_area = NULL;
	sheet_item_class->show_labels = NULL;
	sheet_item_class->paste = NULL;

	sheet_item_class->moved = NULL;
	sheet_item_class->selection_changed = NULL;
	sheet_item_class->mouse_over = NULL;

	so_signals[PLACED] =
	    g_signal_new ("placed", G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_FIRST, 0, NULL, NULL,
	                  g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

	so_signals[MOVED] = g_signal_new ("moved", G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_FIRST,
	                                  G_STRUCT_OFFSET (SheetItemClass, moved), NULL, NULL,
	                                  g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

	so_signals[SELECTION_CHANGED] =
	    g_signal_new ("selection_changed", G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_FIRST,
	                  G_STRUCT_OFFSET (SheetItemClass, selection_changed), NULL, NULL,
	                  g_cclosure_marshal_VOID__INT, G_TYPE_NONE, 1, G_TYPE_INT);

	so_signals[MOUSE_OVER] =
	    g_signal_new ("mouse_over", G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_FIRST,
	                  G_STRUCT_OFFSET (SheetItemClass, mouse_over), NULL, NULL,
	                  g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

	so_signals[DOUBLE_CLICKED] =
	    g_signal_new ("double_clicked", G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_FIRST, 0,
	                  NULL, NULL, g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);
}

static void sheet_item_init (SheetItem *item)
{
	GError *error = NULL;

	item->priv = g_new0 (SheetItemPriv, 1);
	item->priv->selected = FALSE;
	item->priv->preserve_selection = FALSE;
	item->priv->data = NULL;
	item->priv->ui_manager = NULL;
	item->priv->action_group = NULL;

	item->priv->ui_manager = gtk_ui_manager_new ();
	item->priv->action_group = gtk_action_group_new ("action_group");
	gtk_action_group_set_translation_domain (item->priv->action_group, GETTEXT_PACKAGE);
	gtk_action_group_add_actions (item->priv->action_group, action_entries,
	                              G_N_ELEMENTS (action_entries), NULL);
	gtk_ui_manager_insert_action_group (item->priv->ui_manager, item->priv->action_group, 0);

	if (!gtk_ui_manager_add_ui_from_string (item->priv->ui_manager, sheet_item_context_menu, -1,
	                                        &error)) {
		g_warning ("building menus failed: %s", error->message);
		g_error_free (error);
	}
}

static void sheet_item_set_property (GObject *object, guint prop_id, const GValue *value,
                                     GParamSpec *spec)
{
	GooCanvasItemSimple *simple = (GooCanvasItemSimple *)object;
	SheetItem *sheet_item;
	Coords pos;

	sheet_item = SHEET_ITEM (object);

	switch (prop_id) {
	case ARG_X:
		sheet_item->x = g_value_get_double (value);
		break;
	case ARG_Y:
		sheet_item->y = g_value_get_double (value);
		break;
	case ARG_WIDTH:
		sheet_item->width = g_value_get_double (value);
		break;
	case ARG_HEIGHT:
		sheet_item->height = g_value_get_double (value);
		break;
	case ARG_DATA:
		if (sheet_item->priv->data) {
			g_warning (_ ("Cannot set SheetItem after creation."));
			break;
		}
		sheet_item->priv->data = g_value_get_pointer (value);
		item_data_get_pos (sheet_item->priv->data, &pos);
		sheet_item->x = pos.x;
		sheet_item->y = pos.y;
		break;
	case ARG_ACTION_GROUP:
		sheet_item->priv->action_group = g_value_get_pointer (value);
		gtk_ui_manager_insert_action_group (sheet_item->priv->ui_manager,
		                                    sheet_item->priv->action_group, 0);
		break;
	default:
		break;
	}
	goo_canvas_item_simple_changed (simple, TRUE);
}

static void sheet_item_get_property (GObject *object, guint prop_id, GValue *value,
                                     GParamSpec *spec)
{
	SheetItem *sheet_item;

	sheet_item = SHEET_ITEM (object);

	switch (prop_id) {
	case ARG_X:
		g_value_set_double (value, sheet_item->x);
		break;
	case ARG_Y:
		g_value_set_double (value, sheet_item->y);
		break;
	case ARG_WIDTH:
		g_value_set_double (value, sheet_item->width);
		break;
	case ARG_HEIGHT:
		g_value_set_double (value, sheet_item->height);
		break;
	case ARG_DATA:
		g_value_set_pointer (value, sheet_item->priv->data);
		break;
	case ARG_SHEET:
		g_value_set_pointer (value, sheet_item_get_sheet (sheet_item));
		break;
	case ARG_ACTION_GROUP:
		g_value_set_pointer (value, sheet_item->priv->action_group);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (sheet_item, prop_id, spec);
		break;
	}
}

static void sheet_item_finalize (GObject *object)
{
	SheetItem *sheet_item = SHEET_ITEM (object);

	g_free (sheet_item->priv);
	sheet_item->priv = NULL;

	G_OBJECT_CLASS (sheet_item_parent_class)->finalize (object);
}

static void sheet_item_run_menu (SheetItem *item, Sheet *sheet, GdkEventButton *event)
{
	GtkWidget *menu;

	menu = gtk_ui_manager_get_widget (item->priv->ui_manager, "/ItemMenu");
	gtk_menu_popup (GTK_MENU (menu), NULL, NULL, NULL, sheet, event->button, event->time);
}

// Event handler for a SheetItem
gboolean sheet_item_event (GooCanvasItem *sheet_item, GooCanvasItem *sheet_target_item,
                           GdkEvent *event, Sheet *sheet)
{
	// Remember the last position of the mouse cursor.
	GooCanvas *canvas;
	SheetPriv *priv;
	GList *list;

	static Coords last, current, snapped;
	// snapped : Mouse cursor position in window coordinates, snapped to the grid
	// spacing.
	// delta   : Move the selected item(s) by this movement.
	Coords delta, after;

	g_return_val_if_fail (sheet_item != NULL, FALSE);
	g_return_val_if_fail (sheet != NULL, FALSE);

	priv = sheet->priv;

	canvas = GOO_CANVAS (sheet);

	switch (event->type) {
	case GDK_BUTTON_PRESS:
		// Grab focus to sheet for correct use of events
		gtk_widget_grab_focus (GTK_WIDGET (sheet));
		switch (event->button.button) {
		case 1:
			g_signal_stop_emission_by_name (sheet_item, "button_press_event");
			sheet->state = SHEET_STATE_DRAG_START;
			g_assert (sheet_get_pointer (sheet, &last.x, &last.y));
			break;
		case 3:
			g_signal_stop_emission_by_name (sheet_item, "button_press_event");

			if (sheet->state != SHEET_STATE_NONE)
				return TRUE;

			// Bring up a context menu for right button clicks.
			if (!SHEET_ITEM (sheet_item)->priv->selected &&
			    !((event->button.state & GDK_SHIFT_MASK) == GDK_SHIFT_MASK))
				sheet_select_all (sheet, FALSE);

			sheet_item_select (SHEET_ITEM (sheet_item), TRUE);

			sheet_item_run_menu (SHEET_ITEM (sheet_item), sheet, (GdkEventButton *)event);
			break;
		default:
			return FALSE;
		}
		break;

	case GDK_2BUTTON_PRESS:
		// Do not interfere with object dragging.
		if (sheet->state == SHEET_STATE_DRAG)
			return FALSE;

		switch (event->button.button) {
		case 1:
			if (sheet->state == SHEET_STATE_DRAG_START)
				sheet->state = SHEET_STATE_NONE;
			g_signal_stop_emission_by_name (sheet_item, "button_press_event");
			g_signal_emit_by_name (sheet_item, "double_clicked");
			break;

		default:
			return FALSE;
		}
		break;

	case GDK_3BUTTON_PRESS:
		g_signal_stop_emission_by_name (sheet_item, "button_press_event");
		return TRUE;

	case GDK_BUTTON_RELEASE:
		switch (event->button.button) {
		case 1:
			if (sheet->state != SHEET_STATE_DRAG && sheet->state != SHEET_STATE_DRAG_START)
				return TRUE;

			g_signal_stop_emission_by_name (sheet_item, "button-release-event");

			if (sheet->state == SHEET_STATE_DRAG_START) {
				sheet->state = SHEET_STATE_NONE;

				if (!(event->button.state & GDK_SHIFT_MASK))
					sheet_select_all (sheet, FALSE);

				if (IS_SHEET_ITEM (sheet_item))
					sheet_item_select (SHEET_ITEM (sheet_item), TRUE);

				return TRUE;
			}

			// Get the mouse motion
			g_assert (sheet_get_pointer (sheet, &snapped.x, &snapped.y));
			delta = coords_sub (&snapped, &last);

			sheet->state = SHEET_STATE_NONE;
			goo_canvas_pointer_ungrab (canvas, GOO_CANVAS_ITEM (sheet_item), event->button.time);

			// Reparent the selected objects to the normal group
			// to have correct behaviour
			for (list = priv->selected_objects; list; list = list->next) {
				sheet_item_reparent (SHEET_ITEM (list->data), sheet->object_group);
			}

			for (list = priv->selected_objects; list; list = list->next) {
				ItemData *item_data;

				item_data = SHEET_ITEM (list->data)->priv->data;
				item_data_move (item_data, &delta);
				item_data_get_pos (item_data, &after);
				snap_to_grid (sheet->grid, &after.x, &after.y);
				item_data_set_pos (item_data, &after);
				item_data_register (item_data);
			}
			break;
		}

	case GDK_KEY_PRESS:
		switch (event->key.keyval) {
		case GDK_KEY_r: {
#ifndef FIXME_STILL_MINI_OFFSET
			Coords bbdelta;
			GooCanvasBounds bounds;

			// Center the objects around the mouse pointer.
			goo_canvas_item_get_bounds (GOO_CANVAS_ITEM (priv->selected_group), &bounds);

			bbdelta.x = (bounds.x2 - bounds.x1) / 2.;
			bbdelta.y = (bounds.y2 - bounds.y1) / 2.;
#endif
			sheet_rotate_selection (sheet, 90);
#ifndef FIXME_STILL_MINI_OFFSET
			// Center the objects around the mouse pointer.
			goo_canvas_item_get_bounds (GOO_CANVAS_ITEM (priv->selected_group), &bounds);

			bbdelta.x -= (bounds.x2 - bounds.x1) / 2.;
			bbdelta.y -= (bounds.y2 - bounds.y1) / 2.;

			snap_to_grid (sheet->grid, &bbdelta.x, &bbdelta.y);

			goo_canvas_item_translate (GOO_CANVAS_ITEM (priv->selected_group), bbdelta.x,
			                           bbdelta.y);
#endif
		} break;
		default:
			return FALSE;
		}
		return TRUE;

	case GDK_MOTION_NOTIFY:
		if (sheet->state != SHEET_STATE_DRAG && sheet->state != SHEET_STATE_DRAG_START)
			return FALSE;

		if (sheet->state == SHEET_STATE_DRAG_START) {
			sheet->state = SHEET_STATE_DRAG;

			// Update the selection if needed.
			if (IS_SHEET_ITEM (sheet_item) && (!SHEET_ITEM (sheet_item)->priv->selected)) {
				if (!(event->button.state & GDK_SHIFT_MASK)) {
					sheet_select_all (sheet, FALSE);
				}
				sheet_item_select (SHEET_ITEM (sheet_item), TRUE);
			}

			// Reparent the selected objects so that we can move them
			// efficiently.
			for (list = priv->selected_objects; list; list = list->next) {
				ItemData *item_data;

				item_data = SHEET_ITEM (list->data)->priv->data;
				item_data_unregister (item_data);
				sheet_item_reparent (SHEET_ITEM (list->data), priv->selected_group);
			}

			goo_canvas_pointer_grab (canvas, GOO_CANVAS_ITEM (sheet_item),
			                         GDK_POINTER_MOTION_MASK | GDK_BUTTON_RELEASE_MASK, NULL,
			                         event->button.time);
		}

		// Set last_x & last_y to the pointer position
		sheet_get_pointer (sheet, &snapped.x, &snapped.y);

		delta = coords_sub (&snapped, &last);

// Check that we don't move outside the sheet...
// Horizontally:
/*
if (cx1 <= 0) {  // leftmost edge
        dx = dx - x1;
        snap_to_grid (sheet->grid, &dx, NULL);
        snapped_x = last_x + dx;
}
else if (cx2 >= sheet_width) {  // rightmost edge
        dx = dx - (x2 - sheet_width / priv->zoom);
        snap_to_grid (sheet->grid, &dx, NULL);
        snapped_x = last_x + dx;
}

// And vertically:
if (cy1 <= 0) {  // upper edge
        dy = dy - y1;
        snap_to_grid (sheet->grid, NULL, &dy);
        snapped_y = last_y + dy;
}
else if (cy2 >= sheet_height) {  // lower edge
        dy = dy - (y2 - sheet_height / priv->zoom);
        snap_to_grid (sheet->grid, NULL, &dy);
        snapped_y = last_y + dy;
}
//last_x = snapped_x;
//last_y = snapped_y;
*/

#if !FIXME_INCREMENTAL_MOVMENT_DOES_NOT_WORK
		last = snapped;
#else
		goo_canvas_item_set_transform (GOO_CANVAS_ITEM (priv->selected_group), NULL);
#endif
		goo_canvas_item_translate (GOO_CANVAS_ITEM (priv->selected_group), delta.x, delta.y);
		return TRUE;

	default:
		return FALSE;
	}
	return TRUE;
}

// Cancel the placement of floating items and remove them.
void sheet_item_cancel_floating (Sheet *sheet)
{
	GooCanvasGroup *group;
	GList *list;

	g_return_if_fail (sheet != NULL);
	g_return_if_fail (IS_SHEET (sheet));

	group = GOO_CANVAS_GROUP (sheet->priv->floating_group);
	if (group == NULL)
		return;

	if (sheet->state != SHEET_STATE_FLOAT && sheet->state != SHEET_STATE_FLOAT_START)
		return;

	if (g_signal_handler_is_connected (sheet, sheet->priv->float_handler_id))
		g_signal_handler_disconnect (sheet, sheet->priv->float_handler_id);

	// TODO verfiy that the following has no nasty sideffects
	for (list = sheet->priv->floating_objects; list; list = list->next) {
		goo_canvas_item_remove (list->data); // remove from canvas and free
	}
	g_list_free (sheet->priv->floating_objects);
	sheet->priv->floating_objects = NULL;
	goo_canvas_item_remove (GOO_CANVAS_ITEM (group));

	// Create a new empty group to prepare next floating group
	sheet->priv->floating_group = GOO_CANVAS_GROUP (
	    goo_canvas_group_new (GOO_CANVAS_ITEM (sheet->object_group), "x", 0.0, "y", 0.0, NULL));

	// sheet_clear_ghosts (sheet);
	sheet->priv->float_handler_id = 0;
	sheet->state = SHEET_STATE_NONE;
}

// Event handler for a "floating" group of objects.
int sheet_item_floating_event (Sheet *sheet, const GdkEvent *event)
{
	SheetPriv *priv;
	GList *list;
	static gboolean keep = FALSE;

	// Remember the start position of the mouse cursor.
	static Coords last = {0., 0.};

	// Mouse cursor position in window coordinates, snapped to the grid spacing.
	static Coords snapped = {0., 0.};

	// Move the selected item(s) by this movement.
	Coords delta = {0., 0.};

	g_return_val_if_fail (sheet != NULL, FALSE);
	g_return_val_if_fail (IS_SHEET (sheet), FALSE);
	g_return_val_if_fail (sheet->priv->floating_objects != NULL, FALSE);

	priv = sheet->priv;

	switch (event->type) {
	case GDK_BUTTON_RELEASE:
		g_signal_stop_emission_by_name (sheet, "event");
		break;

	case GDK_BUTTON_PRESS:
		if (sheet->state != SHEET_STATE_FLOAT)
			return TRUE;

		switch (event->button.button) {
		case 2:
		case 4:
		case 5:
			return FALSE;

		case 1:
			// do not free the floating items, but use them like a stamp
			keep = event->button.state & GDK_CONTROL_MASK;

			// Continue adding if CTRL is pressed
			if (!keep) {
				sheet->state = SHEET_STATE_NONE;
				g_signal_stop_emission_by_name (sheet, "event");
				if (g_signal_handler_is_connected (sheet, sheet->priv->float_handler_id))
					g_signal_handler_disconnect (sheet, sheet->priv->float_handler_id);

				sheet->priv->float_handler_id = 0;
			}
			// FIXME assert that `Coords current` has been set by now!
			for (list = priv->floating_objects; list; list = list->next) {
				SheetItem *floating_item;
				ItemData *floating_data;

				// Create a real item.
				floating_item = list->data;
				if (!keep) {
					floating_data = sheet_item_get_data (floating_item);
					g_object_set (floating_item, "visibility", GOO_CANVAS_ITEM_INVISIBLE, NULL);
				} else {
					// FIXME the bounding box of the clone is wrong
					floating_data = item_data_clone (sheet_item_get_data (floating_item));
				}

				NG_DEBUG ("Item Data Pos will be %lf %lf", snapped.x, snapped.y)

				item_data_set_pos (floating_data, &snapped);

				schematic_add_item (schematic_view_get_schematic_from_sheet (sheet), floating_data);

				if (!keep)
					g_object_unref (G_OBJECT (floating_item));
			}

			if (keep) {
				g_object_set (G_OBJECT (priv->floating_group), "x", snapped.x, "y", snapped.y,
				              NULL);
			} else {
				g_list_free (priv->floating_objects);
				priv->floating_objects = NULL;
			}
			break;

		case 3:
			// Cancel the "float-placement" for button-3 clicks.
			g_signal_stop_emission_by_name (sheet, "event");
			sheet_item_cancel_floating (sheet);
			break;
		}
		break;

	case GDK_2BUTTON_PRESS:
	case GDK_3BUTTON_PRESS:
		g_signal_stop_emission_by_name (sheet, "event");
		return TRUE;

	case GDK_MOTION_NOTIFY:
// keep track of the position, as `sheet_get_pointer*()` does not work
// in other events than MOTION_NOTIFY
#if 0
		{
			Coords tmp;
			last = current;
			if (sheet_get_pointer (sheet, &tmp.x, &tmp.y)) {
				snapped_current = current = tmp;
				snap_to_grid (sheet->grid, &snapped_current.x, &snapped_current.y);
			}
		}
#endif
		if (sheet->state != SHEET_STATE_FLOAT && sheet->state != SHEET_STATE_FLOAT_START)
			return FALSE;

		g_signal_stop_emission_by_name (sheet, "event");

		// Get pointer position independantly of the zoom

		if (sheet->state == SHEET_STATE_FLOAT_START) {
			sheet->state = SHEET_STATE_FLOAT;
			last.x = last.y = 0.;
			// Reparent the selected objects so that we can move them
			// efficiently.
			for (list = priv->floating_objects; list; list = list->next) {
				sheet_item_reparent (SHEET_ITEM (list->data), priv->floating_group);
				// Set the floating item visible
				g_object_set (G_OBJECT (list->data), "visibility", GOO_CANVAS_ITEM_VISIBLE, NULL);
			}
#if 0
			GooCanvasBounds box;
			goo_canvas_item_get_bounds (priv->floating_group, &box);
#endif
			NG_DEBUG ("\n\n\nFLOAT ### START\n\n\n\n");
		}

		sheet_get_pointer_snapped (sheet, &snapped.x, &snapped.y);

		delta = coords_sub (&snapped, &last);
		NG_DEBUG ("drag floating current      sx=%lf sy=%lf \n", snapped.x, snapped.y);
		NG_DEBUG ("drag floating last         lx=%lf ly=%lf \n", last.x, last.y);
		NG_DEBUG ("drag floating delta     -> dx=%lf dy=%lf \n", delta.x, delta.y);

#if !FIXME_INCREMENTAL_MOVMENT_DOES_NOT_WORK
		last = snapped;
#else
		goo_canvas_item_set_transform (GOO_CANVAS_ITEM (priv->floating_group), NULL);
#endif
		goo_canvas_item_translate (GOO_CANVAS_ITEM (priv->floating_group), delta.x, delta.y);

		break;

	case GDK_KEY_PRESS:
		switch (event->key.keyval) {
		case GDK_KEY_r:
		case GDK_KEY_R: {
			Coords bbdelta;
			GooCanvasBounds bounds;

			// Center the objects around the mouse pointer.
			goo_canvas_item_get_bounds (GOO_CANVAS_ITEM (priv->floating_group), &bounds);

			bbdelta.x = (bounds.x2 - bounds.x1) / 2.;
			bbdelta.y = (bounds.y2 - bounds.y1) / 2.;

			sheet_rotate_ghosts (sheet);

			// Center the objects around the mouse pointer.
			goo_canvas_item_get_bounds (GOO_CANVAS_ITEM (priv->floating_group), &bounds);

			bbdelta.x -= (bounds.x2 - bounds.x1) / 2.;
			bbdelta.y -= (bounds.y2 - bounds.y1) / 2.;

			snap_to_grid (sheet->grid, &bbdelta.x, &bbdelta.y);

			goo_canvas_item_translate (GOO_CANVAS_ITEM (priv->floating_group), bbdelta.x,
			                           bbdelta.y);
		} break;
		default:
			return FALSE;
		}
	default:
		return FALSE;
	}
	return TRUE;
}

gboolean sheet_item_select (SheetItem *item, gboolean select)
{
	g_return_val_if_fail (item != NULL, FALSE);
	g_return_val_if_fail (IS_SHEET_ITEM (item), FALSE);

	if ((item->priv->selected && select) || (!item->priv->selected && !select)) {
		return FALSE;
	}

	g_signal_emit_by_name (item, "selection_changed", select);
	item->priv->selected = select;

	return TRUE;
}

void sheet_item_select_in_area (SheetItem *item, Coords *p1, Coords *p2)
{
	SheetItemClass *sheet_item_class;
	gboolean in_area;

	g_return_if_fail (item != NULL);
	g_return_if_fail (IS_SHEET_ITEM (item));
	g_return_if_fail (p1 != NULL);
	g_return_if_fail (p2 != NULL);

	sheet_item_class = SHEET_ITEM_CLASS (G_OBJECT_GET_CLASS (item));
	in_area = sheet_item_class->is_in_area (item, p1, p2);

	if (in_area && !item->priv->selected)
		sheet_item_select (item, TRUE);
	else if (!in_area && item->priv->selected && !item->priv->preserve_selection)
		sheet_item_select (item, FALSE);
}

// Reparent a sheet object without moving it on the sheet.
void sheet_item_reparent (SheetItem *item, GooCanvasGroup *group)
{
	g_return_if_fail (item != NULL);
	g_return_if_fail (IS_SHEET_ITEM (item));
	g_return_if_fail (group != NULL);

	g_object_ref (item);
	goo_canvas_item_remove (GOO_CANVAS_ITEM (item));
	goo_canvas_item_add_child (GOO_CANVAS_ITEM (group), GOO_CANVAS_ITEM (item), -1);
	g_object_unref (item);
}

void sheet_item_edit_properties (SheetItem *item)
{
	SheetItemClass *sheet_item_class;

	g_return_if_fail (item != NULL);
	g_return_if_fail (IS_SHEET_ITEM (item));

	sheet_item_class = SHEET_ITEM_CLASS (G_OBJECT_GET_CLASS (item));

	if (sheet_item_class->edit_properties)
		sheet_item_class->edit_properties (item);
}

void sheet_item_rotate (SheetItem *sheet_item, int angle, Coords *center)
{
	g_return_if_fail (sheet_item != NULL);
	g_return_if_fail (IS_SHEET_ITEM (sheet_item));

	item_data_rotate (sheet_item->priv->data, angle, center);
}

void sheet_item_paste (Sheet *sheet, ClipboardData *data)
{
	SheetItemClass *item_class;
	ItemDataClass *id_class;
	ItemData *item_data, *clone;

	g_return_if_fail (sheet != NULL);
	g_return_if_fail (IS_SHEET (sheet));
	g_return_if_fail (data != NULL);

	item_data = ITEM_DATA (clipboard_data_get_item_data (data));

	id_class = ITEM_DATA_CLASS (G_OBJECT_GET_CLASS (item_data));
	if (id_class->clone == NULL)
		return;

	// Duplicate the data for the item and paste the item on the sheet.
	item_class = SHEET_ITEM_CLASS (clipboard_data_get_item_class (data));
	if (item_class->paste) {
		clone = id_class->clone (item_data);
		item_class->paste (sheet, clone);
	}
}

ItemData *sheet_item_get_data (SheetItem *item)
{
	g_return_val_if_fail (item != NULL, NULL);
	g_return_val_if_fail (IS_SHEET_ITEM (item), NULL);

	return item->priv->data;
}

Sheet *sheet_item_get_sheet (SheetItem *item)
{
	g_return_val_if_fail (item != NULL, NULL);
	g_return_val_if_fail (IS_SHEET_ITEM (item), NULL);

	return SHEET (goo_canvas_item_get_canvas (GOO_CANVAS_ITEM (item)));
}

gboolean sheet_item_get_selected (SheetItem *item)
{
	g_return_val_if_fail (item != NULL, FALSE);
	g_return_val_if_fail (IS_SHEET_ITEM (item), FALSE);

	return item->priv->selected;
}

gboolean sheet_item_get_preserve_selection (SheetItem *item)
{
	g_return_val_if_fail (item != NULL, FALSE);
	g_return_val_if_fail (IS_SHEET_ITEM (item), FALSE);

	return item->priv->preserve_selection;
}

void sheet_item_set_preserve_selection (SheetItem *item, gboolean set)
{
	g_return_if_fail (item != NULL);
	g_return_if_fail (IS_SHEET_ITEM (item));

	item->priv->preserve_selection = set;
}

void sheet_item_place (SheetItem *item, Sheet *sheet)
{
	SheetItemClass *sheet_item_class;

	g_return_if_fail (item != NULL);
	g_return_if_fail (IS_SHEET_ITEM (item));

	sheet_item_class = SHEET_ITEM_CLASS (G_OBJECT_GET_CLASS (item));

	if (sheet_item_class->place)
		sheet_item_class->place (item, sheet);
}

void sheet_item_place_ghost (SheetItem *item, Sheet *sheet)
{
	SheetItemClass *sheet_item_class;

	g_return_if_fail (item != NULL);
	g_return_if_fail (IS_SHEET_ITEM (item));

	sheet_item_class = SHEET_ITEM_CLASS (G_OBJECT_GET_CLASS (item));

	if (sheet_item_class->place_ghost)
		sheet_item_class->place_ghost (item, sheet);
}

void sheet_item_add_menu (SheetItem *item, const char *menu, const GtkActionEntry *action_entries,
                          int nb_entries)
{
	GError *error = NULL;

	gtk_action_group_add_actions (item->priv->action_group, action_entries, nb_entries, NULL);

	if (!gtk_ui_manager_add_ui_from_string (item->priv->ui_manager, menu, -1, &error)) {
		g_message ("building menus failed: %s", error->message);
		g_error_free (error);
	}
}
