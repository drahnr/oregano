SUBDIRS = 		\
	dialogs		\
	samples		\
	stock		\
	pixmaps

oreganodir = $(datadir)/oregano

INCLUDES = 						\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\" 	\
	-I$(includedir) $(GNOME_INCLUDEDIR)		\
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
	grid.c \
	grid.h \
	item-data.c \
	item-data.h \
	load-common.h \
	load-library.c \
	load-library.h \
	load-schematic.c \
	load-schematic.h \
	main.c \
	main.h \
	netlist.c \
	netlist.h \
	node.c \
	node.h \
	node-item.c \
	node-item.h \
	node-store.c \
	node-store.h \
	oregano-config.c \
	oregano-config.h \
	oregano-utils.c \
	oregano-utils.h \
	part-browser.c \
	part-browser.h \
	part-item.c \
	part-item.h \
	part-label.h \
	part-private.h \
	part-property.c \
	part-property.h \
	part.c \
	part.h \
	plot.c \
	plot.h \
	print.c \
	print.h \
	save-schematic.c \
	save-schematic.h \
	schematic.c \
	schematic.h \
	schematic-view.c \
	schematic-view.h \
	schematic-view-ui.h \
	settings.c \
	settings.h \
	sheet-item.c \
	sheet-item.h \
	sheet-item-factory.c \
	sheet-item-factory.h \
	sheet-pos.h \
	sheet-private.h \
	sheet.c \
	sheet.h \
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
	textbox.c \
	textbox.h \
	textbox-item.c \
	textbox-item.h \
	type-utils.h \
	wire-item.c \
	wire-item.h \
	wire-private.h \
	wire.c \
	wire.h \
	xml-compat.h \
	xml-helper.c \
	xml-helper.h \
	splash.h \
	splash.c \
	netlist-editor.h \
	netlist-editor.c \
	gtkcairoplot.c \
	gtkcairoplot.h \
	gtkcairoplotitems.c \
	gtkcairoplotitems.h \
	gtkcairoplotmodel.c \
	gtkcairoplotmodel.h \
	gtkcairoplotview.c \
	gtkcairoplotview.h 

oregano_LDADD = \
	$(OREGANO_LIBS) 

pspice2oregano_SOURCES = \
	pspice2oregano.c

pspice2oregano_LDADD = \
	$(OREGANO_LIBS) 

