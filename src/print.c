/*
 * print.c
 *
 *
 * Author:
 *  Richard Hult <rhult@hem.passagen.se>
 *  Ricardo Markiewicz <rmarkie@fi.uba.ar>
 *  Andres de Barbara <adebarbara@fi.uba.ar>
 *
 * Web page: http://arrakis.lug.fi.uba.ar/
 *
 * Copyright (C) 1999-2001  Richard Hult
 * Copyright (C) 2003,2005  LUGFI
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <math.h>
#include <gnome.h>
#include <libgnomeprint/gnome-print.h>
#include "schematic.h"
#include "schematic-view.h"
#include "sheet.h"
#include "node-store.h"
#include "print.h"

#include <libgnomeprintui/gnome-print-dialog.h>
#include <libgnomeprintui/gnome-print-job-preview.h>
#include <libgnomeprint/gnome-print-job.h>



/*
 * This is stolen from Dia:
 */
#define ELLIPSE_CTRL1 0.26521648984
#define ELLIPSE_CTRL2 0.519570402739
#define ELLIPSE_CTRL3 M_SQRT1_2
#define ELLIPSE_CTRL4 0.894643159635

void
print_draw_ellipse (GnomePrintContext *ctx, ArtPoint *center,
	gdouble width, gdouble height)
{
	gdouble x1 = center->x - width/2,  x2 = center->x + width/2;
	gdouble y1 = center->y - height/2, y2 = center->y + height/2;
	gdouble cw1 = ELLIPSE_CTRL1 * width / 2, cw2 = ELLIPSE_CTRL2 * width / 2;
	gdouble cw3 = ELLIPSE_CTRL3 * width / 2, cw4 = ELLIPSE_CTRL4 * width / 2;
	gdouble ch1 = ELLIPSE_CTRL1 * height / 2, ch2 = ELLIPSE_CTRL2 * height / 2;
	gdouble ch3 = ELLIPSE_CTRL3 * height / 2, ch4 = ELLIPSE_CTRL4 * height / 2;

	gnome_print_moveto (ctx, x1, center->y);
	gnome_print_curveto (ctx,
		x1, center->y - ch1,
		center->x - cw4, center->y - ch2,
		center->x - cw3, center->y - ch3
	);
	gnome_print_curveto (ctx,
		center->x - cw2, center->y - ch4,
		center->x - cw1, y1,
		center->x, y1
	);
	gnome_print_curveto (ctx,
		center->x + cw1, y1,
		center->x + cw2, center->y - ch4,
		center->x + cw3, center->y - ch3
	);
	gnome_print_curveto (ctx,
		center->x + cw4, center->y - ch2,
		x2, center->y - ch1,
		x2, center->y
	);
	gnome_print_curveto (ctx,
		x2, center->y + ch1,
		center->x + cw4, center->y + ch2,
		center->x + cw3, center->y + ch3
	);
	gnome_print_curveto (ctx,
		center->x + cw2, center->y + ch4,
		center->x + cw1, y2,
		center->x, y2
	);
	gnome_print_curveto (ctx,
		center->x - cw1, y2,
		center->x - cw2, center->y + ch4,
		center->x - cw3, center->y + ch3
	);
	gnome_print_curveto (ctx,
		center->x - cw4, center->y + ch2,
		x1, center->y + ch1,
		x1, center->y
	);
}

void
print_draw_text (GnomePrintContext *ctx, const gchar *text, ArtPoint *pos)
{
	gnome_print_moveto (ctx, pos->x, pos->y);

	gnome_print_gsave (ctx);
	gnome_print_scale (ctx, 1, -1);
	gnome_print_show (ctx, text);
	gnome_print_grestore (ctx);
}

static void
print_rotule (OreganoPrintContext *opc, ArtDRect *bounds, gdouble lmargin, gdouble bmargin)
{
	gnome_print_newpath (opc->ctx);
	gnome_print_moveto (opc->ctx, bounds->x0, bounds->y0);
	gnome_print_lineto (opc->ctx, bounds->x1, bounds->y0);
	gnome_print_lineto (opc->ctx, bounds->x1, bounds->y1);
	gnome_print_lineto (opc->ctx, bounds->x0, bounds->y1);
	gnome_print_lineto (opc->ctx, bounds->x0, bounds->y0);
	gnome_print_stroke (opc->ctx);
}

