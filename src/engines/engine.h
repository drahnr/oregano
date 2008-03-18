/*
 * engine.h
 *
 * Authors:
 *  Ricardo Markiewicz <rmarkie@fi.uba.ar>
 *
 * Web page: http://oregano.lug.fi.uba.ar/
 *
 * Copyright (C) 1999-2001  Richard Hult
 * Copyright (C) 2003,2006  Ricardo Markiewicz
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
#ifndef __ENGINE_H
#define __ENGINE_H 1

#include <gtk/gtk.h>
#include "sim-settings.h"
#include "schematic.h"
#include "simulation.h"

#define OREGANO_TYPE_ENGINE             (oregano_engine_get_type ())
#define OREGANO_ENGINE(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), OREGANO_TYPE_ENGINE, OreganoEngine))
#define OREGANO_ENGINE_CLASS(klass)	    (G_TYPE_CHECK_CLASS_CAST((klass), OREGANO_TYPE_ENGINE, OreganoEngineClass))
#define OREGANO_IS_ENGINE(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), OREGANO_TYPE_ENGINE))
#define OREGANO_IS_ENGINE_CLASS(klass)  (G_TYPE_CLASS_TYPE((klass), OREGANO_TYPE_ENGINE, OreganoEngineClass))
#define OREGANO_ENGINE_GET_CLASS(klass) (G_TYPE_INSTANCE_GET_INTERFACE((klass), OREGANO_TYPE_ENGINE, OreganoEngineClass))

typedef struct _OreganoEngine OreganoEngine;
typedef struct _OreganoEngineClass OreganoEngineClass;

struct _OreganoEngineClass {
	GTypeInterface parent;

	void (*start) (OreganoEngine *engine);
	void (*stop) (OreganoEngine *engine);
	void (*progress) (OreganoEngine *engine, double *p);
	void (*get_netlist) (OreganoEngine *engine, const gchar *sm, GError **error);
	GList* (*get_results) (OreganoEngine *engine);
	gchar* (*get_operation) (OreganoEngine *engine);
	gboolean (*has_warnings) (OreganoEngine *engine);
	gboolean (*is_available) (OreganoEngine *engine);

	/* Signals */
	void (*done) ();
	void (*abort) ();
};

/* Engines IDs */
enum {
	OREGANO_ENGINE_GNUCAP=0,
	OREGANO_ENGINE_NGSPICE,
	OREGANO_ENGINE_COUNT
};

/* Engines Titles */
static const gchar*
engines[] = {
	"GnuCap",
	"NgSpice"
};

OreganoEngine *oregano_engine_factory_create_engine (gint type, Schematic *sm);

GType    oregano_engine_get_type (void);
void     oregano_engine_start (OreganoEngine *engine);
void     oregano_engine_stop (OreganoEngine *engine);
gboolean oregano_engine_has_warnings (OreganoEngine *engine);
void     oregano_engine_get_progress (OreganoEngine *engine, double *p);
void     oregano_engine_generate_netlist (OreganoEngine *engine, const gchar *file, GError **error);
GList   *oregano_engine_get_results (OreganoEngine *engine);
gchar   *oregano_engine_get_current_operation (OreganoEngine *);
gboolean oregano_engine_is_available (OreganoEngine *);
gchar   *oregano_engine_get_analysis_name (SimulationData *id);

#endif
