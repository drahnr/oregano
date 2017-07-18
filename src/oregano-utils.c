/*
 * oregano-utils.c
 *
 *
 * Author:
 *  Richard Hult <rhult@hem.passagen.se>
 *  Ricardo Markiewicz <rmarkie@fi.uba.ar>
 *  Andres de Barbara <adebarbara@fi.uba.ar>
 *  Marc Lorber <lorber.marc@wanadoo.fr>
 *  Guido Trentalancia <guido@trentalancia.com>
 *
 * Web page: https://ahoi.io/project/oregano
 *
 * Copyright (C) 1999-2001  Richard Hult
 * Copyright (C) 2003,2006  Ricardo Markiewicz
 * Copyright (C) 2009-2012  Marc Lorber
 * Copyright (C) 2017       Guido Trentalancia
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
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include <string.h>
#include <gtk/gtk.h>

#include "debug.h"
#include "oregano-utils.h"

gdouble oregano_strtod (const gchar *str, const gchar *unit)
{
	gboolean unit_does_not_match = FALSE;
	gdouble ret;
	gchar *endptr, *c;
	size_t unit_length = 0;

	if (unit)
		unit_length = strlen (unit);

	if (!str)
		return 0.0;

	ret = g_ascii_strtod (str, &endptr);

	for (c = endptr; *c; c++) {
		switch (*c) {
		case 'T':
			ret *= 1e12;
			break;
		case 'G':
			ret *= 1e9;
			break;
		case 'M':
			ret *= 1e6;
			break;
		case 'k':
			ret *= 1e3;
			break;
		case 'm':
			ret *= 1e-3;
			break;
		case 'u':
			ret *= 1e-6;
			break;
		case 'n':
			ret *= 1e-9;
			break;
		case 'p':
			ret *= 1e-12;
			break;
		case 'f':
			ret *= 1e-15;
			break;
		case ' ':
			break;
		default:
			if (c) {
				if (!g_ascii_strncasecmp (c, unit, unit_length)) {
					return ret;
				} else {
					unit_does_not_match = TRUE;
					break;
				}
			}
			break;
		}
	}

	if (unit_does_not_match)
		g_printf ("Unexpected unit of measurement\n");	

	return ret;
}
