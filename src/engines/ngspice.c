/*
 * ngspice.c
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

#include <glib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <ctype.h>
#include "ngspice.h"
#include "netlist.h"

// TODO Move analisys data and result to another file
#include "simulation.h"

/* Parser STATUS */
struct _OreganoNgSpicePriv {
	GPid child_pid;
	Schematic *schematic;

	gboolean aborted;

	GList *analysis;
	gint num_analysis;
	SimulationData *current;
	double progress;
	gboolean char_last_newline;
	guint status;
	guint buf_count;
	gchar buf[256]; // FIXME later
};

static void ngspice_instance_init (GTypeInstance *instance, gpointer g_class);
static void ngspice_interface_init (gpointer g_iface, gpointer iface_data);
static gboolean ngspice_child_stdout_cb (GIOChannel *source, GIOCondition condition, OreganoNgSpice *ngspice);
static void ngspice_parse (FILE *, OreganoNgSpice *ngspice);

GType 
oregano_ngspice_get_type (void)
{
	static GType type = 0;
	if (type == 0) {
		static const GTypeInfo info = {
			sizeof (OreganoNgSpiceClass),
			NULL,   /* base_init */
			NULL,   /* base_finalize */
			NULL,   /* class_init */
			NULL,   /* class_finalize */
			NULL,   /* class_data */
			sizeof (OreganoNgSpice),
			0,      /* n_preallocs */
			ngspice_instance_init    /* instance_init */
		};

		static const GInterfaceInfo ngspice_info = {
			(GInterfaceInitFunc) ngspice_interface_init,    /* interface_init */
			NULL,               /* interface_finalize */
			NULL                /* interface_data */
		};

		type = g_type_register_static (G_TYPE_OBJECT, "OreganoNgSpice", &info, 0);
		g_type_add_interface_static (type, OREGANO_TYPE_ENGINE, &ngspice_info);
	}
	return type;
}

static gboolean
ngspice_has_warnings (OreganoEngine *self)
{
	return FALSE;
}

static gboolean
ngspice_is_available (OreganoEngine *self)
{
	gchar *exe;
	exe = g_find_program_in_path ("ngspice");

	if (!exe) return FALSE; // ngspice not found

	g_free (exe);
	return TRUE;
}

static void
ngspice_generate_netlist (OreganoEngine *engine, const gchar *filename, GError **error)
{
	OreganoNgSpice *ngspice;
	Netlist output;
	GList *list;
	SimOption *so;
	GError *local_error = NULL;
	FILE *file;

	ngspice = OREGANO_NGSPICE (engine);

	netlist_helper_create (ngspice->priv->schematic, &output, &local_error);
	if (local_error != NULL) {
		g_propagate_error (error, local_error);
		return;
	}

	file = fopen (filename, "w");
	if (!file) {
		g_print ("No se pudo crear %s\n", filename);
		return;
	}
			
	/* Prints title */	
	fputs ("* ",file);
	fputs (output.title ? output.title : "Title: <unset>", file);
	fputs ("\n"
		"*----------------------------------------------"
		"\n"
		"*\tSPICE 3 - NETLIST"
		"\n", file);

	/* Prints Options */
	fputs (".options\n", file);

	list = sim_settings_get_options (output.settings);
	while (list) {
		so = list->data;
		if (so->value)
			if (strlen(so->value) > 0)
				fprintf (file,"+ %s=%s\n", so->name, so->value);
		list = list->next;
	}
	fputc ('\n',file);

	/* Include of subckt models */
	fputs ("*------------- Models -------------------------\n",file);
	list = output.models;
	while (list) {
		gchar *model;
		model = (gchar *)list->data;
		fprintf (file,".include %s/%s.model\n", OREGANO_MODELDIR, model);
		list = list->next;
	}

	/* Prints template parts */ 
	fputs ("\n*----------------------------------------------\n\n",file);
	fputs (output.template->str, file);
	fputs ("\n*----------------------------------------------\n\n",file);

	/* Prints Transient Analisis */
	if (sim_settings_get_trans (output.settings)) {
		gdouble st = 0;
		if (sim_settings_get_trans_step_enable (output.settings))
			st = sim_settings_get_trans_step (output.settings);
		else
			st = (sim_settings_get_trans_stop (output.settings) -
				sim_settings_get_trans_start (output.settings)) / 50;

		fprintf (file, ".tran %g %g %g", st,
			sim_settings_get_trans_stop (output.settings),
			sim_settings_get_trans_start (output.settings));
		if (sim_settings_get_trans_init_cond (output.settings)) {
			fputs(" UIC\n", file);
		} else {
			fputs("\n", file);
		}
	}

	/*	Print dc Analysis */
	if (sim_settings_get_dc (output.settings)) {
		fputs(".dc ",file);
		if (sim_settings_get_dc_vsrc (output.settings, 0)) {
			fprintf (file, "%s %g %g %g",
				sim_settings_get_dc_vsrc (output.settings, 0),
				sim_settings_get_dc_start (output.settings, 0),
				sim_settings_get_dc_stop (output.settings, 0),
				sim_settings_get_dc_step (output.settings, 0));
		}
		if (sim_settings_get_dc_vsrc (output.settings, 1)) {
			fprintf (file, "%s %g %g %g",
				sim_settings_get_dc_vsrc (output.settings, 1),
				sim_settings_get_dc_start (output.settings, 1),
				sim_settings_get_dc_stop (output.settings, 1),
				sim_settings_get_dc_step (output.settings, 1));
		}
	}

	/* Prints ac Analysis*/
	if (sim_settings_get_ac (output.settings)) {
		fprintf (file, ".ac %s %d %g %g\n",
			sim_settings_get_ac_type (output.settings),
			sim_settings_get_ac_npoints (output.settings),
			sim_settings_get_ac_start (output.settings),
			sim_settings_get_ac_stop (output.settings));
	}

	/* Debug op analysis. */
	fputs (".op\n", file);
	fputs ("\n.END\n", file);
	fclose (file);
}

