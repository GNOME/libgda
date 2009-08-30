/* GNOME DB library
 * Copyright (C) 1999 - 2008 The GNOME Foundation.
 *
 * AUTHORS:
 *      Rodrigo Moya <rodrigo@gnome-db.org>
 *      Vivien Malerba <malerba@gnome-db.org>
 *
 * This Library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this Library; see the file COPYING.LIB.  If not,
 * write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <string.h>
#include <gtk/gtk.h>
#ifdef HAVE_GTKSOURCEVIEW
  #include <gtksourceview/gtksourceview.h>
  #include <gtksourceview/gtksourcelanguagemanager.h>
  #include <gtksourceview/gtksourcebuffer.h>
#endif
#include "query-editor.h"

#define PARENT_TYPE GTK_TYPE_VBOX

typedef void (* CreateTagsFunc) (QueryEditor *editor, const gchar *language);

struct _QueryEditorPrivate {
	GtkWidget *scrolled_window;
	GtkWidget *text;
	guint config_lid;
};

static void query_editor_class_init (QueryEditorClass *klass);
static void query_editor_init       (QueryEditor *editor, QueryEditorClass *klass);
static void query_editor_finalize   (GObject *object);

static GObjectClass *parent_class = NULL;
static GHashTable *supported_languages = NULL;
static gint number_of_objects = 0;

/*
 * Private functions
 */
static void
create_tags_for_sql (QueryEditor *editor, const gchar *language)
{
	GtkTextBuffer *buffer;
#ifdef HAVE_GTKSOURCEVIEW
	GtkSourceLanguageManager *mgr;
	GtkSourceLanguage *lang;
#endif

	g_return_if_fail (language != NULL);
	g_return_if_fail (!strcmp (language, QUERY_EDITOR_LANGUAGE_SQL));

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (editor->priv->text));
#ifdef HAVE_GTKSOURCEVIEW
	mgr = gtk_source_language_manager_new ();
	lang = gtk_source_language_manager_get_language (mgr, "sql");

	if (lang) {
		gtk_source_buffer_set_language (GTK_SOURCE_BUFFER (buffer), lang);
		/* FIXME: extend for Gda's variables syntax */
	}

	g_object_unref (mgr);
#endif
}

/*
 * QueryEditor class implementation
 */

static void
query_editor_class_init (QueryEditorClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = query_editor_finalize;
}

#ifdef HAVE_GCONF
static void
configuration_changed_cb (GConfClient *conf_client, guint cnxn_id, GConfEntry *entry, gpointer user_data)
{
	QueryEditor *editor = (QueryEditor *) user_data;

	g_return_if_fail (QUERY_IS_EDITOR (editor));

	if (!strcmp (entry->key, QUERY_CONFIG_KEY_EDITOR_SHOW_LINE_NUMBERS)) {
#ifdef HAVE_GTKSOURCEVIEW
		gtk_source_view_set_show_line_numbers (
			GTK_SOURCE_VIEW (editor->priv->text),
			gconf_client_get_bool (gconf_client_get_default (), QUERY_CONFIG_KEY_EDITOR_SHOW_LINE_NUMBERS, NULL));
#else
#endif
	} else if (!strcmp (entry->key, QUERY_CONFIG_KEY_EDITOR_TAB_STOP)) {
#ifdef HAVE_GTKSOURCEVIEW
		int tab = gconf_client_get_int (gconf_client_get_default (), QUERY_CONFIG_KEY_EDITOR_TAB_STOP, NULL);
		gtk_source_view_set_tab_width (GTK_SOURCE_VIEW (editor->priv->text),
					       tab ? tab : 8);
#else
#endif
	} else if (!strcmp (entry->key, QUERY_CONFIG_KEY_EDITOR_HIGHLIGHT)) {
		query_editor_set_highlight_syntax (editor,
					       gconf_client_get_bool (gconf_client_get_default (), 
								      QUERY_CONFIG_KEY_EDITOR_HIGHLIGHT, NULL));
	}
}
#endif

static void
query_editor_init (QueryEditor *editor, QueryEditorClass *klass)
{
	int tab = 8;
	gboolean highlight = TRUE;
	gboolean showlinesno = FALSE;

	g_return_if_fail (QUERY_IS_EDITOR (editor));

	/* allocate private structure */
	editor->priv = g_new0 (QueryEditorPrivate, 1);

	/* set up widgets */
	editor->priv->scrolled_window = gtk_scrolled_window_new (NULL, NULL);
        gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (editor->priv->scrolled_window),
                                        GTK_POLICY_AUTOMATIC,
                                        GTK_POLICY_AUTOMATIC);
        gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (editor->priv->scrolled_window),
					     GTK_SHADOW_NONE);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (editor->priv->scrolled_window),
					GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_box_pack_start (GTK_BOX (editor), editor->priv->scrolled_window, TRUE, TRUE, 2);

