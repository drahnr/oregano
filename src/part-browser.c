/*
 * part-browser.c
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

#include <gtk/gtk.h>
#include <gtk/gtktreeselection.h>
#include <gtk/gtktreemodel.h>
#include <glade/glade.h>
#include <math.h>
#include <string.h>
#include "main.h"
#include "load-library.h"
#include "schematic.h"
#include "schematic-view.h"
#include "part-browser.h"
#include "part-item.h"
#include "dialogs.h"
#include "sheet-pos.h"

typedef struct _Browser Browser;
struct _Browser {
	SchematicView	 *schematic_view;
	GtkWidget		 *viewport;
	GtkWidget		 *list;
	GtkWidget		 *canvas;
	GnomeCanvasText	 *description;
	GnomeCanvasGroup *preview;
	Library			 *library;
	/**
	 * The currently selected option menu item.
	 */
	GtkWidget		 *library_menu_item;

	gboolean		  hidden;

	/**
	 * Models for the TreeView
	 */
	GtkTreeModel *real_model;
	GtkTreeModel *sort_model;
	GtkTreeModel *filter_model;
	GtkEntry	 *filter_entry;
	gint		  filter_len;
};

typedef struct {
	Browser *br;
	SchematicView *schematic_view;
	char *library_name;
	char *part_name;
} DndData;

#define PREVIEW_WIDTH 100
#define PREVIEW_HEIGHT 100
#define PREVIEW_TEXT_HEIGHT 25

static void update_preview (Browser *br);
static void add_part (gpointer key, LibraryPart *part, Browser *br);
static int part_selected (GtkTreeView *list, GtkTreePath *arg1,
	GtkTreeViewColumn *col, Browser *br);
static void part_browser_setup_libs (Browser *br, GladeXML *gui);
static void library_switch_cb (GtkWidget *item, Browser *br);
static void preview_realized (GtkWidget *widget, Browser *br);
static void wrap_string(char* str, int width);
static void place_cmd (GtkWidget *widget, Browser *br);

static gboolean
part_list_filter_func (GtkTreeModel *model, GtkTreeIter *iter, gpointer data)
{
	char *part_name;
	const char *s;
	char *comp1, *comp2; /* auxilires para guardar el nombre en upcase */
	Browser *br = (Browser *)data;

	s = gtk_entry_get_text (GTK_ENTRY (br->filter_entry));
	/* Si no hay filtro, la componente se muestra */
	if (s == NULL) return TRUE;
	if (br->filter_len == 0) return TRUE;

	gtk_tree_model_get (model, iter, 0, &part_name, -1);

	if (part_name) {
		comp1 = g_utf8_strup (s, -1);
		comp2 = g_utf8_strup (part_name, -1);

		if (g_strrstr(comp2, comp1)) {
			g_free (comp1);
			g_free (comp2);
			return TRUE;
		}

		g_free (comp1);
		g_free (comp2);
	}
	return FALSE;
}

static int
part_search_change (GtkWidget *widget, Browser *br)
{
	const char *s = gtk_entry_get_text (GTK_ENTRY (widget));

	if (s) {
		/* Keep record of the filter text length for each item. */
		br->filter_len = strlen (s);
		gtk_tree_model_filter_refilter (GTK_TREE_MODEL_FILTER (br->filter_model));
	}

	return TRUE;
}

static void
part_search_activate (GtkWidget *widget, Browser *br)
{
	GtkTreeSelection *selection;
	GtkTreePath *path;

	path = gtk_tree_path_new_from_string ("0");
	if (path) {
		selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (br->list));

		gtk_tree_selection_select_path (selection, path);

		gtk_tree_path_free (path);
		place_cmd (widget, br);
	}
}

static void
place_cmd (GtkWidget *widget, Browser *br)
{
	LibraryPart *library_part;
	char *part_name;
	SheetPos pos;
	Sheet *sheet;
	Part *part;
	GtkTreeModel *model;
	GtkTreeIter iter;
	GtkTreeSelection *selection;

	schematic_view_reset_tool (br->schematic_view);
	sheet = schematic_view_get_sheet (br->schematic_view);

	/* Get the current selected row */
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (br->list));
	model = gtk_tree_view_get_model (GTK_TREE_VIEW (br->list));

	if (!gtk_tree_selection_get_selected (selection, NULL, &iter)) {
		return;
	}

	gtk_tree_model_get(model, &iter, 0, &part_name, -1);

	library_part = library_get_part (br->library, part_name);
	part = part_new_from_library_part (library_part);
	if (!part) {
		oregano_error(_("Unable to load required part"));
		return;
	}

	pos.x = pos.y = 0;
	item_data_set_pos (ITEM_DATA (part), &pos);

	schematic_view_select_all(br->schematic_view, FALSE);
	schematic_view_clear_ghosts(br->schematic_view);
	schematic_view_add_ghost_item(br->schematic_view, ITEM_DATA(part));

	part_item_signal_connect_floating_group (sheet, br->schematic_view);
}

