/*
 * engine-internal.h
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
#ifndef __ENGINE_INTERNAL_H
#define __ENGINE_INTERNAL_H 1

#include <gtk/gtk.h>
#include "sim-settings.h"
#include "schematic.h"
#include "simulation.h"

#define OREGANO_TYPE_ENGINE             (oregano_engine_get_type ())
#define OREGANO_ENGINE_CLASS(klass)	    (G_TYPE_CHECK_CLASS_CAST((klass), OREGANO_TYPE_ENGINE, OreganoEngineClass))
#define OREGANO_IS_ENGINE_CLASS(klass)  (G_TYPE_CLASS_TYPE((klass), OREGANO_TYPE_ENGINE, OreganoEngineClass))
#define OREGANO_ENGINE_GET_CLASS(klass) (G_TYPE_INSTANCE_GET_INTERFACE((klass), OREGANO_TYPE_ENGINE, OreganoEngineClass))

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

	// Signals
	void (*done)  ();
	void (*abort) ();
};

#endif
