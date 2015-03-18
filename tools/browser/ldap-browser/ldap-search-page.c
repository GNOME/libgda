/*
 * Copyright (C) 2011 Murray Cumming <murrayc@murrayc.com>
 * Copyright (C) 2011 - 2015 Vivien Malerba <malerba@gnome-db.org>
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
#include <string.h>
#include "ldap-search-page.h"
#include "filter-editor.h"
#include "../gdaui-bar.h"
#include "../ui-support.h"
#include "../browser-page.h"
#include "../browser-window.h"
#include "common/t-connection.h"
#include <virtual/gda-ldap-connection.h>
#include "../ui-formgrid.h"
#include "vtable-dialog.h"
#include <libgda/gda-debug-macros.h>

#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

typedef struct {
	gchar              *base_dn;
	gchar              *filter;
	gchar              *attributes;
	GdaLdapSearchScope  scope;
	GdaDataModel       *result;
} HistoryItem;

static void
history_item_free (HistoryItem *item)
{
	g_free (item->base_dn);
	g_free (item->filter);
	g_free (item->attributes);
	if (item->result)
		g_object_unref (item->result);
	g_free (item);
}

struct _LdapSearchPagePrivate {
	TConnection *tcnc;

	GtkWidget *search_entry;
	GtkWidget *result_view;

	GtkActionGroup *agroup;
	GArray *history_items; /* array of @HistoryItem */
	guint history_max_len; /* max allowed length of @history_items */
	gint current_hitem; /* index in @history_items, or -1 */
	gboolean add_hist_item;
};

static void ldap_search_page_class_init (LdapSearchPageClass *klass);
static void ldap_search_page_init       (LdapSearchPage *epage, LdapSearchPageClass *klass);
static void ldap_search_page_dispose   (GObject *object);

/* BrowserPage interface */
static void                 ldap_search_page_page_init (BrowserPageIface *iface);
static GtkActionGroup      *ldap_search_page_page_get_actions_group (BrowserPage *page);
static const gchar         *ldap_search_page_page_get_actions_ui (BrowserPage *page);
static GtkWidget           *ldap_search_page_page_get_tab_label (BrowserPage *page, GtkWidget **out_close_button);

static GObjectClass *parent_class = NULL;


/*
 * LdapSearchPage class implementation
 */

static void
ldap_search_page_class_init (LdapSearchPageClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->dispose = ldap_search_page_dispose;
}

static void
ldap_search_page_page_init (BrowserPageIface *iface)
{
	iface->i_get_actions_group = ldap_search_page_page_get_actions_group;
	iface->i_get_actions_ui = ldap_search_page_page_get_actions_ui;
	iface->i_get_tab_label = ldap_search_page_page_get_tab_label;
}

static void
ldap_search_page_init (LdapSearchPage *epage, G_GNUC_UNUSED LdapSearchPageClass *klass)
{
	epage->priv = g_new0 (LdapSearchPagePrivate, 1);
	epage->priv->history_items = g_array_new (FALSE, FALSE, sizeof (HistoryItem*));
	epage->priv->history_max_len = 20;
	epage->priv->add_hist_item = TRUE;

	gtk_orientable_set_orientation (GTK_ORIENTABLE (epage), GTK_ORIENTATION_VERTICAL);
}

static void
ldap_search_page_dispose (GObject *object)
{
	LdapSearchPage *epage = (LdapSearchPage *) object;

	/* free memory */
	if (epage->priv) {
		if (epage->priv->tcnc)
			g_object_unref (epage->priv->tcnc);
		if (epage->priv->agroup)
			g_object_unref (epage->priv->agroup);
		if (epage->priv->history_items) {
			guint i;
			for (i = 0; i < epage->priv->history_items->len; i++) {
				HistoryItem *hitem = g_array_index (epage->priv->history_items,
								    HistoryItem*, i);
				history_item_free (hitem);
			}
			g_array_free (epage->priv->history_items, TRUE);
		}
		g_free (epage->priv);
		epage->priv = NULL;
	}

	parent_class->dispose (object);
}

GType
ldap_search_page_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo info = {
			sizeof (LdapSearchPageClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) ldap_search_page_class_init,
			NULL,
			NULL,
			sizeof (LdapSearchPage),
			0,
			(GInstanceInitFunc) ldap_search_page_init,
			0
		};

		static GInterfaceInfo page_info = {
                        (GInterfaceInitFunc) ldap_search_page_page_init,
			NULL,
                        NULL
                };

		type = g_type_register_static (GTK_TYPE_BOX, "LdapSearchPage", &info, 0);
		g_type_add_interface_static (type, BROWSER_PAGE_TYPE, &page_info);
	}
	return type;
}

