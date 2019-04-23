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
#include "classes-view.h"
#include "../dnd.h"
#include "../ui-support.h"
#include "../gdaui-bar.h"
#include <providers/ldap/gda-ldap-connection.h>
#include "mgr-ldap-classes.h"
#include <libgda-ui/gdaui-tree-store.h>
#include <libgda/gda-debug-macros.h>

struct _ClassesViewPrivate {
	TConnection *tcnc;

	GdaTree           *classes_tree;
	GdauiTreeStore    *classes_store;

	gchar             *current_class;
};

static void classes_view_class_init (ClassesViewClass *klass);
static void classes_view_init       (ClassesView *eview, ClassesViewClass *klass);
static void classes_view_dispose   (GObject *object);

static GObjectClass *parent_class = NULL;


/*
 * ClassesView class implementation
 */

static void
classes_view_class_init (ClassesViewClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->dispose = classes_view_dispose;
}

static void
classes_view_init (ClassesView *eview, G_GNUC_UNUSED ClassesViewClass *klass)
{
	eview->priv = g_new0 (ClassesViewPrivate, 1);
	eview->priv->current_class = NULL;
}

static void
classes_view_dispose (GObject *object)
{
	ClassesView *eview = (ClassesView *) object;

	/* free memory */
	if (eview->priv) {
		if (eview->priv->tcnc)
			g_object_unref (eview->priv->tcnc);
		if (eview->priv->classes_tree)
			g_object_unref (eview->priv->classes_tree);

		g_free (eview->priv);
		eview->priv = NULL;
	}

	parent_class->dispose (object);
}

GType
classes_view_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo info = {
			sizeof (ClassesViewClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) classes_view_class_init,
			NULL,
			NULL,
			sizeof (ClassesView),
			0,
			(GInstanceInitFunc) classes_view_init,
			0
		};

		type = g_type_register_static (GTK_TYPE_TREE_VIEW, "ClassesView", &info, 0);
	}
	return type;
}

static gchar *
classes_view_to_selection (G_GNUC_UNUSED ClassesView *eview)
{
	/*
	  GString *string;
	  gchar *tmp;
	  string = g_string_new ("OBJ_TYPE=classes");
	  tmp = gda_rfc1738_encode (eview->priv->schema);
	  g_string_append_printf (string, ";OBJ_SCHEMA=%s", tmp);
	  g_free (tmp);
	  tmp = gda_rfc1738_encode (eview->priv->classes_name);
	  g_string_append_printf (string, ";OBJ_NAME=%s", tmp);
	  g_free (tmp);
	  tmp = gda_rfc1738_encode (eview->priv->classes_short_name);
	  g_string_append_printf (string, ";OBJ_SHORT_NAME=%s", tmp);
	  g_free (tmp);
	  return g_string_free (string, FALSE);
	*/
	if (eview->priv->current_class)
		return g_strdup (eview->priv->current_class);
	else
		return NULL;
}

static void
source_drag_data_get_cb (G_GNUC_UNUSED GtkWidget *widget, G_GNUC_UNUSED GdkDragContext *context,
			 GtkSelectionData *selection_data,
			 guint view, G_GNUC_UNUSED guint time, ClassesView *eview)
{
	switch (view) {
	case TARGET_KEY_VALUE: {
		gchar *str;
		str = classes_view_to_selection (eview);
		gtk_selection_data_set (selection_data,
					gtk_selection_data_get_target (selection_data), 8, (guchar*) str,
					strlen (str));
		g_free (str);
		break;
	}
	default:
	case TARGET_PLAIN: {
		gtk_selection_data_set_text (selection_data, classes_view_get_current_class (eview), -1);
		break;
	}
	case TARGET_ROOTWIN:
		TO_IMPLEMENT; /* dropping on the Root Window => create a file */
		break;
	}
}

static void selection_changed_cb (GtkTreeSelection *sel, ClassesView *eview);

enum {
	COLUMN_CLASS,
	COLUMN_ICON,
	COLUMN_NAME,
	NUM_COLUMNS
};

static void
text_cell_data_func (G_GNUC_UNUSED GtkTreeViewColumn *tree_column, GtkCellRenderer *cell,
		     GtkTreeModel *tree_model, GtkTreeIter *iter, G_GNUC_UNUSED gpointer data)
{
	gchar *tmp;
	gtk_tree_model_get (tree_model, iter,
                            COLUMN_CLASS, &tmp, -1);
	if (tmp) {
		g_object_set ((GObject*) cell, "text", tmp,
			      "weight-set", FALSE,
			      "background-set", FALSE,
			      NULL);
		g_free (tmp);
	}
	else {
		gtk_tree_model_get (tree_model, iter,
				    COLUMN_NAME, &tmp, -1);
		g_object_set ((GObject*) cell, "text", tmp,
			      "weight", PANGO_WEIGHT_BOLD,
			      "background", "grey", NULL);
		g_free (tmp);
	}
}

/**
 * classes_view_new
 *
 * Returns: a new #GtkWidget
 */
