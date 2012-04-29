/*
 * sim-settings.c
 *
 *
 * Authors:
 *  Richard Hult <rhult@hem.passagen.se>
 *  Ricardo Markiewicz <rmarkie@fi.uba.ar>
 *  Andres de Barbara <adebarbara@fi.uba.ar>
 *  Marc Lorber <lorber.marc@wanadoo.fr>
 *
 * Web page: https://github.com/marc-lorber/oregano
 *
 * Copyright (C) 1999-2001  Richard Hult
 * Copyright (C) 2003,2006  Ricardo Markiewicz
 * Copyright (C) 2009-2012  Marc Lorber
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

struct _SimSettingsPriv {
	// Transient analysis.
	GtkWidget *w_main;
	GtkWidget *w_trans_enable, *w_trans_start, *w_trans_stop;
	GtkWidget *w_trans_step, *w_trans_step_enable, *w_trans_init_cond,
		*w_trans_frame;
	gboolean trans_enable;
	gboolean trans_init_cond;
	gchar *trans_start;
	gchar *trans_stop;
	gchar *trans_step;
	gboolean trans_step_enable;

	// AC
	GtkWidget *w_ac_enable, *w_ac_type, *w_ac_npoints, *w_ac_start, *w_ac_stop, 
		*w_ac_frame;
	gboolean ac_enable;
	gchar   *ac_type;
	gchar   *ac_npoints;
	gchar   *ac_start;
	gchar   *ac_stop;

	// DC
	GtkWidget *w_dc_enable,*w_dc_vin,*w_dc_start,*w_dc_stop,*w_dc_step,
		*w_dcsweep_frame;
	gboolean dc_enable;
	gchar   *dc_vin;
	gchar   *dc_start,*dc_stop,*dc_step;

	// Fourier analysis. Replace with something sane later.
	GtkWidget *w_four_enable,*w_four_freq,*w_four_vout,*w_four_combobox,
		*w_four_add,*w_four_rem,*w_fourier_frame;
	gboolean fourier_enable;
	gchar *fourier_frequency;
	gchar *fourier_nb_vout;
	GSList *fourier_vout;

	// Options
	GtkEntry *w_opt_value;
	GtkTreeView  *w_opt_list;
	GList *options;
};

static char *AC_types_list[] = {
	"DEC",
	"LIN",
	"OCT",
	NULL
};

static SimOption default_options[] = {
		{"TEMP" ,NULL},

		{"GMIN" ,NULL},
		{"ABSTOL" ,NULL},
		{"CHGTOL" ,NULL},
		{"RELTOL" ,NULL},
		{"VNTOL" ,NULL},

		{"ITL1" ,NULL},
		{"ITL2" ,NULL},
		{"ITL4", NULL},

		{"PIVREL", NULL},
		{"PIVTOL", NULL},

		{"TNOM", NULL},
		{"TRTOL", NULL},

		{"DEFAD", NULL},
		{"DEFAS", NULL},
		{"DEFL", NULL},
		{"DEFW", NULL},
		{NULL, NULL}
};

#define NG_DEBUG(s) if (0) g_print ("%s\n", s)

static void
fourier_add_vout_cb (GtkButton *w, SimSettings *sim)
{
	GtkComboBoxText *node_box;
	GSList *node_slist;
	gint i;
	gint result = FALSE;
	
	node_box = GTK_COMBO_BOX_TEXT (sim->priv->w_four_combobox);
	
	// Get the node identifier
	for (i=0; (i <1000 && result == FALSE); ++i) {
		if (g_strcmp0 (g_strdup_printf ("V(%d)", i), 
		               gtk_combo_box_text_get_active_text (node_box)) == 0)
			result = TRUE;
	}
	result=FALSE;
	
	// Is the node identifier already available?
	node_slist = g_slist_copy (sim->priv->fourier_vout);
	while (node_slist) {
		if ((i-1) == atoi (node_slist->data)) {
			result = TRUE;
		}
		node_slist=node_slist->next;
	}
	g_slist_free (node_slist);
	if (!result) {
		GSList *node_slist;
		gchar *text = NULL;

		// Add Node (i-1) at the end of fourier_vout
		text = g_strdup_printf ("%d", i-1);
		sim->priv->fourier_vout = g_slist_append (sim->priv->fourier_vout, 
		                                          g_strdup_printf ("%d", i-1));

		// Update the fourier_vout widget
		node_slist = g_slist_copy (sim->priv->fourier_vout);
		if (node_slist->data)
			text = g_strdup_printf ("V(%d)", atoi (node_slist->data));
		node_slist=node_slist->next;
		while (node_slist)
		{
			if (node_slist->data)
				text = g_strdup_printf ("%s V(%d)", text, 
				                        atoi (node_slist->data));
			node_slist = node_slist->next;
		}
		if (text) 
			gtk_entry_set_text (GTK_ENTRY (sim->priv->w_four_vout), text);		
		else gtk_entry_set_text (GTK_ENTRY (sim->priv->w_four_vout), "");
		g_slist_free (node_slist);
	}
}

static void
fourier_remove_vout_cb (GtkButton *w, SimSettings *sim)
{
	GtkComboBoxText *node_box;
	gint result = FALSE;
	gint i;
	
	node_box = GTK_COMBO_BOX_TEXT (sim->priv->w_four_combobox);

	// Get the node identifier
	for (i=0; (i < 1000 && result == FALSE); i++) {
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
  			while (tmp)
    			{
      				if (atoi (tmp->data) == i-1)
					{
	  					if (prev) prev->next = tmp->next; 
	  					else sim->priv->fourier_vout = tmp->next;

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
		if (node_slist) node_slist=node_slist->next;
		while (node_slist)
		{
			if (node_slist->data) 
				text = g_strdup_printf ("%s V(%d)", text, 
				                        atoi (node_slist->data));
			node_slist = node_slist->next;
		}
		if (text) gtk_entry_set_text (GTK_ENTRY (sim->priv->w_four_vout), text);
		else gtk_entry_set_text (GTK_ENTRY (sim->priv->w_four_vout), "");

		g_slist_free (node_slist);	
	}
}

static void
set_options_in_list (gchar *key, gchar *val, GtkTreeView *cl)
{
	int i;
	GtkTreeModel *model;
	GtkTreeIter iter;
	char *name;
	char *value;

	model = gtk_tree_view_get_model (cl);

	i=0;
	while (gtk_tree_model_iter_nth_child (model, &iter, NULL, i)) {
		gtk_tree_model_get (model, &iter, 0, &name, 1, &value, -1);
		if (!strcmp (name, key)) {
			gtk_list_store_set (GTK_LIST_STORE(model), &iter, 1, val, -1);
		}
		i++;
	}
}

static void
get_options_from_list (SimSettings *s)
{
	int i;
	gchar *name,*value;
	SimOption *so;
	GtkTreeModel *model;
	GtkTreeIter iter;
	GtkTreeView *cl = s->priv->w_opt_list;

	//  Empty the list   
	while (s->priv->options) {
		so = s->priv->options->data;
		if (so) {
			// Prevent sigfault on NULL 
			if (so->name) g_free (so->name);
			if (so->value) g_free (so->value);
			s->priv->options = g_list_remove (s->priv->options,so);
			g_free (so);
		}
		if (s->priv->options) s->priv->options = s->priv->options->next;
	}

	model = gtk_tree_view_get_model (cl);
	i = 0;
	while (gtk_tree_model_iter_nth_child (model, &iter, NULL, i)) {
		SimOption *so;
		gtk_tree_model_get (model, &iter, 0, &name, 1, &value, -1);

		so = g_new0 (SimOption,1);
		so->name = g_strdup (name);
		so->value = g_strdup (value);
		s->priv->options = g_list_append (s->priv->options, so);
		i++;
	}
}

static gboolean
select_opt_callback (GtkWidget *widget,
					 GdkEventButton *event, SimSettings *sim)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	GtkTreeSelection *selection;
	char *value;

	if (event->button != 1) return FALSE;

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

static void
option_setvalue (GtkWidget *w, SimSettings *s)
{
	const gchar *value;
	GtkTreeModel *model;
	GtkTreeIter iter;
	GtkTreeSelection *selection;

	value = gtk_entry_get_text (s->priv->w_opt_value);
	if (!value) return;

	// Get the current selected row 
	selection = gtk_tree_view_get_selection (s->priv->w_opt_list);
	model = gtk_tree_view_get_model (s->priv->w_opt_list);

	if (!gtk_tree_selection_get_selected (selection, NULL, &iter)) {
		return;
	}

	gtk_list_store_set(GTK_LIST_STORE (model), &iter, 1, value, -1);
}

static void
add_option (GtkWidget *w, SimSettings *s)
{
	GtkEntry *entry;
	GtkWidget *dialog = gtk_dialog_new_with_buttons (_("Add new option"),
		NULL,
		GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
		GTK_STOCK_CANCEL,
		GTK_RESPONSE_REJECT,
		GTK_STOCK_OK,
		GTK_RESPONSE_OK,
		NULL);

	entry = GTK_ENTRY (gtk_entry_new ());
	gtk_container_add (GTK_CONTAINER (gtk_dialog_get_content_area (
	    GTK_DIALOG (dialog))), GTK_WIDGET (entry));
	gtk_widget_show (GTK_WIDGET (entry));

	if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_OK) {
		GtkTreeIter iter;
		SimOption *opt = g_new0 (SimOption, 1);
		opt->name = g_strdup (gtk_entry_get_text (entry));
		opt->value = g_strdup ("");
		// Warning : don't free opt later, is added to the list 
		sim_settings_add_option (s, opt);
	
		gtk_list_store_append (GTK_LIST_STORE (gtk_tree_view_get_model(
		                       s->priv->w_opt_list)), &iter);
		gtk_list_store_set (GTK_LIST_STORE (gtk_tree_view_get_model(
		                       s->priv->w_opt_list)), &iter, 0, opt->name, -1);
	}

	gtk_widget_destroy (dialog);
}


static void
option_remove (GtkWidget *w, SimSettings *s)
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

static void
trans_enable_cb (GtkWidget *widget, SimSettings *s)
{
	gboolean enable;
	enable = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
	
	if (enable)
		gtk_widget_show (s->priv->w_trans_frame);
	else
		gtk_widget_hide (s->priv->w_trans_frame);
	gtk_container_resize_children (GTK_CONTAINER (s->notebook));
}

static void
trans_step_enable_cb (GtkWidget *widget, SimSettings *s)
{
	gboolean enable, step_enable;

	step_enable = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
	enable = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (
	                                s->priv->w_trans_enable));

	gtk_widget_set_sensitive (s->priv->w_trans_step, step_enable & enable);
}

static void
entry_changed_cb (GtkWidget *widget, SimSettings *s)
{
	
}

static void
ac_enable_cb (GtkWidget *widget, SimSettings *s)
{
	gboolean enable;
	enable = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
	
	if (enable)
		gtk_widget_show (s->priv->w_ac_frame);
	else
		gtk_widget_hide (s->priv->w_ac_frame);
	gtk_container_resize_children (GTK_CONTAINER (s->notebook));
}

static void
fourier_enable_cb (GtkWidget *widget, SimSettings *s)
{
	gboolean enable;
	enable = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
	
	if (enable) 
		gtk_widget_show (s->priv->w_fourier_frame);
	else
		gtk_widget_hide (s->priv->w_fourier_frame);
	gtk_container_resize_children (GTK_CONTAINER (s->notebook));
}

static void
dc_enable_cb (GtkWidget *widget, SimSettings *s)
{
	gboolean enable;
	enable = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
	
	if (enable)
		gtk_widget_show (s->priv->w_dcsweep_frame);
	else
		gtk_widget_hide (s->priv->w_dcsweep_frame);
	gtk_container_resize_children (GTK_CONTAINER (s->notebook));
}

static int
delete_event_cb (GtkWidget *widget, GdkEvent *event, SimSettings *s)
{
	s->pbox = NULL;
	return FALSE;
}

SimSettings *
sim_settings_new (Schematic *sm)
{
	SimSettings *s;

	s = g_new0 (SimSettings, 1);
	s->sm = sm;

	s->priv = g_new0 (SimSettingsPriv, 1);

	//Set some default settings.
	// transient
	s->priv->trans_enable = TRUE;
	s->priv->trans_start = g_strdup ("0 s");
	s->priv->trans_stop = g_strdup ("5 ms");
	s->priv->trans_step = g_strdup ("0.1 ms");
	s->priv->trans_step_enable = FALSE;

	//  AC
	s->priv->ac_enable = FALSE;
	s->priv->ac_type   = g_strdup ("DEC");
	s->priv->ac_npoints= g_strdup ("50");
	s->priv->ac_start  = g_strdup ("1");
	s->priv->ac_stop   = g_strdup ("1 Meg");

	// DC
	s->priv->dc_enable = FALSE;
	s->priv->dc_vin   = g_strdup ("");
	s->priv->dc_start = g_strdup ("");
	s->priv->dc_stop  = g_strdup ("");
	s->priv->dc_step  = g_strdup ("");

	// Fourier
	s->priv->fourier_enable = FALSE;
	s->priv->fourier_frequency = g_strdup ("");
	s->priv->fourier_vout  = NULL;

	s->priv->options=0;

	return s;
}

gboolean
sim_settings_get_trans (SimSettings *sim_settings)
{
	return sim_settings->priv->trans_enable;
}

gboolean
sim_settings_get_trans_init_cond (SimSettings *sim_settings)
{
	return sim_settings->priv->trans_init_cond;
}

gdouble
sim_settings_get_trans_start (SimSettings *sim_settings)
{
	gchar *text = sim_settings->priv->trans_start;
	return oregano_strtod (text, 's');
}

gdouble
sim_settings_get_trans_stop (SimSettings *sim_settings)
{
	gchar *text = sim_settings->priv->trans_stop;
	return oregano_strtod (text, 's');
}

gdouble
sim_settings_get_trans_step (SimSettings *sim_settings)
{
	gchar *text = sim_settings->priv->trans_step;
	return oregano_strtod (text, 's');
}

gdouble
sim_settings_get_trans_step_enable (SimSettings *sim_settings)
{
	return sim_settings->priv->trans_step_enable;
}

void
sim_settings_set_trans (SimSettings *sim_settings, gboolean enable)
{
	sim_settings->priv->trans_enable = enable;
}

void
sim_settings_set_trans_start (SimSettings *sim_settings, gchar *str)
{
	if (sim_settings->priv->trans_start)
		g_strdup (sim_settings->priv->trans_start);
	sim_settings->priv->trans_start = g_strdup (str);
}

void
sim_settings_set_trans_init_cond (SimSettings *sim_settings, gboolean enable)
{
	sim_settings->priv->trans_init_cond = enable;
}

void
sim_settings_set_trans_stop (SimSettings *sim_settings, gchar *str)
{
	if (sim_settings->priv->trans_stop)
		g_strdup (sim_settings->priv->trans_stop);
	sim_settings->priv->trans_stop = g_strdup (str);
}

void
sim_settings_set_trans_step (SimSettings *sim_settings, gchar *str)
{
	if (sim_settings->priv->trans_step)
		g_strdup (sim_settings->priv->trans_step);
	sim_settings->priv->trans_step = g_strdup (str);
}

void
sim_settings_set_trans_step_enable (SimSettings *sim_settings, gboolean enable)
{
	sim_settings->priv->trans_step_enable = enable;
}

gboolean
sim_settings_get_ac (SimSettings *sim_settings)
{
	return sim_settings->priv->ac_enable;
}

gchar *
sim_settings_get_ac_type (SimSettings *sim_settings)
{
	return sim_settings->priv->ac_type;
}

gint
sim_settings_get_ac_npoints (SimSettings *sim_settings)
{
	return atoi (sim_settings->priv->ac_npoints);
}

gdouble
sim_settings_get_ac_start (SimSettings *sim_settings)
{
	return oregano_strtod (sim_settings->priv->ac_start, 's');
}

gdouble
sim_settings_get_ac_stop (SimSettings *sim_settings)
{
	return oregano_strtod (sim_settings->priv->ac_stop,'s');
}

void
sim_settings_set_ac (SimSettings *sim_settings,gboolean enable)
{
	sim_settings->priv->ac_enable = enable;
}

void
sim_settings_set_ac_type (SimSettings *sim_settings,gchar *str)
{
	if (sim_settings->priv->ac_type)
		g_free (sim_settings->priv->ac_type);
	sim_settings->priv->ac_type = g_strdup (str);
}

void
sim_settings_set_ac_npoints (SimSettings *sim_settings,gchar *str)
{
	if (sim_settings->priv->ac_npoints)
		g_free (sim_settings->priv->ac_npoints);
	sim_settings->priv->ac_npoints = g_strdup (str);

}

void
sim_settings_set_ac_start (SimSettings *sim_settings,gchar *str)
{
	if (sim_settings->priv->ac_start)
		g_free (sim_settings->priv->ac_start);
	sim_settings->priv->ac_start = g_strdup (str);
}

void
sim_settings_set_ac_stop (SimSettings *sim_settings,gchar *str)
{
	if (sim_settings->priv->ac_stop)
		g_free (sim_settings->priv->ac_stop);
	sim_settings->priv->ac_stop = g_strdup (str);
}

gboolean
sim_settings_get_dc (SimSettings *sim_settings)
{
	return sim_settings->priv->dc_enable;
}

gchar*
sim_settings_get_dc_vsrc (SimSettings *sim_settings)
{
	return sim_settings->priv->dc_vin;
}

gdouble
sim_settings_get_dc_start (SimSettings *sim_settings)
{
	return oregano_strtod (sim_settings->priv->dc_start, 's');
}

gdouble
sim_settings_get_dc_stop (SimSettings *sim_settings)
{
	return oregano_strtod (sim_settings->priv->dc_stop, 's');
}

gdouble
sim_settings_get_dc_step  (SimSettings *sim_settings)
{
	return oregano_strtod (sim_settings->priv->dc_step, 's');
}

void
sim_settings_set_dc (SimSettings *sim_settings, gboolean enable)
{
	sim_settings->priv->dc_enable = enable;
}

void
sim_settings_set_dc_vsrc (SimSettings *sim_settings, gchar *str)
{
	if (sim_settings->priv->dc_vin)
		g_free (sim_settings->priv->dc_vin);
	sim_settings->priv->dc_vin = g_strdup (str);
}

void
sim_settings_set_dc_start (SimSettings *sim_settings, gchar *str) 
{	
	if (sim_settings->priv->dc_start)
		g_free (sim_settings->priv->dc_start);
	sim_settings->priv->dc_start = g_strdup (str);
}

void
sim_settings_set_dc_stop  (SimSettings *sim_settings, gchar *str)
{
	if (sim_settings->priv->dc_stop)
		g_free (sim_settings->priv->dc_stop);
	sim_settings->priv->dc_stop = g_strdup (str);
}

void
sim_settings_set_dc_step  (SimSettings *sim_settings, gchar *str)
{
	if (sim_settings->priv->dc_step)
		g_free (sim_settings->priv->dc_step);
	sim_settings->priv->dc_step = g_strdup (str);	
}

void 
sim_settings_set_fourier (SimSettings *sim_settings, gboolean enable)
{
	sim_settings->priv->fourier_enable = enable;
}

void 
sim_settings_set_fourier_frequency (SimSettings *sim_settings,gchar *str)
{
	if (sim_settings->priv->fourier_frequency)
		g_free (sim_settings->priv->fourier_frequency);
	sim_settings->priv->fourier_frequency = g_strdup (str);	
}

void 
sim_settings_set_fourier_vout (SimSettings *sim_settings, 
    		gchar *str)
{
	gchar **node_ids=NULL;
	gint i;
	if (sim_settings->priv->fourier_vout)
		g_free (sim_settings->priv->fourier_vout);
	node_ids = g_strsplit (g_strdup (str), " ", 0);
	for (i=0; node_ids[i]!= NULL; i++) {
		if (node_ids[i]) 
			sim_settings->priv->fourier_vout = 
				g_slist_append (sim_settings->priv->fourier_vout, 
				               g_strdup (node_ids[i]));
	}
}


gboolean 
sim_settings_get_fourier (SimSettings *sim_settings)
{	
	return sim_settings->priv->fourier_enable;
}

gint 
sim_settings_get_fourier_frequency (SimSettings *sim_settings)
{	
	return atoi (sim_settings->priv->fourier_frequency);
}

gchar * 
sim_settings_get_fourier_vout (SimSettings *sim)
{
	GSList *node_slist;
	gchar *text = NULL;
	
	node_slist = g_slist_copy (sim->priv->fourier_vout);
	if (node_slist) text = g_strdup_printf ("%d", atoi (node_slist->data));
	if (node_slist) node_slist=node_slist->next;
	while (node_slist)
	{
		if (node_slist->data) 
			text = g_strdup_printf ("%s %d", text, atoi (node_slist->data));
		node_slist = node_slist->next;
	}
	g_slist_free (node_slist);
	return text;
}

gchar * 
sim_settings_get_fourier_nodes (SimSettings *sim)
{
	GSList *node_slist;
	gchar *text = NULL;
	
	node_slist = g_slist_copy (sim->priv->fourier_vout);
	if (node_slist->data) 
		text = g_strdup_printf ("V(%d)", atoi (node_slist->data));
	if (node_slist) node_slist=node_slist->next;
	while (node_slist)
	{
		if (node_slist->data) 
			text = g_strdup_printf ("%s V(%d)", text, atoi (node_slist->data));
		node_slist = node_slist->next;
	}
	g_slist_free (node_slist);
	return text;
}

static void
response_callback (GtkButton *button, Schematic *sm)
{
	gint page;
	SimSettings *s;
	gchar *tmp = NULL, **node_ids= NULL;

	s = schematic_get_sim_settings (sm);

	g_object_get (s->notebook, "page", &page, NULL);

	// Trans 
	s->priv->trans_enable = gtk_toggle_button_get_active (
	                         GTK_TOGGLE_BUTTON (s->priv->w_trans_enable));

	if (s->priv->trans_start) 
		g_free (s->priv->trans_start);
	s->priv->trans_start = gtk_editable_get_chars (
	                       GTK_EDITABLE (s->priv->w_trans_start), 0, -1);

	if (s->priv->trans_stop) 
		g_free (s->priv->trans_stop);
	s->priv->trans_stop = gtk_editable_get_chars (
	                       GTK_EDITABLE (s->priv->w_trans_stop), 0, -1);

	if (s->priv->trans_step) 
		g_free (s->priv->trans_step);
	s->priv->trans_step = gtk_editable_get_chars (
	                       GTK_EDITABLE (s->priv->w_trans_step), 0, -1);

	s->priv->trans_step_enable = gtk_toggle_button_get_active (
	                       GTK_TOGGLE_BUTTON (s->priv->w_trans_step_enable));

	s->priv->trans_init_cond = gtk_toggle_button_get_active (
	                       GTK_TOGGLE_BUTTON (s->priv->w_trans_init_cond));

	// DC 
	s->priv->dc_enable = gtk_toggle_button_get_active (
	                       GTK_TOGGLE_BUTTON (s->priv->w_dc_enable));
	if (s->priv->dc_vin) 
		g_free (s->priv->dc_vin);

	tmp = gtk_combo_box_text_get_active_text (
	                       GTK_COMBO_BOX_TEXT (s->priv->w_dc_vin));
	node_ids = g_strsplit (g_strdup (tmp), "V(", 0);
	tmp = node_ids[1];
	node_ids = g_strsplit (g_strdup (tmp), ")", 0);
	s->priv->dc_vin = node_ids[0];
	
	if (s->priv->dc_start) 
		g_free (s->priv->dc_start);
	s->priv->dc_start = g_strdup (gtk_entry_get_text (
	                       GTK_ENTRY (s->priv->w_dc_start)));						   

	if (s->priv->dc_stop) 
		g_free (s->priv->dc_stop);
	s->priv->dc_stop = g_strdup (gtk_entry_get_text (
	                       GTK_ENTRY (s->priv->w_dc_stop)));

	if (s->priv->dc_step) 
		g_free (s->priv->dc_step);
	s->priv->dc_step = g_strdup (gtk_entry_get_text (
	                       GTK_ENTRY (s->priv->w_dc_step)));

	// AC 
	s->priv->ac_enable = gtk_toggle_button_get_active (
	                       GTK_TOGGLE_BUTTON (s->priv->w_ac_enable));

	if (s->priv->ac_type) 
		g_free (s->priv->ac_type);
	s->priv->ac_type = gtk_combo_box_text_get_active_text (
	                         GTK_COMBO_BOX_TEXT (s->priv->w_ac_type));

	if (s->priv->ac_npoints) 
		g_free (s->priv->ac_npoints);
	s->priv->ac_npoints = g_strdup(gtk_entry_get_text (
	                       GTK_ENTRY (s->priv->w_ac_npoints)));

	if (s->priv->ac_start) 
		g_free (s->priv->ac_start);
	s->priv->ac_start = g_strdup (gtk_entry_get_text(
	                       GTK_ENTRY (s->priv->w_ac_start)));

	if (s->priv->ac_stop) 
		g_free (s->priv->ac_stop);
	s->priv->ac_stop = g_strdup (gtk_entry_get_text(
	                       GTK_ENTRY (s->priv->w_ac_stop)));

	// Fourier analysis 
	s->priv->fourier_enable = gtk_toggle_button_get_active (
	                       GTK_TOGGLE_BUTTON (s->priv->w_four_enable));

	if (s->priv->fourier_frequency) 
		g_free (s->priv->fourier_frequency);
	s->priv->fourier_frequency = g_strdup (gtk_entry_get_text (
	                       GTK_ENTRY (s->priv->w_four_freq)));

	gtk_container_resize_children (GTK_CONTAINER (s->notebook));

	// Options 
	get_options_from_list (s);
	gtk_widget_hide (GTK_WIDGET(s->pbox));
	s->pbox = NULL;
	s->notebook = NULL;

	// Schematic is dirty now ;-) 
	schematic_set_dirty (sm, TRUE);
	g_free (tmp);
	g_free (node_ids);
}

void
sim_settings_show (GtkWidget *widget, SchematicView *sv)
{
	int i;
	GtkWidget *toplevel, *w, *pbox, *combo_box;
	GtkTreeView *opt_list;
	GtkCellRenderer *cell_option, *cell_value;
	GtkTreeViewColumn *column_option, *column_value;
	GtkListStore *opt_model;
	GtkTreeIter iter;
	GtkBuilder *gui;
	GError *perror = NULL;
	gchar * msg;
	SimSettings *s;
	Schematic *sm;
	GList *list;
	GList *sources = NULL, *ltmp;
	GtkComboBox *node_box;
	GtkListStore *node_list_store;
	gchar *text = NULL;
	GSList * slist = NULL, *node_list = NULL;

	g_return_if_fail (sv != NULL);

	sm = schematic_view_get_schematic (sv);
	s = schematic_get_sim_settings (sm);

	// Only allow one instance of the property box per schematic. 
	if (s->pbox) {
		gdk_window_raise (gtk_widget_get_window (GTK_WIDGET (s->pbox)));
		return;
	}

	if (!g_file_test (OREGANO_UIDIR "/sim-settings.ui", G_FILE_TEST_EXISTS)) {
		gchar *msg;
		msg = g_strdup_printf (
			_("The file %s could not be found." 
			  "You might need to reinstall Oregano to fix this."),
			OREGANO_UIDIR "/sim-settings.ui");
		oregano_error_with_title (
		             _("Could not create simulation settings dialog"), msg);
		g_free (msg);
		return;
	}

	if ((gui = gtk_builder_new ()) == NULL) {
		oregano_error (_("Could not create simulation settings dialog"));
		return;
	} 
	else 
		gtk_builder_set_translation_domain (gui, NULL);

	if (gtk_builder_add_from_file (gui, OREGANO_UIDIR "/sim-settings.ui", 
	    &perror) <= 0) {
		msg = perror->message;
		oregano_error_with_title (
		              _("Could not create simulation settings dialog"), msg);
		g_error_free (perror);
		return;
	}

	toplevel = GTK_WIDGET (gtk_builder_get_object (gui, "toplevel"));
	if (!toplevel) {
		oregano_error (_("Could not create simulation settings dialog"));
		return;
	}

	pbox = toplevel;
	s->pbox = pbox;
	s->notebook = GTK_NOTEBOOK (gtk_builder_get_object (gui, "notebook"));
	g_signal_connect (G_OBJECT (pbox), "delete_event", 
		G_CALLBACK (delete_event_cb), s);

	//  Prepare options list   
	s->priv->w_opt_value = GTK_ENTRY (gtk_builder_get_object (gui, "opt_value"));
	opt_list = s->priv->w_opt_list  = GTK_TREE_VIEW (
	    gtk_builder_get_object (gui, "option_list"));

	//  Grab the frames 
	s->priv->w_trans_frame = GTK_WIDGET (gtk_builder_get_object (gui, 
	    "trans_frame"));
	s->priv->w_ac_frame = GTK_WIDGET (gtk_builder_get_object (gui, 
	    "ac_frame"));
	s->priv->w_dcsweep_frame = GTK_WIDGET (gtk_builder_get_object (gui, 
	    "dcsweep_frame"));
	s->priv->w_fourier_frame = GTK_WIDGET (gtk_builder_get_object (gui, 
	    "fourier_frame"));
	
	// Create the Columns 
	cell_option = gtk_cell_renderer_text_new ();
	cell_value = gtk_cell_renderer_text_new ();
	column_option = gtk_tree_view_column_new_with_attributes (N_("Option"),
		cell_option, "text", 0, NULL);
	column_value = gtk_tree_view_column_new_with_attributes (N_("Value"),
		cell_value, "text", 1, NULL);

	// Create the model 
	opt_model = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_STRING);
	gtk_tree_view_set_model (opt_list, GTK_TREE_MODEL (opt_model));
	gtk_tree_view_append_column (opt_list, column_option);
	gtk_tree_view_append_column (opt_list, column_value);

	if (s->priv->options == NULL) {
		// Load defaults 
		for (i = 0; default_options[i].name; i++) {
			gtk_list_store_append (opt_model, &iter);
			gtk_list_store_set (opt_model, &iter, 0, 
			                   default_options[i].name, -1);
		}
	} 
	else {
		// Load schematic options 
		list = s->priv->options;
		while (list) {
			SimOption *so = list->data;
			if (so) {
				gtk_list_store_append (opt_model, &iter);
				gtk_list_store_set (opt_model, &iter, 0, so->name, -1);
			}
			list = list->next;
		}
	}

	// Set the options already stored 
	list = s->priv->options;
	while (list) {
		SimOption *so = list->data;
		if (so)
			set_options_in_list (so->name, so->value, opt_list);
		list = list->next;
	}

	g_signal_connect (G_OBJECT (opt_list), "button_release_event", 
		G_CALLBACK (select_opt_callback), s);

	w = GTK_WIDGET (gtk_builder_get_object (gui, "opt_accept"));
	g_signal_connect (G_OBJECT (w), "clicked", 
		G_CALLBACK (option_setvalue), s);
	w = GTK_WIDGET (gtk_builder_get_object (gui, "opt_remove"));
	g_signal_connect (G_OBJECT (w), "clicked", 
	   	G_CALLBACK (option_remove), s);
	w = GTK_WIDGET (gtk_builder_get_object (gui, "add_option"));
	g_signal_connect (G_OBJECT (w), "clicked", 
	    G_CALLBACK (add_option), s);

	// Creation of Close Button 
	w = GTK_WIDGET (gtk_builder_get_object (gui, "button1"));
	g_signal_connect (G_OBJECT (w), "clicked", 
		G_CALLBACK (response_callback), sm);

	// Transient //
	// ********* //
	w = GTK_WIDGET (gtk_builder_get_object (gui, "trans_start"));
	if (s->priv->trans_start) gtk_entry_set_text (GTK_ENTRY (w),
		s->priv->trans_start);
	s->priv->w_trans_start = w;
	g_signal_connect(G_OBJECT (w), "changed", 
		G_CALLBACK (entry_changed_cb), s);

	w = GTK_WIDGET (gtk_builder_get_object (gui, "trans_stop"));
	if (s->priv->trans_stop)
		gtk_entry_set_text (GTK_ENTRY (w), s->priv->trans_stop);
	s->priv->w_trans_stop = w;
	g_signal_connect(G_OBJECT (w), "changed", 
		G_CALLBACK (entry_changed_cb), s);

	w = GTK_WIDGET (gtk_builder_get_object (gui, "trans_step"));
	if (s->priv->trans_step)
		gtk_entry_set_text (GTK_ENTRY (w), s->priv->trans_step);
	s->priv->w_trans_step = w;
	g_signal_connect(G_OBJECT (w), "changed", 
		G_CALLBACK (entry_changed_cb), s);

	w = GTK_WIDGET (gtk_builder_get_object (gui, "trans_enable"));
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w), s->priv->trans_enable);
	s->priv->w_trans_enable = w;
	g_signal_connect (G_OBJECT (s->priv->w_trans_enable), "clicked", 
		G_CALLBACK (trans_enable_cb), s);

	w = GTK_WIDGET (gtk_builder_get_object (gui, "trans_step_enable"));
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w),
		s->priv->trans_step_enable);
	s->priv->w_trans_step_enable = w;
	g_signal_connect (G_OBJECT (s->priv->w_trans_step_enable), "clicked", 
		G_CALLBACK (trans_step_enable_cb), s);

	w = GTK_WIDGET (gtk_builder_get_object (gui, "trans_init_cond"));
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w),
		s->priv->trans_init_cond);
	s->priv->w_trans_init_cond = w;

	g_signal_connect (G_OBJECT (s->priv->w_trans_enable), "clicked", 
		G_CALLBACK (trans_enable_cb), s);

	g_signal_connect (G_OBJECT (s->priv->w_trans_step_enable), "clicked", 
		G_CALLBACK (trans_step_enable_cb), s);

	// AC  //
	// *** //
	w = GTK_WIDGET (gtk_builder_get_object (gui, "ac_enable"));
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w), s->priv->ac_enable);
	s->priv->w_ac_enable=w;
	g_signal_connect (G_OBJECT (w), "clicked", 
		G_CALLBACK (ac_enable_cb), s);

	// Initialilisation of the various AC types
	w =  GTK_WIDGET (gtk_builder_get_object (gui, "ac_type"));
	gtk_widget_destroy (w);
	w = GTK_WIDGET (gtk_builder_get_object (gui, "table14"));
	combo_box = gtk_combo_box_text_new ();
	gtk_table_attach (GTK_TABLE (w),combo_box, 1, 2, 0, 1,
	                  GTK_EXPAND | GTK_FILL, 
	                  GTK_SHRINK, 
	                  0, 0);
	s->priv->w_ac_type = combo_box;

	{
		gint index = 0;
		for (index = 0; AC_types_list[index]!= NULL ; index++) {
			gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo_box), 
			                           AC_types_list[index]);
		}
		gtk_combo_box_set_active (GTK_COMBO_BOX (combo_box),0);
	}
	g_signal_connect (G_OBJECT (GTK_COMBO_BOX (combo_box)), "changed",
		G_CALLBACK (entry_changed_cb), s);

	w = GTK_WIDGET (gtk_builder_get_object (gui, "ac_npoints"));
	gtk_entry_set_text (GTK_ENTRY (w), s->priv->ac_npoints);
	s->priv->w_ac_npoints = w;
	g_signal_connect (G_OBJECT (w), "changed", 
		G_CALLBACK (entry_changed_cb), s);

	w = GTK_WIDGET (gtk_builder_get_object (gui, "ac_start"));
	gtk_entry_set_text (GTK_ENTRY (w), s->priv->ac_start);
	s->priv->w_ac_start = w;
	g_signal_connect (G_OBJECT (w), "changed", 
		G_CALLBACK (entry_changed_cb), s);

	w = GTK_WIDGET (gtk_builder_get_object (gui, "ac_stop"));
	gtk_entry_set_text(GTK_ENTRY (w), s->priv->ac_stop);
	s->priv->w_ac_stop = w;
	g_signal_connect (G_OBJECT (w), "changed", 
		G_CALLBACK (entry_changed_cb), s);

	//  DC   //
	// ***** //
	w = GTK_WIDGET (gtk_builder_get_object (gui, "dc_enable"));
	s->priv->w_dc_enable = w;
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w), s->priv->dc_enable);
	g_signal_connect (G_OBJECT (w), "clicked", 
		G_CALLBACK (dc_enable_cb), s);
	
	//  Get list of sources
	node_list = netlist_helper_get_voltmeters_list (
	                    schematic_view_get_schematic (sv), &perror);
	if (perror != NULL) {
		msg = perror->message;
		oregano_error_with_title (_("Could not create a netlist"), msg);
		g_error_free (perror);
		return;
	}
	if (!node_list) {
		oregano_error (_("No node in the schematic!"));
		return;
	}
	for (; node_list; node_list = node_list->next) {
		gchar * tmp;
		tmp = g_strdup_printf ("V(%d)", atoi (node_list->data));
	   	sources = g_list_prepend (sources, tmp);
	}
	w = GTK_WIDGET (gtk_builder_get_object (gui, "dc_vin1"));
	gtk_widget_destroy (w);
	w = GTK_WIDGET (gtk_builder_get_object (gui, "table13"));
	combo_box = gtk_combo_box_text_new ();
	
	gtk_table_attach (GTK_TABLE (w),combo_box, 1, 2, 0, 1,
	                  GTK_EXPAND | GTK_FILL, 
	                  GTK_SHRINK, 
	                  0, 0);
	s->priv->w_dc_vin = combo_box;
	if (sources) {
		for (; sources; sources = sources->next) {
			gtk_combo_box_text_append_text (
			             GTK_COMBO_BOX_TEXT (combo_box), sources->data);
		}
		gtk_combo_box_set_active (GTK_COMBO_BOX (combo_box),0);		
	}
	g_signal_connect (G_OBJECT (combo_box), "changed", 
		G_CALLBACK (entry_changed_cb), s);
	
	w = GTK_WIDGET (gtk_builder_get_object (gui, "dc_start1"));
	s->priv->w_dc_start = w;
	gtk_entry_set_text (GTK_ENTRY (w), s->priv->dc_start);
	g_signal_connect (G_OBJECT (w), "changed", 
		G_CALLBACK (entry_changed_cb), s);

	w = GTK_WIDGET (gtk_builder_get_object (gui, "dc_stop1"));
	s->priv->w_dc_stop = w;
	gtk_entry_set_text (GTK_ENTRY (w), s->priv->dc_stop);
	g_signal_connect (G_OBJECT (w), "changed", 
		G_CALLBACK (entry_changed_cb), s);

	w = GTK_WIDGET (gtk_builder_get_object (gui, "dc_step1"));
	s->priv->w_dc_step = w;
	gtk_entry_set_text (GTK_ENTRY (w), s->priv->dc_step);
	g_signal_connect (G_OBJECT (w), "changed", 
		G_CALLBACK (entry_changed_cb), s);

	for (ltmp = sources; ltmp; ltmp = ltmp->next) g_free (ltmp->data);
	g_list_free (sources);

	// Fourier //
	// ******* //
	w = GTK_WIDGET (gtk_builder_get_object (gui, "fourier_enable"));
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w), 
	                         s->priv->fourier_enable);
	s->priv->w_four_enable = w;
	g_signal_connect (G_OBJECT(w), "clicked", 
		G_CALLBACK (fourier_enable_cb), s);

	w = GTK_WIDGET (gtk_builder_get_object (gui, "fourier_freq"));
	s->priv->w_four_freq = w;
	g_signal_connect (G_OBJECT(w), "changed", 
		G_CALLBACK (entry_changed_cb), s);
	gtk_entry_set_text (GTK_ENTRY (w), s->priv->fourier_frequency);
	
	w = GTK_WIDGET (gtk_builder_get_object (gui, "fourier_vout"));
	s->priv->w_four_vout = w;
	g_signal_connect (G_OBJECT (w), "changed", 
		G_CALLBACK (entry_changed_cb), s);

	text = NULL;
	slist = g_slist_copy (s->priv->fourier_vout);
	if (slist) {
		if (atoi (slist->data) != 0)
			text = g_strdup_printf ("V(%d)", atoi (slist->data));
	
		slist = slist->next;
		while (slist)
		{
			if (atoi (slist->data) != 0)
				text = g_strdup_printf ("%s V(%d)", text, atoi (slist->data));
			slist = slist->next;
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
	w = GTK_WIDGET (gtk_builder_get_object (gui, "fourier_select_out"));
	gtk_widget_destroy (w);

	w = GTK_WIDGET (gtk_builder_get_object (gui, "table12"));
	combo_box = gtk_combo_box_text_new ();
	
	gtk_table_attach (GTK_TABLE (w),combo_box, 2, 3, 2, 3,
	                  GTK_EXPAND | GTK_FILL, 
	                  GTK_SHRINK, 
	                  0, 0);
	
	s->priv->w_four_combobox = combo_box;
	node_box = GTK_COMBO_BOX (combo_box);	
	node_list_store = GTK_LIST_STORE (gtk_combo_box_get_model (node_box));
	gtk_list_store_clear (node_list_store);	

	// Get the identification of the schematic nodes
	node_list = netlist_helper_get_voltmeters_list (
	                             schematic_view_get_schematic (sv), &perror);
	if (perror != NULL) {
		msg = perror->message;
		oregano_error_with_title (_("Could not create a netlist"), msg);
		g_error_free (perror);
		return;
	}
	if (!node_list) 
		oregano_error (_("No node in the schematic!"));

	text = NULL;
	while (node_list) {
		if (node_list->data)
			text = g_strdup_printf ("V(%d)", atoi (node_list->data));
		if (text) gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (node_box), text);
		node_list = node_list->next;
	}
	gtk_combo_box_set_active (node_box, 0);
	w = GTK_WIDGET (gtk_builder_get_object (gui, "fourier_add"));
	s->priv->w_four_add = w;
	g_signal_connect (G_OBJECT (w), "clicked", 
		G_CALLBACK (fourier_add_vout_cb), s);
	w = GTK_WIDGET (gtk_builder_get_object (gui, "fourier_rem"));
	s->priv->w_four_rem = w;
	g_signal_connect (G_OBJECT (w), "clicked", 
		G_CALLBACK (fourier_remove_vout_cb), s);
	
	gtk_widget_show_all (toplevel);

	ac_enable_cb (s->priv->w_ac_enable,s);
	fourier_enable_cb (s->priv->w_four_enable, s);
	dc_enable_cb (s->priv->w_dc_enable,s);
	trans_enable_cb (s->priv->w_trans_enable, s);
	trans_step_enable_cb (s->priv->w_trans_step_enable, s);

	// Cleanup
	g_list_free_full (list, g_object_unref);
}

GList *
sim_settings_get_options (SimSettings *s)
{
	return s->priv->options;
}

void
sim_settings_add_option (SimSettings *s, SimOption *opt)
{
	GList *list=s->priv->options;

	// Remove the option if already in the list. 
	while (list) {
		SimOption *so=list->data;
		if (so && !strcmp (opt->name,so->name)) {
			g_free (so->name);
			g_free (so->value);
			s->priv->options = g_list_remove (s->priv->options, so);
			g_free (so);
		}
		list = list->next;
	}
	s->priv->options = g_list_append (s->priv->options, opt);
	g_list_free_full (list, g_object_unref);
}
