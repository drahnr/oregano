/*
 * ngspice-analysis.h
 *
 * Authors:
 *  Ricardo Markiewicz <rmarkie@fi.uba.ar>
 *  Marc Lorber <lorber.marc@wanadoo.fr>
 *  Guido Trentalancia <guido@trentalancia.com>
 *
 * Web page: https://ahoi.io/project/oregano
 *
 * Copyright (C) 1999-2001  Richard Hult
 * Copyright (C) 2003,2006  Ricardo Markiewicz
 * Copyright (C) 2009-2012  Marc Lorber
 * Copyright (C) 2017       Guido Trentalancia
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
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef __NGSPICE_ANALYSIS_H
#define __NGSPICE_ANALYSIS_H

#include <glib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <ctype.h>
#include <glib/gi18n.h>
#include "../tools/thread-pipe.h"
#include "../tools/cancel-info.h"
#include "ngspice.h"
#include "netlist-helper.h"
#include "dialogs.h"
#include "engine-internal.h"
#include "ngspice.h"

/*
 * The file buffer size (recommended value
 * is 512 bytes).
 */
#define BSIZE_SP	512

/**
 * Progress is a shared variable between GUI thread
 * that displays the progress bar and working thread
 * which executes the heavy work.
 */
typedef struct {
	gdouble progress;
	// time (from g_get_monotonic_time) of the last writing access
	gint64 time;
	GMutex progress_mutex;
} ProgressResources;

/**
 * AnalysisType is a shared variable between progress
 * bar displaying GUI thread and the working thread.
 */
typedef struct {
	AnalysisType type;
	GMutex mutex;
} AnalysisTypeShared;

typedef struct {
	ThreadPipe *pipe;
	gchar *buf;
	gboolean is_vanilla;
	const SimSettings* sim_settings;
	AnalysisTypeShared *current;
	GList **analysis;
	guint *num_analysis;
	ProgressResources *progress_reader;
	guint64 no_of_data_rows_ac;
	guint64 no_of_data_rows_dc;
	guint64 no_of_data_rows_op;
	guint64 no_of_data_rows_transient;
	guint64 no_of_data_rows_noise;
	guint no_of_variables;
	CancelInfo *cancel_info;
} NgspiceAnalysisResources;

// Parser STATUS
struct _OreganoNgSpicePriv
{
	gboolean is_vanilla;

	GPid child_pid;

	Schematic *schematic;

	gboolean aborted;
	CancelInfo *cancel_info;

	GList *analysis;
	guint num_analysis;
	AnalysisTypeShared current;

	ProgressResources progress_ngspice;
	ProgressResources progress_reader;
};

void ngspice_analysis (NgspiceAnalysisResources *resources);
void ngspice_save (const gchar *path_to_file, ThreadPipe *pipe, CancelInfo *cancel_info);

#endif
