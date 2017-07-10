/*
 * part-browser.c
 *
 *
 * Authors:
 *  Richard Hult <rhult@hem.passagen.se>
 *  Ricardo Markiewicz <rmarkie@fi.uba.ar>
 *  Andres de Barbara <adebarbara@fi.uba.ar>
 *  Marc Lorber <lorber.marc@wanadoo.fr>
 *  Bernhard Schuster <bernhard@ahoi.io>
 *
 * Web page: https://ahoi.io/project/oregano
 *
 * Copyright (C) 1999-2001  Richard Hult
 * Copyright (C) 2003,2006  Ricardo Markiewicz
 * Copyright (C) 2009-2012  Marc Lorber
 * Copyright (C) 2013       Bernhard Schuster
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

#include <gtk/gtk.h>
#include <string.h>
#include <glib/gi18n.h>
#include <goocanvas.h>

#include "oregano.h"
#include "load-library.h"
#include "schematic.h"
#include "schematic-view.h"
#include "part-browser.h"
#include "part-item.h"
#include "dialogs.h"
#include "coords.h"
#include "sheet.h"

#include "debug.h"

typedef struct _Browser Browser;
struct _Browser
{
	SchematicView *schematic_view;
	GtkWidget *viewport;
	GtkWidget *list;
	GtkWidget *canvas;
	GooCanvasText *description;
	GooCanvasGroup *preview;
	Library *library;
	gboolean hidden;
	// Models for the TreeView
	GtkTreeModel *real_model;
	GtkTreeModel *sort_model;
	GtkTreeModel *filter_model;
	GtkEntry *filter_entry;
	gint filter_len;
};

typedef struct
{
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
static int part_selected (GtkTreeView *list, GtkTreePath *arg1, GtkTreeViewColumn *col,
                          Browser *br);
static void part_browser_setup_libs (Browser *br, GtkBuilder *gui);
static void library_switch_cb (GtkWidget *item, Browser *br);
static void preview_realized (GtkWidget *widget, Browser *br);
static void wrap_string (char *str, int width);
static void place_cmd (GtkWidget *widget, Browser *br);

static gboolean part_list_filter_func (GtkTreeModel *model, GtkTreeIter *iter, gpointer data)
{
	char *part_name;
	const char *s;
	// Auxiliary parameters shall keep their number in upcase
	char *comp1, *comp2;
	Browser *br = (Browser *)data;

	s = gtk_entry_get_text (GTK_ENTRY (br->filter_entry));
	// Without filter, the part is shown
	if (s == NULL)
		return TRUE;
	if (br->filter_len == 0)
		return TRUE;

	gtk_tree_model_get (model, iter, 0, &part_name, -1);

	if (part_name) {
		comp1 = g_utf8_strup (s, -1);
		comp2 = g_utf8_strup (part_name, -1);

		if (g_strrstr (comp2, comp1)) {
			g_free (comp1);
			g_free (comp2);
			return TRUE;
		}

		g_free (comp1);
		g_free (comp2);
	}
	return FALSE;
}

static int part_search_change (GtkWidget *widget, Browser *br)
{
	const char *s = gtk_entry_get_text (GTK_ENTRY (widget));

	if (s) {
		// Keep record of the filter text length for each item.
		br->filter_len = strlen (s);
		gtk_tree_model_filter_refilter (GTK_TREE_MODEL_FILTER (br->filter_model));
	}

	return TRUE;
}

static void part_search_activate (GtkWidget *widget, Browser *br)
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

static void place_cmd (GtkWidget *widget, Browser *br)
{
	LibraryPart *library_part;
	char *part_name;
	Coords pos;
	Sheet *sheet;
	Part *part;
	GtkTreeModel *model;
	GtkTreeIter iter;
	GtkTreeSelection *selection;

	schematic_view_reset_tool (br->schematic_view);
	sheet = schematic_view_get_sheet (br->schematic_view);

	// Get the current selected row
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (br->list));
	model = gtk_tree_view_get_model (GTK_TREE_VIEW (br->list));

	if (!gtk_tree_selection_get_selected (selection, NULL, &iter)) {
		return;
	}

	gtk_tree_model_get (model, &iter, 0, &part_name, -1);

	library_part = library_get_part (br->library, part_name);
	part = part_new_from_library_part (library_part);
	if (!part) {
		oregano_error (_ ("Unable to load required part"));
		return;
	}

	pos.x = pos.y = 0;
	item_data_set_pos (ITEM_DATA (part), &pos);
	sheet_connect_part_item_to_floating_group (sheet, (gpointer)br->schematic_view);

	sheet_select_all (sheet, FALSE);
	sheet_clear_ghosts (sheet);
	sheet_add_ghost_item (sheet, ITEM_DATA (part));
}

static int part_selected (GtkTreeView *list, GtkTreePath *arg1, GtkTreeViewColumn *col, Browser *br)
{
	// if double-click over an item, place it on work area
	place_cmd (NULL, br);

	return FALSE;
}

static void update_preview (Browser *br)
{
	LibraryPart *library_part;
	gdouble new_x, new_y, x1, y1, x2, y2;
	gdouble text_width;
	gdouble width, height;
	GooCanvasBounds bounds;
	gdouble scale;
	cairo_matrix_t transf, affine;
	gchar *part_name;
	char *description;
	GtkTreeModel *model;
	GtkTreeIter iter;
	GtkTreeSelection *selection;

	// Get the current selected row
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (br->list));
	model = gtk_tree_view_get_model (GTK_TREE_VIEW (br->list));

	if (!GTK_IS_TREE_SELECTION (selection))
		return;

	if (!gtk_tree_selection_get_selected (selection, NULL, &iter)) {
		return;
	}

	gtk_tree_model_get (model, &iter, 0, &part_name, -1);

	library_part = library_get_part (br->library, part_name);

	// If there is already a preview part-item, destroy its group and create a
	// new one.
	if (br->preview != NULL) {
		goo_canvas_item_remove (GOO_CANVAS_ITEM (br->preview));
	}

	br->preview = GOO_CANVAS_GROUP (
	    goo_canvas_group_new (goo_canvas_get_root_item (GOO_CANVAS (br->canvas)), NULL));

	goo_canvas_set_bounds (GOO_CANVAS (br->canvas), 0.0, 0.0, 250.0, 110.0);

	g_object_get (br->preview, "width", &width, "height", &height, NULL);

	if (!library_part)
		return;

	part_item_create_canvas_items_for_preview (br->preview, library_part);

	// Unconstraint the canvas width & height to adjust for the part description
	g_object_set (br->preview, "width", -1.0, "height", -1.0, NULL);

	// Get the coordonates */
	goo_canvas_item_get_bounds (GOO_CANVAS_ITEM (br->preview), &bounds);
	x1 = bounds.x1;
	x2 = bounds.x2;
	y1 = bounds.y1;
	y2 = bounds.y2;

	// Translate in such a way that the canvas centre remains in (0, 0)
	cairo_matrix_init_translate (&transf, -(x2 + x1) / 2.0f + PREVIEW_WIDTH / 2,
	                             -(y2 + y1) / 2.0f + PREVIEW_HEIGHT / 2);

	// Compute the scale of the widget
	if ((x2 - x1 != 0) || (y2 - y1 != 0)) {
		if ((x2 - x1) < (y2 - y1))
			scale = 0.60f * PREVIEW_HEIGHT / (y2 - y1);
		else
			scale = 0.60f * PREVIEW_WIDTH / (x2 - x1);
	} else
		scale = 5;

	cairo_matrix_init_scale (&affine, scale, scale);
	cairo_matrix_multiply (&transf, &transf, &affine);

	// Apply the transformation
	goo_canvas_item_set_transform (GOO_CANVAS_ITEM (br->preview), &transf);

	// Compute the motion to centre the Preview widget
	new_x = 100 + (PREVIEW_WIDTH - x1 - x2) / 2;
	new_y = (PREVIEW_HEIGHT - y1 - y2) / 2 - 10;

	// Apply the transformation
	if (scale > 5.0)
		scale = 3.0;
	goo_canvas_item_set_simple_transform (GOO_CANVAS_ITEM (br->preview), new_x, new_y, scale, 0.0);

	description = g_strdup (library_part->description);
	wrap_string (description, 20);
	g_object_set (br->description, "text", description, NULL);
	g_free (description);

	g_object_get (G_OBJECT (br->description), "width", &text_width, NULL);

	goo_canvas_item_set_simple_transform (GOO_CANVAS_ITEM (br->description), 50.0, -20.0, 1.0, 0.0);
	g_free (part_name);
}

