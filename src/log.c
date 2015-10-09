#include "log.h"

#define LOG_GET_PRIVATE(object) (G_TYPE_INSTANCE_GET_PRIVATE ((object), LOG_TYPE, LogPrivate))

struct _LogPrivate
{
	char x;
};

G_DEFINE_TYPE (Log, log, GTK_TYPE_LIST_STORE);

static void log_finalize (GObject *object) { G_OBJECT_CLASS (log_parent_class)->finalize (object); }

static void log_class_init (LogClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = log_finalize;

	g_type_class_add_private (object_class, sizeof(LogPrivate));
}

static void log_init (Log *self)
{
	static GType v[] = {G_TYPE_STRING, G_TYPE_STRING};
	self->priv = LOG_GET_PRIVATE (self);
	gtk_list_store_set_column_types (GTK_LIST_STORE (self), 2, v);
}

Log *log_new () { return g_object_new (LOG_TYPE, NULL); }

/**
 * show on top of the items in the logview but
 * if exported at the end of the exported debug output
 */
void log_append (Log *log, const gchar *prefix, const gchar *format, ...)
{
	g_assert (format);

	va_list args;
	va_start (args, format);
	const char *tmp = g_strdup_vprintf (format, args);
	va_end (args);
	oregano_echo_static (tmp);

	GtkTreeIter item;
	gtk_list_store_insert (GTK_LIST_STORE (log), &item, 0);
	gtk_list_store_set (GTK_LIST_STORE (log), &item, 0, g_strdup (prefix), 1, tmp, -1);
	// do not free tmp!, the liststore takes over ownership
}

void log_append_error (Log *log, const gchar *prefix, const gchar *message, GError *error)
{
	if (error == NULL) {
		return log_append (log, prefix, message);
	}
	gchar *concat = NULL;

	if (message) {
		concat = g_strdup_printf ("%s\n%s - %i", message, error->message, error->code);
	} else {
		concat = g_strdup_printf ("\n%s - %i", error->message, error->code);
	}

	GtkTreeIter item;
	gtk_list_store_insert (GTK_LIST_STORE (log), &item, 0);
	gtk_list_store_set (GTK_LIST_STORE (log), &item, 0, g_strdup (prefix), 1, concat, -1);
	// do not free tmp!, the liststore takes over ownership
}

void log_clear (Log *log) { gtk_list_store_clear (GTK_LIST_STORE (log)); }

void log_export (Log *log, const gchar *path)
{
	// FIXME TODO
}
