/*
 * debug.h
 *
 *
 * Authors:
 *  Bernhard Schuster <bernhard@ahoi.io>
 *
 * Web page: https://ahoi.io/project/oregano
 *
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

#include <glib/gprintf.h>

#ifdef DEBUG_THIS
#undef DEBUG_THIS
#endif
#define DEBUG_THIS 0

#ifndef DEBUG_ALL
#define DEBUG_ALL 0
#endif

#define NG_DEBUG(msg, ...)                                                                         \
	{                                                                                              \
		if (DEBUG_THIS || DEBUG_ALL) {                                                             \
			g_printf ("%s:%d @ %s +++ " msg "\n", __FILE__, __LINE__, __FUNCTION__,                \
			          ##__VA_ARGS__);                                                              \
		}                                                                                          \
	}
