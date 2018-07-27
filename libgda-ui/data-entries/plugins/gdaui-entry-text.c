/*
 * Copyright (C) 2009 - 2012 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2010 David King <davidk@openismus.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#include <string.h>
#include <binreloc/gda-binreloc.h>
#include "gdaui-entry-text.h"
#include <libgda/gda-data-handler.h>
#include <libgda/gda-blob-op.h>

#ifdef HAVE_GTKSOURCEVIEW
  #include <gtksourceview/gtksource.h>
#endif
#define LANGUAGE_SQL "gda-sql"

/* 
 * Main static functions 
 */
static void gdaui_entry_text_class_init (GdauiEntryTextClass * class);
static void gdaui_entry_text_init (GdauiEntryText * srv);
static void gdaui_entry_text_dispose (GObject   * object);
static void gdaui_entry_text_finalize (GObject   * object);

/* virtual functions */
static GtkWidget *create_entry (GdauiEntryWrapper *mgwrap);
static void       real_set_value (GdauiEntryWrapper *mgwrap, const GValue *value);
static GValue    *real_get_value (GdauiEntryWrapper *mgwrap);
static void       connect_signals(GdauiEntryWrapper *mgwrap, GCallback modify_cb, GCallback activate_cb);
static void       set_editable (GdauiEntryWrapper *mgwrap, gboolean editable);

/* get a pointer to the parents to be able to call their destructor */
static GObjectClass  *parent_class = NULL;

/* private structure */
struct _GdauiEntryTextPrivate
{
	GtkTextBuffer *buffer;
	GtkWidget     *view;
	gchar         *lang; /* for code colourisation */
	GtkWrapMode    wrapmode;
};


GType
gdaui_entry_text_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo info = {
			sizeof (GdauiEntryTextClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gdaui_entry_text_class_init,
			NULL,
			NULL,
			sizeof (GdauiEntryText),
			0,
			(GInstanceInitFunc) gdaui_entry_text_init,
			0
		};
		
		type = g_type_register_static (GDAUI_TYPE_ENTRY_WRAPPER, "GdauiEntryText", &info, 0);
	}
	return type;
}

static void
gdaui_entry_text_class_init (GdauiEntryTextClass * class)
{
	GObjectClass   *object_class = G_OBJECT_CLASS (class);

	parent_class = g_type_class_peek_parent (class);

	object_class->dispose = gdaui_entry_text_dispose;
	object_class->finalize = gdaui_entry_text_finalize;

	GDAUI_ENTRY_WRAPPER_CLASS (class)->create_entry = create_entry;
	GDAUI_ENTRY_WRAPPER_CLASS (class)->real_set_value = real_set_value;
	GDAUI_ENTRY_WRAPPER_CLASS (class)->real_get_value = real_get_value;
	GDAUI_ENTRY_WRAPPER_CLASS (class)->connect_signals = connect_signals;
	GDAUI_ENTRY_WRAPPER_CLASS (class)->set_editable = set_editable;
}

static void
gdaui_entry_text_init (GdauiEntryText *gdaui_entry_text)
{
	gdaui_entry_text->priv = g_new0 (GdauiEntryTextPrivate, 1);
	gdaui_entry_text->priv->buffer = NULL;
	gdaui_entry_text->priv->view = NULL;
	gdaui_entry_text->priv->wrapmode = GTK_WRAP_NONE;
	gtk_widget_set_vexpand (GTK_WIDGET (gdaui_entry_text), TRUE);
	gtk_widget_set_hexpand (GTK_WIDGET (gdaui_entry_text), TRUE);
}

/**
 * gdaui_entry_text_new
 * @dh: the data handler to be used by the new widget
 * @type: the requested data type (compatible with @dh)
 * @options: the options
 *
 * Creates a new widget which is mainly a GtkEntry
 *
 * Returns: the new widget
 */