static void
print_page (NodeStore *store, OreganoPrintContext *opc, ArtDRect *bounds,
	gdouble lmargin, gdouble bmargin)
{
	if (node_store_count_items (store, bounds) == 0) {
		return;
	}

	gnome_print_beginpage (opc->ctx, "");

	gnome_print_gsave (opc->ctx);

	gnome_print_scale (opc->ctx, 28.346457 * opc->scale, -28.346457 * opc->scale);
	gnome_print_translate (opc->ctx, lmargin/opc->scale - bounds->x0, -bmargin/opc->scale - bounds->y1);

	gnome_print_newpath (opc->ctx);
	gnome_print_moveto (opc->ctx, bounds->x0, bounds->y0);
	gnome_print_lineto (opc->ctx, bounds->x1, bounds->y0);
	gnome_print_lineto (opc->ctx, bounds->x1, bounds->y1);
	gnome_print_lineto (opc->ctx, bounds->x0, bounds->y1);
	gnome_print_lineto (opc->ctx, bounds->x0, bounds->y0);
	gnome_print_clip (opc->ctx);

	print_rotule (opc, bounds, lmargin, bmargin);

	gnome_print_newpath (opc->ctx);
	node_store_print_items (store, opc, bounds);
	node_store_print_labels (store, opc, bounds);

	gnome_print_grestore (opc->ctx);
	gnome_print_showpage (opc->ctx);
}

static void
paginate (SchematicView *sv, OreganoPrintContext *opc, const GnomePrintPaper *paper)
{
	ArtDRect rect;
	const GnomePrintUnit *unit;
	const GnomePrintUnit *psunit;
	gdouble pswidth, psheight, lmargin, tmargin, rmargin, bmargin;
	gdouble width, height;
	gdouble x, y;
	Schematic *sm;
	NodeStore *store;
	gchar *page_h;
	PageProperties *page;

	page = schematic_view_get_page_properties (sv);

	unit = gnome_print_unit_get_by_name ("Centimeter");
	psunit = 	GNOME_PRINT_PS_UNIT;

	page_h = gnome_print_config_get(
		opc->config,
		GNOME_PRINT_KEY_PAGE_ORIENTATION
	);
	
	if (!strcmp(page_h, "R90")) {
		pswidth = paper->height;
		psheight = paper->width;
	} else {
		pswidth = paper->width;
		psheight = paper->height;
	}

	/* convert postscript cordinates into centimeter */
	gnome_print_convert_distance (&pswidth, psunit, unit);
	gnome_print_convert_distance (&psheight, psunit, unit);

	lmargin = 2.5;
	tmargin = 2;
	rmargin = 2;
	bmargin = 2;

	/* Printable area. */
	width = pswidth - lmargin - rmargin;
	height = psheight - tmargin - bmargin;

	sm = schematic_view_get_schematic (sv);
	store = schematic_get_store (sm);
	node_store_get_bounds (store, &rect);

	/* The bounds are a bit off so we add a bit for now. */
	/* TODO : i'm noy sure for this! */
	/* Calculate scale to fix the page! */
	if (page->fit_to_page)
	{
		double sx, sy;
		sx = width / (rect.x1 - rect.x0);
		sy = height / (rect.y1 - rect.y0);
		if (sx < sy)
			opc->scale = sx*0.9;
		else
			opc->scale = sy*0.9;
	} else {
		rect.x0 -= 20;
		rect.y0 -= 20;
		rect.x1 -= 20;
		rect.y1 -= 20;
	}

	/* Center Horizontally? */
	if (page->center_h) {
		gint w, offx;
		
		w = rect.x1 - rect.x0;

		offx = pswidth/2 - w/2;
		rect.x0 = offx;
		rect.x1 = offx+w;
	}
	/* Center Vertically? */
	if (page->center_v) {
		gint h, offy;
		
		h = rect.y1 - rect.y0;

		offy = psheight/2 - h/2;
		rect.y0 = offy;
		rect.y1 = offy+h;
	}

	width /= opc->scale;
	height /= opc->scale;

	for (y = rect.y0; y < rect.y1; y += height) {
		for (x = rect.x0; x < rect.x1; x += width) {
			ArtDRect page_rect;

			page_rect.x0 = x;
			page_rect.x1 = x + width;
			page_rect.y0 = y + tmargin;
			page_rect.y1 = y + height;

			print_page (store, opc, &page_rect, lmargin, bmargin);
		}
	}
}

