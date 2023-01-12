#ifndef __TEST_STACK_H
#define __TEST_STACK_H

#include "sheet-item.h"

extern historial_stack_t *test_undo_stack;
extern historial_stack_t *test_redo_stack;

unsigned long long test_stack_get_size (historial_stack_t *stack);
stack_item_t *test_stack_get_top (historial_stack_t *stack);
gboolean test_stack_push (historial_stack_t *stack, stack_data_t *data);
stack_data_t *test_stack_pop (historial_stack_t *stack);
#endif
