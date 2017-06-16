/*
 * cancel-info.c
 *
 *  Created on: Jun 12, 2017
 *      Author: michi
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
