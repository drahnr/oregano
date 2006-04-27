/*
 * sim-engine.c
 *
 *
 * Authors:
 *  Richard Hult <rhult@hem.passagen.se>
 *  Ricardo Markiewicz <rmarkie@fi.uba.ar>
 *  Andres de Barbara <adebarbara@fi.uba.ar>
 *
 * Web page: http://arrakis.lug.fi.uba.ar/
 *
 * Copyright (C) 1999-2001  Richard Hult
 * Copyright (C) 2003,2005  LUGFI
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
/**
 * Simulate engine class, interface to SPICE.
 */

#include <gnome.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/stat.h>

#include "main.h" /* get rid of this... */
#include "sim-settings.h"
#include "sim-engine.h"
#include "schematic.h"

#define READ 0
#define WRITE 1

static void sim_engine_class_init (SimEngineClass *klass);

static void sim_engine_init (SimEngine *engine);

static void data_input_cb (SimEngine *engine,
	gint source,
	GdkInputCondition condition);

static void error_input_cb (SimEngine *engine,
	gint source,
	GdkInputCondition condition);

enum {
	DONE,
	ABORTED,
	CANCELLED,
	LAST_SIGNAL
};

typedef enum {
	STATE_IDLE,
	IN_VARIABLES,
	IN_VALUES,
	STATE_ABORT
} ParseDataState;

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
	N_("Unknown Analysis"),
	NULL
};
static char *analisys_tags[] = {
	"#",	 /* OP		  */
	"#Time", /* Transient */
	"#DC",	 /* DC		  */
	"#Freq", /* AC		  */
	NULL
};

#define IS_THIS_ITEM(str,item)  (!strncmp(str,item,strlen(item)))


#define GNUCAP_TITLE "#"

typedef struct _gnucap_variable_ {
	gchar *name;
	//gchar *unit;
} GCap_Variable;


static guint sim_engine_signals [LAST_SIGNAL] = { 0 };
static GObject *parent_class = NULL;

GType
sim_engine_get_type (void)
{
	static GType sim_engine_type = 0;

	if (!sim_engine_type) {
		static const GTypeInfo sim_engine_info = {
			sizeof (SimEngineClass),
			NULL,
			NULL,
			(GClassInitFunc) sim_engine_class_init,
			NULL,
			NULL,
			sizeof (SimEngine),
			0,
			(GInstanceInitFunc) sim_engine_init,
			NULL
		};

		sim_engine_type = g_type_register_static(G_TYPE_OBJECT, "SimEngine",
			&sim_engine_info, 0);
	}

	return sim_engine_type;
}

static void
sim_engine_finalize(GObject *object)
{
	SimEngine      *engine;
	SimulationData *sdat;
	GList          *analysis;
	int i, num;

	engine = SIM_ENGINE (object);

	analysis = engine->analysis;
	for ( ; analysis; analysis = analysis->next ) {
		sdat = SIM_DATA (analysis->data);
		num = sdat->n_variables;

		if (sdat->var_names) g_free(sdat->var_names);
		if (sdat->var_units) g_free(sdat->var_units);

		for (i = 0; i < num; i++) {
			if (sdat->data[i])
				g_array_free (sdat->data[i], FALSE);
		}

		g_free(sdat->data);
		g_free(sdat->min_data);
		g_free(sdat->max_data);
		g_free(sdat);
		analysis->data = NULL;
	}

	g_list_free (engine->analysis);
	engine->analysis = NULL;

	G_OBJECT_CLASS(parent_class)->finalize(object);
}

