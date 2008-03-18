/*
 * ngspice.c
 *
 * Authors:
 *  Ricardo Markiewicz <rmarkie@fi.uba.ar>
 *
 * Web page: http://oregano.lug.fi.uba.ar/
 *
 * Copyright (C) 1999-2001  Richard Hult
 * Copyright (C) 2003,2006  Ricardo Markiewicz
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
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __NGSPICE_ENGINE
#define __NGSPICE_ENGINE

#include <gtk/gtk.h>
#include "engine.h"

#define OREGANO_TYPE_NGSPICE             (oregano_ngspice_get_type ())
#define OREGANO_NGSPICE(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), OREGANO_TYPE_NGSPICE, OreganoNgSpice))
#define OREGANO_NGSPICE_CLASS(vtable)    (G_TYPE_CHECK_CLASS_CAST ((vtable), OREGANO_TYPE_NGSPICE, OreganoNgSpiceClass))
#define OREGANO_IS_NGSPICE(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), OREGANO_TYPE_NGSPICE))
#define OREGANO_IS_NGSPICE_CLASS(vtable) (G_TYPE_CHECK_CLASS_TYPE ((vtable), OREGANO_TYPE_NGSPICE))
#define OREGANO_NGSPICE_GET_CLASS(inst)  (G_TYPE_INSTANCE_GET_CLASS ((inst), OREGANO_TYPE_NGSPICE, OreganoNgSpiceClass))

typedef struct _OreganoNgSpice OreganoNgSpice;
typedef struct _OreganoNgSpicePriv OreganoNgSpicePriv;
typedef struct _OreganoNgSpiceClass OreganoNgSpiceClass;

struct _OreganoNgSpice {
	GObject parent;

	OreganoNgSpicePriv *priv;
};

struct _OreganoNgSpiceClass {
	GObjectClass parent;
};

GType          oregano_ngspice_get_type (void);
OreganoEngine *oregano_ngspice_new (Schematic *sm);

#endif

