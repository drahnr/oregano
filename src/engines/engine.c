/*
 * engine.c
 *
 * Authors:
 *  Ricardo Markiewicz <rmarkie@fi.uba.ar>
 *
 * Web page: http://oregano.lug.fi.uba.ar/
 *
 * Copyright (C) 1999-2001  Richard Hult
 * Copyright (C) 2003,2006  LUGFI
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

#include "engine.h"

/* Signals */
enum {
	DONE,
	LAST_SIGNAL
};
static guint engine_signals[LAST_SIGNAL] = { 0 };

static void
oregano_engine_base_init (gpointer g_class)
{
	static gboolean initialized = FALSE;

	if (!initialized) {
		/* create interface signals here. */
		engine_signals[DONE] = g_signal_new ("done", G_TYPE_FROM_CLASS (g_class),
			G_SIGNAL_RUN_FIRST,
			G_STRUCT_OFFSET (OreganoEngineClass, done),
			NULL,
			NULL,
			g_cclosure_marshal_VOID__VOID,
			G_TYPE_NONE,
			0);
		initialized = TRUE;
	}
}

GType
oregano_engine_get_type (void)
{
	static GType type = 0;

	if (type == 0) {
		static const GTypeInfo info = {
			sizeof (OreganoEngineClass),
			oregano_engine_base_init,   /* base_init */
			NULL,   /* base_finalize */
			NULL,   /* class_init */
			NULL,   /* class_finalize */
			NULL,   /* class_data */
			0,
			0,      /* n_preallocs */
			NULL    /* instance_init */
		};
		type = g_type_register_static (G_TYPE_INTERFACE, "OreganoEngine", &info, 0);
	}
	return type;
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

