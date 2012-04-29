/*
 * simulation.h
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
 * Copyright (C) 2003,2004  Ricardo Markiewicz
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

#ifndef __SIMULATION_H
#define __SIMULATION_H

#include <gtk/gtk.h>

#include "schematic.h"
#include "schematic-view.h"

typedef struct _SimulationData SimulationData;

typedef enum {
	OP_POINT	,
	TRANSIENT	,
	DC_TRANSFER ,
	AC			,
	TRANSFER	,
	DISTORTION	,
	NOISE		,
	POLE_ZERO	,
	SENSITIVITY ,
	FOURIER		,
	ANALYSIS_UNKNOWN
} AnalysisType;

#define INFINITE 1e50f

typedef enum {
	FUNCTION_MINUS = 0,
	FUNCTION_TRANSFER
} SimulationFunctionType;

typedef struct _SimulationFunction {
	SimulationFunctionType type;
	guint first;
	guint second;
} SimulationFunction;

struct _SimulationData {
	AnalysisType type;
	gint		 state;
	gint		 n_variables;
	gchar	   **var_names;
	gchar	   **var_units;
	GArray	   **data;
	gdouble		*min_data;
	gdouble		*max_data;
	gint		 got_var;
	gint		 got_points;

	// Functions  typeof SimulationFunction
	GList 		*functions;
};


typedef struct {
	SimulationData sim_data;
	int		 state;
} SimOp;

typedef struct {
	SimulationData sim_data;
	double		   freq;
	gint		   nb_var;
} SimFourier;

typedef struct {
	SimulationData sim_data;
	int		 state;
	double	 sim_length;
	double	 step_size;
} SimTransient;

typedef struct {
	SimulationData sim_data;
	int		 state;
	double	 sim_length;
	double	 start,stop;
} SimAC;

typedef struct {
	SimulationData sim_data;
	int		 state;
	double	 sim_length;
	double	 start,stop,step;
} SimDC;

typedef union {
	SimOp		 op;
	SimTransient transient;
	SimFourier	 fourier;
	SimAC		 ac;
	SimDC		 dc;
} Analysis;

void simulation_show (GtkWidget *widget, SchematicView *sv);
gpointer simulation_new (Schematic *sm);
gchar *sim_engine_analysis_name (SimulationData *);

#define SIM_DATA(obj)			   ((SimulationData *)(obj))
#define ANALYSIS(obj)			   ((Analysis *)(obj))

#endif /* __SIMULATION_H */
