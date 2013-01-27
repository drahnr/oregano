#! /usr/bin/env python
# encoding: utf-8

VERSION = '0.7.3'
APPNAME = 'oregano'

top = '.'
out = 'build'

import os
from waflib import Logs as logs
	
def options(ctx):
	ctx.load('compiler_c gnu_dirs glib2')
	ctx.add_option('--destdir', action='store', default='', help='A global prefix prepended for all and every position string. Be aware that changerooting into that destdir will leave you with an unasable binary due to messy paths ui, partslib, model, locale and lang directories', dest='destdir')
	ctx.add_option('--prefix', action='store', default='/usr', help='The prefix which will be utilized for the install target, except those which are dependant of a subsequent prefix')
	ctx.add_option('--prefix-ui', action='store', default='/usr/share', help='Prefix for gtkbuilder ui files')
	ctx.add_option('--prefix-partslib', action='store', default='/usr/share', help='Prefix for the parts library shipped with citronell')
	ctx.add_option('--prefix-model', action='store', default='/usr/share', help='Prefix for custom models which will be used by oregano in addition to the parts library')
	ctx.add_option('--prefix-locale', action='store', default='/usr/share', help='Prefix for the localisation/translation files')
	ctx.add_option('--prefix-lang', action='store', default='/usr/share', help='Prefix for the gtksourceview hightlighting rules file')
	ctx.add_option('--prefix-icons', action='store', default='/usr/share', help='Prefix for icons')
	ctx.add_option('--prefix-mime', action='store', default='/usr/share', help='Prefix for mime register magic')
	ctx.add_option('--prefix-examples', action='store', default='/usr/share', help='Prefix for the examples subdirectory')

	ctx.add_option('--run', action='store_true', default=False, help='Run imediatly if the build succeeds')
	ctx.add_option('--gnomelike', action='store_true', default=True, help='Determines if gnome shemas and gnome iconcache should be installed')



def configure(ctx):
	ctx.load('compiler_c gnu_dirs glib2')

	ctx.define('VERSION', VERSION)
	ctx.define('GETTEXT_PACKAGE', APPNAME)

	ctx.env.path_prefix = os.path.join (ctx.options.destdir, ctx.options.prefix, '')
	ctx.define('OREGANO_PREFIX', ctx.env.path_prefix)

	ctx.env.path_ui = os.path.join (ctx.options.destdir, ctx.options.prefix_ui, 'oregano/ui/')
	ctx.define('OREGANO_UIDIR', ctx.env.path_ui)

	ctx.env.path_model = os.path.join (ctx.options.destdir, ctx.options.prefix_model, 'oregano/models/')
	ctx.define('OREGANO_MODELDIR', ctx.env.path_model)

	ctx.env.path_locale = os.path.join (ctx.options.destdir, ctx.options.prefix_locale, 'oregano/locale/')
	ctx.define('OREGANO_LOCALEDIR', ctx.env.path_locale)

	ctx.env.path_partslib = os.path.join (ctx.options.destdir, ctx.options.prefix_partslib, 'oregano/library/')
	ctx.define('OREGANO_LIBRARYDIR', ctx.env.path_partslib)

	ctx.env.path_lang = os.path.join (ctx.options.destdir, ctx.options.prefix_lang, 'oregano/language-specs/')
	ctx.define('OREGANO_LANGDIR', ctx.env.path_lang)

	ctx.env.path_icons = os.path.join (ctx.options.destdir, ctx.options.prefix_icons, 'oregano/icons/')
	ctx.define('OREGANO_ICONDIR', ctx.env.path_icons)

	ctx.env.path_mime = os.path.join (ctx.options.destdir, ctx.options.prefix_mime, 'oregano/mime/')
	ctx.define('OREGANO_MIMEDIR', ctx.env.path_mime)

	ctx.env.path_examples = os.path.join (ctx.options.destdir, ctx.options.prefix_examples, 'oregano/examples/')
	ctx.define('OREGANO_EXAMPLEDIR', ctx.env.path_examples)


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


def translate(ctx):
	f = open ('po/potfiles.in', 'w+')

	lst = locate('.*/.+\.c')
	for item in lst:
		f.write("%s\n" % item)

#TODO gtkbuilder extension (.ui) not recognized properly by xgettext
# Bug #7
#	f.write ("\n\n")
#	lst = locate('.*/.+\.ui')
#	for item in lst:
#		f.write("%s\n" % item)
#		f.write("[type: gettext/glade]%s\n" % item)

	f.close ()
	os.system ('xgettext -k_ -E -f po/potfiles.in --package-name='+APPNAME+' --package-version '+VERSION+' -o po/'+APPNAME+'.pot')


def docs(ctx):
	logs.info("TODO: docs generation is not yet supported")


#def test(ctx):
#	print('test2 ' + ctx.path.abspath())
#	ctx.recurse('src')



#def dist(ctx):
#	ctx.base_name = APPNAME
#	ctx.algo = 'tar.xz'
#	ctx.excl = ['.*', '*~'],
#	ctx.files = cts.path.ant_glob('**/wscript')

def pre(ctx):
	if ctx.cmd != 'install':
		logs.info ('Variant: \'' + ctx.variant + '\'')



def post(ctx):
	if ctx.options.run:
		ctx.exec_command('')


def build(bld):
	bld.add_pre_fun(pre)
	bld.add_post_fun(post)

	if bld.cmd != 'install':
		if not bld.variant:
			bld.fatal('Do "waf debug" or "waf release"')
		if os.geteuid()==0:
			bld.fatal ('Do not run "' + ctx.cmd + '" as root, just don\'t!. Aborting.')
	else:
		if not os.geteuid()==0:
			logs.warn ('You most likely need root privileges to install '+APPNAME+' properly.')

	exe = bld.program(
 		features = ['c', 'cprogram', 'glib2'],
		target = APPNAME+'.bin',
		source = bld.path.ant_glob(['src/*.c', 'src/engines/*.c', 'src/gplot/*.c', 'src/model/*.c', 'src/sheet/*.c']),
		includes = ['src/', 'src/engines/', 'src/gplot/', 'src/model/', 'src/sheet/'],
		export_includes = ['src/', 'src/engines/', 'src/gplot/', 'src/model/', 'src/sheet/'],
		uselib = 'M XML GOBJECT GLIB GTK3 XML GOOCANVAS GTKSOURCEVIEW3',
		settings_schema_files = ['data/settings/apps.oregano.gschema.xml']
	)
	for item in exe.includes:
		logs.debug(item)

	bld.install_files (bld.env.path_mime, locate('(\./)?data/mime/.+\.(keys|xml)'))
	bld.install_files (bld.env.path_icons, locate('(\./)?data/mime/.+\.(png|svg)'))
	bld.install_files (bld.env.path_lang, locate('(\./)?data/.*\.lang'))

	bld.install_files (bld.env.path_model, locate('(\./)?data/models/.+\.model'))
	bld.install_files (bld.env.path_examples, locate('(\./)?data/examples/.+'))
	bld.install_files (bld.env.path_partslib, locate('(\./)?data/libraries/.+\.oreglib'))
	bld.install_files (bld.env.path_ui, locate('(\./)?data/xml/.+\.ui'))



from waflib.Build import BuildContext

class release(BuildContext):
	      cmd = 'release'
	      variant = 'release'

class debug(BuildContext):
	      cmd = 'debug'
	      variant = 'debug'
