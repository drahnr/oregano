/*
 * test_engine_ngspice.c
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

#include "../src/engines/ngspice-watcher.h"
#include <glib.h>
#include <glib/gstdio.h>
#include <glib/gprintf.h>

gchar* get_test_base_dir();

static void test_engine_ngspice_basic();
static void test_engine_ngspice_error_no_such_file_or_directory();
static void test_engine_ngspice_error_step_zero();

void
add_funcs_test_engine_ngspice() {
	g_test_add_func ("/core/engine/ngspice/watcher/basic", test_engine_ngspice_basic);
	g_test_add_func ("/core/engine/ngspice/watcher/error/no_such_file_or_directory", test_engine_ngspice_error_no_such_file_or_directory);
	g_test_add_func ("/core/engine/ngspice/watcher/error/step_zero", test_engine_ngspice_error_step_zero);
}

static void test_engine_ngspice_log_append_error(GList **list, const gchar *string) {
	*list = g_list_append(*list, g_strdup(string));
}

static gboolean test_engine_ngspice_timeout(GMainLoop *loop) {
	g_main_loop_quit(loop);
	return FALSE;
}

static void print_log(const GList *list) {
	for (const GList *walker = list; walker; walker = walker->next)
		g_printf("%s", (char *)walker->data);
}

typedef struct {
	NgspiceWatcherBuildAndLaunchResources *resources;
	OreganoNgSpice *ngspice;
	GMainLoop *loop;
	guint handler_id_timeout;
	GList *log_list;
	SimSettings *sim_settings;
} TestEngineNgspiceResources;

static TestEngineNgspiceResources *test_engine_ngspice_resources_new() {
	TestEngineNgspiceResources *test_resources = g_new0(TestEngineNgspiceResources, 1);


	test_resources->resources = g_new0(NgspiceWatcherBuildAndLaunchResources, 1);
	NgspiceWatcherBuildAndLaunchResources *resources = test_resources->resources;
	test_resources->ngspice = OREGANO_NGSPICE(oregano_ngspice_new(NULL));
	OreganoNgSpice *ngspice = test_resources->ngspice;
	GMainLoop *loop = g_main_loop_new(NULL, FALSE);
	test_resources->loop = loop;
	g_signal_connect_swapped(G_OBJECT(ngspice), "done", G_CALLBACK(g_main_loop_quit), loop);
	g_signal_connect_swapped(G_OBJECT(ngspice), "aborted", G_CALLBACK(g_main_loop_quit), loop);
	test_resources->handler_id_timeout = g_timeout_add(10000, (GSourceFunc)test_engine_ngspice_timeout, loop);

	resources->aborted = &ngspice->priv->aborted;
	resources->analysis = &ngspice->priv->analysis;
	resources->child_pid = &ngspice->priv->child_pid;
	resources->saver = &ngspice->priv->saver;
	resources->current = &ngspice->priv->current;
	resources->emit_instance = ngspice;

	resources->log.log = (gpointer)&test_resources->log_list;
	resources->log.log_append = NULL;
	resources->log.log_append_error = (LogFunction)test_engine_ngspice_log_append_error;

	resources->num_analysis = &ngspice->priv->num_analysis;
	resources->progress_ngspice = &ngspice->priv->progress_ngspice;
	resources->progress_reader = &ngspice->priv->progress_reader;
	test_resources->sim_settings = sim_settings_new(NULL);
	resources->sim_settings = test_resources->sim_settings;
	resources->netlist_file = g_strdup("/tmp/netlist.tmp");
	resources->ngspice_result_file = g_strdup("/tmp/netlist.lst");

	resources->cancel_info = ngspice->priv->cancel_info;
	cancel_info_subscribe(resources->cancel_info);

	return test_resources;
}

static void test_engine_ngspice_resources_finalize(TestEngineNgspiceResources *test_resources) {
	NgspiceWatcherBuildAndLaunchResources *resources = test_resources->resources;

	g_main_loop_unref(test_resources->loop);

	sim_settings_finalize(test_resources->sim_settings);
	g_object_unref(test_resources->ngspice);

	g_list_free_full(test_resources->log_list, g_free);

	ngspice_watcher_build_and_launch_resources_finalize(resources);
}

/**
 * Prepares the given netlist file to be executable on any test system.
 */
