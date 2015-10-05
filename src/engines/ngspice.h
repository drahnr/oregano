/*
 * ngspice.h
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

#ifndef __NGSPICE_H
#define __NGSPICE_H

#include <gtk/gtk.h>

#include "engine.h"

#define TYPE_NGSPICE (ngspice_get_type ())
#define NGSPICE(obj)                                                                       \
	(G_TYPE_CHECK_INSTANCE_CAST ((obj), TYPE_NGSPICE, NgSpice))
#define NGSPICE_CLASS(vtable)                                                              \
	(G_TYPE_CHECK_CLASS_CAST ((vtable), TYPE_NGSPICE, NgSpiceClass))
#define IS_NGSPICE(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TYPE_NGSPICE))
#define IS_NGSPICE_CLASS(vtable) (G_TYPE_CHECK_CLASS_TYPE ((vtable), TYPE_NGSPICE))
#define NGSPICE_GET_CLASS(inst)                                                            \
	(G_TYPE_INSTANCE_GET_CLASS ((inst), TYPE_NGSPICE, NgSpiceClass))

typedef struct _NgSpice NgSpice;
typedef struct _NgSpicePrivate NgSpicePrivate;
typedef struct _NgSpiceClass NgSpiceClass;

struct _NgSpice
{
	GObject parent;

	NgSpicePrivate *priv;
};

struct _NgSpiceClass
{
	GObjectClass parent;
};

GType ngspice_get_type (void);
Engine *ngspice_new (Schematic *sm);

#endif
