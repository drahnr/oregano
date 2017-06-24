/*
 * test_thread_pipe.c
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

#ifndef TEST_THREAD_PIPE_H_
#define TEST_THREAD_PIPE_H_

#include <string.h>
#include "../src/tools/thread-pipe.h"


typedef struct _GTestAddDataFuncParameters GTestAddDataFuncParameters;
static gchar *get_testpath_g_test_add_data_func_parameters(GTestAddDataFuncParameters *parameters);
static gconstpointer get_test_data_g_test_add_data_func_parameters(GTestAddDataFuncParameters *parameters);
static GTestDataFunc get_test_func_g_test_add_data_func_parameters(GTestAddDataFuncParameters *parameters);
static GTestAddDataFuncParameters *get_next_g_test_add_data_func_parameters(GTestAddDataFuncParameters *parameters);

static GTestAddDataFuncParameters *test_thread_pipe_buffered_create_test_data();

/**
 * Automatically creates some test cases.
 */
void add_funcs_test_thread_pipe_buffered() {
	GTestAddDataFuncParameters *parameters = test_thread_pipe_buffered_create_test_data();
	while (parameters) {
		gchar *testpath = get_testpath_g_test_add_data_func_parameters(parameters);
		gconstpointer test_data = get_test_data_g_test_add_data_func_parameters(parameters);
		GTestDataFunc test_func = get_test_func_g_test_add_data_func_parameters(parameters);
		GTestAddDataFuncParameters *next = get_next_g_test_add_data_func_parameters(parameters);

		g_test_add_data_func(testpath, test_data, test_func);
		GTestAddDataFuncParameters *ptr = parameters;
		parameters = next;
		g_free(ptr);
	}
}

struct _GTestAddDataFuncParameters {
	gchar *testpath;
	gconstpointer test_data;
	GTestDataFunc test_func;
	GTestAddDataFuncParameters *next;
};

/**
 * getters for GTestAddDataFuncParameters
 */
static gchar *get_testpath_g_test_add_data_func_parameters(GTestAddDataFuncParameters *parameters) { return parameters->testpath; }
static gconstpointer get_test_data_g_test_add_data_func_parameters(GTestAddDataFuncParameters *parameters) { return parameters->test_data; }
static GTestDataFunc get_test_func_g_test_add_data_func_parameters(GTestAddDataFuncParameters *parameters) { return parameters->test_func; }
static GTestAddDataFuncParameters *get_next_g_test_add_data_func_parameters(GTestAddDataFuncParameters *parameters) { return parameters->next; }


typedef struct _TestThreadPipeBufferedTestData TestThreadPipeBufferedTestData;
typedef struct _TestThreadPipeBufferedThreadData TestThreadPipeBufferedThreadData;
typedef struct _TestThreadPipeBufferedData TestThreadPipeBufferedData;

static gchar *test_thread_pipe_buffered_get_testpath(guint *parameter_config, gchar ***parameter_names);
static TestThreadPipeBufferedTestData *test_thread_pipe_buffered_test_data_new(guint *parameter_config);
static void test_thread_pipe_buffered_test_func(TestThreadPipeBufferedTestData *test_data);
static gboolean test_thread_pipe_buffered_variate_parameters(guint *parameter_config, gchar ***parameter_names);
static guint test_thread_pipe_buffered_ptr_array_length(gpointer *array);

static gpointer test_thread_pipe_buffered_writer(TestThreadPipeBufferedThreadData *pipe_data);
static gpointer test_thread_pipe_buffered_reader(TestThreadPipeBufferedThreadData *pipe_data);
static TestThreadPipeBufferedData *test_thread_pipe_buffered_data_new_varargs(const gchar *str, ...);
static TestThreadPipeBufferedData *test_thread_pipe_buffered_data_new(gchar **str);
static void test_thread_pipe_buffered_data_assert_equal(TestThreadPipeBufferedData *data1, TestThreadPipeBufferedData* data2);

struct _TestThreadPipeBufferedThreadData {
	ThreadPipe *pipe;
	gulong sleep;
	TestThreadPipeBufferedData *data;
	ThreadPipe *(*pop)(ThreadPipe *pipe, gpointer *data_out, gsize *size);
};

struct _TestThreadPipeBufferedTestData {
	TestThreadPipeBufferedThreadData write_data;
	TestThreadPipeBufferedThreadData read_data;
	TestThreadPipeBufferedData *expected;
};

struct _TestThreadPipeBufferedData {
	gpointer data;
	gsize size;
	TestThreadPipeBufferedData *next;
};

/**
 * Creates generic test data out by varying parameters.
 */
