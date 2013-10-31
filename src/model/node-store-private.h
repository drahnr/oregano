#include <glib.h>

#include "coords.h"
#include "node.h"
#include "wire.h"

#include "debug.h"

static void
add_node (gpointer key, Node *node, GList **list)
{
	*list = g_list_prepend (*list, node);
}




static void
add_node_position (gpointer key, Node *node, GList **list)
{
	if (node_needs_dot (node))
		*list = g_list_prepend (*list, key);
}


/**
 * check if 2 wires intersect
 */
static gboolean
do_wires_intersect (Wire *a, Wire *b, Coords *where)
{
	g_assert (a);
	g_assert (b);
	Coords delta1, start1;
	Coords delta2, start2;
	wire_get_pos_and_length (a, &start1, &delta1);
	wire_get_pos_and_length (b, &start2, &delta2);

	// parallel check
	const gdouble d = coords_cross (&delta1, &delta2);
	if (fabs(d) < NODE_EPSILON) {
	    NG_DEBUG ("do_wires_intersect(%p,%p): NO! d=%lf\n", a,b, d);
		return FALSE;
	}

	// implemented according to
	// http://stackoverflow.com/questions/563198/how-do-you-detect-where-two-line-segments-intersect
	const Coords qminusp = coords_sub (&start2, &start1);

	// p = start1, q = start2, r = delta1, s = delta2
	const gdouble t = coords_cross (&qminusp, &delta1) / d;
	const gdouble u = coords_cross (&qminusp, &delta2) / d;

	if (t >= -NODE_EPSILON && t-NODE_EPSILON <= 1.f &&
	    u >= -NODE_EPSILON && u-NODE_EPSILON <= 1.f) {
	    NG_DEBUG ("do_wires_intersect(%p,%p): YES! t,u = %lf,%lf\n",a,b, t, u);
	    if (where) {
			where->x = start1.x + u * delta1.x;
			where->y = start1.y + u * delta1.y;
	    }
	    return TRUE;
	}
	NG_DEBUG ("do_wires_intersect(%p,%p): NO! t,u = %lf,%lf\n",a,b, t, u);
	return FALSE;
}





/**
 * evaluates if x1,y1 to x2,y2 contain pos.x/pos.y if connected by a birds eye line
 */
static inline gboolean
is_wire_at_pos (double x1, double y1, double x2, double y2, Coords pos)
{
	//calculate the hessenormalform and check the results vs eps
	// 0 = (vx - vp0) dot (n)
	double d;
	Coords a, n;

	a.x = x2 - x1;
	a.y = y2 - y1;
	n.x = -a.y;
	n.y = +a.x;

	d = (pos.x - x1) * n.x + (pos.y - y1) * n.y;

	if (fabs(d) > NODE_EPSILON)
		return FALSE;

	if (pos.x<MIN(x1,x2) || pos.x>MAX(x1,x2) || pos.y<MIN(y1,y2) || pos.y>MAX(y1,y2))
		return FALSE;

	NG_DEBUG ("on wire (DIST=%g): linear start:(%g %g); end:(%g %g); point:(%g %g)", d, x1, y1, x2, y2, pos.x, pos.y);

	return TRUE;
}

static inline gboolean
is_wire_at_coords(Wire *w, Coords *coo)
{
	Coords p1, p2, len;
	wire_get_pos_and_length (w, &p1, &len);
	p2 = p1;
	coords_add (&p2, &len);
	return is_wire_at_pos (p1.x, p1.y, p2.x, p2.y, *coo);
}


/**
 * @param store which NodeStore to use
 * @param pos where to look for wires
 * @returns list of wire at position p (including endpoints)
 */
static GSList *
get_wires_at_pos (NodeStore *store, Coords pos)
{
	GList *list;
	GSList *wire_list;
	Wire *wire;
	Coords start, end;

	g_return_val_if_fail (store, FALSE);
	g_return_val_if_fail (IS_NODE_STORE (store), FALSE);

	wire_list = NULL;

	for (list = store->wires; list; list = list->next) {
		wire = list->data;

		if (is_wire_at_coords (wire, &pos))
			wire_list = g_slist_prepend (wire_list, wire);
	}

	return wire_list;
}