static void
sim_engine_class_init (SimEngineClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS(klass);
	parent_class = g_type_class_peek(G_TYPE_OBJECT);

	object_class->finalize = sim_engine_finalize;

	sim_engine_signals [DONE] =
		g_signal_new ("done",
			G_TYPE_FROM_CLASS(object_class),
			G_SIGNAL_RUN_FIRST,
			G_STRUCT_OFFSET (SimEngineClass, done),
			NULL,
			NULL,
			g_cclosure_marshal_VOID__VOID,
			G_TYPE_NONE, 0);

	sim_engine_signals [ABORTED] =
		g_signal_new ("aborted",
			G_TYPE_FROM_CLASS(object_class),
			G_SIGNAL_RUN_FIRST,
			G_STRUCT_OFFSET(SimEngineClass, aborted),
			NULL,
			NULL,
			g_cclosure_marshal_VOID__VOID,
			G_TYPE_NONE, 0);

	sim_engine_signals [CANCELLED] =
		g_signal_new ("cancelled",
			G_TYPE_FROM_CLASS(object_class),
			G_SIGNAL_RUN_FIRST,
			G_STRUCT_OFFSET(SimEngineClass, cancelled),
			NULL,
			NULL,
			g_cclosure_marshal_VOID__VOID,
			G_TYPE_NONE, 0);
}

static void
sim_engine_init (SimEngine *engine)
{
	engine->running = FALSE;
	engine->progress = 0.0;

	engine->child_pid = 0;

	engine->data_input_id = 0;
	engine->error_input_id = 0;

	engine->inputfp = NULL;
	engine->outputfp = NULL;
	engine->errorfp = NULL;

	engine->has_warnings = FALSE;
	engine->has_errors = FALSE;
	engine->aborted = FALSE;

	engine->num_analysis=0;
	engine->analysis=0;
}

SimEngine *
sim_engine_new (Schematic *sm)
{
	SimEngine *engine;

	engine = SIM_ENGINE(g_object_new(TYPE_SIM_ENGINE, 0));

	engine->sm = sm;
	engine->sim_settings = schematic_get_sim_settings (sm);

	return engine;
}

void
sim_engine_start_with_file (SimEngine *engine, const gchar *netlist)
{
	int netlist_fd;

	if (pipe (engine->to_child) < 0)
		g_error ("Error creating stdin pipe");
	if (pipe (engine->to_parent) < 0)
		g_error ("Error creating stdout pipe");
	if (pipe (engine->to_parent_error) < 0)
		g_error ("Error creating stderr pipe");

	/* TODO Would be recomendable to use pthread? */
	engine->child_pid = fork();
	if (engine->child_pid == 0) {
		setpgrp ();
		/* Now oregano.simtype has gnucap or ngspice */
		gchar *simexec = oregano.simexec;
        /* !!!!!!!!!!! "-s" "-n" */
		gchar *args[4] = { simexec, oregano.simtype, (gchar *)netlist, NULL };


		/* The child executes here. */
		/*netlist_fd = open (netlist, O_RDONLY);
		if (netlist_fd == -1)
			g_error ("Error opening netlist.");*/

		close (engine->to_child[WRITE]);
		close (engine->to_parent[READ]);
		close (engine->to_parent_error[READ]);

/*		dup2 (netlist_fd, STDIN_FILENO);
		close (netlist_fd);*/
		close (engine->to_child[READ]);

		/* Map stderr and stdout to their pipes. */
		dup2 (engine->to_parent[WRITE], STDOUT_FILENO);
		dup2 (engine->to_parent_error[WRITE], STDERR_FILENO);
		close (engine->to_parent[WRITE]);
		close (engine->to_parent_error[WRITE]);

		//setlocale (LC_NUMERIC, "C");

		/* Execute Sim Engine. */
		/* SimExec deberia apuntar a oregano_parser.pl con
		 * path y todo
		 */
		execvp(simexec, args);

		/* We should never get here. */
		g_warning ("Error executing the simulation engine.");
		_exit (1);
	}

	if (engine->child_pid == -1) {
		g_warning ("Could not fork child process.");
		return;
	}

	/* The parent executes here. */

	schematic_log_clear (engine->sm);

	/* Set up communication. */
	close (engine->to_child[READ]);
	close (engine->to_parent[WRITE]);
	close (engine->to_parent_error[WRITE]);

	engine->errorfp = fdopen (engine->to_parent_error[READ], "r");
	engine->inputfp = fdopen (engine->to_parent[READ], "r");
	engine->outputfp = fdopen (engine->to_child[WRITE], "w");
	if (engine->inputfp == NULL || engine->outputfp == NULL) {
		g_warning ("Error executing child process");
		return;
	}

	engine->running = TRUE;
	// Dejo solo el parse de Gnucap, ahora todos vienen igual
	engine->data_input_id = gdk_input_add (
		engine->to_parent[READ],
		GDK_INPUT_READ,
		(GdkInputFunction) data_input_cb,
		(gpointer) engine
	);

	engine->error_input_id = gdk_input_add (
		engine->to_parent_error[READ],
		GDK_INPUT_READ,
		(GdkInputFunction) error_input_cb,
		(gpointer) engine);
}

