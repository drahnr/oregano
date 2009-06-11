/*
 * schematic.h
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
 * Copyright (C) 2003,2004  Ricardo Markiewicz
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

#ifndef __SCHEMATIC_H__
#define __SCHEMATIC_H__

#include <gtk/gtk.h>
#include <cairo/cairo.h>
#include <cairo/cairo-features.h>
#ifdef CAIRO_HAS_SVG_SURFACE
#include <cairo/cairo-svg.h>
#endif
#ifdef CAIRO_HAS_PDF_SURFACE
#include <cairo/cairo-pdf.h>
#endif
#ifdef CAIRO_HAS_PS_SURFACE
#include <cairo/cairo-ps.h>
#endif

#include "item-data.h"
#include "part.h"
#include "wire.h"
#include "node-store.h"

#define TYPE_SCHEMATIC			  (schematic_get_type ())
#define SCHEMATIC(obj)			  (G_TYPE_CHECK_INSTANCE_CAST ((obj), TYPE_SCHEMATIC, Schematic))
#define SCHEMATIC_CLASS(klass)	  (G_TYPE_CHECK_CLASS_CAST ((klass), TYPE_SCHEMATIC, SchematicClass))
#define IS_SCHEMATIC(obj)		  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TYPE_SCHEMATIC))
#define IS_SCHEMATIC_CLASS(klass) (G_TYPE_INSTANCE_GET_CLASS ((obj), TYPE_SCHEMATIC, SchematicClass))

typedef struct _Schematic	   Schematic;
typedef struct _SchematicClass SchematicClass;
typedef struct _SchematicPriv  SchematicPriv;

typedef void (*ForeachItemDataFunc) (ItemData *item_data, gpointer user_data);

struct _Schematic {
	GObject parent;

	gpointer corba_server;
	SchematicPriv *priv;
};

struct _SchematicClass {
	GObjectClass parent_class;

	/* signals */
	void (*title_changed) (Schematic* ,gchar*);
	void (*item_data_added) (Schematic*, gpointer*);
	void (*log_updated) (gpointer);
	void (*dot_added) (Schematic*);
	void (*dot_removed) (Schematic*, gpointer*);
	void (*last_schematic_destroyed) (Schematic*);
};

GType	   schematic_get_type (void);
Schematic *schematic_new (void);
char	  *schematic_get_title (Schematic *schematic);
void	   schematic_set_title (Schematic *schematic, const gchar *title);
char	  *schematic_get_author (Schematic *schematic);
void	   schematic_set_author (Schematic *schematic, const gchar *author);
char	  *schematic_get_comments (Schematic *schematic);
void	   schematic_set_comments (Schematic *schematic, const gchar *comments);
char	  *schematic_get_filename (Schematic *schematic);
void	   schematic_set_filename (Schematic *schematic, const gchar *filename);
char	  *schematic_get_netlist_filename (Schematic *schematic);
void	   schematic_set_netlist_filename (Schematic *schematic, char *filename);
int		   schematic_count (void);
double	   schematic_get_zoom (Schematic *schematic);
void	   schematic_set_zoom (Schematic *schematic, double zoom);
void	   schematic_add_item (Schematic *sm, ItemData *data);
void	   schematic_parts_foreach (Schematic *schematic,
	ForeachItemDataFunc func, gpointer user_data);
void	   schematic_wires_foreach (Schematic *schematic,
	ForeachItemDataFunc func, gpointer user_data);
void	   schematic_items_foreach (Schematic *schematic,
	ForeachItemDataFunc func, gpointer user_data);
GList	  *schematic_get_items (Schematic *sm);
NodeStore *schematic_get_store (Schematic *schematic);
gpointer   schematic_get_settings (Schematic *schematic);
gpointer   schematic_get_sim_settings (Schematic *schematic);
gpointer   schematic_get_simulation (Schematic *schematic);
void	   schematic_log_clear (Schematic *schematic);
void	   schematic_log_append (Schematic *schematic, const char *message);
void	   schematic_log_append_error (Schematic *schematic, const char *message);
void	   schematic_log_show (Schematic *schematic);
GtkTextBuffer *schematic_get_log_text (Schematic *schematic);
int	   schematic_count (void);
gboolean schematic_is_dirty(Schematic *sm);	
void     schematic_set_dirty(Schematic *sm, gboolean b);
gint schematic_save_file (Schematic *sm, GError **error);
Schematic *schematic_read (char *fname, GError **error);
void       schematic_print (Schematic *sm, GtkPageSetup *p, GtkPrintSettings *s, gboolean preview);
void schematic_export (Schematic *sm, const gchar *filename, gint img_w, gint img_h, int bg, int color, int format);

#endif /* __SCHEMATIC_H__ */



