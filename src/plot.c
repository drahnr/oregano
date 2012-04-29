/*
 * plot.c
 *
 *
 * Authors:
 *  Richard Hult <rhult@hem.passagen.se>
 *	Ricardo Markiewicz <rmarkie@fi.uba.ar>
 *	Andres de Barbara <adebarbara@fi.uba.ar>
 *  Marc Lorber <lorber.marc@wanadoo.fr>
 *
 * Web page: https://github.com/marc-lorber/oregano
 *
 * Copyright (C) 1999-2001	Richard Hult
 * Copyright (C) 2003,2006	Ricardo Markiewicz
 * Copyright (C) 2009-2012	Marc Lorber
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

#include <math.h>
#include <string.h>
#include <glib/gi18n.h>

#include "file.h"
#include "oregano.h"
#include "schematic.h"
#include "dialogs.h"
#include "plot.h"

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

#define PLOT_AXIS_COLOR "black"
#define PLOT_CURVE_COLOR "medium sea green"
#define NG_DEBUG(s) if (0) g_print ("%s\n", s)

static guint next_color = 0;
static guint next_pulse = 0;
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

typedef struct {
	GtkWidget 		*window;
	GtkWidget 		*canvas;
	GtkWidget 		*coord;  // shows the coordinates of the mouse
	GtkWidget 		*combo_box;

	GtkWidget 		*plot;

	gboolean 		 show_cursor;

	OreganoEngine 	*sim;
	SimulationData 	*current;

	gboolean 		 logx;
	gboolean 		 logy;

	gchar 			*title;
	gchar 			*xtitle;
	gchar 			*ytitle;

	gint 			 width;  // width of the plot, excluding 
	gint 			 height; // axes, titles and padding etc 

	gdouble 		 plot_min;
	gdouble			 plot_max;
	gdouble 		 x_min;
	gdouble			 x_max;

	gdouble 		 padding_x; // padding around the plot. Note that 
	gdouble 		 padding_y; // titles, axes etc live here 

	gint 			 selected; // the currently selected plot in the clist 
	gint 			 prev_selected;
} Plot;

static GtkWidget *plot_window_create (Plot *plot);
static void destroy_window (GtkWidget *widget, Plot *plot);
static gint delete_event_cb (GtkWidget *widget, GdkEvent *event, Plot *plot);
static void plot_canvas_movement (GtkWidget *, GdkEventMotion *, Plot *);
static void add_function (GtkMenuItem *menuitem, Plot *plot);
static void close_window (GtkMenuItem *menuitem, Plot *plot);

static gchar *
get_variable_units (gchar *str)
{
gchar *tmp;

	if (str == NULL)
		return g_strdup ("##");

	if (!strcmp (str, _("voltage"))) {
		tmp = g_strdup ("V");
	} 
	else if (!strcmp (str, "db") ) {
		tmp = g_strdup ("db");
	} 
	else if (!strcmp (str, _("time")) ) {
		tmp = g_strdup ("s");
	} 
	else if (!strcmp (str, _("frequency")) ) {
		tmp = g_strdup ("Hz");
	} 
	else if (!strcmp (str, _("current")) ) {
		tmp = g_strdup ("A");
	} 
	else {
		tmp = g_strdup ("##");
	}
	return tmp;
}

static gint
delete_event_cb (GtkWidget *widget, GdkEvent *event, Plot *plot)
{
	plot->window = NULL;
	g_object_unref (plot->sim);
	if (plot->ytitle) g_free (plot->ytitle);
	g_free (plot);
	plot = NULL;
	return FALSE;
}

// Call this to close the plot window 
static void
destroy_window (GtkWidget *widget, Plot *plot)
{
	gtk_widget_destroy (plot->canvas);
	gtk_widget_destroy (plot->coord);
	gtk_widget_destroy (plot->combo_box);
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

	treeview = GTK_TREE_VIEW (g_object_get_data (G_OBJECT (plot->window), 
	    "clist"));

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
	gdouble width = 1;
	GraphicType graphic_type = FUNCTIONAL_CURVE;
	gdouble shift_step = 0;

	len = current->data[i]->len;
	X = g_new0 (double, len);
	Y = g_new0 (double, len);
	
	for (j = 0; j < len; j++) {
		Y[j] = g_array_index (current->data[i], double, j);
		data = g_array_index (current->data[0], double, j);
		X[j] = data;
	}
	if (current->type == FOURIER) {
		graphic_type = FREQUENCY_PULSE;
		next_pulse++;
		width = 5.0;
		shift_step = X[1] / 20;
		NG_DEBUG (g_strdup_printf ("shift_step = %lf\n", shift_step));
	}
	else {
		next_pulse = 0;
		width = 1.0;
	}

	f = g_plot_lines_new (X, Y, len);
	g_object_set (G_OBJECT (f), "color", 
	    plot_curve_colors[(next_color++)%n_curve_colors], NULL);
	g_object_set (G_OBJECT (f), "graph-type", 
	    graphic_type, NULL);
	g_object_set (G_OBJECT (f), "shift",
	    shift_step*next_pulse, NULL);
	g_object_set (G_OBJECT (f), "width",
	    width, NULL);
	NG_DEBUG (g_strdup_printf ("plot: create_plot_function_from_simulation_data: shift = %lf\n", 0.1*next_pulse));

	return f;
}

static void
analysis_selected (GtkWidget *combo_box, Plot *plot)
{
	int i;
	const gchar *ca;
	GtkTreeView *list;
	GtkTreeModel *model;
	GtkTreeIter parent_nodes, parent_functions;
	GList *analysis;
	SimulationData *sdat;

	list = GTK_TREE_VIEW (g_object_get_data (G_OBJECT (plot->window), "clist"));
	if (gtk_combo_box_get_active (GTK_COMBO_BOX (combo_box)) == -1) {
		// Simulation failed?
		g_warning (_("The simulation produced no data!!\n"));
		return;
	}
	ca = gtk_combo_box_text_get_active_text (GTK_COMBO_BOX_TEXT (combo_box));

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
		// Simulation failed?
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

	//  Set the variable names in the list
	model = gtk_tree_view_get_model (GTK_TREE_VIEW (list));
	gtk_tree_store_clear (GTK_TREE_STORE (model));

	// Create root nodes
	gtk_tree_store_append (GTK_TREE_STORE (model), &parent_nodes, NULL);
	gtk_tree_store_set (GTK_TREE_STORE (model), &parent_nodes, 0, FALSE, 1, 
	    _("Nodes"), 2, FALSE, 3, "white", -1);

	gtk_tree_store_append (GTK_TREE_STORE (model), &parent_functions, NULL);
	gtk_tree_store_set (GTK_TREE_STORE (model), &parent_functions, 0, NULL, 1, 
	    _("Functions"), 2, FALSE, 3, "white", -1);

	g_plot_set_axis_labels (GPLOT (plot->plot), plot->xtitle, plot->ytitle);
	g_plot_clear (GPLOT (plot->plot));

	for (i = 1; i < plot->current->n_variables; i++) {
		GtkTreeIter iter;
		GPlotFunction *f;
		if (plot->current->type != DC_TRANSFER) {
			if (strchr(plot->current->var_names[i], '#') == NULL) {
				gchar *color;

				f = create_plot_function_from_simulation_data (i, 
				    plot->current);
				g_object_set (G_OBJECT (f), "visible", FALSE, NULL);
				g_object_get (G_OBJECT (f), "color", &color, NULL);

				g_plot_add_function (GPLOT (plot->plot), f);
				gtk_tree_store_append (GTK_TREE_STORE (model), &iter, 
				    &parent_nodes);
				gtk_tree_store_set (GTK_TREE_STORE (model), &iter, 0, FALSE, 
					1, plot->current->var_names[i],
					2, TRUE,
					3, color,
					4, f,
					-1);
			}
		} 
		else {
			gchar *color;

			f = create_plot_function_from_simulation_data (i, plot->current);
			g_object_set (G_OBJECT (f), "visible", FALSE, NULL);
			g_object_get (G_OBJECT (f), "color", &color, NULL);

			g_plot_add_function (GPLOT (plot->plot), f);
			gtk_tree_store_append (GTK_TREE_STORE (model), &iter, 
			    &parent_nodes);
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
	GtkWidget *vbox, *menubar, *menu, *menuitem, *combo_box, *w;
	GtkBuilder *gui;
	GError *perror = NULL;
	gchar *msg;
  	GtkAccelGroup *accel_group;

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title (GTK_WINDOW (window), _("Oregano - Plot"));
  	gtk_container_set_border_width (GTK_CONTAINER (window), 0);
	g_signal_connect (G_OBJECT (window), "delete-event",
		G_CALLBACK (delete_event_cb), plot);
	accel_group = gtk_accel_group_new ();
  	gtk_window_add_accel_group (GTK_WINDOW (window), accel_group);

	if ((gui = gtk_builder_new ()) == NULL) {
		oregano_error (_("Could not create plot window."));
		return NULL;
	} 
	else gtk_builder_set_translation_domain (gui, NULL);

	if (!g_file_test (OREGANO_UIDIR "/plot-window.ui",G_FILE_TEST_EXISTS))
	{
		msg = g_strdup_printf (_("The file %s could not be found."
			    "You might need to reinstall Oregano to fix this."),
			OREGANO_UIDIR "/plot-window.ui");
		oregano_error_with_title (_("Could not create plot window."), msg);
		g_free (msg);
		return NULL;
	}

	if (gtk_builder_add_from_file (gui, OREGANO_UIDIR "/plot-window.ui", 
	    &perror) <= 0) {
		msg = perror->message;
		oregano_error_with_title (_("Could not create plot window."), msg);
		g_error_free (perror);
		return NULL;
	}

	outer_table = GTK_WIDGET (gtk_builder_get_object (gui, "plot_table"));
	plot_scrolled = plot->canvas = GTK_WIDGET (gtk_builder_get_object (gui, 
	    "plot_scrolled"));
	plot->coord  = GTK_WIDGET (gtk_builder_get_object (gui, "pos_label"));

	plot->plot = g_plot_new ();

	gtk_container_add (GTK_CONTAINER (plot_scrolled), plot->plot);
	gtk_widget_set_size_request (plot_scrolled, 600, 400);

	g_signal_connect (G_OBJECT (plot->plot), "motion_notify_event",
		G_CALLBACK (plot_canvas_movement), plot);

	// Creation of the menubar	
	vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
    menubar = gtk_menu_bar_new ();
	gtk_box_pack_start (GTK_BOX (vbox), menubar, FALSE, TRUE, 0);
    gtk_widget_show (menubar);

	// Creation of the menu
	menu = gtk_menu_new ();
	menuitem = gtk_menu_item_new_with_label (_("Add Function"));
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem); 
	g_signal_connect (menuitem, "activate", G_CALLBACK (add_function), 
	    plot);
	gtk_widget_show (menuitem);
	menuitem = gtk_separator_menu_item_new ();
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
	gtk_widget_show (menuitem);
	menuitem = gtk_menu_item_new_with_label (_("Close"));
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem); 
	g_signal_connect (menuitem, "activate", G_CALLBACK (close_window), 
	    plot);
	gtk_widget_show (menuitem);

	// Installation of the menu in the menubar
    menuitem = gtk_menu_item_new_with_label (_("File"));
    gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), menu);
    gtk_menu_shell_append (GTK_MENU_SHELL (menubar), menuitem);
    gtk_widget_show (menuitem);
	
	g_object_ref (outer_table);
	gtk_widget_unparent (outer_table);
	gtk_container_add (GTK_CONTAINER (vbox), outer_table);
	gtk_widget_show (vbox);
	
	g_object_ref (vbox);
	gtk_widget_unparent (vbox);
	gtk_container_add (GTK_CONTAINER (window), vbox);
	
	button = GTK_WIDGET (gtk_builder_get_object (gui, "close_button"));
	g_signal_connect (G_OBJECT (button), "clicked",
		G_CALLBACK (destroy_window), plot);

	list = GTK_TREE_VIEW (gtk_builder_get_object (gui, "variable_list"));
	tree_model = gtk_tree_store_new (5, G_TYPE_BOOLEAN, G_TYPE_STRING, 
	    G_TYPE_BOOLEAN, G_TYPE_STRING, G_TYPE_POINTER);

	// One Column with 2 CellRenderer. First the Toggle and next a Text
	column = gtk_tree_view_column_new ();

	cell = gtk_cell_renderer_toggle_new ();
	g_signal_connect (G_OBJECT (cell), "toggled", 
                      G_CALLBACK (on_plot_selected), plot);
	gtk_tree_view_column_pack_start (column, cell, FALSE);
	gtk_tree_view_column_set_attributes (column, cell, "active", 0, "visible", 
	    2, NULL);

	cell = gtk_cell_renderer_text_new ();
	gtk_tree_view_column_pack_start (column, cell, FALSE);
	gtk_tree_view_column_set_attributes (column, cell, "text", 1, NULL);

	cell = gtk_cell_renderer_text_new ();
	gtk_tree_view_column_pack_start (column, cell, FALSE);
	gtk_cell_renderer_set_fixed_size (cell, 20, 20);
	gtk_tree_view_column_set_attributes (column, cell, "background", 3, NULL);

	gtk_tree_view_append_column (list, column);
	gtk_tree_view_set_model (list, GTK_TREE_MODEL (tree_model));

	g_object_set_data (G_OBJECT (window), "clist", list);

	gtk_widget_realize (GTK_WIDGET (window));

	w = GTK_WIDGET (gtk_builder_get_object (gui, "analysis_combobox"));
	gtk_widget_destroy (w);

	w = GTK_WIDGET (gtk_builder_get_object (gui, "table1"));
	combo_box = gtk_combo_box_text_new_with_entry ();
	
	gtk_table_attach (GTK_TABLE (w),combo_box,0,1,0,1,
	                  GTK_SHRINK,
	                  //GTK_EXPAND | GTK_FILL, 
	                  GTK_SHRINK, 
	                  0, 0);

	plot->combo_box = combo_box;
	plot->window = window;
	gtk_widget_show_all (window);

	return window;
}

int
plot_show (OreganoEngine *engine)
{
	GList *analysis = NULL;
	GList *combo_items = NULL;
	Plot *plot;
	gchar *s_current = NULL;
	SimulationData *first = NULL;

	g_return_val_if_fail (engine != NULL, FALSE);

	plot = g_new0 (Plot, 1);

	g_object_ref (engine);
	plot->sim = engine;
	plot->window = plot_window_create (plot);

	plot->logx = FALSE;
	plot->logy = FALSE;

	plot->padding_x = PLOT_PADDING_X;
	plot->padding_y = PLOT_PADDING_Y;

	plot->show_cursor = TRUE;

	next_color = 0;

	//  Get the analysis we have
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
		gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (plot->combo_box), str);
		gtk_combo_box_set_active (GTK_COMBO_BOX (plot->combo_box),0);
	}
	
	g_signal_connect (G_OBJECT (plot->combo_box), "changed",
		G_CALLBACK (analysis_selected), plot);

	plot->title = g_strdup_printf (_("Plot - %s"), s_current);
	plot->xtitle = get_variable_units (first ? first->var_units[0] : "##");
	plot->ytitle = get_variable_units (first ? first->var_units[1] : "!!");

	g_free (s_current);

	analysis_selected ((plot->combo_box), plot);

	return TRUE;
}

static GPlotFunction*
create_plot_function_from_data (SimulationFunction *func, 
    SimulationData *current)
{
	GPlotFunction *f;
	double *X;
	double *Y;
	double data;
	guint j, len;
	GraphicType graphic_type = FUNCTIONAL_CURVE;
	gdouble width = 1.0;

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
				} 
				else {
					Y[j] /= data;
				}
		}
		
		X[j] = g_array_index (current->data[0], double, j);
	}
	if (current->type == FOURIER) {
		graphic_type = FREQUENCY_PULSE;
		next_pulse++;
		width = 5.0;
	}
	else {
		next_pulse = 0;
		width = 1.0;
	}

	f = g_plot_lines_new (X, Y, len);
	g_object_set (G_OBJECT (f), 
	              "color", plot_curve_colors[(next_color++)%n_curve_colors], 
	              "graph-type", graphic_type, 
	              "shift", 50.0*next_pulse, 
	              "width", width,
	              NULL);

	return f;
}

static void
close_window (GtkMenuItem *menuitem, Plot *plot)
{
	destroy_window (GTK_WIDGET (menuitem), plot);
}

static void
add_function (GtkMenuItem *menuitem, Plot *plot)
{
	GtkTreeView *tree;
	GtkTreeModel *model;
	GtkTreeIter iter;
	GtkTreePath *path;
	GList *lst;
	gchar *color;

	plot_add_function_show (plot->sim, plot->current);

	tree = GTK_TREE_VIEW (g_object_get_data(G_OBJECT (plot->window), "clist"));
	model = gtk_tree_view_get_model (tree);

	path = gtk_tree_path_new_from_string ("1");
	gtk_tree_model_get_iter (model, &iter, path);
	gtk_tree_path_free (path);
	
	gtk_tree_store_remove (GTK_TREE_STORE (model), &iter);

	gtk_tree_store_append(GTK_TREE_STORE (model), &iter, NULL);
	gtk_tree_store_set (GTK_TREE_STORE (model), &iter, 0, FALSE, 1, 
	    _("Functions"), 2, FALSE, 3, "white", -1);

	lst = plot->current->functions;
	while (lst)	{
		GPlotFunction *f;
		GtkTreeIter child;
		int i;
		gchar *str = NULL, *name1 = NULL, *name2 = NULL;
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
				str = g_strdup_printf ("%s(%s, %s)", _("TRANSFER"), name1, 
				    name2);
		}

		f = create_plot_function_from_data (func, plot->current);
		g_object_get (G_OBJECT (f), "color", &color, NULL);

		g_plot_add_function (GPLOT (plot->plot), f);

		gtk_tree_store_append (GTK_TREE_STORE (model), &child, &iter);
		gtk_tree_store_set (GTK_TREE_STORE (model), &child, 0, TRUE, 1, str, 2, 
		    TRUE, 3, color, 4, f, -1);

		lst = lst->next;
	}
	g_list_free_full (lst, g_object_unref);

	gtk_widget_queue_draw (plot->plot);
}