void
sim_engine_stop (SimEngine *engine)
{
	int status;

	g_return_if_fail (engine != NULL);
	g_return_if_fail (IS_SIM_ENGINE (engine));

	if (!engine->running)
		return;

	gdk_input_remove (engine->data_input_id);
	gdk_input_remove (engine->error_input_id);
	engine->data_input_id = 0;
	engine->error_input_id = 0;

	kill (-engine->child_pid, SIGTERM);

	waitpid (engine->child_pid, &status, WUNTRACED);
	engine->child_pid = 0;

	fclose (engine->errorfp);
	fclose (engine->inputfp);
	fclose (engine->outputfp);

	engine->running = FALSE;
}

static void
error_input_cb (SimEngine *engine, gint source, GdkInputCondition condition)
{
	static gchar buf[256];

	if (!(condition & GDK_INPUT_READ))
		return;

	/*
	 * Store the error messages.
	 */
	if (fgets (buf, 255, engine->errorfp) != NULL) {
		if (strncmp (buf, "@@@", 3) != 0) {
			if (strstr (buf, "abort")) {
				engine->aborted = TRUE;
			}

			schematic_log_append (engine->sm, buf);
			engine->has_warnings = TRUE;
		}
	}
}

gboolean
sim_engine_get_op_value (SimEngine *engine, char *node_name, double *value)
{
	int i;
	GList *analysis;
	SimulationData *sdat = NULL;
	/*
	 * FIXME: build a hash table with the values.
	 */

	g_return_val_if_fail (engine != NULL, FALSE);
	g_return_val_if_fail (IS_SIM_ENGINE (engine), FALSE);
	g_return_val_if_fail (node_name != NULL, FALSE);

	/* FIXME : Ver que se supone que hace :-) */
	return FALSE;

	/* Look for the OP analysis. */
	for (analysis = engine->analysis; analysis; analysis = analysis->next ) {
		if (SIM_DATA (analysis->data)->type == OP_POINT) {
			sdat = SIM_DATA (analysis->data);
			break;
		}
	}

	for (i = 0; i < sdat->n_variables; i++) {
		if (g_strcasecmp (sdat->var_names[i], node_name) == 0) {
			*value = g_array_index (sdat->data[i], double, 0);

			return TRUE;
		}
	}

	return FALSE;
}

gchar *
sim_engine_analysis_name (SimulationData *sdat)
{
	if (sdat == NULL)
		return g_strdup (_(analysis_names[ANALYSIS_UNKNOWN]));

	return g_strdup (_(analysis_names[sdat->type]));
}

void _free_variables(GCap_Variable *v, gint count)
{
	int i;
	for(i=0; i<count; i++)
		g_free(v[i].name);
	g_free(v);
}

GCap_Variable *_get_variables(gchar *str, gint *count)
{
	GCap_Variable *out;
	/* FIXME hacer mas bonito esto */
	gchar *tmp[100];
	gchar *ini, *fin;
	gint i = 0;

	// Don't USE!. Is not smarty !!
	// It generate empty strings that really sucks all!!!
	//arr = g_strsplit_set (str, " ", -1);

	i = 0;
	ini = str;
	/* saco espacios adelante */
	while (isspace(*ini)) ini++;
	fin = ini;
	while (*fin != '\0') {
		if (isspace(*fin)) {
			*fin = '\0';
			tmp[i] = g_strdup(ini);
			*fin = ' ';
			i++;
			ini = fin;
			while (isspace(*ini)) ini++;
			fin = ini;
		} else fin++;
	}

	if (i == 0) {
		g_warning ("NO COLUMNS FOUND\n");
		return NULL;
	}

	out = g_new0 (GCap_Variable, i);
	(*count) = i;
	for ( i=0; i<(*count); i++ ) {
		out[i].name = tmp[i];
	}

	return out;
}

