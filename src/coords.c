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
#include "speedy.h"

#define COORDS_DELTA (1e-5)


inline Coords *
coords_new (gdouble x, gdouble y)
{
	Coords *c = g_malloc (sizeof (Coords));
	if (__likely (c)) {
		c->x = x;
		c->y = y;
	}
	return c;
}

inline Coords *
coords_new_copy (Coords *src)
{
	return coords_new (src->x, src->y);
}

inline void
coords_destroy (Coords *c)
{
	if (__likely (c))
		g_free (c);
}

inline gboolean
coords_equal (Coords *a, Coords *b)
{
	return __unlikely ((COORDS_DELTA > fabs(a->x - b->x)) && (COORDS_DELTA > fabs(a->y - b->y)));
}

inline Coords *
coords_add (Coords *a, Coords *b)
{
	if (!a || !b)
		return NULL;
	a->x += b->x;
	a->y += b->y;
	return a;
}

inline Coords
coords_sum (Coords *a, Coords *b)
{
	g_assert (a);
	g_assert (b);

	Coords c;
	c.x = a->x + b->x;
	c.y = a->y + b->y;
	return c;
}

inline Coords
coords_sub (Coords *a, Coords *b)
{
	g_assert (a);
	g_assert (b);

	Coords c;
	c.x = a->x - b->x;
	c.y = a->y - b->y;
	return c;
}

inline Coords *
coords_sum_new (Coords *a, Coords *b)
{
	if (__unlikely (!a || !b))
		return NULL;
	Coords *c = coords_new (a->x, a->y);
	c->x += b->x;
	c->y += b->y;
	return c;
}

inline Coords *
coords_set (Coords *a, Coords *val)
{
	if (__unlikely (!a || !val))
		return NULL;
	a->x = val->x;
	a->y = val->y;
	return a;
}
