/*
 * ngspice.h
 *
 * Authors:
 *  Ricardo Markiewicz <rmarkie@fi.uba.ar>
 *  Marc Lorber <lorber.marc@wanadoo.fr>
 *  Guido Trentalancia <guido@trentalancia.com>
 *
 * Web page: https://ahoi.io/project/oregano
 *
 * Copyright (C) 1999-2001  Richard Hult
 * Copyright (C) 2003,2006  Ricardo Markiewicz
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

#ifndef __NGSPICE_H
#define __NGSPICE_H

/*
 * The name of the vanilla spice3 executable.
 */
#define	SPICE_EXE	"spice3"

/*
 * The name of the ngspice executable.
 */
#define	NGSPICE_EXE	"ngspice"

/*
 * The filename used for the temporary noise
 * analysis file.
 */
#define	NOISE_ANALYSIS_FILENAME	"oregano-noise.txt"

#include <gtk/gtk.h>

#include "engine.h"

#define OREGANO_TYPE_NGSPICE (oregano_ngspice_get_type ())
#define OREGANO_NGSPICE(obj)                                                                       \
	(G_TYPE_CHECK_INSTANCE_CAST ((obj), OREGANO_TYPE_NGSPICE, OreganoNgSpice))
#define OREGANO_NGSPICE_CLASS(vtable)                                                              \
	(G_TYPE_CHECK_CLASS_CAST ((vtable), OREGANO_TYPE_NGSPICE, OreganoNgSpiceClass))
#define OREGANO_IS_NGSPICE(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), OREGANO_TYPE_NGSPICE))
#define OREGANO_IS_NGSPICE_CLASS(vtable) (G_TYPE_CHECK_CLASS_TYPE ((vtable), OREGANO_TYPE_NGSPICE))
#define OREGANO_NGSPICE_GET_CLASS(inst)                                                            \
	(G_TYPE_INSTANCE_GET_CLASS ((inst), OREGANO_TYPE_NGSPICE, OreganoNgSpiceClass))

typedef struct _OreganoNgSpice OreganoNgSpice;
typedef struct _OreganoNgSpicePriv OreganoNgSpicePriv;
typedef struct _OreganoNgSpiceClass OreganoNgSpiceClass;

struct _OreganoNgSpice
{
	GObject parent;

	OreganoNgSpicePriv *priv;
};

struct _OreganoNgSpiceClass
{
	GObjectClass parent;
};

GType oregano_ngspice_get_type (void);
OreganoEngine *oregano_spice_new (Schematic *sm, gboolean is_vanilla);

#endif
