/*
 * textbox-item.c
 *
 *
 * Author:
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
/*
 * TextboxItem object: the graphical representation of a textbox.
 *
 * Author:
 *   Richard Hult (rhult@hem2.passagen.se)
 *
 * (C) 1999, 2000 Richard Hult, http://www.dtek.chalmers.se/~d4hult/oregano/
 */

#include <math.h>
#include <gnome.h>
#include <glade/glade.h>
#include "cursors.h"
#include "sheet-private.h"
#include "sheet-pos.h"
#include "textbox-item.h"
#include "textbox.h"
#include "dialogs.h"

#define NORMAL_COLOR "black"
#define SELECTED_COLOR "green"
//#define TEXTBOX_FONT "-*-helvetica-medium-r-*-*-*-100-*-*-*-*-*-*"
#define TEXTBOX_FONT "Arial 10"

static void textbox_item_class_init (TextboxItemClass *klass);
static void textbox_item_init (TextboxItem *item);
static void textbox_item_set_arg (GtkObject *object, GtkArg *arg, guint arg_id);
static void textbox_item_get_arg (GtkObject *object, GtkArg *arg, guint arg_id);
static void textbox_item_destroy (GtkObject *object);
static void textbox_item_moved (SheetItem *object);

static void textbox_rotated_callback (ItemData *data, int angle,
	SheetItem *sheet_item);
static void textbox_flipped_callback (ItemData *data, gboolean horizontal,
	SheetItem *sheet_item);
static void textbox_moved_callback (ItemData *data, SheetPos *pos,
	SheetItem *item);
static void textbox_text_changed_callback (ItemData *data, gchar *new_text,
	SheetItem *item);
static void textbox_font_changed_callback (ItemData *data, gchar *new_font,
	SheetItem *item);

static void textbox_item_paste (SchematicView *sv, ItemData *data);
static void selection_changed (TextboxItem *item, gboolean select,
	gpointer user_data);
static int  select_idle_callback (TextboxItem *item);
static int  deselect_idle_callback (TextboxItem *item);
static gboolean is_in_area (SheetItem *object, SheetPos *p1, SheetPos *p2);
inline static void get_cached_bounds (TextboxItem *item, SheetPos *p1,
	SheetPos *p2);

static void textbox_item_place (SheetItem *item, SchematicView *sv);
static void textbox_item_place_ghost (SheetItem *item, SchematicView *sv);

static void edit_cmd (GtkWidget *widget, SchematicView *sv);
static void edit_textbox (SheetItem *sheet_item);

typedef struct {
	GtkDialog *dialog;
	GtkFontSelection *font;
	GtkEntry *entry;
} TextboxPropDialog;

static TextboxPropDialog *prop_dialog = NULL;
static SheetItemClass *textbox_item_parent_class = NULL;

/* Use EDIT!! */
static const char *textbox_item_context_menu =
"<ui>"
"  <popup name='ItemMenu'>"
"    <menuitem action='EditText'/>"
"  </popup>"
"</ui>";

enum {
	TEXTBOX_ITEM_ARG_0,
	TEXTBOX_ITEM_ARG_NAME
};

struct _TextboxItemPriv {
	guint cache_valid : 1;

	guint highlight : 1;

	// FIXME: More members.
	GnomeCanvasItem *text_canvas_item;

	/*
	 * Cached bounding box. This is used to make
	 * the rubberband selection a bit faster.
	 */
	SheetPos bbox_start;
	SheetPos bbox_end;
};

GType
textbox_item_get_type ()
{
	static GType textbox_item_type = 0;

	if (!textbox_item_type) {
		static const GTypeInfo textbox_item_info = {
			sizeof(TextboxItemClass),
			NULL,
			NULL,
			(GClassInitFunc)textbox_item_class_init,
			NULL,
			NULL,
			sizeof(TextboxItem),
			0,
			(GInstanceInitFunc)textbox_item_init,
			NULL
		};

		textbox_item_type = g_type_register_static(TYPE_SHEET_ITEM,
			"TextboxItem", &textbox_item_info, 0);
	}
	return textbox_item_type;
}