static gchar *test_engine_ngspice_get_netlist_file(const gchar *relative_path) {
	g_autofree gchar *test_dir = get_test_base_dir();
	g_autofree gchar *file_raw = g_strdup_printf("%s/%s/input-raw.netlist", test_dir, relative_path);
	gchar *file = g_strdup_printf("%s/%s/input.netlist", test_dir, relative_path);

	g_autofree gchar *content_raw = NULL;
	gsize length_raw = 0;
	gboolean success_raw = g_file_get_contents(file_raw, &content_raw, &length_raw, NULL);
	g_assert_true(success_raw);

	GRegex *regex = g_regex_new("\\<VARIABLE\\>OREGANO\\_MODELDIR\\<\\/VARIABLE\\>", 0, 0, NULL);
	g_assert_nonnull(regex);
	g_autofree gchar *content = g_regex_replace(regex, content_raw, -1, 0, OREGANO_MODELDIR, 0, NULL);
	g_regex_unref(regex);

	g_file_set_contents(file, content, -1, NULL);

	return file;
}

static gpointer ngspice_worker (NgspiceAnalysisResources *resources) {

	ngspice_analysis(resources);

	return NULL;
}

static GList *test_engine_ngspice_parse_file(const gchar *path) {
	NgspiceAnalysisResources resources;
	GList *analysis = NULL;
	AnalysisTypeShared current;
	current.type = ANALYSIS_TYPE_NONE;
	g_mutex_init(&current.mutex);
	guint num_analysis = 0;
	ProgressResources progress_reader;
	progress_reader.progress = 0.0;
	progress_reader.time = g_get_monotonic_time();
	g_mutex_init(&progress_reader.progress_mutex);
	SimSettings *sim_settings = sim_settings_new(NULL);
	sim_settings->trans_enable = TRUE;
	sim_settings->ac_enable = FALSE;
	sim_settings->dc_enable = FALSE;
	sim_settings->fourier_enable = FALSE;

	resources.analysis = &analysis;
	resources.buf = NULL;
	resources.cancel_info = cancel_info_new();
	resources.current = &current;
	resources.no_of_data_rows = 0;
	resources.no_of_variables = 0;
	resources.num_analysis = &num_analysis;
	resources.pipe = thread_pipe_new(0, 0);
	resources.progress_reader = &progress_reader;
	resources.sim_settings = sim_settings;

	GThread *thread = g_thread_new("test_engine_ngspice_parse_file", (GThreadFunc)ngspice_worker, &resources);

	FILE *file = fopen(path, "r");
	gchar buf[256];
	while (!feof(file)) {
		fgets(buf, 255, file);
		thread_pipe_push(resources.pipe, buf, strlen(buf) + 1);
	}
	thread_pipe_set_write_eof(resources.pipe);

	g_thread_join(thread);

	return analysis;
}

