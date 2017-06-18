#include <glib.h>

// may be already defined, if not:
#ifndef DEBUG_FORCE_FAIL
#define DEBUG_FORCE_FAIL 0
#endif

#include "helper.c"
#include "test_wire.c"
#include "test_engine.c"
#include "test_nodestore.c"
#include "test_update_connection_designators.c"
#include "test_thread_pipe.c"
#include "test_engine_ngspice.c"

#if DEBUG_FORCE_FAIL
void
test_false ()
{
	g_assert (FALSE==TRUE);
}
#endif

int
main (int argc, char *argv[])
{
#if !(GLIB_CHECK_VERSION(2,36,0))
	g_type_init();
#endif

	g_test_init (&argc, &argv, NULL);

	g_test_add_func ("/core/coords", test_coords);
	g_test_add_func ("/core/model/wire/intersection", test_wire_intersection);
	g_test_add_func ("/core/model/wire/tcrossing", test_wire_tcrossing);
	g_test_add_func ("/core/model/nodestore", test_nodestore);
	g_test_add_func ("/core/engine", test_engine);
	add_funcs_test_update_connection_designators();
	add_funcs_test_thread_pipe_buffered();
	add_funcs_test_engine_ngspice();
#if DEBUG_FORCE_FAIL
	g_test_add_func ("/false", test_false);
#endif
	return g_test_run ();
}
