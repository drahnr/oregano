/*
 * engine.c
 *
 * Authors:
 *  Ricardo Markiewicz <rmarkie@fi.uba.ar>
 *  Marc Lorber <lorber.marc@wanadoo.fr>
 *  Guido Trentalancia <guido@trentalancia.com>
 *
 * Web page: https://ahoi.io/project/oregano
 *
 * Copyright (C) 1999-2001  Richard Hult
 * Copyright (C) 2003,2006  Ricardo Markiewicz
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
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include <glib/gi18n.h>

#include "engine.h"
#include "engine-internal.h"
#include "gnucap.h"
#include "ngspice.h"

static gchar *analysis_names[] = {
	[ANALYSIS_TYPE_NONE]             = N_ ("None"),
	[ANALYSIS_TYPE_OP_POINT]         = N_ ("Operating Point"),
	[ANALYSIS_TYPE_TRANSIENT]        = N_ ("Transient Analysis"),
	[ANALYSIS_TYPE_DC_TRANSFER]      = N_ ("DC transfer characteristic"),
	[ANALYSIS_TYPE_AC]               = N_ ("AC Analysis"),
	[ANALYSIS_TYPE_TRANSFER]         = N_ ("Transfer Function"),
	[ANALYSIS_TYPE_DISTORTION]       = N_ ("Distortion Analysis"),
	[ANALYSIS_TYPE_NOISE]            = N_ ("Noise Spectral Density Curves"),
	[ANALYSIS_TYPE_POLE_ZERO]        = N_ ("Pole-Zero Analysis"),
	[ANALYSIS_TYPE_SENSITIVITY]      = N_ ("Sensitivity Analysis"),
	[ANALYSIS_TYPE_FOURIER]          = N_ ("Fourier analysis"),
	[ANALYSIS_TYPE_UNKNOWN]          = N_ ("Unknown Analysis"),
	NULL};

// Engines Types
static gchar *engine_names[] = {
	[OREGANO_ENGINE_GNUCAP]          = N_ ("gnucap"),
	[OREGANO_ENGINE_SPICE3]          = N_ ("spice3"),
	[OREGANO_ENGINE_NGSPICE]         = N_ ("ngspice"),
	NULL};

// Signals
enum { DONE, ABORTED, LAST_SIGNAL };

static guint engine_signals[LAST_SIGNAL] = {0};

static void oregano_engine_base_init (gpointer g_class);

GType oregano_engine_get_type (void)
{
	static GType type = 0;

	if (type == 0) {
		static const GTypeInfo info = {
		    sizeof(OreganoEngineClass),
		    oregano_engine_base_init, // base_init
		    NULL,                     // base_finalize
		    NULL,                     // class_init
		    NULL,                     // class_finalize
		    NULL,                     // class_data
		    0,
		    0,   // n_preallocs
		    NULL // instance_init
		};
		type = g_type_register_static (G_TYPE_INTERFACE, "OreganoEngine", &info, 0);
	}
	return type;
}

static void oregano_engine_base_init (gpointer g_class)
{
	static gboolean initialized = FALSE;

	if (!initialized) {
		// create interface signals here.
		engine_signals[DONE] =
		    g_signal_new ("done", G_TYPE_FROM_CLASS (g_class), G_SIGNAL_RUN_FIRST,
		                  G_STRUCT_OFFSET (OreganoEngineClass, done), NULL, NULL,
		                  g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

		engine_signals[ABORTED] =
		    g_signal_new ("aborted", G_TYPE_FROM_CLASS (g_class), G_SIGNAL_RUN_FIRST,
		                  G_STRUCT_OFFSET (OreganoEngineClass, abort), NULL, NULL,
		                  g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

		initialized = TRUE;
	}
}

void oregano_engine_start (OreganoEngine *self) { OREGANO_ENGINE_GET_CLASS (self)->start (self); }

void oregano_engine_stop (OreganoEngine *self) { OREGANO_ENGINE_GET_CLASS (self)->stop (self); }

gboolean oregano_engine_has_warnings (OreganoEngine *self)
{
	return OREGANO_ENGINE_GET_CLASS (self)->has_warnings (self);
}

gboolean oregano_engine_is_available (OreganoEngine *self)
{
	return OREGANO_ENGINE_GET_CLASS (self)->is_available (self);
}

void oregano_engine_get_progress_solver (OreganoEngine *self, double *p)
{
	g_return_if_fail(OREGANO_ENGINE_GET_CLASS (self)->progress_solver != NULL);
	OREGANO_ENGINE_GET_CLASS (self)->progress_solver (self, p);
}

void oregano_engine_get_progress_reader (OreganoEngine *self, double *p)
{
	g_return_if_fail(OREGANO_ENGINE_GET_CLASS(self)->progress_reader != NULL);
	OREGANO_ENGINE_GET_CLASS (self)->progress_reader (self, p);
}

gboolean oregano_engine_generate_netlist (OreganoEngine *self, const gchar *file, GError **error)
{
	return OREGANO_ENGINE_GET_CLASS (self)->get_netlist (self, file, error);
}

GList *oregano_engine_get_results (OreganoEngine *self)
{
	return OREGANO_ENGINE_GET_CLASS (self)->get_results (self);
}

gchar *oregano_engine_get_current_operation_solver (OreganoEngine *self)
{
	g_return_val_if_fail(OREGANO_ENGINE_GET_CLASS (self)->get_operation_solver != NULL, NULL);
	return OREGANO_ENGINE_GET_CLASS (self)->get_operation_solver (self);
}

gchar *oregano_engine_get_current_operation_reader (OreganoEngine *self)
{
	g_return_val_if_fail(OREGANO_ENGINE_GET_CLASS (self)->get_operation_reader != NULL, NULL);
	return OREGANO_ENGINE_GET_CLASS (self)->get_operation_reader (self);
}

OreganoEngine *oregano_engine_factory_create_engine (gint type, Schematic *sm)
{
	OreganoEngine *engine;

	switch (type) {
	case OREGANO_ENGINE_GNUCAP:
		engine = oregano_gnucap_new (sm);
		break;
	case OREGANO_ENGINE_SPICE3:
		engine = oregano_spice_new (sm, TRUE);
		break;
	case OREGANO_ENGINE_NGSPICE:
		engine = oregano_spice_new (sm, FALSE);
		break;
	default:
		engine = NULL;
	}
	return engine;
}

gchar *oregano_engine_get_analysis_name_by_type(AnalysisType type) {
	return g_strdup(_(analysis_names[type]));
}

gchar *oregano_engine_get_engine_name_by_index (const guint index) {
	return g_strdup (_(engine_names[index]));
}

/**
 * @sdat: nullable
 */
gchar *oregano_engine_get_analysis_name (SimulationData *sdat)
{
	if (sdat == NULL) {
		return g_strdup (_ (analysis_names[ANALYSIS_TYPE_UNKNOWN]));
	}
	return g_strdup (_ (analysis_names[sdat->type]));
}