static void
update_history_actions (LdapSearchPage *epage)
{
	if (!epage->priv->agroup)
		return;
	/*
	gboolean is_first = TRUE;
	gboolean is_last = TRUE;
	const gchar *current_classname;
	epage->priv->current_hitem = -1;
	current_classname = ldap_search_page_get_current_class (epage);
	if (current_classname) {
		guint i;
		for (i = 0; i < epage->priv->history_items->len; i++) {
			HistoryItem *hitem;
			hitem = g_array_index (epage->priv->history_items, HistoryItem*, i);
			if (!strcmp (hitem->classname, current_classname)) {
				if (i != 0)
					is_first = FALSE;
				if (i+1 < epage->priv->history_items->len)
					is_last = FALSE;
				epage->priv->current_hitem = (gint) i;
				break;
			}
		}
	}

	GtkAction *action;
	action = gtk_action_group_get_action (epage->priv->agroup, "DnBack");
	gtk_action_set_sensitive (action, !is_first);
	action = gtk_action_group_get_action (epage->priv->agroup, "DnForward");
	gtk_action_set_sensitive (action, !is_last);
	*/
}

static void
filter_exec_clicked_cb (G_GNUC_UNUSED GtkWidget *button, LdapSearchPage *epage)
{
	guint id;
	gchar *base_dn, *filter, *attributes;
	GdaLdapSearchScope scope;
	GError *lerror = NULL;
	filter_editor_get_settings (FILTER_EDITOR (epage->priv->search_entry),
				    &base_dn, &filter, &attributes, &scope);

	GdaDataModel *model;
	model = t_connection_ldap_search (epage->priv->tcnc,
						base_dn, filter,
						attributes, scope, &lerror);
	g_free (base_dn);
	g_free (filter);
	g_free (attributes);

	if (epage->priv->result_view) {
		gtk_widget_destroy (epage->priv->result_view);
		epage->priv->result_view = NULL;
	}

	if (model) {
		GtkWidget *wid;
		wid = ui_formgrid_new (model, TRUE, GDAUI_DATA_PROXY_INFO_CURRENT_ROW);
		g_object_unref (model);
		gtk_box_pack_start (GTK_BOX (epage), wid, TRUE, TRUE, 0);
		epage->priv->result_view = wid;
		gtk_widget_show (wid);
	}
	else {
		TO_IMPLEMENT;
	}
}

static void
filter_clear_clicked_cb (G_GNUC_UNUSED GtkWidget *button, LdapSearchPage *epage)
{
	filter_editor_clear (FILTER_EDITOR (epage->priv->search_entry));
}

static void
search_entry_activated_cb (G_GNUC_UNUSED FilterEditor *feditor, LdapSearchPage *epage)
{
	filter_exec_clicked_cb (NULL, epage);
}

/**
 * ldap_search_page_new:
 * @tcnc:
 * @base_dn: the base DN to put in the search page by default
 *
 * Returns: a new #GtkWidget
 */
GtkWidget *
ldap_search_page_new (TConnection *tcnc, const gchar *base_dn)
{
	LdapSearchPage *epage;

	g_return_val_if_fail (T_IS_CONNECTION (tcnc), NULL);

	epage = LDAP_SEARCH_PAGE (g_object_new (LDAP_SEARCH_PAGE_TYPE, NULL));
	epage->priv->tcnc = g_object_ref ((GObject*) tcnc);

	/* header */
        GtkWidget *label;
	gchar *str;

	str = g_strdup_printf ("<b>%s</b>", _("LDAP search page"));
	label = gdaui_bar_new (str);
	g_free (str);
        gtk_box_pack_start (GTK_BOX (epage), label, FALSE, FALSE, 0);
        gtk_widget_show (label);

	/* search filter settings */
	GtkWidget *hb, *bb, *button, *wid;
	gchar *tmp;
	tmp = g_markup_printf_escaped ("<b>%s:</b>", _("LDAP search settings"));
	label = gtk_label_new ("");
	gtk_label_set_markup (GTK_LABEL (label), tmp);
	g_free (tmp);
	gtk_widget_set_halign (label, GTK_ALIGN_START);
	gtk_box_pack_start (GTK_BOX (epage), label, FALSE, FALSE, 0);

	hb = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_pack_start (GTK_BOX (epage), hb, FALSE, FALSE, 3);

	wid = filter_editor_new (tcnc);
	if (!base_dn)
		base_dn = t_connection_ldap_get_base_dn (tcnc);
	filter_editor_set_settings (FILTER_EDITOR (wid), base_dn, NULL, NULL,
				    GDA_LDAP_SEARCH_SUBTREE);
	gtk_box_pack_start (GTK_BOX (hb), wid, TRUE, TRUE, 0);
	epage->priv->search_entry = wid;
	g_signal_connect (wid, "activate",
			  G_CALLBACK (search_entry_activated_cb), epage);

	bb = gtk_button_box_new (GTK_ORIENTATION_VERTICAL);
	gtk_button_box_set_layout (GTK_BUTTON_BOX (bb), GTK_BUTTONBOX_END);
	gtk_box_pack_start (GTK_BOX (hb), bb, FALSE, FALSE, 5);

	button = ui_make_small_button (FALSE, FALSE, _("Clear"),
				       GTK_STOCK_CLEAR, _("Clear the search settings"));
	gtk_box_pack_start (GTK_BOX (bb), button, TRUE, TRUE, 0);
	g_signal_connect (button, "clicked",
			  G_CALLBACK (filter_clear_clicked_cb), epage);

	button = ui_make_small_button (FALSE, FALSE, _("Execute"),
				       GTK_STOCK_EXECUTE, _("Execute LDAP search"));
	gtk_box_pack_start (GTK_BOX (bb), button, TRUE, TRUE, 0);
	g_signal_connect (button, "clicked",
			  G_CALLBACK (filter_exec_clicked_cb), epage);

	/* results */
	tmp = g_markup_printf_escaped ("<b>%s:</b>", _("Results"));
	label = gtk_label_new ("");
	gtk_label_set_markup (GTK_LABEL (label), tmp);
	g_free (tmp);
	gtk_widget_set_halign (label, GTK_ALIGN_START);
	gtk_box_pack_start (GTK_BOX (epage), label, FALSE, FALSE, 5);

	wid = ui_formgrid_new (NULL, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (epage), wid, TRUE, TRUE, 0);
	epage->priv->result_view = wid;

	gtk_widget_show_all ((GtkWidget*) epage);
	gtk_widget_hide ((GtkWidget*) epage);

	return (GtkWidget*) epage;
}

