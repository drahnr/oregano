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

#ifndef _SCHEMATIC_VIEW_MENU_
#define _SCHEMATIC_VIEW_MENU_
static GtkActionEntry entries[] = {
	/* Name, ICON, Text, CTRL, DESC, CALLBACK */
	{"MenuFile", NULL, "_File"},
	{"MenuEdit", NULL, "_Edit"},
	{"MenuTools", NULL, "_Tools"},
	{"MenuView", NULL, "_View"},
	{"MenuHelp", NULL, "_Help"},
	{"MenuZoom", NULL, "_Zoom"},
	{"New", GTK_STOCK_NEW, "_New", "<control>N", "Create a new schematic", G_CALLBACK (new_cmd)},
	{"Open", GTK_STOCK_OPEN, "_Open", "<control>O", "Open a schematic", G_CALLBACK (open_cmd)},
	{"Save", GTK_STOCK_SAVE, "_Save", "<control>S", "Save a schematic", G_CALLBACK (save_cmd)},
	{"SaveAs", GTK_STOCK_SAVE_AS, "Save _As ...", "<control><shift>S", "Save a schematic with other name", G_CALLBACK (save_as_cmd)},
	{"PageProperties", NULL, "Page Properties", NULL, "Set print properties", G_CALLBACK (page_properties_cmd)},
	{"Print", GTK_STOCK_PRINT, "_Print", NULL, "Print schematic", G_CALLBACK (print_cmd)},
	{"PrintPreview", GTK_STOCK_PRINT_PREVIEW, "Print Preview", NULL, "Preview the schematic before printing", G_CALLBACK (print_preview_cmd)},
	{"SchematicProperties", NULL, "Schematic Pr_operties...", NULL, "Modify the schematic's properties", G_CALLBACK (properties_cmd)},
	{"Export", NULL, "_Export ...", NULL, "Export schematic", G_CALLBACK (export_cmd)},
	{"Close", GTK_STOCK_CLOSE, "_Close", "<control>W", "Close the current schematic", G_CALLBACK (close_cmd)},
	{"Quit", GTK_STOCK_QUIT, "_Quit", "<control>Q", "Close all schematic", G_CALLBACK (quit_cmd)},
 	{"Cut", GTK_STOCK_CUT, "C_ut", "<control>X", NULL, G_CALLBACK (cut_cmd)},
 	{"Copy", GTK_STOCK_COPY, "_Copy", "<control>C", NULL, G_CALLBACK (copy_cmd)},
 	{"Paste", GTK_STOCK_PASTE, "_Paste", "<control>V", NULL, G_CALLBACK (paste_cmd)},
	{"Delete", GTK_STOCK_DELETE, "_Delete", "<control>D", "Delete the selection", G_CALLBACK (delete_cmd)},
	{"Rotate", STOCK_PIXMAP_ROTATE, "_Rotate", "<control>R", "Rotate the selection clockwise", G_CALLBACK (rotate_cmd)},
	{"FlipH", NULL, "Flip _horizontally", "<control>F", "Flip the selection horizontally", G_CALLBACK (flip_horizontal_cmd)},
	{"FlipV", NULL, "Flip _vertically", "<control><shift>F", "Flip the selection vertically", G_CALLBACK (flip_vertical_cmd)},
	{"SelectAll", NULL, "Select _all", "<control>A", "Select all objects on the sheet", G_CALLBACK (select_all_cmd)},
	{"SelectNone", NULL, "Select _none", "<control><shift>A", "Deselect the selected objects", G_CALLBACK (deselect_all_cmd)},
	{"ObjectProperties", GTK_STOCK_PROPERTIES, "_Object Properties...", NULL, "Modify the object's properties", G_CALLBACK (object_properties_cmd)},
	{"SimulationSettings", GTK_STOCK_PROPERTIES, "Simulation S_ettings...", NULL, "Edit the simulation settings", G_CALLBACK (sim_settings_show)},
	{"Settings", NULL, "_Preferences", NULL, "Edit Oregano settings", G_CALLBACK (settings_show)},
	{"Simulate", GTK_STOCK_EXECUTE, "_Simulate", "F5", "Run a simulation", G_CALLBACK (simulate_cmd)},
	{"Netlist", NULL, "_Generate netlist", NULL, "Generate a netlist", G_CALLBACK (netlist_cmd)},
	{"Log", NULL, "_Log", NULL, "View the latest simulation log", G_CALLBACK (log_cmd)},
	{"NetlistView", NULL, "N_etlist", NULL, "View the circuit netlist", G_CALLBACK (netlist_view_cmd)},
	{"About", GTK_STOCK_HELP, "_About", NULL, "About Oregano", G_CALLBACK (about_cmd)},
	{"ZoomIn", GTK_STOCK_ZOOM_IN, "Zoom _In", NULL, "Zoom in", G_CALLBACK (zoom_in_cmd)},
	{"ZoomOut", GTK_STOCK_ZOOM_OUT, "Zoom _Out", NULL, "Zoom out", G_CALLBACK (zoom_out_cmd)},
};