GtkWidget *
gdaui_entry_text_new (GdaDataHandler *dh, GType type, const gchar *options)
{
	GObject *obj;
	GdauiEntryText *mgtxt;

	g_return_val_if_fail (dh && GDA_IS_DATA_HANDLER (dh), NULL);
	g_return_val_if_fail (gda_data_handler_accepts_g_type (dh, type), NULL);

	obj = g_object_new (GDAUI_TYPE_ENTRY_TEXT, "handler", dh, NULL);
	mgtxt = GDAUI_ENTRY_TEXT (obj);
	if (options && *options) {

                GdaQuarkList *params;
                const gchar *str;

                params = gda_quark_list_new_from_string (options);
#ifdef HAVE_GTKSOURCEVIEW
                str = gda_quark_list_find (params, "PROG_LANG");
                if (str)
			mgtxt->priv->lang = g_strdup (str);
#endif
		str = gda_quark_list_find (params, "WRAP_MODE");
                if (str) {
			if (*str == 'N')
				mgtxt->priv->wrapmode = GTK_WRAP_NONE;
			else if (*str == 'C')
				mgtxt->priv->wrapmode = GTK_WRAP_CHAR;
			else if (*str == 'W')
				mgtxt->priv->wrapmode = GTK_WRAP_WORD;
			else
				mgtxt->priv->wrapmode = GTK_WRAP_WORD_CHAR;
		}
                gda_quark_list_free (params);
        }

	gdaui_data_entry_set_value_type (GDAUI_DATA_ENTRY (mgtxt), type);

	return GTK_WIDGET (obj);
}


static void
gdaui_entry_text_dispose (GObject   * object)
{
	GdauiEntryText *gdaui_entry_text;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GDAUI_IS_ENTRY_TEXT (object));

	gdaui_entry_text = GDAUI_ENTRY_TEXT (object);
	if (gdaui_entry_text->priv) {
	}

	/* parent class */
	parent_class->dispose (object);
}

static void
gdaui_entry_text_finalize (GObject   * object)
{
	GdauiEntryText *gdaui_entry_text;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GDAUI_IS_ENTRY_TEXT (object));

	gdaui_entry_text = GDAUI_ENTRY_TEXT (object);
	if (gdaui_entry_text->priv) {
		g_free (gdaui_entry_text->priv->lang);

		g_free (gdaui_entry_text->priv);
		gdaui_entry_text->priv = NULL;
	}

	/* parent class */
	parent_class->finalize (object);
}

#ifdef HAVE_GTKSOURCEVIEW
static void
create_tags_for_sql (GtkTextBuffer *buffer, const gchar *language)
{
	GtkSourceLanguageManager *mgr;
	GtkSourceLanguage *lang;
	gchar ** current_search_path;
	gint len;
	gchar ** search_path;

	GtkSourceStyleSchemeManager* sch_mgr;
	GtkSourceStyleScheme *sch;

	g_return_if_fail (language != NULL);
	g_return_if_fail (!strcmp (language, LANGUAGE_SQL));
	mgr = gtk_source_language_manager_new ();

	/* alter search path */
	current_search_path = (gchar **) gtk_source_language_manager_get_search_path (mgr);
	len = g_strv_length (current_search_path);
	search_path = g_new0 (gchar*, len + 2);
	memcpy (search_path, current_search_path, sizeof (gchar*) * len);
	search_path [len] = gda_gbr_get_file_path (GDA_DATA_DIR, LIBGDA_ABI_NAME, "language-specs", NULL);
	gtk_source_language_manager_set_search_path (mgr, search_path);
	g_free (search_path [len]);
	g_free (search_path);

	lang = gtk_source_language_manager_get_language (mgr, "gda-sql");

	if (!lang) {
		gchar *tmp;
		tmp = gda_gbr_get_file_path (GDA_DATA_DIR, LIBGDA_ABI_NAME, "language-spec", NULL);
		g_print ("Could not find the gda-sql.lang file in %s,\nusing the default SQL highlighting rules.\n",
			 tmp);
		g_free (tmp);
		lang = gtk_source_language_manager_get_language (mgr, "sql");
	}
	if (lang)
		gtk_source_buffer_set_language (GTK_SOURCE_BUFFER (buffer), lang);

	g_object_unref (mgr);

	sch_mgr = gtk_source_style_scheme_manager_get_default ();
	sch = gtk_source_style_scheme_manager_get_scheme (sch_mgr, "tango");
	if (sch) 
		gtk_source_buffer_set_style_scheme (GTK_SOURCE_BUFFER (buffer), sch);
}	
#endif



