#ifndef OPTION_H__
#define OPTION_H__


#ifndef GETTEXT_PACKAGE
#define GETTEXT_PACKAGE "oregano"
#endif

#include <glib.h>

typedef struct {
	struct {
		gboolean wires;
	} debug;

} OreganoOptions;


extern OreganoOptions opts;

gboolean
oregano_options_parse (int *argc, char **argv[], GError **e);

#endif /* OPTION_H__ */
