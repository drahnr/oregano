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
 * Copyright (C) 2003,2005	LUGFI
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

#include <config.h>
#include <gtk/gtk.h>
#include <glade/glade.h>
#include <math.h>
#include <string.h>

#include "file.h"
#include "main.h"
#include "schematic.h"
#include "sim-engine.h"
#include "dialogs.h"
#include "plot.h"
#include "smallicon.h"

#include "gtkcairoplot.h"
#include "gtkcairoplotview.h"
#include "gtkcairoplotitems.h"
#include "gtkcairoplotmodel.h"

/*
 * Unused header
#include "pixmaps/mini_icon_plot.xpm"
*/
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

static int n_curve_colors = 5;
static char *plot_curve_colors[] = {
	"medium sea green",
	"royal blue",
	"firebrick",
	"gold",
	"turquoise",
	0
};

static gdouble x_min, x_max;

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
	GtkCairoPlotView *plot_view;
	GtkCairoPlotViewItem *plot_draw;
	GtkCairoPlotModel *plot_model;

	gboolean show_cursor;

	SimEngine *sim;
	SimulationData *current;

	gboolean logx;
	gboolean logy;

	gchar *title;
	gchar *xtitle;
	gchar *ytitle;

	gint width;  /* width of the plot, excluding */
	gint height; /* axes, titles and padding etc */

	gdouble x_factor,x_scale;
	gdouble y_factor,y_scale;

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
static void make_plot (Plot *plot, gint plot_number);
static void destroy_window (GtkWidget *widget, Plot *plot);
static void destroy_plot (Plot *plot);
static gint delete_event_cb (GtkWidget *widget, GdkEvent *event, Plot *plot);
static void getbins (gdouble, gdouble, gint, gdouble *, gdouble *, gint *,
	gdouble *);
static void plot_canvas_movement(GtkWidget *, GdkEventMotion *, Plot *);
static void plot_draw_axis (Plot *, AxisType, double, double);

static void plot_zoom_50_cmd (GtkWidget *widget, Plot *plot);
static void plot_zoom_75_cmd (GtkWidget *widget, Plot *plot);
static void plot_zoom_100_cmd (GtkWidget *widget, Plot *plot);
static void plot_zoom_125_cmd (GtkWidget *widget, Plot *plot);
static void plot_zoom_150_cmd (GtkWidget *widget, Plot *plot);
static void plot_zoom_200_cmd (GtkWidget *widget, Plot *plot);
static void plot_toggle_cross (GtkWidget *widget, Plot *plot);
static void plot_export (GtkWidget *widget, Plot *plot);


static GnomeUIInfo plot_file_menu[] =
{
	GNOMEUIINFO_MENU_PRINT_ITEM(NULL,NULL),
	GNOMEUIINFO_MENU_PRINT_SETUP_ITEM(NULL,NULL),
	GNOMEUIINFO_ITEM_NONE(N_("Print Preview"),
		N_("Preview the plot before printing"), NULL),
	GNOMEUIINFO_SEPARATOR,
	GNOMEUIINFO_ITEM_NONE(N_("Export plot"),
		N_("Show the export menu"), plot_export),
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
	  N_("Set the zoom factor to 100%"), plot_zoom_100_cmd, NULL, NULL, 0,
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
	GNOMEUIINFO_SUBTREE(N_("_Plot"), plot_plot_menu),
	GNOMEUIINFO_MENU_HELP_TREE (plot_help_menu),
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
	g_object_unref(plot->sim);
	plot->window = NULL;
	g_object_unref(plot->sim);
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
	g_object_unref(plot->sim);
	plot->window = NULL;
	if (plot->ytitle) g_free(plot->ytitle);
	g_free(plot);
	plot = NULL;
}

static void
show_plot (Plot *plot)
{
	if (plot) {
		destroy_plot (plot);
	}

	make_plot (plot, plot->selected);
}

static gboolean
plot_selected(GtkWidget *widget, GdkEventButton *event, Plot *plot)
{
	plot->selected = 0;
	if (plot->ytitle) {
		g_free (plot->ytitle);
	}

	plot->ytitle = get_variable_units(plot->current->var_units[plot->selected+1]);
	show_plot (plot);
	return TRUE;
}

