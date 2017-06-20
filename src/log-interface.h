/*
 * log-interface.h
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
 * Use this interface to get rid of GUI dependencies. For example
 * if you want to write a unit test in which the test function
 * needs a log to not throw an exception.
 */

#ifndef LOG_INTERFACE_H_
#define LOG_INTERFACE_H_

typedef void (*LogFunction)(gpointer log, const char *message);

typedef struct {
	LogFunction log_append;
	LogFunction log_append_error;
	gpointer log;
} LogInterface;

#endif /* LOG_INTERFACE_H_ */
