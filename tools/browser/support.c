/*
 * Copyright (C) 2009 - 2012 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2010 David King <davidk@openismus.com>
 * Copyright (C) 2011 Murray Cumming <murrayc@murrayc.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <glib/gi18n-lib.h>
#include <libgda/binreloc/gda-binreloc.h>
#include "support.h"
#include "browser-core.h"
#include "browser-window.h"
#include "login-dialog.h"
#include <libgda-ui/internal/utility.h>

#ifdef HAVE_MAC_INTEGRATION
#include <gtkosxapplication.h>
GtkOSXApplication *theApp = NULL;
#endif

/**
 * browser_connection_open
 * @error: a place to store errors, or %NULL
 * 
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
	gtk_widget_destroy (GTK_WIDGET (dialog));
	return bcnc;
}

/**
 * browser_connection_close
 * @parent: a #GtkWindow
 * @bcnc: a #BrowserConnection object
 *
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
 * Displays an error message until the user aknowledges it. I @parent is a #BrowserWindow, then
 * the error message is displayed in the window if possible
 */
void
browser_show_error (GtkWindow *parent, const gchar *format, ...)
{
        va_list args;
        gchar sz[2048];
        GtkWidget *dialog;
	gchar *tmp;

        /* build the message string */
        va_start (args, format);
        vsnprintf (sz, sizeof (sz), format, args);
        va_end (args);

	if (BROWSER_IS_WINDOW (parent)) {
		browser_window_show_notice (BROWSER_WINDOW (parent), GTK_MESSAGE_ERROR,
					    NULL, sz);
		return;
	}

        /* create the error message dialog */
	dialog = gtk_message_dialog_new (parent,
					 GTK_DIALOG_DESTROY_WITH_PARENT |
					 GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR,
					 GTK_BUTTONS_CLOSE, NULL);
	tmp = g_strdup_printf ("<span weight=\"bold\">%s</span>\n%s", _("Error:"), sz);
	gtk_message_dialog_set_markup (GTK_MESSAGE_DIALOG (dialog), tmp);
	g_free (tmp);

        gtk_widget_show_all (dialog);
        gtk_dialog_run (GTK_DIALOG (dialog));
        gtk_widget_destroy (dialog);
}

/**
 * browser_show_message
 * @parent: a #GtkWindow
 * @format: printf() style format string
 * @...: arguments for @format
 *
 * Displays an error message until the user aknowledges it. I @parent is a #BrowserWindow, then
 * the error message is displayed in the window if possible
 */
void
browser_show_message (GtkWindow *parent, const gchar *format, ...)
{
        va_list args;
        gchar sz[2048];
        GtkWidget *dialog;
	gchar *tmp;

        /* build the message string */
        va_start (args, format);
        vsnprintf (sz, sizeof (sz), format, args);
        va_end (args);

	if (BROWSER_IS_WINDOW (parent)) {
		browser_window_show_notice (BROWSER_WINDOW (parent), GTK_MESSAGE_INFO,
					    NULL, sz);
		return;
	}

        /* create the error message dialog */
	dialog = gtk_message_dialog_new (parent,
					 GTK_DIALOG_DESTROY_WITH_PARENT |
					 GTK_DIALOG_MODAL, GTK_MESSAGE_INFO,
					 GTK_BUTTONS_CLOSE, NULL);
	tmp = g_strdup_printf ("<span weight=\"bold\">%s</span>\n%s", _("Information:"), sz);
	gtk_message_dialog_set_markup (GTK_MESSAGE_DIALOG (dialog), tmp);
	g_free (tmp);

        gtk_widget_show_all (dialog);
        gtk_dialog_run (GTK_DIALOG (dialog));
        gtk_widget_destroy (dialog);
}

#ifdef HAVE_GDU
/**
 * browser_show_help
 * @topic: (allow-none): the help topic, or %NULL for the global index
 */
