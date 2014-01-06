/*
 * sheet-item.h
 *
 *
 * Authors:
 *  Richard Hult <rhult@hem.passagen.se>
 *  Ricardo Markiewicz <rmarkie@fi.uba.ar>
 *  Andres de Barbara <adebarbara@fi.uba.ar>
 *  Marc Lorber <lorber.marc@wanadoo.fr>
 *
 * Web page: https://srctwig.com/oregano
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
#ifndef __SHEET_ITEM_H
#define __SHEET_ITEM_H

#include <gtk/gtk.h>
#include <glib.h>
#include <goocanvas.h>

#include "sheet.h"
#include "coords.h"

#define TYPE_SHEET_ITEM			 (sheet_item_get_type())
#define SHEET_ITEM(obj)			 (G_TYPE_CHECK_INSTANCE_CAST(obj, sheet_item_get_type (), SheetItem))
#define SHEET_ITEM_CLASS(klass)	 (G_TYPE_CHECK_CLASS_CAST(klass, sheet_item_get_type (), SheetItemClass))
#define IS_SHEET_ITEM(obj)		 (G_TYPE_CHECK_INSTANCE_TYPE(obj, TYPE_SHEET_ITEM))

typedef struct _SheetItemClass SheetItemClass;
typedef struct _SheetItemPriv SheetItemPriv;

#include "sheet.h"
#include "clipboard.h"

struct _SheetItem {
	GooCanvasGroup	 canvas_group;
	gdouble		 width;
	gdouble		 height;
	gdouble 	 x; //left
	gdouble		 y; //top
	SheetItemPriv	*priv;
};

struct _SheetItemClass {
	GooCanvasGroupClass parent_class;

	// Methods.
	gboolean (*is_in_area)		 (SheetItem *item, Coords *p1, Coords *p2);
	void	 (*show_labels)		 (SheetItem *sheet_item, gboolean show);
	void	 (*edit_properties)	 (SheetItem *item);
	void	 (*paste)		 (Sheet *sheet, ItemData *data);
	void	 (*place)		 (SheetItem *item, Sheet *sheet);
	void	 (*place_ghost)		 (SheetItem *item, Sheet *sheet);

	// Signal handlers.
	void	 (*moved)		 (SheetItem *item);
	void	 (*selection_changed)	 (SheetItem *item);
	void	 (*mouse_over)		 (SheetItem *item);
};

GType		sheet_item_get_type (void);
void 		sheet_item_select_all (Sheet *sheet, gboolean select);
gboolean	sheet_item_select (SheetItem *item, gboolean  select);
Sheet	*	sheet_item_get_sheet (SheetItem *item);
gboolean    	sheet_item_event (GooCanvasItem *sheet_item, 
                                  GooCanvasItem *sheet_target_item, GdkEvent *event, 
                                  Sheet *sheet);
int 		sheet_item_floating_event (Sheet *sheet, const GdkEvent *event);
void 		sheet_item_cancel_floating (Sheet *sheet);
void 		sheet_item_edit_properties (SheetItem *item);
ItemData *	sheet_item_get_data (SheetItem *item);
void 		sheet_item_paste (Sheet *sheet, ClipboardData *data);
void 		sheet_item_rotate (SheetItem *sheet_item, int angle, Coords *center);
gboolean 	sheet_item_get_selected (SheetItem *item);
gboolean 	sheet_item_get_preserve_selection (SheetItem *item);
void 		sheet_item_set_preserve_selection (SheetItem *item, gboolean set);
void 		sheet_item_select_in_area (SheetItem *item, Coords *p1, Coords *p2);
void 		sheet_item_place (SheetItem *item, Sheet *sheet);
void 		sheet_item_place_ghost (SheetItem *item, Sheet *sheet);
void 		sheet_item_add_menu (SheetItem *item, const char *menu, 
    		                     const GtkActionEntry *action_entries, int nb_entries);

#endif
