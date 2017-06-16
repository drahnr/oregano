/*
 * ngspice-watcher.h
 *
 *  Created on: Jun 5, 2017
 *      Author: michi
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