/**
 * projects the point @p onto wire @w
 * @param w the wire
 * @param p the point
 * @param projected [out] the point p is projected onto - this is always filled no matter of return value
 * @param d [out] the distance between @p and @projected - this is always filled no matter of return value
 * @returns TRUE if the point was within the line segement bounds, else FALSE
 */
static gboolean
project_point_on_wire (Wire *w, Coords *p, Coords *projected, gdouble *d)
{
	Coords cstart, clen;
	wire_get_pos_and_length (w, &cstart, &clen);

	const Coords delta = coords_sub (p, &cstart);
	const gdouble l2 = coords_euclid2 (&clen);
	const gdouble lambda = coords_dot (&delta, &clen) / l2;
	Coords pro = {cstart.x + lambda * clen.x,
	              cstart.y + lambda * clen.y};
	if (projected) {
		*projected = pro;
	}
	if (d) {
		pro.x -= p->x;
		pro.y -= p->y;
		*d = coords_euclid2(&pro);
	}

	return (lambda >= 0.-NODE_EPSILON &&
	        lambda <= 1.+NODE_EPSILON);
}



/**
 * test if point is within a wire
 * @param w the wire
 * @param p the point to test
 */
static gboolean
is_point_on_wire (Wire *w, Coords *p)
{
	g_assert (w);
	g_assert (p);
	gdouble d = -7777.7777;
	const gboolean ret = project_point_on_wire (w, p, NULL, &d);
	return (ret && fabs(d)<NODE_EPSILON);
}


/**
 * check if the two given wires have any incomon endpoints
 * @param w the wire
 * @param p the point to test
 */
static gboolean
is_point_at_end_of_wire (Wire *w, Coords *p)
{
	g_assert (w);
	g_assert (p);

	Coords start1, end1;
	wire_get_start_and_end_pos (w, &start1, &end1);

	return coords_equal (&start1, p) ||
		   coords_equal (&end1, p);
}

/**
 * check if 2 wires are colinear/parallel
 */
static gboolean
check_colinear (Wire *a, Wire *b)
{
	Coords la, sa;
	Coords lb, sb;
	wire_get_pos_and_length (a, &sa, &la);
	wire_get_pos_and_length (b, &sb, &lb);

// below implementations are idential
#if 0
	Coords nb;
	n1.x = +lb.y;
	n1.y = -lb.x;
	return fabs(coords_dot (&la,&nb)) < NODE_EPSILON;
#else
	return fabs(coords_cross(&la,&lb)) < NODE_EPSILON;
#endif
}


/**
 * check if the two given wires overlap
 * @param so start overlap
 * @param se end overlap
 */
static gboolean
do_wires_overlap (Wire *a, Wire *b, Coords *so, Coords *eo)
{
	g_assert (a);
	g_assert (b);
	Coords sa, la, ea;
	Coords sb, lb, eb;

	wire_get_pos_and_length (a, &sa, &la);
	wire_get_pos_and_length (b, &sb, &lb);
	ea = coords_sum (&sa, &la);
	eb = coords_sum (&sb, &lb);

	// parallel check
	if (!check_colinear (a,b))
		return FALSE;

	const gboolean sb_on_a = is_point_on_wire (a, &sb);
	const gboolean eb_on_a = is_point_on_wire (a, &eb);
	const gboolean sa_on_b = is_point_on_wire (b, &sa);
	const gboolean ea_on_b = is_point_on_wire (b, &ea);

	// a-----b++++++b---a
	if (sb_on_a && eb_on_a) {
		*so = sb;
		*eo = eb;
		return TRUE;
	}
	if (sa_on_b && ea_on_b) {
		*so = sa;
		*eo = ea;
		return TRUE;
	}

	// a----b~~~~a++++++b
	// this also covers "touching" of colinear wires
	if (sb_on_a && sa_on_b) {
		*so = sb;
		*eo = sa;
		return TRUE;
	}
	if (eb_on_a && ea_on_b) {
		*so = eb;
		*eo = ea;
		return TRUE;
	}
	if (sb_on_a && ea_on_b) {
		*so = sb;
		*eo = ea;
		return TRUE;
	}
	if (eb_on_a && ea_on_b) {
		*so = eb;
		*eo = sa;
		return TRUE;
	}
	return FALSE;
}


