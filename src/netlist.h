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
 * Copyright (C) 2003,2004  LUGFI
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

#include <config.h>
#include <gnome.h>
#include "schematic.h"


typedef struct {
		gint	   node_nr;
		GHashTable *pins;
		GHashTable *models;
		GSList	   *gnd_list;
		GSList     *clamp_list;
		GSList	   *mark_list;
		GList	   *node_and_number_list;
		NodeStore  *store;
} NetlistData;

typedef struct {
		gint node_nr;
		gchar *name;
} Marker;

typedef struct {
		gint	   node_nr;
		Node	  *node;
} NodeAndNumber;

void node_foreach_reset (gpointer key, gpointer value, gpointer user_data);

void wire_traverse (Wire *wire, NetlistData *data);

void node_traverse (Node *node, NetlistData *data);

void node_foreach_traverse (gpointer key, gpointer value, NetlistData *data);

gint compare_marker (gconstpointer a, gconstpointer b);

void foreach_model_write (char *model, gpointer user_data, FILE *f);

void foreach_model_free (gpointer key, char *model, gpointer user_data);

char *linebreak (char *str);

#endif
