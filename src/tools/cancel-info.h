/*
 * cancel-info.h
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

#ifndef TOOLS_CANCEL_INFO_H_
#define TOOLS_CANCEL_INFO_H_

typedef struct _CancelInfo CancelInfo;

CancelInfo *cancel_info_new();
void cancel_info_subscribe(CancelInfo *info);
void cancel_info_unsubscribe(CancelInfo *info);
gboolean cancel_info_is_cancel(CancelInfo *info);
void cancel_info_set_cancel(CancelInfo *info);

#endif /* TOOLS_CANCEL_INFO_H_ */
