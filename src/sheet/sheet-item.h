/*
 * sheet-item.h
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
#ifndef __SHEET_ITEM_H
#define __SHEET_ITEM_H

#include <gtk/gtk.h>
#include <glib.h>
#include <libgnomecanvas/libgnomecanvas.h>
#include "sheet-pos.h"
#include "item-data.h"


#define TYPE_SHEET_ITEM			 (sheet_item_get_type())
#define SHEET_ITEM(obj)			 (G_TYPE_CHECK_INSTANCE_CAST(obj, sheet_item_get_type (), SheetItem))
#define SHEET_ITEM_CLASS(klass)	 (G_TYPE_CHECK_CLASS_CAST(klass, sheet_item_get_type (), SheetItemClass))
#define IS_SHEET_ITEM(obj)		 (G_TYPE_INSTANCE_GET_CLASS(obj, TYPE_SHEET_ITEM, SheetItemClass))

typedef struct _SheetItem SheetItem;
typedef struct _SheetItemClass SheetItemClass;
typedef struct _SheetItemPriv SheetItemPriv;

#include "schematic-view.h"
#include "sheet.h"
#include "clipboard.h"

struct _SheetItem {
	GnomeCanvasGroup canvas_group;
	SheetItemPriv *priv;
};

struct _SheetItemClass {
	GnomeCanvasGroupClass parent_class;

	/*
	 * Methods.
	 */
	gboolean (*is_in_area)		 (SheetItem *item, SheetPos *p1, SheetPos *p2);
	void	 (*show_labels)		 (SheetItem *sheet_item, gboolean show);
	void	 (*edit_properties)	 (SheetItem *item);
	void	 (*paste)			 (SchematicView *schematic_view,
								  ItemData *data);
	void	 (*place)			 (SheetItem *item, SchematicView *sv);
	void	 (*place_ghost)		 (SheetItem *item, SchematicView *sv);

	/*
	 * Signal handlers.
	 */
	void	 (*moved)			 (SheetItem *item);
	void	 (*selection_changed)(SheetItem *item);
	void	 (*mouse_over)		 (SheetItem *item);
};

GType sheet_item_get_type (void);

void sheet_item_select_all (SchematicView *sv,
							gboolean	   select);

gboolean sheet_item_select (SheetItem *item,
							 gboolean  select);

Sheet *sheet_item_get_sheet (SheetItem *item);

int sheet_item_event (SheetItem		 *sheet_item,
					  const GdkEvent *event,
					  SchematicView	 *sv);

int sheet_item_floating_event (Sheet		  *sheet,
							   const GdkEvent *event,
							   SchematicView  *schematic_view);

void sheet_item_reparent (SheetItem		   *item,
	GnomeCanvasGroup *group);

void sheet_item_cancel_floating (SchematicView *sv);

void sheet_item_edit_properties (SheetItem *item);

ItemData *sheet_item_get_data (SheetItem *item);

void sheet_item_paste (SchematicView *schematic_view,
	ClipboardData *data);

void sheet_item_rotate (SheetItem *sheet_item,
	int		   angle, SheetPos  *center);

gboolean sheet_item_get_selected (SheetItem *item);

gboolean sheet_item_get_preserve_selection (SheetItem *item);

void sheet_item_set_preserve_selection (SheetItem *item,
										gboolean   set);

void sheet_item_select_in_area (SheetItem *item,
								SheetPos  *p1,
								SheetPos  *p2);

void sheet_item_place (SheetItem *item, SchematicView *sv);
void sheet_item_place_ghost (SheetItem *item, SchematicView *sv);
void sheet_item_add_menu (SheetItem *item, const char *menu, 
    const GtkActionEntry *action_entries, int nb_entries); 

#endif
