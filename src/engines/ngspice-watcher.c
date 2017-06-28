/*
 * ngspice-watcher.c
 *
 *
 * Authors:
 *  Michi <st101564@stud.uni-stuttgart.de>
 *
 * Web page: https://ahoi.io/project/oregano
 *
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
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include <glib.h>
#include <glib/gprintf.h>
#include <math.h>

#include "../tools/thread-pipe.h"
#include "ngspice.h"
#include "ngspice-analysis.h"
#include "../log-interface.h"
#include "ngspice-watcher.h"

enum ERROR_STATE {
	ERROR_STATE_NO_ERROR,
	ERROR_STATE_NO_SUCH_FILE_OR_DIRECTORY,
	ERROR_STATE_ERROR_IN_NETLIST
};

//data wrapper
typedef struct {
	GMutex mutex;
	GCond cond;
	gboolean boolean;
} IsNgspiceStderrDestroyed;

//data wrapper
typedef struct {
	gchar *path_to_file;
	ThreadPipe *pipe;
	CancelInfo *cancel_info;
} NgSpiceSaverResources;

//data wrapper
typedef struct {
	ThreadPipe *thread_pipe_worker;
	ThreadPipe *thread_pipe_saver;
} NgSpiceWatchForkResources;

//data wrapper
typedef struct {
	NgSpiceWatchForkResources ngspice_watch_fork_resources;
	guint cancel_info_count;
	CancelInfo *cancel_info;
} NgSpiceWatchSTDOUTResources;

//data wrapper
typedef struct {
	ProgressResources *progress_ngspice;
	LogInterface log;
	const SimSettings *sim_settings;
	enum ERROR_STATE *error_state;
	IsNgspiceStderrDestroyed *is_ngspice_stderr_destroyed;
} NgSpiceWatchSTDERRResources;

//data wrapper
typedef struct {
	GThread *worker;
	GThread *saver;
	LogInterface log;
	const void* emit_instance;
	GPid *child_pid;
	gboolean *aborted;
	guint *num_analysis;
	GMainLoop *main_loop;
	gchar *netlist_file;
	gchar *ngspice_result_file;
	enum ERROR_STATE *error_state;
	IsNgspiceStderrDestroyed *is_ngspice_stderr_destroyed;
	CancelInfo *cancel_info;
} NgSpiceWatcherWatchNgSpiceResources;

/**
 * Wraps the heavy work of a function into a thread.
 */
static gpointer ngspice_worker (NgspiceAnalysisResources *resources) {

	ngspice_analysis(resources);

	cancel_info_unsubscribe(resources->cancel_info);
	g_free(resources);

	return NULL;
}

/**
 * Wraps the heavy work of a function into a thread.
 */
static gpointer ngspice_saver (NgSpiceSaverResources *resources)
{
	ngspice_save(resources->path_to_file, resources->pipe, resources->cancel_info);

	cancel_info_unsubscribe(resources->cancel_info);
	g_free(resources->path_to_file);
	g_free(resources);

	return NULL;
}

/**
 * returns the number of strings in a NULL terminated array of strings
 */
static int get_count(gchar** array) {
	int i = 0;
	while (array[i] != NULL)
		i++;
	return i;
}

/**
 * adds the line number followed by a colon and a space at the beginning of each line
 */
static void add_line_numbers(gchar **string) {
	gchar **splitted = g_regex_split_simple("\\n", *string, 0, 0);
	GString *new_string = g_string_new("");
	//why -1? Because g_regex_split_simple adds one empty string too much at the end
	//of the array.
	int count = get_count(splitted) - 1;
	int max_length = floor(log10((double) count)) + 1;
	//splitted[i+1] != NULL (why not only i but i+1?) because g_regex_split_simple
	//adds one empty string too much at the end of the array
	for (int i = 0; splitted[i+1] != NULL; i++)
		g_string_append_printf(new_string, "%0*d: %s\n", max_length, i+1, splitted[i]);
	//remove the last newline, which was added additionally
	new_string = g_string_truncate(new_string, new_string->len - 1);
	g_free(*string);
	*string = new_string->str;
	g_string_free(new_string, FALSE);
}

//data wrapper
typedef struct {
	const void* emit_instance;
	gchar *signal_name;
} NgspiceEmitData;

/**
 * Use this function to return the program main control flow to the
 * main thread (which is the gui thread).
 */
