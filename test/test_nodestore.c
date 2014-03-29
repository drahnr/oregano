#ifndef TEST_NODESTORE
#define TEST_NODESTORE

#include <glib.h>

void
test_nodestore ()
{
	gint i;
	NodeStore *store;
	Part *part;
	Wire *wire;
	Node *node;

	Coords p_pos = {111.,22.};
	Coords n_pos = {111.,33.};
	Coords w_pos = {111.,7.};
	Coords w_len = {0.,88.};

	store = node_store_new ();
	part = part_new ();
	wire = wire_new ();

	// add one Pin with a offset that is on the wire when rotation N*Pi times
	Pin *pin = g_new (Pin, 1);
	pin->offset.x = n_pos.x - p_pos.x;
	pin->offset.y = n_pos.y - p_pos.y;
	GSList *list = NULL;
	list = g_slist_prepend (list, pin);
	part_set_pins (part, list);
	g_slist_free (list);

	item_data_set_pos (ITEM_DATA (part), &p_pos);

	item_data_set_pos (ITEM_DATA (wire), &w_pos);
	wire_set_length (wire, &w_len);

	node_store_add_part (store, part);
	node_store_add_wire (store, wire);

	{
		for (i=0; i<11; i++)
			item_data_rotate (ITEM_DATA (part), 90, NULL);
		item_data_set_pos (ITEM_DATA (part), &w_len);
		for (i=0; i<4; i++)
			item_data_rotate (ITEM_DATA (part), 90, NULL);
		item_data_set_pos (ITEM_DATA (part), &n_pos);
		for (i=0; i<7; i++)
			item_data_rotate (ITEM_DATA (part), -90, NULL);
		item_data_set_pos (ITEM_DATA (part), &p_pos);
	}
	g_assert (node_store_is_wire_at_pos (store, n_pos));
	g_assert (node_store_is_pin_at_pos (store, n_pos));

	node = node_store_get_node (store, n_pos);
	g_assert (!node_is_empty (node));
	g_assert (node_needs_dot (node));

	node_store_remove_part (store, part);
	node_store_remove_wire (store, wire);

	g_object_unref (store);
}

#endif
