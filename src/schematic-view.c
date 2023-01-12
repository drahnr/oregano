/*
 * schematic-view.c
 *
 *
 * Authors:
 *  Richard Hult <rhult@hem.passagen.se>
 *  Ricardo Markiewicz <rmarkie@fi.uba.ar>
 *  Andres de Barbara <adebarbara@fi.uba.ar>
 *  Marc Lorber <lorber.marc@wanadoo.fr>
 *  Bernhard Schuster <bernhard@ahoi.io>
 *  Guido Trentalancia <guido@trentalancia.com>
 *  Daniel Dwek <todovirtual15@gmail.com>
 *
 * Web page: https://ahoi.io/project/oregano
 *
 * Copyright (C) 1999-2001  Richard Hult
 * Copyright (C) 2003,2006  Ricardo Markiewicz
 * Copyright (C) 2009-2012  Marc Lorber
 * Copyright (C) 2013-2014  Bernhard Schuster
 * Copyright (C) 2017       Guido Trentalancia
 * Copyright (C) 2022-2023  Daniel Dwek
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

#include <string.h>
#include <math.h>
#include <unistd.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <glib/gi18n.h>
#include <sys/time.h>
#include <cairo/cairo-features.h>

#include "schematic.h"
#include "schematic-view.h"
#include "part-browser.h"
#include "stock.h"
#include "oregano.h"
#include "load-library.h"
#include "netlist-helper.h"
#include "dialogs.h"
#include "cursors.h"
#include "file.h"
#include "settings.h"
#include "errors.h"
#include "engine.h"
#include "netlist-editor.h"
#include "sheet.h"
#include "sheet-item-factory.h"
#include "textbox-item.h"
#include "log-view.h"
#include "log.h"
#include "sheet-private.h"
#include "debug.h"

#include "part-item.h"
#include "part-private.h"
#include "sheet-private.h"
#include "wire-item.h"
#include "stack.h"

#define ZOOM_MIN 0.35
#define ZOOM_MAX 3

enum { CHANGED, RESET_TOOL, LAST_SIGNAL };

typedef enum {
	SCHEMATIC_TOOL_ARROW,
	SCHEMATIC_TOOL_PART,
	SCHEMATIC_TOOL_WIRE,
	SCHEMATIC_TOOL_TEXT
} SchematicTool;

typedef struct
{
	GtkWidget *log_window;
	GtkTextView *log_text;
	GtkBuilder *log_gui;
} LogInfo;

struct _SchematicView
{
	GObject parent;
	GtkWidget *toplevel;
	SchematicViewPriv *priv;
};

struct _SchematicViewClass
{
	GObjectClass parent_class;

	// Signals go here
	void (*changed)(SchematicView *schematic_view);
	void (*reset_tool)(SchematicView *schematic_view);
};

struct _SchematicViewPriv
{
	Schematic *schematic;

	Sheet *sheet;

	GtkPageSetup *page_setup;
	GtkPrintSettings *print_settings;

	gboolean empty;

	GtkActionGroup *action_group;
	GtkUIManager *ui_manager;

	gpointer browser;

	GtkWidget *logview;
	GtkPaned *paned;
	guint grid : 1;
	SchematicTool tool;
	int cursor;

	LogInfo *log_info;
};

struct _SchematicPriv
{
        char *oregano_version;
        char *title;
        char *filename;
        char *author;
        char *comments;
        char *netlist_filename;

//        SchematicColors colors;
//        SchematicPrintOptions *printoptions;

        // Data for various dialogs.
        gpointer settings;
        SimSettingsGui *sim_settings;
        gpointer simulation;

        GList *current_items;

        NodeStore *store;
        GHashTable *symbols;
        GHashTable *refdes_values;

        guint width;
        guint height;

        double zoom;

        gboolean dirty;

        Log *logstore;

        GtkTextBuffer *log;
        GtkTextTag *tag_error;
};

G_DEFINE_TYPE (SchematicView, schematic_view, G_TYPE_OBJECT)

// Class functions and members.
static void schematic_view_init (SchematicView *sv);
static void schematic_view_class_init (SchematicViewClass *klass);
static void schematic_view_dispose (GObject *object);
static void schematic_view_finalize (GObject *object);
static void schematic_view_load (SchematicView *sv, Schematic *sm);
static void schematic_view_do_load (SchematicView *sv, Schematic *sm, const gboolean reload);
static void schematic_view_reload (SchematicView *sv, Schematic *sm);
GtkUIManager *schematic_view_get_ui_manager (SchematicView *sv);

// Signal callbacks.
static void title_changed_callback (Schematic *schematic, char *new_title, SchematicView *sv);
static void set_focus (GtkWindow *window, GtkWidget *focus, SchematicView *sv);
static int delete_event (GtkWidget *widget, GdkEvent *event, SchematicView *sv);
static void data_received (GtkWidget *widget, GdkDragContext *context, gint x, gint y,
                           GtkSelectionData *selection_data, guint info, guint32 time,
                           SchematicView *sv);
static void item_data_added_callback (Schematic *schematic, ItemData *data, SchematicView *sv);
static void item_selection_changed_callback (SheetItem *item, gboolean selected, SchematicView *sv);
static void reset_tool_cb (Sheet *sheet, SchematicView *sv);

// Misc.
static int can_close (SchematicView *sv);
static void setup_dnd (SchematicView *sv);
static void set_tool (SchematicView *sv, SchematicTool tool);

static GList *schematic_view_list = NULL;
static GObjectClass *parent_class = NULL;
static guint schematic_view_signals[LAST_SIGNAL] = {0};

static void new_cmd (GtkWidget *widget, Schematic *sv)
{
	Schematic *new_sm;
	SchematicView *new_sv;

	new_sm = schematic_new ();
	new_sv = schematic_view_new (new_sm);

	gtk_widget_show_all (new_sv->toplevel);
}

static void properties_cmd (GtkWidget *widget, SchematicView *sv)
{
	Schematic *s;
	GtkBuilder *builder;
	GError *e = NULL;
	GtkWidget *window;
	GtkEntry *title, *author;
	GtkTextView *comments;
	GtkTextBuffer *buffer;
	gchar *s_title, *s_author, *s_comments;
	gint button;

	s = schematic_view_get_schematic (sv);

	if ((builder = gtk_builder_new ()) == NULL) {
		log_append (schematic_get_log_store (s), _ ("SchematicView"),
		            _ ("Could not create properties dialog"));
		return;
	}
	gtk_builder_set_translation_domain (builder, NULL);

	if (gtk_builder_add_from_file (builder, OREGANO_UIDIR "/properties.ui", &e) <= 0) {
		log_append_error (schematic_get_log_store (s), _ ("SchematicView"),
		                  _ ("Could not create properties dialog due to issues with " OREGANO_UIDIR
		                     "/properties.ui file."),
		                  e);
		g_clear_error (&e);
		return;
	}

	window = GTK_WIDGET (gtk_builder_get_object (builder, "properties"));
	title = GTK_ENTRY (gtk_builder_get_object (builder, "title"));
	author = GTK_ENTRY (gtk_builder_get_object (builder, "author"));
	comments = GTK_TEXT_VIEW (gtk_builder_get_object (builder, "comments"));
	buffer = gtk_text_view_get_buffer (comments);

	s_title = schematic_get_title (s);
	s_author = schematic_get_author (s);
	s_comments = schematic_get_comments (s);

	if (s_title)
		gtk_entry_set_text (title, s_title);
	if (s_author)
		gtk_entry_set_text (author, s_author);
	if (s_comments)
		gtk_text_buffer_set_text (buffer, s_comments, strlen (s_comments));

	button = gtk_dialog_run (GTK_DIALOG (window));

	if (button == GTK_RESPONSE_OK) {
		GtkTextIter start, end;

		gtk_text_buffer_get_start_iter (buffer, &start);
		gtk_text_buffer_get_end_iter (buffer, &end);

		s_title = (gchar *)gtk_entry_get_text (title);
		s_author = (gchar *)gtk_entry_get_text (author);
		s_comments = gtk_text_buffer_get_text (buffer, &start, &end, FALSE);

		schematic_set_title (s, s_title);
		schematic_set_author (s, s_author);
		schematic_set_comments (s, s_comments);

		g_free (s_comments);
	}

	gtk_widget_destroy (window);
}

static void find_file (GtkButton *button, GtkEntry *text)
{
	GtkWidget *dialog;

	dialog = gtk_file_chooser_dialog_new (_ ("Export to..."),
	                                      NULL,
	                                      GTK_FILE_CHOOSER_ACTION_SAVE,
	                                      _("_Cancel"),
	                                      GTK_RESPONSE_CANCEL,
	                                      _("_Open"),
	                                      GTK_RESPONSE_ACCEPT,
	                                      NULL);

	if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT) {
		char *filename;

		filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));
		gtk_entry_set_text (text, filename);
		g_free (filename);
	}

	gtk_widget_destroy (dialog);
}

static void export_cmd (GtkWidget *widget, SchematicView *sv)
{
	Schematic *s;
	GtkBuilder *builder;
	GError *e = NULL;
	GtkWidget *window;
	GtkWidget *warning;
	GtkWidget *w;
	GtkEntry *file = NULL;
	GtkComboBoxText *combo;
	gint button = GTK_RESPONSE_NONE;
	gint formats[5], fc;

	s = schematic_view_get_schematic (sv);

	if ((builder = gtk_builder_new ()) == NULL) {
		log_append (schematic_get_log_store (s), _ ("SchematicView"),
		            _ ("Could not create properties dialog"));
		return;
	}
	gtk_builder_set_translation_domain (builder, NULL);

	if (gtk_builder_add_from_file (builder, OREGANO_UIDIR "/export.ui", &e) <= 0) {
		log_append_error (schematic_get_log_store (s), _ ("SchematicView"),
		                  _ ("Could not create properties dialog due to issues with " OREGANO_UIDIR
		                     "/exportp.ui file."),
		                  e);
		g_clear_error (&e);
		return;
	}

	window = GTK_WIDGET (gtk_builder_get_object (builder, "export"));

	combo = GTK_COMBO_BOX_TEXT (gtk_builder_get_object (builder, "format"));
	fc = 0;
#ifdef CAIRO_HAS_SVG_SURFACE
	gtk_combo_box_text_append_text (combo, "Scalar Vector Graphics (SVG)");
	formats[fc++] = 0;
#endif
#ifdef CAIRO_HAS_PDF_SURFACE
	gtk_combo_box_text_append_text (combo, "Portable Document Format (PDF)");
	formats[fc++] = 1;
#endif
#ifdef CAIRO_HAS_PS_SURFACE
	gtk_combo_box_text_append_text (combo, "Postscript (PS)");
	formats[fc++] = 2;
#endif
#ifdef CAIRO_HAS_PNG_FUNCTIONS
	gtk_combo_box_text_append_text (combo, "Portable Network Graphics (PNG)");
	formats[fc++] = 3;
#endif

	file = GTK_ENTRY (gtk_builder_get_object (builder, "file"));

	w = GTK_WIDGET (gtk_builder_get_object (builder, "find"));
	g_signal_connect (G_OBJECT (w), "clicked", G_CALLBACK (find_file), file);

	gtk_combo_box_set_active (GTK_COMBO_BOX (combo), 0);

	button = gtk_dialog_run (GTK_DIALOG (window));

	if (button == GTK_RESPONSE_OK) {

		if (g_path_skip_root (gtk_entry_get_text (file)) == NULL) {

			warning = gtk_message_dialog_new_with_markup (
			    NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_WARNING, GTK_BUTTONS_OK,
			    _ ("<span weight=\"bold\" size=\"large\">No filename has "
			       "been chosen</span>\n\n"
			       "Please, click on the tag, beside, to select an output."));

			if (gtk_dialog_run (GTK_DIALOG (warning)) == GTK_RESPONSE_OK) {
				gtk_widget_destroy (GTK_WIDGET (warning));
				export_cmd (widget, sv);
				gtk_widget_destroy (GTK_WIDGET (window));
				return;
			}
		} else {

			int bg = 0;
			GtkSpinButton *spinw, *spinh;
			int color_scheme = 0;
			GtkWidget *w;
			int i = gtk_combo_box_get_active (GTK_COMBO_BOX (combo));

			w = GTK_WIDGET (gtk_builder_get_object (builder, "bgwhite"));
			if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (w)))
				bg = 1;
			w = GTK_WIDGET (gtk_builder_get_object (builder, "bgblack"));
			if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (w)))
				bg = 2;
			w = GTK_WIDGET (gtk_builder_get_object (builder, "color"));
			if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (w)))
				color_scheme = 1;

			spinw = GTK_SPIN_BUTTON (gtk_builder_get_object (builder, "export_width"));
			spinh = GTK_SPIN_BUTTON (gtk_builder_get_object (builder, "export_height"));
			schematic_export (
			    s, gtk_entry_get_text (file), gtk_spin_button_get_value_as_int (spinw),
			    gtk_spin_button_get_value_as_int (spinh), bg, color_scheme, formats[i]);
		}
	}

	gtk_widget_destroy (window);
}

static void page_properties_cmd (GtkWidget *widget, SchematicView *sv)
{
	if (sv->priv->page_setup) {
		g_object_unref (sv->priv->page_setup);
	}

	sv->priv->page_setup =
	    gtk_print_run_page_setup_dialog (NULL, sv->priv->page_setup, sv->priv->print_settings);
}

static void open_cmd (GtkWidget *widget, SchematicView *sv)
{
	Schematic *new_sm;
	SchematicView *new_sv, *t;
	char *fname, *uri = NULL;
	GList *iter;
	GError *e = NULL;

	fname = dialog_open_file (sv);
	if (!fname)
		return;

	// Repaint the other schematic windows before loading the new file.
	new_sv = NULL;
	for (iter = schematic_view_list; iter; iter = iter->next) {
		t = SCHEMATIC_VIEW (iter->data);
		if (t->priv->empty)
			new_sv = t;
		gtk_widget_queue_draw (GTK_WIDGET (t->toplevel));
	}

	while (gtk_events_pending ())
		gtk_main_iteration ();

	new_sm = schematic_read (fname, &e);
	if (e) {
		gchar *const msg = g_strdup_printf (_ ("Could not load file \"file://%s\""), fname);
		Schematic *old = schematic_view_get_schematic (sv);
		log_append_error (schematic_get_log_store (old), _ ("SchematicView"), msg, e);
		g_clear_error (&e);
		g_free (msg);
	}

	if (new_sm) {
		GtkRecentManager *manager;
		GtkRecentInfo *rc;

		manager = gtk_recent_manager_get_default ();
		uri = g_strdup_printf ("file://%s", fname);

		if (uri) {
			rc = gtk_recent_manager_lookup_item (manager, uri, &e);
			if (e) {
				g_clear_error (&e);
			} else {
				gtk_recent_manager_remove_item (manager, uri, &e);
				if (e) {
					gchar *const msg =
					    g_strdup_printf (_ ("Could not load recent file \"%s\""), uri);
					Schematic *old = schematic_view_get_schematic (sv);
					log_append_error (schematic_get_log_store (old), _ ("SchematicView"), msg, e);
					g_clear_error (&e);
					g_free (msg);
				}
			}
			gtk_recent_manager_add_item (manager, uri);
			if (rc)
				gtk_recent_info_unref (rc);
		}

		if (!new_sv)
			new_sv = schematic_view_new (new_sm);
		else
			schematic_view_load (new_sv, new_sm);

		gtk_widget_show_all (new_sv->toplevel);
		schematic_set_filename (new_sm, fname);
		schematic_set_title (new_sm, g_path_get_basename (fname));

		g_free (uri);
	}
	g_free (fname);
}

static void oregano_recent_open (GtkRecentChooser *chooser, SchematicView *sv)
{
	gchar *uri;
	const gchar *mime;
	GtkRecentInfo *item;
	GtkRecentManager *manager;
	Schematic *new_sm;
	SchematicView *new_sv = NULL;
	GError *e = NULL;

	uri = gtk_recent_chooser_get_current_uri (GTK_RECENT_CHOOSER (chooser));
	if (!uri)
		return;

	manager = gtk_recent_manager_get_default ();
	item = gtk_recent_manager_lookup_item (manager, uri, NULL);
	if (!item)
		return;
	// remove and re-add in order to update the ordering
	gtk_recent_manager_remove_item (manager, uri, &e);
	if (e) {
		gchar *const msg =
		    g_strdup_printf (_ ("Could not load recent file \"%s\"\n%s"), uri, e->message);
		oregano_error_with_title (_ ("Could not load recent file"), msg);
		g_clear_error (&e);
		g_free (msg);
	}
	gtk_recent_manager_add_item (manager, uri);

	mime = gtk_recent_info_get_mime_type (item);
	if (!mime || strcmp (mime, "application/x-oregano") != 0) {
		gchar *const msg = g_strdup_printf (_ ("Can not handle file with mimetype \"%s\""), mime);
		oregano_error_with_title (_ ("Could not load recent file"), msg);
		g_clear_error (&e);
		g_free (msg);

	} else {
		new_sm = schematic_read (uri, &e);
		if (e) {
			oregano_error_with_title (_ ("Could not load recent file"), e->message);
			g_clear_error (&e);
		}
		if (new_sm) {
			if (!new_sv)
				new_sv = schematic_view_new (new_sm);
			else
				schematic_view_load (new_sv, new_sm);

			gtk_widget_show_all (new_sv->toplevel);
			schematic_set_filename (new_sm, uri);
			schematic_set_title (new_sm, g_path_get_basename (uri));
		}
	}

	g_free (uri);
	gtk_recent_info_unref (item);
}

static GtkWidget *create_recent_chooser_menu (GtkRecentManager *manager)
{
	GtkWidget *menu;
	GtkRecentFilter *filter;

	menu = gtk_recent_chooser_menu_new_for_manager (manager);

	gtk_recent_chooser_set_limit (GTK_RECENT_CHOOSER (menu), 10);

	gtk_recent_chooser_set_local_only (GTK_RECENT_CHOOSER (menu), TRUE);
	gtk_recent_chooser_set_sort_type (GTK_RECENT_CHOOSER (menu), GTK_RECENT_SORT_MRU);

	filter = gtk_recent_filter_new ();
	gtk_recent_filter_add_mime_type (filter, "application/x-oregano");
	gtk_recent_filter_add_application (filter, g_get_application_name ());
	gtk_recent_chooser_add_filter (GTK_RECENT_CHOOSER (menu), filter);
	gtk_recent_chooser_set_filter (GTK_RECENT_CHOOSER (menu), filter);
	gtk_recent_chooser_set_local_only (GTK_RECENT_CHOOSER (menu), TRUE);
	g_signal_connect (menu, "item-activated", G_CALLBACK (oregano_recent_open), NULL);

	gtk_widget_show_all (menu);

	return menu;
}

static void display_recent_files (GtkWidget *menu, SchematicView *sv)
{
	SchematicViewPriv *priv = sv->priv;
	GtkWidget *menuitem;
	GtkWidget *recentmenu;
	GtkRecentManager *manager = NULL;

	manager = gtk_recent_manager_get_default ();
	menuitem =
	    gtk_ui_manager_get_widget (priv->ui_manager, "/MainMenu/MenuFile/DisplayRecentFiles");
	recentmenu = create_recent_chooser_menu (manager);
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), recentmenu);
	gtk_widget_show (menuitem);
}

static void save_cmd (GtkWidget *widget, SchematicView *sv)
{
	Schematic *sm;
	char *filename;
	GError *e = NULL;
	sm = sv->priv->schematic;
	filename = schematic_get_filename (sm);

	if (filename == NULL || !strcmp (filename, _ ("Untitled.oregano"))) {
		dialog_save_as (sv);
		return;
	} else {
		if (!schematic_save_file (sm, &e)) {
			log_append_error (schematic_get_log_store (sm), _ ("SchematicView"),
			                  _ ("Failed to save schematic file."), e);
			g_clear_error (&e);
		}
	}
}

static void save_as_cmd (GtkWidget *widget, SchematicView *sv) { dialog_save_as (sv); }

static void close_cmd (GtkWidget *widget, SchematicView *sv)
{
	if (can_close (sv)) {
		NG_DEBUG (" --- not dirty (anymore), do close schematic_view: %p -- vs -- "
		          "toplevel: %p",
		          sv, schematic_view_get_toplevel (sv));

		gtk_widget_destroy (GTK_WIDGET (schematic_view_get_toplevel (sv)));
		sv->toplevel = NULL;

		g_object_unref (G_OBJECT (sv));
	}
}

static void undo_cmd (GtkWidget *widget, SchematicView *sv)
{
	stack_data_t **sdata = NULL;
	ItemData *item_data = NULL;
	Coords abs_coords = { .0 };
	gint n_items = 0, i, j;
	GooCanvasGroup *canvas_group_wire = NULL;

	if (stack_get_size (undo_stack)) {
		sdata = stack_pop (undo_stack, &n_items);
		if (!sdata)
			return;

		for (i = 0; i < n_items; i++) {
			switch (sdata[i]->type) {
			case PART_CREATED:
				item_data = sheet_item_get_data (sdata[i]->s_item);
				abs_coords.x = - sdata[i]->u.moved.coords.x - 100.0;
				abs_coords.y = - sdata[i]->u.moved.coords.y - 100.0;
				item_data_set_pos (item_data, &abs_coords, EMIT_SIGNAL_CHANGED);
				break;
			case PART_MOVED:
				item_data = sheet_item_get_data (sdata[i]->s_item);
				abs_coords.x = sdata[i]->u.moved.coords.x - sdata[i]->u.moved.delta.x;
				abs_coords.y = sdata[i]->u.moved.coords.y - sdata[i]->u.moved.delta.y;
				item_data_set_pos (item_data, &abs_coords, EMIT_SIGNAL_MOVED | EMIT_SIGNAL_CHANGED);
				break;
			case PART_ROTATED:
				item_data = sheet_item_get_data (sdata[i]->s_item);
				item_data_rotate (item_data, - sdata[i]->u.rotated.angle, &sdata[i]->u.rotated.center,
							&sdata[i]->u.rotated.bbox1, &sdata[i]->u.rotated.bbox2, "undo_cmd");
				break;
			case PART_DELETED:
				for (j = 0; j < sdata[i]->u.deleted.canvas_group->items->len; j++) {
					goo_canvas_item_translate (GOO_CANVAS_ITEM (sdata[i]->u.deleted.canvas_group->items->pdata[j]),
									- sdata[i]->u.deleted.coords.x + 100.0,
									- sdata[i]->u.deleted.coords.y + 100.0);
				}
				break;
			case WIRE_CREATED:
				g_object_set (G_OBJECT (sdata[i]->s_item), "visibility", GOO_CANVAS_ITEM_INVISIBLE, NULL);
				break;
			case WIRE_MOVED:
				canvas_group_wire = GOO_CANVAS_GROUP (WIRE_ITEM (SHEET_ITEM (sdata[i]->s_item)));
				abs_coords.x = - sdata[i]->u.moved.delta.x;
				abs_coords.y = - sdata[i]->u.moved.delta.y;
				for (j = 0; j < canvas_group_wire->items->len; j++)
					goo_canvas_item_translate (GOO_CANVAS_ITEM (canvas_group_wire->items->pdata[j]),
								   abs_coords.x, abs_coords.y);
				break;
			case WIRE_ROTATED:
				item_data = sheet_item_get_data (sdata[i]->s_item);
				item_data_rotate (item_data, - sdata[i]->u.rotated.angle, &sdata[i]->u.rotated.center,
							&sdata[i]->u.rotated.bbox1, &sdata[i]->u.rotated.bbox2, "undo_cmd");
				break;
			case WIRE_DELETED:
				for (j = 0; j < sdata[i]->u.deleted.canvas_group->items->len; j++) {
					goo_canvas_item_translate (GOO_CANVAS_ITEM (sdata[i]->u.deleted.canvas_group->items->pdata[j]),
									- sdata[i]->u.deleted.coords.x + 100.0,
									- sdata[i]->u.deleted.coords.y + 100.0);
				}
				break;
			};

			stack_push (redo_stack, sdata[i], sv);
			g_free (sdata[i]);
			sdata[i] = NULL;
		}
	}
}

static void redo_cmd (GtkWidget *widget, SchematicView *sv)
{
	stack_data_t **sdata = NULL;
	ItemData *item_data = NULL;
	gint i, j, n_items = 0;
	GooCanvasGroup *canvas_group_wire = NULL;

	if (stack_get_size (redo_stack)) {
		sdata = stack_pop (redo_stack, &n_items);
		if (!sdata)
			return;

		for (i = 0; i < n_items; i++) {
			switch (sdata[i]->type) {
			case PART_CREATED:
				item_data = sheet_item_get_data (sdata[i]->s_item);
				item_data_set_pos (item_data, &sdata[i]->u.moved.coords, EMIT_SIGNAL_CHANGED);
				break;
			case PART_MOVED:
				item_data = sheet_item_get_data (sdata[i]->s_item);
				item_data_set_pos (item_data, &sdata[i]->u.moved.coords, EMIT_SIGNAL_CHANGED);
				break;
			case PART_ROTATED:
				item_data = sheet_item_get_data (sdata[i]->s_item);
				item_data_rotate (item_data, - sdata[i]->u.rotated.angle, &sdata[i]->u.rotated.center,
							&sdata[i]->u.rotated.bbox1, &sdata[i]->u.rotated.bbox2, "redo_cmd");
				break;
			case PART_DELETED:
				for (j = 0; j < sdata[i]->u.deleted.canvas_group->items->len; j++)
					goo_canvas_item_translate (GOO_CANVAS_ITEM (sdata[i]->u.deleted.canvas_group->items->pdata[j]),
									sdata[i]->u.deleted.coords.x - 100.0,
									sdata[i]->u.deleted.coords.y - 100.0);
				break;
			case WIRE_CREATED:
				g_object_set (G_OBJECT (sdata[i]->s_item), "visibility", GOO_CANVAS_ITEM_VISIBLE, NULL);
				break;
			case WIRE_MOVED:
				canvas_group_wire = GOO_CANVAS_GROUP (WIRE_ITEM (SHEET_ITEM (sdata[i]->s_item)));
				for (j = 0; j < canvas_group_wire->items->len; j++)
					goo_canvas_item_translate (GOO_CANVAS_ITEM (canvas_group_wire->items->pdata[j]),
								   sdata[i]->u.moved.delta.x, sdata[i]->u.moved.delta.y);
				g_object_set (G_OBJECT (sdata[i]->s_item), "visibility", GOO_CANVAS_ITEM_VISIBLE, NULL);
				break;
			case WIRE_ROTATED:
				item_data = sheet_item_get_data (sdata[i]->s_item);
				item_data_rotate (item_data, - sdata[i]->u.rotated.angle, &sdata[i]->u.rotated.center,
							&sdata[i]->u.rotated.bbox1, &sdata[i]->u.rotated.bbox2, "redo_cmd");
				break;
			case WIRE_DELETED:
				for (j = 0; j < sdata[i]->u.deleted.canvas_group->items->len; j++)
					goo_canvas_item_translate (GOO_CANVAS_ITEM (sdata[i]->u.deleted.canvas_group->items->pdata[j]),
									sdata[i]->u.deleted.coords.x - 100.0,
									sdata[i]->u.deleted.coords.y - 100.0);
				break;
			};

			stack_push (undo_stack, sdata[i], sv);
			g_free (sdata[i]);
			sdata[i] = NULL;
		}
	}
}

static void select_all_cmd (GtkWidget *widget, SchematicView *sv)
{
	sheet_select_all (sv->priv->sheet, TRUE);
}

static void deselect_all_cmd (GtkWidget *widget, SchematicView *sv)
{
	sheet_select_all (sv->priv->sheet, FALSE);
}

static void delete_cmd (GtkWidget *widget, SchematicView *sv)
{
	sheet_delete_selection (sv->priv->sheet);
}

static void rotate_cmd (GtkWidget *widget, SchematicView *sv)
{
	if (sv->priv->sheet->state == SHEET_STATE_NONE)
		sheet_rotate_selection (sv->priv->sheet, 90);
	else if (sv->priv->sheet->state == SHEET_STATE_FLOAT ||
	         sv->priv->sheet->state == SHEET_STATE_FLOAT_START)
		sheet_rotate_ghosts (sv->priv->sheet);
}

static void flip_horizontal_cmd (GtkWidget *widget, SchematicView *sv)
{
	GList *iter = NULL;

	g_return_if_fail (sv != NULL);
	g_return_if_fail (IS_SCHEMATIC_VIEW (sv));

	for (iter = sheet_get_floating_objects (sv->priv->sheet); iter; iter = iter->next)
		sv->priv->sheet->priv->selected_objects = g_list_prepend (sv->priv->sheet->priv->selected_objects, iter);

	for (iter = sv->priv->sheet->priv->selected_objects; iter; iter = iter->next) {
		if (sv->priv->sheet->state == SHEET_STATE_NONE)
			flip_items (sheet_get_selection (sv->priv->sheet), sv->priv->sheet, ID_FLIP_HORIZ);
		else if (sv->priv->sheet->state == SHEET_STATE_FLOAT || sv->priv->sheet->state == SHEET_STATE_FLOAT_START)
			flip_items (sheet_get_floating_objects (sv->priv->sheet), sv->priv->sheet, ID_FLIP_HORIZ);
	}
}

static void flip_vertical_cmd (GtkWidget *widget, SchematicView *sv)
{
#if 0
	Part *part;
	LibrarySymbol *symbol;
	GSList *objects;
	SymbolObject *object;

	g_return_if_fail (sv != NULL);
	g_return_if_fail (IS_SCHEMATIC_VIEW (sv));

	part = PART (sv->priv->sheet->priv->data);
	symbol = library_get_symbol (part->priv->symbol_name);

	for (objects = symbol->symbol_objects; objects; objects = objects->next) {
		object = (SymbolObject *)(objects->data);
		if (sv->priv->sheet->state == SHEET_STATE_NONE)
			sheet_flip_selection (sv, FALSE);
		else if (sv->priv->sheet->state == SHEET_STATE_FLOAT || sv->priv->sheet->state == SHEET_STATE_FLOAT_START)
			sheet_flip_ghosts (sv, FALSE);
	}
#endif
}

static void object_properties_cmd (GtkWidget *widget, SchematicView *sv)
{
	sheet_provide_object_properties (sv->priv->sheet);
}

/**
 * copy the currently selected items
 */