/*
 * UI actions
 */
static void
action_define_as_table_cb (G_GNUC_UNUSED GtkAction *action, LdapSearchPage *epage)
{
	GtkWidget *dlg;
	gint res;
	GtkWindow *parent;

	parent = (GtkWindow*) gtk_widget_get_toplevel ((GtkWidget*) epage);

	dlg = vtable_dialog_new (parent, epage->priv->tcnc);
	res = gtk_dialog_run (GTK_DIALOG (dlg));
	gtk_widget_hide (dlg);
	if (res == GTK_RESPONSE_OK) {
		gchar *base_dn, *filter, *attributes;
		GdaLdapSearchScope scope;
		GError *lerror = NULL;
		const gchar *tname;
		gboolean replace;
		filter_editor_get_settings (FILTER_EDITOR (epage->priv->search_entry),
					    &base_dn, &filter, &attributes, &scope);
		tname = vtable_dialog_get_table_name (VTABLE_DIALOG (dlg));
		replace = vtable_dialog_get_replace_if_exists (VTABLE_DIALOG (dlg));
		if (replace)
			t_connection_undeclare_table (epage->priv->tcnc, tname, NULL);
		if (! t_connection_declare_table (epage->priv->tcnc, tname, base_dn, filter,
							attributes, scope, &lerror)) {
			ui_show_error (parent,
					    _("Could not define virtual table for this LDAP search: %s"),
					    lerror && lerror->message ? lerror->message : _("No detail"));
			g_clear_error (&lerror);
		}
		else
			ui_show_message (parent,
					      _("Virtual table '%s' for this LDAP search has been defined"),
					      tname);
	}
	gtk_widget_destroy (dlg);
}
/*
static void
action_class_back_cb (G_GNUC_UNUSED GtkAction *action, LdapSearchPage *epage)
{
	epage->priv->add_hist_item = FALSE;
	if (epage->priv->current_hitem > 0) {
		HistoryItem *hitem = NULL;
		gboolean use_dn = TRUE;
		hitem = g_array_index (epage->priv->history_items, HistoryItem*,
				       epage->priv->current_hitem - 1);
		if (hitem->reference) {
			if (gtk_tree_row_reference_valid (hitem->reference)) {
				GtkTreeSelection *sel;
				GtkTreePath *path;
				path = gtk_tree_row_reference_get_path (hitem->reference);
				sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (epage->priv->search_view));
				gtk_tree_selection_select_path (sel, path);
				gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW (epage->priv->search_view),
							      path, NULL, TRUE, 0.5, 0.5);
				gtk_tree_path_free (path);
				use_dn = FALSE;
			}
			else {
				gtk_tree_row_reference_free (hitem->reference);
				hitem->reference = NULL;
			}
		}
		if (use_dn)
			search_view_set_current_class (SEARCH_VIEW (epage->priv->search_view),
						       hitem->classname);
	}
	epage->priv->add_hist_item = TRUE;
}

static void
action_class_forward_cb (G_GNUC_UNUSED GtkAction *action, LdapSearchPage *epage)
{
	epage->priv->add_hist_item = FALSE;
	if ((epage->priv->current_hitem >=0) &&
	    ((guint) epage->priv->current_hitem < epage->priv->history_items->len)) {
		HistoryItem *hitem = NULL;
		gboolean use_dn = TRUE;
		hitem = g_array_index (epage->priv->history_items, HistoryItem*,
				       epage->priv->current_hitem + 1);
		if (hitem->reference) {
			if (gtk_tree_row_reference_valid (hitem->reference)) {
				GtkTreeSelection *sel;
				GtkTreePath *path;
				path = gtk_tree_row_reference_get_path (hitem->reference);
				sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (epage->priv->search_view));
				gtk_tree_selection_select_path (sel, path);
				gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW (epage->priv->search_view),
							      path, NULL, TRUE, 0.5, 0.5);
				gtk_tree_path_free (path);
				use_dn = FALSE;
			}
			else {
				gtk_tree_row_reference_free (hitem->reference);
				hitem->reference = NULL;
			}
		}
		if (use_dn)
			search_view_set_current_class (SEARCH_VIEW (epage->priv->search_view),
						       hitem->classname);
	}
	epage->priv->add_hist_item = TRUE;
}

*/
static GtkActionEntry ui_actions[] = {
	{ "LDAP", NULL, N_("_LDAP"), NULL, N_("LDAP"), NULL },
	{ "DefineAsTable", /*BROWSER_STOCK_TABLE_ADD*/ NULL, N_("Define as Table"), NULL, N_("Define search as a virtual table"),
	  G_CALLBACK (action_define_as_table_cb)},
	/*
	{ "DnBack", GTK_STOCK_GO_BACK, N_("Previous class"), NULL, N_("Move back to previous LDAP class"),
	  G_CALLBACK (action_class_back_cb)},
	{ "DnForward", GTK_STOCK_GO_FORWARD, N_("Next class"), NULL, N_("Move to next LDAP class"),
	  G_CALLBACK (action_class_forward_cb)},
	*/
};
static const gchar *ui_actions_page =
	"<ui>"
	"  <menubar name='MenuBar'>"
	"    <placeholder name='MenuExtension'>"
        "      <menu name='LDAP' action='LDAP'>"
        "        <menuitem name='DefineAsTable' action= 'DefineAsTable'/>"
	/*"        <separator/>"
        "        <menuitem name='DnBack' action= 'DnBack'/>"
	  "        <menuitem name='DnForward' action= 'DnForward'/>"*/
        "      </menu>"
        "    </placeholder>"
	"  </menubar>"
	"  <toolbar name='ToolBar'>"
	"    <separator/>"
	"    <toolitem action='DefineAsTable'/>"
	/*"    <toolitem action='DnBack'/>"
	  "    <toolitem action='DnForward'/>"*/
	"  </toolbar>"
	"</ui>";

