/*
 * test_update_schematic.c
 *
 *  Created on: May 19, 2017
 *      Author: michi
 */


#ifndef TEST_UPDATE_SCHEMATIC
#define TEST_UPDATE_SCHEMATIC

#include <glib.h>

#include "../src/model/part-property.h"
#include "../src/model/part.h"
#include "../src/model/part-private.h"

void test_basic();
void test_AT();
void test_AMPERSAND_TRUE();
void test_AMPERSAND_FALSE();
void test_QUESTION_MARK_TRUE();
void test_QUESTION_MARK_FALSE();
void test_TILDE_TRUE();
void test_TILDE_FALSE();
void test_HASHTAG_TRUE();
void test_HASHTAG_FALSE();
void doit(char *names[], char *test_values[], char *expected_values[]);

void
test_update_connection_designators()
{
	test_basic();
	test_AT();
	test_AMPERSAND_TRUE();
	test_AMPERSAND_FALSE();
	test_QUESTION_MARK_TRUE();
	test_QUESTION_MARK_FALSE();
	test_TILDE_TRUE();
	test_TILDE_FALSE();
	test_HASHTAG_TRUE();
	test_HASHTAG_FALSE();
}

void test_basic() {
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
	doit(names, test_values, expected_values);
}

void test_AT() {
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
	doit(names, test_values, expected_values);
}

void test_AMPERSAND_TRUE() {
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
	doit(names, test_values, expected_values);
}

void test_AMPERSAND_FALSE() {
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
	doit(names, test_values, expected_values);
}

void test_QUESTION_MARK_TRUE() {
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
			"@refdes %qtg %sd SIN(@offset @freq 0 0) ?DC|DC @DC %agoi |(ampl @ampl %agoi (",
			NULL };
	char *expected_values[] = {
			"V1",
			"5",
			"5555 %34 asdf",
			"555",
			"5555 %3 asdf",
			"@refdes %1 %2 SIN(@offset @freq 0 0) ?DC|DC @DC %4 |(ampl @ampl %agoi (",
			NULL };
	doit(names, test_values, expected_values);
}

void test_QUESTION_MARK_FALSE() {
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
			"@refdes %qtg %sd SIN(@offset @freq 0 0) ?DC|DC @DC %agoi |(ampl @ampl %agoi ( %-gh ",
			NULL };
	char *expected_values[] = {
			"V1",
			"5",
			"5555 %3 asdf",
			"555",
			"@refdes %1 %2 SIN(@offset @freq 0 0) ?DC|DC @DC %agoi |(ampl @ampl %4 ( %5 ",
			NULL };
	doit(names, test_values, expected_values);
}

void test_TILDE_TRUE() {
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
			"55",
			"5555 %34 asdf",
			"@refdes %qtg %sd SIN(@offset @freq 0 0) ~DC|ampl %lkj @ampl|(DC %lkj @DC(",
			NULL };
	char *expected_values[] = {
			"V1",
			"5",
			"5555 %34 asdf",
			"55",
			"5555 %4 asdf",
			"@refdes %1 %2 SIN(@offset @freq 0 0) ~DC|ampl %lkj @ampl|(DC %3 @DC(",
			NULL };
	doit(names, test_values, expected_values);
}

void test_TILDE_FALSE() {
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
			"@refdes %qtg %sd SIN(@offset @freq 0 0) ~DC|ampl %lkj @ampl|(DC %lkj @DC( %-gh ",
			NULL };
	char *expected_values[] = {
			"V1",
			"5",
			"5555 %4 asdf",
			"555",
			"@refdes %1 %2 SIN(@offset @freq 0 0) ~DC|ampl %3 @ampl|(DC %lkj @DC( %5 ",
			NULL };
	doit(names, test_values, expected_values);
}

void test_HASHTAG_TRUE() {
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
	doit(names, test_values, expected_values);
}

void test_HASHTAG_FALSE() {
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
	doit(names, test_values, expected_values);
}

void doit(char *names[], char *test_values[], char *expected_values[]) {
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
