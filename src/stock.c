/*
 * stock.c
 *
 *
 * Authors:
 *  Richard Hult <rhult@hem.passagen.se>
 *  Ricardo Markiewicz <rmarkie@fi.uba.ar>
 *  Andres de Barbara <adebarbara@fi.uba.ar>
 *  Marc Lorber <lorber.marc@wanadoo.fr>
 *
 * Web page: https://ahoi.io/project/oregano
 *
 * Copyright (C) 1999-2001  Richard Hult
 * Copyright (C) 2003,2006  Ricardo Markiewicz
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
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */
/*
 * Stock icon code, stolen from:
 *  Eye of Gnome image viewer - stock icons
 *
 * Copyright (C) 1999 The Free Software Foundation
 *
 * Author: Federico Mena-Quintero <federico@gimp.org>
 */

#include <gdk/gdk.h>
#include <gtk/gtk.h>

#include "stock.h"
#include "stock/sim-settings.xpm"
#include "stock/rotate.xpm"
#include "stock/zoom_in.xpm"
#include "stock/zoom_out.xpm"
#include "stock/plot.xpm"
#include "stock/part-browser.xpm"
#include "stock/grid.xpm"
#include "stock/arrow.xpm"
#include "stock/text.xpm"
#include "stock/wire.xpm"
#include "stock/voltmeter.xpm"
#include "stock/vclamp.xpm"
#include "stock/zoom_pan.xpm"
#include "stock/zoom_region.xpm"

static void add_stock_entry (const gchar *stock_id, char **xpm_data)
{
	static GtkIconFactory *factory = NULL;
	GdkPixbuf *pixbuf;
	GtkIconSet *icon_set;

	if (!factory) {
		factory = gtk_icon_factory_new ();
		gtk_icon_factory_add_default (factory);
	}

	pixbuf = gdk_pixbuf_new_from_xpm_data ((const gchar **)xpm_data);
	icon_set = gtk_icon_set_new_from_pixbuf (pixbuf);
	gtk_icon_factory_add (factory, stock_id, icon_set);
	gtk_icon_set_unref (icon_set);
	g_object_unref (G_OBJECT (pixbuf));
}

void stock_init (void)
{
	add_stock_entry (STOCK_PIXMAP_SIM_SETTINGS, sim_settings_xpm);
	add_stock_entry (STOCK_PIXMAP_ROTATE, rotate_xpm);
	add_stock_entry (STOCK_PIXMAP_ZOOM_IN, zoom_in_xpm);
	add_stock_entry (STOCK_PIXMAP_ZOOM_OUT, zoom_out_xpm);
	add_stock_entry (STOCK_PIXMAP_PLOT, plot_xpm);
	add_stock_entry (STOCK_PIXMAP_PART_BROWSER, part_browser_xpm);
	add_stock_entry (STOCK_PIXMAP_GRID, grid_xpm);
	add_stock_entry (STOCK_PIXMAP_ARROW, arrow_xpm);
	add_stock_entry (STOCK_PIXMAP_TEXT, text_xpm);
	add_stock_entry (STOCK_PIXMAP_WIRE, wire_xpm);
	add_stock_entry (STOCK_PIXMAP_VOLTMETER, voltmeter_xpm);
	add_stock_entry (STOCK_PIXMAP_V_CLAMP, vclamp_xpm);
	add_stock_entry (STOCK_PIXMAP_ZOOM_PAN, zoom_pan_xpm);
	add_stock_entry (STOCK_PIXMAP_ZOOM_REGION, zoom_region_xpm);
}
