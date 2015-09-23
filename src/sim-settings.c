/*
 * sim-settings.c
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
 * Copyright (C) 2013-2014  Bernhard Schuster
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

#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include "oregano.h"
#include "sim-settings.h"
#include "schematic.h"
#include "schematic-view.h"
#include "dialogs.h"
#include "oregano-utils.h"
#include "netlist-helper.h"
#include "errors.h"
#include "log.h"

struct _SimSettingsPriv
{
	// Transient analysis.
	GtkWidget *w_main;
	GtkWidget *w_trans_enable, *w_trans_start, *w_trans_stop;
	GtkWidget *w_trans_step, *w_trans_step_enable, *w_trans_init_cond, *w_trans_frame;
	gboolean trans_enable;
	gboolean trans_init_cond;
	gchar *trans_start;
	gchar *trans_stop;
	gchar *trans_step;
	gboolean trans_step_enable;

	// AC
	GtkWidget *w_ac_enable, *w_ac_type, *w_ac_npoints, *w_ac_start, *w_ac_stop, *w_ac_frame;
	gboolean ac_enable;
	gchar *ac_type;
	gchar *ac_npoints;
	gchar *ac_start;
	gchar *ac_stop;

	// DC
	GtkWidget *w_dc_enable, *w_dc_vin, *w_dc_start, *w_dc_stop, *w_dc_step, *w_dcsweep_frame;
	gboolean dc_enable;
	gchar *dc_vin;
	gchar *dc_start, *dc_stop, *dc_step;

	// Fourier analysis. Replace with something sane later.
	GtkWidget *w_four_enable, *w_four_freq, *w_four_vout, *w_four_combobox, *w_four_add,
	    *w_four_rem, *w_fourier_frame;
	gboolean fourier_enable;
	gchar *fourier_frequency;
	gchar *fourier_nb_vout;
	GSList *fourier_vout;

	// Options
	GtkEntry *w_opt_value;
	GtkTreeView *w_opt_list;
	GList *options;
};

static char *AC_types_list[] = {"DEC", "LIN", "OCT", NULL};

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

#include "debug.h"

static void fourier_add_vout_cb (GtkButton *w, SimSettings *sim)
{
	GtkComboBoxText *node_box;
	GSList *node_slist;
	gint i;
	gint result = FALSE;

	node_box = GTK_COMBO_BOX_TEXT (sim->priv->w_four_combobox);

	// Get the node identifier
	for (i = 0; (i < 1000 && result == FALSE); ++i) {
		if (g_strcmp0 (g_strdup_printf ("V(%d)", i),
		               gtk_combo_box_text_get_active_text (node_box)) == 0)
			result = TRUE;
	}
	result = FALSE;

	// Is the node identifier already available?
	node_slist = g_slist_copy (sim->priv->fourier_vout);
	while (node_slist) {
		if ((i - 1) == atoi (node_slist->data)) {
			result = TRUE;
		}
		node_slist = node_slist->next;
	}
	g_slist_free (node_slist);
	if (!result) {
		GSList *node_slist;
		gchar *text = NULL;

		// Add Node (i-1) at the end of fourier_vout
		text = g_strdup_printf ("%d", i - 1);
		sim->priv->fourier_vout =
		    g_slist_append (sim->priv->fourier_vout, g_strdup_printf ("%d", i - 1));

		// Update the fourier_vout widget
		node_slist = g_slist_copy (sim->priv->fourier_vout);
		if (node_slist->data)
			text = g_strdup_printf ("V(%d)", atoi (node_slist->data));
		node_slist = node_slist->next;
		while (node_slist) {
			if (node_slist->data)
				text = g_strdup_printf ("%s V(%d)", text, atoi (node_slist->data));
			node_slist = node_slist->next;
		}
		if (text)
			gtk_entry_set_text (GTK_ENTRY (sim->priv->w_four_vout), text);
		else
			gtk_entry_set_text (GTK_ENTRY (sim->priv->w_four_vout), "");
		g_slist_free (node_slist);
	}
}

static void fourier_remove_vout_cb (GtkButton *w, SimSettings *sim)
{
	GtkComboBoxText *node_box;
	gint result = FALSE;
	gint i;

	node_box = GTK_COMBO_BOX_TEXT (sim->priv->w_four_combobox);

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

			tmp = sim->priv->fourier_vout;
			while (tmp) {
				if (atoi (tmp->data) == i - 1) {
					if (prev)
						prev->next = tmp->next;
					else
						sim->priv->fourier_vout = tmp->next;

					g_slist_free_1 (tmp);
					break;
				}
				prev = tmp;
				tmp = prev->next;
			}
		}

		// Update the fourier_vout widget
		node_slist = g_slist_copy (sim->priv->fourier_vout);
		if (node_slist->data)
			text = g_strdup_printf ("V(%d)", atoi (node_slist->data));
		if (node_slist)
			node_slist = node_slist->next;
		while (node_slist) {
			if (node_slist->data)
				text = g_strdup_printf ("%s V(%d)", text, atoi (node_slist->data));
			node_slist = node_slist->next;
		}
		if (text)
			gtk_entry_set_text (GTK_ENTRY (sim->priv->w_four_vout), text);
		else
			gtk_entry_set_text (GTK_ENTRY (sim->priv->w_four_vout), "");

		g_slist_free (node_slist);
	}
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

static void get_options_from_list (SimSettings *s)
{
	int i;
	gchar *name, *value;
	SimOption *so;
	GtkTreeModel *model;
	GtkTreeIter iter;
	GtkTreeView *cl = s->priv->w_opt_list;

	//  Empty the list
	while (s->priv->options) {
		so = s->priv->options->data;
		if (so) {
			// Prevent sigfault on NULL
			if (so->name)
				g_free (so->name);
			if (so->value)
				g_free (so->value);
			s->priv->options = g_list_remove (s->priv->options, so);
			g_free (so);
		}
		if (s->priv->options)
			s->priv->options = s->priv->options->next;
	}

	model = gtk_tree_view_get_model (cl);
	i = 0;
	while (gtk_tree_model_iter_nth_child (model, &iter, NULL, i)) {
		SimOption *so;
		gtk_tree_model_get (model, &iter, 0, &name, 1, &value, -1);

		so = g_new0 (SimOption, 1);
		so->name = g_strdup (name);
		so->value = g_strdup (value);
		s->priv->options = g_list_append (s->priv->options, so);
		i++;
	}
}

static gboolean select_opt_callback (GtkWidget *widget, GdkEventButton *event, SimSettings *sim)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	GtkTreeSelection *selection;
	char *value;

	if (event->button != 1)
		return FALSE;

	// Get the current selected row
	selection = gtk_tree_view_get_selection (sim->priv->w_opt_list);
	model = gtk_tree_view_get_model (sim->priv->w_opt_list);

	if (!gtk_tree_selection_get_selected (selection, NULL, &iter)) {
		return FALSE;
	}

	gtk_tree_model_get (model, &iter, 1, &value, -1);

	if (value)
		gtk_entry_set_text (sim->priv->w_opt_value, value);
	else
		gtk_entry_set_text (sim->priv->w_opt_value, "");

	return FALSE;
}

static void option_setvalue (GtkWidget *w, SimSettings *s)
{
	const gchar *value;
	GtkTreeModel *model;
	GtkTreeIter iter;
	GtkTreeSelection *selection;

	value = gtk_entry_get_text (s->priv->w_opt_value);
	if (!value)
		return;

	// Get the current selected row
	selection = gtk_tree_view_get_selection (s->priv->w_opt_list);
	model = gtk_tree_view_get_model (s->priv->w_opt_list);

	if (!gtk_tree_selection_get_selected (selection, NULL, &iter)) {
		return;
	}

	gtk_list_store_set (GTK_LIST_STORE (model), &iter, 1, value, -1);
}

static void add_option (GtkWidget *w, SimSettings *s)
{
	GtkEntry *entry;
	GtkWidget *dialog = gtk_dialog_new_with_buttons (
	    _ ("Add new option"), NULL, GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
	    GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT, GTK_STOCK_OK, GTK_RESPONSE_OK, NULL);

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
		sim_settings_add_option (s, opt);

		gtk_list_store_append (GTK_LIST_STORE (gtk_tree_view_get_model (s->priv->w_opt_list)),
		                       &iter);
		gtk_list_store_set (GTK_LIST_STORE (gtk_tree_view_get_model (s->priv->w_opt_list)), &iter,
		                    0, opt->name, -1);
	}

	gtk_widget_destroy (dialog);
}

static void option_remove (GtkWidget *w, SimSettings *s)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	GtkTreeSelection *selection;

	// Get the current selected row
	selection = gtk_tree_view_get_selection (s->priv->w_opt_list);
	model = gtk_tree_view_get_model (s->priv->w_opt_list);

	if (!gtk_tree_selection_get_selected (selection, NULL, &iter)) {
		return;
	}

	gtk_list_store_set (GTK_LIST_STORE (model), &iter, 1, "", -1);
}

static void trans_enable_cb (GtkWidget *widget, SimSettings *s)
{
	gboolean enable;
	enable = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));

	gtk_widget_set_sensitive (s->priv->w_trans_frame, enable);
}

static void trans_step_enable_cb (GtkWidget *widget, SimSettings *s)
{
	gboolean enable, step_enable;

	step_enable = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
	enable = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (s->priv->w_trans_enable));

	gtk_widget_set_sensitive (s->priv->w_trans_step, step_enable & enable);
}

static void entry_changed_cb (GtkWidget *widget, SimSettings *s)
{
	// FIXME?
}

static void ac_enable_cb (GtkWidget *widget, SimSettings *s)
{
	gboolean enable;
	enable = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
	gtk_widget_set_sensitive (s->priv->w_ac_frame, enable);
}

static void fourier_enable_cb (GtkWidget *widget, SimSettings *s)
{
	gboolean enable;
	enable = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
	gtk_widget_set_sensitive (s->priv->w_fourier_frame, enable);
}

static void dc_enable_cb (GtkWidget *widget, SimSettings *s)
{
	gboolean enable;
	enable = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
	gtk_widget_set_sensitive (s->priv->w_dcsweep_frame, enable);
}

static int delete_event_cb (GtkWidget *widget, GdkEvent *event, SimSettings *s)
{
	s->pbox = NULL;
	return FALSE;
}

SimSettings *sim_settings_new (Schematic *sm)
{
	SimSettings *s;

	s = g_new0 (SimSettings, 1);
	s->sm = sm;

	s->priv = g_new0 (SimSettingsPriv, 1);

	// Set some default settings.
	// transient
	s->priv->trans_enable = TRUE;
	s->priv->trans_start = g_strdup ("0 s");
	s->priv->trans_stop = g_strdup ("5 ms");
	s->priv->trans_step = g_strdup ("0.1 ms");
	s->priv->trans_step_enable = FALSE;

	//  AC
	s->priv->ac_enable = FALSE;
	s->priv->ac_type = g_strdup ("DEC");
	s->priv->ac_npoints = g_strdup ("50");
	s->priv->ac_start = g_strdup ("1");
	s->priv->ac_stop = g_strdup ("1 Meg");

	// DC
	s->priv->dc_enable = FALSE;
	s->priv->dc_vin = g_strdup ("");
	s->priv->dc_start = g_strdup ("");
	s->priv->dc_stop = g_strdup ("");
	s->priv->dc_step = g_strdup ("");

	// Fourier
	s->priv->fourier_enable = FALSE;
	s->priv->fourier_frequency = g_strdup ("");
	s->priv->fourier_vout = NULL;

	s->priv->options = NULL;

	return s;
}

gboolean sim_settings_get_trans (SimSettings *sim_settings)
{
	return sim_settings->priv->trans_enable;
}

gboolean sim_settings_get_trans_init_cond (SimSettings *sim_settings)
{
	return sim_settings->priv->trans_init_cond;
}

gdouble sim_settings_get_trans_start (SimSettings *sim_settings)
{
	gchar *text = sim_settings->priv->trans_start;
	return oregano_strtod (text, 's');
}

gdouble sim_settings_get_trans_stop (SimSettings *sim_settings)
{
	gchar *text = sim_settings->priv->trans_stop;
	return oregano_strtod (text, 's');
}

gdouble sim_settings_get_trans_step (SimSettings *sim_settings)
{
	gchar *text = sim_settings->priv->trans_step;
	return oregano_strtod (text, 's');
}

gdouble sim_settings_get_trans_step_enable (SimSettings *sim_settings)
{
	return sim_settings->priv->trans_step_enable;
}

void sim_settings_set_trans (SimSettings *sim_settings, gboolean enable)
{
	sim_settings->priv->trans_enable = enable;
}

void sim_settings_set_trans_start (SimSettings *sim_settings, gchar *str)
{
	if (sim_settings->priv->trans_start)
		g_strdup (sim_settings->priv->trans_start);
	sim_settings->priv->trans_start = g_strdup (str);
}

void sim_settings_set_trans_init_cond (SimSettings *sim_settings, gboolean enable)
{
	sim_settings->priv->trans_init_cond = enable;
}

void sim_settings_set_trans_stop (SimSettings *sim_settings, gchar *str)
{
	if (sim_settings->priv->trans_stop)
		g_strdup (sim_settings->priv->trans_stop);
	sim_settings->priv->trans_stop = g_strdup (str);
}

void sim_settings_set_trans_step (SimSettings *sim_settings, gchar *str)
{
	if (sim_settings->priv->trans_step)
		g_strdup (sim_settings->priv->trans_step);
	sim_settings->priv->trans_step = g_strdup (str);
}

void sim_settings_set_trans_step_enable (SimSettings *sim_settings, gboolean enable)
{
	sim_settings->priv->trans_step_enable = enable;
}

gboolean sim_settings_get_ac (SimSettings *sim_settings) { return sim_settings->priv->ac_enable; }

gchar *sim_settings_get_ac_type (SimSettings *sim_settings) { return sim_settings->priv->ac_type; }

gint sim_settings_get_ac_npoints (SimSettings *sim_settings)
{
	return atoi (sim_settings->priv->ac_npoints);
}

gdouble sim_settings_get_ac_start (SimSettings *sim_settings)
{
	return oregano_strtod (sim_settings->priv->ac_start, 's');
}

gdouble sim_settings_get_ac_stop (SimSettings *sim_settings)
{
	return oregano_strtod (sim_settings->priv->ac_stop, 's');
}

void sim_settings_set_ac (SimSettings *sim_settings, gboolean enable)
{
	sim_settings->priv->ac_enable = enable;
}

void sim_settings_set_ac_type (SimSettings *sim_settings, gchar *str)
{
	if (sim_settings->priv->ac_type)
		g_free (sim_settings->priv->ac_type);
	sim_settings->priv->ac_type = g_strdup (str);
}

void sim_settings_set_ac_npoints (SimSettings *sim_settings, gchar *str)
{
	if (sim_settings->priv->ac_npoints)
		g_free (sim_settings->priv->ac_npoints);
	sim_settings->priv->ac_npoints = g_strdup (str);
}

void sim_settings_set_ac_start (SimSettings *sim_settings, gchar *str)
{
	if (sim_settings->priv->ac_start)
		g_free (sim_settings->priv->ac_start);
	sim_settings->priv->ac_start = g_strdup (str);
}

void sim_settings_set_ac_stop (SimSettings *sim_settings, gchar *str)
{
	if (sim_settings->priv->ac_stop)
		g_free (sim_settings->priv->ac_stop);
	sim_settings->priv->ac_stop = g_strdup (str);
}

gboolean sim_settings_get_dc (SimSettings *sim_settings) { return sim_settings->priv->dc_enable; }

gchar *sim_settings_get_dc_vsrc (SimSettings *sim_settings) { return sim_settings->priv->dc_vin; }

gdouble sim_settings_get_dc_start (SimSettings *sim_settings)
{
	return oregano_strtod (sim_settings->priv->dc_start, 's');
}

gdouble sim_settings_get_dc_stop (SimSettings *sim_settings)
{
	return oregano_strtod (sim_settings->priv->dc_stop, 's');
}

gdouble sim_settings_get_dc_step (SimSettings *sim_settings)
{
	return oregano_strtod (sim_settings->priv->dc_step, 's');
}

void sim_settings_set_dc (SimSettings *sim_settings, gboolean enable)
{
	sim_settings->priv->dc_enable = enable;
}

void sim_settings_set_dc_vsrc (SimSettings *sim_settings, gchar *str)
{
	if (sim_settings->priv->dc_vin)
		g_free (sim_settings->priv->dc_vin);
	sim_settings->priv->dc_vin = g_strdup (str);
}

void sim_settings_set_dc_start (SimSettings *sim_settings, gchar *str)
{
	if (sim_settings->priv->dc_start)
		g_free (sim_settings->priv->dc_start);
	sim_settings->priv->dc_start = g_strdup (str);
}

void sim_settings_set_dc_stop (SimSettings *sim_settings, gchar *str)
{
	if (sim_settings->priv->dc_stop)
		g_free (sim_settings->priv->dc_stop);
	sim_settings->priv->dc_stop = g_strdup (str);
}

void sim_settings_set_dc_step (SimSettings *sim_settings, gchar *str)
{
	if (sim_settings->priv->dc_step)
		g_free (sim_settings->priv->dc_step);
	sim_settings->priv->dc_step = g_strdup (str);
}

void sim_settings_set_fourier (SimSettings *sim_settings, gboolean enable)
{
	sim_settings->priv->fourier_enable = enable;
}

void sim_settings_set_fourier_frequency (SimSettings *sim_settings, gchar *str)
{
	if (sim_settings->priv->fourier_frequency)
		g_free (sim_settings->priv->fourier_frequency);
	sim_settings->priv->fourier_frequency = g_strdup (str);
}

void sim_settings_set_fourier_vout (SimSettings *sim_settings, gchar *str)
{
	gchar **node_ids = NULL;
	gint i;
	if (sim_settings->priv->fourier_vout)
		g_free (sim_settings->priv->fourier_vout);
	node_ids = g_strsplit (g_strdup (str), " ", 0);
	for (i = 0; node_ids[i] != NULL; i++) {
		if (node_ids[i])
			sim_settings->priv->fourier_vout =
			    g_slist_append (sim_settings->priv->fourier_vout, g_strdup (node_ids[i]));
	}
}

gboolean sim_settings_get_fourier (SimSettings *sim_settings)
{
	return sim_settings->priv->fourier_enable;
}

gint sim_settings_get_fourier_frequency (SimSettings *sim_settings)
{
	return atoi (sim_settings->priv->fourier_frequency);
}

gchar *sim_settings_get_fourier_vout (SimSettings *sim)
{
	GSList *node_slist;
	gchar *text = NULL;

	node_slist = g_slist_copy (sim->priv->fourier_vout);
	if (node_slist)
		text = g_strdup_printf ("%d", atoi (node_slist->data));
	if (node_slist)
		node_slist = node_slist->next;
	while (node_slist) {
		if (node_slist->data)
			text = g_strdup_printf ("%s %d", text, atoi (node_slist->data));
		node_slist = node_slist->next;
	}
	g_slist_free (node_slist);
	return text;
}

gchar *sim_settings_get_fourier_nodes (SimSettings *sim)
{
	GSList *node_slist;
	gchar *text = NULL;

	node_slist = g_slist_copy (sim->priv->fourier_vout);
	if (node_slist->data)
		text = g_strdup_printf ("V(%d)", atoi (node_slist->data));
	if (node_slist)
		node_slist = node_slist->next;
	while (node_slist) {
		if (node_slist->data)
			text = g_strdup_printf ("%s V(%d)", text, atoi (node_slist->data));
		node_slist = node_slist->next;
	}
	g_slist_free (node_slist);
	return text;
}

static void response_callback (GtkButton *button, Schematic *sm)
{
	g_return_if_fail (sm != NULL);
	g_return_if_fail (IS_SCHEMATIC (sm));
	g_return_if_fail (button != NULL);
	g_return_if_fail (GTK_IS_BUTTON (button));
	gint page;
	SimSettings *s;
	SimSettingsPriv *priv;
	gchar *tmp = NULL;
	gchar **node_ids = NULL;

	s = schematic_get_sim_settings (sm);
	priv = s->priv;

	g_object_get (s->notebook, "page", &page, NULL);

	// Trans
	priv->trans_enable = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->w_trans_enable));

	if (priv->trans_start)
		g_free (priv->trans_start);
	priv->trans_start = gtk_editable_get_chars (GTK_EDITABLE (priv->w_trans_start), 0, -1);

	if (priv->trans_stop)
		g_free (priv->trans_stop);
	priv->trans_stop = gtk_editable_get_chars (GTK_EDITABLE (priv->w_trans_stop), 0, -1);

	if (priv->trans_step)
		g_free (priv->trans_step);
	priv->trans_step = gtk_editable_get_chars (GTK_EDITABLE (priv->w_trans_step), 0, -1);

	priv->trans_step_enable =
	    gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->w_trans_step_enable));

	priv->trans_init_cond =
	    gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->w_trans_init_cond));

	// DC
	priv->dc_enable = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->w_dc_enable));
	if (priv->dc_vin)
		g_free (priv->dc_vin);

	tmp = gtk_combo_box_text_get_active_text (GTK_COMBO_BOX_TEXT (priv->w_dc_vin));
	node_ids = g_strsplit (g_strdup (tmp), "V(", 0);
	tmp = node_ids[1];
	node_ids = g_strsplit (g_strdup (tmp), ")", 0);
	priv->dc_vin = node_ids[0];

	if (priv->dc_start)
		g_free (priv->dc_start);
	priv->dc_start = g_strdup (gtk_entry_get_text (GTK_ENTRY (priv->w_dc_start)));

	if (priv->dc_stop)
		g_free (priv->dc_stop);
	priv->dc_stop = g_strdup (gtk_entry_get_text (GTK_ENTRY (priv->w_dc_stop)));

	if (priv->dc_step)
		g_free (priv->dc_step);
	priv->dc_step = g_strdup (gtk_entry_get_text (GTK_ENTRY (priv->w_dc_step)));

	// AC
	priv->ac_enable = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->w_ac_enable));

	if (priv->ac_type)
		g_free (priv->ac_type);
	priv->ac_type = gtk_combo_box_text_get_active_text (GTK_COMBO_BOX_TEXT (priv->w_ac_type));

	if (priv->ac_npoints)
		g_free (priv->ac_npoints);
	priv->ac_npoints = g_strdup (gtk_entry_get_text (GTK_ENTRY (priv->w_ac_npoints)));

	if (priv->ac_start)
		g_free (priv->ac_start);
	priv->ac_start = g_strdup (gtk_entry_get_text (GTK_ENTRY (priv->w_ac_start)));

	if (priv->ac_stop)
		g_free (priv->ac_stop);
	priv->ac_stop = g_strdup (gtk_entry_get_text (GTK_ENTRY (priv->w_ac_stop)));

	// Fourier analysis
	priv->fourier_enable = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->w_four_enable));

	if (priv->fourier_frequency)
		g_free (priv->fourier_frequency);
	priv->fourier_frequency = g_strdup (gtk_entry_get_text (GTK_ENTRY (priv->w_four_freq)));

	// Options
	get_options_from_list (s);
	gtk_widget_hide (GTK_WIDGET (s->pbox));
	s->pbox = NULL;
	s->notebook = NULL;

	// Schematic is dirty now ;-)
	schematic_set_dirty (sm, TRUE);
	g_free (tmp);
	g_free (node_ids);
}

/**
 * this code is an uggly piece of shit, fix it!
 */
