/*
 * print.h
 *
 *
 * Authors:
 *  Richard Hult <rhult@hem.passagen.se>
 *  Ricardo Markiewicz <rmarkie@fi.uba.ar>
 *  Andres de Barbara <adebarbara@fi.uba.ar>
 *
 * Web page: http://arrakis.lug.fi.uba.ar/
 *
 * Copyright (C) 1999-2001  Richard Hult
 * Copyright (C) 2003,2004  LUGFI
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
#ifndef __PRINT_H
#define __PRINT_H

#include <gnome.h>
#include <libgnomeprint/gnome-print.h>

typedef struct {
	GnomePrintContext *ctx;
	GnomeFontFace *label_font;
	GnomePrintConfig *config;
	gdouble scale;
	gdouble lmargin, tmargin, rmargin, bmargin;
	gdouble width, height;
} OreganoPrintContext;

void print_schematic (GObject *w, gboolean preview_only);
void print_draw_ellipse (GnomePrintContext *ctx, ArtPoint *center, gdouble width, gdouble height);
void print_draw_text (GnomePrintContext *ctx, const gchar *text, ArtPoint *pos);

#endif
