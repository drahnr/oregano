/*
 * part-item.c
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

#include <glade/glade.h>
#include <math.h>
#include <string.h>
#include "main.h"
#include "schematic-view.h"
#include "sheet-private.h"
#include "sheet-item.h"
#include "part-item.h"
#include "part-private.h"
#include "part-property.h"
#include "load-library.h"
#include "load-common.h"
#include "part-label.h"
#include "stock.h"
#include "dialogs.h"


#define NORMAL_COLOR "red"
#define LABEL_COLOR "dark cyan"
#define SELECTED_COLOR "green"
#define O_DEBUG 0

static void part_item_class_init(PartItemClass *klass);
static void part_item_init(PartItem *gspart);
static void part_item_destroy(GtkObject *object);
static void part_item_moved(SheetItem *sheet_item);

static void edit_properties (SheetItem *object);

static void selection_changed (PartItem *item, gboolean select,
	gpointer user_data);
static int select_idle_callback (PartItem *item);
static int deselect_idle_callback (PartItem *item);

static void update_canvas_labels (PartItem *part_item);

static gboolean is_in_area (SheetItem *object, SheetPos *p1, SheetPos *p2);
inline static void get_cached_bounds (PartItem *item, SheetPos *p1,
	SheetPos *p2);
static void show_labels (SheetItem *sheet_item, gboolean show);
static void part_item_paste (SchematicView *sv, ItemData *data);

static void part_rotated_callback (ItemData *data, int angle, SheetItem *item);
static void part_flipped_callback (ItemData *data, gboolean horizontal,
	SheetItem *sheet_item);

static void part_moved_callback (ItemData *data, SheetPos *pos,
	SheetItem *item);

static void part_item_place (SheetItem *item, SchematicView *sv);
static void part_item_place_ghost (SheetItem *item, SchematicView *sv);

static void create_canvas_items (GnomeCanvasGroup *group,
	LibraryPart *library_part);
static void create_canvas_labels (PartItem *item, Part *part);
static void create_canvas_label_nodes (PartItem *item, Part *part);

static void part_item_get_property (GObject *object, guint prop_id,
	GValue *value, GParamSpec *spec);
static void part_item_set_property (GObject *object, guint prop_id,
	const GValue *value, GParamSpec *spec);

enum {
	ARG_0,
	ARG_NAME,
	ARG_SYMNAME,
	ARG_LIBNAME,
	ARG_REFDES,
	ARG_TEMPLATE,
	ARG_MODEL
};

struct _PartItemPriv {
	guint cache_valid : 1;
	guint highlight : 1;

	GnomeCanvasGroup *label_group;
	GSList *label_items;

	GnomeCanvasGroup *node_group;
	GSList *label_nodes;

	/*
	 * Cached bounding box. This is used to make
	 * the rubberband selection a bit faster.
	 */
	SheetPos bbox_start;
	SheetPos bbox_end;
};

typedef struct {
	GtkDialog *dialog;
	PartItem  *part_item;

	/* List of GtkEntry's */
	GList *widgets;
} PartPropDialog;

static PartPropDialog *prop_dialog = NULL;
static SheetItemClass *parent_class = NULL;

static const char *part_item_context_menu =
"<ui>"
"  <popup name='ItemMenu'>"
"    <menuitem action='ObjectProperties'/>"
"  </popup>"
"</ui>";

enum {
	ANCHOR_NORTH,
	ANCHOR_SOUTH,
	ANCHOR_WEST,
	ANCHOR_EAST
};

GtkAnchorType part_item_get_anchor_from_part (Part *part)
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
			/* Invert Rotation */
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
		return GTK_ANCHOR_NORTH_EAST;
	if ((anchor_v == ANCHOR_WEST) && (anchor_h == ANCHOR_NORTH))
		return GTK_ANCHOR_NORTH_WEST;
	if ((anchor_v == ANCHOR_WEST) && (anchor_h == ANCHOR_SOUTH))
		return GTK_ANCHOR_SOUTH_WEST;
	if ((anchor_v == ANCHOR_EAST) && (anchor_h == ANCHOR_SOUTH))
		return GTK_ANCHOR_SOUTH_EAST;

	return GTK_ANCHOR_SOUTH_EAST;
}

GType
part_item_get_type ()
{
	static GType part_item_type = 0;

	if (!part_item_type) {
		static const GTypeInfo part_item_info = {
			sizeof(PartItemClass),
			NULL,
			NULL,
			(GClassInitFunc) part_item_class_init,
			NULL,
			NULL,
			sizeof (PartItem),
			0,
			(GInstanceInitFunc) part_item_init,
			NULL
		};
		part_item_type = g_type_register_static(TYPE_SHEET_ITEM,
			"PartItem", &part_item_info, 0);
	}
	return part_item_type;
}

