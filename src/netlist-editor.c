/*
 * netlist-editor.c
 *
 *
 * Author:
 *  Andres de Barbara <adebarbara@fi.uba.ar>
 *
 * Web page: http://arrakis.lug.fi.uba.ar/
 *
 * Copyright (C) 2004-2008 Ricardo Markiewicz
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

#include "netlist-editor.h"
#include "netlist.h"
#include "simulation.h"
#include "file.h"
#include <glade/glade.h>
#include <stdio.h>
#include <stdlib.h>
#include <gnome.h>
#include <gtksourceview/gtksourceprintcompositor.h>

static void netlist_editor_finalize (GObject *object);
static void netlist_editor_dispose (GObject *object);
static void netlist_editor_class_init (NetlistEditorClass *klass);
static void netlist_editor_finalize (GObject *object);
void netlist_editor_init (NetlistEditor *nle);

static GObjectClass *parent_class = NULL;

struct _NetlistEditorPriv {
	SchematicView *sv;
	gchar * font;
	GdkColor bgcolor, selectcolor, textcolor;
	GtkSourceLanguageManager *lm;
	GtkTextView *view;
	GtkSourceBuffer *buffer;
	GtkWindow *toplevel;
	GtkButton *sim, *save, *close;
};	

static void
netlist_editor_class_init (NetlistEditorClass *klass) {
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS(klass);
	parent_class = g_type_class_peek_parent(klass);

/*	netlist_editor_signals[CHANGED] =
		g_signal_new ("changed",
			G_TYPE_FROM_CLASS (object_class),
			G_SIGNAL_RUN_FIRST,
			G_STRUCT_OFFSET (NetlistEditorClass, changed),
			NULL,
			NULL,
			g_cclosure_marshal_VOID__VOID,
			G_TYPE_NONE, 0);
*/
	object_class->finalize = netlist_editor_finalize;
	object_class->dispose = netlist_editor_dispose;
}

static void
netlist_editor_finalize (GObject *object)
{
	NetlistEditor *nle = NETLIST_EDITOR (object);

	if (nle->priv) {
		/* kill the priv struct */
		if (nle->priv->toplevel) {
			gtk_widget_destroy (GTK_WIDGET (nle->priv->toplevel));
			nle->priv->toplevel = NULL;
		}

		g_free (nle->priv);
	}

	G_OBJECT_CLASS(parent_class)->finalize(object);
}

static void
netlist_editor_dispose (GObject *object)
{
	NetlistEditor *nle = NETLIST_EDITOR(object);

	if (nle->priv) {
	}
	G_OBJECT_CLASS(parent_class)->dispose(object);
}

GType
netlist_editor_get_type (void)
{
	static GType netlist_editor_type = 0;

	if (!netlist_editor_type) {
		static const GTypeInfo netlist_editor_info = {
			sizeof(NetlistEditorClass),
			NULL,
			NULL,
			(GClassInitFunc)netlist_editor_class_init,
			NULL,
			NULL,
			sizeof(NetlistEditor),
			0,
			(GInstanceInitFunc)netlist_editor_init,
			NULL
		};
		netlist_editor_type = g_type_register_static(G_TYPE_OBJECT,
			"NetlistEditor",
			&netlist_editor_info, 0);
	}
	return netlist_editor_type;
}

void
netlist_editor_get_config (NetlistEditor * nle)
{
	GConfClient *gconf_client;
	
	gconf_client = gconf_client_get_default ();
	
	if (gconf_client) {	
		g_object_unref (gconf_client);
	}
}

void
netlist_editor_set_config (NetlistEditor * nle)
{
	GConfClient *gconf_client;
	
	gconf_client = gconf_client_get_default ();
	
	if (gconf_client) {	
		g_object_unref (gconf_client);
	}
}

