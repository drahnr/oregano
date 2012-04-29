/*
 * ngspice-analysis.h
 *
 * Authors:
 *  Ricardo Markiewicz <rmarkie@fi.uba.ar>
 *  Marc Lorber <lorber.marc@wanadoo.fr>
 *
 * Web page: https://github.com/marc-lorber/oregano
 *
 * Copyright (C) 1999-2001  Richard Hult
 * Copyright (C) 2003,2006  Ricardo Markiewicz
 * Copyright (C) 2009-2012  Marc Lorber
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
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __NGSPICE_ANALYSIS_H
#define __NGSPICE_ANALYSIS_H

#include <glib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <ctype.h>
#include <glib/gi18n.h>
#include "ngspice.h"
#include "netlist-helper.h"
#include "dialogs.h"
#include "engine-internal.h"
#include "ngspice.h"

// Parser STATUS
struct _OreganoNgSpicePriv {
	GPid 		child_pid;
	gint 		child_stdout;
	gint 		child_error;
	GIOChannel *child_iochannel;
	GIOChannel *child_ioerror;
	gint 		child_iochannel_watch;
	gint 		child_ioerror_watch;
	Schematic  *schematic;

	gboolean 	aborted;

	GList 	   *analysis;
	gint 		num_analysis;
	SimulationData *current;
	double 		progress;
	gboolean 	char_last_newline;
	guint 		status;
	guint 		buf_count;
	// Added to store ngspice output into a file
	// 		input for oregano...
	FILE       *inputfp;
};

void ngspice_parse (OreganoNgSpice *ngspice);

#endif
