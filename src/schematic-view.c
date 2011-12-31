/*
 * schematic-view.c
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
#include <string.h>
#include <unistd.h>
#include <glade/glade.h>
#include <gtk/gtk.h>
#include <libgnomeui/gnome-app.h>
#include <libbonobo-2.0/libbonobo.h>
#include <sys/time.h>
#include <cairo/cairo-features.h>

#include "sheet-private.h"
#include "schematic-view.h"
#include "schematic.h"
#include "part-browser.h"
#include "stock.h"
#include "main.h"
#include "load-schematic.h"
#include "load-common.h"
#include "netlist.h"
#include "dialogs.h"
#include "cursors.h"
#include "save-schematic.h"
#include "file.h"
#include "settings.h"
#include "sim-settings.h"
#include "simulation.h"
#include "smallicon.h"
#include "plot.h"
#include "sheet-item-factory.h"
#include "part-item.h"
#include "errors.h"
#include "node-item.h"
#include "engine.h"

/* remove: */
#include "create-wire.h"

/*
 * Pixmaps.
 */
#include "pixmaps/plot.xpm"
#include "pixmaps/log.xpm"
#include "pixmaps/menu_zoom.xpm"

/*
 * Mini-icon for IceWM, KWM, tasklist etc.
 */
#include "pixmaps/mini_icon.xpm"

enum {
	CHANGED,
	LAST_SIGNAL
};

typedef enum {
	SCHEMATIC_TOOL_ARROW,
	SCHEMATIC_TOOL_PART,
	SCHEMATIC_TOOL_WIRE,
	SCHEMATIC_TOOL_TEXT
} SchematicTool;

typedef enum {
	RUBBER_NO = 0,
	RUBBER_YES,
	RUBBER_START
} RubberState;

typedef struct {
	RubberState state;
	int timeout_id;
	int click_start_state;

	GnomeCanvasItem *rectangle;
	double start_x, start_y;
} RubberbandInfo;

struct _SchematicViewPriv {
	Schematic *schematic;

	Sheet *sheet;
	GList *items;

	GHashTable *dots;

	RubberbandInfo *rubberband;
	GtkPageSetup *page_setup;
	GtkPrintSettings *print_settings;

	GList *preserve_selection_items;

	gboolean empty;

	GtkActionGroup *action_group;
	GtkUIManager *ui_manager;

	GtkWidget *floating_part_browser;

	guint grid : 1;
	SchematicTool tool;
	int cursor;

	CreateWireContext *create_wire_context; /* FIXME: get rid of this somehow. */

	gpointer browser;

	/*
	 * FIXME: Move these to a special struct instead.
	 */
	GtkWidget *log_window;
	GtkTextView *log_text;
	GladeXML *log_gui;

	gboolean show_voltmeters;
	GList *voltmeter_items; /* List of GnomeCanvasItem. */
	GHashTable *voltmeter_nodes;
};

/*
 * Class functions and members.
 */
static void schematic_view_init(SchematicView *sv);
static void schematic_view_class_init(SchematicViewClass *klass);
static void schematic_view_dispose(GObject *object);
static void schematic_view_finalize(GObject *object);
static GObjectClass *parent_class = NULL;
static guint schematic_view_signals[LAST_SIGNAL] = { 0 };
static void dot_removed_callback (Schematic *schematic, SheetPos *pos, SchematicView *sv);

static GList *schematic_view_list = NULL;
GdkBitmap *stipple;
char stipple_pattern [] = { 0x02, 0x01 };
extern GnomeCanvasClass *sheet_parent_class; /*			FIXME: Remove later. */

/*
 * Signal callbacks.
 */
static void title_changed_callback (Schematic *schematic, char *new_title, SchematicView *sv);
static void set_focus (GtkWindow *window, GtkWidget *focus, SchematicView *sv);
static int delete_event (GtkWidget *widget, GdkEvent *event, SchematicView *sv);
static void item_destroy_callback (SheetItem *item, SchematicView *sv);
static int sheet_event_callback (GtkWidget *widget, GdkEvent *event, SchematicView *sv);
static void data_received (GtkWidget *widget, GdkDragContext *context,
	gint x, gint y, GtkSelectionData *selection_data, guint info,
	guint32 time, SchematicView *sv);

static void	item_data_added_callback (Schematic *schematic, ItemData *data, SchematicView *sv);
static void	item_selection_changed_callback (SheetItem *item, gboolean selected, SchematicView *sv);
static void	reset_tool_cb (Sheet *sheet, SchematicView *sv);

/*
 * Misc.
 */
static int can_close (SchematicView *sv);
static void setup_dnd (SchematicView *sv);
static int rubberband_timeout_cb (SchematicView *sv);
static void stop_rubberband (SchematicView *sv, GdkEventButton *event);
static void setup_rubberband (SchematicView *sv, GdkEventButton *event);
static void set_tool (SchematicView *sv, SchematicTool tool);

static int dot_equal (gconstpointer a, gconstpointer b);
static guint dot_hash (gconstpointer key);

static void schematic_view_show_voltmeters (SchematicView *sv, gboolean show);

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
	GladeXML *xml;
	GtkWidget *win;
	GtkEntry *title, *author;
	GtkTextView *comments;
	GtkTextBuffer *buffer;
	gchar *s_title, *s_author, *s_comments;
	gint btn;

	s = schematic_view_get_schematic (sv);

	if (!g_file_test (OREGANO_GLADEDIR "/properties.glade", G_FILE_TEST_EXISTS)) {
			gchar *msg;
			msg = g_strdup_printf (
					_("The file %s could not be found. You might need to reinstall Oregano to fix this."),
					OREGANO_GLADEDIR "/properties.glade");

			oregano_error_with_title (_("Could not create properties dialog"), msg);
			g_free (msg);
			return;
	}

	xml = glade_xml_new (
			OREGANO_GLADEDIR "/properties.glade",
			"properties", NULL
	);

	win = glade_xml_get_widget (xml, "properties");
	title = GTK_ENTRY (glade_xml_get_widget (xml, "title"));
	author = GTK_ENTRY (glade_xml_get_widget (xml, "author")); 
	comments = GTK_TEXT_VIEW (glade_xml_get_widget (xml, "comments")); 
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

	btn = gtk_dialog_run (GTK_DIALOG (win));

	if (btn == GTK_RESPONSE_OK) {
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

	gtk_widget_destroy (win);
}

static void
find_file (GtkButton *btn, GtkEntry *text)
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
	GladeXML *xml;
	GtkWidget *win;
	GtkWidget *w;
	GtkEntry *file = NULL;
	GtkComboBox *combo;
	gint btn = GTK_RESPONSE_NONE;
	gint formats[5], fc;
	GtkSpinButton *spinw, *spinh;

	s = schematic_view_get_schematic (sv);
			
	if (!g_file_test (OREGANO_GLADEDIR "/export.glade", G_FILE_TEST_EXISTS)) {
			gchar *msg;
			msg = g_strdup_printf (
					_("The file %s could not be found. You might need to reinstall Oregano to fix this."),
					OREGANO_GLADEDIR "/export.glade");

			oregano_error_with_title (_("Could not create properties dialog"), msg);
			g_free (msg);
			return;
	}

	xml = glade_xml_new (OREGANO_GLADEDIR "/export.glade", "export", NULL);

	win = glade_xml_get_widget (xml, "export");
	combo = GTK_COMBO_BOX (glade_xml_get_widget (xml, "format"));

	gtk_list_store_clear (GTK_LIST_STORE (gtk_combo_box_get_model (combo)));
	fc = 0;
#ifdef CAIRO_HAS_SVG_SURFACE
	gtk_combo_box_append_text (combo, "Scalar Vector Graphics (SVG)");
	formats[fc++] = 0;
#endif
#ifdef CAIRO_HAS_PDF_SURFACE
	gtk_combo_box_append_text (combo, "Portable Document Format (PDF)");
	formats[fc++] = 1;
#endif
#ifdef CAIRO_HAS_PS_SURFACE
	gtk_combo_box_append_text (combo, "Postscript (PS)");
	formats[fc++] = 2;
#endif
#ifdef CAIRO_HAS_PNG_FUNCTIONS
	gtk_combo_box_append_text (combo, "Portable Network Graphics (PNG)");
	formats[fc++] = 3;
