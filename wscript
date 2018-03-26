#! /usr/bin/env python3
# encoding: utf-8

VERSION = '0.84.22'
APPNAME = 'oregano'

top = '.'
out = 'build'

import os
from waflib import Logs as logs
from waflib import Utils as utils

rec = ['src', 'po', 'data', 'test']

def options(opt):
	opt.load('compiler_c gnu_dirs glib2')

	opt.add_option('--debug', dest='build_debug', action='store_true', default=False, help='Build with debug flags')
	opt.add_option('--release', dest='build_release', action='store_true', default=False, help='Build with release flags')
	opt.add_option('--no-install-gschema', dest='no_install_gschema', action='store_true', default=False, help='Do not install the schema file')
	opt.add_option('--run', action='store_true', default=False, help='Run imediatly if the build succeeds.')
	opt.add_option('--gnomelike', action='store_true', default=False, help='Determines if gnome shemas and gnome iconcache should be installed.')
#	opt.add_option('--intl', action='store_true', default=False, help='Use intltool-merge to extract messages.')


def configure(conf):
	conf.load('compiler_c gnu_dirs glib2 intltool')

	conf.env.appname = APPNAME
	conf.env.version = VERSION

	conf.define('VERSION', VERSION)
	conf.define('GETTEXT_PACKAGE', APPNAME)


	#things the applications needs to know about, for easier re-use in subdir wscript(s)
	conf.env.path_datadir =  utils.subst_vars('${DATADIR}/', conf.env)
	conf.env.path_ui = utils.subst_vars('${DATADIR}/oregano/ui/', conf.env)
	conf.env.path_model = utils.subst_vars('${DATADIR}/oregano/models/', conf.env)
	conf.env.path_partslib = utils.subst_vars('${DATADIR}/oregano/library/', conf.env)
	conf.env.path_lang = utils.subst_vars('${DATADIR}/oregano/language-specs/', conf.env)
	conf.env.path_examples =  utils.subst_vars('${DATADIR}/oregano/examples/', conf.env)
#	conf.env.path_icons = '${DATADIR}/oregano/icons/'
#	conf.env.path_mime = '${DATADIR}/oregano/mime/'
#	conf.env.path_locale = '${DATADIR}/oregano/locale/'
#	conf.env.path_schemas =  utils.subst_vars('${DATADIR}/glib-2.0/schemas/', conf.env)


	#define the above paths so the application does know about files locations
	conf.define('OREGANO_UIDIR', conf.env.path_ui)
	conf.define('OREGANO_MODELDIR', conf.env.path_model)
	conf.define('OREGANO_LIBRARYDIR', conf.env.path_partslib)
	conf.define('OREGANO_LANGDIR', conf.env.path_lang)
	conf.define('OREGANO_EXAMPLEDIR', conf.env.path_examples)
#	conf.define('OREGANO_ICONDIR', conf.env.path_icons)
#	conf.define('OREGANO_MIMEDIR', conf.env.path_mime)
#	conf.define('OREGANO_LOCALEDIR', conf.env.path_locale)
#	conf.define('OREGANO_SCHEMASDIR', conf.env.path_schemas)


	conf.env.gschema_name = "io.ahoi.oregano.gschema.xml"
	conf.define('OREGANO_SCHEMA_NAME', conf.env.gschema_name)


	conf.check_cc(lib='m', uselib_store='M', mandatory=True)

	conf.check_cfg(atleast_pkgconfig_version='0.26')
	conf.check_cfg(package='glib-2.0', uselib_store='GLIB', args=['glib-2.0 >= 2.44', '--cflags', '--libs'], mandatory=True)
	conf.check_cfg(package='gobject-2.0', uselib_store='GOBJECT', args=['--cflags', '--libs'], mandatory=True)
	conf.check_cfg(package='gtk+-3.0', uselib_store='GTK3', args=['gtk+-3.0 >= 3.12', '--cflags', '--libs'], mandatory=True)
	conf.check_cfg(package='libxml-2.0', uselib_store='XML', args=['--cflags', '--libs'], mandatory=True)
	conf.check_cfg(package='goocanvas-2.0', uselib_store='GOOCANVAS', args=['--cflags', '--libs'], mandatory=True)
	conf.check_cfg(package='gtksourceview-3.0', uselib_store='GTKSOURCEVIEW3', args=['--cflags', '--libs'], mandatory=True)

	conf.check_large_file(mandatory=False)
	conf.check_endianness(mandatory=False)
	conf.check_inline(mandatory=False)

	conf.find_program('clang-format', var='CODEFORMAT', mandatory=False)
	conf.find_program('gdb', var='GDB', mandatory=False)
	conf.find_program('nemiver', var='NEMIVER', mandatory=False)
	conf.find_program('valgrind', var='VALGRIND', mandatory=False)

	# -ggdb vs -g -- http://stackoverflow.com/questions/668962
	if conf.options.build_debug:
		conf.env.CFLAGS = ['-ggdb', '-Wall']
		conf.define('DEBUG',1)
	elif conf.options.build_release:
		conf.env.CFLAGS = ['-O2', '-Wall']
		conf.define('RELEASE',1)

