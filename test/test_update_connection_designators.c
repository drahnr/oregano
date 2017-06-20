/*
 * test_update_schematic.c
 *
 *
 * Authors:
 *  Michi <st101564@stud.uni-stuttgart.de>
 *
 * Web page: https://ahoi.io/project/oregano
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
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef TEST_UPDATE_CONNECTION_DESIGNATORS
#define TEST_UPDATE_CONNECTION_DESIGNATORS

#include <glib.h>

#include "../src/model/part-property.h"
#include "../src/model/part.h"
#include "../src/model/part-private.h"

void test_update_connection_designators_basic();
void test_update_connection_designators_AT();
void test_update_connection_designators_AMPERSAND_TRUE();
void test_update_connection_designators_AMPERSAND_FALSE();
void test_update_connection_designators_QUESTION_MARK_TRUE();
void test_update_connection_designators_QUESTION_MARK_FALSE();
void test_update_connection_designators_TILDE_TRUE();
void test_update_connection_designators_TILDE_FALSE();
void test_update_connection_designators_HASHTAG_TRUE();
void test_update_connection_designators_HASHTAG_FALSE();
void doit_update_connection_designators(char *names[], char *test_values[], char *expected_values[]);

void
add_funcs_test_update_connection_designators()
{
	g_test_add_func ("/core/model/part-property/update_connection_designators/basic", test_update_connection_designators_basic);
	g_test_add_func ("/core/model/part-property/update_connection_designators/AT", test_update_connection_designators_AT);
	g_test_add_func ("/core/model/part-property/update_connection_designators/AMPERSAND_TRUE", test_update_connection_designators_AMPERSAND_TRUE);
	g_test_add_func ("/core/model/part-property/update_connection_designators/AMPERSAND_FALSE", test_update_connection_designators_AMPERSAND_FALSE);
	g_test_add_func ("/core/model/part-property/update_connection_designators/QUESTION_MARK_TRUE", test_update_connection_designators_QUESTION_MARK_TRUE);
	g_test_add_func ("/core/model/part-property/update_connection_designators/QUESTION_MARK_FALSE", test_update_connection_designators_QUESTION_MARK_FALSE);
	g_test_add_func ("/core/model/part-property/update_connection_designators/TILDE_TRUE", test_update_connection_designators_TILDE_TRUE);
	g_test_add_func ("/core/model/part-property/update_connection_designators/TILDE_FALSE", test_update_connection_designators_TILDE_FALSE);
	g_test_add_func ("/core/model/part-property/update_connection_designators/HASHTAG_TRUE", test_update_connection_designators_HASHTAG_TRUE);
	g_test_add_func ("/core/model/part-property/update_connection_designators/HASHTAG_FALSE", test_update_connection_designators_HASHTAG_FALSE);
}

void test_update_connection_designators_basic() {
	char *names[] = {
			"Refdes",
			"Res",
			"Template",
			NULL };
	char *test_values[] = {
			"R1",
			"1k",
			"@refdes %asdf %asdf @res",
			NULL };
	char *expected_values[] = {
			"R1",
			"1k",
			"@refdes %1 %2 @res",
			NULL };
	doit_update_connection_designators(names, test_values, expected_values);
}

void test_update_connection_designators_AT() {
	char *names[] = {
			"Refdes",
			"cur",
			"Template",
			NULL };
	char *test_values[] = {
			"G1",
			"'0.001*(V( %+i )-V( %-i ))'",
			"@refdes %a %b cur=@cur",
			NULL };
	char *expected_values[] = {
			"G1",
			"'0.001*(V( %3 )-V( %4 ))'",
			"@refdes %1 %2 cur=@cur",
			NULL };
	doit_update_connection_designators(names, test_values, expected_values);
}

void test_update_connection_designators_AMPERSAND_TRUE() {
	char *names[] = {
			"Refdes",
			"mname",
			"Template",
			NULL };
	char *test_values[] = {
			"RMOD",
			"RMODEL %adsf jkloe",
			"@refdes %a %b &mname %asdf ",
			NULL };
	char *expected_values[] = {
			"RMOD",
			"RMODEL %3 jkloe",
			"@refdes %1 %2 &mname %4 ",
			NULL };
	doit_update_connection_designators(names, test_values, expected_values);
}

void test_update_connection_designators_AMPERSAND_FALSE() {
	char *names[] = {
			"Refdes",
			"Template",
			NULL };
	char *test_values[] = {
			"RMOD",
			"@refdes %a %b &mname %asdf ",
			NULL };
	char *expected_values[] = {
			"RMOD",
			"@refdes %1 %2 &mname %3 ",
			NULL };
	doit_update_connection_designators(names, test_values, expected_values);
}

void test_update_connection_designators_QUESTION_MARK_TRUE() {
	char *names[] = {
			"Refdes",
			"offset",
			"ampl",
			"freq",
			"DC",
			"Template",
			NULL };
	char *test_values[] = {
			"V1",
			"5",
			"5555 %34 asdf",
			"555",
			"5555 %34 asdf",
			"@refdes %qtg %sd SIN(@offset 0 0) ?DC|DC @DC %agoi |(ampl @ampl %agoi ( ?DC|freq @freq %agoi |",
			NULL };
	char *expected_values[] = {
			"V1",
			"5",
			"5555 %34 asdf",
			"555",
			"5555 %3 asdf",
			"@refdes %1 %2 SIN(@offset 0 0) ?DC|DC @DC %4 |(ampl @ampl %agoi ( ?DC|freq @freq %5 |",
			NULL };
	doit_update_connection_designators(names, test_values, expected_values);
}

void test_update_connection_designators_QUESTION_MARK_FALSE() {
	char *names[] = {
			"Refdes",
			"offset",
			"ampl",
			"freq",
			"Template",
			NULL };
	char *test_values[] = {
			"V1",
			"5",
			"5555 %34 asdf",
			"555",
			"@refdes %qtg %sd SIN(@offset 0 0) ?DC|DC @DC %agoi |(ampl @ampl %agoi ( %-gh ?DC|freq @freq %agoi | %-gh ",
			NULL };
	char *expected_values[] = {
			"V1",
			"5",
			"5555 %3 asdf",
			"555",
			"@refdes %1 %2 SIN(@offset 0 0) ?DC|DC @DC %agoi |(ampl @ampl %4 ( %5 ?DC|freq @freq %agoi | %6 ",
			NULL };
	doit_update_connection_designators(names, test_values, expected_values);
}

void test_update_connection_designators_TILDE_TRUE() {
	char *names[] = {
			"Refdes",
			"offset",
			"ampl",
			"freq",
			"DC",
			"Template",
			NULL };
	char *test_values[] = {
			"V1",
			"5",
			"5555 %34 asdf",
			"5555 %34 freq",
			"5555 %34 asdf",
			"@refdes %qtg %sd SIN(@offset 0 0) ~DC|ampl %lkj @ampl|(DC %lkj @DC( ~DC|freq %lkj @freq|",
			NULL };
	char *expected_values[] = {
			"V1",
			"5",
			"5555 %34 asdf",
			"5555 %34 freq",
			"5555 %4 asdf",
			"@refdes %1 %2 SIN(@offset 0 0) ~DC|ampl %lkj @ampl|(DC %3 @DC( ~DC|freq %lkj @freq|",
			NULL };
	doit_update_connection_designators(names, test_values, expected_values);
}

void test_update_connection_designators_TILDE_FALSE() {
	char *names[] = {
			"Refdes",
			"offset",
			"ampl",
			"freq",
			"Template",
			NULL };
	char *test_values[] = {
			"V1",
			"5",
			"5555 %34 asdf",
			"5555 %34 freq",
			"@refdes %qtg %sd SIN(@offset 0 0) ~DC|freq %lkj @freq| ~DC|ampl %lkj @ampl|(DC %lkj @DC( %-gh ",
			NULL };
	char *expected_values[] = {
			"V1",
			"5",
			"5555 %6 asdf",
			"5555 %4 freq",
			"@refdes %1 %2 SIN(@offset 0 0) ~DC|freq %3 @freq| ~DC|ampl %5 @ampl|(DC %lkj @DC( %7 ",
			NULL };
	doit_update_connection_designators(names, test_values, expected_values);
}

void test_update_connection_designators_HASHTAG_TRUE() {
	char *names[] = {
			"Refdes",
			"offset",
			"ampl",
			"freq",
			"DC",
			"Template",
			NULL };
	char *test_values[] = {
			"V1",
			"5",
			"55",
			"555",
			"5555 %34 asdf",
			"@refdes %qtg %sd SIN(@offset @ampl @freq 0 0) #DC|DC %wfl @DC %wfl | aslkhgl %jkl ",
			NULL };
	char *expected_values[] = {
			"V1",
			"5",
			"55",
			"555",
			"5555 %4 asdf",
			"@refdes %1 %2 SIN(@offset @ampl @freq 0 0) #DC|DC %3 @DC %5 | aslkhgl %6 ",
			NULL };
	doit_update_connection_designators(names, test_values, expected_values);
}

void test_update_connection_designators_HASHTAG_FALSE() {
	char *names[] = {
			"Refdes",
			"offset",
			"ampl",
			"freq",
			"Template",
			NULL };
	char *test_values[] = {
			"RMOD",
			"5",
			"5555 %34 asdf",
			"555",
			"@refdes %qtg %sd SIN(@offset @freq 0 0) #DC|DC %jkl @ampl %wfl | %-gh jkl",
			NULL };
	char *expected_values[] = {
			"RMOD",
			"5",
			"5555 %34 asdf",
			"555",
			"@refdes %1 %2 SIN(@offset @freq 0 0) #DC|DC %jkl @ampl %wfl | %-gh jkl",
			NULL };
	doit_update_connection_designators(names, test_values, expected_values);
}

//this function does not free the allocated memory
//do not use this function in productive code
void doit_update_connection_designators(char *names[], char *test_values[], char *expected_values[]) {
	Part *test_part = part_new();

	Property *prop;

	for (int i = 0; names[i] != NULL; i++) {
		prop = g_new0 (Property, 1);
		prop->name = g_strdup(names[i]);
		prop->value = g_strdup(test_values[i]);
		test_part->priv->properties = g_slist_prepend(test_part->priv->properties, prop);
	}

	int node_ctr = 1;
	update_connection_designators(test_part, part_get_property_ref(test_part, "template"), &node_ctr);
	for (int i = 0; names[i] != NULL; i++)
		g_assert_cmpstr(part_get_property(test_part, names[i]), ==, expected_values[i]);
}

#endif