static int
part_selected (GtkTreeView *list, GtkTreePath *arg1, GtkTreeViewColumn *col,
			   Browser *br)
{
	/* if double-click over an item, place it on work area */
	place_cmd (NULL, br);

	return FALSE;
}

static void
update_preview (Browser *br)
{
	LibraryPart *library_part;
	gdouble new_x, new_y, x1, y1, x2, y2;
	gdouble scale;
	double affine[6];
	double transf[6];
	gdouble width, height;
	gchar *part_name;
	char *description;
	gdouble text_width;
	GtkTreeModel *model;
	GtkTreeIter iter;
	GtkTreeSelection *selection;

	/* Get the current selected row */
	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(br->list));
	model = gtk_tree_view_get_model(GTK_TREE_VIEW(br->list));

	if (!gtk_tree_selection_get_selected(selection, NULL, &iter)) {
		return;
	}

	gtk_tree_model_get(model, &iter, 0, &part_name, -1);

	library_part = library_get_part (br->library, part_name);

	/*
	 * If there already is a preview part, destroy its group and create a
	 * new one.
	 */
	if (br->preview != NULL)
		/* FIXME: Check if gnomecanvas are killed by this way */
		gtk_object_destroy (GTK_OBJECT (br->preview));

	br->preview = GNOME_CANVAS_GROUP (gnome_canvas_item_new (
		gnome_canvas_root (GNOME_CANVAS (br->canvas)),
		gnome_canvas_group_get_type(),
		"x", 0.0,
		"y", 0.0,
		NULL));

	if (!library_part)
		return;

	part_item_create_canvas_items_for_preview (br->preview, library_part);

	width = br->canvas->allocation.width;
	height = br->canvas->allocation.height - PREVIEW_TEXT_HEIGHT;

	/* Get the coordonates */
	gnome_canvas_item_get_bounds (GNOME_CANVAS_ITEM (br->preview),
		&x1, &y1, &x2, &y2);

	/* Translate in such a way that the canvas centre remains in (0, 0) */
	art_affine_translate(transf, -(x2 + x1) / 2.0f, -(y2 + y1) / 2.0f);

	/* Compute the scale of the widget */
	if((x2 - x1 != 0) || (y2 - y1 != 0)) {
		if ((x2 - x1) < (y2 - y1))
			scale = 0.60f * PREVIEW_HEIGHT / (y2 - y1);
		else
			scale = 0.60f * PREVIEW_WIDTH / (x2 - x1);

	} else
		scale = 5;

	art_affine_scale (affine, scale, scale);
	art_affine_multiply (transf, transf, affine);

	/* Apply the transformation */
	gnome_canvas_item_affine_absolute (GNOME_CANVAS_ITEM (br->preview), transf);

	/* Get the new coordonates after transformation */
	gnome_canvas_item_get_bounds (GNOME_CANVAS_ITEM (br->preview),
		&x1, &y1, &x2, &y2);

	/* Compute the motion to centre the Preview widget */
	new_x = (PREVIEW_WIDTH - x1 - x2) / 2;
	new_y = (PREVIEW_HEIGHT - y1 - y2) / 2;

	art_affine_translate (affine, new_x, new_y);
	art_affine_multiply (transf, transf, affine);

	/* Apply the transformation */
	gnome_canvas_item_affine_absolute (GNOME_CANVAS_ITEM (br->preview),
		transf);

	description = g_strdup (library_part->description);
	wrap_string (description, 20);
	gnome_canvas_item_set (GNOME_CANVAS_ITEM (br->description), "text",
						   description, NULL);
	g_free (description);

	g_object_get (G_OBJECT (br->description),
		"text_width", &text_width, NULL);

	g_object_set (G_OBJECT(br->description), "x",
		PREVIEW_WIDTH / 2 - text_width / 2, NULL);
	gnome_canvas_item_request_update (GNOME_CANVAS_ITEM (br->description));
	g_free(part_name);
}

