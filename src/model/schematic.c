/*
 * schematic.c
 *
 *
 * Authors:
 *  Richard Hult <rhult@hem.passagen.se>
 *  Ricardo Markiewicz <rmarkie@fi.uba.ar>
 *  Andres de Barbara <adebarbara@fi.uba.ar>
 *  Marc Lorber <lorber.marc@wanadoo.fr>
 *
 * Web page: https://github.com/marc-lorber/oregano
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
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <gtk/gtk.h>
#include <string.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <math.h>

#include "schematic.h"
#include "node-store.h"
#include "file-manager.h"
#include "settings.h"
#include "sim-settings.h"
#include "simulation.h"
#include "errors.h"
#include "schematic-print-context.h"

#define NG_DEBUG(s) if (0) g_print ("%s\n", s)
typedef struct _SchematicsPrintOptions {
	GtkColorButton *components;
	GtkColorButton *labels;
	GtkColorButton *wires;
	GtkColorButton *text;
	GtkColorButton *background;
} SchematicPrintOptions;

struct _SchematicPriv {
	char 					*title;
	char 					*filename;
	char 					*author;
	char 					*comments;
	char 					*netlist_filename;

	SchematicColors 		 colors;
	SchematicPrintOptions 	*printoptions;

	// Data for various dialogs.
	gpointer 				 settings;
	gpointer 				 sim_settings;
	gpointer 				 simulation;

	GList 					*current_items;

	NodeStore  				*store;
	GHashTable 				*symbols;
	GHashTable 				*refdes_values;

	double 					 zoom;

	gboolean 				 dirty;

	GtkTextBuffer 			*log;
	GtkTextTag 				*tag_error;
};

typedef enum {
	REFDATA_SINGLE,
	REFDATA_RANGE,
	REFDATA_MIN,
	REFDATA_MAX
} RefDataType;

typedef struct {
	RefDataType type;
	union {
		int nr;
		struct { int min; int max; } range;
	} u;
} RefData;

enum {
	TITLE_CHANGED,
	ITEM_DATA_ADDED,
	LOG_UPDATED,
	NODE_DOT_ADDED,
	NODE_DOT_REMOVED,
	LAST_SCHEMATIC_DESTROYED,
	LAST_SIGNAL
};

G_DEFINE_TYPE (Schematic, schematic, G_TYPE_OBJECT)

static void schematic_init (Schematic *schematic);
static void schematic_class_init (SchematicClass	*klass);
static void schematic_finalize (GObject *object);
static void schematic_dispose (GObject *object);
static void item_data_destroy_callback (gpointer s, GObject *data);
static void item_moved_callback (ItemData *data, SheetPos *pos, Schematic *sm);

static int  schematic_get_lowest_available_refdes (Schematic *schematic,
				char *prefix);
static void schematic_set_lowest_available_refdes (Schematic *schematic,
				char *prefix, int num);

static GObjectClass *parent_class = NULL;
static guint schematic_signals[LAST_SIGNAL] = { 0 };

static GList *schematic_list = NULL;
static int schematic_count_ = 0;

static void
schematic_class_init (SchematicClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);
	parent_class = g_type_class_peek_parent (klass);

	schematic_signals[TITLE_CHANGED] = g_signal_new ("title_changed",
		TYPE_SCHEMATIC,
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET (SchematicClass, title_changed),
		NULL, NULL,
		g_cclosure_marshal_VOID__STRING,
		G_TYPE_NONE, 1,
		G_TYPE_STRING );

	schematic_signals[LAST_SCHEMATIC_DESTROYED] = g_signal_new ("last_schematic_destroyed",
		TYPE_SCHEMATIC,
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET (SchematicClass, last_schematic_destroyed),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	schematic_signals[ITEM_DATA_ADDED] = g_signal_new ("item_data_added",
		TYPE_SCHEMATIC,
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET (SchematicClass, item_data_added),
	  	NULL, NULL,
	  	g_cclosure_marshal_VOID__POINTER,
	  	G_TYPE_NONE, 1, G_TYPE_POINTER);

	schematic_signals[NODE_DOT_ADDED] = g_signal_new ("node_dot_added",
		TYPE_SCHEMATIC,
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET (SchematicClass, node_dot_added),
		NULL, NULL,
		g_cclosure_marshal_VOID__POINTER,
		G_TYPE_NONE, 1,
		G_TYPE_POINTER);

	schematic_signals[NODE_DOT_REMOVED] = g_signal_new ("node_dot_removed",
		TYPE_SCHEMATIC,
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET (SchematicClass, node_dot_removed),
		NULL, NULL, g_cclosure_marshal_VOID__POINTER,
		G_TYPE_NONE, 1, G_TYPE_POINTER);

	schematic_signals[LOG_UPDATED] = g_signal_new ("log_updated",
	  TYPE_SCHEMATIC,
	  G_SIGNAL_RUN_FIRST,
	  G_STRUCT_OFFSET (SchematicClass, log_updated),
	  NULL, NULL, g_cclosure_marshal_VOID__VOID,
	  G_TYPE_NONE,0);

	object_class->finalize = schematic_finalize;
	object_class->dispose = schematic_dispose;
}

static void
node_dot_added_callback (NodeStore *store, SheetPos *pos, Schematic *schematic)
{
	g_return_if_fail (schematic != NULL);
	g_return_if_fail (IS_SCHEMATIC (schematic));

	g_signal_emit_by_name (schematic, "node_dot_added", pos);
}

static void
node_dot_removed_callback (NodeStore *store, SheetPos *pos, Schematic *schematic)
{
	g_return_if_fail (schematic != NULL);
	g_return_if_fail (IS_SCHEMATIC (schematic));

	g_signal_emit_by_name (schematic, "node_dot_removed", pos);
}

static void
schematic_init (Schematic *schematic)
{
	SchematicPriv *priv;

	priv = schematic->priv = g_new0 (SchematicPriv, 1);

	priv->printoptions = NULL;
	// Colors
	priv->colors.components.red = 65535;
	priv->colors.components.green = 0;
	priv->colors.components.blue = 0;
	priv->colors.labels.red = 0;
	priv->colors.labels.green = 35723;
	priv->colors.labels.blue = 35723;
	priv->colors.wires.red = 0;
	priv->colors.wires.green = 0;
	priv->colors.wires.blue = 65535;
	priv->colors.text.red = 0;
	priv->colors.text.green = 0;
	priv->colors.text.blue = 0;

	priv->symbols = g_hash_table_new (g_str_hash, g_str_equal);
	priv->refdes_values = g_hash_table_new (g_str_hash, g_str_equal);
	priv->store = node_store_new ();
	priv->dirty = FALSE;
	priv->log = gtk_text_buffer_new (NULL);
	priv->tag_error = gtk_text_buffer_create_tag (priv->log, "error",
		"foreground", "red", 
	    "weight", PANGO_WEIGHT_BOLD, 
	    NULL);

	g_signal_connect_object (priv->store, "node_dot_added",
		G_CALLBACK (node_dot_added_callback), G_OBJECT (schematic), 
	    G_CONNECT_AFTER);

	g_signal_connect_object (priv->store, "node_dot_removed",
		G_CALLBACK (node_dot_removed_callback), G_OBJECT (schematic),
		G_CONNECT_AFTER);

	priv->sim_settings = sim_settings_new (schematic);
	priv->settings = settings_new (schematic);
	priv->simulation = simulation_new (schematic);

	priv->filename = NULL;
	priv->netlist_filename = NULL;
	priv->author = g_strdup ("");
	priv->comments = g_strdup ("");
}

Schematic *
schematic_new (void)
{
	Schematic *schematic;

	schematic = SCHEMATIC (g_object_new (TYPE_SCHEMATIC, NULL));

	schematic_count_++;
	schematic_list = g_list_prepend (schematic_list, schematic);

	return schematic;
}

static void
schematic_dispose (GObject *object)
{
	Schematic *schematic;
	GList *list;

	schematic = SCHEMATIC (object);

	// Disconnect weak item signal
	for (list=schematic->priv->current_items; list; list=list->next)
		g_object_weak_unref (G_OBJECT (list->data),
			item_data_destroy_callback, G_OBJECT (schematic));

	g_object_unref (G_OBJECT (schematic->priv->log));

	schematic_count_--;
	schematic_list = g_list_remove (schematic_list, schematic);

	if (schematic_count_ == 0) {
		g_signal_emit_by_name (schematic, "last_schematic_destroyed", NULL);
	}

	g_list_free_full (list, g_object_unref);

	G_OBJECT_CLASS (parent_class)->dispose (G_OBJECT (schematic));
}

static void
schematic_finalize (GObject *object)
{
	Schematic *sm;

	sm = SCHEMATIC (object);
	if (sm->priv) {
		g_free (sm->priv->simulation);
		g_hash_table_destroy (sm->priv->symbols);
		g_hash_table_destroy (sm->priv->refdes_values);
		if (sm->priv->netlist_filename)
			g_free (sm->priv->netlist_filename);

		g_free (sm->priv);
		sm->priv = NULL;
	}

	G_OBJECT_CLASS (parent_class)->finalize (G_OBJECT (sm));
}

// Get/set functions.
// ***************** /

char *
schematic_get_title (Schematic *schematic)
{
	g_return_val_if_fail (schematic != NULL, NULL);
	g_return_val_if_fail (IS_SCHEMATIC (schematic), NULL);

	return schematic->priv->title;
}

char *
schematic_get_author (Schematic *schematic)
{
	g_return_val_if_fail (schematic != NULL, NULL);
	g_return_val_if_fail (IS_SCHEMATIC (schematic), NULL);

	return schematic->priv->author;
}

char *
schematic_get_comments (Schematic *schematic)
{
	g_return_val_if_fail (schematic != NULL, NULL);
	g_return_val_if_fail (IS_SCHEMATIC (schematic), NULL);

	return schematic->priv->comments;
}

void
schematic_set_title (Schematic *schematic, const gchar *title)
{
	g_return_if_fail (schematic != NULL);
	g_return_if_fail (IS_SCHEMATIC (schematic));

	if (!title) return;

	if (schematic->priv->title)
		g_free (schematic->priv->title);
	schematic->priv->title = g_strdup (title);

	g_signal_emit_by_name (schematic, "title_changed", schematic->priv->title);
}

void
schematic_set_author (Schematic *schematic, const gchar *author)
{
	g_return_if_fail (schematic != NULL);
	g_return_if_fail (IS_SCHEMATIC (schematic));

	if (!author) return;

	if (schematic->priv->author)
		g_free (schematic->priv->author);
	schematic->priv->author = g_strdup (author);
}

void
schematic_set_comments (Schematic *schematic, const gchar *comments)
{
	g_return_if_fail (schematic != NULL);
	g_return_if_fail (IS_SCHEMATIC (schematic));

	if (schematic->priv->comments)
		g_free (schematic->priv->comments);
	schematic->priv->comments = g_strdup (comments);
}

char *
schematic_get_filename (Schematic *schematic)
{
	g_return_val_if_fail (schematic != NULL, NULL);
	g_return_val_if_fail (IS_SCHEMATIC (schematic), NULL);

	return schematic->priv->filename;
}

void
schematic_set_filename (Schematic *schematic, const gchar *filename)
{
	g_return_if_fail (schematic != NULL);
	g_return_if_fail (IS_SCHEMATIC (schematic));

	g_free (schematic->priv->filename);
	schematic->priv->filename = g_strdup (filename);
}

char *
schematic_get_netlist_filename (Schematic *schematic)
{
	g_return_val_if_fail (schematic != NULL, NULL);
	g_return_val_if_fail (IS_SCHEMATIC (schematic), NULL);

	return schematic->priv->netlist_filename;
}

void
schematic_set_netlist_filename (Schematic *schematic, char *filename)
{
	g_return_if_fail (schematic != NULL);
	g_return_if_fail (IS_SCHEMATIC (schematic));

	if (schematic->priv->netlist_filename)
		g_free (schematic->priv->netlist_filename);

	schematic->priv->netlist_filename = g_strdup (filename);
}

double
schematic_get_zoom (Schematic *schematic)
{
	g_return_val_if_fail (schematic != NULL, 1.0);
	g_return_val_if_fail (IS_SCHEMATIC (schematic), 1.0);

	return schematic->priv->zoom;
}

void
schematic_set_zoom (Schematic *schematic, double zoom)
{
	g_return_if_fail (schematic != NULL);
	g_return_if_fail (IS_SCHEMATIC (schematic));

	schematic->priv->zoom = zoom;
}

NodeStore *
schematic_get_store (Schematic *schematic)
{
	g_return_val_if_fail (schematic != NULL, NULL);
	g_return_val_if_fail (IS_SCHEMATIC (schematic), NULL);

	return schematic->priv->store;
}

gpointer
schematic_get_settings (Schematic *schematic)
{
	g_return_val_if_fail (schematic != NULL, NULL);
	g_return_val_if_fail (IS_SCHEMATIC (schematic), NULL);

	return schematic->priv->settings;
}

gpointer
schematic_get_sim_settings (Schematic *schematic)
{
	g_return_val_if_fail (schematic != NULL, NULL);
	g_return_val_if_fail (IS_SCHEMATIC (schematic), NULL);

	return schematic->priv->sim_settings;
}

gpointer
schematic_get_simulation (Schematic *schematic)
{
	g_return_val_if_fail (schematic != NULL, NULL);
	g_return_val_if_fail (IS_SCHEMATIC (schematic), NULL);

	return schematic->priv->simulation;
}

void
schematic_log_append (Schematic *schematic, const char *message)
{
	g_return_if_fail (schematic != NULL);
	g_return_if_fail (IS_SCHEMATIC (schematic));

  gtk_text_buffer_insert_at_cursor (
		schematic->priv->log,
		message, strlen (message));
}

void
schematic_log_append_error (Schematic *schematic, const char *message)
{
	GtkTextIter iter;
	SchematicPriv *priv;

	g_return_if_fail (schematic != NULL);
	g_return_if_fail (IS_SCHEMATIC (schematic));

	priv = schematic->priv;

	gtk_text_buffer_get_end_iter (priv->log, &iter);
	gtk_text_buffer_insert_with_tags (priv->log, &iter, message,
		-1, priv->tag_error, NULL);
}

void
schematic_log_show (Schematic *schematic)
{
	g_return_if_fail (schematic != NULL);
	g_return_if_fail (IS_SCHEMATIC (schematic));

	g_signal_emit_by_name (schematic, "log_updated", schematic->priv->log);
}

void
schematic_log_clear (Schematic *schematic)
{
	g_return_if_fail (schematic != NULL);
	g_return_if_fail (IS_SCHEMATIC (schematic));

	gtk_text_buffer_set_text (
		schematic->priv->log, "", -1);
}

GtkTextBuffer*
schematic_get_log_text (Schematic *schematic)
{
	g_return_val_if_fail (schematic != NULL, NULL);
	g_return_val_if_fail (IS_SCHEMATIC (schematic), NULL);

	return schematic->priv->log;
}

int
schematic_count (void)
{
	return schematic_count_;
}

Schematic *
schematic_read (char *name, GError **out_error)
{
	Schematic *new_sm;
	int ret;
	char *fname;
	GError *error = NULL;
	FileType *ft;

	g_return_val_if_fail (name != NULL, NULL);

	fname = g_filename_from_uri  (name, NULL, &error);

	if (!fname) {
		fname = name;
		if (error) {
			g_error_free (error);
			error = NULL;
		}
	}

	if (!g_file_test (fname, G_FILE_TEST_EXISTS)) {
		g_set_error (out_error, OREGANO_ERROR, OREGANO_SCHEMATIC_FILE_NOT_FOUND,
			_("File %s does not exists."), fname);
		return NULL;
	}

	// Get File Handler
	ft = file_manager_get_handler (fname);
	if (ft == NULL) {
		g_set_error (out_error, OREGANO_ERROR, OREGANO_SCHEMATIC_FILE_NOT_FOUND,
			_("Unknown file format for %s."), fname);
		return NULL;
	}

	new_sm = schematic_new ();

	/* TODO : Add GError-like error reporting! */
	ret = ft->load_func (new_sm, fname, &error);

	if (error != NULL) {
		g_propagate_error (out_error, error);
		g_object_unref (G_OBJECT (new_sm));
		return NULL;
	}

	if (ret) {
		g_object_unref (G_OBJECT (new_sm));
		new_sm = NULL;
		g_set_error (out_error, OREGANO_ERROR, OREGANO_SCHEMATIC_FILE_NOT_FOUND,
			_("Load fails!."));
	} 
	else
		schematic_set_dirty (new_sm, FALSE);

	return new_sm;
}

