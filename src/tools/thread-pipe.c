/**
 * thread-pipe.c
 *
 *
 * Authors:
 *  Michi <st101564@stud.uni-stuttgart.de>
 *
 * Web page: https://ahoi.io/project/oregano
 *
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

/*
 *
 * BASICS
 * ------
 * ThreadPipe can be used to efficiently communicate large
 * or small data between 2 threads, whereas the data pops up
 * bit by bit (or at once).
 * It consists of a chain of data blocks. New
 * data is appended at the one end of the chain, old data
 * can be read at the other end of the pipe.
 *
 * If there is data (chain has more than 1 chain link), the
 * reader gets to know this by locking a mutex and asking
 * the state of a shared variable.
 *
 * If all data is read and the reader needs more data, he
 * will wait until new data is available by locking a mutex
 * and waiting for a conditional signal of the writer.
 *
 *
 * Locking, unlocking and signaling can slow down the
 * communication process very hard, if the data blocks are
 * small. To solve this problem, an additional buffer is
 * introduced.
 *
 * BUFFERED VERSION
 * ----------------
 * The problem of unbuffered ThreadPipe is that locking
 * and unlocking is a very heavy work that should not
 * be done too frequently. Now if the data blocks pushed
 * to ThreadPipe are too small, the lock operation is
 * done very often. However the positive side of small blocks
 * is the real time ability.
 *
 * If you need only weak real time or no real time but an
 * efficient use of time and money, you should use the
 * buffered version of thread pipe with large (but not too
 * large) buffer numbers (you can use a buffer by setting
 * the buffer numbers in the constructor unequal 1).
 *
 * There are some approaches how to realize a buffer in the
 * case of thread pipe:
 * - melting incoming blocks together with realloc,
 * counting the number of bytes or melting operations,
 * release the block for popping when number is large enough,
 * - appending incoming blocks together,
 * counting the number of bytes or/and number of blocks not released,
 * release the block queue if number is large enough,
 * - counting time instead of space,
 * - other shit
 *
 * Because blocks are structure elements that can be used to
 * make the code more efficient AND melting operations
 * can be heavy work, I think that simply appending some
 * blocks and not locking/releasing every single block is
 * a good way to go. Counting time instead of space may be
 * a good approach for real time applications that want to
 * be also efficient. But because real time is not demanded
 * in a non real time simulation like ngspice or gnucap, we don't
 * need to bother. But you can set the buffer numbers to 1
 * and then you can use thread pipe in real time applications,
 * that have far too much nop-time and really small amounts of
 * data workload.
 *
 * Furthermore I have decided to count the number of blocks
 * and the total size of not released blocks. When one of
 * the counting numbers are larger than their corresponding
 * buffer constants, the whole pipe queue is released for
 * popping (what needs a small locking access).
 *
 * You have some possibilities to define the buffer constants:
 * - define statements in thread-pipe.h
 * - telling the constructor function what are your wishes
 *
 * THE FUTURE
 * ----------
 * An enhanced version of this could be an adaptive buffer size
 * decision maker that detects the frequency of collisions
 * and uses this information to adapt the optimal buffer size
 * as a function of time, by extrapolating the behavior of
 * the program like branch prediction technology that is used in
 * processors. A good version of this could be real time capable
 * in low workload times and would be optimal for time, space
 * and money saving in times of high workload.
 *
 */

#include <glib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

#include "thread-pipe.h"

typedef struct _ThreadPipeData ThreadPipeData;

/**
 * chain link/chain element
 */
struct _ThreadPipeData {
	// introduced for efficient string support
	gpointer malloc_address;
	// data block
	gpointer data;
	// size of data block
	gsize size;
	// link to next chain element
	ThreadPipeData *next;
};

/**
 * structuring structure
 */
typedef struct {
	// closing/freeing information
	gboolean write_eof;
	gboolean read_eof;

	// buffer state information
	gsize size_total;
	gsize block_counter;
} ThreadPipeBufferData;

