/*
 * gplotlines.h
 *
 * Authors:
 *  Ricardo Markiewicz <rmarkie@fi.uba.ar>
 *  Marc Lorber <lorber.marc@wanadoo.fr>
 * 
 * Web page: https://github.com/marc-lorber/oregano
 *
 * Copyright (C) 1999-2001  Richard Hult
 * Copyright (C) 2003,2004  Ricardo Markiewicz
 * Copyright (C) 2009-2012  Marc Lorber
 *
 * This library is free software; you can redistribute it and/or
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef _GPLOT_LINES_H_
#define _GPLOT_LINES_H_

#include "gplotfunction.h" 


typedef enum {
	FUNCTIONAL_CURVE=0,
	FREQUENCY_PULSE
} GraphicType;

#define GPLOT_LINES(obj)        G_TYPE_CHECK_INSTANCE_CAST (obj, TYPE_GPLOT_LINES, GPlotLines)
#define IS_GPLOT_LINES(obj)     G_TYPE_CHECK_INSTANCE_TYPE (obj, TYPE_GPLOT_LINES)

GPlotFunction* g_plot_lines_new (gdouble *x, gdouble *y, guint points);

#endif
