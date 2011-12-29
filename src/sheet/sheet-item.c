/*
 * sheet-item.c
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

#include <math.h>
#include "main.h"
#include "sheet-private.h"
#include "sheet-item.h"
#include "stock.h"
#include "schematic.h"
#include "schematic-view.h"

static void sheet_item_class_init (SheetItemClass * klass);

static void sheet_item_init (SheetItem *item);

static void sheet_item_set_property (GObject *object, guint prop_id,
									 const GValue *value, GParamSpec *spec);

static void sheet_item_get_property (GObject *object, guint prop_id,
										 GValue *value, GParamSpec *spec);

static void sheet_item_destroy (GtkObject *object);

static void sheet_item_run_menu (SheetItem *item, SchematicView *sv, GdkEventButton *event);

static GnomeCanvasGroupClass *sheet_item_parent_class = NULL;
extern GObject *clipboard_data_get_item_data ();
extern GObjectClass *clipboard_data_get_item_class ();

struct _SheetItemPriv {
	guint selected : 1;
	guint preserve_selection : 1;
	ItemData *data;
	GtkActionGroup *action_group;
	GtkUIManager *ui_manager;
};

enum {
	ARG_0,
	ARG_DATA,
	ARG_SHEET,
	ARG_ACTION_GROUP
};

enum {
	MOVED,
	PLACED,
	SELECTION_CHANGED,
	MOUSE_OVER,
	DOUBLE_CLICKED,

	LAST_SIGNAL
};

static guint so_signals[LAST_SIGNAL] = { 0 };
/*
 * This is the upper part of the object popup menu. It contains actions
 * that are the same for all objects, e.g. parts and wires.
 */
static const char *sheet_item_context_menu =
"<ui>"
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

GType
sheet_item_get_type ()
{
	static GType sheet_item_type = 0;

	if (!sheet_item_type) {
		static const GTypeInfo sheet_item_info = {
			sizeof (SheetItemClass),
			NULL,
			NULL,
			(GClassInitFunc) sheet_item_class_init,
			NULL,
			NULL,
			sizeof (SheetItem),
			0,
			(GInstanceInitFunc)sheet_item_init,
			NULL
		};
		sheet_item_type = g_type_register_static(GNOME_TYPE_CANVAS_GROUP,
												 "SheetItem",
												 &sheet_item_info, 0);
	}
	return sheet_item_type;
}

