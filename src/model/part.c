/*
 * part.c
 *
 * Part object: represents a schematic part
 *
 * Authors:
 *  Richard Hult <rhult@hem.passagen.se>
 *  Ricardo Markiewicz <rmarkie@fi.uba.ar>
 *  Andres de Barbara <adebarbara@fi.uba.ar>
 *
 * Web page: http://arrakis.lug.fi.uba.ar/
 *
 * Copyright (C) 1999-2001  Richard Hult
 * Copyright (C) 2003,2006  Ricardo Markiewicz
 *
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

#include <ctype.h>
#include <gnome.h>
#include <math.h>
#include <string.h>
#include "part.h"
#include "item-data.h"
#include "part-property.h"
#include "part-label.h"
#include "node-store.h"
#include "load-common.h"
#include "load-library.h"
#include "part-private.h"
#include "schematic-print-context.h"

static void part_class_init (PartClass *klass);

static void part_init (Part *part);

static void part_set_gproperty (GObject *object, guint prop_id,
	const GValue *value, GParamSpec *spec);

static void part_get_gproperty (GObject *object, guint prop_id, GValue *value,
	GParamSpec *spec);

static int part_set_properties (Part *part, GSList *properties);
static gboolean part_has_properties (ItemData *part);

static int part_set_labels (Part *part, GSList *labels);

static void part_copy (ItemData *dest, ItemData *src);

static ItemData *part_clone (ItemData *src);

static void part_rotate (ItemData *data, int angle, SheetPos *center);

static void part_flip (ItemData *data, gboolean horizontal, SheetPos *center);

static void part_update_bbox (Part *part);

static void part_unregister (ItemData *data);

static int part_register (ItemData *data);

static void part_set_property (ItemData *data, char *property, char *value);

static char *part_get_refdes_prefix (ItemData *data);
static void part_print (ItemData *data, cairo_t *cr, SchematicPrintContext *ctx);

enum {
	ARG_0,
	ARG_PROPERTIES,
	ARG_LABELS,
};

enum {
	CHANGED,
	LAST_SIGNAL
};

static guint part_signals [LAST_SIGNAL] = { 0 };
static ItemDataClass *parent_class = NULL;

GType
part_get_type (void)
{
	static GType part_type = 0;

	if (!part_type) {
		static const GTypeInfo part_info = {
			sizeof(PartClass),
			NULL, /* Base Init */
			NULL, /* Base Finalize */
			(GClassInitFunc)part_class_init, /* Class Init */
			NULL, /* Class Finalize */
			NULL, /* Class Data */
			sizeof(Part),
			0, /* n_preallocs */
			(GInstanceInitFunc)part_init, /* Instance init */
			NULL
		};
		part_type = g_type_register_static(TYPE_ITEM_DATA,
			"Part", &part_info, 0);
	}

	return part_type;
}

static void
part_finalize(GObject *object)
{
	Part *part;
	PartPriv *priv;
	GSList *list;

	part = PART (object);
	priv = part->priv;

	if (priv) {
		g_free (priv->name);

		for (list = priv->properties; list; list = list->next) {
			PartProperty *property = list->data;

			g_free (property->name);
			g_free (property->value);
			g_free (property);
		}
		g_slist_free (priv->properties);

		for (list = priv->labels; list; list = list->next) {
			PartLabel *label = list->data;

			g_free (label->name);
			g_free (label->text);
			g_free (label);
		}
		g_slist_free (priv->labels);

		g_free (priv->pins);
		g_free (priv->symbol_name);
		g_free (priv);
		part->priv = NULL;
	}

	G_OBJECT_CLASS(parent_class)->finalize(object);
}

static void
part_dispose(GObject *object)
{
	G_OBJECT_CLASS(parent_class)->dispose(object);
}