#ifdef HAVE_GTKSOURCEVIEW
	editor->priv->text = gtk_source_view_new ();
	gtk_source_buffer_set_highlight_syntax (GTK_SOURCE_BUFFER (gtk_text_view_get_buffer (GTK_TEXT_VIEW (editor->priv->text))),
					 highlight);
	gtk_source_view_set_show_line_numbers (GTK_SOURCE_VIEW (editor->priv->text), showlinesno);
	gtk_source_view_set_tab_width (GTK_SOURCE_VIEW (editor->priv->text), tab);
#else
	editor->priv->text = gtk_text_view_new ();
#endif
	gtk_widget_show (editor->priv->text);

	gtk_container_add (GTK_CONTAINER (editor->priv->scrolled_window), editor->priv->text);

	/* initialize common data */
	number_of_objects++;
	if (!supported_languages) {
		supported_languages = g_hash_table_new (g_str_hash, g_str_equal);

		/* add the built-in languages */
		g_hash_table_insert (supported_languages, QUERY_EDITOR_LANGUAGE_SQL, create_tags_for_sql);
	}

	create_tags_for_sql (editor, QUERY_EDITOR_LANGUAGE_SQL);
}

static void
query_editor_finalize (GObject *object)
{
	QueryEditor *editor = (QueryEditor *) object;

	g_return_if_fail (QUERY_IS_EDITOR (editor));

	/* free memory */
#ifdef HAVE_GCONF
	gconf_client_notify_remove (gconf_client_get_default (), editor->priv->config_lid);
#endif
	g_free (editor->priv);
	editor->priv = NULL;

	parent_class->finalize (object);

	/* update common data */
	number_of_objects--;
	if (number_of_objects == 0) {
		g_hash_table_destroy (supported_languages);
		supported_languages = NULL;
	}
}

GType
query_editor_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo info = {
			sizeof (QueryEditorClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) query_editor_class_init,
			NULL,
			NULL,
			sizeof (QueryEditor),
			0,
			(GInstanceInitFunc) query_editor_init
		};
		type = g_type_register_static (PARENT_TYPE, "QueryEditor", &info, 0);
	}
	return type;
}

/**
 * query_editor_new
 *
 * Create a new #QueryEditor widget, which is a multiline text widget
 * with support for several languages used to write database stored
 * procedures and functions. If libgnomedb was compiled with gtksourceview
 * in, this widget will support syntax highlighting for all supported
 * languages.
 *
 * Returns: the newly created widget.
 */
GtkWidget *
query_editor_new (void)
{
	QueryEditor *editor;

	editor = g_object_new (QUERY_TYPE_EDITOR, NULL);

	return GTK_WIDGET (editor);
}

/**
 * query_editor_get_editable
 * @editor: a #QueryEditor widget.
 *
 * Retrieve the editable status of the given editor widget.
 *
 * Returns: the editable status.
 */
gboolean
query_editor_get_editable (QueryEditor *editor)
{
        g_return_val_if_fail (QUERY_IS_EDITOR (editor), FALSE);

        return gtk_text_view_get_editable (GTK_TEXT_VIEW (editor->priv->text));
}

/**
 * query_editor_set_editable
 * @editor: a #QueryEditor widget.
 * @editable: editable state.
 *
 * Set the editable state of the given editor widget.
 */
void
query_editor_set_editable (QueryEditor *editor, gboolean editable)
{
	g_return_if_fail (QUERY_IS_EDITOR (editor));
	gtk_text_view_set_editable (GTK_TEXT_VIEW (editor->priv->text), editable);
}

/**
 * query_editor_get_highlight
 * @editor: a #QueryEditor widget.
 *
 * Retrieve the highlighting status of the given editor widget.
 *
 * Returns: the highlighting status.
 */
gboolean
query_editor_get_highlight (QueryEditor *editor)
{
	g_return_val_if_fail (QUERY_IS_EDITOR (editor), FALSE);

#ifdef HAVE_GTKSOURCEVIEW
	return gtk_source_buffer_get_highlight_syntax (
		GTK_SOURCE_BUFFER (gtk_text_view_get_buffer (GTK_TEXT_VIEW (editor->priv->text))));
#else
	return FALSE;
#endif
}

/**
 * query_editor_set_highlight
 * @editor: a #QueryEditor widget.
 * @highlight: highlighting status.
 *
 * Set the highlighting status on the given editor widget.
 */
void
query_editor_set_highlight (QueryEditor *editor, gboolean highlight)
{
	g_return_if_fail (QUERY_IS_EDITOR (editor));

#ifdef HAVE_GTKSOURCEVIEW
	gtk_source_buffer_set_highlight_syntax (
		GTK_SOURCE_BUFFER (gtk_text_view_get_buffer (GTK_TEXT_VIEW (editor->priv->text))), highlight);
#endif
}

