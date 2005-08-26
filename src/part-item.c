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
 * Copyright (C) 2003,2005  LUGFI
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

#include <config.h>
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
#include "netlist.h"
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
static void properties_cmd (GtkWidget *widget, SchematicView *sv);
static gboolean edit_properties_unselect_row (GtkWidget *list,
	GdkEventButton *event, GladeXML *gui);
static gboolean edit_properties_select_row (GtkWidget *list,
	GdkEventButton *event, GladeXML *gui);
/*static void edit_properties_select_row (GtkTreeView *list, GtkTreePath *path,
  GtkTreeViewColumn *column, GladeXML *gui);*/

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
	GtkTreeView *list;
	GtkWidget *name_entry;
	GtkWidget *value_entry;
	gint	   selected_row;
	PartItem  *part_item;
} PartPropDialog;

static PartPropDialog *prop_dialog = NULL;
static SheetItemClass *parent_class = NULL;

/*
 * This is the lower part of the object popup menu. It contains actions
 * that are specific for parts.
 */
static GnomeUIInfo part_popup_menu [] = {
	GNOMEUIINFO_SEPARATOR,

	{
		GNOME_APP_UI_ITEM,
		N_("Properties"), N_("Edit the part's properties"),
		properties_cmd, NULL, NULL, GNOME_APP_PIXMAP_STOCK,
		GTK_STOCK_PROPERTIES, 0, 0
	},
	GNOMEUIINFO_END
};

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

	sheet_item_class->context_menu = g_new0 (SheetItemMenu, 1);
	sheet_item_class->context_menu->menu = part_popup_menu;
	sheet_item_class->context_menu->size =
		sizeof (part_popup_menu) / sizeof (part_popup_menu[0]);
}

