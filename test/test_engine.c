#ifndef TEST_ENGINE
#define TEST_ENGINE

#include <glib.h>

void
test_engine ()
{
	extern gchar** get_variables (const gchar *str, gint *count);

	const gchar *test[] = {
	                 "   foo  bar\twhat \t the\t heck \tis    this",
	                 "foo bar\twhat \t the\t heck \tis    this\t  ",
	                 "  \tfoo  bar\twhat \t the\t heck \tis    this\t  ",
	                 "\t  foo  bar   what \t the\t heck \tis this   \t",
	                 "\n  foo  bar   what \n the\n heck \nis\t this   \n",
	                 "  \nfoo  bar\t\twhat \t\n\t the\n heck \nis\t this\n\n\n",
	                 NULL
	};
	const gchar *const expected[] = {"foo","bar","what","the","heck","is","this"};
	gint i,j, k;

	for (i=0; test[i]; i++) {
		gchar **v = get_variables (test[i], &k);
		g_assert_cmpint (k,==,7);
		for (j=0; j<k; j++) {
			g_assert_cmpstr (v[j], ==, expected[j]);
		}
		g_strfreev (v);
	}
}

#endif
