/*
 * Copyright (C) 2014 Anders Jonsson <anders.jonsson@norsjovallen.se>
 * Copyright (C) 2014 - 2015 Vivien Malerba <malerba@gnome-db.org>
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

#include <string.h>
#include <glib/gi18n-lib.h>
#include "ui-support.h"
#include <base/base-tool-decl.h>
#include "browser-window.h"
#include <libgda/binreloc/gda-binreloc.h>

#ifdef HAVE_LDAP

/**
 * ui_connection_ldap_icon_for_class_kind:
 * @kind: a #GdaLdapClassKind
 *
 * Returns: (transfer none): the correct icon, or %NULL if it could not be determined
 */
GdkPixbuf *
ui_connection_ldap_icon_for_class_kind (GdaLdapClassKind kind)
{
	switch (kind) {
	case GDA_LDAP_CLASS_KIND_ABSTRACT:
		return ui_get_pixbuf_icon (UI_ICON_LDAP_CLASS_ABSTRACT);
	case GDA_LDAP_CLASS_KIND_STRUTURAL:
		return ui_get_pixbuf_icon (UI_ICON_LDAP_CLASS_STRUCTURAL);
	case GDA_LDAP_CLASS_KIND_AUXILIARY:
		return ui_get_pixbuf_icon (UI_ICON_LDAP_CLASS_AUXILIARY);
	case GDA_LDAP_CLASS_KIND_UNKNOWN:
		return ui_get_pixbuf_icon (UI_ICON_LDAP_CLASS_UNKNOWN);
	default:
		g_assert_not_reached ();
		return NULL;
	}
}

/**
 * ui_connection_ldap_class_kind_to_string:
 * @kind:
 *
 * Returns: (transfer none): a string
 */
const gchar *
ui_connection_ldap_class_kind_to_string (GdaLdapClassKind kind)
{
	switch (kind) {
        case GDA_LDAP_CLASS_KIND_ABSTRACT:
                return _("Abstract");
        case GDA_LDAP_CLASS_KIND_STRUTURAL:
                return _("Structural");
        case GDA_LDAP_CLASS_KIND_AUXILIARY:
                return _("Auxiliary");
	case GDA_LDAP_CLASS_KIND_UNKNOWN:
                return _("Unknown");
        default:
		g_assert_not_reached ();
		return NULL;
        }
}

/**
 * ui_connection_ldap_icon_for_class:
 * @objectclass: objectClass attribute
 *
 * Returns: (transfer none): the correct icon, or %NULL if it could not be determined
 */
GdkPixbuf *
ui_connection_ldap_icon_for_class (GdaLdapAttribute *objectclass)
{
	gint type = 0;
	if (objectclass) {
		guint i;
		for (i = 0; i < objectclass->nb_values; i++) {
			const gchar *class = g_value_get_string (objectclass->values[i]);
			if (!class)
				continue;
			else if (!strcmp (class, "organization"))
				type = MAX(type, 1);
			else if (!strcmp (class, "groupOfNames") ||
				 !strcmp (class, "posixGroup"))
				type = MAX(type, 2);
			else if (!strcmp (class, "account") ||
				 !strcmp (class, "mailUser") ||
				 !strcmp (class, "organizationalPerson") ||
				 !strcmp (class, "person") ||
				 !strcmp (class, "pilotPerson") ||
				 !strcmp (class, "newPilotPerson") ||
				 !strcmp (class, "pkiUser") ||
				 !strcmp (class, "posixUser") ||
				 !strcmp (class, "posixAccount") ||
				 !strcmp (class, "residentalPerson") ||
				 !strcmp (class, "shadowAccount") ||
				 !strcmp (class, "strongAuthenticationUser") ||
				 !strcmp (class, "inetOrgPerson"))
				type = MAX(type, 3);
		}
	}

	switch (type) {
	case 0:
		return ui_get_pixbuf_icon (UI_ICON_LDAP_ENTRY);
	case 1:
		return ui_get_pixbuf_icon (UI_ICON_LDAP_ORGANIZATION);
	case 2:
		return ui_get_pixbuf_icon (UI_ICON_LDAP_GROUP);
	case 3:
		return ui_get_pixbuf_icon (UI_ICON_LDAP_PERSON);
	default:
		g_assert_not_reached ();
		return NULL;
	}
}

