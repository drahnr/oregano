/*
 * log-interface.h
 *
 *  Created on: Jun 7, 2017
 *      Author: michi
 *
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
