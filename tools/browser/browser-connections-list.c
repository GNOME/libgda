/*
 * Copyright (C) 2009 - 2011 Vivien Malerba <malerba@gnome-db.org>
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

#include <string.h>
#include <glib/gi18n-lib.h>
#include <libgda/binreloc/gda-binreloc.h>
#include "browser-connections-list.h"
#include "browser-core.h"
#include "browser-window.h"
#include "browser-connection.h"
#include <libgda-ui/gdaui-login.h>
#include "support.h"
#include "login-dialog.h"

/* 
 * Main static functions 
 */
static void browser_connections_list_class_init (BrowserConnectionsListClass *klass);
static void browser_connections_list_init (BrowserConnectionsList *stmt);
static void browser_connections_list_dispose (GObject *object);

static void connection_added_cb (BrowserCore *bcore, BrowserConnection *bcnc, BrowserConnectionsList *clist);
static void connection_removed_cb (BrowserCore *bcore, BrowserConnection *bcnc, BrowserConnectionsList *clist);

struct _BrowserConnectionsListPrivate
{
	GtkGrid     *layout_grid;
	GtkTreeView *treeview;
	gulong       cnc_added_sigid;
	gulong       cnc_removed_sigid;

	GtkWidget   *cnc_params_editor;

	GtkWidget   *close_cnc_button;
};

/* get a pointer to the parents to be able to call their destructor */
static GObjectClass  *parent_class = NULL;
static BrowserConnectionsList *_clist = NULL;

GType
browser_connections_list_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static GMutex registering;
		static const GTypeInfo info = {
			sizeof (BrowserConnectionsListClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) browser_connections_list_class_init,
			NULL,
			NULL,
			sizeof (BrowserConnectionsList),
			0,
			(GInstanceInitFunc) browser_connections_list_init,
			0
		};

		g_mutex_lock (&registering);
		if (type == 0)
			type = g_type_register_static (GTK_TYPE_WINDOW, "BrowserConnectionsList", &info, 0);
		g_mutex_unlock (&registering);
	}
	return type;
}

static void
browser_connections_list_class_init (BrowserConnectionsListClass * klass)
{
	GObjectClass   *object_class = G_OBJECT_CLASS (klass);
	parent_class = g_type_class_peek_parent (klass);

	object_class->dispose = browser_connections_list_dispose;
}

static void
browser_connections_list_init (BrowserConnectionsList *clist)
{
	clist->priv = g_new0 (BrowserConnectionsListPrivate, 1);
	clist->priv->treeview = NULL;
	clist->priv->cnc_params_editor = NULL;
	clist->priv->cnc_added_sigid = 0;
	clist->priv->cnc_removed_sigid = 0;
}

static void
browser_connections_list_dispose (GObject *object)
{
	BrowserConnectionsList *clist;

	g_return_if_fail (object != NULL);
	g_return_if_fail (BROWSER_IS_CONNECTIONS_LIST (object));

	clist = BROWSER_CONNECTIONS_LIST (object);

	if (clist->priv) {
		if (clist->priv->cnc_added_sigid > 0)
			g_signal_handler_disconnect (browser_core_get (), clist->priv->cnc_added_sigid);
		if (clist->priv->cnc_removed_sigid > 0)
			g_signal_handler_disconnect (browser_core_get (), clist->priv->cnc_removed_sigid);

		g_free (clist->priv);
		clist->priv = NULL;
	}

	/* parent class */
	parent_class->dispose (object);
}

enum
{
	COLUMN_BCNC,
	NUM_COLUMNS
};

static gboolean
delete_event (GtkWidget *widget, G_GNUC_UNUSED GdkEvent *event, G_GNUC_UNUSED gpointer data)
{
	gtk_widget_hide (widget);
	return TRUE;
}

