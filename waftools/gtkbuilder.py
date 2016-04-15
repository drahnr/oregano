#!/usr/bin/env python
# encoding: utf-8
# Bernhard Schuster, 2016

from waflib import TaskGen

TaskGen.declare_chain(
	name = 'gtkbuilder',
	rule = 'gtk-builder-tool validate ${SRC}',
	ext_in = ['.ui','.ui.in'],
	reentrant = False,
	color = 'CYAN',
)
