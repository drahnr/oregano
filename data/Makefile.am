SUBDIRS = libraries models glade mime

desktopdir = $(datadir)/applications
desktop_in_files = oregano.desktop.in
desktop_DATA = oregano.desktop
@INTLTOOL_DESKTOP_RULE@

EXTRA_DIST = \
	$(desktop_in_files)

CLEANFILES = \
	$(desktop_DATA)

DISTCLEANFILES = \
	$(desktop_DATA)
