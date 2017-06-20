/*
 * dialogs.h
 *
 *
 * Author:
 *  Richard Hult <rhult@hem.passagen.se>
 *  Ricardo Markiewicz <rmarkie@fi.uba.ar>
 *  Andres de Barbara <adebarbara@fi.uba.ar>
 *  Marc Lorber <lorber.marc@wanadoo.fr>
 *  Guido Trentalancia <guido@trentalancia.com>
 *
 * Web page: https://ahoi.io/project/oregano
 *
 * Copyright (C) 1999-2001  Richard Hult
 * Copyright (C) 2003,2004  Ricardo Markiewicz
 * Copyright (C) 2009-2012  Marc Lorber
 * Copyright (C) 2017       Guido Trentalancia
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
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef __DIALOGS_H
#define __DIALOGS_H

#include "schematic.h"

typedef struct _OreganoTitleMsg OreganoTitleMsg;
typedef struct _OreganoQuestionAnswer OreganoQuestionAnswer;

struct _OreganoTitleMsg
{
	gchar *title;
	gchar *msg;
};

struct _OreganoQuestionAnswer
{
	gchar *msg;
	gint ans;
};

gboolean oregano_schedule_error (gchar *msg);
gboolean oregano_schedule_error_with_title (OreganoTitleMsg *tm);
void oregano_error (gchar *msg);
void oregano_error_with_title (gchar *title, gchar *msg);
gboolean oregano_schedule_warning (gchar *msg);
gboolean oregano_schedule_warning_with_title (OreganoTitleMsg *tm);
void oregano_warning (gchar *msg);
void oregano_warning_with_title (gchar *title, gchar *msg);
gboolean oregano_schedule_question (OreganoQuestionAnswer *qa);
gint oregano_question (gchar *msg);
void dialog_about (void);

#endif