struct _ThreadPipe {
	/**
	 * variables of reading thread
	 */
	ThreadPipeData *read_data;
	ThreadPipeBufferData read_buffer_data;

	/**
	 * variables of writing thread
	 */
	ThreadPipeData *write_data;
	ThreadPipeBufferData write_buffer_data;

	/**
	 * shared variables
	 */
	ThreadPipeBufferData ready_buffer_data;

	/**
	 * synchronizing variables
	 */
	GMutex mutex;
	GCond cond;

	/**
	 * read-only variables
	 */
	guint max_block_counter;
	gsize max_size_total;
};

static ThreadPipeData *thread_pipe_data_new(gpointer data, gsize size);
static ThreadPipeData *thread_pipe_data_destroy(ThreadPipe *pipe);
static void thread_pipe_destroy(ThreadPipe *pipe);

/**
 * Creates a new ThreadPipe structure.
 *
 * I recommend using ThreadPipes like normal FIFO pipes, whereas one thread
 * uses only read functions and another thread uses only write functions.
 * The reading thread should make sure that read_eof will be set,
 * the writing thread should make sure that write_eof will be set,
 * because:
 *
 * ThreadPipes have 3 possibilities to close:
 * 1. call thread_pipe_set_write_eof and after that read the pipe to the end
 * 2. call thread_pipe_set_write_eof and after that call thread_pipe_set_read_eof
 * 3. call thread_pipe_set_read_eof and after that call thread_pipe_set_write_eof
 *
 * Now if the creating thread of the ThreadPipe wants to close the ThreadPipe,
 * and he did not touch any reading or writing functions, he does not know
 * whether the pipe has been closed already automatically and will cause a
 * segmentation fault eventually if he tries.
 *
 * @max_buffer_block_counter: 1 is the lowest number (you can't push no data).
 * If 0, the default value from thread-pipe.h will be used.
 * @max_buffer_block_counter: 1 is the lowest value (ThreadPipe does not allow
 * to push blocks of size 0). If 0, the default value from thread-pipe.h
 * will be used.
 *
 * returns a new ThreadPipe structure
 */
ThreadPipe *thread_pipe_new(
		guint max_buffer_block_counter,
		gsize max_buffer_size_total) {
	ThreadPipe *thread_pipe = g_new0(ThreadPipe, 1);

	g_mutex_init(&thread_pipe->mutex);
	g_cond_init(&thread_pipe->cond);

	ThreadPipeData *pipe_data = thread_pipe_data_new(NULL, 0);
	thread_pipe->read_data = pipe_data;
	thread_pipe->write_data = pipe_data;

	thread_pipe->max_block_counter = max_buffer_block_counter != 0 ? max_buffer_block_counter : THREAD_PIPE_MAX_BUFFER_BLOCK_COUNTER_DEFAULT;
	thread_pipe->max_size_total = max_buffer_size_total != 0 ? max_buffer_size_total : THREAD_PIPE_MAX_BUFFER_SIZE_TOTAL_DEFAULT;

	return thread_pipe;
}

/**
 * Pushes a block of size size to the end of the pipe. The data is copied
 * to heap.
 *
 * Don't push, if you set write_eof already.
 *
 * return value will be NULL, if
 * - read_eof has been set by thread_pipe_set_read_eof or
 * - write_eof has been set by thread_pipe_set_write_eof (with fail message) or
 * - pipe is NULL (with fail message).
 *
 * If read_eof has been set by thread_pipe_set_read_eof before, you can close
 * the pipe by setting write_eof and you will save much time and money,
 * so be sure to check return value every call. But you can also call this
 * function if it makes no sense. The function recognizes then that
 * pushing makes no sense and returns fast. The function is not closing the
 * pipe automatically in this case because it is safer for example if the
 * programmer does not check the return value.
 *
 * returns TRUE or FALSE
 * - FALSE, if the pipe has been closed by read_eof or other events that
 * indicate that you can close the pipe by setting write_eof
 * - TRUE, if the pipe is living further on and it makes sense that it
 * will live further on
 */
