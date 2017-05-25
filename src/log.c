#include <string.h>
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
 * trim only newlines
 */
gchar *log_append_trim_message(const gchar *string) {
	if (string == NULL)
		return NULL;
	gchar **array = g_regex_split_simple("[\\r\\n]*(.*?)[\\n$]+", string, 0, 0);
	gchar *ret_val = g_strdup(array[0]);
	g_strfreev(array);
	if (*ret_val == 0) {
		g_free(ret_val);
		ret_val = NULL;
	}
	return ret_val;
}

/**
 * Show on top of the items in the logview.
 *
 * If the previous message had an '\r' at the end, the previous message will be
 * replaced by the new message instead of prepending the new message.
 *
 * Leading and trailing '\n's will be truncated.
 */
void log_append (Log *log, const gchar *prefix, const gchar *message)
{
	//trim message (only newlines), because newlines do not make sense in the log view
	gchar *message_trimed = log_append_trim_message(message);
	if (message_trimed == NULL)
		return;

	GtkTreeIter iter;
	//if the previous message had an '\r' at the end, the previous message will be
	//replaced by the new message instead of prepending the new message
	if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(log), &iter)) {
		GValue previous_value;
		previous_value.g_type = 0;
		gtk_tree_model_get_value(GTK_TREE_MODEL(log), &iter, 1, &previous_value);
		const gchar *previous_message = g_value_get_string(&previous_value);

		if (previous_message != NULL && previous_message[strlen(previous_message) - 1] != '\r')
			gtk_list_store_prepend (GTK_LIST_STORE (log), &iter);
		g_value_unset(&previous_value);
	} else {
		gtk_list_store_prepend (GTK_LIST_STORE (log), &iter);
	}
	gtk_list_store_set (GTK_LIST_STORE (log), &iter, 0, g_strdup (prefix), 1, g_strdup (message_trimed),
                        -1);
	g_free(message_trimed);
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
}

void log_clear (Log *log) { gtk_list_store_clear (GTK_LIST_STORE (log)); }

void log_export (Log *log, const gchar *path)
{
	// FIXME TODO
}