/**
 * query_editor_set_text
 * @editor: a #QueryEditor widget.
 * @text: text to display in the editor.
 * @len: length of @text.
 *
 * Set the contents of the given editor widget.
 */
void
query_editor_set_text (QueryEditor *editor, const gchar *text, gint len)
{
	g_return_if_fail (QUERY_IS_EDITOR (editor));
	gtk_text_buffer_set_text (gtk_text_view_get_buffer (GTK_TEXT_VIEW (editor->priv->text)),
				  text, len);
}

/**
 * query_editor_get_all_text
 * @editor: a #QueryEditor widget.
 *
 * Retrieve the full contents of the given editor widget.
 *
 * Returns: the current contents of the editor buffer. You must free
 * the returned value when no longer needed.
 */
gchar *
query_editor_get_all_text (QueryEditor *editor)
{
	g_return_val_if_fail (QUERY_IS_EDITOR (editor), NULL);

	GtkTextBuffer *buffer;
        GtkTextIter start;
        GtkTextIter end;

        buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (editor->priv->text));

	gtk_text_buffer_get_start_iter (buffer, &start);
	gtk_text_buffer_get_end_iter (buffer, &end);

        return gtk_text_buffer_get_text (gtk_text_view_get_buffer (GTK_TEXT_VIEW (editor->priv->text)),
                                         &start, &end, FALSE);
}

/**
 * query_editor_load_from_file
 * @editor: a #QueryEditor widget.
 * @filename: the file to be loaded.
 *
 * Load the given filename into the editor widget.
 *
 * Returns: TRUE if successful, FALSE otherwise.
 */
gboolean
query_editor_load_from_file (QueryEditor *editor, const gchar *filename)
{
	gboolean retval = TRUE;
	gchar *contents;

	g_return_val_if_fail (QUERY_IS_EDITOR (editor), FALSE);
	g_return_val_if_fail (filename != NULL, FALSE);

	if (g_file_get_contents (filename, &contents, NULL, NULL)) {
		query_editor_set_text (editor, contents, -1);
		g_free (contents);
	}
	else 
		retval = FALSE;

	return retval;
}

/**
 * query_editor_save_to_file
 * @editor: a #QueryEditor widget.
 * @filename: the file to save to.
 *
 * Save the current editor contents to the given file.
 *
 * Returns: TRUE if successful, FALSE otherwise.
 */
gboolean
query_editor_save_to_file (QueryEditor *editor, const gchar *filename)
{
	gchar *contents;
	gboolean retval = TRUE;

	g_return_val_if_fail (QUERY_IS_EDITOR (editor), FALSE);
	g_return_val_if_fail (filename != NULL, FALSE);

	contents = query_editor_get_all_text (editor);
	if (!g_file_set_contents (filename, contents, strlen (contents), NULL))
		retval = FALSE;

	g_free (contents);

	return retval;
}

/**
 * query_editor_copy_clipboard
 * @editor: a #QueryEditor widget.
 *
 * Copy currently selected text in the given editor widget to the clipboard.
 */
void
query_editor_copy_clipboard (QueryEditor *editor)
{
	g_return_if_fail (QUERY_IS_EDITOR (editor));
	gtk_text_buffer_copy_clipboard (gtk_text_view_get_buffer (GTK_TEXT_VIEW (editor->priv->text)),
                                        gtk_clipboard_get (GDK_SELECTION_CLIPBOARD));
}

/**
 * query_editor_cut_clipboard
 * @editor: a #QueryEditor widget.
 *
 * Moves currently selected text in the given editor widget to the clipboard.
 */
void
query_editor_cut_clipboard (QueryEditor *editor)
{
	g_return_if_fail (QUERY_IS_EDITOR (editor));

	gtk_text_buffer_cut_clipboard (gtk_text_view_get_buffer (GTK_TEXT_VIEW (editor->priv->text)),
                                       gtk_clipboard_get (GDK_SELECTION_CLIPBOARD),
				       gtk_text_view_get_editable (GTK_TEXT_VIEW (editor->priv->text)));
}

/**
 * query_editor_paste_clipboard
 * @editor: a #QueryEditor widget.
 *
 * Paste clipboard contents into editor widget, at the current position.
 */
void
query_editor_paste_clipboard (QueryEditor *editor)
{
	g_return_if_fail (QUERY_IS_EDITOR (editor));

	gtk_text_buffer_paste_clipboard (gtk_text_view_get_buffer (GTK_TEXT_VIEW (editor->priv->text)),
                                         gtk_clipboard_get (GDK_SELECTION_CLIPBOARD),
                                         NULL,
                                         gtk_text_view_get_editable (GTK_TEXT_VIEW (editor->priv->text)));
}