static gboolean
select_row (GtkTreeView *list, Browser *br)
{
	update_preview (br);
	return FALSE;
}

static void
add_part (gpointer key, LibraryPart *part, Browser *br)
{
	GtkTreeIter iter;
	//GtkTreeModel *sort; /* The sortable interface */
	GtkListStore *model;

	g_return_if_fail (part != NULL);
	g_return_if_fail (part->name != NULL);
	g_return_if_fail (br != NULL);
	g_return_if_fail (br->list != NULL);

	model = GTK_LIST_STORE(br->real_model);
	gtk_list_store_append(model, &iter);
	gtk_list_store_set(model, &iter, 0, part->name, -1);
}

/*
 * Read the available parts from the library and put them in the browser clist.
 */
static void
update_list (Browser *br)
{
	GtkListStore *model;

	model = GTK_LIST_STORE (br->real_model);
	gtk_list_store_clear (model);
	g_hash_table_foreach (br->library->part_hash, (GHFunc) add_part, br);
}


/**
 * Show a part browser. If one already exists, just bring it up, otherwise
 * create it.  We can afford to keep it in memory all the time, and we don't
 * have to delete it and build it every time it is needed. If already shown,
 * hide it.
 */
void
part_browser_toggle_show (SchematicView *schematic_view)
{
	Browser *browser = schematic_view_get_browser (schematic_view);
		
	if (browser == NULL){
		part_browser_create (schematic_view);
	} else {
		browser->hidden = !(browser->hidden);
		if (browser->hidden){
			gtk_widget_hide_all (browser->viewport);
		} else {
			gtk_widget_show_all (browser->viewport);
		}
	}
}

void
part_browser_dnd (GtkSelectionData *selection_data, int x, int y)
{
	LibraryPart *library_part;
	SheetPos pos;
	DndData *data;
	Sheet *sheet;
	Part *part;

	data = (DndData *) (selection_data->data);

	g_return_if_fail (data != NULL);

	pos.x = x;
	pos.y = y;

	sheet = schematic_view_get_sheet (data->schematic_view);

	snap_to_grid (sheet->grid, &pos.x, &pos.y);

	library_part = library_get_part (data->br->library, data->part_name);
	part = part_new_from_library_part (library_part);
	if (!part) {
		oregano_error(_("Unable to load required part"));
		return;
	}

	item_data_set_pos (ITEM_DATA (part), &pos);

	schematic_view_select_all (data->schematic_view, FALSE);
	schematic_view_clear_ghosts (data->schematic_view);
	schematic_view_add_ghost_item (data->schematic_view, ITEM_DATA (part));

	part_item_signal_connect_floating_group (sheet, data->schematic_view);
}

static void
drag_data_get (GtkWidget *widget, GdkDragContext *context,
	GtkSelectionData *selection_data, guint info, guint time, Browser *br)
{
	DndData *data;
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkTreeIter iter;
	gchar *part_name;

	schematic_view_reset_tool (br->schematic_view);

	data = g_new0 (DndData, 1);

	data->schematic_view = br->schematic_view;
	data->br = br;

	data->library_name = br->library->name;

	/* Get the current selected row */
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (br->list));
	model = gtk_tree_view_get_model (GTK_TREE_VIEW (br->list));

	if (!gtk_tree_selection_get_selected (selection, NULL, &iter)) {
		/* No selection, Action canceled */
		return;
	}

	gtk_tree_model_get(model, &iter, 0, &part_name, -1);

	data->part_name = part_name;

	gtk_selection_data_set (selection_data,
		selection_data->target,
		8,
		(gpointer) data,
		sizeof (DndData));

	/*
	 * gtk_selection_data_set copies the information so we can free it now.
	 */
	g_free (data);
}

/*
 * part_browser_create
 *
 * Creates a new part browser. This is only called once per schematic window.
 */