static void cell_name_data_func (G_GNUC_UNUSED GtkTreeViewColumn *tree_column,
				 GtkCellRenderer *cell,
				 GtkTreeModel *tree_model,
				 GtkTreeIter *iter,
				 G_GNUC_UNUSED gpointer data)
{
	BrowserConnection *bcnc;
	gchar *str, *cncstr = NULL;
	gchar *markup;
	const GdaDsnInfo *cncinfo;

	gtk_tree_model_get (tree_model, iter, COLUMN_BCNC, &bcnc, -1);
	cncinfo = browser_connection_get_information (bcnc);
	if (cncinfo) {
		if (cncinfo->name)
			cncstr = g_strdup_printf (_("DSN: %s"), cncinfo->name);
		else if (cncinfo->provider)
			cncstr = g_strdup_printf (_("Provider: %s"), cncinfo->provider);
	}

	markup = g_markup_escape_text (browser_connection_get_name (bcnc), -1);
	if (cncstr)
		str = g_strdup_printf ("%s\n<small>%s</small>",
				       markup, cncstr);
	else
		str = g_strdup (markup);
	g_free (cncstr);
	g_free (markup);
	
	g_object_set ((GObject*) cell, "markup", str, NULL);
	g_free (str);
	g_object_unref (bcnc);
}

static void
selection_changed_cb (GtkTreeSelection *select, BrowserConnectionsList *clist)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	BrowserConnection *bcnc = NULL;
	const GdaDsnInfo *cncinfo = NULL;

	if (gtk_tree_selection_get_selected (select, &model, &iter)) {
		gtk_tree_model_get (model, &iter, COLUMN_BCNC, &bcnc, -1);
		cncinfo = browser_connection_get_information (bcnc);
		g_object_unref (bcnc);

		gtk_widget_set_sensitive (_clist->priv->close_cnc_button, TRUE);
	}
	else
		gtk_widget_set_sensitive (_clist->priv->close_cnc_button, FALSE);

	if (clist->priv->cnc_params_editor) {
		gtk_widget_destroy (clist->priv->cnc_params_editor);
		clist->priv->cnc_params_editor = NULL;
	}
	
	if (cncinfo && cncinfo->provider) {
		/* create GdaSet for parameters to display */
		GdaSet *dset;
		GdaHolder *holder;
		dset = gda_set_new_inline (1, "PROVIDER_NAME", G_TYPE_STRING, cncinfo->provider);
		holder = GDA_HOLDER (dset->holders->data);
		g_object_set (G_OBJECT (holder), "name", _("Database provider"), NULL);

		GdaProviderInfo *pinfo;
		pinfo = gda_config_get_provider_info (cncinfo->provider);
		if (pinfo && pinfo->dsn_params)
			gda_set_merge_with_set (dset, pinfo->dsn_params);

		holder = gda_holder_new_inline (G_TYPE_STRING, "GDA_BROWSER_DICT_FILE", _("In memory"));
		g_object_set (G_OBJECT (holder),
			      "name", _("Dictionary file"),
			      "description", _("File used to store any information associated\n"
					       "to this connection (favorites, descriptions, ...)"), NULL);
		gda_set_add_holder (dset, holder);
		g_object_unref (holder);
		if (bcnc) {
			const gchar *dict_file_name;
			dict_file_name = browser_connection_get_dictionary_file (bcnc);
			
			if (dict_file_name)
				gda_set_set_holder_value (dset, NULL, "GDA_BROWSER_DICT_FILE",
							  dict_file_name);
		}

		/* create form */
		GtkWidget *wid;
		wid = gdaui_basic_form_new (dset);
		g_object_set ((GObject*) wid, "show-actions", FALSE, NULL);
		gdaui_basic_form_entry_set_editable (GDAUI_BASIC_FORM (wid), NULL, FALSE);
		gtk_grid_attach (clist->priv->layout_grid, wid, 1, 2, 1, 1);
		gtk_widget_show (wid);
		clist->priv->cnc_params_editor = wid;

		/* fill GdaSet's parameters with values */
		if (cncinfo->cnc_string) {
                        gchar **array = NULL;

                        array = g_strsplit (cncinfo->cnc_string, ";", 0);
                        if (array) {
                                gint index = 0;
                                gchar *tok;
                                gchar *value;
                                gchar *name;

                                for (index = 0; array[index]; index++) {
                                        name = strtok_r (array [index], "=", &tok);
                                        if (name)
                                                value = strtok_r (NULL, "=", &tok);
                                        else
                                                value = NULL;
                                        if (name && value) {
                                                GdaHolder *param;
                                                gda_rfc1738_decode (name);
                                                gda_rfc1738_decode (value);

                                                param = gda_set_get_holder (dset, name);
                                                if (param)
                                                        g_assert (gda_holder_set_value_str (param, NULL, value, NULL));
                                        }
                                }

                                g_strfreev (array);
                        }
                }
		
		g_object_unref (dset);
	}
}

