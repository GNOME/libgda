/*
 * Copyright (C) 2009 - 2013 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2010 David King <davidk@openismus.com>
 * Copyright (C) 2018 Daniel Espinosa <esodan@gmail.com>
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

#include "gdaui-data-store.h"
#include <string.h>
#include <libgda/gda-data-proxy.h>
#include <glib/gi18n-lib.h>
#include <libgda/gda-debug-macros.h>

static void gdaui_data_store_class_init (GdauiDataStoreClass *class);
static void gdaui_data_store_init (GdauiDataStore *store);
static void gdaui_data_store_dispose (GObject *object);

static void gdaui_data_store_set_property (GObject *object,
					   guint param_id,
					   const GValue *value,
					   GParamSpec *pspec);
static void gdaui_data_store_get_property (GObject *object,
					   guint param_id,
					   GValue *value,
					   GParamSpec *pspec);

/* properties */
enum {
	PROP_0,
	PROP_MODEL,
	PROP_PROXY,
	PROP_ADD_NULL_ENTRY
};

typedef struct {
	GdaDataProxy *proxy;
	gint          nrows;
	gint          stamp; /* Random integer to check whether an iter belongs to our model */
	gboolean      resetting; /* TRUE when GdauiDataStore is handling proxy reset callback */
} GdauiDataStorePrivate;

/*
 * GtkTreeModel interface
 */
static void              data_store_tree_model_init (GtkTreeModelIface *iface);
static GtkTreeModelFlags data_store_get_flags       (GtkTreeModel *tree_model);
static gint              data_store_get_n_columns   (GtkTreeModel *tree_model);
static GType             data_store_get_column_type (GtkTreeModel *tree_model, gint index);
static gboolean          data_store_get_iter        (GtkTreeModel *tree_model, GtkTreeIter *iter, GtkTreePath *path);
static GtkTreePath      *data_store_get_path        (GtkTreeModel *tree_model, GtkTreeIter *iter);
static void              data_store_get_value       (GtkTreeModel *tree_model, GtkTreeIter *iter, gint column, GValue *value);
static gboolean          data_store_iter_next       (GtkTreeModel *tree_model, GtkTreeIter *iter);
static gboolean          data_store_iter_children   (GtkTreeModel *tree_model, GtkTreeIter *iter, GtkTreeIter *parent);
static gboolean          data_store_iter_has_child  (GtkTreeModel *tree_model, GtkTreeIter *iter);
static gint              data_store_iter_n_children (GtkTreeModel *tree_model, GtkTreeIter *iter);
static gboolean          data_store_iter_nth_child  (GtkTreeModel *tree_model, GtkTreeIter *iter, GtkTreeIter *parent, gint n);
static gboolean          data_store_iter_parent     (GtkTreeModel *tree_model, GtkTreeIter *iter, GtkTreeIter *child);

/* get a pointer to the parents to be able to call their destructor */

G_DEFINE_TYPE_WITH_CODE(GdauiDataStore, gdaui_data_store, G_TYPE_OBJECT,
                        G_ADD_PRIVATE (GdauiDataStore)
                        G_IMPLEMENT_INTERFACE(GTK_TYPE_TREE_MODEL, data_store_tree_model_init))

static void
data_store_tree_model_init (GtkTreeModelIface *iface)
{
        iface->get_flags       = data_store_get_flags;
        iface->get_n_columns   = data_store_get_n_columns;
        iface->get_column_type = data_store_get_column_type;
        iface->get_iter        = data_store_get_iter;
        iface->get_path        = data_store_get_path;
        iface->get_value       = data_store_get_value;
        iface->iter_next       = data_store_iter_next;
        iface->iter_children   = data_store_iter_children;
        iface->iter_has_child  = data_store_iter_has_child;
        iface->iter_n_children = data_store_iter_n_children;
        iface->iter_nth_child  = data_store_iter_nth_child;
        iface->iter_parent     = data_store_iter_parent;
}