void
netlist_editor_print (GtkWidget * widget, NetlistEditor * nle)
{
/*	GnomePrintJob *print_job;
	GtkSourcePrintJob *job;
	GtkWidget *preview_widget;
	char *header_left, *header_right;
	Schematic *sm;

	sm = schematic_view_get_schematic (nle->priv->sv);

	job = gtk_source_print_job_new_with_buffer (gnome_print_config_default (), nle->priv->buffer);

	header_left = g_strdup_printf (_("Netlist for %s"), schematic_get_title (sm));
	header_right = g_strdup_printf (_("Author : %s"), schematic_get_author (sm));

	gtk_source_print_job_set_header_format (job,
		header_left,
		NULL,
		header_right,
		FALSE
	);
	gtk_source_print_job_set_print_header (job, TRUE);

	gtk_source_print_job_set_footer_format (job,
		NULL,
		NULL,
		_("Page %N of %Q"),
		FALSE
	);
	gtk_source_print_job_set_print_footer (job, TRUE);

	print_job = gtk_source_print_job_print (job);

	preview_widget = gnome_print_job_preview_new (print_job, _("Print Preview"));
	gtk_widget_show (GTK_WIDGET (preview_widget));
	*/
}

void
netlist_editor_simulate (GtkWidget * widget, NetlistEditor * nle)
{
	Schematic *sm;
	gchar *name;
	GtkTextView *tview;
	GtkTextBuffer *buffer;
	GtkTextIter start, end;
	FILE *fp;
	GError *error = 0;
	OreganoEngine *engine;

	// FIXME: OreganoEngine now override the netlist when start!
	return;

	tview = nle->priv->view;
	buffer = gtk_text_view_get_buffer (tview);

	gtk_text_buffer_get_start_iter (buffer, &start);
	gtk_text_buffer_get_end_iter (buffer, &end);

	sm = schematic_view_get_schematic (nle->priv->sv);
	//name = nl_generate (sm, NULL, &error);

	if (error != NULL) {
		if (g_error_matches (error, OREGANO_ERROR, OREGANO_SIMULATE_ERROR_NO_CLAMP) ||
				g_error_matches (error, OREGANO_ERROR, OREGANO_SIMULATE_ERROR_NO_GND) ||
				g_error_matches (error, OREGANO_ERROR, OREGANO_SIMULATE_ERROR_IO_ERROR)) {
			oregano_error_with_title (_("Could not create a netlist"), error->message);
			g_clear_error (&error);
		} else {
			oregano_error (
				_("An unexpected error has occurred"));
		}
		return;
	}

	/* Save TextBuffer to 'name' */
	fp = fopen (name, "wt");
	if (!fp) {
		gchar *msg;
		msg = g_strdup_printf (_("The file %s could not be saved"), name);
		oregano_error_with_title (_("Could not save temporary netlist file"), msg);
		g_free (msg);
		return;
	}
	
	fputs (gtk_text_buffer_get_text (buffer, &start, &end, FALSE), fp);
	fclose (fp);

	schematic_set_netlist_filename (sm, name);
	simulation_show (NULL, nle->priv->sv);
	g_free (name);
}

void
netlist_editor_save (GtkWidget * widget, NetlistEditor * nle)
{
	char *name;

	name = dialog_netlist_file ((SchematicView *)NULL);

	if (name != NULL) {
		GtkTextView *tview;
		GtkTextBuffer *buffer;
		GtkTextIter start, end;
		FILE *fp;

		tview = nle->priv->view;
		buffer = gtk_text_view_get_buffer (tview);

		gtk_text_buffer_get_start_iter (buffer, &start);
		gtk_text_buffer_get_end_iter (buffer, &end);

		fp = fopen (name, "wt");
		if (!fp) {
			gchar *msg;
    		msg = g_strdup_printf (_("The file %s could not be saved"), name);
    		oregano_error_with_title (_("Could not save temporary netlist file"), msg);
			g_free (msg);
			return;
		}

		fputs (gtk_text_buffer_get_text (buffer, &start, &end, FALSE), fp);
		fclose (fp);

		g_free (name);
	}
}

