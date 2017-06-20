/*
 * cancel-info.c
 *
 *
 * Authors:
 *  Michi <st101564@stud.uni-stuttgart.de>
 *
 * Web page: https://ahoi.io/project/oregano
 *
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

#include <glib.h>
#include "cancel-info.h"

struct _CancelInfo {
	guint ref_count;
	GMutex mutex;
	gboolean is_cancel;
};

static void cancel_info_finalize(CancelInfo *info);

CancelInfo *cancel_info_new() {
	CancelInfo *info =  g_new0(CancelInfo, 1);
	info->ref_count = 1;
	g_mutex_init(&info->mutex);

	return info;
}

void cancel_info_subscribe(CancelInfo *info) {
	g_mutex_lock(&info->mutex);
	info->ref_count++;
	g_mutex_unlock(&info->mutex);
}

void cancel_info_unsubscribe(CancelInfo *info) {
	g_mutex_lock(&info->mutex);
	info->ref_count--;
	guint ref_count = info->ref_count;
	g_mutex_unlock(&info->mutex);

	if (ref_count == 0)
		cancel_info_finalize(info);
}

gboolean cancel_info_is_cancel(CancelInfo *info) {
	g_mutex_lock(&info->mutex);
	gboolean is_cancel = info->is_cancel;
	g_mutex_unlock(&info->mutex);

	return is_cancel;
}

void cancel_info_set_cancel(CancelInfo *info) {
	g_mutex_lock(&info->mutex);
	info->is_cancel = TRUE;
	g_mutex_unlock(&info->mutex);
}

static void cancel_info_finalize(CancelInfo *info) {
	g_mutex_clear(&info->mutex);
	g_free(info);
}
