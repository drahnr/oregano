/*
 * stack.c
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <gtk/gtk.h>
#include <schematic-view.h>
#include "debug.h"
#include "stack.h"

historial_stack_t *undo_stack = NULL;
historial_stack_t *redo_stack = NULL;

guint stack_get_size (historial_stack_t *stack)
{
	return stack->size;
}

stack_item_t *stack_get_top (historial_stack_t *stack)
{
	if (stack->size)
		return stack->top;
	else
		return NULL;
}

SheetItem *stack_get_sheetitem_by_itemdata (ItemData *data)
{
	stack_item_t *iter = NULL;

	if (!data)
		return NULL;

	for (iter = stack_get_top (undo_stack); iter; iter = iter->prev) {
		if (sheet_item_get_data (iter->data->s_item) == data)
			return iter->data->s_item;
	}

	return NULL;
}

Coords *stack_get_multiple_group (SheetItem *item, Coords *pos, gint *ret_group)
{
	stack_item_t *iter = NULL;
	Coords *delta = NULL;
	static Coords prev_delta = { .0 };

	g_return_val_if_fail (item != NULL, NULL);
	g_return_val_if_fail (ret_group != NULL, NULL);

	delta = g_new0 (Coords, 1);
	g_assert (delta != NULL);

	for (iter = stack_get_top (undo_stack); iter; iter = iter->prev) {
		if (iter->data->s_item != item)
			continue;

		delta->x = pos->x - iter->data->u.moved.coords.x;
		delta->y = pos->y - iter->data->u.moved.coords.y;

		if (fabs (delta->x - prev_delta.x) < 1e-2 && fabs (delta->y - prev_delta.y) < 1e-2) {
			*ret_group = stack_get_group (iter->data->s_item, SAME_GROUP);
		} else {
			*ret_group = stack_get_group (iter->data->s_item, NEW_GROUP);
			prev_delta.x = delta->x;
			prev_delta.y = delta->y;
		}

		return delta;
	}

	return NULL;
}

gint stack_get_group_for_rotation (SheetItem *item, Coords *center, gint angle, Coords *bbox1, Coords *bbox2)
{
	gint ret;
	stack_item_t *iter = NULL;
	static Coords gcenter = { .0 };

	if (!item || !center)
		return -1;

	for (iter = stack_get_top (undo_stack); iter; iter = iter->prev) {
		if (iter->data->type == PART_MOVED || iter->data->type == WIRE_MOVED) {
			gcenter.x = center->x;
			gcenter.y = center->y;
			ret = stack_get_group (item, NEW_GROUP);
			break;
		} else if (iter->data->type == PART_ROTATED || iter->data->type == WIRE_ROTATED) {
			if (fabs (gcenter.x - center->x) < 1e-2 && fabs (gcenter.y - center->y) < 1e-2) {
				ret = stack_get_group (item, SAME_GROUP);
				break;
			} else {
				gcenter.x = center->x;
				gcenter.y = center->y;
				ret = stack_get_group (item, NEW_GROUP);
				break;
			}
		}
	}

	return ret;
}

gint stack_get_group (SheetItem *item, group_assignment_t hint)
{
	stack_item_t *iter = NULL;
	static gint group = 0;

	g_return_val_if_fail (item != NULL, 0);

	for (iter = stack_get_top (undo_stack); iter; iter = iter->prev) {
		if (hint == SAME_GROUP)
			return group;
		else if (hint == NEW_GROUP)
			return ++group;
	}

	return ++group;
}

gboolean stack_is_item_registered (stack_data_t *ref)
{
	stack_item_t *iter = NULL;

	g_return_val_if_fail (ref != NULL, FALSE);

	for (iter = stack_get_top (undo_stack); iter; iter = iter->prev) {
		switch (ref->type) {
		case PART_CREATED:
		case WIRE_CREATED:
		case PART_MOVED:
		case WIRE_MOVED:
			if (fabs (iter->data->u.moved.coords.x - ref->u.moved.coords.x) < 1e-2 &&
			    fabs (iter->data->u.moved.coords.y - ref->u.moved.coords.y) < 1e-2)
				return TRUE;
			break;
		case PART_ROTATED:
		case WIRE_ROTATED:
			if (iter->data->s_item == ref->s_item && iter->data->u.rotated.angle == ref->u.rotated.angle &&
			    fabs (iter->data->u.rotated.center.x - ref->u.rotated.center.x) < 1e-2 &&
			    fabs (iter->data->u.rotated.center.y - ref->u.rotated.center.y) < 1e-2 &&
			    fabs (iter->data->u.rotated.bbox1.x - ref->u.rotated.bbox1.x) < 1e-2 &&
			    fabs (iter->data->u.rotated.bbox1.y - ref->u.rotated.bbox1.y) < 1e-2 &&
			    fabs (iter->data->u.rotated.bbox2.x - ref->u.rotated.bbox2.x) < 1e-2 &&
			    fabs (iter->data->u.rotated.bbox2.y - ref->u.rotated.bbox2.y) < 1e-2)
				return TRUE;
			break;
		case PART_DELETED:
		case WIRE_DELETED:
			if (fabs (iter->data->u.deleted.coords.x - ref->u.deleted.coords.x) < 1e-2 &&
			    fabs (iter->data->u.deleted.coords.y - ref->u.deleted.coords.y) < 1e-2)
				return TRUE;
			break;
		};
	}

	return FALSE;
}

static void stack_set_sensitive (historial_stack_t *stack, SchematicView *sv, gboolean activate)
{
	GtkUIManager *ui_mgr = NULL;
	const gchar *commands[] = { "/MainMenu/MenuEdit/Undo", "/MainMenu/MenuEdit/Redo" };

	if (!stack || !sv)
		return;

	ui_mgr = schematic_view_get_ui_manager (sv);
	gtk_action_set_sensitive (gtk_ui_manager_get_action (ui_mgr, (stack == undo_stack) ? commands[0] : commands[1]), activate);
}

static void stack_debug (historial_stack_t *stack)
{
	gint i;
	stack_item_t *iter = NULL;
	gchar *str[] = { "PART_CREATED", "PART_MOVED", "PART_ROTATED", "PART_DELETED",
			 "WIRE_CREATED", "WIRE_MOVED", "WIRE_ROTATED", "WIRE_DELETED" };

	if (stack == undo_stack)
		fprintf (stderr, "%sUNDO:%s\n", color_orange, color_restore);
	else
		fprintf (stderr, "%sREDO:%s\n", color_magenta, color_restore);

	i = stack_get_size (stack) - 1;
	for (iter = stack_get_top (stack); iter; iter = iter->prev, i--) {
		fprintf (stderr, "type = %s / group = %d / item = %s%p%s / ", str[iter->data->type],
			iter->data->group, COLOR_SHEETITEM, iter->data->s_item, COLOR_NORMAL);
		switch (iter->data->type) {
		case PART_CREATED:
			fprintf (stderr, "part created at (%s%f, %f%s)\n", COLOR_COORDS,
				iter->data->u.moved.coords.x, iter->data->u.moved.coords.y, COLOR_NORMAL);
			break;
		case PART_MOVED:
			fprintf (stderr, "part moved to (%s%f, %f%s) with delta = (%s%f, %f%s)\n", COLOR_COORDS,
				iter->data->u.moved.coords.x, iter->data->u.moved.coords.y, COLOR_NORMAL,
				COLOR_COORDS, iter->data->u.moved.delta.x, iter->data->u.moved.delta.y, COLOR_NORMAL);
			break;
		case PART_ROTATED:
			fprintf (stderr, "part rotated %d degrees around (%s%f, %f%s) and bounds (%f, %f) -> (%f, %f)\n",
				iter->data->u.rotated.angle, COLOR_COORDS,
				iter->data->u.rotated.center.x, iter->data->u.rotated.center.y, COLOR_NORMAL,
				iter->data->u.rotated.bbox1.x, iter->data->u.rotated.bbox1.y,
				iter->data->u.rotated.bbox2.x, iter->data->u.rotated.bbox2.y);
			break;
		case PART_DELETED:
			fprintf (stderr, "part deleted and relocated to (%s%f, %f%s)\n", COLOR_COORDS,
				iter->data->u.deleted.coords.x, iter->data->u.deleted.coords.y, COLOR_NORMAL);
			break;
		case WIRE_CREATED:
			fprintf (stderr, "wire created at (%s%f, %f%s)\n", COLOR_COORDS,
				iter->data->u.moved.coords.x, iter->data->u.moved.coords.y, COLOR_NORMAL);
			break;
		case WIRE_MOVED:
			fprintf (stderr, "wire moved to (%s%f, %f%s) with delta = (%s%f, %f%s)\n", COLOR_COORDS,
				iter->data->u.moved.coords.x, iter->data->u.moved.coords.y, COLOR_NORMAL,
				COLOR_COORDS, iter->data->u.moved.delta.x, iter->data->u.moved.delta.y, COLOR_NORMAL);
			break;
		case WIRE_ROTATED:
			fprintf (stderr, "wire rotated %d degrees around (%s%f, %f%s)\n",
				iter->data->u.rotated.angle, COLOR_COORDS,
				iter->data->u.rotated.center.x, iter->data->u.rotated.center.y, COLOR_NORMAL);
			break;
		case WIRE_DELETED:
			fprintf (stderr, "wire deleted and relocated to (%s%f, %f%s)\n", COLOR_COORDS,
				iter->data->u.deleted.coords.x, iter->data->u.deleted.coords.y, COLOR_NORMAL);
			break;
		};
	}
}

gboolean stack_push (historial_stack_t *stack, stack_data_t *data, SchematicView *sv)
{
	stack_item_t *top = NULL;

	if (stack && data && sv) {
		if (stack_is_item_registered (data))
			return FALSE;

		top = g_new0 (stack_item_t, 1);
		if (!top)
			return FALSE;

		if (!stack->size) {
			stack->top = top;
			stack->top->prev = NULL;
		} else {
			top->prev =stack->top;
			stack->top = top;
		}

		stack->top->data = g_new0 (stack_data_t, 1);
		if (!stack->top->data)
			return FALSE;
		stack->top->data = memcpy (stack->top->data, data, sizeof (stack_data_t));
		stack->size++;

		if (stack_get_size (undo_stack))
			stack_set_sensitive (undo_stack, sv, TRUE);
		else
			stack_set_sensitive (undo_stack, sv, FALSE);

		if (stack_get_size (redo_stack))
			stack_set_sensitive (redo_stack, sv, TRUE);
		else
			stack_set_sensitive (redo_stack, sv, FALSE);

		stack_debug (stack);

		return TRUE;
	} else {
		return FALSE;
	}
}

stack_data_t **stack_pop (historial_stack_t *stack, gint *ret_nitems)
{
	stack_data_t **ret = NULL;
	gint i = 0, prev_group = -1;
	stack_item_t *iter = NULL;

	for (iter = stack_get_top (stack); iter; iter = iter->prev) {
		if (!i)
			prev_group = iter->data->group;
		if (iter->data->group == prev_group)
			i++;
	}

	if (ret_nitems)
		*ret_nitems = i;

	ret = g_new0 (stack_data_t *, i);
	if (!ret)
		return NULL;

	for (; i > 0; i--) {
		ret[i - 1] = g_new0 (stack_data_t, 1);
		ret[i - 1] = memcpy (ret[i - 1], stack->top->data, sizeof (stack_data_t));
		g_free (stack->top->data);
		stack->top->data = NULL;
		stack->top = stack->top->prev;
		stack->size--;
	}

	return ret;
}

