/*
 * sim-settings-gui.h
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
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef SIM_SETTINGS_GUI_H_
#define SIM_SETTINGS_GUI_H_

typedef struct _SimSettingsGui SimSettingsGui;

#include "model/schematic.h"
#include "sim-settings.h"
#include "schematic-view.h"

struct _SimSettingsGui {
	SimSettings *sim_settings;

	GtkWidget *pbox;
	GtkNotebook *notebook;

	GtkWidget *w_main;

	// Transient analysis.
	GtkWidget *w_trans_enable,
	          *w_trans_start,
	          *w_trans_stop,
	          *w_trans_step,
	          *w_trans_step_enable,
	          *w_trans_init_cond,
	          *w_trans_analyze_all,
	          *w_trans_frame;

	// AC
	GtkWidget *w_ac_enable,
		  *w_ac_vout,
		  *w_ac_type,
	          *w_ac_npoints,
	          *w_ac_start,
	          *w_ac_stop,
	          *w_ac_frame;

	// DC
	GtkWidget *w_dc_enable,
	          *w_dc_vin,
		  *w_dc_vout,
		  *w_dc_start,
	          *w_dc_stop,
	          *w_dc_step,
	          *w_dcsweep_frame;

	// Fourier analysis. Replace with something sane later.
	GtkWidget *w_four_enable,
	          *w_four_freq,
	          *w_four_vout,
	          *w_four_combobox,
	          *w_four_add,
	          *w_four_rem,
	          *w_fourier_frame;

	// Noise
	GtkWidget *w_noise_enable,
		  *w_noise_vin,
		  *w_noise_vout,
		  *w_noise_type,
	          *w_noise_npoints,
	          *w_noise_start,
	          *w_noise_stop,
	          *w_noise_frame;

	GtkEntry *w_opt_value;
	GtkTreeView *w_opt_list;
};

SimSettingsGui *sim_settings_gui_new();
void sim_settings_gui_finalize(SimSettingsGui *gui);
void sim_settings_show (GtkWidget *widget, SchematicView *sm);

gint get_voltmeters_list (GList **voltmeters, Schematic *sm, GError *e, gboolean with_type);

gint get_voltage_sources_list (GList **sources, Schematic *sm, GError *e, gboolean ac_only);

#endif /* SIM_SETTINGS_GUI_H_ */