static void
part_item_init (PartItem *item)
{
	PartItemPriv *priv;

	priv = g_new0 (PartItemPriv, 1);

	priv->highlight = FALSE;
	priv->cache_valid = FALSE;

	item->priv = priv;
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
	GSList *labels, *label_items;
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
prop_dialog_response(GtkWidget *widget, gint response,
	PartPropDialog *prop_dialog)
{
	GSList		 *properties;
	Property	 *prop;
	PartItem	 *item;
	PartItemPriv *priv;
	Part		 *part;
	gint		  row;
	gchar		 *value;
	const gchar	 *tmp;
	GtkTreeModel *model;
	GtkTreeIter iter;
	GtkTreeSelection *selection;

	item = prop_dialog->part_item;

	priv = item->priv;
	part = PART (sheet_item_get_data (SHEET_ITEM (item)));

	selection = gtk_tree_view_get_selection(prop_dialog->list);
	model = gtk_tree_view_get_model(prop_dialog->list);

	if (gtk_tree_selection_get_selected(selection, NULL, &iter)) {
		tmp = gtk_entry_get_text(GTK_ENTRY(prop_dialog->value_entry));
		value = g_strdup(tmp);
		gtk_list_store_set(GTK_LIST_STORE(model), &iter, 1, value, -1);
		g_free(value);
	}

	row = 0;
	for (properties = part_get_properties (part); properties;
	     properties = properties->next) {
		prop = properties->data;

		if (!g_strcasecmp (prop->name, "internal")) {
			continue;
		}

		if (gtk_tree_model_iter_nth_child(model, &iter, NULL, row)) {
			gtk_tree_model_get(model, &iter, 1, &value, -1);
			if (prop->value) g_free(prop->value);
			prop->value = g_strdup(value);
		}

		row++;
	}

	update_canvas_labels (item);
}

static gboolean
edit_properties_select_row (GtkWidget *list, GdkEventButton *event,
	GladeXML *gui)
{
	char *value;
	char *name;
	GtkTreeModel *model;
	GtkTreeIter iter;
	GtkTreeSelection *selection;

	if (event->button != 1) return TRUE;

	/* Get the current selected row */
	selection = gtk_tree_view_get_selection(prop_dialog->list);
	model = gtk_tree_view_get_model(prop_dialog->list);

	if (!gtk_tree_selection_get_selected(selection, NULL, &iter)) {
		return FALSE;
	}

	gtk_tree_model_get(model, &iter, 0, &name, 1, &value, -1);

	if (name)
		gtk_entry_set_text(GTK_ENTRY(prop_dialog->name_entry), name);
	else
		gtk_entry_set_text(GTK_ENTRY(prop_dialog->name_entry), "");
	if (value)
		gtk_entry_set_text(GTK_ENTRY(prop_dialog->value_entry), value);
	else
		gtk_entry_set_text(GTK_ENTRY(prop_dialog->value_entry), "");

	return FALSE;
}

static gboolean
edit_properties_unselect_row (GtkWidget *list, GdkEventButton *event,
	GladeXML *gui)
{
	char *value;
	GtkTreeModel *model;
	GtkTreeIter iter;
	GtkTreeSelection *selection;

	if (event->button != 1) return TRUE;

	/* Get the current selected row */
	selection = gtk_tree_view_get_selection(prop_dialog->list);
	model = gtk_tree_view_get_model(prop_dialog->list);

	if (!gtk_tree_selection_get_selected(selection, NULL, &iter)) {
		/* If none, do nothing */
		return FALSE;
	}

	value = gtk_editable_get_chars (GTK_EDITABLE(prop_dialog->value_entry),
		0, -1);
	gtk_list_store_set(GTK_LIST_STORE(model), &iter, 1, value, -1);
	g_free (value);

	return FALSE;
}

static void
edit_properties_punta (PartItem *item)
{
	GSList *properties;
	Sheet *sheet;
	PartItemPriv *priv;
	Part *part;
	char *msg, *value, *name;
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
			_("The file %s could not be found. You might need to reinstall Oregano to fix this"),
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
	if (response == GTK_RESPONSE_OK) {
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
	}

	gtk_widget_destroy (GTK_WIDGET (prop_dialog->dialog));
}

static void
edit_properties (SheetItem *object)
{
	GSList *properties;
	Sheet *sheet;
	PartItem *item;
	PartItemPriv *priv;
	Part *part;
	char *internal, *msg, *value, *name;
	GladeXML *gui;
	GtkTreeIter iter;
	GtkCellRenderer *cell_name, *cell_value;
	GtkTreeViewColumn *cell_column_name, *cell_column_value;
	GtkListStore *prop_list;
	GtkTreeModel *model;
	GtkTreePath *path;
	GtkTreeSelection *selection;
	gint response;
	gboolean got_iter;

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
			_("The file %s could not be found. You might need to reinstall Oregano to fix this"),
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

	prop_dialog->dialog = GTK_DIALOG (
		glade_xml_get_widget (gui, "part-properties-dialog"));
	gtk_dialog_set_has_separator (prop_dialog->dialog, FALSE);

	prop_dialog->list = GTK_TREE_VIEW (glade_xml_get_widget (gui, "properties-list"));
	/* Create the model */
	prop_list = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_STRING);
	/* Create the Cells and Columns */
	cell_name = gtk_cell_renderer_text_new();
	cell_value = gtk_cell_renderer_text_new();
	cell_column_name = gtk_tree_view_column_new_with_attributes("Name",
		cell_name, "text", 0, NULL);
	cell_column_value = gtk_tree_view_column_new_with_attributes("Value",
		cell_value, "text", 1, NULL);
	/* Add to the tree */
	gtk_tree_view_set_model(prop_dialog->list, GTK_TREE_MODEL(prop_list));
	gtk_tree_view_append_column(prop_dialog->list, cell_column_name);
	gtk_tree_view_append_column(prop_dialog->list, cell_column_value);

	prop_dialog->value_entry = glade_xml_get_widget (gui, "value-entry");
	prop_dialog->name_entry = glade_xml_get_widget (gui, "name-entry");

/*	g_signal_connect (prop_dialog->dialog, "response",
		(GCallback) prop_dialog_response,
		prop_dialog
	);*/

	g_signal_connect (prop_dialog->dialog, "destroy",
		(GCallback) prop_dialog_destroy,
		prop_dialog
	);

	for (properties = part_get_properties (part); properties;
		properties = properties->next) {
		Property *prop;
		prop = properties->data;
		if (prop->name) {
			if (!g_strcasecmp (prop->name, "internal"))
				continue;

			gtk_list_store_append(prop_list, &iter);
			gtk_list_store_set(prop_list, &iter, 0, prop->name, 1,
				prop->value, -1);
		}
	}

	gtk_dialog_set_default_response (prop_dialog->dialog, 1);

	gtk_widget_set_sensitive (prop_dialog->name_entry, FALSE);

	g_signal_connect (G_OBJECT (prop_dialog->list), "button_press_event",
		GTK_SIGNAL_FUNC (edit_properties_unselect_row), gui);

	g_signal_connect (G_OBJECT (prop_dialog->list), "button_release_event",
		GTK_SIGNAL_FUNC (edit_properties_select_row), gui);

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (prop_dialog->list));
	path = gtk_tree_path_new_first ();
	gtk_tree_view_set_cursor (GTK_TREE_VIEW (prop_dialog->list),path, NULL, FALSE);
	got_iter = gtk_tree_model_get_iter (GTK_TREE_MODEL (model), &iter, path);
	gtk_tree_path_free (path);
	

	if (got_iter) {
		gtk_tree_model_get (GTK_TREE_MODEL (model), &iter, 0, &name, 1, &value, -1);

		if (name)
			gtk_entry_set_text (GTK_ENTRY (prop_dialog->name_entry), name);
		else
			gtk_entry_set_text (GTK_ENTRY (prop_dialog->name_entry), "");
		if (value)
			gtk_entry_set_text (GTK_ENTRY (prop_dialog->value_entry), value);
		else
			gtk_entry_set_text (GTK_ENTRY (prop_dialog->value_entry), "");
	}
	response = gtk_dialog_run(prop_dialog->dialog);
	if (response == GTK_RESPONSE_OK) {
		prop_dialog_response (GTK_WIDGET (prop_dialog->dialog), response, prop_dialog);
	}

	gtk_widget_destroy (GTK_WIDGET (prop_dialog->dialog));
}