static void
textbox_item_class_init (TextboxItemClass *textbox_item_class)
{
	GObjectClass *object_class;
	GtkObjectClass *gtk_object_class;
	SheetItemClass *sheet_item_class;

	object_class = G_OBJECT_CLASS(textbox_item_class);
	gtk_object_class = GTK_OBJECT_CLASS(textbox_item_class);
	sheet_item_class = SHEET_ITEM_CLASS(textbox_item_class);
	textbox_item_parent_class =
		g_type_class_peek_parent(textbox_item_class);
/*	gtk_object_add_arg_type ("TextboxItem::name",
 *              GTK_TYPE_POINTER, GTK_ARG_READWRITE, TEXTBOX_ITEM_ARG_NAME);

	object_class->set_arg = textbox_item_set_arg;
	object_class->get_arg = textbox_item_get_arg;*/
	gtk_object_class->destroy = textbox_item_destroy;

	sheet_item_class->moved = textbox_item_moved;
	sheet_item_class->paste = textbox_item_paste;
	sheet_item_class->is_in_area = is_in_area;
	sheet_item_class->selection_changed = (gpointer) selection_changed;
	sheet_item_class->edit_properties = edit_textbox;
	sheet_item_class->place = textbox_item_place;
	sheet_item_class->place_ghost = textbox_item_place_ghost;
}

static void
textbox_item_init (TextboxItem *item)
{
	TextboxItemPriv *priv;

	priv = g_new0 (TextboxItemPriv, 1);
	item->priv = priv;

	priv->highlight = FALSE;
	priv->cache_valid = FALSE;

	sheet_item_add_menu (SHEET_ITEM (item), textbox_item_context_menu);
}

/*static void
textbox_item_set_arg (GObject *object, GtkArg *arg, guint arg_id)
{
	TextboxItem *textbox_item = TEXTBOX_ITEM (object);

	textbox_item = TEXTBOX_ITEM (object);

	switch (arg_id) {
	case TEXTBOX_ITEM_ARG_NAME:
		break;
	}
}

static void
textbox_item_get_arg (GtkObject *object, GtkArg *arg, guint arg_id)
{
	TextboxItem *textbox_item = TEXTBOX_ITEM (object);

	textbox_item = TEXTBOX_ITEM (object);

	switch (arg_id) {
	case TEXTBOX_ITEM_ARG_NAME:
		break;
	default:
		arg->type = GTK_TYPE_INVALID;
		break;
	}
}*/

static void
textbox_item_destroy (GtkObject *object)
{
	TextboxItem *textbox;
	TextboxItemPriv *priv;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_TEXTBOX_ITEM (object));

	textbox = TEXTBOX_ITEM (object);
	priv = textbox->priv;

	if (priv) {
		if (priv->text_canvas_item) {
			gtk_object_destroy(GTK_OBJECT(priv->text_canvas_item));
		}
		textbox->priv = NULL;
		g_free (priv);
	}

	if (GTK_OBJECT_CLASS(textbox_item_parent_class)->destroy){
		GTK_OBJECT_CLASS(textbox_item_parent_class)->destroy(object);
	}
}

/*
 * textbox_item_moved
 *
 * "moved" signal handler. Invalidates the bounding box cache.
 */
static void
textbox_item_moved (SheetItem *object)
{
	TextboxItem *item;
	TextboxItemPriv *priv;

	item = TEXTBOX_ITEM (object);
	priv = item->priv;

	priv->cache_valid = FALSE;
}

TextboxItem *
textbox_item_new (Sheet *sheet, Textbox *textbox)
{
	TextboxItem *item;
	TextboxItemPriv *priv;
	SheetPos pos;

	g_return_val_if_fail(sheet != NULL, NULL);
	g_return_val_if_fail(IS_SHEET(sheet), NULL);

	item_data_get_pos(ITEM_DATA(textbox), &pos);

	item = TEXTBOX_ITEM(gnome_canvas_item_new (
		sheet->object_group,
		textbox_item_get_type(),
		"data", textbox,
		"x", (double) pos.x,
		"y", (double) pos.y,
		NULL));

	priv = item->priv;

	priv->text_canvas_item = gnome_canvas_item_new (
		GNOME_CANVAS_GROUP (item),
		gnome_canvas_text_get_type (),
		"x", 0.0,
		"y", 0.0,
		"text", textbox_get_text (textbox),
		"fill_color", NORMAL_COLOR,
		"anchor", GTK_ANCHOR_SW,
		"font", TEXTBOX_FONT,
		NULL);

	g_signal_connect_object(G_OBJECT (textbox),
		"rotated", G_CALLBACK(textbox_rotated_callback),
		G_OBJECT(item), 0);
	g_signal_connect_object(G_OBJECT (textbox),
		"flipped", G_CALLBACK(textbox_flipped_callback),
		G_OBJECT(item), 0);
	g_signal_connect_object(G_OBJECT (textbox),
		"moved", G_CALLBACK(textbox_moved_callback),
		G_OBJECT(item), 0);
	g_signal_connect_object(G_OBJECT (textbox),
		"text_changed", G_CALLBACK(textbox_text_changed_callback),
		G_OBJECT(item), 0);
	g_signal_connect_object(G_OBJECT (textbox),
		"font_changed", G_CALLBACK(textbox_font_changed_callback),
		G_OBJECT(item), 0);

	textbox_update_bbox (textbox);

	return item;
}