static void
part_item_class_init (PartItemClass *part_item_class)
{
	GObjectClass *object_class;
	GtkObjectClass *gtk_object_class;
	SheetItemClass *sheet_item_class;

	object_class = G_OBJECT_CLASS(part_item_class);
	gtk_object_class = GTK_OBJECT_CLASS(part_item_class);
	sheet_item_class = SHEET_ITEM_CLASS(part_item_class);
	parent_class = g_type_class_peek_parent(part_item_class);

	object_class->set_property = part_item_set_property;
	object_class->get_property = part_item_get_property;

	g_object_class_install_property	(
		object_class,
		ARG_NAME,
		g_param_spec_pointer ("name",
			"PartItem::name",
			"name",
			G_PARAM_READABLE | G_PARAM_WRITABLE));
	g_object_class_install_property	(
		object_class,
		ARG_SYMNAME,
		g_param_spec_pointer ("symbol_name",
			"PartItem::symbol_name",
			"symbol name",
			G_PARAM_READABLE | G_PARAM_WRITABLE));
	g_object_class_install_property	(
		object_class,
		ARG_REFDES,
		g_param_spec_pointer ("refdes",
			"PartItem::refdes",
			"refdes",
			G_PARAM_READABLE | G_PARAM_WRITABLE));
	g_object_class_install_property	(
		object_class,
		ARG_LIBNAME,
		g_param_spec_pointer ("library_name",
			"PartItem::library_name",
			"library_name",
			G_PARAM_READABLE | G_PARAM_WRITABLE));
	g_object_class_install_property	(
		object_class,
		ARG_TEMPLATE,
		g_param_spec_pointer ("template",
			"PartItem::template",
			"template",
			G_PARAM_READABLE | G_PARAM_WRITABLE));
	g_object_class_install_property	(
		object_class,
		ARG_MODEL,
		g_param_spec_pointer ("model",
			"PartItem::model",
			"model",
			G_PARAM_READABLE | G_PARAM_WRITABLE));

//	object_class->dispose = part_item_dispose;
//	object_class->finalize = item_data_finalize;

	gtk_object_class->destroy = part_item_destroy;

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

	priv = g_new0 (PartItemPriv, 1);

	priv->highlight = FALSE;
	priv->cache_valid = FALSE;

	item->priv = priv;

	sheet_item_add_menu (SHEET_ITEM (item), part_item_context_menu);
}

static void
part_item_set_property (GObject *object, guint propety_id, const GValue *value,
	GParamSpec *pspec)
{
	PartItem *part_item;
	PartItemPriv *priv;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_PART_ITEM(object));

	part_item = PART_ITEM(object);
	priv = part_item->priv;

	switch (propety_id) {
	default:
		g_warning ("PartItem: Invalid argument.\n");

	}
}

static void
part_item_get_property (GObject *object, guint propety_id, GValue *value,
	GParamSpec *pspec)
{
	PartItem *part_item;
	PartItemPriv *priv;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_PART_ITEM (object));

	part_item = PART_ITEM (object);
	priv = part_item->priv;

	switch (propety_id) {
	default:
		pspec->value_type = G_TYPE_INVALID;
		break;
	}
}

static void
part_item_destroy(GtkObject *object)
{
	PartItem *item;
	PartItemPriv *priv;
	/*
	 * Unused variable
	Part *part;
	*/
	ArtPoint *old;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_PART_ITEM (object));

	/*
	 * Free the stored coordinate that lets us rotate circles.
	 */
	old = g_object_get_data(G_OBJECT(object), "hack");
	if (old)
		g_free (old);

	item = PART_ITEM (object);
	priv = item->priv;

	if (priv) {
		if (priv->label_group) {
			/* GnomeCanvasGroups utiliza GtkObject todavia */
			gtk_object_destroy(GTK_OBJECT(priv->label_group));
			priv->label_group = NULL;
		}

		g_free (priv);
		item->priv = NULL;
	}

	if (GTK_OBJECT_CLASS (parent_class)->destroy) {
		GTK_OBJECT_CLASS (parent_class)->destroy(object);
	}
}

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

/*
 * part_item_moved
 *
 * "moved" signal handler. Invalidates the bounding box cache.
 */
static void
part_item_moved (SheetItem *sheet_item)
{
	PartItem *part_item;

	part_item = PART_ITEM (sheet_item);
	part_item->priv->cache_valid = FALSE;
}

PartItem *
part_item_new (Sheet *sheet, Part *part)
{
	PartItem *item;
	PartItemPriv *priv;

	g_return_val_if_fail(sheet != NULL, NULL);
	g_return_val_if_fail(IS_SHEET (sheet), NULL);
	g_return_val_if_fail(part != NULL, NULL);
	g_return_val_if_fail(IS_PART (part), NULL);

	item = PART_ITEM(gnome_canvas_item_new(
		sheet->object_group,
		TYPE_PART_ITEM,
		"data", part,
		NULL));

	priv = item->priv;

	priv->label_group = GNOME_CANVAS_GROUP(gnome_canvas_item_new(
		GNOME_CANVAS_GROUP(item),
		GNOME_TYPE_CANVAS_GROUP,
		"x", 0.0,
		"y", 0.0,
		NULL));

	priv->node_group = GNOME_CANVAS_GROUP(gnome_canvas_item_new(
		GNOME_CANVAS_GROUP(item),
		GNOME_TYPE_CANVAS_GROUP,
		"x", 0.0,
		"y", 0.0,
		NULL));
		
	gnome_canvas_item_hide (GNOME_CANVAS_ITEM (priv->node_group));

	g_signal_connect_object(G_OBJECT(part),
		"rotated",
		G_CALLBACK (part_rotated_callback),
		G_OBJECT(item),
		0);
	g_signal_connect_object(G_OBJECT(part),
		"flipped",
		G_CALLBACK(part_flipped_callback),
		G_OBJECT(item),
		0);
	g_signal_connect_object(G_OBJECT(part),
		"moved",
		G_CALLBACK(part_moved_callback),
		G_OBJECT(item),
		0);

	return item;
}

static void
update_canvas_labels (PartItem *item)
{
	PartItemPriv *priv;
	Part *part;
	GSList *labels, *label_items;
	GnomeCanvasItem *canvas_item;

	g_return_if_fail(item != NULL);
	g_return_if_fail(IS_PART_ITEM (item));

	priv = item->priv;
	part = PART(sheet_item_get_data(SHEET_ITEM(item)));

	label_items = priv->label_items;

	/* Pone las etiquetas de Item */
	for (labels = part_get_labels (part); labels;
	     labels = labels->next, label_items = label_items->next) {
		char *text;
		PartLabel *label = (PartLabel*) labels->data;
		g_assert (label_items != NULL);
		canvas_item = label_items->data;

		text = part_property_expand_macros (part, label->text);
		gnome_canvas_item_set (canvas_item, "text", text, NULL);
		g_free (text);
	}
}

