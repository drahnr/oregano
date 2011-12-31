/*
 * sim-settings.c
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

#include <unistd.h>
#include <ctype.h>
#include <gnome.h>
#include <glade/glade.h>
#include <gtk/gtkclist.h>
#include <gtk/gtkentry.h>
#include "main.h"
#include "sim-settings.h"
#include "schematic.h"
#include "schematic-view.h"
#include "dialogs.h"
#include "oregano-utils.h"

struct _SimSettingsPriv {
	/* Transient analysis. */
	GtkWidget *w_main;
	GtkWidget *w_trans_enable, *w_trans_start, *w_trans_stop;
	GtkWidget *w_trans_step, *w_trans_step_enable, *w_trans_init_cond, *w_trans_frame;
	gboolean trans_enable;
	gboolean trans_init_cond;
	gchar *trans_start;
	gchar *trans_stop;
	gchar *trans_step;
	gboolean trans_step_enable;

	/*  AC   */
	GtkWidget *w_ac_enable, *w_ac_type, *w_ac_npoints, *w_ac_start, *w_ac_stop, *w_ac_frame;
	gboolean ac_enable;
	gchar   *ac_type;
	gchar   *ac_npoints;
	gchar   *ac_start;
	gchar   *ac_stop;

	/*  DC  */
	GtkWidget *w_dc_enable,*w_dc_vin1,*w_dc_start1,*w_dc_stop1,*w_dc_step1,
		*w_dc_vin2,*w_dc_start2,*w_dc_stop2,*w_dc_step2,*w_dcsweep_frame;
	gboolean dc_enable;
	gchar   *dc_vin1;
	gchar   *dc_start1,*dc_stop1,*dc_step1;
	gchar   *dc_vin2;
	gchar   *dc_start2,*dc_stop2,*dc_step2;

	/* Fourier analysis. Replace with something sane later. */
	GtkWidget *w_four_enable,*w_four_freq,*w_four_vout,*w_four_combo,*w_four_add,*w_fourier_frame;
	gboolean four_enable;
	gchar *four_freq;
	gchar *four_vout;

	/*  Options   */
	GtkEntry *w_opt_value;
	GtkTreeView  *w_opt_list;
	GList *options;
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


