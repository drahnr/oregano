/*
 * coords.c
 *
 *
 * Authors:
 *  Richard Hult <rhult@hem.passagen.se>
 *  Ricardo Markiewicz <rmarkie@fi.uba.ar>
 *  Andres de Barbara <adebarbara@fi.uba.ar>
 *  Marc Lorber <lorber.marc@wanadoo.fr>
 *  Bernhard Schuster <schuster.bernhard@gmail.com>
 *
 * Web page: https://srctwig.com/oregano
 *
 * Copyright (C) 1999-2001  Richard Hult
 * Copyright (C) 2003,2004  Ricardo Markiewicz
 * Copyright (C) 2009-2012  Marc Lorber
 * Copyright (C) 2012-2013  Bernhard Schuster
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
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "coords.h"

#define COORDS_DELTA (1e-5)


inline Coords *
coords_new (gdouble x, gdouble y)
{
	Coords *c = g_malloc (sizeof (Coords));
	if (G_LIKELY(c)) {
		c->x = x;
		c->y = y;
	}
	return c;
}

inline Coords *
coords_new_copy (const Coords *src)
{
	return coords_new (src->x, src->y);
}

inline void
coords_destroy (Coords *c)
{
	if (G_LIKELY (c))
		g_free (c);
}

inline Coords *
coords_add (Coords *a, const Coords *b)
{
	if (!a || !b)
		return NULL;
	a->x += b->x;
	a->y += b->y;
	return a;
}

inline Coords
coords_sum (const Coords *a, const Coords *b)
{
	g_assert (a);
	g_assert (b);

	Coords c;
	c.x = a->x + b->x;
	c.y = a->y + b->y;
	return c;
}

inline Coords
coords_sub (const Coords *a, const Coords *b)
{
	g_assert (a);
	g_assert (b);

	Coords c;
	c.x = a->x - b->x;
	c.y = a->y - b->y;
	return c;
}

inline Coords *
coords_sum_new (const Coords *a, const Coords *b)
{
	if (G_UNLIKELY (!a || !b))
		return NULL;
	Coords *c = coords_new (a->x, a->y);
	c->x += b->x;
	c->y += b->y;
	return c;
}

inline Coords *
coords_set (Coords *a, const Coords *val)
{
	if (G_UNLIKELY (!a || !val))
		return NULL;
	a->x = val->x;
	a->y = val->y;
	return a;
}

inline Coords
coords_average (const Coords *a, const Coords *b)
{
	Coords r = (Coords)(*a);
	r.x += b->x;	r.x /= 2.0;
	r.y += b->y;	r.y /= 2.0;
	return r;
}

inline gdouble
coords_dot (const Coords *a, const Coords *b)
{
	return (a->x * b->x) + (a->y * b->y);
}

inline gdouble
coords_cross (const Coords *a, const Coords *b)
{
	return (a->x * b->y) - (a->y * b->x);
}

inline gdouble
coords_euclid (const Coords *a)
{
	return sqrt (coords_dot (a,a));
}

inline gdouble
coords_euclid2 (const Coords *a)
{
	return coords_dot (a,a);
}

#define CIRCLERSHIFT(x,r) ((x>>r) | (x<<(sizeof(x)*8-r)))
#define CIRCLELSHIFT(x,l) ((x<<l) | (x>>(sizeof(x)*8-l)))
inline guint
coords_hash (gconstpointer v)
{
	const Coords *c = v;
	const guint x = (guint)(c->x);
	const guint y = (guint)(c->y);
	return CIRCLELSHIFT(x,7) ^ CIRCLERSHIFT(y,3);
}


inline gboolean
coords_equal (const Coords *a, const Coords *b)
{
	return G_UNLIKELY(fabs(a->x - b->x) < COORDS_DELTA &&
	                  fabs(a->y - b->y) < COORDS_DELTA);
}
