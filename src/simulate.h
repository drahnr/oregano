/*
 * simulate.h
 *
 *
 * Author:
 *  Andres de Barbara <adebarbara.fi.uba.ar>
 *
 * Copyright (C) 2003  Andres de Barbara
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

#ifndef __SIMULATE_H
#define __SIMULATE_H

#include <gnome.h>
#include <glib.h>
#include "schematic.h"
#include "errors.h"

/**
 * Please add a real comment. Describe:
 *
 * @param sm Schematic and
 * @param name gchar also
 * @return gchar*
 */
gchar *nl_generate (Schematic *sm, gchar *name, GError **error);

#endif