static gboolean g_signal_emit_by_name_main_thread(NgspiceEmitData *data) {
	const void* emit_instance = data->emit_instance;
	gchar *signal_name = data->signal_name;
	g_free(data);
	g_signal_emit_by_name (G_OBJECT (emit_instance), signal_name);
	g_free(signal_name);
	return G_SOURCE_REMOVE;
}

/**
 * main function of the ngspice watcher
 */
static gpointer ngspice_watcher_main(GMainLoop *main_loop) {
	g_main_loop_run(main_loop);

	// unrefs its GMainContext by 1
	g_main_loop_unref(main_loop);

	return NULL;
}

/**
 * forks data to file and heap
 */
static void ngspice_watcher_fork_data(NgSpiceWatchForkResources *resources, gpointer data, gsize size) {
	thread_pipe_push(resources->thread_pipe_worker, data, size);
	/**
	 * size_in = size - 1, because the trailing 0 of the string should not
	 * be written to file.
	 */
	thread_pipe_push(resources->thread_pipe_saver, data, size - 1);
}

/**
 * forks eof to file-pipe and heap-pipe
 */
static void ngspice_watcher_fork_eof(NgSpiceWatchForkResources *resources) {
	thread_pipe_set_write_eof(resources->thread_pipe_worker);
	thread_pipe_set_write_eof(resources->thread_pipe_saver);
}

/**
 * Does not handle input resources.
 */
static gboolean ngspice_watcher_watch_stdout_resources(GIOChannel *channel, GIOCondition condition, NgSpiceWatchSTDOUTResources *resources) {
	gchar *str_return = NULL;
	gsize length;
	gsize terminator_pos;
	GError *error = NULL;

	resources->cancel_info_count++;
	if (resources->cancel_info_count % 50 == 0 && cancel_info_is_cancel(resources->cancel_info)) {
		return G_SOURCE_REMOVE;
	}

	GIOStatus status = g_io_channel_read_line(channel, &str_return, &length, &terminator_pos, &error);
	if (error) {
		gchar *message = g_strdup_printf ("spice pipe stdout: %s - %i", error->message, error->code);
		g_printf("%s", message);
		g_free(message);
		g_clear_error (&error);
	} else if (status == G_IO_STATUS_NORMAL && length > 0) {
		ngspice_watcher_fork_data(&resources->ngspice_watch_fork_resources, str_return, length + 1);
	} else if (status == G_IO_STATUS_EOF) {
		return G_SOURCE_REMOVE;
	}
	if (str_return)
		g_free(str_return);
	return G_SOURCE_CONTINUE;
}

/**
 * ngspice reading source function
 *
 * reads the pipe (stdout) of ngspice
 */
static gboolean ngspice_watcher_watch_stdout(GIOChannel *channel, GIOCondition condition, NgSpiceWatchSTDOUTResources *resources) {

	gboolean g_source_continue = ngspice_watcher_watch_stdout_resources(channel, condition, resources);

	if (g_source_continue == G_SOURCE_CONTINUE)
		return G_SOURCE_CONTINUE;

	ngspice_watcher_fork_eof(&resources->ngspice_watch_fork_resources);
	g_source_destroy(g_main_current_source());
	cancel_info_unsubscribe(resources->cancel_info);
	g_free(resources);
	return G_SOURCE_REMOVE;
}

static void ngspice_watch_ngspice_resources_finalize(NgSpiceWatcherWatchNgSpiceResources *resources) {
	g_source_destroy(g_main_current_source());
	g_main_loop_quit(resources->main_loop);
	g_spawn_close_pid (*resources->child_pid);
	*resources->child_pid = 0;

	g_free(resources->ngspice_result_file);
	g_free(resources->netlist_file);
	g_free(resources->error_state);
	g_mutex_clear(&resources->is_ngspice_stderr_destroyed->mutex);
	g_cond_clear(&resources->is_ngspice_stderr_destroyed->cond);
	g_free(resources);
}

