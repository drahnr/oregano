
lib_LTLIBRARIES=liboregano-sheet.la

oreganodir = $(datadir)/oregano

INCLUDES = 						\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\" 	\
	-I$(includedir) $(GNOME_INCLUDEDIR)		\
	-I.. \
	-I../model \
  -DOREGANO_GLADEDIR=\""$(oreganodir)/glade"\"	\
  -DOREGANO_LIBRARYDIR=\""$(oreganodir)/libraries"\" \
	-DOREGANO_MODELDIR=\""$(oreganodir)/models"\" \
	-DDATADIR=\""$(datadir)"\" \
	$(OREGANO_CFLAGS)

liboregano_sheet_la_SOURCES= \
	grid.c \
	grid.h \
	node-item.c \
	node-item.h \
	part-item.c \
	part-item.h \
	sheet.c \
	sheet.h \
	sheet-item.c \
	sheet-item-factory.c \
	sheet-item-factory.h \
	sheet-item.h \
	sheet-private.h \
	textbox-item.c \
	textbox-item.h \
	wire-item.c \
	wire-item.h

liboregano_sheet_la_LIBADD= \
	$(OREGANO_LIBS)