void
browser_show_help (GtkWindow *parent, const gchar *topic)
{
	GdkScreen *screen;
	GError *error = NULL;

	const gchar *const *langs;
	gchar *uri = NULL;
	gint i;
	
	langs = g_get_language_names ();
	for (i = 0; langs[i]; i++) {
		const gchar *lang;
		lang = langs[i];
		if (strchr (lang, '.'))
			continue;

		uri = gda_gbr_get_file_path (GDA_DATA_DIR, "gnome", "help", "gda-browser", lang, NULL);

		/*g_print ("TST URI [%s]\n", uri);*/
		if (g_file_test (uri, G_FILE_TEST_EXISTS)) {
			/* terminate URI with a '/' for images to load properly,
			 * see http://mail.gnome.org/archives/gnome-doc-list/2009-August/msg00058.html
			 */
			gchar *tmp;
			if (topic)
				tmp = g_strdup_printf ("%s/?%s", uri, topic);
			else
				tmp = g_strdup_printf ("%s/", uri);
			g_free (uri);
			uri = tmp;

			break;
		}
		g_free (uri);
		uri = NULL;
	}
	/*g_print ("URI [%s]\n", uri);*/
	if (uri == NULL) {
		browser_show_error (NULL,  _("Unable to display help. Please make sure the "
					     "documentation package is installed."));
		return;
	}

	gchar *ruri;

	screen = gtk_widget_get_screen ((GtkWidget*) parent);
	ruri = g_strdup_printf ("ghelp:%s", uri);
	gtk_show_uri (screen, ruri,  gtk_get_current_event_time (), &error);
	g_free (ruri);

	if (error) {
		GtkWidget *d;
		d = gtk_message_dialog_new (parent, 
					    GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
					    GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE, 
					    "%s", _("Unable to open help file"));
		gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (d),
							  "%s", error->message);
		g_signal_connect (d, "response", G_CALLBACK (gtk_widget_destroy), NULL);
		gtk_window_present (GTK_WINDOW (d));
		
		g_error_free (error);
	}
	else if (BROWSER_IS_WINDOW (parent))
		browser_window_show_notice (BROWSER_WINDOW (parent), GTK_MESSAGE_INFO,
					    "show-help", _("Help is being loaded, please wait..."));
	else
		browser_show_message (parent, "%s", _("Help is being loaded, please wait..."));

	g_free (uri);
}
#endif

