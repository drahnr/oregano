/*
 * engine.h
 *
 * Authors:
 *  Ricardo Markiewicz <rmarkie@fi.uba.ar>
 *  Marc Lorber <lorber.marc@wanadoo.fr>
 *
 * Web page: https://github.com/marc-lorber/oregano
 *
 * Copyright (C) 1999-2001  Richard Hult
 * Copyright (C) 2003,2006  Ricardo Markiewicz
 * Copyright (C) 2009-2010  Marc Lorber
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


#define OREGANO_ENGINE(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), OREGANO_TYPE_ENGINE, OreganoEngine))
#define OREGANO_IS_ENGINE(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), OREGANO_TYPE_ENGINE))

typedef struct _OreganoEngine OreganoEngine;

// Engines IDs 
enum {
	OREGANO_ENGINE_GNUCAP=0,
	OREGANO_ENGINE_NGSPICE,
	OREGANO_ENGINE_COUNT
};

OreganoEngine *oregano_engine_factory_create_engine (gint type, Schematic *sm);

GType    oregano_engine_get_type (void);
void     oregano_engine_start (OreganoEngine *engine);
void     oregano_engine_stop (OreganoEngine *engine);
gboolean oregano_engine_has_warnings (OreganoEngine *engine);
void     oregano_engine_get_progress (OreganoEngine *engine, double *p);
void     oregano_engine_generate_netlist (OreganoEngine *engine, 
    			const gchar *file, GError **error);
GList   *oregano_engine_get_results (OreganoEngine *engine);
gchar   *oregano_engine_get_current_operation (OreganoEngine *);
gboolean oregano_engine_is_available (OreganoEngine *);
gchar   *oregano_engine_get_analysis_name (SimulationData *id);

#endif
