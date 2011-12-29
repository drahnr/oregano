/*
 * plot.c
 *
 *
 * Authors:
 *  Richard Hult <rhult@hem.passagen.se>
 *	Ricardo Markiewicz <rmarkie@fi.uba.ar>
 *	Andres de Barbara <adebarbara@fi.uba.ar>
 *
 * Web page: http://arrakis.lug.fi.uba.ar/
 *
 * Copyright (C) 1999-2001	Richard Hult
 * Copyright (C) 2003,2006	Ricardo Markiewicz
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <gtk/gtk.h>
#include <glade/glade.h>
#include <math.h>
#include <string.h>

#include "file.h"
#include "main.h"
#include "schematic.h"
#include "dialogs.h"
#include "plot.h"
#include "smallicon.h"

#include "gplot.h"
#include "gplotfunction.h"
#include "gplotlines.h"
#include "plot-add-function.h"

#define PLOT_PADDING_X 50
#define PLOT_PADDING_Y 40
#define PLOT_WIDTH 400
#define PLOT_HEIGHT 300
#define DEBUG_PLOT 0

#define GET_Y_POINT(y_val,y_min,factor) \
	(PLOT_HEIGHT-((y_val)-(y_min))*(factor))
#define GET_Y_VALUE(y_point,y_min,factor) \
	((y_min)+(PLOT_HEIGHT-(y_point))/(factor))
#define GET_X_POINT(x_val,x_min,factor) (((x_val)-(x_min))*(factor))
#define GET_X_VALUE(x_point,x_min,factor) ((x_min)+(x_point)/(factor))

const gchar *TITLE_FONT	   = N_("Sans 10");
const gchar *AXIS_FONT	   = N_("Sans 10");
const gchar *UPSCRIPT_FONT = N_("Sans 8");

#define PLOT_AXIS_COLOR "black"
#define PLOT_CURVE_COLOR "medium sea green"

static guint next_color = 0;
static guint n_curve_colors = 7;
static char *plot_curve_colors[] = {
	"white",
	"green",
	"red",
	"yellow",
	"cyan",
	"pink",
	"orange1",
	0
};

/* Hack! */
#ifndef G_FUNC
#define G_FUNC(x) (x)
#endif

typedef struct {
	GtkWidget *window;
	GtkWidget *canvas;
	GtkWidget *coord;  /* shows the coordinates of the mose */
	GtkWidget *combobox;

	GtkWidget *plot;

	gboolean show_cursor;

	OreganoEngine *sim;
	SimulationData *current;

	gboolean logx;
	gboolean logy;

	gchar *title;
	gchar *xtitle;
	gchar *ytitle;

	gint width;  /* width of the plot, excluding */
	gint height; /* axes, titles and padding etc */

	gdouble plot_min, plot_max;
	gdouble x_min,x_max;

	gdouble padding_x; /* padding around the plot. Note that */
	gdouble padding_y; /* titles, axes etc live here */

	gint selected; /* the currently selected plot in the clist */
	gint prev_selected;
} Plot;

typedef enum {
	X_AXIS,
	Y_AXIS
} AxisType;

static GtkWidget *plot_window_create (Plot *plot);
static void destroy_window (GtkWidget *widget, Plot *plot);
static void zoom_100 (GtkWidget *widget, Plot *plot);
static void zoom_region (GtkWidget *widget, Plot *plot);
static void zoom_pan (GtkWidget *widget, Plot *plot);
static gint delete_event_cb (GtkWidget *widget, GdkEvent *event, Plot *plot);
static void plot_canvas_movement(GtkWidget *, GdkEventMotion *, Plot *);

static void plot_toggle_cross (GtkWidget *widget, Plot *plot);
static void plot_export (GtkWidget *widget, Plot *plot);
static void add_function (GtkWidget *widget, Plot *plot);


