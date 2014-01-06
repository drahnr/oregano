/*
 * part-item.c
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
 * Copyright (C) 2013-2014  Bernhard Schuster
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

#include <glib/gi18n.h>
#include <string.h>
#include <goocanvas-2.0/goocanvas.h>
#include <math.h>

#include "oregano.h"
#include "sheet-item.h"
#include "part-item.h"
#include "part-private.h"
#include "part-property.h"
#include "load-library.h"
#include "load-common.h"
#include "part-label.h"
#include "stock.h"
#include "dialogs.h"
#include "sheet.h"
#include "oregano-utils.h"
#include "options.h"

#define NORMAL_COLOR "red"
#define LABEL_COLOR "dark cyan"
#define SELECTED_COLOR "green"

#include "debug.h"

static void 		       part_item_class_init (PartItemClass *klass);
static void 		       part_item_init (PartItem *gspart);
static void 		       part_item_finalize (GObject *object);
static void 		       part_item_moved (SheetItem *sheet_item);
static void 		       edit_properties (SheetItem *object);
static void 		       selection_changed (PartItem *item, gboolean select,
								gpointer user_data);
static int  		       select_idle_callback (PartItem *item);
static int  		       deselect_idle_callback (PartItem *item);
static void 			   update_canvas_labels (PartItem *part_item);
static gboolean 		   is_in_area (SheetItem *object, Coords *p1, 
		                  		Coords *p2);
inline static void 		   get_cached_bounds (PartItem *item, Coords *p1, 
		                   		Coords *p2);
static void 		       show_labels (SheetItem *sheet_item, gboolean show);
static void 		       part_item_paste (Sheet *sheet, ItemData *data);
static void 		       part_rotated_callback (ItemData *data, int angle, SheetItem *item);
static void 		       part_flipped_callback (ItemData *data, IDFlip direction, SheetItem *sheet_item);
static void 		       part_moved_callback (ItemData *data, Coords *pos, SheetItem *item);
static void 		       part_changed_callback (ItemData *data, SheetItem *sheet_item);

static void 		       part_item_place (SheetItem *item, Sheet *sheet);
static void 		       part_item_place_ghost (SheetItem *item, Sheet *sheet);
static void 		       create_canvas_items (GooCanvasGroup *group, 
		                		LibraryPart *library_part);      
static void 		       create_canvas_labels (PartItem *item, Part *part);
static void 		       create_canvas_label_nodes (PartItem *item, Part *part);
static PartItem *	       part_item_canvas_new (Sheet *sheet, Part *part);
static void 		       part_item_get_property (GObject *object, 
		                   		guint prop_id, GValue *value, GParamSpec *spec);
static void 		       part_item_set_property (GObject *object, 
		                   		guint prop_id, const GValue *value, 
		                        GParamSpec *spec);
static void                part_item_dispose (GObject *object);
static GooCanvasAnchorType part_item_get_anchor_from_part (Part *part);


enum {
	ARG_0,
	ARG_DATA,
	ARG_SHEET,
	ARG_ACTION_GROUP,
	ARG_NAME,
	ARG_SYMNAME,
	ARG_LIBNAME,
	ARG_REFDES,
	ARG_TEMPLATE,
	ARG_MODEL
};

struct _PartItemPriv {
	guint 			  cache_valid : 1;
	GooCanvasItem	 *label_group;
	GSList 			 *label_items;
	GooCanvasItem    *node_group;
	GSList           *label_nodes;

	GooCanvasItem    *rect;
	// Cached bounding box. This is used to make
	// the rubberband selection a bit faster.
	Coords 		  bbox_start;
	Coords 		  bbox_end;
};

typedef struct {
	GtkDialog *dialog;
	PartItem  *part_item;
	// List of GtkEntry's
	GList     *widgets;
} PartPropDialog;

static PartPropDialog *prop_dialog = NULL;
static SheetItemClass *parent_class = NULL;

static const char *part_item_context_menu =
"<ui>"
"  <popup name='ItemMenu'>"
"    <menuitem action='ObjectProperties'/>"
"  </popup>"
"</ui>";

static GtkActionEntry action_entries[] = {
	{"ObjectProperties", GTK_STOCK_PROPERTIES, N_("_Object Properties..."), 
		NULL, N_("Modify object properties"),
		NULL}
};

enum {
	ANCHOR_NORTH,
	ANCHOR_SOUTH,
	ANCHOR_WEST,
	ANCHOR_EAST
};

G_DEFINE_TYPE (PartItem, part_item, TYPE_SHEET_ITEM)

static void
part_item_class_init (PartItemClass *part_item_class)
{
	GObjectClass *object_class;
	SheetItemClass *sheet_item_class;

	object_class = G_OBJECT_CLASS (part_item_class);
	sheet_item_class = SHEET_ITEM_CLASS (part_item_class);
	parent_class = g_type_class_peek_parent (part_item_class);

	object_class->finalize = part_item_finalize;
	object_class->dispose = part_item_dispose;
	object_class->set_property = part_item_set_property;
	object_class->get_property = part_item_get_property;

	sheet_item_class->moved = part_item_moved;
	sheet_item_class->is_in_area = is_in_area;
	sheet_item_class->show_labels = show_labels;
	sheet_item_class->paste = part_item_paste;
	sheet_item_class->edit_properties = edit_properties;
	sheet_item_class->selection_changed = (gpointer) selection_changed;

	sheet_item_class->place = part_item_place;
	sheet_item_class->place_ghost = part_item_place_ghost;
}

static void
part_item_init (PartItem *item)
{
	PartItemPriv *priv;

	priv = g_slice_new0 (PartItemPriv);
	priv->rect = NULL;
	priv->cache_valid = FALSE;

	item->priv = priv;

	sheet_item_add_menu (SHEET_ITEM (item), part_item_context_menu,
	    action_entries, G_N_ELEMENTS (action_entries));
}

static void
part_item_set_property (GObject *object, guint propety_id, const GValue *value,
	GParamSpec *pspec)
{
	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_PART_ITEM(object));

	switch (propety_id) {
	default:
		g_warning ("PartItem: Invalid argument.\n");
	}
}

static void
part_item_get_property (GObject *object, guint propety_id, GValue *value,
	GParamSpec *pspec)
{
	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_PART_ITEM (object));

	switch (propety_id) {
	default:
		pspec->value_type = G_TYPE_INVALID;
		break;
	}
}

static void
part_item_dispose (GObject *object)
{
	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
part_item_finalize (GObject *object)
{
	PartItemPriv *priv;

	priv = PART_ITEM (object)->priv;

	g_slist_free (priv->label_nodes);
	g_slist_free (priv->label_items);
	g_slice_free (PartItemPriv, priv);
	priv = NULL;
	G_OBJECT_CLASS (parent_class)->finalize (object);
}


////////////////////////////////////////////////////////////////////////////////
// END BOILER PLATE
////////////////////////////////////////////////////////////////////////////////

static void
part_item_set_label_items (PartItem *item, GSList *item_list)
{
	PartItemPriv *priv;

	g_return_if_fail (item != NULL);
	g_return_if_fail (IS_PART_ITEM (item));

	priv = item->priv;

	if (priv->label_items)
		g_slist_free (priv->label_items);

	priv->label_items = item_list;
}


static void
part_item_moved (SheetItem *sheet_item)
{
//	g_warning ("part MOVED callback called - LEGACY");
}

PartItem *
part_item_canvas_new (Sheet *sheet, Part *part)
{
	PartItem *part_item;
	PartItemPriv *priv;
	GooCanvasItem *goo_item;
	ItemData *item_data;

	g_return_val_if_fail (sheet != NULL, NULL);
	g_return_val_if_fail (IS_SHEET (sheet), NULL);
	g_return_val_if_fail (part != NULL, NULL);
	g_return_val_if_fail (IS_PART (part), NULL);

	part_item = g_object_new (TYPE_PART_ITEM, NULL);
	goo_item = GOO_CANVAS_ITEM (part_item);

	g_object_set (part_item,
	              "parent", sheet->object_group,
	              NULL);

	g_object_set (part_item,
                  "data", part,
                  NULL);

	priv = part_item->priv;


	Coords b1, b2;
	item_data_get_relative_bbox (ITEM_DATA (part), &b1, &b2);

	priv->rect = goo_canvas_rect_new (
	        goo_item,
	        b1.x, b1.y,
	        b2.x-b1.x, b2.y-b1.y,
	        "stroke-color", "green",
	        "line-width", .0,
	        "fill-color-rgba", 0x7733aa66,
	        "radius-x", 1.0,
	        "radius-y", 1.0,
	        "visibility", opts.debug.boundingboxes ? GOO_CANVAS_ITEM_VISIBLE : GOO_CANVAS_ITEM_INVISIBLE,
	        NULL);

	priv->label_group = goo_canvas_group_new (
	        goo_item,
	        "width", -1.0,
	        "height", -1.0,
	        NULL);
	g_object_unref (goo_item); //FIXME wtf? why?

	priv->node_group = goo_canvas_group_new (
	        goo_item,
	        NULL);

	g_object_set (priv->node_group,
	              "visibility", GOO_CANVAS_ITEM_INVISIBLE,
	              NULL);

	item_data = ITEM_DATA (part);
	item_data->rotated_handler_id = g_signal_connect_object (part,
	                                "rotated",
	                                G_CALLBACK (part_rotated_callback),
	                                part_item, 0);
	item_data->flipped_handler_id = g_signal_connect_object (part,
	                                "flipped",
	                                G_CALLBACK (part_flipped_callback),
	                                part_item, 0);
	item_data->moved_handler_id = g_signal_connect_object (part,
	                                "moved",
	                                G_CALLBACK (part_moved_callback),
	                                part_item, 0);
	item_data->changed_handler_id = g_signal_connect_object (part,
	                                "changed",
	                                G_CALLBACK (part_changed_callback),
	                                part_item, 0);

	return part_item;
}

static void
update_canvas_labels (PartItem *item)
{
	PartItemPriv *priv;
	Part *part;
	GSList *labels, *label_items;
	GooCanvasItem *canvas_item;

	g_return_if_fail (item != NULL);
	g_return_if_fail (IS_PART_ITEM (item));

	priv = item->priv;
	part = PART (sheet_item_get_data (SHEET_ITEM (item)));

	label_items = priv->label_items;

	// Put the label of each item
	for (labels = part_get_labels (part); labels;
	     labels = labels->next, label_items = label_items->next) {
		char *text;
		PartLabel *label = (PartLabel*) labels->data;
		g_assert (label_items != NULL);
		canvas_item = label_items->data;

		text = part_property_expand_macros (part, label->text);
		g_object_set (canvas_item, 
					  "text", text, 
					  NULL);
		g_free (text);
	}
}

void
part_item_update_node_label (PartItem *item)
{
	PartItemPriv *priv;
	Part *part;
	GSList *labels;
	GooCanvasItem *canvas_item;
	Pin *pins;
	gint num_pins;

	g_return_if_fail (item != NULL);
	g_return_if_fail (IS_PART_ITEM (item));
	priv = item->priv;
	part = PART (sheet_item_get_data (SHEET_ITEM (item)));

	g_return_if_fail (IS_PART (part) );

	// Put the label of each node
	num_pins = part_get_num_pins (part);
	
	if (num_pins == 1) {
		pins = part_get_pins (part);
		labels = priv->label_nodes;
		for (labels = priv->label_nodes; labels; labels=labels->next) {
			char *txt;

			txt = g_strdup_printf ("V(%d)", pins[0].node_nr);
			canvas_item = labels->data;
			if (pins[0].node_nr != 0)
				g_object_set (canvas_item, 
			              "text", txt,  
			        	  "fill_color", LABEL_COLOR,
			        	  "font", "Sans 8", 
			              NULL);
			else
				g_object_set (canvas_item, 
			              "text", "", 
			              NULL);

			g_free (txt);
		}
	}
}

static void 
prop_dialog_destroy (GtkWidget *widget, PartPropDialog *prop_dialog)
{
	g_free (prop_dialog);
}

static void
prop_dialog_response (GtkWidget *dialog, gint response,
	PartPropDialog *prop_dialog)
{
	GSList		 *props;
	GList		 *widget;
	Property	 *prop;
	PartItem	 *item;
	Part		 *part;
	gchar        *prop_name;
	const gchar  *prop_value;
	GtkWidget    *w;

	item = prop_dialog->part_item;

	part = PART (sheet_item_get_data (SHEET_ITEM (item)));

	for (widget = prop_dialog->widgets; widget;
	     widget = widget->next) {
		w = widget->data;

		prop_name = g_object_get_data (G_OBJECT (w), "user");
		prop_value = gtk_entry_get_text (GTK_ENTRY (w));

		for (props = part_get_properties (part); props; props = props->next) {
			prop = props->data;
			if (g_ascii_strcasecmp (prop->name, prop_name) == 0) {
				if (prop->value) g_free (prop->value);
				prop->value = g_strdup (prop_value);
			}
		}
		g_free (prop_name);
	}
	g_slist_free_full (props, g_object_unref);

	update_canvas_labels (item);
}

static void
edit_properties_point (PartItem *item)
{
	GSList *properties;
	Part *part;
	GtkBuilder *gui;
	GError *error = NULL;
	GtkRadioButton *radio_v, *radio_c;
	GtkRadioButton *ac_r, *ac_m, *ac_i, *ac_p;
	GtkCheckButton *chk_db;

	part = PART (sheet_item_get_data (SHEET_ITEM (item)));

	if ((gui = gtk_builder_new ()) == NULL) {
		oregano_error (_("Could not create part properties dialog."));
		return;
	}
	gtk_builder_set_translation_domain (gui, NULL);

	if (gtk_builder_add_from_file (gui, OREGANO_UIDIR "/clamp-properties-dialog.ui", 
	                               &error) <= 0) {
		oregano_error_with_title (_("Could not create part properties dialog."), error->message);
		g_error_free (error);
		return;
	}	

	prop_dialog = g_new0 (PartPropDialog, 1);

	prop_dialog->part_item = item;

	prop_dialog->dialog = GTK_DIALOG (gtk_builder_get_object (gui, 
	                                   "clamp-properties-dialog"));

	radio_v = GTK_RADIO_BUTTON (gtk_builder_get_object (gui, "radio_v"));
	radio_c = GTK_RADIO_BUTTON (gtk_builder_get_object (gui, "radio_c"));

	gtk_widget_set_sensitive (GTK_WIDGET (radio_c), FALSE);

	ac_r = GTK_RADIO_BUTTON (gtk_builder_get_object (gui, "radio_r"));
	ac_m = GTK_RADIO_BUTTON (gtk_builder_get_object (gui, "radio_m"));
	ac_p = GTK_RADIO_BUTTON (gtk_builder_get_object (gui, "radio_p"));
	ac_i = GTK_RADIO_BUTTON (gtk_builder_get_object (gui, "radio_i"));

	chk_db = GTK_CHECK_BUTTON (gtk_builder_get_object (gui, "check_db"));
	
	// Setup GUI from properties
	for (properties = part_get_properties (part); properties;
		properties = properties->next) {
		Property *prop;
		prop = properties->data;
		if (prop->name) {
			if (!g_ascii_strcasecmp (prop->name, "internal"))
				continue;

			if (!g_ascii_strcasecmp (prop->name, "type")) {
				if (!g_ascii_strcasecmp (prop->value, "v")) {
					gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio_v), TRUE);
				} 
				else {
					gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio_c), TRUE);
				}
			} 
			else if (!g_ascii_strcasecmp (prop->name, "ac_type")) {
				if (!g_ascii_strcasecmp (prop->value, "m")) {
					gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (ac_m), TRUE);
				} 
				else if (!g_ascii_strcasecmp (prop->value, "i")) {
					gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (ac_i), TRUE);
				} 
				else if (!g_ascii_strcasecmp (prop->value, "p")) {
					gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (ac_p), TRUE);
				} 
				else if (!g_ascii_strcasecmp (prop->value, "r")) {
					gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (ac_r), TRUE);
				}
			} 
			else if (!g_ascii_strcasecmp (prop->name, "ac_db")) {
				if (!g_ascii_strcasecmp (prop->value, "true"))
					gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (chk_db), TRUE);
			}
		}
	}

	gtk_dialog_run (prop_dialog->dialog);

	// Save properties from GUI
	for (properties = part_get_properties (part); properties;
		properties = properties->next) {
		Property *prop;
		prop = properties->data;

		if (prop->name) {
			if (!g_ascii_strcasecmp (prop->name, "internal"))
				continue;
	
			if (!g_ascii_strcasecmp (prop->name, "type")) {
				g_free (prop->value);
				if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (radio_v))) {
					prop->value = g_strdup ("v");
				} 
				else {
					prop->value = g_strdup ("i");
				}
			} 
			else if (!g_ascii_strcasecmp (prop->name, "ac_type")) {
				g_free (prop->value);
				if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (ac_m))) {
					prop->value = g_strdup ("m");
				} 
				else if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (ac_i))) {
					prop->value = g_strdup ("i");
				} 
				else if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (ac_p))) {
					prop->value = g_strdup ("p");
				} 
				else if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (ac_r))) {
					prop->value = g_strdup ("r");
				}
			} 
			else if (!g_ascii_strcasecmp (prop->name, "ac_db")) {
				g_free (prop->value);
				if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (chk_db)))
					prop->value = g_strdup ("true");
				else
					prop->value = g_strdup ("false");
			}
		}
	}
	g_slist_free_full (properties, g_object_unref);
	gtk_widget_destroy (GTK_WIDGET (prop_dialog->dialog));
}

static void
edit_properties (SheetItem *object)
{
	GSList *properties;
	PartItem *item;
	Part *part;
	char *internal, *msg;
	GtkBuilder *gui;
	GError *error = NULL;
	GtkGrid *prop_grid;
	GtkNotebook *notebook;
	gint response, y = 0;
	gboolean has_model;
	gchar *model_name = NULL;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_PART_ITEM (object));

	item = PART_ITEM (object);
	part = PART (sheet_item_get_data (SHEET_ITEM (item)));

	internal = part_get_property (part, "internal");
	if (internal) {
		if (g_ascii_strcasecmp (internal, "ground") == 0) {
			g_free (internal);
			return;
		}
		if (g_ascii_strcasecmp (internal, "point") == 0) {
			edit_properties_point (item);
			return;
		}
	}

	g_free (internal);

	if ((gui = gtk_builder_new ()) == NULL) {
		oregano_error (_("Could not create part properties dialog."));
		return;
	} 
	else
		 gtk_builder_set_translation_domain (gui, NULL);


	if (gtk_builder_add_from_file (gui, OREGANO_UIDIR "/part-properties-dialog.ui", 
	    &error) <= 0) {
		msg = error->message;
		oregano_error_with_title (_("Could not create part properties dialog."), 
		                          msg);
		g_error_free (error);
		return;
	}

	prop_dialog = g_new0 (PartPropDialog, 1);

	prop_dialog->part_item = item;

	prop_dialog->dialog = GTK_DIALOG (gtk_builder_get_object (gui, 
	                      "part-properties-dialog"));

	prop_grid = GTK_GRID (gtk_builder_get_object (gui, "prop_grid"));
	notebook  = GTK_NOTEBOOK (gtk_builder_get_object (gui, "notebook"));

	g_signal_connect (prop_dialog->dialog, "destroy",
		G_CALLBACK (prop_dialog_destroy),  prop_dialog);

	prop_dialog->widgets = NULL;
	has_model = FALSE;

	for (properties = part_get_properties (part); properties;
		properties = properties->next) {
		Property *prop;
		
		prop = properties->data;

		if (prop->name) {
			GtkWidget *entry;
			GtkWidget *label;
			gchar *temp=NULL;
			
			if (!g_ascii_strcasecmp (prop->name, "internal"))
				continue;

			if (!g_ascii_strcasecmp (prop->name,  "model")) {
				has_model = TRUE;
				model_name = g_strdup (prop->value);
			}
			
			// Find the Refdes and replace by their real value
			temp = prop->name;
			if (!g_ascii_strcasecmp (temp,  "Refdes")) temp = _("Designation");
			if (!g_ascii_strcasecmp (temp,  "Template")) temp  = _("Template");
			if (!g_ascii_strcasecmp (temp,  "Res")) temp  = _("Resistor");
			if (!g_ascii_strcasecmp (temp,  "Cap")) temp  = _("Capacitor");
			if (!g_ascii_strcasecmp (temp,  "Ind")) temp  = _("Inductor");
			label = gtk_label_new (temp);

			entry = gtk_entry_new ();
			gtk_entry_set_text (GTK_ENTRY (entry),  prop->value);
			g_object_set_data (G_OBJECT (entry),  "user",  g_strdup (prop->name));

			gtk_grid_attach (prop_grid, label, 0,y, 1,1);
			
			gtk_grid_attach (prop_grid, entry, 1,y, 1,1);
			
			y++;
			gtk_widget_show (label);
			gtk_widget_show (entry);

			prop_dialog->widgets = g_list_prepend (prop_dialog->widgets, entry);
		}
	}

	if (!has_model) {
		gtk_notebook_remove_page (notebook, 1); 
	} 
	else {
		GtkTextBuffer *txtbuffer;
		GtkTextView *txtmodel;
		gchar *filename, *str;
		GError *read_error = NULL;

		txtmodel = GTK_TEXT_VIEW (gtk_builder_get_object (gui, "txtmodel"));
		txtbuffer = gtk_text_buffer_new (NULL);

		filename = g_strdup_printf ("%s/%s.model", OREGANO_MODELDIR, model_name);
		if (g_file_get_contents (filename, &str, NULL, &read_error)) {
			gtk_text_buffer_set_text (txtbuffer, str, -1);
			g_free (str);
		} 
		else {
			gtk_text_buffer_set_text (txtbuffer, read_error->message, -1);
			g_error_free (read_error);
		}

		g_free (filename);
		g_free (model_name);

		gtk_text_view_set_buffer (txtmodel, txtbuffer);
	}

	gtk_dialog_set_default_response (prop_dialog->dialog, 1);

	response = gtk_dialog_run (prop_dialog->dialog);

	prop_dialog_response (GTK_WIDGET (prop_dialog->dialog), response, prop_dialog);

	g_slist_free_full (properties, g_object_unref);
	gtk_widget_destroy (GTK_WIDGET (prop_dialog->dialog));
}




inline static GooCanvasAnchorType
angle_to_anchor (int angle)
{
	GooCanvasAnchorType anchor;
	// Get the right anchor for the labels. This is needed since the
	// canvas doesn't know how to rotate text and since we rotate the
	// label_group instead of the labels directly.

	while (angle<0)
		angle+=360;
	angle %= 360;

	if (90-45 < angle && angle < 90+45) {
		anchor = GOO_CANVAS_ANCHOR_NORTH_WEST;	
	} else if (180-45 < angle && angle < 180+45) {
		anchor = GOO_CANVAS_ANCHOR_NORTH_EAST;
	} else if (270-45 < angle && angle < 270+45) {
		anchor = GOO_CANVAS_ANCHOR_SOUTH_EAST;
	} else/* if (360-45 < angle && angle < 0+45) */{
		anchor = GOO_CANVAS_ANCHOR_SOUTH_WEST;
	}
	
	return anchor;
}


