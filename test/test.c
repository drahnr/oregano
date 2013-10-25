#include <glib.h>
#include "coords.h"
#include "wire.h"
#include "node-store.h"

void
test_coords ()
{
	const Coords l1 = {-100.,+100.};
	const Coords l2 = {+100.,+100.};
	Coords tmp;

	g_assert_cmpfloat (coords_cross (&l1, &l2), ==, -2e+4);

	g_assert_cmpfloat (coords_dot (&l1,&l2), ==, 0.);

	tmp = coords_average (&l1,&l2);
	g_assert_cmpfloat (tmp.x, ==, 0.);
	g_assert_cmpfloat (tmp.y, ==, 100.);

	g_assert_cmpfloat (coords_euclid (&l1), ==, coords_euclid (&l2));

	tmp = coords_sum (&l1, &l2);
	g_assert_cmpfloat (tmp.x, ==, 0.);
	g_assert_cmpfloat (tmp.y, ==, 200.);
}

void
test_wire_wire_interaction ()
{
	// test the most fragile stuff here


	Coords p1 = {100.,0.};
	Coords l1 = {-100.,100.};
	Coords p2 = {0.,0.};
	Coords l2 = {100.,100.};

	Wire *a = wire_new (NULL);
	Wire *b = wire_new (NULL);

	Coords where = {-77777.77,-77.7777};
	Coords expected = {50.0,50.0};

	item_data_set_pos (ITEM_DATA (a), &p1);
	wire_set_length (a, &l1);
	item_data_set_pos (ITEM_DATA (b), &p2);
	wire_set_length (b, &l2);


	extern gboolean do_wires_intersect (Wire *a, Wire *b, Coords *where);

	g_assert (do_wires_intersect (a,b,&where));
	g_assert (coords_equal (&where, &expected));

	g_object_unref (a);
	g_object_unref (b);
}

int
main (int argc, char *argv[])
{
	g_test_init (&argc, &argv, NULL);
	g_test_add_func ("/core/coords", test_coords);
	g_test_add_func ("/core/model/wires", test_wire_wire_interaction);
	return g_test_run ();
}
