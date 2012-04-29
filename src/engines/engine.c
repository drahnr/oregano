/*
 * engine.c
 *
 * Authors:
 *  Ricardo Markiewicz <rmarkie@fi.uba.ar>
 *  Marc Lorber <lorber.marc@wanadoo.fr>
 *
 * Web page: https://github.com/marc-lorber/oregano
 *
 * Copyright (C) 1999-2001  Richard Hult
 * Copyright (C) 2003,2006  Ricardo Markiewicz
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

#include <glib/gi18n.h>

#include "engine.h"
#include "engine-internal.h"
#include "gnucap.h"
#include "ngspice.h"

static gchar *analysis_names[] = {
	N_("Operating Point"),
	N_("Transient Analysis"),
	N_("DC transfer characteristic"),
	N_("AC Analysis"),
	N_("Transfer Function"),
	N_("Distortion Analysis"),
	N_("Noise Analysis"),
	N_("Pole-Zero Analysis"),
	N_("Sensitivity Analysis"),
	N_("Fourier Analysis"),
	N_("Unknown Analysis"),
	NULL
};

// Signals 
enum {
	DONE,
	ABORTED,
	LAST_SIGNAL
};

static guint engine_signals[LAST_SIGNAL] = { 0 };

static void oregano_engine_base_init (gpointer g_class);

GType
oregano_engine_get_type (void)
{
	static GType type = 0;

	if (type == 0) {
		static const GTypeInfo info = {
			sizeof (OreganoEngineClass),
			oregano_engine_base_init,   // base_init
			NULL,   // base_finalize
			NULL,   // class_init
			NULL,   // class_finalize
			NULL,   // class_data
			0,
			0,      // n_preallocs
			NULL    // instance_init
		};
		type = g_type_register_static (G_TYPE_INTERFACE, "OreganoEngine", &info, 
		                               0);
	}
	return type;
}

static void
oregano_engine_base_init (gpointer g_class)
{
	static gboolean initialized = FALSE;

	if (!initialized) {
		// create interface signals here.
		engine_signals[DONE] = g_signal_new ("done", 
		    G_TYPE_FROM_CLASS (g_class),
			G_SIGNAL_RUN_FIRST,
			G_STRUCT_OFFSET (OreganoEngineClass, done),
			NULL,
			NULL,
			g_cclosure_marshal_VOID__VOID,
			G_TYPE_NONE,
			0);

		engine_signals[ABORTED] = g_signal_new ("aborted", 
		    G_TYPE_FROM_CLASS (g_class),
			G_SIGNAL_RUN_FIRST,
			G_STRUCT_OFFSET (OreganoEngineClass, abort),
			NULL,
			NULL,
			g_cclosure_marshal_VOID__VOID,
			G_TYPE_NONE,
			0);

		initialized = TRUE;
	}
}

void
oregano_engine_start (OreganoEngine *self)
{
	OREGANO_ENGINE_GET_CLASS (self)->start (self);
}

void
oregano_engine_stop (OreganoEngine *self)
{
	OREGANO_ENGINE_GET_CLASS (self)->stop (self);
}

gboolean
oregano_engine_has_warnings (OreganoEngine *self)
{
	return OREGANO_ENGINE_GET_CLASS (self)->has_warnings (self);
}

gboolean
oregano_engine_is_available (OreganoEngine *self)
{
	return OREGANO_ENGINE_GET_CLASS (self)->is_available (self);
}

void
oregano_engine_get_progress (OreganoEngine *self, double *p)
{
	OREGANO_ENGINE_GET_CLASS (self)->progress (self, p);
}

void
oregano_engine_generate_netlist (OreganoEngine *self, const gchar *file, GError **error)
{
	OREGANO_ENGINE_GET_CLASS (self)->get_netlist (self, file, error);
}

GList*
oregano_engine_get_results (OreganoEngine *self)
{
	return OREGANO_ENGINE_GET_CLASS (self)->get_results (self);
}

gchar*
oregano_engine_get_current_operation (OreganoEngine *self)
{
	return OREGANO_ENGINE_GET_CLASS (self)->get_operation (self);
}
OreganoEngine*
oregano_engine_factory_create_engine (gint type, Schematic *sm)
{
	OreganoEngine *engine;

	switch (type) {
		case OREGANO_ENGINE_GNUCAP:
			engine = oregano_gnucap_new (sm);
		break;
		case OREGANO_ENGINE_NGSPICE:
			engine = oregano_ngspice_new (sm);
		break;
		default:
			engine = NULL;
	}
	return engine;
}

gchar *
oregano_engine_get_analysis_name (SimulationData *sdat)
{
	if (sdat == NULL) {
		return g_strdup (_(analysis_names[ANALYSIS_UNKNOWN]));
	}
	return g_strdup (_(analysis_names[sdat->type]));
}
