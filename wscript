#! /usr/bin/env python3
# encoding: utf-8

VERSION = '0.83.3'
APPNAME = 'oregano'

top = '.'
out = 'build'

import os
from waflib import Logs as logs
from waflib import Utils as utils


recurse = ['src', 'test', 'po', 'data']


def options(opt):
	opt.load('gnu_dirs')
	opt.add_option('--run', action='store_true', default=False, help='Run imediatly if the build succeeds.')
	opt.recurse(recurse)

def configure(cfg):
	cfg.load('gnu_dirs')
	cfg.env.appname = APPNAME
	cfg.env.version = VERSION

	#things the applications needs to know about, for easier re-use in subdir wscript(s)
	cfg.env.path_ui = utils.subst_vars('${DATADIR}/oregano/ui/', cfg.env)
	cfg.env.path_model = utils.subst_vars('${DATADIR}/oregano/models/', cfg.env)
	cfg.env.path_partslib = utils.subst_vars('${DATADIR}/oregano/library/', cfg.env)
	cfg.env.path_lang = utils.subst_vars('${DATADIR}/oregano/language-specs/', cfg.env)
	cfg.env.path_examples =  utils.subst_vars('${DATADIR}/oregano/examples/', cfg.env)
	cfg.env.path_icons = utils.subst_vars('${DATADIR}/oregano/icons/', cfg.env)
	cfg.env.gschema_name = "io.ahoi.oregano.gschema"

	cfg.recurse(recurse)


def build(bld):
	bld.load('gnu_dirs')
	bld(features='subst', source='oregano.spec.in', target='oregano.spec', install_path=None, VERSION=bld.env.version)
	bld.recurse(recurse)


def dist(ctx):
	ctx.base_name = APPNAME+'-'+VERSION
	ctx.algo = 'tar.xz'
	ctx.excl = ['.*', '*~','./build','*.'+ctx.algo],
	ctx.files = ctx.path.ant_glob('**/wscript')



from waflib.Build import BuildContext

class release(BuildContext):
	"""compile release binary"""
	cmd = 'release'
	variant = 'release'

class debug(BuildContext):
	"""compile debug binary"""
	cmd = 'debug'
	variant = 'debug'





def gdb_fun(ctx):
	if ctx.env.GDB:
		os.system ("G_DEBUG=resident-modules,fatal-warnings "+ctx.env.GDB[0]+" --args ./build/debug/"+APPNAME+" --debug-all")
	else:
		logs.warn ("Did not find \"gdb\". Re-cfgigure if you installed it in the meantime.")

def nemiver_fun(ctx):
	if ctx.env.NEMIVER:
		os.system (" "+ctx.env.NEMIVER[0]+" --env=\"G_DEBUG=resident-modules,fatal-warnings\" ./build/debug/"+APPNAME+" --debug-all")
	else:
		logs.warn ("Did not find \"nemiver\". Re-cfgigure if you installed it in the meantime.")


def valgrind_fun(ctx):
	if ctx.env.VALGRIND:
		os.system ("G_DEBUG=resident-modules,always-malloc,fatal-warnings "+ctx.env.VALGRIND[0]+" --leak-check=full --leak-resolution=high --show-reachable=no --track-origins=yes --undef-value-errors=yes --show-leak-kinds=definite --free-fill=0x77 ./build/debug/"+APPNAME+" --debug-all")
	else:
		logs.warn ("Did not find \"valgrind\". Re-cfgigure if you installed it in the meantime.")



def massif_fun(ctx):
	if ctx.env.VALGRIND:
		os.system ("G_DEBUG=resident-modules,always-malloc "+ctx.env.VALGRIND[0]+" --tool=massif --depth=10 --max-snapshots=1000 --alloc-fn=g_malloc --alloc-fn=g_realloc --alloc-fn=g_try_malloc --alloc-fn=g_malloc0 --alloc-fn=g_mem_chunk_alloc --threshold=0.01 build/debug/ ./build/debug/"+APPNAME+" --debug-all")
	else:
		logs.warn ("Did not find \"massif\". Re-cfgigure if you installed it in the meantime.")




def codeformat_fun(ctx):
	if ctx.env.CODEFORMAT:
		nodes = ctx.path.ant_glob(\
		    ['src/*.[ch]',
		     'src/gplot/*.[ch]',
		     'src/engines/*.[ch]',
		     'src/sheet/*.[ch]',
		     'src/model/*.[ch]'])
		args = ''
		for item in nodes:
			args += str(item.abspath())+' '
		os.system (''+ctx.env.CODEFORMAT[0]+' -i '+ args)
	else:
		logs.warn ("Did not find \"clang-format\". Re-cfgigure if you installed it in the meantime.")


import platform
from waflib.Context import Context
import re

def spawn_pot(ctx):
#	"create a .pot from all sources (.ui,.c,.h,.desktop)"

	ctx.recurse ('po')


def update_po(ctx):
#	"update the .po files"
	ctx.recurse ('po')

# we need to subclass BuildContext instead of Context
# in order to access ctx.env.some_variable
# TODO create a custom subclass of waflib.Context.Context\
# TODO which implements the load_env from BuildContext
class spawnpot(BuildContext):
	"""spawn .pot files"""
	cmd = 'spawnpot'
	fun = 'spawn_pot'

class updatepo(BuildContext):
	"""update the translate .po files"""
	cmd = 'updatepo'
	fun = 'update_po'


class codeformat(BuildContext):
	"""Format the source tree"""
	cmd = 'codeformat'
	fun = 'codeformat_fun'

class massif(BuildContext):
	"""Run with \"massif\""""
	cmd = 'massif'
	fun = 'massif_fun'

class valgrind(BuildContext):
	"""Run with \"valgrind\""""
	cmd = 'valgrind'
	fun = 'valgrind_fun'

class gdb(BuildContext):
	"""Run with \"gdb\""""
	cmd = 'gdb'
	fun = 'gdb_fun'

class nemiver(BuildContext):
	"""Run with \"nemiver\""""
	cmd = 'nemiver'
	fun = 'nemiver_fun'

def builddeps_fun(ctx):
	pl = platform.linux_distribution()
	if pl[0] == 'Fedora':
		os.system('dnf install gtk3-devel libxml2-devel gtksourceview3-devel intltool glib2-devel goocanvas2-devel desktop-file-utils')
	elif pl[0] == 'Ubuntu':
		os.system('apt-get install libglib2.0-dev intltool libgtk-3-dev libxml2-dev libgoocanvas-2.0-dev libgtksourceview-3.0-dev gnucap')
	else:
		logs.warn ('Unknown Linux distribution. Do nothing.')

class builddeps(Context):
	"""Install build dependencies"""
	cmd = 'builddeps'
	fun = 'builddeps_fun'
