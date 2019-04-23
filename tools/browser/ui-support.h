/*
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

#ifndef __UI_SUPPORT_H_
#define __UI_SUPPORT_H_

#include <libgda/libgda.h>
#include <gtk/gtk.h>
#ifdef HAVE_LDAP
  #include <providers/ldap/gda-ldap-connection.h>
  #include <providers/ldap/gda-ldap-connection.h>
#endif /* HAVE_LDAP */
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <common/t-connection.h>
#include <libgda-ui/internal/utility.h>

G_BEGIN_DECLS

#ifdef HAVE_LDAP
GdkPixbuf           *ui_connection_ldap_icon_for_dn (TConnection *tcnc, const gchar *dn, GError **error);
GdkPixbuf           *ui_connection_ldap_icon_for_class_kind (GdaLdapClassKind kind);
GdkPixbuf           *ui_connection_ldap_icon_for_class (GdaLdapAttribute *objectclass);
const gchar         *ui_connection_ldap_class_kind_to_string (GdaLdapClassKind kind);

#endif /* HAVE_LDAP */

void                 ui_show_error (GtkWindow *parent, const gchar *format, ...);
void                 ui_show_message (GtkWindow *parent, const gchar *format, ...);
#ifdef HAVE_GDU
void                 ui_show_help (GtkWindow *parent, const gchar *topic);
#endif

GtkWidget           *ui_find_parent_widget (GtkWidget *current, GType requested_type);
GtkWidget           *ui_make_tree_view (GtkTreeModel *model);


GtkWidget           *ui_make_small_button (gboolean is_toggle, gboolean with_arrow,
					   const gchar *label, const gchar *icon_name,
					   const gchar *tooltip);
GtkWidget           *ui_make_tab_label_with_icon (const gchar *label,
						  const gchar *icon_name, gboolean with_close,
						  GtkWidget **out_close_button);
GtkWidget           *ui_make_tab_label_with_pixbuf (const gchar *label,
						    GdkPixbuf *pixbuf, gboolean with_close,
						    GtkWidget **out_close_button);
GtkWidget           *ui_widget_add_toolbar (GtkWidget *widget, GtkWidget **out_toolbar);

/*
 * icons, use with ui_get_pixbuf_icon() for the associated icons
 */
typedef enum {
        UI_ICON_BOOKMARK,
        UI_ICON_SCHEMA,
        UI_ICON_TABLE,
        UI_ICON_COLUMN,
        UI_ICON_COLUMN_PK,
        UI_ICON_COLUMN_FK,
        UI_ICON_COLUMN_FK_NN,
        UI_ICON_COLUMN_NN,
        UI_ICON_REFERENCE,
        UI_ICON_DIAGRAM,
        UI_ICON_QUERY,
        UI_ICON_ACTION,

        UI_ICON_MENU_INDICATOR,

        UI_ICON_LDAP_ENTRY,
        UI_ICON_LDAP_GROUP,
        UI_ICON_LDAP_ORGANIZATION,
        UI_ICON_LDAP_PERSON,
        UI_ICON_LDAP_CLASS_STRUCTURAL,
        UI_ICON_LDAP_CLASS_ABSTRACT,
        UI_ICON_LDAP_CLASS_AUXILIARY,
        UI_ICON_LDAP_CLASS_UNKNOWN,

        UI_ICON_LAST
} UiIconType;

GdkPixbuf           *ui_get_pixbuf_icon (UiIconType type);

#define VARIABLES_HELP _("<small>This area allows to give values to\n" \
                         "variables defined in the SQL code\n"          \
                         "using the following syntax:\n"                \
                         "<b><tt>##&lt;variable name&gt;::&lt;type&gt;[::null]</tt></b>\n" \
                         "For example:\n"                               \
                         "<span foreground=\"#4e9a06\"><b><tt>##id::int</tt></b></span>\n      defines <b>id</b> as a non NULL integer\n" \
                         "<span foreground=\"#4e9a06\"><b><tt>##age::string::null</tt></b></span>\n      defines <b>age</b> as a string\n\n" \
                         "Valid types are: <tt>string</tt>, <tt>boolean</tt>, <tt>int</tt>,\n" \
                         "<tt>date</tt>, <tt>time</tt>, <tt>timestamp</tt>, <tt>guint</tt>, <tt>blob</tt> and\n" \
                         "<tt>binary</tt></small>")

G_END_DECLS

#endif