static void print_additional_info(LogInterface log, const gchar *ngspice_result_file, const gchar *netlist_file) {
	log.log_append_error(log.log, "\n### spice output: ###\n\n");
	gchar *ngspice_error_contents = NULL;
	gsize ngspice_error_length;
	GError *ngspice_error_read_error = NULL;

	g_file_get_contents(ngspice_result_file, &ngspice_error_contents, &ngspice_error_length, &ngspice_error_read_error);
	add_line_numbers(&ngspice_error_contents);
	log.log_append_error(log.log, ngspice_error_contents);
	g_free(ngspice_error_contents);
	if (ngspice_error_read_error != NULL)
		g_error_free(ngspice_error_read_error);

	gchar *netlist_contents = NULL;
	gsize netlist_lentgh;
	GError *netlist_read_error = NULL;
	g_file_get_contents(netlist_file, &netlist_contents, &netlist_lentgh, &netlist_read_error);
	add_line_numbers(&netlist_contents);
	log.log_append_error(log.log, "\n\n### netlist: ###\n\n");
	log.log_append_error(log.log, netlist_contents);
	g_free(netlist_contents);
	if (netlist_read_error != NULL)
		g_error_free(netlist_read_error);
}

enum NGSPICE_WATCHER_RETURN_VALUE {
	NGSPICE_WATCHER_RETURN_VALUE_DONE,
	NGSPICE_WATCHER_RETURN_VALUE_ABORTED,
	NGSPICE_WATCHER_RETURN_VALUE_CANCELED
};

/**
 * Does not care about input resource handling.
 */
static enum NGSPICE_WATCHER_RETURN_VALUE
ngspice_watcher_watch_ngspice_resources (GPid pid, gint status, NgSpiceWatcherWatchNgSpiceResources *resources) {
	GThread *worker = resources->worker;
	GThread *saver = resources->saver;
	LogInterface log = resources->log;
	guint *num_analysis = resources->num_analysis;
	enum ERROR_STATE *error_state = resources->error_state;
	IsNgspiceStderrDestroyed *is_ngspice_stderr_destroyed = resources->is_ngspice_stderr_destroyed;

	// wait for stderr to finish reading
	g_mutex_lock(&is_ngspice_stderr_destroyed->mutex);
	while (!is_ngspice_stderr_destroyed->boolean)
		g_cond_wait(&is_ngspice_stderr_destroyed->cond, &is_ngspice_stderr_destroyed->mutex);
	g_mutex_unlock(&is_ngspice_stderr_destroyed->mutex);

	GError *exit_error = NULL;
	gboolean exited_normal = g_spawn_check_exit_status(status, &exit_error);
	if (exit_error != NULL)
		g_error_free(exit_error);

	g_thread_join(worker);

	if (cancel_info_is_cancel(resources->cancel_info))
		return NGSPICE_WATCHER_RETURN_VALUE_CANCELED;


	if (!exited_normal) {
		// check for exit via return in main, exit() or _exit() of the child, see man
		// waitpid(2)
		//       WIFEXITED(wstatus)
		//              returns true if the child terminated normally, that is, by call‐
		//              ing exit(3) or _exit(2), or by returning from main().
		if (!(WIFEXITED (status)))
			log.log_append_error(log.log, "### spice exited with exception ###\n");
		else
			log.log_append_error(log.log, "### spice exited abnormally ###\n");

		g_thread_join(saver);

		switch (*error_state) {
		case ERROR_STATE_NO_ERROR:
			log.log_append_error(log.log, "### unknown error detected ###\n");
			log.log_append_error(log.log, "The following information might help you to analyze the error.\n");

			print_additional_info(log, resources->ngspice_result_file, resources->netlist_file);
			break;
		case ERROR_STATE_NO_SUCH_FILE_OR_DIRECTORY:
			log.log_append_error(log.log, "spice could not simulate because netlist generation failed.\n");
			break;
		case ERROR_STATE_ERROR_IN_NETLIST:
			log.log_append_error(log.log, "### netlist error detected ###\n");
			log.log_append_error(log.log, "You made a mistake in the simulation settings or part properties.\n");
			log.log_append_error(log.log, "The following information will help you to analyze the error.\n");

			print_additional_info(log, resources->ngspice_result_file, resources->netlist_file);
			break;
		}

		return NGSPICE_WATCHER_RETURN_VALUE_ABORTED;
	}
	// saver not needed any more. It could have been needed by error handling.
	g_thread_unref(saver);

	if (*num_analysis == 0) {
		log.log_append_error(log.log, _("### Too few or none analysis found ###\n"));
		return NGSPICE_WATCHER_RETURN_VALUE_ABORTED;
	}

	return NGSPICE_WATCHER_RETURN_VALUE_DONE;

}