/**
 * whenever the model changes, this one gets called to update the view representation
 * @attention this recalculates the matrix every time, this makes sure no errors stack up
 * @attention further reading on matrix manipulations
 * @attention http://www.cairographics.org/matrix_transform/
 * @param data the model item, a bare C struct derived from ItemData
 * @param sheet_item the view item, derived from goo_canvas_group/item
 */
static void
part_changed_callback (ItemData *data, SheetItem *sheet_item)
{
	g_return_if_fail (sheet_item != NULL);
	g_return_if_fail (IS_PART_ITEM (sheet_item));


	//TODO add static vars in order to skip the redraw if nothing changed
	//TODO may happen once in a while and the check is really cheap
	GSList *iter;
	GooCanvasAnchorType anchor;
	GooCanvasGroup *group;
	GooCanvasItem *canvas_item;
	PartItem *item;
	PartItemPriv *priv;
	Part *part;
	int index = 0;
	Coords pos;


	item = PART_ITEM (sheet_item);
	group = GOO_CANVAS_GROUP (item);
	part = PART (data);

	priv = item->priv;

	// init the states

	cairo_matrix_t morph, inv;
	cairo_status_t done;

	inv = *(item_data_get_rotate(data)); //copy
	cairo_matrix_multiply (&morph, &inv, item_data_get_translate (data));


	done = cairo_matrix_invert (&inv);
	if (done != CAIRO_STATUS_SUCCESS) {
		g_warning ("Failed to invert matrix. This should never happen. Never!");
		return;
	}
	// no translations
	inv.y0 = inv.x0 = 0.;

	goo_canvas_item_set_transform (GOO_CANVAS_ITEM (sheet_item), &(morph));

	priv->cache_valid = FALSE;
	return; /* FIXME */
#if 0
	// rotate all items in the canvas group
	for (index = 0; index < group->items->len; index++) {
		canvas_item = GOO_CANVAS_ITEM (group->items->pdata[index]);
		goo_canvas_item_set_transform (GOO_CANVAS_ITEM (canvas_item), &morph);
	}

	// revert the rotation of all labels and change their anchor to not overlap too badly
	// this assures that the text is always horizontal and properly aligned
	anchor = angle_to_anchor (rotation);

	for (iter = priv->label_items; iter; iter = iter->next) {
		g_object_set (iter->data,
		              "anchor", anchor,
		              NULL);

		goo_canvas_item_set_transform (iter->data, &inv);

	}
	// same for label nodes
	for (iter = priv->label_nodes; iter; iter = iter->next) {
		g_object_set (iter->data,
		              "anchor", anchor, 
		              NULL);

		goo_canvas_item_set_transform (iter->data, &inv);
	}


	// Invalidate the bounding box cache.
	priv->cache_valid = FALSE;
#endif
}