static gboolean select_row (GtkTreeView *list, Browser *br)
{
	update_preview (br);
	return FALSE;
}

static void add_part (gpointer key, LibraryPart *part, Browser *br)
{
	GtkTreeIter iter;
	GtkListStore *model;

	g_return_if_fail (part != NULL);
	g_return_if_fail (part->name != NULL);
	g_return_if_fail (br != NULL);
	g_return_if_fail (br->list != NULL);

	model = GTK_LIST_STORE (br->real_model);
	gtk_list_store_append (model, &iter);
	gtk_list_store_set (model, &iter, 0, part->name, -1);
}

// Read the available parts from the library and put them in the browser clist.
static void update_list (Browser *br)
{
	GtkListStore *model;

	model = GTK_LIST_STORE (br->real_model);
	gtk_list_store_clear (model);
	g_hash_table_foreach (br->library->part_hash, (GHFunc)add_part, br);
}

// Show a part browser. If one already exists, just bring it up, otherwise
// create it.  We can afford to keep it in memory all the time, and we don't
// have to delete it and build it every time it is needed. If already shown,
// hide it.
void part_browser_toggle_visibility (SchematicView *schematic_view)
{
	Browser *browser = schematic_view_get_browser (schematic_view);

	if (browser == NULL) {
		//	part_browser_create (schematic_view);
		g_warning ("This should never happen. No Part Browser.");
	} else {
		browser->hidden = !(browser->hidden);
		if (browser->hidden) {
			gtk_widget_hide (browser->viewport);
		} else {
			gtk_widget_show_all (browser->viewport);
		}
	}
}