static GnomeUIInfo plot_file_menu[] =
{
	/*GNOMEUIINFO_MENU_PRINT_ITEM(NULL,NULL),
	GNOMEUIINFO_MENU_PRINT_SETUP_ITEM(NULL,NULL),
	GNOMEUIINFO_ITEM_NONE(N_("Print Preview"),
		N_("Preview the plot before printing"), NULL),
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_ITEM_NONE(N_("Export plot"),
		N_("Show the export menu"), plot_export),*/
	GNOMEUIINFO_ITEM_NONE(N_("Add Function"),
		N_("Add new function to the graph"), add_function),
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_MENU_CLOSE_ITEM(destroy_window,NULL),
	GNOMEUIINFO_END
};

static GnomeUIInfo plot_zoom_menu [] = {
//	GNOMEUIINFO_ITEM_NONE(N_("50%"),
//		N_("Set the zoom factor to 50%"), plot_zoom_50_cmd),
//	GNOMEUIINFO_ITEM_NONE(N_("75%"),
//		N_("Set the zoom factor to 75%"), plot_zoom_75_cmd),
	{ GNOME_APP_UI_ITEM, N_("100%"),
	  N_("Set the zoom factor to 100%"), NULL, NULL, NULL, 0,
	  0, '2', 0 },
//	GNOMEUIINFO_ITEM_NONE(N_("125%"),
//		N_("Set the zoom factor to 125%"), plot_zoom_125_cmd),
//	GNOMEUIINFO_ITEM_NONE(N_("150%"),
//		N_("Set the zoom factor to 150%"), plot_zoom_150_cmd),
//	GNOMEUIINFO_ITEM_NONE(N_("200%"),
//		N_("Set the zoom factor to 200%"), plot_zoom_200_cmd),
	GNOMEUIINFO_END
};

static GnomeUIInfo plot_plot_menu [] = {
	{ GNOME_APP_UI_ITEM, N_("_Preferences..."), NULL, NULL, NULL, NULL,
	  GNOME_APP_PIXMAP_STOCK, GTK_STOCK_PREFERENCES, 0, 0 },
	GNOMEUIINFO_TOGGLEITEM (N_("Show crosshairs"), "show or hide crosshairs", plot_toggle_cross, NULL),
	GNOMEUIINFO_SUBTREE(N_("_Zoom"), plot_zoom_menu),
	GNOMEUIINFO_END
};


static GnomeUIInfo plot_help_menu[] =
{
	GNOMEUIINFO_HELP(N_("Schematic Plot")),
/*	GNOMEUIINFO_MENU_ABOUT_ITEM(NULL, NULL), */
	GNOMEUIINFO_END
};

GnomeUIInfo plot_main_menu[] = {
	GNOMEUIINFO_SUBTREE(N_("_File"), plot_file_menu),
	/*GNOMEUIINFO_SUBTREE(N_("_Plot"), plot_plot_menu),*/
	/*GNOMEUIINFO_MENU_HELP_TREE (plot_help_menu),*/
	GNOMEUIINFO_END
};

static gchar *
get_variable_units (gchar *str)
{
gchar *tmp;

	if (str == NULL)
		return g_strdup ("##");

	if (!strcmp (str, "voltage")) {
		tmp = g_strdup ("Volt");
	} else if (!strcmp (str, "db") ) {
		tmp = g_strdup ("db");
	} else if (!strcmp (str, "time") ) {
		tmp = g_strdup ("s");
	} else if (!strcmp (str, "frequency") ) {
		tmp = g_strdup ("Hz");
	} else if (!strcmp (str, "current") ) {
		tmp = g_strdup ("A");
	} else {
		tmp = g_strdup ("##");
	}
	return tmp;
}

static gint
delete_event_cb (GtkWidget *widget, GdkEvent *event, Plot *plot)
{
	plot->window = NULL;
	g_object_unref (plot->sim);
	if (plot->ytitle) g_free(plot->ytitle);
	g_free(plot);
	plot = NULL;
	return FALSE; /* yes, please destroy me */
}

