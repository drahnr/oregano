/*
 * schematic-view-menu.h
 *
 *
 * Authors:
 *  Richard Hult <rhult@hem.passagen.se>
 *  Ricardo Markiewicz <rmarkie@fi.uba.ar>
 *  Andres de Barbara <adebarbara@fi.uba.ar>
 *  Marc Lorber <lorber.marc@wanadoo.fr>
 *  Guido Trentalancia <guido@trentalancia.com>
 *
 * Web page: https://ahoi.io/project/oregano
 *
 * Copyright (C) 1999-2001  Richard Hult
 * Copyright (C) 2003,2006  Ricardo Markiewicz
 * Copyright (C) 2009-2012  Marc Lorber
 * Copyright (C) 2017       Guido Trentalancia
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
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef _SCHEMATIC_VIEW_MENU_
#define _SCHEMATIC_VIEW_MENU_

#include "sim-settings-gui.h"

// TODO: Create only two entries instead of four for stretching the schematic horizontally
//       or vertically (needs proper icons not provided by Gtk).
static GtkActionEntry entries[] = {
    // Name, ICON, Text, CTRL, DESC, CALLBACK
    {"MenuFile", NULL, N_ ("_File")},
    {"MenuEdit", NULL, N_ ("_Edit")},
    {"MenuTools", NULL, N_ ("_Tools")},
    {"MenuView", NULL, N_ ("_View")},
    {"MenuHelp", NULL, N_ ("_Help")},
    {"MenuZoom", NULL, N_ ("_Zoom")},
    {"New", GTK_STOCK_NEW, N_ ("_New"), "<control>N", N_ ("Create a new schematic"),
     G_CALLBACK (new_cmd)},
    {"Open", GTK_STOCK_OPEN, N_ ("_Open"), "<control>O", N_ ("Open a schematic"),
     G_CALLBACK (open_cmd)},
    {"DisplayRecentFiles", NULL, N_ ("_Recent Files"), NULL, NULL, NULL},
    {"Save", GTK_STOCK_SAVE, N_ ("_Save"), "<control>S", N_ ("Save a schematic"),
     G_CALLBACK (save_cmd)},
    {"SaveAs", GTK_STOCK_SAVE_AS, N_ ("Save _As..."), "<control><shift>S",
     N_ ("Save a schematic with other name"), G_CALLBACK (save_as_cmd)},
    {"PrintProperties", NULL, N_ ("Print Properties"), NULL, N_ ("Set print properties"),
     G_CALLBACK (page_properties_cmd)},
    {"Print", GTK_STOCK_PRINT, N_ ("_Print"), NULL, N_ ("Print schematic"), G_CALLBACK (print_cmd)},
    {"PrintPreview", GTK_STOCK_PRINT_PREVIEW, N_ ("Print Preview"), NULL,
     N_ ("Preview the schematic before printing"), G_CALLBACK (print_preview_cmd)},
    {"SchematicProperties", NULL, N_ ("Schematic Pr_operties..."), NULL,
     N_ ("Modify the schematic's properties"), G_CALLBACK (properties_cmd)},
    {"Export", NULL, N_ ("_Export..."), NULL, N_ ("Export schematic"), G_CALLBACK (export_cmd)},
    {"Close", GTK_STOCK_CLOSE, N_ ("_Close"), "<control>W", N_ ("Close the current schematic"),
     G_CALLBACK (close_cmd)},
    {"Quit", GTK_STOCK_QUIT, N_ ("_Quit"), "<control>Q", N_ ("Close all schematics"),
     G_CALLBACK (quit_cmd)},
    {"Cut", GTK_STOCK_CUT, N_ ("C_ut"), "<control>X", NULL, G_CALLBACK (cut_cmd)},
    {"Copy", GTK_STOCK_COPY, N_ ("_Copy"), "<control>C", NULL, G_CALLBACK (copy_cmd)},
    {"Paste", GTK_STOCK_PASTE, N_ ("_Paste"), "<control>V", NULL, G_CALLBACK (paste_cmd)},
    {"Delete", GTK_STOCK_DELETE, N_ ("_Delete"), "<control>D", N_ ("Delete the selection"),
     G_CALLBACK (delete_cmd)},
    {"Rotate", STOCK_PIXMAP_ROTATE, N_ ("_Rotate"), "<control>R",
     N_ ("Rotate the selection clockwise"), G_CALLBACK (rotate_cmd)},
    {"FlipH", NULL, N_ ("Flip _horizontally"), "<control>F", N_ ("Flip the selection horizontally"),
     G_CALLBACK (flip_horizontal_cmd)},
    {"FlipV", NULL, N_ ("Flip _vertically"), "<control><shift>F",
     N_ ("Flip the selection vertically"), G_CALLBACK (flip_vertical_cmd)},
    {"SelectAll", NULL, N_ ("Select _all"), "<control>A", N_ ("Select all objects on the sheet"),
     G_CALLBACK (select_all_cmd)},
    {"SelectNone", NULL, N_ ("Select _none"), "<control><shift>A",
     N_ ("Deselect the selected objects"), G_CALLBACK (deselect_all_cmd)},
    {"ObjectProperties", GTK_STOCK_PROPERTIES, N_ ("_Object Properties..."), NULL,
     N_ ("Modify the object's properties"), G_CALLBACK (object_properties_cmd)},
    {"SimulationSettings", GTK_STOCK_PROPERTIES, N_ ("Simulation S_ettings..."), NULL,
     N_ ("Edit the simulation settings"), G_CALLBACK (sim_settings_show)},
    {"Settings", NULL, N_ ("_Preferences"), NULL, N_ ("Edit Oregano settings"),
     G_CALLBACK (settings_show)},
    {"Simulate", GTK_STOCK_EXECUTE, N_ ("_Simulate"), "F5", N_ ("Run a simulation"),
     G_CALLBACK (schematic_view_simulate_cmd)},
    {"Netlist", NULL, N_ ("_Generate netlist"), NULL, N_ ("Generate a netlist"),
     G_CALLBACK (netlist_cmd)},
    {"SmartSearch", NULL, N_ ("Smart Search"), NULL, N_ ("Search a part within all the librarys"),
     G_CALLBACK (smartsearch_cmd)},
    {"Log", NULL, N_ ("_Log"), NULL, N_ ("View the latest simulation log"), G_CALLBACK (log_cmd)},
    {"NetlistView", NULL, N_ ("N_etlist"), NULL, N_ ("View the circuit netlist"),
     G_CALLBACK (netlist_view_cmd)},
    {"About", GTK_STOCK_HELP, N_ ("_About"), NULL, N_ ("About Oregano"), G_CALLBACK (about_cmd)},
    {"UserManual", NULL, N_ ("User's Manual"), NULL, N_ ("Oregano User's Manual"),
     G_CALLBACK (show_help)},
    {"ZoomIn", GTK_STOCK_ZOOM_IN, N_ ("Zoom _In"), NULL, N_ ("Zoom in"), G_CALLBACK (zoom_in_cmd)},
    {"ZoomOut", GTK_STOCK_ZOOM_OUT, N_ ("Zoom _Out"), NULL, N_ ("Zoom out"),
     G_CALLBACK (zoom_out_cmd)},
    {"StretchLeft", GTK_STOCK_GO_BACK, N_ ("Stretch to the left"), NULL, N_ ("Stretch to the left"),
     G_CALLBACK (stretch_horizontal_cmd)},
    {"StretchRight", GTK_STOCK_GO_FORWARD, N_ ("Stretch to the right"), NULL, N_ ("Stretch to the right"),
     G_CALLBACK (stretch_horizontal_cmd)},
    {"StretchTop", GTK_STOCK_GO_UP, N_ ("Stretch the top"), NULL, N_ ("Stretch the top"),
     G_CALLBACK (stretch_vertical_cmd)},
    {"StretchBottom", GTK_STOCK_GO_DOWN, N_ ("Stretch the bottom"), NULL, N_ ("Stretch the bottom"),
     G_CALLBACK (stretch_vertical_cmd)},
};

static GtkToggleActionEntry toggle_entries[] = {
    {"Labels", NULL, N_ ("_Node labels"), NULL, N_ ("Toggle node label visibility"),
     G_CALLBACK (show_label_cmd), FALSE},
    {"Parts", STOCK_PIXMAP_PART_BROWSER, N_ ("_Parts"), NULL, N_ ("Toggle part browser visibility"),
     G_CALLBACK (part_browser_cmd), TRUE},
    {"Grid", STOCK_PIXMAP_GRID, N_ ("_Grid"), NULL, N_ ("Toggle grid visibility"),
     G_CALLBACK (grid_toggle_snap_cmd), TRUE},
    {"LogView", GTK_STOCK_DIALOG_WARNING, N_ ("LogView"), NULL, N_ ("Toggle log view visibility"),
     G_CALLBACK (log_toggle_visibility_cmd), TRUE},
};

static GtkRadioActionEntry zoom_entries[] = {
    {"Zoom50", NULL, "50%", NULL, N_ ("Set the zoom to 50%"), 0},
    {"Zoom75", NULL, "75%", NULL, N_ ("Set the zoom to 75%"), 1},
    {"Zoom100", NULL, "100%", "1", N_ ("Set the zoom to 100%"), 2},
    {"Zoom125", NULL, "125%", NULL, N_ ("Set the zoom to 125%"), 3},
    {"Zoom150", NULL, "150%", NULL, N_ ("Set the zoom to 150%"), 4},
};

static GtkRadioActionEntry tools_entries[] = {
    {"Arrow", STOCK_PIXMAP_ARROW, N_ ("Arrow"), NULL, N_ ("Select, move and modify objects"), 0},
    {"Text", GTK_STOCK_BOLD, N_ ("Text"), NULL, N_ ("Put text on the schematic"), 1},
    {"Wire", STOCK_PIXMAP_WIRE, N_ ("Wire"), "1", N_ ("Draw wires"), 2},
    {"VClamp", STOCK_PIXMAP_V_CLAMP, N_ ("Clamp"), NULL, N_ ("Add voltage clamp"), 3},
};

static const char *ui_description = "<ui>"
                                    "  <menubar name='MainMenu'>"
                                    "    <menu action='MenuFile'>"
                                    "      <menuitem action='New'/>"
                                    "      <menuitem action='Open'/>"
                                    "      <menuitem action='DisplayRecentFiles'/>"
                                    "      <menuitem action='Save'/>"
                                    "      <menuitem action='SaveAs'/>"
                                    "      <separator/>"
                                    "      <menuitem action='PrintProperties'/>"
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
                                    "      <separator/>"
                                    "	   <menuitem action='SmartSearch'/>"
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
                                    "      <menuitem action='UserManual'/>"
                                    "      <menuitem action='About'/>"
                                    "    </menu>"
                                    "  </menubar>"
                                    "  <toolbar name='StandardToolbar'>"
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
                                    "    <toolitem action='StretchLeft'/>"
                                    "    <toolitem action='StretchRight'/>"
                                    "    <toolitem action='StretchTop'/>"
                                    "    <toolitem action='StretchBottom'/>"
				    "    <separator/>"
                                    "    <toolitem action='Grid'/>"
                                    "    <toolitem action='Parts'/>"
                                    "    <toolitem action='LogView'/>"
                                    "  </toolbar>"
                                    "  <popup name='MainPopup'>"
                                    "    <menuitem action='Paste'/>"
                                    "  </popup>"
                                    "</ui>";

#endif