static void
analysis_selected (GtkEditable *editable, Plot *plot)
{
	int i;
	const gchar *ca;
	GtkWidget *entry;
	GtkTreeView *list;
	GtkTreeModel *model;
	GtkTreeIter iter;
	GList *analysis;
	SimulationData *sdat;

	list = GTK_TREE_VIEW(gtk_object_get_data(GTK_OBJECT (plot->window), "clist"));
	entry = GTK_COMBO (plot->combobox)->entry;

	ca = gtk_entry_get_text( GTK_ENTRY (entry));
	g_object_set (G_OBJECT (plot->plot_model), "title", ca, NULL);
	plot->current = NULL;
	for (analysis = plot->sim->analysis; analysis; analysis = analysis->next) {
		sdat = SIM_DATA (analysis->data);
		if (!strcmp (ca, sim_engine_analysis_name (sdat))) {
			plot->current = sdat;
			break;
		}
	}

	if (plot->current == NULL) {
		/* Simulation failed? */
		//g_warning("No hay datos de la simulacion!!\n");
		g_warning (_("No data for the Simulation !!\n"));
		return;
	}

	g_free (plot->xtitle);
	plot->xtitle = get_variable_units (plot->current->var_units[0]);

	g_free (plot->title);
	plot->title = g_strdup_printf(_("Plot - %s"),
		sim_engine_analysis_name (plot->current));
	g_object_set (G_OBJECT (plot->plot_model),
		"title", plot->title,
		"x-unit", plot->xtitle,
		"y-unit", plot->ytitle,
		NULL);


	/*  Set the variable names in the list  */
	model = gtk_tree_view_get_model(GTK_TREE_VIEW(list));
	//model = gtk_tree_model_sort_get_model(GTK_TREE_MODEL_SORT(sort));
	gtk_list_store_clear(GTK_LIST_STORE(model));
	for (i = 1; i < plot->current->n_variables; i++) {
		if (plot->current->type != DC_TRANSFER) {
			if (strchr(plot->current->var_names[i], '#') == NULL) {
				gtk_list_store_append(GTK_LIST_STORE(model), &iter);
				gtk_list_store_set(GTK_LIST_STORE(model),
					&iter, 0, plot->current->var_names[i],
					-1);
			}
		} else {
			gtk_list_store_append(GTK_LIST_STORE(model), &iter);
			gtk_list_store_set(GTK_LIST_STORE(model), &iter, 0,
				plot->current->var_names[i], -1);
		}
	}
}

static void
destroy_plot (Plot *plot)
{
	// FIXME
	/* Aca va object_destroy porque el Canvas usa destroy!! */
	//if (plot->axis_group) gtk_object_destroy (GTK_OBJECT (plot->axis_group));
	//if (plot->plot_group) gtk_object_destroy (GTK_OBJECT (plot->plot_group));

	//plot->axis_group = NULL;
	//plot->plot_group = NULL;
	/* Was destroyed with group */
	//plot->x_cursor = NULL;
	//plot->y_cursor = NULL;
}

static void
plot_canvas_movement (GtkWidget *w, GdkEventMotion *event, Plot *plot)
{
	gchar *coord;
	gdouble x,y, x1, y1;
	
	x = event->x;
	y = event->y;

	gtk_cairo_plot_view_get_point (plot->plot_view, plot->plot_model, x, y, &x1, &y1);
	coord = g_strdup_printf ("(%g, %g)", x1, y1);

	gtk_entry_set_text (GTK_ENTRY (plot->coord), coord);

	g_free (coord);
}

