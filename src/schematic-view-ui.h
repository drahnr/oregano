/*
 * schematic-view-ui.h
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
 * Copyright (C) 2003,2004  LUGFI
 *
 * This library is free software; you can redistribute it and/or
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __SCHEMATIC_VIEW_UI_H
#define __SCHEMATIC_VIEW_UI_H

/*
 * File menu.
 */
static GnomeUIInfo schematic_menu_file [] = {
	GNOMEUIINFO_MENU_NEW_ITEM(N_("_New"), N_("Create a new schematic"),
		new_cmd, NULL),
	GNOMEUIINFO_MENU_OPEN_ITEM(open_cmd, NULL),
	GNOMEUIINFO_MENU_SAVE_ITEM(save_cmd, NULL),
	GNOMEUIINFO_MENU_SAVE_AS_ITEM(save_as_cmd, NULL),

	GNOMEUIINFO_SEPARATOR,
	
	{ GNOME_APP_UI_ITEM,
	  N_("Page Properties"),
	  N_("Set print properties"),
	  page_properties_cmd, NULL, NULL,
	  GNOME_APP_PIXMAP_STOCK, 0,
	  0, 0, NULL },

	GNOMEUIINFO_MENU_PRINT_ITEM(print_cmd, NULL),

	{ GNOME_APP_UI_ITEM,
	  N_("Print Preview"),
	  N_("Preview the schematic before printing"),
	  print_preview_cmd, NULL, NULL,
	  GNOME_APP_PIXMAP_STOCK, GTK_STOCK_PRINT_PREVIEW,
	  0, 0, NULL },

	GNOMEUIINFO_SEPARATOR,

	{ GNOME_APP_UI_ITEM,
	  N_("Schematic _Properties..."),
	  N_("Modify the schematic's properties"),
	  properties_cmd, NULL, NULL,
	  GNOME_APP_PIXMAP_STOCK, GTK_STOCK_PROPERTIES,
	  0, 0, NULL },

	GNOMEUIINFO_SEPARATOR,

	GNOMEUIINFO_MENU_CLOSE_ITEM(close_cmd, NULL),
	GNOMEUIINFO_MENU_EXIT_ITEM(quit_cmd, NULL),
	GNOMEUIINFO_END
};

/*
 * Edit menu.
 */
