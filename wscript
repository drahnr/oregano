#! /usr/bin/env python
# encoding: utf-8

VERSION = '0.7.3'
APPNAME = 'oregano'

top = '.'
out = 'build'

import os
	
def options(ctx):
	ctx.load('compiler_c')
	ctx.add_option('--destdir', action='store', default='', help='A global prefix prepended for all and every position string. Be aware that changerooting into that destdir will leave you with an unasable binary due to messy paths ui, partslib, model, locale and lang directories', dest='destdir')
	ctx.add_option('--prefix', action='store', default='/usr/local', help='The prefix which will be utilized for the install target, except those which are dependant of a subsequent prefix')
	ctx.add_option('--prefix-ui', action='store', default='/usr/local', help='Prefix for gtkbuilder ui files')
	ctx.add_option('--prefix-partslib', action='store', default='/usr/local', help='Prefix for the parts library shipped with citronell')
	ctx.add_option('--prefix-model', action='store', default='/usr/local', help='Prefix for custom models which will be used by oregano in addition to the parts library')
	ctx.add_option('--prefix-locale', action='store', default='/usr/local', help='Prefix for the localisation/translation files')
	ctx.add_option('--prefix-lang', action='store', default='/usr/local', help='Prefix for the gtksourceview hightlighting rules file')

	ctx.add_option('--run', action='store_true', default=False, help='Run imediatly if the build succeeds')
	ctx.add_option('--gnomelike', action='store_true', default=True, help='Determines if gnome shemas and gnome iconcache should be installed')


def configure(ctx):
	ctx.load('compiler_c gnu_dirs')

	ctx.define('VERSION', VERSION)
	ctx.define('GETTEXT_PACKAGE', APPNAME)

	ctx.define('OREGANO_PREFIX', os.path.join (ctx.options.destdir, ctx.options.prefix, ''))
	ctx.define('OREGANO_UIDIR', os.path.join (ctx.options.destdir, ctx.options.prefix_ui, 'oregano/ui/'))
	ctx.define('OREGANO_MODELDIR', os.path.join (ctx.options.destdir, ctx.options.prefix_model, 'oregano/models/'))
	ctx.define('OREGANO_LOCALEDIR', os.path.join (ctx.options.destdir, ctx.options.prefix_locale, 'oregano/locale/'))
	ctx.define('OREGANO_LIBRARYDIR', os.path.join (ctx.options.destdir, ctx.options.prefix_partslib, 'oregano/library/'))
	ctx.define('OREGANO_LANGDIR', os.path.join (ctx.options.destdir, ctx.options.prefix_lang, 'oregano/language-specs/'))

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




def pre(ctx):
	print ('Building [[[' + ctx.variant + ']]] ...')



def post(ctx):
	print ('Building is complete.')
	if ctx.options.run:
	  ctx.exec_command('')


def translate(ctx):
	os.system ('xgettext -k_ -f ./locale/potfiles.in -D . --package-name='+APPNAME+' --package-version '+VERSION+' -o locale/'+APPNAME+'.pot')


#def test(ctx):
#	print('test2 ' + ctx.path.abspath())
#	ctx.recurse('src')



#def dist(ctx):
#	ctx.base_name = 'quoo'
#	ctx.algo = 'tar.xz'
#	ctx.excl = ''
#	ctx.files = cts.path.ant_glob('**/wscript')



def build(ctx):
	ctx.add_pre_fun(pre)
	ctx.add_post_fun(post)

	if not ctx.variant:
		ctx.fatal('Do "waf debug" or "waf release"')

	exe = ctx.program(
 		features = ['c', 'cprogram'],
		target = APPNAME+'.bin',
		source = ctx.path.ant_glob(['src/*.c', 'src/engines/*.c', 'src/gplot/*.c', 'src/model/*.c', 'src/sheet/*.c']),
		includes = ['src/', 'src/engines/', 'src/gplot/', 'src/model/', 'src/sheet/'],
		export_includes = ['src/', 'src/engines/', 'src/gplot/', 'src/model/', 'src/sheet/'],
		uselib = 'M XML GOBJECT GLIB GTK3 XML GOOCANVAS GTKSOURCEVIEW3'
	)
	for item in exe.includes:
		print(item)



from waflib.Build import BuildContext

class release(BuildContext):
	      cmd = 'release'
	      variant = 'release'

class debug(BuildContext):
	      cmd = 'debug'
	      variant = 'debug'
