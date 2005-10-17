
lib_LTLIBRARIES=liboregano-model.la

INCLUDES = 						\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\" 	\
	-I$(includedir) $(GNOME_INCLUDEDIR)		\
	-I../ \
	-I../sheet \
  -DOREGANO_GLADEDIR=\""$(oreganodir)/glade"\"	\
  -DOREGANO_LIBRARYDIR=\""$(oreganodir)/libraries"\" \
	-DOREGANO_MODELDIR=\""$(oreganodir)/models"\" \
	-DDATADIR=\""$(datadir)"\" \
	$(OREGANO_CFLAGS)

liboregano_model_la_SOURCES= \
	item-data.c \
	item-data.h \
	node.c \
	node.h \
	node-store.c \
	node-store.h \
	part.c \
	part.h \
	part-private.h \
	part-property.h \
	part-property.c \
	schematic.c \
	schematic.h \
	textbox.c \
	textbox.h \
	wire.c \
	wire.h \
	wire-private.h

liboregano_model_la_LIBADD= \
	$(OREGANO_LIBS)