void
part_item_update_node_label (PartItem *item)
{
	PartItemPriv *priv;
	Part *part;
	GSList *labels;
	GnomeCanvasItem *canvas_item;
	Pin *pins;
	gint num_pins, i;


	g_return_if_fail(item != NULL);
	g_return_if_fail(IS_PART_ITEM (item));
	priv = item->priv;
	part = PART(sheet_item_get_data(SHEET_ITEM(item)));

	g_return_if_fail( IS_PART(part) );

	/* Pone las etiquetas de los nodos */
	num_pins = part_get_num_pins(part);
	pins = part_get_pins(part);
	labels = priv->label_nodes;
	for (i=0; i<num_pins; i++, labels=labels->next) {
		int x, y;
		char *txt;
		x = pins[i].offset.x;
		y = pins[i].offset.y;

		txt = g_strdup_printf("%d", pins[i].node_nr);
		canvas_item = labels->data;
		gnome_canvas_item_set (canvas_item, "text", txt, NULL);

		g_free(txt);
	}
}

static void 
prop_dialog_destroy (GtkWidget *widget, PartPropDialog *prop_dialog)
{
	g_free (prop_dialog);
}

static void
prop_dialog_response(GtkWidget *dialog, gint response,
	PartPropDialog *prop_dialog)
{
	GSList		 *props;
	GList		   *widget;
	Property	 *prop;
	PartItem	 *item;
	PartItemPriv *priv;
	Part		 *part;
	gchar *prop_name;
	const gchar *prop_value;
	GtkWidget *w;

	item = prop_dialog->part_item;

	priv = item->priv;
	part = PART (sheet_item_get_data (SHEET_ITEM (item)));

	for (widget = prop_dialog->widgets; widget;
	     widget = widget->next) {
		w = widget->data;

		prop_name = gtk_object_get_user_data (GTK_OBJECT (w));
		prop_value = gtk_entry_get_text (GTK_ENTRY (w));

		for (props = part_get_properties (part); props; props = props->next) {
			prop = props->data;
			if (g_strcasecmp (prop->name, prop_name) == 0) {
				if (prop->value) g_free (prop->value);
				prop->value = g_strdup (prop_value);
			}
		}
		g_free (prop_name);
	}

	update_canvas_labels (item);
}

static void
edit_properties_punta (PartItem *item)
{
	GSList *properties;
	PartItemPriv *priv;
	Part *part;
	char *msg;
	GladeXML *gui;
	GtkRadioButton *radio_v, *radio_c;
	GtkRadioButton *ac_r, *ac_m, *ac_i, *ac_p;
	GtkCheckButton *chk_db;
	gint response;

	priv = item->priv;
	part = PART (sheet_item_get_data (SHEET_ITEM (item)));

	if (!g_file_test(
		OREGANO_GLADEDIR "/clamp-properties-dialog.glade2",
		G_FILE_TEST_EXISTS)) {
		msg = g_strdup_printf (
			_("The file %s could not be found. You might need to reinstall Oregano to fix this."),
			OREGANO_GLADEDIR "/clamp-properties-dialog.glade2");
		oregano_error_with_title (_("Could not create part properties dialog."), msg);
		g_free (msg);
		return;
	}

	gui = glade_xml_new (OREGANO_GLADEDIR "/clamp-properties-dialog.glade2",
		NULL, NULL);
	if (!gui) {
		oregano_error (_("Could not create part properties dialog."));
		return;
	}

	prop_dialog = g_new0 (PartPropDialog, 1);

	prop_dialog->part_item = item;

	prop_dialog->dialog = GTK_DIALOG (glade_xml_get_widget (gui, "clamp-properties-dialog"));
	gtk_dialog_set_has_separator (prop_dialog->dialog, FALSE);

	radio_v = GTK_RADIO_BUTTON (glade_xml_get_widget (gui, "radio_v"));
	radio_c = GTK_RADIO_BUTTON (glade_xml_get_widget (gui, "radio_c"));

	/* FIXME : Desactivada mientras se trabaja en el backend */
	gtk_widget_set_sensitive (GTK_WIDGET (radio_c), FALSE);

	ac_r = GTK_RADIO_BUTTON (glade_xml_get_widget (gui, "radio_r"));
	ac_m = GTK_RADIO_BUTTON (glade_xml_get_widget (gui, "radio_m"));
	ac_p = GTK_RADIO_BUTTON (glade_xml_get_widget (gui, "radio_p"));
	ac_i = GTK_RADIO_BUTTON (glade_xml_get_widget (gui, "radio_i"));

	chk_db = GTK_CHECK_BUTTON (glade_xml_get_widget (gui, "check_db"));
	
	/* Setup GUI from properties */
	for (properties = part_get_properties (part); properties;
		properties = properties->next) {
		Property *prop;
		prop = properties->data;
		if (prop->name) {
			if (!g_strcasecmp (prop->name, "internal"))
				continue;

			if (!g_strcasecmp (prop->name, "type")) {
				if (!g_strcasecmp (prop->value, "v")) {
					gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio_v), TRUE);
				} else {
					gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio_c), TRUE);
				}
			} else if (!g_strcasecmp (prop->name, "ac_type")) {
				if (!g_strcasecmp (prop->value, "m")) {
					gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (ac_m), TRUE);
				} else if (!g_strcasecmp (prop->value, "i")) {
					gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (ac_i), TRUE);
				} else if (!g_strcasecmp (prop->value, "p")) {
					gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (ac_p), TRUE);
				} else if (!g_strcasecmp (prop->value, "r")) {
					gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (ac_r), TRUE);
				}
			} else if (!g_strcasecmp (prop->name, "ac_db")) {
				if (!g_strcasecmp (prop->value, "true"))
					gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (chk_db), TRUE);
			}
		}
	}

	response = gtk_dialog_run(prop_dialog->dialog);

	/* Save properties from GUI */
	for (properties = part_get_properties (part); properties;
		properties = properties->next) {
		Property *prop;
		prop = properties->data;

		if (prop->name) {
			if (!g_strcasecmp (prop->name, "internal"))
				continue;
	
			if (!g_strcasecmp (prop->name, "type")) {
				g_free (prop->value);
				if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (radio_v))) {
					prop->value = g_strdup ("v");
				} else {
					prop->value = g_strdup ("i");
				}
			} else if (!g_strcasecmp (prop->name, "ac_type")) {
				g_free (prop->value);
				if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (ac_m))) {
					prop->value = g_strdup ("m");
				} else if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (ac_i))) {
					prop->value = g_strdup ("i");
				} else if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (ac_p))) {
					prop->value = g_strdup ("p");
				} else if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (ac_r))) {
					prop->value = g_strdup ("r");
				}
			} else if (!g_strcasecmp (prop->name, "ac_db")) {
				g_free (prop->value);
				if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (chk_db)))
					prop->value = g_strdup ("true");
				else
					prop->value = g_strdup ("false");
			}
		}
	}

	gtk_widget_destroy (GTK_WIDGET (prop_dialog->dialog));
}

