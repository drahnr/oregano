/*
 * thread-pipe.h
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
