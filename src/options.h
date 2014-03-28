#ifndef OPTION_H__
#define OPTION_H__


#ifndef GETTEXT_PACKAGE
#define GETTEXT_PACKAGE "oregano"
#endif

#include <glib.h>

typedef struct {
	struct {
		gboolean wires;
		gboolean boxes;
		gboolean dots;
		gboolean all;
	} debug;

} OreganoOptions;


gboolean
oregano_options_parse (int *argc, char **argv[], GError **e);

gboolean
oregano_options_debug_wires ();

gboolean
oregano_options_debug_boxes ();

gboolean
oregano_options_debug_dots ();

#endif /* OPTION_H__ */