static void
edit_properties (SheetItem *object)
{
	GSList *properties;
	PartItem *item;
	PartItemPriv *priv;
	Part *part;
	char *internal, *msg;
	GladeXML *gui;
	GtkTable *prop_table;
	GtkNotebook *notebook;
	gint response, y = 0;
	gboolean has_model;
	gchar *model_name = NULL;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_PART_ITEM (object));

	item = PART_ITEM (object);
	priv = item->priv;
	part = PART (sheet_item_get_data (SHEET_ITEM (item)));

	internal = part_get_property (part, "internal");
	if (internal) {
		if (g_strcasecmp (internal, "ground") == 0) {
			g_free (internal);
			return;
		}
		/* Hack!! */
		if (g_strcasecmp (internal, "punta") == 0) {
			edit_properties_punta (item);
			return;
		}
	}

	g_free (internal);

	if (!g_file_test(
		OREGANO_GLADEDIR "/part-properties-dialog.glade2",
		G_FILE_TEST_EXISTS)) {
		msg = g_strdup_printf (
			_("The file %s could not be found. You might need to reinstall Oregano to fix this."),
			OREGANO_GLADEDIR "/part-properties-dialog.glade2");
		oregano_error_with_title (_("Could not create part properties dialog."), msg);
		g_free (msg);
		return;
	}

	gui = glade_xml_new (OREGANO_GLADEDIR "/part-properties-dialog.glade2",
		NULL, NULL);
	if (!gui) {
		oregano_error (_("Could not create part properties dialog."));
		return;
	}

	prop_dialog = g_new0 (PartPropDialog, 1);

	prop_dialog->part_item = item;

	prop_dialog->dialog = GTK_DIALOG ( glade_xml_get_widget (gui, "part-properties-dialog"));

	prop_table = GTK_TABLE (glade_xml_get_widget (gui, "prop_table"));
	notebook  = GTK_NOTEBOOK (glade_xml_get_widget (gui, "notebook"));

	g_signal_connect (prop_dialog->dialog, "destroy",
		(GCallback) prop_dialog_destroy,
		prop_dialog
	);

	prop_dialog->widgets = NULL;
	has_model = FALSE;
	for (properties = part_get_properties (part); properties;
		properties = properties->next) {
		Property *prop;
		prop = properties->data;
		if (prop->name) {
			GtkWidget *entry;
			GtkWidget *label;
			if (!g_strcasecmp (prop->name, "internal"))
				continue;

			if (!g_strcasecmp (prop->name, "model")) {
				has_model = TRUE;
				model_name = g_strdup (prop->value);
			}

			label = gtk_label_new (prop->name);
			entry = gtk_entry_new ();
			gtk_entry_set_text (GTK_ENTRY (entry), prop->value);
			gtk_object_set_user_data (GTK_OBJECT (entry), g_strdup (prop->name));

			gtk_table_attach (
				prop_table, label,
				0, 1, y, y+1,
				GTK_FILL|GTK_SHRINK,
				GTK_FILL|GTK_SHRINK,
				8, 8
			);
			gtk_table_attach (
				prop_table, entry,
				1, 2, y, y+1,
				GTK_EXPAND|GTK_FILL,
				GTK_FILL|GTK_SHRINK,
				8, 8
			);
			y++;
			gtk_widget_show (label);
			gtk_widget_show (entry);

			prop_dialog->widgets = g_list_prepend (prop_dialog->widgets, entry);
		}
	}

	if (!has_model) {
		gtk_notebook_remove_page (notebook, 1); 
	} else {
		GtkTextBuffer *txtbuffer;
		GtkTextView *txtmodel;
		gchar *filename, *str;
		GError *read_error = NULL;

		txtmodel = GTK_TEXT_VIEW (glade_xml_get_widget (gui, "txtmodel"));
		txtbuffer = gtk_text_buffer_new (NULL);

		filename = g_strdup_printf ("%s/%s.model", OREGANO_MODELDIR, model_name);
		if (g_file_get_contents (filename, &str, NULL, &read_error)) {
			gtk_text_buffer_set_text (txtbuffer, str, -1);
			g_free (str);
		} else {
			gtk_text_buffer_set_text (txtbuffer, read_error->message, -1);
			g_error_free (read_error);
		}

		g_free (filename);
		g_free (model_name);

		gtk_text_view_set_buffer (txtmodel, txtbuffer);
	}

	gtk_dialog_set_default_response (prop_dialog->dialog, 1);

	response = gtk_dialog_run(prop_dialog->dialog);

	prop_dialog_response (GTK_WIDGET (prop_dialog->dialog), response, prop_dialog);

	gtk_widget_destroy (GTK_WIDGET (prop_dialog->dialog));
}

/* Matrix Multiplication
   Tried art_affine_multiply, but that didn't work */
static void matrix_mult(double* a,double* b,double* c){
  c[0]=a[0]*b[0]+a[1]*b[2];
  c[2]=a[2]*b[0]+a[3]*b[2];
  c[1]=a[0]*b[1]+a[1]*b[3];
  c[3]=a[2]*b[1]+a[3]*b[3];
}

