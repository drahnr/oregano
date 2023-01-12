/*
 * stack.h
 *
 *
 * Authors:
 *  Daniel Dwek <todovirtual15@gmail.com>
 *
 * Web page: https://ahoi.io/project/oregano
 *
 * Copyright (C) 2022-2023  Daniel Dwek
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
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef __STACK_H
#define __STACK_H	1

#include "goocanvas.h"
#include "item-data.h"
#include "wire-item.h"

typedef enum { PART_CREATED, PART_MOVED, PART_ROTATED, PART_DELETED,
		WIRE_CREATED, WIRE_MOVED, WIRE_ROTATED, WIRE_DELETED, } item_type_t;

typedef enum { SAME_GROUP, NEW_GROUP } group_assignment_t;

typedef struct stack_data_st {
	item_type_t type;
	gint group;
	SheetItem *s_item;
	union {
		struct {
			Coords coords;
			Coords delta;
		} moved;
		struct {
			Coords center;
			gint angle;
			Coords bbox1;
			Coords bbox2;
		} rotated;
		struct {
			Coords coords;
			GooCanvasGroup *canvas_group;
		} deleted;
	} u;
} stack_data_t;

typedef struct stack_item_st {
	stack_data_t *data;
	struct stack_item_st *prev;
} stack_item_t;

typedef struct stack_st {
	stack_item_t *top;
	unsigned long long size;
} historial_stack_t;

extern historial_stack_t *undo_stack;
extern historial_stack_t *redo_stack;

#include "sheet-item.h"
#include "load-library.h"

guint stack_get_size (historial_stack_t *stack);
stack_item_t *stack_get_top (historial_stack_t *stack);
SheetItem *stack_get_sheetitem_by_itemdata (ItemData *data);
Coords *stack_get_multiple_group (SheetItem *item, Coords *pos, gint *ret_group);
gint stack_get_group_for_rotation (SheetItem *item, Coords *center, gint angle, Coords *bbox1, Coords *bbox2);
gint stack_get_group (SheetItem *item, group_assignment_t hint);
gboolean stack_is_item_registered (stack_data_t *data);
gboolean stack_push (historial_stack_t *stack, stack_data_t *data, SchematicView *sv);
stack_data_t **stack_pop (historial_stack_t *stack, gint *ret_nitems);
#endif
