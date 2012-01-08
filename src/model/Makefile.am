oreganodir = $(datadir)/oregano

AM_CFLAGS = -Wall -DG_DISABLE_DEPRECATED -DGSEAL_ENABLE \
	-DGDK_PIXBUF_DISABLE_DEPRECATED -DGDK_DISABLE_DEPRECATED \
	-DGTK_DISABLE_DEPRECATED \
	-DG_DISABLE_SINGLE_INCLUDES -DGDK_PIXBUF_DISABLE_SINGLE_INCLUDES \
	-DGTK_DISABLE_SINGLE_INCLUDES

INCLUDES =  						\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\" 	\
	-I$(includedir) $(GNOME_INCLUDEDIR)		\
	-DOREGANO_UIDIR=\""$(oreganodir)/xml"\"	\
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