static void
gdaui_data_store_class_init (GdauiDataStoreClass * class)
{
	GObjectClass   *object_class = G_OBJECT_CLASS (class);

	object_class->dispose = gdaui_data_store_dispose;

	/* Properties */
        object_class->set_property = gdaui_data_store_set_property;
        object_class->get_property = gdaui_data_store_get_property;

        g_object_class_install_property (object_class, PROP_MODEL,
                                         g_param_spec_pointer ("model", _("Data model"), NULL,
                                                               (G_PARAM_READABLE | G_PARAM_WRITABLE |
                                                                G_PARAM_CONSTRUCT_ONLY)));
        g_object_class_install_property (object_class, PROP_PROXY,
                                         g_param_spec_pointer ("proxy", _("Internal GdaDataProxy data model"), NULL,
                                                               (G_PARAM_READABLE)));
        g_object_class_install_property (object_class, PROP_ADD_NULL_ENTRY,
                                         g_param_spec_boolean ("prepend-null-entry", NULL, NULL, FALSE,
                                                               (G_PARAM_READABLE | G_PARAM_WRITABLE)));
}

static void
gdaui_data_store_init (GdauiDataStore *store)
{
	/* Private structure */
	GdauiDataStorePrivate *priv = gdaui_data_store_get_instance_private (store);
	priv->proxy = NULL;
	priv->nrows = 0;
	priv->stamp = g_random_int ();
	priv->resetting = FALSE;
}

static void
row_inserted_cb (G_GNUC_UNUSED GdaDataProxy *proxy, gint row, GtkTreeModel *model)
{
	GtkTreePath *path;
	GtkTreeIter iter;
	GdauiDataStore *store = (GdauiDataStore*) model;
	GdauiDataStorePrivate *priv = gdaui_data_store_get_instance_private (store);


	priv->nrows ++;
	priv->stamp = g_random_int ();
	path = gtk_tree_path_new ();
	gtk_tree_path_append_index (path, row);
	if (gtk_tree_model_get_iter (model, &iter, path))
		gtk_tree_model_row_inserted (model, path, &iter);
	gtk_tree_path_free (path);
}

static void
row_updated_cb (G_GNUC_UNUSED GdaDataProxy *proxy, gint row, GtkTreeModel *model)
{
	GtkTreePath *path;
	GtkTreeIter iter;
	GdauiDataStore *store = (GdauiDataStore*) model;
	GdauiDataStorePrivate *priv = gdaui_data_store_get_instance_private (store);

	priv->stamp = g_random_int ();
	path = gtk_tree_path_new ();
	gtk_tree_path_append_index (path, row);
	if (gtk_tree_model_get_iter (model, &iter, path))
		gtk_tree_model_row_changed (model, path, &iter);
	gtk_tree_path_free (path);
}

static void
row_removed_cb (G_GNUC_UNUSED GdaDataProxy *proxy, gint row, GtkTreeModel *model)
{
	GtkTreePath *path;
	GdauiDataStore *store = (GdauiDataStore*) model;
	GdauiDataStorePrivate *priv = gdaui_data_store_get_instance_private (store);

	priv->nrows --;
	priv->stamp = g_random_int ();
	path = gtk_tree_path_new ();
	gtk_tree_path_append_index (path, row);
	gtk_tree_model_row_deleted (model, path);
	gtk_tree_path_free (path);
}

static void
proxy_reset_cb (GdaDataProxy *proxy, GtkTreeModel *model)
{
	gint i, nrows;
	GdauiDataStore *store = (GdauiDataStore*) model;
	GdauiDataStorePrivate *priv = gdaui_data_store_get_instance_private (store);

	/* invalidate any existing iterator */
	priv->resetting = TRUE;
	while (priv->nrows > 0)
		row_removed_cb (proxy, priv->nrows - 1, model);
	priv->resetting = FALSE;

	nrows = gda_data_model_get_n_rows ((GdaDataModel *) proxy);;
	priv->nrows = 0;
	for (i = 0; i < nrows; i++)
		row_inserted_cb (proxy, i, model);
}

