/*
 * plot-add-function.c
 *
 *
 * Authors:
 *	Ricardo Markiewicz <rmarkie@fi.uba.ar>
 *	Andres de Barbara <adebarbara@fi.uba.ar>
 *  Marc Lorber <lorber.marc@wanadoo.fr>
 *
 * Web page: https://github.com/marc-lorber/oregano
 *
 * Copyright (C) 1999-2001	Richard Hult
 * Copyright (C) 2003,2006	Ricardo Markiewicz
 * Copyright (C) 2009-2012  Marc Lorber
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
#include <glib/gi18n.h>

#include "plot-add-function.h"
#include "dialogs.h"

void
plot_add_function_show (OreganoEngine *engine, SimulationData *current)
{
	GtkBuilder *gui;
	GError *perror = NULL;
	GtkDialog *dialog;
	gchar *msg;
	GtkComboBoxText *op1, *op2, *functiontype;
	int i;
	gint result = 0;
	GtkWidget *warning;
	GtkWidget *container_temp;
	
	SimulationFunction *func = g_new0 (SimulationFunction, 1);

	if ((gui = gtk_builder_new ()) == NULL) {
		oregano_error (_("Could not create plot window."));
		return;
	} 
	else gtk_builder_set_translation_domain (gui, NULL);

	if (!g_file_test (OREGANO_UIDIR "/plot-add-function.ui",
		    G_FILE_TEST_EXISTS)) {
		msg = g_strdup_printf (
			_("The file %s could not be found. You might need to reinstall Oregano to fix this"),
			OREGANO_UIDIR "/plot-add-function.ui");
		oregano_error_with_title (_("Could not create plot window."), msg);
		g_free (msg);
		return;
	}

	if (gtk_builder_add_from_file (gui, OREGANO_UIDIR "/plot-add-function.ui", 
	    &perror) <= 0) {
		msg = perror->message;
		oregano_error_with_title (_("Could not create plot window."), msg);
		g_error_free (perror);
		return;
	}

	dialog = GTK_DIALOG (gtk_builder_get_object (gui, "toplevel"));
	container_temp = GTK_WIDGET (gtk_builder_get_object (gui, "op1_alignment"));
	op1 = GTK_COMBO_BOX_TEXT (gtk_combo_box_text_new ());
	gtk_container_add (GTK_CONTAINER (container_temp), GTK_WIDGET (op1));
	gtk_widget_show (GTK_WIDGET (op1));
	
	container_temp = GTK_WIDGET (gtk_builder_get_object (gui, "op2_alignment"));
	op2 = GTK_COMBO_BOX_TEXT (gtk_combo_box_text_new ());
	gtk_container_add (GTK_CONTAINER (container_temp), GTK_WIDGET (op2));
	gtk_widget_show (GTK_WIDGET (op2));

	container_temp = GTK_WIDGET (gtk_builder_get_object (gui, "function_alignment"));
	functiontype = GTK_COMBO_BOX_TEXT (gtk_combo_box_text_new ());
	gtk_container_add (GTK_CONTAINER (container_temp), GTK_WIDGET (functiontype));
	gtk_widget_show (GTK_WIDGET (functiontype));

	gtk_combo_box_text_append_text (functiontype, _("Substraction"));
	gtk_combo_box_text_append_text (functiontype, _("Division"));

	for (i = 1; i < current->n_variables; i++) {
		if (current->type != DC_TRANSFER) {
			if (strchr (current->var_names[i], '#') == NULL) {
				gtk_combo_box_text_append_text (op1, current->var_names[i]);
				gtk_combo_box_text_append_text (op2, current->var_names[i]);
			}
		} 
		else {
			gtk_combo_box_text_append_text (op1, current->var_names[i]);
			gtk_combo_box_text_append_text (op2, current->var_names[i]);
		}
	}
	gtk_combo_box_set_active (GTK_COMBO_BOX (op1),0);
	gtk_combo_box_set_active (GTK_COMBO_BOX (op2),1);
	gtk_combo_box_set_active (GTK_COMBO_BOX (functiontype),0);

	result = gtk_dialog_run (GTK_DIALOG (dialog));
	
	if ((result == GTK_RESPONSE_OK) &&
	    ((gtk_combo_box_get_active (GTK_COMBO_BOX (op1)) == -1) ||
		 (gtk_combo_box_get_active (GTK_COMBO_BOX (op2)) == -1) ||
		 (gtk_combo_box_get_active (GTK_COMBO_BOX (functiontype)) == -1))) 
	{	
		warning = gtk_message_dialog_new_with_markup (
					NULL,
					GTK_DIALOG_MODAL,
					GTK_MESSAGE_WARNING,
					GTK_BUTTONS_OK, 
					_("<span weight=\"bold\" size=\"large\">Neither function, nor operators have been chosen</span>\n\n"
					"Please, take care to choose a function and their associated operators")); 

		if (gtk_dialog_run (GTK_DIALOG (warning)) == GTK_RESPONSE_OK)  {
			gtk_widget_destroy (GTK_WIDGET (warning));
			plot_add_function_show (engine, current);
			gtk_widget_destroy (GTK_WIDGET (dialog));
			return;
		}
	}

	if  ((result == GTK_RESPONSE_OK) &&
	     ((gtk_combo_box_get_active (GTK_COMBO_BOX (op1)) != -1) &&
		  (gtk_combo_box_get_active (GTK_COMBO_BOX (op2)) != -1) &&
		  (gtk_combo_box_get_active (GTK_COMBO_BOX (functiontype)) != -1))) {
	
		for (i = 1; i < current->n_variables; i++) {
			if (g_strcmp0 (current->var_names[i], gtk_combo_box_text_get_active_text (op1)) == 0)
				func->first = i;
			if (g_strcmp0 (current->var_names[i], gtk_combo_box_text_get_active_text (op2)) == 0)
				func->second = i;
			}
		current->functions = g_list_append (current->functions, func);
	}

	gtk_widget_destroy (GTK_WIDGET (dialog));
}