GtkWidget *
part_browser_create (SchematicView *schematic_view)
{
	Browser *br;
	GladeXML *gui;
	char *msg;
	GtkWidget *w;
	GtkCellRenderer *cell_text;
	GtkTreeViewColumn *cell_column;
	GtkStyle *style;
	GdkColormap *colormap;
	GnomeCanvasPoints *points;
	static GtkTargetEntry dnd_types[] =
		{ { "x-application/oregano-part", 0, DRAG_PART_INFO } };

	static int dnd_num_types = sizeof (dnd_types) / sizeof (dnd_types[0]);
	GtkTreePath *path;

	br = g_new0 (Browser, 1);
	br->preview = NULL;
	br->schematic_view = schematic_view;
	br->hidden = FALSE;

	schematic_view_set_browser(schematic_view, br);

	if (!g_file_test (OREGANO_GLADEDIR "/part-browser.glade",
		    G_FILE_TEST_EXISTS)) {
		msg = g_strdup_printf (
			_("The file %s could not be found. You might need to reinstall Oregano to fix this."),
			OREGANO_GLADEDIR "/part-browser.glade");

		oregano_error_with_title (_("Could not create part browser"), msg);
		g_free (msg);
		return NULL;
	}

	gui = glade_xml_new (OREGANO_GLADEDIR "/part-browser.glade",
		"part_browser_vbox", GETTEXT_PACKAGE);
	if (!gui) {
		oregano_error (_("Could not create part browser"));
		return NULL;
	}

	w = glade_xml_get_widget (gui, "preview_canvas");
	br->canvas = w;

	g_signal_connect(w, "realize", (GCallback) preview_realized, br);

	style =  gtk_style_new ();
	colormap = gtk_widget_get_colormap (GTK_WIDGET (w));
	style->bg[GTK_STATE_NORMAL].red = 65535;
	style->bg[GTK_STATE_NORMAL].blue = 65535;
	style->bg[GTK_STATE_NORMAL].green = 65535;
	gdk_colormap_alloc_color (colormap, &style->bg[GTK_STATE_NORMAL],
		TRUE, TRUE);
	gtk_widget_set_style (GTK_WIDGET (w), style);
	gtk_widget_set_usize (w, PREVIEW_WIDTH,
		PREVIEW_HEIGHT + PREVIEW_TEXT_HEIGHT);
	gnome_canvas_set_scroll_region (GNOME_CANVAS (w), 0, 0, PREVIEW_WIDTH,
		PREVIEW_HEIGHT + PREVIEW_TEXT_HEIGHT);

	points = gnome_canvas_points_new (2);
	points->coords[0] = - 10;
	points->coords[1] = PREVIEW_HEIGHT - 10;
	points->coords[2] = PREVIEW_WIDTH + 10;
	points->coords[3] = PREVIEW_HEIGHT - 10;

	gnome_canvas_item_new (gnome_canvas_root (GNOME_CANVAS(br->canvas)),
		gnome_canvas_line_get_type (),
		"fill_color", "gray",
		"line_style", GDK_LINE_ON_OFF_DASH,
		"points", points,
		NULL);

	gnome_canvas_points_unref (points);

	br->description = GNOME_CANVAS_TEXT (
		gnome_canvas_item_new (
			gnome_canvas_root (GNOME_CANVAS (br->canvas)),
			gnome_canvas_text_get_type (),
			"x", 0.0,
			"y", PREVIEW_HEIGHT - 9.0,
			"anchor", GTK_ANCHOR_NORTH_WEST,
			"justification", GTK_JUSTIFY_CENTER,
			"font", "sans 10",
			"text", "",
			NULL));

	/*
	 * Set up dnd.
	 */
	g_signal_connect (G_OBJECT (br->canvas), "drag_data_get",
		GTK_SIGNAL_FUNC (drag_data_get), br);

	gtk_drag_source_set (br->canvas, GDK_BUTTON1_MASK | GDK_BUTTON3_MASK,
		dnd_types, dnd_num_types, GDK_ACTION_MOVE);

	br->filter_entry = GTK_ENTRY (glade_xml_get_widget(gui, "part_search"));

	g_signal_connect (G_OBJECT (br->filter_entry), "changed",
		G_CALLBACK (part_search_change), br);
	g_signal_connect (G_OBJECT (br->filter_entry), "activate",
		G_CALLBACK (part_search_activate), br);

	/* Buttons. */
	w = glade_xml_get_widget (gui, "place_button");
	g_signal_connect (G_OBJECT (w), "clicked",
		GTK_SIGNAL_FUNC (place_cmd), br);

	/* Update the libraries option menu */
	br->library = g_list_nth_data (oregano.libraries, 0);
	part_browser_setup_libs (br, gui);

	/* Parts list. */
	w = glade_xml_get_widget (gui, "parts_list");
	br->list = w;

	/* Create de ListModel for TreeView, this is a Real model */
	br->real_model = GTK_TREE_MODEL (gtk_list_store_new (1, G_TYPE_STRING));
	cell_text = gtk_cell_renderer_text_new ();
	cell_column = gtk_tree_view_column_new_with_attributes (
		" ", cell_text, "text", 0, NULL);

	/* Create the sort model for the items, this sort the real model */
	br->sort_model = gtk_tree_model_sort_new_with_model (
		GTK_TREE_MODEL(br->real_model));

	gtk_tree_sortable_set_sort_column_id (
		GTK_TREE_SORTABLE(br->sort_model),
		0, GTK_SORT_ASCENDING);

	/* Create the filter sorted model. This filter items based on user
	   request for fast item search */

	br->filter_model = gtk_tree_model_filter_new (br->sort_model, NULL);
	gtk_tree_model_filter_set_visible_func (
		GTK_TREE_MODEL_FILTER (br->filter_model),
		part_list_filter_func, br, NULL);

	/* If we have TreeFilter use it, if not, just use sorting model only */
	if (br->filter_model)
		gtk_tree_view_set_model (GTK_TREE_VIEW(w), br->filter_model);
	else
		gtk_tree_view_set_model (GTK_TREE_VIEW(w), br->sort_model);

	gtk_tree_view_append_column (GTK_TREE_VIEW(w), cell_column);
	update_list (br);

	/*
	 * Set up TreeView dnd.
	 */
	g_signal_connect (G_OBJECT (w), "drag_data_get",
		GTK_SIGNAL_FUNC (drag_data_get), br);

	gtk_drag_source_set (w, GDK_BUTTON1_MASK | GDK_BUTTON3_MASK,
		dnd_types, dnd_num_types, GDK_ACTION_MOVE);

	g_signal_connect (G_OBJECT (w),
		"cursor_changed", G_CALLBACK(select_row), br);
	g_signal_connect (G_OBJECT (w),
		"row_activated", G_CALLBACK(part_selected), br);

	br->viewport = glade_xml_get_widget (gui, "part_browser_vbox");

	path = gtk_tree_path_new_first ();
	gtk_tree_view_set_cursor (GTK_TREE_VIEW (w),path, NULL, FALSE);
	gtk_tree_path_free (path);

	return br->viewport;
}