gboolean thread_pipe_push(ThreadPipe *pipe, gpointer data, gsize size) {
	// Give me an object.
	g_return_val_if_fail(pipe != NULL, FALSE);

	// Don't push, if you set write_eof already.
	g_return_val_if_fail(pipe->write_buffer_data.write_eof != TRUE, FALSE);

	// Don't push no data to pipe.
	g_return_val_if_fail(data != NULL, !pipe->write_buffer_data.read_eof);
	g_return_val_if_fail(size != 0, !pipe->write_buffer_data.read_eof);
	
	// pipe not active any more because no reader has interest.
	if (pipe->write_buffer_data.read_eof)
		return FALSE;


	pipe->write_data->next = thread_pipe_data_new(data, size);
	pipe->write_data = pipe->write_data->next;

	pipe->write_buffer_data.block_counter++;
	pipe->write_buffer_data.size_total += size;

	if (pipe->write_buffer_data.block_counter < pipe->max_block_counter
			&& pipe->write_buffer_data.size_total < pipe->max_size_total)
		return TRUE;


	g_mutex_lock(&pipe->mutex);

	pipe->ready_buffer_data.block_counter += pipe->write_buffer_data.block_counter;
	pipe->write_buffer_data.block_counter = 0;

	pipe->ready_buffer_data.size_total += pipe->write_buffer_data.size_total;
	pipe->write_buffer_data.size_total = 0;

	pipe->write_buffer_data.read_eof = pipe->ready_buffer_data.read_eof;

	g_cond_signal(&pipe->cond);
	g_mutex_unlock(&pipe->mutex);
	
	return !pipe->write_buffer_data.read_eof;
}

/**
 * Reads a block of memory that has been pushed earlier by thread_pipe_push.
 * If there is no block that has been pushed earlier, this function will
 * wait until a block will be pushed or write_eof will be set.
 *
 * pipe will be destroyed automatically if write_eof has been set && pipe is
 * empty, so be sure to check return value always!
 *
 * The data, stored to data_out, will be freed in the next thread_pipe_pop call,
 * so be sure to copy the data, if you need it longer than one thread_pipe_pop
 * cycle.
 *
 * Don't pop, if you set read_eof already.
 *
 * returns pipe or NULL
 * - NULL, if the pipe has been destroyed
 * - pipe, if the pipe is living further on
 */
ThreadPipe *thread_pipe_pop(ThreadPipe *pipe, gpointer *data_out, gsize *size) {
	g_return_val_if_fail(pipe != NULL, pipe);
	g_return_val_if_fail(data_out != NULL, pipe);
	g_return_val_if_fail(size != NULL, pipe);
	//Don't pop, if you set read_eof already.
	g_return_val_if_fail(pipe->read_buffer_data.read_eof != TRUE, NULL);

	*data_out = NULL;
	*size = 0;

	if (pipe->read_buffer_data.block_counter <= 0) {

		g_mutex_lock(&pipe->mutex);

		while (pipe->ready_buffer_data.block_counter <= 0 && !pipe->ready_buffer_data.write_eof)
			g_cond_wait(&pipe->cond, &pipe->mutex);

		pipe->read_buffer_data.block_counter = pipe->ready_buffer_data.block_counter;
		pipe->ready_buffer_data.block_counter = 0;

		pipe->read_buffer_data.size_total = pipe->ready_buffer_data.size_total;
		pipe->ready_buffer_data.size_total = 0;

		pipe->read_buffer_data.write_eof = pipe->ready_buffer_data.write_eof;

		g_mutex_unlock(&pipe->mutex);

	}

	if (!thread_pipe_data_destroy(pipe)) {
		thread_pipe_destroy(pipe);
		return NULL;
	}
	*data_out = pipe->read_data->data;
	*size = pipe->read_data->size;

	return pipe;
}