/**
 * ui_connection_ldap_icon_for_dn:
 * @tcnc: a #TConnection
 * @dn: the DN of the entry
 * @error: a place to store errors, or %NULL
 *
 * Determines the correct icon type for @dn based on which class it belongs to
 *
 * Returns: (transfer none): a #GdkPixbuf, or %NULL
 */
GdkPixbuf *
ui_connection_ldap_icon_for_dn (TConnection *tcnc, const gchar *dn, GError **error)
{
	g_return_val_if_fail (T_IS_CONNECTION (tcnc), NULL);
	g_return_val_if_fail (t_connection_is_ldap (tcnc), NULL);
	g_return_val_if_fail (dn && *dn, NULL);

	GdaLdapEntry *lentry;
	lentry = t_connection_ldap_describe_entry (tcnc, dn, error);
	if (lentry) {
		GdaLdapAttribute *attr;
		attr = g_hash_table_lookup (lentry->attributes_hash, "objectClass");

		GdkPixbuf *pixbuf;
		pixbuf = ui_connection_ldap_icon_for_class (attr);
		gda_ldap_entry_free (lentry);
		return pixbuf;
	}
	else
		return NULL;
}

#endif /* HAVE_LDAP */

/**
 * ui_show_error:
 * @parent: a #GtkWindow
 * @format: printf() style format string
 * @...: arguments for @format
 *
 * Displays an error message until the user acknowledges it. I @parent is a #BrowserWindow, then
 * the error message is displayed in the window if possible
 */ 