void
textbox_item_signal_connect_placed (TextboxItem *textbox_item,
	SchematicView *sv)
{
	g_signal_connect (
		G_OBJECT (textbox_item),
		"event",
		G_CALLBACK(sheet_item_event),
		sv);
}

static void
textbox_rotated_callback (ItemData *data, int angle, SheetItem *sheet_item)
{
	TextboxItem *item;

	g_return_if_fail (sheet_item != NULL);
	g_return_if_fail (IS_TEXTBOX_ITEM (sheet_item));

	item = TEXTBOX_ITEM (sheet_item);

	item->priv->cache_valid = FALSE;
}

static void
textbox_flipped_callback (ItemData *data,
	gboolean horizontal, SheetItem *sheet_item)
{
	TextboxItem *item;

	g_return_if_fail (sheet_item != NULL);
	g_return_if_fail (IS_TEXTBOX_ITEM (sheet_item));

	item = TEXTBOX_ITEM (sheet_item);

	item->priv->cache_valid = FALSE;
}

static int
select_idle_callback (TextboxItem *item)
{
	SheetPos bbox_start, bbox_end;
	TextboxItemPriv *priv = item->priv;

	//	if (GTK_OBJECT_DESTROYED (item))
	//		return FALSE;

	get_cached_bounds (item, &bbox_start, &bbox_end);
	gnome_canvas_item_set (GNOME_CANVAS_ITEM (priv->text_canvas_item),
		"fill_color", SELECTED_COLOR, NULL);

	priv->highlight = TRUE;

	//	g_object_unref(G_OBJECT(item));
	return FALSE;
}

static int
deselect_idle_callback (TextboxItem *item)
{
	TextboxItemPriv *priv = item->priv;

	gnome_canvas_item_set (GNOME_CANVAS_ITEM (priv->text_canvas_item),
		"fill_color", NORMAL_COLOR, NULL);

	priv->highlight = FALSE;

	//g_object_unref(G_OBJECT(item));
	return FALSE;
}

static void
selection_changed (TextboxItem *item, gboolean select, gpointer user_data)
{
	//	g_object_ref(G_OBJECT(item));
	if (select)
		gtk_idle_add ((gpointer) select_idle_callback, item);
	else
		gtk_idle_add ((gpointer) deselect_idle_callback, item);
}

static gboolean
is_in_area (SheetItem *object, SheetPos *p1, SheetPos *p2)
{
	TextboxItem *item;
	SheetPos bbox_start, bbox_end;

	item = TEXTBOX_ITEM (object);

	get_cached_bounds (item, &bbox_start, &bbox_end);

	if (p1->x < bbox_start.x &&
	    p2->x > bbox_end.x &&
	    p1->y < bbox_start.y &&
	    p2->y > bbox_end.y)
		return TRUE;

	return FALSE;
}

/**
 * Retrieves the bounding box. We use a caching scheme for this
 * since it's too expensive to calculate it every time we need it.
 */
inline static void
get_cached_bounds (TextboxItem *item, SheetPos *p1, SheetPos *p2)
{
	PangoFontDescription *font;
	PangoFontMetrics *font_metric;
	int width;
	int rbearing;
	int lbearing;
	int ascent, descent;
	SheetPos pos;

	TextboxItemPriv *priv;
	priv = item->priv;

	if (!priv->cache_valid) {
		SheetPos start_pos, end_pos;

		font = pango_font_description_from_string(TEXTBOX_FONT);
		//gdk_string_extents (font,
		//		    textbox_get_text (TEXTBOX (sheet_item_get_data (SHEET_ITEM (item)))),
		//		    &lbearing,
		//		    &rbearing,
		//		    &width,
		//		    &ascent,
		//		    &descent);

		item_data_get_pos (sheet_item_get_data (SHEET_ITEM (item)),
			&pos);

		start_pos.x = pos.x;
		start_pos.y = pos.y-5;// - font->ascent;
		end_pos.x = pos.x+5; // + rbearing;
		end_pos.y = pos.y+5;// + font->descent;

		priv->bbox_start = start_pos;
		priv->bbox_end = end_pos;
		priv->cache_valid = TRUE;
		pango_font_description_free(font);
	}

	memcpy (p1, &priv->bbox_start, sizeof (SheetPos));
	memcpy (p2, &priv->bbox_end, sizeof (SheetPos));

}