static void
sheet_item_class_init (SheetItemClass *sheet_item_class)
{
	GObjectClass *object_class;
	GtkObjectClass *gtk_object_class;

	object_class = G_OBJECT_CLASS(sheet_item_class);
	gtk_object_class = GTK_OBJECT_CLASS(sheet_item_class);
	sheet_item_parent_class = g_type_class_peek_parent(sheet_item_class);

	object_class->set_property = sheet_item_set_property;
	object_class->get_property = sheet_item_get_property;

	g_object_class_install_property(
				object_class,
				ARG_DATA,
				g_param_spec_pointer("data", "SheetItem::data", "the data",
									 G_PARAM_READWRITE)
	);
	g_object_class_install_property(
				object_class,
				ARG_SHEET,
				g_param_spec_pointer("sheet", "SheetItem::sheet", "the sheet",
									 G_PARAM_READABLE)
	);
	g_object_class_install_property(
				object_class,
				ARG_ACTION_GROUP,
				g_param_spec_pointer("action_group", "SheetItem::action_group", "action group",
									 G_PARAM_READWRITE)
	);

	gtk_object_class->destroy = sheet_item_destroy;

	sheet_item_class->is_in_area = NULL;
	sheet_item_class->show_labels = NULL;
	sheet_item_class->paste = NULL;

	sheet_item_class->moved = NULL;
	sheet_item_class->selection_changed = NULL;
	sheet_item_class->mouse_over = NULL;

	so_signals[PLACED] =
		g_signal_new ("placed",
				G_TYPE_FROM_CLASS(object_class),
				G_SIGNAL_RUN_FIRST,
				0,
				NULL,
				NULL,
				g_cclosure_marshal_VOID__VOID,
				G_TYPE_NONE, 0);

	so_signals[MOVED] = g_signal_new ("moved",
		G_TYPE_FROM_CLASS(object_class),
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET(SheetItemClass, moved),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	so_signals[SELECTION_CHANGED] = g_signal_new ("selection_changed",
		G_TYPE_FROM_CLASS(object_class),
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET(SheetItemClass, selection_changed),
		NULL, NULL,
		g_cclosure_marshal_VOID__INT,
		G_TYPE_NONE, 1, G_TYPE_INT);

	so_signals[MOUSE_OVER] = g_signal_new ("mouse_over",
		G_TYPE_FROM_CLASS(object_class),
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET(SheetItemClass, mouse_over),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	so_signals[DOUBLE_CLICKED] = g_signal_new ("double_clicked",
		G_TYPE_FROM_CLASS(object_class),
		G_SIGNAL_RUN_FIRST,
		0, NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);
}

static void
sheet_item_init (SheetItem *item)
{
	GError *error = NULL;

	item->priv = g_new0 (SheetItemPriv, 1);
	item->priv->selected = FALSE;
	item->priv->preserve_selection = FALSE;
	item->priv->data = NULL;
	item->priv->ui_manager = NULL;
	item->priv->action_group = NULL;

	item->priv->ui_manager = gtk_ui_manager_new ();
	if (!gtk_ui_manager_add_ui_from_string (item->priv->ui_manager, sheet_item_context_menu, -1, &error)) {
		g_message ("building menus failed: %s", error->message);
		g_error_free (error);
		exit (EXIT_FAILURE);
	}
}

static void
sheet_item_set_property (GObject *object, guint prop_id, const GValue *value,
						 GParamSpec *spec)
{
	SheetItem *sheet_item;
	SheetPos pos;

	sheet_item = SHEET_ITEM (object);

	switch (prop_id) {
	case ARG_DATA:
		if (sheet_item->priv->data) {
			g_warning ("Cannot set ItemData after creation.");
			break;
		}
		sheet_item->priv->data = g_value_get_pointer (value);
		item_data_get_pos (sheet_item->priv->data, &pos);
		gnome_canvas_item_set (GNOME_CANVAS_ITEM (object),
			"x", pos.x,
			"y", pos.y,
			NULL);
		break;
	case ARG_ACTION_GROUP:
		sheet_item->priv->action_group = g_value_get_pointer (value);
		gtk_ui_manager_insert_action_group (sheet_item->priv->ui_manager, sheet_item->priv->action_group, 0);
	default:
		break;
	}
}

static void
sheet_item_get_property (GObject *object, guint prop_id, GValue *value,
	GParamSpec *spec)
{
	SheetItem *sheet_item;

	sheet_item = SHEET_ITEM (object);

	switch (prop_id) {
	case ARG_DATA:
		g_value_set_pointer(value, sheet_item->priv->data);
		break;
	case ARG_SHEET:
		g_value_set_pointer (value, sheet_item_get_sheet (sheet_item));
		break;
	case ARG_ACTION_GROUP:
		g_value_set_pointer (value, sheet_item->priv->action_group);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(sheet_item, prop_id, spec);
		break;
	}
}

static void
sheet_item_destroy (GtkObject *object)
{
	SheetItem *sheet_item;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_SHEET_ITEM (object));

	sheet_item = SHEET_ITEM (object);

	if (sheet_item->priv) {
		g_object_unref(G_OBJECT(sheet_item->priv->data));
		g_free(sheet_item->priv);
		sheet_item->priv = NULL;
	}

	if (GTK_OBJECT_CLASS(sheet_item_parent_class)->destroy) {
		GTK_OBJECT_CLASS(sheet_item_parent_class)->destroy(object);
	}
}

static void
sheet_item_run_menu (SheetItem *item, SchematicView *sv, GdkEventButton *event)
{
	GtkWidget *menu;

	menu = gtk_ui_manager_get_widget (item->priv->ui_manager, "/ItemMenu");

	gtk_menu_popup (GTK_MENU (menu), NULL, NULL, NULL, sv, event->button, event->time);
}

static int
scroll_timeout_cb (Sheet *sheet)
{
	int width, height;
	int x, y, dx = 0, dy = 0;

	/* Get the current mouse position so that we can decide if the pointer is
	   inside the viewport. */
	gdk_window_get_pointer (GTK_WIDGET (sheet)->window, &x, &y, NULL);

	width = GTK_WIDGET (sheet)->allocation.width;
	height = GTK_WIDGET (sheet)->allocation.height;

	if (x < 0)
		dx = -1;
	else if (x > width)
		dx = 1;

	if (y < 0)
		dy = -1;
	else if (y > height)
		dy = 1;

	if (!(x > 0 && x < width && y > 0 && y < height))
		sheet_scroll (sheet, dx*5, dy*5);

	return TRUE;
}


/*
 * sheet_item_event
 *
 * Event handler for a SheetItem
 */
int
sheet_item_event (SheetItem *sheet_item, const GdkEvent *event, SchematicView *sv)
{
	SheetItemClass *class;
	GnomeCanvas *canvas;
	Sheet *sheet;
	SheetPriv *priv;
	GList *list;
	SheetPos delta;
	/* Remember the last position of the mouse cursor. */
	static double last_x, last_y;
	/* Mouse cursor position in window coordinates, snapped to the grid spacing. */
	double snapped_x, snapped_y;
	/* Move the selected item(s) by this movement. */
	double dx, dy;
	/* The selected group's bounding box in window resp. canvas coordinates. */
	double x1, y1, x2, y2;
	static double bb_x1, bb_y1, bb_x2, bb_y2;
	int cx1, cy1, cx2, cy2;
	/* The sheet's width and the its viewport's width. */
	guint sheet_width, sheet_height;

	g_return_val_if_fail (sheet_item != NULL, FALSE);
	g_return_val_if_fail (IS_SHEET_ITEM (sheet_item), FALSE);
	g_return_val_if_fail (sv != NULL, FALSE);
	g_return_val_if_fail (IS_SCHEMATIC_VIEW (sv), FALSE);

	sheet = schematic_view_get_sheet (sv);
	priv = sheet->priv;
	canvas = GNOME_CANVAS (sheet);

	switch (event->type){
	case GDK_ENTER_NOTIFY:
		/*
		 * Debugging...
		 */
		if (event->crossing.state & GDK_CONTROL_MASK)
			g_signal_emit_by_name (G_OBJECT (sheet_item), "mouse_over");
		return TRUE;

	case GDK_BUTTON_PRESS:
		/* Grab focus to sheet for correct use of events */
		gtk_widget_grab_focus (GTK_WIDGET (sheet));
		switch (event->button.button){
		case 1:
			g_signal_stop_emission_by_name (G_OBJECT (sheet), "event");
			sheet->state = SHEET_STATE_DRAG_START;

			last_x = event->button.x;
			last_y = event->button.y;
			snap_to_grid (sheet->grid, &last_x, &last_y);
			break;
		case 3:
			g_signal_stop_emission_by_name (G_OBJECT (sheet), "event");

			if (sheet->state != SHEET_STATE_NONE)
				return TRUE;

			/*
			 * Bring up a context menu for right button clicks.
			 */
			if (!sheet_item->priv->selected &&
				!((event->button.state & GDK_SHIFT_MASK) == GDK_SHIFT_MASK))
				schematic_view_select_all (sv, FALSE);

//			if (!sheet_item->priv->selected)
				sheet_item_select (sheet_item, TRUE);

			class = SHEET_ITEM_CLASS (GTK_OBJECT_GET_CLASS(sheet_item));
			sheet_item_run_menu ( sheet_item, sv, (GdkEventButton *) event);
			break;
		default:
			return FALSE;
		}
		break;

	case GDK_2BUTTON_PRESS:
		/*
		 * Do not interfere with object dragging.
		 */
		if (sheet->state == SHEET_STATE_DRAG)
			return FALSE;

		switch (event->button.button){
		case 1:
			if (sheet->state == SHEET_STATE_DRAG_START)
				sheet->state = SHEET_STATE_NONE;
			g_signal_stop_emission_by_name (G_OBJECT (sheet), "event");
			g_signal_emit_by_name (G_OBJECT (sheet_item), "double_clicked");
			break;

		default:
			return FALSE;
		}
		break;

	case GDK_3BUTTON_PRESS:
		g_signal_stop_emission_by_name (G_OBJECT (sheet), "event");
		return TRUE;

	case GDK_BUTTON_RELEASE:
		switch (event->button.button){
		case 1:
			if (sheet->state != SHEET_STATE_DRAG &&
				sheet->state != SHEET_STATE_DRAG_START)
				return TRUE;

			g_signal_stop_emission_by_name (G_OBJECT (sheet), "event");

			if (sheet->state == SHEET_STATE_DRAG_START) {
				sheet->state = SHEET_STATE_NONE;

				if (!(event->button.state & GDK_SHIFT_MASK))
					schematic_view_select_all (sv, FALSE);

				sheet_item_select (sheet_item, TRUE);

				return TRUE;
			}

			g_object_get (G_OBJECT (priv->selected_group),
			  "x", &delta.x,
			  "y", &delta.y,
			  NULL);

			gtk_timeout_remove (priv->scroll_timeout_id); // Esto no esta bien.

			sheet->state = SHEET_STATE_NONE;
			gnome_canvas_item_ungrab (GNOME_CANVAS_ITEM (sheet_item), event->button.time);


			/*
			 * HACK :(
			 * FIXME: fix this later. The problem is that we don't want to
			 * update the current view, since it acts as the controller and
			 * already is updated. It's not really a problem, but an ugly hack.
			 */
			gnome_canvas_item_move (
				GNOME_CANVAS_ITEM (priv->selected_group),
				-delta.x, -delta.y
			);

			/*
			 * Make sure the objects are reparented back to the object
			 * group _before_ we register them. Otherwise they will get
			 * incorrect coordinates.
			 */
			for (list = priv->selected_objects; list; list = list->next) {
				sheet_item_reparent (SHEET_ITEM (list->data), sheet->object_group);
			}

			/*
			 * FIXME: this is not the best way to solve this. Ideally, the
			 * moving would take care of re-registering items.
			 */
			for (list = priv->selected_objects; list; list = list->next) {
				ItemData *item_data;

				item_data = SHEET_ITEM (list->data)->priv->data;
				item_data_move (item_data, &delta);
				item_data_register (item_data);
			}

			break;
	case GDK_KEY_PRESS:
		switch (event->key.keyval) {
		case GDK_r:
			schematic_view_rotate_selection (sv);
			{
				int x, y;

				gdk_window_get_pointer (GDK_WINDOW (GTK_WIDGET (sheet)->window), &x, &y, NULL);
				gnome_canvas_window_to_world (GNOME_CANVAS (sheet), x, y, &snapped_x, &snapped_y);

				/*
				 * Center the objects around the mouse pointer.
				 */
				gnome_canvas_item_get_bounds (
					GNOME_CANVAS_ITEM (priv->floating_group),
					&x1, &y1, &x2, &y2
				);

				snap_to_grid (sheet->grid, &snapped_x, &snapped_y);

				dx = snapped_x - (x1 + (x2 - x1) / 2);
				dy = snapped_y - (y1 + (y2 - y1) / 2);
				snap_to_grid (sheet->grid, &dx, &dy);

				gnome_canvas_item_move (
					GNOME_CANVAS_ITEM (priv->floating_group),
					dx, dy
				);

				last_x = snapped_x;
				last_y = snapped_y;
			}
			break;
		default:
			return FALSE;
		}
		return TRUE;

		default:
			return FALSE;
		}
		break;

	case GDK_MOTION_NOTIFY:
		if (sheet->state != SHEET_STATE_DRAG &&
			sheet->state != SHEET_STATE_DRAG_START)
			return FALSE;

		if (sheet->state == SHEET_STATE_DRAG_START) {
			sheet->state = SHEET_STATE_DRAG;

			/*
			 * Update the selection if needed.
			 */
			if (!sheet_item->priv->selected){
				if (!(event->motion.state & GDK_SHIFT_MASK))
					schematic_view_select_all (sv, FALSE);
				sheet_item_select (sheet_item, TRUE);
			}

			gnome_canvas_item_set (
				GNOME_CANVAS_ITEM (priv->selected_group),
				"x", 0.0, "y", 0.0, NULL
			);

			/*
			 * Reparent the selected objects so that we can
			 * move them efficiently.
			 */
			for (list = priv->selected_objects; list; list = list->next){
				ItemData *item_data;

				item_data = SHEET_ITEM (list->data)->priv->data;
				item_data_unregister (item_data);
				sheet_item_reparent (SHEET_ITEM (list->data), priv->selected_group);
			}

			gnome_canvas_item_grab (GNOME_CANVAS_ITEM (sheet_item),
				GDK_POINTER_MOTION_MASK |
				GDK_BUTTON_RELEASE_MASK,
				NULL, event->button.time);

			gnome_canvas_item_get_bounds (
				GNOME_CANVAS_ITEM (priv->selected_group),
				&bb_x1, &bb_y1, &bb_x2, &bb_y2
			);

			/*
			 * Start the autoscroll timeout.
			 */
			priv->scroll_timeout_id =
				g_timeout_add (50, (void *) scroll_timeout_cb, sheet);
		}

		snapped_x = event->motion.x;
		snapped_y = event->motion.y;
		snap_to_grid (sheet->grid, &snapped_x, &snapped_y);

		dx = snapped_x - last_x;
		dy = snapped_y - last_y;

		x1 = bb_x1 + dx;
		x2 = bb_x2 + dx;
		y1 = bb_y1 + dy;
		y2 = bb_y2 + dy;

		gnome_canvas_w2c (canvas, x1, y1, &cx1, &cy1);
		gnome_canvas_w2c (canvas, x2, y2, &cx2,	&cy2);
		sheet_get_size_pixels (sheet, &sheet_width, &sheet_height);

		/* Check that we don't move outside the sheet... horizontally: */
		if (cx1 <= 0){  /* leftmost edge */
			dx = dx - x1;
			snap_to_grid (sheet->grid, &dx, NULL);
			snapped_x = last_x + dx;
		} else if (cx2 >= sheet_width){  /* rightmost edge */
			dx = dx - (x2 - sheet_width / priv->zoom);
			snap_to_grid (sheet->grid, &dx, NULL);
			snapped_x = last_x + dx;
		}

		/* And vertically: */
		if (cy1 <= 0){  /* upper edge */
			dy = dy - y1;
			snap_to_grid (sheet->grid, NULL, &dy);
			snapped_y = last_y + dy;
		} else if (cy2 >= sheet_height){  /* lower edge */
			dy = dy - (y2 - sheet_height / priv->zoom);
			snap_to_grid (sheet->grid, NULL, &dy);
			snapped_y = last_y + dy;
		}

		last_x = snapped_x;
		last_y = snapped_y;

		if (dx != 0 || dy != 0)
			gnome_canvas_item_move (GNOME_CANVAS_ITEM (priv->selected_group), dx, dy);

		/* Update the bounding box. */
		bb_x1 += dx;
		bb_x2 += dx;
		bb_y1 += dy;
		bb_y2 += dy;

		break;

	default:
		return FALSE;
	}
	return TRUE;
}

/*
 * Cancel the placement of floating items and remove them.
 */
void
sheet_item_cancel_floating (SchematicView *sv)
{
	GnomeCanvasGroup *group;
	Sheet *sheet;
	GList *list;

	g_return_if_fail (sv != NULL);
	g_return_if_fail (IS_SCHEMATIC_VIEW (sv));

	sheet = schematic_view_get_sheet (sv);
	group = sheet->priv->floating_group;
	if (group == NULL)
		return;

	if (sheet->state != SHEET_STATE_FLOAT && sheet->state != SHEET_STATE_FLOAT_START)
		return;

	g_signal_handler_disconnect(G_OBJECT (sheet), sheet->priv->float_handler_id);

	gtk_object_destroy (GTK_OBJECT (group));

	/*
	 * If the state is _START, the items are not yet put in the
	 * object_group. This means we have to destroy them one by one.
	 */
	if (sheet->state == SHEET_STATE_FLOAT_START) {
		for (list = sheet->priv->floating_objects; list; list = list->next) {
			gtk_object_destroy (GTK_OBJECT (list->data));
		}
	}

	sheet->priv->floating_group = GNOME_CANVAS_GROUP (
		gnome_canvas_item_new (
			sheet->object_group,
			gnome_canvas_group_get_type (),
			"x", 0.0,
			"y", 0.0,
			NULL)
		);

	sheet->priv->float_handler_id = 0;
	sheet->state = SHEET_STATE_NONE;
	schematic_view_clear_ghosts (sv);
}

/*
 * Event handler for a "floating" group of objects.
 */
int
sheet_item_floating_event (Sheet *sheet, const GdkEvent *event, SchematicView *schematic_view)
{
	GnomeCanvas *canvas;
	SheetPriv *priv;
	GList *list;
	static SheetPos delta, tmp;
	static int control_key_down = 0;
//	GtkArg arg[2];

	/* Remember the last position of the mouse cursor. */
	static double last_x, last_y;

	/* Mouse cursor position in window coordinates, snapped to the grid
	   spacing. */
	double snapped_x, snapped_y;

	/* Move the selected item(s) by this movement. */
	double dx, dy;

	/* The sheet is scrolled by (canvas coordinates): */
	int offset_x, offset_y;

	/* The selected group's bounding box in window resp. canvas coordinates. */
	double x1, y1, x2, y2;
	int cx1, cy1, cx2, cy2;

	/* The sheet's width and the its viewport's width. */
	guint sheet_width, sheet_height;

	g_return_val_if_fail (sheet != NULL, FALSE);
	g_return_val_if_fail (IS_SHEET (sheet), FALSE);
	g_return_val_if_fail (schematic_view != NULL, FALSE);
	g_return_val_if_fail (IS_SCHEMATIC_VIEW (schematic_view), FALSE);
	/* assert? */
	g_return_val_if_fail (sheet->priv->floating_objects != NULL, FALSE);

	priv = sheet->priv;
	canvas = GNOME_CANVAS (sheet);

	switch (event->type) {
	case GDK_BUTTON_RELEASE:
		g_signal_stop_emission_by_name (G_OBJECT (sheet), "event");
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
			control_key_down = event->button.state & GDK_CONTROL_MASK;

			/* Continue adding if CTRL is pressed */
			if (!control_key_down) {
				sheet->state = SHEET_STATE_NONE;
				g_signal_stop_emission_by_name (G_OBJECT (sheet), "event");
				g_signal_handler_disconnect ( G_OBJECT (sheet), sheet->priv->float_handler_id);
				sheet->priv->float_handler_id = 0;
			}

			g_object_get (G_OBJECT (sheet->priv->floating_group),
				"x", &tmp.x,
				"y", &tmp.y,
				NULL);

			delta.x = tmp.x - delta.x;
			delta.y = tmp.y - delta.y;

			for (list = priv->floating_objects; list; list = list->next) {
				SheetItem *floating_item;
				ItemData *floating_data;
				/*
				 * Destroy the ghost item and place a real item.
				 */
				floating_item = list->data;
				if (!control_key_down)
					floating_data = sheet_item_get_data (floating_item);
				else
					floating_data = item_data_clone (sheet_item_get_data (floating_item));

				g_object_ref (G_OBJECT (floating_data));

				item_data_move (floating_data, &delta);
				schematic_add_item (schematic_view_get_schematic (schematic_view),
									floating_data);

				if (!control_key_down)
					gtk_object_destroy (GTK_OBJECT(floating_item));
			}

			if (!control_key_down) {
				g_list_free (sheet->priv->floating_objects);
				sheet->priv->floating_objects = NULL;
			} else {
				gtk_object_set (GTK_OBJECT (sheet->priv->floating_group),
						"x", tmp.x,
						"y", tmp.y,
						NULL);
			}
			delta.x = 0;
			delta.y = 0;
			break;

		case 3:
			/*
			 * Cancel the "float-placement" for button-3 clicks.
			 */
			g_signal_stop_emission_by_name (G_OBJECT (sheet), "event");
			sheet_item_cancel_floating (schematic_view);
			break;
		}
		break;

	case GDK_2BUTTON_PRESS:
	case GDK_3BUTTON_PRESS:
		g_signal_stop_emission_by_name (G_OBJECT (sheet), "event");
		return TRUE;

	case GDK_MOTION_NOTIFY:
		if (sheet->state != SHEET_STATE_FLOAT &&
			sheet->state != SHEET_STATE_FLOAT_START)
			return FALSE;

		g_signal_stop_emission_by_name (G_OBJECT (sheet), "event");

		if (sheet->state == SHEET_STATE_FLOAT_START) {
			sheet->state = SHEET_STATE_FLOAT;

			/*
			 * Reparent the selected objects so that we can
			 * move them efficiently.
			 */
			for (list = priv->floating_objects; list; list = list->next) {
				sheet_item_reparent (SHEET_ITEM (list->data), priv->floating_group);
			}

			gtk_object_get (GTK_OBJECT (sheet->priv->floating_group),
					"x", &delta.x,
					"y", &delta.y,
					NULL);

			/*
			 * Center the objects around the mouse pointer.
			 */
			gnome_canvas_item_get_bounds (GNOME_CANVAS_ITEM (priv->floating_group),
										  &x1, &y1, &x2, &y2);

			gnome_canvas_window_to_world (canvas, event->motion.x, event->motion.y,
										  &snapped_x, &snapped_y);
			snap_to_grid (sheet->grid, &snapped_x, &snapped_y);

			dx = snapped_x - (x1 + (x2 - x1) / 2);
			dy = snapped_y - (y1 + (y2 - y1) / 2);
			snap_to_grid (sheet->grid, &dx, &dy);

			gnome_canvas_item_move (GNOME_CANVAS_ITEM (priv->floating_group),
									dx, dy);

			x1 += dx;
			y1 += dy;
			x2 += dx;
			y2 += dy;

			last_x = snapped_x;
			last_y = snapped_y;

			return TRUE;
		}

		/*
		 * We have to convert from window to world coordinates here,
		 * since we get the coordinates relative the sheet and not the
		 * item like we do in sheet_item_event ().
		 */
		gnome_canvas_window_to_world (canvas, event->motion.x, event->motion.y,
									  &snapped_x, &snapped_y);

		snap_to_grid (sheet->grid, &snapped_x, &snapped_y);

		/* Calculate which amount to move the selected objects by. */
		dx = snapped_x - last_x;
		dy = snapped_y - last_y;

		/* We need the bounding box to check that we don't move anything
		   off-sheet. */
		gnome_canvas_item_get_bounds (GNOME_CANVAS_ITEM (priv->floating_group),
									  &x1, &y1, &x2, &y2);

		x1 += dx;
		x2 += dx;
		y1 += dy;
		y2 += dy;

		gnome_canvas_get_scroll_offsets (canvas, &offset_x, &offset_y);
		gnome_canvas_w2c (canvas, x1, y1, &cx1, &cy1);
		gnome_canvas_w2c (canvas, x2, y2, &cx2,	&cy2);
		sheet_get_size_pixels (sheet, &sheet_width, &sheet_height);

		/* Check that we don't move outside the sheet horizontally */
		if (cx1 <= 0){  /* leftmost edge */
			dx = dx - x1;
			snap_to_grid (sheet->grid, &dx, NULL);
			snapped_x = last_x + dx;
		} else if (cx2 >= sheet_width){  /* rightmost edge */
			dx = dx - (x2 - sheet_width/sheet->priv->zoom);
			snap_to_grid (sheet->grid, &dx, NULL);
			snapped_x = last_x + dx;
		}

		/* And vertically */
		if (cy1 <= 0){  /* upper edge */
			dy = dy - y1;
			snap_to_grid (sheet->grid, NULL, &dy);
			snapped_y = last_y + dy;
		} else if (cy2 >= sheet_height){  /* lower edge */
			dy = dy - (y2 - sheet_height/sheet->priv->zoom);
			snap_to_grid (sheet->grid, NULL, &dy);
			snapped_y = last_y + dy;
		}

		last_x = snapped_x;
		last_y = snapped_y;

		if (dx != 0 || dy != 0){
			gnome_canvas_item_move (GNOME_CANVAS_ITEM (priv->floating_group),
									dx, dy);
		}
		break;

	case GDK_KEY_PRESS:
		switch (event->key.keyval) {
		case GDK_r:
			schematic_view_rotate_ghosts (schematic_view);
			{
				int x, y;

				gdk_window_get_pointer (GDK_WINDOW (GTK_WIDGET (sheet)->window),
										&x, &y, NULL);
				gnome_canvas_window_to_world (GNOME_CANVAS (sheet), x, y,
											  &snapped_x, &snapped_y);

				/*
				 * Center the objects around the mouse pointer.
				 */
				gnome_canvas_item_get_bounds (GNOME_CANVAS_ITEM (priv->floating_group),
											  &x1, &y1, &x2, &y2);

				snap_to_grid (sheet->grid, &snapped_x, &snapped_y);

				dx = snapped_x - (x1 + (x2 - x1) / 2);
				dy = snapped_y - (y1 + (y2 - y1) / 2);
				snap_to_grid (sheet->grid, &dx, &dy);

				gnome_canvas_item_move (GNOME_CANVAS_ITEM (priv->floating_group),
										dx, dy);

				last_x = snapped_x;
				last_y = snapped_y;
			}
			break;
		default:
			return FALSE;
		}
		return TRUE;

	default:
		return FALSE;
	}
	return TRUE;
}

