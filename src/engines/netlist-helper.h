/*
 * netlist-helper.h
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
 * Copyright (C) 2003,2004  Ricardo Markiewicz
 * Copyright (C) 2009-2010  Marc Lorber
 * Copyright (C) 2017       Guido Trentalancia
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
#ifndef __NETLIST_HELPER_H
#define __NETLIST_HELPER_H

#include <glib.h>

#include "schematic.h"
#include "sim-settings.h"

typedef struct
{
	gint node_nr; ///< Node number
	GHashTable *pins;
	GHashTable *models;
	GSList *gnd_list;   ///< Ground parts on the schematic
	GSList *clamp_list; ///< Test clamps on the schematic
	GSList *mark_list;
	GList *node_and_number_list;
	NodeStore *store;
} NetlistData;

typedef struct
{
	gchar *cmd;
	gchar *title;
	GString *template;
	SimSettings *settings;
	NodeStore *store;
	GList *models;
} Netlist;

typedef struct
{
	gint node_nr;
	gchar *name;
} Marker;

typedef struct
{
	gint node_nr;
	Node *node;
} NodeAndNumber;

void update_schematic(Schematic *sm);
void netlist_helper_init_data (NetlistData *data);
void netlist_helper_create (Schematic *sm, Netlist *out, GError **error);
char *netlist_helper_create_analysis_string (NodeStore *store, gboolean do_ac);
GSList *netlist_helper_get_voltmeters_list (Schematic *sm, GError **error, gboolean with_type);
GSList *netlist_helper_get_voltage_sources_list (Schematic *sm, GError **error, gboolean ac_only);

#endif
