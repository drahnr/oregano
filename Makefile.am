SUBDIRS = po src help data

MIMEFILES	=	oregano.xml.in

mimedir = $(datadir)/mime/packages
mime_DATA = oregano.xml

CLEANFILES = $(mime_DATA)

@INTLTOOL_XML_RULE@

install-data-hook: 
if UPDATE_MIME
	update-mime-database $(datadir)/mime
else
	@true
endif

EXTRA_DIST = \
	oregano.desktop README COPYING AUTHORS INSTALL TODO \
	oregano.keys.in oregano.mime $(appicon_DATA) oregano.spec \
	$(MIMEFILES)	

appicondir = $(datadir)/pixmaps
appicon_DATA = gnome-oregano.svg

Applicationsdir = $(datadir)/gnome/apps/Applications
Applications_DATA = oregano.desktop

Mimedir = $(datadir)/mime-info
Mime_DATA = oregano.keys oregano.mime

oregano.keys: oregano.keys.in Makefile
	sed -e 's^\@icondir\@^$(appicondir)^g' < $(srcdir)/oregano.keys.in > oregano.keys.tmp \
	&& mv oregano.keys.tmp oregano.keys 