void sim_settings_show (GtkWidget *widget, SchematicView *sv)
{
	int i;
	GtkWidget *toplevel, *w, *pbox, *combo_box;
	GtkTreeView *opt_list;
	GtkCellRenderer *cell_option, *cell_value;
	GtkTreeViewColumn *column_option, *column_value;
	GtkListStore *opt_model;
	GtkTreeIter treeiter;
	GtkBuilder *builder;
	GError *e = NULL;
	SimSettings *s;
	SimSettingsPriv *priv;
	Schematic *sm;
	GList *iter;
	GSList *siter;
	GList *sources = NULL;
	GtkComboBox *node_box;
	GtkListStore *node_list_store;
	gchar *text = NULL;
	GSList *slist = NULL, *node_list = NULL;

	g_return_if_fail (sv != NULL);

	sm = schematic_view_get_schematic (sv);
	s = schematic_get_sim_settings (sm);
	priv = s->priv;

	// Only allow one instance of the property box per schematic.
	if (s->pbox != NULL) {
		if (gtk_widget_get_visible (s->pbox) == FALSE) {
			gtk_widget_set_visible (s->pbox, TRUE);
		}
		GdkWindow *window = gtk_widget_get_window (s->pbox);
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
	s->pbox = pbox;
	s->notebook = GTK_NOTEBOOK (gtk_builder_get_object (builder, "notebook"));
	g_signal_connect (G_OBJECT (pbox), "delete_event", G_CALLBACK (delete_event_cb), s);

	//  Prepare options list
	priv->w_opt_value = GTK_ENTRY (gtk_builder_get_object (builder, "opt_value"));
	opt_list = GTK_TREE_VIEW (gtk_builder_get_object (builder, "option_list"));
	priv->w_opt_list = opt_list;

	//  Grab the frames
	priv->w_trans_frame = GTK_WIDGET (gtk_builder_get_object (builder, "trans_frame"));
	priv->w_ac_frame = GTK_WIDGET (gtk_builder_get_object (builder, "ac_frame"));
	priv->w_dcsweep_frame = GTK_WIDGET (gtk_builder_get_object (builder, "dcsweep_frame"));
	priv->w_fourier_frame = GTK_WIDGET (gtk_builder_get_object (builder, "fourier_frame"));

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

	if (priv->options == NULL) {
		// Load defaults
		for (i = 0; default_options[i].name; i++) {
			gtk_list_store_append (opt_model, &treeiter);
			gtk_list_store_set (opt_model, &treeiter, 0, default_options[i].name, -1);
		}
	} else {
		// Load schematic options

		for (iter = priv->options; iter; iter = iter->next) {
			SimOption *so = iter->data;
			if (so) {
				gtk_list_store_append (opt_model, &treeiter);
				gtk_list_store_set (opt_model, &treeiter, 0, so->name, -1);
			}
		}
	}

	// Set the options already stored
	for (iter = priv->options; iter; iter = iter->next) {
		SimOption *so = iter->data;
		if (so)
			set_options_in_list (so->name, so->value, opt_list);
	}

	g_signal_connect (G_OBJECT (opt_list), "button_release_event", G_CALLBACK (select_opt_callback),
	                  s);

	w = GTK_WIDGET (gtk_builder_get_object (builder, "opt_accept"));
	g_signal_connect (G_OBJECT (w), "clicked", G_CALLBACK (option_setvalue), s);
	w = GTK_WIDGET (gtk_builder_get_object (builder, "opt_remove"));
	g_signal_connect (G_OBJECT (w), "clicked", G_CALLBACK (option_remove), s);
	w = GTK_WIDGET (gtk_builder_get_object (builder, "add_option"));
	g_signal_connect (G_OBJECT (w), "clicked", G_CALLBACK (add_option), s);

	// Creation of Close Button
	w = GTK_WIDGET (gtk_builder_get_object (builder, "button1"));
	g_signal_connect (G_OBJECT (w), "clicked", G_CALLBACK (response_callback), sm);

	// Transient //
	// ********* //
	w = GTK_WIDGET (gtk_builder_get_object (builder, "trans_start"));
	priv->w_trans_start = w;
	if (priv->trans_start)
		gtk_entry_set_text (GTK_ENTRY (w), priv->trans_start);
	g_signal_connect (G_OBJECT (w), "changed", G_CALLBACK (entry_changed_cb), s);

	w = GTK_WIDGET (gtk_builder_get_object (builder, "trans_stop"));
	priv->w_trans_stop = w;
	if (priv->trans_stop)
		gtk_entry_set_text (GTK_ENTRY (w), priv->trans_stop);
	g_signal_connect (G_OBJECT (w), "changed", G_CALLBACK (entry_changed_cb), s);

	w = GTK_WIDGET (gtk_builder_get_object (builder, "trans_step"));
	priv->w_trans_step = w;
	if (priv->trans_step)
		gtk_entry_set_text (GTK_ENTRY (w), priv->trans_step);
	g_signal_connect (G_OBJECT (w), "changed", G_CALLBACK (entry_changed_cb), s);

	w = GTK_WIDGET (gtk_builder_get_object (builder, "trans_enable"));
	priv->w_trans_enable = w;
	g_signal_connect (G_OBJECT (w), "toggled", G_CALLBACK (trans_enable_cb), s);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w), priv->trans_enable);

	w = GTK_WIDGET (gtk_builder_get_object (builder, "trans_step_enable"));
	priv->w_trans_step_enable = w;
	g_signal_connect (G_OBJECT (w), "toggled", G_CALLBACK (trans_step_enable_cb), s);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w), priv->trans_step_enable);

	// FIXME this does do nothing!
	w = GTK_WIDGET (gtk_builder_get_object (builder, "trans_init_cond"));
	priv->w_trans_init_cond = w;
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w), priv->trans_init_cond);

	// AC  //
	// *** //
	w = GTK_WIDGET (gtk_builder_get_object (builder, "ac_enable"));
	priv->w_ac_enable = w;
	g_signal_connect (G_OBJECT (w), "toggled", G_CALLBACK (ac_enable_cb), s);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w), priv->ac_enable);

	// Initialilisation of the various AC types
	w = GTK_WIDGET (gtk_builder_get_object (builder, "ac_type"));
	gtk_widget_destroy (w); // FIXME wtf??
	w = GTK_WIDGET (gtk_builder_get_object (builder, "grid14"));
	combo_box = gtk_combo_box_text_new ();
	gtk_grid_attach (GTK_GRID (w), combo_box, 1, 0, 1, 1);
	priv->w_ac_type = combo_box;

	{
		gint index = 0;
		for (; AC_types_list[index] != NULL; index++) {
			gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo_box), AC_types_list[index]);
		}
		gtk_combo_box_set_active (GTK_COMBO_BOX (combo_box), 0);
	}
	g_assert (GTK_IS_COMBO_BOX (combo_box));
	g_signal_connect (G_OBJECT (combo_box), "changed", G_CALLBACK (entry_changed_cb), s);

	w = GTK_WIDGET (gtk_builder_get_object (builder, "ac_npoints"));
	priv->w_ac_npoints = w;
	gtk_entry_set_text (GTK_ENTRY (w), priv->ac_npoints);
	g_signal_connect (G_OBJECT (w), "changed", G_CALLBACK (entry_changed_cb), s);

	w = GTK_WIDGET (gtk_builder_get_object (builder, "ac_start"));
	priv->w_ac_start = w;
	gtk_entry_set_text (GTK_ENTRY (w), priv->ac_start);
	g_signal_connect (G_OBJECT (w), "changed", G_CALLBACK (entry_changed_cb), s);

	w = GTK_WIDGET (gtk_builder_get_object (builder, "ac_stop"));
	priv->w_ac_stop = w;
	gtk_entry_set_text (GTK_ENTRY (w), priv->ac_stop);
	g_signal_connect (G_OBJECT (w), "changed", G_CALLBACK (entry_changed_cb), s);

	//  DC   //
	// ***** //
	w = GTK_WIDGET (gtk_builder_get_object (builder, "dc_enable"));
	priv->w_dc_enable = w;
	g_signal_connect (G_OBJECT (w), "toggled", G_CALLBACK (dc_enable_cb), s);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w), priv->dc_enable);

	//  Get list of sources
	node_list = netlist_helper_get_voltmeters_list (sm, &e);
	if (e) {
		log_append_error (schematic_get_log_store (sm), _ ("SimulationSettings"),
		                  _ ("Failed to create netlist"), e);
		g_clear_error (&e);
		return; // FIXME we never get to the fourier because of this
	}
	if (node_list == NULL) {
		log_append (schematic_get_log_store (sm), _ ("SimulationSettings"),
		            _ ("No node in the schematic!"));
		return; // FIXME we never get to the fourier because of this
	}

	sources = NULL;
	for (siter = node_list; siter; siter = siter->next) {
		gchar *tmp;
		tmp = g_strdup_printf ("V(%d)", atoi (siter->data));
		sources = g_list_prepend (sources, tmp);
	}
	w = GTK_WIDGET (gtk_builder_get_object (builder, "dc_vin1"));
	gtk_widget_destroy (w); // FIXME wtf??
	w = GTK_WIDGET (gtk_builder_get_object (builder, "grid13"));
	combo_box = gtk_combo_box_text_new ();

	gtk_grid_attach (GTK_GRID (w), combo_box, 1, 0, 1, 1);
	priv->w_dc_vin = combo_box;
	if (sources) {
		for (iter = sources; iter; iter = iter->next) {
			gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo_box), iter->data);
		}
		gtk_combo_box_set_active (GTK_COMBO_BOX (combo_box), 0);
	}
	g_signal_connect (G_OBJECT (combo_box), "changed", G_CALLBACK (entry_changed_cb), s);

	w = GTK_WIDGET (gtk_builder_get_object (builder, "dc_start1"));
	priv->w_dc_start = w;
	gtk_entry_set_text (GTK_ENTRY (w), priv->dc_start);
	g_signal_connect (G_OBJECT (w), "changed", G_CALLBACK (entry_changed_cb), s);

	w = GTK_WIDGET (gtk_builder_get_object (builder, "dc_stop1"));
	priv->w_dc_stop = w;
	gtk_entry_set_text (GTK_ENTRY (w), priv->dc_stop);
	g_signal_connect (G_OBJECT (w), "changed", G_CALLBACK (entry_changed_cb), s);

	w = GTK_WIDGET (gtk_builder_get_object (builder, "dc_step1"));
	priv->w_dc_step = w;
	gtk_entry_set_text (GTK_ENTRY (w), priv->dc_step);
	g_signal_connect (G_OBJECT (w), "changed", G_CALLBACK (entry_changed_cb), s);

	g_list_free_full (sources, g_free);

	// Fourier //
	// ******* //
	g_print ("XXXXXXXXXXXXXXXX\n");
	w = GTK_WIDGET (gtk_builder_get_object (builder, "fourier_enable"));
	priv->w_four_enable = w;
	g_print ("XXXXXXXXXXXXXXXX %p %s\n", w, priv->fourier_enable ? "TRUE" : "FALSE");
	g_signal_connect (G_OBJECT (w), "toggled", G_CALLBACK (fourier_enable_cb), s);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w), priv->fourier_enable);

	w = GTK_WIDGET (gtk_builder_get_object (builder, "fourier_freq"));
	priv->w_four_freq = w;
	g_signal_connect (G_OBJECT (w), "changed", G_CALLBACK (entry_changed_cb), s);
	gtk_entry_set_text (GTK_ENTRY (w), priv->fourier_frequency);

	w = GTK_WIDGET (gtk_builder_get_object (builder, "fourier_vout"));
	priv->w_four_vout = w;
	g_signal_connect (G_OBJECT (w), "changed", G_CALLBACK (entry_changed_cb), s);

	text = NULL;
	slist = g_slist_copy (priv->fourier_vout); // TODO why copy here?
	if (slist) {
		if (atoi (slist->data) != 0)
			text = g_strdup_printf ("V(%d)", atoi (slist->data));

		for (siter = slist; siter; siter = siter->next) {
			if (atoi (siter->data) != 0)
				text = g_strdup_printf ("%s V(%d)", text, atoi (siter->data));
		}

		if (text)
			gtk_entry_set_text (GTK_ENTRY (w), text);
		else
			gtk_entry_set_text (GTK_ENTRY (w), "");
	}
	if (text)
		gtk_entry_set_text (GTK_ENTRY (w), text);
	else
		gtk_entry_set_text (GTK_ENTRY (w), "");
	g_slist_free (slist);

	// Present in the combo box the nodes of the schematic
	w = GTK_WIDGET (gtk_builder_get_object (builder, "fourier_select_out"));
	gtk_widget_destroy (w); // FIXME wtf???

	w = GTK_WIDGET (gtk_builder_get_object (builder, "grid12"));
	combo_box = gtk_combo_box_text_new ();

	gtk_grid_attach (GTK_GRID (w), combo_box, 2, 2, 1, 1);

	priv->w_four_combobox = combo_box;
	node_box = GTK_COMBO_BOX (combo_box);
	node_list_store = GTK_LIST_STORE (gtk_combo_box_get_model (node_box));
	gtk_list_store_clear (node_list_store);

	// Get the identification of the schematic nodes
	node_list = netlist_helper_get_voltmeters_list (sm, &e);
	if (e) {
		log_append_error (schematic_get_log_store (sm), _ ("SimulationSettings"),
		                  _ ("Failed to create netlist"), e);
		g_clear_error (&e);
		return;
	}
	if (!node_list) {
		log_append (schematic_get_log_store (sm), _ ("SimulationSettings"),
		            _ ("No node in the schematic!"));
		return;
	}

	text = NULL;
	for (siter = node_list; siter; siter = siter->next) {
		if (siter->data)
			text = g_strdup_printf ("V(%d)", atoi (siter->data));
		if (text)
			gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (node_box), text);
	}
	gtk_combo_box_set_active (node_box, 0);
	w = GTK_WIDGET (gtk_builder_get_object (builder, "fourier_add"));
	priv->w_four_add = w;
	g_signal_connect (G_OBJECT (w), "clicked", G_CALLBACK (fourier_add_vout_cb), s);
	w = GTK_WIDGET (gtk_builder_get_object (builder, "fourier_rem"));
	priv->w_four_rem = w;
	g_signal_connect (G_OBJECT (w), "clicked", G_CALLBACK (fourier_remove_vout_cb), s);

	gtk_widget_show_all (toplevel);

	ac_enable_cb (priv->w_ac_enable, s);
	fourier_enable_cb (priv->w_four_enable, s);
	dc_enable_cb (priv->w_dc_enable, s);
	trans_enable_cb (priv->w_trans_enable, s);
	trans_step_enable_cb (priv->w_trans_step_enable, s);
}

GList *sim_settings_get_options (SimSettings *s)
{
	g_return_val_if_fail (s != NULL, NULL);
	g_return_val_if_fail (s->priv != NULL, NULL);
	return s->priv->options;
}

void sim_settings_add_option (SimSettings *s, SimOption *opt)
{
	GList *iter;
	SimSettingsPriv *priv = s->priv;
	// Remove the option if already in the list.
	for (iter = priv->options; iter; iter = iter->next) {
		SimOption *so = iter->data;
		if (so && !strcmp (opt->name, so->name)) {
			g_free (so->name);
			g_free (so->value);
			priv->options = g_list_remove (priv->options, so);
			g_free (so);
		}
	}
	priv->options = g_list_append (priv->options, opt);
}