static void
connection_close_cb (G_GNUC_UNUSED GtkButton *button, BrowserConnectionsList *clist)
{
	GtkTreeSelection *select;
	GtkTreeModel *model;
	GtkTreeIter iter;
	
	select = gtk_tree_view_get_selection (clist->priv->treeview);
	if (gtk_tree_selection_get_selected (select, &model, &iter)) {
		BrowserConnection *bcnc;
		gtk_tree_model_get (model, &iter, COLUMN_BCNC, &bcnc, -1);
		g_object_unref (bcnc);
		browser_connection_close (NULL, bcnc);
	}
}

static void
connection_new_cb (G_GNUC_UNUSED GtkButton *button, G_GNUC_UNUSED BrowserConnectionsList *clist)
{
	LoginDialog *dialog;
	GdaConnection *cnc;
	GError *error = NULL;
	dialog = login_dialog_new (NULL);
	
	cnc = login_dialog_run (dialog, TRUE, &error);
	if (cnc) {
		BrowserConnection *bcnc;
		BrowserWindow *nbwin;
		bcnc = browser_connection_new (cnc);
		nbwin = browser_window_new (bcnc, NULL);

		browser_core_take_window (nbwin);
		browser_core_take_connection (bcnc);
	}
}

/**
 * browser_connections_list_show
 * @current: (allow-none): a connection to select for displaed properties, or %NULL
 *
 * Creates a new #BrowserConnectionsList widget and displays it.
 * Only one is created and shown (singleton)
 *
 * Returns: the new object
 */