static GtkWidget *
create_entry (GdauiEntryWrapper *mgwrap)
{
	GdauiEntryText *mgtxt;
	GtkWidget *sw;

	g_return_val_if_fail (mgwrap && GDAUI_IS_ENTRY_TEXT (mgwrap), NULL);
	mgtxt = GDAUI_ENTRY_TEXT (mgwrap);
	g_return_val_if_fail (mgtxt->priv, NULL);

#ifdef HAVE_GTKSOURCEVIEW
	if (mgtxt->priv->lang) {
		GtkSourceBuffer *sbuf;
		mgtxt->priv->view = gtk_source_view_new ();
		sbuf = GTK_SOURCE_BUFFER (gtk_text_view_get_buffer (GTK_TEXT_VIEW (mgtxt->priv->view)));

		GtkSourceLanguageManager *lm;
		GtkSourceLanguage *sl;
		lm = gtk_source_language_manager_get_default ();
		sl = gtk_source_language_manager_get_language (lm, mgtxt->priv->lang);
		
		gtk_source_buffer_set_language (sbuf, sl);
		gtk_source_buffer_set_highlight_syntax (sbuf, TRUE);
		if (! strcmp (mgtxt->priv->lang, LANGUAGE_SQL))
			create_tags_for_sql (GTK_TEXT_BUFFER (sbuf), LANGUAGE_SQL);
	}
	else
		mgtxt->priv->view = gtk_text_view_new ();
#else
	mgtxt->priv->view = gtk_text_view_new ();
#endif
	mgtxt->priv->buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (mgtxt->priv->view));
	gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (mgtxt->priv->view), mgtxt->priv->wrapmode);
	sw = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw), GTK_SHADOW_NONE);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);
	gtk_container_add (GTK_CONTAINER (sw), mgtxt->priv->view);
	gtk_widget_show (mgtxt->priv->view);
	
	return sw;
}

static void
real_set_value (GdauiEntryWrapper *mgwrap, const GValue *value)
{
	GdauiEntryText *mgtxt;

	g_return_if_fail (mgwrap && GDAUI_IS_ENTRY_TEXT (mgwrap));
	mgtxt = GDAUI_ENTRY_TEXT (mgwrap);
	g_return_if_fail (mgtxt->priv);

	gtk_text_buffer_set_text (mgtxt->priv->buffer, "", -1);
	if (value) {
		if (! gda_value_is_null ((GValue *) value)) {
			GdaDataHandler *dh;		
			gchar *str;
			gboolean done = FALSE;

			if (G_VALUE_TYPE (value) == GDA_TYPE_BLOB) {
				GdaBlob *blob;
				GdaBinary *bin;
				blob = g_value_get_boxed (value);
				bin = gda_blob_get_binary (blob);
				if (gda_blob_get_op (blob) &&
				    (gda_binary_get_size (bin) != gda_blob_op_get_length (gda_blob_get_op (blob))))
                                        gda_blob_op_read_all (gda_blob_get_op (blob), (GdaBlob*) blob);
				if (g_utf8_validate ((gchar*) gda_binary_get_data (bin), gda_binary_get_size (bin), NULL)) {
					gtk_text_buffer_set_text (mgtxt->priv->buffer, (gchar*) gda_binary_get_data (bin), 
								  gda_binary_get_size (bin));
					done = TRUE;
				}
			}
			else  if (G_VALUE_TYPE (value) == GDA_TYPE_BINARY) {
				GdaBinary *bin;
				bin = g_value_get_boxed (value);
				if (g_utf8_validate ((gchar*) gda_binary_get_data (bin), gda_binary_get_size (bin), NULL)) {
					gtk_text_buffer_set_text (mgtxt->priv->buffer, (gchar*) gda_binary_get_data (bin), 
								  gda_binary_get_size (bin));
					done = TRUE;
				}
			}

			if (!done) {
				dh = gdaui_data_entry_get_handler (GDAUI_DATA_ENTRY (mgwrap));
				str = gda_data_handler_get_str_from_value (dh, value);
				if (str) {
					gtk_text_buffer_set_text (mgtxt->priv->buffer, str, -1);
					g_free (str);
				}
			}
		}
	}
}

