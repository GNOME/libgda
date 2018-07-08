/*
 * Copyright (C) 2011 Murray Cumming <murrayc@murrayc.com>
 * Copyright (C) 2011 - 2012 Vivien Malerba <malerba@gnome-db.org>
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
#include <gtk/gtk.h>
#include <string.h>
#include <libgda-ui/libgda-ui.h>
#include <libgda/libgda.h>
#include <libgda/virtual/gda-ldap-connection.h>

gchar *host = NULL;
gchar *base = NULL;
gchar *user = NULL;
gchar *password = NULL;
gboolean usessl = FALSE;

static GOptionEntry entries[] = {
        { "server", 'h', 0, G_OPTION_ARG_STRING, &host, "Server", NULL},
        { "basename", 'b', 0, G_OPTION_ARG_STRING, &base, "Base name", NULL},
        { "user", 'u', 0, G_OPTION_ARG_STRING, &user, "User", NULL},
        { "password", 'p', 0, G_OPTION_ARG_STRING, &password, "Password", NULL},
        { "usessl", 's', 0, G_OPTION_ARG_NONE, &usessl, "Use SSL", NULL},
        { NULL }
};


static GdaConnection *
open_ldap_connection ()
{
	GString *cncstring = NULL;
	gchar *enc;
	GdaConnection *cnc;
	GError *error = NULL;

	if (host) {
		cncstring = g_string_new ("");
		enc = gda_rfc1738_encode (host);
		g_string_append_printf (cncstring, "HOST=%s", enc);
		g_free (enc);
	}

	if (base) {
		if (cncstring)
			g_string_append_c (cncstring, ';');
		else
			cncstring = g_string_new ("");
		enc = gda_rfc1738_encode (base);
		g_string_append_printf (cncstring, "DB_NAME=%s", enc);
		g_free (enc);
	}

	if (user) {
		if (cncstring)
			g_string_append_c (cncstring, ';');
		else
			cncstring = g_string_new ("");
		enc = gda_rfc1738_encode (user);
		g_string_append_printf (cncstring, "USERNAME=%s", enc);
		g_free (enc);
	}

	if (password) {
		if (cncstring)
			g_string_append_c (cncstring, ';');
		else
			cncstring = g_string_new ("");
		enc = gda_rfc1738_encode (password);
		g_string_append_printf (cncstring, "PASSWORD=%s", enc);
		g_free (enc);
	}

	if (usessl) {
		if (cncstring)
			g_string_append_c (cncstring, ';');
		else
			cncstring = g_string_new ("");
		g_string_append (cncstring, "USER_SSL=TRUE");
	}
	
	if (! cncstring) {
		g_print ("No connection specified!\n");
		exit (1);
	}

	g_print ("Using connection string: %s\n", cncstring->str);
        cnc = gda_connection_open_from_string ("Ldap", cncstring->str,
                                               NULL,
                                               GDA_CONNECTION_OPTIONS_NONE, &error);
        if (!cnc) {
                g_print ("Error opening connection (cncstring=[%s]): %s\n",
                         cncstring->str,
                         error && error->message ? error->message : "No detail");
                exit (1);
        }
        g_string_free (cncstring, TRUE);

	return cnc;
}

typedef struct {
	GtkTreeView *tview;
	GdauiTreeStore *store;
	GdaTree *tree;
	GdaTreeNode *node;
} IdleData;

static void
idle_data_free (IdleData *data)
{
	g_object_unref (data->tview);
	g_object_unref (data->store);
	g_object_unref (data->tree);
	g_object_unref (data->node);
	g_free (data);
}

static gboolean ldap_update_tree_part (IdleData *data);
static gboolean
test_expand_row_cb (GtkTreeView *tree_view, GtkTreeIter *iter,
                    G_GNUC_UNUSED GtkTreePath *path, GdaTree *tree)
{
        GdaTreeNode *node;
	GtkTreeModel *store;

	store = gtk_tree_view_get_model (tree_view);
        node = gdaui_tree_store_get_node (GDAUI_TREE_STORE (store), iter);
        if (gda_tree_node_get_child_index (node, 0))
                return FALSE;

	const GValue *cv;
	cv = gda_tree_node_get_node_attribute (node,
					       GDA_ATTRIBUTE_TREE_NODE_UNKNOWN_CHILDREN);
	if (cv && (G_VALUE_TYPE (cv) == G_TYPE_BOOLEAN) &&
	    g_value_get_boolean (cv)) {
		IdleData *data;
		data = g_new (IdleData, 1);
		data->tview = g_object_ref (G_OBJECT (tree_view));
		data->store = g_object_ref (G_OBJECT (store));
		data->tree = g_object_ref (G_OBJECT (tree));
		data->node = g_object_ref (G_OBJECT (node));

		g_idle_add_full (G_PRIORITY_HIGH_IDLE, (GSourceFunc) ldap_update_tree_part,
				 data, (GDestroyNotify) idle_data_free);
	}
	return TRUE;
}

static gboolean
ldap_update_tree_part (IdleData *data)
{
        gda_tree_update_children (data->tree, data->node, NULL);

	GtkTreeIter iter;
	if (gdaui_tree_store_get_iter_from_node (data->store, &iter, data->node) &&
	    gda_tree_node_get_child_index (data->node, 0)) {
		GtkTreePath *path;
		path = gtk_tree_model_get_path (GTK_TREE_MODEL (data->store), &iter);
		g_signal_handlers_block_by_func (data->tview,
						 G_CALLBACK (test_expand_row_cb), data->tree);
		gtk_tree_view_expand_row (data->tview, path, FALSE);
		g_signal_handlers_unblock_by_func (data->tview,
						   G_CALLBACK (test_expand_row_cb), data->tree);
		gtk_tree_path_free (path);
	}

	return FALSE;
}

static void
selection_changed_cb (GtkTreeSelection *sel, GdauiTreeStore *store)
{
	GtkTreeIter iter;
	if (gtk_tree_selection_get_selected (sel, NULL, &iter)) {
		gchar *rdn;
		gtk_tree_model_get (GTK_TREE_MODEL (store), &iter, 0, &rdn, -1);
		g_print ("Selected RDN: %s\n", rdn);
		g_free (rdn);

		GdaTreeNode *node;
		const GValue *cvalue;
		node = gdaui_tree_store_get_node (store, &iter);
		g_assert (node);
		cvalue = gda_tree_node_get_node_attribute (node, "dn");
		g_assert (cvalue);
		g_print ("         DN:  %s\n", g_value_get_string (cvalue));
	}
}

static GtkWidget *
create_window (GdaConnection *cnc)
{
	GtkWidget *win = NULL;

	/* Window */
	win = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title  (GTK_WINDOW (win), "LDAP Browser");
	gtk_window_set_default_size (GTK_WINDOW (win), 640, 480);
	gtk_container_set_border_width (GTK_CONTAINER (win), 5);
	gtk_window_set_position (GTK_WINDOW (win), GTK_WIN_POS_CENTER);
	g_signal_connect (G_OBJECT (win), "destroy", gtk_main_quit, NULL);

	/* container */
	GtkWidget *vb, *hp;
	vb = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_container_add (GTK_CONTAINER (win), vb);
	hp = gtk_paned_new (GTK_ORIENTATION_HORIZONTAL);
        gtk_box_pack_start (GTK_BOX (vb), hp, TRUE, TRUE, 0);

	GdaTree *tree;
	GdaTreeManager *mgr;
	GtkTreeModel *store;
	GtkWidget *tview, *sw;
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;
	
	tree = gda_tree_new ();
	mgr = gda_tree_mgr_ldap_new (cnc, NULL);
	gda_tree_add_manager (tree, mgr);
	gda_tree_manager_add_manager (mgr, mgr);
	g_object_unref (mgr);

	gda_tree_update_children (tree, NULL, NULL);

	store = gdaui_tree_store_new (tree, 1, G_TYPE_STRING, "rdn");
	g_object_unref (tree);
	tview = gtk_tree_view_new_with_model (store);
	g_signal_connect (tview, "test-expand-row",
                          G_CALLBACK (test_expand_row_cb), tree);

	renderer = gtk_cell_renderer_text_new ();
        column = gtk_tree_view_column_new_with_attributes ("DN", renderer, "text", 0, NULL);
        gtk_tree_view_append_column (GTK_TREE_VIEW (tview), column);
        gtk_tree_view_set_expander_column (GTK_TREE_VIEW (tview), column);

	sw = gtk_scrolled_window_new (NULL, NULL);
        gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw), GTK_POLICY_AUTOMATIC,
                                        GTK_POLICY_AUTOMATIC);
        gtk_container_add (GTK_CONTAINER (sw), tview);
        gtk_paned_add1 (GTK_PANED (hp), sw);

	/* selection */
	GtkTreeSelection *sel;
	sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (tview));
	gtk_tree_selection_set_mode (sel, GTK_SELECTION_SINGLE);
	g_signal_connect (sel, "changed",
			  G_CALLBACK (selection_changed_cb), store);

	gtk_widget_show_all (vb);

	return win;
}

/*
 * Entree du programme:
 */
int main (int argc, char ** argv)
{
	GdaConnection *cnc;
	GOptionContext *context;
	GError *error = NULL;

	context = g_option_context_new (NULL);
        g_option_context_add_main_entries (context, entries, NULL);
        if (!g_option_context_parse (context, &argc, &argv, &error)) {
                g_print ("Can't parse arguments: %s", error->message);
                exit (1);
        }
        g_option_context_free (context);

	gtk_init (& argc, & argv);
	gdaui_init ();

	cnc = open_ldap_connection ();
	gtk_widget_show (create_window (cnc));
	gtk_main ();

	g_object_unref (cnc);

	return 0;
}
