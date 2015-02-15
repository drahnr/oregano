#include "log-view.h"

#define LOG_VIEW_GET_PRIVATE(object)                                                               \
	(G_TYPE_INSTANCE_GET_PRIVATE ((object), TYPE_LOG_VIEW, LogViewPrivate))

struct _LogViewPrivate
{
	char x;
};

G_DEFINE_TYPE (LogView, log_view, GTK_TYPE_TREE_VIEW);

static void log_view_finalize (GObject *object)
{
	G_OBJECT_CLASS (log_view_parent_class)->finalize (object);
}

static void log_view_class_init (LogViewClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = log_view_finalize;

	g_type_class_add_private (object_class, sizeof(LogViewPrivate));
}

static void log_view_init (LogView *self) { self->priv = LOG_VIEW_GET_PRIVATE (self); }

LogView *log_view_new ()
{
	LogView *self = g_object_new (TYPE_LOG_VIEW, NULL);
	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (self), TRUE);

	GtkTreeViewColumn *col1, *col2;
	GtkCellRenderer *renderer = gtk_cell_renderer_text_new ();

	g_object_ref (renderer);

	col1 = gtk_tree_view_column_new_with_attributes ("Source", renderer, "text", 0, NULL);

	col2 = gtk_tree_view_column_new_with_attributes ("Message", renderer, "text", 1, NULL);

	gtk_tree_view_insert_column (GTK_TREE_VIEW (self), col1, 0);
	gtk_tree_view_insert_column (GTK_TREE_VIEW (self), col2, 1);

	return self;
}

void log_view_set_store (LogView *lv, Log *ls)
{
	gtk_tree_view_set_model (GTK_TREE_VIEW (lv), GTK_TREE_MODEL (ls));
}