static void
part_class_init (PartClass *klass)
{
	GObjectClass *object_class;
	ItemDataClass *item_data_class;

	parent_class = g_type_class_peek(TYPE_ITEM_DATA);

	object_class = G_OBJECT_CLASS(klass);
	item_data_class = ITEM_DATA_CLASS(klass);

	object_class->set_property = part_set_gproperty;
	object_class->get_property = part_get_gproperty;
	object_class->dispose = part_dispose;
	object_class->finalize = part_finalize;

	g_object_class_install_property ( object_class,	ARG_PROPERTIES,
		g_param_spec_pointer("properties", "properties",
			"the properties", G_PARAM_READWRITE));
	g_object_class_install_property(object_class, ARG_LABELS,
		g_param_spec_pointer("labels", "labels", "the labels",
			G_PARAM_READWRITE));

	item_data_class->clone = part_clone;
	item_data_class->copy = part_copy;
	item_data_class->rotate = part_rotate;
	item_data_class->flip = part_flip;
	item_data_class->unreg = part_unregister;
	item_data_class->reg = part_register;

	item_data_class->get_refdes_prefix = part_get_refdes_prefix;
	item_data_class->set_property = part_set_property;
	item_data_class->has_properties = part_has_properties;
	item_data_class->print = part_print;

	part_signals[CHANGED] =
		g_signal_new ("changed", G_TYPE_FROM_CLASS(object_class),
			G_SIGNAL_RUN_FIRST,
			G_STRUCT_OFFSET (PartClass, changed),
			NULL,
			NULL,
			g_cclosure_marshal_VOID__VOID,
			G_TYPE_NONE,
			0);

// ?	g_object_class_add_signals (object_class, part_signals, LAST_SIGNAL);
}

static void
part_init (Part *part)
{
	PartPriv *priv;

	priv = g_new0(PartPriv, 1);

	part->priv = priv;
}

Part *
part_new (void)
{
	Part *part;

	part = PART(g_object_new(TYPE_PART, NULL));

	return part;
}

Part *
part_new_from_library_part (LibraryPart *library_part)
{
	Part *part;
	GSList *pins;
	PartPriv *priv;
	LibrarySymbol *symbol;

	g_return_val_if_fail (library_part != NULL, NULL);

	part = part_new();
	if (!part)
		return NULL;

	priv = part->priv;

	symbol = library_get_symbol (library_part->symbol_name);
	if (symbol ==  NULL){
		g_warning ("Couldn't find the requested symbol %s for part %s in library.\n",
			library_part->symbol_name,
			library_part->name);
		return NULL;
	}

	pins = symbol->connections;

	if ( pins )
		part_set_pins (part, pins);


	g_object_set (
		G_OBJECT (part),
		"Part::properties", library_part->properties,
		"Part::labels", library_part->labels,
		NULL);

	/* FIXME: */
	priv->name = g_strdup (library_part->name);
	priv->symbol_name = g_strdup (library_part->symbol_name);
	priv->library = library_part->library;

	part_update_bbox (part);

	return part;
}

static void
part_set_gproperty(GObject *object, guint prop_id, const GValue *value,
	GParamSpec *spec)
{
	GSList *list;
	Part *part = PART (object);

	switch (prop_id) {
	case ARG_PROPERTIES:
		list = g_value_get_pointer(value);
		part_set_properties (part, list);
		break;
	case ARG_LABELS:
		list = g_value_get_pointer(value);
		part_set_labels (part, list);
		break;
	}
}

static void
part_get_gproperty(GObject *object, guint prop_id, GValue *value,
	GParamSpec *spec)
{
	Part *part = PART (object);
	PartPriv *priv = part->priv;

	switch (prop_id) {
	case ARG_LABELS:
		g_value_set_pointer(value, priv->labels);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(part, prop_id, spec);
	}
}

int
part_get_num_pins (Part *part)
{
	PartPriv *priv;

	g_return_val_if_fail (part != NULL, 0);
	g_return_val_if_fail (IS_PART (part), 0);

	priv = part->priv;
	return priv->num_pins;
}

int
part_get_rotation (Part *part)
{
	PartPriv *priv;

	g_return_val_if_fail (part != NULL, 0);
	g_return_val_if_fail (IS_PART (part), 0);

	priv = part->priv;
	return priv->rotation;
}

IDFlip
part_get_flip (Part *part)
{
	PartPriv *priv;

	g_return_val_if_fail (part != NULL, 0);
	g_return_val_if_fail (IS_PART (part), 0);

	priv = part->priv;
	return priv->flip;
}

Pin *
part_get_pins (Part *part)
{
	PartPriv *priv;

	g_return_val_if_fail (part != NULL, NULL);
	g_return_val_if_fail (IS_PART (part), NULL);

	priv = part->priv;
	return priv->pins;
}

