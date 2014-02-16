#ifndef TEST_WIRE_H
#define TEST_WIRE_H

#include <glib.h>

#include "coords.h"
#include "wire.h"

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

#define NODE_EPSILON 1e-3
#include "node-store.h"

void
test_wire_intersection ()
{
	extern gboolean do_wires_intersect (Wire *a, Wire *b, Coords *where);
	// intersection
	Coords p1 = {100.,0.};
	Coords l1 = {-100.,100.};
	Coords p2 = {0.,0.};
	Coords l2 = {100.,100.};
	Coords where = {-77777.77,-77.7777};
	const Coords expected = {50.0,50.0};

	Wire *a = wire_new (NULL);
	Wire *b = wire_new (NULL);

	item_data_set_pos (ITEM_DATA (a), &p1);
	wire_set_length (a, &l1);
	item_data_set_pos (ITEM_DATA (b), &p2);
	wire_set_length (b, &l2);

	g_assert (do_wires_intersect (a,b,&where));
	g_assert (coords_equal (&where, &expected));

	g_object_unref (a);
	g_object_unref (b);

}


void
test_wire_tcrossing ()
{
	extern gboolean is_t_crossing (Wire *a, Wire *b, Coords *t);
	// t crossing
	Coords p1 = {50.,0.};
	Coords l1 = {0., 100.};
	Coords p2 = {0.,0.};
	Coords l2 = {100.,0.};
	Coords where = {-77.77,-77.77};
	const Coords expected = p1;

	Wire *a = wire_new (NULL);
	Wire *b = wire_new (NULL);

	{
		item_data_set_pos (ITEM_DATA (a), &p1);
		wire_set_length (a, &l1);
		item_data_set_pos (ITEM_DATA (b), &p2);
		wire_set_length (b, &l2);

		g_assert (is_t_crossing (a, b, &where));
		g_assert (coords_equal (&where, &expected));
	}

	{
		item_data_set_pos (ITEM_DATA (a), &p1);
		wire_set_length (a, &l2);
		item_data_set_pos (ITEM_DATA (b), &p2);
		wire_set_length (b, &l1);

		g_assert (!is_t_crossing (a, b, &where));
	}
	g_object_unref (a);
	g_object_unref (b);
}
#endif
