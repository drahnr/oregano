/*
 * cancel-info.h
 *
 *  Created on: Jun 12, 2017
 *      Author: michi
 */

#ifndef TOOLS_CANCEL_INFO_H_
#define TOOLS_CANCEL_INFO_H_

typedef struct _CancelInfo CancelInfo;

CancelInfo *cancel_info_new();
void cancel_info_subscribe(CancelInfo *info);
void cancel_info_unsubscribe(CancelInfo *info);
gboolean cancel_info_is_cancel(CancelInfo *info);
void cancel_info_set_cancel(CancelInfo *info);

#endif /* TOOLS_CANCEL_INFO_H_ */