/**
 * a part got rotated
 *
 * @angle the angle the item is rotated towards the default (0) rotation
 *
 */
static void
part_rotated_callback (ItemData *data, int angle, SheetItem *sheet_item)
{
//	g_warning ("ROTATED callback called - LEGACY\n");
}


/**
 * handles the update of the canvas item when a part gets flipped (within the backend alias model)
 * @data the part in form of a ItemData pointer
 * @direction the new flip state
 * @sheet_item the corresponding sheet_item to the model item @data
 */
static void
part_flipped_callback (ItemData *data, IDFlip direction, SheetItem *sheet_item)
{
//	g_warning ("FLIPPED callback called - LEGACY\n");
}

void
part_item_signal_connect_floating (PartItem *item)
{
	Sheet *sheet;

	sheet = sheet_item_get_sheet (SHEET_ITEM (item));
	sheet->state = SHEET_STATE_FLOAT_START;

	g_signal_connect (G_OBJECT (item), "double_clicked",
		G_CALLBACK (edit_properties), item);
}

static void
selection_changed (PartItem *item, gboolean select, gpointer user_data)
{
	g_object_ref (G_OBJECT (item));
	if (select)
		g_idle_add ((gpointer) select_idle_callback, item);
	else
		g_idle_add ((gpointer) deselect_idle_callback, item);
}