gint
schematic_save_file (Schematic *sm, GError **error)
{
	FileType *ft;
	GError *internal_error = NULL;

	g_return_val_if_fail (sm != NULL, FALSE);

	ft = file_manager_get_handler (schematic_get_filename (sm));

	if (ft == NULL) {
		g_set_error (error, OREGANO_ERROR, OREGANO_SCHEMATIC_FILE_NOT_FOUND,
			_("Unknown file format for %s."), schematic_get_filename (sm));
		return FALSE;
	}

	if (ft->save_func (sm, &internal_error)) {
		schematic_set_title (sm, g_path_get_basename (sm->priv->filename));
		schematic_set_dirty (sm, FALSE);
		return TRUE;
	}

	g_propagate_error (error, internal_error);

	g_error_free (internal_error);
	return FALSE; // Save fails!
}

void
schematic_add_item (Schematic *sm, ItemData *data)
{
	NodeStore *store;
	char *prefix = NULL, *refdes = NULL;
	int num;

	g_return_if_fail (sm != NULL);
	g_return_if_fail (IS_SCHEMATIC (sm));
	g_return_if_fail (data != NULL);
	g_return_if_fail (IS_ITEM_DATA (data));

	store = sm->priv->store;
	g_object_set (G_OBJECT (data), 
	              "store", store, 
	              NULL);
	
	if (item_data_register (data) == -1)
		// Item does not be added
		return;

	// Some items need a reference designator. Find a good one.
	prefix = item_data_get_refdes_prefix (data);
	if (prefix != NULL) {
		num = schematic_get_lowest_available_refdes (sm, prefix);
		refdes = g_strdup_printf ("%s%d", prefix, num);
		item_data_set_property (data, "refdes", refdes);

		schematic_set_lowest_available_refdes (sm, prefix, num + 1);
	}
	g_free (prefix);
	g_free (refdes);

	sm->priv->current_items = g_list_prepend (sm->priv->current_items, data);
	g_object_weak_ref (G_OBJECT (data), item_data_destroy_callback, 
		G_OBJECT (sm));

	g_signal_connect_object (data, "moved", G_CALLBACK (item_moved_callback), 
	                         sm, 0);

	sm->priv->dirty = TRUE;

	g_signal_emit_by_name (sm, 
	                       "item_data_added", data);
}

