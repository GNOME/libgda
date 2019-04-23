/*
 * Copyright (C) 2011 Murray Cumming <murrayc@murrayc.com>
 * Copyright (C) 2011 - 2014 Vivien Malerba <malerba@gnome-db.org>
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
#include "hierarchy-view.h"
#include "../dnd.h"
#include "../ui-support.h"
#include "../gdaui-bar.h"
#include <providers/ldap/gda-ldap-connection.h>
#include "mgr-ldap-entries.h"
#include <libgda-ui/gdaui-tree-store.h>
#include <libgda/gda-debug-macros.h>

struct _HierarchyViewPrivate {
	TConnection *tcnc;

	GdaTree           *ldap_tree;
	GdauiTreeStore    *ldap_store;

	gchar             *current_dn;
	gchar             *current_cn;

	GArray            *to_show;
};

static void hierarchy_view_class_init (HierarchyViewClass *klass);
static void hierarchy_view_init       (HierarchyView *eview, HierarchyViewClass *klass);
static void hierarchy_view_dispose   (GObject *object);

static GObjectClass *parent_class = NULL;


/*
 * HierarchyView class implementation
 */

static void
hierarchy_view_class_init (HierarchyViewClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->dispose = hierarchy_view_dispose;
}

static void
hierarchy_view_init (HierarchyView *eview, G_GNUC_UNUSED HierarchyViewClass *klass)
{
	eview->priv = g_new0 (HierarchyViewPrivate, 1);
	eview->priv->current_dn = NULL;
	eview->priv->current_cn = NULL;
}

static void
hierarchy_view_dispose (GObject *object)
{
	HierarchyView *eview = (HierarchyView *) object;

	/* free memory */
	if (eview->priv) {
		if (eview->priv->tcnc)
			g_object_unref (eview->priv->tcnc);
		if (eview->priv->ldap_tree)
			g_object_unref (eview->priv->ldap_tree);
		if (eview->priv->to_show) {
			guint i;
			for (i = 0; i < eview->priv->to_show->len; i++) {
				gchar *tmp;
				tmp = (gchar*) g_array_index (eview->priv->to_show, gchar*, i);
				g_free (tmp);
			}
			g_array_free (eview->priv->to_show, TRUE);
		}

		g_free (eview->priv);
		eview->priv = NULL;
	}

	parent_class->dispose (object);
}

GType
hierarchy_view_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo info = {
			sizeof (HierarchyViewClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) hierarchy_view_class_init,
			NULL,
			NULL,
			sizeof (HierarchyView),
			0,
			(GInstanceInitFunc) hierarchy_view_init,
			0
		};

		type = g_type_register_static (GTK_TYPE_TREE_VIEW, "HierarchyView", &info, 0);
	}
	return type;
}

static gchar *
hierarchy_view_to_selection (G_GNUC_UNUSED HierarchyView *eview)
{
	/*
	  GString *string;
	  gchar *tmp;
	  string = g_string_new ("OBJ_TYPE=hierarchy");
	  tmp = gda_rfc1738_encode (eview->priv->schema);
	  g_string_append_printf (string, ";OBJ_SCHEMA=%s", tmp);
	  g_free (tmp);
	  tmp = gda_rfc1738_encode (eview->priv->hierarchy_name);
	  g_string_append_printf (string, ";OBJ_NAME=%s", tmp);
	  g_free (tmp);
	  tmp = gda_rfc1738_encode (eview->priv->hierarchy_short_name);
	  g_string_append_printf (string, ";OBJ_SHORT_NAME=%s", tmp);
	  g_free (tmp);
	  return g_string_free (string, FALSE);
	*/
	if (eview->priv->current_dn)
		return g_strdup (eview->priv->current_dn);
	else
		return NULL;
}

