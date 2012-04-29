SUBDIRS = libraries models mime xml

# gsettings_SCHEMAS is a list of all the schemas you want to install
gsettings_SCHEMAS = settings/apps.oregano.gschema.xml
# include the appropriate makefile rules for schema handling
@GSETTINGS_RULES@

desktopdir = $(datadir)/applications
desktop_in_files = oregano.desktop.in
desktop_DATA = oregano.desktop
@INTLTOOL_DESKTOP_RULE@

EXTRA_DIST = \
	$(desktop_in_files)\
	$(prefs_keyfile)

CLEANFILES = \
	$(desktop_DATA)

DISTCLEANFILES = \
	$(desktop_DATA)
