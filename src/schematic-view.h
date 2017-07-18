/*
 * schematic-view.h
 *
 *
 * Authors:
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
 * Copyright (C) 2009-2012  Marc Lorber
 * Copyright (C) 2017       Guido Trentalancia
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
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef __SCHEMATIC_VIEW_H
#define __SCHEMATIC_VIEW_H

// typedefing before including makes circular dependencies possible
typedef struct _SchematicView SchematicView;

#include <gtk/gtk.h>

#include "schematic.h"
#include "sheet.h"

/*
 * When stretching a schematic to resize
 * it, increase its width or height of
 * this percentage (the recommended factor
 * is 0.15 for a 15% increase).
 */
#define	SCHEMATIC_STRETCH_FACTOR	0.15

typedef enum { DRAG_URI_INFO, DRAG_PART_INFO } DragTypes;

#define TYPE_SCHEMATIC_VIEW (schematic_view_get_type ())
#define SCHEMATIC_VIEW(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), TYPE_SCHEMATIC_VIEW, SchematicView))
#define SCHEMATIC_VIEW_CLASS(klass)                                                                \
	(G_TYPE_CHECK_CLASS_CAST ((klass), TYPE_SCHEMATIC_VIEW, SchematicViewClass))
#define IS_SCHEMATIC_VIEW(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TYPE_SCHEMATIC_VIEW))
#define IS_SCHEMATIC_VIEW_CLASS(klass)                                                             \
	(G_TYPE_INSTANCE_GET_CLASS ((obj), TYPE_SCHEMATIC_VIEW, SchematicViewClass))

typedef struct _SchematicViewClass SchematicViewClass;
typedef struct _SchematicViewPriv SchematicViewPriv;

GType schematic_view_get_type (void);

void schematic_view_simulate_cmd (GtkWidget *widget, SchematicView *sv);

SchematicView *schematic_view_new (Schematic *schematic);
Sheet *schematic_view_get_sheet (SchematicView *sv);
void schematic_view_set_sheet (SchematicView *sv, Sheet *sheet);
Schematic *schematic_view_get_schematic (SchematicView *sv);
Schematic *schematic_view_get_schematic_from_sheet (Sheet *sheet);
SchematicView *schematic_view_get_schematicview_from_sheet (Sheet *sheet);
void run_context_menu (SchematicView *sv, GdkEventButton *event);

// Signal emission wrappers.
void schematic_view_reset_tool (SchematicView *sv);

// Misc.
void schematic_view_set_browser (SchematicView *sv, gpointer p);
gpointer schematic_view_get_browser (SchematicView *sv);
void schematic_view_set_parent (SchematicView *sv, GtkDialog *dialog);

// Logging.
void schematic_view_log_show (SchematicView *sv, gboolean explicit);
gboolean schematic_view_get_log_window_exists (SchematicView *sv);

// Windows services.
GtkWidget *schematic_view_get_toplevel (SchematicView *sv);

#endif /* __SCHEMATIC_VIEW_H */
