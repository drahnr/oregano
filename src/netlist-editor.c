/*
 * netlist-editor.c
 *
 *
 * Author:
 *  Andres de Barbara <adebarbara@fi.uba.ar>
 *  Marc Lorber <lorber.marc@wanadoo.fr>
 *
 * Web page: https://github.com/marc-lorber/oregano
 *
 * Copyright (C) 2004-2008 Ricardo Markiewicz
 * Copyright (C) 2009-2012  Marc Lorber
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

#include <stdio.h>
#include <stdlib.h>
#include <glib/gi18n.h>
#include <gtksourceview/gtksourcelanguagemanager.h>

#include "netlist-editor.h"
#include "netlist-helper.h"
#include "simulation.h"
#include "file.h"
#include "dialogs.h"

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
	GtkTextView *view;
	GtkSourceBuffer *buffer;
	GtkWindow *toplevel;
	GtkButton *save, *close;
};	

static void
netlist_editor_class_init (NetlistEditorClass *klass) {
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);
	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = netlist_editor_finalize;
	object_class->dispose = netlist_editor_dispose;
}

static void
netlist_editor_finalize (GObject *object)
{
	NetlistEditor *nle = NETLIST_EDITOR (object);

	if (nle->priv) {
		// kill the priv struct
		if (nle->priv->toplevel) {
			gtk_widget_destroy (GTK_WIDGET (nle->priv->toplevel));
			nle->priv->toplevel = NULL;
		}
		g_free (nle->priv);
	}

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
netlist_editor_dispose (GObject *object)
{
	NetlistEditor *nle = NETLIST_EDITOR (object);

	if (nle->priv) {
	}
	G_OBJECT_CLASS (parent_class)->dispose (object);
}

GType
netlist_editor_get_type (void)
{
	static GType netlist_editor_type = 0;

	if (!netlist_editor_type) {
		static const GTypeInfo netlist_editor_info = {
			sizeof (NetlistEditorClass),
			NULL,
			NULL,
			(GClassInitFunc) netlist_editor_class_init,
			NULL,
			NULL,
			sizeof (NetlistEditor),
			0,
			(GInstanceInitFunc) netlist_editor_init,
			NULL
		};
		netlist_editor_type = g_type_register_static (G_TYPE_OBJECT,
			"NetlistEditor",
			&netlist_editor_info, 0);
	}
	return netlist_editor_type;
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

// This method append OREGANO_LANGDIR directory where the netlist.lang file
// is located to the search path of GtkSourceLanguageManager.
void
setup_language_manager_path (GtkSourceLanguageManager *lm)
{
	gchar **lang_files;
	const gchar * const * temp;
	GPtrArray *dirs;
	int i, lang_files_count;
	char **new_langs;

	dirs = g_ptr_array_new ();

	// Stolen from gtranslator
	for (temp = gtk_source_language_manager_get_search_path(lm);
            temp != NULL && *temp != NULL; ++temp)
                g_ptr_array_add (dirs, g_strdup(*temp));

	g_ptr_array_add (dirs, NULL);
	lang_files = (gchar **) g_ptr_array_free (dirs, FALSE);
	lang_files_count = g_strv_length (lang_files);
	new_langs = g_new (char*, lang_files_count + 2);

	for (i = 0; lang_files[i]; i++)
		new_langs[i] = lang_files[i];

	new_langs[lang_files_count] = g_strdup (OREGANO_LANGDIR);
	new_langs[lang_files_count+1] = NULL;

	g_strfreev (lang_files);

	gtk_source_language_manager_set_search_path (lm, new_langs);
}

NetlistEditor *
netlist_editor_new (GtkSourceBuffer * textbuffer) {
	NetlistEditor * nle;
	GtkBuilder *gui;
	GError *perror = NULL;
	GtkWidget * toplevel;
	GtkScrolledWindow * scroll;
	GtkSourceView * source_view;
	GtkSourceLanguageManager * lm;
	GtkButton * save, * close;
	GtkSourceLanguage *lang=NULL;

	if (!textbuffer) 
		return NULL;

	if ((gui = gtk_builder_new ()) == NULL) {
		oregano_error (_("Could not create the netlist dialog"));
		return NULL;
	} 
	else gtk_builder_set_translation_domain (gui, NULL);
	
	nle = NETLIST_EDITOR (g_object_new (netlist_editor_get_type (), NULL));
	
	if (!g_file_test (OREGANO_UIDIR "/view-netlist.ui", G_FILE_TEST_EXISTS)) {
		gchar *msg;
		msg = g_strdup_printf (
			_("The file %s could not be found. You might need to reinstall Oregano to fix this."),
			OREGANO_UIDIR "/view-netlist.ui");
		oregano_error_with_title (_("Could not create the netlist dialog"), msg);
		g_free (msg);
		return NULL;
	}

	if (gtk_builder_add_from_file (gui, OREGANO_UIDIR "/view-netlist.ui", 
	    &perror) <= 0) {
			gchar *msg;
		msg = perror->message;
		oregano_error_with_title (_("Could not create the netlist dialog"), msg);
		g_error_free (perror);
		return NULL;
	}
	
	toplevel = GTK_WIDGET (gtk_builder_get_object (gui, "toplevel"));
	gtk_window_set_default_size (GTK_WINDOW (toplevel), 800, 600);
	gtk_window_set_title (GTK_WINDOW (toplevel), "Net List Editor\n");
	
	scroll = GTK_SCROLLED_WINDOW (gtk_builder_get_object (gui, "netlist-scrolled-window"));
	
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
	} 
	else {
		g_warning ("Can't load netlist.lang in %s", OREGANO_LANGDIR "/netlist.lang");
	}

	gtk_text_view_set_editable (GTK_TEXT_VIEW (source_view), TRUE);
	gtk_text_view_set_buffer (GTK_TEXT_VIEW (source_view), GTK_TEXT_BUFFER (textbuffer));	

	gtk_container_add (GTK_CONTAINER (scroll), GTK_WIDGET (source_view));
	
	close = GTK_BUTTON (gtk_builder_get_object (gui, "btn_close"));
	g_signal_connect_swapped (G_OBJECT (close), "clicked", 
		G_CALLBACK (g_object_unref), G_OBJECT (nle));
	save = GTK_BUTTON (gtk_builder_get_object (gui, "btn_save"));
	g_signal_connect (G_OBJECT (save), "clicked", 
		G_CALLBACK (netlist_editor_save), nle);
	
	//  Set tab, fonts, wrap mode, colors, etc. according
	//  to preferences 
	nle->priv->view = GTK_TEXT_VIEW (source_view);
	nle->priv->toplevel = GTK_WINDOW (toplevel);
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
		// gettext support
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
}

NetlistEditor *
netlist_editor_new_from_schematic_view (SchematicView *sv)
{
	NetlistEditor *editor;
	gchar *name = "/tmp/oregano.netlist";
	GError *error = 0;
	Schematic *sm;
	OreganoEngine *engine;

	sm = schematic_view_get_schematic (sv);

	engine = oregano_engine_factory_create_engine (OREGANO_ENGINE_GNUCAP, sm);
	oregano_engine_generate_netlist (engine, name, &error);
	g_object_unref (engine);

	if (error != NULL) {
		if (g_error_matches (error, OREGANO_ERROR, OREGANO_SIMULATE_ERROR_NO_CLAMP) ||
			g_error_matches (error, OREGANO_ERROR, OREGANO_SIMULATE_ERROR_NO_GND)   ||
			g_error_matches (error, OREGANO_ERROR, OREGANO_SIMULATE_ERROR_IO_ERROR)) {
				oregano_error_with_title (_("Could not create a netlist"), error->message);
				g_clear_error (&error);
		} 
		else {
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