/* This method append OREGANO_LANGDIR directory where the netlist.lang file
 * is located to the search path of GtkSourceLanguageManager.
 */
void
setup_language_manager_path(GtkSourceLanguageManager *lm)
{
	gchar **lang_files;
	int i, lang_files_count;
	char **new_langs;

	lang_files = g_strdupv (gtk_source_language_manager_get_search_path (lm));

	lang_files_count = g_strv_length (lang_files);
	new_langs = g_new (char*, lang_files_count + 2);

	for (i = 0; lang_files[i]; i++)
		new_langs[i] = lang_files[i];

	new_langs[lang_files_count] = g_strdup (OREGANO_LANGDIR);
	new_langs[lang_files_count+1] = NULL;

	g_free (lang_files);

	gtk_source_language_manager_set_search_path (lm, new_langs);
}

NetlistEditor *
netlist_editor_new (GtkSourceBuffer * textbuffer) {
	gchar** lang_files;
	NetlistEditor * nle;
	GladeXML * gui;
	GtkWidget * toplevel;
	GtkScrolledWindow * scroll;
	GtkSourceView * source_view;
	GtkSourceLanguageManager * lm;
	GtkButton * save, * sim, * close, * print;
	GtkSourceLanguage *lang=NULL;
	const GSList *list;

	if (!textbuffer) 
		return NULL;
	
	nle = NETLIST_EDITOR (g_object_new (netlist_editor_get_type (), NULL));
	
	netlist_editor_get_config (nle);
		
	if (!g_file_test (OREGANO_GLADEDIR "/view-netlist.glade2", G_FILE_TEST_EXISTS)) {
		gchar *msg;
		msg = g_strdup_printf (
			_("The file %s could not be found. You might need to reinstall Oregano to fix this."),
			OREGANO_GLADEDIR "/view-netlist.glade2");
		oregano_error_with_title (_("Could not create the netlist dialog"), msg);
		g_free (msg);
		return NULL;
	}

	gui = glade_xml_new (OREGANO_GLADEDIR "/view-netlist.glade2", NULL, NULL);
	
	toplevel = glade_xml_get_widget (gui, "toplevel");
	gtk_window_set_default_size (GTK_WINDOW (toplevel), 800, 600);
	gtk_window_set_title (GTK_WINDOW (toplevel), "Net List Editor\n");
	
	scroll = GTK_SCROLLED_WINDOW (glade_xml_get_widget (gui, "netlist-scrolled-window"));
	
	source_view = GTK_SOURCE_VIEW (gtk_source_view_new ());
	lm = GTK_SOURCE_LANGUAGE_MANAGER (gtk_source_language_manager_new ());

	setup_language_manager_path (lm);

	g_object_set_data_full (G_OBJECT (source_view), "language-manager",
		lm, (GDestroyNotify) g_object_unref);

	lang = gtk_source_language_manager_get_language (lm, "netlist");

	if (lang) {
		gtk_source_buffer_set_language (GTK_SOURCE_BUFFER (textbuffer), lang);
		gtk_source_buffer_set_highlight_syntax (GTK_SOURCE_BUFFER (textbuffer), TRUE);
		gtk_source_buffer_set_highlight_matching_brackets (GTK_SOURCE_BUFFER (textbuffer), TRUE);
	} else {
		g_warning ("Can't load netlist.lang in %s", OREGANO_LANGDIR "/netlist.lang");
	}

	gtk_text_view_set_editable (GTK_TEXT_VIEW (source_view), TRUE);
	gtk_text_view_set_buffer (GTK_TEXT_VIEW (source_view), GTK_TEXT_BUFFER (textbuffer));	

	gtk_container_add (GTK_CONTAINER (scroll), GTK_WIDGET (source_view));
	
	close = GTK_BUTTON (glade_xml_get_widget (gui, "btn_close"));
	g_signal_connect_swapped (G_OBJECT (close), "clicked", G_CALLBACK (g_object_unref), G_OBJECT (nle));
	save = GTK_BUTTON (glade_xml_get_widget (gui, "btn_save"));
	g_signal_connect (G_OBJECT (save), "clicked", G_CALLBACK (netlist_editor_save), nle);
	sim = GTK_BUTTON (glade_xml_get_widget (gui, "btn_sim"));
	g_signal_connect (G_OBJECT (sim), "clicked", G_CALLBACK (netlist_editor_simulate), nle);
	print = GTK_BUTTON (glade_xml_get_widget (gui, "btn_print"));	
	g_signal_connect (G_OBJECT (print), "clicked", G_CALLBACK (netlist_editor_print), nle);
	/*
	 *  Set tab, fonts, wrap mode, colors, etc. according
	 *  to preferences 
	 */
	
	nle->priv->lm = lm;
	nle->priv->view = GTK_TEXT_VIEW (source_view);
	nle->priv->toplevel = GTK_WINDOW (toplevel);
	nle->priv->sim = sim;
	nle->priv->save = save;
	nle->priv->close = close;
	nle->priv->buffer = textbuffer;

	gtk_widget_show_all (GTK_WIDGET (toplevel));
	
	return nle;	
}