static void
source_drag_data_get_cb (G_GNUC_UNUSED GtkWidget *widget, G_GNUC_UNUSED GdkDragContext *context,
			 GtkSelectionData *selection_data,
			 guint view, G_GNUC_UNUSED guint time, HierarchyView *eview)
{
	switch (view) {
	case TARGET_KEY_VALUE: {
		gchar *str;
		str = hierarchy_view_to_selection (eview);
		gtk_selection_data_set (selection_data,
					gtk_selection_data_get_target (selection_data), 8, (guchar*) str,
					strlen (str));
		g_free (str);
		break;
	}
	default:
	case TARGET_PLAIN: {
		gtk_selection_data_set_text (selection_data, hierarchy_view_get_current_dn (eview, NULL), -1);
		break;
	}
	case TARGET_ROOTWIN:
		TO_IMPLEMENT; /* dropping on the Root Window => create a file */
		break;
	}
}

static gboolean test_expand_row_cb (GtkTreeView *tree_view, GtkTreeIter *iter, GtkTreePath *path, HierarchyView *eview);
static void selection_changed_cb (GtkTreeSelection *sel, HierarchyView *eview);
static gboolean make_dn_hierarchy (const gchar *basedn, const gchar *dn, GArray *in);
static void go_to_row (HierarchyView *eview, GtkTreePath *path);

enum
{
        COLUMN_RDN,
	COLUMN_ICON,
	COLUMN_DN,
	COLUMN_CN,
        NUM_COLUMNS
};

static void
text_cell_data_func (G_GNUC_UNUSED GtkTreeViewColumn *tree_column, GtkCellRenderer *cell,
		     GtkTreeModel *tree_model, GtkTreeIter *iter, G_GNUC_UNUSED gpointer data)
{
	gchar *rdn, *cn;
	gtk_tree_model_get (tree_model, iter,
                            COLUMN_RDN, &rdn, COLUMN_CN, &cn, -1);
	g_object_set ((GObject*) cell, "text", cn && *cn ? cn : rdn, NULL);
        g_free (cn);
        g_free (rdn);
}

/**
 * hierarchy_view_new
 *
 * Returns: a new #GtkWidget
 */
GtkWidget *
hierarchy_view_new (TConnection *tcnc, const gchar *dn)
{
	HierarchyView *eview;

	g_return_val_if_fail (T_IS_CONNECTION (tcnc), NULL);

	eview = HIERARCHY_VIEW (g_object_new (HIERARCHY_VIEW_TYPE, NULL));
	eview->priv->tcnc = T_CONNECTION (g_object_ref ((GObject*) tcnc));
	g_signal_connect (eview, "drag-data-get",
			  G_CALLBACK (source_drag_data_get_cb), eview);

	GdaTreeManager *mgr;
	GtkTreeModel *store;
	GtkCellRenderer *renderer;
        GtkTreeViewColumn *column;
	eview->priv->ldap_tree = gda_tree_new ();
	mgr = mgr_ldap_entries_new (eview->priv->tcnc, NULL);
	gda_tree_add_manager (eview->priv->ldap_tree, mgr);
	gda_tree_manager_add_manager (mgr, mgr);
	gda_tree_update_children (eview->priv->ldap_tree, NULL, NULL);
	g_object_unref (mgr);

	store = gdaui_tree_store_new (eview->priv->ldap_tree, NUM_COLUMNS,
				      G_TYPE_STRING, "rdn",
				      G_TYPE_OBJECT, "icon",
				      G_TYPE_STRING, "dn",
				      G_TYPE_STRING, "cn");
	gtk_tree_view_set_model (GTK_TREE_VIEW (eview), GTK_TREE_MODEL (store));

	eview->priv->ldap_store = GDAUI_TREE_STORE (store);
	g_object_unref (G_OBJECT (store));
	g_signal_connect (eview, "test-expand-row",
			  G_CALLBACK (test_expand_row_cb), eview);

	column = gtk_tree_view_column_new ();

	renderer = gtk_cell_renderer_pixbuf_new ();
        gtk_tree_view_column_pack_start (column, renderer, FALSE);
        gtk_tree_view_column_add_attribute (column, renderer, "pixbuf", COLUMN_ICON);
        g_object_set ((GObject*) renderer, "yalign", 0., NULL);

	renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_column_pack_start (column, renderer, TRUE);
	gtk_tree_view_column_set_cell_data_func (column, renderer,
						 (GtkTreeCellDataFunc) text_cell_data_func,
						 NULL, NULL);
        //gtk_tree_view_column_add_attribute (column, renderer, "text", COLUMN_RDN);

        gtk_tree_view_append_column (GTK_TREE_VIEW (eview), column);
	gtk_tree_view_set_expander_column (GTK_TREE_VIEW (eview), column);
	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (eview), FALSE);

	/* tree selection */
	GtkTreeSelection *sel;
	sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (eview));
	gtk_tree_selection_set_mode (sel, GTK_SELECTION_SINGLE);
	g_signal_connect (sel, "changed",
			  G_CALLBACK (selection_changed_cb), eview);

	if (dn) {
		const gchar *basedn;
		GArray *array;

		basedn = t_connection_ldap_get_base_dn (eview->priv->tcnc);
		array = g_array_new (TRUE, FALSE, sizeof (gchar*));
		make_dn_hierarchy (basedn, dn, array);
		if (array->len > 0) {
			eview->priv->to_show = array;
			go_to_row (eview, NULL);
		}
		else
			g_array_free (array, TRUE);
	}

	return (GtkWidget*) eview;
}


