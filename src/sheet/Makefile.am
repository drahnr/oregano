oreganodir = $(datadir)/oregano

AM_CFLAGS = -Wall -DG_DISABLE_DEPRECATED -DGSEAL_ENABLE \
	-DGDK_PIXBUF_DISABLE_DEPRECATED -DGDK_DISABLE_DEPRECATED \
	-DGTK_DISABLE_DEPRECATED -DG_DISABLE_SINGLE_INCLUDES \
	-DGDK_PIXBUF_DISABLE_SINGLE_INCLUDES -DGTK_DISABLE_SINGLE_INCLUDES

INCLUDES = \
	$(OREGANO_CFLAGS) -I$(top_srcdir)/src -I$(top_srcdir)/src/model \
	-I$(top_srcdir)/src/engines \
	-DOREGANO_UIDIR=\""$(oreganodir)/xml"\"  \
	-DOREGANO_LIBRARYDIR=\""$(oreganodir)/libraries"\" \
	-DOREGANO_MODELDIR=\""$(oreganodir)/models"\"

noinst_LIBRARIES = libsheet.a

libsheet_a_SOURCES = \
	create-wire.h	\
	create-wire.c	\
	grid.c	\
	grid.h	\
	node-item.c	\
	node-item.h	\
	part-item.c	\
	part-item.h	\
	plot-add-function.c \
	plot-add-function.h \
	sheet.c \
	sheet.h \
	sheet-item.c \
	sheet-item.h \
	sheet-item-factory.c \
	sheet-item-factory.h \
	sheet-private.h \
	textbox-item.c	\
	textbox-item.h	\
	wire-item.c \
	wire-item.h
