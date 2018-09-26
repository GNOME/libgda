/*
 * Copyright (C) 2007 - 2014 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2008 - 2011 Murray Cumming <murrayc@murrayc.com>
 * Copyright (C) 2009 Bas Driessen <bas.driessen@xobas.com>
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

#include <glib/gi18n-lib.h>
#include <glib.h>
#include <string.h>
#include "gda-vconnection-data-model.h"
#include "gda-vconnection-data-model-private.h"
#include "gda-virtual-provider.h"
#include <libgda/gda-connection-private.h>
#include <libgda/gda-connection-internal.h>
#include "../gda-sqlite.h"

struct _GdaVconnectionDataModelPrivate {
	GSList *table_data_list; /* list of GdaVConnectionTableData structures */
	gboolean being_disposed;

	GMutex        lock_context;
	GdaStatement *executed_stmt;
};

static void gda_vconnection_data_model_class_init (GdaVconnectionDataModelClass *klass);
static void gda_vconnection_data_model_init       (GdaVconnectionDataModel *cnc, GdaVconnectionDataModelClass *klass);
static void gda_vconnection_data_model_dispose   (GObject *object);

static gboolean get_rid_of_vtable (GdaVconnectionDataModel *cnc, GdaVConnectionTableData *td, gboolean force, GError **error);

enum {
	VTABLE_CREATED,
	VTABLE_DROPPED,
	LAST_SIGNAL
};

static gint gda_vconnection_data_model_signals[LAST_SIGNAL] = { 0, 0 };

static GObjectClass  *parent_class = NULL;

#ifdef GDA_DEBUG_NO
static void
dump_all_tables (GdaVconnectionDataModel *cnc)
{
	GSList *list;
	g_print ("GdaVconnectionDataModel's tables:\n");
	for (list = cnc->priv->table_data_list; list; list = list->next) {
		GdaVConnectionTableData *td = (GdaVConnectionTableData *) list->data;
		g_print ("    table %s, td=%p, spec=%p\n", td->table_name, td, td->spec);
	}
}
#endif

static void
vtable_created (GdaVconnectionDataModel *cnc, const gchar *table_name)
{
	_gda_connection_signal_meta_table_update ((GdaConnection *)cnc, table_name);
#ifdef GDA_DEBUG_NO
	dump_all_tables (cnc);
#endif
}

static void
vtable_dropped (GdaVconnectionDataModel *cnc, const gchar *table_name)
{
	GdaVConnectionTableData *td;
	td = _gda_vconnection_get_table_data_by_name (cnc, table_name);
	if (td)
		cnc->priv->table_data_list = g_slist_remove (cnc->priv->table_data_list, td);
	if (! cnc->priv->being_disposed)
		_gda_connection_signal_meta_table_update ((GdaConnection *)cnc, table_name);
#ifdef GDA_DEBUG_NO
	dump_all_tables (cnc);
#endif
}

/*
 * GdaVconnectionDataModel class implementation
 */
static void
gda_vconnection_data_model_class_init (GdaVconnectionDataModelClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	/**
	 * GdaVconnectionDataModel::vtable-created
	 * @cnc: the #GdaVconnectionDataModel connection
	 * @spec: the #GdaVconnectionDataModelSpec for the new virtual table
	 *
	 * Signal emitted when a new virtual table has been declared
	 */
	gda_vconnection_data_model_signals[VTABLE_CREATED] =
		g_signal_new ("vtable-created",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (GdaVconnectionDataModelClass, vtable_created),
                              NULL, NULL,
                              g_cclosure_marshal_VOID__STRING,
			      G_TYPE_NONE, 1, G_TYPE_STRING);
	/**
	 * GdaVconnectionDataModel::vtable-dropped
	 * @cnc: the #GdaVconnectionDataModel connection
	 * @spec: the #GdaVconnectionDataModelSpec for the new virtual table
	 *
	 * Signal emitted when a new virtual table has been undeclared
	 */
	gda_vconnection_data_model_signals[VTABLE_DROPPED] =
		g_signal_new ("vtable-dropped",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (GdaVconnectionDataModelClass, vtable_dropped),
                              NULL, NULL,
                              g_cclosure_marshal_VOID__STRING,
			      G_TYPE_NONE, 1, G_TYPE_STRING);

	klass->vtable_created = vtable_created;
	klass->vtable_dropped = vtable_dropped;

	object_class->dispose = gda_vconnection_data_model_dispose;
}