void
schematic_parts_foreach (Schematic *schematic,
	ForeachItemDataFunc func, gpointer user_data)
{
	GList *list;

	g_return_if_fail (schematic != NULL);
	g_return_if_fail (IS_SCHEMATIC (schematic));

	if (func == NULL)
		return;

	for (list = node_store_get_parts (schematic->priv->store); list; list = list->next) {
		func (list->data, user_data);
	}
	g_list_free_full (list, g_object_unref);
}

void
schematic_wires_foreach (Schematic *schematic,
	ForeachItemDataFunc func, gpointer user_data)
{
	GList *list;

	g_return_if_fail (schematic != NULL);
	g_return_if_fail (IS_SCHEMATIC (schematic));

	if (func == NULL)
		return;

	for (list = node_store_get_wires (schematic->priv->store); list; list = list->next) {
		func (list->data, user_data);
	}
	g_list_free_full (list, g_object_unref);
}

void
schematic_items_foreach (Schematic *schematic,
	ForeachItemDataFunc func, gpointer user_data)
{
	GList *list;

	g_return_if_fail (schematic != NULL);
	g_return_if_fail (IS_SCHEMATIC (schematic));

	if (func == NULL)
		return;

	for (list = schematic->priv->current_items; list; list = list->next) {
		func (list->data, user_data);
	}
	g_list_free_full (list, g_object_unref);
}