/**
 * function is called after ngspice process has died and the ngspice reading
 * source function is finished with reading to
 * - clean up,
 * - check if all went good or fail,
 * - wait for data conversion thread,
 * - return the main program flow to the gui thread.
 */
static void ngspice_watcher_watch_ngspice (GPid pid, gint status, NgSpiceWatcherWatchNgSpiceResources *resources) {
	enum NGSPICE_WATCHER_RETURN_VALUE ret_val = ngspice_watcher_watch_ngspice_resources (pid, status, resources);

	NgspiceEmitData *emitData = g_malloc(sizeof(NgspiceEmitData));
	emitData->emit_instance = resources->emit_instance;

	switch(ret_val) {
	case NGSPICE_WATCHER_RETURN_VALUE_ABORTED:
	case NGSPICE_WATCHER_RETURN_VALUE_CANCELED:
		emitData->signal_name = g_strdup("aborted");
		*resources->aborted = TRUE;
		break;
	case NGSPICE_WATCHER_RETURN_VALUE_DONE:
		emitData->signal_name = g_strdup("done");
		break;
	}

	cancel_info_unsubscribe(resources->cancel_info);
	ngspice_watch_ngspice_resources_finalize(resources);

	/*
	 * return to main thread
	 *
	 * Don't return too early, because if you do, the ngspice
	 * object could be finalized but some resources depend on it.
	 */
	g_main_context_invoke(NULL, (GSourceFunc)g_signal_emit_by_name_main_thread, emitData);
}

/**
 * Extracts a progress number (time of transient analysis)
 * out of a string (if existing) and saves it to the thread-shared
 * progress variable.
 */
static void read_progress_ngspice(ProgressResources *progress_ngspice, gdouble progress_end, const gchar *line) {
	if (!g_regex_match_simple("Reference value.*\\r", line, 0, 0))
		return;
	gchar **splitted = g_regex_split_simple(".* (.+)\\r", line, 0, 0);
	gchar **ptr;
	for (ptr = splitted; *ptr != NULL; ptr++)
		if (**ptr != 0)
			break;
	if (*ptr != NULL) {
		gdouble progress_absolute = g_ascii_strtod(*ptr, NULL);

		g_mutex_lock(&progress_ngspice->progress_mutex);
		progress_ngspice->progress = progress_absolute / progress_end;
		if (g_str_has_suffix(line, "\r\n"))
			progress_ngspice->progress = 1;
		progress_ngspice->time = g_get_monotonic_time();
		g_mutex_unlock(&progress_ngspice->progress_mutex);
	}
	g_strfreev(splitted);
}

/**
 * Reads stderr of ngspice.
 *
 * stderr of ngspice might contain progress information.
 */
static gboolean ngspice_child_stderr_cb (GIOChannel *channel, GIOCondition condition,
                                         NgSpiceWatchSTDERRResources *resources)
{
	LogInterface log = resources->log;
	const SimSettings* const sim_settings = resources->sim_settings;
	ProgressResources *progress_ngspice = resources->progress_ngspice;
	enum ERROR_STATE *error_state = resources->error_state;
	IsNgspiceStderrDestroyed *is_ngspice_stderr_destroyed = resources->is_ngspice_stderr_destroyed;

	gchar *line = NULL;
	gsize len, terminator;
	GError *e = NULL;

	GIOStatus status = g_io_channel_read_line (channel, &line, &len, &terminator, &e);
	if (e) {
		gchar *message = g_strdup_printf("spice pipe stderr: %s - %i", e->message, e->code);
		log.log_append_error(log.log, message);
		g_free(message);
		g_clear_error (&e);
	} else if (status == G_IO_STATUS_NORMAL && len > 0) {
		log.log_append_error(log.log, line);

		if (g_str_has_suffix(line, ": No such file or directory\n"))
			*error_state = ERROR_STATE_NO_SUCH_FILE_OR_DIRECTORY;
		if (g_str_equal(line, "spice stopped due to error, no simulation run!\n"))
			*error_state = ERROR_STATE_ERROR_IN_NETLIST;

		gdouble progress_end = sim_settings_get_trans_stop(sim_settings);
		read_progress_ngspice(progress_ngspice, progress_end, line);
	} else if (status == G_IO_STATUS_EOF) {
		g_source_destroy(g_main_current_source());
		g_free(resources);

		// emit signal, that stderr reading has finished
		g_mutex_lock(&is_ngspice_stderr_destroyed->mutex);
		is_ngspice_stderr_destroyed->boolean = TRUE;
		g_cond_signal(&is_ngspice_stderr_destroyed->cond);
		g_mutex_unlock(&is_ngspice_stderr_destroyed->mutex);

		return G_SOURCE_REMOVE;
	}
	if (line)
		g_free (line);

	return G_SOURCE_CONTINUE;
}