static void
gda_vconnection_data_model_init (GdaVconnectionDataModel *cnc, G_GNUC_UNUSED GdaVconnectionDataModelClass *klass)
{
	cnc->priv = g_new (GdaVconnectionDataModelPrivate, 1);
	cnc->priv->table_data_list = NULL;
	cnc->priv->being_disposed = FALSE;
	g_mutex_init (& (cnc->priv->lock_context));

	g_object_set (G_OBJECT (cnc), "cnc-string", "_IS_VIRTUAL=TRUE", NULL);
}

static void
gda_vconnection_data_model_dispose (GObject *object)
{
	GdaVconnectionDataModel *cnc = (GdaVconnectionDataModel *) object;

	g_return_if_fail (GDA_IS_VCONNECTION_DATA_MODEL (cnc));

	/* free memory */
	if (cnc->priv) {
		cnc->priv->being_disposed = TRUE;
		while (cnc->priv->table_data_list) {
			GdaVConnectionTableData *td;
			td = (GdaVConnectionTableData *) cnc->priv->table_data_list->data;
			get_rid_of_vtable (cnc, td, TRUE, NULL);
		}
		gda_connection_close ((GdaConnection *) cnc, NULL);

		g_mutex_clear (& (cnc->priv->lock_context));
		g_free (cnc->priv);
		cnc->priv = NULL;
	}

	/* chain to parent class */
	parent_class->dispose (object);
}

/**
 * gda_vconnection_data_model_get_type:
 *
 * Returns: a new #GType
 */
GType
gda_vconnection_data_model_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static GMutex registering;
		if (type == 0) {
			static GTypeInfo info = {
				sizeof (GdaVconnectionDataModelClass),
				(GBaseInitFunc) NULL,
				(GBaseFinalizeFunc) NULL,
				(GClassInitFunc) gda_vconnection_data_model_class_init,
				NULL, NULL,
				sizeof (GdaVconnectionDataModel),
				0,
				(GInstanceInitFunc) gda_vconnection_data_model_init,
				0
			};

			g_mutex_lock (&registering);
			if (type == 0)
				type = g_type_register_static (GDA_TYPE_VIRTUAL_CONNECTION, "GdaVconnectionDataModel", &info, 0);
			g_mutex_unlock (&registering);
		}
	}

	return type;
}

static void
spec_destroy_func (GdaVconnectionDataModelSpec *spec)
{
	g_object_unref (spec->data_model);
	g_free (spec);
}

static GList *
create_columns (GdaVconnectionDataModelSpec *spec, G_GNUC_UNUSED GError **error)
{
	g_return_val_if_fail (spec->data_model, NULL);

	GList *columns = NULL;
	guint i, ncols;
	ncols = gda_data_model_get_n_columns (spec->data_model);
	for (i = 0; i < ncols; i++) {
		GdaColumn *mcol = gda_data_model_describe_column (spec->data_model, i);
		GdaColumn *ccol = gda_column_copy (mcol);
		columns = g_list_prepend (columns, ccol);
	}
	return g_list_reverse (columns);
}

/**
 * gda_vconnection_data_model_add_model:
 * @cnc: a #GdaVconnectionDataModel connection
 * @model: a #GdaDataModel
 * @table_name: the name of the table
 * @error: a place to store errors, or %NULL
 *
 * Make @model appear as a table named @table_name in the @cnc connection (as if a
 * "CREATE TABLE..." statement was executed, except that the data contained within @model
 * is actually used when @table_name's contents is read or written).
 *
 * For a more general approach, see the gda_vconnection_data_model_add() method.
 *
 * Returns: %TRUE if no error occurred
 */
gboolean
gda_vconnection_data_model_add_model (GdaVconnectionDataModel *cnc, 
				      GdaDataModel *model, const gchar *table_name, GError **error)
{
	GdaVconnectionDataModelSpec *spec;
	gboolean retval;

	spec = g_new0 (GdaVconnectionDataModelSpec, 1);
	spec->data_model = model;
	spec->create_columns_func = create_columns;
	g_object_ref (model);
	retval = gda_vconnection_data_model_add (cnc, spec, (GDestroyNotify) spec_destroy_func, table_name, error);

	return retval;
}