static void
set_options_in_list(gchar *key, gchar *val, GtkTreeView *cl)
{
	int i;
	GtkTreeModel *model;
	GtkTreeIter iter;
	char *name;
	char *value;

	model = gtk_tree_view_get_model(cl);

	i=0;
	while (gtk_tree_model_iter_nth_child(model, &iter, NULL, i)) {
		gtk_tree_model_get(model, &iter, 0, &name, 1, &value, -1);
		if (!strcmp (name, key) ) {
			gtk_list_store_set(GTK_LIST_STORE(model), &iter, 1, val, -1);
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

	/*  Empty the list   */
	while (s->priv->options) {
		so = s->priv->options->data;
		if (so) {
			/* Prevent sigfault on NULL */
			if (so->name) g_free (so->name);
			if (so->value) g_free (so->value);
			s->priv->options = g_list_remove (s->priv->options,so);
			g_free (so);
		}
		if (s->priv->options) s->priv->options = s->priv->options->next;
	}

	model = gtk_tree_view_get_model(cl);
	i = 0;
	while (gtk_tree_model_iter_nth_child(model, &iter, NULL, i)) {
		SimOption *so;
		gtk_tree_model_get(model, &iter, 0, &name, 1, &value, -1);

		so = g_new0 (SimOption,1);
		so->name = g_strdup(name);
		so->value = g_strdup(value);
		s->priv->options = g_list_append (s->priv->options, so);
		/* TODO : name y value deben ser liberados ? */
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

	/* Get the current selected row */
	selection = gtk_tree_view_get_selection(sim->priv->w_opt_list);
	model = gtk_tree_view_get_model(sim->priv->w_opt_list);

	if (!gtk_tree_selection_get_selected(selection, NULL, &iter)) {
		return FALSE;
	}

	gtk_tree_model_get(model, &iter, 1, &value, -1);

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

	/* Get the current selected row */
	selection = gtk_tree_view_get_selection(s->priv->w_opt_list);
	model = gtk_tree_view_get_model(s->priv->w_opt_list);

	if (!gtk_tree_selection_get_selected(selection, NULL, &iter)) {
		return;
	}

	gtk_list_store_set(GTK_LIST_STORE(model), &iter, 1, value, -1);

//	gnome_property_box_set_modified (GNOME_PROPERTY_BOX (s->pbox), TRUE);
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
	gtk_container_add (GTK_CONTAINER (GTK_DIALOG(dialog)->vbox), GTK_WIDGET (entry));
	gtk_widget_show (GTK_WIDGET (entry));

	if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_OK) {
		GtkTreeIter iter;
		SimOption *opt = g_new0 (SimOption, 1);
		opt->name = g_strdup (gtk_entry_get_text (entry));
		opt->value = g_strdup ("");
		/* Warning : don't free opt later, is added to the list */
		sim_settings_add_option (s, opt);
	
		gtk_list_store_append (GTK_LIST_STORE (gtk_tree_view_get_model(s->priv->w_opt_list)), &iter);
		gtk_list_store_set (GTK_LIST_STORE (gtk_tree_view_get_model(s->priv->w_opt_list)), &iter, 0, opt->name, -1);
	}

	gtk_widget_destroy (dialog);
}


static void
option_remove (GtkWidget *w, SimSettings *s)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	GtkTreeSelection *selection;

	/* Get the current selected row */
	selection = gtk_tree_view_get_selection(s->priv->w_opt_list);
	model = gtk_tree_view_get_model(s->priv->w_opt_list);

	if (!gtk_tree_selection_get_selected(selection, NULL, &iter)) {
		return;
	}

	gtk_list_store_set(GTK_LIST_STORE(model), &iter, 1, "", -1);
}

static void
trans_enable_cb (GtkWidget *widget, SimSettings *s)
{
	gboolean enable, step_enable;

	enable = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
	step_enable = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (s->priv->w_trans_step_enable));
	
	if (enable)
		gtk_widget_show (s->priv->w_trans_frame);
	else
		gtk_widget_hide (s->priv->w_trans_frame);
	/*	
	gtk_widget_set_sensitive (s->priv->w_trans_start, enable);
	gtk_widget_set_sensitive (s->priv->w_trans_stop, enable);
	gtk_widget_set_sensitive (s->priv->w_trans_step, enable & step_enable);
	gtk_widget_set_sensitive (s->priv->w_trans_step_enable, enable);
	gtk_widget_set_sensitive (s->priv->w_trans_init_cond, enable);
	gnome_property_box_changed (GNOME_PROPERTY_BOX (s->pbox));
	*/
}

static void
trans_step_enable_cb (GtkWidget *widget, SimSettings *s)
{
	gboolean enable, step_enable;

	step_enable = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
	enable = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (s->priv->w_trans_enable));

	gtk_widget_set_sensitive (s->priv->w_trans_step, step_enable & enable);

//	gnome_property_box_changed (GNOME_PROPERTY_BOX (s->pbox));
}

static void
entry_changed_cb (GtkWidget *widget, SimSettings *s)
{
//	gnome_property_box_changed (GNOME_PROPERTY_BOX (s->pbox));
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
/*	gtk_widget_set_sensitive (s->priv->w_ac_type,enable);
	gtk_widget_set_sensitive (s->priv->w_ac_npoints,enable);
	gtk_widget_set_sensitive (s->priv->w_ac_start,enable);
	gtk_widget_set_sensitive (s->priv->w_ac_stop,enable);

	gnome_property_box_changed (GNOME_PROPERTY_BOX (s->pbox)); */
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
	/*
	gtk_widget_set_sensitive (s->priv->w_dc_vin1,enable);
	gtk_widget_set_sensitive (s->priv->w_dc_start1,enable);
	gtk_widget_set_sensitive (s->priv->w_dc_stop1,enable);
	gtk_widget_set_sensitive (s->priv->w_dc_step1,enable);
	gtk_widget_set_sensitive (s->priv->w_dc_vin2,enable);
	gtk_widget_set_sensitive (s->priv->w_dc_start2,enable);
	gtk_widget_set_sensitive (s->priv->w_dc_stop2,enable);
	gtk_widget_set_sensitive (s->priv->w_dc_step2,enable);
	gnome_property_box_changed (GNOME_PROPERTY_BOX (s->pbox)); */
}