GList *
schematic_get_items (Schematic *sm)
{
	g_return_val_if_fail (sm != NULL, NULL);
	g_return_val_if_fail (IS_SCHEMATIC (sm), NULL);

	return sm->priv->current_items;
}

static void
item_data_destroy_callback (gpointer s, GObject *data)
{
	Schematic *sm = SCHEMATIC (s);
	schematic_set_dirty (sm, TRUE);
	if (sm->priv) {
		sm->priv->current_items = g_list_remove (sm->priv->current_items, data);
	}
}

static int
schematic_get_lowest_available_refdes (Schematic *schematic, char *prefix)
{
	gpointer key, value;

	g_return_val_if_fail (schematic != NULL, -1);
	g_return_val_if_fail (IS_SCHEMATIC (schematic), -1);
	g_return_val_if_fail (prefix != NULL, -1);

	if ( g_hash_table_lookup_extended (schematic->priv->refdes_values,
		     prefix, &key, &value) ) {
		return GPOINTER_TO_INT (value);
	} 
	else {
		return 1;
	}
}

static void
schematic_set_lowest_available_refdes (Schematic *schematic,
	char *prefix, int num)
{
	gpointer key, value;

	g_return_if_fail (schematic != NULL);
	g_return_if_fail (IS_SCHEMATIC (schematic));
	g_return_if_fail (prefix != NULL);

	// If there already is a key, use it, otherwise copy the prefix and
	// use as key.
	if (!g_hash_table_lookup_extended (schematic->priv->refdes_values,
		prefix, &key, &value))
		key = g_strdup (prefix);

	g_hash_table_insert (schematic->priv->refdes_values,
						 key, GINT_TO_POINTER (num));
}

