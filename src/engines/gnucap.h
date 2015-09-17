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

#define OREGANO_TYPE_GNUCAP (oregano_gnucap_get_type ())
#define OREGANO_GNUCAP(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), OREGANO_TYPE_GNUCAP, OreganoGnuCap))
#define OREGANO_GNUCAP_CLASS(vtable)                                                               \
	(G_TYPE_CHECK_CLASS_CAST ((vtable), OREGANO_TYPE_GNUCAP, OreganoGnuCapClass))
#define OREGANO_IS_GNUCAP(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), OREGANO_TYPE_GNUCAP))
#define OREGANO_IS_GNUCAP_CLASS(vtable) (G_TYPE_CHECK_CLASS_TYPE ((vtable), OREGANO_TYPE_GNUCAP))
#define OREGANO_GNUCAP_GET_CLASS(inst)                                                             \
	(G_TYPE_INSTANCE_GET_CLASS ((inst), OREGANO_TYPE_GNUCAP, OreganoGnuCapClass))

typedef struct _OreganoGnuCap OreganoGnuCap;
typedef struct _OreganoGnuCapPriv OreganoGnuCapPriv;
typedef struct _OreganoGnuCapClass OreganoGnuCapClass;

struct _OreganoGnuCap
{
	GObject parent;

	OreganoGnuCapPriv *priv;
};

struct _OreganoGnuCapClass
{
	GObjectClass parent;
};

GType oregano_gnucap_get_type (void);
OreganoEngine *oregano_gnucap_new (Schematic *sm);

#endif