from waflib.Context import Context
from waflib.Build import BuildContext

def pre_fun(ctx):
	if ctx.cmd != 'install':
		logs.info ('Variant: \'' + ctx.variant + '\'')

def post_fun(ctx):
	if ctx.options.run:
		ctx.exec_command('')

def build(bld):
	bld(features='subst', source='oregano.spec.in', target='oregano.spec', install_path=None, VERSION=bld.env.version)
	if bld.variant != 'rpmspec':
		bld.add_pre_fun(pre_fun)
		bld.add_post_fun(post_fun)
		bld.recurse(rec)




class rpmspec(BuildContext):
	"""fill rpmspec"""
	cmd = 'rpmspec'
	variant = 'rpmspec'

class release(BuildContext):
	"""compile release binary"""
	cmd = 'release'
	variant = 'release'

class debug(BuildContext):
	"""compile debug binary"""
	cmd = 'debug'
	variant = 'debug'


def docs(ctx):
	logs.info("TODO: docs generation is not yet supported")


def dist(ctx):
	ctx.tar_prefix = APPNAME
	ctx.base_name = APPNAME+'-'+VERSION
	ctx.algo = 'tar.xz'
	ctx.excl = ['.*', '*~','./build','*.'+ctx.algo],
	ctx.files = ctx.path.ant_glob('src/**/*', excl=['**/*~'])
	ctx.files.extend(ctx.path.ant_glob('**/wscript'))
	ctx.files.extend(ctx.path.ant_glob('**/*', excl=['**/.*', 'build', 'buildrpm', '*.tar.*']))



def gdb_fun(ctx):
	if ctx.env.GDB:
		os.system ("G_DEBUG=resident-modules,fatal-warnings "+ctx.env.GDB[0]+" --args ./build/debug/"+APPNAME+" --debug-all")
	else:
		logs.warn ("Did not find \"gdb\". Re-configure if you installed it in the meantime.")

def nemiver_fun(ctx):
	if ctx.env.NEMIVER:
		os.system (" "+ctx.env.NEMIVER[0]+" --env=\"G_DEBUG=resident-modules,fatal-warnings\" ./build/debug/"+APPNAME+" --debug-all")
	else:
		logs.warn ("Did not find \"nemiver\". Re-configure if you installed it in the meantime.")


def valgrind_fun(ctx):
	if ctx.env.VALGRIND:
		os.system ("G_DEBUG=resident-modules,always-malloc "+ctx.env.VALGRIND[0]+" --leak-check=full --leak-resolution=high --show-reachable=no --track-origins=yes --undef-value-errors=yes --show-leak-kinds=definite --free-fill=0x77 ./build/debug/"+APPNAME+" --debug-all")
	else:
		logs.warn ("Did not find \"valgrind\". Re-configure if you installed it in the meantime.")



def massif_fun(ctx):
	if ctx.env.VALGRIND:
		os.system ("G_DEBUG=resident-modules,always-malloc "+ctx.env.MASSIF[0]+" --tool=massif --depth=10 --max-snapshots=1000 --alloc-fn=g_malloc --alloc-fn=g_realloc --alloc-fn=g_try_malloc --alloc-fn=g_malloc0 --alloc-fn=g_mem_chunk_alloc --threshold=0.01 build/debug/ ./build/debug/"+APPNAME+" --debug-all")
	else:
		logs.warn ("Did not find \"massif\". Re-configure if you installed it in the meantime.")




def codeformat_fun(ctx):
	if ctx.env.CODEFORMAT:
		nodes = ctx.path.ant_glob(\
		    ['src/*.[ch]',
		     'src/tools/*.[ch]',
		     'src/gplot/*.[ch]',
		     'src/engines/*.[ch]',
		     'src/sheet/*.[ch]',
		     'src/model/*.[ch]'])
		args = ''
		for item in nodes:
			args += str(item.abspath())+' '
		os.system (''+ctx.env.CODEFORMAT[0]+' -i '+ args)
	else:
		logs.warn ("Did not find \"clang-format\". Re-configure if you installed it in the meantime.")


def spawn_pot(ctx):
#	"create a .pot from all sources (.ui,.c,.h,.desktop)"

	ctx.recurse ('po')


def update_po(ctx):
#	"update the .po files"
	ctx.recurse ('po')

# we need to subclass BuildContext instead of Context
# in order to access ctx.env.some_variable
# TODO create a custom subclass of waflib.Context.Context which implements the load_env from BuildContext
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
	os.system('./builddeps.sh')

class builddeps(Context):
	"""Install build dependencies"""
	cmd = 'builddeps'
	fun = 'builddeps_fun'

def version_fun(ctx):
	with open("VERSION", "w") as f:
		f.write(VERSION + "\n")

class version(Context):
	"""Oregano Version to file VERSION"""
	cmd = 'version'
	fun = 'version_fun'