#endif

	file = GTK_ENTRY (glade_xml_get_widget (xml, "file"));

	w = glade_xml_get_widget (xml, "find");
	g_signal_connect (G_OBJECT (w), "clicked", G_CALLBACK (find_file), file);

	gtk_combo_box_set_active (combo, 0);

		btn = gtk_dialog_run (GTK_DIALOG (win));
	
		if  (btn == GTK_RESPONSE_OK) {
			int bg = 0;
			GtkSpinButton *spinw, *spinh;
			int color_scheme = 0;
			GtkWidget *w;
			int i = gtk_combo_box_get_active (combo);

			w = glade_xml_get_widget (xml, "bgwhite");
			if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (w)))
					bg = 1;
			w = glade_xml_get_widget (xml, "bgblack");
			if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (w)))
					bg = 2;

			w = glade_xml_get_widget (xml, "color");
			if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (w)))
					color_scheme = 1;

			spinw = GTK_SPIN_BUTTON (glade_xml_get_widget (xml, "export_width"));
			spinh = GTK_SPIN_BUTTON (glade_xml_get_widget (xml, "export_height"));

			schematic_export (s,
					gtk_entry_get_text (file),
					gtk_spin_button_get_value_as_int (spinw),
					gtk_spin_button_get_value_as_int (spinh),
					bg,
					color_scheme,
					formats[i]);
	}

	gtk_widget_destroy (win);
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
	char *fname, *uri;
	GList *list;
	GError *error = NULL;

	fname = dialog_open_file (sv);
	if (fname == NULL)
			return;

	/*
	 * Repaint the other schematic windows before loading the new file.
	 */
	new_sv = NULL;
	for (list = schematic_view_list; list; list = list->next) {
			if (SCHEMATIC_VIEW (list->data)->priv->empty) {
					new_sv = SCHEMATIC_VIEW (list->data);
			}
			gtk_widget_queue_draw( GTK_WIDGET(SCHEMATIC_VIEW(list->data)->toplevel));
	}

	while (gtk_events_pending())
			gtk_main_iteration();

	new_sm = schematic_read(fname, &error);

	if (error != NULL) {
			oregano_error_with_title (_("Could not load file"), error->message);
			g_error_free (error);
	}

	if (new_sm) {
		GtkRecentManager *manager;
		const gchar *mime;
		gchar *uri;
		GtkRecentInfo *item;
		
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

	g_free (fname);
	g_free (uri);
}


