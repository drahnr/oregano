#include <glib.h>

gchar* get_test_base_dir() {
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

/**
 * Let i be an integer greater or equal 0.
 * 1. The x value at index i should be mapped to the y value at index i.
 *
 * 2. The (x, y) pairs should be sorted ascending.
 *
 * Let x_min be the value of x at index 0.
 * Let x_max be the value of x at the last index.
 * 3. 1 and 2 together imply that the function is only defined in the
 * interval [x_min, x_max].
 *
 * 4. Most functions that do some operations on functions assert linear
 * functions between two neighbored x values.
 */
typedef struct {
	GArray *x;
	GArray *y;

	/**
	 * The length of both arrays should be greater than this variable.
	 */
	guint count_stuetzstellen;
} XYData;

XYData *xy_data_new(guint count_stuetzstellen_expected) {
	XYData *data = g_new0(XYData, 1);

	data->x = g_array_sized_new(TRUE, TRUE, sizeof(double), count_stuetzstellen_expected + 1);
	data->y = g_array_sized_new(TRUE, TRUE, sizeof(double), count_stuetzstellen_expected + 1);

	return data;
}

/**
 * free memory
 */
void xy_data_finalize(XYData *data) {
	g_array_free(data->x, TRUE);
	g_array_free(data->y, TRUE);
	g_free(data);
}

/**
 * linear interpolation
 */
static double xy_data_get_y_new(const XYData *data, double x_new) {
	guint i;
	for (i = 0; i < data->x->len; i++) {
		if (g_array_index(data->x, double, i) >= x_new)
			break;
	}
	g_return_val_if_fail(i != data->x->len, 0);

	double x_0 = g_array_index(data->x, double, i-1);
	double y_0 = g_array_index(data->y, double, i-1);
	double x_1 = g_array_index(data->x, double, i);
	double y_1 = g_array_index(data->y, double, i);
	return y_0 + (x_new - x_0) / (x_1 - x_0) * (y_1 - y_0);
}

/**
 * Append a point to the end of the interval.
 */
void xy_data_add_point(XYData *xy_data, double x, double y) {
	if (xy_data->count_stuetzstellen != 0)
		g_return_if_fail(g_array_index(xy_data->x, double, xy_data->count_stuetzstellen - 1) < x);

	g_array_append_val(xy_data->x, x);
	g_array_append_val(xy_data->y, y);
	xy_data->count_stuetzstellen++;
}

/**
 * Use {@link #xy_data_finalize} if you are done with return value.
 *
 * @count_stuetzstellen > 1
 */
XYData *xy_data_resample(const XYData *data, guint count_stuetzstellen_new) {
	XYData *data_new = xy_data_new(count_stuetzstellen_new);
	g_return_val_if_fail(count_stuetzstellen_new >= 2, data_new);

	guint count_intervals = count_stuetzstellen_new - 1;
	double x_min = g_array_index(data->x, double, 0);
	double x_max = g_array_index(data->x, double, data->x->len - 1);
	double delta_x = (x_max - x_min)/count_intervals;

	xy_data_add_point(data_new, x_min, g_array_index(data->y, double, 0));

	for (guint i = 1; i < count_intervals; i++) {
		double x_new = x_min + delta_x * i;
		double y_new = xy_data_get_y_new(data, x_new);
		xy_data_add_point(data_new, x_new, y_new);
	}

	xy_data_add_point(data_new, x_max, g_array_index(data->y, double, data->y->len - 1));

	return data_new;
}

/**
 * Asserts linear functions between two neighbored stuetzstellen.
 *
 * Use {@link #xy_data_finalize} if you are done with return value.
 */
XYData *xy_data_integrate(const XYData *function) {
	XYData *function_new = xy_data_new(function->count_stuetzstellen);
	double value = 0;
	double x_0, x_1, y_0, y_1;

	for (int i = 0; i < function->count_stuetzstellen - 1; i++) {
		x_0 = g_array_index(function->x, double, i);
		x_1 = g_array_index(function->x, double, i+1);
		y_0 = g_array_index(function->y, double, i);
		y_1 = g_array_index(function->y, double, i+1);
		xy_data_add_point(function_new, x_0, value);
		value += (x_1 - x_0) * 1./2. * (y_0 + y_1);
	}
	xy_data_add_point(function_new, x_1, value);
	return function_new;
}

/**
 * Let i be an integer greater or equal to 0.
 * The x values of both given functions should be equal.
 * So the intervals have to be equal and the number of
 * stuetzstellen has to be equal.
 *
 * You can use {@link #xy_data_resample} to achieve that goal if the intervals
 * are already equal. There is not yet a helping function to change intervals.
 * You have to do it manually or implement one.
 *
 * Use {@link #xy_data_finalize} if you are done with return value.
 */
XYData *xy_data_multiply(const XYData *data_1, const XYData *data_2) {
	guint count_stuetzstellen = MIN(data_1->count_stuetzstellen, data_2->count_stuetzstellen);
	XYData *new = xy_data_new(count_stuetzstellen);

	for (guint i = 0; i < count_stuetzstellen; i++) {
		double new_x = g_array_index(data_1->x, double, i);
		double new_y = g_array_index(data_1->y, double, i) * g_array_index(data_2->y, double, i);
		xy_data_add_point(new, new_x, new_y);
	}

	return new;
}

/**
 * euclidic norm:
 *
 * ||f|| = sqrt( \int f(x)^2 dx )
 */
double xy_data_norm(const XYData *data) {
	XYData *square = xy_data_multiply(data, data);
	XYData *integrated = xy_data_integrate(square);
	double last_value = g_array_index(integrated->y, double, integrated->count_stuetzstellen-1);
	xy_data_finalize(square);
	xy_data_finalize(integrated);

	return sqrt(last_value);
}

/**
 * Let i be an integer greater or equal to 0.
 * The x values of both given functions should be equal.
 * So the intervals have to be equal and the number of
 * stuetzstellen has to be equal.
 *
 * You can use {@link #xy_data_resample} to achieve that goal if the intervals
 * are already equal. There is not yet a helping function to change intervals.
 * You have to do it manually or implement one.
 *
 * Use {@link #xy_data_finalize} if you are done with return value.
 *
 * @return arg1 - arg2
 */
XYData *xy_data_subtract(const XYData *data_1, const XYData *data_2) {
	guint count_stuetzstellen = MIN(data_1->count_stuetzstellen, data_2->count_stuetzstellen);
	XYData *new = xy_data_new(count_stuetzstellen);

	for (guint i = 0; i < count_stuetzstellen; i++) {
		double new_x = g_array_index(data_1->x, double, i);
		double new_y = g_array_index(data_1->y, double, i) - g_array_index(data_2->y, double, i);
		xy_data_add_point(new, new_x, new_y);
	}

	return new;
}

/**
 * d(f_1, f_2) := ||f_1 - f_2||
 *
 * Let i be an integer greater or equal to 0.
 * The x values of both given functions should be equal (because of subtraction).
 * So the intervals have to be equal and the number of
 * stuetzstellen has to be equal.
 *
 * You can use {@link #xy_data_resample} to achieve that goal if the intervals
 * are already equal. There is not yet a helping function to change intervals.
 * You have to do it manually or implement one.
 */
double xy_data_metric(const XYData *element_1, const XYData *element_2) {
	XYData *subtracted = xy_data_subtract(element_1, element_2);
	double distance = xy_data_norm(subtracted);
	xy_data_finalize(subtracted);

	return distance;
}