/**
 * gda_vconnection_data_model_add:
 * @cnc: a #GdaVconnectionDataModel connection
 * @spec: a #GdaVconnectionDataModelSpec structure, used AS IS (not copied) and can be modified
 * @spec_free_func: (allow-none): function to call when freeing @spec, or %NULL
 * @table_name: the name of the table
 * @error: a place to store errors, or %NULL
 *
 * Create a new virtual table named @table_name in @cnc. The contents of that new table
 * is dictated by what's in @spec.
 *
 * If there is just one #GdaDataModel to make appear as a table
 * then the gda_vconnection_data_model_add_model() method is easier to use.
 *
 * The @spec_free_func can (depending on your code) be used to clean memory allocated for @spec or
 * @spec->data_model.
 *
 * If an error occurs, then the @spec_free_func function is called using @spec as argument.
 *
 * Returns: %TRUE if no error occurred
 */
gboolean
gda_vconnection_data_model_add (GdaVconnectionDataModel *cnc, GdaVconnectionDataModelSpec *spec, 
				GDestroyNotify spec_free_func, const gchar *table_name, GError **error)
{
	GdaVirtualProvider *prov;
	gchar *str;
	int rc;
	char *zErrMsg = NULL;
	gboolean retval = TRUE;
	SqliteConnectionData *scnc;

	static gint counter = 0;

	g_return_val_if_fail (GDA_IS_VCONNECTION_DATA_MODEL (cnc), FALSE);
	g_return_val_if_fail (table_name && *table_name, FALSE);
	g_return_val_if_fail (spec, FALSE);
	g_return_val_if_fail (spec->data_model || (spec->create_columns_func && (spec->create_model_func || spec->create_filtered_model_func)), FALSE);

	/* cleaning functions */
	if (spec->data_model) {
		g_return_val_if_fail (GDA_IS_DATA_MODEL (spec->data_model), FALSE);
		spec->create_columns_func = create_columns;
		spec->create_model_func = NULL;
		spec->create_filter_func = NULL;
		spec->create_filtered_model_func = NULL;
	}
	else if (spec->create_filter_func)
		spec->create_model_func = NULL;
	else {
		spec->create_filter_func = NULL;
		spec->create_filtered_model_func = NULL;
	}

	scnc = (SqliteConnectionData*) gda_connection_internal_get_provider_data_error ((GdaConnection *) cnc, error);
	if (!scnc) 
		return FALSE;

	/* create a new GdaVConnectionTableData structure for this virtual table */
	GdaVConnectionTableData *td;
	td = g_new0 (GdaVConnectionTableData, 1);
	td->spec = spec;
	td->spec_free_func = spec_free_func;
	td->table_name = _gda_connection_compute_table_virtual_name (GDA_CONNECTION (cnc), table_name);
	td->unique_name = g_strdup_printf ("Spec%d", counter++);
	cnc->priv->table_data_list = g_slist_append (cnc->priv->table_data_list, td);

	/* actually create the virtual table in @cnc */
	prov = (GdaVirtualProvider *) gda_connection_get_provider (GDA_CONNECTION (cnc));
	str = g_strdup_printf ("CREATE VIRTUAL TABLE %s USING %s ('%s')", td->table_name, G_OBJECT_TYPE_NAME (prov), td->unique_name);
	rc = SQLITE3_CALL (sqlite3_exec) (scnc->connection, str, NULL, 0, &zErrMsg);
	g_free (str);
	if (rc != SQLITE_OK) {
		g_set_error (error, GDA_SERVER_PROVIDER_ERROR,
			     GDA_SERVER_PROVIDER_INTERNAL_ERROR,
			     "%s", zErrMsg);
		SQLITE3_CALL (sqlite3_free) (zErrMsg);
		_gda_vconnection_data_model_table_data_free (td);
		cnc->priv->table_data_list = g_slist_remove (cnc->priv->table_data_list, td);
		retval = FALSE;
	}
	else {
		g_signal_emit (G_OBJECT (cnc), gda_vconnection_data_model_signals[VTABLE_CREATED], 0,
			       td->table_name);
		/*g_print ("Virtual connection: added table %s (spec = %p)\n", td->table_name, td->spec);*/
	}

	return retval;
}

