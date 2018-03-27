/*
 * sim-settings-gui.c
 *
 *
 * Authors:
 *  Richard Hult <rhult@hem.passagen.se>
 *  Ricardo Markiewicz <rmarkie@fi.uba.ar>
 *  Andres de Barbara <adebarbara@fi.uba.ar>
 *  Marc Lorber <lorber.marc@wanadoo.fr>
 *  Bernhard Schuster <bernhard@ahoi.io>
 *  Guido Trentalancia <guido@trentalancia.com>
 *
 * Web page: https://ahoi.io/project/oregano
 *
 * Copyright (C) 1999-2001  Richard Hult
 * Copyright (C) 2003,2006  Ricardo Markiewicz
 * Copyright (C) 2009-2012  Marc Lorber
 * Copyright (C) 2013-2014  Bernhard Schuster
 * Copyright (C) 2017-2017  Guido Trentalancia
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

#include "engines/netlist-helper.h"
#include "schematic-view.h"
#include "sim-settings-gui.h"
#include <glib/gi18n.h>
#include <stdlib.h>

static char *scale_types_list[] = {"DEC", "LIN", "OCT", NULL};

static SimOption default_options[] = {{"TEMP", NULL},

                                      {"GMIN", NULL},
                                      {"ABSTOL", NULL},
                                      {"CHGTOL", NULL},
                                      {"RELTOL", NULL},
                                      {"VNTOL", NULL},

                                      {"ITL1", NULL},
                                      {"ITL2", NULL},
                                      {"ITL4", NULL},

                                      {"PIVREL", NULL},
                                      {"PIVTOL", NULL},

                                      {"TNOM", NULL},
                                      {"TRTOL", NULL},

                                      {"DEFAD", NULL},
                                      {"DEFAS", NULL},
                                      {"DEFL", NULL},
                                      {"DEFW", NULL},
                                      {NULL, NULL}};

SimSettingsGui *sim_settings_gui_new() {
	SimSettingsGui *sim_settings_gui = g_new0(SimSettingsGui, 1);

	sim_settings_gui->sim_settings = sim_settings_new();

	return sim_settings_gui;
}

void sim_settings_gui_finalize(SimSettingsGui *gui) {
	sim_settings_finalize(gui->sim_settings);
	g_free(gui);
}

static void set_options_in_list (gchar *key, gchar *val, GtkTreeView *cl)
{
	int i;
	GtkTreeModel *model;
	GtkTreeIter iter;
	char *name;
	char *value;

	model = gtk_tree_view_get_model (cl);

	i = 0;
	while (gtk_tree_model_iter_nth_child (model, &iter, NULL, i)) {
		gtk_tree_model_get (model, &iter, 0, &name, 1, &value, -1);
		if (!strcmp (name, key)) {
			gtk_list_store_set (GTK_LIST_STORE (model), &iter, 1, val, -1);
		}
		i++;
	}
}

static void get_options_from_list (SimSettingsGui *s_gui)
{
	int i;
	gchar *name, *value;
	SimOption *so;
	GtkTreeModel *model;
	GtkTreeIter iter;
	GtkTreeView *cl = s_gui->w_opt_list;
	SimSettings *s = s_gui->sim_settings;

	//  Empty the list
	while (s->options) {
		so = s->options->data;
		if (so) {
			g_free (so->name);
			g_free (so->value);
			s->options = g_list_remove (s->options, so);
			g_free (so);
		}
		if (s->options)
			s->options = s->options->next;
	}

	model = gtk_tree_view_get_model (cl);
	i = 0;
	while (gtk_tree_model_iter_nth_child (model, &iter, NULL, i)) {
		SimOption *so;
		gtk_tree_model_get (model, &iter, 0, &name, 1, &value, -1);

		so = g_new0 (SimOption, 1);
		so->name = g_strdup (name);
		so->value = g_strdup (value);
		s->options = g_list_append (s->options, so);
		i++;
	}
}

static gboolean select_opt_callback (GtkWidget *widget, GdkEventButton *event, SimSettingsGui *sim)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	GtkTreeSelection *selection;
	char *value;

	if (event->button != 1)
		return FALSE;

	// Get the current selected row
	selection = gtk_tree_view_get_selection (sim->w_opt_list);
	model = gtk_tree_view_get_model (sim->w_opt_list);

	if (!gtk_tree_selection_get_selected (selection, NULL, &iter)) {
		return FALSE;
	}

	gtk_tree_model_get (model, &iter, 1, &value, -1);

	if (value)
		gtk_entry_set_text (sim->w_opt_value, value);
	else
		gtk_entry_set_text (sim->w_opt_value, "");

	return FALSE;
}

static void option_setvalue (GtkWidget *w, SimSettingsGui *s)
{
	const gchar *value;
	GtkTreeModel *model;
	GtkTreeIter iter;
	GtkTreeSelection *selection;

	value = gtk_entry_get_text (s->w_opt_value);
	if (!value)
		return;

	// Get the current selected row
	selection = gtk_tree_view_get_selection (s->w_opt_list);
	model = gtk_tree_view_get_model (s->w_opt_list);

	if (!gtk_tree_selection_get_selected (selection, NULL, &iter)) {
		return;
	}

	gtk_list_store_set (GTK_LIST_STORE (model), &iter, 1, value, -1);
}

static void add_option (GtkWidget *w, SimSettingsGui *s)
{
	GtkEntry *entry;
	GtkWidget *dialog = gtk_dialog_new_with_buttons (
	    _ ("Add new option"), NULL, GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
	    "_Cancel", GTK_RESPONSE_REJECT, "_OK", GTK_RESPONSE_OK, NULL);

	entry = GTK_ENTRY (gtk_entry_new ());
	gtk_container_add (GTK_CONTAINER (gtk_dialog_get_content_area (GTK_DIALOG (dialog))),
	                   GTK_WIDGET (entry));
	gtk_widget_show (GTK_WIDGET (entry));

	if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_OK) {
		GtkTreeIter iter;
		SimOption *opt = g_new0 (SimOption, 1);
		opt->name = g_strdup (gtk_entry_get_text (entry));
		opt->value = g_strdup ("");
		// Warning : don't free opt later, is added to the list
		sim_settings_add_option (s->sim_settings, opt);

		gtk_list_store_append (GTK_LIST_STORE (gtk_tree_view_get_model (s->w_opt_list)),
		                       &iter);
		gtk_list_store_set (GTK_LIST_STORE (gtk_tree_view_get_model (s->w_opt_list)), &iter,
		                    0, opt->name, -1);
	}

	gtk_widget_destroy (dialog);
}

static void option_remove (GtkWidget *w, SimSettingsGui *s)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	GtkTreeSelection *selection;

	// Get the current selected row
	selection = gtk_tree_view_get_selection (s->w_opt_list);
	model = gtk_tree_view_get_model (s->w_opt_list);

	if (!gtk_tree_selection_get_selected (selection, NULL, &iter)) {
		return;
	}

	gtk_list_store_set (GTK_LIST_STORE (model), &iter, 1, "", -1);
}

static void entry_changed_cb (GtkWidget *widget, SimSettingsGui *s)
{
	// FIXME?
}

static int delete_event_cb (GtkWidget *widget, GdkEvent *event, SimSettingsGui *s)
{
	s->pbox = NULL;
	return FALSE;
}

static void trans_enable_cb (GtkWidget *widget, SimSettingsGui *s)
{
	gboolean enable;
	enable = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));

	gtk_widget_set_sensitive (s->w_trans_frame, enable);
}

static void trans_step_enable_cb (GtkWidget *widget, SimSettingsGui *s)
{
	gboolean enable, step_enable;

	step_enable = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
	enable = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (s->w_trans_enable));

	gtk_widget_set_sensitive (s->w_trans_step, step_enable & enable);
}

static void ac_enable_cb (GtkWidget *widget, SimSettingsGui *s)
{
	gboolean enable;
	enable = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
	gtk_widget_set_sensitive (s->w_ac_frame, enable);
}

static void dc_enable_cb (GtkWidget *widget, SimSettingsGui *s)
{
	gboolean enable;
	enable = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
	gtk_widget_set_sensitive (s->w_dcsweep_frame, enable);
}

static void fourier_enable_cb (GtkWidget *widget, SimSettingsGui *s)
{
	gboolean enable;
	enable = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
	gtk_widget_set_sensitive (s->w_fourier_frame, enable);
}

static void fourier_add_vout_cb (GtkButton *w, SimSettingsGui *sim)
{
	GtkComboBoxText *node_box;
	guint i;
	gboolean result = FALSE;
	gchar *text;

	node_box = GTK_COMBO_BOX_TEXT (sim->w_four_combobox);

	// Get the node identifier
	for (i = 0; (i < 1000 && result == FALSE); ++i) {
		text = g_strdup_printf ("V(%d)", i);
		if (!g_strcmp0 (text, gtk_combo_box_text_get_active_text (node_box)))
			result = TRUE;
		g_free (text);
	}

	text = NULL;
	if (result == TRUE)
		text = fourier_add_vout(sim->sim_settings, i);

	if (text)
		gtk_entry_set_text (GTK_ENTRY (sim->w_four_vout), text);
	else
		gtk_entry_set_text (GTK_ENTRY (sim->w_four_vout), "");
}

static void fourier_remove_vout_cb (GtkButton *w, SimSettingsGui *sim)
{
	GtkComboBoxText *node_box;
	gint result = FALSE;
	gint i;
	SimSettings *s = sim->sim_settings;

	node_box = GTK_COMBO_BOX_TEXT (sim->w_four_combobox);

	// Get the node identifier
	for (i = 0; (i < 1000 && result == FALSE); i++) {
		if (g_strcmp0 (g_strdup_printf ("V(%d)", i),
		               gtk_combo_box_text_get_active_text (node_box)) == 0)
			result = TRUE;
	}

	if (result) {
		GSList *node_slist;
		gchar *text = NULL;

		// Remove current data in the g_slist
		{
			GSList *tmp, *prev = NULL;

			tmp = s->fourier_vout;
			while (tmp) {
				if (atoi (tmp->data) == i - 1) {
					if (prev)
						prev->next = tmp->next;
					else
						s->fourier_vout = tmp->next;

					g_slist_free_1 (tmp);
					break;
				}
				prev = tmp;
				tmp = prev->next;
			}
		}

		// Update the fourier_vout widget
		node_slist = g_slist_copy (s->fourier_vout);
		if (node_slist) {
			if (node_slist->data && atoi (node_slist->data) > 0)
				text = g_strdup_printf ("V(%d)", atoi (node_slist->data));
			node_slist = node_slist->next;
		}
		while (node_slist) {
			if (node_slist->data && atoi (node_slist->data) > 0) {
				if (text)
					text = g_strdup_printf ("%s V(%d)", text, atoi (node_slist->data));
				else
					text = g_strdup_printf ("V(%d)", atoi (node_slist->data));
			}
			node_slist = node_slist->next;
		}
		if (text)
			gtk_entry_set_text (GTK_ENTRY (sim->w_four_vout), text);
		else
			gtk_entry_set_text (GTK_ENTRY (sim->w_four_vout), "");

		g_slist_free_full (node_slist, g_free);
	}
}

static void noise_enable_cb (GtkWidget *widget, SimSettingsGui *s)
{
	gboolean enable;
	enable = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
	gtk_widget_set_sensitive (s->w_noise_frame, enable);
}

static void response_callback (GtkButton *button, SchematicView *sv)
{
	g_return_if_fail (sv != NULL);
	g_return_if_fail (IS_SCHEMATIC_VIEW (sv));
	g_return_if_fail (button != NULL);
	g_return_if_fail (GTK_IS_BUTTON (button));
	gint page;
	gchar *tmp = NULL;
	gchar **node_ids = NULL;

	Schematic *sm = schematic_view_get_schematic (sv);

	SimSettingsGui *s_gui = schematic_get_sim_settings_gui(sm);
	SimSettings *s = s_gui->sim_settings;

	g_object_get (s_gui->notebook, "page", &page, NULL);

	// Transient analysis
	s->trans_enable = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (s_gui->w_trans_enable));

	g_free (s->trans_start);
	s->trans_start = gtk_editable_get_chars (GTK_EDITABLE (s_gui->w_trans_start), 0, -1);

	g_free (s->trans_stop);
	s->trans_stop = gtk_editable_get_chars (GTK_EDITABLE (s_gui->w_trans_stop), 0, -1);

	g_free (s->trans_step);
	s->trans_step = gtk_editable_get_chars (GTK_EDITABLE (s_gui->w_trans_step), 0, -1);

	s->trans_step_enable =
	    gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (s_gui->w_trans_step_enable));

	s->trans_init_cond =
	    gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (s_gui->w_trans_init_cond));

	s->trans_analyze_all =
	    gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (s_gui->w_trans_analyze_all));

	// DC
	s->dc_enable = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (s_gui->w_dc_enable));

	g_free (s->dc_vin);
	s->dc_vin = gtk_combo_box_text_get_active_text (GTK_COMBO_BOX_TEXT (s_gui->w_dc_vin));

	g_free (s->dc_vout);
	s->dc_vout = NULL;
	tmp = gtk_combo_box_text_get_active_text (GTK_COMBO_BOX_TEXT (s_gui->w_dc_vout));
	if (tmp) {
		node_ids = g_strsplit (tmp, "V(", 0);
		tmp = g_strdup (node_ids[1]);
		g_strfreev (node_ids);
		if (tmp) {
			node_ids = g_strsplit (tmp, ")", 0);
			g_free (tmp);
			if (node_ids[0])
				s->dc_vout = g_strdup (node_ids[0]);
			else
				s->dc_vout = g_strdup("");
			g_strfreev (node_ids);
		}
	}
	if (s->dc_vout == NULL)
		s->dc_vout = g_strdup("");

	g_free (s->dc_start);
	s->dc_start = g_strdup (gtk_entry_get_text (GTK_ENTRY (s_gui->w_dc_start)));

	g_free (s->dc_stop);
	s->dc_stop = g_strdup (gtk_entry_get_text (GTK_ENTRY (s_gui->w_dc_stop)));

	g_free (s->dc_step);
	s->dc_step = g_strdup (gtk_entry_get_text (GTK_ENTRY (s_gui->w_dc_step)));

	// AC
	s->ac_enable = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (s_gui->w_ac_enable));

	g_free (s->ac_vout);
	s->ac_vout = NULL;
	tmp = gtk_combo_box_text_get_active_text (GTK_COMBO_BOX_TEXT (s_gui->w_ac_vout));
	if (tmp) {
		node_ids = g_strsplit (tmp, "V(", 0);
		tmp = g_strdup (node_ids[1]);
		g_strfreev (node_ids);
		if (tmp) {
			node_ids = g_strsplit (tmp, ")", 0);
			g_free (tmp);
			if (node_ids[0])
				s->ac_vout = g_strdup (node_ids[0]);
			else
				s->ac_vout = g_strdup("");
			g_strfreev (node_ids);
		}
	}
	if (s->ac_vout == NULL)
		s->ac_vout = g_strdup("");

	g_free (s->ac_type);
	s->ac_type = gtk_combo_box_text_get_active_text (GTK_COMBO_BOX_TEXT (s_gui->w_ac_type));

	g_free (s->ac_npoints);
	s->ac_npoints = g_strdup (gtk_entry_get_text (GTK_ENTRY (s_gui->w_ac_npoints)));

	g_free (s->ac_start);
	s->ac_start = g_strdup (gtk_entry_get_text (GTK_ENTRY (s_gui->w_ac_start)));

	g_free (s->ac_stop);
	s->ac_stop = g_strdup (gtk_entry_get_text (GTK_ENTRY (s_gui->w_ac_stop)));

	// Fourier analysis
	s->fourier_enable = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (s_gui->w_four_enable));

	g_free (s->fourier_frequency);
	s->fourier_frequency = g_strdup (gtk_entry_get_text (GTK_ENTRY (s_gui->w_four_freq)));

	// Noise
	s->noise_enable = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (s_gui->w_noise_enable));

	g_free (s->noise_vin);
	s->noise_vin = gtk_combo_box_text_get_active_text (GTK_COMBO_BOX_TEXT (s_gui->w_noise_vin));

	g_free (s->noise_vout);
	s->noise_vout = NULL;
	tmp = gtk_combo_box_text_get_active_text (GTK_COMBO_BOX_TEXT (s_gui->w_noise_vout));
	if (tmp) {
		node_ids = g_strsplit (tmp, "V(", 0);
		tmp = g_strdup (node_ids[1]);
		g_strfreev (node_ids);
		if (tmp) {
			node_ids = g_strsplit (tmp, ")", 0);
			g_free (tmp);
			if (node_ids[0])
				s->noise_vout = g_strdup (node_ids[0]);
			else
				s->noise_vout = g_strdup("");
			g_strfreev (node_ids);
		}
	}
	if (s->noise_vout == NULL)
		s->noise_vout = g_strdup("");

	g_free (s->noise_type);
	s->noise_type = g_strdup (gtk_combo_box_text_get_active_text (GTK_COMBO_BOX_TEXT (s_gui->w_noise_type)));

	g_free (s->noise_npoints);
	s->noise_npoints = g_strdup (gtk_entry_get_text (GTK_ENTRY (s_gui->w_noise_npoints)));

	g_free (s->noise_start);
	s->noise_start = g_strdup (gtk_entry_get_text (GTK_ENTRY (s_gui->w_noise_start)));

	g_free (s->noise_stop);
	s->noise_stop = g_strdup (gtk_entry_get_text (GTK_ENTRY (s_gui->w_noise_stop)));

	// Options
	get_options_from_list (s_gui);
	gtk_widget_hide (GTK_WIDGET (s_gui->pbox));
	s_gui->pbox = NULL;
	s_gui->notebook = NULL;

	// Schematic is dirty now ;-)
	schematic_set_dirty (sm, TRUE);

	s->configured = TRUE;

	// The simulation settings configuration has
	// been triggered by an attempt to lauch the
	// simulation for the first time without
	// configuring it first.
	if (s->simulation_requested) {
		s->simulation_requested = FALSE;
		schematic_view_simulate_cmd (NULL, sv);
	}
}

/**
 * Get the list of voltmeters (test clamps).
 *
 * In normal mode, this does not include the
 * the type of measurement (normal, magnitude,
 * phase, real, imaginary or dB) and it is used
 * in DC and Fourier analysis.
 *
 * In AC mode, each element includes the type
 * of measurement (normal, magnitude, phase,
 * real, imaginary or dB) and it is used in
 * AC analysis.
*/ 
gint get_voltmeters_list(GList **voltmeters, Schematic *sm, GError *e, gboolean with_type)
{
	GSList *siter, *node_list = NULL;

	if (with_type)
		node_list = netlist_helper_get_voltmeters_list (sm, &e, TRUE);
	else
		node_list = netlist_helper_get_voltmeters_list (sm, &e, FALSE);
	if (e) {
		log_append_error (schematic_get_log_store (sm), _ ("SimulationSettings"),
		_ ("Failed to create netlist"), e);
		g_clear_error (&e);
		return -1;
	}
	if (node_list == NULL) {
		log_append (schematic_get_log_store (sm), _ ("SimulationSettings"),
			_ ("No node in the schematic!"));
		return -2;
	}

	*voltmeters = NULL;
	for (siter = node_list; siter; siter = siter->next) {
		gchar *tmp;
		if (with_type)
			tmp = g_strdup (siter->data);
		else
			tmp = g_strdup_printf ("V(%d)", atoi (siter->data));
		*voltmeters = g_list_prepend (*voltmeters, tmp);
	}

	return 0;
}