static gboolean
make_dn_hierarchy (const gchar *basedn, const gchar *dn, GArray *in)
{
	if (basedn && !strcmp (basedn, dn))
		return TRUE;

	gchar **split;
	gchar *tmp;
	tmp = g_strdup (dn);
	g_array_prepend_val (in, tmp);
	split = gda_ldap_dn_split (dn, FALSE);
	if (split) {
		gboolean retval = TRUE;
		if (split [0] && split [1])
			retval = make_dn_hierarchy (basedn, split [1], in);
		g_strfreev (split);
		return retval;
	}
	else
		return FALSE;
}

static void
selection_changed_cb (GtkTreeSelection *sel, HierarchyView *eview)
{
	GtkTreeIter iter;
	GtkTreeModel *model;
	if (gtk_tree_selection_get_selected (sel, &model, &iter)) {
		GdaTreeNode *node;
		const GValue *cvalue;
		node = gdaui_tree_store_get_node (GDAUI_TREE_STORE (model), &iter);
		g_assert (node);
		cvalue = gda_tree_node_get_node_attribute (node, "dn");
		g_assert (cvalue);

		g_free (eview->priv->current_dn);
		eview->priv->current_dn = g_value_dup_string (cvalue);
		g_free (eview->priv->current_cn);
		eview->priv->current_cn = NULL;
		cvalue = gda_tree_node_get_node_attribute (node, "cn");
		if (cvalue)
			eview->priv->current_cn = g_value_dup_string (cvalue);
	}
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
		    G_GNUC_UNUSED GtkTreePath *path, HierarchyView *eview)
{
	GdaTreeNode *node;
	GtkTreeModel *store;
#ifdef DEBUG_GOTO
	g_print ("%s (path => %s)\n", __FUNCTION__, gtk_tree_path_to_string (path));
#endif
	store = gtk_tree_view_get_model (tree_view);
        node = gdaui_tree_store_get_node (GDAUI_TREE_STORE (store), iter);
        if (!node || gda_tree_node_get_child_index (node, 0))
                return FALSE;

	const GValue *cv;
	cv = gda_tree_node_get_node_attribute (node,
					       GDA_ATTRIBUTE_TREE_NODE_UNKNOWN_CHILDREN);
	if (cv && (G_VALUE_TYPE (cv) == G_TYPE_BOOLEAN) &&
	    g_value_get_boolean (cv)) {
		IdleData *data;
		data = g_new (IdleData, 1);
		data->tview = GTK_TREE_VIEW (g_object_ref (G_OBJECT (tree_view)));
		data->store = GDAUI_TREE_STORE (g_object_ref (G_OBJECT (store)));
		data->tree = GDA_TREE (g_object_ref (G_OBJECT (eview->priv->ldap_tree)));
		data->node = GDA_TREE_NODE (g_object_ref (G_OBJECT (node)));

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
	if (gdaui_tree_store_get_iter_from_node (data->store, &iter, data->node)) {
		if (gda_tree_node_get_child_index (data->node, 0)) {
			/* actually expand the row */
			GtkTreePath *path;
			path = gtk_tree_model_get_path (GTK_TREE_MODEL (data->store), &iter);
			g_signal_handlers_block_by_func (data->tview,
							 G_CALLBACK (test_expand_row_cb), data->tree);
			gtk_tree_view_expand_row (data->tview, path, FALSE);
			gtk_tree_view_scroll_to_cell (data->tview,
						      path, NULL, TRUE, 0.5, 0.5);
			g_signal_handlers_unblock_by_func (data->tview,
							   G_CALLBACK (test_expand_row_cb), data->tree);
			gtk_tree_path_free (path);
		}
		if (HIERARCHY_VIEW (data->tview)->priv->to_show) {
			GtkTreePath *path;
			path = gtk_tree_model_get_path (GTK_TREE_MODEL (data->store), &iter);
			go_to_row (HIERARCHY_VIEW (data->tview), path);
			gtk_tree_path_free (path);
		}
	}

	return FALSE; /* => remove IDLE call */
}

static void
go_to_row (HierarchyView *eview, GtkTreePath *path)
{
	GtkTreePath *lpath = NULL;
	gchar *tofind = NULL;
	if (path)
		lpath = gtk_tree_path_copy (path);
#ifdef DEBUG_GOTO
	g_print ("%s (starting from path=>%s)\n", __FUNCTION__, path ? gtk_tree_path_to_string (path) : "null");
#endif
	while (1) {
		if (!eview->priv->to_show)
			goto out;
		if (eview->priv->to_show->len == 0) {
			g_array_free (eview->priv->to_show, TRUE);
			eview->priv->to_show = NULL;
			goto out;
		}

		g_free (tofind);
		tofind = g_array_index (eview->priv->to_show, gchar*, 0);
		g_array_remove_index (eview->priv->to_show, 0);
		
#ifdef DEBUG_GOTO
		g_print ("tofind: [%s] starting from [%s]\n", tofind, lpath ? gtk_tree_path_to_string (lpath) : "null");
#endif
		GtkTreeModel *model;
		GtkTreeIter iter, parent;
		gboolean found = FALSE;
		model = GTK_TREE_MODEL (eview->priv->ldap_store);
		if ((lpath && gtk_tree_model_get_iter (model, &parent, lpath) &&
		     gtk_tree_model_iter_children (model, &iter, &parent)) ||
		    (!lpath && gtk_tree_model_get_iter_first (model, &iter))) {
			gboolean init = TRUE;
			for (; init || (!init && gtk_tree_model_iter_next (model, &iter)); init = FALSE) {
				gchar *dn;
				gtk_tree_model_get (model, &iter, COLUMN_DN, &dn, -1);
				if (dn) {
					if (!strcmp (dn, tofind)) {
						found = TRUE;
						g_free (dn);
						break;
					}
					g_free (dn);
				}
			}
		}
		if (!found) {
			/* perform the same search but case insensitive */
			if ((lpath && gtk_tree_model_get_iter (model, &parent, lpath) &&
			     gtk_tree_model_iter_children (model, &iter, &parent)) ||
			    (!lpath && gtk_tree_model_get_iter_first (model, &iter))) {
				gboolean init = TRUE;
				for (; init || (!init && gtk_tree_model_iter_next (model, &iter)); init = FALSE) {
					gchar *dn;
					gtk_tree_model_get (model, &iter, COLUMN_DN, &dn, -1);
					if (dn) {
						if (!g_ascii_strcasecmp (dn, tofind)) {
							found = TRUE;
							break;
						}
						g_free (dn);
					}
				}
			}
		}
		if (lpath) {
			gtk_tree_path_free (lpath);
			lpath = NULL;
		}

		if (found) {
			/* iter is pointing to the requested DN */
			lpath = gtk_tree_model_get_path (model, &iter);
			if (eview->priv->to_show->len == 0) {
				/* no more after => set selection */
				GtkTreeSelection *sel;
				sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (eview));
				gtk_tree_selection_select_path (sel, lpath);
				gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW (eview),
							      lpath, NULL, TRUE, 0.5, 0.5);
#ifdef DEBUG_GOTO
				g_print ("SELECTED %s\n", gtk_tree_path_to_string (lpath));
#endif
				goto out;
			}
			else {
				/* more to find */
				if (! gtk_tree_view_row_expanded (GTK_TREE_VIEW (eview), lpath)) {
					GdaTreeNode *node;
					node = gdaui_tree_store_get_node (eview->priv->ldap_store, &iter);
					if (node && gda_tree_node_get_child_index (node, 0))
						gtk_tree_view_expand_row (GTK_TREE_VIEW (eview), lpath, FALSE);
					else {

#ifdef DEBUG_GOTO
						g_print (">>REQUEST ROW EXP for path %s\n", gtk_tree_path_to_string (lpath));
#endif
						gtk_tree_view_expand_row (GTK_TREE_VIEW (eview), lpath, FALSE);
						
#ifdef DEBUG_GOTO
						g_print ("<<REQUEST ROW EXP for path %s\n", gtk_tree_path_to_string (lpath));
#endif
						goto out;
					}
				}
			}
		}
		else {
			/* DN not found! */
			ui_show_message (GTK_WINDOW (gtk_widget_get_toplevel ((GtkWidget*) eview)),
					      _("Could not find LDAP entry with DN '%s'"), tofind);
			goto out;
		}
	}
 out:
	g_free (tofind);
	if (lpath)
		gtk_tree_path_free (lpath);
}


