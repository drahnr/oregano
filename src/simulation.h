/*
 * simulation.h
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

#ifndef __SIMULATION_H
#define __SIMULATION_H

#include <gtk/gtk.h>

#include "schematic.h"
#include "schematic-view.h"

typedef struct _SimulationData SimulationData;

typedef enum {
	ANALYSIS_TYPE_NONE,
	ANALYSIS_TYPE_OP_POINT,
	ANALYSIS_TYPE_TRANSIENT,
	ANALYSIS_TYPE_DC_TRANSFER,
	ANALYSIS_TYPE_AC,
	ANALYSIS_TYPE_TRANSFER,
	ANALYSIS_TYPE_DISTORTION,
	ANALYSIS_TYPE_NOISE,
	ANALYSIS_TYPE_POLE_ZERO,
	ANALYSIS_TYPE_SENSITIVITY,
	ANALYSIS_TYPE_FOURIER,
	ANALYSIS_TYPE_UNKNOWN
} AnalysisType;

#define INFINITE 1e50f

//keep in mind the relation to global variable
//const char const *SimulationFunctionTypeString[]
//in simulation.c (strings representing the functions in GUI)
typedef enum {
	FUNCTION_SUBTRACT = 0,
	FUNCTION_DIVIDE
} SimulationFunctionType;

typedef struct _SimulationFunction
{
	SimulationFunctionType type;
	guint first;
	guint second;
} SimulationFunction;

struct _SimulationData
{
	AnalysisType type;
	gint n_variables;
	gchar **var_names;
	gchar **var_units;
	GArray **data;
	gdouble *min_data;
	gdouble *max_data;
	gint got_var;
	gint got_points;

	// Functions  typeof SimulationFunction
	GList *functions;
};

typedef struct
{
	SimulationData sim_data;
	int state;
} SimOp;

typedef struct
{
	SimulationData sim_data;
	double freq;
	gint nb_var;
} SimFourier;

typedef struct
{
	SimulationData sim_data;
	int state;
	double sim_length;
	double step_size;
} SimTransient;

typedef struct
{
	SimulationData sim_data;
	int state;
	double sim_length;
	double start, stop;
} SimAC;

typedef struct
{
	SimulationData sim_data;
	int state;
	double sim_length;
	double start, stop, step;
} SimDC;

typedef struct
{
	SimulationData sim_data;
	int state;
	double sim_length;
	double start, stop;
} SimNoise;

typedef union
{
	SimOp op;
	SimTransient transient;
	SimFourier fourier;
	SimAC ac;
	SimDC dc;
	SimNoise noise;
} Analysis;

void simulation_show_progress_bar (GtkWidget *widget, SchematicView *sv);
gpointer simulation_new (Schematic *sm, Log *logstore);

#define SIM_DATA(obj) ((SimulationData *)(obj))
#define ANALYSIS(obj) ((Analysis *)(obj))

#endif /* __SIMULATION_H */