static void
oregano_recent_open (GtkRecentChooser *chooser,
			 SchematicView *sv)
{
	gchar *uri;
    const gchar *mime;
    GtkRecentInfo *item;
	GtkRecentManager *manager;
	Schematic *new_sm;
	SchematicView *new_sv;
	GError *error = NULL;

	uri = gtk_recent_chooser_get_current_uri (GTK_RECENT_CHOOSER (chooser));
	if (!uri) return;

	manager = gtk_recent_manager_get_default ();
	item = gtk_recent_manager_lookup_item (manager, uri, NULL);
	if (!item) return;

	mime = gtk_recent_info_get_mime_type (item);
	if (!mime) {
		g_warning ("Unrecognized mime type");
		return;
	}

	if (!strcmp (mime, "application/x-oregano")) {
		new_sm = schematic_read(uri, &error);
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
	GtkWidget *menuitem;

	menu = gtk_recent_chooser_menu_new_for_manager (manager);

	gtk_recent_chooser_set_limit (GTK_RECENT_CHOOSER (menu), 10);

	gtk_recent_chooser_set_local_only (GTK_RECENT_CHOOSER (menu), TRUE);
	gtk_recent_chooser_set_sort_type (GTK_RECENT_CHOOSER (menu),
			GTK_RECENT_SORT_MRU); 
	
	filter = gtk_recent_filter_new ();
	gtk_recent_filter_add_mime_type (filter, "application/x-oregano");
	gtk_recent_chooser_add_filter (GTK_RECENT_CHOOSER (menu), filter);
	gtk_recent_chooser_set_filter (GTK_RECENT_CHOOSER (menu), filter);
	gtk_recent_chooser_set_local_only (GTK_RECENT_CHOOSER (menu), TRUE);
	g_signal_connect (menu, "item-activated",
	                G_CALLBACK (oregano_recent_open),
                    NULL);

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
	menuitem = gtk_ui_manager_get_widget (priv->ui_manager, "/MainMenu/MenuFile/DisplayRecentFiles");
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

	if (filename == NULL || !strcmp (filename, _("Untitled.oregano"))){
			dialog_save_as (sv);
			return;
	} else {
			if (!schematic_save_file (sm, &error)){
					oregano_error_with_title (_("Could not save schematic file"), error->message);
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
			g_object_unref(G_OBJECT(sv));
	}
}

static void
select_all_cmd (GtkWidget *widget, SchematicView *sv)
{
	schematic_view_select_all (sv, TRUE);
}

static void
deselect_all_cmd (GtkWidget *widget, SchematicView *sv)
{
	schematic_view_select_all (sv, FALSE);
}

static void
delete_cmd (GtkWidget *widget, SchematicView *sv)
{
	schematic_view_delete_selection (sv);
}

static void
rotate_cmd (GtkWidget *widget, SchematicView *sv)
{
	if (sv->priv->sheet->state == SHEET_STATE_NONE)
			schematic_view_rotate_selection (sv);
	else if (sv->priv->sheet->state == SHEET_STATE_FLOAT ||
					 sv->priv->sheet->state == SHEET_STATE_FLOAT_START)
			schematic_view_rotate_ghosts (sv);
}

static void
flip_horizontal_cmd (GtkWidget *widget, SchematicView *sv)
{
	if (sv->priv->sheet->state == SHEET_STATE_NONE)
			schematic_view_flip_selection (sv, TRUE);
	else if (sv->priv->sheet->state == SHEET_STATE_FLOAT ||
					 sv->priv->sheet->state == SHEET_STATE_FLOAT_START)
			schematic_view_flip_ghosts (sv, TRUE);
}

static void
flip_vertical_cmd (GtkWidget *widget, SchematicView *sv)
{
	if (sv->priv->sheet->state == SHEET_STATE_NONE)
			schematic_view_flip_selection (sv, FALSE);
	else if (sv->priv->sheet->state == SHEET_STATE_FLOAT ||
					 sv->priv->sheet->state == SHEET_STATE_FLOAT_START)
			schematic_view_flip_ghosts (sv, FALSE);
}

static void
object_properties_cmd (GtkWidget *widget, SchematicView *sv)
{
	Sheet *sheet;

	sheet = sv->priv->sheet;

	if (g_list_length (sheet->priv->selected_objects) == 1)
			sheet_item_edit_properties (sheet->priv->selected_objects->data);
}

static void
copy_cmd (GtkWidget *widget, SchematicView *sv)
{
	SheetItem *item;
	GList *list;

	if (sv->priv->sheet->state != SHEET_STATE_NONE)
			return;

	schematic_view_clear_ghosts (sv);
	clipboard_empty ();

	list = schematic_view_get_selection (sv);
	for (; list; list = list->next) {
			item = list->data;
			clipboard_add_object (G_OBJECT (item));
	}

	if (clipboard_is_empty ())
		gtk_action_set_sensitive (gtk_ui_manager_get_action (sv->priv->ui_manager, "/MainMenu/MenuEdit/Paste"), FALSE);
	else
		gtk_action_set_sensitive (gtk_ui_manager_get_action (sv->priv->ui_manager, "/MainMenu/MenuEdit/Paste"), TRUE);
}

void
schematic_view_copy_selection (SchematicView *sv)
{
	g_return_if_fail (sv != NULL);
	g_return_if_fail (IS_SCHEMATIC_VIEW (sv));

	copy_cmd (NULL, sv);
}

static void
cut_cmd (GtkWidget *widget, SchematicView *sv)
{
	if (sv->priv->sheet->state != SHEET_STATE_NONE)
			return;

	copy_cmd (NULL, sv);

	schematic_view_delete_selection (sv);

	if (clipboard_is_empty ())
		gtk_action_set_sensitive (gtk_ui_manager_get_action (sv->priv->ui_manager, "/MainMenu/MenuEdit/Paste"), FALSE);
	else
		gtk_action_set_sensitive (gtk_ui_manager_get_action (sv->priv->ui_manager, "/MainMenu/MenuEdit/Paste"), TRUE);
}

void
schematic_view_cut_selection (SchematicView *sv)
{
	g_return_if_fail (sv != NULL);
	g_return_if_fail (IS_SCHEMATIC_VIEW (sv));

	cut_cmd (NULL, sv);
}

/**
* Ugly hack (gpointer...) to avoid a .h-file dependancy... :/
*/
static void
paste_objects (gpointer data, gpointer user_data)
{
	SchematicView *sv;

	sv = SCHEMATIC_VIEW (user_data);
	sheet_item_paste (sv, data);
}

static void
paste_cmd (GtkWidget *widget, SchematicView *sv)
{
	if (sv->priv->sheet->state != SHEET_STATE_NONE)
			return;

	if (sv->priv->sheet->priv->floating_objects != NULL) {
			schematic_view_clear_ghosts (sv);
	}

	schematic_view_select_all (sv, FALSE);
	clipboard_foreach ((ClipBoardFunction) paste_objects, sv);

	if (sv->priv->sheet->priv->floating_objects != NULL) {
			part_item_signal_connect_floating_group (sv->priv->sheet, sv);
	}
}

void
schematic_view_paste (SchematicView *sv)
{
	g_return_if_fail (sv != NULL);
	g_return_if_fail (IS_SCHEMATIC_VIEW (sv));

	paste_cmd (NULL, sv);
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
	gboolean b;
	GList *item;

	b = gtk_toggle_action_get_active (toggle);

	for (item = sv->priv->items; item; item=item->next) {
			if (IS_PART_ITEM(item->data))
					part_item_show_node_labels (PART_ITEM (item->data), b);
	}
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

	/*
	 * Duplicate the list as the list is modified during destruction.
	 */
	copy = g_list_copy (schematic_view_list);

	for (list = copy; list; list = list->next){
			if (can_close (list->data))
					g_object_unref (list->data);
	}

	g_list_free (copy);
}

static void
v_clamp_cmd (SchematicView *sv)
{
	LibraryPart *library_part;
	SheetPos pos;
	Sheet *sheet;
	Part *part;
	GList *lib;
	Library *l;

	set_tool (sv, SCHEMATIC_TOOL_PART);
	sheet = schematic_view_get_sheet (sv);

	/* Find default lib */
	for(lib=oregano.libraries; lib; lib=lib->next) {
			l = (Library *)(lib->data);
			if (!g_strcasecmp(l->name, "Default")) break;
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

	schematic_view_select_all(sv, FALSE);
	schematic_view_clear_ghosts(sv);
	schematic_view_add_ghost_item(sv, ITEM_DATA(part));

	part_item_signal_connect_floating_group (sheet, sv);
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
voltmeter_cmd (GtkWidget *widget, SchematicView *sv)
{
	sv->priv->show_voltmeters = !sv->priv->show_voltmeters;

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget),
			sv->priv->show_voltmeters);

	schematic_view_show_voltmeters (sv, sv->priv->show_voltmeters);
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
	schematic_view_update_parts (sv);

	g_free (netlist_name);
	g_object_unref (engine);
	
	if (error != NULL) {
			if (g_error_matches (error, OREGANO_ERROR, OREGANO_SIMULATE_ERROR_NO_CLAMP) ||
				 g_error_matches (error, OREGANO_ERROR, OREGANO_SIMULATE_ERROR_NO_GND) ||
				 g_error_matches (error, OREGANO_ERROR, OREGANO_SIMULATE_ERROR_IO_ERROR)) {
						oregano_error_with_title (_("Could not create a netlist"), error->message);
						g_clear_error (&error);
			} 
			else 	oregano_error (_("An unexpected error has occurred"));
			
			return;
	}
}

static void
netlist_view_cmd (GtkWidget *widget, SchematicView *sv)
{
	netlist_editor_new_from_schematic_view (sv);
}

#define ZOOM_MIN 0.35
#define ZOOM_MAX 3

static void
zoom_check (SchematicView *sv)
{
	double zoom;

	g_return_if_fail (sv != NULL);
	g_return_if_fail (IS_SCHEMATIC_VIEW (sv));

	sheet_get_zoom (sv->priv->sheet, &zoom);

	gtk_action_set_sensitive (gtk_ui_manager_get_action (sv->priv->ui_manager, "/StandartToolbar/ZoomIn"), zoom < ZOOM_MAX);
	gtk_action_set_sensitive (gtk_ui_manager_get_action (sv->priv->ui_manager, "/StandartToolbar/ZoomOut"), zoom > ZOOM_MIN);
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
			g_object_set(G_OBJECT(sv->priv->sheet), "zoom", 0.50, NULL);
			break;
		case 1:
			g_object_set(G_OBJECT(sv->priv->sheet), "zoom", 0.75, NULL);
			break;
		case 2:
			g_object_set(G_OBJECT (sv->priv->sheet), "zoom", 1.0, NULL);
			break;
		case 3:
			g_object_set(G_OBJECT(sv->priv->sheet), "zoom", 1.25, NULL);
			break;
		case 4:
			g_object_set(G_OBJECT(sv->priv->sheet), "zoom", 1.5, NULL);
		break;
	}
	zoom_check (sv);
}

static void
simulate_cmd (GtkWidget *widget, SchematicView *sv)
{
	Schematic *sm;

	sm = sv->priv->schematic;

	simulation_show (NULL, sv);

	schematic_view_update_parts (sv);
	return;
}

GType
schematic_view_get_type(void)
{
	static GType schematic_view_type = 0;

	if (!schematic_view_type) {
			static const GTypeInfo schematic_view_info = {
					sizeof(SchematicViewClass),
					NULL,
					NULL,
					(GClassInitFunc)schematic_view_class_init,
					NULL,
					NULL,
					sizeof(SchematicView),
					0,
					(GInstanceInitFunc)schematic_view_init,
					NULL
			};

			schematic_view_type = g_type_register_static(G_TYPE_OBJECT,
					"SchematicView", &schematic_view_info, 0);
	}

	return schematic_view_type;
}

static void
schematic_view_class_init (SchematicViewClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS(klass);

	parent_class = g_type_class_peek_parent(klass);

	schematic_view_signals[CHANGED] =
			g_signal_new ("changed",
					G_TYPE_FROM_CLASS (object_class),
					G_SIGNAL_RUN_FIRST,
					G_STRUCT_OFFSET (SchematicViewClass, changed),
					NULL,
					NULL,
					g_cclosure_marshal_VOID__VOID,
					G_TYPE_NONE, 0);

	object_class->finalize = schematic_view_finalize;
	object_class->dispose = schematic_view_dispose;

	stipple = gdk_bitmap_create_from_data(NULL, stipple_pattern, 2, 2);
}

static void
schematic_view_init (SchematicView *sv)
{
	sv->priv = g_new0 (SchematicViewPriv, 1);

	sv->priv->preserve_selection_items = NULL;
	sv->priv->rubberband = g_new0 (RubberbandInfo, 1);
	sv->priv->rubberband->state = RUBBER_NO;
	sv->priv->items = NULL;
	sv->priv->empty = TRUE;
	sv->priv->schematic = NULL;
	sv->priv->page_setup = NULL;
	sv->priv->print_settings = gtk_print_settings_new ();
	sv->priv->grid = TRUE;

	sv->priv->show_voltmeters = FALSE;
	sv->priv->voltmeter_items = NULL;
	sv->priv->voltmeter_nodes = g_hash_table_new_full (g_str_hash,
			g_str_equal, g_free, g_free);
}

static void
schematic_view_finalize(GObject *object)
{
	SchematicView *sv = SCHEMATIC_VIEW (object);

	if (sv->priv) {
			g_hash_table_destroy (sv->priv->dots);
			g_free(sv->priv->rubberband);

			g_free (sv->priv);
			sv->priv = NULL;
	}

	if (sv->toplevel) {
			gtk_widget_destroy (GTK_WIDGET (sv->toplevel));
			sv->toplevel = NULL;
	}

	G_OBJECT_CLASS(parent_class)->finalize(object);
}

static void
schematic_view_dispose(GObject *object)
{
	SchematicView *sv = SCHEMATIC_VIEW(object);
	GList *list;

	schematic_view_list = g_list_remove (schematic_view_list, sv);

	/* Disconnect sheet's events */
	g_signal_handlers_disconnect_by_func (G_OBJECT (sv->priv->sheet),
			G_CALLBACK(sheet_event_callback), sv);

	/* Disconnect focus signal */
	g_signal_handlers_disconnect_by_func (G_OBJECT (sv->toplevel),
			G_CALLBACK(set_focus), sv);

	/* Disconnect schematic dot_removed signal */
	g_signal_handlers_disconnect_by_func (G_OBJECT (sv->priv->schematic),
			G_CALLBACK(dot_removed_callback), sv);

	/* Disconnect destroyed item signal */
	for(list=sv->priv->items; list; list=list->next)
			g_signal_handlers_disconnect_by_func (G_OBJECT (list->data),
					G_CALLBACK(item_destroy_callback), sv);

	/* Disconnect destroy event from toplevel */
	g_signal_handlers_disconnect_by_func (G_OBJECT (sv->toplevel),
			G_CALLBACK(delete_event), sv);

	if (sv->priv) {
			g_object_unref(G_OBJECT(sv->priv->schematic));
	}
	G_OBJECT_CLASS(parent_class)->dispose(object);
}

/**
* Set up a mini icon for the window.
*/
static void
realized (GtkWidget *widget)
{
	GdkPixmap *icon;
	GdkBitmap *icon_mask;

	icon = gdk_pixmap_create_from_xpm_d (widget->window,
			&icon_mask, NULL, mini_icon_xpm);
	set_small_icon (widget->window, icon, icon_mask);
}

static void
dot_added_callback (Schematic *schematic, SheetPos *pos, SchematicView *sv)
{
	/* GnomeCanvasItem *dot_item; */
	NodeItem *node_item;

	SheetPos *key;

	g_return_if_fail (sv != NULL);
	g_return_if_fail (IS_SCHEMATIC_VIEW (sv));

	node_item = g_hash_table_lookup (sv->priv->dots, pos);
	if (node_item == NULL) {
			node_item = NODE_ITEM (
					gnome_canvas_item_new (
							gnome_canvas_root (GNOME_CANVAS (sv->priv->sheet)),
							node_item_get_type (),
							"x", pos->x,
							"y", pos->y,
							NULL));
	} 

	node_item_show_dot (node_item, TRUE);

	key = g_new0 (SheetPos, 1);
	key->x = pos->x;
	key->y = pos->y;

	g_hash_table_insert (sv->priv->dots, key, node_item);
}

static void
dot_removed_callback (Schematic *schematic, SheetPos *pos, SchematicView *sv)
{
	gpointer *node_item; /* GnomeCanvasItem* */
	gpointer *orig_key;  /* SheetPos* */
	gboolean found;

	g_return_if_fail (sv != NULL);
	g_return_if_fail (IS_SCHEMATIC_VIEW (sv));

	found = g_hash_table_lookup_extended (
			sv->priv->dots,
			pos,
			(gpointer) &orig_key,
			(gpointer) &node_item);

	if (found) {
			gtk_object_destroy(GTK_OBJECT (node_item));
			g_hash_table_remove(sv->priv->dots, pos);
	} else
			g_warning ("No dot to remove!");
}

static void
show_help (GtkWidget *widget, SchematicView *sv)
{
	GError *error = NULL;

	if (!gnome_help_display_uri("ghelp:oregano",&error)) {
			printf("Error %s\n", error->message);
			g_error_free(error);
	}
}

#include "schematic-view-menu.h"

SchematicView *
schematic_view_new (Schematic *schematic)
{
	SchematicView *sv;
	SchematicViewPriv *priv;
	GtkAdjustment *vadj, *hadj;
	GtkWidget *w, *hbox, *vbox;
	GtkWidget *toolbar;
	GtkActionGroup *action_group;
	GtkUIManager *ui_manager;
	GtkAccelGroup *accel_group;
	GtkWidget *menubar;
	GError *error;

	g_return_val_if_fail (schematic != NULL, NULL);
	g_return_val_if_fail (IS_SCHEMATIC (schematic), NULL);

	sv = SCHEMATIC_VIEW(g_object_new(schematic_view_get_type(), NULL));

	schematic_view_list = g_list_prepend (schematic_view_list, sv);

	sv->toplevel = gnome_app_new ("Oregano", "Oregano");

	w = gnome_appbar_new (FALSE, TRUE, GNOME_PREFERENCES_NEVER);
	gnome_app_set_statusbar (GNOME_APP (sv->toplevel), w);

	sv->priv->sheet = SHEET (sheet_new (10000, 10000));

	vbox = gtk_vbox_new (FALSE, 0);
	hbox = gtk_hbox_new (FALSE, 0);

	w = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (w), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (w), GTK_SHADOW_IN);
	gtk_container_add (GTK_CONTAINER (w), GTK_WIDGET (sv->priv->sheet));
	vadj = gtk_layout_get_vadjustment (GTK_LAYOUT (sv->priv->sheet));
	hadj = gtk_layout_get_hadjustment (GTK_LAYOUT (sv->priv->sheet));
	vadj->step_increment = 10;
	hadj->step_increment = 10;
	gtk_scrolled_window_set_vadjustment (GTK_SCROLLED_WINDOW (w), vadj);
	gtk_scrolled_window_set_hadjustment (GTK_SCROLLED_WINDOW (w), hadj);

	gtk_box_pack_start (GTK_BOX (hbox), w, TRUE, TRUE, 0);

	g_signal_connect (G_OBJECT (sv->priv->sheet), "event", G_CALLBACK(sheet_event_callback), sv);

	priv = sv->priv;
	priv->log_window = NULL;

	gtk_box_pack_start (GTK_BOX (hbox), part_browser_create (sv), FALSE, FALSE, 0);

	priv->action_group = action_group = gtk_action_group_new ("MenuActions");
	gtk_action_group_set_translation_domain (priv->action_group, GETTEXT_PACKAGE);
	gtk_action_group_add_actions (action_group, entries, G_N_ELEMENTS (entries), sv);
	gtk_action_group_add_radio_actions (action_group, zoom_entries, G_N_ELEMENTS (zoom_entries), 2, G_CALLBACK (zoom_cmd), sv);
	gtk_action_group_add_radio_actions (action_group, tools_entries, G_N_ELEMENTS (tools_entries), 0, G_CALLBACK (tool_cmd), sv);
	gtk_action_group_add_toggle_actions (action_group, toggle_entries, G_N_ELEMENTS (toggle_entries), sv);

	priv->ui_manager = ui_manager = gtk_ui_manager_new ();
	gtk_ui_manager_insert_action_group (ui_manager, action_group, 0);

	accel_group = gtk_ui_manager_get_accel_group (ui_manager);
	gtk_window_add_accel_group (GTK_WINDOW (sv->toplevel), accel_group);

	error = NULL;
	if (!gtk_ui_manager_add_ui_from_string (ui_manager, ui_description, -1, &error)) {
			g_message ("building menus failed: %s", error->message);
			g_error_free (error);
			exit (EXIT_FAILURE);
	}

	menubar = gtk_ui_manager_get_widget (ui_manager, "/MainMenu");
	
	// Upgrade the menu bar with the recent files used by oregano
	display_recent_files (menubar, sv);
	gtk_box_pack_start (GTK_BOX (vbox), menubar, FALSE, FALSE, 0);

	toolbar = gtk_ui_manager_get_widget (ui_manager, "/StandartToolbar");
	gtk_toolbar_set_style (GTK_TOOLBAR (toolbar), GTK_TOOLBAR_ICONS);
	gtk_box_pack_start (GTK_BOX (vbox), toolbar, FALSE, FALSE, 0);

	// Fill the window
	gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, TRUE, 0);
	gnome_app_set_contents (GNOME_APP (sv->toplevel), vbox);

	gtk_window_set_focus (GTK_WINDOW (sv->toplevel), GTK_WIDGET (sv->priv->sheet)); 
	gtk_widget_grab_focus (GTK_WIDGET (sv->priv->sheet));

	g_signal_connect_after (G_OBJECT (sv->toplevel), "set_focus", G_CALLBACK (set_focus), sv);
	g_signal_connect (G_OBJECT (sv->toplevel), "delete_event", G_CALLBACK (delete_event), sv);
	g_signal_connect (G_OBJECT (sv->toplevel), "realize", G_CALLBACK (realized), NULL);

	setup_dnd (sv);

	/* Set default sensitive for items */
	gtk_action_set_sensitive (gtk_ui_manager_get_action (ui_manager, "/MainMenu/MenuEdit/ObjectProperties"), FALSE);
	gtk_action_set_sensitive (gtk_ui_manager_get_action (ui_manager, "/MainMenu/MenuEdit/Paste"), FALSE);

	/*
	 * Set the window size to something reasonable. Stolen from Gnumeric.
	 */
	{
			int sx, sy;

			gtk_window_set_policy (GTK_WINDOW (sv->toplevel), TRUE, TRUE, FALSE);
			sx = MAX (gdk_screen_width		() - 64, 640);
			sy = MAX (gdk_screen_height () - 64, 480);
			sx = (sx * 3) / 4;
			sy = (sy * 3) / 4;
			gtk_window_set_default_size (GTK_WINDOW (sv->toplevel),
					sx, sy);
	}

	/*
	 * Hash table that keeps maps coordinate to a specific dot.
	 */
	priv->dots = g_hash_table_new_full (dot_hash, dot_equal, g_free, NULL);

	g_signal_connect_object(G_OBJECT (sv->priv->sheet),
			"reset_tool",
			G_CALLBACK (reset_tool_cb),
			G_OBJECT (sv),
			0);

	schematic_view_load (sv, schematic);

	if (!schematic_get_title (sv->priv->schematic)) {
			gtk_window_set_title (
					GTK_WINDOW (sv->toplevel),
					_("Untitled.oregano")
			);
	} else {
			gtk_window_set_title (
					GTK_WINDOW (sv->toplevel),
					schematic_get_title (sv->priv->schematic)
			);
	}
	schematic_set_filename (sv->priv->schematic, _("Untitled.oregano"));
	schematic_set_netlist_filename (sv->priv->schematic, _("Untitled.netlist"));

	return sv;
}