/* Call this to close the plot window */
static void
destroy_window(GtkWidget *widget, Plot *plot)
{
	/* TODO - chequear si no hay que destruir el combo box */
	gtk_widget_destroy (plot->window);
	g_object_unref (plot->sim);
	plot->window = NULL;
	if (plot->title) g_free (plot->title);
	g_free (plot);
	plot = NULL;
}

static void
on_plot_selected (GtkCellRendererToggle *cell_renderer, gchar *path, Plot *plot)
{
	GPlotFunction *f;
	GtkTreeIter iter;
	GtkTreeModel *model;
	GtkTreeView *treeview;
	gboolean activo;

	treeview = GTK_TREE_VIEW(gtk_object_get_data(GTK_OBJECT(plot->window), "clist"));

	model = gtk_tree_view_get_model (treeview);
	if (!gtk_tree_model_get_iter_from_string (model , &iter, path))
		return;

	gtk_tree_model_get (model, &iter, 0, &activo, 4, &f, -1);
	activo = !activo;
	gtk_tree_store_set (GTK_TREE_STORE (model), &iter, 0, activo, -1);

	g_object_set (G_OBJECT (f), "visible", activo, NULL);

	gtk_widget_queue_draw (plot->plot);
}

static GPlotFunction*
create_plot_function_from_simulation_data (guint i, SimulationData *current)
{
	GPlotFunction *f;
	double *X;
	double *Y;
	double data;
	guint len, j;

	len = current->data[i]->len;
	X = g_new0 (double, len);
	Y = g_new0 (double, len);
	
	for (j = 0; j < len; j++) {
		Y[j] = g_array_index (current->data[i], double, j);
	
		data = g_array_index (current->data[0], double, j);
		
		/*if (plot->logx)
			data = log10 (data);
		*/
		X[j] = data;
	}

	f = g_plot_lines_new (X, Y, len);
	g_object_set (G_OBJECT (f), "color", plot_curve_colors[(next_color++)%n_curve_colors], NULL);

	return f;
}

