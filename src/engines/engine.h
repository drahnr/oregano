/*
 * engine.h
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
 * Copyright (C) 2009-2010  Marc Lorber
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
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef __ENGINE_H
#define __ENGINE_H 1

#include <gtk/gtk.h>
#include "sim-settings.h"
#include "schematic.h"
#include "simulation.h"

#define ENGINE(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), TYPE_ENGINE, Engine))
#define IS_ENGINE(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TYPE_ENGINE))

typedef struct _Engine Engine;

// Engines IDs
enum { ENGINE_GNUCAP = 0, ENGINE_NGSPICE, ENGINE_COUNT };

G_DEFINE_AUTOPTR_CLEANUP_FUNC (Engine, g_object_unref)

Engine *engine_factory_create_engine (gint type, Schematic *sm);

GType engine_get_type (void);
void engine_start (Engine *engine);
void engine_stop (Engine *engine);
gboolean engine_has_warnings (Engine *engine);
void engine_get_progress (Engine *engine, double *p);
gboolean engine_generate_netlist (Engine *engine, const gchar *file, GError **error);
GList *engine_get_results (Engine *engine);
gchar *engine_get_current_operation (Engine *);
gboolean engine_is_available (Engine *);
gchar *engine_get_analysis_name (SimulationData *id);

#endif