static gboolean
part_has_properties (ItemData *item)
{
	Part *part = PART (item);

	return part->priv->properties != NULL;
}

static int
part_set_properties (Part *part, GSList *properties)
{
	PartPriv *priv;
	GSList *list;

	g_return_val_if_fail (part != NULL, FALSE);
	g_return_val_if_fail (IS_PART (part), FALSE);

	priv = part->priv;

	if (priv->properties != NULL)
		g_warning ("Properties already set!");

	/*
	 * Copy the properties list to the part.
	 */
	for (list = properties; list; list = list->next) {
		PartProperty *prop_new, *prop;

		prop = list->data;
		prop_new = g_new0 (PartProperty, 1);
		prop_new->name = g_strdup (prop->name);
		prop_new->value = g_strdup (prop->value);

		priv->properties = g_slist_prepend (priv->properties, prop_new);
	}

	return TRUE;
}

GSList *
part_get_properties (Part *part)
{
	PartPriv *priv;

	g_return_val_if_fail (part != NULL, FALSE);
	g_return_val_if_fail (IS_PART (part), FALSE);

	priv = part->priv;

	return priv->properties;
}

char *
part_get_property (Part *part, char *name)
{
	PartPriv *priv;
	GSList *props;
	PartProperty *prop;

	g_return_val_if_fail (part != NULL, NULL);
	g_return_val_if_fail (IS_PART (part), NULL);
	g_return_val_if_fail (name != NULL, NULL);

	priv = part->priv;

	for (props = priv->properties; props; props = props->next) {
		prop = props->data;
		if (g_strcasecmp (prop->name, name) == 0) {
			return g_strdup (prop->value);
		}
	}
	return NULL;
}

static int
part_set_labels (Part *part, GSList *labels)
{
	PartPriv *priv;
	GSList *list;

	g_return_val_if_fail (part != NULL, FALSE);
	g_return_val_if_fail (IS_PART (part), FALSE);

	priv = part->priv;

	if (priv->labels != NULL) {
		g_warning ("Part already has labels.");
		for (list = priv->labels; list; list = list->next) {
			PartLabel *label = list->data;
			g_free (label->name);
			g_free (label->text);
			g_free (label);
		}
		g_slist_free (priv->labels);
		priv->labels = NULL;
	}

	for (list = labels; list; list = list->next) {
		PartLabel *label, *label_copy;

		label = list->data;

		label_copy = g_new0 (PartLabel, 1);
		label_copy->name = g_strdup (label->name);
		label_copy->text = g_strdup (label->text);
		label_copy->pos.x = label->pos.x;
		label_copy->pos.y = label->pos.y;
		priv->labels = g_slist_prepend (priv->labels, label_copy);
	}

	return TRUE;
}

int
part_set_pins (Part *part, GSList *pins)
{
	PartPriv *priv;
	GSList *list;
	int num_pins, i;

	g_return_val_if_fail (part != NULL, FALSE);
	g_return_val_if_fail (IS_PART (part), FALSE);
	g_return_val_if_fail (pins != NULL, FALSE);

	priv = part->priv;

	num_pins = g_slist_length (pins);

	if (priv->pins)
		g_free (priv->pins);

	priv->pins = g_new0 (Pin, num_pins);
	priv->num_pins = num_pins;

	for (list = pins, i = 0; list; list = list->next, i++) {
		/*
		 * Note that this is slightly hackish. The list contains
		 * Connections which only have the SheetPos field.
		 */
		Pin *pin = list->data;

		priv->pins[i].pin_nr = i;
		priv->pins[i].node_nr= 0;
		priv->pins[i].offset.x = pin->offset.x;
		priv->pins[i].offset.y = pin->offset.y;
		priv->pins[i].part = part;
	}

	return TRUE;
}

GSList *
part_get_labels (Part *part)
{
	PartPriv *priv;

	g_return_val_if_fail (part != NULL, NULL);
	g_return_val_if_fail (IS_PART (part), NULL);

	priv = part->priv;

	return priv->labels;
}