static void
analysis_selected (GtkEditable *editable, Plot *plot)
{
	int i;
	const gchar *ca;
	GtkWidget *entry;
	GtkTreeView *list;
	GtkTreeModel *model;
	GtkTreeIter parent_nodes, parent_functions;
	GList *analysis;
	SimulationData *sdat;

	list = GTK_TREE_VIEW (gtk_object_get_data (GTK_OBJECT (plot->window), "clist"));
	entry = GTK_COMBO (plot->combobox)->entry;

	ca = gtk_entry_get_text (GTK_ENTRY (entry));

	plot->current = NULL;
	analysis = oregano_engine_get_results (plot->sim);
	for (; analysis; analysis = analysis->next) {
		sdat = SIM_DATA (analysis->data);
		if (!strcmp (ca, oregano_engine_get_analysis_name (sdat))) {
			plot->current = sdat;
			break;
		}
	}

	if (plot->current == NULL) {
		/* Simulation failed? */
		g_warning (_("The simulation produced no data!!\n"));
		return;
	}

	g_free (plot->xtitle);
	plot->xtitle = get_variable_units (plot->current->var_units[0]);
	g_free (plot->ytitle);
	plot->ytitle = get_variable_units (plot->current->var_units[1]);

	g_free (plot->title);
	plot->title = g_strdup_printf (_("Plot - %s"),
		oregano_engine_get_analysis_name (plot->current));

	/*  Set the variable names in the list  */
	model = gtk_tree_view_get_model (GTK_TREE_VIEW (list));
	gtk_tree_store_clear (GTK_TREE_STORE (model));

	/* Create root nodes */
	gtk_tree_store_append (GTK_TREE_STORE (model), &parent_nodes, NULL);
	gtk_tree_store_set (GTK_TREE_STORE (model), &parent_nodes, 0, FALSE, 1, _("Nodes"), 2, FALSE, 3, "white", -1);

	gtk_tree_store_append (GTK_TREE_STORE (model), &parent_functions, NULL);
	gtk_tree_store_set (GTK_TREE_STORE (model), &parent_functions, 0, NULL, 1, _("Functions"), 2, FALSE, 3, "white", -1);

	g_plot_set_axis_labels (GPLOT (plot->plot), plot->xtitle, plot->ytitle);
	g_plot_clear (GPLOT (plot->plot));

	for (i = 1; i < plot->current->n_variables; i++) {
		GtkTreeIter iter;
		GPlotFunction *f;

		if (plot->current->type != DC_TRANSFER) {
			if (strchr(plot->current->var_names[i], '#') == NULL) {
				gchar *color;

				f = create_plot_function_from_simulation_data (i, plot->current);
				g_object_set (G_OBJECT (f), "visible", FALSE, NULL);
				g_object_get (G_OBJECT (f), "color", &color, NULL);

				g_plot_add_function (GPLOT (plot->plot), f);
				gtk_tree_store_append (GTK_TREE_STORE (model), &iter, &parent_nodes);
				gtk_tree_store_set (GTK_TREE_STORE (model),
					&iter,
				 	0, FALSE, 
					1, plot->current->var_names[i],
				 	2, TRUE,
					3, color,
					4, f,
					-1);
			}
		} else {
			gchar *color;

			f = create_plot_function_from_simulation_data (i, plot->current);
			g_object_set (G_OBJECT (f), "visible", FALSE, NULL);
			g_object_get (G_OBJECT (f), "color", &color, NULL);

			g_plot_add_function (GPLOT (plot->plot), f);
			gtk_tree_store_append (GTK_TREE_STORE (model), &iter, &parent_nodes);
			gtk_tree_store_set (GTK_TREE_STORE (model), &iter, 
				0, FALSE, 
				1, plot->current->var_names[i],
			 	2, TRUE,
				3, color,
				4, f,
			 	-1);
		}
	}

	gtk_widget_queue_draw (plot->plot);
}

static void
plot_canvas_movement (GtkWidget *w, GdkEventMotion *event, Plot *plot)
{
	gchar *coord;
	gdouble x,y;

	x = event->x;
	y = event->y;

	g_plot_window_to_device (GPLOT (plot->plot), &x, &y);

	coord = g_strdup_printf ("(%g, %g)", x, y);

	gtk_entry_set_text (GTK_ENTRY (plot->coord), coord);

	g_free (coord);
}