static int
select_idle_callback (PartItem *item)
{
	GooCanvasItem *canvas_item = NULL;
	int index;
	
	g_return_val_if_fail (item != NULL, FALSE);

	for (index = 0; index < GOO_CANVAS_GROUP (item)->items->len; index++) {	
		canvas_item = GOO_CANVAS_ITEM (GOO_CANVAS_GROUP (item)->items->pdata[index]);
		g_object_set (canvas_item, 
		              "stroke-color", SELECTED_COLOR, 
		              NULL);
	}
	g_object_unref (G_OBJECT (item));
	return FALSE;
}

static int
deselect_idle_callback (PartItem *item)
{
	GooCanvasItem *canvas_item = NULL;
	int index;

	for (index = 0; index < GOO_CANVAS_GROUP (item)->items->len; index++) {
		canvas_item = GOO_CANVAS_ITEM (GOO_CANVAS_GROUP (item)->items->pdata[index]);
		
		if (GOO_IS_CANVAS_TEXT (canvas_item)) {
			g_object_set (canvas_item, 
			              "stroke-color", LABEL_COLOR, 
			              NULL);
		}
		else {
			g_object_set (canvas_item, 
			              "stroke-color", NORMAL_COLOR, 
			              NULL);
		}
	}
	g_object_unref (G_OBJECT (item));
	return FALSE;
}