void
ui_show_error (GtkWindow *parent, const gchar *format, ...)
{
	va_list args;
        gchar sz[2048];
        GtkWidget *dialog;
        gchar *tmp;

        /* build the message string */
        va_start (args, format);
        vsnprintf (sz, sizeof (sz), format, args);
        va_end (args);

	/*
	  FIXME
        if (BROWSER_IS_WINDOW (parent)) {
                browser_window_show_notice (BROWSER_WINDOW (parent), GTK_MESSAGE_ERROR,
                                            NULL, sz);
                return;
        }
	*/

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
 * ui_show_message:
 * @parent: a #GtkWindow
 * @format: printf() style format string
 * @...: arguments for @format
 *
 * Displays an error message until the user aknowledges it. I @parent is a #BrowserWindow, then
 * the error message is displayed in the window if possible
 */
void
ui_show_message (GtkWindow *parent, const gchar *format, ...)
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
 * ui_show_help:
 * @current: a #GtkWidget
 * @topic: the topic to show
 */
void
ui_show_help (GtkWindow *parent, const gchar *topic)
{
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

		uri = gda_gbr_get_file_path (GDA_DATA_DIR, "help", lang, "gda-browser", NULL);

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

	if (!uri) {
		ui_show_error (parent,  _("Unable to display help. Please make sure the "
					  "documentation package is installed."));
		return;
	}

	gchar *ruri;

	ruri = g_strdup_printf ("ghelp:%s", uri);
	gtk_show_uri_on_window (parent, ruri, gtk_get_current_event_time (), &error);
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

	g_free (uri);	
}
#endif


/**
 * ui_find_parent_widget:
 * @current: a #GtkWidget
 * @requested_type: the requested type for the return value
 *
 * Finds the 1st parent widget of @current which is of the @requested_type type.
 *
 * Returns: a #GtkWidget of the requested type, or %NULL if non found
 */
GtkWidget *
ui_find_parent_widget (GtkWidget *current, GType requested_type)
{
        GtkWidget *wid;
        g_return_val_if_fail (GTK_IS_WIDGET (current), NULL);

        for (wid = gtk_widget_get_parent (current); wid; wid = gtk_widget_get_parent (wid)) {
                if (G_OBJECT_TYPE (wid) == requested_type)
                        return wid;
        }
        return NULL;
}

/**
 * ui_make_tree_view:
 * @model: a #GtkTreeModel
 *
 * Creates a #GtkTreeView which, when right clicked, selects the row underneath the mouse
 * cursor.
 *
 * Returns: a new #GtkWidget
 */
GtkWidget *
ui_make_tree_view (GtkTreeModel *model)
{
        GtkWidget *tv;
        g_return_val_if_fail (GTK_IS_TREE_MODEL (model), NULL);
        tv = gtk_tree_view_new_with_model (model);
	gtk_widget_set_vexpand (GTK_WIDGET (tv), TRUE);

        _gdaui_setup_right_click_selection_on_treeview (GTK_TREE_VIEW (tv));
        return tv;
}

/**
 * ui_make_small_button:
 */
GtkWidget *
ui_make_small_button (gboolean is_toggle, gboolean with_arrow,
		      const gchar *label, const gchar *icon_name,
		      const gchar *tooltip)
{
	GtkWidget *button, *hbox = NULL;

	if (is_toggle)
		button = gtk_toggle_button_new ();
	else
		button = gtk_button_new ();
	gtk_widget_set_halign (button, GTK_ALIGN_CENTER);
	if ((label && icon_name) || with_arrow) {
		hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
		gtk_container_add (GTK_CONTAINER (button), hbox);
		gtk_widget_show (hbox);
	}

	if (icon_name) {
		GtkWidget *image;
		image = gtk_image_new_from_icon_name (icon_name, GTK_ICON_SIZE_MENU);
		if (hbox)
			gtk_box_pack_start (GTK_BOX (hbox), image, FALSE, FALSE, 0);
		else
			gtk_container_add (GTK_CONTAINER (button), image);
		gtk_widget_show (image);
	}
	if (label) {
		GtkWidget *wid;
		wid = gtk_label_new (label);
		gtk_widget_set_halign (wid, GTK_ALIGN_START);
		if (hbox)
			gtk_box_pack_start (GTK_BOX (hbox), wid, FALSE, FALSE, 5);
		else
			gtk_container_add (GTK_CONTAINER (button), wid);
		gtk_widget_show (wid);
	}
	if (with_arrow) {
		GtkWidget *arrow;
		arrow = gtk_image_new_from_icon_name ("go-next-symbolic", GTK_ICON_SIZE_MENU);
		gtk_box_pack_start (GTK_BOX (hbox), arrow, FALSE, FALSE, 0);
		gtk_widget_set_halign (arrow, GTK_ALIGN_END);
		gtk_widget_set_valign (arrow, GTK_ALIGN_CENTER);
		gtk_widget_show (arrow);
	}

	if (tooltip)
		gtk_widget_set_tooltip_text (button, tooltip);
	return button;
}

static GtkWidget *
_make_tab_label (const gchar *label,
		 GtkWidget *img, gboolean with_close,
		 GtkWidget **out_close_button)
{
        GtkWidget  *hbox, *wid, *close_button, *image = NULL;

	if (out_close_button)
		*out_close_button = NULL;

        hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);

	if (img)
		gtk_box_pack_start (GTK_BOX (hbox), img, FALSE, FALSE, 0);
	
        wid = gtk_label_new (label);
        gtk_label_set_single_line_mode (GTK_LABEL (wid), TRUE);
	gtk_widget_set_halign (wid, GTK_ALIGN_START);
	gtk_widget_set_valign (wid, GTK_ALIGN_CENTER);
	gtk_box_pack_start (GTK_BOX (hbox), wid, TRUE, TRUE, 0);

	if (with_close) {
		image = gtk_image_new_from_icon_name ("window-close-symbolic", GTK_ICON_SIZE_MENU);
		close_button = gtk_button_new ();
		gtk_button_set_relief (GTK_BUTTON (close_button), GTK_RELIEF_NONE);
		gtk_widget_set_focus_on_click (GTK_WIDGET (close_button), FALSE);

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

/**
 * ui_make_tab_label_with_icon:
 */
GtkWidget *
ui_make_tab_label_with_icon (const gchar *label,
			     const gchar *icon_name, gboolean with_close,
			     GtkWidget **out_close_button)
{
	GtkWidget *image = NULL;
	if (icon_name)
		image = gtk_image_new_from_icon_name (icon_name, GTK_ICON_SIZE_MENU);

	return _make_tab_label (label, image, with_close, out_close_button);
}

/**
 * ui_make_tab_label_with_pixbuf:
 */
GtkWidget *
ui_make_tab_label_with_pixbuf (const gchar *label,
			       GdkPixbuf *pixbuf, gboolean with_close,
			       GtkWidget **out_close_button)
{
	GtkWidget *image = NULL;
	if (pixbuf)
		image = gtk_image_new_from_pixbuf (pixbuf);
	return _make_tab_label (label, image, with_close, out_close_button);
}


static const gchar *fnames[] = {
	"-bookmark.png",
	"-schema.png",
	"-table.png",
	"-column.png",
	"-column-pk.png",
	"-column-fk.png",
	"-column-fknn.png",
	"-column-nn.png",
	"-reference.png",
	"-diagram.png",
	"-query.png",
	"-action.png",

	"-menu-ind.png",

	"-ldap-entry.png",
	"-ldap-group.png",
	"-ldap-organization.png",
	"-ldap-person.png",
	"-ldap-class-s.png",
	"-ldap-class-a.png",
	"-ldap-class-x.png",
	"-ldap-class-u.png",

	/*
	"-auth-big.png",
	"-auth.png",

	"-connected-big.png",
	"-connected.png",

	"-non-connected-big.png",
	"-non-connected.png",
	"-table.png",
	"-view.png"
	*/
};

/**
 * ui_get_pixbuf_icon:
 * Returns: (transfer none): a #GkdPixbuf
 */
GdkPixbuf *
ui_get_pixbuf_icon (UiIconType type)
{
	g_return_val_if_fail (type >= 0, NULL);
	g_return_val_if_fail (type < UI_ICON_LAST, NULL);

	static GdkPixbuf **pixbufs = NULL;

	if (!pixbufs)
		pixbufs = g_new0 (GdkPixbuf*, UI_ICON_LAST);

	GdkPixbuf *pix;
	pix = pixbufs [type];

	if (pix)
		return pix;
	else {
		gchar *tmp;
		tmp = g_strdup_printf ("/images/gda-browser%s", fnames [type]);
		pix = gdk_pixbuf_new_from_resource (tmp, NULL);
		g_free (tmp);
		pixbufs [type] = pix;

		return pix;
	}
}

/**
 * ui_widget_add_toolbar:
 * @widget: the widget to wrap
 * @out_toolbar: a place holder for the new toolbar
 *
 * Wraps @widget and add a toolbar. The call is then responsible to add things to the toolbar.
 *
 * The returned widget is the one which needs to be incorporated in the UI by the caller, who is also responsible
 * for calling gtk_widget_show().
 *
 * Returns: a new #GtkWidget
 */
GtkWidget *
ui_widget_add_toolbar (GtkWidget *widget, GtkWidget **out_toolbar)
{
	g_return_val_if_fail (out_toolbar, NULL);
	g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

	GtkWidget *vbox;
	vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_set_margin_bottom (vbox, 6);

	GtkWidget *frame;
	frame = gtk_frame_new (NULL);
	gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_IN);
	gtk_box_pack_start (GTK_BOX (vbox), frame, TRUE, TRUE, 0);

	gtk_container_add (GTK_CONTAINER (frame), widget);
	gtk_widget_show (widget);

	GtkWidget *toolbar;
	toolbar = gtk_toolbar_new ();
	gtk_widget_set_halign (toolbar, GTK_ALIGN_START);
	gtk_toolbar_set_icon_size (GTK_TOOLBAR (toolbar), GTK_ICON_SIZE_SMALL_TOOLBAR);
	gtk_style_context_add_class (gtk_widget_get_style_context (toolbar), "inline-toolbar");
	gtk_box_pack_start (GTK_BOX (vbox), toolbar, FALSE, FALSE, 0);
	gtk_widget_show (toolbar);
	*out_toolbar = toolbar;

	return vbox;
}