/**
 * check if wires a and b form a t crossing
 * @param a
 * @param b
 * @param t [out] which endpoint is the T point (where both wires "intersect")
 * @returns which of the inputs
 * @attention wire @a and @b are taken as granted to be intersecting
 */
static gboolean
is_t_crossing (Wire *a, Wire *b, Coords *t)
{
	g_assert (a);
	g_assert (b);

	Coords sa, ea, sb, eb;

	wire_get_start_and_end_pos (a, &sa, &ea);
	wire_get_start_and_end_pos (b, &sb, &eb);

	if (is_point_on_wire (a, &sb) && !is_point_on_wire (a, &sb)) {
		if (t)
			*t = sb;
		return TRUE;
	}
	if (is_point_on_wire (a, &eb) && !is_point_on_wire (a, &eb)) {
		if (t)
			*t = eb;
		return TRUE;
	}
	if (is_point_on_wire (b, &sa) && !is_point_on_wire (b, &sa)) {
		if (t)
			*t = sa;
		return TRUE;
	}
	if (is_point_on_wire (b, &ea) && !is_point_on_wire (b, &ea)) {
		if (t)
			*t = ea;
		return TRUE;
	}
	return FALSE;
}



/**
 * merge wire @b into wire @a, where @so and @eo are the overlapping wire part
 * @param a wire
 * @param b wire to merge into @a
 * @param so [out] coords of the overlapping wire part - start
 * @param eo [out] coords of the overlapping wire part - end
 * @returns the pointer to a, or NUL if something went wrong
 * @attention only ever call this for two parallel and overlapping wires!
 */
static Wire *
vulcanize_wire (NodeStore *store, Wire *a, Wire *b, Coords *so, Coords *eo)
{
	g_assert (store);
	g_assert (IS_NODE_STORE (store));
	g_assert (a);
	g_assert (IS_WIRE (a));
	g_assert (b);
	g_assert (IS_WIRE (b));

	Coords starta, enda;
	Coords startb, endb;
	GSList *list;

	wire_get_start_pos (a, &starta);
	wire_get_end_pos (a, &enda);
	wire_get_start_pos (b, &startb);
	wire_get_end_pos (b, &endb);

	Coords start, end, len;
	start.x = MIN(MIN(starta.x, startb.x),MIN(enda.x,endb.x));
	start.y = MIN(MIN(starta.y, startb.y),MIN(enda.y,endb.y));
	end.x = MAX(MAX(starta.x, startb.x),MAX(enda.x,endb.x));
	end.y = MAX(MAX(starta.y, startb.y),MAX(enda.y,endb.y));
	len.x = end.x - start.x;
	len.y = end.y - start.y;

	g_assert ((fabs(len.x) < NODE_EPSILON) ^ (fabs(len.y) < NODE_EPSILON));

//FIXME register and unregister to new position
#define CREATE_NEW_WIRE 0
	// always null, or schematic_add_item in create_wire
	// will return pure bogus (and potentially crash!)
#if CREATE_NEW_WIRE
	Wire *w = wire_new (item_data_get_grid (ITEM_DATA (a)));
	g_return_val_if_fail (w, NULL);
	g_return_val_if_fail (IS_WIRE (w), NULL);
#else
	Wire *w = a;
#endif
	item_data_set_pos (ITEM_DATA (w), &start);
	wire_set_length (w, &len);

	for (list = wire_get_nodes(b); list;) {
		Node *n = list->data;
		list = list->next;
		if (!IS_NODE (n))
			g_warning ("Found bogus node entry in wire %p, ignored.", b);
		wire_add_node (w, n);
		node_add_wire (n, w);
	}
#if CREATE_NEW_WIRE
	node_store_remove_wire (store, a);//equiv wire_unregister
#endif
	node_store_remove_wire (store, b);//equiv wire_unregister
	wire_dbg_print (w);
	return w;
}