static void
part_rotate (ItemData *data, int angle, SheetPos *center)
{
	double affine[6], dx, dy;
	ArtPoint src, dst;
	Part *part;
	PartPriv *priv;
	int i, tot_rotation;
	SheetPos b1, b2, part_center_before, part_center_after, delta;

	g_return_if_fail (data != NULL);
	g_return_if_fail (IS_PART (data));

	if (angle == 0)
		return;

	part = PART (data);

	priv = part->priv;

	tot_rotation = (priv->rotation + angle)%360;
	
	priv->rotation = tot_rotation;

	art_affine_rotate (affine, angle);

	/*
	 * Rotate the pins.
	 */
	for (i = 0; i < priv->num_pins; i++) {
		src.x = priv->pins[i].offset.x;
		src.y = priv->pins[i].offset.y;
		art_affine_point (&dst, &src, affine);

		if (fabs (dst.x) < 1e-2)
			dst.x = 0.0;
		if (fabs (dst.y) < 1e-2)
			dst.y = 0.0;

		priv->pins[i].offset.x = dst.x;
		priv->pins[i].offset.y = dst.y;
	}

	item_data_get_relative_bbox (ITEM_DATA (part), &b1, &b2);
	part_center_before.x = b1.x + (b2.x - b1.x) / 2;
	part_center_before.y = b1.y + (b2.y - b1.y) / 2;

	/*
	 * Rotate the bounding box.
	 */
	src.x = b1.x;
	src.y = b1.y;
	art_affine_point (&dst, &src, affine);
	b1.x = dst.x;
	b1.y = dst.y;

	src.x = b2.x;
	src.y = b2.y;
	art_affine_point (&dst, &src, affine);
	b2.x = dst.x;
	b2.y = dst.y;

	item_data_set_relative_bbox (ITEM_DATA (part), &b1, &b2);

	if (center) {
		SheetPos part_pos;

		part_center_after.x = b1.x + (b2.x - b1.x) / 2;
		part_center_after.y = b1.y + (b2.y - b1.y) / 2;

		dx = part_center_before.x - part_center_after.x;
		dy = part_center_before.y - part_center_after.y;

		item_data_get_pos (ITEM_DATA (part), &part_pos);

		src.x = part_center_before.x - center->x + part_pos.x;
		src.y = part_center_before.y - center->y + part_pos.y;
		art_affine_point (&dst, &src, affine);

		delta.x = dx - src.x + dst.x;
		delta.y = dy - src.y + dst.y;

		item_data_move (ITEM_DATA (part), &delta);
	}
	
	/*
	 * Let the views (canvas items) know about the rotation.
	 */
	g_signal_emit_by_name (G_OBJECT (part), "rotated", angle);

}

static void
part_flip (ItemData *data, gboolean horizontal, SheetPos *center)
{
	Part *part;
	PartPriv *priv;
	SheetPos b1, b2;
	int i;
	double affine[6];
	ArtPoint src, dst;
	SheetPos part_center_before, part_center_after;

	g_return_if_fail (data != NULL);
	g_return_if_fail (IS_PART (data));

	part = PART (data);
	priv = part->priv;

	if (horizontal && !(priv->flip & ID_FLIP_HORIZ))
		priv->flip = priv->flip | ID_FLIP_HORIZ;
	else if (horizontal && (priv->flip & ID_FLIP_HORIZ))
		priv->flip = priv->flip & ~ID_FLIP_HORIZ;
	else if (!horizontal && !(priv->flip & ID_FLIP_VERT))
		priv->flip = priv->flip | ID_FLIP_VERT;
	else if (!horizontal && (priv->flip & ID_FLIP_VERT))
		priv->flip = priv->flip & ~ID_FLIP_VERT;

	if (horizontal)
		art_affine_scale (affine, -1, 1);
	else
		art_affine_scale (affine, 1, -1);

	/*
	 * Flip the pins.
	 */
	item_data_get_relative_bbox (ITEM_DATA (part), &b1, &b2);
	for (i = 0; i < priv->num_pins; i++) {
		src.x = priv->pins[i].offset.x;
		src.y = priv->pins[i].offset.y;
		art_affine_point (&dst, &src, affine);

		if (fabs (dst.x) < 1e-2)
			dst.x = 0;
		if (fabs (dst.y) < 1e-2)
			dst.y = 0;

		priv->pins[i].offset.x = dst.x;
		priv->pins[i].offset.y = dst.y;
	}

	/*
	 * Tell the views.
	 */
	g_signal_emit_by_name (G_OBJECT (part), "flipped", horizontal);

	if (center) {
		item_data_get_relative_bbox (ITEM_DATA (part), &b1, &b2);
		part_center_before.x = b1.x + (b2.x - b1.x) / 2;
		part_center_before.y = b1.y + (b2.y - b1.y) / 2;
	}

	/*
	 * Flip the bounding box.
	 */
	src.x = b1.x;
	src.y = b1.y;
	art_affine_point (&dst, &src, affine);
	b1.x = dst.x;
	b1.y = dst.y;

	src.x = b2.x;
	src.y = b2.y;
	art_affine_point (&dst, &src, affine);
	b2.x = dst.x;
	b2.y = dst.y;

	item_data_set_relative_bbox (ITEM_DATA (part), &b1, &b2);

	if (center) {
		SheetPos part_pos, delta;
		double dx, dy;

		part_center_after.x = b1.x + (b2.x - b1.x) / 2;
		part_center_after.y = b1.y + (b2.y - b1.y) / 2;

		dx = part_center_before.x - part_center_after.x;
		dy = part_center_before.y - part_center_after.y;

		item_data_get_pos (ITEM_DATA (part), &part_pos);

		src.x = part_center_before.x - center->x + part_pos.x;
		src.y = part_center_before.y - center->y + part_pos.y;
		art_affine_point (&dst, &src, affine);

		delta.x = dx - src.x + dst.x;
		delta.y = dy - src.y + dst.y;

		item_data_move (ITEM_DATA (part), &delta);
	}
}

