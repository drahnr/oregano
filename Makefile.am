## Process this file with automake to produce Makefile.in

ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}

SUBDIRS = po src data help

oreganodocdir = $(datadir)/doc/oregano
oreganodoc_DATA = \
	README\
	COPYING\
	AUTHORS\
	ChangeLog\
	INSTALL\
	NEWS\
	TODO

oregano_datadir = $(datadir)/oregano


EXTRA_DIST = \
	autogen.sh \
	setup-gettext \
	stamp.h.in \
	xmldocs.make \
	omf.make

nomerelease:
	scp *.tar.bz2 master.gnome.org:

DISTCHECK_CONFIGURE_FLAGS = --enable-gtk-doc

DISTCLEANFILES = \
	intltool-extract \
	intltool-merge \
	intltool-update

clean-local:
	find . -name "*~" | xargs rm -f
	find . -name "*.sav" | xargs rm -rf
	rm -rf autom4te.cache
	find . -name "*.a" | xargs rm -rf
	find . -name "Makefile.in" | xargs rm -rf
	rm -rf configure
