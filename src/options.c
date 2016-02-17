#include "options.h"
#include <gtk/gtk.h> //for gtk_get_option_group

OreganoOptions opts = {
    .debug = {.wires = FALSE, .boxes = FALSE, .dots = FALSE, .directions = FALSE, .all = FALSE}};

GOptionEntry entries[] = {
    {"version", 0, 0, G_OPTION_ARG_NONE, &(opts.version),
     "Print the version and quit.", NULL},
    {"debug-wires", 0, 0, G_OPTION_ARG_NONE, &(opts.debug.wires),
     "Give them randomly alternating colors.", NULL},
    {"debug-boundingboxes", 0, 0, G_OPTION_ARG_NONE, &(opts.debug.boxes),
     "Draw them in semi transparent purple.", NULL},
    {"debug-dots", 0, 0, G_OPTION_ARG_NONE, &(opts.debug.dots),
     "Draw an extra color circle around dots which are always shown.", NULL},
    {"debug-directions", 0, 0, G_OPTION_ARG_NONE, &(opts.debug.directions),
     "Draw fancy direction arrows top left edge of the sheet.", NULL},
    {"debug-all", 0, 0, G_OPTION_ARG_NONE, &(opts.debug.all), "Enable all debug-* options.", NULL},
    {NULL}};

/**
 * parse the commandline options for gtk args and oregano recognized ones
 * results will be written to a global Options struct (opts)
 * @param argc pointer to argc from main
 * @param argv pointer to argv from main
 * @param e a GError ptr ptr which will be filled in case of an error
 */
gboolean oregano_options_parse (int *argc, char **argv[], GError **e)
{
	GError *error = NULL;
	gboolean r = FALSE;
	GOptionContext *context;
	context = g_option_context_new ("- electrical engineering tool");
	g_option_context_add_main_entries (context, entries, GETTEXT_PACKAGE);
	g_option_context_add_group (context, gtk_get_option_group (TRUE));
	r = g_option_context_parse (context, argc, argv, &error);
	if (error) {
		if (e)
			*e = g_error_copy (error);
		g_error_free (error);
		error = NULL;
	}
	g_option_context_free (context);
	return r;
}


inline gboolean oregano_options_version () { return opts.version; }

inline gboolean oregano_options_debug_wires () { return opts.debug.wires || opts.debug.all; }

inline gboolean oregano_options_debug_boxes () { return opts.debug.boxes || opts.debug.all; }

inline gboolean oregano_options_debug_dots () { return opts.debug.dots || opts.debug.all; }

inline gboolean oregano_options_debug_directions ()
{
	return opts.debug.directions || opts.debug.all;
}
