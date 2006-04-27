import os;
import SCons;

# APP Version #
VERSION = '0.50.0'

# Command line options #
opts = Options ('oregano.conf');
opts.Add (PathOption ('PREFIX', 'System base prefix path', '/usr'));
opts.Add (PathOption ('DESTDIR', 'System base installation path', '/'));

# Dependencies #
deps = []
deps.append ({'lib': 'gtk+-2.0',            'ver': '2.8.0'})
deps.append ({'lib': 'libglade-2.0',        'ver': '1.99.4'})
deps.append ({'lib': 'libgnomeui-2.0',      'ver': '1.107.0'})
deps.append ({'lib': 'libgnomeprint-2.2',   'ver': '1.106.0'})
deps.append ({'lib': 'libxml-2.0',          'ver': '0.14.0'})
deps.append ({'lib': 'libgnomecanvas-2.0',  'ver': '2.2'})
deps.append ({'lib': 'gtksourceview-1.0',   'ver': '1.0'})
deps.append ({'lib': 'cairo',               'ver': '1.0.0'})
deps.append ({'lib': 'libgnomeprintui-2.2', 'ver':'1.106.0'})

# CUSTOM CHECK FUNC #
def CheckPkg (context, pkg, version):
	msg = 'Checking for pkg %s >= %s ... ' % (pkg, version)
	context.Message ('   %-60s ' % (msg));
	result = os.system ('pkg-config --exists \'%s >= %s\'' % (pkg, version) );
	if result == 0:
		result = 'yes';
	else:
		result = 0;
	context.Result(result)
	return result;

CEnv = Environment (options = opts);

CEnv.Append (CCFLAGS = Split ('-Wall'));

# Check dependencies #
if not CEnv.GetOption ('clean'):
	Cconf = Configure (CEnv, custom_tests = {'CheckPkg' : CheckPkg })

	for dep in deps:
		if not Cconf.CheckPkg (dep['lib'], dep['ver']): Exit(1);

	Cconf.Finish ();

# Create compiler command line from dependencies #
for dep in deps:
	CEnv.ParseConfig ('pkg-config --cflags --libs '+dep['lib']);

# Configure environment #
DataDir = CEnv['PREFIX']+'/share'
OreganoDir = DataDir+'/oregano'

CEnv['DATADIR'] = DataDir 
CEnv['OREGANODIR'] = OreganoDir
CEnv['VERSION'] = VERSION

Export ('CEnv')

SConscript ('src/SConscript');
SConscript ('data/SConscript');

