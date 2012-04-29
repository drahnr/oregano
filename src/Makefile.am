SUBDIRS = 		\
	gplot		\
	engines		\
	model		\
	sheet		\
	stock		\
	pixmaps

AM_CFLAGS = -Wall -DG_DISABLE_DEPRECATED -DGSEAL_ENABLE \
	-DGDK_PIXBUF_DISABLE_DEPRECATED -DGDK_DISABLE_DEPRECATED \
	-DGTK_DISABLE_DEPRECATED \
	-DG_DISABLE_SINGLE_INCLUDES -DGDK_PIXBUF_DISABLE_SINGLE_INCLUDES \
	-DGTK_DISABLE_SINGLE_INCLUDES

oreganodir = $(datadir)/oregano

INCLUDES = 	\
	-DVERSION="\"$(VERSION)\"" \
	-DPACKAGE=\""oregano\""	\
	-DGETTEXT_PACKAGE=\""oregano\""	\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\" 	\
	-I$(includedir) $(GNOME_INCLUDEDIR)		\
	-DOREGANO_UIDIR=\""$(oreganodir)/xml"\"	\
	-DOREGANO_LIBRARYDIR=\""$(oreganodir)/libraries"\" \
	-DOREGANO_MODELDIR=\""$(oreganodir)/models"\" \
	-DOREGANO_LANGDIR=\""$(oreganodir)/language-specs"\" \
	-DDATADIR=\""$(datadir)"\" \
	-I./sheet \
	-I./gplot \
	-I./model \
	-I./engines \
	-I. \
	$(OREGANO_CFLAGS)

bin_PROGRAMS = oregano

OREGANO_LIBS += gplot/libgplot.a \
		engines/libengines.a \
		model/libmodel.a \
		sheet/libsheet.a

oregano_PIXMAPS = \
	plot.xpm 	\
	tool_wire.xpm	\
	tool_part.xpm	\
	tool_arrow.xpm \
	tool_text.xpm \
	mini_icon.xpm \
	logo.png

oregano_SOURCES = 	\
	clipboard.h \
	clipboard.c \
	cursors.c \
	cursors.h \
	dialogs.c \
	dialogs.h \
	errors.c \
	errors.h \
	file.c \
	file.h \
	file-manager.c \
	file-manager.h \
	load-common.h \
	load-library.c \
	load-library.h \
	load-schematic.c \
	load-schematic.h \
	main.c \
	netlist-editor.c \
	netlist-editor.h \
	oregano-config.c \
	oregano-config.h \
	oregano-utils.c \
	oregano-utils.h \
	part-browser.c \
	part-browser.h \
	plot.c \
	plot.h \
	save-schematic.c \
	save-schematic.h \
	schematic-view.c \
	schematic-view.h \
	schematic-view-menu.h \
	settings.c \
	settings.h \
	sim-settings.c \
	sim-settings.h \
	simulation.c \
	simulation.h \
	splash.h \
	splash.c \
	stock.c \
	stock.h \
	xml-compat.h \
	xml-helper.c \
	xml-helper.h \
	oregano.c \
	oregano.h

oregano_LDADD = \
	$(OREGANO_LIBS)
