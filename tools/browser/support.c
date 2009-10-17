/* 
 * Copyright (C) 2009 The GNOME Foundation.
 *
 * AUTHORS:
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

#include <glib/gi18n-lib.h>
#include <libgda/binreloc/gda-binreloc.h>
#include "support.h"
#include "browser-core.h"
#include "browser-window.h"
#include "login-dialog.h"

/**
 * Display a login dialog and, if validated, create a new #BrowserConnection
 *
 * Returns: a new #BrowserConnection, or %NULL (the caller DOES NOT OWN a reference to the returned value)
 */
BrowserConnection *
browser_connection_open (GError **error)
{
	BrowserConnection *bcnc = NULL;
	LoginDialog *dialog;
	GdaConnection *cnc;
	dialog = login_dialog_new (NULL);
	
	cnc = login_dialog_run (dialog, TRUE, error);
	if (cnc) {
		BrowserWindow *nbwin;
		bcnc = browser_connection_new (cnc);
		g_object_unref (cnc);
		nbwin = browser_window_new (bcnc, NULL);

		browser_core_take_window (nbwin);
		browser_core_take_connection (bcnc);
	}
	return bcnc;
}

/**
 * Displays a warning dialog and close @bcnc
 *
 * Returns: %TRUE if the connection has been closed
 */
gboolean
browser_connection_close (GtkWindow *parent, BrowserConnection *bcnc)
{
	/* confirmation dialog */
	GtkWidget *dialog;
	char *str;

	g_return_val_if_fail (BROWSER_IS_CONNECTION (bcnc), FALSE);

	str = g_strdup_printf (_("Do you want to close the '%s' connection?"),
			       browser_connection_get_name (bcnc));
	dialog = gtk_message_dialog_new_with_markup (parent, GTK_DIALOG_MODAL,
						     GTK_MESSAGE_QUESTION,
						     GTK_BUTTONS_YES_NO,
						     "<b>%s</b>", str);
	g_free (str);

	if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_YES) {
		/* actual connection closing */
		browser_core_close_connection (bcnc);

		/* list all the windows using bwin's connection */
		GSList *list, *windows, *bwin_list = NULL;

		windows = browser_core_get_windows ();
		for (list = windows; list; list = list->next) {
			if (browser_window_get_connection (BROWSER_WINDOW (list->data)) == bcnc)
				bwin_list = g_slist_prepend (bwin_list, list->data);
		}
		g_slist_free (windows);

		for (list = bwin_list; list; list = list->next)
			browser_core_close_window (BROWSER_WINDOW (list->data));
		g_slist_free (bwin_list);
		gtk_widget_destroy (dialog);

		list = browser_core_get_windows ();
		return TRUE;
	}
	else {
		gtk_widget_destroy (dialog);
		return FALSE;
	}
}


/**
 * browser_show_error
 * @parent: a #GtkWindow
 * @format: printf() style format string
 * @...: arguments for @format
 *
 * Displays a modal error until the user aknowledges it.
 */
void
browser_show_error (GtkWindow *parent, const gchar *format, ...)
{
        va_list args;
        gchar sz[2048];
        GtkWidget *dialog;

        /* build the message string */
        va_start (args, format);
        vsnprintf (sz, sizeof (sz), format, args);
        va_end (args);

        /* create the error message dialog */
	dialog = gtk_message_dialog_new_with_markup (parent,
                                                     GTK_DIALOG_DESTROY_WITH_PARENT |
                                                     GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR,
                                                     GTK_BUTTONS_CLOSE,
						     "<span weight=\"bold\">%s</span>\n%s", _("Error:"), sz);

        gtk_widget_show_all (dialog);
        gtk_dialog_run (GTK_DIALOG (dialog));
        gtk_widget_destroy (dialog);
}

/* hash table to remain which context notices have to be hidden: key=context, value=GINT_TO_POINTER (1) */
static GHashTable *hidden_contexts = NULL;

static void
hide_notice_toggled_cb (GtkToggleButton *toggle, gchar *context)
{
	g_assert (hidden_contexts);
	if (gtk_toggle_button_get_active (toggle))
		g_hash_table_insert (hidden_contexts, g_strdup (context), GINT_TO_POINTER (TRUE));
	else
		g_hash_table_remove (hidden_contexts, context);
}

/**
 * browser_show_notice
 * @parent: a #GtkWindow
 * @context: a context string or %NULL
 * @format: printf() style format string
 * @...: arguments for @format
 *
 * Displays a modal notice until the user aknowledges it.
 *
 * If @context is not NULL, then the message box contains a check box to avoid displaying the
 * same massage again.
 */