static void
ngspice_progress (OreganoEngine *self, double *d)
{
	(*d) = OREGANO_NGSPICE (self)->priv->progress;
}

static void
ngspice_stop (OreganoEngine *self)
{
	OreganoNgSpice *ngspice = OREGANO_NGSPICE (self);
	g_spawn_close_pid (ngspice->priv->child_pid);
}

static void
ngspice_watch_cb (GPid pid, gint status, OreganoNgSpice *ngspice)
{
	/* check for status, see man waitpid(2) */
	if (WIFEXITED (status)) {
		FILE *fp;

		g_spawn_close_pid (ngspice->priv->child_pid);

		/* Parse data */
		if ((fp = fopen ("/tmp/netlist.raw", "r")) != NULL) {
			g_print ("File Open\n");
			while (!feof (fp))
				ngspice_parse (fp, ngspice);
		}

		ngspice->priv->current = NULL;

		if (ngspice->priv->num_analysis == 0) {
			schematic_log_append_error (ngspice->priv->schematic, _("### Too few or none analysis found ###\n"));
			ngspice->priv->aborted = TRUE;
			g_signal_emit_by_name (G_OBJECT (ngspice), "aborted");
		} else
			g_signal_emit_by_name (G_OBJECT (ngspice), "done");
	}
}

static void
ngspice_start (OreganoEngine *self)
{
	OreganoNgSpice *ngspice;
	GError *error = NULL;
	char *argv[] = {"ngspice", "-r", "/tmp/netlist.raw", "-b", "/tmp/netlist.tmp", NULL};

	ngspice = OREGANO_NGSPICE (self);
	oregano_engine_generate_netlist (self, "/tmp/netlist.tmp", &error);
	if (error != NULL) {
		ngspice->priv->aborted = TRUE;
		schematic_log_append_error (ngspice->priv->schematic, error->message);
		g_signal_emit_by_name (G_OBJECT (ngspice), "aborted");
		g_error_free (error);
		return;
	}

	error = NULL;
	if (g_spawn_async_with_pipes (
			NULL, /* Working directory */
			argv,
			NULL,
			G_SPAWN_DO_NOT_REAP_CHILD | G_SPAWN_SEARCH_PATH,
			NULL,
			NULL,
			&ngspice->priv->child_pid,
			NULL, /* STDIN */
			NULL, /* STDOUT */
			NULL, /* STDERR*/
			&error
		)) {
		/* Add a watch for process status */
		g_child_watch_add (ngspice->priv->child_pid, (GChildWatchFunc)ngspice_watch_cb, ngspice);
	} else {
		ngspice->priv->aborted = TRUE;
		schematic_log_append_error (ngspice->priv->schematic, _("Unable to execute NgSpice."));
		g_signal_emit_by_name (G_OBJECT (ngspice), "aborted");
	}
}

