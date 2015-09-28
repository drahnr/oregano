#include <gtk/gtk.h>
#include "icons.h"

void oregano_icon_theme_changed (GtkIconTheme *iconteme, gpointer user_data) {}

void oregano_icon_init ()
{
	GtkIconTheme *icon_theme = gtk_icon_theme_get_default ();
	g_assert (icon_theme != NULL);
	gtk_icon_theme_append_search_path (icon_theme, OREGANO_ICONDIR);
	g_signal_connect (icon_theme, "changed", G_CALLBACK (oregano_icon_theme_changed), NULL);
}


GdkPixbuf *oregano_get_icon (const gchar *icon_name)
{
	GtkIconTheme *icon_theme = gtk_icon_theme_get_default ();
	g_assert (icon_theme != NULL);
	GError *e = NULL;

	GtkIconInfo *icon_info = gtk_icon_theme_lookup_icon (icon_theme, icon_name, 64, 0);
	g_assert (icon_info);

	GdkPixbuf *pixbuf = gtk_icon_info_load_icon (icon_info, &e);
	if (e) {
		g_message ("Failed to get %s %s %i", icon_name, e->message, e->code);
		g_clear_error (&e);
		return NULL;
	}
	return pixbuf;
}
