/*
 * debug.h
 *
 *
 * Authors:
 *  Bernhard Schuster <bernhard@ahoi.io>
 *  Daniel Dwek <todovirtual15@gmail.com>
 *
 * Web page: https://ahoi.io/project/oregano
 *
 * Copyright (C) 2012-2013  Bernhard Schuster
 * Copyright (C) 2022-2023  Daniel Dwek
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

#define color_black		"\033[00;30m"
#define color_red		"\033[00;31m"
#define color_green		"\033[00;32m"
#define color_orange		"\033[00;33m"
#define color_blue		"\033[00;34m"
#define color_magenta		"\033[00;35m"
#define color_cyan		"\033[00;36m"
#define color_lightgray		"\033[00;37m"
#define color_darkgray		"\033[01;30m"
#define color_lightred		"\033[01;31m"
#define color_lightgreen	"\033[01;32m"
#define color_yellow		"\033[01;33m"
#define color_lightblue		"\033[01;34m"
#define color_lightmagenta	"\033[01;35m"
#define color_lightcyan		"\033[01;36m"
#define color_white		"\033[01;37m"

#define color_restore		"\033[00m"

#define COLOR_SHEETITEM		color_blue
#define COLOR_COORDS		color_green
#define COLOR_CENTER		color_red
#define COLOR_NORMAL		color_restore