static GList*
ngspice_get_results (OreganoEngine *self)
{
	return OREGANO_NGSPICE (self)->priv->analysis;
}

const gchar*
ngspice_get_operation (OreganoEngine *self)
{
	OreganoNgSpicePriv *priv = OREGANO_NGSPICE (self)->priv;

	if (priv->current == NULL)
		return _("Waiting NgSpice backend");

	return oregano_engine_get_analysis_name (priv->current);
}

static void
ngspice_interface_init (gpointer g_iface, gpointer iface_data)
{
	OreganoEngineClass *klass = (OreganoEngineClass *)g_iface;
	klass->start = ngspice_start;
	klass->stop = ngspice_stop;
	klass->progress = ngspice_progress;
	klass->get_netlist = ngspice_generate_netlist;
	klass->has_warnings = ngspice_has_warnings;
	klass->get_results = ngspice_get_results;
	klass->get_operation = ngspice_get_operation;
	klass->is_available = ngspice_is_available;
}

static void
ngspice_instance_init (GTypeInstance *instance, gpointer g_class)
{
	OreganoNgSpice *self = OREGANO_NGSPICE (instance);

	self->priv = g_new0 (OreganoNgSpicePriv, 1);
}

OreganoEngine*
oregano_ngspice_new (Schematic *sc)
{
	OreganoNgSpice *ngspice;

	ngspice = OREGANO_NGSPICE (g_object_new (OREGANO_TYPE_NGSPICE, NULL));
	ngspice->priv->schematic = sc;

	return OREGANO_ENGINE (ngspice);
}

/* Parser stuff */

/* By now, the parser read the STDOUt just like in GnuCap. In a near future I'll
 * to use the raw outout feature of NgSpice wich will improve the parser
 */

typedef enum {
	STATE_IDLE,
	IN_VARIABLES,
	IN_VALUES,
	STATE_ABORT
} ParseDataState;

#define SP_TITLE      "Title"
#define SP_DATE       "Date:"
#define SP_PLOT_NAME  "Plotname:"
#define SP_FLAGS      "Flags:"
#define SP_N_VAR      "No. Variables:"
#define SP_N_POINTS   "No. Points:"
#define SP_COMMAND    "Command:"
#define SP_VARIABLES  "Variables:"
#define SP_BINARY     "Binary:"
#define SP_VALUES     "Values:" 
#define IS_THIS_ITEM(str,item)  (!strncmp(str,item,strlen(item)))  

static gchar *analysis_names[] = {
	N_("Operating Point")            ,
	N_("Transient Analysis")         ,
	N_("DC transfer characteristic") ,
	N_("AC Analysis")                ,
	N_("Transfer Function")          ,
	N_("Distortion Analysis")        ,
	N_("Noise Analysis")             ,
	N_("Pole-Zero Analysis")         ,
	N_("Sensitivity Analysis")       ,
	N_("Unknown Analysis")           ,
	NULL
};

#define NG_DEBUG(s) g_print ("NG: %s\n", s)