static GValue *
real_get_value (GdauiEntryWrapper *mgwrap)
{
	GValue *value;
	GdauiEntryText *mgtxt;
	GdaDataHandler *dh;
	gchar *str;
	GtkTextIter start, end;

	g_return_val_if_fail (mgwrap && GDAUI_IS_ENTRY_TEXT (mgwrap), NULL);
	mgtxt = GDAUI_ENTRY_TEXT (mgwrap);
	g_return_val_if_fail (mgtxt->priv, NULL);

	dh = gdaui_data_entry_get_handler (GDAUI_DATA_ENTRY (mgwrap));
	gtk_text_buffer_get_start_iter (mgtxt->priv->buffer, &start);
	gtk_text_buffer_get_end_iter (mgtxt->priv->buffer, &end);
	str = gtk_text_buffer_get_text (mgtxt->priv->buffer, &start, &end, FALSE);
	value = gda_data_handler_get_value_from_str (dh, str, 
						     gdaui_data_entry_get_value_type (GDAUI_DATA_ENTRY (mgwrap)));
	g_free (str);
	if (!value) {
		/* in case the gda_data_handler_get_value_from_sql() returned an error because
		   the contents of the GtkEntry cannot be interpreted as a GValue */
		value = gda_value_new_null ();
	}

	return value;
}

typedef void (*Callback2) (gpointer, gpointer);
static gboolean
focus_out_cb (GtkWidget *widget, GdkEventFocus *event, GdauiEntryText *mgtxt)
{
	GCallback activate_cb;
	activate_cb = g_object_get_data (G_OBJECT (widget), "_activate_cb");
	g_assert (activate_cb);
	((Callback2)activate_cb) (widget, mgtxt);

	return gtk_widget_event (GTK_WIDGET (mgtxt), (GdkEvent*) event);
}

static void
connect_signals(GdauiEntryWrapper *mgwrap, GCallback modify_cb, GCallback activate_cb)
{
	GdauiEntryText *mgtxt;

	g_return_if_fail (mgwrap && GDAUI_IS_ENTRY_TEXT (mgwrap));
	mgtxt = GDAUI_ENTRY_TEXT (mgwrap);
	g_return_if_fail (mgtxt->priv);

	g_object_set_data (G_OBJECT (mgtxt->priv->view), "_activate_cb", activate_cb);
	g_signal_connect_swapped (G_OBJECT (mgtxt->priv->buffer), "changed",
				  modify_cb, mgwrap);
	g_signal_connect_swapped (G_OBJECT (mgtxt->priv->view), "focus-out-event",
				  G_CALLBACK (focus_out_cb), mgtxt);
	/* FIXME: how does the user "activates" the GtkTextView widget ? */
}

static void
set_editable (GdauiEntryWrapper *mgwrap, gboolean editable)
{
	GdauiEntryText *mgtxt;

	g_return_if_fail (mgwrap && GDAUI_IS_ENTRY_TEXT (mgwrap));
	mgtxt = GDAUI_ENTRY_TEXT (mgwrap);

	gtk_text_view_set_editable (GTK_TEXT_VIEW (mgtxt->priv->view), editable);
}