static void
make_plot (Plot *plot, gint plot_number)
{
	GtkTreeView *clist;
	GtkTreeModel *model; //, *sort;

	GtkTreeSelection *selection;
	GList *sel, *l_iter;

	gchar *xtitle, *ytitle, *title;

	double plot_title_width, plot_title_height;
	double y_factor, y;
	double x_factor;
	double y_min, y_max;
	gint   i, j, k, npts, n_curve;
	double data, data1;
	char   *ysc=0,*xsc=0;
	gdouble sx,sy,ex,ey;
	gdouble xmin, xmax;
	gdouble ymin, ymax;

	clist = GTK_TREE_VIEW(gtk_object_get_data(GTK_OBJECT(plot->window), "clist"));
	y_min = 1.0e20;
	y_max =-1.0e20;
	y = 0.0;

	model = gtk_tree_view_get_model(GTK_TREE_VIEW(clist));

	selection = gtk_tree_view_get_selection(clist);
	sel = gtk_tree_selection_get_selected_rows(selection, &model);

	l_iter = sel;
	while (l_iter) {
		gint *id = gtk_tree_path_get_indices(l_iter->data);
		i = id[0] + 1;
		y = plot->current->min_data[i];
		if (y_min>y)
			y_min=y;
		y = plot->current->max_data[i];
		if (y_max < y)
			y_max = y;

		l_iter = l_iter->next;
	}

	plot->logx = FALSE;
	if (plot->current->type == AC) {
		if (!strcmp (sim_settings_get_ac_type (plot->sim->sim_settings), "DEC"))
			plot->logx = TRUE;
	}

	if (y_max == y_min) {
		plot->plot_max = (y_max==0.0 ? 1.0 : 1.1 * y_max);
		plot->plot_min = (y_max==0.0 ? -1.0 : 0.9 * y_max);
	} else {
		gdouble interval = (y_max - y_min);
		plot->plot_max = y_max + 0.1 * interval;
		plot->plot_min = y_min - 0.1 * interval;
	}

	if (fabs(plot->plot_max) > fabs(plot->plot_min)) {
		sx = plot->plot_max;
	} else {
		sx = plot->plot_min;
	}

/*	get_order_of_magnitude(sx, &sy, &ey);
	if (ey != 0.0) {
		sy = pow (10.0, ey);
		ysc = g_strdup_printf("%.1e", sy);
	} else {
		sy = 1.0;
	}
*/
	plot->y_scale = sy;

	x_min = plot->current->min_data[0];
	x_max = plot->current->max_data[0];

/*	get_order_of_magnitude(x_max, &sx, &ex);
	if (ex != 0.0) {
		sx = pow(10.0, ex);
		xsc = g_strdup_printf("%.1e", sx);
	} else
		sx = 1.0;
*/
	plot->x_scale = sx;
	if (plot->logx) {
		x_min = log10(x_min);
		x_max = log10(x_max);
		plot->x_scale = 1.0;
	}

	/*
	 * 2. Plot the curve, using the calculated scale factors.
	 */
	n_curve = 0;
	l_iter = sel;
	gtk_cairo_plot_model_clear (plot->plot_model);
	while (l_iter) {
		gint *id = gtk_tree_path_get_indices(l_iter->data);
		gdouble text_height;
		plot_number = id[0]+1;

		for (j = 0; j < plot->current->data[plot_number]->len; j++) {
			/* Loop while the step is positive in the X axis. */
			Point p;
			data = g_array_index (
				plot->current->data[plot_number],
				double, j);
			p.y = data;

			data = g_array_index (plot->current->data[0], double, j);
			/* TODO : que lo haga el plot view! */
			if (plot->logx)
				data = log10 (data);
			p.x = data;
			gtk_cairo_plot_model_add_point (plot->plot_model, plot->current->var_names[plot_number], p);
		}

		g_object_set (G_OBJECT (plot->plot_model), "logx", plot->logx, NULL);
		n_curve++;
		l_iter = l_iter->next;
	}

	gtk_cairo_plot_model_get_x_minmax (plot->plot_model, &xmin, &xmax);
	gtk_cairo_plot_model_get_y_minmax (plot->plot_model, &ymin, &ymax);
	gtk_cairo_plot_item_draw_set_scroll_region (
		GTK_CAIRO_PLOT_ITEM_DRAW (plot->plot_draw),
		xmin, xmax, ymin, ymax);
}