void
schematic_view_load (SchematicView *sv, Schematic *sm)
{
	GList *list;		
	g_return_if_fail (sv->priv->empty != FALSE);
	g_return_if_fail (sm != NULL);

	if (sv->priv->schematic) g_object_unref (G_OBJECT (sv->priv->schematic));

	sv->priv->schematic = sm;

	g_signal_connect_object(G_OBJECT (sm),
			"title_changed",
			G_CALLBACK(title_changed_callback),
			G_OBJECT (sv),
			0);
	g_signal_connect_object(G_OBJECT (sm),
			"item_data_added",
			G_CALLBACK(item_data_added_callback),
			G_OBJECT (sv),
			0);
	g_signal_connect_object(G_OBJECT (sm),
			"dot_added",
			G_CALLBACK(dot_added_callback),
			G_OBJECT (sv),
			0);
	g_signal_connect_object(G_OBJECT (sm),
			"dot_removed",
			G_CALLBACK(dot_removed_callback),
			G_OBJECT (sv),
			0);

	list = schematic_get_items (sm);

	for (; list; list = list->next)
			g_signal_emit_by_name(G_OBJECT (sm), "item_data_added", list->data);

	list = node_store_get_node_positions (schematic_get_store (sm));
	for (; list; list = list->next)
			dot_added_callback (sm, list->data, sv);

	g_list_free (list);
}