gboolean
schematic_is_dirty (Schematic *sm)
{
	g_return_val_if_fail (sm != NULL, FALSE);
	g_return_val_if_fail (IS_SCHEMATIC (sm), FALSE);

	return sm->priv->dirty;
}

void
schematic_set_dirty (Schematic *sm, gboolean b)
{
	g_return_if_fail (sm != NULL);
	g_return_if_fail (IS_SCHEMATIC (sm));

	sm->priv->dirty = b;
}

static void
item_moved_callback (ItemData *data, SheetPos *pos, Schematic *sm)
{
	g_return_if_fail (data != NULL);
	g_return_if_fail (IS_ITEM_DATA (data));

	schematic_set_dirty (sm, TRUE);
}

static void
schematic_render (Schematic *sm, cairo_t *cr)
{
	NodeStore *store;
	SchematicPrintContext schematic_print_context;
	schematic_print_context.colors = sm->priv->colors;
	store = schematic_get_store (sm);
	
	node_store_print_items (store, cr, &schematic_print_context);
}

GdkColor
convert_to_grayscale (GdkColor *source)
{
	GdkColor color;
	int factor;

	factor = (source->red + source->green + source->blue)/3;
	color.red = factor;
	color.green = factor;
	color.blue = factor;
	color.pixel = source->pixel;

	return color;
}

