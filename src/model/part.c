/*
 * part.c
 *
 * Part object: represents a schematic part
 *
 * Authors:
 *  Richard Hult <rhult@hem.passagen.se>
 *  Ricardo Markiewicz <rmarkie@fi.uba.ar>
 *  Andres de Barbara <adebarbara@fi.uba.ar>
 *  Marc Lorber <lorber.marc@wanadoo.fr>
 *  Bernhard Schuster <schuster.bernhard@gmail.com>
 *
 * Web page: https://srctwig.com/oregano
 *
 * Copyright (C) 1999-2001  Richard Hult
 * Copyright (C) 2003,2006  Ricardo Markiewicz
 * Copyright (C) 2009-2012  Marc Lorber
 * Copyright (C) 2013       Bernhard Schuster
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
#include <math.h>
#include <string.h>
#include <glib/gi18n.h>
#include <stdlib.h>

#include "part.h"
#include "part-property.h"
#include "part-label.h"
#include "node-store.h"
#include "load-common.h"
#include "load-library.h"
#include "part-private.h"
#include "schematic-print-context.h"
#include "dialogs.h"

#include "debug.h"

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

static void part_rotate (ItemData *data, int angle, Coords *center);

static void part_flip (ItemData *data, IDFlip direction, Coords *center);

static void part_update_bbox (Part *part);

static void part_unregister (ItemData *data);

static int part_register (ItemData *data);

static void part_changed (ItemData *data);

static void part_set_property (ItemData *data, char *property, char *value);

static char *part_get_refdes_prefix (ItemData *data);
static void part_print (ItemData *data, cairo_t *cr, SchematicPrintContext *ctx);

enum {
	ARG_0,
	ARG_PROPERTIES,
	ARG_LABELS,
};

G_DEFINE_TYPE (Part, part, TYPE_ITEM_DATA)

static ItemDataClass *parent_class = NULL;

static void
part_init (Part *part)
{
	part->priv = g_slice_new0 (PartPriv);
}


static void
part_dispose (GObject *object)
{
	G_OBJECT_CLASS (parent_class)->dispose (object);
}


static void
part_finalize (GObject *object)
{
	PartPriv *priv;
	GSList *list;

	priv = PART (object)->priv;

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

		g_slice_free (PartPriv, priv);
	}
	G_OBJECT_CLASS (parent_class)->finalize (object);
}


static void
part_class_init (PartClass *klass)
{
	GObjectClass *object_class;
	ItemDataClass *item_data_class;

	parent_class = g_type_class_peek (TYPE_ITEM_DATA);

	object_class = G_OBJECT_CLASS (klass);
	item_data_class = ITEM_DATA_CLASS (klass);

	object_class->set_property = part_set_gproperty;
	object_class->get_property = part_get_gproperty;
	object_class->dispose = part_dispose;
	object_class->finalize = part_finalize;

	g_object_class_install_property (object_class,	ARG_PROPERTIES,
		g_param_spec_pointer ("properties", "properties",
			"the properties", G_PARAM_READWRITE));
	g_object_class_install_property (object_class, ARG_LABELS,
		g_param_spec_pointer ("labels", "labels", "the labels",
			G_PARAM_READWRITE));

	item_data_class->clone = part_clone;
	item_data_class->copy = part_copy;
	item_data_class->rotate = part_rotate;
	item_data_class->flip = part_flip;
	item_data_class->unreg = part_unregister;
	item_data_class->reg = part_register;
	item_data_class->changed = part_changed;

	item_data_class->get_refdes_prefix = part_get_refdes_prefix;
	item_data_class->set_property = part_set_property;
	item_data_class->has_properties = part_has_properties;
	item_data_class->print = part_print;

}

////////////////////////////////////////////////////////////////////////////////
// END BOILER PLATE
////////////////////////////////////////////////////////////////////////////////

Part *
part_new (Grid *grid)
{
	Part *part;

	part = PART (g_object_new (TYPE_PART, NULL));

	item_data_set_grid (ITEM_DATA (part), grid);

	return part;
}

Part *
part_new_from_library_part (LibraryPart *library_part, Grid *grid)
{
	Part *part;
	GSList *pins;
	PartPriv *priv;
	LibrarySymbol *symbol;

	g_return_val_if_fail (library_part != NULL, NULL);

	part = part_new (grid);
	if (!part)
		return NULL;

	priv = part->priv;

	symbol = library_get_symbol (library_part->symbol_name);
	if (symbol ==  NULL) {
		oregano_warning (g_strdup_printf (_("Couldn't find the requested symbol"
		"%s for part %s in library.\n"),
			library_part->symbol_name,
			library_part->name));
		return NULL;
	}

	pins = symbol->connections;

	if (pins)
		part_set_pins (part, pins);

	g_object_set (G_OBJECT (part),
		"Part::properties", library_part->properties,
		"Part::labels", library_part->labels,
		NULL);

	priv->name = g_strdup (library_part->name);
	priv->symbol_name = g_strdup (library_part->symbol_name);
	priv->library = library_part->library;

	part_update_bbox (part);

	return part;
}

static void
part_set_gproperty (GObject *object, guint prop_id, const GValue *value,
	GParamSpec *spec)
{
	GSList *list;
	Part *part = PART (object);

	switch (prop_id) {
	case ARG_PROPERTIES:
		list = g_value_get_pointer (value);
		part_set_properties (part, list);
		break;
	case ARG_LABELS:
		list = g_value_get_pointer (value);
		part_set_labels (part, list);
		break;
	}
}

static void
part_get_gproperty (GObject *object, guint prop_id, GValue *value,
	GParamSpec *spec)
{
	Part *part = PART (object);
	PartPriv *priv = part->priv;

	switch (prop_id) {
	case ARG_LABELS:
		g_value_set_pointer (value, priv->labels);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (part, prop_id, spec);
	}
}

gint
part_get_num_pins (Part *part)
{
	PartPriv *priv;

	g_return_val_if_fail (part != NULL, 0);
	g_return_val_if_fail (IS_PART (part), 0);

	priv = part->priv;
	return priv->num_pins;
}

gint
part_get_rotation (Part *part)
{
	ItemData *item;
	PartPriv *priv;
	gdouble register a,b,c,d, sx,sy;
	cairo_matrix_t *t;

	g_return_val_if_fail (part != NULL, 0);
	g_return_val_if_fail (IS_PART (part), 0);

	item = ITEM_DATA (part);
	priv = part->priv;

	t = item_data_get_rotate (item);
	a = t->xx;
	b =	t->xy;
	c = t->yx;
	d =	t->yy;

	sx = sqrt (a*a + c*c);
    sy = sqrt (b*b + d*d);
    if (G_UNLIKELY (abs(sx)<1e-10 && abs(sy)<1e-10)) {
		g_warning ("Unabled to calculate rotation from matrix. Assuming 0°.");
		return 0;
    }
#if 0
    if (abs(b)>abs(c))
		a = (gint)(180. * atan2(-b, a) / M_PI);
	echo
	return (gint)(180. * atan2(c, d) / M_PI);
#else
	if (abs(sx)>abs(sy))
		return (gint)(180. * acos(a / sx) / M_PI);
	return (gint)(180. * acos(d / sy) / M_PI);
#endif
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

static gboolean
part_set_properties (Part *part, GSList *properties)
{
	PartPriv *priv;
	GSList *list;

	g_return_val_if_fail (part != NULL, FALSE);
	g_return_val_if_fail (IS_PART (part), FALSE);

	priv = part->priv;

	if (priv->properties != NULL)
		g_warning ("Properties already set!");

	// Copy the properties list to the part.
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
		if (g_ascii_strcasecmp (prop->name, name) == 0) {
			return g_strdup (prop->value);
		}
	}
	return NULL;
}

static gboolean
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


/**
 * overwrite the pins with those given in the list
 */