static void
part_rotated_callback (ItemData *data, int angle, SheetItem *sheet_item)
{
	double affine[6];
	double affine_scale[6];
	double affine_rotate[6];

	GList *list;
	GSList *label_items;
	GtkAnchorType anchor;
	GnomeCanvasGroup *group;
	GnomeCanvasItem *canvas_item;
	PartItem *item;
	PartItemPriv *priv;
	Part *part;

	g_return_if_fail (sheet_item != NULL);
	g_return_if_fail (IS_PART_ITEM (sheet_item));

	item = PART_ITEM (sheet_item);
	group = GNOME_CANVAS_GROUP (item);
	part = PART(data);

	priv = item->priv;

	if (angle!=0) {
	// check if the part is flipped
	  if ((part->priv->flip & 1) && (part->priv->flip & 2)) {
	    // Nothing to do in this case
	    art_affine_rotate (affine, angle);
	  } else {
	    if ((part->priv->flip & 1) || (part->priv->flip & 2)) {
	      // mirror over point (0,0)
	      art_affine_scale(affine_scale,-1,-1);
	      art_affine_rotate(affine_rotate,angle);
	      /* Matrix multiplication */
	      /* Don't use art_affine_multiply() here !*/
	      matrix_mult(affine_rotate,affine_scale,affine);
	      affine[4]=affine_rotate[4];
	      affine[5]=affine_rotate[5];
	    } else
	      art_affine_rotate (affine, angle);
	  }
	} else {
	  // angle==0: Nothing has changed, therefore do nothing
	  art_affine_scale(affine,1,1);
	}

	for (list = group->item_list; list; list = list->next) {
		canvas_item = GNOME_CANVAS_ITEM (list->data);

		gnome_canvas_item_affine_relative (canvas_item, affine);
	}

	/*
	 * Get the right anchor for the labels. This is needed since the
	 * canvas don't know how to rotate text and since we rotate the
	 * label_group instead of the labels directly.
	 */

	switch (part_get_rotation (part)) {
	case 0:
		anchor = GTK_ANCHOR_SOUTH_WEST;
		break;
	case 90:
		anchor = GTK_ANCHOR_NORTH_WEST;
		break;
	case 180:
		anchor = GTK_ANCHOR_NORTH_EAST;
		break;
	case 270:
		anchor = GTK_ANCHOR_SOUTH_EAST;
		break;
	default:
		anchor = GTK_ANCHOR_SOUTH_WEST;
		break;
	}


	for (label_items = priv->label_items; label_items;
	     label_items = label_items->next) {
		gnome_canvas_item_set (
			GNOME_CANVAS_ITEM (label_items->data),
			"anchor", anchor,
			NULL);
	}

	for (label_items = priv->label_nodes; label_items;
	     label_items = label_items->next) {
		gnome_canvas_item_set (
			GNOME_CANVAS_ITEM (label_items->data),
			"anchor", anchor,
			NULL);
	}

	/*
	 * Invalidate the bounding box cache.
	 */
	priv->cache_valid = FALSE;
}

static void
part_flipped_callback (ItemData *data, gboolean horizontal,
	SheetItem *sheet_item)
{
	GList *list;
	GSList *label;
	GtkAnchorType anchor;
	GnomeCanvasGroup *group;
	GnomeCanvasItem *canvas_item;
	PartItem *item;
	PartItemPriv *priv;
	Part *part;
	IDFlip flip;
	double affine[6];
	double affine_rotate[6];
	double affine_scale[6];
	int angle;

	g_return_if_fail (sheet_item != NULL);
	g_return_if_fail (IS_PART_ITEM (sheet_item));

	item = PART_ITEM (sheet_item);
	group = GNOME_CANVAS_GROUP (item);
	part = PART (data);
	flip = part_get_flip (part);

	priv = item->priv;

	if (horizontal)
		art_affine_scale (affine, -1, 1);
	else
		art_affine_scale (affine, 1, -1);

	angle=part_get_rotation(part);
	if (angle!=0) {
	  switch (angle){
	  case 90: art_affine_rotate(affine_rotate,180);
	    memcpy(affine_scale,affine,6*sizeof(double));
	    break;
	  case 180: //don't do a thing
	    art_affine_rotate(affine_rotate,0);
	    memcpy(affine_scale,affine,6*sizeof(double));
	    break;
	  case 270: art_affine_rotate(affine_rotate,0);
	    // only to reset the memory of affine_scale, just a hack
	    memcpy(affine_scale,affine,6*sizeof(double));
	    // switch the flipping from horizontal to vertical and vice versa
	    affine_scale[0]=affine[3];
	    affine_scale[3]=affine[0];
	  }
	  matrix_mult(affine_scale,affine_rotate,affine);
	}


	for (list = group->item_list; list; list = list->next) {
		canvas_item = GNOME_CANVAS_ITEM (list->data);

		gnome_canvas_item_affine_relative (canvas_item, affine);
	}

	anchor = part_item_get_anchor_from_part (part);

	/*if (horizontal) {
		if (flip & ID_FLIP_HORIZ)
			anchor = GTK_ANCHOR_SOUTH_EAST;
		else
			anchor = GTK_ANCHOR_SOUTH_WEST;
	} else {
		if (flip & ID_FLIP_VERT)
			anchor = GTK_ANCHOR_NORTH_WEST;
		else
			anchor = GTK_ANCHOR_NORTH_EAST;
	}
	if ((flip & ID_FLIP_HORIZ) && (flip & ID_FLIP_VERT)) {
		anchor = GTK_ANCHOR_NORTH_EAST;
	}*/

	for (label = item->priv->label_items; label; label = label->next) {
		gnome_canvas_item_set (
			GNOME_CANVAS_ITEM (label->data),
			"anchor", anchor,
			NULL);
	}

	/*
	 * Invalidate the bounding box cache.
	 */
	priv->cache_valid = FALSE;
}

