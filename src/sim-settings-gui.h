/*
 * sim-settings-gui.h
 *
 *  Created on: Jun 14, 2017
 *      Author: michi
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
	          *w_ac_type,
	          *w_ac_npoints,
	          *w_ac_start,
	          *w_ac_stop,
	          *w_ac_frame;

	// DC
	GtkWidget *w_dc_enable,
	          *w_dc_vin,
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

	GtkEntry *w_opt_value;
	GtkTreeView *w_opt_list;

};

SimSettingsGui *sim_settings_gui_new();
void sim_settings_gui_finalize(SimSettingsGui *gui);
void sim_settings_show (GtkWidget *widget, SchematicView *sm);

#endif /* SIM_SETTINGS_GUI_H_ */