static GTestAddDataFuncParameters *test_thread_pipe_buffered_create_test_data() {
	//0 terminated
	gchar ***parameter_list = g_new0(gchar **, 6);
	parameter_list[0] = (gchar *[]){"read_write", "write_read", NULL};
	parameter_list[1] = (gchar *[]){"nowhere", "middle", "end", NULL};
	parameter_list[2] = (gchar *[]){"first", "not_first", NULL};
	parameter_list[3] = (gchar *[]){"last", "not_last", NULL};
	parameter_list[4] = (gchar *[]){"pop", "pop_line", NULL};

	//increase function needs one field more, that's why size+1
	guint *parameters = g_new0(guint, test_thread_pipe_buffered_ptr_array_length((gpointer *)parameter_list) + 1);
	parameters[1] = test_thread_pipe_buffered_ptr_array_length((gpointer *)parameter_list[1]);
	parameters[2] = test_thread_pipe_buffered_ptr_array_length((gpointer *)parameter_list[2]);
	parameters[3] = test_thread_pipe_buffered_ptr_array_length((gpointer *)parameter_list[3]);
	GTestAddDataFuncParameters *ret_val = NULL;
	GTestAddDataFuncParameters **walker = &ret_val;

	do {
		*walker = g_new0(GTestAddDataFuncParameters, 1);
		(*walker)->testpath = test_thread_pipe_buffered_get_testpath(parameters, parameter_list);
		(*walker)->test_data = test_thread_pipe_buffered_test_data_new(parameters);
		(*walker)->test_func = (GTestDataFunc)test_thread_pipe_buffered_test_func;
		walker = &(*walker)->next;
	} while (test_thread_pipe_buffered_variate_parameters(parameters, parameter_list));

	return ret_val;
}

/**
 * Creates unique test path out of a given parameter specification.
 */
static gchar *test_thread_pipe_buffered_get_testpath(guint *parameter_config, gchar ***parameter_names) {
	GString *ret_val = g_string_new("/tools/thread_pipe_buffered/test");

	for (int i = 0; parameter_names[i] != NULL; i++) {
		if (parameter_names[i][parameter_config[i]] != NULL) {
			g_string_append_printf(ret_val, "_%s", parameter_names[i][parameter_config[i]]);
		}
	}
	gchar *ret_val_str = ret_val->str;
	g_string_free(ret_val, FALSE);
	return ret_val_str;
}

/**
 * Creates generic test data out of a given parameter specification.
 */
static TestThreadPipeBufferedTestData *test_thread_pipe_buffered_test_data_new(guint *parameter_config) {
	TestThreadPipeBufferedTestData *tpipe = g_new0(TestThreadPipeBufferedTestData, 1);
	ThreadPipe *pipe = thread_pipe_new(0, 0);
	tpipe->write_data.pipe = pipe;
	tpipe->read_data.pipe = pipe;

	switch (parameter_config[0]) {
	case 0:
		tpipe->write_data.sleep = 10000;
		break;
	case 1:
		tpipe->read_data.sleep = 10000;
		break;
	}

	switch (parameter_config[4]) {
	case 0:
		tpipe->read_data.pop = thread_pipe_pop;
		tpipe->write_data.data = test_thread_pipe_buffered_data_new_varargs(
				"asdf",
				"jklö",
				"qwer",
				"uiop",
				"yxcv",
				"m,.-",
				NULL);
		tpipe->expected = tpipe->write_data.data;
		break;
	case 1:
		tpipe->read_data.pop = (ThreadPipe *(*)(ThreadPipe *pipe, gpointer *data_out, gsize *size))thread_pipe_pop_line;

		GString *string = g_string_new("asdf");
		GString *string_all = g_string_new(string->str);

		if (parameter_config[1] == 1) {
			g_string_append(string, "\nasdf");
			g_string_append(string_all, "\nasdf");
		}
		else if (parameter_config[1] == 2) {
			g_string_append(string, "\n");
			g_string_append(string_all, "\n");
		}

		GList *list_write = NULL;
		list_write = g_list_append(list_write, string->str);
		g_string_free(string, FALSE);

		if (parameter_config[2] == 1) {
			list_write = g_list_prepend(list_write, g_strdup("asdf"));
			g_string_prepend(string_all, "asdf");
		}

		if (parameter_config[3] == 1) {
			list_write = g_list_append(list_write, g_strdup("asdf"));
			g_string_append(string_all, "asdf");
		}

		guint length = g_list_length(list_write);

		gchar **str_array = g_new0(gchar *, length + 1);
		guint i = 0;
		for (GList *l = list_write; l != NULL; l = l->next, i++)
			str_array[i] = l->data;

		tpipe->write_data.data = test_thread_pipe_buffered_data_new(str_array);

		gchar **splitted = g_regex_split_simple("\\n", string_all->str, 0, 0);
		gchar **walker;
		for (walker = splitted; *(walker + 1) != NULL; walker++)
			*walker = g_strdup_printf("%s\n", *walker);
		if (**walker == 0)
			*walker = NULL;

		tpipe->expected = test_thread_pipe_buffered_data_new(splitted);

		break;
	}

	return tpipe;
}

/**
 * Executes the test with a specified test input data.
 */