static ItemData *
part_clone (ItemData *src)
{
	Part *src_part, *new_part;
	ItemDataClass *id_class;

	g_return_val_if_fail (src != NULL, NULL);
	g_return_val_if_fail (IS_PART (src), NULL);

	id_class = ITEM_DATA_CLASS (G_OBJECT_GET_CLASS(src));
	if (id_class->copy == NULL)
		return NULL;

	src_part = PART (src);
	new_part = PART(g_object_new(TYPE_PART, NULL));
	new_part->priv->pins = g_new0 (Pin, src_part->priv->num_pins);
	id_class->copy (ITEM_DATA (new_part), src);

	return ITEM_DATA (new_part);
}

static void
part_copy (ItemData *dest, ItemData *src)
{
	Part *dest_part, *src_part;
	GSList *list;
	int i;

	g_return_if_fail (dest != NULL);
	g_return_if_fail (IS_PART (dest));
	g_return_if_fail (src != NULL);
	g_return_if_fail (IS_PART (src));

	if (parent_class->copy != NULL)
		parent_class->copy (dest, src);

	dest_part = PART (dest);
	src_part = PART (src);

	dest_part->priv->rotation = src_part->priv->rotation;
	dest_part->priv->flip = src_part->priv->flip;
	dest_part->priv->num_pins = src_part->priv->num_pins;
	dest_part->priv->library = src_part->priv->library;
	dest_part->priv->name = g_strdup (src_part->priv->name);
	dest_part->priv->symbol_name = g_strdup (src_part->priv->symbol_name);

	memcpy (dest_part->priv->pins, src_part->priv->pins,
		src_part->priv->num_pins * sizeof (Pin));
	for (i = 0; i < dest_part->priv->num_pins; i++)
		dest_part->priv->pins[i].part = dest_part;

	/*
	 * Copy properties and labels.
	 */
	dest_part->priv->properties =
		g_slist_copy (src_part->priv->properties);
	for (list = dest_part->priv->properties; list; list = list->next) {
		PartProperty *prop, *new_prop;

		new_prop = g_new0 (PartProperty, 1);
		prop = list->data;
		new_prop->name = g_strdup (prop->name);
		new_prop->value = g_strdup (prop->value);
		list->data = new_prop;
	}

	dest_part->priv->labels = g_slist_copy (src_part->priv->labels);
	for (list = dest_part->priv->labels; list; list = list->next) {
		PartLabel *label, *new_label;

		new_label = g_new0 (PartLabel, 1);
		label = list->data;
		new_label->name = g_strdup (label->name);
		new_label->text = g_strdup (label->text);
		new_label->pos = label->pos;
		list->data = new_label;
	}
}