void
schematic_export (Schematic *sm, const gchar *filename,
	gint img_w, gint img_h, int bg, int color, int format)
{
	NodeRect bbox;
	NodeStore *store;
	cairo_surface_t *surface;
	cairo_t *cr;
	gdouble graph_w, graph_h;
	gdouble scale, scalew, scaleh;
	SchematicColors colors;

	if (!color) {
		colors = sm->priv->colors;
		sm->priv->colors.components = convert_to_grayscale (&sm->priv->colors.components);
		sm->priv->colors.labels = convert_to_grayscale (&sm->priv->colors.labels);
		sm->priv->colors.wires = convert_to_grayscale (&sm->priv->colors.wires);
		sm->priv->colors.text = convert_to_grayscale (&sm->priv->colors.text);
		sm->priv->colors.background = convert_to_grayscale (&sm->priv->colors.background);
	}

	store = schematic_get_store (sm);
	node_store_get_bounds (store, &bbox);

	switch (format) {
#ifdef CAIRO_HAS_SVG_SURFACE
		case 0:
			surface = cairo_svg_surface_create (filename, img_w, img_h);
		break;
#endif
#ifdef CAIRO_HAS_PDF_SURFACE
		case 1:
			surface = cairo_pdf_surface_create (filename, img_w, img_h);
		break;
#endif
#ifdef CAIRO_HAS_PS_SURFACE
		case 2:
			surface = cairo_ps_surface_create (filename, img_w, img_h);
		break;
#endif
		default:
			surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, img_w, img_h);
	}
	cr = cairo_create (surface);

	// Background
	switch (bg) {
		case 1: // White
			cairo_set_source_rgb (cr, 1.0, 1.0, 1.0);
			cairo_rectangle (cr, 0, 0, img_w, img_h);
			cairo_fill (cr);
			break;
		case 2: // Black
			cairo_set_source_rgb (cr, 0.0, 0.0, 0.0);
			cairo_rectangle (cr, 0, 0, img_w, img_h);
			cairo_fill (cr);
	}

	graph_w = img_w * 0.8;
	graph_h = img_h * 0.8;
	scalew = graph_w / (bbox.x1 - bbox.x0);
	scaleh = graph_h / (bbox.y1 - bbox.y0);
	if (scalew < scaleh)
		scale = scalew;
	else
		scale = scaleh;

	// Preparing...
	cairo_save (cr);
	cairo_translate (cr, (img_w - graph_w)/2.0, (img_h - graph_h) / 2.0);
	cairo_scale (cr, scale, scale);	
	cairo_translate (cr, -bbox.x0, -bbox.y0);
	cairo_set_line_width (cr, 0.5);

	// Render...
	schematic_render (sm, cr);

	cairo_restore (cr);
	cairo_show_page (cr);

	// Saving...
	if (format >= 3)
		cairo_surface_write_to_png (surface, filename);
	cairo_destroy (cr);
	cairo_surface_destroy (surface);

	// Restore color information
	if (!color) {
		sm->priv->colors = colors;
	}
}

