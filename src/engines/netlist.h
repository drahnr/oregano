/*
 * netlist.h
 *
 *
 * Author:
 *  Richard Hult <rhult@hem.passagen.se>
 *  Ricardo Markiewicz <rmarkie@fi.uba.ar>
 *  Andres de Barbara <adebarbara@fi.uba.ar>
 *
 * Web page: http://arrakis.lug.fi.uba.ar/
 *
 * Copyright (C) 1999-2001  Richard Hult
 * Copyright (C) 2003,2004  Ricardo Markiewicz
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
#ifndef __NETLIST_H
#define __NETLIST_H

#include <glib.h>
#include "schematic.h"
#include "sim-settings.h"

typedef struct {
	gint node_nr; ///< Node number
	GHashTable *pins;
	GHashTable *models;
	GSList	   *gnd_list;   ///< Ground parts on the schematic
	GSList     *clamp_list; ///< Test clamps on the schematic
	GSList	   *mark_list;
	GList	     *node_and_number_list;
	NodeStore  *store;
} NetlistData;

typedef struct {
	gchar *cmd;
	gchar *title;
	GString *template;
	SimSettings *settings;
	NodeStore *store;
	GList *models;
} Netlist;

typedef struct {
	gint node_nr;
	gchar *name;
} Marker;

typedef struct {
	gint  node_nr;
	Node *node;
} NodeAndNumber;

void  netlist_helper_init_data (NetlistData *data);
void  netlist_helper_create (Schematic *sm, Netlist *out, GError **error);
char *netlist_helper_create_analisys_string (NodeStore *store, gboolean do_ac);

#endif