gboolean
sheet_item_select (SheetItem *item, gboolean select)
{
	g_return_val_if_fail (item != NULL, FALSE);
	g_return_val_if_fail (IS_SHEET_ITEM (item), FALSE);

	if ((item->priv->selected && select) ||
		(!item->priv->selected && !select)) {
		return FALSE;
	}

	g_signal_emit_by_name (G_OBJECT (item), "selection_changed", select);
	item->priv->selected = select;

	return TRUE;
}

void
sheet_item_select_in_area (SheetItem *item, SheetPos *p1, SheetPos *p2)
{
	SheetItemClass *si_class;
	gboolean in_area;

	g_return_if_fail (item != NULL);
	g_return_if_fail (IS_SHEET_ITEM (item));
	g_return_if_fail (p1 != NULL);
	g_return_if_fail (p2 != NULL);

	si_class = SHEET_ITEM_CLASS (GTK_OBJECT_GET_CLASS (item));
	in_area = si_class->is_in_area (item, p1, p2);

	if (in_area && !item->priv->selected)
		sheet_item_select (item, TRUE);
	else if (!in_area && item->priv->selected &&
			 !item->priv->preserve_selection)
		sheet_item_select (item, FALSE);
}

/*
 * sheet_item_reparent ()
 *
 * Reparent a sheet object without moving it on the sheet.
 */
