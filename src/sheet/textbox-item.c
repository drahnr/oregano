/*
 * textbox-item.c
 *
 *
 * Author:
 *  Richard Hult <rhult@hem.passagen.se>
 *  Ricardo Markiewicz <rmarkie@fi.uba.ar>
 *  Andres de Barbara <adebarbara@fi.uba.ar>
 *  Marc Lorber <lorber.marc@wanadoo.fr>
 *
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

#include <string.h>
#include <glib/gi18n.h>

#include "cursors.h"
#include "sheet-pos.h"
#include "textbox-item.h"
#include "textbox.h"
#include "dialogs.h"

#define NORMAL_COLOR "black"
#define SELECTED_COLOR "green"
#define TEXTBOX_FONT "Arial 10"

static void 		textbox_item_class_init (TextboxItemClass *klass);
static void 		textbox_item_init (TextboxItem *item);
static void 		textbox_item_finalize (GObject *object);
static void 		textbox_item_moved (SheetItem *object);
static void 		textbox_rotated_callback (ItemData *data, int angle,
						SheetItem *sheet_item);
static void 		textbox_flipped_callback (ItemData *data, 
		                gboolean horizontal, SheetItem *sheet_item);
static void 		textbox_moved_callback (ItemData *data, SheetPos *pos,
						SheetItem *item);
static void 		textbox_text_changed_callback (ItemData *data, 
		                gchar *new_text, SheetItem *item);
static void 		textbox_item_paste (Sheet *sheet, ItemData *data);
static void 		selection_changed (TextboxItem *item, gboolean select,
						gpointer user_data);
static int  		select_idle_callback (TextboxItem *item);
static int  		deselect_idle_callback (TextboxItem *item);
static gboolean 	is_in_area (SheetItem *object, SheetPos *p1, SheetPos *p2);
inline static void 	get_cached_bounds (TextboxItem *item, SheetPos *p1,
						SheetPos *p2);
static void 		textbox_item_place (SheetItem *item, Sheet *sheet);
static void 		textbox_item_place_ghost (SheetItem *item, Sheet *sheet);
static void 		edit_textbox (SheetItem *sheet_item); 
static void 		edit_cmd (GtkWidget *widget, Sheet *sheet);

#define NG_DEBUG(s) if (0) g_print ("%s\n", s)

typedef struct {
	GtkDialog *dialog;
	GtkEntry *entry;
} TextboxPropDialog;

static TextboxPropDialog *prop_dialog = NULL;

static const char *textbox_item_context_menu =
"<ui>"
"  <popup name='ItemMenu'>"
"    <menuitem action='EditText'/>"
"  </popup>"
"</ui>";

static GtkActionEntry action_entries[] = {
	{"EditText", GTK_STOCK_PROPERTIES, N_("_Edit the text..."), NULL,
	N_("Edit the text"),G_CALLBACK (edit_cmd)}
};

enum {
	TEXTBOX_ITEM_ARG_0,
	TEXTBOX_ITEM_ARG_NAME
};

struct _TextboxItemPriv {
	guint cache_valid : 1;
	guint highlight : 1;
	GooCanvasItem *text_canvas_item;
	// Cached bounding box. This is used to make
	// the rubberband selection a bit faster.
	SheetPos bbox_start;
	SheetPos bbox_end;
};

G_DEFINE_TYPE (TextboxItem, textbox_item, TYPE_SHEET_ITEM)

static void
textbox_item_class_init (TextboxItemClass *textbox_item_class)
{
	GObjectClass *object_class;
	SheetItemClass *sheet_item_class;

	object_class = G_OBJECT_CLASS (textbox_item_class);
	sheet_item_class = SHEET_ITEM_CLASS (textbox_item_class);
	textbox_item_parent_class =
		g_type_class_peek_parent (textbox_item_class);

	object_class->finalize = textbox_item_finalize;

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

	sheet_item_add_menu (SHEET_ITEM (item), textbox_item_context_menu,
	    action_entries, G_N_ELEMENTS (action_entries));
}

static void
textbox_item_finalize (GObject *object)
{
	TextboxItem *item;

	item = TEXTBOX_ITEM (object);

	if (item->priv)
		g_free (item->priv);
	
	G_OBJECT_CLASS (textbox_item_parent_class)->finalize (object);
}

// "moved" signal handler. Invalidates the bounding box cache.
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
	GooCanvasItem *item;
	TextboxItem *textbox_item;
	TextboxItemPriv *priv;
	SheetPos pos;
	ItemData *item_data;

	g_return_val_if_fail (sheet != NULL, NULL);
	g_return_val_if_fail (IS_SHEET (sheet), NULL);

	item_data_get_pos (ITEM_DATA (textbox), &pos);
	
	item = g_object_new (TYPE_TEXTBOX_ITEM, NULL);
	
	g_object_set (item, 
	              "parent", sheet->object_group, 
	              NULL);
	
	textbox_item = TEXTBOX_ITEM (item);
	g_object_set (textbox_item, 
	              "data", textbox, 
	              NULL);

	priv = textbox_item->priv;

	priv->text_canvas_item = goo_canvas_text_new (GOO_CANVAS_ITEM (textbox_item),
	    textbox_get_text (textbox), 0.0, 0.0, -1, GOO_CANVAS_ANCHOR_SW,
	    "font", TEXTBOX_FONT,
	    "fill-color", NORMAL_COLOR,
	    NULL);

	item_data = ITEM_DATA (textbox);

	item_data->rotated_handler_id = g_signal_connect_object (G_OBJECT (textbox), 
	    "rotated", 
		G_CALLBACK (textbox_rotated_callback), 
	    G_OBJECT (textbox_item), 
	    0);
	item_data->flipped_handler_id = g_signal_connect_object (G_OBJECT (textbox), 
	    "flipped", 
	    G_CALLBACK (textbox_flipped_callback), 
	    G_OBJECT (textbox_item), 
	    0);
	item_data->moved_handler_id = g_signal_connect_object (G_OBJECT (textbox), 
	    "moved", 
		G_CALLBACK (textbox_moved_callback), 
	    G_OBJECT (textbox_item), 
	    0);
	textbox->text_changed_handler_id = g_signal_connect_object (G_OBJECT (textbox), 
	    "text_changed", 
		G_CALLBACK (textbox_text_changed_callback), 
	    G_OBJECT (textbox_item), 
	    0);

	textbox_update_bbox (textbox);

	return textbox_item;
}

void
textbox_item_signal_connect_placed (TextboxItem *textbox_item,
	Sheet *sheet)
{
	g_signal_connect (G_OBJECT (textbox_item), "button_press_event",
	    G_CALLBACK (sheet_item_event), sheet);

	g_signal_connect (G_OBJECT (textbox_item), "button_release_event",
	    G_CALLBACK (sheet_item_event), sheet);
	
	g_signal_connect (G_OBJECT (textbox_item), "key_press_event",
	    G_CALLBACK (sheet_item_event), sheet);
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

	get_cached_bounds (item, &bbox_start, &bbox_end);
	g_object_set (priv->text_canvas_item, 
	              "fill_color", SELECTED_COLOR, 
	              NULL);

	priv->highlight = TRUE;

	return FALSE;
}

static int
deselect_idle_callback (TextboxItem *item)
{
	TextboxItemPriv *priv = item->priv;

	g_object_set (priv->text_canvas_item, 
	              "fill_color", NORMAL_COLOR, 
	              NULL);

	priv->highlight = FALSE;

	return FALSE;
}

static void
selection_changed (TextboxItem *item, gboolean select, gpointer user_data)
{
	if (select)
		g_idle_add ((gpointer) select_idle_callback, item);
	else
		g_idle_add ((gpointer) deselect_idle_callback, item);
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

// Retrieves the bounding box. We use a caching scheme for this
// since it's too expensive to calculate it every time we need it.
inline static void
get_cached_bounds (TextboxItem *item, SheetPos *p1, SheetPos *p2)
{
	PangoFontDescription *font;
	SheetPos pos;

	TextboxItemPriv *priv;
	priv = item->priv;

	if (!priv->cache_valid) {
		SheetPos start_pos, end_pos;

		font = pango_font_description_from_string (TEXTBOX_FONT);

		item_data_get_pos (sheet_item_get_data (SHEET_ITEM (item)),
			&pos);

		start_pos.x = pos.x;
		start_pos.y = pos.y-5;// - font->ascent;
		end_pos.x = pos.x+5;  // + rbearing;
		end_pos.y = pos.y+5;  // + font->descent;

		priv->bbox_start = start_pos;
		priv->bbox_end = end_pos;
		priv->cache_valid = TRUE;
		pango_font_description_free (font);
	}

	memcpy (p1, &priv->bbox_start, sizeof (SheetPos));
	memcpy (p2, &priv->bbox_end, sizeof (SheetPos));
}

static void
textbox_item_paste (Sheet *sheet, ItemData *data)
{
	g_return_if_fail (sheet != NULL);
	g_return_if_fail (IS_SHEET (sheet));
	g_return_if_fail (data != NULL);
	g_return_if_fail (IS_TEXTBOX (data));

	sheet_add_ghost_item (sheet, data);
}

// This is called when the textbox data was moved. Update the view accordingly.
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

	// Move the canvas item and invalidate the bbox cache.
	goo_canvas_item_translate (GOO_CANVAS_ITEM (item), pos->x, pos->y);
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

	g_object_set (textbox_item->priv->text_canvas_item, 
				  "text", new_text, 
	              NULL );
	goo_canvas_item_ensure_updated (GOO_CANVAS_ITEM (textbox_item));
}

static void
textbox_item_place (SheetItem *item, Sheet *sheet)
{
	textbox_item_signal_connect_placed (TEXTBOX_ITEM (item), sheet);

	g_signal_connect (G_OBJECT (item), "double_clicked",
		G_CALLBACK (edit_textbox), item);
}

static void
textbox_item_place_ghost (SheetItem *item, Sheet *sheet)
{
//	textbox_item_signal_connect_placed (TEXTBOX_ITEM (item));
}

static gboolean
create_textbox_event (Sheet *sheet, GdkEvent *event)
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
			} 
			else
				return FALSE;

		case GDK_BUTTON_RELEASE:
			if (event->button.button == 4 || event->button.button == 5)
				return FALSE;

			if (sheet->state == SHEET_STATE_TEXTBOX_START) {
				Textbox *textbox;
				SheetPos pos;

				sheet->state = SHEET_STATE_NONE;

				sheet_get_pointer (sheet, &pos.x, &pos.y);
				textbox = textbox_new (NULL);
				
				textbox_set_text (textbox, _("Label"));

				schematic_add_item (schematic_view_get_schematic_from_sheet (sheet),
								ITEM_DATA (textbox));
				item_data_set_pos (ITEM_DATA (textbox), &pos);
			
				schematic_view_reset_tool (
					schematic_view_get_schematicview_from_sheet (sheet));
				g_signal_handlers_disconnect_by_func (G_OBJECT (sheet),
						G_CALLBACK (create_textbox_event), sheet);
			}

			return TRUE;

		default:
			return FALSE;
		}

	return TRUE;
}

	void
textbox_item_cancel_listen (Sheet *sheet)
{
	g_return_if_fail (sheet != NULL);
	g_return_if_fail (IS_SHEET (sheet));

	sheet->state = SHEET_STATE_NONE;
	g_signal_handlers_disconnect_by_func (G_OBJECT (sheet), 
		G_CALLBACK (create_textbox_event), sheet);
}

void
textbox_item_listen (Sheet *sheet)
{
	g_return_if_fail (sheet != NULL);
	g_return_if_fail (IS_SHEET (sheet));

	// Connect to a signal handler that will let the user create a new textbox.
	sheet->state = SHEET_STATE_TEXTBOX_WAIT;
	g_signal_connect (G_OBJECT (sheet), "event",
		G_CALLBACK (create_textbox_event), sheet);
}

// Go through the properties and commit the changes.
void
edit_dialog_ok (TextboxItem *item)
{
	const gchar *value;
	Textbox *textbox;

	g_return_if_fail (item != NULL);
	g_return_if_fail (IS_TEXTBOX_ITEM (item));

	textbox = TEXTBOX (sheet_item_get_data (SHEET_ITEM (item)));
	value = gtk_entry_get_text (GTK_ENTRY (prop_dialog->entry));
	textbox_set_text (textbox, value);
}

static void
edit_textbox (SheetItem *sheet_item)
{
	TextboxItem *item;
	Textbox *textbox;
	char *msg;
	const char *value;
	GtkBuilder *gui;
	GError *perror = NULL;

	g_return_if_fail (sheet_item != NULL);
	g_return_if_fail (IS_TEXTBOX_ITEM (sheet_item));

	if ((gui = gtk_builder_new ()) == NULL) {
		oregano_error (_("Could not create textbox properties dialog"));
		return;
	} 
	else gtk_builder_set_translation_domain (gui, NULL);

	item = TEXTBOX_ITEM (sheet_item);
	textbox = TEXTBOX (sheet_item_get_data (sheet_item));

	if (!g_file_test (OREGANO_UIDIR "/textbox-properties-dialog.ui",
		    G_FILE_TEST_EXISTS)) {
		msg = g_strdup_printf (
			_("The file %s could not be found. You might need to reinstall "
			  "Oregano to fix this."),
			OREGANO_UIDIR "/textbox-properties-dialog.ui");
		oregano_error (_("Could not create textbox properties dialog"));
		g_free (msg);
		return;
	}

	if (gtk_builder_add_from_file (gui, OREGANO_UIDIR "/textbox-properties-dialog.ui", 
	    &perror) <= 0) {
		msg = perror->message;
		oregano_error_with_title (_("Could not create textbox properties dialog"), msg);
		g_error_free (perror);
		return;
	}

	prop_dialog = g_new0 (TextboxPropDialog, 1);
	prop_dialog->dialog = GTK_DIALOG (
		gtk_builder_get_object (gui, "textbox-properties-dialog"));

	prop_dialog->entry = GTK_ENTRY (gtk_builder_get_object (gui, "entry"));

	value = textbox_get_text (textbox);
	gtk_entry_set_text (GTK_ENTRY (prop_dialog->entry), value);

	gtk_dialog_run (GTK_DIALOG (prop_dialog->dialog));
	edit_dialog_ok (item);

	// Clean / destroy the dialog
	gtk_widget_destroy (GTK_WIDGET (prop_dialog->dialog));
	prop_dialog = NULL;
}

static void
edit_cmd (GtkWidget *widget, Sheet *sheet)
{
	GList *list;

	list = sheet_get_selection (sheet);
	if ((list != NULL) && IS_TEXTBOX_ITEM (list->data)) 
		edit_textbox (list->data);
	g_list_free_full (list, g_object_unref);
}
