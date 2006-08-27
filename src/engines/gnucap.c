/*
 * gnucap.c
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

#include <glib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "gnucap.h"

struct _OreganoGnuCapPriv {
	GPid child;
};

static void gnucap_instance_init (GTypeInstance *instance, gpointer g_class);
static void gnucap_interface_init (gpointer g_iface, gpointer iface_data);
static void gnucap_generate_netlist (Schematic *sm, gchar *file, GError **error);

GType 
oregano_gnucap_get_type (void)
{
	static GType type = 0;
	if (type == 0) {
		static const GTypeInfo info = {
			sizeof (OreganoGnuCapClass),
			NULL,   /* base_init */
			NULL,   /* base_finalize */
			NULL,   /* class_init */
			NULL,   /* class_finalize */
			NULL,   /* class_data */
			sizeof (OreganoGnuCap),
			0,      /* n_preallocs */
			gnucap_instance_init    /* instance_init */
		};

		static const GInterfaceInfo gnucap_info = {
			(GInterfaceInitFunc) gnucap_interface_init,    /* interface_init */
			NULL,               /* interface_finalize */
			NULL                /* interface_data */
		};

		type = g_type_register_static (G_TYPE_OBJECT, "OreganoGnuCap", &info, 0);
		g_type_add_interface_static (type, OREGANO_TYPE_ENGINE, &gnucap_info);
	}
	return type;
}

static void
gnucap_stop (OreganoEngine *self)
{
}

static void
gnucap_watch_cb (GPid pid, gint status, OreganoGnuCap *gnucap)
{
	/* check for status, see man waitpid(2) */
	if (WIFEXITED (status)) {
		g_signal_emit_by_name (G_OBJECT (gnucap), "done");
	}
}

static void
gnucap_start (OreganoEngine *self)
{
	OreganoGnuCap *gnucap;
	GError *error = NULL;
	char *argv[] = {"gnucap"};

	gnucap = OREGANO_GNUCAP (self);

	if (g_spawn_async_with_pipes (
			NULL, /* Working directory */
			argv,
			NULL,
			G_SPAWN_DO_NOT_REAP_CHILD | G_SPAWN_SEARCH_PATH,
			NULL,
			NULL,
			&gnucap->priv->child,
			NULL, /* STDIN */
			NULL, /* STDOUT */
			NULL, /* STDERR*/
			&error
		)) {
		/* Add a watch */
		g_child_watch_add (gnucap->priv->child, (GChildWatchFunc)gnucap_watch_cb, gnucap);
	} else {
		g_print ("Imposible lanzar el proceso hijo.");
	}
}

static void
gnucap_interface_init (gpointer g_iface, gpointer iface_data)
{
	OreganoEngineClass *klass = (OreganoEngineClass *)g_iface;
	klass->start = gnucap_start;
	klass->stop = gnucap_stop;
}

static void
gnucap_instance_init (GTypeInstance *instance, gpointer g_class)
{
	OreganoGnuCap *self = OREGANO_GNUCAP (instance);

	self->priv = g_new0 (OreganoGnuCapPriv, 1);
}

OreganoEngine*
oregano_gnucap_new (void)
{
	OreganoGnuCap *gnucap;

	gnucap = OREGANO_GNUCAP (g_object_new (OREGANO_TYPE_GNUCAP, NULL));

	return OREGANO_ENGINE (gnucap);
}

static void
gnucap_generate_netlist (Schematic *sm, gchar *file, GError **error)
{
	/* Poner el codigo para generar la netlist, y hacerlo mas bonito */
}