static void assert_equal_analysis(const GList *expected, const GList *actual) {
	const SimulationData *expected_sdat = SIM_DATA (expected->data);
	const SimulationData *actual_sdat = SIM_DATA (actual->data);

	g_assert_cmpint(expected_sdat->n_variables, ==, actual_sdat->n_variables);
	gint n_variables = expected_sdat->n_variables;
	for (int i = 1; i < n_variables; i++) {
		XYData actual_xy_data;
		actual_xy_data.x = actual_sdat->data[0];
		actual_xy_data.y = actual_sdat->data[i];
		actual_xy_data.count_stuetzstellen = actual_sdat->data[i]->len;

		XYData expected_xy_data;
		expected_xy_data.x = expected_sdat->data[0];
		expected_xy_data.y = expected_sdat->data[i];
		expected_xy_data.count_stuetzstellen = expected_sdat->data[i]->len;

		/**
		 * assert interval length equal
		 * tolerance: 1 percent of expected interval length
		 */
		double x_min_expected = g_array_index(expected_xy_data.x, double, 0);
		double x_min_actual = g_array_index(actual_xy_data.x, double, 0);
		double x_max_expected = g_array_index(expected_xy_data.x, double, expected_xy_data.count_stuetzstellen - 1);
		double x_max_actual = g_array_index(actual_xy_data.x, double, actual_xy_data.count_stuetzstellen - 1);
		double interval_length_expected = x_max_expected - x_min_expected;
		double interval_length_actual = x_max_actual - x_min_actual;
		g_assert_cmpfloat(ABS(interval_length_actual - interval_length_expected), <, 0.01 * interval_length_expected);

		/**
		 * assert x_min equal
		 * tolerance: 1 percent of expected interval length
		 */
		g_assert_cmpfloat(ABS(x_min_actual - x_min_expected), <, 0.01 * interval_length_expected);

		/**
		 * assert equal count of stuetzstellen
		 * absolute tolerance: sqrt(expected count of stuetzstellen)
		 */
		guint expected_count_stuetzstellen = expected_xy_data.count_stuetzstellen;
		guint actual_count_stuetzstellen = actual_xy_data.count_stuetzstellen;
		g_assert_cmpfloat(ABS(expected_count_stuetzstellen - actual_count_stuetzstellen), <=, sqrt(expected_count_stuetzstellen));

		/**
		 * assert function (y-values) equal
		 * ||f - g|| / ( ||f|| + ||g|| ) < 0.01
		 */
		XYData *actual_xy_resampled = xy_data_resample(&actual_xy_data, 101);
		XYData *expected_xy_resampled = xy_data_resample(&expected_xy_data, 101);

		double actual_norm = xy_data_norm(actual_xy_resampled);
		double expected_norm = xy_data_norm(expected_xy_resampled);

		if (actual_norm == 0) {
			g_assert_cmpfloat(actual_norm, ==, expected_norm);
		} else {
			double distance = xy_data_metric(actual_xy_resampled, expected_xy_resampled);
			double normed_distance = distance / (actual_norm + expected_norm);
			g_assert_cmpfloat(normed_distance, <, 0.01);
		}

		xy_data_finalize(actual_xy_resampled);
		xy_data_finalize(expected_xy_resampled);
	}
}

static void test_engine_ngspice_basic() {

	TestEngineNgspiceResources *test_resources = test_engine_ngspice_resources_new();
	
	g_autofree gchar *test_dir = get_test_base_dir();

	g_free(test_resources->resources->netlist_file);
	test_resources->resources->netlist_file = test_engine_ngspice_get_netlist_file("test-files/test_engine_ngspice_watcher/basic");

	g_free(test_resources->resources->ngspice_result_file);
	test_resources->resources->ngspice_result_file = g_strdup_printf("%s/test-files/test_engine_ngspice_watcher/basic/result/actual.txt", test_dir);

	g_autofree gchar *actual_file = g_strdup_printf("%s/test-files/test_engine_ngspice_watcher/basic/result/actual.txt", test_dir);
	g_remove(actual_file);
	g_autofree gchar *expected_file = g_strdup_printf("%s/test-files/test_engine_ngspice_watcher/basic/result/expected.txt", test_dir);

	ngspice_watcher_build_and_launch(test_resources->resources);
	print_log(test_resources->log_list);
	g_assert_null(test_resources->log_list);
	g_main_loop_run(test_resources->loop);

	GList *actual_analysis = test_resources->ngspice->priv->analysis;
	/**
	 * ## Test Parser Thread
	 * Compare expected ngspice output with
	 * parsed output (parsed directly from RAM).
	 */
	GList *expected_analysis = test_engine_ngspice_parse_file(expected_file);
	assert_equal_analysis(expected_analysis, actual_analysis);
	ngspice_analysis_finalize(expected_analysis);

	/**
	 * Wait for saver thread to finish saving.
	 */
	if (test_resources->ngspice->priv->saver != NULL) {
		g_thread_join(test_resources->ngspice->priv->saver);
		test_resources->ngspice->priv->saver = NULL;
	}
	/**
	 * ## Test Saver Thread
	 * Compare ngspice output (data on HDD/SSD) with
	 * parsed output (data on RAM).
	 */
	expected_analysis = test_engine_ngspice_parse_file(actual_file);
	assert_equal_analysis(expected_analysis, actual_analysis);
	ngspice_analysis_finalize(expected_analysis);

	test_engine_ngspice_resources_finalize(test_resources);
}

