/*
 * netlist-editor.h
 *
 *
 * Author:
 *  Andres de Barbara <adebarbara@fi.uba.ar>
 *
 * Web page: http://arrakis.lug.fi.uba.ar/
 *
 * Copyright (C) 2004-2008 Ricardo Markiewicz
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

#ifndef __NETLIST_EDIT_H
#define __NETLIST_EDIT_H

#include <gconf/gconf-client.h>
#include <gtk/gtk.h>
#include <gtksourceview/gtksourceview.h>
#include "schematic-view.h"
#include "errors.h"
#include "engine.h"

#define TYPE_NETLIST_EDITOR				(netlist_editor_get_type ())
#define NETLIST_EDITOR(obj)			(G_TYPE_CHECK_INSTANCE_CAST ((obj), TYPE_NETLIST_EDITOR, NetlistEditor))
#define NETLIST_EDITOR_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST ((klass), TYPE_NETLIST_EDITOR, NetlistEditorClass))
#define IS_NETLIST_EDITOR(obj)			(G_TYPE_CHECK_INSTANCE_TYPE ((obj), TYPE_NETLIST_EDITOR))
#define IS_NETLIST_EDITOR_CLASS(klass) 		(G_TYPE_INSTANCE_GET_CLASS ((obj), TYPE_NETLIST_EDITOR, NetlistEditorClass))


typedef struct _NetlistEditor	   NetlistEditor;
typedef struct _NetlistEditorClass NetlistEditorClass;
typedef struct _NetlistEditorPriv  NetlistEditorPriv;

struct _NetlistEditor {
	GObject parent;

	NetlistEditorPriv *priv;
};

struct _NetlistEditorClass {
	GObjectClass parent_class;

	/* Signals go here */
};

GType netlist_editor_get_type (void);
NetlistEditor *netlist_editor_new_from_file (gchar * filename); 
NetlistEditor *netlist_editor_new_from_schematic_view (SchematicView *sv);
NetlistEditor *netlist_editor_new (GtkSourceBuffer * textbuffer);

#endif
