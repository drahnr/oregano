oreganodir = $(datadir)/oregano
INCLUDES = \
	$(OREGANO_CFLAGS) -I$(top_srcdir)/src \
	-I$(top_srcdir)/src/model -I$(top_srcdir)/src/sheet \
	-DOREGANO_XMLDIR=\""$(oreganodir)/xml"\"    \
	-DOREGANO_LIBRARYDIR=\""$(oreganodir)/libraries"\" \
	-DOREGANO_MODELDIR=\""$(oreganodir)/models"\"

noinst_LIBRARIES = libengines.a
libengines_a_SOURCES = \
	engine.c	\
	engine.h	\
	gnucap.c	\
	gnucap.h	\
	netlist.c \
	netlist.h \
	ngspice.c \
	ngspice.h

libengines_a_LIBADD = libengines.a