static GtkWidget *
plot_window_create (Plot *plot)
{
	GtkTreeView *list;
	GtkTreeStore *tree_model;
	GtkCellRenderer *cell;
	GtkTreeViewColumn *column;

	GtkWidget *outer_table, *window, *button, *plot_scrolled;
	GladeXML *gui;
	gchar *msg;

	window = gnome_app_new("plot", _("Oregano - Plot"));

	gnome_app_create_menus_with_data (GNOME_APP (window), plot_main_menu, plot);

	g_signal_connect (G_OBJECT (window), "delete_event",
		G_CALLBACK (delete_event_cb), plot);

	if (!g_file_test (OREGANO_GLADEDIR "/plot-window.glade2",
		    G_FILE_TEST_EXISTS)) {
		msg = g_strdup_printf (
			_("The file %s could not be found. You might need to reinstall Oregano to fix this."),
			OREGANO_GLADEDIR "/plot-window.glade2");
		oregano_error_with_title (_("Could not create plot window."), msg);
		g_free (msg);
		return NULL;
	}

	gui = glade_xml_new (OREGANO_GLADEDIR "/plot-window.glade2", NULL, NULL);
	if (!gui) {
		oregano_error (_("Could not create plot window."));
		return NULL;
	}

	outer_table = glade_xml_get_widget (gui, "plot_table");
	plot_scrolled = plot->canvas = glade_xml_get_widget (gui, "plot_scrolled");
	plot->coord  = glade_xml_get_widget(gui, "pos_label");

	plot->plot = g_plot_new ();

	gtk_widget_set_size_request (plot->plot, 600, 400);
	gtk_container_add (GTK_CONTAINER (plot_scrolled), plot->plot);

	g_signal_connect(G_OBJECT (plot->plot),
		"motion_notify_event",
		G_CALLBACK(plot_canvas_movement), plot
	);

	g_object_ref(outer_table);
	gtk_widget_unparent(outer_table);
	gnome_app_set_contents(GNOME_APP(window), outer_table);
	/* XXX Sin desreferenciar el objeto anda sin problemas.
	 * * Se liberara la menoria de todos modos ?
	 * * gnome_app_set_contents referencia al objeto que se le pasa?
	 */
	//g_object_unref(outer_table);

	button = glade_xml_get_widget (gui, "close_button");
	g_signal_connect(G_OBJECT(button), "clicked",
		G_CALLBACK(destroy_window), plot);

	button = glade_xml_get_widget (gui, "zoom_panning");
	g_signal_connect(G_OBJECT(button), "clicked",
		G_CALLBACK(zoom_pan), plot);

	button = glade_xml_get_widget (gui, "zoom_region");
	g_signal_connect(G_OBJECT(button), "clicked",
		G_CALLBACK(zoom_region), plot);

	button = glade_xml_get_widget (gui, "zoom_normal");
	g_signal_connect(G_OBJECT(button), "clicked",
		G_CALLBACK(zoom_100), plot);

	list = GTK_TREE_VIEW(glade_xml_get_widget (gui, "variable_list"));
	tree_model = gtk_tree_store_new (5, G_TYPE_BOOLEAN, G_TYPE_STRING, G_TYPE_BOOLEAN, G_TYPE_STRING, G_TYPE_POINTER);

	/* One Column with 2 CellRenderer. First the Toggle and next a Text */
	column = gtk_tree_view_column_new ();

	cell = gtk_cell_renderer_toggle_new ();
	g_signal_connect (G_OBJECT (cell), "toggled", G_CALLBACK (on_plot_selected), plot);
	gtk_tree_view_column_pack_start (column, cell, FALSE);
	gtk_tree_view_column_set_attributes (column, cell, "active", 0, "visible", 2, NULL);

	cell = gtk_cell_renderer_text_new();
	gtk_tree_view_column_pack_start (column, cell, TRUE);
	gtk_tree_view_column_set_attributes (column, cell, "text", 1, NULL);

	cell = gtk_cell_renderer_text_new();
	gtk_tree_view_column_pack_start (column, cell, FALSE);
	gtk_cell_renderer_set_fixed_size (cell, 20, 20);
	gtk_tree_view_column_set_attributes (column, cell, "background", 3, NULL);

	gtk_tree_view_append_column(list, column);
	gtk_tree_view_set_model(list, GTK_TREE_MODEL(tree_model));

	gtk_object_set_data(GTK_OBJECT(window), "clist", list);

	gtk_widget_realize(GTK_WIDGET(window));
	
	//gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (plot_plot_menu[1].widget), TRUE);

	plot->combobox = glade_xml_get_widget (gui, "analysis_combo");
	plot->window = window;
	gtk_widget_show_all(window);

	return window;
}