/**
 * @resources: caller frees
 *
 * Prepares data structures to launch some threads and finally launches them.
 *
 * The launched threads are:
 * - process ngspice
 * - thread watcher
 * - thread saver
 * - thread worker
 *
 * As you should know ngspice is the program that actually simulates the simulation.
 *
 * The watcher thread handles stdout- and death-events of the ngspice process.
 * stdout data is forked to the threads "saver" and "worker".
 * As response to the death-event of ngspice, the watcher
 * - cleans the field of war,
 * - checks if all went good and creates error messages if not all went good,
 * - waits for the worker to finish work,
 * - finally returns the main program flow to the gui thread.
 *
 * The stderr-events are handled by the main (gui) thread, because it is not heavy work.
 * Furthermore additionally shared variables can be avoided.
 *
 * The saver saves the data to SSD/HDD (temporary folder). It is needed to create
 * good error messages in case of failure. Besides of that the user can analyze
 * the data with external/other programs.
 *
 * The worker parses the stream of data, interprets and converts it to structured data
 * so it can be plotted by gui.
 */
void ngspice_watcher_build_and_launch(const NgspiceWatcherBuildAndLaunchResources *resources) {
	LogInterface log = resources->log;
	const SimSettings* const sim_settings = resources->sim_settings;
	gboolean is_vanilla = resources->is_vanilla;
	GPid *child_pid = resources->child_pid;
	gboolean *aborted = resources->aborted;
	const void* emit_instance = resources->emit_instance;
	guint *num_analysis = resources->num_analysis;
	ProgressResources *progress_ngspice = resources->progress_ngspice;
	ProgressResources *progress_reader = resources->progress_reader;
	GList **analysis = resources->analysis;
	AnalysisTypeShared *current = resources->current;
	GError *e = NULL;

	char *argv[] = {NULL, "-b", resources->netlist_file, NULL};

	if (is_vanilla)
		argv[0] = SPICE_EXE;
	else
		argv[0] = NGSPICE_EXE;

	gint ngspice_stdout_fd;
	gint ngspice_stderr_fd;
	// Launch ngspice
	if (!g_spawn_async_with_pipes (NULL, // Working directory
		                              argv, NULL, G_SPAWN_DO_NOT_REAP_CHILD | G_SPAWN_SEARCH_PATH, NULL,
		                              NULL, child_pid,
		                              NULL,                // STDIN
		                              &ngspice_stdout_fd, // STDOUT
		                              &ngspice_stderr_fd,  // STDERR
		                              &e)) {

		*aborted = TRUE;
		log.log_append_error(log.log, _("Unable to execute NgSpice."));
		g_signal_emit_by_name (G_OBJECT (emit_instance), "aborted");
		g_clear_error (&e);
		return;

	}

	// synchronizes stderr listener with is_ngspice_finished listener (needed for error handling)
	IsNgspiceStderrDestroyed *is_ngspice_stderr_destroyed = g_new0(IsNgspiceStderrDestroyed, 1);
	g_mutex_init(&is_ngspice_stderr_destroyed->mutex);
	g_cond_init(&is_ngspice_stderr_destroyed->cond);
	is_ngspice_stderr_destroyed->boolean = FALSE;

	// variable needed for error handling
	enum ERROR_STATE *error_state = g_new0(enum ERROR_STATE, 1);

	GMainContext *forker_context = g_main_context_new();
	GMainLoop *forker_main_loop = g_main_loop_new(forker_context, FALSE);
	g_main_context_unref(forker_context);

	// Create pipes to fork the stdout data of ngspice
	ThreadPipe *thread_pipe_worker = thread_pipe_new(20, 2048);
	ThreadPipe *thread_pipe_saver = thread_pipe_new(20, 2048);

	/**
	 * Launch analyzer
	 */
	NgspiceAnalysisResources *ngspice_worker_resources = g_new0(NgspiceAnalysisResources, 1);
	ngspice_worker_resources->analysis = analysis;
	ngspice_worker_resources->buf = NULL;
	ngspice_worker_resources->is_vanilla = is_vanilla;
	ngspice_worker_resources->current = current;
	ngspice_worker_resources->no_of_data_rows_ac = 0;
	ngspice_worker_resources->no_of_data_rows_dc = 0;
	ngspice_worker_resources->no_of_data_rows_op = 0;
	ngspice_worker_resources->no_of_data_rows_transient = 0;
	ngspice_worker_resources->no_of_data_rows_noise = 0;
	ngspice_worker_resources->no_of_variables = 0;
	ngspice_worker_resources->num_analysis = num_analysis;
	ngspice_worker_resources->pipe = thread_pipe_worker;
	ngspice_worker_resources->progress_reader = progress_reader;
	ngspice_worker_resources->sim_settings = sim_settings;
	ngspice_worker_resources->cancel_info = resources->cancel_info;
	cancel_info_subscribe(ngspice_worker_resources->cancel_info);

	GThread *worker = g_thread_new("spice worker", (GThreadFunc)ngspice_worker, ngspice_worker_resources);

	/**
	 * Launch output saver
	 */
	NgSpiceSaverResources *ngspice_saver_resources = g_new0(NgSpiceSaverResources, 1);
	ngspice_saver_resources->path_to_file = g_strdup(resources->ngspice_result_file);
	ngspice_saver_resources->pipe = thread_pipe_saver;
	ngspice_saver_resources->cancel_info = resources->cancel_info;
	cancel_info_subscribe(ngspice_saver_resources->cancel_info);

	GThread *saver = g_thread_new("spice saver", (GThreadFunc)ngspice_saver, ngspice_saver_resources);

	/**
	 * Add an ngspice-is-finished watcher
	 */
	NgSpiceWatcherWatchNgSpiceResources *ngspice_watcher_watch_ngspice_resources = g_new0(NgSpiceWatcherWatchNgSpiceResources, 1);
	ngspice_watcher_watch_ngspice_resources->emit_instance = emit_instance;
	ngspice_watcher_watch_ngspice_resources->aborted = aborted;
	ngspice_watcher_watch_ngspice_resources->child_pid = child_pid;
	ngspice_watcher_watch_ngspice_resources->log = log;
	ngspice_watcher_watch_ngspice_resources->num_analysis = num_analysis;
	ngspice_watcher_watch_ngspice_resources->worker = worker;
	ngspice_watcher_watch_ngspice_resources->saver = saver;
	ngspice_watcher_watch_ngspice_resources->main_loop = forker_main_loop;
	ngspice_watcher_watch_ngspice_resources->ngspice_result_file = g_strdup(resources->ngspice_result_file);
	ngspice_watcher_watch_ngspice_resources->netlist_file = g_strdup(resources->netlist_file);
	ngspice_watcher_watch_ngspice_resources->error_state = error_state;
	ngspice_watcher_watch_ngspice_resources->is_ngspice_stderr_destroyed = is_ngspice_stderr_destroyed;
	ngspice_watcher_watch_ngspice_resources->cancel_info = resources->cancel_info;
	cancel_info_subscribe(ngspice_watcher_watch_ngspice_resources->cancel_info);

	GSource *child_watch_source = g_child_watch_source_new (*child_pid);
	g_source_set_priority (child_watch_source, G_PRIORITY_LOW);
	g_source_set_callback (child_watch_source, (GSourceFunc)ngspice_watcher_watch_ngspice, ngspice_watcher_watch_ngspice_resources, NULL);
	g_source_attach (child_watch_source, forker_context);
	g_source_unref (child_watch_source);

	/**
	 * Add a GIOChannel to read from process stdout
	 */
	NgSpiceWatchSTDOUTResources *ngspice_watch_stdout_resources = g_new0(NgSpiceWatchSTDOUTResources, 1);
	ngspice_watch_stdout_resources->ngspice_watch_fork_resources.thread_pipe_worker = thread_pipe_worker;
	ngspice_watch_stdout_resources->ngspice_watch_fork_resources.thread_pipe_saver = thread_pipe_saver;
	ngspice_watch_stdout_resources->cancel_info = resources->cancel_info;
	cancel_info_subscribe(ngspice_watch_stdout_resources->cancel_info);

	GIOChannel *ngspice_stdout_channel = g_io_channel_unix_new(ngspice_stdout_fd);
	g_io_channel_set_close_on_unref(ngspice_stdout_channel, TRUE);
	GSource *ngspice_stdout_source = g_io_create_watch (ngspice_stdout_channel, G_IO_IN | G_IO_PRI | G_IO_HUP | G_IO_NVAL);
	g_io_channel_unref(ngspice_stdout_channel);
	g_source_set_priority (ngspice_stdout_source, G_PRIORITY_HIGH);
	g_source_set_callback (ngspice_stdout_source, (GSourceFunc)ngspice_watcher_watch_stdout, ngspice_watch_stdout_resources, NULL);
	g_source_attach (ngspice_stdout_source, forker_context);
	g_source_unref (ngspice_stdout_source);

	/**
	 * Add a GIOChannel to read from process stderr (attach to gui thread because it prints to log).
	 * I hope that ngspice does not print too much errors so that it is a minor work
	 * that does not hold the gui back from paint and user events
	 */
	NgSpiceWatchSTDERRResources *ngspice_watch_stderr_resources = g_new0(NgSpiceWatchSTDERRResources, 1);
	ngspice_watch_stderr_resources->log = log;
	ngspice_watch_stderr_resources->sim_settings = sim_settings;
	ngspice_watch_stderr_resources->progress_ngspice = progress_ngspice;
	ngspice_watch_stderr_resources->error_state = error_state;
	ngspice_watch_stderr_resources->is_ngspice_stderr_destroyed = is_ngspice_stderr_destroyed;

	GIOChannel *ngspice_stderr_channel = g_io_channel_unix_new (ngspice_stderr_fd);
	g_io_channel_set_close_on_unref(ngspice_stderr_channel, TRUE);
	// sometimes there is no data and then the GUI will hang up if NONBLOCK not set
	g_io_channel_set_flags(ngspice_stderr_channel, g_io_channel_get_flags(ngspice_stderr_channel) | G_IO_FLAG_NONBLOCK, NULL);
	GSource *channel_stderr_watch_source = g_io_create_watch(ngspice_stderr_channel, G_IO_IN | G_IO_PRI | G_IO_HUP | G_IO_NVAL);
	g_io_channel_unref(ngspice_stderr_channel);
	g_source_set_priority (channel_stderr_watch_source, G_PRIORITY_LOW);
	g_source_set_callback (channel_stderr_watch_source, (GSourceFunc)ngspice_child_stderr_cb, ngspice_watch_stderr_resources, NULL);
	g_source_attach (channel_stderr_watch_source, NULL);
	g_source_unref (channel_stderr_watch_source);

	// Launch watcher
	g_thread_unref(g_thread_new("spice forker", (GThreadFunc)ngspice_watcher_main, forker_main_loop));
}

