/*
 * gplotfunction.c
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

#include "gplot-internal.h"
#include "gplotfunction.h"

	
static void
g_plot_function_base_init (gpointer g_class)
{
	static gboolean initialized = FALSE;
	if (!initialized) {
		// create interface signals here.
		initialized = TRUE;

		g_object_interface_install_property (g_class,
			g_param_spec_boolean ("visible",
				"gplot_function_visible",
				"Get/Set if function is visible",
				TRUE,
				G_PARAM_READWRITE));
	}
}

GType
g_plot_function_get_type (void)
{
	static GType type = 0;
		if (type == 0) {
			static const GTypeInfo info = {
				sizeof (GPlotFunctionClass),
				g_plot_function_base_init,   // base_init
				NULL,   // base_finalize
				NULL,   // class_init
				NULL,   // class_finalize
				NULL,   // class_data
				0,
				0,      // n_preallocs
				NULL    // instance_init
			};
			type = g_type_register_static (G_TYPE_INTERFACE, "GPlotFunction", &info, 0);
		}
		return type;
}

void
g_plot_function_draw (GPlotFunction *self, cairo_t * cr, GPlotFunctionBBox *bbox)
{
  if (GPLOT_FUNCTION_GET_CLASS (self)->draw)
  	GPLOT_FUNCTION_GET_CLASS (self)->draw (self, cr, bbox);
}

void
g_plot_function_get_bbox (GPlotFunction *self, GPlotFunctionBBox *bbox) 
{
  if (GPLOT_FUNCTION_GET_CLASS (self)->get_bbox)
  	GPLOT_FUNCTION_GET_CLASS (self)->get_bbox (self, bbox);
}

