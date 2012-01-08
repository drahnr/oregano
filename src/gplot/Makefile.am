oreganodir = $(datadir)/oregano

AM_CFLAGS = -Wall -DG_DISABLE_DEPRECATED -DGSEAL_ENABLE \
	-DGDK_PIXBUF_DISABLE_DEPRECATED -DGDK_DISABLE_DEPRECATED \
	-DGTK_DISABLE_DEPRECATED -DGTK_DISABLE_SINGLE_INCLUDES \
	-DG_DISABLE_SINGLE_INCLUDES -DGDK_PIXBUF_DISABLE_SINGLE_INCLUDES

INCLUDES = \
	$(OREGANO_CFLAGS)

noinst_LIBRARIES = libgplot.a
libgplot_a_SOURCES = \
	gplot.c	\
	gplotfunction.c	\
	gplotfunction.h	\
	gplot-internal.h \
	gplot.h \
	gplotlines.c \
	gplotlines.h
