#ifndef TEST_STACK
#define TEST_STACK

#include <stdio.h>
#include <stdlib.h>
#include "test_stack.h"

historial_stack_t *test_undo_stack = NULL;
historial_stack_t *test_redo_stack = NULL;

unsigned long long test_stack_get_size (historial_stack_t *stack)
{
	return stack->size;
}

stack_item_t *test_stack_get_top (historial_stack_t *stack)
{
	if (stack->size)
		return stack->top;
	else
		return NULL;
}

gboolean test_stack_push (historial_stack_t *stack, stack_data_t *data)
{
	stack_item_t *top = NULL;

	if (stack && data) {
		top = (stack_item_t *) calloc (1, sizeof (stack_item_t));
		if (!top)
			return FALSE;

		if (!stack->size) {
			stack->top = top;
			stack->top->prev = NULL;
		} else {
			top->prev =stack->top;
			stack->top = top;
		}

		stack->top->data = (stack_data_t *) calloc (1, sizeof (stack_data_t));
		if (!stack->top->data)
			return FALSE;

		stack->top->data = memcpy (stack->top->data, data, sizeof (stack_data_t));
		stack->size++;
		return TRUE;
	} else {
		return FALSE;
	}
}

stack_data_t *test_stack_pop (historial_stack_t *stack)
{
	stack_data_t *ret = NULL;

	if (stack->top) {
		ret = (stack_data_t *) calloc (1, sizeof (stack_data_t));
		if (!ret)
			return NULL;

		ret = memcpy (ret, stack->top->data, sizeof (stack_data_t));
		free (stack->top->data);
		stack->top->data = NULL;
		stack->top =stack->top->prev;
		stack->size--;
		return ret;
	} else {
		return NULL;
	}
}

void test_stack (void)
{
	int i;
	gboolean ret = FALSE;
	stack_data_t data, *rdata = NULL;

	test_undo_stack = (historial_stack_t *) calloc (1, sizeof (historial_stack_t));
	if (!test_undo_stack)
		return;

	test_redo_stack = (historial_stack_t *) calloc (1, sizeof (historial_stack_t));
	if (!test_redo_stack)
		return;

	for (i = 0; i < 10; i++) {
		ret = test_stack_push (test_undo_stack, &data);
		if (ret)
			fprintf (stderr, "Pushing item %d onto the UNDO-STACK\n", i);
	}

	fprintf (stderr, "\n");

	i = test_stack_get_size (test_undo_stack) - 1;
	while (1) {
		rdata = test_stack_pop (test_undo_stack);
		if (!rdata)
			break;

		fprintf (stderr, "Popping item %d from the UNDO-STACK\n", i);
		ret = test_stack_push (test_redo_stack, rdata);
		if (ret)
			fprintf (stderr, "Pushing item %d onto the REDO-STACK\n", i);

		i--;
	};

	fprintf (stderr, "\n");

	i = test_stack_get_size (test_redo_stack) - 1;
	while (1) {
		rdata = test_stack_pop (test_redo_stack);
		if (!rdata)
			break;

		fprintf (stderr, "Popping item %d from the REDO-STACK\n", i);
		i--;
	}
}

#endif