void part_browser_dnd (GtkSelectionData *selection_data, int x, int y)
{
	LibraryPart *library_part;
	Coords pos;
	DndData *data;
	Sheet *sheet;
	Part *part;

	data = (DndData *)(gtk_selection_data_get_data (selection_data));

	g_return_if_fail (data != NULL);

	pos.x = x;
	pos.y = y;

	sheet = schematic_view_get_sheet (data->schematic_view);

	snap_to_grid (sheet->grid, &pos.x, &pos.y);

	library_part = library_get_part (data->br->library, data->part_name);
	part = part_new_from_library_part (library_part);
	if (!part) {
		oregano_error (_ ("Unable to load required part"));
		return;
	}

	item_data_set_pos (ITEM_DATA (part), &pos);
	sheet_connect_part_item_to_floating_group (sheet, (gpointer)data->schematic_view);

	sheet_select_all (sheet, FALSE);
	sheet_clear_ghosts (schematic_view_get_sheet (data->schematic_view));
	sheet_add_ghost_item (sheet, ITEM_DATA (part));
}

static void drag_data_get (GtkWidget *widget, GdkDragContext *context,
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

	// Get the current selected row
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (br->list));
	model = gtk_tree_view_get_model (GTK_TREE_VIEW (br->list));

	if (!gtk_tree_selection_get_selected (selection, NULL, &iter)) {
		// No selection, Action canceled
		return;
	}

	gtk_tree_model_get (model, &iter, 0, &part_name, -1);

	data->part_name = part_name;

	gtk_selection_data_set (selection_data, gtk_selection_data_get_target (selection_data), 8,
	                        (gpointer)data, sizeof(DndData));

	// gtk_selection_data_set copies the information so we can free it now.
	g_free (data);
}