static gboolean
get_rid_of_vtable (GdaVconnectionDataModel *cnc, GdaVConnectionTableData *td, gboolean force, GError **error)
{
	gchar *str;
	int rc;
	char *zErrMsg = NULL;
	gboolean allok = TRUE;

	SqliteConnectionData *scnc;
	scnc = (SqliteConnectionData*) gda_connection_internal_get_provider_data_error ((GdaConnection *) cnc, error);
	if (!scnc && !force)
		return FALSE;

	if (scnc) {
		str = g_strdup_printf ("DROP TABLE %s", td->table_name);
		rc = SQLITE3_CALL (sqlite3_exec) (scnc->connection, str, NULL, 0, &zErrMsg);
		g_free (str);

		if (rc != SQLITE_OK) {
			g_set_error (error, GDA_SERVER_PROVIDER_ERROR,
				     GDA_SERVER_PROVIDER_INTERNAL_ERROR,
				     "%s", zErrMsg);
			SQLITE3_CALL (sqlite3_free) (zErrMsg);
			allok = FALSE;
			if (!force)
				return FALSE;
		}
	}

	/* clean the cnc->priv->table_data_list list */
	cnc->priv->table_data_list = g_slist_remove (cnc->priv->table_data_list, td);
	g_signal_emit (G_OBJECT (cnc), gda_vconnection_data_model_signals[VTABLE_DROPPED], 0,
		       td->table_name);
	/*g_print ("Virtual connection: removed table %s (%p)\n", td->table_name, td->spec->data_model);*/
	_gda_vconnection_data_model_table_data_free (td);

	return allok;
}

/**
 * gda_vconnection_data_model_remove:
 * @cnc: a #GdaVconnectionDataModel connection
 * @table_name: the name of the table to remove from @cnc
 * @error: a place to store errors, or %NULL
 *
 * Remove the table named @table_name in the @cnc connection (as if a "DROP TABLE..."
 * statement was executed, except that no data gets destroyed as the associated data model remains the same).
 *
 * Returns: %TRUE if no error occurred
 */
gboolean
gda_vconnection_data_model_remove (GdaVconnectionDataModel *cnc, const gchar *table_name, GError **error)
{
	GdaVConnectionTableData *td;

	g_return_val_if_fail (GDA_IS_VCONNECTION_DATA_MODEL (cnc), FALSE);
	g_return_val_if_fail (table_name && *table_name, FALSE);

	td = _gda_vconnection_get_table_data_by_name (cnc, table_name);
	if (!td) {
		g_set_error (error, GDA_SERVER_PROVIDER_ERROR,
			     GDA_SERVER_PROVIDER_MISUSE_ERROR,
			     "%s", _("Table to remove not found"));
		return FALSE;
	}

	return get_rid_of_vtable (cnc, td, FALSE, error);
}

/**
 * gda_vconnection_data_model_get:
 * @cnc: a #GdaVconnectionDataModel connection
 * @table_name: a table name within @cnc
 *
 * Find the #GdaVconnectionDataModelSpec specifying how the table named @table_name is represented
 * in @cnc.
 *
 * Returns: (transfer none) (allow-none): a #GdaVconnectionDataModelSpec pointer, of %NULL if there is no table named @table_name
 *
 * Since: 4.2.6
 */
GdaVconnectionDataModelSpec *
gda_vconnection_data_model_get (GdaVconnectionDataModel *cnc, const gchar *table_name)
{
	GdaVConnectionTableData *td;
	g_return_val_if_fail (GDA_IS_VCONNECTION_DATA_MODEL (cnc), NULL);
	if (!table_name || !(*table_name))
		return NULL;
	td = _gda_vconnection_get_table_data_by_name (cnc, table_name);
	if (td)
		return td->spec;
	else
		return NULL;
}

/**
 * gda_vconnection_data_model_get_model:
 * @cnc: a #GdaVconnectionDataModel connection
 * @table_name: a table name within @cnc
 *
 * Find the #GdaDataModel object representing the @table_name table in @cnc. it can return %NULL
 * either if no table named @table_name exists, or if that table actually exists but no #GdaDataModel
 * has yet been created. For a more general approach, use the gda_vconnection_data_model_get().
 *
 * Returns: (transfer none) (allow-none): the #GdaDataModel, or %NULL
 */
