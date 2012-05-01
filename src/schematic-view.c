/*
 * schematic-view.c
 *
 *
 * Authors:
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
#include <math.h>
#include <unistd.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <glib/gi18n.h>
#include <sys/time.h>
#include <cairo/cairo-features.h>

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

#define NG_DEBUG(s) if (0) g_print ("%s\n", s)

#define ZOOM_MIN 0.35
#define ZOOM_MAX 3

enum {
	CHANGED,
	RESET_TOOL,
	LAST_SIGNAL
};

typedef enum {
	SCHEMATIC_TOOL_ARROW,
	SCHEMATIC_TOOL_PART,
	SCHEMATIC_TOOL_WIRE,
	SCHEMATIC_TOOL_TEXT
} SchematicTool;

typedef struct {
	GtkWidget 			*log_window;
	GtkTextView 		*log_text;
	GtkBuilder 			*log_gui;
} LogInfo;

struct _SchematicView
{
	GObject 		   parent;
	GtkWidget 		  *toplevel;
	SchematicViewPriv *priv;
};

struct _SchematicViewClass
{
	GObjectClass 		parent_class;

	// Signals go here
	void (*changed)		(SchematicView *schematic_view);
	void (*reset_tool) 	(SchematicView *schematic_view);
};

struct _SchematicViewPriv {
	Schematic 			*schematic;

	Sheet 				*sheet;

	GtkPageSetup 		*page_setup;
	GtkPrintSettings 	*print_settings;

	gboolean 			 empty;

	GtkActionGroup 		*action_group;
	GtkUIManager 		*ui_manager;

	GtkWidget 			*floating_part_browser;
	gpointer 			 browser;

	guint 				 grid : 1;
	SchematicTool 		 tool;
	int 				 cursor;

	LogInfo				*log_info;
};

G_DEFINE_TYPE (SchematicView, schematic_view, G_TYPE_OBJECT)

// Class functions and members.
static void schematic_view_init(SchematicView *sv);
static void schematic_view_class_init(SchematicViewClass *klass);
static void schematic_view_dispose(GObject *object);
static void schematic_view_finalize(GObject *object);
static void schematic_view_load (SchematicView *sv, Schematic *sm);

// Signal callbacks.
static void title_changed_callback (Schematic *schematic, char *new_title, 
    			SchematicView *sv);
static void set_focus (GtkWindow *window, GtkWidget *focus, SchematicView *sv);
static int  delete_event (GtkWidget *widget, GdkEvent *event, 
    			SchematicView *sv);
static void data_received (GtkWidget *widget, GdkDragContext *context,
				gint x, gint y, GtkSelectionData *selection_data, guint info,
				guint32 time, SchematicView *sv);
static void	item_data_added_callback (Schematic *schematic, ItemData *data, 
    			SchematicView *sv);
static void	item_selection_changed_callback (SheetItem *item, gboolean selected, 
    			SchematicView *sv);
static void	reset_tool_cb (Sheet *sheet, SchematicView *sv);

// Misc.
static int 			 can_close (SchematicView *sv);
static void 		 setup_dnd (SchematicView *sv);
static void 		 set_tool (SchematicView *sv, SchematicTool tool);

static GList 		*schematic_view_list = NULL;
static GObjectClass *parent_class = NULL;
static guint 		 schematic_view_signals[LAST_SIGNAL] = { 0 };

static void
new_cmd (GtkWidget *widget, Schematic *sv)
{
	Schematic *new_sm;
	SchematicView *new_sv;

	new_sm = schematic_new ();
	new_sv = schematic_view_new (new_sm);

	gtk_widget_show_all (new_sv->toplevel);
}

static void
properties_cmd (GtkWidget *widget, SchematicView *sv)
{
	Schematic *s;
	GtkBuilder *gui;
	GError *perror = NULL;
	gchar *msg;
	GtkWidget *window;
	GtkEntry *title, *author;
	GtkTextView *comments;
	GtkTextBuffer *buffer;
	gchar *s_title, *s_author, *s_comments;
	gint button;

	s = schematic_view_get_schematic (sv);

	if ((gui = gtk_builder_new ()) == NULL) {
		oregano_error (_("Could not create properties dialog"));
		return;
	} 
	else gtk_builder_set_translation_domain (gui, NULL);

	if (!g_file_test (OREGANO_UIDIR "/properties.ui", G_FILE_TEST_EXISTS)) {
		msg = g_strdup_printf (
		    _("The file %s could not be found. You might need to reinstall"
			    "Oregano to fix this."),
			OREGANO_UIDIR "/properties.ui");

		oregano_error_with_title (_("Could not create properties dialog"), msg);
		return;
	}

	if (gtk_builder_add_from_file (gui, OREGANO_UIDIR "/properties.ui", 
	    &perror) <= 0) {
		msg = perror->message;
		oregano_error_with_title (_("Could not create properties dialog"), msg);
		g_error_free (perror);
		return;
	}

	window = GTK_WIDGET (gtk_builder_get_object (gui, "properties"));
	title = GTK_ENTRY (gtk_builder_get_object (gui, "title"));
	author = GTK_ENTRY (gtk_builder_get_object (gui, "author")); 
	comments = GTK_TEXT_VIEW (gtk_builder_get_object (gui, "comments")); 
	buffer = gtk_text_view_get_buffer (comments);

	s_title = schematic_get_title (s);
	s_author = schematic_get_author (s);
	s_comments = schematic_get_comments (s);
	
	if (s_title)
		gtk_entry_set_text (title, s_title);
	if (s_author)
		gtk_entry_set_text (author, s_author);
	if (s_comments)
		gtk_text_buffer_set_text (buffer, s_comments, strlen(s_comments));

	button = gtk_dialog_run (GTK_DIALOG (window));

	if (button == GTK_RESPONSE_OK) {
		GtkTextIter start, end;

		gtk_text_buffer_get_start_iter (buffer, &start);
		gtk_text_buffer_get_end_iter (buffer, &end);

		s_title = (gchar *) gtk_entry_get_text (title);
		s_author = (gchar *) gtk_entry_get_text (author);
		s_comments = gtk_text_buffer_get_text (buffer, &start, &end, FALSE);

		schematic_set_title (s, s_title);
		schematic_set_author (s, s_author);
		schematic_set_comments (s, s_comments);

		g_free (s_comments);
	}

	gtk_widget_destroy (window);
}

static void
find_file (GtkButton *button, GtkEntry *text)
{
	GtkWidget *dialog;

	dialog = gtk_file_chooser_dialog_new (
			_("Export to..."),
			NULL,
			GTK_FILE_CHOOSER_ACTION_SAVE,
			GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
			NULL);

	if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT) {
		char *filename;

		filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));
		gtk_entry_set_text (text, filename);
		g_free (filename);
	}

	gtk_widget_destroy (dialog);
}

static void
export_cmd (GtkWidget *widget, SchematicView *sv)
{
	Schematic *s;
	GtkBuilder *gui;
	GError *perror = NULL;
	gchar *msg;
	GtkWidget *window;
	GtkWidget* warning;
	GtkWidget *w;
	GtkEntry *file = NULL;
	GtkComboBoxText *combo;
	gint button = GTK_RESPONSE_NONE;
	gint formats[5], fc;

	s = schematic_view_get_schematic (sv);

	if ((gui = gtk_builder_new ()) == NULL) {
		oregano_error (_("Could not create export dialog."));
		return;
	} 
	else gtk_builder_set_translation_domain (gui, NULL);
	
	if (!g_file_test (OREGANO_UIDIR "/export.ui", G_FILE_TEST_EXISTS)) {
		gchar *msg;
		msg = g_strdup_printf (_("The file %s could not be found. You might "
		    "need to reinstall Oregano to fix this."), 
		    OREGANO_UIDIR "/export.xml");

		oregano_error_with_title (_("Could not create export dialog."), msg);
		g_free (msg);
		return;
	}

	if (gtk_builder_add_from_file (gui, OREGANO_UIDIR "/export.ui", &perror) <= 0) {
		msg = perror->message;
		oregano_error_with_title (_("Could not create export dialog."), msg);
		g_error_free (perror);
		return;
	}

	window = GTK_WIDGET (gtk_builder_get_object (gui, "export"));
	
	combo = GTK_COMBO_BOX_TEXT (gtk_builder_get_object (gui, "format"));
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

	file = GTK_ENTRY (gtk_builder_get_object (gui, "file"));

	w = GTK_WIDGET (gtk_builder_get_object (gui, "find"));
	g_signal_connect (G_OBJECT (w), "clicked", 
	                  G_CALLBACK (find_file), file);

	gtk_combo_box_set_active (GTK_COMBO_BOX (combo), 0);

	button = gtk_dialog_run (GTK_DIALOG (window));
	
	if (button == GTK_RESPONSE_OK) {
		
	    if (g_path_skip_root (gtk_entry_get_text(file)) == NULL) {	

			warning = gtk_message_dialog_new_with_markup (
					NULL,
					GTK_DIALOG_MODAL,
					GTK_MESSAGE_WARNING,
					GTK_BUTTONS_OK, 
					_("<span weight=\"bold\" size=\"large\">No filename has "
					   "been chosen</span>\n\n"
					"Please, click on the tag, beside, to select an output.")); 

			if (gtk_dialog_run (GTK_DIALOG (warning)) == GTK_RESPONSE_OK)  {
				gtk_widget_destroy (GTK_WIDGET (warning));
				export_cmd (widget, sv);
				gtk_widget_destroy (GTK_WIDGET (window));
				return;
			}
		}
		else  {
			 
			int bg = 0;
			GtkSpinButton *spinw, *spinh;
			int color_scheme = 0;
			GtkWidget *w;
			int i = gtk_combo_box_get_active (GTK_COMBO_BOX (combo));

			w = GTK_WIDGET (gtk_builder_get_object (gui, "bgwhite"));
			if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (w))) bg = 1;
			w = GTK_WIDGET (gtk_builder_get_object (gui, "bgblack"));
			if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (w))) bg = 2;
			w = GTK_WIDGET (gtk_builder_get_object (gui, "color"));
			if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (w))) 
				color_scheme = 1;

			spinw = GTK_SPIN_BUTTON (gtk_builder_get_object (gui, "export_width"));
			spinh = GTK_SPIN_BUTTON (gtk_builder_get_object (gui, "export_height"));
			schematic_export (s,
				gtk_entry_get_text (file),
				gtk_spin_button_get_value_as_int (spinw),
				gtk_spin_button_get_value_as_int (spinh),
				bg,
				color_scheme,
				formats[i]);
		}
	}

	gtk_widget_destroy (window);
}

static void
page_properties_cmd (GtkWidget *widget, SchematicView *sv)
{
sv->priv->page_setup = gtk_print_run_page_setup_dialog (NULL,
					sv->priv->page_setup,
					sv->priv->print_settings);
}

static void
open_cmd (GtkWidget *widget, SchematicView *sv)
{
	Schematic *new_sm;
	SchematicView *new_sv;
	char *fname, *uri = NULL;
	GList *list;
	GError *error = NULL;

	fname = dialog_open_file (sv);
	if (fname == NULL)
			return;

	// Repaint the other schematic windows before loading the new file.
	new_sv = NULL;
	for (list = schematic_view_list; list; list = list->next) {
		if (SCHEMATIC_VIEW (list->data)->priv->empty)
			new_sv = SCHEMATIC_VIEW (list->data);
		gtk_widget_queue_draw (GTK_WIDGET (SCHEMATIC_VIEW (list->data)->toplevel));
	}

	while (gtk_events_pending ())
		gtk_main_iteration ();

	new_sm = schematic_read(fname, &error);
	if (error != NULL) {
		oregano_error_with_title (_("Could not load file"), error->message);
		g_error_free (error);
	}

	if (new_sm) {
		GtkRecentManager *manager;
		gchar *uri;
		
		manager = gtk_recent_manager_get_default ();
		uri = g_strdup_printf ("file://%s", fname);
		if (!gtk_recent_manager_has_item (manager, uri)	&&
		    (!(uri==NULL))) 	gtk_recent_manager_add_item (manager, uri);
		if (!new_sv)
			new_sv = schematic_view_new (new_sm);
		else
			schematic_view_load (new_sv, new_sm);

		gtk_widget_show_all (new_sv->toplevel);
		schematic_set_filename (new_sm, fname);
		schematic_set_title (new_sm, g_path_get_basename(fname));
	}

	g_list_free_full (list, g_object_unref);
	g_free (fname);
	g_free (uri);
}


static void
oregano_recent_open (GtkRecentChooser *chooser, SchematicView *sv)
{
	gchar *uri;
    const gchar *mime;
    GtkRecentInfo *item;
	GtkRecentManager *manager;
	Schematic *new_sm;
	SchematicView *new_sv = NULL;
	GError *error = NULL;

	uri = gtk_recent_chooser_get_current_uri (GTK_RECENT_CHOOSER (chooser));
	if (!uri) return;

	manager = gtk_recent_manager_get_default ();
	item = gtk_recent_manager_lookup_item (manager, uri, NULL);
	if (!item) return;

	mime = gtk_recent_info_get_mime_type (item);
	if (!mime) {
		g_warning (_("Unrecognized mime type"));
		return;
	}

	if (!strcmp (mime, "application/x-oregano")) {
		new_sm = schematic_read (uri, &error);
		if (error != NULL) {
			oregano_error_with_title (_("Could not load file"), error->message);
			g_error_free (error);
		}
		if (new_sm) {
			if (!new_sv)
				new_sv = schematic_view_new (new_sm);
			else
				schematic_view_load (new_sv, new_sm);

			gtk_widget_show_all (new_sv->toplevel);
			schematic_set_filename (new_sm, uri);
			schematic_set_title (new_sm, g_path_get_basename(uri));
		}
	}
	else
		g_warning (_("Unknown type of file can't open"));

	g_free(uri);
	gtk_recent_info_unref (item);
}


static GtkWidget *
create_recent_chooser_menu (GtkRecentManager *manager)
{
	GtkWidget *menu;
	GtkRecentFilter *filter;

	menu = gtk_recent_chooser_menu_new_for_manager (manager);

	gtk_recent_chooser_set_limit (GTK_RECENT_CHOOSER (menu), 10);

	gtk_recent_chooser_set_local_only (GTK_RECENT_CHOOSER (menu), TRUE);
	gtk_recent_chooser_set_sort_type (GTK_RECENT_CHOOSER (menu),
			GTK_RECENT_SORT_MRU); 
	
	filter = gtk_recent_filter_new ();
	gtk_recent_filter_add_mime_type (filter, "application/x-oregano");
	gtk_recent_filter_add_application (filter, g_get_application_name());
	gtk_recent_chooser_add_filter (GTK_RECENT_CHOOSER (menu), filter);
	gtk_recent_chooser_set_filter (GTK_RECENT_CHOOSER (menu), filter);
	gtk_recent_chooser_set_local_only (GTK_RECENT_CHOOSER (menu), TRUE);
	g_signal_connect (menu, "item-activated",
	        G_CALLBACK (oregano_recent_open), NULL);

	gtk_widget_show_all (menu);

	return menu;
}

static void
display_recent_files (GtkWidget *menu, SchematicView *sv)
{
	SchematicViewPriv *priv = sv->priv;
	GtkWidget *menuitem;
	GtkWidget *recentmenu;
	GtkRecentManager *manager = NULL;

	manager = gtk_recent_manager_get_default ();
	menuitem = gtk_ui_manager_get_widget (priv->ui_manager, 
	    "/MainMenu/MenuFile/DisplayRecentFiles");
	recentmenu = create_recent_chooser_menu (manager);
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), recentmenu);
	gtk_widget_show (menuitem);
}

static void
save_cmd (GtkWidget *widget, SchematicView *sv)
{
	Schematic *sm;
	char *filename;
	GError *error = NULL;
	sm = sv->priv->schematic;
	filename = schematic_get_filename (sm);

	if (filename == NULL || !strcmp (filename, _("Untitled.oregano"))) {
		dialog_save_as (sv);
		return;
	} 
	else {
		if (!schematic_save_file (sm, &error)) {
			oregano_error_with_title (_("Could not save schematic file"), 
			    error->message);
			g_error_free (error);
		}
	}
}

static void
save_as_cmd (GtkWidget *widget, SchematicView *sv)
{
	dialog_save_as (sv);
}

static void
close_cmd (GtkWidget *widget, SchematicView *sv)
{
	if (can_close (sv)) {
		g_object_unref (G_OBJECT (sv));
	}
}

static void
select_all_cmd (GtkWidget *widget, SchematicView *sv)
{
	sheet_select_all (sv->priv->sheet, TRUE);
}

static void
deselect_all_cmd (GtkWidget *widget, SchematicView *sv)
{
	sheet_select_all (sv->priv->sheet, FALSE);
}

static void
delete_cmd (GtkWidget *widget, SchematicView *sv)
{
	sheet_delete_selection (sv->priv->sheet);
}

static void
rotate_cmd (GtkWidget *widget, SchematicView *sv)
{
	if (sv->priv->sheet->state == SHEET_STATE_NONE)
		sheet_rotate_selection (sv->priv->sheet);
	else if (sv->priv->sheet->state == SHEET_STATE_FLOAT ||
			 sv->priv->sheet->state == SHEET_STATE_FLOAT_START)
		sheet_rotate_ghosts (sv->priv->sheet);
}

static void
flip_horizontal_cmd (GtkWidget *widget, SchematicView *sv)
{
	if (sv->priv->sheet->state == SHEET_STATE_NONE)
		sheet_flip_selection (sv->priv->sheet, TRUE);
	else if (sv->priv->sheet->state == SHEET_STATE_FLOAT ||
			 sv->priv->sheet->state == SHEET_STATE_FLOAT_START)
		sheet_flip_ghosts (sv->priv->sheet, TRUE);
}

static void
flip_vertical_cmd (GtkWidget *widget, SchematicView *sv)
{
	if (sv->priv->sheet->state == SHEET_STATE_NONE)
		sheet_flip_selection (sv->priv->sheet, FALSE);
	else if (sv->priv->sheet->state == SHEET_STATE_FLOAT ||
			 sv->priv->sheet->state == SHEET_STATE_FLOAT_START)
		sheet_flip_ghosts (sv->priv->sheet, FALSE);
}

static void
object_properties_cmd (GtkWidget *widget, SchematicView *sv)
{
	sheet_provide_object_properties (sv->priv->sheet);
}

static void
copy_cmd (GtkWidget *widget, SchematicView *sv)
{
	SheetItem *item;
	GList *list;

	if (sv->priv->sheet->state != SHEET_STATE_NONE)
		return;

	sheet_clear_ghosts (sv->priv->sheet);
	clipboard_empty ();

	list = sheet_get_selection (sv->priv->sheet);
	for (; list; list = list->next) {
		item = list->data;
		clipboard_add_object (G_OBJECT (item));
	}
	g_list_free_full (list, g_object_unref);

	if (clipboard_is_empty ())
		gtk_action_set_sensitive (gtk_ui_manager_get_action (sv->priv->ui_manager,
		    "/MainMenu/MenuEdit/Paste"), FALSE);
	else
		gtk_action_set_sensitive (gtk_ui_manager_get_action (sv->priv->ui_manager, 
		    "/MainMenu/MenuEdit/Paste"), TRUE);
}

static void
cut_cmd (GtkWidget *widget, SchematicView *sv)
{
	if (sv->priv->sheet->state != SHEET_STATE_NONE)
		return;

	copy_cmd (NULL, sv);
	sheet_delete_selection (sv->priv->sheet);

	if (clipboard_is_empty ())
		gtk_action_set_sensitive (gtk_ui_manager_get_action (sv->priv->ui_manager, 
		    "/MainMenu/MenuEdit/Paste"), FALSE);
	else
		gtk_action_set_sensitive (gtk_ui_manager_get_action (sv->priv->ui_manager, 
		    "/MainMenu/MenuEdit/Paste"), TRUE);
}

static void
paste_objects (gpointer data, Sheet *sheet)
{
	sheet_item_paste (sheet, data);
}

static void
paste_cmd (GtkWidget *widget, SchematicView *sv)
{
	if (sv->priv->sheet->state != SHEET_STATE_NONE)
		return;
	if (sheet_get_floating_objects (sv->priv->sheet))
		sheet_clear_ghosts (sv->priv->sheet);

	sheet_select_all (sv->priv->sheet, FALSE);
	clipboard_foreach ((ClipBoardFunction) paste_objects, sv->priv->sheet);
	if (sheet_get_floating_objects (sv->priv->sheet))
		sheet_connect_part_item_to_floating_group (sv->priv->sheet, (gpointer) sv);
}

static void
about_cmd (GtkWidget *widget, Schematic *sm)
{
	dialog_about ();
}

static void
log_cmd (GtkWidget *widget, SchematicView *sv)
{
	schematic_view_log_show (sv, TRUE);
}

static void
show_label_cmd (GtkToggleAction *toggle, SchematicView *sv)
{
	gboolean show;
	Schematic *sm;	
	Netlist netlist;
	GError *error = 0;

	show = gtk_toggle_action_get_active (toggle);

	// Use of netlist_helper_create 
	sm = sv->priv->schematic;
	netlist_helper_create (sm, &netlist, &error);
	if (error != NULL) {
		if (g_error_matches (error, OREGANO_ERROR, OREGANO_SIMULATE_ERROR_NO_CLAMP) ||
			g_error_matches (error, OREGANO_ERROR, OREGANO_SIMULATE_ERROR_NO_GND)   ||
			g_error_matches (error, OREGANO_ERROR, OREGANO_SIMULATE_ERROR_IO_ERROR)) {
				oregano_error_with_title (_("Could not create a netlist"), 
				    error->message);
				g_clear_error (&error);
		} 
		else
			oregano_error (_("An unexpected error has occurred"));
		g_clear_error (&error);
		return;
	}
	
	sheet_show_node_labels (sv->priv->sheet, show);
	sheet_update_parts (sv->priv->sheet);	
}

static void
print_cmd (GtkWidget *widget, SchematicView *sv)
{
	schematic_print (schematic_view_get_schematic (sv),
		sv->priv->page_setup,
		sv->priv->print_settings, FALSE);
}

static void
print_preview_cmd (GtkWidget *widget, SchematicView *sv)
{
	schematic_print (schematic_view_get_schematic (sv),
		sv->priv->page_setup,
		sv->priv->print_settings, TRUE);
}

static void
quit_cmd (GtkWidget *widget, SchematicView *sv)
{
	GList *list, *copy;

	// Duplicate the list as the list is modified during destruction.
	copy = g_list_copy (schematic_view_list);

	for (list = copy; list; list = list->next) {
		if (can_close (list->data))
			g_object_unref (list->data);
	}

	g_list_free (copy);
	g_list_free_full (list, g_object_unref);
}

static void
v_clamp_cmd (SchematicView *sv)
{
	LibraryPart *library_part;
	SheetPos pos;
	Sheet *sheet;
	Part *part;
	GList *lib;
	Library *l = NULL;

	set_tool (sv, SCHEMATIC_TOOL_PART);
	sheet = sv->priv->sheet;

	// Find default lib 
	for (lib = oregano.libraries; lib; lib = lib->next) {
		l = (Library *)(lib->data);
		if (!g_ascii_strcasecmp(l->name, "Default")) break;
	}

	library_part = library_get_part (l, "Test Clamp");
	
	part = part_new_from_library_part (library_part);
	if (!part) {
		g_warning ("Clamp not found!");
		return;
	}
	pos.x = pos.y = 0;
	item_data_set_pos (ITEM_DATA (part), &pos);
	item_data_set_property (ITEM_DATA (part), "type", "v");

	sheet_select_all (sv->priv->sheet, FALSE);
	sheet_clear_ghosts (sv->priv->sheet);
	sheet_add_ghost_item (sv->priv->sheet, ITEM_DATA (part));
	sheet_connect_part_item_to_floating_group (sheet, (gpointer) sv);
}

static void
tool_cmd (GtkAction *action, GtkRadioAction *current, SchematicView *sv)
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

static void
part_browser_cmd (GtkToggleAction *action, SchematicView *sv)
{
	part_browser_toggle_show (sv);
}

static void
grid_toggle_snap_cmd (GtkToggleAction *action, SchematicView *sv)
{
	sv->priv->grid = !sv->priv->grid;

	grid_snap (sv->priv->sheet->grid, sv->priv->grid);
	grid_show (sv->priv->sheet->grid, sv->priv->grid);
}

static void
smartsearch_cmd (GtkWidget *widget, SchematicView *sv)
{
	// Perform a research of a part within all librarys
	g_print ("TODO: search_smart ()\n");
}

static void
netlist_cmd (GtkWidget *widget, SchematicView *sv)
{
	Schematic *sm;
	gchar *netlist_name;
	GError *error = 0;
	OreganoEngine *engine;

	g_return_if_fail (sv != NULL);

	sm = sv->priv->schematic;

	netlist_name = dialog_netlist_file (sv);
	if (netlist_name == NULL)
		return;

	schematic_set_netlist_filename (sm, netlist_name);
	engine = oregano_engine_factory_create_engine (oregano.engine, sm);
	oregano_engine_generate_netlist (engine, netlist_name, &error);
	sheet_update_parts (sv->priv->sheet);

	g_free (netlist_name);
	g_object_unref (engine);
	
	if (error != NULL) {
		if (g_error_matches (error, OREGANO_ERROR, OREGANO_SIMULATE_ERROR_NO_CLAMP) ||
			g_error_matches (error, OREGANO_ERROR, OREGANO_SIMULATE_ERROR_NO_GND)   ||
			g_error_matches (error, OREGANO_ERROR, OREGANO_SIMULATE_ERROR_IO_ERROR)) {
				oregano_error_with_title (_("Could not create a netlist"), 
				    error->message);
				g_clear_error (&error);
		} 
		else 	
			oregano_error (_("An unexpected error has occurred"));
			return;
	}
}

static void
netlist_view_cmd (GtkWidget *widget, SchematicView *sv)
{
	netlist_editor_new_from_schematic_view (sv);
}

static void
zoom_check (SchematicView *sv)
{
	double zoom;

	g_return_if_fail (sv != NULL);
	g_return_if_fail (IS_SCHEMATIC_VIEW (sv));

	sheet_get_zoom (sv->priv->sheet, &zoom);

	gtk_action_set_sensitive (gtk_ui_manager_get_action (sv->priv->ui_manager, 
	    "/StandardToolbar/ZoomIn"), zoom < ZOOM_MAX);
	gtk_action_set_sensitive (gtk_ui_manager_get_action (sv->priv->ui_manager, 
	    "/StandardToolbar/ZoomOut"), zoom > ZOOM_MIN);
}

static void
zoom_in_cmd (GtkWidget *widget, SchematicView *sv)
{
	g_return_if_fail (sv != NULL);
	g_return_if_fail (IS_SCHEMATIC_VIEW (sv));

	sheet_change_zoom (sv->priv->sheet, 1.1);
	zoom_check (sv);
}

static void
zoom_out_cmd (GtkWidget *widget, SchematicView *sv)
{
	g_return_if_fail (sv != NULL);
	g_return_if_fail (IS_SCHEMATIC_VIEW (sv));

	sheet_change_zoom (sv->priv->sheet, 0.9);
	zoom_check (sv);
}

static void
zoom_cmd (GtkAction *action, GtkRadioAction *current, SchematicView *sv)
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

static void
simulate_cmd (GtkWidget *widget, SchematicView *sv)
{
	simulation_show (NULL, sv);

	sheet_update_parts (sv->priv->sheet);
	return;
}

static void
schematic_view_class_init (SchematicViewClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);
	parent_class = g_type_class_peek_parent (klass);
	schematic_view_signals[CHANGED] = g_signal_new ("changed",
					G_TYPE_FROM_CLASS (object_class),
					G_SIGNAL_RUN_FIRST,
					G_STRUCT_OFFSET (SchematicViewClass, changed),
					NULL,
					NULL,
					g_cclosure_marshal_VOID__VOID,
					G_TYPE_NONE, 0);
	schematic_view_signals[RESET_TOOL] = g_signal_new ("reset_tool",
					G_TYPE_FROM_CLASS (object_class),
					G_SIGNAL_RUN_FIRST,
					G_STRUCT_OFFSET (SchematicViewClass, reset_tool),
					NULL,
					NULL,
					g_cclosure_marshal_VOID__VOID,
					G_TYPE_NONE, 0);

	object_class->finalize = schematic_view_finalize;
	object_class->dispose = schematic_view_dispose;
}

static void
schematic_view_init (SchematicView *sv)
{
	sv->priv = g_new0 (SchematicViewPriv, 1);
	sv->priv->log_info = g_new0 (LogInfo, 1);
	sv->priv->empty = TRUE;
	sv->priv->schematic = NULL;
	sv->priv->page_setup = NULL;
	sv->priv->print_settings = gtk_print_settings_new ();
	sv->priv->grid = TRUE;
}

static void
schematic_view_finalize (GObject *object)
{
	SchematicView *sv = SCHEMATIC_VIEW (object);

	if (sv->priv) {
		g_free (sv->priv);
		sv->priv = NULL;
	}

	if (sv->toplevel) {
		gtk_widget_destroy (GTK_WIDGET (sv->toplevel));
		sv->toplevel = NULL;
	}

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
schematic_view_dispose (GObject *object)
{
	SchematicView *sv = SCHEMATIC_VIEW (object);

	schematic_view_list = g_list_remove (schematic_view_list, sv);

	// Disconnect sheet's events 
	g_signal_handlers_disconnect_by_func (G_OBJECT (sv->priv->sheet),
			G_CALLBACK (sheet_event_callback), sv->priv->sheet);

	// Disconnect focus signal 
	g_signal_handlers_disconnect_by_func (G_OBJECT (sv->toplevel),
			G_CALLBACK (set_focus), sv);

	// Disconnect destroy event from toplevel 
	g_signal_handlers_disconnect_by_func (G_OBJECT (sv->toplevel),
			G_CALLBACK (delete_event), sv);

	if (sv->priv) {
		g_object_unref (G_OBJECT (sv->priv->schematic));
	}
	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
show_help (GtkWidget *widget, SchematicView *sv)
{
	GError *error = NULL;

	GtkWidget *temp;
	temp = sv->toplevel;

	if (!gtk_show_uri (gtk_widget_get_screen (temp), "ghelp:oregano",
	             gtk_get_current_event_time (), &error)) {     
		NG_DEBUG (g_strdup_printf ("Error %s\n", error->message));
		g_error_free (error);
	}
}

#include "schematic-view-menu.h"

SchematicView *
schematic_view_new (Schematic *schematic)
{
	SchematicView *sv;
	SchematicViewPriv *priv;
	GtkWidget *w, *hbox, *vbox;
	GtkWidget *toolbar, *part_browser;
	GtkActionGroup *action_group;
	GtkUIManager *ui_manager;
	GtkAccelGroup *accel_group;
	GtkWidget *menubar;
	GtkTable *table;
	GError *error = NULL;
	GtkBuilder *gui;
	gchar *msg;

	g_return_val_if_fail (schematic != NULL, NULL);
	g_return_val_if_fail (IS_SCHEMATIC (schematic), NULL);

	sv = SCHEMATIC_VIEW (g_object_new (schematic_view_get_type(), NULL));

	schematic_view_list = g_list_prepend (schematic_view_list, sv);

	if ((gui = gtk_builder_new ()) == NULL) {
		oregano_error (_("Could not create main window."));
		return NULL;
	} 
	else gtk_builder_set_translation_domain (gui, NULL);

	if (!g_file_test (OREGANO_UIDIR "/oregano-main.ui",
		    G_FILE_TEST_EXISTS)) {
		msg = g_strdup_printf (
			_("The file %s could not be found. You might need to reinstall "
			  "Oregano to fix this"), OREGANO_UIDIR "/oregano-main.ui");
		oregano_error_with_title (_("Could not create main window."), msg);
		g_free (msg);
		return NULL;
	}

	if (gtk_builder_add_from_file (gui, OREGANO_UIDIR "/oregano-main.ui", 
	    &error) <= 0) {
		msg = error->message;
		oregano_error_with_title (_("Could not create main window."), msg);
		g_error_free (error);
		return NULL;
	}

	sv->toplevel = GTK_WIDGET (gtk_builder_get_object (gui, "toplevel"));
	table = GTK_TABLE (gtk_builder_get_object (gui, "table1"));

	sv->priv->sheet = SHEET (sheet_new (10000, 10000));

	g_signal_connect (G_OBJECT (sv->priv->sheet),
	    "event", G_CALLBACK (sheet_event_callback), 
	    sv->priv->sheet);

	vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);

	w = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (w), 
	    GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (w), GTK_SHADOW_IN);
	gtk_container_add (GTK_CONTAINER (w), GTK_WIDGET (sv->priv->sheet));
	gtk_box_pack_start (GTK_BOX (hbox), w, TRUE, TRUE, 5);
	
	part_browser = part_browser_create (sv);
	gtk_widget_set_hexpand (part_browser, FALSE);
	gtk_box_pack_start (GTK_BOX (hbox), part_browser, FALSE, FALSE, 5);
	
	priv = sv->priv;
	priv->log_info->log_window = NULL;

	priv->action_group = action_group = gtk_action_group_new ("MenuActions");
	gtk_action_group_set_translation_domain (priv->action_group, 
	    GETTEXT_PACKAGE);
	gtk_action_group_add_actions (action_group, entries, G_N_ELEMENTS (entries), 
	    sv);
	gtk_action_group_add_radio_actions (action_group, zoom_entries, 
	    G_N_ELEMENTS (zoom_entries), 2, G_CALLBACK (zoom_cmd), sv);
	gtk_action_group_add_radio_actions (action_group, tools_entries, 
	    G_N_ELEMENTS (tools_entries), 0, G_CALLBACK (tool_cmd), sv);
	gtk_action_group_add_toggle_actions (action_group, toggle_entries, 
	    G_N_ELEMENTS (toggle_entries), sv);

	priv->ui_manager = ui_manager = gtk_ui_manager_new ();
	gtk_ui_manager_insert_action_group (ui_manager, action_group, 0);

	accel_group = gtk_ui_manager_get_accel_group (ui_manager);
	gtk_window_add_accel_group (GTK_WINDOW (sv->toplevel), accel_group);

	error = NULL;
	if (!gtk_ui_manager_add_ui_from_string (ui_manager, ui_description, -1, 
	    &error)) {
		g_message ("building menus failed: %s", error->message);
		g_error_free (error);
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
	gtk_table_attach_defaults (table, vbox, 0, 1, 0, 1);

	gtk_window_set_focus (GTK_WINDOW (sv->toplevel), 
	    GTK_WIDGET (sv->priv->sheet)); 
	gtk_widget_grab_focus (GTK_WIDGET (sv->priv->sheet));

	g_signal_connect_after (G_OBJECT (sv->toplevel), "set_focus", 
	    G_CALLBACK (set_focus), sv);
	g_signal_connect (G_OBJECT (sv->toplevel), "delete_event", 
	    G_CALLBACK (delete_event), sv);

	setup_dnd (sv);

	// Set default sensitive for items 
	gtk_action_set_sensitive (gtk_ui_manager_get_action (ui_manager, 
	    "/MainMenu/MenuEdit/ObjectProperties"), FALSE);
	gtk_action_set_sensitive (gtk_ui_manager_get_action (ui_manager, 
	    "/MainMenu/MenuEdit/Paste"), FALSE);

	// Set the window size to something reasonable.
	gtk_window_set_default_size (GTK_WINDOW (sv->toplevel),
		3 * gdk_screen_width () / 5, 3 * gdk_screen_height () / 5);

	g_signal_connect_object (G_OBJECT (sv), "reset_tool",
		G_CALLBACK (reset_tool_cb), G_OBJECT (sv), 0);

	schematic_view_load (sv, schematic);

	if (!schematic_get_title (sv->priv->schematic)) {
		gtk_window_set_title (GTK_WINDOW (sv->toplevel), _("Untitled.oregano"));
	} 
	else {
		gtk_window_set_title (GTK_WINDOW (sv->toplevel),
					schematic_get_title (sv->priv->schematic));
	}
	schematic_set_filename (sv->priv->schematic, _("Untitled.oregano"));
	schematic_set_netlist_filename (sv->priv->schematic, _("Untitled.netlist"));

	return sv;
}

static void
schematic_view_load (SchematicView *sv, Schematic *sm)
{
	GList *list;		
	g_return_if_fail (sv->priv->empty != FALSE);
	g_return_if_fail (sm != NULL);

	if (sv->priv->schematic) g_object_unref (G_OBJECT (sv->priv->schematic));

	sv->priv->schematic = sm;

	g_signal_connect_object (G_OBJECT (sm), "title_changed",
			G_CALLBACK (title_changed_callback), G_OBJECT (sv), 0);
	g_signal_connect_object (G_OBJECT (sm), "item_data_added",
			G_CALLBACK (item_data_added_callback), G_OBJECT (sv), 0);

	list = schematic_get_items (sm);

	for (; list; list = list->next) {
		g_signal_emit_by_name (G_OBJECT (sm), "item_data_added", list->data);
	}

	sheet_connect_node_dots_to_signals (sv->priv->sheet);

	g_list_free_full (list, g_object_unref);
}

static void
item_selection_changed_callback (SheetItem *item, gboolean selected,
	SchematicView *sv)
{
	guint length;

	if (selected) {
		sheet_prepend_selected_object (sv->priv->sheet, item);
	} 
	else {
		sheet_remove_selected_object (sv->priv->sheet, item);
	}

	length = sheet_get_selected_objects_length (sv->priv->sheet);
	if (length && item_data_has_properties (sheet_item_get_data (item)))
		gtk_action_set_sensitive (gtk_ui_manager_get_action (sv->priv->ui_manager, 
		    "/MainMenu/MenuEdit/ObjectProperties"), TRUE);
	else
		gtk_action_set_sensitive (gtk_ui_manager_get_action (sv->priv->ui_manager, 
		    "/MainMenu/MenuEdit/ObjectProperties"), FALSE);
}

// An ItemData got added; create an Item and set up the neccessary handlers.
static void
item_data_added_callback (Schematic *schematic, ItemData *data,
	SchematicView *sv)
{
	Sheet *sheet;
	SheetItem *item;

	sheet = sv->priv->sheet;

	item = sheet_item_factory_create_sheet_item (sheet, data);

	if (item != NULL) {
		sheet_item_place (item, sv->priv->sheet);

		g_object_set (G_OBJECT (item), "action_group", sv->priv->action_group, 
		    NULL);

		g_signal_connect (G_OBJECT (item), "selection_changed",
			G_CALLBACK (item_selection_changed_callback), sv);

		sheet_add_item (sheet, item);
		sv->priv->empty = FALSE;
		if (sv->priv->tool == SCHEMATIC_TOOL_PART)
			schematic_view_reset_tool (sv);
	}
}

static void
title_changed_callback (Schematic *schematic, char *new_title, SchematicView *sv)
{
	g_return_if_fail (schematic != NULL);
	g_return_if_fail (IS_SCHEMATIC (schematic));
	g_return_if_fail (sv != NULL);
	g_return_if_fail (IS_SCHEMATIC_VIEW (sv));

	gtk_window_set_title (GTK_WINDOW (sv->toplevel), new_title);
}

static void
set_focus (GtkWindow *window, GtkWidget *focus, SchematicView *sv)
{
	g_return_if_fail (sv->priv != NULL);
	g_return_if_fail (sv->priv->sheet != NULL);

	if (!gtk_window_get_focus (window))
		gtk_widget_grab_focus (GTK_WIDGET (sv->priv->sheet));
}

static int
delete_event (GtkWidget *widget, GdkEvent *event, SchematicView *sv)
{
	if (can_close (sv)) {
		g_object_unref (G_OBJECT (sv));
		return FALSE;
	} 
	else
		return TRUE;
}

static int
can_close (SchematicView *sv)
{
	GtkWidget *dialog;
	gchar *text, *filename;
	GError *error = NULL;
	gint result;

	if (!schematic_is_dirty (schematic_view_get_schematic (sv)))
			return TRUE;
	
	filename = schematic_get_filename (sv->priv->schematic);
	text = g_strdup_printf (_("<span weight=\"bold\" size=\"large\">Save "
	    "changes to schematic %s before closing?</span>\n\nIf you don't save, "
	    "all changes since you last saved will be permanently lost."),
		filename ?	g_path_get_basename (filename) : NULL );

	dialog = gtk_message_dialog_new_with_markup (
			NULL,
			GTK_DIALOG_MODAL,
			GTK_MESSAGE_WARNING,
			GTK_BUTTONS_NONE,
			_(text), NULL);

	gtk_dialog_add_buttons (GTK_DIALOG (dialog),
			_("Close _without Saving"),
			GTK_RESPONSE_NO,
			GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			GTK_STOCK_SAVE, GTK_RESPONSE_YES,
			NULL);

	g_free (text);

	result = gtk_dialog_run (GTK_DIALOG (dialog));

	gtk_widget_destroy (dialog);

	switch (result) {
		case GTK_RESPONSE_YES:
			schematic_save_file (sv->priv->schematic, &error);
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

Sheet *
schematic_view_get_sheet (SchematicView *sv)
{
	g_return_val_if_fail (sv != NULL, NULL);
	g_return_val_if_fail (IS_SCHEMATIC_VIEW (sv), NULL);

	return sv->priv->sheet;
}

Schematic *
schematic_view_get_schematic (SchematicView *sv)
{
	g_return_val_if_fail (sv != NULL, NULL);
	g_return_val_if_fail (IS_SCHEMATIC_VIEW (sv), NULL);

	return sv->priv->schematic;
}

void
schematic_view_reset_tool (SchematicView *sv)
{
	g_return_if_fail (sv != NULL);
	g_return_if_fail (IS_SCHEMATIC_VIEW (sv));

	g_signal_emit_by_name (G_OBJECT (sv), "reset_tool");
}

static void
setup_dnd (SchematicView *sv)
{
	static GtkTargetEntry dnd_types[] = {
			{ "text/uri-list", 0, DRAG_URI_INFO },
			{ "x-application/oregano-part", 0, DRAG_PART_INFO }
	};
	static gint dnd_num_types = sizeof (dnd_types) / sizeof (dnd_types[0]);

	gtk_drag_dest_set (GTK_WIDGET (sv->priv->sheet),
			GTK_DEST_DEFAULT_MOTION |
			GTK_DEST_DEFAULT_HIGHLIGHT |
			GTK_DEST_DEFAULT_DROP,
			dnd_types, dnd_num_types, GDK_ACTION_MOVE);

	g_signal_connect (G_OBJECT (sv->priv->sheet), "drag_data_received",
			G_CALLBACK (data_received),
			"koko");
}

static void
data_received (GtkWidget *widget, GdkDragContext *context, gint x, gint y,
		 GtkSelectionData *selection_data, guint info, guint32 time,
		 SchematicView *sv)
{
	gchar **files;
	GError *error = NULL;

	// Extract the filenames from the URI-list we received.
	switch (info) {
		case DRAG_PART_INFO:
			part_browser_dnd (selection_data, x, y);
		break;
		case DRAG_URI_INFO:
			files = g_strsplit ((gchar *) gtk_selection_data_get_data (selection_data), "\n", -1);
			if (files) {
				int i=0;
				while (files[i]) {
					Schematic *new_sm = NULL;
					int l = strlen (files[i]);
					// Algo remains bad after the split: we agregate back into one \0 
					files[i][l-1] = '\0';

					if (l <= 0) {
						// Empty file name, ignore! 
						i++;
						continue;
					}

					gchar *fname = files[i];

					new_sm = schematic_read (fname, &error);
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
				}
	}
	gtk_drag_finish (context, TRUE, TRUE, time);
}


static void
set_tool (SchematicView *sv, SchematicTool tool)
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
		cursor_set_widget (GTK_WIDGET (sv->priv->sheet),
			OREGANO_CURSOR_PENCIL);
		sv->priv->sheet->state = SHEET_STATE_WIRE;
		sheet_initiate_create_wire (sv->priv->sheet);
		break;
	case SCHEMATIC_TOOL_TEXT:
		cursor_set_widget (GTK_WIDGET (sv->priv->sheet), OREGANO_CURSOR_CARET);
		sv->priv->sheet->state = SHEET_STATE_TEXTBOX_WAIT;

		textbox_item_listen (sv->priv->sheet);
		break;
	case SCHEMATIC_TOOL_PART:
		cursor_set_widget (GTK_WIDGET (sv->priv->sheet), 
		                   OREGANO_CURSOR_LEFT_PTR);
	default:
		break;
	}

	sv->priv->tool = tool;
}

static void
reset_tool_cb (Sheet *sheet, SchematicView *sv)
{
	set_tool (sv, SCHEMATIC_TOOL_ARROW);

	gtk_radio_action_set_current_value (GTK_RADIO_ACTION (
	    gtk_ui_manager_get_action (sv->priv->ui_manager, 
		    "/StandardToolbar/Arrow")), 0);
}

gpointer
schematic_view_get_browser (SchematicView *sv)
{
	g_return_val_if_fail (sv != NULL, NULL);
	g_return_val_if_fail (IS_SCHEMATIC_VIEW (sv), NULL);

	return sv->priv->browser;
}

void
schematic_view_set_browser (SchematicView *sv, gpointer p)
{
	g_return_if_fail (sv != NULL);
	g_return_if_fail (IS_SCHEMATIC_VIEW (sv));

	sv->priv->browser = p;
}

static gboolean
log_window_delete_event (GtkWidget *widget, GdkEvent *event, SchematicView *sv)
{
	sv->priv->log_info->log_window = NULL;
	return FALSE;
}

static void
log_window_destroy_event (GtkWidget *widget, SchematicView *sv)
{
	sv->priv->log_info->log_window = NULL;
}

static void
log_window_close_cb (GtkWidget *widget, SchematicView *sv)
{
	gtk_widget_destroy (sv->priv->log_info->log_window);
	sv->priv->log_info->log_window = NULL;
}

static void
log_window_clear_cb (GtkWidget *widget, SchematicView *sv)
{
	GtkTextTagTable *tag;
	GtkTextBuffer *buf;

	tag = gtk_text_tag_table_new ();
	buf = gtk_text_buffer_new (GTK_TEXT_TAG_TABLE (tag));

	schematic_log_clear (sv->priv->schematic);

	gtk_text_view_set_buffer (GTK_TEXT_VIEW (sv->priv->log_info->log_text),
			GTK_TEXT_BUFFER (buf));
}

static void
log_updated_callback (Schematic *sm, SchematicView *sv)
{
	schematic_view_log_show (sv, FALSE);
}

void
schematic_view_log_show (SchematicView *sv, gboolean explicit)
{
	GtkWidget *w;
	gchar *msg;
	Schematic *sm;
	GError *perror = NULL;

	g_return_if_fail (sv != NULL);
	g_return_if_fail (IS_SCHEMATIC_VIEW (sv));

	sm = sv->priv->schematic;

	if ((sv->priv->log_info->log_gui = gtk_builder_new ()) == NULL) {
		oregano_error (_("Could not create the log window."));
		return;
	} 
	else 
		gtk_builder_set_translation_domain (sv->priv->log_info->log_gui, NULL);

	if (sv->priv->log_info->log_window == NULL) {
		// Create the log window if not already done.
		if (!explicit && !oregano.show_log)
			return;

		if (!g_file_test (OREGANO_UIDIR "/log-window.ui", G_FILE_TEST_EXISTS)) {
			msg = g_strdup_printf (
				_("The file %s could not be found. You might need to reinstall Oregano to fix this."),
					OREGANO_UIDIR "/log-window.ui");
			oregano_error_with_title ( _("Could not create the log window"), msg);
			g_free (msg);
			return;
		}

		if (gtk_builder_add_from_file (sv->priv->log_info->log_gui, 
		    OREGANO_UIDIR "/log-window.ui", &perror) <= 0) {
			msg = perror->message;
			oregano_error_with_title (_("Could not create the log window."), msg);
			g_error_free (perror);
			return;
		}

		sv->priv->log_info->log_window = GTK_WIDGET (
		    gtk_builder_get_object (sv->priv->log_info->log_gui,
			"log-window"));
		sv->priv->log_info->log_text = GTK_TEXT_VIEW (
			gtk_builder_get_object (sv->priv->log_info->log_gui,
			"log-text"));

		gtk_window_set_default_size (GTK_WINDOW (sv->priv->log_info->log_window),
				500, 250);

		// Delete event.
		g_signal_connect (G_OBJECT (sv->priv->log_info->log_window), 
		    "delete_event", G_CALLBACK (log_window_delete_event), sv);

		g_signal_connect (G_OBJECT (sv->priv->log_info->log_window), 
		    "destroy_event", G_CALLBACK (log_window_destroy_event), sv);

		w = GTK_WIDGET (gtk_builder_get_object (sv->priv->log_info->log_gui, 
		    "close-button"));
		g_signal_connect (G_OBJECT (w), "clicked", 
		    G_CALLBACK (log_window_close_cb), sv);

		w = GTK_WIDGET (gtk_builder_get_object (sv->priv->log_info->log_gui, 
		    "clear-button"));
		g_signal_connect (G_OBJECT (w), "clicked", 
		    G_CALLBACK (log_window_clear_cb), sv);
		g_signal_connect (G_OBJECT (sm), "log_updated", 
		    G_CALLBACK (log_updated_callback), sv);
	} 
	else {
		gdk_window_raise (gtk_widget_get_window (sv->priv->log_info->log_window));
	}

	gtk_text_view_set_buffer (sv->priv->log_info->log_text, 
	        schematic_get_log_text (sm));

	gtk_widget_show_all (sv->priv->log_info->log_window);
}

gboolean
schematic_view_get_log_window_exists (SchematicView *sv)
{
	if (sv->priv->log_info->log_window != NULL)
			return TRUE;
	else
			return FALSE;
}

GtkWidget	*	
schematic_view_get_toplevel (SchematicView *sv)
{
	return sv->toplevel;
}

Schematic	   *
schematic_view_get_schematic_from_sheet (Sheet *sheet)
{
	g_return_val_if_fail ((sheet != NULL), NULL);
	g_return_val_if_fail (IS_SHEET (sheet), NULL);
	
	GList *list, *copy;

	copy = g_list_copy (schematic_view_list);

	for (list=copy; list; list = list->next) {
		if (SCHEMATIC_VIEW (list->data)->priv->sheet == sheet) {
			return SCHEMATIC_VIEW (list->data)->priv->schematic;
		}
	}
	g_list_free (copy);
	g_list_free_full (list, g_object_unref);
	return NULL;
}

SchematicView  *
schematic_view_get_schematicview_from_sheet (Sheet *sheet)
{
	g_return_val_if_fail ((sheet != NULL), NULL);
	g_return_val_if_fail (IS_SHEET (sheet), NULL);
	
	GList *list, *copy;

	copy = g_list_copy (schematic_view_list);

	for (list=copy; list; list = list->next) {
		if (SCHEMATIC_VIEW (list->data)->priv->sheet == sheet)
			return SCHEMATIC_VIEW (list->data);
	}
	g_list_free (copy);
	g_list_free_full (list, g_object_unref);
	return NULL;
}

void
run_context_menu (SchematicView *sv, GdkEventButton *event)
{
	GtkWidget *menu;

	g_return_if_fail (sv != NULL);
	g_return_if_fail (IS_SCHEMATIC_VIEW (sv));

	menu = gtk_ui_manager_get_widget (sv->priv->ui_manager, "/MainPopup");

	gtk_menu_popup (GTK_MENU (menu), NULL, NULL, NULL, sv, event->button, 
	    event->time);
}