// part_browser_create
//
// Creates a new part browser. This is only called once per schematic window.
GtkWidget *part_browser_create (SchematicView *schematic_view)
{
	Browser *br;
	GtkBuilder *builder;
	GError *e = NULL;
	GtkWidget *w, *view;
	GtkCellRenderer *cell_text;
	GtkTreeViewColumn *cell_column;
	static GtkTargetEntry dnd_types[] = {{"x-application/oregano-part", 0, DRAG_PART_INFO}};

	static int dnd_num_types = sizeof(dnd_types) / sizeof(dnd_types[0]);
	GtkTreePath *path;

	if ((builder = gtk_builder_new ()) == NULL) {
		oregano_error (_ ("Could not create part browser"));
		return NULL;
	} else
		gtk_builder_set_translation_domain (builder, NULL);

	br = g_new0 (Browser, 1);
	br->preview = NULL;
	br->schematic_view = schematic_view;
	br->hidden = FALSE;

	schematic_view_set_browser (schematic_view, br);

	if (gtk_builder_add_from_file (builder, OREGANO_UIDIR "/part-browser.ui", &e) <= 0) {
		oregano_error_with_title (_ ("Could not create part browser"), e->message);
		g_error_free (e);
		return NULL;
	}

	view = GTK_WIDGET (gtk_builder_get_object (builder, "viewport1"));
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (view), GTK_POLICY_AUTOMATIC,
	                                GTK_POLICY_AUTOMATIC);

	gtk_scrolled_window_set_min_content_height (GTK_SCROLLED_WINDOW (view), 115);

	w = goo_canvas_new ();
	gtk_container_add (GTK_CONTAINER (view), GTK_WIDGET (w));

	br->canvas = w;

	g_signal_connect (w, "realize", (GCallback)preview_realized, br);

	// gtk_widget_set_size_request (w, PREVIEW_WIDTH,
	//	PREVIEW_HEIGHT + PREVIEW_TEXT_HEIGHT);
	goo_canvas_set_bounds (GOO_CANVAS (w), 0, 0, PREVIEW_WIDTH,
	                       (PREVIEW_HEIGHT + PREVIEW_TEXT_HEIGHT));

	br->description = GOO_CANVAS_TEXT (goo_canvas_text_new (
	    goo_canvas_get_root_item (GOO_CANVAS (br->canvas)), "", 0.0, PREVIEW_HEIGHT - 9.0, 100.0,
	    GOO_CANVAS_ANCHOR_NORTH_WEST, "font", "sans 9", NULL));

	// Set up dnd.
	g_signal_connect (G_OBJECT (br->canvas), "drag_data_get", G_CALLBACK (drag_data_get), br);

	gtk_drag_source_set (br->canvas, GDK_BUTTON1_MASK | GDK_BUTTON3_MASK, dnd_types, dnd_num_types,
	                     GDK_ACTION_MOVE);

	br->filter_entry = GTK_ENTRY (gtk_builder_get_object (builder, "part_search"));

	g_signal_connect (G_OBJECT (br->filter_entry), "changed", G_CALLBACK (part_search_change), br);
	g_signal_connect (G_OBJECT (br->filter_entry), "activate", G_CALLBACK (part_search_activate),
	                  br);

	// Buttons.
	w = GTK_WIDGET (gtk_builder_get_object (builder, "place_button"));
	g_signal_connect (G_OBJECT (w), "clicked", G_CALLBACK (place_cmd), br);

	// Update the libraries option menu
	br->library = g_list_nth_data (oregano.libraries, 0);
	part_browser_setup_libs (br, builder);

	// Parts list.
	w = GTK_WIDGET (gtk_builder_get_object (builder, "parts_list"));
	br->list = w;

	// Create the List Model for TreeView, this is a Real model
	br->real_model = GTK_TREE_MODEL (gtk_list_store_new (1, G_TYPE_STRING));
	cell_text = gtk_cell_renderer_text_new ();
	cell_column = gtk_tree_view_column_new_with_attributes ("", cell_text, "text", 0, NULL);

	// Create the sort model for the items, this sort the real model
	br->sort_model = gtk_tree_model_sort_new_with_model (GTK_TREE_MODEL (br->real_model));

	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (br->sort_model), 0,
	                                      GTK_SORT_ASCENDING);

	// Create the filter sorted model. This filter items based on user
	//   request for fast item search
	br->filter_model = gtk_tree_model_filter_new (br->sort_model, NULL);
	gtk_tree_model_filter_set_visible_func (GTK_TREE_MODEL_FILTER (br->filter_model),
	                                        part_list_filter_func, br, NULL);

	// If we have TreeFilter use it, if not, just use sorting model only
	if (br->filter_model)
		gtk_tree_view_set_model (GTK_TREE_VIEW (w), br->filter_model);
	else
		gtk_tree_view_set_model (GTK_TREE_VIEW (w), br->sort_model);

	gtk_tree_view_append_column (GTK_TREE_VIEW (w), cell_column);
	update_list (br);

	// Set up TreeView dnd.
	g_signal_connect (G_OBJECT (w), "drag_data_get", G_CALLBACK (drag_data_get), br);

	gtk_drag_source_set (w, GDK_BUTTON1_MASK | GDK_BUTTON3_MASK, dnd_types, dnd_num_types,
	                     GDK_ACTION_MOVE);

	g_signal_connect (G_OBJECT (w), "cursor_changed", G_CALLBACK (select_row), br);
	g_signal_connect (G_OBJECT (w), "row_activated", G_CALLBACK (part_selected), br);

	br->viewport = GTK_WIDGET (gtk_builder_get_object (builder, "part_browser_vbox"));

	path = gtk_tree_path_new_first ();
	gtk_tree_view_set_cursor (GTK_TREE_VIEW (w), path, NULL, FALSE);
	gtk_tree_path_free (path);

	gtk_widget_unparent (br->viewport);
	return br->viewport;
}

