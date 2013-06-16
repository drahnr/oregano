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
 * Web page: https://github.com/drahnr/oregano
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

#ifndef __COORDS_H
#define __COORDS_H

#include <glib.h>
#include <math.h>

typedef struct _Coords {
	gdouble x;
	gdouble y;
} Coords;


Coords *
coords_new (gdouble x, gdouble y);

Coords *
coords_new_copy (Coords *c);

void
coords_destroy (Coords *c);

gboolean
coords_equal (Coords *a, Coords *b);

/*
 * Adds b to a and returns a pointer to a which holds now the result
 */
Coords *
coords_add (Coords *a, Coords *b);

/*
 * Adds b to a and returns a ptr to a Coord struct which has to be freed by either coords_destroy
 */
Coords *
coords_sum_new (Coords *a, Coords *b);

Coords *
coords_set (Coords *a, Coords *val);

/*
 *
 */
Coords
coords_sum (Coords *a, Coords *b);

Coords
coords_sub (Coords *a, Coords *b);


/*
 * calculates the average of two points
 */
Coords
coords_average (const Coords *a, const Coords *b);
#endif /* __COORDS_H */