static void copy_cmd (GtkWidget *widget, SchematicView *sv)
{
	SheetItem *item;
	GList *iter;

	if (sv->priv->sheet->state != SHEET_STATE_NONE)
		return;

	sheet_clear_ghosts (sv->priv->sheet);
	clipboard_empty ();

	iter = sheet_get_selection (sv->priv->sheet);
	for (; iter; iter = iter->next) {
		item = iter->data;
		clipboard_add_object (G_OBJECT (item));
	}

	if (clipboard_is_empty ())
		gtk_action_set_sensitive (
		    gtk_ui_manager_get_action (sv->priv->ui_manager, "/MainMenu/MenuEdit/Paste"), FALSE);
	else
		gtk_action_set_sensitive (
		    gtk_ui_manager_get_action (sv->priv->ui_manager, "/MainMenu/MenuEdit/Paste"), TRUE);
}

/**
 * snip the currently selected items
 */
static void cut_cmd (GtkWidget *widget, SchematicView *sv)
{
	if (sv->priv->sheet->state != SHEET_STATE_NONE)
		return;

	copy_cmd (NULL, sv);
	sheet_delete_selection (sv->priv->sheet);

	if (clipboard_is_empty ())
		gtk_action_set_sensitive (
		    gtk_ui_manager_get_action (sv->priv->ui_manager, "/MainMenu/MenuEdit/Paste"), FALSE);
	else
		gtk_action_set_sensitive (
		    gtk_ui_manager_get_action (sv->priv->ui_manager, "/MainMenu/MenuEdit/Paste"), TRUE);
}

