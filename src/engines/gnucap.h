/*
 * engine.c
 *
 * Authors:
 *  Ricardo Markiewicz <rmarkie@fi.uba.ar>
 *  Marc Lorber <lorber.marc@wanadoo.fr>
 *
 * Web page: https://ahoi.io/project/oregano
 *
 * Copyright (C) 1999-2001  Richard Hult
 * Copyright (C) 2003,2006  Ricardo Markiewicz
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

#ifndef __GNUCAP_ENGINE
#define __GNUCAP_ENGINE

#include <gtk/gtk.h>

#include "engine.h"

#define TYPE_GNUCAP (gnucap_get_type ())
#define GNUCAP(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), TYPE_GNUCAP, GnuCap))
#define GNUCAP_CLASS(vtable)                                                               \
	(G_TYPE_CHECK_CLASS_CAST ((vtable), TYPE_GNUCAP, GnuCapClass))
#define IS_GNUCAP(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TYPE_GNUCAP))
#define IS_GNUCAP_CLASS(vtable) (G_TYPE_CHECK_CLASS_TYPE ((vtable), TYPE_GNUCAP))
#define GNUCAP_GET_CLASS(inst)                                                             \
	(G_TYPE_INSTANCE_GET_CLASS ((inst), TYPE_GNUCAP, GnuCapClass))

typedef struct _GnuCap GnuCap;
typedef struct _GnuCapPrivate GnuCapPrivate;
typedef struct _GnuCapClass GnuCapClass;

struct _GnuCap
{
	GObject parent;

	GnuCapPrivate *priv;
};

struct _GnuCapClass
{
	GObjectClass parent;
};

GType gnucap_get_type (void);
Engine *gnucap_new (Schematic *sm);

#endif