/**
 * Reads to the end of a line like fgets and pops it, or reads to the end of pipe.
 *
 * size_out will be the length + 1 of the string like strlen.
 *
 * Pushed data blocks should be 0 terminated, but don't have to.
 *
 * possible independent cases:
 * - newline at position of block, where position in Po := {nowhere, beginning/middle, end}
 * - newline at is_first block, where is_first in If := {first, not first}
 * - newline at is_last block, where is_last in Il := {last, not last}
 * In total there are 12 pairwise unequal cases by forming the Cartesian product of Po, If and Il
 * and adding the case where there is no newline at all.
 *
 * returns pipe or NULL like thread_pipe_pop
 */
ThreadPipe *thread_pipe_pop_line(ThreadPipe *pipe_in, gchar **string_out, gsize *size_out) {
	g_return_val_if_fail(pipe_in != NULL, pipe_in);
	g_return_val_if_fail(string_out != NULL, pipe_in);
	g_return_val_if_fail(size_out != NULL, pipe_in);
	//Don't pop, if you set read_eof already.
	g_return_val_if_fail(pipe_in->read_buffer_data.read_eof != TRUE, NULL);


	*string_out = NULL;
	*size_out = 0;

	size_t line_size;
	gchar *line;
	FILE *line_file = open_memstream(&line, &line_size);

	ThreadPipeData *current = NULL;
	gchar *ptr = NULL;

	while (TRUE) {

		ptr = NULL;

		if (pipe_in->read_buffer_data.block_counter <= 0) {

			g_mutex_lock(&pipe_in->mutex);

			while (pipe_in->ready_buffer_data.block_counter <= 0 && !pipe_in->ready_buffer_data.write_eof)
				g_cond_wait(&pipe_in->cond, &pipe_in->mutex);

			pipe_in->read_buffer_data.block_counter = pipe_in->ready_buffer_data.block_counter;
			pipe_in->ready_buffer_data.block_counter = 0;

			pipe_in->read_buffer_data.size_total = pipe_in->ready_buffer_data.size_total;
			pipe_in->ready_buffer_data.size_total = 0;

			pipe_in->read_buffer_data.write_eof = pipe_in->ready_buffer_data.write_eof;

			g_mutex_unlock(&pipe_in->mutex);

		}

		current = pipe_in->read_data->next;


		if (current == NULL)
			break;

		ptr = current->data;
		while (*ptr != '\n' &&
		       *ptr != 0 &&
		       ptr - (gchar *)current->data < current->size	//somebody forgot to close the string with 0?
			   ) {
			fputc(*ptr, line_file);
			ptr++;
		}

		if (ptr - (gchar *)current->data < current->size && *ptr == '\n') {
			fputc(*ptr, line_file);
			ptr++;
			break;
		}

		thread_pipe_data_destroy(pipe_in);
	}

	fputc(0, line_file);
	fclose(line_file);

	if (current == NULL) {

		if (line_size == 1) {
			g_free(line);
			thread_pipe_destroy(pipe_in);
			return NULL;
		}

	} else {

		gchar **current_data = (gchar **)&current->data;
		gsize *current_size = &current->size;

		*current_size -= (ptr - *current_data);
		pipe_in->read_buffer_data.size_total -= (ptr - *current_data);
		*current_data = ptr;

		if (*current_size == 0 || (*current_size == 1 && *ptr == 0))
			thread_pipe_data_destroy(pipe_in);

	}

	/**
	 * current == NULL && line_size != 1
	 * ||
	 * current != NULL
	 */

	gchar **old_data = (gchar **)&pipe_in->read_data->malloc_address;
	gsize *old_size = &pipe_in->read_data->size;

	g_free(*old_data);
	*old_data = line;
	*old_size = line_size;

	*string_out = *old_data;
	*size_out = *old_size;

	return pipe_in;
}

/**
 * If you are finished with writing, you have to set write_eof so that the
 * memory of pipe can be freed. You can set write_eof independent of
 * - read_eof
 * - emptiness of pipe
 * - other shit
 *
 * Don't push, after you called thread_pipe_set_write_eof.
 *
 * The memory of data and pipe will be freed, if read_eof && write_eof == TRUE.
 */