NetlistEditor *
netlist_editor_new_from_file (gchar * filename)
{
	GtkSourceBuffer *buffer;
	gchar *content;
	gsize length;
	GError *error = NULL;
	NetlistEditor *editor;
	
	if (!filename)
		return NULL;
	if (!(g_file_test (filename, G_FILE_TEST_EXISTS))) {
		gchar *msg;
		/* gettext support */
		msg = g_strdup_printf (_("The file %s could not be found."), filename);
		oregano_error_with_title (_("Could not find the required file"), msg);
		g_free (msg);
		return NULL;
	}

	buffer = gtk_source_buffer_new (NULL);
	g_file_get_contents (filename, &content, &length, &error);
	gtk_text_buffer_set_text (GTK_TEXT_BUFFER (buffer), content, -1);

	editor = netlist_editor_new (buffer);

	return editor;
}

void
netlist_editor_init (NetlistEditor * nle) {
	nle->priv = g_new0 (NetlistEditorPriv, 1);
	
	nle->priv->toplevel = NULL;
	nle->priv->sv = NULL;
	/*nle->priv->source_view = NULL;
	nle->priv->bgcolor = 1;
	nle->priv->selectcolor = 0;
	nle->priv->textcolor = 0;*/
}

NetlistEditor *
netlist_editor_new_from_schematic_view (SchematicView *sv)
{
	NetlistEditor *editor;
	gchar *name = "/tmp/oregano.netlist"; // FIXME
	GError *error = 0;
	Schematic *sm;
	OreganoEngine *engine;

	sm = schematic_view_get_schematic (sv);

	engine = oregano_engine_factory_create_engine (OREGANO_ENGINE_GNUCAP, sm);
	oregano_engine_generate_netlist (engine, name, &error);
	g_object_unref (engine);

	if (error != NULL) {
		if (g_error_matches (error, OREGANO_ERROR, OREGANO_SIMULATE_ERROR_NO_CLAMP) ||
				g_error_matches (error, OREGANO_ERROR, OREGANO_SIMULATE_ERROR_NO_GND) ||
				g_error_matches (error, OREGANO_ERROR, OREGANO_SIMULATE_ERROR_IO_ERROR)) {
			oregano_error_with_title (_("Could not create a netlist"), error->message);
			g_clear_error (&error);
		} else {
			oregano_error (_("An unexpected error has occurred"));
		}
		return NULL;
	}

	editor = netlist_editor_new_from_file (name);
	if (editor) {
		editor->priv->sv = sv;
	}

	return editor;
}

