/*
 * oregano-utils.h
 *
 *
 * Author:
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
 * Copyright (C) 2013       Bernhard Schuster
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

#ifndef __OREGANO_UTILS_H
#define __OREGANO_UTILS_H

gdouble oregano_strtod (const gchar *str, const gchar unit);

#define DEGSANITY(x)                                                                               \
	do {                                                                                           \
		while (rotation < 0)                                                                       \
			x += 360;                                                                              \
		x %= 360;                                                                                  \
	} while (0)
#define DEG2RAD(x) ((double)x * M_PI / 180.)
#define RAD2DEG(x) ((double)x * 180. / M_PI)
#define COORDS_AVERAGE(tl, br)                                                                     \
	{                                                                                              \
		(tl.x + br.x) / 2., (tl.y + br.y) / 2.                                                     \
	}

#endif
