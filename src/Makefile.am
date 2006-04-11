SUBDIRS = 		\
	dialogs		\
	samples		\
	stock		\
	pixmaps \
	model \
	sheet \
	gplot

oreganodir = $(datadir)/oregano

INCLUDES = 						\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\" 	\
	-I$(includedir) $(GNOME_INCLUDEDIR)		\
	-I./model \
	-I./gplot \
	-I./sheet \
  -DOREGANO_GLADEDIR=\""$(oreganodir)/glade"\"	\
  -DOREGANO_LIBRARYDIR=\""$(oreganodir)/libraries"\" \
	-DOREGANO_MODELDIR=\""$(oreganodir)/models"\" \
	-DDATADIR=\""$(datadir)"\" \
	$(OREGANO_CFLAGS)

bin_SCRIPTS = oregano_parser.pl
bin_PROGRAMS = oregano pspice2oregano

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
	file.c \
	file.h \
	load-common.h \
	load-library.c \
	load-library.h \
	load-schematic.c \
	load-schematic.h \
	main.c \
	main.h \
	netlist.c \
	netlist.h \
	oregano-config.c \
	oregano-config.h \
	oregano-utils.c \
	oregano-utils.h \
	part-browser.c \
	part-browser.h \
	plot.c \
	plot.h \
	print.c \
	print.h \
	save-schematic.c \
	save-schematic.h \
	schematic-view.c \
	schematic-view.h \
	schematic-view-ui.h \
	settings.c \
	settings.h \
	sim-engine.c \
	sim-engine.h \
	sim-settings.c \
	sim-settings.h \
	simulation.c \
	simulation.h \
	simulate.c \
	simulate.h \
	smallicon.c \
	smallicon.h \
	stock.c \
	stock.h \
	type-utils.h \
	xml-compat.h \
	xml-helper.c \
	xml-helper.h \
	splash.h \
	splash.c \
	netlist-editor.h \
	netlist-editor.c \
	plot-add-function.h \
	plot-add-function.c \
	file-manager.c \
	file-manager.h \
	errors.h \
	errors.c

oregano_LDADD = \
	model/liboregano-model.la \
	sheet/liboregano-sheet.la \
	gplot/liboregano-gplot.la \
	$(OREGANO_LIBS) 

pspice2oregano_SOURCES = \
	pspice2oregano.c

pspice2oregano_LDADD = \
	$(OREGANO_LIBS) 