static void
gdaui_data_store_dispose (GObject *object)
{
	GdauiDataStore *store;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GDAUI_IS_DATA_STORE (object));

	store = GDAUI_DATA_STORE (object);
	GdauiDataStorePrivate *priv = gdaui_data_store_get_instance_private (store);

	if (priv->proxy) {
		g_signal_handlers_disconnect_by_func (G_OBJECT (priv->proxy),
						      G_CALLBACK (row_inserted_cb), store);
		g_signal_handlers_disconnect_by_func (G_OBJECT (priv->proxy),
						      G_CALLBACK (row_updated_cb), store);
		g_signal_handlers_disconnect_by_func (G_OBJECT (priv->proxy),
						      G_CALLBACK (row_removed_cb), store);
		g_signal_handlers_disconnect_by_func (G_OBJECT (priv->proxy),
						      G_CALLBACK (proxy_reset_cb), store);
		g_object_unref (priv->proxy);
		priv->proxy = NULL;
		priv->nrows = 0;
		priv->stamp = g_random_int ();
	}

	/* for the parent class */
	G_OBJECT_CLASS (gdaui_data_store_parent_class)->dispose (object);
}

static void
gdaui_data_store_set_property (GObject *object,
			       guint param_id,
			       const GValue *value,
			       GParamSpec *pspec)
{
	GdauiDataStore *store;

	store = GDAUI_DATA_STORE (object);
	GdauiDataStorePrivate *priv = gdaui_data_store_get_instance_private (store);
	GdaDataModel *model;
	GdaDataProxy *proxy;

	switch (param_id) {
	case PROP_MODEL:
		g_assert (!priv->proxy);
		model = (GdaDataModel*) g_value_get_pointer (value);
		g_return_if_fail (GDA_IS_DATA_MODEL (model));
		if (GDA_IS_DATA_PROXY (model)) {
			proxy = (GdaDataProxy *) model;
			g_object_ref (model);
		}
		else
			proxy = (GdaDataProxy *) gda_data_proxy_new (model);
		priv->proxy = proxy;
		g_object_set (G_OBJECT (proxy), "defer-sync", FALSE, "sample-size", 0, NULL);
		priv->nrows = gda_data_model_get_n_rows (GDA_DATA_MODEL (priv->proxy));

		/* connect to row changes */
		g_signal_connect (G_OBJECT (proxy), "row-inserted",
				  G_CALLBACK (row_inserted_cb), store);
		g_signal_connect (G_OBJECT (proxy), "row-updated",
				  G_CALLBACK (row_updated_cb), store);
		g_signal_connect (G_OBJECT (proxy), "row-removed",
				  G_CALLBACK (row_removed_cb), store);
		g_signal_connect (G_OBJECT (proxy), "reset",
				  G_CALLBACK (proxy_reset_cb), store);
		priv->stamp = g_random_int ();
		proxy_reset_cb (GDA_DATA_PROXY (proxy), (GtkTreeModel *) store);
		break;
	case PROP_ADD_NULL_ENTRY:
		g_return_if_fail (priv->proxy);
		g_object_set (priv->proxy, "prepend-null-entry",
			      g_value_get_boolean (value), NULL);
		priv->stamp = g_random_int ();
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}
}

static void
gdaui_data_store_get_property (GObject *object,
			       guint param_id,
			       GValue *value,
			       GParamSpec *pspec)
{
	GdauiDataStore *store;

	store = GDAUI_DATA_STORE (object);
	GdauiDataStorePrivate *priv = gdaui_data_store_get_instance_private (store);
	switch (param_id) {
	case PROP_MODEL:
		/* FIXME: the requested model can be different than the proxy as
		   the aim of this property is to retrieve the data model given to
		   the new() function (which is the same as the one given to the PROP_MODEL
		   property) */
              case PROP_PROXY:
		g_value_set_pointer (value, priv->proxy);
		break;
	case PROP_ADD_NULL_ENTRY: {
		gboolean prop;

		g_object_get (priv->proxy, "prepend-null-entry", &prop, NULL);
		g_value_set_boolean (value, prop);
		break;
	}
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}
}

/**
 * gdaui_data_store_new:
 * @model: a #GdaDataModel object
 *
 * Creates a #GtkTreeModel interface with a #GdaDataModel
 *
 * Returns: (transfer full): the new object
 *
 * Since: 4.2
 */