static void
part_update_bbox (Part *part)
{
	GSList *objects;
	LibrarySymbol *symbol;
	SymbolObject *object;
	GnomeCanvasPoints *points;
	int i;

	SheetPos b1, b2;

	symbol = library_get_symbol (part->priv->symbol_name);
	if (symbol == NULL) {
		g_warning ("Couldn't find the requested symbol.");
		return;
	}

	b1.x = b1.y = b2.x = b2.y = 0.0;

	for (objects = symbol->symbol_objects; objects;
	     objects = objects->next){
		object = objects->data;
		switch (object->type){
		case SYMBOL_OBJECT_LINE:
			points = object->u.uline.line;

			for (i = 0; i < points->num_points; i++) {
				b1.x = MIN (points->coords[i * 2], b1.x);
				b1.y = MIN (points->coords[i * 2 + 1], b1.y);

				b2.x = MAX (points->coords[i * 2], b2.x);
				b2.y = MAX (points->coords[i * 2 + 1], b2.y);
			}
			break;

		case SYMBOL_OBJECT_ARC:
			b1.x = MIN (object->u.arc.x1, b1.x);
			b1.y = MIN (object->u.arc.y1, b1.y);

			b2.x = MAX (object->u.arc.x1, b2.x);
			b2.y = MAX (object->u.arc.y1, b2.y);

			b1.x = MIN (object->u.arc.x2, b1.x);
			b1.y = MIN (object->u.arc.y2, b1.y);

			b2.x = MAX (object->u.arc.x2, b2.x);
			b2.y = MAX (object->u.arc.y2, b2.y);

			break;

		case SYMBOL_OBJECT_TEXT:
		   {
			   /*GdkFont *font = gdk_font_load ("Sans 10");
				 b1.x = b1.y =  0;
				 b2.x = 2*object->u.text.x +
				 gdk_string_width (font, object->u.text.str );
				 b2.y = 2*object->u.text.y +
				 gdk_string_height (font,object->u.text.str );
				*/
		   }
		   break;


		default:
			g_warning ("Unknown symbol object.\n");
			continue;
		}
	}

	item_data_set_relative_bbox (ITEM_DATA (part), &b1, &b2);
}

static void
part_unregister (ItemData *data)
{
	NodeStore *store;

	g_return_if_fail (IS_PART (data));

	store = item_data_get_store (data);
	node_store_remove_part (store, PART (data));
}

static int 
part_register (ItemData *data)
{
	NodeStore *store;

	g_return_val_if_fail (IS_PART (data), -1);

	store = item_data_get_store (data);
	node_store_add_part (store, PART (data));

	return 0;
}

static char *
part_get_refdes_prefix (ItemData *data)
{
	Part *part;
	char *refdes;
	int i, length;
	gboolean found_num;

	g_return_val_if_fail (IS_PART (data), NULL);

	part = PART (data);

	refdes = part_get_property (part, "refdes");
	if (refdes == NULL)
		return NULL;

	/*
	 * FIXME: the ref.des. should be stored in two parts,
	 * as a char and an integer.
	 */

	 /*
	 * Get the 'prefix' i.e R for resistors.
	 */
	found_num = FALSE;
	length = strlen (refdes);
	for (i = 0; i < length; i++) {
		if (!isalpha (refdes[i])) {
			found_num = TRUE;
			break;
		}
	}

	if (found_num)
		return NULL; //g_strndup (refdes, i - 1);
	else
		return g_strdup (refdes);

	return NULL;
}

static void
part_set_property (ItemData *data, char *property, char *value)
{
	Part *part;
	PartPriv *priv;
	GSList *props;
	PartProperty *prop;

	part = PART (data);

	g_return_if_fail (part != NULL);
	g_return_if_fail (IS_PART (part));
	g_return_if_fail (property != NULL);

	priv = part->priv;

	for (props = priv->properties; props; props = props->next) {
		prop = props->data;
		if (g_strcasecmp (prop->name, property) == 0) {
			g_free (prop->value);
			if (value != NULL)
				prop->value = g_strdup (value);
			else
				prop->value = NULL;
			break;
		}
	}
}