gboolean
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

	if (priv->pins_orig)
		g_free (priv->pins_orig);

	priv->pins = g_new0 (Pin, num_pins);
	priv->pins_orig = g_new0 (Pin, num_pins);

	priv->num_pins = num_pins;

	for (list = pins, i = 0; list; list = list->next, i++) {
		// Note that this is slightly hackish. The list contains
		// Connections which only have the Coords field.
		Pin *pin = list->data;

		priv->pins_orig[i].pin_nr = i;
		priv->pins_orig[i].node_nr= 0;
		priv->pins_orig[i].offset.x = pin->offset.x;
		priv->pins_orig[i].offset.y = pin->offset.y;
		priv->pins_orig[i].part = part;

		memcpy (priv->pins, priv->pins_orig, sizeof(Pin)*num_pins);
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


/**
 * rotate an item by an @angle increment (may be negative)
 * @angle the increment the item will be rotated (usually 90° steps)
 * @center_pos if rotated as part of a group, this is the center to rotate around
 */
static void
part_rotate (ItemData *data, int angle, Coords *center_pos)
{
	g_return_if_fail (data);
	g_return_if_fail (IS_PART (data));

	cairo_matrix_t morph, morph_rot, local_rot;
	Part *part;
	PartPriv *priv;
	gboolean handler_connected;
	Coords b1, b2;

	part = PART (data);

	priv = part->priv;


	// FIXME store vanilla coords, apply the morph
	// FIXME to these and store the result in the
	// FIXME instance then everything will be fine
	// XXX also prevents rounding yiggle up downs



	cairo_matrix_init_rotate (&local_rot, (double)angle * M_PI / 180.);
	cairo_matrix_multiply (item_data_get_rotate (data), item_data_get_rotate (data), &local_rot);

	morph_rot = *(item_data_get_rotate (data));
	cairo_matrix_multiply (&morph,
	                       &morph_rot,
	                       item_data_get_translate (data));

	Coords delta_to_center, delta_to_center_transformed;
	Coords delta_to_apply, delta_bbox;
	Coords bbox_center, bbox_center_transformed;
	Coords item_pos;

	//get bbox
	item_data_get_relative_bbox (ITEM_DATA (part), &b1, &b2);

	bbox_center = coords_average (&b1, &b2);
	item_data_get_pos (ITEM_DATA (part), &item_pos);
#define DEBUG_THIS 1
	NG_DEBUG ("bbox[vanilla] = %lf,%lf to %lf,%lf - centered at %lf,%lf", b1.x, b1.y, b2.x, b2.y, bbox_center.x, bbox_center.y);


	if (center_pos) {
		delta_to_center_transformed = delta_to_center = coords_sub (center_pos, &item_pos);

		cairo_matrix_transform_point (&local_rot,
			                          &(delta_to_center_transformed.x),
			                          &(delta_to_center_transformed.y));
		delta_to_apply = coords_sub (&delta_to_center, &delta_to_center_transformed);
		NG_DEBUG ("delta{group} = transform from %lf,%lf to %lf,%lf", delta_to_center.x,delta_to_center.y, delta_to_center_transformed.x,delta_to_center_transformed.y);
		NG_DEBUG ("delta{group} = centered at %lf,%lf resulting in shift %lf,%lf", center_pos->x, center_pos->y, delta_to_apply.x, delta_to_apply.y);
	} else {
		delta_to_apply.x = delta_to_apply.y = 0.;
	}

	// use the cairo matrix funcs to transform the pin
	// positions relative to the item center
	// this is only indirectly related to displayin
	// HINT: we need to modify the actual pins to make the
	// pin tests work being used to detect connections

	gint i;
	gdouble x,y;
	// Rotate the pins.
	for (i = 0; i < priv->num_pins; i++) {
		x = priv->pins_orig[i].offset.x;
		y = priv->pins_orig[i].offset.y;
		cairo_matrix_transform_point (&morph_rot, &x, &y);

		if (fabs (x) < 1e-2)
			x = 0.0;
		if (fabs (y) < 1e-2)
			y = 0.0;

		priv->pins[i].offset.x = x;
		priv->pins[i].offset.y = y;
	}


	cairo_matrix_transform_point (&local_rot, &b1.x, &b1.y);
	cairo_matrix_transform_point (&local_rot, &b2.x, &b2.y);

	item_data_set_relative_bbox (data, &b1, &b2);

	bbox_center_transformed = coords_average (&b1, &b2);
	delta_bbox = coords_sub (&bbox_center, &bbox_center_transformed);
	coords_add (&delta_to_apply, &delta_bbox);
	NG_DEBUG ("bbox[trans] = %lf,%lf to %lf,%lf - centered at %lf,%lf", b1.x, b1.y, b2.x, b2.y, bbox_center_transformed.x, bbox_center_transformed.y);
	NG_DEBUG ("bbox[delta] = %lf,%lf", delta_bbox.x, delta_bbox.y);

#if 0
	part_center_after = coords_average (&b1, &b2);

	NG_DEBUG ("part_center_after = (%lf,%lf)", part_center_after.x, part_center_after.y);

	delta = coords_sub (&part_center_after, &part_center_before);
	NG_DEBUG ("center_delta = (%lf,%lf)", delta.x, delta.y);
	if (center_pos) {
		coords_add (&delta, &delta_delta);
	}
	item_data_move (data, &delta);
#endif


//	NG_DEBUG ("total[delta] = %lf,%lf", delta_to_apply.x, delta_to_apply.y);
	item_data_move (data, &delta_to_apply);
	item_data_snap (data);

#if 0
	handler_connected = g_signal_handler_is_connected (G_OBJECT (data),
	                                   data->rotated_handler_id);
	if (handler_connected) {
		g_signal_emit_by_name (G_OBJECT (data),
		                       "rotated", tot_rotation);
	}
#endif
	handler_connected = g_signal_handler_is_connected (G_OBJECT (data),
	                                   data->changed_handler_id);
	if (handler_connected) {
		g_signal_emit_by_name (G_OBJECT (data),
		                       "changed");
	}
	NG_DEBUG ("\n\n");
}


/**
 * flip a part in a given direction
 * @direction gives the direction the item will be flipped, end users pov!
 * @center the center to flip over - currently ignored FIXME
 */
static void
part_flip (ItemData *data, IDFlip direction, Coords *center)
{
#if 0
	Part *part;
	PartPriv *priv;
	int i;
	cairo_matrix_t affine;
	double x, y;
	double scale_v, scale_h;
	gboolean handler_connected;
	Coords delta;
	Coords pos, trans;
	Coords b1, b2;
	Coords pos_new, pos_old;
	//FIXME properly recenter after flipping
	//Coords part_center_before, part_center_after, delta;

	g_return_if_fail (data);
	g_return_if_fail (IS_PART (data));

	part = PART (data);
	priv = part->priv;

	item_data_get_pos (data, &trans);

	// mask, just for the sake of cleanness
	direction &= ID_FLIP_MASK;

	// TODO evaluate if we really want to be able to do double flips (180* rots via flipping)
	g_assert (direction != ID_FLIP_MASK);


	// create a transformation _relativ_ to the current _state_
	// reverse axis and fix the created offset by adding 2*pos.x or .y

	// convert the flip direction to binary, used in the matrix setup
	// keep in mind that we do relativ manipulations within the model
	// which in turn makes this valid for all rotations!
	scale_h = ((direction & ID_FLIP_HORIZ) != 0) ? -1. : 1.;
	scale_v = ((direction & ID_FLIP_VERT) != 0) ? -1. : 1.;

	// magic, if we are in either 270 or 90 state, we need to rotate the flip state by 90° to draw it properly
	// TODO maybe better put this into the rotation function
	if ((priv->rotation / 90) % 2 == 1) {
		priv->flip ^= ID_FLIP_MASK;
	}
	// toggle the direction
	priv->flip ^= direction;
	if ((priv->flip & ID_FLIP_MASK)== ID_FLIP_MASK) {
		priv->flip = ID_FLIP_NONE;
		priv->rotation += 180;
		priv->rotation %= 360;
	}

	cairo_matrix_init_scale (&affine, scale_h, scale_v);

	item_data_get_pos (data, &pos_old);
	pos_new = pos_old;
	cairo_matrix_transform_point (&affine, &pos_new.x, &pos_new.y);

	g_printf ("\ncenter %p [old] x=%lf,y=%lf -->", data, pos_old.x, pos_old.y);
	g_printf ("  x=%lf, y=%lf\n", pos_new.x, pos_new.y);
	delta.x = - pos_new.x + pos_old.x;
	delta.y = - pos_new.y + pos_old.y;

	// flip the pins
	for (i = 0; i < priv->num_pins; i++) {

		x = priv->pins[i].offset.x;
		y = priv->pins[i].offset.y;
		cairo_matrix_transform_point (&affine, &x, &y);

		if (fabs (x) < 1e-2)
			x = 0.0;
		if (fabs (y) < 1e-2)
			y = 0.0;

		priv->pins[i].offset.x = x;
		priv->pins[i].offset.y = y;
	}
	item_data_snap (data);

	// tell the view
	handler_connected = g_signal_handler_is_connected (G_OBJECT (part),
	                                                   ITEM_DATA(part)->flipped_handler_id);
	if (handler_connected) {
		g_signal_emit_by_name (G_OBJECT (part), "flipped", priv->flip);

		// TODO - proper boundingbox center calculation

		item_data_get_relative_bbox (ITEM_DATA (part), &b1, &b2);

		// flip the bounding box.
		cairo_matrix_transform_point (&affine, &b1.x, &b1.y);
		cairo_matrix_transform_point (&affine, &b2.x, &b2.y);

		item_data_set_relative_bbox (ITEM_DATA (part), &b1, &b2);
		item_data_set_pos (ITEM_DATA (part), &pos);

		// FIXME - proper recenter to boundingbox center
	}
	if (g_signal_handler_is_connected (G_OBJECT (part),
	                                   ITEM_DATA (part)->changed_handler_id)) {
		g_signal_emit_by_name (G_OBJECT (part),
		                       "changed");
	}
#endif
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
	new_part = g_object_new (TYPE_PART, NULL);
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

//	dest_part->priv->rotation = src_part->priv->rotation;
	dest_part->priv->flip = src_part->priv->flip;
	dest_part->priv->num_pins = src_part->priv->num_pins;
	dest_part->priv->library = src_part->priv->library;
	dest_part->priv->name = g_strdup (src_part->priv->name);
	dest_part->priv->symbol_name = g_strdup (src_part->priv->symbol_name);

	memcpy (dest_part->priv->pins, src_part->priv->pins,
		src_part->priv->num_pins * sizeof (Pin));
	for (i = 0; i < dest_part->priv->num_pins; i++)
		dest_part->priv->pins[i].part = dest_part;

	// Copy properties and labels.
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
	GSList *iter;
	LibrarySymbol *symbol;
	SymbolObject *object;
	GooCanvasPoints *points;
	int i;

	Coords b1, b2;

	symbol = library_get_symbol (part->priv->symbol_name);
	if (symbol == NULL) {
		g_warning ("Couldn't find the requested symbol.");
		return;
	}

	b1.x = b1.y = b2.x = b2.y = 0.0;

	for (iter = symbol->symbol_objects; iter; iter=iter->next) {
		object = iter->data;
		switch (object->type) {
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
				//FIXME
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

/**
 * register a part to its nodestore
 * @param data the part
 * @attention the @data has to have a valid nodestore set
 */
static int
part_register (ItemData *data)
{
	NodeStore *store;

	g_return_val_if_fail (IS_PART (data), FALSE);

	store = item_data_get_store (data);
	node_store_add_part (store, PART (data));

	return TRUE;
}


/**
 * simply signal a change in the part
 */
static void
part_changed (ItemData *data)
{
	g_return_if_fail (IS_PART (data));

#if 0
	Part *part;
	Coords loc = {0., 0.};
	int angle = 0;
	IDFlip flip = ID_FLIP_NONE;

	part = (Part *)data;


	flip = part_get_flip (part);
	angle = part_get_rotation (part);
	item_data_get_pos (data, &loc);

	//FIXME isn't it more sane to just emit the changed?
	g_signal_emit_by_name (data, "moved", &loc);
	g_signal_emit_by_name (data, "flipped", flip);
	g_signal_emit_by_name (data, "rotated", angle);
#endif
	g_signal_emit_by_name (data, "changed");
}


static char *
part_get_refdes_prefix (ItemData *data)
{
	Part *part;
	char *refdes;
	int i, length;

	g_return_val_if_fail (IS_PART (data), NULL);

	part = PART (data);

	refdes = part_get_property (part, "refdes");
	if (refdes == NULL)
		return NULL;

	// Get the 'prefix' i.e R for resistors.
	length = strlen (refdes);
	for (i = 0; i < length; i++) {
		if (isdigit (refdes[length-i -1])) {
			refdes[length -i -1] = '\0';
		}
		else break;
	}
	return g_strdup (refdes);
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
		if (g_ascii_strcasecmp (prop->name, property) == 0) {
			g_free (prop->value);
			if (value != NULL)
				prop->value = g_strdup (value);
			else
				prop->value = NULL;
			break;
		}
	}
}

/**
 * print the part onto a physical sheet of paper or pdf, which is represented by @cr
 */
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
	Coords pos;
	IDFlip flip;
	GooCanvasPoints *line;

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

	gdk_cairo_set_source_rgba (cr, &ctx->colors.components);
	rotation = part_get_rotation (part);
	flip = part_get_flip (part);

	if ((flip & ID_FLIP_HORIZ) && (flip & ID_FLIP_VERT))
		rotation += 180;
	else if (flip == ID_FLIP_HORIZ)
		cairo_scale (cr, -1, 1);
	else if (flip == ID_FLIP_VERT)
		cairo_scale (cr, 1, -1);

	if (rotation %= 360)
		cairo_rotate (cr, rotation*M_PI/180);

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

				x = (x2 + x1) / 2;
				y = (y2 + y1) / 2;
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

	// We don't want to rotate labels text, only the (x,y) coordinate
	gdk_cairo_set_source_rgba (cr, &ctx->colors.labels);
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