gdouble strtofloat(char *s) {
	gdouble val;
	char *error;

	val = strtod(s, &error);
	/* If the value looks like : 100.u, adjust it */
	/* We need this because GNU Cap's or ngSpice float notation */
	switch (error[0]) {
		case 'u':
			val /= 1000000;
		break;
		case 'n':
			val /= 1000000;
			val /= 1000;
		break;
		case 'p':
			val /= 1000000;
			val /= 1000000;
		break;
		case 'f':
			val /= 100;
		break;
		case 'K':
			val *= 1000;
		break;
		default:
			if (strcmp(error, "Meg") == 0) val *= 1000000;
	}

	return val;
}

static void
data_input_cb (SimEngine *engine, gint source, GdkInputCondition condition)
{
	static gchar buf[256];
	static SimulationData *sdata = NULL;
	gint status, iter, n, i;
	gdouble val, np1, np2;
	GCap_Variable *variables;
	gchar **tmp = NULL;
	gchar *send;

	if (!(condition & GDK_INPUT_READ))
		return;

	/* inputfp is the file descriptor of the output */
	if (fgets (buf, 255, engine->inputfp) == NULL){
		/* If there's no more input, we reached the end. */
		engine->running = FALSE;

		gdk_input_remove (engine->data_input_id);
		gdk_input_remove (engine->error_input_id);

		kill (engine->child_pid, SIGKILL);
		waitpid (engine->child_pid, &status, WUNTRACED);

		fclose (engine->errorfp);
		fclose (engine->inputfp);
		fclose (engine->outputfp);

		if (!sdata || engine->aborted || engine->num_analysis < 1) {
			if (engine->num_analysis < 1) {
				schematic_log_append (engine->sm,
					_("\n\n### Too few or none analysis found ###\n\n"));
			}
			engine->has_errors = TRUE;
			g_signal_emit_by_name (G_OBJECT (engine), "aborted");
		}	else {
			g_signal_emit_by_name (G_OBJECT (engine), "done");
		}

		return;
	}

	if (IS_THIS_ITEM (buf, GNUCAP_TITLE)) {
		gchar *analysis = buf; //+strlen(SP_PLOT_NAME)+1;

		sdata = SIM_DATA (g_new0 (Analysis, 1));
		engine->analysis = g_list_append (engine->analysis, sdata);
		engine->num_analysis++;
		sdata->state = STATE_IDLE;
		sdata->type  = ANALYSIS_UNKNOWN;
		sdata->functions = NULL;

		/* Calculates the quantity of variables */
		variables = _get_variables(buf, &n);

		for (i = 0; analisys_tags[i]; i++)
			if (IS_THIS_ITEM(variables[0].name, analisys_tags[i])) {
				sdata->type = i;
				// Don't use break;
			}

		sdata->state = IN_VALUES;
		sdata->n_variables = n;
		sdata->got_points  = 0;
		sdata->got_var	   = 0;
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
		for(i=0; i<n; i++) {
			sdata->var_names[i] = g_strdup(variables[i].name);
			switch (sdata->type) {
			case TRANSIENT:
				if (i==0)
					sdata->var_units[i] = g_strdup("time");
				else {
					if (strstr (sdata->var_names[i], "db") != NULL) {
						sdata->var_units[i] = g_strdup("db");
					} else
						sdata->var_units[i] = g_strdup("voltage");
				}
				break;
			case AC:
				if (i==0)
					sdata->var_units[i] = g_strdup("frequency");
				else {
					if (strstr (sdata->var_names[i], "db") != NULL) {
						sdata->var_units[i] = g_strdup("db");
					} else
						sdata->var_units[i] = g_strdup("voltage");
				}
				break;
			default:
				sdata->var_units[i] = g_strdup("");
			}
		}
		sdata->n_variables = n;

		switch ( sdata->type ) {
		case TRANSIENT:
			ANALYSIS(sdata)->transient.sim_length =
				sim_settings_get_trans_stop  (engine->sim_settings) -
				sim_settings_get_trans_start (engine->sim_settings);
			ANALYSIS(sdata)->transient.step_size =
				sim_settings_get_trans_step (engine->sim_settings);
			break;
		case AC:
			ANALYSIS(sdata)->ac.start =
				sim_settings_get_ac_start (engine->sim_settings);
			ANALYSIS(sdata)->ac.stop  =
				sim_settings_get_ac_stop  (engine->sim_settings);
			ANALYSIS(sdata)->ac.sim_length =
				sim_settings_get_ac_npoints (engine->sim_settings);
			break;
		case OP_POINT:
		case DC_TRANSFER:
			np1 = np2 = 1.;
			ANALYSIS(sdata)->dc.start1 =
				sim_settings_get_dc_start (engine->sim_settings,0);
			ANALYSIS(sdata)->dc.stop1  =
				sim_settings_get_dc_stop  (engine->sim_settings,0);
			ANALYSIS(sdata)->dc.step1  =
				sim_settings_get_dc_step  (engine->sim_settings,0);
			ANALYSIS(sdata)->dc.start2 =
				sim_settings_get_dc_start (engine->sim_settings,1);
			ANALYSIS(sdata)->dc.stop2  =
				sim_settings_get_dc_stop  (engine->sim_settings,1);
			ANALYSIS(sdata)->dc.step2  =
				sim_settings_get_dc_step  (engine->sim_settings,1);

			np1 = (ANALYSIS(sdata)->dc.stop1-ANALYSIS(sdata)->dc.start1) /
				ANALYSIS(sdata)->dc.step1;
			if ( ANALYSIS(sdata)->dc.step2 != 0. ) {
				np2 = (ANALYSIS(sdata)->dc.stop2-ANALYSIS(sdata)->dc.start2) /
					ANALYSIS(sdata)->dc.step2;
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
			g_error("Unknown analysis: %s",analysis);
			break;
		}
	} else {
		if ((engine->analysis == 0) || (isalpha (buf[0]))) {
			if (strlen (buf) > 1)
				schematic_log_append (engine->sm, buf);
			return;
		}

		switch (sdata->state) {
		case IN_VALUES:
			val = strtofloat(buf);
			switch (sdata->type) {
			case TRANSIENT:
				engine->progress = val / ANALYSIS(sdata)->transient.sim_length;
				break;
			case AC:
				engine->progress = (val - ANALYSIS(sdata)->ac.start) /
					ANALYSIS(sdata)->ac.sim_length;
				break;
			case DC_TRANSFER:
				engine->progress = ((gdouble) iter) /
					ANALYSIS(sdata)->ac.sim_length;
			default:
				break;
			}
			if (engine->progress > 1.0)
				engine->progress = 1.0;

			variables = _get_variables(buf, &n);
			for(i=0; i<n; i++) {
				val = strtofloat(variables[i].name);

				sdata->data[i] = g_array_append_val( sdata->data[i], val);

				/* Update the minimal and maximal values so far. */
				if (val < sdata->min_data[i])
					sdata->min_data[i] = val;
				if (val > sdata->max_data[i])
					sdata->max_data[i] = val;
			}

			_free_variables(variables, n);
			sdata->got_points++;
			sdata->got_var = n;
			break;
		default:
			if (strlen (buf) > 1) {
				if (strstr (buf,"abort"))
					sdata->state = STATE_ABORT;
				schematic_log_append (engine->sm, buf);
			}
			break;

		}
	}
}

double
sim_engine_do_function (SimulationFunctionType type, double first, double second)
{
	double outval = 0.0f;

	switch (type) {
		case FUNCTION_MINUS:
			outval = first - second;
		break;
		case FUNCTION_TRANSFER:
			if (second < 0.000001f) {
				/* TODO */
				if (first < 0)
					outval = -G_MAXDOUBLE;
				else
					outval = G_MAXDOUBLE;
			} else {
				outval = first / second;
			}
		break;
	}
	return outval;
}

