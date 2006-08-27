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
#include "netlist.h"

struct _OreganoGnuCapPriv {
	GPid child;
	Schematic *schematic;
};

static void gnucap_instance_init (GTypeInstance *instance, gpointer g_class);
static void gnucap_interface_init (gpointer g_iface, gpointer iface_data);

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

static gboolean
gnucap_has_warnings (OreganoEngine *self)
{
	return FALSE;
}

static void
gnucap_generate_netlist (OreganoEngine *engine, const gchar *filename, GError **error)
{
	OreganoGnuCap *gnucap;
	Netlist output;
	SimOption *so;
	GList *list;
	FILE *file;

	gnucap = OREGANO_GNUCAP (engine);

	netlist_helper_create (gnucap->priv->schematic, &output, error); //TODO: usar bien error

	file = fopen (filename, "w");
	if (!file) {
		g_print ("No se pudo crear %s\n", filename);
		return;
	}

	list = sim_settings_get_options (output.settings);

	/* Prints title */
	fputs (output.title ? output.title : "Title: <unset>", file);
	fputs ("\n"
		   "*----------------------------------------------"
		   "\n"
		   "*\tGNUCAP - NETLIST"
		   "\n", file);
	/* Prints Options */
	fputs (".options OUT=120 ",file);

	while (list) {
		so = list->data;
		/* Prevent send NULL text */
		if (so->value) {
			if (strlen(so->value) > 0) {
				fprintf (file,"%s=%s ",so->name,so->value);
			}
		}
		list = list->next;
	}
	fputc ('\n',file);

	/* Prints template parts */

	/* Inclusion of complex part */
	/* TODO : add GNU Cap model inclusion */
	/* g_hash_table_foreach (data->models, (GHFunc) foreach_model_write_gnucap, file); */

	fputs ("*----------------------------------------------\n",file);
	fputs (output.template->str,file);
	fputs ("\n*----------------------------------------------\n",file);

	/* Prints Transient Analisis */
	if (sim_settings_get_trans (output.settings)) {
		fprintf(file, ".print tran %s\n", netlist_helper_create_analisys_string (output.store, FALSE));
		fprintf (file, ".tran %g %g ",
			sim_settings_get_trans_start(output.settings),
			sim_settings_get_trans_stop(output.settings));
			if (!sim_settings_get_trans_step_enable(output.settings))
				/* FIXME Do something to get the right resolution */
				fprintf(file,"%g",
						(sim_settings_get_trans_stop(output.settings)-
						sim_settings_get_trans_start(output.settings))/100);
			else
				fprintf(file,"%g", sim_settings_get_trans_step(output.settings));

			if (sim_settings_get_trans_init_cond(output.settings)) {
				fputs(" UIC\n", file);
			} else {
				fputs("\n", file);
			}
	}

	/*	Print dc Analysis */
	if (sim_settings_get_dc (output.settings)) {
		fprintf(file, ".print dc %s\n", netlist_helper_create_analisys_string (output.store, FALSE));
		fputs(".dc ",file);

		/* GNUCAP don t support nesting so the first or the second */
		/* Maybe a error message must be show if both are on	   */

		if ( sim_settings_get_dc_vsrc (output.settings,0) ) {
			fprintf (file,"%s %g %g %g",
					sim_settings_get_dc_vsrc(output.settings,0),
					sim_settings_get_dc_start (output.settings,0),
					sim_settings_get_dc_stop (output.settings,0),
					sim_settings_get_dc_step (output.settings,0) );
		}

		else if ( sim_settings_get_dc_vsrc (output.settings,1) ) {
			fprintf (file,"%s %g %g %g",
					sim_settings_get_dc_vsrc(output.settings,1),
					sim_settings_get_dc_start (output.settings,1),
					sim_settings_get_dc_stop (output.settings,1),
					sim_settings_get_dc_step (output.settings,1) );
		};

		fputc ('\n',file);
	}

	/* Prints ac Analysis*/
	if ( sim_settings_get_ac (output.settings) ) {
		double ac_start, ac_stop, ac_step;
		/* GNUCAP dont support OCT or DEC */
		/* Maybe a error message must be show if is set in that way */
		ac_start = sim_settings_get_ac_start(output.settings) ;
		ac_stop = sim_settings_get_ac_stop(output.settings);
		ac_step = (ac_stop - ac_start) / sim_settings_get_ac_npoints(output.settings);
		fprintf(file, ".print ac %s\n", netlist_helper_create_analisys_string (output.store, TRUE));
		/* AC format : ac start stop step_size */
		fprintf (file, ".ac %g %g %g\n", ac_start, ac_stop, ac_step);
	}

	/* Debug op analysis. */
	fputs(".print op v(nodes)\n", file);
	fputs(".op\n", file);
	fputs(".end\n", file);
	fclose (file);
}

static void
gnucap_progress (OreganoEngine *self, double *d)
{
	(*d) = 0;
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
	char *argv[] = {"gnucap", "-b", "/tmp/netlist.tmp"};

	gnucap = OREGANO_GNUCAP (self);

	oregano_engine_generate_netlist (self, "/tmp/netlist.tmp", &error);

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
	klass->progress = gnucap_progress;
	klass->get_netlist = gnucap_generate_netlist;
	klass->has_warnings = gnucap_has_warnings;
}

static void
gnucap_instance_init (GTypeInstance *instance, gpointer g_class)
{
	OreganoGnuCap *self = OREGANO_GNUCAP (instance);

	self->priv = g_new0 (OreganoGnuCapPriv, 1);
}

OreganoEngine*
oregano_gnucap_new (Schematic *sc)
{
	OreganoGnuCap *gnucap;

	gnucap = OREGANO_GNUCAP (g_object_new (OREGANO_TYPE_GNUCAP, NULL));
	gnucap->priv->schematic = sc;

	return OREGANO_ENGINE (gnucap);
}