int
plot_show (OreganoEngine *engine)
{
	GtkEntry *entry;
	GtkTreeView *list;
	GList *analysis = NULL;
	GList *combo_items = NULL;
	Plot *plot;
	gchar *s_current = NULL;
	SimulationData *first = NULL;

	g_return_val_if_fail (engine != NULL, FALSE);

	plot = g_new0(Plot, 1);

	g_object_ref (engine);
	plot->sim = engine;
	plot->window = plot_window_create(plot);

	plot->logx = FALSE;
	plot->logy = FALSE;

	plot->padding_x = PLOT_PADDING_X;
	plot->padding_y = PLOT_PADDING_Y;

	plot->show_cursor = TRUE;

	next_color = 0;

	/*  Get the analysis we have */
	analysis = oregano_engine_get_results (engine);
	for (; analysis ; analysis = analysis->next) {
		SimulationData *sdat = SIM_DATA (analysis->data);
		gchar *str = oregano_engine_get_analysis_name (sdat);
		if (sdat->type == OP_POINT) {
			continue;
		}
		if (s_current == NULL) {
			s_current = str;
			first = sdat;
		}
		combo_items = g_list_append (combo_items, str);
	}

	gtk_combo_set_popdown_strings (GTK_COMBO (plot->combobox), combo_items);

	entry = GTK_ENTRY (GTK_COMBO (plot->combobox)->entry);
	g_signal_connect (G_OBJECT (entry), "changed",
		G_CALLBACK (analysis_selected), plot);

	gtk_entry_set_text (GTK_ENTRY (entry), s_current ? s_current : "?");

	list = GTK_TREE_VIEW (gtk_object_get_data (GTK_OBJECT (plot->window), "clist"));

	plot->title = g_strdup_printf (_("Plot - %s"), s_current);
	plot->xtitle = get_variable_units (first ? first->var_units[0] : "##");
	plot->ytitle = get_variable_units (first ? first->var_units[1] : "!!");

	g_free (s_current);

	analysis_selected (GTK_EDITABLE (entry), plot);

	return TRUE;
}

static void
plot_toggle_cross (GtkWidget *widget, Plot *plot)
{
	plot->show_cursor = gtk_check_menu_item_get_active (GTK_CHECK_MENU_ITEM (widget));
}

static GPlotFunction*
create_plot_function_from_data (SimulationFunction *func, SimulationData *current)
{
	GPlotFunction *f;
	double *X;
	double *Y;
	double data;
	guint j, len;

	len = current->data[func->first]->len;
	X = g_new0 (double, len);
	Y = g_new0 (double, len);

	for (j = 0; j < len; j++) {
		Y[j] = g_array_index (current->data[func->first], double, j);
	
		data = g_array_index (current->data[func->second], double, j);
		switch (func->type) {
			case FUNCTION_MINUS:
				Y[j] -= data;
			break;
			case FUNCTION_TRANSFER:
				if (data < 0.000001f) {
					if (Y[j] < 0)
						Y[j] = -G_MAXDOUBLE;
					else
						Y[j] = G_MAXDOUBLE;
				} else {
					Y[j] /= data;
				}
		}
		
		X[j] = g_array_index (current->data[0], double, j);
	}

	f = g_plot_lines_new (X, Y, len);
	g_object_set (G_OBJECT (f), "color", plot_curve_colors[(next_color++)%n_curve_colors], NULL);

	return f;
}