void
browser_connections_list_show (BrowserConnection *current)
{
	if (!_clist) {
		GtkWidget *clist, *sw, *grid, *treeview, *label, *wid;
		gchar *str;
		clist = GTK_WIDGET (g_object_new (BROWSER_TYPE_CONNECTIONS_LIST, 
						  NULL));
		gtk_window_set_default_size ((GtkWindow*) clist, 550, 450);
		_clist = (BrowserConnectionsList *) clist;
		gtk_window_set_title (GTK_WINDOW (clist), _("Opened connections"));
		gtk_container_set_border_width (GTK_CONTAINER (clist), 6);
		g_signal_connect (G_OBJECT (clist), "delete-event",
				  G_CALLBACK (delete_event), NULL);

		str = gda_gbr_get_file_path (GDA_DATA_DIR, LIBGDA_ABI_NAME, "pixmaps", "gda-browser-connected.png", NULL);
		gtk_window_set_icon_from_file (GTK_WINDOW (clist), str, NULL);
		g_free (str);

		/* table layout */
		grid = gtk_grid_new ();
		gtk_grid_set_column_spacing (GTK_GRID (grid), 10);
		gtk_grid_set_row_spacing (GTK_GRID (grid), 6);
		gtk_container_add (GTK_CONTAINER (clist), grid);
		_clist->priv->layout_grid = GTK_GRID (grid);

		/* image and explaining label */
		GtkWidget *hbox;
		hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
		gtk_grid_attach (GTK_GRID (grid), hbox, 0, 0, 3, 1);

		str = gda_gbr_get_file_path (GDA_DATA_DIR, LIBGDA_ABI_NAME, "pixmaps", "gda-browser-connected-big.png", NULL);
		wid = gtk_image_new_from_file (str);
		g_free (str);
		gtk_box_pack_start (GTK_BOX (hbox), wid, FALSE, FALSE, 0);

		wid = gtk_label_new ("");
		str = g_strdup_printf ("<big><b>%s:\n</b></big>%s",
				       _("List of opened connections"),
				       "The connection properties are read-only.");
		gtk_label_set_markup (GTK_LABEL (wid), str);
		g_free (str);
		gtk_misc_set_alignment (GTK_MISC (wid), 0., -1);
		gtk_box_pack_start (GTK_BOX (hbox), wid, TRUE, FALSE, 6);

		/* left column */		
		label = gtk_label_new ("");
		str = g_strdup_printf ("<b>%s:</b>", _("Connections"));
		gtk_label_set_markup (GTK_LABEL (label), str);
		g_free (str);
		gtk_misc_set_alignment (GTK_MISC (label), 0., -1);
		gtk_grid_attach (GTK_GRID (grid), label, 0, 1, 1, 1);

		sw = gtk_scrolled_window_new (NULL, NULL);
		gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw),
						     GTK_SHADOW_ETCHED_IN);
		gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
						GTK_POLICY_NEVER,
						GTK_POLICY_AUTOMATIC);
		gtk_grid_attach (GTK_GRID (grid), sw, 0, 2, 1, 2);
		
		/* connection's properties */
		label = gtk_label_new ("");
		str = g_strdup_printf ("<b>%s:</b>", _("Connection's properties"));
		gtk_label_set_markup (GTK_LABEL (label), str);
		g_free (str);
		gtk_misc_set_alignment (GTK_MISC (label), 0., -1);
		gtk_grid_attach (GTK_GRID (grid), label, 1, 1, 1, 1);

		/* buttons at the bottom*/
		GtkWidget *bbox, *button;
		bbox = gtk_button_box_new (GTK_ORIENTATION_HORIZONTAL);
		gtk_grid_attach (GTK_GRID (grid), bbox, 1, 3, 1, 1);
		gtk_button_box_set_layout (GTK_BUTTON_BOX (bbox), GTK_BUTTONBOX_END);
		button = gtk_button_new_with_label (_("Close connection"));
		gtk_box_pack_start (GTK_BOX (bbox), button, TRUE, TRUE, 0);
		g_signal_connect (button, "clicked",
				  G_CALLBACK (connection_close_cb), clist);
		gtk_widget_set_tooltip_text (button, _("Close selected connection"));
		_clist->priv->close_cnc_button = button;

		button = gtk_button_new_with_label (_("Connect"));
		gtk_box_pack_start (GTK_BOX (bbox), button, TRUE, TRUE, 0);
		g_signal_connect (button, "clicked",
				  G_CALLBACK (connection_new_cb), clist);
		gtk_widget_set_tooltip_text (button, _("Open a new connection"));

		/* GtkTreeModel and view */
		GtkListStore *store;
		store = gtk_list_store_new (NUM_COLUMNS,
					    BROWSER_TYPE_CONNECTION);
		
		treeview = browser_make_tree_view (GTK_TREE_MODEL (store));
		_clist->priv->treeview = GTK_TREE_VIEW (treeview);
		gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (treeview), FALSE);
		gtk_tree_view_set_rules_hint (GTK_TREE_VIEW (treeview), TRUE);
		g_object_unref (G_OBJECT (store));
		gtk_container_add (GTK_CONTAINER (sw), treeview);

		/* treeview's columns */
		GtkTreeViewColumn *col;
		GtkCellRenderer *cell;
		cell = gtk_cell_renderer_text_new ();
		col = gtk_tree_view_column_new ();
		gtk_tree_view_column_pack_start (col, cell, TRUE);
		gtk_tree_view_column_set_cell_data_func (col, cell, 
							 (GtkTreeCellDataFunc) cell_name_data_func, NULL, NULL);
		gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), col);
		
		/* selection handling */
		GtkTreeSelection *select;
		select = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
		gtk_tree_selection_set_mode (select, GTK_SELECTION_SINGLE);
		g_signal_connect (G_OBJECT (select), "changed",
				  G_CALLBACK (selection_changed_cb), clist);
		

		/* initial filling */
		GSList *connections, *list;
		connections =  browser_core_get_connections ();
		for (list = connections; list; list = list->next)
			connection_added_cb (browser_core_get(), BROWSER_CONNECTION (list->data),
					     (BrowserConnectionsList*) clist);
		g_slist_free (connections);

		_clist->priv->cnc_added_sigid = g_signal_connect (browser_core_get (), "connection-added",
								  G_CALLBACK (connection_added_cb), _clist);
		_clist->priv->cnc_removed_sigid = g_signal_connect (browser_core_get (), "connection-removed",
								    G_CALLBACK (connection_removed_cb), _clist);
		
		gtk_widget_show_all (clist);
	}
	else {
		gtk_window_set_screen (GTK_WINDOW (_clist), gdk_screen_get_default ()); /* FIXME: specify GdkScreen */
		gtk_window_present (GTK_WINDOW (_clist));
	}

	if (current) {
		GtkTreeModel *model;
		GtkTreeIter iter;
		model = gtk_tree_view_get_model (GTK_TREE_VIEW (_clist->priv->treeview));
		if (gtk_tree_model_get_iter_first (model, &iter)) {
			do {
				BrowserConnection *bcnc;
				gtk_tree_model_get (model, &iter, COLUMN_BCNC, &bcnc, -1);
				g_object_unref (bcnc);
				if (bcnc == current) {
					GtkTreeSelection *select;
					select = gtk_tree_view_get_selection (GTK_TREE_VIEW (_clist->priv->treeview));
					gtk_tree_selection_select_iter (select, &iter);
					break;
				}
			} while (gtk_tree_model_iter_next (model, &iter));
		}
	}
	else {
		/* select the 1st available */
		GtkTreeModel *model;
		GtkTreeIter iter;
		model = gtk_tree_view_get_model (GTK_TREE_VIEW (_clist->priv->treeview));
		if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (model), &iter)) {
			GtkTreeSelection *select;
                        select = gtk_tree_view_get_selection (GTK_TREE_VIEW (_clist->priv->treeview));	
			gtk_tree_selection_select_iter (select, &iter);
		}
	}
}