static gboolean
is_in_area (SheetItem *object, Coords *p1, Coords *p2)
{
	PartItem *item;
	Coords bbox_start, bbox_end;

	item = PART_ITEM (object);

	get_cached_bounds (item, &bbox_start, &bbox_end);

	if ((p1->x < bbox_start.x) &&
	    (p2->x > bbox_end.x)   &&
	    (p1->y < bbox_start.y) &&
	    (p2->y > bbox_end.y))  {
		return TRUE;
	}
	return FALSE;
}

static void
show_labels (SheetItem *sheet_item, gboolean show)
{
	PartItem *item;
	PartItemPriv *priv;

	g_return_if_fail (sheet_item != NULL);
	g_return_if_fail (IS_PART_ITEM (sheet_item));

	item = PART_ITEM (sheet_item);
	priv = item->priv;

	if (show)
		g_object_set (priv->label_group, 
				      "visibility", GOO_CANVAS_ITEM_VISIBLE, 
		              NULL);
	else
		g_object_set (priv->label_group, 
				      "visibility", GOO_CANVAS_ITEM_INVISIBLE, 
		              NULL);
}

// Retrieves the bounding box. We use a caching scheme for this
// since it's too expensive to calculate it every time we need it.
inline static void
get_cached_bounds (PartItem *item, Coords *p1, Coords *p2)
{
	PartItemPriv *priv;
	priv = item->priv;

	if (!priv->cache_valid) {
		Coords start_pos, end_pos;
		GooCanvasBounds bounds;
		
		goo_canvas_item_get_bounds (GOO_CANVAS_ITEM (item), &bounds);

		start_pos.x = bounds.x1;
		start_pos.y = bounds.y1;
		end_pos.x = bounds.x2;
		end_pos.y = bounds.y2;

		priv->bbox_start = start_pos;
		priv->bbox_end = end_pos;
		priv->cache_valid = TRUE;
	}

	memcpy (p1, &priv->bbox_start, sizeof (Coords));
	memcpy (p2, &priv->bbox_end, sizeof (Coords));
}