static void
add_function (GtkWidget *widget, Plot *plot)
{
	GtkTreeView *tree;
	GtkTreeModel *model;
	GtkTreeIter iter;
	GtkTreePath *path;
	GList *lst;
	gchar *color;

	plot_add_function_show (plot->sim, plot->current);

	tree = GTK_TREE_VIEW(gtk_object_get_data(GTK_OBJECT (plot->window), "clist"));
	model = gtk_tree_view_get_model (tree);

	path = gtk_tree_path_new_from_string ("1");
	gtk_tree_model_get_iter (model, &iter, path);
	gtk_tree_path_free (path);
	
	gtk_tree_store_remove (GTK_TREE_STORE (model), &iter);

	gtk_tree_store_append(GTK_TREE_STORE(model), &iter, NULL);
	gtk_tree_store_set(GTK_TREE_STORE(model), &iter, 0, FALSE, 1, _("Functions"), 2, FALSE, 3, "white", -1);

	lst = plot->current->functions;
	while (lst)	{
		GPlotFunction *f;
		GtkTreeIter child;
		int i;
		gchar *str, *name1, *name2;
		SimulationFunction *func = (SimulationFunction *)lst->data;
	
		for (i = 1; i < plot->current->n_variables; i++) {
			if (func->first == i) {
				name1 = g_strdup (plot->current->var_names[i]);
			}
			if (func->second == i) {
				name2 = g_strdup (plot->current->var_names[i]);
			}
		}
		switch (func->type) {
			case FUNCTION_MINUS:
				str = g_strdup_printf ("%s - %s", name1, name2);
			break;
			case FUNCTION_TRANSFER:
				str = g_strdup_printf ("%s(%s, %s)", _("TRANSFER"), name1, name2);
		}

		f = create_plot_function_from_data (func, plot->current);
		g_object_get (G_OBJECT (f), "color", &color, NULL);

		g_plot_add_function (GPLOT (plot->plot), f);

		gtk_tree_store_append(GTK_TREE_STORE(model), &child, &iter);
		gtk_tree_store_set(GTK_TREE_STORE(model), &child, 0, TRUE, 1, str, 2, TRUE, 3, color, 4, f, -1);

		lst = lst->next;
	}


	gtk_widget_queue_draw (plot->plot);
}

static void
plot_export (GtkWidget *widget, Plot *plot)
{
	gchar *fn = NULL;
	gint w, h, format;
	GladeXML *gui;
	gchar *msg;
	GtkWidget *window;
	GtkRadioButton *export_png, *export_ps;
	GtkSpinButton *width, *height;
	
	if (!g_file_test (OREGANO_GLADEDIR "/plot-export.glade2", G_FILE_TEST_EXISTS)) {
		msg = g_strdup_printf (
			_("The file %s could not be found. You might need to reinstall Oregano to fix this."),
			OREGANO_GLADEDIR "/plot-export.glade2");
		oregano_error_with_title (_("Could not create plot export window."), msg);
		g_free (msg);
		return;
	}

	gui = glade_xml_new (OREGANO_GLADEDIR "/plot-export.glade2", NULL, NULL);
	if (!gui) {
		oregano_error (_("Could not create plot export window."));
		return;
	}

	window = glade_xml_get_widget (gui, "plot_export");
	export_png = GTK_RADIO_BUTTON (glade_xml_get_widget (gui, "radio_png"));
	export_ps = GTK_RADIO_BUTTON (glade_xml_get_widget (gui, "radio_ps"));
	width = GTK_SPIN_BUTTON (glade_xml_get_widget (gui, "export_width"));
	height = GTK_SPIN_BUTTON (glade_xml_get_widget (gui, "export_height"));

	if ((gtk_dialog_run (GTK_DIALOG (window)) != GTK_RESPONSE_ACCEPT)) {
		gtk_widget_destroy (window);
		return;
	}

	format = 0;
	/*if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (export_png)))
		format = GTK_CAIRO_PLOT_VIEW_SAVE_PNG;
	else
		format = GTK_CAIRO_PLOT_VIEW_SAVE_PS;*/

	fn = dialog_file_open (_("Save PNG"));
	if (fn == NULL) {
		gtk_widget_destroy (window);
		return;
	}

	w = gtk_spin_button_get_value_as_int (width);
	h = gtk_spin_button_get_value_as_int (height);

	gtk_widget_destroy (window);
}

static void
zoom_100 (GtkWidget *widget, Plot *plot)
{
	g_plot_reset_zoom (GPLOT (plot->plot));
}

static void
zoom_region (GtkWidget *widget, Plot *plot)
{
	g_plot_set_zoom_mode (GPLOT (plot->plot), GPLOT_ZOOM_REGION);
}

static void
zoom_pan (GtkWidget *widget, Plot *plot)
{
	g_plot_set_zoom_mode (GPLOT (plot->plot), GPLOT_ZOOM_INOUT);
}

