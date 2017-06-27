/*
 * ngspice-watcher.h
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

#ifndef ENGINES_NGSPICE_WATCHER_H_
#define ENGINES_NGSPICE_WATCHER_H_

#include "../log-interface.h"
#include "../sim-settings.h"
#include "ngspice-analysis.h"

typedef struct _NgspiceWatcherBuildAndLaunchResources NgspiceWatcherBuildAndLaunchResources;

/**
 * This struct has to be public for testing purposes.
 */
struct _NgspiceWatcherBuildAndLaunchResources {
	LogInterface log;//in
	const SimSettings* sim_settings;//in
	gboolean is_vanilla;//in
	GPid* child_pid;//out
	gboolean* aborted;//out
	//OreganoNgSpice object
	const void* emit_instance;//in
	guint* num_analysis;//out
	ProgressResources* progress_ngspice;//out
	ProgressResources* progress_reader;//out
	GList** analysis;//out
	AnalysisTypeShared* current;//out
	gchar* ngspice_result_file;//in
	gchar* netlist_file;//in
	CancelInfo *cancel_info;//in
};

NgspiceWatcherBuildAndLaunchResources *ngspice_watcher_build_and_launch_resources_new(OreganoNgSpice *ngspice);
void ngspice_watcher_build_and_launch_resources_finalize(NgspiceWatcherBuildAndLaunchResources *resources);

void ngspice_watcher_build_and_launch(const NgspiceWatcherBuildAndLaunchResources *resources);

#endif /* ENGINES_NGSPICE_WATCHER_H_ */