void
print_schematic (GObject *w, gboolean preview_only)
{
	GtkWidget *dialog;

	SchematicView *sv;
	gchar *paper_name;
	GnomePrintJob *print_job;
	GnomePrintContext *ctx;
	const GnomePrintPaper *paper;
	OreganoPrintContext *opc;
	gboolean preview = FALSE;
	int btn;
	GnomePrintConfig *config;


	sv = SCHEMATIC_VIEW (w);

	dialog = NULL;
	config = NULL;
	paper_name = NULL;
		
	config = gnome_print_config_default ();	

	if (!preview_only) {
		dialog = g_object_new (GNOME_TYPE_PRINT_DIALOG, "print_config", config, NULL);
		gnome_print_dialog_construct (GNOME_PRINT_DIALOG (dialog),
			_("Print"), GNOME_PRINT_DIALOG_RANGE | GNOME_PRINT_DIALOG_COPIES);
					
		//lines = gedit_document_get_line_count (pji->doc);
						
		gnome_print_dialog_construct_range_page (
			GNOME_PRINT_DIALOG (dialog),
		  GNOME_PRINT_RANGE_ALL | GNOME_PRINT_RANGE_RANGE,
			1, 1, "A", _("Pages")
		);

		gtk_window_set_modal (GTK_WINDOW (dialog), TRUE); 
		gtk_window_set_destroy_with_parent (GTK_WINDOW (dialog), TRUE);
		
		btn = gtk_dialog_run (GTK_DIALOG (dialog));

		switch (btn) {
			case GNOME_PRINT_DIALOG_RESPONSE_PRINT:
				g_print ("Imprimiendo\n");
			break;
			case GNOME_PRINT_DIALOG_RESPONSE_PREVIEW:
				preview = TRUE;
			break;
			default:
				gtk_widget_destroy (dialog);
				return;
		}

		/* Preview? */
		//preview = (btn == 1);
		//printer = gnome_printer_widget_get_printer (GNOME_PRINTER_WIDGET (printersel));
		//scale = gtk_spin_button_get_value_as_float (GTK_SPIN_BUTTON (scalewid));
		paper_name = gnome_print_config_get (config, GNOME_PRINT_KEY_PAPER_SIZE);
	} else {
		preview = TRUE;
	}

	print_job = gnome_print_job_new (config);

	/* Get the paper metrics. */
	if (paper_name) {
		paper = gnome_print_paper_get_by_name (paper_name);
		if (paper == NULL) {
			g_warning (_("Can't get paper info! .. getting default!"));
			paper = gnome_print_paper_get_default ();
		}
	} else {
		paper = gnome_print_paper_get_default ();
	}

	ctx = gnome_print_job_get_context (print_job);
	opc = g_new0 (OreganoPrintContext, 1);
	opc->ctx = ctx;
	opc->label_font = gnome_font_face_find_closest("Arial 12");
	opc->config = config;
	opc->scale = 0.03;
	if (!opc->label_font) {
		g_warning (_("Could not create font for printing."));
		goto bail_out;
	} else {
		paginate (sv, opc, paper);
	}

	gnome_print_job_close (print_job);

	if (preview) {
		/*
		 * Unused variable
		gboolean landscape = FALSE;
		*/
		GtkWidget *preview_widget;
		preview_widget = gnome_print_job_preview_new (print_job, _("Print Preview"));
		gtk_widget_show (GTK_WIDGET (preview_widget));
	} else {
		int result = gnome_print_job_print (print_job);
		if (result == -1) {
			g_warning (_("Printing failed."));
		}
	}

 bail_out:
	g_object_unref (G_OBJECT (opc->label_font));
	g_object_unref (G_OBJECT (print_job));
	if (!preview_only) {
		gtk_widget_destroy (dialog);
	}
	g_free (opc);
}