static void
part_item_paste (Sheet *sheet, ItemData *data)
{
	g_return_if_fail (sheet != NULL);
	g_return_if_fail (IS_SHEET (sheet));
	g_return_if_fail (data != NULL);
	g_return_if_fail (IS_PART (data));

	sheet_add_ghost_item (sheet, data);
}

PartItem *
part_item_new (Sheet *sheet, Part *part)
{
	Library *library;
	LibraryPart *library_part;
	PartPriv *priv;
	PartItem *item;

	priv = part->priv;

	library = priv->library;
	library_part = library_get_part (library, priv->name);

	// Create the PartItem canvas item
	item = part_item_canvas_new (sheet, part);
	create_canvas_items (GOO_CANVAS_GROUP (item), library_part);
	create_canvas_labels (item, part);
	create_canvas_label_nodes (item, part);
	goo_canvas_item_ensure_updated (GOO_CANVAS_ITEM (item));
	return item;
}

void
part_item_create_canvas_items_for_preview (GooCanvasGroup *group,
	LibraryPart *library_part)
{
	g_return_if_fail (group != NULL);
	g_return_if_fail (library_part != NULL);

	create_canvas_items (group, library_part);
}

static void
create_canvas_items (GooCanvasGroup *group, LibraryPart *library_part)
{
	GooCanvasItem   *item;
	GooCanvasPoints *points;
	GSList		*objects;
	LibrarySymbol	*symbol;
	SymbolObject	*object;
	gdouble          height, width;
	GooCanvasBounds  bounds, group_bounds = {0,0,0,0};


	g_return_if_fail (group != NULL);
	g_return_if_fail (library_part != NULL);

	symbol = library_get_symbol (library_part->symbol_name);
	if (symbol ==  NULL) {
		g_warning ("Couldn't find the requested symbol %s for part %s in "
		           "library.\n",
			       library_part->symbol_name,
			       library_part->name);
		return;
	}

	for (objects = symbol->symbol_objects; objects; objects = objects->next) {
		object = (SymbolObject *)(objects->data);
		switch (object->type) {
			case SYMBOL_OBJECT_LINE:
				points = object->u.uline.line;
				item = goo_canvas_polyline_new (GOO_CANVAS_ITEM (group), 
			        FALSE,
			        0,
			       	"points", points,
			        "stroke-color", NORMAL_COLOR,
					"line-width", 0.5,
					NULL);
				if (object->u.uline.spline) {
					g_object_set (item, 
				              "smooth", TRUE, 
				              "spline_steps", 5, 
				              NULL);
				}
				break;
			case SYMBOL_OBJECT_ARC:
				item = goo_canvas_ellipse_new (GOO_CANVAS_ITEM (group), 
					(object->u.arc.x2 + object->u.arc.x1) / 2.0,
			        (object->u.arc.y1 + object->u.arc.y2) / 2.0,
			        (object->u.arc.x2 - object->u.arc.x1) / 2.0,
			        (object->u.arc.y1 - object->u.arc.y2) / 2.0,
			        "stroke-color", NORMAL_COLOR,
			        "line_width", 1.0,
			        NULL);
				break;
			case SYMBOL_OBJECT_TEXT:
				item = goo_canvas_text_new (GOO_CANVAS_ITEM (group), 
			        object->u.text.str, 
			     	(double) object->u.text.x, 
			        (double) object->u.text.y, 
			        -1,
			        GOO_CANVAS_ANCHOR_NORTH_EAST, 
			        "fill_color", LABEL_COLOR,
			        "font", "Sans 8", 
			        NULL);
			break;
			default:
				g_warning ("Unknown symbol object.\n");
				continue;
		}
		goo_canvas_item_get_bounds (item, &bounds);
		if (group_bounds.x1 > bounds.x1) group_bounds.x1 = bounds.x1;
		if (group_bounds.x2 < bounds.x2) group_bounds.x2 = bounds.x2;
		if (group_bounds.y1 > bounds.y1) group_bounds.y1 = bounds.y1;
		if (group_bounds.y2 < bounds.y2) group_bounds.y2 = bounds.y2;
		
	}
	
	g_object_get (group,
	              "width", &width,
	              "height", &height,
	              NULL);
	width = group_bounds.x2 - group_bounds.x1;
	height = group_bounds.y2 - group_bounds.y1;
	
	g_object_set (group,
	              "width", width,
	              "height", height,
	              NULL);
	
	g_slist_free_full (objects, g_object_unref);
}

