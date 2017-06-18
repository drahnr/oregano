/*
 * test_engine_ngspice.c
 *
 *  Created on: Jun 8, 2017
 *      Author: michi
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

	g_autofree gchar *actual_content = NULL;
	gsize actual_size;
	if (test_resources->ngspice->priv->saver != NULL) {
		g_thread_join(test_resources->ngspice->priv->saver);
		test_resources->ngspice->priv->saver = NULL;
	}
	test_engine_ngspice_resources_finalize(test_resources);
	g_assert_true(g_file_get_contents(actual_file, &actual_content, &actual_size, NULL));

	g_autofree gchar *expected_content = NULL;
	gsize expected_size;
	g_assert_true(g_file_get_contents(expected_file, &expected_content, &expected_size, NULL));

	g_assert_true(expected_size > 350);
	g_assert_true(actual_size > expected_size - 350);
	double distance = 0;
	for (gsize i = 0; i < expected_size - 350; i++)
		distance += ABS(actual_content[i] - expected_content[i]);

	g_assert_true(distance < 3*16*20);
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
	test_resources->resources->netlist_file = g_strdup_printf("%s/test-files/test_engine_ngspice_watcher/error/step_zero/input.netlist", test_dir);

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
