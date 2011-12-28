## Process this file with automake to produce Makefile.in

SUBDIRS = po src data help

oreganodocdir = $(datadir)/doc/oregano
oreganodoc_DATA = \
	README\
	COPYING\
	AUTHORS\
	INSTALL\
	NEWS\
	TODO

oregano_datadir = $(datadir)/oregano


EXTRA_DIST = \
	autogen.sh \
	setup-gettext \
	intltool-extract.in\
	intltool-merge.in\
	intltool-update.in

nomerelease:
	scp *.tar.bz2 master.gnome.org:

DISTCHECK_CONFIGURE_FLAGS = --enable-gtk-doc

DISTCLEANFILES = \
	intltool-extract \
	intltool-merge \
	intltool-update