static void
create_canvas_labels (PartItem *item, Part *part)
{
	GooCanvasItem *canvas_item;
	GSList *list, *item_list;
	GooCanvasGroup *group;

	g_return_if_fail (item != NULL);
	g_return_if_fail (IS_PART_ITEM (item));
	g_return_if_fail (part != NULL);
	g_return_if_fail (IS_PART (part));

	group = GOO_CANVAS_GROUP (item->priv->label_group);
	item_list = NULL;

	for (list = part_get_labels (part); list; list = list->next) {
		PartLabel *label = list->data;
		char *text;

		text = part_property_expand_macros (part, label->text);

		canvas_item = goo_canvas_text_new (GOO_CANVAS_ITEM (group),
		                text,
		                (double) label->pos.x,
		                (double) label->pos.y,
		                0,
		                GOO_CANVAS_ANCHOR_SOUTH_WEST,
		                "fill_color", LABEL_COLOR,
		                "font", "Sans 8",
		                NULL);

		item_list = g_slist_prepend (item_list, canvas_item);
		g_free (text);
	}
	g_slist_free_full (list, g_object_unref);

	item_list = g_slist_reverse (item_list);
	part_item_set_label_items (item, item_list);
}


static void
create_canvas_label_nodes (PartItem *item, Part *part)
{
	GooCanvasItem *canvas_item;
	GSList *item_list;
	GooCanvasItem *group;
	Pin *pins;
	int num_pins, i;
	Coords p1, p2;
	GooCanvasAnchorType anchor;
	
	g_return_if_fail (item != NULL);
	g_return_if_fail (IS_PART_ITEM (item));
	g_return_if_fail (part != NULL);
	g_return_if_fail (IS_PART (part));

	num_pins = part_get_num_pins (part);
	pins = part_get_pins (part);
	group = item->priv->node_group;
	item_list = NULL;

	get_cached_bounds (item, &p1, &p2);

	switch (part_get_rotation (part)) {
		case 0:
			anchor = GOO_CANVAS_ANCHOR_SOUTH_WEST;
		break;
		case 90:
			anchor = GOO_CANVAS_ANCHOR_NORTH_WEST;
		break;
		case 180:
			anchor = GOO_CANVAS_ANCHOR_NORTH_EAST;
		break;
		case 270:
			anchor = GOO_CANVAS_ANCHOR_SOUTH_EAST;
		break;
		default:
			anchor = GOO_CANVAS_ANCHOR_SOUTH_WEST;
	}

	for (i = 0; i < num_pins; i++) {
		int x, y;
		char *text;
		x = pins[i].offset.x;
		y = pins[i].offset.y;

		text = g_strdup_printf ("%d", pins[i].node_nr);
		canvas_item = goo_canvas_text_new (GOO_CANVAS_ITEM (group),
		                text,
		                (double) x,
		                (double) y,
		                0,
		                anchor,
		                "fill_color", "black",
		                "font", "Sans 8",
		                NULL);
		// Shift slightly the label for a Voltmeter
		if (i == 0) goo_canvas_item_translate (canvas_item, -15.0, -10.0);

		item_list = g_slist_prepend (item_list, canvas_item);
		g_free (text);
	}
	item_list = g_slist_reverse (item_list);
	item->priv->label_nodes = item_list;
}