/**
 * Get the list of sources (indipendent voltage)
 */ 
gint get_voltage_sources_list(GList **sources, Schematic *sm, GError *e, gboolean ac_only)
{
	GSList *siter, *node_list = NULL;

	node_list = netlist_helper_get_voltage_sources_list (sm, &e, ac_only);
	if (e) {
		log_append_error (schematic_get_log_store (sm), _ ("SimulationSettings"),
		_ ("Failed to create netlist"), e);
		g_clear_error (&e);
		return -1;
	}
	if (node_list == NULL) {
		log_append (schematic_get_log_store (sm), _ ("SimulationSettings"),
			_ ("No node in the schematic!"));
		return -2;
	}

	*sources = NULL;
	for (siter = node_list; siter; siter = siter->next) {
		gchar *tmp;
		tmp = g_strdup_printf ("V%d", atoi (siter->data));
		*sources = g_list_prepend (*sources, tmp);
	}

	return 0;
}

/**
 * FIXME this code is an ugly piece of shit, fix it!
 */
void sim_settings_show (GtkWidget *widget, SchematicView *sv)
{
	int i;
	gboolean found;
	gint rc, active, index;
	GtkWidget *toplevel, *w, *w1, *pbox, *combo_box;
	GtkTreeView *opt_list;
	GtkCellRenderer *cell_option, *cell_value;
	GtkTreeViewColumn *column_option, *column_value;
	GtkListStore *opt_model;
	GtkTreeIter treeiter;
	GtkBuilder *builder;
	GError *e = NULL;
	SimSettings *s;
	GList *iter;
	GList *voltmeters = NULL;
	GList *voltmeters_with_type = NULL;
	GList *sources = NULL;
	GList *sources_ac = NULL;
	GtkComboBox *node_box;
	GtkListStore *node_list_store;
	gchar *text, *text2;
	gchar **node_ids;
	GSList *slist = NULL;

	g_return_if_fail (sv != NULL);

	Schematic *sm = schematic_view_get_schematic (sv);
	SimSettingsGui *s_gui = schematic_get_sim_settings_gui (sm);
	s = s_gui->sim_settings;

	// Only allow one instance of the property box per schematic.
	if (s_gui->pbox != NULL) {
		if (gtk_widget_get_visible (s_gui->pbox) == FALSE) {
			gtk_widget_set_visible (s_gui->pbox, TRUE);
		}
		GdkWindow *window = gtk_widget_get_window (s_gui->pbox);
		g_assert (window != NULL);
		gdk_window_raise (window);
		return;
	}

	if ((builder = gtk_builder_new ()) == NULL) {
		log_append (schematic_get_log_store (sm), _ ("SimulationSettings"),
		            _ ("Could not create simulation settings dialog"));
		return;
	}
	gtk_builder_set_translation_domain (builder, NULL);

	gtk_builder_add_from_file (builder, OREGANO_UIDIR "/sim-settings.ui", &e);
	if (e) {
		log_append_error (schematic_get_log_store (sm), _ ("SimulationSettings"),
		                  _ ("Could not create simulation settings dialog due to "
		                     "missing " OREGANO_UIDIR "/sim-settings.ui file"),
		                  e);
		g_clear_error (&e);
		return;
	}

	toplevel = GTK_WIDGET (gtk_builder_get_object (builder, "toplevel"));
	if (toplevel == NULL) {
		log_append (schematic_get_log_store (sm), _ ("SimulationSettings"),
		            _ ("Could not create simulation settings dialog due to missing "
		               "\"toplevel\" widget in " OREGANO_UIDIR "/sim-settings.ui file"));
		return;
	}

	pbox = toplevel;
	s_gui->pbox = pbox;
	s_gui->notebook = GTK_NOTEBOOK (gtk_builder_get_object (builder, "notebook"));
	g_signal_connect (G_OBJECT (pbox), "delete_event", G_CALLBACK (delete_event_cb), s_gui);

	//  Prepare options list
	s_gui->w_opt_value = GTK_ENTRY (gtk_builder_get_object (builder, "opt_value"));
	opt_list = GTK_TREE_VIEW (gtk_builder_get_object (builder, "option_list"));
	s_gui->w_opt_list = opt_list;

	//  Grab the frames
	s_gui->w_trans_frame = GTK_WIDGET (gtk_builder_get_object (builder, "trans_frame"));
	s_gui->w_ac_frame = GTK_WIDGET (gtk_builder_get_object (builder, "ac_frame"));
	s_gui->w_dcsweep_frame = GTK_WIDGET (gtk_builder_get_object (builder, "dcsweep_frame"));
	s_gui->w_fourier_frame = GTK_WIDGET (gtk_builder_get_object (builder, "fourier_frame"));
	s_gui->w_noise_frame = GTK_WIDGET (gtk_builder_get_object (builder, "noise_frame"));

	// Create the Columns
	cell_option = gtk_cell_renderer_text_new ();
	cell_value = gtk_cell_renderer_text_new ();
	column_option =
	    gtk_tree_view_column_new_with_attributes (N_ ("Option"), cell_option, "text", 0, NULL);
	column_value =
	    gtk_tree_view_column_new_with_attributes (N_ ("Value"), cell_value, "text", 1, NULL);

	// Create the model
	opt_model = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_STRING);
	gtk_tree_view_set_model (opt_list, GTK_TREE_MODEL (opt_model));
	gtk_tree_view_append_column (opt_list, column_option);
	gtk_tree_view_append_column (opt_list, column_value);

	if (s->options == NULL) {
		// Load defaults
		for (i = 0; default_options[i].name; i++) {
			gtk_list_store_append (opt_model, &treeiter);
			gtk_list_store_set (opt_model, &treeiter, 0, default_options[i].name, -1);
		}
	} else {
		// Load schematic options

		for (iter = s->options; iter; iter = iter->next) {
			SimOption *so = iter->data;
			if (so) {
				gtk_list_store_append (opt_model, &treeiter);
				gtk_list_store_set (opt_model, &treeiter, 0, so->name, -1);
			}
		}
	}

	// Set the options already stored
	for (iter = s->options; iter; iter = iter->next) {
		SimOption *so = iter->data;
		if (so)
			set_options_in_list (so->name, so->value, opt_list);
	}

	g_signal_connect (G_OBJECT (opt_list), "button_release_event", G_CALLBACK (select_opt_callback),
	                  s);

	w = GTK_WIDGET (gtk_builder_get_object (builder, "opt_accept"));
	g_signal_connect (G_OBJECT (w), "clicked", G_CALLBACK (option_setvalue), s_gui);
	w = GTK_WIDGET (gtk_builder_get_object (builder, "opt_remove"));
	w = GTK_WIDGET (gtk_builder_get_object (builder, "add_option"));
	g_signal_connect (G_OBJECT (w), "clicked", G_CALLBACK (option_remove), s_gui);
	g_signal_connect (G_OBJECT (w), "clicked", G_CALLBACK (add_option), s_gui);

	// Creation of Close Button
	w = GTK_WIDGET (gtk_builder_get_object (builder, "button1"));
	g_signal_connect (G_OBJECT (w), "clicked", G_CALLBACK (response_callback), sv);

	// Get the list of voltmeters (normal mode)
	rc = get_voltmeters_list(&voltmeters, sm, e, FALSE);
	if (rc) {
		sim_settings_set_dc(s, FALSE);
		sim_settings_set_fourier(s, FALSE);
	}

	// Get the list of voltmeters (AC mode, i.e. with measurement type)
	rc = get_voltmeters_list(&voltmeters_with_type, sm, e, TRUE);
	if (rc) {
		sim_settings_set_ac(s, FALSE);
	}

	// Get the list of sources (all types)
	rc = get_voltage_sources_list(&sources, sm, e, FALSE);
	if (rc) {
		sim_settings_set_dc(s, FALSE);
	}

	// Get the list of AC sources
	rc = get_voltage_sources_list(&sources_ac, sm, e, TRUE);
	if (rc) {
		sim_settings_set_noise(s, FALSE);
	}

	// Transient //
	// ********* //
	w = GTK_WIDGET (gtk_builder_get_object (builder, "trans_start"));
	s_gui->w_trans_start = w;
	if (s->trans_start)
		gtk_entry_set_text (GTK_ENTRY (w), s->trans_start);
	g_signal_connect (G_OBJECT (w), "changed", G_CALLBACK (entry_changed_cb), s_gui);

	w = GTK_WIDGET (gtk_builder_get_object (builder, "trans_stop"));
	s_gui->w_trans_stop = w;
	if (s->trans_stop)
		gtk_entry_set_text (GTK_ENTRY (w), s->trans_stop);
	g_signal_connect (G_OBJECT (w), "changed", G_CALLBACK (entry_changed_cb), s_gui);

	w = GTK_WIDGET (gtk_builder_get_object (builder, "trans_step"));
	s_gui->w_trans_step = w;
	if (s->trans_step)
		gtk_entry_set_text (GTK_ENTRY (w), s->trans_step);
	g_signal_connect (G_OBJECT (w), "changed", G_CALLBACK (entry_changed_cb), s_gui);

	w = GTK_WIDGET (gtk_builder_get_object (builder, "trans_enable"));
	s_gui->w_trans_enable = w;
	g_signal_connect (G_OBJECT (w), "toggled", G_CALLBACK (trans_enable_cb), s_gui);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w), s->trans_enable);

	w = GTK_WIDGET (gtk_builder_get_object (builder, "trans_step_enable"));
	s_gui->w_trans_step_enable = w;
	g_signal_connect (G_OBJECT (w), "toggled", G_CALLBACK (trans_step_enable_cb), s_gui);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w), s->trans_step_enable);

	// get the gui element
	w = GTK_WIDGET (gtk_builder_get_object (builder, "trans_init_cond"));
	// save the gui element to struct
	s_gui->w_trans_init_cond = w;
	// Set checkbox enabled, if trans_init_cond equal true.
	// trans_init_cond could be set to true because
	// - user opened the settings dialog some seconds ago and has set the checkbox
	// - user opened old file in which there was set the checkbox state to true
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w), s->trans_init_cond);

	// get the gui element
	w = GTK_WIDGET (gtk_builder_get_object (builder, "trans_analyze_all"));
	// save the gui element to struct
	s_gui->w_trans_analyze_all = w;
	// Set checkbox enabled, if trans_analyze_all equal true.
	// trans_init_cond could be set to true because
	// - user opened the settings dialog some seconds ago and has set the checkbox
	// - user opened old file in which there was set the checkbox state to true
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w), s->trans_analyze_all);

	// AC  //
	// *** //
	w = GTK_WIDGET (gtk_builder_get_object (builder, "ac_enable"));
	s_gui->w_ac_enable = w;
	g_signal_connect (G_OBJECT (w), "toggled", G_CALLBACK (ac_enable_cb), s_gui);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w), s->ac_enable);

	w1 = GTK_WIDGET (gtk_builder_get_object (builder, "grid14"));

	// FIXME: Should enable more than just one output as in the Fourier analysis
	w = GTK_WIDGET (gtk_builder_get_object (builder, "ac_vout"));
	gtk_widget_destroy (w); // FIXME wtf??
	combo_box = gtk_combo_box_text_new ();

	gtk_grid_attach (GTK_GRID (w1), combo_box, 1, 0, 1, 1);
	s_gui->w_ac_vout = combo_box;
	if (voltmeters_with_type) {
		index = 0;
		active = 0;
		for (iter = voltmeters_with_type; iter; iter = iter->next) {
			if (g_strcmp0 (iter->data, "VM(0)"))
				gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo_box), iter->data);
			if (!g_strcmp0(s->ac_vout, iter->data))
				active = index;
			index++;
		}
		gtk_combo_box_set_active (GTK_COMBO_BOX (combo_box), active);
	}
	g_signal_connect (G_OBJECT (combo_box), "changed", G_CALLBACK (entry_changed_cb), s);

	// Initialisation of the various scale types
	w = GTK_WIDGET (gtk_builder_get_object (builder, "ac_type"));
	gtk_widget_destroy (w); // FIXME wtf??
	combo_box = gtk_combo_box_text_new ();
	gtk_grid_attach (GTK_GRID (w1), combo_box, 1, 1, 1, 1);
	s_gui->w_ac_type = combo_box;

	{
		index = 0;
		active = 0;
		for (; scale_types_list[index] != NULL; index++) {
			gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo_box), scale_types_list[index]);
			if (!g_strcmp0(s->ac_type, scale_types_list[index]))
				active = index;
		}
		gtk_combo_box_set_active (GTK_COMBO_BOX (combo_box), active);
	}
	g_assert (GTK_IS_COMBO_BOX (combo_box));
	g_signal_connect (G_OBJECT (combo_box), "changed", G_CALLBACK (entry_changed_cb), s_gui);

	w = GTK_WIDGET (gtk_builder_get_object (builder, "ac_npoints"));
	s_gui->w_ac_npoints = w;
	gtk_entry_set_text (GTK_ENTRY (w), s->ac_npoints);
	g_signal_connect (G_OBJECT (w), "changed", G_CALLBACK (entry_changed_cb), s_gui);

	w = GTK_WIDGET (gtk_builder_get_object (builder, "ac_start"));
	s_gui->w_ac_start = w;
	gtk_entry_set_text (GTK_ENTRY (w), s->ac_start);
	g_signal_connect (G_OBJECT (w), "changed", G_CALLBACK (entry_changed_cb), s_gui);

	w = GTK_WIDGET (gtk_builder_get_object (builder, "ac_stop"));
	s_gui->w_ac_stop = w;
	gtk_entry_set_text (GTK_ENTRY (w), s->ac_stop);
	g_signal_connect (G_OBJECT (w), "changed", G_CALLBACK (entry_changed_cb), s_gui);

	//  DC   //
	// ***** //
	w = GTK_WIDGET (gtk_builder_get_object (builder, "dc_enable"));
	s_gui->w_dc_enable = w;
	g_signal_connect (G_OBJECT (w), "toggled", G_CALLBACK (dc_enable_cb), s_gui);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w), s->dc_enable);

	w1 = GTK_WIDGET (gtk_builder_get_object (builder, "grid13"));
	w = GTK_WIDGET (gtk_builder_get_object (builder, "dc_vin"));
	gtk_widget_destroy (w); // FIXME wtf??
	combo_box = gtk_combo_box_text_new ();

	gtk_grid_attach (GTK_GRID (w1), combo_box, 1, 0, 1, 1);
	s_gui->w_dc_vin = combo_box;
	if (sources) {
		index = 0;
		active = 0;
		for (iter = sources; iter; iter = iter->next) {
			gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo_box), iter->data);
			if (!g_strcmp0(s->dc_vin, iter->data))
				active = index;
			index++;
		}
		gtk_combo_box_set_active (GTK_COMBO_BOX (combo_box), active);
	}
	g_signal_connect (G_OBJECT (combo_box), "changed", G_CALLBACK (entry_changed_cb), s);

	// FIXME: Should enable more than just one output as in the Fourier analysis
	// FIXME: Should also allow to print currents through voltage sources
	w = GTK_WIDGET (gtk_builder_get_object (builder, "dc_vout"));
	gtk_widget_destroy (w); // FIXME wtf??
	combo_box = gtk_combo_box_text_new ();

	gtk_grid_attach (GTK_GRID (w1), combo_box, 1, 1, 1, 1);
	s_gui->w_dc_vout = combo_box;
	if (voltmeters) {
		index = 0;
		active = 0;
		text = g_strdup_printf("V(%s)", s->dc_vout);
		for (iter = voltmeters; iter; iter = iter->next) {
			if (g_strcmp0 (iter->data, "V(0)"))
				gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo_box), iter->data);
			if (!g_strcmp0(text, iter->data))
				active = index;
			index++;
		}
		g_free (text);
		gtk_combo_box_set_active (GTK_COMBO_BOX (combo_box), active);
	}
	g_signal_connect (G_OBJECT (combo_box), "changed", G_CALLBACK (entry_changed_cb), s_gui);

	w = GTK_WIDGET (gtk_builder_get_object (builder, "dc_start"));
	s_gui->w_dc_start = w;
	gtk_entry_set_text (GTK_ENTRY (w), s->dc_start);
	g_signal_connect (G_OBJECT (w), "changed", G_CALLBACK (entry_changed_cb), s_gui);

	w = GTK_WIDGET (gtk_builder_get_object (builder, "dc_stop"));
	s_gui->w_dc_stop = w;
	gtk_entry_set_text (GTK_ENTRY (w), s->dc_stop);
	g_signal_connect (G_OBJECT (w), "changed", G_CALLBACK (entry_changed_cb), s_gui);

	w = GTK_WIDGET (gtk_builder_get_object (builder, "dc_step"));
	s_gui->w_dc_step = w;
	gtk_entry_set_text (GTK_ENTRY (w), s->dc_step);
	g_signal_connect (G_OBJECT (w), "changed", G_CALLBACK (entry_changed_cb), s_gui);

	// Fourier //
	// ******* //
	g_print ("XXXXXXXXXXXXXXXX\n");
	w = GTK_WIDGET (gtk_builder_get_object (builder, "fourier_enable"));
	s_gui->w_four_enable = w;
	g_print ("XXXXXXXXXXXXXXXX %p %s\n", w, s->fourier_enable ? "TRUE" : "FALSE");
	g_signal_connect (G_OBJECT (w), "toggled", G_CALLBACK (fourier_enable_cb), s_gui);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w), s->fourier_enable);

	w = GTK_WIDGET (gtk_builder_get_object (builder, "fourier_freq"));
	s_gui->w_four_freq = w;
	g_signal_connect (G_OBJECT (w), "changed", G_CALLBACK (entry_changed_cb), s_gui);
	gtk_entry_set_text (GTK_ENTRY (w), s->fourier_frequency);

	w = GTK_WIDGET (gtk_builder_get_object (builder, "fourier_vout"));
	s_gui->w_four_vout = w;
	g_signal_connect (G_OBJECT (w), "changed", G_CALLBACK (entry_changed_cb), s_gui);

	// Get rid of inexistent output vectors
	text2 = NULL;
	if (voltmeters) {
		text = sim_settings_get_fourier_vout (s);
		node_ids = g_strsplit (text, " ", 0);
		g_free (text);
		for (i = 0; node_ids[i] != NULL; i++) {
			text = g_strdup_printf ("V(%s)", node_ids[i]);
			found = FALSE;
			for (iter = voltmeters; iter; iter = iter->next) {
				if (!g_strcmp0 (text, iter->data))
					found = TRUE;
			}	
			g_free (text);
			if (found) {
				if (text2) {
					text = text2;
					text2 = g_strdup_printf ("%s %s", text2, node_ids[i]);
					g_free (text);
				} else {
					text2 = g_strdup_printf ("%s", node_ids[i]);
				}
			}
		}
		if (!text2)
			text2 = g_strdup ("");
		sim_settings_set_fourier_vout (s, text2);
		g_free (text2);
		g_strfreev (node_ids);
	}

	text = NULL;
	slist = g_slist_copy (s->fourier_vout);
	if (slist) {
		if (slist->data && atoi (slist->data) > 0)
			text = g_strdup_printf ("V(%d)", atoi (slist->data));
		slist = slist->next;
	}
	while (slist) {
		if (slist->data && atoi (slist->data) > 0) {
			if (text) {
				text2 = text;
				text = g_strdup_printf ("%s V(%d)", text, atoi (slist->data));
				g_free (text2);
			} else {
				text = g_strdup_printf ("V(%d)", atoi (slist->data));
			}
		}
		slist = slist->next;
	}

	if (text)
		gtk_entry_set_text (GTK_ENTRY (w), text);
	else
		gtk_entry_set_text (GTK_ENTRY (w), "");

	g_slist_free_full (slist, g_free);

	// Present in the combo box the nodes of the schematic
	w = GTK_WIDGET (gtk_builder_get_object (builder, "fourier_select_out"));
	gtk_widget_destroy (w); // FIXME wtf???

	w = GTK_WIDGET (gtk_builder_get_object (builder, "grid12"));
	combo_box = gtk_combo_box_text_new ();

	gtk_grid_attach (GTK_GRID (w), combo_box, 2, 2, 1, 1);

	s_gui->w_four_combobox = combo_box;
	node_box = GTK_COMBO_BOX (combo_box);
	node_list_store = GTK_LIST_STORE (gtk_combo_box_get_model (node_box));
	gtk_list_store_clear (node_list_store);

	if (voltmeters) {
		for (iter = voltmeters; iter; iter = iter->next) {
			if (g_strcmp0 (iter->data, "V(0)"))
				gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (node_box), iter->data);
		}
		gtk_combo_box_set_active (GTK_COMBO_BOX (node_box), 0);
        }
	g_signal_connect (G_OBJECT (node_box), "changed", G_CALLBACK (entry_changed_cb), s);

	w = GTK_WIDGET (gtk_builder_get_object (builder, "fourier_add"));
	s_gui->w_four_add = w;
	g_signal_connect (G_OBJECT (w), "clicked", G_CALLBACK (fourier_add_vout_cb), s_gui);
	w = GTK_WIDGET (gtk_builder_get_object (builder, "fourier_rem"));
	s_gui->w_four_rem = w;
	g_signal_connect (G_OBJECT (w), "clicked", G_CALLBACK (fourier_remove_vout_cb), s_gui);

	// Noise  //
	// *** //
	w = GTK_WIDGET (gtk_builder_get_object (builder, "noise_enable"));
	s_gui->w_noise_enable = w;
	g_signal_connect (G_OBJECT (w), "toggled", G_CALLBACK (noise_enable_cb), s_gui);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w), s->noise_enable);

	w1 = GTK_WIDGET (gtk_builder_get_object (builder, "grid1"));

	w = GTK_WIDGET (gtk_builder_get_object (builder, "noise_vin"));
	gtk_widget_destroy (w); // FIXME wtf??
	combo_box = gtk_combo_box_text_new ();

	gtk_grid_attach (GTK_GRID (w1), combo_box, 1, 0, 1, 1);
	s_gui->w_noise_vin = combo_box;
	if (sources_ac) {
		index = 0;
		active = 0;
		for (iter = sources_ac; iter; iter = iter->next) {
			gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo_box), iter->data);
			if (!g_strcmp0(s->noise_vin, iter->data))
				active = index;
			index++;
		}
		gtk_combo_box_set_active (GTK_COMBO_BOX (combo_box), active);
	}
	g_signal_connect (G_OBJECT (combo_box), "changed", G_CALLBACK (entry_changed_cb), s);

	// FIXME: Should enable more than just one output as in the Fourier analysis
	w = GTK_WIDGET (gtk_builder_get_object (builder, "noise_vout"));
	gtk_widget_destroy (w); // FIXME wtf??
	combo_box = gtk_combo_box_text_new ();

	gtk_grid_attach (GTK_GRID (w1), combo_box, 1, 1, 1, 1);
	s_gui->w_noise_vout = combo_box;
	if (voltmeters) {
		index = 0;
		active = 0;
		text = g_strdup_printf ("V(%s)", s->noise_vout);
		for (iter = voltmeters; iter; iter = iter->next) {
			if (g_strcmp0 (iter->data, "V(0)"))
				gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo_box), iter->data);
			if (!g_strcmp0(text, iter->data))
				active = index;
			index++;
		}
		g_free (text);
		gtk_combo_box_set_active (GTK_COMBO_BOX (combo_box), active);
	}
	g_signal_connect (G_OBJECT (combo_box), "changed", G_CALLBACK (entry_changed_cb), s);

	// Initialisation of the various scale types
	w = GTK_WIDGET (gtk_builder_get_object (builder, "noise_type"));
	gtk_widget_destroy (w); // FIXME wtf??
	combo_box = gtk_combo_box_text_new ();
	gtk_grid_attach (GTK_GRID (w1), combo_box, 1, 2, 1, 1);
	s_gui->w_noise_type = combo_box;

	{
		index = 0;
		active = 0;
		for (; scale_types_list[index] != NULL; index++) {
			gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo_box), scale_types_list[index]);
			if (!g_strcmp0(s->noise_type, scale_types_list[index]))
				active = index;
		}
		gtk_combo_box_set_active (GTK_COMBO_BOX (combo_box), active);
	}
	g_assert (GTK_IS_COMBO_BOX (combo_box));
	g_signal_connect (G_OBJECT (combo_box), "changed", G_CALLBACK (entry_changed_cb), s_gui);

	w = GTK_WIDGET (gtk_builder_get_object (builder, "noise_npoints"));
	s_gui->w_noise_npoints = w;
	gtk_entry_set_text (GTK_ENTRY (w), s->noise_npoints);
	g_signal_connect (G_OBJECT (w), "changed", G_CALLBACK (entry_changed_cb), s_gui);

	w = GTK_WIDGET (gtk_builder_get_object (builder, "noise_start"));
	s_gui->w_noise_start = w;
	gtk_entry_set_text (GTK_ENTRY (w), s->noise_start);
	g_signal_connect (G_OBJECT (w), "changed", G_CALLBACK (entry_changed_cb), s_gui);

	w = GTK_WIDGET (gtk_builder_get_object (builder, "noise_stop"));
	s_gui->w_noise_stop = w;
	gtk_entry_set_text (GTK_ENTRY (w), s->noise_stop);
	g_signal_connect (G_OBJECT (w), "changed", G_CALLBACK (entry_changed_cb), s_gui);

	gtk_widget_show_all (toplevel);

	trans_enable_cb (s_gui->w_trans_enable, s_gui);
	trans_step_enable_cb (s_gui->w_trans_step_enable, s_gui);
	ac_enable_cb (s_gui->w_ac_enable, s_gui);
	dc_enable_cb (s_gui->w_dc_enable, s_gui);
	fourier_enable_cb (s_gui->w_four_enable, s_gui);
	noise_enable_cb (s_gui->w_noise_enable, s_gui);

	g_list_free(sources);
	g_list_free(voltmeters);
	g_list_free(voltmeters_with_type);
}
