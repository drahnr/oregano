/*
 * errors.h
 *
 *
 * Author:
 *  Ricardo Markiewicz <rmarkie@fi.uba.ar>
 *  Marc Lorber <lorber.marc@wanadoo.fr>
 *
 * Web page: https://github.com/marc-lorber/oregano
 * 
 * Copyright (C) 2003-2008  Ricardo Markiewicz
 * Copyright (C) 2009-2012  Marc Lorber
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

#ifndef __ERRORS_H
#define __ERRORS_H

#include <glib.h>

#define OREGANO_ERROR (oregano_error_quark())

GQuark oregano_error_quark (void);

typedef enum {
	OREGANO_SIMULATE_ERROR_NO_GND,
	OREGANO_SIMULATE_ERROR_NO_CLAMP,
	OREGANO_SIMULATE_ERROR_IO_ERROR,
	OREGANO_SCHEMATIC_BAD_FILE_FORMAT,
	OREGANO_SCHEMATIC_FILE_NOT_FOUND,
} OREGANO_ERRORS;

#endif