void
sheet_item_reparent (SheetItem *object, GnomeCanvasGroup *group)
{
	double x1, y1, x2, y2;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_SHEET_ITEM (object));
	g_return_if_fail (group != NULL);
	g_return_if_fail (GNOME_IS_CANVAS_GROUP (group));

	g_object_get (G_OBJECT (object),
				  "x", &x1,
				  "y", &y1,
				  NULL);

	gnome_canvas_item_i2w (GNOME_CANVAS_ITEM (object), &x1, &y1);

	gnome_canvas_item_reparent (GNOME_CANVAS_ITEM (object), group);

	g_object_get (G_OBJECT (object), "x", &x2, "y", &y2, NULL);

	gnome_canvas_item_i2w (GNOME_CANVAS_ITEM (object), &x2, &y2);

	gnome_canvas_item_move (GNOME_CANVAS_ITEM (object), x1 - x2, y1 - y2);

	/* This is needed because of a bug (?) in gnome-libs. */
	gnome_canvas_item_request_update (GNOME_CANVAS_ITEM (object));
}

void
sheet_item_edit_properties (SheetItem *item)
{
	SheetItemClass *si_class;

	g_return_if_fail (item != NULL);
	g_return_if_fail (IS_SHEET_ITEM (item));

	si_class = SHEET_ITEM_CLASS (GTK_OBJECT_GET_CLASS (item));

	if (si_class->edit_properties)
		si_class->edit_properties (item);
}

