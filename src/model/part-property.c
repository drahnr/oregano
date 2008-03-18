/*
 * part-property.c
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
#include <glib.h>
#include <string.h>
#include "part.h"
#include "part-property.h"

/**
 * Gets the name of a macro variable.
 *
 * @param str  str
 * @param cls1 returns first conditional clause
 * @param cls2 returns second clause
 * @param sz   returns number of characters parsed
 * @return the name of a macro variable
 */
static char *get_macro_name (const char *str, char **cls1,
	char **cls2, size_t *sz)
{
	char separators[] = { ",.;/|()" };
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

	/* Get the name */
	for (q = str; (*q) && (*q != ' ') && !(csep = strchr (separators, *q)); q++) {
		if ( q > qend ) {
			g_warning ("Expand macro error.");
			rc = 1;
			break;
		}
		out = g_string_append_c (out, *q);
	}

	/* if error found, return here */
	if (rc)
		goto error;

	/* Look for conditional clauses */
	if (csep) {
		/* get the first one */
		GString *aux;
		q++; /* skip the separator and store the clause in tmp */
		aux = g_string_new ("");
		for (; (*q) && (*q != *csep); q++)
			g_string_append_c (aux, *q);

		if (!*q) {
			g_string_free (aux, TRUE);
			goto error;
		}

		*cls1 = aux->str;
		q++; /* skip the end-of-clause separator */
		g_string_free (aux, FALSE);

		/* Check for the second one */
		if ( (*q) && (csep = strchr (separators, *q))) {
			q++; /* skip the separator and store in tmp*/
			aux = g_string_new ("");
			for (; (*q) && (*q != *csep); q++)
				g_string_append_c (aux, *q);

			if (!(*q)) {
				g_free (*cls1);
				*cls1 = NULL;
				goto error;
			}

			*cls2 = aux->str;
			q++; /* skip the end-of-clause separator */
			g_string_free (aux, FALSE);
		}
	}

	*sz = out->len;
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

char *
part_property_expand_macros (Part *part, char *string)
{
	static char mcode[] = {"@?~#&"};
	char *value;
	char *tmp0, *temp, *qn, *q0, *qend, *t0;
	char *cls1, *cls2;
	GString *out;
	size_t sln;
	char *ret;

	g_return_val_if_fail (part != NULL, NULL);
	g_return_val_if_fail (IS_PART (part), NULL);
	g_return_val_if_fail (string != NULL, NULL);

	cls1 = cls2 = q0 = NULL;
	/* Rules:
	   @<id>             value of <id>. If no value, error
	   &<id>             value of <id> if <id> is defined
	   ?<id>s...s        text between s...s separators if <id> defined
	   ?<id>s...ss...s   text between 1st s...s separators if <id> defined
	   else 2nd s...s clause
	   ~<id>s...s        text between s...s separators if <id> undefined
	   ~<id>s...ss...s   text between 1st s...s separators if <id> undefined
	   else 2nd s...s clause
	   #<id>s...s        text between s...s separators if <id> defined, but
	   delete rest of tempalte if <id> undefined

	   Separatos can be any of (, . ; / |) For an opening-closing pair of
	   separators the same character ahs to be used.

	   Examples: R^@refdes %1 %2 @value
	   V^@refdes %+ %- SIN(@offset @ampl @freq 0 0)
	   ?DC|DC @DC|
	*/
	tmp0 = temp = g_strdup (string);
	qend = temp + strlen (temp);

	out = g_string_new ("");

	for (temp = string; *temp;) {
		/*
		 * Look for any of the macro char codes.
		 */
		if (strchr (mcode, *temp)) {
			qn = get_macro_name (temp + 1, &cls1, &cls2, &sln);
			if (qn == NULL)
				return NULL;
			value = part_get_property (part, qn);
			if ((*temp == '@' || *temp == '&') && value) {
				out = g_string_append (out, value);
			} else if (*temp =='&' && !value) {
				g_warning ( "expand macro error: macro %s undefined", qn);
				g_free (qn);
				return NULL;
			} else if (*temp == '?' || *temp == '~') {
				if (cls1 == NULL) {
					g_warning ("error in template: %s", temp);
					g_free (qn);
					return NULL;
				}
				q0 = (value
					  ? (*temp == '?' ? cls1 : cls2)
					  : (*temp == '?' ? cls2 : cls1)
					);
				if (q0) {
					t0 = part_property_expand_macros (part, q0);
					if (!t0) {
						g_warning ( "error in template: %s", temp);
						g_free (qn);
					} else {
						out = g_string_append (out, t0);
						g_free (t0);
					}
				}
			} else if (*temp=='#') {
				if (value) {
					t0 = part_property_expand_macros (part, value);
					if (!t0) {
						g_warning ( "error in template: %s", temp);
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
			if (qn) g_free (qn);
			if (cls1) g_free (cls1);
			if (cls2) g_free (cls2);
		} else {
			if ( *temp== '\\' ) {
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

	if (tmp0) g_free (tmp0);

	out = g_string_append_c (out, '\0');
	ret = g_strdup (out->str);
	g_string_free (out, TRUE);

	return ret;
}


#if 0
//---------------------
char *
part_property_expand_macros (Part *part, char *string)
{
	char *name;
	char *value;
	char *temp;
	char buffer[512];
	gint in_index = 0, out_index = 0;

	g_return_val_if_fail (part != NULL, NULL);
	g_return_val_if_fail (IS_PART (part), NULL);
	g_return_val_if_fail (string != NULL, NULL);

	/* Examples: R^@refdes %1 %2 @value
	 *           V^@refdes %+ %- SIN(@offset @ampl @freq 0 0)
	 */

	temp = string;
	in_index = 0;
	out_index = 0;

	while (*temp != 0) {
		/* Is it a macro? */
		if (temp[0] == '@'){
			int i = 0;
			/* Find the end of the macro. */
			while (1){
				if (temp[i] == ' ' || temp[i] == '(' ||
					temp[i] == ')' || temp[i] == 0)
					break;
				else {
					i++;
					if (i > strlen (string)){
						g_warning (
							"expand macro error.");
						break;
					}
				}
			}

			/* Perform a lookup on the macro. */
			name = g_strndup (temp + 1, i - 1);
			value = part_get_property (part, name);
			g_free (name);

			if (value) {
				snprintf (buffer + out_index, 16,
					"%s ", value);
				out_index += strlen (value);
				in_index += i + 1;
			}
			temp += i;
		} else{
			buffer[out_index] = *temp;
			out_index++;
			in_index++;
			temp++;
		}
	}

	buffer[out_index] = '\0';
	return g_strdup (buffer);
}
#endif