void
browser_show_notice (GtkWindow *parent, const gchar *context, const gchar *format, ...)
{
        va_list args;
        gchar sz[2048];
        GtkWidget *dialog;
	gboolean hide = FALSE;

        /* build the message string */
        va_start (args, format);
        vsnprintf (sz, sizeof (sz), format, args);
        va_end (args);

	if (context) {
		if (!hidden_contexts)
			hidden_contexts = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
		hide = GPOINTER_TO_INT (g_hash_table_lookup (hidden_contexts, context));
	}

	if (hide) {
		if (BROWSER_IS_WINDOW (parent)) {
			gchar *ptr;
			for (ptr = sz; *ptr && (*ptr != '\n'); ptr++);
			if (*ptr) {
				*ptr = '.'; ptr++;
				*ptr = '.'; ptr++;
				*ptr = '.'; ptr++;
				*ptr = 0;
			}
			browser_window_push_status (BROWSER_WINDOW (parent),
						    "SupportNotice", sz, TRUE);
		}
	}
	else {
		/* create the error message dialog */
		dialog = gtk_message_dialog_new_with_markup (parent,
							     GTK_DIALOG_DESTROY_WITH_PARENT |
							     GTK_DIALOG_MODAL, GTK_MESSAGE_INFO,
							     GTK_BUTTONS_CLOSE,
							     "<span weight=\"bold\">%s</span>\n%s", _("Note:"), sz);
		if (context) {
			GtkWidget *cb;
			cb = gtk_check_button_new_with_label (_("Don't show this message again"));
			g_signal_connect_data (cb, "toggled",
					       G_CALLBACK (hide_notice_toggled_cb), g_strdup (context),
					       (GClosureNotify) g_free, 0);
			gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), cb, FALSE, FALSE, 10);
		}
		
		gtk_widget_show_all (dialog);
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);
	}
}

static GtkWidget *
_browser_make_tab_label (const gchar *label,
			 GtkWidget *img, gboolean with_close,
			 GtkWidget **out_close_button)
{
        GtkWidget  *hbox, *wid, *close_button, *image = NULL;
	static gboolean rc_done = FALSE;

	if (out_close_button)
		*out_close_button = NULL;

	if (!rc_done) {
		rc_done = TRUE;
		gtk_rc_parse_string ("style \"browser-tab-close-button-style\"\n"
				     "{\n"
				     "GtkWidget::focus-padding = 0\n"
				     "GtkWidget::focus-line-width = 0\n"
				     "xthickness = 0\n"
				     "ythickness = 0\n"
				     "}\n"
				     "widget \"*.browser-tab-close-button\" style \"browser-tab-close-button-style\"");
	}

        hbox = gtk_hbox_new (FALSE, 4);

	if (img)
		gtk_box_pack_start (GTK_BOX (hbox), img, FALSE, FALSE, 0);
	
        wid = gtk_label_new (label);
        gtk_label_set_single_line_mode (GTK_LABEL (wid), TRUE);
        gtk_misc_set_alignment (GTK_MISC (wid), 0.0, 0.5);
        gtk_misc_set_padding (GTK_MISC (wid), 0, 0);
	gtk_box_pack_start (GTK_BOX (hbox), wid, TRUE, TRUE, 0);

	if (with_close) {
		image = gtk_image_new_from_stock (GTK_STOCK_CLOSE, GTK_ICON_SIZE_MENU);
		close_button = gtk_button_new ();
		gtk_button_set_relief (GTK_BUTTON (close_button), GTK_RELIEF_NONE);
		gtk_button_set_focus_on_click (GTK_BUTTON (close_button), FALSE);
		gtk_widget_set_name (close_button, "browser-tab-close-button");

		gtk_widget_set_tooltip_text (close_button, _("Close tab"));
		
		gtk_container_add (GTK_CONTAINER (close_button), image);
		gtk_container_set_border_width (GTK_CONTAINER (close_button), 0);
		gtk_box_pack_start (GTK_BOX (hbox), close_button, FALSE, FALSE, 0);

		if (out_close_button)
			*out_close_button = close_button;
	}

	gtk_widget_show_all (hbox);
        return hbox;
}

GtkWidget *
browser_make_tab_label_with_stock (const gchar *label,
				   const gchar *stock_id, gboolean with_close,
				   GtkWidget **out_close_button)
{
	GtkWidget *image = NULL;
	if (stock_id) {
		image = gtk_image_new_from_icon_name (stock_id, GTK_ICON_SIZE_MENU);
		if (!image)
			image = gtk_image_new_from_stock (stock_id, GTK_ICON_SIZE_MENU);
	}
	return _browser_make_tab_label (label, image, with_close, out_close_button);
}