static void paste_objects (gpointer data, Sheet *sheet) { sheet_item_paste (sheet, data); }

static void paste_cmd (GtkWidget *widget, SchematicView *sv)
{
	if (sv->priv->sheet->state != SHEET_STATE_NONE)
		return;
	if (sheet_get_floating_objects (sv->priv->sheet))
		sheet_clear_ghosts (sv->priv->sheet);

	sheet_select_all (sv->priv->sheet, FALSE);
	clipboard_foreach ((ClipBoardFunction)paste_objects, sv->priv->sheet);
	if (sheet_get_floating_objects (sv->priv->sheet))
		sheet_connect_part_item_to_floating_group (sv->priv->sheet, (gpointer)sv);
}

static void about_cmd (GtkWidget *widget, Schematic *sm) { dialog_about (); }

static void log_cmd (GtkWidget *widget, SchematicView *sv) { schematic_view_log_show (sv, TRUE); }

static void show_label_cmd (GtkToggleAction *toggle, SchematicView *sv)
{
	gboolean show;
	Schematic *sm;
	Netlist netlist;
	GError *e = NULL;

	show = gtk_toggle_action_get_active (toggle);

	// Use of netlist_helper_create
	sm = sv->priv->schematic;
	netlist_helper_create (sm, &netlist, &e);
	if (e != NULL) {
		if (g_error_matches (e, OREGANO_ERROR, OREGANO_SIMULATE_ERROR_NO_CLAMP) ||
		    g_error_matches (e, OREGANO_ERROR, OREGANO_SIMULATE_ERROR_NO_GND) ||
		    g_error_matches (e, OREGANO_ERROR, OREGANO_SIMULATE_ERROR_IO_ERROR)) {
			log_append_error (schematic_get_log_store (sm), _ ("SchematicView"),
			                  _ ("Could not create a netlist."), e);
		} else {
			log_append_error (schematic_get_log_store (sm), _ ("SchematicView"),
			                  _ ("Unexpect failure occured."), e);
		}
		g_clear_error (&e);
		return;
	}

	sheet_show_node_labels (sv->priv->sheet, show);
	sheet_update_parts (sv->priv->sheet);
}