GtkTreeModel *
gdaui_data_store_new (GdaDataModel *model)
{
	GObject *obj;

	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), NULL);
	obj = g_object_new (GDAUI_TYPE_DATA_STORE, "model", model, NULL);

	return (GtkTreeModel *) obj;
}

/**
 * gdaui_data_store_set_value:
 * @store: a #GdauiDataStore object
 * @iter: the considered row
 * @col: the data model column
 * @value: the value to store (gets copied)
 *
 * Stores a value in the @store data model.
 *
 * Returns: %TRUE on success
 *
 * Since: 4.2
 */
gboolean
gdaui_data_store_set_value (GdauiDataStore *store, GtkTreeIter *iter,
			    gint col, const GValue *value)
{
	gint row, model_nb_cols;

        g_return_val_if_fail (GDAUI_IS_DATA_STORE (store), FALSE);
        GdauiDataStorePrivate *priv = gdaui_data_store_get_instance_private (store);
        g_return_val_if_fail (priv, FALSE);
        g_return_val_if_fail (priv->proxy, FALSE);
        g_return_val_if_fail (iter, FALSE);
        g_return_val_if_fail (iter->stamp == priv->stamp, FALSE);

	if (priv->resetting) {
		g_warning (_("Can't modify row while data model is being reset"));
		return FALSE;
	}

	model_nb_cols = gda_data_proxy_get_proxied_model_n_cols (priv->proxy);
        row = GPOINTER_TO_INT (iter->user_data);

	/* Global attributes */
	if (col < 0) {
		switch (col) {
		case GDAUI_DATA_STORE_COL_MODEL_N_COLUMNS:
		case GDAUI_DATA_STORE_COL_MODEL_POINTER:
		case GDAUI_DATA_STORE_COL_MODEL_ROW:
		case GDAUI_DATA_STORE_COL_MODIFIED:
			g_warning (_("Trying to modify a read-only row"));
			break;
		case GDAUI_DATA_STORE_COL_TO_DELETE:
			if (g_value_get_boolean (value))
				gda_data_proxy_delete (priv->proxy, row);
			else
				gda_data_proxy_undelete (priv->proxy, row);
			break;
		default:
			g_assert_not_reached ();
		}
	}

	/* current proxy values or original ones */
	if (((col >= 0) && (col < model_nb_cols)) ||
	    ((col >= 2 * model_nb_cols) && (col < 3 * model_nb_cols))) {
		gint proxy_col;

		proxy_col = (col < model_nb_cols) ? col : col - model_nb_cols;
		return gda_data_model_set_value_at (GDA_DATA_MODEL (priv->proxy),
						    proxy_col, row, value, NULL);
	}

	/* value's attributes */
	if ((col >= model_nb_cols) && (col < 2 * model_nb_cols)) {
		gda_data_proxy_alter_value_attributes (priv->proxy, row, col - model_nb_cols,
						       g_value_get_uint (value));
		return TRUE;
	}
	return FALSE;
}

/**
 * gdaui_data_store_delete:
 * @store: a #GdauiDataStore object
 * @iter: the considered row
 *
 * Marks the row pointed by @iter to be deleted
 *
 * Since: 4.2
 */
void
gdaui_data_store_delete (GdauiDataStore *store, GtkTreeIter *iter)
{
	gint row;

        g_return_if_fail (GDAUI_IS_DATA_STORE (store));
        GdauiDataStorePrivate *priv = gdaui_data_store_get_instance_private (store);
        g_return_if_fail (priv);
        g_return_if_fail (priv->proxy);
        g_return_if_fail (iter);
        g_return_if_fail (iter->stamp == priv->stamp);

	if (priv->resetting) {
		g_warning (_("Can't modify row while data model is being reset"));
		return;
	}

        row = GPOINTER_TO_INT (iter->user_data);
	gda_data_proxy_delete (priv->proxy, row);
}


/**
 * gdaui_data_store_undelete:
 * @store: a #GdauiDataStore object
 * @iter: the considered row
 *
 * Remove the "to be deleted" mark the row pointed by @iter, if it existed.
 *
 * Since: 4.2
 */
