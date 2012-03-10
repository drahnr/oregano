SUBDIRS = libraries models mime xml

desktopdir = $(datadir)/applications
desktop_in_files = oregano.desktop.in
desktop_DATA = oregano.desktop
prefs_keyfile = apps.oregano.gschema.xml
@INTLTOOL_DESKTOP_RULE@

EXTRA_DIST = \
	$(desktop_in_files)\
	$(prefs_keyfile)

CLEANFILES = \
	$(desktop_DATA)

DISTCLEANFILES = \
	$(desktop_DATA)