GtkWidget *
browser_make_tab_label_with_pixbuf (const gchar *label,
				    GdkPixbuf *pixbuf, gboolean with_close,
				    GtkWidget **out_close_button)
{
	GtkWidget *image = NULL;
	if (pixbuf)
		image = gtk_image_new_from_pixbuf (pixbuf);
	return _browser_make_tab_label (label, image, with_close, out_close_button);
}

/**
 * browser_get_pixbuf_icon
 *
 * Get a pointer to an internal #GdkPixbuf for the requested @type. Don't unref it!
 *
 * Returns: an already existing #GdkPixbuf, or %NULL if the icon was not found
 */
GdkPixbuf *
browser_get_pixbuf_icon (BrowserIconType type)
{
	static GdkPixbuf **array = NULL;
	static const gchar* names[] = {
		"gda-browser-bookmark.png",
		"gda-browser-schema.png",
		"gda-browser-table.png",
		"gda-browser-column.png",
		"gda-browser-column-pk.png",
		"gda-browser-column-fk.png",
		"gda-browser-column-fknn.png",
		"gda-browser-column-nn.png",
		"gda-browser-reference.png",
		"gda-browser-diagram.png",
		"gda-browser-query.png"
	};

	if (!array)
		array = g_new0 (GdkPixbuf *, BROWSER_ICON_LAST);
	if (!array [type]) {
		gchar *path;
		path = gda_gbr_get_file_path (GDA_DATA_DIR, LIBGDA_ABI_NAME, "pixmaps", names[type], NULL);
		array [type] = gdk_pixbuf_new_from_file (path, NULL);
		g_free (path);
		
		if (!array [type])
			array [type] = (GdkPixbuf*) 0x01;
	}
	if (array [type] == (GdkPixbuf*) 0x01)
		return NULL;
	else
		return array [type];
}

/**
 * browser_find_parent_widget
 *
 * Finds the 1st parent widget of @current which is of the @requested_type type.
 */
GtkWidget *
browser_find_parent_widget (GtkWidget *current, GType requested_type)
{
	GtkWidget *wid;
	g_return_val_if_fail (GTK_IS_WIDGET (current), NULL);

	for (wid = gtk_widget_get_parent (current); wid; wid = gtk_widget_get_parent (wid)) {
		if (G_OBJECT_TYPE (wid) == requested_type)
			return wid;
	}
	return NULL;
}

static void connection_added_cb (BrowserCore *bcore, BrowserConnection *bcnc, GdaDataModel *model);
static void connection_removed_cb (BrowserCore *bcore, BrowserConnection *bcnc, GdaDataModel *model);

/**
 * browser_get_connections_list
 *
 * Creates a unique instance of tree model listing all the connections, and returns
 * a pointer to it. The object will always exist after it has been created, so no need
 * to reference it.
 *
 * Returns: a pointer to the #GtkTreeModel, the caller must not assume it has a reference to it.
 */
GdaDataModel *
browser_get_connections_list (void)
{
	static GdaDataModel *model = NULL;
	if (!model) {
		model = gda_data_model_array_new_with_g_types (CNC_LIST_NUM_COLUMNS,
							       BROWSER_TYPE_CONNECTION,
							       G_TYPE_STRING);
		/* initial filling */
		GSList *connections, *list;
		connections =  browser_core_get_connections ();
		for (list = connections; list; list = list->next)
			connection_added_cb (browser_core_get(), BROWSER_CONNECTION (list->data),
					     model);
		g_slist_free (connections);

		g_signal_connect (browser_core_get (), "connection-added",
				  G_CALLBACK (connection_added_cb), model);
		g_signal_connect (browser_core_get (), "connection-removed",
				  G_CALLBACK (connection_removed_cb), model);
	}

	return model;
}

static void
connection_added_cb (BrowserCore *bcore, BrowserConnection *bcnc, GdaDataModel *model)
{
	GList *values;
	GValue *value;
	
	g_value_set_string ((value = gda_value_new (G_TYPE_STRING)), browser_connection_get_name (bcnc));
	values = g_list_prepend (NULL, value);
	g_value_set_object ((value = gda_value_new (BROWSER_TYPE_CONNECTION)), bcnc);
	values = g_list_prepend (values, value);

	g_assert (gda_data_model_append_values (model, values, NULL) >= 0);

	g_list_foreach (values, (GFunc) gda_value_free, NULL);
	g_list_free (values);
}

static void
connection_removed_cb (BrowserCore *bcore, BrowserConnection *bcnc, GdaDataModel *model)
{
	gint i, nrows;
	nrows = gda_data_model_get_n_rows (model);
	for (i = 0; i < nrows; i++) {
		const GValue *value;
		value = gda_data_model_get_value_at (model, 0, i, NULL);
		g_assert (value);
		if (g_value_get_object (value) == bcnc) {
			g_assert (gda_data_model_remove_row (model, i, NULL));
			break;
		}
	}
}