void
gdaui_data_store_undelete (GdauiDataStore *store, GtkTreeIter *iter)
{
	gint row;

        g_return_if_fail (GDAUI_IS_DATA_STORE (store));
        GdauiDataStorePrivate *priv = gdaui_data_store_get_instance_private (store);
        g_return_if_fail (priv);
        g_return_if_fail (priv->proxy);
        g_return_if_fail (iter);
        g_return_if_fail (iter->stamp == priv->stamp);

	if (priv->resetting) {
		g_warning (_("Can't modify row while data model is being reset"));
		return;
	}

        row = GPOINTER_TO_INT (iter->user_data);
	gda_data_proxy_undelete (priv->proxy, row);
}


/**
 * gdaui_data_store_append:
 * @store: a #GdauiDataStore object
 * @iter: an unset #GtkTreeIter to set to the appended row
 *
 * Appends a new row.
 *
 * Returns: %TRUE if no error occurred
 *
 * Since: 4.2
 */
gboolean
gdaui_data_store_append (GdauiDataStore *store, GtkTreeIter *iter)
{
	gint row;

        g_return_val_if_fail (GDAUI_IS_DATA_STORE (store), FALSE);
        GdauiDataStorePrivate *priv = gdaui_data_store_get_instance_private (store);
        g_return_val_if_fail (priv, FALSE);
        g_return_val_if_fail (priv->proxy, FALSE);

	if (priv->resetting) {
		g_warning (_("Can't modify row while data model is being reset"));
		return FALSE;
	}

        row = gda_data_model_append_row (GDA_DATA_MODEL (priv->proxy), NULL);
	if (row >= 0) {
		if (iter) {
			iter->user_data = GINT_TO_POINTER (row);
			iter->stamp = priv->stamp;
		}
		return TRUE;
	}
	else
		return FALSE;
}

/**
 * gdaui_data_store_get_proxy:
 * @store: a #GdauiDataStore object
 *
 * Returns: (transfer none): the internal #GdaDataProxy being used by @store
 *
 * Since: 4.2
 */
GdaDataProxy *
gdaui_data_store_get_proxy (GdauiDataStore *store)
{
	g_return_val_if_fail (GDAUI_IS_DATA_STORE (store), FALSE);
        GdauiDataStorePrivate *priv = gdaui_data_store_get_instance_private (store);
        g_return_val_if_fail (priv, FALSE);

	return priv->proxy;
}

/**
 * gdaui_data_store_get_row_from_iter:
 * @store: a #GdauiDataStore object
 * @iter: a valid #GtkTreeIter
 *
 * Get the number of the row represented by @iter
 *
 * Returns: the row number, or -1 if an error occurred
 *
 * Since: 4.2
 */
gint
gdaui_data_store_get_row_from_iter (GdauiDataStore *store, GtkTreeIter *iter)
{
	g_return_val_if_fail (GDAUI_IS_DATA_STORE (store), -1);
        GdauiDataStorePrivate *priv = gdaui_data_store_get_instance_private (store);
        g_return_val_if_fail (priv, -1);
	g_return_val_if_fail (iter, -1);
        g_return_val_if_fail (iter->stamp == priv->stamp, -1);

	return GPOINTER_TO_INT (iter->user_data);
}

/**
 * gdaui_data_store_get_iter_from_values:
 * @store: a #GdauiDataStore object
 * @iter: (out): an unset #GtkTreeIter to set to the requested row
 * @values: (element-type GValue): a list of #GValue values
 * @cols_index: an array of #gint containing the column number to match each value of @values
 *
 * Sets @iter to the first row where all the values in @values at the columns identified at
 * @cols_index match. If the row can't be identified, then the contents of @iter is not modified.
 *
 * NOTE: the @cols_index array MUST contain a column index for each value in @values
 *
 * Returns: %TRUE if the row has been identified @iter was set
 *
 * Since: 4.2
 */
