/*
 * sim-engine.h
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
 * Copyright (C) 2003,2004  LUGFI
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
#ifndef __SIM_ENGINE_H
#define __SIM_ENGINE_H

#include <gtk/gtk.h>
#include "sim-settings.h"

#define TYPE_SIM_ENGINE			   (sim_engine_get_type ())
#define SIM_ENGINE(obj)			   (G_TYPE_CHECK_INSTANCE_CAST ((obj), TYPE_SIM_ENGINE, SimEngine))
#define SIM_ENGINE_CLASS(klass)	   (G_TYPE_CHECK_CLASS_CAST ((klass), TYPE_SIM_ENGINE, SimEngineClass))
#define IS_SIM_ENGINE(obj)		   (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TYPE_SIM_ENGINE))
#define IS_SIM_ENGINE_CLASS(klass) (G_TYPE_INSTANCE_GET_CLASS ((klass), TYPE_SIM_ENGINE))

typedef struct _SimEngine SimEngine;
typedef struct _SimEngineClass SimEngineClass;
typedef struct _SimulationData SimulationData;

#include "schematic.h"

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
	ANALYSIS_UNKNOWN
} AnalysisType;

struct _SimulationData {
	AnalysisType type;
	gboolean	 binary;
	gint		 state;
	gint		 n_variables;
	gint		 n_points;
	gchar	   **var_names;
	gchar	   **var_units;
	GArray	   **data;
	gdouble		*min_data;
	gdouble		*max_data;
	gint		 got_var;
	gint		 got_points;
	gint		 num_data;
};


typedef struct {
	SimulationData sim_data;
	int		 state;
} SimOp;

/* Placeholder for something real later. */
typedef struct {
	SimulationData sim_data;
	double		   freq;
	GList		  *out_var;
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
	double	 start1,stop1,step1;
	double	 start2,stop2,step2;
} SimDC;


typedef union {
	SimOp		 op;
	SimTransient transient;
	SimFourier	 fourier;
	SimAC		 ac;
	SimDC		 dc;
} Analysis;

#define SIM_DATA(obj)			   ((SimulationData *)(obj))
#define ANALYSIS(obj)			   ((Analysis *)(obj))

struct _SimEngine {
	GObject parent;

	Schematic	*sm;
	SimSettings *sim_settings;

	gint		  num_analysis;
	GList		 *analysis;

	gboolean	 running;
	double		 progress;
	gboolean	 has_warnings;
	gboolean	 has_errors;
	gboolean	 aborted;

	pid_t		 child_pid;
	gint		 data_input_id;
	gint		 error_input_id;
	gint		 to_child[2];
	gint		 to_parent[2];
	gint		 to_parent_error[2];
	FILE		*inputfp;
	FILE		*outputfp;
	FILE		*errorfp;
};

struct _SimEngineClass
{
	GObjectClass parent_class;

	void (*done) (SimEngine *engine);
	void (*aborted) (SimEngine *engine);
	void (*cancelled) (SimEngine *engine);
};

GType sim_engine_get_type (void);

SimEngine *sim_engine_new (Schematic *sm);

void sim_engine_start_with_file (SimEngine   *engine,
								 const gchar *netlist);

void sim_engine_stop (SimEngine *engine);

gboolean sim_engine_get_op_value (SimEngine *engine,
								  char      *node_name,
								  double    *value);

gchar *sim_engine_analysis_name(SimulationData *);

#endif
