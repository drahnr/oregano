#! /usr/bin/env python
# encoding: utf-8

VERSION = '0.83.3'
APPNAME = 'oregano'

top = '.'
out = 'build'

import os
from waflib import Logs as logs
from waflib import Utils as utils

def options(opt):
	opt.load('compiler_c gnu_dirs glib2')
	opt.load('unites', tooldir='waftools')

	opt.add_option('--run', action='store_true', default=False, help='Run imediatly if the build succeeds.')
	opt.add_option('--gnomelike', action='store_true', default=False, help='Determines if gnome shemas and gnome iconcache should be installed.')
#	opt.add_option('--intl', action='store_true', default=False, help='Use intltool-merge to extract messages.')


def configure(conf):
	conf.load('compiler_c gnu_dirs glib2 intltool')
	conf.load('unites', tooldir='waftools')

	conf.env.appname = APPNAME
	conf.env.version = VERSION

	conf.define('VERSION', VERSION)
	conf.define('GETTEXT_PACKAGE', APPNAME)


	#things the applications needs to know about, for easier re-use in subdir wscript(s)
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
	conf.check_cfg(package='glib-2.0', uselib_store='GLIB', args=['glib-2.0 >= 2.24', '--cflags', '--libs'], mandatory=True)
	conf.check_cfg(package='gobject-2.0', uselib_store='GOBJECT', args=['--cflags', '--libs'], mandatory=True)
	conf.check_cfg(package='gtk+-3.0', uselib_store='GTK3', args=['--cflags', '--libs'], mandatory=True)
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
	conf.setenv('debug', env=conf.env.derive())
	conf.env.CFLAGS = ['-ggdb', '-Wall']
	conf.define('DEBUG',1)
	conf.define('DEBUG_DISABLE_GRABBING',1)

	conf.setenv('release', env=conf.env.derive())
	conf.env.CFLAGS = ['-O2', '-Wall']
	conf.define('RELEASE',1)





def docs(ctx):
	logs.info("TODO: docs generation is not yet supported")


def dist(ctx):
	ctx.base_name = APPNAME+'-'+VERSION
	ctx.algo = 'tar.xz'
	ctx.excl = ['.*', '*~','./build','*.'+ctx.algo],
	ctx.files = ctx.path.ant_glob('**/wscript')



def pre(ctx):
	if ctx.cmd != 'install':
		logs.info ('Variant: \'' + ctx.variant + '\'')



def post(ctx):
	if ctx.options.run:
		ctx.exec_command('')


from waftools.unites import summary as unites_summary

def build(bld):
	bld.add_pre_fun(pre)
	bld.add_post_fun(post)
	bld.recurse(['po','data'])

	if bld.cmd != 'install' and bld.cmd != 'uninstall':
		if not bld.variant:
			bld.variant = 'debug'
			logs.warn ('Defaulting to \'debug\' build variant')
			logs.warn ('Do "waf debug" or "waf release" to avoid this warning')
	else:
		if not os.geteuid()==0:
			logs.warn ('You most likely need root privileges to install or uninstall '+APPNAME+' properly.')



	nodes =  bld.path.ant_glob(\
	    ['src/*.c',
	     'src/gplot/*.c',
	     'src/engines/*.c',
	     'src/sheet/*.c',
	     'src/model/*.c'],
	     excl='*/main.c')

	bld.objects (
		['c','glib2'],
		source = nodes,
		includes = ['src/', 'src/engines/', 'src/gplot/', 'src/model/', 'src/sheet/'],
		export_includes = ['src/', 'src/engines/', 'src/gplot/', 'src/model/', 'src/sheet/'],
		uselib = 'M XML GOBJECT GLIB GTK3 XML GOOCANVAS GTKSOURCEVIEW3',
		target = 'shared_objects'
	)

	exe = bld.program(
		features = ['c', 'glib2'],
		target = APPNAME,
		source = ['src/main.c'],
		includes = ['src/', 'src/engines/', 'src/gplot/', 'src/model/', 'src/sheet/'],
		export_includes = ['src/', 'src/engines/', 'src/gplot/', 'src/model/', 'src/sheet/'],
		use = 'shared_objects',
		uselib = 'M XML GOBJECT GLIB GTK3 XML GOOCANVAS GTKSOURCEVIEW3',
		settings_schema_files = ['data/settings/'+bld.env.gschema_name],
		install_path = "${BINDIR}"
	)

	for item in exe.includes:
		logs.debug(item)
	test = bld.program(
		features = ['c', 'glib2', 'unites'],
		target = 'microtests',
		source = ['test/test.c'],
		includes = ['src/', 'src/engines/', 'src/gplot/', 'src/model/', 'src/sheet/'],
		export_includes = ['src/', 'src/engines/', 'src/gplot/', 'src/model/', 'src/sheet/'],
		use = 'shared_objects',
		uselib = 'M XML GOBJECT GLIB GTK3 XML GOOCANVAS GTKSOURCEVIEW3'
	)

	bld.add_post_fun(unites_summary)



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
		os.system ("G_DEBUG=resident-modules,always-malloc "+ctx.env.VALGRIND[0]+" --tool=massif --depth=10 --max-snapshots=1000 --alloc-fn=g_malloc --alloc-fn=g_realloc --alloc-fn=g_try_malloc --alloc-fn=g_malloc0 --alloc-fn=g_mem_chunk_alloc --threshold=0.01 build/debug/ ./build/debug/"+APPNAME+" --debug-all")
	else:
		logs.warn ("Did not find \"massif\". Re-configure if you installed it in the meantime.")




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
		logs.warn ("Did not find \"clang-format\". Re-configure if you installed it in the meantime.")


import platform
from waflib.Context import Context
import re
def bumprpmver_fun(ctx):

	spec = ctx.path.find_node('oregano.spec')
	data = None
	with open(spec.abspath()) as f:
		data = f.read()

	if data:
		data = (re.sub(r'^(\s*Version\s*:\s*)[\w.]+\s*', r'\1 {0}\n'.format(VERSION), data, flags=re.MULTILINE))

		with open(spec.abspath(),'w') as f:
			f.write(data)
	else:
		logs.warn("Didn't find that spec file: '{0}'".format(spec.abspath()))


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

class bumprpmver(Context):
	"""Bump version"""
	cmd = 'bumprpmver'
	fun = 'bumprpmver_fun'


def builddeps_fun(ctx):
	pl = platform.linux_distribution()
	if pl[0] == 'Fedora':
		os.system('yum install gtk3-devel libxml2-devel gtksourceview3-devel intltool glib2-devel goocanvas2-devel desktop-file-utils')
	elif pl[0] == 'Ubuntu':
		os.system('apt-get install libglib2.0-dev intltool libgtk-3-dev libxml2-dev libgoocanvas-2.0-dev libgtksourceview-3.0-dev gnucap')
	else:
		logs.warn ('Unknown Linux distribution. Do nothing.')

class builddeps(Context):
	"""Install build dependencies"""
	cmd = 'builddeps'
	fun = 'builddeps_fun'
