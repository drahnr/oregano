/*
 * sheet.h
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

#ifndef __SHEET_H
#define __SHEET_H

#include <goocanvas.h>
#include <gtk/gtk.h>

#include "grid.h"
#include "item-data.h"

typedef struct _Sheet      Sheet;
typedef struct _SheetPriv  SheetPriv;
typedef struct _SheetClass SheetClass;
typedef struct _SheetItem  SheetItem;

#define TYPE_SHEET			(sheet_get_type ())
#define SHEET(obj)			(G_TYPE_CHECK_INSTANCE_CAST (obj, TYPE_SHEET, Sheet))
#define SHEET_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST (klass, TYPE_SHEET, SheetClass))
#define IS_SHEET(obj)		(G_TYPE_CHECK_INSTANCE_TYPE (obj, TYPE_SHEET))

typedef enum {
	SHEET_STATE_NONE,
	SHEET_STATE_DRAG_START,
	SHEET_STATE_DRAG,
	SHEET_STATE_FLOAT_START,
	SHEET_STATE_FLOAT,
	SHEET_STATE_WIRE,
	SHEET_STATE_TEXTBOX_WAIT,
	SHEET_STATE_TEXTBOX_START,
	SHEET_STATE_TEXTBOX_CREATING
} SheetState;

struct _Sheet {
	GooCanvas		 parent_canvas;
	SheetState		 state;
	GooCanvasGroup	*object_group;
	Grid            *grid;
	SheetPriv		*priv;
};

struct _SheetClass {
	GooCanvasClass				parent_class;

	void (*selection_changed)	(Sheet *sheet);
	gint (*button_press)		(Sheet *sheet, GdkEventButton *event);
	void (*context_click)		(Sheet *sheet, const char *name, gpointer data);
	void (*cancel)				(Sheet *sheet);
};

GType	   	sheet_get_type (void);
GtkWidget *	sheet_new (int width, int height);
void	   	sheet_scroll (const Sheet *sheet, int dx, int dy);
void	   	sheet_get_size_pixels (const Sheet *sheet, guint *width, 
	              guint *height);
gpointer   	sheet_get_first_selected_item (const Sheet *sheet);
void	   	sheet_change_zoom (const Sheet *sheet, double rate);
void	   	sheet_get_zoom (const Sheet *sheet, gdouble *zoom);
void	   	sheet_delete_selected_items (const Sheet *sheet);
void	   	sheet_rotate_selected_items (const Sheet *sheet);
void	   	sheet_rotate_floating_items (const Sheet *sheet);
void	   	sheet_reset_floating_items (const Sheet *sheet);
void 	   	sheet_remove_selected_object (const Sheet *sheet, SheetItem *item);
void 	   	sheet_prepend_selected_object (Sheet *sheet, SheetItem *item);
void 	   	sheet_remove_floating_object (const Sheet *sheet, SheetItem *item);
void 	   	sheet_prepend_floating_object (Sheet *sheet, SheetItem *item);
void       	sheet_connect_part_item_to_floating_group (Sheet *sheet, 
                  gpointer *sv);
void       	sheet_show_node_labels (Sheet *sheet, gboolean show);
void		sheet_add_item (Sheet *sheet, SheetItem *item);
void 		sheet_stop_rubberband (Sheet *sheet, GdkEventButton *event);
void 		sheet_setup_rubberband (Sheet *sheet, GdkEventButton *event);
int			sheet_event_callback (GtkWidget *widget, GdkEvent *event, 
				Sheet *sheet);
void		sheet_select_all (Sheet *sheet, gboolean select);
void		sheet_rotate_selection (Sheet *sheet);
void		sheet_delete_selection (Sheet *sheet);
void		sheet_release_selected_objects (Sheet *sheet);
GList	*	sheet_get_selection (Sheet *sheet);
void		sheet_update_parts (Sheet *sheet);
void		sheet_destroy_sheet_item (SheetItem *item, Sheet *sheet);
void		sheet_rotate_ghosts (Sheet *sheet);
void		sheet_flip_selection (Sheet *sheet, gboolean horizontal);
void		sheet_flip_ghosts (Sheet *sheet, gboolean horizontal);
void		sheet_clear_op_values (Sheet *sheet);
void		sheet_provide_object_properties (Sheet *sheet);
void		sheet_clear_ghosts (Sheet *sheet);
guint		sheet_get_selected_objects_length (Sheet *sheet);
GList	*	sheet_get_floating_objects (Sheet *sheet);
void		sheet_add_ghost_item (Sheet *sheet, ItemData *data);
GList	*	sheet_get_items (const Sheet *sheet);
void		sheet_stop_create_wire (Sheet *sheet);
void		sheet_initiate_create_wire (Sheet *sheet);
void		sheet_connect_node_dots_to_signals (Sheet *sheet);
void		sheet_remove_item_in_sheet (SheetItem *item, Sheet *sheet);
void		sheet_get_pointer (Sheet *sheet, gdouble *x, gdouble *y); 

#endif