/*
  Wherefore this function?
  Formerly, the above defined callback functions were called after
  reading data from file or from clipboard, in both cases there was neither
  a change in flip nor rotate, but the above functions only work for
  data which has changed. The "Not Changed, Just Redraw" was missing here.
 */
static void
part_arrange_canvas_item (ItemData *data, SheetItem *sheet_item)
{
  double affine[6];
	double affine_scale[6];
	double affine_rotate[6];
	GList *list;
	GSList *label_items;
	GtkAnchorType anchor;
	GnomeCanvasGroup *group;
	GnomeCanvasItem *canvas_item;
	PartItem *item;
	PartItemPriv *priv;
	Part *part;
	int angle;

	g_return_if_fail (sheet_item != NULL);
	g_return_if_fail (IS_PART_ITEM (sheet_item));

	item = PART_ITEM (sheet_item);
	group = GNOME_CANVAS_GROUP (item);
	part = PART(data);

	priv = item->priv;
	
	angle=part_get_rotation(part);

	if (angle!=0) {
	  if ((part->priv->flip & ID_FLIP_HORIZ)
	      && (part->priv->flip & ID_FLIP_VERT)) {
	    art_affine_rotate(affine,angle+180);
	  } else { if ((part->priv->flip & ID_FLIP_HORIZ)
		       || (part->priv->flip & ID_FLIP_VERT)) {
	      if (part->priv->flip & ID_FLIP_HORIZ)
		art_affine_scale(affine_scale,-1,1);
	      else
		art_affine_scale(affine_scale,1,-1);

	      art_affine_rotate(affine_rotate,angle);

	      matrix_mult(affine_rotate,affine_scale,affine);
	      affine[4]=affine_rotate[4];
	      affine[5]=affine_rotate[5];
	    } else
	      art_affine_rotate (affine, angle);
	  }
	} else {
	  art_affine_scale(affine,1,1); //default
	  if ((part->priv->flip & ID_FLIP_HORIZ)
	      && (part->priv->flip & ID_FLIP_VERT))
	    art_affine_scale(affine,-1,-1);
	  if ((part->priv->flip & ID_FLIP_HORIZ)
	      && !(part->priv->flip & ID_FLIP_VERT))
	    art_affine_scale(affine,-1,1);
	  if (!(part->priv->flip & ID_FLIP_HORIZ)
	      && (part->priv->flip & ID_FLIP_VERT))
	    art_affine_scale(affine,1,-1);
	}

	for (list = group->item_list; list; list = list->next) {
		canvas_item = GNOME_CANVAS_ITEM (list->data);
		gnome_canvas_item_affine_relative (canvas_item, affine);
	}

	/*
	 * Get the right anchor for the labels. This is needed since the
	 * canvas don't know how to rotate text and since we rotate the
	 * label_group instead of the labels directly.
	 */
	switch (part_get_rotation (part)) {
	case 0:
		anchor = GTK_ANCHOR_SOUTH_WEST;
		break;
	case 90:
		anchor = GTK_ANCHOR_NORTH_WEST;
		break;
	case 180:
		anchor = GTK_ANCHOR_NORTH_EAST;
		break;
	case 270:
		anchor = GTK_ANCHOR_SOUTH_EAST;
		break;
	default:
		anchor = GTK_ANCHOR_SOUTH_WEST;
		break;
	}


	for (label_items = priv->label_items; label_items;
	     label_items = label_items->next) {
		gnome_canvas_item_set (
			GNOME_CANVAS_ITEM (label_items->data),
			"anchor", anchor,
			NULL);
	}

	for (label_items = priv->label_nodes; label_items;
	     label_items = label_items->next) {
		gnome_canvas_item_set (
			GNOME_CANVAS_ITEM (label_items->data),
			"anchor", anchor,
			NULL);
	}

	/*
	 * Invalidate the bounding box cache.
	 */
	priv->cache_valid = FALSE;
}

void
part_item_signal_connect_floating_group (Sheet *sheet, SchematicView *sv)
{
	g_return_if_fail (sheet != NULL);
	g_return_if_fail (IS_SHEET (sheet));
	g_return_if_fail (sv != NULL);
	g_return_if_fail (IS_SCHEMATIC_VIEW (sv));

	sheet->state = SHEET_STATE_FLOAT_START;

	/* FIXME: clean all this mess with floating groups etc... */
	if (sheet->priv->float_handler_id != 0)
		return;

	sheet->priv->float_handler_id = g_signal_connect(
		G_OBJECT (sheet),
		"event",
		G_CALLBACK(sheet_item_floating_event),
		sv);
}

void
part_item_signal_connect_floating (PartItem *item)
{
	Sheet *sheet;

	sheet = sheet_item_get_sheet (SHEET_ITEM (item));
	sheet->state = SHEET_STATE_FLOAT_START;

	g_signal_connect (
		G_OBJECT (item),
		"double_clicked",
		G_CALLBACK(edit_properties),
		item);
}

static void
selection_changed (PartItem *item, gboolean select, gpointer user_data)
{
	g_object_ref (G_OBJECT (item));
	if (select)
		gtk_idle_add ((gpointer) select_idle_callback, item);
	else
		gtk_idle_add ((gpointer) deselect_idle_callback, item);
}

static int
select_idle_callback (PartItem *item)
{
	PartItemPriv *priv;
	GnomeCanvasItem *canvas_item;
	GList *list;

	priv = item->priv;

	if (priv->highlight) {
		g_object_unref(G_OBJECT (item));
		return FALSE;
	}

	for (list = GNOME_CANVAS_GROUP (item)->item_list; list;
	     list = list->next){
		canvas_item = GNOME_CANVAS_ITEM (list->data);
		if (GNOME_IS_CANVAS_LINE (canvas_item))
			gnome_canvas_item_set (canvas_item, "fill_color", SELECTED_COLOR, NULL);
		else if (GNOME_IS_CANVAS_ELLIPSE (canvas_item))
			gnome_canvas_item_set (canvas_item, "outline_color", SELECTED_COLOR, NULL);
		else if (GNOME_IS_CANVAS_TEXT  (canvas_item))
			gnome_canvas_item_set (canvas_item, "fill_color", SELECTED_COLOR, NULL);
	}

	priv->highlight = TRUE;

	g_object_unref(G_OBJECT(item));
	return FALSE;
}