static void
ngspice_parse (FILE *fp, OreganoNgSpice *ngspice)
{
	static SimulationData *sdata = NULL;
	static char buf[1024];
	static int len;
	static is_complex = FALSE;
	SimSettings *sim_settings;
	gint status, iter;
	gdouble val, np1, np2;
	gchar **tmp = NULL;	
	gchar *send;
	int i, c;

	sim_settings = (SimSettings *)schematic_get_sim_settings (ngspice->priv->schematic);

	if (!sdata || sdata->state != IN_VALUES) {
		/* Read a line */
		len = fscanf (fp, "%[^\n]", buf);
		fgetc (fp);
		if (len == 0) return;
		NG_DEBUG (g_strdup_printf ("(%s)", buf));
	} else {
		buf[0] = '\0';
		len = 0;
	}

	/* We are getting the simulation title */
	if (IS_THIS_ITEM (buf, SP_TITLE)) {
		sdata = SIM_DATA (g_new0 (Analysis, 1));		
		ngspice->priv->analysis = g_list_append (ngspice->priv->analysis, sdata);
		ngspice->priv->num_analysis++;
		NG_DEBUG ("Nuevo Analisis");
	} else if (IS_THIS_ITEM (buf, SP_DATE)) {
		sdata->state = STATE_IDLE;
	} else if (IS_THIS_ITEM (buf,SP_PLOT_NAME)) {
		gint   i;
		gchar *analysis = buf+strlen(SP_PLOT_NAME)+1;

		NG_DEBUG ("Analisis Type");
		sdata->state = STATE_IDLE;
		sdata->type  = ANALYSIS_UNKNOWN;
		for (i = 0; analysis_names[i]; i++)
			if (IS_THIS_ITEM (analysis, analysis_names[i])) {
				sdata->type = i;
				break;
			}

		switch ( sdata->type ) {
			case TRANSIENT:
				ANALYSIS(sdata)->transient.sim_length =
					sim_settings_get_trans_stop  (sim_settings) -
					sim_settings_get_trans_start (sim_settings);
				ANALYSIS(sdata)->transient.step_size = 
					sim_settings_get_trans_step (sim_settings);
				break;
			case AC:
				ANALYSIS(sdata)->ac.start = sim_settings_get_ac_start (sim_settings);
				ANALYSIS(sdata)->ac.stop  = sim_settings_get_ac_stop  (sim_settings);
				ANALYSIS(sdata)->ac.sim_length = sim_settings_get_ac_npoints (sim_settings);
				break;
			case OP_POINT:
			case DC_TRANSFER:
				np1 = np2 = 1.;
				ANALYSIS(sdata)->dc.start1 = sim_settings_get_dc_start (sim_settings, 0);
				ANALYSIS(sdata)->dc.stop1  = sim_settings_get_dc_stop  (sim_settings, 0);
				ANALYSIS(sdata)->dc.step1  = sim_settings_get_dc_step  (sim_settings, 0);
				ANALYSIS(sdata)->dc.start2 = sim_settings_get_dc_start (sim_settings, 1);
				ANALYSIS(sdata)->dc.stop2  = sim_settings_get_dc_stop  (sim_settings, 1);
				ANALYSIS(sdata)->dc.step2  = sim_settings_get_dc_step  (sim_settings, 1);

				np1 = (ANALYSIS(sdata)->dc.stop1-ANALYSIS(sdata)->dc.start1) / ANALYSIS(sdata)->dc.step1;
				if (ANALYSIS(sdata)->dc.step2 != 0.) {
					np2 = (ANALYSIS(sdata)->dc.stop2-ANALYSIS(sdata)->dc.start2) / ANALYSIS(sdata)->dc.step2;
				}	
				ANALYSIS(sdata)->dc.sim_length = np1 * np2; 
				break;
			case TRANSFER:
			case DISTORTION:
			case NOISE:
			case POLE_ZERO:
			case SENSITIVITY:
				break;
			case ANALYSIS_UNKNOWN:
				g_error ("Unknown analysis: %s", analysis);
				break;
		}
		ngspice->priv->current = sdata;
	} else if (IS_THIS_ITEM(buf, SP_FLAGS) ) {
		char *f = buf + strlen (SP_FLAGS) + 1;
		if (strncmp (f, "complex", 7) == 0)
			is_complex = TRUE;
		else
			is_complex = FALSE;
	} else if (IS_THIS_ITEM (buf, SP_COMMAND)) {
		/* pass */
	} else if (IS_THIS_ITEM (buf, SP_N_VAR)) { 
		gint i, n = atoi (buf + strlen (SP_N_VAR));

		NG_DEBUG (g_strdup_printf ("NVAR %d", n));
		sdata->state = STATE_IDLE;
		sdata->n_variables = n;
		sdata->got_points  = 0;
		sdata->got_var     = 0;
		sdata->var_names   = (char**) g_new0 (gpointer, n);
		sdata->var_units   = (char**) g_new0 (gpointer, n);
		sdata->data        = (GArray**) g_new0 (gpointer, n);
		for (i = 0; i < n; i++) 
			sdata->data[i] = g_array_new (TRUE, TRUE, sizeof (double));
		sdata->min_data = g_new (double, n);
		sdata->max_data = g_new (double, n);
		for (i = 0; i < n; i++) {
			sdata->min_data[i] =  G_MAXDOUBLE;
			sdata->max_data[i] = -G_MAXDOUBLE;
		}
	} else if (IS_THIS_ITEM (buf, SP_N_POINTS)) { 
		sdata->state = STATE_IDLE;
		sdata->n_points = atoi (buf + strlen (SP_N_POINTS));
		NG_DEBUG (g_strdup_printf ("NPOINTS %d", sdata->n_points));
	} else if (IS_THIS_ITEM (buf, SP_VARIABLES)) { 
		NG_DEBUG ("In variables");
		sdata->state = IN_VARIABLES;
	} else if (IS_THIS_ITEM (buf, SP_BINARY)) { 
		NG_DEBUG ("Data Bynari");
		sdata->state = IN_VALUES;
		sdata->binary = TRUE;
		sdata->got_var = 0;
		sdata->got_points = 0;
	} else if (IS_THIS_ITEM (buf, SP_VALUES)) { 
		sdata->state = IN_VALUES;
		sdata->binary = FALSE;
		sdata->got_var = 0;
		sdata->got_points = 0;
	} else {
		if (ngspice->priv->analysis == NULL) {
			if (len > 1)
				schematic_log_append (ngspice->priv->schematic, buf);
			return;
		}

		switch (sdata->state) {
			case IN_VARIABLES:
				tmp = g_strsplit (buf, "\t", 0);
				sdata->var_names[sdata->got_var] = g_strdup (tmp[2]);
				sdata->var_units[sdata->got_var] = g_strdup (tmp[3]);
				send = strchr (sdata->var_units[sdata->got_var], '\n');
				if (send)
					*send = 0;
				g_strfreev (tmp);
			
				if ((sdata->got_var + 1) < sdata->n_variables)
					sdata->got_var++;
				break;
			case IN_VALUES:
				if (sdata->binary) {
					int i, j;
					double d, dimg;
					NG_DEBUG ("Reading Binary");
					for (i=0; i<sdata->n_points; i++) {
						for (j=0; j<sdata->n_variables; j++) {
							/* TODO : This show always the real part only. We need to detect
							 * the probe settings for this node, and show the correct
							 * value : real, imaginary, module or phase, of the complex number.
							 */
							fread (&d, sizeof (double), 1, fp);
							if (is_complex)
								fread (&dimg, sizeof (double), 1, fp);
							if (j == 0) {
								switch (sdata->type) {
									case TRANSIENT:
										ngspice->priv->progress = d / ANALYSIS(sdata)->transient.sim_length;
										break;
									case AC:
										ngspice->priv->progress = (d - ANALYSIS(sdata)->ac.start) / ANALYSIS(sdata)->ac.sim_length;
										break;
									case DC_TRANSFER:
										ngspice->priv->progress = ((gdouble) iter) / ANALYSIS(sdata)->ac.sim_length;
								}
								if (ngspice->priv->progress > 1.0)
									ngspice->priv->progress = 1.0;
								if (ngspice->priv->progress < 0.0)
									ngspice->priv->progress = 0.0;
								g_main_context_iteration (NULL, FALSE);
							}
							sdata->data[j] = g_array_append_val (sdata->data[j], d);

							/* Update the minimal and maximal values so far. */
							if (d < sdata->min_data[j])
								sdata->min_data[j] = d;
							if (d > sdata->max_data[j])
								sdata->max_data[j] = d;
						}
					}
					sdata->state = STATE_IDLE;
					NG_DEBUG ("Reading Binary Done");
				} else {
					if (sdata->got_var) 
						sscanf(buf, "\t%lf", &val);
					else
						sscanf(buf, "%d\t\t%lf", &iter, &val);
					if (sdata->got_var == 0) {
						switch (sdata->type) {
							case TRANSIENT:
								ngspice->priv->progress = val /	ANALYSIS(sdata)->transient.sim_length;					
								break;
							case AC:
								ngspice->priv->progress = (val - ANALYSIS(sdata)->ac.start) / ANALYSIS(sdata)->ac.sim_length;
								break;
							case DC_TRANSFER:
								ngspice->priv->progress = ((gdouble) iter) / ANALYSIS(sdata)->ac.sim_length;
						}
						if (ngspice->priv->progress > 1.0)
							ngspice->priv->progress = 1.0;
					}

					sdata->data[sdata->got_var] = g_array_append_val (sdata->data[sdata->got_var], val);

					/* Update the minimal and maximal values so far. */
					if (val < sdata->min_data[sdata->got_var])
						sdata->min_data[sdata->got_var] = val;
					if (val > sdata->max_data[sdata->got_var])
						sdata->max_data[sdata->got_var] = val;

					/* Check for the end of the point. */
					if (sdata->got_var + 1 == sdata->n_variables) {
						sdata->got_var = 0;
						sdata->got_points++;
					} else
						sdata->got_var++;
				}
				break;
			default:
				if (len > 1) {
					if (strstr (buf,"abort"))
						sdata->state = STATE_ABORT;
					schematic_log_append (ngspice->priv->schematic, buf); 
				}
				break;
		}
	}
}

