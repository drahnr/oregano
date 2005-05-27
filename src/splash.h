
#ifndef _OREGANO_SPLASH_H_
#define _OREGANO_SPLASH_H_

#include <gtk/gtk.h>

typedef struct _Splash Splash;

struct _Splash {
	GtkWindow *win;
	GtkWidget *progress;
	GtkWidget *event;
	GtkLabel  *lbl;
	gboolean can_destroy;
};

Splash *oregano_splash_new ();
gboolean oregano_splash_free (Splash *);
void oregano_splash_step (Splash *, char *s);
void oregano_splash_done (Splash *, char *s);

#endif //_OREGANO_SPLASH_H_