void thread_pipe_set_write_eof(ThreadPipe *pipe) {
	g_return_if_fail(pipe != NULL);
	g_return_if_fail(pipe->write_buffer_data.write_eof != TRUE);

	g_mutex_lock(&pipe->mutex);
	gboolean destroy = pipe->ready_buffer_data.read_eof;

	pipe->ready_buffer_data.write_eof = TRUE;
//	pipe->read_buffer_data.write_eof = TRUE;
	pipe->write_buffer_data.write_eof = TRUE;

	pipe->ready_buffer_data.block_counter += pipe->write_buffer_data.block_counter;
	pipe->write_buffer_data.block_counter = 0;

	pipe->ready_buffer_data.size_total += pipe->write_buffer_data.size_total;
	pipe->write_buffer_data.size_total = 0;

	g_cond_signal(&pipe->cond);
	g_mutex_unlock(&pipe->mutex);

	if (destroy)
		thread_pipe_destroy(pipe);
}

/**
 * If you are finished with reading, you have to set read_eof so that the
 * memory of pipe can be freed. You can set read_eof independent of
 * - write_eof
 * - emptiness of pipe
 * - other shit
 *
 * Don't pop, after you called thread_pipe_set_read_eof.
 *
 * The memory of data waiting in pipe to be popped, will all be freed.
 *
 * The memory of pipe will be freed also, if write_eof && write_eof == TRUE.
 */
void thread_pipe_set_read_eof(ThreadPipe *pipe) {
	g_return_if_fail(pipe != NULL);
	g_return_if_fail(pipe->read_buffer_data.read_eof != TRUE);

	g_mutex_lock(&pipe->mutex);
	gboolean destroy = pipe->ready_buffer_data.write_eof;
	pipe->ready_buffer_data.read_eof = TRUE;
	pipe->read_buffer_data.read_eof = TRUE;
//	pipe->write_buffer_data.read_eof = TRUE;

	pipe->read_buffer_data.write_eof = pipe->ready_buffer_data.write_eof;

	pipe->read_buffer_data.block_counter += pipe->ready_buffer_data.block_counter;
	pipe->ready_buffer_data.block_counter = 0;

	pipe->read_buffer_data.size_total += pipe->ready_buffer_data.size_total;
	pipe->ready_buffer_data.size_total = 0;

	while (pipe->read_buffer_data.block_counter)
		thread_pipe_data_destroy(pipe);
	g_mutex_unlock(&pipe->mutex);

	if (destroy)
		thread_pipe_destroy(pipe);
}

/**
 * copy data to a new ThreadPipeData structure
 */
static ThreadPipeData *thread_pipe_data_new(gpointer data, gsize size) {
	ThreadPipeData *pipe_data = g_new0(ThreadPipeData, 1);
	if (data != NULL && size != 0) {
		pipe_data->malloc_address = g_malloc(size);
		memcpy(pipe_data->malloc_address, data, size);
		pipe_data->data = pipe_data->malloc_address;
		pipe_data->size = size;
	}

	return pipe_data;
}

/**
 * free all memory of a ThreadPipeData structure
 *
 * returns
 * - the next block of the linked list, if there is one
 * - else NULL
 */
static ThreadPipeData *thread_pipe_data_destroy(ThreadPipe *pipe) {
	ThreadPipeData *pipe_data = pipe->read_data;
	ThreadPipeData *next = pipe_data->next;
	g_free(pipe_data->malloc_address);
	g_free(pipe_data);
	if (next != NULL) {
		pipe->read_buffer_data.block_counter--;
		pipe->read_buffer_data.size_total -= next->size;
	}
	pipe->read_data = next;
	return next;
}

/**
 * free all memory of a ThreadPipe structure
 */
static void thread_pipe_destroy(ThreadPipe *pipe) {
	g_return_if_fail(pipe != NULL);

	while (pipe->read_data)
		thread_pipe_data_destroy(pipe);
	g_mutex_clear(&pipe->mutex);
	g_cond_clear(&pipe->cond);
	g_free(pipe);
}