static void
textbox_item_paste (SchematicView *sv, ItemData *data)
{
	g_return_if_fail (sv != NULL);
	g_return_if_fail (IS_SCHEMATIC_VIEW (sv));
	g_return_if_fail (data != NULL);
	g_return_if_fail (IS_TEXTBOX (data));

	schematic_view_add_ghost_item (sv, data);
}

/**
 * This is called when the textbox data was moved. Update the view accordingly.
 */
static void
textbox_moved_callback (ItemData *data, SheetPos *pos, SheetItem *item)
{
	TextboxItem *textbox_item;

	g_return_if_fail (data != NULL);
	g_return_if_fail (IS_ITEM_DATA (data));
	g_return_if_fail (item != NULL);
	g_return_if_fail (IS_TEXTBOX_ITEM (item));

	if (pos == NULL)
		return;

	textbox_item = TEXTBOX_ITEM (item);

	/*
	 * Move the canvas item and invalidate the bbox cache.
	 */
	gnome_canvas_item_move (GNOME_CANVAS_ITEM (item), pos->x, pos->y);
	textbox_item->priv->cache_valid = FALSE;
}

static void
textbox_text_changed_callback (ItemData *data,
	gchar *new_text, SheetItem *item)
{
	TextboxItem *textbox_item;

	g_return_if_fail (data != NULL);
	g_return_if_fail (IS_ITEM_DATA (data));
	g_return_if_fail (item != NULL);
	g_return_if_fail (IS_TEXTBOX_ITEM (item));

	textbox_item = TEXTBOX_ITEM (item);

	gnome_canvas_item_set (
		GNOME_CANVAS_ITEM ( textbox_item->priv->text_canvas_item ),
		"text", new_text, NULL );

}

static void
textbox_font_changed_callback (ItemData *data,
	gchar *new_font, SheetItem *item)
{
	TextboxItem *textbox_item;

	g_return_if_fail (data != NULL);
	g_return_if_fail (IS_ITEM_DATA (data));
	g_return_if_fail (item != NULL);
	g_return_if_fail (IS_TEXTBOX_ITEM (item));
	g_return_if_fail (new_font != NULL);

	textbox_item = TEXTBOX_ITEM (item);

	gnome_canvas_item_set (
		GNOME_CANVAS_ITEM (textbox_item->priv->text_canvas_item),
		"font", new_font, NULL);
}

static void
textbox_item_place (SheetItem *item, SchematicView *sv)
{
	textbox_item_signal_connect_placed (TEXTBOX_ITEM (item), sv);

	g_signal_connect (
		G_OBJECT (item),
		"double_clicked",
		G_CALLBACK(edit_textbox),
		item);
}

static void
textbox_item_place_ghost (SheetItem *item, SchematicView *sv)
{
//	textbox_item_signal_connect_placed (TEXTBOX_ITEM (item));
}

static gboolean
create_textbox_event (Sheet *sheet, GdkEvent *event, SchematicView *sv)
{
	switch (event->type) {
	case GDK_3BUTTON_PRESS:
	case GDK_2BUTTON_PRESS:
		return TRUE;

	case GDK_BUTTON_PRESS:
		if (event->button.button == 4 || event->button.button == 5)
			return FALSE;

		if (event->button.button == 1) {
			if (sheet->state == SHEET_STATE_TEXTBOX_WAIT)
				sheet->state = SHEET_STATE_TEXTBOX_START;

			return TRUE;
		} else
			return FALSE;

	case GDK_BUTTON_RELEASE:
		if (event->button.button == 4 || event->button.button == 5)
			return FALSE;

		if (sheet->state == SHEET_STATE_TEXTBOX_START) {
			Textbox *textbox;
			SheetPos pos;

			sheet->state = SHEET_STATE_NONE;

			pos.x = event->button.x;
			pos.y = event->button.y;

			textbox = textbox_new (NULL);
			item_data_set_pos (ITEM_DATA (textbox), &pos);
			textbox_set_text (textbox, _("Label"));

			schematic_add_item (schematic_view_get_schematic (sv),
				ITEM_DATA (textbox));

			g_signal_emit_by_name(G_OBJECT(sheet),
				"reset_tool", NULL);
			g_signal_handlers_disconnect_by_func (G_OBJECT (sheet),
				G_CALLBACK(create_textbox_event), sv);
		}

		return TRUE;

	default:
		return FALSE;
	}

	return TRUE;
}

	void
