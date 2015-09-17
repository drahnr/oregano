/*
 * textbox.h
 *
 *
 * Author:
 *  Richard Hult <rhult@hem.passagen.se>
 *  Ricardo Markiewicz <rmarkie@fi.uba.ar>
 *  Andres de Barbara <adebarbara@fi.uba.ar>
 *  Marc Lorber <lorber.marc@wanadoo.fr>
 *
 * Web page: https://ahoi.io/project/oregano
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
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef __TEXTBOX_H
#define __TEXTBOX_H

#include <gtk/gtk.h>

#include "clipboard.h"
#include "item-data.h"

#define TYPE_TEXTBOX (textbox_get_type ())
#define TEXTBOX(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), TYPE_TEXTBOX, Textbox))
#define TEXTBOX_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), TYPE_TEXTBOX, TextboxClass))
#define IS_TEXTBOX(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TYPE_TEXTBOX))
#define IS_TEXTBOX_CLASS(klass) (G_TYPE_CHECK_GET_CLASS ((klass), TYPE_TEXTBOX))

typedef struct _Textbox Textbox;
typedef struct _TextboxClass TextboxClass;
typedef struct _TextboxPriv TextboxPriv;

struct _Textbox
{
	ItemData parent;
	TextboxPriv *priv;
	gulong text_changed_handler_id;
	gulong font_changed_handler_id;
};

struct _TextboxClass
{
	ItemDataClass parent_class;
	Textbox *(*dup)(Textbox *textbox);
};

GType textbox_get_type (void);
Textbox *textbox_new (char *font);
void textbox_set_text (Textbox *textbox, const char *text);
char *textbox_get_text (Textbox *textbox);
void textbox_set_font (Textbox *textbox, char *font);
char *textbox_get_font (Textbox *textbox);
void textbox_update_bbox (Textbox *textbox);

#endif