/**
 * hierarchy_view_get_current_dn:
 */
const gchar *
hierarchy_view_get_current_dn (HierarchyView *eview, const gchar **out_current_cn)
{
	g_return_val_if_fail (IS_HIERARCHY_VIEW (eview), NULL);
	if (out_current_cn)
		*out_current_cn = eview->priv->current_cn;
	return eview->priv->current_dn;
}

/**
 * hierarchy_view_set_current_dn:
 */
void
hierarchy_view_set_current_dn (HierarchyView *hierarchy_view, const gchar *dn)
{
	const gchar *basedn;
	GArray *array;

	g_return_if_fail (IS_HIERARCHY_VIEW (hierarchy_view));
	g_return_if_fail (dn && *dn);

	if (hierarchy_view->priv->to_show) {
		guint i;
		for (i = 0; i < hierarchy_view->priv->to_show->len; i++) {
			gchar *tmp;
			tmp = (gchar*) g_array_index (hierarchy_view->priv->to_show, gchar*, i);
			g_free (tmp);
		}
		g_array_free (hierarchy_view->priv->to_show, TRUE);
		hierarchy_view->priv->to_show = NULL;
	}

	basedn = t_connection_ldap_get_base_dn (hierarchy_view->priv->tcnc);
	array = g_array_new (TRUE, FALSE, sizeof (gchar*));
	make_dn_hierarchy (basedn, dn, array);
	if (array->len > 0) {
		hierarchy_view->priv->to_show = array;
		go_to_row (hierarchy_view, NULL);
	}
	else
		g_array_free (array, TRUE);
}