void
sheet_item_rotate (SheetItem *sheet_item, int angle, SheetPos *center)
{
	g_return_if_fail (sheet_item != NULL);
	g_return_if_fail (IS_SHEET_ITEM (sheet_item));

	item_data_rotate (sheet_item->priv->data, angle, center);
}

void
sheet_item_paste (SchematicView *schematic_view, ClipboardData *data)
{
	SheetItemClass *item_class;
	ItemDataClass *id_class;
	ItemData *item_data, *clone;

	g_return_if_fail (schematic_view != NULL);
	g_return_if_fail (IS_SCHEMATIC_VIEW (schematic_view));
	g_return_if_fail (data != NULL);

	item_data = ITEM_DATA (clipboard_data_get_item_data (data));

	id_class = ITEM_DATA_CLASS (G_OBJECT_GET_CLASS (item_data));
	if (id_class->clone == NULL)
		return;

	/*
	 * Duplicate the data for the item and paste the item on the sheet.
	 */
	item_class = SHEET_ITEM_CLASS (clipboard_data_get_item_class (data));
	if (item_class->paste) {
		clone = id_class->clone (item_data);
		item_class->paste (schematic_view, clone);
	}
}

ItemData *
sheet_item_get_data (SheetItem *item)
{
	g_return_val_if_fail (item != NULL, NULL);
	g_return_val_if_fail (IS_SHEET_ITEM (item), NULL);

	return item->priv->data;
}

