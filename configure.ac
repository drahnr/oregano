AC_INIT(AUTHORS)
AM_INIT_AUTOMAKE(oregano, 0.81)

AM_CONFIG_HEADER(config.h)
AM_MAINTAINER_MODE

AC_CONFIG_MACRO_DIR([m4])

m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])])œ

GETTEXT_PACKAGE=oregano
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE,"$GETTEXT_PACKAGE",
	[GetText version number])

dnl Management of gsettings
GLIB_GSETTINGS


AC_ISC_POSIX
AC_PROG_CC
AC_HEADER_STDC

AC_PROG_INTLTOOL([0.40.1])

AM_PROG_LIBTOOL

# Just run true if these don't exist
AC_PATH_PROG(UPDATE_MIME_DATABASE, update-mime-database, true)
AC_PATH_PROG(UPDATE_DESKTOP_DATABASE, update-desktop-database, true)

ALL_LINGUAS="ca de es fr it ja pt_BR ro ru sv"
AM_GLIB_GNU_GETTEXT

dnl ###########################################################################
dnl Check for documentation requirements
dnl ###########################################################################
AC_SUBST(SCROLLKEEPER_REQUIRED)
AC_PATH_PROG(SCROLLKEEPER_CONFIG, scrollkeeper-config,no)
if test x$SCROLLKEEPER_CONFIG = xno; then
	AC_MSG_ERROR(Couldn't find scrollkeeper-config, please install the scrollkeeper package)
fi


# 
PKG_CHECK_MODULES(OREGANO,
	gtk+-3.0 >= 3.0 \
	libxml-2.0 >= 2.6.0 \
	goocanvas-2.0 >= 2.0 \
	gtksourceview-3.0 >= 3.0)
#cairo >= 1.8.0)
#gconf-2.0 >=  2.28.0 \


# Taken from Screem HTML editor configure.in
AC_ARG_ENABLE(update-mimedb,
   AC_HELP_STRING([--disable-update-mimedb],
   [disable the update-mime-database after install [default=no]]),,
    enable_update_mimedb=yes)
AM_CONDITIONAL(ENABLE_UPDATE_MIMEDB, test x$enable_update_mimedb = xyes)

AC_SUBST(CFLAGS)
AC_SUBST(CPPFLAGS)
AC_SUBST(LDFLAGS)
AC_SUBST(OREGANO_LIBS)
AC_SUBST(OREGANO_CFLAGS)

AC_OUTPUT([
Makefile
data/oregano.desktop.in
src/Makefile
src/gplot/Makefile
src/sheet/Makefile
src/model/Makefile
src/engines/Makefile
src/stock/Makefile
src/pixmaps/Makefile
po/Makefile.in
help/Makefile
help/C/Makefile
help/es/Makefile
help/fr/Makefile
data/Makefile
data/models/Makefile
data/libraries/Makefile
data/examples/Makefile
data/mime/Makefile
data/xml/Makefile
docs/Makefile
stamp.h
])

echo "Configuration:"
echo "	Source code location:			 ${srcdir}"
echo "	Compiler:				 ${CC}"
echo ""
echo "	Install location:			 ${prefix}"
echo ""
echo "Type make to build Oregano."