static void
draw_rotule (Schematic *sm, cairo_t *cr)
{
	cairo_save (cr);
		cairo_set_source_rgb (cr, 0.0, 0.0, 0.0);
		cairo_set_line_width (cr, 0.5);
		cairo_rectangle (cr, 0, 0, 180, 20);
		cairo_rectangle (cr, 0, 20, 180, 10);
		cairo_stroke (cr);
	cairo_restore (cr);
}

static void
draw_page (GtkPrintOperation *operation,
	GtkPrintContext *context, int page_nr, Schematic *sm)
{
	NodeStore *store;
	NodeRect bbox;
	gdouble page_w, page_h;
	
	page_w = gtk_print_context_get_width (context);
	page_h = gtk_print_context_get_height (context);

	cairo_t *cr = gtk_print_context_get_cairo_context (context);

	// Draw a red rectangle, as wide as the paper (inside the margins)
	cairo_save (cr);
		cairo_set_source_rgb (cr, 0.0, 0.0, 0.0);
		cairo_set_line_width (cr, 0.5);
		cairo_rectangle (cr, 20, 10, page_w-30, page_h-20);
		cairo_stroke (cr);
	cairo_restore (cr);

	cairo_save (cr);
		cairo_translate (cr, page_w-190, page_h-40);
		draw_rotule (sm, cr);
	cairo_restore (cr);

	store = schematic_get_store (sm);

	node_store_get_bounds (store, &bbox);

	cairo_save (cr);
		cairo_set_line_width (cr, 0.5);
		cairo_set_source_rgb (cr, 0, 0, 0);
		cairo_translate (cr, page_w * 0.1, page_h * 0.1);
		// 0.4 is the convert factor between Model unit and
		// milimeters, unit used in printing
		cairo_scale (cr, 0.4, 0.4);	
		cairo_translate (cr, -bbox.x0, -bbox.y0);
		schematic_render (sm, cr);
	cairo_restore (cr);
}