static GtkWidget *
plot_window_create (Plot *plot)
{
	GtkTreeView *list;
	GtkListStore *list_model;
	GtkCellRenderer *cell;
	GtkTreeViewColumn *column;
	GtkTreeSelection *selection;

	GtkWidget *outer_table, *window, *button, *plot_scrolled;
	GtkStyle *style;
	GdkColormap *colormap;
	/*
	 * Unused variables
	GdkPixmap *icon;
	GdkBitmap *icon_mask;
	*/
	GladeXML *gui;
	gchar *msg;
	GtkCairoPlotViewItem *plot_title;

	window = gnome_app_new("plot", _("Oregano - Plot"));

	gnome_app_create_menus_with_data (GNOME_APP (window), plot_main_menu, plot);

	g_signal_connect (G_OBJECT (window), "delete_event",
		G_CALLBACK (delete_event_cb), plot);

	if (!g_file_test (OREGANO_GLADEDIR "/plot-window.glade2",
		    G_FILE_TEST_EXISTS)) {
		msg = g_strdup_printf (
			_("<span weight=\"bold\" size=\"x-large\">Could not find the required file:\n</span>%s\n"),
			OREGANO_GLADEDIR "/plot-window.glade2");
		oregano_error (msg);
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

	/* Create the Plot view */
	plot->plot_model = gtk_cairo_plot_model_new ();
	g_object_set (G_OBJECT (plot->plot_model), "title", _("Plot"), NULL);

	plot->plot_view = gtk_cairo_plot_view_new ();
	plot->plot_draw = gtk_cairo_plot_view_item_new (GTK_CAIRO_PLOT_ITEM_DRAW_TYPE, 0, 0, 100, 100);
	plot_title = gtk_cairo_plot_view_item_new (GTK_CAIRO_PLOT_ITEM_TITLE_TYPE, 5, 5, 300, 25);
	
	gtk_cairo_plot_view_add_item (plot->plot_view, plot->plot_draw, ITEM_POS_FRONT);
	
	/* TODO : Is useful a title in plot area?, is not already in combobox??
	 * gtk_cairo_plot_view_add_item (plot->plot_view, plot_title, ITEM_POS_FRONT);
	 */

	gtk_cairo_plot_view_attach (plot->plot_view, plot->plot_draw);

	plot->plot = gtk_cairo_plot_new ();
	gtk_cairo_plot_set_model (GTK_CAIRO_PLOT (plot->plot), plot->plot_model);
	gtk_cairo_plot_set_view (GTK_CAIRO_PLOT (plot->plot), plot->plot_view);
	gtk_widget_set_size_request (plot->plot, 300, 300);
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

	list = GTK_TREE_VIEW(glade_xml_get_widget (gui, "variable_list"));
	list_model = gtk_list_store_new(1, G_TYPE_STRING);
	cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes("Variable", cell,
		"text", 0, NULL);

	gtk_tree_view_set_model(list, GTK_TREE_MODEL(list_model));
	gtk_tree_view_append_column(list, column);

	gtk_object_set_data(GTK_OBJECT(window), "clist", list);

	selection = gtk_tree_view_get_selection(list);
	gtk_tree_selection_set_mode(selection, GTK_SELECTION_EXTENDED);

	//gtk_widget_set_style (GTK_WIDGET (canvas), style);

	gtk_widget_realize(GTK_WIDGET(window));
//	icon = gdk_pixmap_create_from_xpm_d(window->window, &icon_mask, NULL, mini_icon_plot_xpm);
//	set_small_icon (window->window, icon, icon_mask);
	
	gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (plot_plot_menu[1].widget), TRUE);

	plot->combobox = glade_xml_get_widget (gui, "analysis_combo");
	plot->window = window;
	gtk_widget_show_all(window);

	return window;
}