gboolean
gdaui_data_store_get_iter_from_values (GdauiDataStore *store, GtkTreeIter *iter,
				       GSList *values, gint *cols_index)
{
	gint row;

        g_return_val_if_fail (GDAUI_IS_DATA_STORE (store), FALSE);
        GdauiDataStorePrivate *priv = gdaui_data_store_get_instance_private (store);
        g_return_val_if_fail (priv, FALSE);
        g_return_val_if_fail (priv->proxy, FALSE);
	g_return_val_if_fail (values, FALSE);

	if (priv->resetting) {
		g_warning (_("Can't access row while data model is being reset"));
		return FALSE;
	}

	row = gda_data_model_get_row_from_values (GDA_DATA_MODEL (priv->proxy), values, cols_index);
	if (row >= 0) {
		if (iter) {
			iter->stamp = priv->stamp;
			iter->user_data = GINT_TO_POINTER (row);
		}
		return TRUE;
	}
	else
		return FALSE;
}



/*
 * GtkTreeModel Interface implementation
 *
 * REM about the GtkTreeIter: only the iter->user_data is used to retrieve a row in the data model:
 *     iter->user_data contains the requested row number in the GtkTreeModel numbers
 *     iter->stamp is reset any time the model changes.
 */

static GtkTreeModelFlags
data_store_get_flags (GtkTreeModel *tree_model)
{
        g_return_val_if_fail (GDAUI_IS_DATA_STORE (tree_model), 0);

        return (GTK_TREE_MODEL_LIST_ONLY | GTK_TREE_MODEL_ITERS_PERSIST);
}

static gint
data_store_get_n_columns (GtkTreeModel *tree_model)
{
        GdauiDataStore *store;

        g_return_val_if_fail (GDAUI_IS_DATA_STORE (tree_model), 0);
        store = GDAUI_DATA_STORE (tree_model);
        GdauiDataStorePrivate *priv = gdaui_data_store_get_instance_private (store);
        g_return_val_if_fail (priv, 0);
        g_return_val_if_fail (priv->proxy, 0);

        return gda_data_model_get_n_columns (GDA_DATA_MODEL (priv->proxy));
}

static GType
data_store_get_column_type (GtkTreeModel *tree_model, gint index)
{
	GdauiDataStore *store;
	GType retval = 0;

	g_return_val_if_fail (GDAUI_IS_DATA_STORE (tree_model), 0);
	store = (GdauiDataStore*) tree_model;
	GdauiDataStorePrivate *priv = gdaui_data_store_get_instance_private (store);
	store = GDAUI_DATA_STORE (tree_model);
	g_return_val_if_fail (priv->proxy, 0);

	if (index < 0) {
		switch (index) {
		case GDAUI_DATA_STORE_COL_MODEL_N_COLUMNS:
		case GDAUI_DATA_STORE_COL_MODEL_ROW:
			retval = G_TYPE_INT;
			break;
		case GDAUI_DATA_STORE_COL_MODEL_POINTER:
			retval = G_TYPE_POINTER;
			break;
		case GDAUI_DATA_STORE_COL_MODIFIED:
		case GDAUI_DATA_STORE_COL_TO_DELETE:
			retval = G_TYPE_BOOLEAN;
			break;
		}
	}
	else {
		gint prox_nb_cols;

		prox_nb_cols = gda_data_proxy_get_proxied_model_n_cols (priv->proxy);
		if ((index < prox_nb_cols) ||
		    ((index >= 2 * prox_nb_cols) && (index < 3 * prox_nb_cols)))
			retval = G_TYPE_POINTER;
		else
			if (index < 2 * prox_nb_cols)
				retval = G_TYPE_UINT;
	}

	if (retval == 0)
		g_warning ("Unknown GdaDataProxy column: %d", index);

	return retval;
}

static gboolean
data_store_get_iter (GtkTreeModel *tree_model, GtkTreeIter *iter, GtkTreePath *path)
{
        GdauiDataStore *store;
        gint *indices, n, depth;

        g_return_val_if_fail (GDAUI_IS_DATA_STORE (tree_model), FALSE);
        store = GDAUI_DATA_STORE (tree_model);
        GdauiDataStorePrivate *priv = gdaui_data_store_get_instance_private (store);
        g_return_val_if_fail (priv, FALSE);
        g_return_val_if_fail (priv->proxy, FALSE);
        g_return_val_if_fail (path, FALSE);
        g_return_val_if_fail (iter, FALSE);

        indices = gtk_tree_path_get_indices (path);
        depth = gtk_tree_path_get_depth (path);
        g_return_val_if_fail (depth == 1, FALSE);

        n = indices[0]; /* the n-th top level row */
        if (n < priv->nrows) {
                iter->stamp = priv->stamp;
                iter->user_data = GINT_TO_POINTER (n);
                return TRUE;
        }
        else
                return FALSE;
}

