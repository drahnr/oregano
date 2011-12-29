/*
 * schematic-view.h
 *
 *
 * Authors:
 *  Richard Hult <rhult@hem.passagen.se>
 *  Ricardo Markiewicz <rmarkie@fi.uba.ar>
 *  Andres de Barbara <adebarbara@fi.uba.ar>
 *
 * Web page: http://arrakis.lug.fi.uba.ar/
 *
 * Copyright (C) 1999-2001  Richard Hult
 * Copyright (C) 2003,2004  Ricardo Markiewicz
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

struct _SchematicView
{
	GObject parent;

	GtkWidget *toplevel;

	gpointer corba_server;
	SchematicViewPriv *priv;
};

struct _SchematicViewClass
{
	GObjectClass parent_class;

	/* Signals go here */
	void (*changed)	(SchematicView *schematic_view);
};


GType		   schematic_view_get_type (void);
void       schematic_view_load (SchematicView *sv, Schematic *sm);
SchematicView *schematic_view_new (Schematic *schematic);
Sheet		  *schematic_view_get_sheet (SchematicView *sv);
Schematic	  *schematic_view_get_schematic (SchematicView *sv);

/*
 * Signal emission wrappers.
 */
void		   schematic_view_reset_tool (SchematicView *sv);
void		   schematic_view_cancel (SchematicView *sv);

/*
 * Misc.
 */
void schematic_view_set_browser (SchematicView *sv, gpointer p);
gpointer	   schematic_view_get_browser (SchematicView *sv);
void		   schematic_view_add_ghost_item (SchematicView *sv,
	ItemData *data);
void		   schematic_view_delete_selection (SchematicView *sv);
void		   schematic_view_rotate_selection (SchematicView *sv);
void		   schematic_view_rotate_ghosts (SchematicView *sv);
void		   schematic_view_flip_selection (SchematicView *sv,
	gboolean horizontal);
void		   schematic_view_flip_ghosts (SchematicView *sv,
	gboolean horizontal);
GList		  *schematic_view_get_selection (SchematicView *sv);
void		   schematic_view_clear_ghosts (SchematicView *sv);
void		   schematic_view_set_parent (SchematicView *sv, GtkDialog *dialog);
GList *		   schematic_view_get_items (SchematicView *sv);
void		   schematic_view_select_all (SchematicView *sv, gboolean select);
void		   schematic_view_update_parts (SchematicView *sv);
/*
 * Clipboard operations.
 */
void		   schematic_view_copy_selection (SchematicView *sv);
void		   schematic_view_cut_selection (SchematicView *sv);
void		   schematic_view_paste (SchematicView *sv);

/*
 * Logging.
 */
void		   schematic_view_log_show (SchematicView *sv, gboolean explicit);
gboolean	   schematic_view_log_window_exists (SchematicView *sv);

/*
 * Voltmeter.
 */
/*
void schematic_view_show_op_values (SchematicView *sv,
	SimEngine *engine);
*/

void		   schematic_view_clear_op_values (SchematicView *sv);

#endif /* __SCHEMATIC_VIEW_H */

