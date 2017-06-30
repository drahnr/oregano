/*
 * sim-settings.h
 *
 *
 * Authors:
 *  Richard Hult <rhult@hem.passagen.se>
 *  Ricardo Markiewicz <rmarkie@fi.uba.ar>
 *  Andres de Barbara <adebarbara@fi.uba.ar>
 *  Marc Lorber <lorber.marc@wanadoo.fr>
 *  Guido Trentalancia <guido@trentalancia.com>
 *
 * Web page: https://ahoi.io/project/oregano
 *
 * Copyright (C) 1999-2001  Richard Hult
 * Copyright (C) 2003,2004  Ricardo Markiewicz
 * Copyright (C) 2009-2012  Marc Lorber
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
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef __SIM_SETTINGS_H
#define __SIM_SETTINGS_H

typedef struct {
	gboolean configured;
	gboolean simulation_requested;

	// Transient analysis.
	gboolean trans_enable;
	gboolean trans_init_cond;
	gboolean trans_analyze_all;
	gchar *trans_start;
	gchar *trans_stop;
	gchar *trans_step;
	gboolean trans_step_enable;

	// AC
	gboolean ac_enable;
	gchar *ac_vout;
	gchar *ac_type;
	gchar *ac_npoints;
	gchar *ac_start;
	gchar *ac_stop;

	// DC
	gboolean dc_enable;
	gchar *dc_vin;
	gchar *dc_vout;
	gchar *dc_start, *dc_stop, *dc_step;

	// Fourier analysis. Replace with something sane later.
	gboolean fourier_enable;
	gchar *fourier_frequency;
	gchar *fourier_nb_vout;
	GSList *fourier_vout;

	// Noise
	gboolean noise_enable;
	gchar *noise_vin;
	gchar *noise_vout;
	gchar *noise_type;
	gchar *noise_npoints;
	gchar *noise_start;
	gchar *noise_stop;

	// Options
	GList *options;
} SimSettings;

typedef struct _SimOption
{
	gchar *name;
	gchar *value;
} SimOption;

SimSettings *sim_settings_new ();

void sim_settings_finalize(SimSettings *s);

gdouble sim_settings_get_trans_start (const SimSettings *sim_settings);

gdouble sim_settings_get_trans_stop (const SimSettings *sim_settings);

gdouble sim_settings_get_trans_step (const SimSettings *sim_settings);

gboolean sim_settings_get_trans (const SimSettings *sim_settings);

gdouble sim_settings_get_trans_step_enable (const SimSettings *sim_settings);

gboolean sim_settings_get_trans_init_cond (const SimSettings *sim_settings);

gboolean sim_settings_get_trans_analyze_all (const SimSettings *sim_settings);

void sim_settings_set_trans_start (SimSettings *sim_settings, gchar *str);

void sim_settings_set_trans_stop (SimSettings *sim_settings, gchar *str);

void sim_settings_set_trans_step (SimSettings *sim_settings, gchar *str);

void sim_settings_set_trans (SimSettings *sim_settings, gboolean enable);

void sim_settings_set_trans_step_enable (SimSettings *sim_settings, gboolean enable);

void sim_settings_set_trans_init_cond (SimSettings *sim_settings, gboolean enable);

void sim_settings_set_trans_analyze_all (SimSettings *sim_settings, gboolean enable);

gboolean sim_settings_get_dc (const SimSettings *);

gchar *sim_settings_get_dc_vsrc (const SimSettings *);

gchar *sim_settings_get_dc_vout (const SimSettings *);

gdouble sim_settings_get_dc_start (const SimSettings *);

gdouble sim_settings_get_dc_start (const SimSettings *);

gdouble sim_settings_get_dc_stop (const SimSettings *);

gdouble sim_settings_get_dc_step (const SimSettings *);

void sim_settings_set_dc (SimSettings *, gboolean);

void sim_settings_set_dc_vsrc (SimSettings *, gchar *);

void sim_settings_set_dc_vout (SimSettings *, gchar *);

void sim_settings_set_dc_start (SimSettings *, gchar *);

void sim_settings_set_dc_stop (SimSettings *, gchar *);

void sim_settings_set_dc_step (SimSettings *, gchar *);

gboolean sim_settings_get_ac (const SimSettings *);

gchar *sim_settings_get_ac_vout (const SimSettings *);

gchar *sim_settings_get_ac_type (const SimSettings *);

gint sim_settings_get_ac_npoints (const SimSettings *);

gdouble sim_settings_get_ac_start (const SimSettings *);

gdouble sim_settings_get_ac_stop (const SimSettings *);

void sim_settings_set_ac (SimSettings *, gboolean);

void sim_settings_set_ac_vout (SimSettings *, gchar *);

void sim_settings_set_ac_type (SimSettings *, gchar *);

void sim_settings_set_ac_npoints (SimSettings *, gchar *);

void sim_settings_set_ac_start (SimSettings *, gchar *);

void sim_settings_set_ac_stop (SimSettings *, gchar *);

void sim_settings_set_fourier (SimSettings *, gboolean);

void sim_settings_set_fourier_frequency (SimSettings *, gchar *);

void sim_settings_set_fourier_vout (SimSettings *, gchar *);

gboolean sim_settings_get_fourier (const SimSettings *);

gdouble sim_settings_get_fourier_frequency (const SimSettings *);

gchar *sim_settings_get_fourier_vout (const SimSettings *);

gchar *sim_settings_get_fourier_nodes (const SimSettings *);

gboolean sim_settings_get_noise (const SimSettings *);

gchar *sim_settings_get_noise_vsrc (const SimSettings *);

gchar *sim_settings_get_noise_vout (const SimSettings *);

gchar *sim_settings_get_noise_type (const SimSettings *);

gint sim_settings_get_noise_npoints (const SimSettings *);

gdouble sim_settings_get_noise_start (const SimSettings *);

gdouble sim_settings_get_noise_stop (const SimSettings *);

void sim_settings_set_noise (SimSettings *, gboolean);

void sim_settings_set_noise_vsrc (SimSettings *, gchar *);

void sim_settings_set_noise_vout (SimSettings *, gchar *);

void sim_settings_set_noise_type (SimSettings *, gchar *);

void sim_settings_set_noise_npoints (SimSettings *, gchar *);

void sim_settings_set_noise_start (SimSettings *, gchar *);

void sim_settings_set_noise_stop (SimSettings *, gchar *);

GList *sim_settings_get_options (const SimSettings *sim_settings);

void sim_settings_add_option (SimSettings *, SimOption *);

gchar *fourier_add_vout(SimSettings *sim_settings, guint i);

#endif
