oreganodir = $(datadir)/oregano
INCLUDES =  						\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\" 	\
	-I$(includedir) $(GNOME_INCLUDEDIR)		\
	-DOREGANO_GLADEDIR=\""$(oreganodir)/glade"\"	\
	-DOREGANO_LIBRARYDIR=\""$(oreganodir)/libraries"\" \
	-DOREGANO_MODELDIR=\""$(oreganodir)/models"\" \
	-DDATADIR=\""$(datadir)"\" \
	-DGETTEXT_PACKAGE=\""oregano\"" \
	$(OREGANO_CFLAGS) -I$(top_srcdir)/src -I$(top_srcdir)/src/sheet \
	-I$(top_srcdir)/src/engines

noinst_LIBRARIES = libmodel.a
libmodel_a_SOURCES = \
	item-data.c	\
	item-data.h	\
	node.c	\
	node.h	\
	node-store.c	\
	node-store.h	\
	part.c	\
	part.h	\
	part-property.c	\
	part-property.h	\
	schematic.c	\
	schematic.h	\
	sheet-pos.h	\
	textbox.c	\
	textbox.h	\
	wire.c	\
	wire.h	\
	wire-private.h	\
	part-private.h
#libmodel_a_LIBADD = @LIBMODEL_LIBADD@
libmodel_a_LIBADD =  libmodel.a