static void part_browser_setup_libs (Browser *br, GtkBuilder *builder)
{

	GtkWidget *combo_box, *w;

	GList *libs;
	gboolean first = FALSE;

	w = GTK_WIDGET (gtk_builder_get_object (builder, "library_optionmenu"));
	gtk_widget_destroy (w);

	w = GTK_WIDGET (gtk_builder_get_object (builder, "grid1"));
	combo_box = gtk_combo_box_text_new ();
	gtk_grid_attach (GTK_GRID (w), combo_box, 1, 0, 1, 1);

	libs = oregano.libraries;

	while (libs) {
		gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo_box),
		                                ((Library *)libs->data)->name);
		libs = libs->next;
		if (!first) {
			gtk_combo_box_set_active (GTK_COMBO_BOX (combo_box), 0);
			first = TRUE;
		}
	}
	g_signal_connect (G_OBJECT (combo_box), "changed", G_CALLBACK (library_switch_cb), br);
}

static void library_switch_cb (GtkWidget *combo_box, Browser *br)
{
	GtkTreePath *path;
	GList *libs = oregano.libraries;

	br->library =
	    (Library *)g_list_nth_data (libs, gtk_combo_box_get_active (GTK_COMBO_BOX (combo_box)));

	update_list (br);

	path = gtk_tree_path_new_first ();
	gtk_tree_view_set_cursor (GTK_TREE_VIEW (br->list), path, NULL, FALSE);
	gtk_tree_path_free (path);
}

static void wrap_string (char *str, int width)
{
	const int minl = width / 2;
	char *lnbeg, *sppos, *ptr;
	int te = 0;

	g_return_if_fail (str != NULL);

	lnbeg = sppos = ptr = str;

	while (*ptr) {
		if (*ptr == '\t')
			te += 7;

		if (*ptr == ' ')
			sppos = ptr;

		if (ptr - lnbeg > width - te && sppos >= lnbeg + minl) {
			*sppos = '\n';
			lnbeg = ptr;
			te = 0;
		}

		if (*ptr == '\n') {
			lnbeg = ptr;
			te = 0;
		}
		ptr++;
	}
}

static void preview_realized (GtkWidget *widget, Browser *br) { update_preview (br); }