static void
four_enable_cb (GtkWidget *widget, SimSettings *s)
{
	gboolean enable;
	enable = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
	
	if (enable)
		gtk_widget_show (s->priv->w_fourier_frame);
	else
		gtk_widget_hide (s->priv->w_fourier_frame);
	/*
	gtk_widget_set_sensitive (s->priv->w_four_freq,enable);
	gtk_widget_set_sensitive (s->priv->w_four_vout,enable);
	gtk_widget_set_sensitive (s->priv->w_four_combo,enable);
	gtk_widget_set_sensitive (s->priv->w_four_add,enable);

	gnome_property_box_changed (GNOME_PROPERTY_BOX (s->pbox));*/
}

static void
four_add_vout_cb (GtkWidget *widget, SimSettings *s)
{

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

	/*
	 * Set some default settings.
	 */
	/*  transient   */
	s->priv->trans_enable = TRUE;
	s->priv->trans_start = g_strdup ("0 s");
	s->priv->trans_stop = g_strdup ("5 ms");
	s->priv->trans_step = g_strdup ("0.1 ms");
	s->priv->trans_step_enable = FALSE;

	/*  ac   */
	s->priv->ac_enable = FALSE;
	s->priv->ac_type   = g_strdup ("DEC");
	s->priv->ac_npoints= g_strdup ("50");
	s->priv->ac_start  = g_strdup ("1");
	s->priv->ac_stop   = g_strdup ("1 Meg");

	/*  dc   */
	s->priv->dc_enable = FALSE;
	s->priv->dc_vin1   = g_strdup ("");
	s->priv->dc_start1 = g_strdup ("");
	s->priv->dc_stop1  = g_strdup ("");
	s->priv->dc_step1  = g_strdup ("");
	s->priv->dc_vin2   = g_strdup ("");
	s->priv->dc_start2 = g_strdup ("");
	s->priv->dc_stop2  = g_strdup ("");
	s->priv->dc_step2  = g_strdup ("");

	/*   fourier   */
	s->priv->four_enable = FALSE;

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
sim_settings_set_trans_init_cond(SimSettings *sim_settings, gboolean enable)
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
sim_settings_set_ac_type(SimSettings *sim_settings,gchar *str)
{
	if ( sim_settings->priv->ac_type )
		g_free ( sim_settings->priv->ac_type );
	sim_settings->priv->ac_type = g_strdup (str);
}

void
sim_settings_set_ac_npoints(SimSettings *sim_settings,gchar *str)
{
	if ( sim_settings->priv->ac_npoints )
		g_free ( sim_settings->priv->ac_npoints );
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
sim_settings_set_ac_stop(SimSettings *sim_settings,gchar *str)
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
sim_settings_get_dc_vsrc (SimSettings *sim_settings,gint i)
{
	gchar *tmp = (i==0 ? sim_settings->priv->dc_vin1 : sim_settings->priv->dc_vin2);
	return (tmp && *tmp ? tmp : NULL);
}

gdouble
sim_settings_get_dc_start (SimSettings *sim_settings, gint i)
{
	return oregano_strtod ( i==0
				? sim_settings->priv->dc_start1
				: sim_settings->priv->dc_start2, 's');
}

gdouble
sim_settings_get_dc_stop (SimSettings *sim_settings,gint i)
{
	return oregano_strtod ( i==0
				? sim_settings->priv->dc_stop1
				: sim_settings->priv->dc_stop2, 's');
}

gdouble
sim_settings_get_dc_step  (SimSettings *sim_settings,gint i)
{
	return oregano_strtod ( i==0
				? sim_settings->priv->dc_step1
				: sim_settings->priv->dc_step2, 's');
}

void
sim_settings_set_dc (SimSettings *sim_settings, gboolean enable)
{
	sim_settings->priv->dc_enable = enable;
}

void
sim_settings_set_dc_vsrc (SimSettings *sim_settings, gint i, gchar *str)
{
	gchar **val = (i==0
				   ? &sim_settings->priv->dc_vin1
				   : &sim_settings->priv->dc_vin2);
	if ( *val ) g_free (*val);
	*val = g_strdup (str);
}

void
sim_settings_set_dc_start (SimSettings *sim_settings, gint i, gchar *str) {
	gchar **val = (i==0
				   ? &sim_settings->priv->dc_start1
				   : &sim_settings->priv->dc_start2);
	if ( *val ) g_free (*val);
	*val = g_strdup (str);
}

void
sim_settings_set_dc_stop  (SimSettings *sim_settings, gint i, gchar *str)
{
	gchar **val = (i==0
				   ? &sim_settings->priv->dc_stop1
				   : &sim_settings->priv->dc_stop2);
	if (*val) g_free (*val);
	*val = g_strdup (str);
}

void
sim_settings_set_dc_step  (SimSettings *sim_settings, gint i, gchar *str)
{
	gchar **val = (i==0
				   ? &sim_settings->priv->dc_step1
				   : &sim_settings->priv->dc_step2);
	if (*val) g_free (*val);
	*val = g_strdup (str);
}

static void
response_callback(GtkDialog *dlg, gint id, Schematic *sm)
{
	gint page;
	SimSettings *s;
	
	s = schematic_get_sim_settings (sm);

	g_object_get(s->notebook, "page", &page, NULL);

	/* Trans */
	s->priv->trans_enable = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (s->priv->w_trans_enable));

	if (s->priv->trans_start) g_free (s->priv->trans_start);
	s->priv->trans_start = gtk_editable_get_chars (GTK_EDITABLE (s->priv->w_trans_start), 0, -1);

	if (s->priv->trans_stop) g_free (s->priv->trans_stop);
	s->priv->trans_stop = gtk_editable_get_chars (GTK_EDITABLE (s->priv->w_trans_stop), 0, -1);

	if (s->priv->trans_step) g_free (s->priv->trans_step);
	s->priv->trans_step = gtk_editable_get_chars (GTK_EDITABLE (s->priv->w_trans_step), 0, -1);

	s->priv->trans_step_enable = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (s->priv->w_trans_step_enable));

	s->priv->trans_init_cond = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (s->priv->w_trans_init_cond));

	/* DC */
	s->priv->dc_enable = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (s->priv->w_dc_enable));
	if (s->priv->dc_vin1) g_free (s->priv->dc_vin1);
	s->priv->dc_vin1 = g_strdup (gtk_entry_get_text(GTK_ENTRY(GTK_COMBO (s->priv->w_dc_vin1)->entry)));

	if (s->priv->dc_start1) g_free (s->priv->dc_start1);
	s->priv->dc_start1 = g_strdup (gtk_entry_get_text (GTK_ENTRY (s->priv->w_dc_start1)));

	if (s->priv->dc_stop1) g_free(s->priv->dc_stop1);
	s->priv->dc_stop1 = g_strdup(gtk_entry_get_text (GTK_ENTRY (s->priv->w_dc_stop1)));

	if (s->priv->dc_step1) g_free(s->priv->dc_step1);
	s->priv->dc_step1 = g_strdup(gtk_entry_get_text(GTK_ENTRY (s->priv->w_dc_step1)));

	if (s->priv->dc_vin2) g_free(s->priv->dc_vin2);
	s->priv->dc_vin2 = g_strdup(gtk_entry_get_text(GTK_ENTRY(GTK_COMBO (s->priv->w_dc_vin2)->entry)));

	if (s->priv->dc_start2) g_free(s->priv->dc_start2);
	s->priv->dc_start2 = g_strdup(gtk_entry_get_text(GTK_ENTRY (s->priv->w_dc_start2)));

	if (s->priv->dc_stop2) g_free(s->priv->dc_stop2);
	s->priv->dc_stop2 = g_strdup(gtk_entry_get_text(GTK_ENTRY (s->priv->w_dc_stop2)));

	if (s->priv->dc_step2) g_free(s->priv->dc_step2);
	s->priv->dc_step2 = g_strdup(gtk_entry_get_text(GTK_ENTRY (s->priv->w_dc_step2)));

	/* AC */
	s->priv->ac_enable = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON (s->priv->w_ac_enable));

	if (s->priv->ac_type) g_free(s->priv->ac_type);
	s->priv->ac_type = g_strdup(gtk_entry_get_text(GTK_ENTRY( s->priv->w_ac_type)));

	if (s->priv->ac_npoints) g_free (s->priv->ac_npoints);
	s->priv->ac_npoints = g_strdup(gtk_entry_get_text(GTK_ENTRY( s->priv->w_ac_npoints)));

	if (s->priv->ac_start) g_free (s->priv->ac_start);
	s->priv->ac_start = g_strdup(gtk_entry_get_text(GTK_ENTRY( s->priv->w_ac_start)));

	if (s->priv->ac_stop) g_free (s->priv->ac_stop);
	s->priv->ac_stop = g_strdup(gtk_entry_get_text(GTK_ENTRY( s->priv->w_ac_stop)));

	/* Options */
	get_options_from_list (s);
	gtk_widget_hide(GTK_WIDGET(dlg));
	gtk_widget_destroy(GTK_WIDGET(dlg));
	s->pbox = NULL;
	s->notebook = NULL;

	/* Schematic is dirty now ;-) */
	schematic_set_dirty (sm, TRUE);
}

