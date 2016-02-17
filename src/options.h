#ifndef OPTION_H__
#define OPTION_H__

#ifndef GETTEXT_PACKAGE
#define GETTEXT_PACKAGE "oregano"
#endif

#include <glib.h>

typedef struct
{
	gboolean version;
	struct
	{
		gboolean wires;
		gboolean boxes;
		gboolean dots;
		gboolean directions;
		gboolean all;
	} debug;

} OreganoOptions;

gboolean oregano_options_parse (int *argc, char **argv[], GError **e);

gboolean oregano_options_version ();

gboolean oregano_options_debug_wires ();

gboolean oregano_options_debug_boxes ();

gboolean oregano_options_debug_dots ();

gboolean oregano_options_debug_directions ();

#endif /* OPTION_H__ */
