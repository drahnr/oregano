/*
 * part-property.c
 *
 *
 * Authors:
 *  Richard Hult <rhult@hem.passagen.se>
 *  Ricardo Markiewicz <rmarkie@fi.uba.ar>
 *  Andres de Barbara <adebarbara@fi.uba.ar>
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
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include <glib.h>
#include <string.h>

#include "part.h"
#include "part-property.h"

// Gets the name of a macro variable.
//
// @param str  str
// @param cls1 returns first conditional clause
// @param cls2 returns second clause
// @param sz   returns number of characters parsed
// @return the name of a macro variable
static char *get_macro_name (char macro, const char *str, char **cls1, char **cls2, size_t *sz)
{
	char separators[] = {",.;/|()"};
	GString *out;
	const char *q, *qend;
	char *csep = NULL;
	size_t sln;
	int rc = 0;
	char *ret;

	sln = strlen (str) + 1;
	*sz = 0;
	*cls1 = *cls2 = NULL;
	qend = str + sln;
	out = g_string_sized_new (sln);

	// Get the name
	for (q = str; (*q) && (*q != ' ') && !(csep = strchr (separators, *q)); q++) {
		if (q > qend) {
			g_warning ("Expand macro error.");
			rc = 1;
			break;
		}
		out = g_string_append_c (out, *q);
	}

	// if error found, return here
	if (rc)
		goto error;

	// Look for conditional clauses
	if (csep && macro != '@' && macro != '&') {
		// get the first one
		GString *aux;
		q++; // skip the separator and store the clause in tmp
		aux = g_string_new ("");
		for (; (*q) && (*q != *csep); q++)
			g_string_append_c (aux, *q);

		if (!*q) {
			g_string_free (aux, TRUE);
			goto error;
		}

		*cls1 = aux->str;
		q++; // skip the end-of-clause separator
		g_string_free (aux, FALSE);

		// Check for the second one
		if ((*q) && (csep = strchr (separators, *q))) {
			q++; // skip the separator and store in tmp
			aux = g_string_new ("");
			for (; (*q) && (*q != *csep); q++)
				g_string_append_c (aux, *q);

			if (!(*q)) {
				g_free (*cls1);
				*cls1 = NULL;
				goto error;
			}

			*cls2 = aux->str;
			q++; // skip the end-of-clause separator
			g_string_free (aux, FALSE);
		}
	}

	*sz = out->len + (*cls1 != NULL ? strlen(*cls1) + 2 : 0) + (*cls2 != NULL ? strlen(*cls2) + 2 : 0);
	ret = NULL;
	if (out->len > 0) {
		out = g_string_append_c (out, '\0');
		ret = g_strdup (out->str);
	}
	g_string_free (out, TRUE);

	return ret;

error:
	g_string_free (out, TRUE);
	return NULL;
}

// Rules:
// @<id>             value of <id>. If no value, error
// &<id>             value of <id> if <id> is defined
// ?<id>s...s        text between s...s separators if <id> defined
// ?<id>s...ss...s   text between 1st s...s separators if <id> defined
// else 2nd s...s clause
// ~<id>s...s        text between s...s separators if <id> undefined
// ~<id>s...ss...s   text between 1st s...s separators if <id> undefined
// else 2nd s...s clause
// #<id>s...s        text between s...s separators if <id> defined, but
// delete rest of template if <id> undefined

// Separators can be any of {',', '.', ';', '/', '|', '(', ')'}.
// For an opening-closing pair of
// separators the same character has to be used.

// Examples:
// R^@refdes %1 %2 @value
// V^@refdes %+ %- SIN(@offset @ampl @freq 0 0)
// ?DC|DC @DC|
char *part_property_expand_macros (Part *part, char *string)
{
	static char mcode[] = {"@?~#&"};
	char *value;
	char *tmp0, *temp, *qn, *q0, *t0;
	char *cls1, *cls2;
	GString *out;
	size_t sln;
	char *ret;

	g_return_val_if_fail (part != NULL, NULL);
	g_return_val_if_fail (IS_PART (part), NULL);
	g_return_val_if_fail (string != NULL, NULL);

	cls1 = cls2 = q0 = NULL;

	tmp0 = temp = g_strdup (string);

	out = g_string_new ("");

	for (temp = string; *temp;) {
		// Look for any of the macro char codes.
		if (strchr (mcode, *temp)) {
			qn = get_macro_name (*temp, temp + 1, &cls1, &cls2, &sln);
			if (qn == NULL)
				return NULL;
			value = part_get_property (part, qn);
			if ((*temp == '@' || *temp == '&') && value) {
				out = g_string_append (out, value);
			} else if (*temp == '&' && !value) {
				g_warning ("expand macro error: macro %s undefined", qn);
				g_free (qn);
				return NULL;
			} else if (*temp == '?' || *temp == '~') {
				if (cls1 == NULL) {
					g_warning ("error in template: %s", temp);
					g_free (qn);
					return NULL;
				}
				q0 = (value ? (*temp == '?' ? cls1 : cls2) : (*temp == '?' ? cls2 : cls1));
				if (q0) {
					t0 = part_property_expand_macros (part, q0);
					if (!t0) {
						g_warning ("error in template: %s", temp);
						g_free (qn);
					} else {
						out = g_string_append (out, t0);
						g_free (t0);
					}
				}
			} else if (*temp == '#') {
				if (value) {
					t0 = part_property_expand_macros (part, value);
					if (!t0) {
						g_warning ("error in template: %s", temp);
						g_free (qn);
					} else {
						out = g_string_append (out, t0);
						g_free (t0);
					}
				} else
					*(temp + sln) = 0;
			}
			temp += 1;
			temp += sln;
			g_free (qn);
			g_free (cls1);
			g_free (cls2);
		} else {
			if (*temp == '\\') {
				temp++;
				switch (*temp) {
				case 'n':
					out = g_string_append_c (out, '\n');
					break;
				case 't':
					out = g_string_append_c (out, '\t');
					break;
				case 'r':
					out = g_string_append_c (out, '\r');
					break;
				case 'f':
					out = g_string_append_c (out, '\f');
				}
				temp++;
			} else {
				out = g_string_append_c (out, *temp);
				temp++;
			}
		}
	}

	g_free (tmp0);

	out = g_string_append_c (out, '\0');
	ret = g_strdup (out->str);
	g_string_free (out, TRUE);

	return ret;
}

/**
 * see #168
 */