static void test_engine_ngspice_error_no_such_file_or_directory() {
	TestEngineNgspiceResources *test_resources = test_engine_ngspice_resources_new();

	// make sure that the given file does not exist
	g_free(test_resources->resources->netlist_file);
	gint fd = g_file_open_tmp(NULL, &test_resources->resources->netlist_file, NULL);
	g_close(fd, NULL);
	g_remove(test_resources->resources->netlist_file);

	ngspice_watcher_build_and_launch(test_resources->resources);
	g_main_loop_run(test_resources->loop);

	g_assert_nonnull(test_resources->log_list);
	g_assert_true(g_str_has_suffix(test_resources->log_list->data, " No such file or directory\n"));

	test_engine_ngspice_resources_finalize(test_resources);
}

static void test_engine_ngspice_error_step_zero() {
	TestEngineNgspiceResources *test_resources = test_engine_ngspice_resources_new();


	g_autofree gchar *test_dir = get_test_base_dir();

	g_free(test_resources->resources->netlist_file);
	test_resources->resources->netlist_file = test_engine_ngspice_get_netlist_file("test-files/test_engine_ngspice_watcher/error/step_zero");

	g_free(test_resources->resources->ngspice_result_file);
	test_resources->resources->ngspice_result_file = g_strdup_printf("%s/test-files/test_engine_ngspice_watcher/error/step_zero/result/actual.txt", test_dir);

	g_autofree gchar *actual_file = g_strdup_printf("%s/test-files/test_engine_ngspice_watcher/error/step_zero/result/actual.txt", test_dir);
	g_autofree gchar *expected_file = g_strdup_printf("%s/test-files/test_engine_ngspice_watcher/error/step_zero/result/expected.txt", test_dir);


	ngspice_watcher_build_and_launch(test_resources->resources);
	g_main_loop_run(test_resources->loop);

	g_autofree gchar *actual_content = NULL;
	gsize actual_size;
	g_file_get_contents(actual_file, &actual_content, &actual_size, NULL);

	g_autofree gchar *expected_content = NULL;
	gsize expected_size;
	g_file_get_contents(expected_file, &expected_content, &expected_size, NULL);

	g_assert_cmpstr(actual_content, ==, expected_content);

	const gchar *array[] = {
			"\n",
			"ngspice stopped due to error, no simulation run!\n",
			"\n",
			"ERROR: fatal error in ngspice, exit(1)\n",
			"### ngspice exited abnormally ###\n",
			"### netlist error detected ###\n",
			"You made a mistake in the simulation settings or part properties.\n",
			"The following information will help you to analyze the error.\n",
			NULL
	};

	GList *walker = test_resources->log_list;

	for (int i = 0; array[i] != NULL; i++) {
		g_assert_nonnull(walker);
		g_assert_nonnull(walker->data);
		g_assert_cmpstr(walker->data, ==, array[i]);
		walker = walker->next;
	}

	test_engine_ngspice_resources_finalize(test_resources);
}