static void
properties_cmd (GtkWidget *widget, SchematicView *sv)
{
	GList *list;

	list = schematic_view_get_selection (sv);
	if ((list != NULL) && IS_PART_ITEM (list->data))
		edit_properties (list->data);
}

static void
canvas_text_affine (GnomeCanvasItem *canvas_item, double *affine)
{
	ArtPoint src, dst;

	g_object_get (G_OBJECT (canvas_item),
		"x", &src.x,
		"y", &src.y,
		NULL);

	art_affine_point (&dst, &src, affine);

	gnome_canvas_item_set (canvas_item, "x", dst.x,"y",dst.y, NULL);
}

static void
canvas_line_affine (GnomeCanvasItem *canvas_item, double *affine)
{
	GnomeCanvasPoints *points;
	ArtPoint src, dst;
//	GtkArg arg[2];
	int i;

	g_object_get(G_OBJECT (canvas_item), "points", &points, NULL);

	for (i = 0; i < points->num_points * 2; i += 2) {
		src.x = points->coords[i];
		src.y = points->coords[i + 1];
		art_affine_point (&dst, &src, affine);
		if (fabs (dst.x) < 1e-2)
			dst.x = 0;
		if (fabs (dst.y) < 1e-2)
			dst.y = 0;
		points->coords[i] = dst.x;
		points->coords[i + 1] = dst.y;
	}

	gnome_canvas_item_set (canvas_item, "points", points, NULL);
	gnome_canvas_points_unref (points);
}