void update_connection_designators(Part *part, char **prop, int *node_ctr)
{
	if (prop == NULL || *prop == NULL)
		return;
	if (node_ctr == NULL)
		return;
	if (part == NULL || !IS_PART(part))
		return;

	char *temp = *prop;
	GString *out = g_string_new ("");

	int breakout = FALSE;
	while (!breakout) {
		char **prop_split = g_regex_split_simple("[@?~#&%].*", temp, 0, 0);
		if (prop_split[0] == NULL) {
			g_strfreev(prop_split);
			break;
		}
		temp += strlen(prop_split[0]);
		g_string_append_printf(out, "%s", prop_split[0]);
		g_strfreev(prop_split);
		char macro = *temp;
		temp++;
		switch (macro) {
			case '%':
			{
				char **prop_split = g_regex_split_simple(" .*", temp, 0, 0);
				temp += strlen(prop_split[0]);
				g_string_append_printf(out, "%%%d", (*node_ctr)++);
				g_strfreev(prop_split);
				break;
			}
			case '@':
			{
				char **prop_split = g_regex_split_simple("[,.;/|() ].*", temp, 0, 0);
				temp += strlen(prop_split[0]);
				char *prop_ref_name = prop_split[0];
				char **prop_ref_value = part_get_property_ref(part, prop_ref_name);
				g_string_append_printf(out, "@%s", prop_ref_name);
				update_connection_designators(part, prop_ref_value, node_ctr);
				g_strfreev(prop_split);
				break;
			}
			case '&':
			{
				char **prop_split = g_regex_split_simple("[,.;/|() ].*", temp, 0, 0);
				temp += strlen(prop_split[0]);
				char *prop_ref_name = prop_split[0];
				char **prop_ref_value = part_get_property_ref(part, prop_ref_name);
				g_string_append_printf(out, "&%s", prop_ref_name);
				if (prop_ref_value != NULL && *prop_ref_value != NULL)
					update_connection_designators(part, prop_ref_value, node_ctr);
				g_strfreev(prop_split);
				break;
			}
			case '?':
			case '~':
			{
				char **prop_split = g_regex_split_simple("([,.;/|()])(.*?)(\\g{-3})(?(?=[,.;/|()])([,.;/|()])(.*?)(\\g{-3})).*", temp, 0, 0);
				char *prop_ref_name = g_strdup(prop_split[0]);
				char separator1 = *prop_split[1];
				char *cls1 = g_strdup(prop_split[2]);
				//separator1 == *prop_split_name[3]
				char separator2 = prop_split[4] != NULL ? *prop_split[4] : 0;
				char *cls2 = NULL;
				if (separator2 != 0) {
					cls2 = g_strdup(prop_split[5]);
				}
				char **prop_ref_value = part_get_property_ref(part, prop_ref_name);
				for (int i = 0; prop_split[i] != NULL; i++)
					temp += strlen(prop_split[i]);
				g_strfreev(prop_split);

				if	(
						(macro == '?' && prop_ref_value != NULL && *prop_ref_value != NULL)
						||
						(macro == '~' && (prop_ref_value == NULL || *prop_ref_value == NULL))
					)
					update_connection_designators(part, &cls1, node_ctr);
				else if (cls2 != NULL)
					update_connection_designators(part, &cls2, node_ctr);

				g_string_append_printf(out, "%c%s%c%s%c", macro, prop_ref_name, separator1, cls1, separator1);
				if (cls2 != NULL) {
					g_string_append_printf(out, "%c%s%c", separator2, cls2, separator2);
					g_free(cls2);
				}

				g_free(cls1);
				g_free(prop_ref_name);
				break;
			}
			case '#':
			{
				char **prop_split = g_regex_split_simple("([,.;/|()])(.*?)(\\g{-3}).*", temp, 0, 0);
				char *prop_ref_name = g_strdup(prop_split[0]);
				char separator = *prop_split[1];
				char *cls = g_strdup(prop_split[2]);
				//separator == *prop_split_name[3]
				char **prop_ref_value = part_get_property_ref(part, prop_ref_name);
				for (int i = 0; prop_split[i] != NULL; i++)
					temp += strlen(prop_split[i]);
				g_strfreev(prop_split);

				if (prop_ref_value != NULL && *prop_ref_value != NULL) {
					update_connection_designators(part, &cls, node_ctr);
					g_string_append_printf(out, "#%s%c%s%c", prop_ref_name, separator, cls, separator);
				} else {
					g_string_append_printf(out, "#%s%c%s%c%s", prop_ref_name, separator, cls, separator, temp);
					breakout = TRUE;
				}

				g_free(cls);
				g_free(prop_ref_name);
				break;
			}
			default:
			{
				breakout = TRUE;
				break;
			}
		}
	}
	g_free(*prop);
	*prop = out->str;
	g_string_free (out, FALSE);
	return;
}