static void
part_browser_setup_libs(Browser *br, GladeXML *gui) {

	GtkWidget *combo_box, *w;

	GList *libs;
	gboolean first = FALSE;

	w = glade_xml_get_widget (gui, "library_optionmenu");
	gtk_widget_destroy (w);

	w = glade_xml_get_widget (gui, "table1");
	combo_box = gtk_combo_box_new_text ();
	gtk_table_attach_defaults (GTK_TABLE(w),combo_box,1,2,0,1);

	libs = oregano.libraries;

	while (libs) {
		gtk_combo_box_append_text (GTK_COMBO_BOX (combo_box),
			((Library *)libs->data)->name);
		libs = libs->next;
		if (!first) {
			gtk_combo_box_set_active (GTK_COMBO_BOX(combo_box),0);
			first = TRUE;
		}
	}

	g_signal_connect (G_OBJECT (combo_box),
		"changed", G_CALLBACK(library_switch_cb), br);
}

static void
library_switch_cb (GtkWidget *combo_box, Browser *br)
{
	GList *libs = oregano.libraries;

	br->library = (Library *) g_list_nth_data (libs,
		gtk_combo_box_get_active (GTK_COMBO_BOX (combo_box)));

	update_list (br);
}

static void
wrap_string (char* str, int width)
{
	const int minl = width/2;
	char *lnbeg, *sppos, *ptr;
	int te = 0;

	g_return_if_fail (str != NULL);

	lnbeg= sppos = ptr = str;

	while (*ptr) {
		if (*ptr == '\t')
			te += 7;

		if (*ptr == ' ')
			sppos = ptr;

		if(ptr - lnbeg > width - te && sppos >= lnbeg + minl) {
			*sppos = '\n';
			lnbeg = ptr;
			te = 0;
		}

		if(*ptr=='\n') {
			lnbeg = ptr;
			te = 0;
		}
		ptr++;
	}
}

static void
preview_realized (GtkWidget *widget, Browser *br)
{
	update_preview (br);
}

void 
part_browser_reparent (gpointer *br, GtkWidget *new_parent)
{
	Browser *b;
	g_return_if_fail (br != NULL);
	b = (Browser *)br;
	gtk_widget_reparent (GTK_WIDGET (b->viewport), new_parent);
}
