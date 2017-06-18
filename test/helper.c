#include <glib.h>

static gchar* get_test_base_dir() {
	g_autofree gchar *cwd = g_get_current_dir();

	g_autofree gchar *test_file = g_strdup_printf("%s/test/test.c", cwd);
	while (!g_file_test(test_file, G_FILE_TEST_EXISTS)) {
		gchar **split = g_regex_split_simple("\\/*(?:.(?!\\/))+$", cwd, 0, 0);
		cwd = g_strdup(*split);
		g_strfreev(split);
		test_file = g_strdup_printf("%s/test/test.c", cwd);
	}
	return g_strdup_printf("%s/test", cwd);
}