static void
part_rotated_callback (ItemData *data, int angle, SheetItem *sheet_item)
{
	double affine[6];
	double x1, y1, x2, y2, x0, y0;
	double left, top, right, bottom, dx, dy;
	GList *list;
	GSList *label_items;
	GtkAnchorType anchor;
	ArtPoint src, dst;
	GnomeCanvasGroup *group;
	GnomeCanvasItem *canvas_item;
	ArtPoint *old = NULL;
	PartItem *item;
	PartItemPriv *priv;
	Part *part;

	g_return_if_fail (sheet_item != NULL);
	g_return_if_fail (IS_PART_ITEM (sheet_item));

	item = PART_ITEM (sheet_item);
	group = GNOME_CANVAS_GROUP (item);
	part = PART(data);

	priv = item->priv;

	art_affine_rotate (affine, angle);

	for (list = group->item_list; list; list = list->next) {
		canvas_item = GNOME_CANVAS_ITEM (list->data);
		if (GNOME_IS_CANVAS_LINE (canvas_item)) {
			canvas_line_affine (canvas_item, affine);
		} else if ( GNOME_IS_CANVAS_TEXT (canvas_item)) {
			canvas_text_affine (canvas_item,affine);
		} else if (GNOME_IS_CANVAS_ELLIPSE (canvas_item)) {
			/*
			 * Big great hack. Needed to rotate circles in the non
			 * aa-canvas.
			 */
			g_object_get (G_OBJECT (list->data),
				"x1", &x1,
				"y1", &y1,
				"x2", &x2,
				"y2", &y2,
				NULL);

			left = MIN (x1, x2);
			right = MAX (x1, x2);
			top = MIN (y1, y2);
			bottom = MAX (y1, y2);

			old = g_object_get_data (G_OBJECT (canvas_item), "hack");
			if (old  != NULL) {
				x0 = src.x = old->x;
				y0 = src.y = old->y;
			} else {
				x0 = src.x = left + (right - left) / 2;
				y0 = src.y = top + (bottom - top) / 2;
				old = g_new (ArtPoint, 1);
				gtk_object_set_data (GTK_OBJECT (canvas_item), "hack", old);
			}

			art_affine_point (&dst, &src, affine);
			old->x = dst.x;
			old->y = dst.y;

			dx = dst.x - x0;
			dy = dst.y - y0;

			gnome_canvas_item_move (canvas_item, dx, dy);
		}
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
		canvas_text_affine (label_items->data,affine);

		gnome_canvas_item_set (
			GNOME_CANVAS_ITEM (label_items->data),
			"anchor", anchor,
			NULL);
	}

	for (label_items = priv->label_nodes; label_items;
	     label_items = label_items->next) {
		canvas_text_affine (label_items->data,affine);

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
	GSList *label_items;
	/*
	 * Unused variable
	GtkAnchorType anchor;
	*/
	GnomeCanvasGroup *group;
	GnomeCanvasItem *canvas_item;
	PartItem *item;
	PartItemPriv *priv;
	Part *part;
	double affine[6], x1, y1, x2, y2, left, top, right, bottom;
	ArtPoint src, dst;
	gdouble text_heigth, text_width;

	g_return_if_fail (sheet_item != NULL);
	g_return_if_fail (IS_PART_ITEM (sheet_item));

	item = PART_ITEM (sheet_item);
	group = GNOME_CANVAS_GROUP (item);
	part = PART (data);

	priv = item->priv;

	if (horizontal)
		art_affine_scale (affine, -1, 1);
	else
		art_affine_scale (affine, 1, -1);

	for (list = group->item_list; list; list = list->next) {
		canvas_item = GNOME_CANVAS_ITEM (list->data);
		if (GNOME_IS_CANVAS_LINE (canvas_item)) {
			/*
			 * Flip the line.
			 */
			canvas_line_affine (canvas_item, affine);
		} else if (GNOME_IS_CANVAS_ELLIPSE (canvas_item)) {
			/*
			 * Flip the ellipse.
			 */
			g_object_get (G_OBJECT (canvas_item),
				"x1", &src.x,
				"y1", &src.y,
				NULL);

			art_affine_point (&dst, &src, affine);
			if (fabs (dst.x) < 1e-2)
				dst.x = 0;
			if (fabs (dst.y) < 1e-2)
				dst.y = 0;
			x1 = dst.x;
			y1 = dst.y;

			g_object_get (G_OBJECT (canvas_item),
				"x2", &src.x,
				"y2", &src.y,
				NULL);
			art_affine_point (&dst, &src, affine);
			if (fabs (dst.x) < 1e-2)
				dst.x = 0;
			if (fabs (dst.y) < 1e-2)
				dst.y = 0;
			x2 = dst.x;
			y2 = dst.y;

			left = MIN (x1, x2);
			right = MAX (x1, x2);
			top = MIN (y1, y2);
			bottom = MAX (y1, y2);

			gnome_canvas_item_set (
				canvas_item,
				"x1", left,
				"y1", top,
				"x2", right,
				"y2", bottom,
				NULL);
		}
	}

	/*
	 * Flip the labels as well.
	 */
	for(label_items = priv->label_items; label_items;
	    label_items = label_items->next) {
		GnomeCanvasItem *label;

		label = label_items->data;

		g_object_get(G_OBJECT(label),
			"x", &src.x,
			"y", &src.y,
			"text-width", &text_width,
			"text-height", &text_heigth,
			NULL);

		art_affine_point (&dst, &src, affine);

		if (fabs (dst.x) < 1e-2)
			dst.x = 0;
		if (fabs (dst.y) < 1e-2)
			dst.y = 0;
		if (horizontal) {
			if (dst.x)
				dst.x = dst.x - text_width;
		} else if (dst.y)
			dst.y = dst.y + text_heigth;

		gnome_canvas_item_set (
			GNOME_CANVAS_ITEM (label_items->data),
			"x", dst.x,
			"y", dst.y,
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
	int angle;
	IDFlip flip;

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

	angle = part_get_rotation (part);
	part_rotated_callback (ITEM_DATA (part), angle, SHEET_ITEM (item));

	flip = part_get_flip (part);
	if (flip & ID_FLIP_HORIZ)
		part_flipped_callback (ITEM_DATA (part),
			TRUE, SHEET_ITEM (item));

	if (flip & ID_FLIP_VERT)
		part_flipped_callback (ITEM_DATA (part),
			FALSE, SHEET_ITEM (item));

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

