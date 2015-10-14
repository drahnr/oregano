/*
 * engine.c
 *
 * Authors:
 *  Ricardo Markiewicz <rmarkie@fi.uba.ar>
 *  Marc Lorber <lorber.marc@wanadoo.fr>
 *  Bernhard Schuster <bernhard@ahoi.io>
 *
 * Web page: https://ahoi.io/project/oregano
 *
 * Copyright (C) 1999-2001  Richard Hult
 * Copyright (C) 2003,2006  Ricardo Markiewicz
 * Copyright (C) 2009-2012  Marc Lorber
 * Copyright (C) 2015       Bernhard Schuster
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
#include "ngspice.h"
#include "gnucap.h"
#include "echo.h"

static gchar *analysis_names[] = {
    N_ ("Operating Point"),  N_ ("Transient Analysis"), N_ ("DC transfer characteristic"),
    N_ ("AC Analysis"),      N_ ("Transfer Function"),  N_ ("Distortion Analysis"),
    N_ ("Noise Analysis"),   N_ ("Pole-Zero Analysis"), N_ ("Sensitivity Analysis"),
    N_ ("Fourier Analysis"), N_ ("Unknown Analysis"),   NULL};

// Signals
enum { DONE, ABORTED, LAST_SIGNAL };

static guint engine_signals[LAST_SIGNAL] = {0};

static void engine_default_init (EngineInterface *iface);


G_DEFINE_INTERFACE (Engine, engine, G_TYPE_OBJECT)

static void engine_default_init (EngineInterface *iface)
{
	engine_signals[DONE] = g_signal_new ("done", G_TYPE_FROM_INTERFACE (iface), G_SIGNAL_RUN_FIRST,
	                                     G_STRUCT_OFFSET (EngineInterface, done), NULL, NULL,
	                                     g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

	engine_signals[ABORTED] =
	    g_signal_new ("aborted", G_TYPE_FROM_INTERFACE (iface), G_SIGNAL_RUN_FIRST,
	                  G_STRUCT_OFFSET (EngineInterface, abort), NULL, NULL,
	                  g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);
}

void engine_start (Engine *self) { ENGINE_GET_IFACE (self)->start (self); }

void engine_stop (Engine *self) { ENGINE_GET_IFACE (self)->stop (self); }

gboolean engine_has_warnings (Engine *self) { return ENGINE_GET_IFACE (self)->has_warnings (self); }

gboolean engine_is_available (Engine *self) { return ENGINE_GET_IFACE (self)->is_available (self); }

void engine_get_progress (Engine *self, double *p) { ENGINE_GET_IFACE (self)->progress (self, p); }

gboolean engine_generate_netlist (Engine *self, const gchar *file, GError **error)
{
	return ENGINE_GET_IFACE (self)->get_netlist (self, file, error);
}

GList *engine_get_results (Engine *self) { return ENGINE_GET_IFACE (self)->get_results (self); }

gchar *engine_get_current_operation (Engine *self)
{
	return ENGINE_GET_IFACE (self)->get_operation (self);
}

Engine *engine_factory_create_engine (gint type, Schematic *sm)
{
	Engine *engine;

	switch (type) {
	case ENGINE_NGSPICE:
		engine = ngspice_new (sm);
		break;
	case ENGINE_GNUCAP:
		engine = gnucap_new (sm);
		break;
	default:
		oregano_echo("Unknown engine type with index %u", (unsigned)type);
		engine = NULL;
	}
	return engine;
}

gchar *engine_get_analysis_name (SimulationData *sdat)
{
	if (sdat == NULL) {
		return g_strdup (_ (analysis_names[ANALYSIS_UNKNOWN]));
	}
	return g_strdup (_ (analysis_names[sdat->type]));
}