static GnomeUIInfo schematic_menu_edit [] = {

/*	GNOMEUIINFO_MENU_UNDO_ITEM(NULL, NULL),
	GNOMEUIINFO_MENU_REDO_ITEM(NULL, NULL),

	GNOMEUIINFO_SEPARATOR,*/

	GNOMEUIINFO_MENU_CUT_ITEM(cut_cmd, NULL),
	GNOMEUIINFO_MENU_COPY_ITEM(copy_cmd, NULL),
	GNOMEUIINFO_MENU_PASTE_ITEM(paste_cmd, NULL),

	GNOMEUIINFO_SEPARATOR,

	{ GNOME_APP_UI_ITEM,
	  N_("_Delete"),
	  N_("Delete the selection"),
	  delete_cmd, NULL, NULL,
	  GNOME_APP_PIXMAP_STOCK, GTK_STOCK_DELETE,
	  'd', GDK_CONTROL_MASK, NULL },

	{ GNOME_APP_UI_ITEM,
	  N_("_Rotate"),
	  N_("Rotate the selection clockwise"),
	  rotate_cmd, NULL, NULL,
	  GNOME_APP_PIXMAP_STOCK, STOCK_PIXMAP_ROTATE,
	  'r', GDK_CONTROL_MASK, NULL },

	{ GNOME_APP_UI_ITEM,
	  N_("Flip _horizontally"),
	  N_("Flip the selection horizontally"),
	  flip_horizontal_cmd, NULL, NULL,
	  GNOME_APP_PIXMAP_STOCK, STOCK_PIXMAP_ROTATE,
	  'f', GDK_CONTROL_MASK, NULL },

	{ GNOME_APP_UI_ITEM,
	  N_("Flip _vertically"),
	  N_("Flip the selection vertically"),
	  flip_vertical_cmd, NULL, NULL,
	  GNOME_APP_PIXMAP_STOCK, STOCK_PIXMAP_ROTATE,
	  'f', GDK_CONTROL_MASK|GDK_SHIFT_MASK, NULL },
	GNOMEUIINFO_SEPARATOR,

	{ GNOME_APP_UI_ITEM,
	  N_("Select _all"),
	  N_("Select all objects on the sheet"),
	  select_all_cmd, NULL, NULL,
	  GNOME_APP_PIXMAP_NONE, NULL,
	  'a', GDK_CONTROL_MASK, NULL },

	{ GNOME_APP_UI_ITEM,
	  N_("Deselect a_ll"),
	  N_("Deselect the selected objects"),
	  deselect_all_cmd, NULL, NULL,
	  GNOME_APP_PIXMAP_NONE, NULL,
	  'a', GDK_CONTROL_MASK | GDK_SHIFT_MASK, NULL },

	GNOMEUIINFO_SEPARATOR,

	{ GNOME_APP_UI_ITEM,
	  N_("_Object Properties..."),
	  N_("Modify the object's properties"),
	  object_properties_cmd, NULL, NULL,
	  GNOME_APP_PIXMAP_STOCK, GTK_STOCK_PROPERTIES,
	  'e', GDK_CONTROL_MASK, NULL },

	{ GNOME_APP_UI_ITEM, N_("Simulation S_ettings..."),
	  N_("Edit the simulation settings"), sim_settings_show, NULL,
	  GTK_STOCK_PROPERTIES, 0, 0, 0, 0 },

	GNOMEUIINFO_MENU_PREFERENCES_ITEM (settings_show, NULL),

	GNOMEUIINFO_END
};

/*
 * Tools.
 */
static GnomeUIInfo schematic_menu_tools [] = {

	{ GNOME_APP_UI_ITEM,
	  N_("_Simulate"),
	  N_("Run a simulation"),
	  simulate_cmd, NULL, NULL,
	  GNOME_APP_PIXMAP_STOCK, GTK_STOCK_EXECUTE,
	  GDK_F11, 0, NULL },

	GNOMEUIINFO_SEPARATOR,

	{ GNOME_APP_UI_ITEM, N_("_Generate netlist"),
	  N_("Generate a netlist"), netlist_cmd, NULL,
	  NULL, 0, 0, 0, 0 },

	GNOMEUIINFO_END
};

static GnomeUIInfo schematic_menu_zoom [] = {
	GNOMEUIINFO_ITEM_NONE(N_("50%"),
		N_("Set the zoom factor to 50%"), zoom_50_cmd),
	GNOMEUIINFO_ITEM_NONE(N_("75%"),
		N_("Set the zoom factor to 75%"), zoom_75_cmd),
	{ GNOME_APP_UI_ITEM, N_("100%"),
	  N_("Set the zoom factor to 100%"), zoom_100_cmd, NULL, NULL, 0, 0,
	  '1', 0 },
	GNOMEUIINFO_ITEM_NONE(N_("125%"),
		N_("Set the zoom factor to 125%"), zoom_125_cmd),
	GNOMEUIINFO_ITEM_NONE(N_("150%"),
		N_("Set the zoom factor to 150%"), zoom_150_cmd),
//	GNOMEUIINFO_ITEM_NONE(N_("Custom..."), N_("Set the zoom factor to a custom value"), zoom_custom_cmd),
	GNOMEUIINFO_END
};

/*
 * View menu.
 */
