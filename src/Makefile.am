SUBDIRS = 		\
	gplot		\
	engines		\
	model		\
	sheet		\
	stock		\
	pixmaps

oreganodir = $(datadir)/oregano

INCLUDES = 	\
	-DVERSION="\"$(VERSION)\"" \
	-DPACKAGE=\""oregano\""	\
	-DGETTEXT_PACKAGE=\""oregano\""	\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\" 	\
	-I$(includedir) $(GNOME_INCLUDEDIR)		\
	-DOREGANO_GLADEDIR=\""$(oreganodir)/glade"\"	\
	-DOREGANO_LIBRARYDIR=\""$(oreganodir)/libraries"\" \
	-DOREGANO_MODELDIR=\""$(oreganodir)/models"\" \
	-DOREGANO_LANGDIR=\""$(oreganodir)/language-specs"\" \
	-DDATADIR=\""$(datadir)"\" \
	-I./sheet \
	-I./gplot \
	-I./model \
	-I./engines \
	-I$(top_srcdir)/data/glade \
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
	create-wire.c \
	create-wire.h \
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
	main.h \
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
	plot-add-function.c \
	plot-add-function.h \
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
	smallicon.c \
	smallicon.h \
	splash.h \
	splash.c \
	stock.c \
	stock.h \
	xml-compat.h \
	xml-helper.c \
	xml-helper.h

oregano_LDADD = \
	$(OREGANO_LIBS) 