static GtkTreePath *
data_store_get_path (GtkTreeModel *tree_model, GtkTreeIter *iter)
{
        GdauiDataStore *store;
        GtkTreePath *path;

        g_return_val_if_fail (GDAUI_IS_DATA_STORE (tree_model), NULL);
        store = GDAUI_DATA_STORE (tree_model);
        GdauiDataStorePrivate *priv = gdaui_data_store_get_instance_private (store);
        g_return_val_if_fail (priv, NULL);
        g_return_val_if_fail (iter, NULL);
        g_return_val_if_fail (iter->stamp == priv->stamp, NULL);

        path = gtk_tree_path_new ();
        gtk_tree_path_append_index (path, GPOINTER_TO_INT (iter->user_data));

        return path;
}

static void
data_store_get_value (GtkTreeModel *tree_model, GtkTreeIter *iter, gint column, GValue *value)
{
	GdauiDataStore *store;
	const GValue *tmp;
	GType rettype;
	gint model_nb_cols = -10;

	g_return_if_fail (GDAUI_IS_DATA_STORE (tree_model));
        store = GDAUI_DATA_STORE (tree_model);
        GdauiDataStorePrivate *priv = gdaui_data_store_get_instance_private (store);
        g_return_if_fail (priv);
        g_return_if_fail (priv->proxy);
        g_return_if_fail (iter);
        g_return_if_fail (iter->stamp == priv->stamp);
        g_return_if_fail (value);

	rettype = data_store_get_column_type (tree_model, column);
	g_value_init (value, rettype);

	if (priv->resetting)
		return;

	/* Global attributes */
	if (column < 0) {
		switch (column) {
		case GDAUI_DATA_STORE_COL_MODEL_N_COLUMNS:
			model_nb_cols = gda_data_proxy_get_proxied_model_n_cols (priv->proxy);
			g_value_set_int (value, model_nb_cols);
			break;
		case GDAUI_DATA_STORE_COL_MODEL_POINTER:
			g_value_set_pointer (value, gda_data_proxy_get_proxied_model (priv->proxy));
			break;
		case GDAUI_DATA_STORE_COL_MODEL_ROW:
			g_value_set_int (value, gda_data_proxy_get_proxied_model_row (priv->proxy,
										      GPOINTER_TO_INT (iter->user_data)));
			break;
		case GDAUI_DATA_STORE_COL_MODIFIED:
			g_value_set_boolean (value, gda_data_proxy_row_has_changed (priv->proxy,
										    GPOINTER_TO_INT (iter->user_data)));
			break;
		case GDAUI_DATA_STORE_COL_TO_DELETE:
			g_value_set_boolean (value, gda_data_proxy_row_is_deleted (priv->proxy,
										   GPOINTER_TO_INT (iter->user_data)));
			break;
		default:
			g_assert_not_reached ();
		}
	}

	if (model_nb_cols == -10)
		model_nb_cols = gda_data_proxy_get_proxied_model_n_cols (priv->proxy);

	/* current proxy values or original ones */
	if (((column >= 0) && (column < model_nb_cols)) ||
	    ((column >= 2 * model_nb_cols) && (column < 3 * model_nb_cols))) {
		gint proxy_col;

		proxy_col = (column < model_nb_cols) ? column : column - model_nb_cols;
		tmp = gda_data_model_get_value_at ((GdaDataModel*) priv->proxy,
						   proxy_col,
						   GPOINTER_TO_INT (iter->user_data), NULL);

		rettype = data_store_get_column_type (tree_model, column);

		if (rettype == G_TYPE_POINTER)
			g_value_set_pointer (value, (gpointer) tmp);
		else if (tmp)
			g_value_copy (tmp, value);
		else {
			/* FIXME: we have an error here but I don't know how to report it. */
			TO_IMPLEMENT;
			gda_value_set_null (value);
		}
	}

	/* value's attributes */
	if ((column >= model_nb_cols) && (column < 2 * model_nb_cols)) {
		guint attr;

		attr = gda_data_proxy_get_value_attributes (priv->proxy,
							    GPOINTER_TO_INT (iter->user_data), column - model_nb_cols);
		g_value_set_uint (value, attr);
	}
}

