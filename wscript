#! /usr/bin/env python
# encoding: utf-8

VERSION = '0.7.3'
APPNAME = 'oregano'

top = '.'
out = 'build'

import os
from waflib import Logs as logs
from waflib import Utils as utils
	
def options(ctx):
	ctx.load('compiler_c gnu_dirs glib2 intltool')

	ctx.add_option('--run', action='store_true', default=False, help='Run imediatly if the build succeeds')
	ctx.add_option('--gnomelike', action='store_true', default=True, help='Determines if gnome shemas and gnome iconcache should be installed')



def configure(ctx):
	ctx.load('compiler_c gnu_dirs glib2 intltool')
	ctx.env.appname = APPNAME
	ctx.env.version = VERSION

	ctx.define('VERSION', VERSION)
	ctx.define('GETTEXT_PACKAGE', APPNAME)


	#things the applications needs to know about, for easier re-use in subdir wscript(s)
	ctx.env.path_ui = utils.subst_vars('${DATADIR}/oregano/ui/', ctx.env)
	ctx.env.path_model = utils.subst_vars('${DATADIR}/oregano/models/', ctx.env)
	ctx.env.path_partslib = utils.subst_vars('${DATADIR}/oregano/library/', ctx.env)
	ctx.env.path_lang = utils.subst_vars('${DATADIR}/oregano/language-specs/', ctx.env)
	ctx.env.path_examples =  utils.subst_vars('${DATADIR}/oregano/examples/', ctx.env)
#	ctx.env.path_icons = '${DATADIR}/oregano/icons/'
#	ctx.env.path_mime = '${DATADIR}/oregano/mime/'
#	ctx.env.path_locale = '${DATADIR}/oregano/locale/'

	#define the above paths so the application does know about files locations
	ctx.define('OREGANO_UIDIR', ctx.env.path_ui)
	ctx.define('OREGANO_MODELDIR', ctx.env.path_model)
	ctx.define('OREGANO_LIBRARYDIR', ctx.env.path_partslib)
	ctx.define('OREGANO_LANGDIR', ctx.env.path_lang)
	ctx.define('OREGANO_EXAMPLEDIR', ctx.env.path_examples)
#	ctx.define('OREGANO_ICONDIR', ctx.env.path_icons)
#	ctx.define('OREGANO_MIMEDIR', ctx.env.path_mime)
#	ctx.define('OREGANO_LOCALEDIR', ctx.env.path_locale)




	ctx.check_cc(lib='m', uselib_store='M', mandatory=True)

	ctx.check_cfg(atleast_pkgconfig_version='0.26')
	ctx.check_cfg(package='glib-2.0', uselib_store='GLIB', args=['glib-2.0 >= 2.24', '--cflags', '--libs'], mandatory=True)
	ctx.check_cfg(package='gobject-2.0', uselib_store='GOBJECT', args=['--cflags', '--libs'], mandatory=True)
	ctx.check_cfg(package='gtk+-3.0', uselib_store='GTK3', args=['--cflags', '--libs'], mandatory=True)
	ctx.check_cfg(package='libxml-2.0', uselib_store='XML', args=['--cflags', '--libs'], mandatory=True)
	ctx.check_cfg(package='goocanvas-2.0', uselib_store='GOOCANVAS', args=['--cflags', '--libs'], mandatory=True)
	ctx.check_cfg(package='gtksourceview-3.0', uselib_store='GTKSOURCEVIEW3', args=['--cflags', '--libs'], mandatory=True)

	ctx.check_large_file(mandatory=False)
	ctx.check_endianness(mandatory=False)
	ctx.check_inline(mandatory=False)

	# -ggdb vs -g -- http://stackoverflow.com/questions/668962
	ctx.setenv('debug', env=ctx.env.derive())
	ctx.env.CFLAGS = ['-ggdb', '-Wall']
	ctx.define('DEBUG',1)

	ctx.setenv('release', env=ctx.env.derive())
	ctx.env.CFLAGS = ['-O2', '-Wall']
	ctx.define('RELEASE',1)







import re
def locate(regex, root=os.curdir, exclpattern="((.*/)?\.(git|svn|cvs).*)|(.*/.+~)|(.*/intl/.*)"):
	lst = []
	for dir_, dirnames, filenames in os.walk(root):
		for filename in filenames:
			abspath = os.path.join(dir_, filename)
			if not re.match(exclpattern, abspath):
				if re.match(regex, abspath):
					if (abspath[0:2]=="./"):
						lst.append(abspath[2:])
					else:
						lst.append(abspath)
	return lst


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


def build(bld):
	bld.add_pre_fun(pre)
	bld.add_post_fun(post)
	bld.recurse(['po','data'])

	if bld.cmd != 'install' and bld.cmd != 'uninstall':
		if not bld.variant:
			bld.variant = 'debug'
			logs.warn ('Defaulting to \'debug\' build variant')
			logs.warn ('Do "waf debug" or "waf release" to avoid this warning')
		if os.geteuid()==0:
			logs.fatal ('Do not run "' + ctx.cmd + '" as root, just don\'t!. Aborting.')
	else:
		if not os.geteuid()==0:
			logs.warn ('You most likely need root privileges to install or uninstall '+APPNAME+' properly.')


	exe = bld.program(
 		features = ['c', 'cprogram', 'glib2'],
		target = APPNAME,
		source = bld.path.ant_glob(['src/*.c', 'src/engines/*.c', 'src/gplot/*.c', 'src/model/*.c', 'src/sheet/*.c']),
		includes = ['src/', 'src/engines/', 'src/gplot/', 'src/model/', 'src/sheet/'],
		export_includes = ['src/', 'src/engines/', 'src/gplot/', 'src/model/', 'src/sheet/'],
		uselib = 'M XML GOBJECT GLIB GTK3 XML GOOCANVAS GTKSOURCEVIEW3',
		settings_schema_files = ['data/settings/apps.oregano.gschema.xml'],
		install_path = "${BINDIR}"
	)
	for item in exe.includes:
		logs.debug(item)





from waflib.Build import BuildContext

class release(BuildContext):
	      cmd = 'release'
	      variant = 'release'

class debug(BuildContext):
	      cmd = 'debug'
	      variant = 'debug'