static int
deselect_idle_callback (PartItem *item)
{
	GList *list;
	GnomeCanvasItem *canvas_item;
	PartItemPriv *priv;

	priv = item->priv;

	if (!priv->highlight) {
		g_object_unref (G_OBJECT (item));
		return FALSE;
	}

	for (list = GNOME_CANVAS_GROUP (item)->item_list; list;
	     list = list->next){
		canvas_item = GNOME_CANVAS_ITEM (list->data);
		if (GNOME_IS_CANVAS_LINE (canvas_item))
			gnome_canvas_item_set (canvas_item, "fill_color", NORMAL_COLOR, NULL);
		else if (GNOME_IS_CANVAS_ELLIPSE (canvas_item))
			gnome_canvas_item_set (canvas_item, "outline_color", NORMAL_COLOR, NULL);
		else if (GNOME_IS_CANVAS_TEXT  (canvas_item))
			gnome_canvas_item_set (canvas_item, "fill_color", LABEL_COLOR, NULL);
	}

	priv->highlight = FALSE;

	g_object_unref(G_OBJECT(item));
	return FALSE;
}

static gboolean
is_in_area (SheetItem *object, SheetPos *p1, SheetPos *p2)
{
	PartItem *item;
	SheetPos bbox_start, bbox_end;

	item = PART_ITEM (object);

	get_cached_bounds (item, &bbox_start, &bbox_end);

	if ((p1->x < bbox_start.x) &&
		(p2->x > bbox_end.x) &&
		(p1->y < bbox_start.y) &&
		(p2->y > bbox_end.y))
		return TRUE;

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
		gnome_canvas_item_show (GNOME_CANVAS_ITEM (priv->label_group));
	else
		gnome_canvas_item_hide (GNOME_CANVAS_ITEM (priv->label_group));
}

/**
 * Retrieves the bounding box. We use a caching scheme for this
 * since it's too expensive to calculate it every time we need it.
 */
inline static void
get_cached_bounds (PartItem *item, SheetPos *p1, SheetPos *p2)
{
	PartItemPriv *priv;
	priv = item->priv;

	if (!priv->cache_valid) {
		SheetPos start_pos, end_pos;
		gdouble x1, y1, x2, y2;

		/*
		 * Hide the labels, then get bounding box, then show them
		 * again.
		 */
		gnome_canvas_item_hide (GNOME_CANVAS_ITEM (priv->label_group));
		gnome_canvas_item_get_bounds (GNOME_CANVAS_ITEM (item),
			&x1, &y1, &x2, &y2);
		gnome_canvas_item_show (GNOME_CANVAS_ITEM (priv->label_group));

		start_pos.x = x1;
		start_pos.y = y1;
		end_pos.x = x2;
		end_pos.y = y2;

		priv->bbox_start = start_pos;
		priv->bbox_end = end_pos;
		priv->cache_valid = TRUE;
	}

	memcpy (p1, &priv->bbox_start, sizeof (SheetPos));
	memcpy (p2, &priv->bbox_end, sizeof (SheetPos));
}

static void
part_item_paste (SchematicView *sv, ItemData *data)
{
	g_return_if_fail (sv != NULL);
	g_return_if_fail (IS_SCHEMATIC_VIEW (sv));
	g_return_if_fail (data != NULL);
	g_return_if_fail (IS_PART (data));

	schematic_view_add_ghost_item (sv, data);
}

/**
 * FIXME: make this the default constructor for PartItem (rename to
 * part_item_new).
 */
PartItem *
part_item_new_from_part (Sheet *sheet, Part *part)
{
	Library *library;
	LibraryPart *library_part;
	PartPriv *priv;
	PartItem *item;

	priv = part->priv;

	library = priv->library;
	library_part = library_get_part (library, priv->name);

	/*
	 * Create the PartItem canvas item.
	 */
	item = part_item_new (sheet, part);

	create_canvas_items (GNOME_CANVAS_GROUP (item), library_part);
	create_canvas_labels (item, part);
	create_canvas_label_nodes(item, part);

	part_arrange_canvas_item(ITEM_DATA (part), SHEET_ITEM (item));
	return item;
}

void
part_item_create_canvas_items_for_preview (GnomeCanvasGroup *group,
	LibraryPart *library_part)
{
	g_return_if_fail (group != NULL);
	g_return_if_fail (GNOME_IS_CANVAS_GROUP (group));
	g_return_if_fail (library_part != NULL);

	create_canvas_items (group, library_part);
}