Sheet *
sheet_item_get_sheet (SheetItem *item)
{
	g_return_val_if_fail (item != NULL, NULL);
	g_return_val_if_fail (IS_SHEET_ITEM (item), NULL);

	return SHEET (GNOME_CANVAS_ITEM (item)->canvas);
}

gboolean
sheet_item_get_selected (SheetItem *item)
{
	g_return_val_if_fail (item != NULL, FALSE);
	g_return_val_if_fail (IS_SHEET_ITEM (item), FALSE);

	return item->priv->selected;
}

gboolean
sheet_item_get_preserve_selection (SheetItem *item)
{
	g_return_val_if_fail (item != NULL, FALSE);
	g_return_val_if_fail (IS_SHEET_ITEM (item), FALSE);

	return item->priv->preserve_selection;
}

void
sheet_item_set_preserve_selection (SheetItem *item, gboolean set)
{
	g_return_if_fail (item != NULL);
	g_return_if_fail (IS_SHEET_ITEM (item));

	item->priv->preserve_selection = set;
}

void
sheet_item_place (SheetItem *item, SchematicView *sv)
{
	SheetItemClass *si_class;

	g_return_if_fail (item != NULL);
	g_return_if_fail (IS_SHEET_ITEM (item));

	si_class = SHEET_ITEM_CLASS (GTK_OBJECT_GET_CLASS (item));

	if (si_class->place)
		si_class->place (item, sv);
}

void
sheet_item_place_ghost (SheetItem *item, SchematicView *sv)
{
	SheetItemClass *si_class;

	g_return_if_fail (item != NULL);
	g_return_if_fail (IS_SHEET_ITEM (item));

	si_class = SHEET_ITEM_CLASS (GTK_OBJECT_GET_CLASS (item));

	if (si_class->place_ghost)
		si_class->place_ghost (item, sv);
}

void
sheet_item_add_menu (SheetItem *item, const char *menu)
{
	GError *error = NULL;
	if (!gtk_ui_manager_add_ui_from_string (item->priv->ui_manager, menu, -1, &error)) {
		g_message ("building menus failed: %s", error->message);
		g_error_free (error);
		exit (EXIT_FAILURE);
	}
}