GdaDataModel *
gda_vconnection_data_model_get_model (GdaVconnectionDataModel *cnc, const gchar *table_name)
{
	GdaVConnectionTableData *td;
	g_return_val_if_fail (GDA_IS_VCONNECTION_DATA_MODEL (cnc), NULL);
	g_return_val_if_fail (cnc->priv, NULL);
	if (!table_name || !(*table_name))
		return NULL;

	td = _gda_vconnection_get_table_data_by_name (cnc, table_name);
	if (td)
		return td->spec->data_model;
	else
		return NULL;
}

/**
 * gda_vconnection_data_model_get_table_name:
 * @cnc: a #GdaVconnectionDataModel connection
 * @model: a #GdaDataModel representing a table within @cnc
 *
 * Find the name of the table associated to @model in @cnc
 *
 * Returns: (transfer none) (allow-none): the table name, or %NULL if not found
 */
const gchar *
gda_vconnection_data_model_get_table_name (GdaVconnectionDataModel *cnc, GdaDataModel *model)
{
	GdaVConnectionTableData *td;
	g_return_val_if_fail (GDA_IS_VCONNECTION_DATA_MODEL (cnc), NULL);
	g_return_val_if_fail (cnc->priv, NULL);
	if (!model)
		return NULL;
	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), NULL);

	td = _gda_vconnection_get_table_data_by_model (cnc, model);
	if (td)
		return td->table_name;
	else
		return NULL;
}

/**
 * gda_vconnection_data_model_foreach:
 * @cnc: a #GdaVconnectionDataModel connection
 * @func: a #GdaVconnectionDataModelFunc function pointer
 * @data: data to pass to @func calls
 *
 * Call @func for each table in @cnc. 
 *
 * Warning: @func will be called for any table present in @cnc even if no data
 * model represents the contents of the table (which means the 1st argument of @func
 * may be %NULL)
 */
void
gda_vconnection_data_model_foreach (GdaVconnectionDataModel *cnc, 
				    GdaVconnectionDataModelFunc func, gpointer data)
{
	GSList *copy, *list;
	g_return_if_fail (GDA_IS_VCONNECTION_DATA_MODEL (cnc));
	g_return_if_fail (cnc->priv);

	if (!func || !cnc->priv->table_data_list)
		return;

	copy = g_slist_copy (cnc->priv->table_data_list);
	for (list = copy; list; list = list->next) {
		GdaVConnectionTableData *td = (GdaVConnectionTableData*) list->data;
		func (td->spec->data_model, td->table_name, data);
	}
	g_slist_free (copy);
}

/* 
 * private 
 */
GdaVConnectionTableData *
_gda_vconnection_get_table_data_by_name (GdaVconnectionDataModel *cnc, const gchar *table_name)
{
	GSList *list;
	gchar *quoted;
	if (!table_name || !*table_name)
		return NULL;
	quoted = _gda_connection_compute_table_virtual_name (GDA_CONNECTION (cnc), table_name);
	for (list = cnc->priv->table_data_list; list; list = list->next) {
		if (!strcmp (((GdaVConnectionTableData*) list->data)->table_name, quoted)) {
			g_free (quoted);
			return (GdaVConnectionTableData*) list->data;
		}
	}
	g_free (quoted);
	return NULL;
}

GdaVConnectionTableData *
_gda_vconnection_get_table_data_by_unique_name (GdaVconnectionDataModel *cnc, const gchar *unique_name)
{
	GSList *list;
	for (list = cnc->priv->table_data_list; list; list = list->next) {
		if (!strcmp (((GdaVConnectionTableData*) list->data)->unique_name, unique_name))
			return (GdaVConnectionTableData*) list->data;
	}
	return NULL;
}

GdaVConnectionTableData *
_gda_vconnection_get_table_data_by_model (GdaVconnectionDataModel *cnc, GdaDataModel *model)
{
	GSList *list;
	for (list = cnc->priv->table_data_list; list; list = list->next) {
		if (((GdaVConnectionTableData*) list->data)->real_model == model)
			return (GdaVConnectionTableData*) list->data;
	}
	return NULL;
}