// This is called when the part data was moved. Update the view accordingly.
static void
part_moved_callback (ItemData *data, Coords *pos, SheetItem *item)
{

}

static void
part_item_place (SheetItem *item, Sheet *sheet)
{
	g_signal_connect (G_OBJECT (item), "button_press_event",
	    G_CALLBACK (sheet_item_event), sheet);
		
	g_signal_connect (G_OBJECT (item), "button_release_event",
	    G_CALLBACK (sheet_item_event), sheet);
			
	g_signal_connect (G_OBJECT (item), "motion_notify_event",
	    G_CALLBACK (sheet_item_event), sheet);
		
	g_signal_connect (G_OBJECT (item), "key_press_event",
	    G_CALLBACK (sheet_item_event), sheet);
		
	g_signal_connect (G_OBJECT (item), "double_clicked",
            G_CALLBACK (edit_properties), item);
}

static void
part_item_place_ghost (SheetItem *item, Sheet *sheet)
{
//	part_item_signal_connect_placed (PART_ITEM (item));
}

void
part_item_show_node_labels (PartItem *part, gboolean show)
{
	PartItemPriv *priv;

	priv = part->priv;

	if (show)
		g_object_set (priv->node_group, 
				      "visibility", GOO_CANVAS_ITEM_VISIBLE, 
		              NULL);
	else
		g_object_set (priv->node_group, 
				      "visibility", GOO_CANVAS_ITEM_INVISIBLE, 
		              NULL);
}

static GooCanvasAnchorType 
part_item_get_anchor_from_part (Part *part)
{
	int anchor_h, anchor_v;
	int angle;
	IDFlip flip;

	flip = part_get_flip (part);
	angle = part_get_rotation (part);

	switch (angle) {
	case 0:
		anchor_h = ANCHOR_SOUTH;
		anchor_v = ANCHOR_WEST;
		break;
	case 90:
		anchor_h = ANCHOR_NORTH;
		anchor_v = ANCHOR_WEST;
		// Invert Rotation 
		if (flip & ID_FLIP_HORIZ)
			flip = ID_FLIP_VERT;
		else if (flip & ID_FLIP_VERT)
			flip = ID_FLIP_HORIZ;
		break;
	}

	if (flip & ID_FLIP_HORIZ) {
		anchor_v = ANCHOR_EAST;
	}
	if (flip & ID_FLIP_VERT) {
		anchor_h = ANCHOR_NORTH;
	}

	if ((anchor_v == ANCHOR_EAST) && (anchor_h == ANCHOR_NORTH))
		return GOO_CANVAS_ANCHOR_NORTH_EAST;
	if ((anchor_v == ANCHOR_WEST) && (anchor_h == ANCHOR_NORTH))
		return GOO_CANVAS_ANCHOR_NORTH_WEST;
	if ((anchor_v == ANCHOR_WEST) && (anchor_h == ANCHOR_SOUTH))
		return GOO_CANVAS_ANCHOR_SOUTH_WEST;

	return GOO_CANVAS_ANCHOR_SOUTH_EAST;
}