static GtkActionGroup *
ldap_search_page_page_get_actions_group (BrowserPage *page)
{
	LdapSearchPage *epage;

	epage = LDAP_SEARCH_PAGE (page);
	if (! epage->priv->agroup) {
		GtkActionGroup *agroup;
		agroup = gtk_action_group_new ("LdapLdapSearchPageActions");
		gtk_action_group_set_translation_domain (agroup, GETTEXT_PACKAGE);
		gtk_action_group_add_actions (agroup, ui_actions, G_N_ELEMENTS (ui_actions), page);
		epage->priv->agroup = agroup;

		GtkAction *action;
		gchar *base_dn, *filter, *attributes;
		gboolean act = FALSE;
		action = gtk_action_group_get_action (agroup, "DefineAsTable");
		filter_editor_get_settings (FILTER_EDITOR (epage->priv->search_entry),
					    &base_dn, &filter, &attributes, NULL);
		if ((base_dn && *base_dn) || (filter && *filter) || (attributes && *attributes))
			act = TRUE;
		g_free (base_dn);
		g_free (filter);
		g_free (attributes);
		gtk_action_set_sensitive (action, act);

		update_history_actions (epage);
	}
	
	return g_object_ref (epage->priv->agroup);
}

static const gchar *
ldap_search_page_page_get_actions_ui (G_GNUC_UNUSED BrowserPage *page)
{
	return ui_actions_page;
}

static GtkWidget *
ldap_search_page_page_get_tab_label (BrowserPage *page, GtkWidget **out_close_button)
{
	const gchar *tab_name;
	GdkPixbuf *search_pixbuf;

	search_pixbuf = gtk_widget_render_icon_pixbuf (GTK_WIDGET (page), GTK_STOCK_FIND, GTK_ICON_SIZE_MENU);
	tab_name = _("LDAP search");
	return ui_make_tab_label_with_pixbuf (tab_name,
						   search_pixbuf,
						   out_close_button ? TRUE : FALSE, out_close_button);
}
