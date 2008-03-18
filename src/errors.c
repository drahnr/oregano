/*
 * errors.c
 *
 *
 * Author:
 *  Ricardo Markiewicz <rmarkie@fi.uba.ar>
 *
 * Copyright (C) 2003-2008  Ricardo Markiewicz
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

#include "errors.h"

GQuark
oregano_error_quark (void) 
{
	static GQuark err = 0;
	if (!err) {
		err = g_quark_from_static_string ("oregano-error");
	}
	return err;
}