void
sim_settings_show (GtkWidget *widget, SchematicView *sv)
{
	int i;
	GtkWidget *toplevel, *w, *pbox;
	GtkTreeView *opt_list;
	GtkCellRenderer *cell_option, *cell_value;
	GtkTreeViewColumn *column_option, *column_value;
	GtkListStore *opt_model;
	GtkTreeIter iter;
	GladeXML *gui;
	SimSettings *s;
	Schematic *sm;
	GList *list;
	GList *items,*sources=NULL,*ltmp;

	g_return_if_fail (sv != NULL);

	sm = schematic_view_get_schematic (sv);
	s = schematic_get_sim_settings (sm);

	/* Only allow one instance of the property box per schematic. */
	if (s->pbox){
		gdk_window_raise (GTK_WIDGET (s->pbox)->window);
		return;
	}

	if (!g_file_test (OREGANO_GLADEDIR "/sim-settings.glade", G_FILE_TEST_EXISTS)) {
		gchar *msg;
		msg = g_strdup_printf (
			_("The file %s could not be found. You might need to reinstall Oregano to fix this."),
			OREGANO_GLADEDIR "/sim-settings.glade");
		oregano_error_with_title (_("Could not create simulation settings dialog"), msg);
		g_free (msg);
		return;
	}

	gui = glade_xml_new (OREGANO_GLADEDIR "/sim-settings.glade", "toplevel", NULL);
	if (!gui) {
		oregano_error (_("Could not create simulation settings dialog"));
		return;
	}

	toplevel = glade_xml_get_widget (gui, "toplevel");
	if (!toplevel) {
		oregano_error (_("Could not create simulation settings dialog"));
		return;
	}

	pbox = toplevel;
	s->pbox = GTK_WIDGET (pbox);
	s->notebook = GTK_NOTEBOOK (glade_xml_get_widget (gui, "notebook"));
	g_signal_connect (G_OBJECT (pbox),
		"delete_event", G_CALLBACK(delete_event_cb), s);

	/*  Prepare options list   */
	s->priv->w_opt_value = GTK_ENTRY (glade_xml_get_widget (gui, "opt_value"));
	opt_list = s->priv->w_opt_list  = GTK_TREE_VIEW(glade_xml_get_widget (gui,
		"option_list"));

	/*  Grab the frames */
	s->priv->w_trans_frame = GTK_WIDGET (glade_xml_get_widget (gui, "trans_frame"));
	s->priv->w_ac_frame = GTK_WIDGET (glade_xml_get_widget (gui, "ac_frame"));
	s->priv->w_dcsweep_frame = GTK_WIDGET (glade_xml_get_widget (gui, "dcsweep_frame"));
	s->priv->w_fourier_frame = GTK_WIDGET (glade_xml_get_widget (gui, "fourier_frame"));
	
	/* Create the Columns */
	cell_option = gtk_cell_renderer_text_new();
	cell_value = gtk_cell_renderer_text_new();
	column_option = gtk_tree_view_column_new_with_attributes(N_("Option"),
		cell_option, "text", 0, NULL);
	column_value = gtk_tree_view_column_new_with_attributes(N_("Value"),
		cell_value, "text", 1, NULL);

	/* Create the model */
	opt_model = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_STRING);
	gtk_tree_view_set_model(opt_list, GTK_TREE_MODEL(opt_model));
	gtk_tree_view_append_column(opt_list, column_option);
	gtk_tree_view_append_column(opt_list, column_value);

	if (s->priv->options == NULL) {
		/* Load defaults */
		for (i = 0; default_options[i].name; i++) {
			gtk_list_store_append(opt_model, &iter);
			gtk_list_store_set(opt_model, &iter, 0, default_options[i].name, -1);
		}
	} else {
		/* Load schematic options */
		list = s->priv->options;
		while (list) {
			SimOption *so = list->data;
			if (so) {
				gtk_list_store_append(opt_model, &iter);
				gtk_list_store_set(opt_model, &iter, 0, so->name, -1);
			}
			list = list->next;
		}
	}

	/* Set the optinos already stored */
	list = s->priv->options;
	while (list) {
		SimOption *so = list->data;
		if (so)
			set_options_in_list (so->name, so->value, opt_list);
		list = list->next;
	}

	g_signal_connect(G_OBJECT (opt_list),
		"button_release_event", G_CALLBACK(select_opt_callback), s);

	w = glade_xml_get_widget (gui, "opt_accept");
	g_signal_connect (G_OBJECT(w),"clicked", G_CALLBACK(option_setvalue), s);
	w = glade_xml_get_widget (gui, "opt_remove");
	g_signal_connect (G_OBJECT(w),"clicked", G_CALLBACK(option_remove), s);
	w = glade_xml_get_widget (gui, "add_option");
	g_signal_connect (G_OBJECT(w),"clicked", G_CALLBACK(add_option), s);

	/* Setup callbacks. */
	g_signal_connect (G_OBJECT (pbox),
		"response", G_CALLBACK(response_callback), sm);

	/*  Transient   */
	w = glade_xml_get_widget (gui, "trans_start");
	if (s->priv->trans_start) gtk_entry_set_text (GTK_ENTRY (w),
		s->priv->trans_start);
	s->priv->w_trans_start = w;
	g_signal_connect(G_OBJECT (w),
		"changed", G_CALLBACK(entry_changed_cb), s);

	w = glade_xml_get_widget (gui, "trans_stop");
	if (s->priv->trans_stop)
		gtk_entry_set_text (GTK_ENTRY (w), s->priv->trans_stop);
	s->priv->w_trans_stop = w;
	g_signal_connect(G_OBJECT (w), "changed", G_CALLBACK(entry_changed_cb), s);

	w = glade_xml_get_widget (gui, "trans_step");
	if (s->priv->trans_step)
		gtk_entry_set_text (GTK_ENTRY (w), s->priv->trans_step);
	s->priv->w_trans_step = w;
	g_signal_connect(G_OBJECT (w), "changed", G_CALLBACK(entry_changed_cb), s);

	w = glade_xml_get_widget (gui, "trans_enable");
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w), s->priv->trans_enable);
	s->priv->w_trans_enable = w;

	w = glade_xml_get_widget (gui, "trans_step_enable");
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w),
		s->priv->trans_step_enable);
	s->priv->w_trans_step_enable = w;

	w = glade_xml_get_widget (gui, "trans_init_cond");
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w),
		s->priv->trans_init_cond);
	s->priv->w_trans_init_cond = w;

	g_signal_connect(G_OBJECT(s->priv->w_trans_enable),
		"clicked", G_CALLBACK(trans_enable_cb), s);

	g_signal_connect(G_OBJECT(s->priv->w_trans_step_enable),
		"clicked", G_CALLBACK(trans_step_enable_cb), s);

	/* AC  */
	w = glade_xml_get_widget (gui, "ac_enable");
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w), s->priv->ac_enable);
	s->priv->w_ac_enable=w;
	g_signal_connect (G_OBJECT (w), "clicked", G_CALLBACK(ac_enable_cb), s);

	w =  glade_xml_get_widget (gui, "ac_type");
	if ( s->priv->ac_type )
		gtk_entry_set_text ( GTK_ENTRY (GTK_COMBO (w)->entry),
							 s->priv->ac_type);
	s->priv->w_ac_type = GTK_COMBO (w)->entry;
	g_signal_connect(G_OBJECT (GTK_COMBO (w)->entry),
					 "changed", G_CALLBACK(entry_changed_cb), s);

	w = glade_xml_get_widget (gui, "ac_npoints");
	gtk_entry_set_text ( GTK_ENTRY (w), s->priv->ac_npoints);
	s->priv->w_ac_npoints = w;
	g_signal_connect(G_OBJECT (w), "changed", G_CALLBACK(entry_changed_cb), s);

	w = glade_xml_get_widget (gui, "ac_start");
	gtk_entry_set_text ( GTK_ENTRY (w), s->priv->ac_start);
	s->priv->w_ac_start = w;
	g_signal_connect(G_OBJECT (w), "changed", G_CALLBACK(entry_changed_cb), s);

	w = glade_xml_get_widget (gui, "ac_stop");
	gtk_entry_set_text(GTK_ENTRY (w), s->priv->ac_stop);
	s->priv->w_ac_stop = w;
	g_signal_connect(G_OBJECT(w), "changed", G_CALLBACK(entry_changed_cb), s);

	/*  DC   */
	/*  Get list of sources */

	items = node_store_get_parts (schematic_get_store (sm));
	for ( ; items; items = items->next ) {
	   gchar *temp = part_get_property (items->data,"template");
	   gchar *name = part_get_property (items->data,"refdes");
	   if (temp) {
		   gchar c = tolower(*temp);
		   if (c=='v' || c=='i' || c=='e' ||
			   c=='f' || c=='g' || c=='h' || c=='b' ) {
			   gchar *vsrc = g_strdup_printf ("%c_%s",*temp,name);
			   sources = g_list_prepend (sources,vsrc);
		   }
	   }
	}

	w = glade_xml_get_widget (gui, "dc_enable");
	s->priv->w_dc_enable = w;
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w), s->priv->dc_enable);
	g_signal_connect(G_OBJECT(w), "clicked", G_CALLBACK(dc_enable_cb), s);

	w = glade_xml_get_widget (gui, "dc_vin1");
	s->priv->w_dc_vin1 = w;
	if (sources)
		gtk_combo_set_popdown_strings (GTK_COMBO (w), sources);
	gtk_entry_set_text (GTK_ENTRY (GTK_COMBO (w)->entry),s->priv->dc_vin1);
	g_signal_connect(G_OBJECT(GTK_COMBO(w)->entry),
					 "changed", G_CALLBACK(entry_changed_cb), s);

	w = glade_xml_get_widget (gui, "dc_vin2");
	s->priv->w_dc_vin2 = w;
	if (sources)
		gtk_combo_set_popdown_strings (GTK_COMBO (w),sources);
	gtk_entry_set_text (GTK_ENTRY (GTK_COMBO (w)->entry),s->priv->dc_vin2);
	g_signal_connect(G_OBJECT(GTK_COMBO(w)->entry),
					 "changed", G_CALLBACK(entry_changed_cb), s);

	w = glade_xml_get_widget (gui, "dc_start1");
	s->priv->w_dc_start1 = w;
	gtk_entry_set_text ( GTK_ENTRY (w), s->priv->dc_start1);
	g_signal_connect(G_OBJECT(w), "changed", G_CALLBACK(entry_changed_cb), s);

	w = glade_xml_get_widget (gui, "dc_stop1");
	s->priv->w_dc_stop1 = w;
	gtk_entry_set_text ( GTK_ENTRY (w), s->priv->dc_stop1);
	g_signal_connect(G_OBJECT(w), "changed", G_CALLBACK(entry_changed_cb), s);

	w = glade_xml_get_widget (gui, "dc_step1");
	s->priv->w_dc_step1 = w;
	gtk_entry_set_text ( GTK_ENTRY (w), s->priv->dc_step1);
	g_signal_connect(G_OBJECT(w), "changed", G_CALLBACK(entry_changed_cb), s);

	w = glade_xml_get_widget (gui, "dc_start2");
	s->priv->w_dc_start2 = w;
	gtk_entry_set_text ( GTK_ENTRY (w), s->priv->dc_start2);
	g_signal_connect(G_OBJECT(w), "changed", G_CALLBACK(entry_changed_cb), s);

	w = glade_xml_get_widget (gui, "dc_stop2");
	s->priv->w_dc_stop2 = w;
	gtk_entry_set_text ( GTK_ENTRY (w), s->priv->dc_stop2);
	g_signal_connect(G_OBJECT(w), "changed", G_CALLBACK(entry_changed_cb), s);
	w = glade_xml_get_widget (gui, "dc_step2");
	s->priv->w_dc_step2 = w;
	gtk_entry_set_text ( GTK_ENTRY (w), s->priv->dc_step2);
	g_signal_connect(G_OBJECT(w), "changed", G_CALLBACK(entry_changed_cb), s);

	for (ltmp = sources; ltmp; ltmp = ltmp->next)
		g_free (ltmp->data);
	g_list_free (sources);

	/* Fourier    */
	w = glade_xml_get_widget (gui, "fourier_enable");
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w), s->priv->four_enable);
	s->priv->w_four_enable = w;
	g_signal_connect(G_OBJECT(w), "clicked", G_CALLBACK(four_enable_cb), s);

	w = glade_xml_get_widget (gui, "fourier_freq");
	s->priv->w_four_freq = w;
	g_signal_connect(G_OBJECT(w), "changed", G_CALLBACK(entry_changed_cb), s);

	w = glade_xml_get_widget (gui, "fourier_vout");
	s->priv->w_four_vout = w;
	g_signal_connect(G_OBJECT(w), "changed", G_CALLBACK(entry_changed_cb), s);

	w = glade_xml_get_widget (gui, "fourier_select_out");
	s->priv->w_four_combo = w;
	g_signal_connect (G_OBJECT (GTK_COMBO (w)->entry),
		"changed", G_CALLBACK(entry_changed_cb), s);

	w = glade_xml_get_widget (gui, "fourier_add");
	s->priv->w_four_add = w;
	g_signal_connect(G_OBJECT(w), "clicked", G_CALLBACK(four_add_vout_cb), s);

	gtk_widget_show_all (toplevel);

	ac_enable_cb (s->priv->w_ac_enable,s);
	four_enable_cb (s->priv->w_four_enable, s);
	dc_enable_cb(s->priv->w_dc_enable,s);
	trans_enable_cb (s->priv->w_trans_enable, s);
	trans_step_enable_cb (s->priv->w_trans_step_enable, s);
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

	/* Remove the option if already in the list. */
	while (list) {
		SimOption *so=list->data;
		if (so && !strcmp (opt->name,so->name)) {
			GList * tmp_list;
			g_free (so->name);
			g_free (so->value);
			tmp_list = g_list_remove (s->priv->options, so);
			g_free (so);
		}
		list = list->next;
	}
	s->priv->options = g_list_append (s->priv->options, opt);
}

