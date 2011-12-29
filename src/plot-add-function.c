/*
 * plot-add-function.c
 *
 *
 * Authors:
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

#include "plot-add-function.h"

int plot_add_function_show (OreganoEngine *engine, SimulationData *current)
{
	GladeXML *gui;
	GtkDialog *dialog;
	gchar *msg;
	GtkComboBox *op1, *op2, *functiontype;
	GtkListStore *model1, *model2;
	GList *analysis = NULL;
	int i;

	if (!g_file_test (OREGANO_GLADEDIR "/plot-add-function.glade2",
		    G_FILE_TEST_EXISTS)) {
		msg = g_strdup_printf (
			_("The file %s could not be found. You might need to reinstall Oregano to fix this"),
			OREGANO_GLADEDIR "/plot-add-function.glade2");
		oregano_error_with_title (_("Could not create plot window"), msg);
		g_free (msg);
		return 0;
	}

	gui = glade_xml_new (OREGANO_GLADEDIR "/plot-add-function.glade2", NULL, NULL);
	if (!gui) {
		oregano_error (_("Could not create plot window"));
		return 0;
	}

	dialog = GTK_DIALOG (glade_xml_get_widget (gui, "toplevel"));
	op1 = GTK_COMBO_BOX (glade_xml_get_widget (gui, "op1"));
	op2 = GTK_COMBO_BOX (glade_xml_get_widget (gui, "op2"));
	functiontype = GTK_COMBO_BOX (glade_xml_get_widget (gui, "functiontype"));

	model1 = GTK_LIST_STORE (gtk_combo_box_get_model (op1));
	model2 = GTK_LIST_STORE (gtk_combo_box_get_model (op2));

	gtk_list_store_clear (model1);
	gtk_list_store_clear (model2);

	for (i = 1; i < current->n_variables; i++) {
		if (current->type != DC_TRANSFER) {
			if (strchr(current->var_names[i], '#') == NULL) {
				gtk_combo_box_append_text (op1, current->var_names[i]);
				gtk_combo_box_append_text (op2, current->var_names[i]);
			}
		} else {
			gtk_combo_box_append_text (op1, current->var_names[i]);
			gtk_combo_box_append_text (op2, current->var_names[i]);
		}
	}

	if (gtk_dialog_run (dialog) == GTK_RESPONSE_OK) {
		SimulationFunction *func = g_new0 (SimulationFunction, 1);
		func->type = gtk_combo_box_get_active (functiontype);
		for (i = 1; i < current->n_variables; i++) {
			if (strcmp (current->var_names[i], gtk_combo_box_get_active_text (op1)) == 0)
				func->first = i;
			if (strcmp (current->var_names[i], gtk_combo_box_get_active_text (op2)) == 0)
				func->second = i;
		}
	
		current->functions = g_list_append (current->functions, func);
	}

	gtk_widget_destroy (GTK_WIDGET (dialog));
}