static void
connection_added_cb (G_GNUC_UNUSED BrowserCore *bcore, BrowserConnection *bcnc, BrowserConnectionsList *clist)
{
	GtkListStore *store;
	GtkTreeIter iter;

	store = GTK_LIST_STORE (gtk_tree_view_get_model (clist->priv->treeview));
	gtk_list_store_append (store, &iter);
	gtk_list_store_set (store, &iter,
			    COLUMN_BCNC, bcnc, -1);
}

static void
connection_removed_cb (G_GNUC_UNUSED BrowserCore *bcore, BrowserConnection *bcnc, BrowserConnectionsList *clist)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	GtkTreeSelection *select;
	gboolean is_selected_item = FALSE;

	select = gtk_tree_view_get_selection (clist->priv->treeview);
	model = gtk_tree_view_get_model (clist->priv->treeview);
	g_assert (gtk_tree_model_get_iter_first (model, &iter));

	do {
		BrowserConnection *mbcnc;
		gtk_tree_model_get (model, &iter, COLUMN_BCNC, &mbcnc, -1);
		g_object_unref (mbcnc);
		if (mbcnc == bcnc) {
			is_selected_item = gtk_tree_selection_iter_is_selected (select, &iter);
			gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
			break;
		}
	} while (gtk_tree_model_iter_next (model, &iter));
	
	/* select the 1st available */
	if (is_selected_item && gtk_tree_model_get_iter_first (model, &iter))
		gtk_tree_selection_select_iter (select, &iter);
}
