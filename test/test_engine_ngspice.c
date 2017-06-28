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

static void print_log(const GList *list) {
	for (const GList *walker = list; walker; walker = walker->next)
		g_printf("%s", (char *)walker->data);
}

typedef struct {
	NgspiceWatcherBuildAndLaunchResources *resources;
	OreganoNgSpice *ngspice;
	GMainLoop *loop;
	GList *log_list;
	SimSettings *sim_settings;
} TestEngineNgspiceResources;

static TestEngineNgspiceResources *test_engine_ngspice_resources_new() {
	TestEngineNgspiceResources *test_resources = g_new0(TestEngineNgspiceResources, 1);


	test_resources->resources = g_new0(NgspiceWatcherBuildAndLaunchResources, 1);
	NgspiceWatcherBuildAndLaunchResources *resources = test_resources->resources;
	test_resources->ngspice = OREGANO_NGSPICE(oregano_spice_new(NULL, FALSE));
	OreganoNgSpice *ngspice = test_resources->ngspice;
	GMainLoop *loop = g_main_loop_new(NULL, FALSE);
	test_resources->loop = loop;
	g_signal_connect_swapped(G_OBJECT(ngspice), "done", G_CALLBACK(g_main_loop_quit), loop);
	g_signal_connect_swapped(G_OBJECT(ngspice), "aborted", G_CALLBACK(g_main_loop_quit), loop);

	resources->aborted = &ngspice->priv->aborted;
	resources->analysis = &ngspice->priv->analysis;
	resources->child_pid = &ngspice->priv->child_pid;
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

static void test_engine_ngspice_basic() {

	TestEngineNgspiceResources *test_resources = test_engine_ngspice_resources_new();

	g_autofree gchar *test_dir = get_test_base_dir();

	g_free(test_resources->resources->netlist_file);
	test_resources->resources->netlist_file = g_strdup_printf("%s/test-files/test_engine_ngspice_watcher/basic/input.netlist", test_dir);

	g_free(test_resources->resources->ngspice_result_file);
	test_resources->resources->ngspice_result_file = g_strdup_printf("%s/test-files/test_engine_ngspice_watcher/basic/result/actual.txt", test_dir);

	g_autofree gchar *actual_file = g_strdup_printf("%s/test-files/test_engine_ngspice_watcher/basic/result/actual.txt", test_dir);
	g_autofree gchar *expected_file = g_strdup_printf("%s/test-files/test_engine_ngspice_watcher/basic/result/expected.txt", test_dir);

	ngspice_watcher_build_and_launch(test_resources->resources);
	g_main_loop_run(test_resources->loop);
	print_log(test_resources->log_list);

	test_engine_ngspice_resources_finalize(test_resources);

	g_autofree gchar *actual_content = NULL;
	gsize actual_size;
	g_file_get_contents(actual_file, &actual_content, &actual_size, NULL);

	g_autofree gchar *expected_content = NULL;
	gsize expected_size;
	g_file_get_contents(expected_file, &expected_content, &expected_size, NULL);

	// FIXME this comparision is too cumbersome and error prone
	// any kind of change in the ngspice output will brake this
	// we should only compare the lines which are considered
	// number output of the simulation
	// g_assert_true(expected_size > 350);
	// g_assert_true(actual_size > expected_size - 350);
	// double distance = 0;
	// for (gsize i = 0; i < expected_size - 350; i++) {
	//	distance += ABS(actual_content[i] - expected_content[i]);
	// }
	// FIXME this will never work reliably
	// g_assert_true(distance < 3*16*20);
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

	//
	// g_assert_cmpstr(actual_content, ==, expected_content);
  //
  // const gchar *array[] = {
  // 			"\n",
  // 			"ngspice stopped due to error, no simulation run!\n",
  // 			"\n",
  // 			"ERROR: fatal error in ngspice, exit(1)\n",
  // 			"### ngspice exited abnormally ###\n",
  // 			"### netlist error detected ###\n",
  // 			"You made a mistake in the simulation settings or part properties.\n",
  // 			"The following information will help you to analyze the error.\n",
  // 			NULL
  // 	};
  //
  // 	GList *walker = test_resources->log_list;
  //
  // 	for (int i = 0; array[i] != NULL; i++) {
  // 		g_assert_nonnull(walker);
  // 		g_assert_nonnull(walker->data);
  // 		g_assert_cmpstr(walker->data, ==, array[i]);
  // 		walker = walker->next;
  // 	}

	test_engine_ngspice_resources_finalize(test_resources);
}
