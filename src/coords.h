/*
 * coords.c
 *
 *
 * Authors:
 *  Richard Hult <rhult@hem.passagen.se>
 *  Ricardo Markiewicz <rmarkie@fi.uba.ar>
 *  Andres de Barbara <adebarbara@fi.uba.ar>
 *  Marc Lorber <lorber.marc@wanadoo.fr>
 *  Bernhard Schuster <bernhard@ahoi.io>
 *
 * Web page: https://ahoi.io/project/oregano
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
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef __COORDS_H
#define __COORDS_H

#define COORDS_DELTA (1e-5)

#include <glib.h>
#include <math.h>

typedef struct _Coords
{
	gdouble x;
	gdouble y;
} Coords;

Coords *coords_new (gdouble x, gdouble y);

Coords *coords_new_copy (const Coords *c);

void coords_destroy (Coords *c);

/*
 * Adds b to a and returns a pointer to a which holds now the result
 */
Coords *coords_add (Coords *a, const Coords *b);

/*
 * Adds b to a and returns a ptr to a Coord struct which has to be freed by
 * either coords_destroy
 */
Coords *coords_sum_new (const Coords *a, const Coords *b);

Coords *coords_set (Coords *a, const Coords *val);

/*
 *
 */
Coords coords_sum (const Coords *a, const Coords *b);

Coords coords_sub (const Coords *a, const Coords *b);

/*
 * calculates the average of two points
 */
Coords coords_average (const Coords *a, const Coords *b);

gdouble coords_cross (const Coords *a, const Coords *b);

gdouble coords_dot (const Coords *a, const Coords *b);

gdouble coords_euclid (const Coords *a);

gdouble coords_euclid2 (const Coords *a);

gdouble coords_distance (const Coords *a, const Coords *b);

gboolean coords_equal (const Coords *a, const Coords *b);

/*
 * used for GHashTable key hashing
 */
guint coords_hash (gconstpointer v);

/*
 * used for comparsion in GHashTable
 */
gint coords_compare (const Coords *a, const Coords *b);

#endif /* __COORDS_H */