int
plot_show (SimEngine *engine)
{
	GtkEntry *entry;
	GtkTreeView *list;
	/*
	 * Unused variable
	GtkTreeSelection *selection;
	*/
	GList *analysis = NULL;
	GList *combo_items = NULL;
	Plot *plot;
	gchar *s_current = NULL;
	SimulationData *first = NULL;

	g_return_val_if_fail (engine != NULL, FALSE);
	g_return_val_if_fail (IS_SIM_ENGINE (engine), FALSE);

	plot = g_new0(Plot, 1);

	g_object_ref(engine);
	plot->sim = engine;
	plot->window = plot_window_create(plot);

	plot->logx = FALSE;
	plot->logy = FALSE;

	plot->padding_x = PLOT_PADDING_X;
	plot->padding_y = PLOT_PADDING_Y;

	plot->show_cursor = TRUE;

	/*  Get the analysis we have */
	analysis = engine->analysis;
	for (; analysis ; analysis = analysis->next) {
		SimulationData *sdat = SIM_DATA (analysis->data);
		gchar *str = sim_engine_analysis_name (sdat);
		if (sdat->type == OP_POINT) {
			free(str);
			continue;
		}
		if (s_current == NULL) {
			s_current = str;
			first = sdat;
		}
		combo_items = g_list_append(combo_items, str);
	}

	gtk_combo_set_popdown_strings(GTK_COMBO (plot->combobox), combo_items);

	entry = GTK_ENTRY(GTK_COMBO(plot->combobox)->entry);
	g_signal_connect(G_OBJECT (entry), "changed",
		G_CALLBACK(analysis_selected), plot);

	gtk_entry_set_text (GTK_ENTRY (entry), s_current ? s_current : "?");

	list = GTK_TREE_VIEW(
		gtk_object_get_data ( GTK_OBJECT (plot->window), "clist"));

	plot->title = g_strdup_printf (_("Plot - %s"), s_current);
	plot->xtitle = get_variable_units (first ? first->var_units[0] : "##");
	plot->ytitle = g_strdup (first ? first->var_units[1] : "!!");

	g_object_set (G_OBJECT (plot->plot_model), "title", plot->title, NULL);

	g_signal_connect(G_OBJECT(list), "button_release_event",
		G_CALLBACK(plot_selected), plot);

	g_free (s_current);

	analysis_selected (GTK_EDITABLE (entry), plot);

	return TRUE;
}


/*
 * GetBins
 *
 *   Author: Carlos
 *   Purpose: Finds out the binning for the scale of the control
 *            Wmin,Wmax       : range of the scale
 *            Nold            : wanted number of bins
 *            BinLow, BinHigh : Optimized low and hig bins for which
 *                              a label is to be drawn
 *            Nbins           : number of bins
 *            Bwidth          : bin width between ticks
 *
 */
static void
getbins (gdouble Wmin, gdouble Wmax, gint Nold,
	 gdouble *BinLow, gdouble *BinHigh, gint *Nbins, gdouble *Bwidth)
{
	gint lwid, kwid;
	gint ntemp = 0;
	gint jlog  = 0;
	gdouble siground = 0;
	gdouble alb, awidth, sigfig,atest;

	gdouble AL = MIN (Wmin, Wmax);
	gdouble AH = MAX (Wmin, Wmax);
	if (AL == AH)
		AH = AL + 1;

/*
 * If Nold  ==  -1, program uses binwidth input from calling routine.
 */
	if (Nold == -1 && (*Bwidth) > 0)
		goto L90;

	ntemp = MAX (Nold, 2);
	if (ntemp < 1)
		ntemp = 1;

/*
 * Get nominal bin width in exponential form.
 */

 L20:
	awidth = (AH - AL) / ((gdouble)ntemp);
	if (awidth > 1.e30)
		goto LOK;

	jlog   = (gint)(log10 (awidth));
	if (awidth <= 1)
		jlog--;

	sigfig = awidth * ((gdouble)pow (10, -jlog));

/*
 * Round mantissa up to 1, 2, 2.5, 5, or 10.
 */

	if (sigfig <= 1)
		siground = 1;
	else if (sigfig <= 2)
		siground = 2;
/*   else if (sigfig <= 2.5)  siground = 2.5;*/
	else if (sigfig <= 5)
		siground = 5;
	else {
		siground = 1;
		jlog++;
	}

	(*Bwidth) = siground * pow (10, jlog);

/*
 * Get new bounds from new width Bwidth.
 */

 L90:
	alb = AL / (*Bwidth);
	lwid = (gint) (alb);
	if (alb < 0)
		lwid--;
	(*BinLow) = siground*((gdouble)lwid)*pow(10,jlog);
	alb = AH / (*Bwidth) + 1.00001f;
	kwid = (gint) alb;
	if (alb < 0)
		kwid--;
	(*BinHigh) = siground * ((gdouble) kwid) * pow (10, jlog);
	(*Nbins) = kwid - lwid;
	if (Nold == -1)
		goto LOK;
	if (Nold <= 5) { /* Request for one bin is difficult case. */
		if (Nold > 1 || (*Nbins) == 1)
			goto LOK;

		(*Bwidth) = (*Bwidth) * 2;
		(*Nbins) = 1;
		goto LOK;
	}
	if (2 * (*Nbins) == Nold) {
		ntemp++;
		goto L20;
	}

 LOK:
	atest = (*Bwidth) * 0.0001f;
	if (fabs ((*BinLow) - Wmin)  >= atest) {
		(*BinLow) += (*Bwidth);
		(*Nbins)--;
	}

	if (fabs ((*BinHigh) - Wmax) >= atest) {
		(*BinHigh) -= (*Bwidth);
		(*Nbins)--;
	}
}