static GnomeUIInfo schematic_menu_view [] = {
	{ GNOME_APP_UI_SUBTREE, N_("Zoom"), NULL, schematic_menu_zoom, NULL,
	  NULL, GNOME_APP_PIXMAP_DATA, menu_zoom_xpm, 0, (GdkModifierType) 0,
	  NULL },

	GNOMEUIINFO_SEPARATOR,

	GNOMEUIINFO_ITEM (N_("_Log"), N_("View the latest simulation log"),
		log_cmd, log_xpm),

	GNOMEUIINFO_TOGGLEITEM (N_("_Node labels"), N_("Show or hide node labels"), show_label_cmd, NULL),

	{ GNOME_APP_UI_ITEM, N_("N_etlist"),
	  N_("View the circuit netlist"), netlist_view_cmd, NULL,
	  NULL, 0, 0, 0, 0 },
/*
	GNOMEUIINFO_ITEM (N_("_Log"), N_("View the latest simulation log"),
		log_cmd, log_xpm),

	GNOMEUIINFO_ITEM (N_("_Plot"), N_("View the latest plot"), NULL,
		plot_xpm),

	{ GNOME_APP_UI_ITEM, N_("_Netlist"),
	  N_("View the latest simulated netlist"), NULL, NULL,
	  NULL, 0, 0, 0, 0 },
*/
	GNOMEUIINFO_END
};

void show_help (GtkWidget *widget, SchematicView *sv)
{
	GError *error = NULL;

	if (!gnome_help_display_uri("ghelp:oregano",&error)) {
		printf("Error %s\n", error->message);
		g_error_free(error);
	}
}

/*
 * Help menu.
 */
static GnomeUIInfo schematic_menu_help [] = {
	//GNOMEUIINFO_HELP ("oregano"),
	GNOMEUIINFO_ITEM_STOCK(
		N_("_Contents"), N_("Show program help"), show_help,
		GTK_STOCK_HELP),
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_MENU_ABOUT_ITEM(about_cmd, NULL),
	GNOMEUIINFO_END
};

/*
 * Main menu structure.
 */
static GnomeUIInfo schematic_menu [] = {
	GNOMEUIINFO_MENU_FILE_TREE(schematic_menu_file),
	GNOMEUIINFO_MENU_EDIT_TREE(schematic_menu_edit),
	GNOMEUIINFO_MENU_VIEW_TREE(schematic_menu_view),
	GNOMEUIINFO_SUBTREE(N_("_Tools"), schematic_menu_tools),
	GNOMEUIINFO_MENU_HELP_TREE(schematic_menu_help),
	GNOMEUIINFO_END
};

/*
 *  Toolbars.
 */
static GnomeUIInfo schematic_standard_toolbar [] = {
	GNOMEUIINFO_ITEM_STOCK (
		N_("New"), N_("Create a new schematic"),
		new_cmd, GTK_STOCK_NEW),
	GNOMEUIINFO_ITEM_STOCK (
		N_("Open"), N_("Open an existing schematic"),
		open_cmd, GTK_STOCK_OPEN),
	GNOMEUIINFO_ITEM_STOCK (
		N_("Save"), N_("Save the schematic"),
		save_cmd, GTK_STOCK_SAVE),
/*
	GNOMEUIINFO_SEPARATOR,

	GNOMEUIINFO_ITEM_STOCK (
		N_("Undo"), N_("Undo"),
		NULL, GTK_STOCK_UNDO),
	GNOMEUIINFO_ITEM_STOCK (
		N_("Redo"), N_("Redo"),
		NULL, GTK_STOCK_REDO),*/

	GNOMEUIINFO_SEPARATOR,

	GNOMEUIINFO_ITEM_STOCK (
		N_("Cut"), N_("Cut the selection to the clipboard"),
		cut_cmd, GTK_STOCK_CUT),
	GNOMEUIINFO_ITEM_STOCK (
		N_("Copy"), N_("Copy the selection to the clipboard"),
		copy_cmd, GTK_STOCK_COPY),
	GNOMEUIINFO_ITEM_STOCK (
		N_("Paste"), N_("Paste the clipboard"),
		paste_cmd, GTK_STOCK_PASTE),

	GNOMEUIINFO_END
};