static GObject*
print_options (GtkPrintOperation *operation, Schematic *sm)
{
	GtkBuilder *gui;
	GError *perror = NULL;
	
	if ((gui = gtk_builder_new ()) == NULL) {
		return G_OBJECT (gtk_label_new (_("Error loading print-options.ui")));
	}

	if (!g_file_test (OREGANO_UIDIR "/print-options.ui", G_FILE_TEST_EXISTS)) {
		return G_OBJECT (gtk_label_new (_("Error loading print-options.ui")));
	}

	if (gtk_builder_add_from_file (gui, OREGANO_UIDIR "/print-options.ui", 
	    &perror) <= 0) {
		g_error_free (perror);
		return G_OBJECT (gtk_label_new (_("Error loading print-options.ui")));
	}

	if (sm->priv->printoptions)
		g_free (sm->priv->printoptions);
	sm->priv->printoptions = g_new0 (SchematicPrintOptions, 1);

	sm->priv->printoptions->components = GTK_COLOR_BUTTON (
					gtk_builder_get_object (gui, "color_components"));
	sm->priv->printoptions->labels = GTK_COLOR_BUTTON (
					gtk_builder_get_object (gui, "color_labels"));
	sm->priv->printoptions->wires = GTK_COLOR_BUTTON (
					gtk_builder_get_object (gui, "color_wires"));
	sm->priv->printoptions->text = GTK_COLOR_BUTTON (
					gtk_builder_get_object (gui, "color_text"));
	sm->priv->printoptions->background = GTK_COLOR_BUTTON (
					gtk_builder_get_object (gui, "color_background"));

	// Set default colors
	gtk_color_button_set_color (GTK_COLOR_BUTTON (
					gtk_builder_get_object (gui, "color_components")),
					&sm->priv->colors.components);
	gtk_color_button_set_color (GTK_COLOR_BUTTON (
					gtk_builder_get_object (gui, "color_labels")),
					&sm->priv->colors.labels);
	gtk_color_button_set_color (GTK_COLOR_BUTTON (
					gtk_builder_get_object (gui, "color_wires")),
					&sm->priv->colors.wires);
	gtk_color_button_set_color (GTK_COLOR_BUTTON (
					gtk_builder_get_object (gui, "color_text")),
					&sm->priv->colors.text);
	gtk_color_button_set_color (GTK_COLOR_BUTTON (
					gtk_builder_get_object (gui, "color_background")),
					&sm->priv->colors.background);

	return gtk_builder_get_object (gui, "widget");
}

static void
read_print_options (GtkPrintOperation *operation, GtkWidget *widget, Schematic *sm)
{
	SchematicPrintOptions *colors = sm->priv->printoptions;

	gtk_color_button_get_color (colors->components, &sm->priv->colors.components);
	gtk_color_button_get_color (colors->labels, &sm->priv->colors.labels);
	gtk_color_button_get_color (colors->wires, &sm->priv->colors.wires);
	gtk_color_button_get_color (colors->text, &sm->priv->colors.text);
	gtk_color_button_get_color (colors->background, &sm->priv->colors.background);

	g_free (sm->priv->printoptions);
	sm->priv->printoptions = NULL;
}

void
schematic_print (Schematic *sm, GtkPageSetup *page, GtkPrintSettings *settings, gboolean preview)
{
	GtkPrintOperation *op;
	GtkPrintOperationResult res;

	op = gtk_print_operation_new ();

	gtk_print_operation_set_print_settings (op, settings);
	gtk_print_operation_set_default_page_setup (op, page);
	gtk_print_operation_set_n_pages (op, 1);
	gtk_print_operation_set_unit (op, GTK_UNIT_MM);
	gtk_print_operation_set_use_full_page (op, TRUE);

	g_signal_connect (op, "create-custom-widget", 
	                  G_CALLBACK (print_options), sm);
	g_signal_connect (op, "custom-widget-apply", 
	                  G_CALLBACK (read_print_options), sm);
	g_signal_connect (op, "draw_page", 
	                  G_CALLBACK (draw_page), sm);

	gtk_print_operation_set_custom_tab_label (op, _("Schematic"));

	if (preview)
		res = gtk_print_operation_run (op, GTK_PRINT_OPERATION_ACTION_PREVIEW,
		NULL, NULL);
	else
		res = gtk_print_operation_run (op, GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG,
		NULL, NULL);

	if (res == GTK_PRINT_OPERATION_RESULT_CANCEL) {
	}
}
