
lib_LTLIBRARIES=libgtkcairoplot.la

INCLUDES = 						\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\" 	\
	-I$(includedir) $(GNOME_INCLUDEDIR)		\
  -DOREGANO_GLADEDIR=\""$(oreganodir)/glade"\"	\
  -DOREGANO_LIBRARYDIR=\""$(oreganodir)/libraries"\" \
	-DOREGANO_MODELDIR=\""$(oreganodir)/models"\" \
	-DDATADIR=\""$(datadir)"\" \
	$(OREGANO_CFLAGS)

libgtkcairoplot_la_SOURCES= \
	gtkcairoplot.c \
	gtkcairoplot.h \
	gtkcairoplotitems.c \
	gtkcairoplotitems.h \
	gtkcairoplotmodel.c \
	gtkcairoplotmodel.h \
	gtkcairoplotview.c \
	gtkcairoplotview.h

libgtkcairoplot_la_LIBADD= \
	$(OREGANO_LIBS)