static GnomeUIInfo tools_group [] = {

	{ GNOME_APP_UI_ITEM,
	  N_("Arrow"),
	  N_("Select, move and modify objects"),
	  arrow_cmd, NULL, NULL,
	  GNOME_APP_PIXMAP_STOCK, STOCK_PIXMAP_ARROW },

	  GNOMEUIINFO_ITEM_STOCK (
		N_("Text"),
		N_("Put text on the schematic"),
		text_cmd,
		GTK_STOCK_BOLD),

	{ GNOME_APP_UI_ITEM,
	  N_("Wires"),
	  N_("Draw wires"),
	  wire_cmd, NULL, NULL,
	  GNOME_APP_PIXMAP_STOCK, STOCK_PIXMAP_WIRE },

	GNOMEUIINFO_END
};

static GnomeUIInfo schematic_tools_toolbar [] = {

	GNOMEUIINFO_RADIOLIST(tools_group),

	GNOMEUIINFO_END
};

static GnomeUIInfo schematic_simulation_toolbar [] = {
	GNOMEUIINFO_ITEM_STOCK (
		N_("Add Voltage Clamp"),
		N_("Add a Voltage test clamp"),
		v_clamp_cmd,
		STOCK_PIXMAP_V_CLAMP),

	GNOMEUIINFO_ITEM_STOCK (
		N_("Simulate"),
		N_("Run a simulation for the current schematic"),
		simulate_cmd,
		GTK_STOCK_EXECUTE),

	GNOMEUIINFO_ITEM_STOCK (
		N_("Simulation settings"),
		N_("Simulation settings"),
		sim_settings_show,
		GTK_STOCK_PROPERTIES),

	/* By now, this can't be done 
	GNOMEUIINFO_ITEM_STOCK (
		N_("View plot"),
		N_("View plot "),
		plot_whow,
		STOCK_PIXMAP_PLOT),
	*/
	GNOMEUIINFO_END
};

static GnomeUIInfo schematic_view_toolbar [] = {

	{ GNOME_APP_UI_TOGGLEITEM,
		N_("Parts"),
		N_("Show or hide the part browser"),
		part_browser_cmd, NULL, NULL,
		GNOME_APP_PIXMAP_STOCK, STOCK_PIXMAP_PART_BROWSER },
	 
	{ GNOME_APP_UI_TOGGLEITEM,
	  	N_("Grid"),
		N_("Turn on/off the grid"),
		grid_toggle_snap_cmd, NULL, NULL,
		GNOME_APP_PIXMAP_STOCK, STOCK_PIXMAP_GRID },
	
	GNOMEUIINFO_ITEM_STOCK (
		N_("Zoom in"),
		N_("Zoom in"),
		zoom_in_cmd,
		GTK_STOCK_ZOOM_IN),

	GNOMEUIINFO_ITEM_STOCK (
		N_("Zoom out"),
		N_("Zoom out"),
		zoom_out_cmd,
		GTK_STOCK_ZOOM_OUT),

	GNOMEUIINFO_SEPARATOR,
	
	/*
	 *	FIXME - Unimplemeted functions 
	 */
	
	/*
	{ GNOME_APP_UI_TOGGLEITEM,
	  N_("Voltmeters"),
	  N_("Enable/disable voltmeters"),
	  voltmeter_cmd, NULL, NULL,
	  GNOME_APP_PIXMAP_STOCK, STOCK_PIXMAP_VOLTMETER },
	*/

	GNOMEUIINFO_END
};

/*
 * Right click context menu for the sheet.
 */
static GnomeUIInfo sheet_context_menu [] = {
	{ GNOME_APP_UI_ITEM, N_("Paste"),
	  N_("Paste the contents of the clipboard to the sheet"), paste_cmd,
	  NULL, NULL, GNOME_APP_PIXMAP_STOCK, GTK_STOCK_PASTE, 0, 0 },

	GNOMEUIINFO_END
};

#endif