/*
* Tidy up: remove the item from the item list and selected items list.
*/
static void
item_destroy_callback (SheetItem *item, SchematicView *sv)
{
	Sheet *sheet;

	sv->priv->items = g_list_remove (sv->priv->items, item);

	sheet = sheet_item_get_sheet (item);

	/*
	 * Remove the object from the selected-list before destroying.
	 */

	/*		FIXME: optimize by checking if the item is selected first. */
	sheet->priv->selected_objects = g_list_remove (sheet->priv->selected_objects, item);
	sheet->priv->floating_objects = g_list_remove (sheet->priv->floating_objects, item);
}

static void
item_selection_changed_callback (SheetItem *item, gboolean selected,
	SchematicView *sv)
{
	guint length;

	/* FIXME! : don't touch sheet->priv directly!!! */
	if (selected) {
		sv->priv->sheet->priv->selected_objects =
		g_list_prepend ( sv->priv->sheet->priv->selected_objects, item);
	} else {
		sv->priv->sheet->priv->selected_objects = g_list_remove ( sv->priv->sheet->priv->selected_objects, item);
	}

	length = g_list_length (sv->priv->sheet->priv->selected_objects);
	if (length && item_data_has_properties (sheet_item_get_data (item)))
		gtk_action_set_sensitive (gtk_ui_manager_get_action (sv->priv->ui_manager, "/MainMenu/MenuEdit/ObjectProperties"), TRUE);
	else
		gtk_action_set_sensitive (gtk_ui_manager_get_action (sv->priv->ui_manager, "/MainMenu/MenuEdit/ObjectProperties"), FALSE);
}

/**
* An ItemData got added; create an Item and set up the neccessary handlers.
*/
static void
item_data_added_callback (Schematic *schematic, ItemData *data,
	SchematicView *sv)
{
	NodeStore *store;
	Sheet *sheet;
	SheetItem *item;

	store = schematic_get_store (schematic);
	sheet = schematic_view_get_sheet (sv);

	item = sheet_item_factory_create_sheet_item (sv, data);
	if (item != NULL) {
			sheet_item_place (item, sv);

			g_object_set (G_OBJECT (item), "action_group", sv->priv->action_group, NULL);
			/*
			 * Hook onto the destroy signal so that we can perform some
			 * cleaning magic before destroying the item (remove it from
			 * lists etc).
			 */
			g_signal_connect (
					G_OBJECT (item),
					"destroy",
					G_CALLBACK (item_destroy_callback),
					sv);

			g_signal_connect (
					G_OBJECT (item),
					"selection_changed",
					G_CALLBACK (item_selection_changed_callback),
					sv);

			sv->priv->items = g_list_prepend (sv->priv->items, item);
			sv->priv->empty = FALSE;
			if (sv->priv->tool == SCHEMATIC_TOOL_PART)
				schematic_view_reset_tool (sv);
	}
}

void
schematic_view_add_ghost_item (SchematicView *sv, ItemData *data)
{
	Sheet *sheet;
	SheetItem *item;

	g_return_if_fail (sv != NULL);
	g_return_if_fail (IS_SCHEMATIC_VIEW (sv));

	sheet = schematic_view_get_sheet (sv);

	item = sheet_item_factory_create_sheet_item (sv, data);

	sheet->priv->floating_objects =
			g_list_prepend (sheet->priv->floating_objects,item);
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
	g_return_if_fail(sv->priv != NULL);
	g_return_if_fail(sv->priv->sheet != NULL);

	if (!window->focus_widget)
			/* gtk_window_set_focus (GTK_WINDOW (sv->toplevel), GTK_WIDGET (sv->priv->sheet)); */
			gtk_widget_grab_focus(GTK_WIDGET (sv->priv->sheet));
}

static int
delete_event(GtkWidget *widget, GdkEvent *event, SchematicView *sv)
{
	if (can_close (sv)) {
			g_object_unref(G_OBJECT(sv));

			if (schematic_count() == 0)
					bonobo_main_quit();

			return FALSE;
	} else
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
	text = g_strdup_printf (
			_("<span weight=\"bold\" size=\"large\">Save changes to schematic %s before closing?</span>\n\n"
					"If you don't save, all changes since you last saved will be permanently lost."),
					filename ?	g_basename (filename) : NULL );

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
					schematic_save_file (schematic_view_get_schematic (sv), &error);
			break;
			case GTK_RESPONSE_NO:
					schematic_set_dirty (schematic_view_get_schematic (sv), FALSE);
			break;
			case GTK_RESPONSE_CANCEL:
			default:
					return FALSE;
	}
	return TRUE;
}