static void test_thread_pipe_buffered_test_func(TestThreadPipeBufferedTestData *test_data) {
	GThread *writer = g_thread_new(
			"test_thread_pipe_buffered_writer",
			(GThreadFunc)test_thread_pipe_buffered_writer,
			(gpointer)(&test_data->write_data));
	GThread *reader = g_thread_new(
			"test_thread_pipe_buffered_reader",
			(GThreadFunc)test_thread_pipe_buffered_reader,
			(gpointer)(&test_data->read_data));

	g_thread_join(writer);
	g_thread_join(reader);

	test_thread_pipe_buffered_data_assert_equal(test_data->expected, test_data->read_data.data);
}

/**
 * varies the input parameter configuration differentially
 */
static gboolean test_thread_pipe_buffered_variate_parameters(guint *parameter_config, gchar ***parameter_names) {
	guint parameter_names_length = test_thread_pipe_buffered_ptr_array_length((gpointer *)parameter_names);
	parameter_config[0]++;
	for (int i = 0; i < parameter_names_length; i++) {
		if (parameter_config[i] >= test_thread_pipe_buffered_ptr_array_length((gpointer *)parameter_names[i])) {
			parameter_config[i] = 0;
			parameter_config[i + 1]++;
		} else
			break;
	}
//	if (parameter_config[1] == 1 && parameter_config[4] == 0) {
//		parameter_config[1] = 0;
//		parameter_config[4]++;
//	}

	return !parameter_config[parameter_names_length];
}

/**
 * get the length of a 0 terminated pointer array
 */
static guint test_thread_pipe_buffered_ptr_array_length(gpointer *array) {
	guint ret_val = 0;

	while (array[ret_val])
		ret_val++;

	return ret_val;
}

/**
 * writes to the pushing end of a thread pipe
 */
static gpointer test_thread_pipe_buffered_writer(TestThreadPipeBufferedThreadData *pipe_data) {
	TestThreadPipeBufferedData *walker = pipe_data->data;
	while (walker) {
		g_usleep(pipe_data->sleep);
		thread_pipe_push(pipe_data->pipe, walker->data, walker->size);
		walker = walker->next;
	}
	thread_pipe_set_write_eof(pipe_data->pipe);

	return NULL;
}

/**
 * reads the popping end of a thread pipe
 */
static gpointer test_thread_pipe_buffered_reader(TestThreadPipeBufferedThreadData *pipe_data) {
	gpointer data;
	gsize size;
	TestThreadPipeBufferedData **walker = &pipe_data->data;
	while (pipe_data->pop(pipe_data->pipe, &data, &size)) {
		*walker = g_new0(TestThreadPipeBufferedData, 1);
		(*walker)->data = g_memdup(data, size);
		(*walker)->size = size;
		walker = &(*walker)->next;
		g_usleep(pipe_data->sleep);
//		puts(data);
	}

	return NULL;
}

/**
 * Creates a new TestThreadPipeBufferedData struct out of an array string list.
 *
 * The list has to be 0 terminated.
 */
static TestThreadPipeBufferedData *test_thread_pipe_buffered_data_new(gchar **str) {
	TestThreadPipeBufferedData *ret_val = NULL;
	TestThreadPipeBufferedData **walker = &ret_val;
	for (int i = 0; str[i] != NULL; i++) {
		*walker = g_new0(TestThreadPipeBufferedData, 1);
		(*walker)->data = g_strdup(str[i]);
		(*walker)->size = strlen(str[i]) + 1;
		walker = &(*walker)->next;
	}

	return ret_val;
}

/**
 * Creates a new TestThreadPipeBufferedData struct out of a vararg string list.
 *
 * The list has to be 0 terminated.
 */
static TestThreadPipeBufferedData *test_thread_pipe_buffered_data_new_varargs(const gchar *str, ...) {

	va_list ptr;
	va_start(ptr, str);

	gchar **str_array;
	size_t size;
	FILE *file = open_memstream((char **)&str_array, &size);

	const gchar *asdf = str;
	while (asdf) {
		fwrite(&asdf, sizeof(const gchar *), 1, file);
		asdf = va_arg(ptr, const gchar *);
	}
	fwrite(&asdf, sizeof(const gchar *), 1, file);

	fclose(file);

	TestThreadPipeBufferedData *ret_val = test_thread_pipe_buffered_data_new(str_array);
	g_free(str_array);

	return ret_val;
}

/**
 * Looks for equality of two TestThreadPipeBufferedData structs.
 *
 * Two structs are equal if
 * - size of each block is equal and
 * - data of each block is equal.
 */
static void test_thread_pipe_buffered_data_assert_equal(TestThreadPipeBufferedData *data1, TestThreadPipeBufferedData* data2) {
	TestThreadPipeBufferedData *walker1 = data1;
	TestThreadPipeBufferedData *walker2 = data2;

	while (walker1 && walker2) {
		g_assert_cmpuint(walker1->size, ==, walker2->size);
		g_assert_cmpmem(walker1->data, walker1->size, walker2->data, walker2->size);
		walker1 = walker1->next;
		walker2 = walker2->next;
	}

	g_assert_true(walker1 == NULL);
	g_assert_true(walker2 == NULL);

}

#endif /* TEST_THREAD_PIPE_H_ */