static void print_cmd (GtkWidget *widget, SchematicView *sv)
{
	schematic_print (schematic_view_get_schematic (sv), sv->priv->page_setup,
	                 sv->priv->print_settings, FALSE);
}

static void print_preview_cmd (GtkWidget *widget, SchematicView *sv)
{
	schematic_print (schematic_view_get_schematic (sv), sv->priv->page_setup,
	                 sv->priv->print_settings, TRUE);
}

static void quit_cmd (GtkWidget *widget, SchematicView *sv)
{
	GList *iter, *copy;

	// Duplicate the list as the list is modified during destruction.
	copy = g_list_copy (schematic_view_list);

	for (iter = copy; iter; iter = iter->next) {
		close_cmd (NULL, iter->data);
	}

	g_list_free (copy);
	g_application_quit (g_application_get_default ());
}

static void v_clamp_cmd (SchematicView *sv)
{
	LibraryPart *library_part;
	Coords pos;
	Sheet *sheet;
	Part *part;
	GList *lib;
	Library *l = NULL;

	set_tool (sv, SCHEMATIC_TOOL_PART);
	sheet = sv->priv->sheet;

	// Find default lib
	for (lib = oregano.libraries; lib; lib = lib->next) {
		l = (Library *)(lib->data);
		if (!g_ascii_strcasecmp (l->name, "Default"))
			break;
	}

	library_part = library_get_part (l, "Test Clamp");

	part = part_new_from_library_part (library_part);
	if (!part) {
		g_warning ("Clamp not found!");
		return;
	}
	pos.x = pos.y = 0;
	item_data_set_pos (ITEM_DATA (part), &pos, EMIT_SIGNAL_CHANGED);
	item_data_set_property (ITEM_DATA (part), "type", "v");

	sheet_select_all (sv->priv->sheet, FALSE);
	sheet_clear_ghosts (sv->priv->sheet);
	sheet_add_ghost_item (sv->priv->sheet, ITEM_DATA (part));
	sheet_connect_part_item_to_floating_group (sheet, (gpointer)sv);
}

static void tool_cmd (GtkAction *action, GtkRadioAction *current, SchematicView *sv)
{
	switch (gtk_radio_action_get_current_value (current)) {
	case 0:
		set_tool (sv, SCHEMATIC_TOOL_ARROW);
		break;
	case 1:
		set_tool (sv, SCHEMATIC_TOOL_TEXT);
		break;
	case 2:
		set_tool (sv, SCHEMATIC_TOOL_WIRE);
		break;
	case 3:
		v_clamp_cmd (sv);
	}
}

static void part_browser_cmd (GtkToggleAction *action, SchematicView *sv)
{
	part_browser_toggle_visibility (sv);
}

static void grid_toggle_snap_cmd (GtkToggleAction *action, SchematicView *sv)
{
	sv->priv->grid = !sv->priv->grid;

	grid_snap (sv->priv->sheet->grid, sv->priv->grid);
	grid_show (sv->priv->sheet->grid, sv->priv->grid);
}

