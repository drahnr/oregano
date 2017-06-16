/*
 * thread-pipe.h
 *
 *  Created on: May 31, 2017
 *      Author: michi
 */

#ifndef THREAD_PIPE_H_
#define THREAD_PIPE_H_

#define THREAD_PIPE_MAX_BUFFER_BLOCK_COUNTER_DEFAULT 20
#define THREAD_PIPE_MAX_BUFFER_SIZE_TOTAL_DEFAULT 2048

typedef struct _ThreadPipe ThreadPipe;

/**
 * Constructor
 */
ThreadPipe *thread_pipe_new(
		guint max_buffer_block_counter,
		gsize max_buffer_size_total);

/**
 * functions for writing thread
 */
gboolean thread_pipe_push(ThreadPipe *pipe, gpointer data, gsize size);
// Destructor
void thread_pipe_set_write_eof(ThreadPipe *pipe);

/**
 * functions for reading thread
 */
ThreadPipe *thread_pipe_pop(ThreadPipe *pipe, gpointer *data_out, gsize *size);
ThreadPipe *thread_pipe_pop_line(ThreadPipe *pipe, gchar **data_out, gsize *size);
// Destructor
void thread_pipe_set_read_eof(ThreadPipe *pipe);

#endif /* THREAD_PIPE_H_ */