textbox_item_cancel_listen (SchematicView *sv)
{
	Sheet *sheet;

	g_return_if_fail (sv != NULL);
	g_return_if_fail (IS_SCHEMATIC_VIEW (sv));

	sheet = schematic_view_get_sheet (sv);

	sheet->state = SHEET_STATE_NONE;
	g_signal_handlers_disconnect_by_func (G_OBJECT (sheet), G_CALLBACK(create_textbox_event), sv);
}

void
textbox_item_listen (SchematicView *sv)
{
	Sheet *sheet;

	g_return_if_fail (sv != NULL);
	g_return_if_fail (IS_SCHEMATIC_VIEW (sv));

//	schematic_view_disconnect_handler (sv);
	sheet = schematic_view_get_sheet (sv);

	/*
	 * Connect to a signal handler that will
	 * let the user create a new textbox.
	 */
	sheet->state = SHEET_STATE_TEXTBOX_WAIT;
	g_signal_connect(G_OBJECT(sheet),
		"event", G_CALLBACK(create_textbox_event), sv);
}

/*
 * Go through the properties and commit the changes.
 */
static void
edit_dialog_ok(TextboxItem *item)
{
	TextboxItemPriv *priv;
	Textbox *textbox;
	const gchar *value;

	g_return_if_fail (item != NULL);
	g_return_if_fail (IS_TEXTBOX_ITEM (item));

	priv = item->priv;
	textbox = TEXTBOX (sheet_item_get_data (SHEET_ITEM (item)));

	value = gtk_entry_get_text(GTK_ENTRY(prop_dialog->entry));

	textbox_set_text(textbox, value);
	textbox_set_font(textbox,
		gtk_font_selection_get_font_name(prop_dialog->font));
}

static void
edit_textbox (SheetItem *sheet_item)
{
	Sheet *sheet;
	TextboxItem *item;
	TextboxItemPriv *priv;
	Textbox *textbox;
	char *msg, *value;
	GladeXML *gui;

	g_return_if_fail (sheet_item != NULL);
	g_return_if_fail (IS_TEXTBOX_ITEM (sheet_item));

	item = TEXTBOX_ITEM (sheet_item);
	priv = item->priv;
	textbox = TEXTBOX (sheet_item_get_data (sheet_item));

	if (!g_file_test (OREGANO_GLADEDIR "/textbox-properties-dialog.glade2",
		    G_FILE_TEST_EXISTS)) {
		msg = g_strdup_printf (
			_("The file %s could not be found. You might need to reinstall Oregano to fix this."),
			OREGANO_GLADEDIR "/textbox-properties-dialog.glade2");
		oregano_error (_("Could not create textbox properties dialog"));
		g_free (msg);
		return;
	}

	gui = glade_xml_new (
		OREGANO_GLADEDIR "/textbox-properties-dialog.glade2",
		NULL, NULL);
	if (!gui) {
		oregano_error (_("Could not create textbox properties dialog"));
		return;
	}

	prop_dialog = g_new0 (TextboxPropDialog, 1);
	prop_dialog->dialog = GTK_DIALOG (
		glade_xml_get_widget (gui, "textbox-properties-dialog"));
	prop_dialog->font = GTK_FONT_SELECTION (
		glade_xml_get_widget (gui, "font_selector"));
	prop_dialog->entry = GTK_ENTRY (glade_xml_get_widget (gui, "entry"));

	value = textbox_get_font (textbox);
	gtk_font_selection_set_font_name (
		GTK_FONT_SELECTION (prop_dialog->font), value);

	value = textbox_get_text(textbox);
	gtk_entry_set_text (GTK_ENTRY (prop_dialog->entry), value);

	sheet = sheet_item_get_sheet (SHEET_ITEM (item));
	sheet_dialog_set_parent (sheet, (GtkDialog*) prop_dialog->dialog);

	gtk_dialog_set_default_response (
		GTK_DIALOG (prop_dialog->dialog), GTK_RESPONSE_OK);

	gtk_dialog_run( GTK_DIALOG (prop_dialog->dialog));

	edit_dialog_ok(item);

	/* Clean the dialog */
	gtk_widget_destroy(GTK_WIDGET(prop_dialog->dialog));
	prop_dialog = NULL;
}

static void
edit_cmd (GtkWidget *widget, SchematicView *sv)
{
	GList *list;

	list = schematic_view_get_selection (sv);
	if ((list != NULL) && IS_TEXTBOX_ITEM (list->data)) 
		edit_textbox (list->data);
}