static void log_toggle_visibility_cmd (GtkToggleAction *action, SchematicView *sv)
{
	g_return_if_fail (sv);
	g_return_if_fail (sv->priv->logview);

	GtkAllocation allocation;
	gboolean b = gtk_toggle_action_get_active (action);
	static int pos = 0;
	if (pos == 0)
		pos = gtk_paned_get_position (GTK_PANED (sv->priv->paned));

	gtk_widget_get_allocation (GTK_WIDGET (sv->priv->paned), &allocation);
	gtk_paned_set_position (GTK_PANED (sv->priv->paned), b ? pos : allocation.height);
}

static void smartsearch_cmd (GtkWidget *widget, SchematicView *sv)
{
	// Perform a research of a part within all librarys
	g_print ("TODO: search_smart ()\n");
}

static void netlist_cmd (GtkWidget *widget, SchematicView *sv)
{
	Schematic *sm;
	gchar *netlist_name;
	GError *e = NULL;
	OreganoEngine *engine;

	g_return_if_fail (sv != NULL);

	sm = sv->priv->schematic;

	netlist_name = dialog_netlist_file (sv);
	if (netlist_name == NULL)
		return;

	schematic_set_netlist_filename (sm, netlist_name);
	engine = oregano_engine_factory_create_engine (oregano.engine, sm);
	oregano_engine_generate_netlist (engine, netlist_name, &e);
	sheet_update_parts (sv->priv->sheet);

	g_free (netlist_name);
	g_object_unref (engine);

	if (e) {
		if (g_error_matches (e, OREGANO_ERROR, OREGANO_SIMULATE_ERROR_NO_CLAMP) ||
		    g_error_matches (e, OREGANO_ERROR, OREGANO_SIMULATE_ERROR_NO_GND) ||
		    g_error_matches (e, OREGANO_ERROR, OREGANO_SIMULATE_ERROR_IO_ERROR)) {
			log_append_error (schematic_get_log_store (sm), _ ("SchematicView"),
			                  _ ("Could not create a netlist."), e);
		} else {
			log_append_error (schematic_get_log_store (sm), _ ("SchematicView"),
			                  _ ("Unexpect failure occured."), e);
		}
		g_clear_error (&e);
		return;
	}
}

static void netlist_view_cmd (GtkWidget *widget, SchematicView *sv)
{
	netlist_editor_new_from_schematic_view (sv);
}

static void zoom_check (SchematicView *sv)
{
	double zoom;

	g_return_if_fail (sv != NULL);
	g_return_if_fail (IS_SCHEMATIC_VIEW (sv));

	sheet_get_zoom (sv->priv->sheet, &zoom);

	gtk_action_set_sensitive (
	    gtk_ui_manager_get_action (sv->priv->ui_manager, "/StandardToolbar/ZoomIn"),
	    zoom < ZOOM_MAX);
	gtk_action_set_sensitive (
	    gtk_ui_manager_get_action (sv->priv->ui_manager, "/StandardToolbar/ZoomOut"),
	    zoom > ZOOM_MIN);
}

static void zoom_in_cmd (GtkWidget *widget, SchematicView *sv)
{
	g_return_if_fail (sv != NULL);
	g_return_if_fail (IS_SCHEMATIC_VIEW (sv));

	sheet_zoom_step (sv->priv->sheet, 1.1);
	zoom_check (sv);
}

static void zoom_out_cmd (GtkWidget *widget, SchematicView *sv)
{
	g_return_if_fail (sv != NULL);
	g_return_if_fail (IS_SCHEMATIC_VIEW (sv));

	sheet_zoom_step (sv->priv->sheet, 0.9);
	zoom_check (sv);
}

static void zoom_cmd (GtkAction *action, GtkRadioAction *current, SchematicView *sv)
{
	switch (gtk_radio_action_get_current_value (current)) {
	case 0:
		g_object_set (G_OBJECT (sv->priv->sheet), "zoom", 0.50, NULL);
		break;
	case 1:
		g_object_set (G_OBJECT (sv->priv->sheet), "zoom", 0.75, NULL);
		break;
	case 2:
		g_object_set (G_OBJECT (sv->priv->sheet), "zoom", 1.0, NULL);
		break;
	case 3:
		g_object_set (G_OBJECT (sv->priv->sheet), "zoom", 1.25, NULL);
		break;
	case 4:
		g_object_set (G_OBJECT (sv->priv->sheet), "zoom", 1.5, NULL);
		break;
	}
	zoom_check (sv);
}

/*
 * Stretch the sheet horizontally.
 */
static void stretch_horizontal_cmd (GtkWidget *widget, SchematicView *sv)
{
	Schematic *sm;
	guint width;

	g_return_if_fail (sv != NULL);
	g_return_if_fail (IS_SCHEMATIC_VIEW (sv));

	sm = sv->priv->schematic;

	g_return_if_fail (sm != NULL);
	g_return_if_fail (IS_SCHEMATIC (sm));

	width = schematic_get_width (sm);
	schematic_set_width (sm, width * (1.0 + SCHEMATIC_STRETCH_FACTOR));

	if (sheet_replace (sv)) {
		schematic_view_reload (sv, sm);

		gtk_widget_show_all (schematic_view_get_toplevel (sv));
	}
}

/*
 * Stretch the sheet vertically.
 */
static void stretch_vertical_cmd (GtkWidget *widget, SchematicView *sv)
{
	Schematic *sm;
	guint height;

	g_return_if_fail (sv != NULL);
	g_return_if_fail (IS_SCHEMATIC_VIEW (sv));

	sm = sv->priv->schematic;

	g_return_if_fail (sm != NULL);
	g_return_if_fail (IS_SCHEMATIC (sm));

	height = schematic_get_height (sm);
	schematic_set_height (sm, height * (1.0 + SCHEMATIC_STRETCH_FACTOR));

	if (sheet_replace (sv)) {
		schematic_view_reload (sv, sm);

		gtk_widget_show_all (schematic_view_get_toplevel (sv));
	}
}

void schematic_view_simulate_cmd (GtkWidget *widget, SchematicView *sv)
{
	Schematic *sm;
	SimSettings *sim_settings;

	sm = schematic_view_get_schematic (sv);
	sim_settings = schematic_get_sim_settings (sm);

	// Before running the simulation for the first time, make
	// sure that the simulation settings are configured (this
	// includes the removal of missing output vectors from
	// previous application runs).
	if (!sim_settings->configured) {
		// The response_callback() function will take care
		// of launching the simulation again when the
		// simulation settings have been accepted.
		sim_settings->simulation_requested = TRUE;
		sim_settings_show (NULL, sv);
		return;
	}

	simulation_show_progress_bar (NULL, sv);

	sheet_update_parts (sv->priv->sheet);
	return;
}

static void schematic_view_class_init (SchematicViewClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);
	parent_class = g_type_class_peek_parent (klass);
	schematic_view_signals[CHANGED] =
	    g_signal_new ("changed", G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_FIRST,
	                  G_STRUCT_OFFSET (SchematicViewClass, changed), NULL, NULL,
	                  g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);
	schematic_view_signals[RESET_TOOL] =
	    g_signal_new ("reset_tool", G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_FIRST,
	                  G_STRUCT_OFFSET (SchematicViewClass, reset_tool), NULL, NULL,
	                  g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

	object_class->finalize = schematic_view_finalize;
	object_class->dispose = schematic_view_dispose;
}

static void schematic_view_init (SchematicView *sv)
{
	sv->priv = g_new0 (SchematicViewPriv, 1);
	sv->priv->log_info = g_new0 (LogInfo, 1);
	sv->priv->empty = TRUE;
	sv->priv->schematic = NULL;
	sv->priv->page_setup = NULL;
	sv->priv->print_settings = gtk_print_settings_new ();
	sv->priv->grid = TRUE;
	sv->priv->logview = NULL;
}