static GtkWidget *
_browser_make_tab_label (const gchar *label,
			 GtkWidget *img, gboolean with_close,
			 GtkWidget **out_close_button)
{
        GtkWidget  *hbox, *wid, *close_button, *image = NULL;
	static GtkCssProvider *css_provider = NULL;

	if (!css_provider) {
		css_provider = gtk_css_provider_new ();
		gtk_css_provider_load_from_data (css_provider,
						 "* {\n"
						 "-GtkWidget-focus-padding : 0;\n"
						 "-GtkWidget-focus-line-width : 0}",
						 -1, NULL);
	}

	if (out_close_button)
		*out_close_button = NULL;

        hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);

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
		gtk_style_context_add_provider (gtk_widget_get_style_context ((GtkWidget*) close_button),
						GTK_STYLE_PROVIDER (css_provider),
						GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

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
	static const gchar* names[] = { /* array indexed by BrowserIconType */
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
		"gda-browser-query.png",
		"gda-browser-action.png",
		"gda-browser-menu-ind.png",
		"gda-browser-ldap-entry.png",
		"gda-browser-ldap-group.png",
		"gda-browser-ldap-organization.png",
		"gda-browser-ldap-person.png",
		"gda-browser-ldap-class-s.png",
		"gda-browser-ldap-class-a.png",
		"gda-browser-ldap-class-x.png",
		"gda-browser-ldap-class-u.png",
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

#ifdef HAVE_LDAP
/**
 * browser_get_pixbuf_for_ldap_class:
 */
GdkPixbuf *
browser_get_pixbuf_for_ldap_class (GdaLdapClassKind kind)
{
	switch (kind) {
	case GDA_LDAP_CLASS_KIND_ABSTRACT:
		return browser_get_pixbuf_icon (BROWSER_ICON_LDAP_CLASS_ABSTRACT);
        case GDA_LDAP_CLASS_KIND_STRUTURAL:
		return browser_get_pixbuf_icon (BROWSER_ICON_LDAP_CLASS_STRUCTURAL);
        case GDA_LDAP_CLASS_KIND_AUXILIARY:
		return browser_get_pixbuf_icon (BROWSER_ICON_LDAP_CLASS_AUXILIARY);
        default:
		return browser_get_pixbuf_icon (BROWSER_ICON_LDAP_CLASS_UNKNOWN);
	}
}

/**
 * browser_get_kind_for_ldap_class:
 */
const gchar *
browser_get_kind_for_ldap_class (GdaLdapClassKind kind)
{
	switch (kind) {
        case GDA_LDAP_CLASS_KIND_ABSTRACT:
                return _("Abstract");
        case GDA_LDAP_CLASS_KIND_STRUTURAL:
                return _("Structural");
        case GDA_LDAP_CLASS_KIND_AUXILIARY:
                return _("Auxilliary");
        default:
                return _("Unknown");
        }
}
#endif


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
connection_added_cb (G_GNUC_UNUSED BrowserCore *bcore, BrowserConnection *bcnc, GdaDataModel *model)
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
connection_removed_cb (G_GNUC_UNUSED BrowserCore *bcore, BrowserConnection *bcnc, GdaDataModel *model)
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

/**
 * browser_make_small_button
 * @is_toggle:
 * @label:
 * @stock_id:
 * @tooltip:
 *
 * Creates a small button
 *
 * Returns: a new #GtkWidget
 */
GtkWidget *
browser_make_small_button (gboolean is_toggle, gboolean with_arrow,
			   const gchar *label, const gchar *stock_id, const gchar *tooltip)
{
	GtkWidget *button, *hbox = NULL;

	if (is_toggle)
		button = gtk_toggle_button_new ();
	else
		button = gtk_button_new ();
	if ((label && stock_id) || with_arrow) {
		hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
		gtk_container_add (GTK_CONTAINER (button), hbox);
		gtk_widget_show (hbox);
	}

	if (stock_id) {
		GtkWidget *image;
		image = gtk_image_new_from_stock (stock_id, GTK_ICON_SIZE_MENU);
		if (hbox)
			gtk_box_pack_start (GTK_BOX (hbox), image, FALSE, FALSE, 0);
		else
			gtk_container_add (GTK_CONTAINER (button), image);
		gtk_widget_show (image);
	}
	if (label) {
		GtkWidget *wid;
		wid = gtk_label_new (label);
		gtk_misc_set_alignment (GTK_MISC (wid), 0., -1);
		if (hbox)
			gtk_box_pack_start (GTK_BOX (hbox), wid, FALSE, FALSE, 5);
		else
			gtk_container_add (GTK_CONTAINER (button), wid);
		gtk_widget_show (wid);
	}
	if (with_arrow) {
		GtkWidget *arrow;
		arrow = gtk_arrow_new (GTK_ARROW_RIGHT, GTK_SHADOW_NONE);
		gtk_box_pack_start (GTK_BOX (hbox), arrow, TRUE, TRUE, 0);
		gtk_misc_set_alignment (GTK_MISC (arrow), 1, .5);
		gtk_widget_show (arrow);
	}

	if (tooltip)
		gtk_widget_set_tooltip_text (button, tooltip);
	return button;
}

/**
 * browser_make_tree_view
 * @model: a #GtkTreeModel
 *
 * Creates a #GtkTreeView which, when right clicked, selects the row underneath the mouse
 * cursor.
 *
 * Returns: a new #GtkWidget
 */
GtkWidget *
browser_make_tree_view (GtkTreeModel *model)
{
	GtkWidget *tv;
	g_return_val_if_fail (GTK_IS_TREE_MODEL (model), NULL);
	tv = gtk_tree_view_new_with_model (model);

	_gdaui_setup_right_click_selection_on_treeview (GTK_TREE_VIEW (tv));
	return tv;
}