static void
part_print (ItemData *data, cairo_t *cr, SchematicPrintContext *ctx)
{
	GSList *objects, *labels;
	SymbolObject *object;
	LibrarySymbol *symbol;
	double x0, y0;
	int i, rotation;
	Part *part;
	PartPriv *priv;
	SheetPos pos;
	IDFlip flip;
	GnomeCanvasPoints *line;

	g_return_if_fail (data != NULL);
	g_return_if_fail (IS_PART (data));

	part = PART (data);
	priv = part->priv;

	symbol = library_get_symbol (priv->symbol_name);
	if (symbol == NULL) {
		return;
	}

	item_data_get_pos (ITEM_DATA (part), &pos);
	x0 = pos.x;
	y0 = pos.y;

	cairo_save (cr);

	gdk_cairo_set_source_color (cr, &ctx->colors.components);
	rotation = part_get_rotation (part);
	if (rotation != 0) {
	  cairo_translate (cr, x0, y0);

	  flip = part_get_flip (part);
	  if (flip) {
	    if ((flip & ID_FLIP_HORIZ) && !(flip & ID_FLIP_VERT))
	      cairo_scale (cr, -1, 1);
	    if (!(flip & ID_FLIP_HORIZ) && (flip & ID_FLIP_VERT))
	      cairo_scale (cr, 1, -1);
	    if ((flip & ID_FLIP_HORIZ) && (flip & ID_FLIP_VERT))
	      rotation+=180;
	  }

	  cairo_rotate (cr, rotation*M_PI/180);
	  cairo_translate (cr, -x0, -y0);
	} else {
	  flip = part_get_flip (part);
	  if (flip) {
	    cairo_translate (cr, x0, y0);	  
	    if ((flip & ID_FLIP_HORIZ) && !(flip & ID_FLIP_VERT))
	      cairo_scale (cr, -1, 1);
	    if (!(flip & ID_FLIP_HORIZ) && (flip & ID_FLIP_VERT))
	      cairo_scale (cr, 1, -1);
	    if ((flip & ID_FLIP_HORIZ) && (flip & ID_FLIP_VERT))
	      cairo_scale(cr,-1,-1);
	    cairo_translate (cr, -x0, -y0);
	  }
	}

	for (objects = symbol->symbol_objects; objects; objects = objects->next) {
		object = (SymbolObject *)(objects->data);

		switch (object->type) {
			case SYMBOL_OBJECT_LINE:
				line = object->u.uline.line;
				for (i = 0; i < line->num_points; i++) {
					double x, y;

					x = line->coords[i * 2];
					y = line->coords[i * 2 + 1];

					if (i == 0)
						cairo_move_to (cr, x0 + x, y0 + y);
					else
						cairo_line_to (cr, x0 + x, y0 + y);
				}
			break;
			case SYMBOL_OBJECT_ARC: {
				gdouble x1 = object->u.arc.x1;
				gdouble y1 = object->u.arc.y1;
				gdouble x2 = object->u.arc.x2;
				gdouble y2 = object->u.arc.y2;
				gdouble width, height, x, y;

				x = (x2 - x1) / 2 + x1;
				y = (y2 - y1) / 2 + y1;
				width = x2 - x1;
				height = y2 - y1;

				cairo_save (cr);
					cairo_translate (cr, x0 + x, y0 + y);
					cairo_scale (cr, width / 2.0, height / 2.0);
					cairo_arc (cr, 0.0, 0.0, 1.0, 0.0, 2 * M_PI);
				cairo_restore (cr);
			}
			break;
			default:
				g_warning (
					"Print part: Part %s contains unknown object.",
					priv->name
				);
			continue;
		}

		cairo_stroke (cr);
	}

	/* We don't want to rotate labels text, only the (x,y)
	 * coordinate
	 */
	gdk_cairo_set_source_color (cr, &ctx->colors.labels);
	for (labels = part_get_labels (part); labels; labels = labels->next) {
		gdouble x, y;
		PartLabel *label = (PartLabel *)labels->data;
		gchar *text;
		/* gint text_width, text_height; */

		x = label->pos.x + x0;
		y = label->pos.y + y0;

		text = part_property_expand_macros (part, label->text);
		/* Align the label.
		switch (rotation) {
			case 90:
				y += text_height*opc->scale;
			break;
			case 180:
			break;
			case 270:
				x -= text_width*opc->scale;
			break;
			case 0:
			default:
			break;
		} */

		cairo_save (cr);
			cairo_move_to (cr, x, y);
			cairo_show_text (cr, text);
		cairo_restore (cr);
		g_free (text);
	}
	cairo_restore (cr);
}