static GtkToggleActionEntry toggle_entries[] = {
	{"Labels", NULL, "_Node labels", NULL, "Show or hide node labels", G_CALLBACK (show_label_cmd), FALSE},
	{"Parts", STOCK_PIXMAP_PART_BROWSER, "_Parts", NULL, "Show or hide the part browser", G_CALLBACK (part_browser_cmd), TRUE},
	{"Grid", STOCK_PIXMAP_GRID, "_Grid", NULL, "Show or hide the grid", G_CALLBACK (grid_toggle_snap_cmd), TRUE},
};

static GtkRadioActionEntry zoom_entries[] = {
	{"Zoom50", NULL, "50%", NULL, "Set the zoom factor to 50%", 0},
	{"Zoom75", NULL, "75%", NULL, "Set the zoom factor to 75%", 1},
	{"Zoom100", NULL, "100%", "1", "Set the zoom factor to 100%", 2},
	{"Zoom125", NULL, "125%", NULL, "Set the zoom factor to 125%", 3},
	{"Zoom150", NULL, "150%", NULL, "Set the zoom factor to 150%", 4},
};

static GtkRadioActionEntry tools_entries[] = {
	{"Arrow", STOCK_PIXMAP_ARROW, "Arrow", NULL, "Select, move and modify objects", 0},
	{"Text", GTK_STOCK_BOLD, "Text", NULL, "Put text on the schematic", 1},
	{"Wire", STOCK_PIXMAP_WIRE, "Wire", "1", "Draw wires%", 2},
	{"VClamp", STOCK_PIXMAP_V_CLAMP, "Clamp", NULL, "Addd voltage clamp", 3},
};

static const char *ui_description =
"<ui>"
"  <menubar name='MainMenu'>"
"    <menu action='MenuFile'>"
"      <menuitem action='New'/>"
"      <menuitem action='Open'/>"
"      <menuitem action='Save'/>"
"      <menuitem action='SaveAs'/>"
"      <separator/>"
"      <menuitem action='PageProperties'/>"
"      <menuitem action='Print'/>"
"      <menuitem action='PrintPreview'/>"
"      <separator/>"
"      <menuitem action='SchematicProperties'/>"
"      <menuitem action='Export'/>"
"      <separator/>"
"      <menuitem action='Close'/>"
"      <menuitem action='Quit'/>"
"    </menu>"
"    <menu action='MenuEdit'>"
"      <menuitem action='Cut'/>"
"      <menuitem action='Copy'/>"
"      <menuitem action='Paste'/>"
"      <separator/>"
"      <menuitem action='Delete'/>"
"      <menuitem action='Rotate'/>"
"      <menuitem action='FlipH'/>"
"      <menuitem action='FlipV'/>"
"      <separator/>"
"      <menuitem action='SelectAll'/>"
"      <menuitem action='SelectNone'/>"
"      <separator/>"
"      <menuitem action='ObjectProperties'/>"
"      <menuitem action='SimulationSettings'/>"
"      <separator/>"
"      <menuitem action='Settings'/>"
"    </menu>"
"    <menu action='MenuTools'>"
"      <menuitem action='Simulate'/>"
"      <separator/>"
"      <menuitem action='Netlist'/>"
"    </menu>"
"    <menu action='MenuView'>"
"      <menu action='MenuZoom'>"
"        <menuitem action='Zoom50'/>"
"        <menuitem action='Zoom75'/>"
"        <menuitem action='Zoom100'/>"
"        <menuitem action='Zoom125'/>"
"        <menuitem action='Zoom150'/>"
"      </menu>"
"      <separator/>"
"      <menuitem action='Log'/>"
"      <menuitem action='Labels'/>"
"      <menuitem action='NetlistView'/>"
"    </menu>"
"    <menu action='MenuHelp'>"
"      <menuitem action='About'/>"
"    </menu>"
"  </menubar>"
"  <toolbar name='StandartToolbar'>"
"    <toolitem action='New'/>"
"    <toolitem action='Open'/>"
"    <toolitem action='Save'/>"
"    <separator/>"
"    <toolitem action='Cut'/>"
"    <toolitem action='Copy'/>"
"    <toolitem action='Paste'/>"
"    <separator/>"
"    <toolitem action='Arrow'/>"
"    <toolitem action='Text'/>"
"    <toolitem action='Wire'/>"
"    <toolitem action='VClamp'/>"
"    <separator/>"
"    <toolitem action='Simulate'/>"
"    <toolitem action='SimulationSettings'/>"
"    <separator/>"
"    <toolitem action='ZoomIn'/>"
"    <toolitem action='ZoomOut'/>"
"    <separator/>"
"    <toolitem action='Grid'/>"
"    <toolitem action='Parts'/>"
"  </toolbar>"
"  <popup name='MainPopup'>"
"    <menuitem action='Paste'/>"
"  </popup>"
"</ui>";

#endif

