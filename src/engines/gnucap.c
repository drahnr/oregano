/*
 * gnucap.c
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
#include "gnucap.h"
#include "netlist.h"

// TODO Move analisys data and result to another file
#include "simulation.h"

typedef enum {
	STATE_IDLE,
	IN_VARIABLES,
	IN_VALUES,
	STATE_ABORT
} ParseDataState;

struct analisys_tag {
	gchar *name;
	guint len;
};

static struct analisys_tag analisys_tags[] = {
	{"#", 1},     /* OP */
	{"#Time", 5}, /* Transient */
	{"#DC", 3},   /* DC */
	{"#Freq", 5}, /* AC */
};

#define IS_THIS_ITEM(str,item)  (!strncmp(str,item.name,item.len))
#define GNUCAP_TITLE '#'
#define TAGS_COUNT (sizeof (analisys_tags) / sizeof (struct analisys_tag))

/* Parser STATUS */
struct _OreganoGnuCapPriv {
	GPid child_pid;
	gint child_stdout;
	GIOChannel *child_iochannel;
	gint child_iochannel_watch;
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

static void gnucap_class_init (OreganoGnuCapClass *klass);
static void gnucap_finalize (GObject *object);
static void gnucap_dispose (GObject *object);
static void gnucap_instance_init (GTypeInstance *instance, gpointer g_class);
static void gnucap_interface_init (gpointer g_iface, gpointer iface_data);
static gboolean gnucap_child_stdout_cb (GIOChannel *source, GIOCondition condition, OreganoGnuCap *gnucap);
static void gnucap_parse (gchar *raw, gint len, OreganoGnuCap *gnucap);

static GObjectClass *parent_class = NULL;

GType 
oregano_gnucap_get_type (void)
{
	static GType type = 0;
	if (type == 0) {
		static const GTypeInfo info = {
			sizeof (OreganoGnuCapClass),
			NULL,   /* base_init */
			NULL,   /* base_finalize */
			gnucap_class_init,   /* class_init */
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
gnucap_class_init (OreganoGnuCapClass *klass)
{
	GObjectClass *object_class;

	parent_class = g_type_class_peek_parent (klass);

	object_class = G_OBJECT_CLASS (klass);

	object_class->dispose = gnucap_dispose;
	object_class->finalize = gnucap_finalize;
}

static void
gnucap_finalize (GObject *object)
{
	SimulationData *data;
	OreganoGnuCap *gnucap;
	GList *lst;
	int i;

	gnucap = OREGANO_GNUCAP (object);

	lst = gnucap->priv->analysis;
	while (lst) {
		data = SIM_DATA (lst->data);
		for (i=0; i<data->n_variables; i++) {
			g_free (data->var_names[i]);
			g_free (data->var_units[i]);
		}
		g_free (data->var_names);
		g_free (data->var_units);
		for (i=0; i<data->n_points; i++)
			g_array_free (data->data[i], TRUE);
		g_free (data->min_data);
		g_free (data->max_data);

		g_free (lst->data);
		lst = lst->next;
	}
	g_list_free (gnucap->priv->analysis);
	gnucap->priv->analysis = NULL;

	parent_class->finalize (object);
}

static void
gnucap_dispose (GObject *object)
{
	parent_class->dispose (object);
}

static gboolean
gnucap_has_warnings (OreganoEngine *self)
{
	return FALSE;
}

static gboolean
gnucap_is_available (OreganoEngine *self)
{
	gchar *exe;
	exe = g_find_program_in_path ("gnucap");

	if (!exe) return FALSE; // gnucap not found

	g_free (exe);
	return TRUE;
}

static void
gnucap_generate_netlist (OreganoEngine *engine, const gchar *filename, GError **error)
{
	OreganoGnuCap *gnucap;
	Netlist output;
	SimOption *so;
	GList *list;
	FILE *file;
	GError *local_error = NULL;

	gnucap = OREGANO_GNUCAP (engine);

	netlist_helper_create (gnucap->priv->schematic, &output, &local_error);
	if (local_error != NULL) {
		g_propagate_error (error, local_error);
		return;
	}

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

	/* Include of subckt models */
	fputs ("*------------- Models -------------------------\n",file);
	list = output.models;
	while (list) {
		gchar *model;
		model = (gchar *)list->data;
		fprintf (file,".include %s/%s.model\n", OREGANO_MODELDIR, model);
		list = list->next;
	}
	
	fputs ("*------------- Circuit Description-------------\n",file);
	fputs (output.template->str,file);
	fputs ("\n*----------------------------------------------\n",file);

	/* Prints Transient Analisis */
	if (sim_settings_get_trans (output.settings)) {
		gchar *tmp_str = netlist_helper_create_analisys_string (output.store, FALSE);
		fprintf(file, ".print tran %s\n", tmp_str);
		g_free (tmp_str);

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
	OreganoGnuCap *gnucap = OREGANO_GNUCAP (self);

	gnucap->priv->progress += 0.1;
	(*d) = gnucap->priv->progress;
}

static void
gnucap_stop (OreganoEngine *self)
{
	OreganoGnuCap *gnucap = OREGANO_GNUCAP (self);
	g_io_channel_shutdown (gnucap->priv->child_iochannel, TRUE, NULL);
	g_source_remove (gnucap->priv->child_iochannel_watch);
	g_spawn_close_pid (gnucap->priv->child_pid);
	close (gnucap->priv->child_stdout);
}

static void
gnucap_watch_cb (GPid pid, gint status, OreganoGnuCap *gnucap)
{
	/* check for status, see man waitpid(2) */
	if (WIFEXITED (status)) {
		gchar *line;
		gint status, len;
		g_io_channel_read_to_end (gnucap->priv->child_iochannel, &line, &len, NULL);
		if (len > 0)
			gnucap_parse (line, len, gnucap);
		g_free (line);

		/* Free stuff */
		g_io_channel_shutdown (gnucap->priv->child_iochannel, TRUE, NULL);
		g_source_remove (gnucap->priv->child_iochannel_watch);
		g_spawn_close_pid (gnucap->priv->child_pid);
		close (gnucap->priv->child_stdout);

		gnucap->priv->current = NULL;

		if (gnucap->priv->num_analysis == 0) {
			schematic_log_append_error (gnucap->priv->schematic, _("### Too few or none analysis found ###\n"));
			gnucap->priv->aborted = TRUE;
			g_signal_emit_by_name (G_OBJECT (gnucap), "aborted");
		} else
			g_signal_emit_by_name (G_OBJECT (gnucap), "done");
	}
}

static gboolean
gnucap_child_stdout_cb (GIOChannel *source, GIOCondition condition, OreganoGnuCap *gnucap)
{
	gchar *line;
	gsize len, terminator;
	GIOStatus status;
	GError *error = NULL;

	status = g_io_channel_read_line (source, &line, &len, &terminator, &error);
	if ((status & G_IO_STATUS_NORMAL) && (len > 0)) {
		gnucap_parse (line, len, gnucap);
		g_free (line);
	}

	/* Let UI update */
	g_main_context_iteration (NULL, FALSE);
	return TRUE;
}

static void
gnucap_start (OreganoEngine *self)
{
	OreganoGnuCap *gnucap;
	GError *error = NULL;
	char *argv[] = {"gnucap", "-b", "/tmp/netlist.tmp", NULL};

	gnucap = OREGANO_GNUCAP (self);
	oregano_engine_generate_netlist (self, "/tmp/netlist.tmp", &error);
	if (error != NULL) {
		gnucap->priv->aborted = TRUE;
		schematic_log_append_error (gnucap->priv->schematic, error->message);
		g_signal_emit_by_name (G_OBJECT (gnucap), "aborted");
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
			&gnucap->priv->child_pid,
			NULL, /* STDIN */
			&gnucap->priv->child_stdout, /* STDOUT */
			NULL, /* STDERR*/
			&error
		)) {
		/* Add a watch for process status */
		g_child_watch_add (gnucap->priv->child_pid, (GChildWatchFunc)gnucap_watch_cb, gnucap);
		/* Add a GIOChannel to read from process stdout */
		gnucap->priv->child_iochannel = g_io_channel_unix_new (gnucap->priv->child_stdout);
		/* Watch the I/O Channel to read child strout */
		gnucap->priv->child_iochannel_watch = g_io_add_watch (gnucap->priv->child_iochannel,
			G_IO_IN|G_IO_PRI|G_IO_HUP|G_IO_NVAL, (GIOFunc)gnucap_child_stdout_cb, gnucap);
	} else {
		gnucap->priv->aborted = TRUE;
		schematic_log_append_error (gnucap->priv->schematic, _("Unable to execute GnuCap."));
		g_signal_emit_by_name (G_OBJECT (gnucap), "aborted");
	}
}

static GList*
gnucap_get_results (OreganoEngine *self)
{
	return OREGANO_GNUCAP (self)->priv->analysis;
}

const gchar*
gnucap_get_operation (OreganoEngine *self)
{
	OreganoGnuCapPriv *priv = OREGANO_GNUCAP (self)->priv;

	if (priv->current == NULL)
		return _("None");

	return oregano_engine_get_analysis_name (priv->current);
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
	klass->get_results = gnucap_get_results;
	klass->get_operation = gnucap_get_operation;
	klass->is_available = gnucap_is_available;
}

static void
gnucap_instance_init (GTypeInstance *instance, gpointer g_class)
{
	OreganoGnuCap *self = OREGANO_GNUCAP (instance);

	self->priv = g_new0 (OreganoGnuCapPriv, 1);
	self->priv->progress = 0.0;
	self->priv->char_last_newline = TRUE;
	self->priv->status = 0;
	self->priv->buf_count = 0;
	self->priv->num_analysis = 0;
	self->priv->analysis = NULL;
	self->priv->current = NULL;
	self->priv->aborted = FALSE;
}

OreganoEngine*
oregano_gnucap_new (Schematic *sc)
{
	OreganoGnuCap *gnucap;

	gnucap = OREGANO_GNUCAP (g_object_new (OREGANO_TYPE_GNUCAP, NULL));
	gnucap->priv->schematic = sc;

	return OREGANO_ENGINE (gnucap);
}

typedef struct {
	gchar *name;
	//gchar *unit;
} GCap_Variable;

GCap_Variable *_get_variables(gchar *str, gint *count)
{
	GCap_Variable *out;
	/* FIXME Improve the code */
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

void
_free_variables(GCap_Variable *v, gint count)
{
	int i;
	for(i=0; i<count; i++)
		g_free(v[i].name);
	g_free(v);
}

gdouble
strtofloat (char *s) {
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

/* Main method. Here we'll transform GnuCap output
 * into SimulationResults!
 */
static void
gnucap_parse (gchar *raw, gint len, OreganoGnuCap *gnucap)
{
	static SimulationData *sdata;
	static Analysis *data;
	GCap_Variable *variables;
	OreganoGnuCapPriv *priv = gnucap->priv;
	gint i, j, n;
	gdouble val;
	gchar *s;

	for (j=0; j < len; j++) {
		if (raw[j] != '\n') {
			priv->buf[priv->buf_count++] = raw[j];
			continue;
		}
		priv->buf[priv->buf_count] = '\0';

		//Got a complete line
		s = priv->buf;
		if (s[0] == GNUCAP_TITLE) {
			SimSettings *sim_settings;
			gdouble np1, np2;

			sim_settings = (SimSettings *)schematic_get_sim_settings (priv->schematic);

			data = g_new0 (Analysis, 1);
			priv->current = sdata = SIM_DATA (data);
			priv->analysis = g_list_append (priv->analysis, sdata);
			priv->num_analysis++;
			sdata->state = STATE_IDLE;
			sdata->type  = ANALYSIS_UNKNOWN;
			sdata->functions = NULL;

			/* Calculates the quantity of variables */
			variables = _get_variables(s, &n);

			for (i = 0; i < TAGS_COUNT; i++)
				if (IS_THIS_ITEM (variables[0].name, analisys_tags[i]))
					sdata->type = i;

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
			for (i = 0; i < n; i++) {
				sdata->var_names[i] = g_strdup (variables[i].name);
				switch (sdata->type) {
					case TRANSIENT:
						if (i==0)
							sdata->var_units[i] = g_strdup (_("time"));
						else {
							if (strstr (sdata->var_names[i], "db") != NULL) {
								sdata->var_units[i] = g_strdup ("db");
							} else
								sdata->var_units[i] = g_strdup (_("voltage"));
						}
					break;
					case AC:
						if (i == 0)
							sdata->var_units[i] = g_strdup (_("frequency"));
						else {
							if (strstr (sdata->var_names[i], "db") != NULL) {
								sdata->var_units[i] = g_strdup ("db");
							} else
								sdata->var_units[i] = g_strdup (_("voltage"));
						}
					break;
					default:
						sdata->var_units[i] = g_strdup ("");
				}
			}
			sdata->n_variables = n;

			switch (sdata->type) {
				case TRANSIENT:
					data->transient.sim_length =
						sim_settings_get_trans_stop  (sim_settings) -
						sim_settings_get_trans_start (sim_settings);
					data->transient.step_size =
						sim_settings_get_trans_step (sim_settings);
				break;
				case AC:
					data->ac.start = sim_settings_get_ac_start (sim_settings);
					data->ac.stop  = sim_settings_get_ac_stop  (sim_settings);
					data->ac.sim_length = sim_settings_get_ac_npoints (sim_settings);
				break;
				case OP_POINT:
				case DC_TRANSFER:
					np1 = np2 = 1.;
					data->dc.start1 = sim_settings_get_dc_start (sim_settings,0);
					data->dc.stop1  = sim_settings_get_dc_stop  (sim_settings,0);
					data->dc.step1  = sim_settings_get_dc_step  (sim_settings,0);
					data->dc.start2 = sim_settings_get_dc_start (sim_settings,1);
					data->dc.stop2  = sim_settings_get_dc_stop  (sim_settings,1);
					data->dc.step2  = sim_settings_get_dc_step  (sim_settings,1);
					np1 = (data->dc.stop1 - data->dc.start1) / data->dc.step1;
					if (data->dc.step2 != 0.0) {
						np2 = (data->dc.stop2 - data->dc.start2) / data->dc.step2;
					}
					data->dc.sim_length = np1 * np2;
				break;
				case TRANSFER:
				case DISTORTION:
				case NOISE:
				case POLE_ZERO:
				case SENSITIVITY:
				break;
				case ANALYSIS_UNKNOWN:
					g_error (_("Unknown analysis"));
				break;
			}
		} else {
			if ((priv->analysis == NULL) || (isalpha (s[0]))) {
				if (priv->buf_count > 1) {
					schematic_log_append (priv->schematic, s);
					schematic_log_append (priv->schematic, "\n");
				}
				priv->buf_count = 0;
				continue;
			}

			switch (sdata->state) {
				case IN_VALUES:
					val = strtofloat (s);
					switch (sdata->type) {
						case TRANSIENT:
							priv->progress = val / data->transient.sim_length;
						break;
						case AC:
							priv->progress = (val - data->ac.start) / data->ac.sim_length;
						break;
						case DC_TRANSFER:
							priv->progress = val / data->ac.sim_length;
					}
					if (priv->progress > 1.0)
						priv->progress = 1.0;

					variables = _get_variables (s, &n);
					for (i = 0; i < n; i++) {
						val = strtofloat (variables[i].name);

						sdata->data[i] = g_array_append_val (sdata->data[i], val);

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
					if (priv->buf_count > 1) {
						if (strstr (s, _("abort")))
							sdata->state = STATE_ABORT;
						schematic_log_append_error (priv->schematic, s);
					}
			}
		}
		priv->buf_count = 0;
	}
}

