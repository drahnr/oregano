#ifndef __LOG_VIEW_H__
#define __LOG_VIEW_H__

#include <glib-object.h>
#include <gtk/gtk.h>

#include "log.h"

G_BEGIN_DECLS

#define TYPE_LOG_VIEW (log_view_get_type ())
#define LOG_VIEW(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), TYPE_LOG_VIEW, LogView))
#define LOG_VIEW_CONST(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), TYPE_LOG_VIEW, LogView const))
#define LOG_VIEW_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), TYPE_LOG_VIEW, LogViewClass))
#define LOG_IS_VIEW(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TYPE_LOG_VIEW))
#define LOG_IS_VIEW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), TYPE_LOG_VIEW))
#define LOG_VIEW_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), TYPE_LOG_VIEW, LogViewClass))

typedef struct _LogView LogView;
typedef struct _LogViewClass LogViewClass;
typedef struct _LogViewPrivate LogViewPrivate;

struct _LogView
{
	GtkTreeView parent;

	LogViewPrivate *priv;
};

struct _LogViewClass
{
	GtkTreeViewClass parent_class;
};

GType log_view_get_type (void) G_GNUC_CONST;
LogView *log_view_new (void);

void log_view_set_store (LogView *lv, Log *ls);

G_END_DECLS

#endif /* __LOG_VIEW_H__ */