void
_gda_vconnection_data_model_table_data_free (GdaVConnectionTableData *td)
{
	ParamType i;

	if (td->real_model)
		g_object_unref (td->real_model);
	if (td->columns) {
		g_list_foreach (td->columns, (GFunc) g_object_unref, NULL);
		g_list_free (td->columns);
	}
	g_free (td->table_name);
	g_free (td->unique_name);
	if (td->spec_free_func)
		td->spec_free_func (td->spec);
	for (i = 0; i < PARAMS_NB; i++) {
		if (td->modif_params[i])
			g_object_unref (td->modif_params[i]);
		if (td->modif_stmt[i])
			g_object_unref (td->modif_stmt[i]);
	}

	if (td->context.hash)
		g_hash_table_destroy (td->context.hash);
	g_free (td);
}


static void
vcontext_object_weak_notify_cb (VContext *context, GObject *old_context_object)
{
	g_assert (context);
	context->context_object = NULL;
	g_hash_table_remove (context->vtable->context.hash, old_context_object);
}

static void
vcontext_free (VContext *context)
{
	if (context->context_object)
		g_object_weak_unref (context->context_object,
				     (GWeakNotify) vcontext_object_weak_notify_cb, context);
	if (context->context_data) {
		g_array_free (context->context_data, TRUE);
		context->context_data = NULL;
	}
	g_free (context);
#ifdef DEBUG_VCONTEXT
	g_print ("VCFree %p\n", context);
#endif
}

void
_gda_vconnection_set_working_obj (GdaVconnectionDataModel *cnc, GObject *obj)
{
	GSList *list;
	if (obj) {
		g_mutex_lock (& (cnc->priv->lock_context));
		for (list = cnc->priv->table_data_list; list; list = list->next) {
			GdaVConnectionTableData *td = (GdaVConnectionTableData*) list->data;
			VContext *vc = NULL;
			
			g_assert (!td->context.current_vcontext);
			td->context.mutex = &(cnc->priv->lock_context);
			if (! td->context.hash)
				td->context.hash = g_hash_table_new_full (g_direct_hash, g_direct_equal,
									  NULL, (GDestroyNotify) vcontext_free);
			else
				vc = g_hash_table_lookup (td->context.hash, obj);

			if (! vc) {
				vc = g_new0 (VContext, 1);
				vc->context_object = obj;
				vc->context_data = g_array_new (FALSE, FALSE,
								sizeof (VirtualFilteredData*));
				g_array_set_clear_func (vc->context_data, (GDestroyNotify) _gda_vconnection_virtual_filtered_data_unref);
				vc->vtable = td;
				g_object_weak_ref (obj, (GWeakNotify) vcontext_object_weak_notify_cb, vc);
				g_hash_table_insert (td->context.hash, obj, vc);
#ifdef DEBUG_VCONTEXT
				g_print ("VCNew %p\n", vc);
#endif
			}
			td->context.current_vcontext = vc;
		}
	}
	else {
		for (list = cnc->priv->table_data_list; list; list = list->next) {
			GdaVConnectionTableData *td = (GdaVConnectionTableData*) list->data;
			/* REM: td->context.current_vcontext may already be NULL in case
			 * an exception already occurred */
			td->context.current_vcontext = NULL;
		}
		g_mutex_unlock (& (cnc->priv->lock_context));
	}
}

void
_gda_vconnection_change_working_obj (GdaVconnectionDataModel *cnc, GObject *obj)
{
	GSList *list;

	for (list = cnc->priv->table_data_list; list; list = list->next) {
		GdaVConnectionTableData *td = (GdaVConnectionTableData*) list->data;
		if (!td->context.hash)
			continue;

		g_assert (td->context.current_vcontext);

		VContext *ovc, *nvc;
		ovc = td->context.current_vcontext;
		nvc = g_new0 (VContext, 1);
		nvc->context_object = obj;
		nvc->vtable = ovc->vtable;
		nvc->context_data = ovc->context_data;
		ovc->context_data = NULL;
		g_object_weak_ref (obj, (GWeakNotify) vcontext_object_weak_notify_cb, nvc);
		g_hash_table_insert (td->context.hash, obj, nvc);
#ifdef DEBUG_VCONTEXT
		g_print ("VCNew %p\n", nvc);
#endif
		g_hash_table_remove (td->context.hash, ovc->context_object);
	}
}