GtkWidget *
classes_view_new (TConnection *tcnc, const gchar *classname)
{
	ClassesView *eview;

	g_return_val_if_fail (T_IS_CONNECTION (tcnc), NULL);

	eview = CLASSES_VIEW (g_object_new (CLASSES_VIEW_TYPE, NULL));
	eview->priv->tcnc = T_CONNECTION (g_object_ref ((GObject*) tcnc));
	g_signal_connect (eview, "drag-data-get",
			  G_CALLBACK (source_drag_data_get_cb), eview);

	GdaTreeManager *mgr;
	GtkTreeModel *store;
	GtkCellRenderer *renderer;
        GtkTreeViewColumn *column;
	eview->priv->classes_tree = gda_tree_new ();
	mgr = mgr_ldap_classes_new (eview->priv->tcnc, FALSE, NULL);
	gda_tree_add_manager (eview->priv->classes_tree, mgr);
	gda_tree_manager_add_manager (mgr, mgr);
	gda_tree_update_all (eview->priv->classes_tree, NULL);
	g_object_unref (mgr);

	store = gdaui_tree_store_new (eview->priv->classes_tree, NUM_COLUMNS,
				      G_TYPE_STRING, "class",
				      G_TYPE_OBJECT, "icon",
				      G_TYPE_STRING, GDA_ATTRIBUTE_NAME);
	gtk_tree_view_set_model (GTK_TREE_VIEW (eview), GTK_TREE_MODEL (store));

	eview->priv->classes_store = GDAUI_TREE_STORE (store);
	g_object_unref (G_OBJECT (store));

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

        gtk_tree_view_append_column (GTK_TREE_VIEW (eview), column);
	gtk_tree_view_set_expander_column (GTK_TREE_VIEW (eview), column);
	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (eview), FALSE);

	/* tree selection */
	GtkTreeSelection *sel;
	sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (eview));
	gtk_tree_selection_set_mode (sel, GTK_SELECTION_SINGLE);
	g_signal_connect (sel, "changed",
			  G_CALLBACK (selection_changed_cb), eview);

	if (classname)
		classes_view_set_current_class (eview, classname);

	return (GtkWidget*) eview;
}

static void
selection_changed_cb (GtkTreeSelection *sel, ClassesView *eview)
{
	GtkTreeIter iter;
	GtkTreeModel *model;
	if (gtk_tree_selection_get_selected (sel, &model, &iter)) {
		GdaTreeNode *node;
		const GValue *cvalue;
		node = gdaui_tree_store_get_node (GDAUI_TREE_STORE (model), &iter);
		g_assert (node);
		cvalue = gda_tree_node_get_node_attribute (node, "class");
		g_free (eview->priv->current_class);
		if (cvalue)
			eview->priv->current_class = g_value_dup_string (cvalue);
		else
			eview->priv->current_class = NULL;
	}
}

/**
 * classes_view_get_current_class:
 */
const gchar *
classes_view_get_current_class (ClassesView *eview)
{
	g_return_val_if_fail (IS_CLASSES_VIEW (eview), NULL);
	return eview->priv->current_class;
}

static GtkTreePath *
search_for_class (GtkTreeModel *model, const gchar *classname, GtkTreeIter *iter)
{
#ifdef GDA_DEBUG_NO
	GtkTreePath *debug;
	if (iter) {
		debug = gtk_tree_model_get_path (model, iter);
		g_print ("%s (%s)\n", __FUNCTION__, gtk_tree_path_to_string (debug));
		gtk_tree_path_free (debug);
	}
	else
		g_print ("%s (TOP)\n", __FUNCTION__);
#endif

	if (iter) {
		/* look in the node itself */
		gchar *cln = NULL;
		gtk_tree_model_get (model, iter, COLUMN_CLASS, &cln, -1);
		if (cln && !strcmp (cln, classname)) {
			g_free (cln);
			return gtk_tree_model_get_path (model, iter);
		}
		g_free (cln);
	}

	/* look at the children */
	GtkTreeIter chiter;
	if (gtk_tree_model_iter_children (model, &chiter, iter)) {
		GtkTreePath *path;
		path = search_for_class (model, classname, &chiter);
		if (path)
			return path;

		for (; gtk_tree_model_iter_next (model, &chiter); ) {
			GtkTreePath *path;
			path = search_for_class (model, classname, &chiter);
			if (path)
				return path;
		}
	}

	return NULL;
}

/**
 * classes_view_set_current_class:
 */
void
classes_view_set_current_class (ClassesView *eview, const gchar *classname)
{
	GtkTreePath *path = NULL;
	g_return_if_fail (IS_CLASSES_VIEW (eview));
	g_return_if_fail (classname && *classname);

	path = search_for_class (GTK_TREE_MODEL (eview->priv->classes_store), classname, NULL);
	if (path) {
		GtkTreeSelection *sel;

#ifdef GDA_DEBUG_NO
		g_print ("Found class [%s] at %s\n", classname, gtk_tree_path_to_string (path));
#endif
		gtk_tree_view_expand_to_path (GTK_TREE_VIEW (eview), path);
		gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW (eview), path, NULL, TRUE, .5, 0.);

		sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (eview));
		gtk_tree_selection_select_path (sel, path);
		gtk_tree_path_free (path);
	}
}
