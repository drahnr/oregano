oreganodir = $(datadir)/oregano

AM_CFLAGS = -g -Wall -DG_DISABLE_DEPRECATED -DGSEAL_ENABLE \
	-DGDK_PIXBUF_DISABLE_DEPRECATED -DGDK_DISABLE_DEPRECATED \
	-DGTK_DISABLE_DEPRECATED \
	-DG_DISABLE_SINGLE_INCLUDES -DGDK_PIXBUF_DISABLE_SINGLE_INCLUDES \
	-DGTK_DISABLE_SINGLE_INCLUDES

INCLUDES = \
	$(OREGANO_CFLAGS) -I$(top_srcdir)/src \
	-I$(top_srcdir)/src/model -I$(top_srcdir)/src/sheet \
	-DOREGANO_XMLDIR=\""$(oreganodir)/xml"\"    \
	-DOREGANO_LIBRARYDIR=\""$(oreganodir)/libraries"\" \
	-DOREGANO_MODELDIR=\""$(oreganodir)/models"\"

noinst_LIBRARIES = libengines.a
libengines_a_SOURCES = 		\
	engine.c		\
	engine-internal.h	\
	engine.h		\
	gnucap.c		\
	gnucap.h		\
	netlist-helper.c	\
	netlist-helper.h	\
	ngspice-analysis.c	\
	ngspice.c 		\
	ngspice-analysis.h	\
	ngspice.h