NgspiceWatcherBuildAndLaunchResources *ngspice_watcher_build_and_launch_resources_new(OreganoNgSpice *ngspice) {

	NgspiceWatcherBuildAndLaunchResources *resources = g_new0(NgspiceWatcherBuildAndLaunchResources, 1);

	resources->is_vanilla = ngspice->priv->is_vanilla;
	resources->aborted = &ngspice->priv->aborted;
	resources->analysis = &ngspice->priv->analysis;
	resources->child_pid = &ngspice->priv->child_pid;
	resources->current = &ngspice->priv->current;
	resources->emit_instance = ngspice;

	resources->log.log = ngspice->priv->schematic;
	resources->log.log_append = (LogFunction)schematic_log_append;
	resources->log.log_append_error = (LogFunction)schematic_log_append_error;

	resources->num_analysis = &ngspice->priv->num_analysis;
	resources->progress_ngspice = &ngspice->priv->progress_ngspice;
	resources->progress_reader = &ngspice->priv->progress_reader;
	resources->sim_settings = schematic_get_sim_settings(ngspice->priv->schematic);

	resources->netlist_file = g_strdup("/tmp/netlist.tmp");
	resources->ngspice_result_file = g_strdup("/tmp/netlist.lst");

	resources->cancel_info = ngspice->priv->cancel_info;
	cancel_info_subscribe(resources->cancel_info);

	return resources;
}

void ngspice_watcher_build_and_launch_resources_finalize(NgspiceWatcherBuildAndLaunchResources *resources) {
	cancel_info_unsubscribe(resources->cancel_info);
	g_free(resources->netlist_file);
	g_free(resources->ngspice_result_file);
	g_free(resources);
}