static gboolean
data_store_iter_next (GtkTreeModel *tree_model, GtkTreeIter *iter)
{
        GdauiDataStore *store;
        gint row;

        g_return_val_if_fail (GDAUI_IS_DATA_STORE (tree_model), FALSE);
        store = GDAUI_DATA_STORE (tree_model);
        GdauiDataStorePrivate *priv = gdaui_data_store_get_instance_private (store);
        g_return_val_if_fail (priv, FALSE);
	g_return_val_if_fail (priv->proxy, FALSE);
        g_return_val_if_fail (iter, FALSE);
        g_return_val_if_fail (iter->stamp == priv->stamp, FALSE);

        row = GPOINTER_TO_INT (iter->user_data);
        row++;
        if (row >= priv->nrows)
                return FALSE;
        else {
                iter->user_data = GINT_TO_POINTER (row);
                return TRUE;
        }
}

static gboolean
data_store_iter_children (GtkTreeModel *tree_model, GtkTreeIter *iter, GtkTreeIter *parent)
{
        GdauiDataStore *store;

        g_return_val_if_fail (GDAUI_IS_DATA_STORE (tree_model), FALSE);
        store = GDAUI_DATA_STORE (tree_model);
        GdauiDataStorePrivate *priv = gdaui_data_store_get_instance_private (store);
        g_return_val_if_fail (priv, FALSE);
        g_return_val_if_fail (priv->proxy, FALSE);
        g_return_val_if_fail (iter, FALSE);

        if (!parent && (priv->nrows > 0)) {
                iter->stamp = priv->stamp;
                iter->user_data = GINT_TO_POINTER (0);
                return TRUE;
        }
        else
                return FALSE;
}

static gboolean
data_store_iter_has_child (G_GNUC_UNUSED GtkTreeModel *tree_model, G_GNUC_UNUSED GtkTreeIter *iter)
{
        return FALSE;
}

static gint
data_store_iter_n_children (GtkTreeModel *tree_model, GtkTreeIter *iter)
{
        GdauiDataStore *store;

        g_return_val_if_fail (GDAUI_IS_DATA_STORE (tree_model), -1);
        store = GDAUI_DATA_STORE (tree_model);
        GdauiDataStorePrivate *priv = gdaui_data_store_get_instance_private (store);
        g_return_val_if_fail (priv, 0);
        g_return_val_if_fail (priv->proxy, 0);

        if (!iter)
                return priv->nrows;
        else
                return 0;
}

static gboolean
data_store_iter_nth_child (GtkTreeModel *tree_model, GtkTreeIter *iter, GtkTreeIter *parent, gint n)
{
        GdauiDataStore *store;

        g_return_val_if_fail (GDAUI_IS_DATA_STORE (tree_model), FALSE);
        store = GDAUI_DATA_STORE (tree_model);
        GdauiDataStorePrivate *priv = gdaui_data_store_get_instance_private (store);
        g_return_val_if_fail (priv, FALSE);
        g_return_val_if_fail (priv->proxy, FALSE);
        g_return_val_if_fail (iter, FALSE);

        if (!parent && (n < priv->nrows)) {
                iter->stamp = priv->stamp;
                iter->user_data = GINT_TO_POINTER (n);
                return TRUE;
        }
        else
                return FALSE;
}

static gboolean
data_store_iter_parent (G_GNUC_UNUSED GtkTreeModel *tree_model, G_GNUC_UNUSED GtkTreeIter *iter, G_GNUC_UNUSED GtkTreeIter *child)
{
        return FALSE;
}
