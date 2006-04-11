
lib_LTLIBRARIES=liboregano-gplot.la

oreganodir = $(datadir)/oregano

INCLUDES = 						\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\" 	\
	-I$(includedir) $(GNOME_INCLUDEDIR)		\
  -DOREGANO_GLADEDIR=\""$(oreganodir)/glade"\"	\
  -DOREGANO_LIBRARYDIR=\""$(oreganodir)/libraries"\" \
	-DOREGANO_MODELDIR=\""$(oreganodir)/models"\" \
	-DDATADIR=\""$(datadir)"\" \
	$(OREGANO_CFLAGS)

liboregano_gplot_la_SOURCES= \
	gplot.c \
	gplotfunction.c \
	gplotfunction.h \
	gplot.h \
	gplotlines.c \
	gplotlines.h

liboregano_gplot_la_LIBADD= \
	$(OREGANO_LIBS)

