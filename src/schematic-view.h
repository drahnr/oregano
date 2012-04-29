/*
 * schematic-view.h
 *
 *
 * Authors:
 *  Richard Hult <rhult@hem.passagen.se>
 *  Ricardo Markiewicz <rmarkie@fi.uba.ar>
 *  Andres de Barbara <adebarbara@fi.uba.ar>
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

#ifndef __SCHEMATIC_VIEW_H
#define __SCHEMATIC_VIEW_H

#include <gtk/gtk.h>

#include "schematic.h"
#include "sheet.h"

typedef enum {
	DRAG_URI_INFO,
	DRAG_PART_INFO
} DragTypes;

#define TYPE_SCHEMATIC_VIEW			   (schematic_view_get_type ())
#define SCHEMATIC_VIEW(obj)			   (G_TYPE_CHECK_INSTANCE_CAST ((obj), TYPE_SCHEMATIC_VIEW, SchematicView))
#define SCHEMATIC_VIEW_CLASS(klass)	   (G_TYPE_CHECK_CLASS_CAST ((klass), TYPE_SCHEMATIC_VIEW, SchematicViewClass))
#define IS_SCHEMATIC_VIEW(obj)		   (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TYPE_SCHEMATIC_VIEW))
#define IS_SCHEMATIC_VIEW_CLASS(klass) (G_TYPE_INSTANCE_GET_CLASS ((obj), TYPE_SCHEMATIC_VIEW, SchematicViewClass))

typedef struct _SchematicView	   SchematicView;
typedef struct _SchematicViewClass SchematicViewClass;
typedef struct _SchematicViewPriv  SchematicViewPriv;

GType		   	schematic_view_get_type (void);
SchematicView  *schematic_view_new (Schematic *schematic);
Sheet		   *schematic_view_get_sheet (SchematicView *sv);
Schematic	   *schematic_view_get_schematic (SchematicView *sv);
Schematic	   *schematic_view_get_schematic_from_sheet (Sheet *sheet);
SchematicView  *schematic_view_get_schematicview_from_sheet (Sheet *sheet);
void			run_context_menu (SchematicView *sv, GdkEventButton *event);

// Signal emission wrappers.
void		   	schematic_view_reset_tool (SchematicView *sv);

// Misc.
void 			schematic_view_set_browser (SchematicView *sv, gpointer p);
gpointer	   	schematic_view_get_browser (SchematicView *sv);
void		   	schematic_view_set_parent (SchematicView *sv, GtkDialog *dialog);

// Logging.
void		   	schematic_view_log_show (SchematicView *sv, gboolean explicit);
gboolean	   	schematic_view_get_log_window_exists (SchematicView *sv);

// Windows services.
GtkWidget	*	schematic_view_get_toplevel (SchematicView *sv);

#endif /* __SCHEMATIC_VIEW_H */