static void
create_canvas_items (GnomeCanvasGroup *group, LibraryPart *library_part)
{
	GnomeCanvasItem	  *item;
	GnomeCanvasPoints *points;
	GSList			  *objects;
	LibrarySymbol	  *symbol;
	SymbolObject	  *object;

	g_return_if_fail (group != NULL);
	g_return_if_fail (GNOME_IS_CANVAS_GROUP (group));
	g_return_if_fail (library_part != NULL);

	symbol = library_get_symbol (library_part->symbol_name);
	if (symbol ==  NULL){
		g_warning ("Couldn't find the requested symbol %s for part %s in library.\n",
			library_part->symbol_name,
			library_part->name);
		return;
	}

	for (objects = symbol->symbol_objects; objects;
	     objects = objects->next) {
		object = (SymbolObject *)(objects->data);
		switch (object->type){
		case SYMBOL_OBJECT_LINE:
			points = object->u.uline.line;
			item = gnome_canvas_item_new (
				group,
				gnome_canvas_line_get_type (),
				"points", points,
				"fill_color", NORMAL_COLOR,
				"width_pixels", 0,
				"cap_style", GDK_CAP_BUTT,
				NULL);
			if (object->u.uline.spline) {
				gnome_canvas_item_set (
					item,
					"smooth", TRUE,
					"spline_steps", 5,
					NULL);
			}
			break;
		case SYMBOL_OBJECT_ARC:
			item = gnome_canvas_item_new (
				group,
				gnome_canvas_ellipse_get_type (),
				"x1", object->u.arc.x1,
				"y1", object->u.arc.y1,
				"x2", object->u.arc.x2,
				"y2", object->u.arc.y2,
				"outline_color", NORMAL_COLOR,
				"width_pixels", 0,
				NULL);
			break;
		case SYMBOL_OBJECT_TEXT:
			item = gnome_canvas_item_new (
				group,
				gnome_canvas_text_get_type (),
				"text",object->u.text.str,
				"x", (double) object->u.text.x,
				"y", (double) object->u.text.y,
				"fill_color", LABEL_COLOR,
				"font", "Sans 8",
				NULL);
		break;
		default:
			g_warning ("Unknown symbol object.\n");
			continue;
		}
	}
}

static void
create_canvas_labels (PartItem *item, Part *part)
{
	GnomeCanvasItem *canvas_item;
	GSList *list, *labels, *item_list;
	GnomeCanvasGroup *group;

	g_return_if_fail (item != NULL);
	g_return_if_fail (IS_PART_ITEM (item));
	g_return_if_fail (part != NULL);
	g_return_if_fail (IS_PART (part));

	labels = part_get_labels (part);
	group = item->priv->label_group;
	item_list = NULL;

	for (list = labels; list; list = list->next) {
		PartLabel *label = list->data;
		char *text;

		text = part_property_expand_macros (part, label->text);

		canvas_item = gnome_canvas_item_new (
			group,
			gnome_canvas_text_get_type (),
			"x", (double) label->pos.x,
			"y", (double) label->pos.y,
			"text", text,
			"anchor", GTK_ANCHOR_SOUTH_WEST,
			"fill_color", LABEL_COLOR,
			"font", "Sans 8",
			NULL);

		item_list = g_slist_prepend (item_list, canvas_item);

		g_free (text);
	}
	item_list = g_slist_reverse (item_list);
	part_item_set_label_items (item, item_list);
}


static void
create_canvas_label_nodes (PartItem *item, Part *part)
{
	GnomeCanvasItem *canvas_item;
	GSList *item_list;
	GnomeCanvasGroup *group;
	Pin *pins;
	int num_pins, i;
	SheetPos p1, p2;
	GtkAnchorType anchor;
	int w, h;

	g_return_if_fail (item != NULL);
	g_return_if_fail (IS_PART_ITEM (item));
	g_return_if_fail (part != NULL);
	g_return_if_fail (IS_PART (part));

	num_pins = part_get_num_pins(part);
	pins = part_get_pins(part);
	group = item->priv->node_group;
	item_list = NULL;

	get_cached_bounds (item, &p1, &p2);

	w = p2.x - p1.x;
	h = p2.y - p1.y;

	switch (part_get_rotation (part)) {
		case 0:
			anchor = GTK_ANCHOR_SOUTH_WEST;
		break;
		case 90:
			anchor = GTK_ANCHOR_NORTH_WEST;
		break;
		case 180:
			anchor = GTK_ANCHOR_NORTH_EAST;
		break;
		case 270:
			anchor = GTK_ANCHOR_SOUTH_EAST;
		break;
		default:
			anchor = GTK_ANCHOR_SOUTH_WEST;
	}

	for (i=0; i<num_pins; i++) {
		int x, y;
		char *txt;
		x = pins[i].offset.x;
		y = pins[i].offset.y;

		txt = g_strdup_printf("%d", pins[i].node_nr);
		canvas_item = gnome_canvas_item_new (
			group,
			gnome_canvas_text_get_type (),
			"x", (double) x,
			"y", (double) y,
			"text", txt,
			"anchor", anchor,
			"fill_color", "black",
			"font", "Sans 8",
			NULL);

		item_list = g_slist_prepend (item_list, canvas_item);
		g_free(txt);
	}
	item_list = g_slist_reverse (item_list);
	item->priv->label_nodes = item_list;
}


/**
 * This is called when the part data was moved. Update the view accordingly.
 */
static void
part_moved_callback (ItemData *data, SheetPos *pos, SheetItem *item)
{
	PartItem *part_item;

	g_return_if_fail (data != NULL);
	g_return_if_fail (IS_ITEM_DATA (data));
	g_return_if_fail (item != NULL);
	g_return_if_fail (IS_PART_ITEM (item));

	if (pos == NULL)
		return;

	part_item = PART_ITEM (item);

	/*
	 * Move the canvas item and invalidate the bbox cache.
	 */
	gnome_canvas_item_move (GNOME_CANVAS_ITEM (item), pos->x, pos->y);
	part_item->priv->cache_valid = FALSE;
}

static void
part_item_place (SheetItem *item, SchematicView *sv)
{
	g_signal_connect (
		G_OBJECT(item),
		"event",
		G_CALLBACK(sheet_item_event),
		sv);

	g_signal_connect (
		G_OBJECT(item),
		"double_clicked",
		G_CALLBACK(edit_properties),
		item);
}

static void
part_item_place_ghost (SheetItem *item, SchematicView *sv)
{
//	part_item_signal_connect_placed (PART_ITEM (item));
}


void
part_item_show_node_labels (PartItem *part, gboolean b)
{
	PartItemPriv *priv;

	priv = part->priv;

	if (b)
		gnome_canvas_item_show (GNOME_CANVAS_ITEM (priv->node_group));
	else
		gnome_canvas_item_hide (GNOME_CANVAS_ITEM (priv->node_group));
}

