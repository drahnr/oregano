/*
 * oregano-utils.c
 *
 *
 * Author:
 *  Richard Hult <rhult@hem.passagen.se>
 *  Ricardo Markiewicz <rmarkie@fi.uba.ar>
 *  Andres de Barbara <adebarbara@fi.uba.ar>
 *
 * Web page: http://arrakis.lug.fi.uba.ar/
 *
 * Copyright (C) 1999-2001  Richard Hult
 * Copyright (C) 2003,2006  Ricardo Markiewicz
 *
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
#include <gtk/gtk.h>
#include "oregano-utils.h"

double
oregano_strtod (const gchar *str, const gchar unit)
{
	double ret;
	gchar *endptr, *c;

	if (!str)
		return 0.0;

	ret = g_strtod (str, &endptr);
	for (c = endptr; *c; c++) {
		switch (*c) {
		case 'T':
			ret *= 1e12;
			return ret;
		case 'G':
			ret *= 1e9;
			return ret;
		case 'M':
			ret *= 1e6;
			return ret;
		case 'k':
			ret *= 1e3;
			return ret;
		case 'm':
			ret *= 1e-3;
			return ret;
		case 'u':
			ret *= 1e-6;
			return ret;
		case 'n':
			ret *= 1e-9;
			return ret;
		case 'p':
			ret *= 1e-12;
			return ret;
		case 'f':
			ret *= 1e-15;
			return ret;
		default:
			if (*c == unit)
				return ret;
			break;
		}
	}
	return ret;
}