static void schematic_view_finalize (GObject *object)
{
	SchematicView *sv = SCHEMATIC_VIEW (object);

	if (sv->priv) {
		if (sv->priv->page_setup) {
			g_object_unref (sv->priv->page_setup);
		}

		g_object_unref (sv->priv->print_settings);
		g_object_unref (sv->priv->action_group);
		g_object_unref (sv->priv->ui_manager);
		g_object_unref (sv->priv->paned);

		g_free (sv->priv->log_info);		
		g_free (sv->priv);
		sv->priv = NULL;
	}

	if (sv->toplevel) {
		g_object_unref (G_OBJECT (sv->toplevel));
		sv->toplevel = NULL;
	}

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void schematic_view_dispose (GObject *object)
{
	SchematicView *sv = SCHEMATIC_VIEW (object);

	schematic_view_list = g_list_remove (schematic_view_list, sv);

	if (sv->toplevel) {
		// Disconnect focus signal
		g_signal_handlers_disconnect_by_func (G_OBJECT (sv->toplevel), G_CALLBACK (set_focus), sv);

		// Disconnect destroy event from toplevel
		g_signal_handlers_disconnect_by_func (G_OBJECT (sv->toplevel), G_CALLBACK (delete_event), sv);
	}

	if (sv->priv) {
		if (IS_SHEET (sv->priv->sheet)) {
			// Disconnect sheet's events
			g_signal_handlers_disconnect_by_func (G_OBJECT (sv->priv->sheet),
							      G_CALLBACK (sheet_event_callback), sv->priv->sheet);
		}

		g_object_unref (G_OBJECT (sv->priv->schematic));
	}

	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void show_help (GtkWidget *widget, SchematicView *sv)
{
	GError *e = NULL;

#if GTK_CHECK_VERSION (3,22,0)
	if (!gtk_show_uri_on_window (GTK_WINDOW (sv->toplevel), "help:oregano", gtk_get_current_event_time (),
#else
	if (!gtk_show_uri (gtk_widget_get_screen (sv->toplevel), "help:oregano", gtk_get_current_event_time (),
#endif
     				&e)) {
		NG_DEBUG ("Error %s\n", e->message);
		g_clear_error (&e);
	}
}

/**
 * Get a suitable value for the window size.
 *
 * Make the window occupy 3/4 of the screen with a padding of 50px in each
 * direction
 */
static void get_window_size (GtkWindow *window, GdkRectangle *rect)
{
	GdkRectangle monitor_rect;
#if GTK_CHECK_VERSION (3,22,0)
	GdkMonitor *monitor;
	GdkDisplay *display = gtk_widget_get_display (GTK_WIDGET (window));
#else
	gint monitor;
	GdkScreen *screen = gdk_screen_get_default ();
#endif

#if GTK_CHECK_VERSION (3,22,0)
	if (display) {
		monitor = gdk_display_get_primary_monitor (display);
		gdk_monitor_get_geometry (monitor, &monitor_rect);
#else
	if (screen) {
		monitor = gdk_screen_get_primary_monitor (screen);
		gdk_screen_get_monitor_geometry (screen, monitor, &monitor_rect);
#endif

		rect->width = 3 * (monitor_rect.width - 50) / 4;
		rect->height = 3 * (monitor_rect.height - 50) / 4;
	} else {
		g_warning ("No default screen found. Falling back to 1024x768 window size.");
		rect->width = 1024;
		rect->height = 768;
	}
}

static void set_window_size (SchematicView *sv)
{
	GdkRectangle rect;

	get_window_size (GTK_WINDOW (sv->toplevel), &rect);

	gtk_window_set_default_size (GTK_WINDOW (sv->toplevel), rect.width, rect.height);
}

#include "schematic-view-menu.h"

SchematicView *schematic_view_new (Schematic *schematic)
{
	SchematicView *sv;
	SchematicViewPriv *priv;
	Schematic *sm;
	GtkWidget *w, *hbox, *vbox;
	GtkWidget *toolbar, *part_browser;
	GtkWidget *logview;
	GtkWidget *logview_scrolled;
	GtkActionGroup *action_group;
	GtkUIManager *ui_manager;
	GtkAccelGroup *accel_group;
	GtkWidget *menubar;
	GtkGrid *grid;
	GtkPaned *paned;
	GError *e = NULL;
	GtkBuilder *builder;
	GdkRectangle window_size;

	g_return_val_if_fail (schematic, NULL);
	g_return_val_if_fail (IS_SCHEMATIC (schematic), NULL);

	sv = SCHEMATIC_VIEW (g_object_new (schematic_view_get_type (), NULL));

	schematic_view_list = g_list_prepend (schematic_view_list, sv);

	sm = schematic_view_get_schematic (sv);

	if ((builder = gtk_builder_new ()) == NULL) {
		log_append (schematic_get_log_store (sm), _ ("SchematicView"),
		            _ ("Failed to spawn builder object."));
		return NULL;
	}
	gtk_builder_set_translation_domain (builder, NULL);

	if (gtk_builder_add_from_file (builder, OREGANO_UIDIR "/oregano-main.ui", &e) <= 0) {
		log_append_error (schematic_get_log_store (sm), _ ("SchematicView"),
		                  _ ("Could not create main window from file."), e);
		g_clear_error (&e);
		return NULL;
	}

	sv->toplevel = GTK_WIDGET (gtk_builder_get_object (builder, "toplevel"));
	grid = GTK_GRID (gtk_builder_get_object (builder, "grid"));
	paned = GTK_PANED (gtk_builder_get_object (builder, "paned"));
	sv->priv->paned = paned;

	get_window_size (GTK_WINDOW (sv->toplevel), &window_size);
	if (schematic_get_width (schematic) < window_size.width)
		schematic_set_width (schematic, window_size.width);
	if (schematic_get_height (schematic) < window_size.height)
		schematic_set_height (schematic, window_size.height);

	sv->priv->sheet = SHEET (sheet_new ((double) schematic_get_width (schematic) + SHEET_BORDER, (double) schematic_get_height (schematic) + SHEET_BORDER));

	g_signal_connect (G_OBJECT (sv->priv->sheet), "event", G_CALLBACK (sheet_event_callback),
	                  sv->priv->sheet);

	vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);

	w = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (w), GTK_POLICY_AUTOMATIC,
	                                GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (w), GTK_SHADOW_IN);
	gtk_widget_set_hexpand (w, TRUE);
	gtk_widget_set_vexpand (w, TRUE);

	gtk_container_add (GTK_CONTAINER (w), GTK_WIDGET (sv->priv->sheet));
	gtk_box_pack_start (GTK_BOX (hbox), w, TRUE, TRUE, 5);

	part_browser = part_browser_create (sv);
	gtk_widget_set_hexpand (part_browser, FALSE);
	gtk_grid_attach_next_to (grid, part_browser, GTK_WIDGET (paned), GTK_POS_RIGHT, 1, 1);

	priv = sv->priv;
	priv->log_info->log_window = NULL;

	priv->action_group = action_group = gtk_action_group_new ("MenuActions");
	gtk_action_group_set_translation_domain (action_group, GETTEXT_PACKAGE);
	gtk_action_group_add_actions (action_group, entries, G_N_ELEMENTS (entries), sv);
	gtk_action_group_add_radio_actions (action_group, zoom_entries, G_N_ELEMENTS (zoom_entries), 2,
	                                    G_CALLBACK (zoom_cmd), sv);
	gtk_action_group_add_radio_actions (action_group, tools_entries, G_N_ELEMENTS (tools_entries),
	                                    0, G_CALLBACK (tool_cmd), sv);
	g_assert_cmpint (G_N_ELEMENTS (toggle_entries), >=, 4);
	gtk_action_group_add_toggle_actions (action_group, toggle_entries,
	                                     G_N_ELEMENTS (toggle_entries), sv);

	priv->ui_manager = ui_manager = gtk_ui_manager_new ();
	gtk_ui_manager_insert_action_group (ui_manager, action_group, 0);

	accel_group = gtk_ui_manager_get_accel_group (ui_manager);
	gtk_window_add_accel_group (GTK_WINDOW (sv->toplevel), accel_group);

	if (!gtk_ui_manager_add_ui_from_string (ui_manager, ui_description, -1, &e)) {
		g_message ("building menus failed: %s", e->message);
		g_clear_error (&e);
		return NULL;
	}

	menubar = gtk_ui_manager_get_widget (ui_manager, "/MainMenu");

	// Upgrade the menu bar with the recent files used by oregano
	display_recent_files (menubar, sv);
	gtk_box_pack_start (GTK_BOX (vbox), menubar, FALSE, FALSE, 0);

	toolbar = gtk_ui_manager_get_widget (ui_manager, "/StandardToolbar");
	gtk_toolbar_set_style (GTK_TOOLBAR (toolbar), GTK_TOOLBAR_ICONS);
	gtk_box_pack_start (GTK_BOX (vbox), toolbar, FALSE, FALSE, 0);

	// Fill the window
	gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, TRUE, 0);

	gtk_paned_pack1 (paned, vbox, FALSE, TRUE);
	gtk_widget_grab_focus (GTK_WIDGET (sv->priv->sheet));

	g_signal_connect_after (G_OBJECT (sv->toplevel), "set_focus", G_CALLBACK (set_focus), sv);
	g_signal_connect (G_OBJECT (sv->toplevel), "delete_event", G_CALLBACK (delete_event), sv);

	sv->priv->logview = logview = GTK_WIDGET (log_view_new ());
	log_view_set_store (LOG_VIEW (logview), schematic_get_log_store (schematic));

	logview_scrolled = gtk_scrolled_window_new (NULL, NULL);
	gtk_container_add (GTK_CONTAINER (logview_scrolled), logview);
	gtk_paned_pack2 (paned, logview_scrolled, FALSE, TRUE);

	setup_dnd (sv);

	// Set default sensitive for items
	gtk_action_set_sensitive (gtk_ui_manager_get_action (ui_manager, "/MainMenu/MenuEdit/ObjectProperties"), FALSE);
	gtk_action_set_sensitive (gtk_ui_manager_get_action (ui_manager, "/MainMenu/MenuEdit/Undo"), FALSE);
	gtk_action_set_sensitive (gtk_ui_manager_get_action (ui_manager, "/MainMenu/MenuEdit/Redo"), FALSE);
	gtk_action_set_sensitive (gtk_ui_manager_get_action (ui_manager, "/MainMenu/MenuEdit/Paste"), FALSE);

	g_signal_connect_object (G_OBJECT (sv), "reset_tool", G_CALLBACK (reset_tool_cb), G_OBJECT (sv),
	                         0);

	set_window_size (sv);

	schematic_view_load (sv, schematic);

	if (!schematic_get_title (sv->priv->schematic)) {
		gtk_window_set_title (GTK_WINDOW (sv->toplevel), _ ("Untitled.oregano"));
	} else {
		gtk_window_set_title (GTK_WINDOW (sv->toplevel), schematic_get_title (sv->priv->schematic));
	}
	schematic_set_filename (sv->priv->schematic, _ ("Untitled.oregano"));
	schematic_set_netlist_filename (sv->priv->schematic, _ ("Untitled.netlist"));

	gtk_window_set_application (GTK_WINDOW (schematic_view_get_toplevel (sv)),
	                            GTK_APPLICATION (g_application_get_default ()));

	return sv;
}

static void schematic_view_load (SchematicView *sv, Schematic *sm)
{
	schematic_view_do_load (sv, sm, FALSE);
}

static void schematic_view_do_load (SchematicView *sv, Schematic *sm, const gboolean reload)
{
	GList *list;
	g_return_if_fail (sv != NULL);
	g_return_if_fail (sm != NULL);
	g_return_if_fail (IS_SCHEMATIC (sm));

	if (!reload)
		g_return_if_fail (sv->priv->empty != FALSE);

	if (!reload && sv->priv->schematic)
		g_object_unref (G_OBJECT (sv->priv->schematic));

	sv->priv->schematic = sm;

	if (!reload) {
		g_signal_connect_object (G_OBJECT (sm), "title_changed", G_CALLBACK (title_changed_callback),
					 G_OBJECT (sv), 0);
		g_signal_connect_object (G_OBJECT (sm), "item_data_added",
					 G_CALLBACK (item_data_added_callback), G_OBJECT (sv), 0);
	}

	list = schematic_get_items (sm);

	for (; list; list = list->next)
		g_signal_emit_by_name (G_OBJECT (sm), "item_data_added", list->data);

	sheet_connect_node_dots_to_signals (sv->priv->sheet);

	// connect logview with logstore
	if (!reload)
		log_view_set_store (LOG_VIEW (sv->priv->logview), schematic_get_log_store (sm));

	schematic_set_dirty (sm, FALSE);

	g_list_free_full (list, g_object_unref);
}

static void schematic_view_reload (SchematicView *sv, Schematic *sm)
{
	schematic_view_do_load (sv, sm, TRUE);
}

GtkUIManager *schematic_view_get_ui_manager (SchematicView *sv)
{
	if (!sv)
		return NULL;

	return sv->priv->ui_manager;
}

static void item_selection_changed_callback (SheetItem *item, gboolean selected, SchematicView *sv)
{
	guint length;

	if (selected) {
		sheet_prepend_selected_object (sv->priv->sheet, item);
	} else {
		sheet_remove_selected_object (sv->priv->sheet, item);
	}

	length = sheet_get_selected_objects_length (sv->priv->sheet);
	if (length && item_data_has_properties (sheet_item_get_data (item)))
		gtk_action_set_sensitive (
		    gtk_ui_manager_get_action (sv->priv->ui_manager, "/MainMenu/MenuEdit/ObjectProperties"),
		    TRUE);
	else
		gtk_action_set_sensitive (
		    gtk_ui_manager_get_action (sv->priv->ui_manager, "/MainMenu/MenuEdit/ObjectProperties"),
		    FALSE);
}

// An ItemData got added; create an Item and set up the neccessary handlers.
static void item_data_added_callback (Schematic *schematic, ItemData *data, SchematicView *sv)
{
	Sheet *sheet;
	SheetItem *item;
	Coords pos = { .0 };
	cairo_matrix_t *mtx = NULL;
	gdouble ret_points[4] = { .0 };

	sheet = sv->priv->sheet;
	item = sheet_item_factory_create_sheet_item (sheet, data, &ret_points[0]);
	if (item != NULL) {
		sheet_item_place (item, sv->priv->sheet);
		g_object_set (G_OBJECT (item), "action_group", sv->priv->action_group, NULL);

		g_signal_connect (G_OBJECT (item), "selection_changed", G_CALLBACK (item_selection_changed_callback), sv);

		mtx = item_data_get_translate (data);
		if (mtx) {
			pos.x = mtx->x0;
			pos.y = mtx->y0;
			g_signal_emit_by_name (G_OBJECT (data), "created", &pos);
		} else {
			g_warning ("Could not get current position of %p item_data\n", data);
		}

		sheet_add_item (sheet, item);

		sv->priv->empty = FALSE;
		if (sv->priv->tool == SCHEMATIC_TOOL_PART)
			schematic_view_reset_tool (sv);

		// refresh _after_ we added it to the Sheet
		// this is required to properly display rotation, flip and others
		item_data_changed (data);
	}
}

static void title_changed_callback (Schematic *schematic, char *new_title, SchematicView *sv)
{
	g_return_if_fail (schematic != NULL);
	g_return_if_fail (IS_SCHEMATIC (schematic));
	g_return_if_fail (sv != NULL);
	g_return_if_fail (IS_SCHEMATIC_VIEW (sv));

	gtk_window_set_title (GTK_WINDOW (sv->toplevel), new_title);
}

static void set_focus (GtkWindow *window, GtkWidget *focus, SchematicView *sv)
{
	g_return_if_fail (sv->priv != NULL);
	g_return_if_fail (sv->priv->sheet != NULL);

	if (!gtk_window_get_focus (window))
		gtk_widget_grab_focus (GTK_WIDGET (sv->priv->sheet));
}

static int delete_event (GtkWidget *widget, GdkEvent *event, SchematicView *sv)
{
	if (can_close (sv)) {
		g_object_unref (G_OBJECT (sv));
		return FALSE;
	} else
		return TRUE;
}

static int can_close (SchematicView *sv)
{
	GtkWidget *dialog;
	gchar *text, *filename;
	GError *e = NULL;
	gint result;

	if (!schematic_is_dirty (schematic_view_get_schematic (sv)))
		return TRUE;

	filename = schematic_get_filename (sv->priv->schematic);
	text =
	    g_strdup_printf (_ ("<span weight=\"bold\" size=\"large\">Save "
	                        "changes to schematic %s before closing?</span>\n\nIf you don't save, "
	                        "all changes since you last saved will be permanently lost."),
	                     filename ? g_path_get_basename (filename) : NULL);

	dialog = gtk_message_dialog_new_with_markup (NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_WARNING,
	                                             GTK_BUTTONS_NONE, _ (text), NULL);

	gtk_dialog_add_buttons (GTK_DIALOG (dialog), _ ("Close _without Saving"), GTK_RESPONSE_NO,
	                        GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, GTK_STOCK_SAVE, GTK_RESPONSE_YES,
	                        NULL);

	g_free (text);

	result = gtk_dialog_run (GTK_DIALOG (dialog));

	gtk_widget_destroy (dialog);

	switch (result) {
	case GTK_RESPONSE_YES:
		schematic_save_file (sv->priv->schematic, &e);
		if (e) {
			g_clear_error (&e);
		}
		break;
	case GTK_RESPONSE_NO:
		schematic_set_dirty (sv->priv->schematic, FALSE);
		break;
	case GTK_RESPONSE_CANCEL:
	default:
		return FALSE;
	}
	return TRUE;
}

Sheet *schematic_view_get_sheet (SchematicView *sv)
{
	g_return_val_if_fail (sv != NULL, NULL);
	g_return_val_if_fail (IS_SCHEMATIC_VIEW (sv), NULL);

	return sv->priv->sheet;
}

void schematic_view_set_sheet (SchematicView *sv, Sheet *sheet)
{
	g_return_if_fail (sv != NULL);
	g_return_if_fail (IS_SCHEMATIC_VIEW (sv));

	sv->priv->sheet = sheet;
}

Schematic *schematic_view_get_schematic (SchematicView *sv)
{
	g_return_val_if_fail (sv != NULL, NULL);
	g_return_val_if_fail (IS_SCHEMATIC_VIEW (sv), NULL);

	return sv->priv->schematic;
}

void schematic_view_reset_tool (SchematicView *sv)
{
	g_return_if_fail (sv != NULL);
	g_return_if_fail (IS_SCHEMATIC_VIEW (sv));

	g_signal_emit_by_name (G_OBJECT (sv), "reset_tool");
}

static void setup_dnd (SchematicView *sv)
{
	static GtkTargetEntry dnd_types[] = {{"text/uri-list", 0, DRAG_URI_INFO},
	                                     {"x-application/oregano-part", 0, DRAG_PART_INFO}};
	static gint dnd_num_types = sizeof(dnd_types) / sizeof(dnd_types[0]);

	gtk_drag_dest_set (GTK_WIDGET (sv->priv->sheet),
	                   GTK_DEST_DEFAULT_MOTION | GTK_DEST_DEFAULT_HIGHLIGHT | GTK_DEST_DEFAULT_DROP,
	                   dnd_types, dnd_num_types, GDK_ACTION_MOVE);

	g_signal_connect (G_OBJECT (sv->priv->sheet), "drag_data_received", G_CALLBACK (data_received),
	                  "koko");
}

static void data_received (GtkWidget *widget, GdkDragContext *context, gint x, gint y,
                           GtkSelectionData *selection_data, guint info, guint32 time,
                           SchematicView *sv)
{
	gchar **files;
	GError *e = NULL;

	// Extract the filenames from the URI-list we received.
	switch (info) {
	case DRAG_PART_INFO:
		part_browser_dnd (selection_data, x, y);
		break;
	case DRAG_URI_INFO:
		files = g_strsplit ((gchar *)gtk_selection_data_get_data (selection_data), "\n", -1);
		if (files) {
			int i = 0;
			while (files[i]) {
				Schematic *new_sm = NULL;
				int l = strlen (files[i]);
				// Algo remains bad after the split: we agregate back into one \0
				files[i][l - 1] = '\0';

				if (l <= 0) {
					// Empty file name, ignore!
					i++;
					continue;
				}

				gchar *fname = files[i];

				new_sm = schematic_read (fname, &e);
				if (e) {
					//						g_warning ()
					g_clear_error (&e);
				}
				if (new_sm) {
					SchematicView *new_view;
					new_view = schematic_view_new (new_sm);
					if (new_view) {
						gtk_widget_show_all (new_view->toplevel);
						schematic_set_filename (new_sm, fname);
					}
					// schematic_set_title (new_sm, fname);
					while (gtk_events_pending ()) // Show something.
						gtk_main_iteration ();
				}
				i++;
			}
			g_strfreev (files);
		}
	}
	gtk_drag_finish (context, TRUE, TRUE, time);
}

static void set_tool (SchematicView *sv, SchematicTool tool)
{
	// Switch from this tool...
	switch (sv->priv->tool) {
	case SCHEMATIC_TOOL_ARROW:
		// In case we are handling a floating object, cancel that so that we can
		// change the tool.
		sheet_item_cancel_floating (sv->priv->sheet);
		break;
	case SCHEMATIC_TOOL_WIRE:
		/*if (sv->priv->create_wire_context) {
		                create_wire_exit (sv->priv->create_wire_context);
		                sv->priv->create_wire_context = NULL;
		}*/
		sheet_stop_create_wire (sv->priv->sheet);
		break;
	case SCHEMATIC_TOOL_TEXT:
		textbox_item_cancel_listen (sv->priv->sheet);
		break;
	case SCHEMATIC_TOOL_PART:
		sheet_item_cancel_floating (sv->priv->sheet);
	default:
		break;
	}

	// ...to this tool.
	switch (tool) {
	case SCHEMATIC_TOOL_ARROW:
		cursor_set_widget (GTK_WIDGET (sv->priv->sheet), OREGANO_CURSOR_LEFT_PTR);
		sv->priv->sheet->state = SHEET_STATE_NONE;
		break;
	case SCHEMATIC_TOOL_WIRE:
		cursor_set_widget (GTK_WIDGET (sv->priv->sheet), OREGANO_CURSOR_PENCIL);
		sv->priv->sheet->state = SHEET_STATE_WIRE;
		sheet_initiate_create_wire (sv->priv->sheet);
		break;
	case SCHEMATIC_TOOL_TEXT:
		cursor_set_widget (GTK_WIDGET (sv->priv->sheet), OREGANO_CURSOR_CARET);
		sv->priv->sheet->state = SHEET_STATE_TEXTBOX_WAIT;

		textbox_item_listen (sv->priv->sheet);
		break;
	case SCHEMATIC_TOOL_PART:
		cursor_set_widget (GTK_WIDGET (sv->priv->sheet), OREGANO_CURSOR_LEFT_PTR);
	default:
		break;
	}

	sv->priv->tool = tool;
}

static void reset_tool_cb (Sheet *sheet, SchematicView *sv)
{
	set_tool (sv, SCHEMATIC_TOOL_ARROW);

	gtk_radio_action_set_current_value (GTK_RADIO_ACTION (gtk_ui_manager_get_action (
	                                        sv->priv->ui_manager, "/StandardToolbar/Arrow")),
	                                    0);
}

gpointer schematic_view_get_browser (SchematicView *sv)
{
	g_return_val_if_fail (sv != NULL, NULL);
	g_return_val_if_fail (IS_SCHEMATIC_VIEW (sv), NULL);

	return sv->priv->browser;
}

void schematic_view_set_browser (SchematicView *sv, gpointer p)
{
	g_return_if_fail (sv != NULL);
	g_return_if_fail (IS_SCHEMATIC_VIEW (sv));

	sv->priv->browser = p;
}

static gboolean log_window_delete_event (GtkWidget *widget, GdkEvent *event, SchematicView *sv)
{
	sv->priv->log_info->log_window = NULL;
	return FALSE;
}

static void log_window_destroy_event (GtkWidget *widget, SchematicView *sv)
{
	sv->priv->log_info->log_window = NULL;
}

static void log_window_close_cb (GtkWidget *widget, SchematicView *sv)
{
	gtk_widget_destroy (sv->priv->log_info->log_window);
	sv->priv->log_info->log_window = NULL;
}

static void log_window_clear_cb (GtkWidget *widget, SchematicView *sv)
{
	GtkTextTagTable *tag;
	GtkTextBuffer *buf;

	tag = gtk_text_tag_table_new ();
	buf = gtk_text_buffer_new (GTK_TEXT_TAG_TABLE (tag));

	schematic_log_clear (sv->priv->schematic);

	gtk_text_view_set_buffer (GTK_TEXT_VIEW (sv->priv->log_info->log_text), GTK_TEXT_BUFFER (buf));
}

static void log_updated_callback (Schematic *sm, SchematicView *sv)
{
	schematic_view_log_show (sv, FALSE);
}

void schematic_view_log_show (SchematicView *sv, gboolean explicit)
{
	GtkWidget *w;
	Schematic *sm;
	GError *e = NULL;

	g_return_if_fail (sv != NULL);
	g_return_if_fail (IS_SCHEMATIC_VIEW (sv));

	sm = sv->priv->schematic;

	if ((sv->priv->log_info->log_gui = gtk_builder_new ()) == NULL) {
		log_append (schematic_get_log_store (sm), _ ("SchematicView"),
		            _ ("Could not create the log window."));
		return;
	}
	gtk_builder_set_translation_domain (sv->priv->log_info->log_gui, NULL);

	if (sv->priv->log_info->log_window == NULL) {
		// Create the log window if not already done.
		if (!explicit && !oregano.show_log)
			return;

		if (gtk_builder_add_from_file (sv->priv->log_info->log_gui, OREGANO_UIDIR "/log-window.ui",
		                               &e) <= 0) {

			log_append_error (schematic_get_log_store (sm), _ ("SchematicView"),
			                  _ ("Could not create the log window."), e);
			g_clear_error (&e);
			return;
		}

		sv->priv->log_info->log_window =
		    GTK_WIDGET (gtk_builder_get_object (sv->priv->log_info->log_gui, "log-window"));
		sv->priv->log_info->log_text =
		    GTK_TEXT_VIEW (gtk_builder_get_object (sv->priv->log_info->log_gui, "log-text"));

		gtk_window_set_default_size (GTK_WINDOW (sv->priv->log_info->log_window), 500, 250);

		// Delete event.
		g_signal_connect (G_OBJECT (sv->priv->log_info->log_window), "delete_event",
		                  G_CALLBACK (log_window_delete_event), sv);

		g_signal_connect (G_OBJECT (sv->priv->log_info->log_window), "destroy_event",
		                  G_CALLBACK (log_window_destroy_event), sv);

		w = GTK_WIDGET (gtk_builder_get_object (sv->priv->log_info->log_gui, "close-button"));
		g_signal_connect (G_OBJECT (w), "clicked", G_CALLBACK (log_window_close_cb), sv);

		w = GTK_WIDGET (gtk_builder_get_object (sv->priv->log_info->log_gui, "clear-button"));
		g_signal_connect (G_OBJECT (w), "clicked", G_CALLBACK (log_window_clear_cb), sv);
		g_signal_connect (G_OBJECT (sm), "log_updated", G_CALLBACK (log_updated_callback), sv);
	} else {
		gdk_window_raise (gtk_widget_get_window (sv->priv->log_info->log_window));
	}

	gtk_text_view_set_buffer (sv->priv->log_info->log_text, schematic_get_log_text (sm));

	gtk_widget_show_all (sv->priv->log_info->log_window);
}

gboolean schematic_view_get_log_window_exists (SchematicView *sv)
{
	if (sv->priv->log_info->log_window != NULL)
		return TRUE;
	else
		return FALSE;
}

GtkWidget *schematic_view_get_toplevel (SchematicView *sv) { return sv->toplevel; }

Schematic *schematic_view_get_schematic_from_sheet (Sheet *sheet)
{
	g_return_val_if_fail ((sheet != NULL), NULL);
	g_return_val_if_fail (IS_SHEET (sheet), NULL);

	GList *iter, *copy;
	Schematic *s = NULL;
	copy = g_list_copy (schematic_view_list); // really needed? probably not

	for (iter = copy; iter; iter = iter->next) {
		SchematicView *sv = SCHEMATIC_VIEW (iter->data);
		if (sv->priv->sheet == sheet) {
			s = sv->priv->schematic;
			break;
		}
	}
	g_list_free (copy);
	return s;
}

SchematicView *schematic_view_get_schematicview_from_sheet (Sheet *sheet)
{
	g_return_val_if_fail (sheet, NULL);
	g_return_val_if_fail (IS_SHEET (sheet), NULL);

	GList *iter, *copy;
	SchematicView *sv = NULL;

	copy = g_list_copy (schematic_view_list); // really needed? probably not

	for (iter = copy; iter; iter = iter->next) {
		sv = SCHEMATIC_VIEW (iter->data);
		if (sv->priv->sheet == sheet)
			break;
	}
	g_list_free (copy);
	return sv;
}

void run_context_menu (SchematicView *sv, GdkEventButton *event)
{
	GtkWidget *menu;

	g_return_if_fail (sv != NULL);
	g_return_if_fail (IS_SCHEMATIC_VIEW (sv));

	menu = gtk_ui_manager_get_widget (sv->priv->ui_manager, "/MainPopup");

	gtk_menu_popup (GTK_MENU (menu), NULL, NULL, NULL, sv, event->button, event->time);
}
