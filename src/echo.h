#ifndef _OREGANO_ECHO_H_
#define _OREGANO_ECHO_H_

#include <glib.h>
#include <glib/gprintf.h>

void oregano_echo_static (const char *message);

void oregano_echo (const char *format, ...);

#endif /* _OREGANO_ECHO_H_ */

