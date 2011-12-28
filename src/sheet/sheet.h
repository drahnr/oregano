/*
 * sheet.h
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

#include <libgnomecanvas/libgnomecanvas.h>
#include <gtk/gtkdialog.h>
#include "grid.h"

typedef struct _Sheet Sheet;
typedef struct _SheetPriv SheetPriv;
typedef struct _SheetClass SheetClass;

#define TYPE_SHEET			(sheet_get_type())
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
	GnomeCanvas		  parent_canvas;
	SheetState		  state;
	GnomeCanvasGroup *object_group;
	Grid			 *grid;
	SheetPriv		 *priv;
};

struct _SheetClass {
	GnomeCanvasClass parent_class;

	void (*selection_changed)	(Sheet *sheet);
	gint (*button_press)		(Sheet *sheet, GdkEventButton *event);
	void (*context_click)		(Sheet *sheet,
								 const char *name, gpointer data);
	void (*cancel)				(Sheet *sheet);
	void (*reset_tool)			(Sheet *sheet);
};

GType	   sheet_get_type (void);
GtkWidget *sheet_new (int width, int height);
void	   sheet_scroll (const Sheet *sheet, int dx, int dy);
void	   sheet_get_size_pixels (const Sheet *sheet, guint *width, guint *height);
int		   sheet_get_num_selected_items (const Sheet *sheet);
gpointer   sheet_get_first_selected_item (const Sheet *sheet);
GSList 	  *sheet_get_selected_items (const Sheet *sheet);
void	   sheet_change_zoom (const Sheet *sheet, double rate);
void	   sheet_get_zoom (const Sheet *sheet, gdouble *zoom);
void	   sheet_dialog_set_parent (const Sheet *sheet, GtkDialog *dialog);
void	   sheet_delete_selected_items (const Sheet *sheet);
void	   sheet_rotate_selected_items (const Sheet *sheet);
void	   sheet_rotate_floating_items (const Sheet *sheet);
void	   sheet_reset_floating_items (const Sheet *sheet);

#endif