static void
plot_zoom_50_cmd (GtkWidget *widget, Plot *plot)
{
	gnome_canvas_set_pixels_per_unit (GNOME_CANVAS(plot->canvas), 0.5);
	gtk_widget_queue_draw (plot->canvas);
}

static void
plot_zoom_75_cmd (GtkWidget *widget, Plot *plot)
{
	gnome_canvas_set_pixels_per_unit (GNOME_CANVAS (plot->canvas), 0.75);
	gtk_widget_queue_draw (plot->canvas);
}

static void 
plot_zoom_100_cmd (GtkWidget *widget, Plot *plot)
{
	gdouble a, b, c, d;
	gtk_cairo_plot_model_get_x_minmax (plot->plot_model, &a, &b);
	gtk_cairo_plot_model_get_y_minmax (plot->plot_model, &c, &d);
	gtk_cairo_plot_view_set_scroll_region (plot->plot_view, a, b, c, d);
	gtk_widget_queue_draw (plot->canvas);
}

static void 
plot_zoom_125_cmd (GtkWidget *widget, Plot *plot)
{
	gnome_canvas_set_pixels_per_unit (GNOME_CANVAS (plot->canvas), 1.25);
	gtk_widget_queue_draw (plot->canvas);
}

static void 
plot_zoom_150_cmd (GtkWidget *widget, Plot *plot)
{
	gnome_canvas_set_pixels_per_unit (GNOME_CANVAS (plot->canvas), 1.5);
	gtk_widget_queue_draw (plot->canvas);
}

static void 
plot_zoom_200_cmd (GtkWidget *widget, Plot *plot)
{
	gnome_canvas_set_pixels_per_unit (GNOME_CANVAS (plot->canvas), 2.0);
	gtk_widget_queue_draw (plot->canvas);
}

static void
plot_toggle_cross (GtkWidget *widget, Plot *plot)
{
	plot->show_cursor = gtk_check_menu_item_get_active (GTK_CHECK_MENU_ITEM (widget));
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
			_("<span weight=\"bold\" size=\"x-large\">Could not find the required file:\n</span>%s\n"),
			OREGANO_GLADEDIR "/plot-export.glade2");
		oregano_error (msg);
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

	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (export_png)))
		format = GTK_CAIRO_PLOT_VIEW_SAVE_PNG;
	else
		format = GTK_CAIRO_PLOT_VIEW_SAVE_PS;

	fn = dialog_file_open (_("Save PNG"));
	if (fn == NULL) {
		gtk_widget_destroy (window);
		return;
	}

	w = gtk_spin_button_get_value_as_int (width);
	h = gtk_spin_button_get_value_as_int (height);

	gtk_cairo_plot_view_save (plot->plot_view,
		plot->plot_model, w, h, fn, format);

	gtk_widget_destroy (window);
}
