#ifndef __LOG_H__
#define __LOG_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define LOG_TYPE (log_get_type ())
#define LOG(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), LOG_TYPE, Log))
#define LOG_CONST(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), LOG_TYPE, Log const))
#define LOG_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), LOG_TYPE, LogClass))
#define LOG_IS(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), LOG_TYPE))
#define LOG_IS_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), LOG_TYPE))
#define LOG_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), LOG_TYPE, LogClass))

typedef struct _Log Log;
typedef struct _LogClass LogClass;
typedef struct _LogPrivate LogPrivate;

struct _Log
{
	GtkListStore parent;

	LogPrivate *priv;
};

struct _LogClass
{
	GtkListStoreClass parent_class;
};

GType log_get_type (void) G_GNUC_CONST;
Log *log_new (void);

void log_append (Log *log, const gchar *prefix, const gchar *message);

void log_append_error (Log *log, const gchar *prefix, const gchar *message, GError *error);

G_END_DECLS

#endif /* __LOG_H__ */