static void
run_context_menu (SchematicView *sv, GdkEventButton *event)
{
	GtkWidget *menu;

	g_return_if_fail (sv != NULL);
	g_return_if_fail (IS_SCHEMATIC_VIEW (sv));

	menu = gtk_ui_manager_get_widget (sv->priv->ui_manager, "/MainPopup");

	gtk_menu_popup (GTK_MENU (menu), NULL, NULL, NULL, sv, event->button, event->time);
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


/*
* FIXME: move these signals to schematic-view instead.
*/
void
schematic_view_reset_tool (SchematicView *sv)
{
	g_return_if_fail (sv != NULL);
	g_return_if_fail (IS_SCHEMATIC_VIEW (sv));

	g_signal_emit_by_name (G_OBJECT (sv->priv->sheet), "reset_tool");
}

void
schematic_view_cancel (SchematicView *sv)
{
	g_return_if_fail (sv != NULL);
	g_return_if_fail (IS_SCHEMATIC_VIEW (sv));

	g_signal_emit_by_name (G_OBJECT (sv->priv->sheet), "cancel");
}

/* Debugging: */
extern void node_store_dump_wires (NodeStore *store);

static int
sheet_event_callback (GtkWidget *widget, GdkEvent *event, SchematicView *sv)
{
	Sheet *sheet;

	sheet = sv->priv->sheet;

	switch (event->type) {
	case GDK_3BUTTON_PRESS:
			/*
			 * We don't not care about triple clicks on the sheet.
			 */
			return FALSE;
	case GDK_2BUTTON_PRESS:
			/*
			 * The sheet does not care about double clicks, but invoke the
			 * canvas event handler and see if an item picks up the event.
			 */
			if ((*GTK_WIDGET_CLASS (sheet_parent_class)->button_press_event) (
					widget, (GdkEventButton *)event))
					return TRUE;
			else
					return FALSE;
	case GDK_BUTTON_PRESS:
			/*
			 * If we are in the middle of something else, don't interfere
			 * with that.
			 */
			if (sheet->state != SHEET_STATE_NONE)
					return FALSE;

			if ((* GTK_WIDGET_CLASS
					(sheet_parent_class)->button_press_event) (widget, (GdkEventButton *) event))
					return TRUE;

			if (event->button.button == 3) {
					run_context_menu (sv, (GdkEventButton *) event);
					return TRUE;
			}

			if (event->button.button == 1) {
					if (!(event->button.state & GDK_SHIFT_MASK))
							schematic_view_select_all (sv, FALSE);

					setup_rubberband (sv, (GdkEventButton *) event);
					return TRUE;
			}
			break;
	case GDK_BUTTON_RELEASE:
			if (event->button.button == 4 || event->button.button == 5)
					return TRUE;

			if (event->button.button == 1 &&
					sv->priv->rubberband->state == RUBBER_YES) {
					stop_rubberband (sv, (GdkEventButton *) event);
					return TRUE;
			}

			if (GTK_WIDGET_CLASS
					(sheet_parent_class)->button_release_event != NULL)
					return GTK_WIDGET_CLASS
							(sheet_parent_class)->button_release_event (
									widget, (GdkEventButton *) event);

			break;

	case GDK_SCROLL:
			if (((GdkEventScroll *)event)->direction == GDK_SCROLL_UP) {
					double zoom;
					sheet_get_zoom (sv->priv->sheet, &zoom);
					if (zoom < ZOOM_MAX)
							zoom_in_cmd (widget, sv);
			} else if (((GdkEventScroll *)event)->direction == GDK_SCROLL_DOWN) {
					double zoom;
					sheet_get_zoom (sv->priv->sheet, &zoom);
					if (zoom > ZOOM_MIN)
					zoom_out_cmd (widget, sv);
			}
			break;
	case GDK_KEY_PRESS:
			switch (event->key.keyval) {
			case GDK_R:
			case GDK_r:
					if (sheet->state == SHEET_STATE_NONE)
							schematic_view_rotate_selection (sv);
					break;
	/*		case GDK_f:
					if (sheet->state == SHEET_STATE_NONE)
							schematic_view_flip_selection (sv, TRUE);
					break;
			case GDK_F:
					if (sheet->state == SHEET_STATE_NONE)
							schematic_view_flip_selection (sv, FALSE);
					break;*/
			case GDK_Home:
					break;
			case GDK_End:
					break;
			case GDK_Left:
					if (event->key.state & GDK_MOD1_MASK)
							sheet_scroll (sheet, -20, 0);
					break;
			case GDK_Up:
					if (event->key.state & GDK_MOD1_MASK)
							sheet_scroll (sheet, 0, -20);
					break;
			case GDK_Right:
					if (event->key.state & GDK_MOD1_MASK)
							sheet_scroll (sheet, 20, 0);
					break;
			case GDK_Down:
					if (event->key.state & GDK_MOD1_MASK)
							sheet_scroll (sheet, 0, 20);
					break;
			case GDK_Page_Up:
					if (event->key.state & GDK_MOD1_MASK)
							sheet_scroll (sheet, 0, -120);
					break;
			case GDK_Page_Down:
					if (event->key.state & GDK_MOD1_MASK)
							sheet_scroll (sheet, 0, 120);
					break;
			case GDK_l:
/*				part_browser_place_selected_part (sheet->schematic); */
					break;
			case GDK_space:

					node_store_dump_wires (
							schematic_get_store (sv->priv->schematic));
					return TRUE;

					break;
			case GDK_Escape:
					g_signal_emit_by_name (G_OBJECT (sheet), "cancel");
					break;
			case GDK_Delete:
					schematic_view_delete_selection (sv);
					break;
			default:
					return FALSE;
			}
	default:
			return FALSE;
	}

	return TRUE;
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

	/*
	 * Extract the filenames from the URI-list we recieved.
	 */
	
	switch (info) {
			case DRAG_PART_INFO:
					part_browser_dnd (selection_data, x, y);
			break;
			case DRAG_URI_INFO:
					files = g_strsplit (selection_data->data, "\n", -1);
					if (files) {
							int i=0;
							while (files[i]) {
									Schematic *new_sm = NULL;
									int l = strlen(files[i]);
									/* Algo queda mal al final luego del split, agrego un \0 */
									files[i][l-1] = '\0';

									if (l <= 0) {
											/* Empty file name, ignore! */
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
											while (gtk_events_pending ()) /* Show something. */
													gtk_main_iteration ();
											}
									i++;
							}
					}
	}
	gtk_drag_finish (context, TRUE, TRUE, time);
}

static void
setup_rubberband (SchematicView *sv, GdkEventButton *event)
{
	double x, y;
	double wx, wy;

	x = event->x; //the x coordinate of the pointer relative to the window.
	y = event->y; //the y coordinate of the pointer relative to the window.
	
	/* Need this for zoomed views */
	gnome_canvas_window_to_world (
			GNOME_CANVAS (sv->priv->sheet),
			x, y,
			&wx, &wy
	);
	x = wx;
	y = wy;

	sv->priv->rubberband->start_x = x;
	sv->priv->rubberband->start_y = y;

	sv->priv->rubberband->state = RUBBER_YES;
	sv->priv->rubberband->click_start_state = event->state;


	sv->priv->rubberband->rectangle = gnome_canvas_item_new (
			sv->priv->sheet->object_group,
			gnome_canvas_rect_get_type (),
			"x1", x,
			"y1", y,
			"x2", x,
			"y2", y,
			"outline_stipple", stipple,
			"outline_color", "black",
			"width_pixels", 1,
			NULL);

	gnome_canvas_item_grab (GNOME_CANVAS_ITEM (sv->priv->sheet->grid),
			(GDK_POINTER_MOTION_MASK | GDK_BUTTON_RELEASE_MASK),
			NULL, event->time);

	/* Mark all the selected objects to preserve their selected state
	 * if SHIFT is pressed while rubberbanding.
	 */
	if (event->state & GDK_SHIFT_MASK) {
			GList *list;
			for (list = sv->priv->sheet->priv->selected_objects; list;
					 list = list->next) {
					sheet_item_set_preserve_selection (
							SHEET_ITEM (list->data), TRUE);
			}
			/* Save the list so that we can remove the preserve_selection
				 flags later. */
			sv->priv->preserve_selection_items =
					g_list_copy (sv->priv->sheet->priv->selected_objects);
	}

	sv->priv->rubberband->timeout_id = gtk_timeout_add (
			15,
			(gpointer) rubberband_timeout_cb,
			(gpointer) sv);
}

static void
stop_rubberband (SchematicView *sv, GdkEventButton *event)
{
	GList *list;

	gtk_idle_remove (sv->priv->rubberband->timeout_id);
	sv->priv->rubberband->state = RUBBER_NO;

	if (sv->priv->preserve_selection_items != NULL) {
			for (list = sv->priv->preserve_selection_items; list;
					 list = list->next)
					sheet_item_set_preserve_selection (
							SHEET_ITEM (list->data), FALSE);
			g_list_free (sv->priv->preserve_selection_items);
			sv->priv->preserve_selection_items = NULL;
	}

	gnome_canvas_item_ungrab (GNOME_CANVAS_ITEM (sv->priv->sheet->grid), event->time);
	gtk_object_destroy (GTK_OBJECT (sv->priv->rubberband->rectangle));
}

static int
rubberband_timeout_cb (SchematicView *sv)
{
	static double x1_old = 0, y1_old = 0, x2_old = 0, y2_old = 0;
	int _x, _y;
	double x, y;
	double x1 = 0, y1 = 0, x2 = 0, y2 = 0;
	double dx, dy, tx, ty;
	GList *list;
	SheetPos p1, p2;

	/* Obtains the current pointer position and modifier state. 
	 * The position is given in coordinates relative to window.
	 */
	gdk_window_get_pointer (GTK_WIDGET (sv->priv->sheet)->window, &_x, &_y, NULL);

	x = _x;
	y = _y;
	
	gnome_canvas_get_scroll_offsets (
			GNOME_CANVAS (sv->priv->sheet),
			&_x, &_y
	);
	
	x += _x;
	y += _y;
	
	/* Need this for zoomed views */
	gnome_canvas_window_to_world (
			GNOME_CANVAS (sv->priv->sheet),
			x, y,
			&tx, &ty
	);
	x = tx;
	y = ty;

	if (x < sv->priv->rubberband->start_x) {
			x1 = x;
			x2 = sv->priv->rubberband->start_x;
	} else {
			x1 = sv->priv->rubberband->start_x;
			x2 = x;
	}

	if (y < sv->priv->rubberband->start_y) {
			y1 = y;
			y2 = sv->priv->rubberband->start_y;
	} else {
			y1 = sv->priv->rubberband->start_y;
			y2 = y;
	}


	p1.x = x1;
	p1.y = y1;
	p2.x = x2;
	p2.y = y2;

	/*
	 * Scroll the sheet if needed.
	 */
	/* Need FIX */
	{
			int width, height;
			int dx = 0, dy = 0;
			
			gdk_window_get_pointer (GTK_WIDGET (sv->priv->sheet)->window, &_x, &_y, NULL);

			width = GTK_WIDGET (sv->priv->sheet)->allocation.width;
			height = GTK_WIDGET (sv->priv->sheet)->allocation.height;

			if (_x < 0)
					dx = -1;
			else if (_x > width)
					dx = 1;

			if (_y < 0)
					dy = -1;
			else if (_y > height)
					dy = 1;

			if (!(_x > 0 && _x < width && _y > 0 && _y < height))
					sheet_scroll (sv->priv->sheet, dx * 5, dy * 5);
	}

	dx = fabs ((x1 - x2) - (x1_old - x2_old));
	dy = fabs ((y1 - y2) - (y1_old - y2_old));
	if (dx > 1.0 || dy > 1.0) {
			/* Save old state */
			x1_old = x1;
			y1_old = y1;
			x2_old = x2;
			y2_old = y2;

			for (list = sv->priv->items; list; list = list->next) {
					sheet_item_select_in_area (list->data, &p1, &p2);
			}

			gnome_canvas_item_set (sv->priv->rubberband->rectangle,
					"x1", (double) x1,
					"y1", (double) y1,
					"x2", (double) x2,
					"y2", (double) y2,
					NULL);
	}

	return TRUE;
}

static void
set_tool (SchematicView *sv, SchematicTool tool)
{
	/*
	 * Switch from this tool...
	 */
	switch (sv->priv->tool) {
	case SCHEMATIC_TOOL_ARROW:
			/*
			 * In case we are handling a floating object,
			 * cancel that so that we can change tool.
			 */
			sheet_item_cancel_floating (sv);
			break;
	case SCHEMATIC_TOOL_WIRE:
			if (sv->priv->create_wire_context) {
					create_wire_exit (sv->priv->create_wire_context);
					sv->priv->create_wire_context = NULL;
			}
			break;
	case SCHEMATIC_TOOL_TEXT:
			textbox_item_cancel_listen (sv);
			break;
	case SCHEMATIC_TOOL_PART:
			sheet_item_cancel_floating (sv);
	default:
			break;
	}

	/*
	 * ...to this tool.
	 */
	switch (tool) {
	case SCHEMATIC_TOOL_ARROW:
			cursor_set_widget (GTK_WIDGET (sv->priv->sheet), OREGANO_CURSOR_LEFT_PTR);
			sv->priv->sheet->state = SHEET_STATE_NONE;
			break;
	case SCHEMATIC_TOOL_WIRE:
			cursor_set_widget (GTK_WIDGET (sv->priv->sheet),
					OREGANO_CURSOR_PENCIL);
			sv->priv->sheet->state = SHEET_STATE_WIRE;
			sv->priv->create_wire_context = create_wire_initiate (sv);
			break;
	case SCHEMATIC_TOOL_TEXT:
			cursor_set_widget (GTK_WIDGET (sv->priv->sheet), OREGANO_CURSOR_CARET);
			sv->priv->sheet->state = SHEET_STATE_TEXTBOX_WAIT;

			textbox_item_listen (sv);
			break;
	case SCHEMATIC_TOOL_PART:
			cursor_set_widget (GTK_WIDGET (sv->priv->sheet), OREGANO_CURSOR_LEFT_PTR);
	default:
			break;
	}

	sv->priv->tool = tool;
}

static void
reset_tool_cb (Sheet *sheet, SchematicView *sv)
{
	set_tool (sv, SCHEMATIC_TOOL_ARROW);

	gtk_radio_action_set_current_value (GTK_RADIO_ACTION (gtk_ui_manager_get_action (sv->priv->ui_manager, "/StandartToolbar/Arrow")), 0);
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

void
schematic_view_delete_selection (SchematicView *sv)
{
	GList *list, *copy;
	Sheet *sheet;

	g_return_if_fail (sv != NULL);
	g_return_if_fail (IS_SCHEMATIC_VIEW (sv));

	sheet = sv->priv->sheet;

	if (sheet->state != SHEET_STATE_NONE)
			return;

	copy = g_list_copy (sheet->priv->selected_objects);

	for (list = copy; list; list = list->next) {
			gtk_object_destroy(GTK_OBJECT(list->data));
	}

	g_list_free (copy);
	g_list_free (sheet->priv->selected_objects);
	sheet->priv->selected_objects = NULL;
}

static void
rotate_items (SchematicView *sv, GList *items)
{
	GList *list, *item_data_list;
	SheetPos center, b1, b2;
	Sheet *sheet;

	sheet = sv->priv->sheet;

	item_data_list = NULL;
	for (list = items; list; list = list->next) {
			item_data_list = g_list_prepend (item_data_list,
					sheet_item_get_data (list->data));
	}

	item_data_list_get_absolute_bbox (item_data_list, &b1, &b2);

	center.x = (b2.x - b1.x) / 2 + b1.x;
	center.y = (b2.y - b1.y) / 2 + b1.y;

	snap_to_grid (sheet->grid, &center.x, &center.y);

	for (list = item_data_list; list; list = list->next) {
			ItemData *item_data = list->data;

			if (sv->priv->sheet->state == SHEET_STATE_NONE)
					item_data_unregister (item_data);

			item_data_rotate (item_data, 90, &center);

			if (sv->priv->sheet->state == SHEET_STATE_NONE)
					item_data_register (item_data);
	}

	g_list_free (item_data_list);
}

void
schematic_view_rotate_selection (SchematicView *sv)
{
	Sheet *sheet;

	g_return_if_fail (sv != NULL);
	g_return_if_fail (IS_SCHEMATIC_VIEW (sv));

	sheet = sv->priv->sheet;

	if (sheet->priv->selected_objects != NULL)
			rotate_items (sv, sheet->priv->selected_objects);
}

void
schematic_view_rotate_ghosts (SchematicView *sv)
{
	Sheet *sheet;

	g_return_if_fail (sv != NULL);
	g_return_if_fail (IS_SCHEMATIC_VIEW (sv));

	sheet = sv->priv->sheet;

	if (sheet->priv->floating_objects != NULL)
			rotate_items (sv, sheet->priv->floating_objects);
}

static void
flip_items (SchematicView *sv, GList *items, gboolean horizontal)
{
	GList *list, *item_data_list;
	SheetPos center, b1, b2;
	SheetPos after, delta;
	Sheet *sheet;
	gboolean got_first = FALSE;

	sheet = sv->priv->sheet;

	item_data_list = NULL;
	for (list = items; list; list = list->next) {
			item_data_list = g_list_prepend (item_data_list,
					sheet_item_get_data (list->data));
	}

	item_data_list_get_absolute_bbox (item_data_list, &b1, &b2);

	center.x = (b2.x - b1.x) / 2 + b1.x;
	center.y = (b2.y - b1.y) / 2 + b1.y;

	for (list = item_data_list; list; list = list->next) {
			ItemData *item_data = list->data;

			if (sv->priv->sheet->state == SHEET_STATE_NONE)
					item_data_unregister (item_data);

			item_data_flip (item_data, horizontal, &center);

			/*
			 * Make sure we snap to grid.
			 */
			if (!got_first) {
					SheetPos off;

					item_data_get_pos (item_data, &off);
					item_data_get_pos (item_data, &after);

					snap_to_grid (sheet->grid, &off.x, &off.y);

					delta.x = off.x - after.x;
					delta.y = off.y - after.y;

					got_first = TRUE;
			}
			item_data_move (item_data, &delta);

			if (sv->priv->sheet->state == SHEET_STATE_NONE)
					item_data_register (item_data);
	}

	g_list_free (item_data_list);
}

void
schematic_view_flip_selection (SchematicView *sv, gboolean horizontal)
{
	Sheet *sheet;

	g_return_if_fail (sv != NULL);
	g_return_if_fail (IS_SCHEMATIC_VIEW (sv));

	sheet = sv->priv->sheet;

	if (sheet->priv->selected_objects != NULL)
			flip_items (sv, sheet->priv->selected_objects, horizontal);
}

void
schematic_view_flip_ghosts (SchematicView *sv, gboolean horizontal)
{
	Sheet *sheet;

	g_return_if_fail (sv != NULL);
	g_return_if_fail (IS_SCHEMATIC_VIEW (sv));

	sheet = sv->priv->sheet;

	if (sheet->priv->floating_objects != NULL)
			flip_items (sv, sheet->priv->floating_objects, horizontal);
}

GList *
schematic_view_get_selection (SchematicView *sv)
{
	g_return_val_if_fail (sv != NULL, NULL);
	g_return_val_if_fail (IS_SCHEMATIC_VIEW (sv), NULL);

	return sv->priv->sheet->priv->selected_objects;
}

GList *
schematic_view_get_items (SchematicView *sv)
{
	g_return_val_if_fail (sv != NULL, NULL);
	g_return_val_if_fail (IS_SCHEMATIC_VIEW (sv), NULL);

	return sv->priv->items;
}

void
schematic_view_clear_ghosts (SchematicView *sv)
{
	Sheet *sheet;

	g_return_if_fail (sv != NULL);
	g_return_if_fail (IS_SCHEMATIC_VIEW (sv));
	sheet = sv->priv->sheet;

	if (sheet->priv->floating_objects == NULL) return;

	g_assert (sheet->state != SHEET_STATE_FLOAT);

	gnome_canvas_item_set (GNOME_CANVAS_ITEM (sheet->priv->floating_group),
			"x", 0.0, "y", 0.0, NULL);

	/* FIXME: should destroy ghosts here. */
	g_print("FIXME : Destroy ghosts here!\n");

	g_list_free (sheet->priv->floating_objects);
	sheet->priv->floating_objects = NULL;
}

void
schematic_view_set_parent (SchematicView *sv, GtkDialog *dialog)
{
	/*gnome_dialog_set_parent (dialog, GTK_WINDOW (sv->toplevel));*/
}

void
schematic_view_select_all (SchematicView *sv, gboolean select)
{
	Sheet *sheet;
	GList *list;

	g_return_if_fail (sv != NULL);
	g_return_if_fail (IS_SCHEMATIC_VIEW (sv));

	sheet = schematic_view_get_sheet (sv);

	for (list = sv->priv->items; list; list = list->next)
			sheet_item_select (SHEET_ITEM (list->data), select);

	if (!select) {
			g_list_free (sheet->priv->selected_objects);
			sheet->priv->selected_objects = NULL;
	}
}

static gboolean
log_window_delete_event (GtkWidget *widget, GdkEvent *event, SchematicView *sv)
{
	sv->priv->log_window = NULL;
	return FALSE;
}

static void
log_window_destroy_event (GtkWidget *widget, SchematicView *sv)
{
	sv->priv->log_window = NULL;
}

static void
log_window_close_cb (GtkWidget *widget, SchematicView *sv)
{
	gtk_widget_destroy (sv->priv->log_window);
	sv->priv->log_window = NULL;
}

static void
log_window_clear_cb (GtkWidget *widget, SchematicView *sv)
{
	GtkTextTagTable *tag;
	GtkTextBuffer *buf;

	tag = gtk_text_tag_table_new ();
	buf = gtk_text_buffer_new (GTK_TEXT_TAG_TABLE (tag));

	schematic_log_clear (sv->priv->schematic);

	gtk_text_view_set_buffer (GTK_TEXT_VIEW (sv->priv->log_text),
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

	g_return_if_fail (sv != NULL);
	g_return_if_fail (IS_SCHEMATIC_VIEW (sv));

	sm = schematic_view_get_schematic (sv);

	if (sv->priv->log_window == NULL) {
			/*
			 * Create the log window if not already done.
			 */

			if (!explicit && !oregano.show_log)
					return;

			if (!g_file_test (OREGANO_GLADEDIR "/log-window.glade", G_FILE_TEST_EXISTS)) {
					msg = g_strdup_printf (
							_("The file %s could not be found. You might need to reinstall Oregano to fix this."),
							OREGANO_GLADEDIR "/log-window.glade");

					oregano_error_with_title ( _("Could not create the log window"), msg);
					g_free (msg);
					return;
			}


			sv->priv->log_gui = glade_xml_new (
					OREGANO_GLADEDIR "/log-window.glade",
					NULL, NULL);

			if (!sv->priv->log_gui) {
					oregano_error (_("Could not create the log window"));
					return;
			}

			sv->priv->log_window = glade_xml_get_widget (sv->priv->log_gui,
					"log-window");
			sv->priv->log_text = GTK_TEXT_VIEW (
					glade_xml_get_widget (sv->priv->log_gui,
							"log-text"));

			/*		gtk_window_set_policy (GTK_WINDOW (sv->priv->log_window),
																 TRUE, TRUE, FALSE); */
			gtk_window_set_default_size (GTK_WINDOW (sv->priv->log_window),
					500, 250);

			/*
			 * Delete event.
			 */
			g_signal_connect (
					G_OBJECT (sv->priv->log_window), "delete_event",
					G_CALLBACK (log_window_delete_event), sv);

			g_signal_connect (
					G_OBJECT (sv->priv->log_window), "destroy_event",
					G_CALLBACK (log_window_destroy_event), sv);

			w = glade_xml_get_widget (sv->priv->log_gui, "close-button");
			g_signal_connect (G_OBJECT (w),
					"clicked", G_CALLBACK (log_window_close_cb), sv);

			w = glade_xml_get_widget (sv->priv->log_gui, "clear-button");
			g_signal_connect (G_OBJECT (w),
					"clicked", G_CALLBACK (log_window_clear_cb), sv);
			g_signal_connect(G_OBJECT (sm),
					"log_updated", G_CALLBACK(log_updated_callback), sv);
	} else {
			gdk_window_raise (sv->priv->log_window->window);
	}

	gtk_text_view_set_buffer (
			sv->priv->log_text,
			schematic_get_log_text (sm)
	);


	gtk_widget_show_all (sv->priv->log_window);
}

/** FIXME: only have one window for each schematic, not one per view. */
gboolean
schematic_view_log_window_exists (SchematicView *sv)
{
	if (sv->priv->log_window != NULL) {
			return TRUE;
	} else {
			return FALSE;
	}
}

static guint
dot_hash (gconstpointer key)
{
	SheetPos *sp = (SheetPos *) key;
	int x, y;

	x = (int)rint (sp->x) % 256;
	y = (int)rint (sp->y) % 256;

	return (y << 8) | x;
}

#define HASH_EPSILON 1e-2

static int
dot_equal (gconstpointer a, gconstpointer b)
{
	SheetPos *spa, *spb;

	g_return_val_if_fail (a!=NULL, 0);
	g_return_val_if_fail (b!=NULL, 0);

	spa = (SheetPos *) a;
	spb = (SheetPos *) b;

	if (fabs (spa->y - spb->y) > HASH_EPSILON)
			return 0;

	if (fabs (spa->x - spb->x) > HASH_EPSILON)
			return 0;

	return 1;
}

#define OP_VALUE_FONT "-*-helvetica-medium-r-*-*-*-80-*-*-*-*-*-*"

/**
* Temporary hack to test the OP analysis visualization.
* FIXME: solve similar to the connection dots.
*/
void
schematic_view_show_op_values (SchematicView *sv, OreganoEngine *engine)
{
	GList *nodes, *list;
	NodeStore *store;
	Node *node;
	GnomeCanvasItem *item, *group;
	double value;
	char *text, *tmp;
	gboolean got;

	g_return_if_fail (sv != NULL);
	g_return_if_fail (IS_SCHEMATIC_VIEW (sv));
	g_return_if_fail (engine != NULL);

	return;

	store = schematic_get_store (sv->priv->schematic);

	/*
	 * Iterate over the Nodes and find their resp. operation point.
	 */
	nodes = node_store_get_nodes (store);
	for (list = nodes; list; list = list->next) {
			node = list->data;

			got = 0; //sim_engine_get_op_value (engine, node->netlist_node_name, &value);
			if (!got) {
					tmp = g_strdup_printf ("V(%s)",
							node->netlist_node_name);
					//got = sim_engine_get_op_value (engine, tmp, &value);
			} else
					tmp = g_strdup (node->netlist_node_name);

			if (got) {
					/* Don't have more than one meter per node. */
					if (g_hash_table_lookup (sv->priv->voltmeter_nodes,
									tmp) != NULL)
							continue;

					g_hash_table_insert (sv->priv->voltmeter_nodes, tmp,
							"there");

					text = g_strdup_printf ("%.3f",			value);

					group = gnome_canvas_item_new (
							gnome_canvas_root (
									GNOME_CANVAS (sv->priv->sheet)),
							gnome_canvas_group_get_type (),
							"x", node->key.x - 20.0,
							"y", node->key.y - 20.0,
							NULL);

					sv->priv->voltmeter_items = g_list_prepend (
							sv->priv->voltmeter_items, group);

					item = gnome_canvas_item_new (
							GNOME_CANVAS_GROUP (group),
							gnome_canvas_rect_get_type (),
							"x1", 0.0,
							"y1", 0.0,
							"x2", 40.0,
							"y2", 10.0,
							"fill_color", "light gray",
							"outline_color", "black",
							NULL);

					gnome_canvas_item_new (
							GNOME_CANVAS_GROUP (group),
							gnome_canvas_text_get_type (),
							"x", 2.0,
							"y", 1.0,
							"anchor", GTK_ANCHOR_NW,
							"text", text,
							"fill_color", "black",
							"font", OP_VALUE_FONT,
							NULL);

					g_free (text);
			} else
					g_free (tmp);

	}

	g_list_free (nodes);

	schematic_view_show_voltmeters (sv, sv->priv->show_voltmeters);
}

void
schematic_view_clear_op_values (SchematicView *sv)
{
	GList *list;

	g_return_if_fail (sv != NULL);
	g_return_if_fail (IS_SCHEMATIC_VIEW (sv));

	for (list = sv->priv->voltmeter_items; list; list = list->next) {
			gtk_object_destroy (GTK_OBJECT (list->data));
	}

	g_list_free (sv->priv->voltmeter_items);
	sv->priv->voltmeter_items = NULL;

	/* DONE (NOT TESTED): free the keys. - */
	g_hash_table_destroy (sv->priv->voltmeter_nodes);
	sv->priv->voltmeter_nodes = g_hash_table_new_full (g_str_hash,
			g_str_equal, g_free, NULL);
}

static void
schematic_view_show_voltmeters (SchematicView *sv, gboolean show)
{
	GList *list;

	g_return_if_fail (sv != NULL);
	g_return_if_fail (IS_SCHEMATIC_VIEW (sv));

	for (list = sv->priv->voltmeter_items; list; list = list->next) {
			if (show)
					gnome_canvas_item_show (
							GNOME_CANVAS_ITEM (list->data));
			else
					gnome_canvas_item_hide (
							GNOME_CANVAS_ITEM (list->data));
	}
}

void
schematic_view_update_parts (SchematicView *sv)
{
	GList *list;

	g_return_if_fail (sv != NULL);
	g_return_if_fail (IS_SCHEMATIC_VIEW (sv));

	for (list = sv->priv->items; list; list = list->next) {
			if (IS_PART_ITEM (list->data))
					part_item_update_node_label (PART_ITEM (list->data));
	}
}

