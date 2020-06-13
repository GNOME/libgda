/*
 * Copyright (C) 2007 - 2014 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2008 - 2011 Murray Cumming <murrayc@murrayc.com>
 * Copyright (C) 2009 Bas Driessen <bas.driessen@xobas.com>
 * Copyright (C) 2010 David King <davidk@openismus.com>
 * Copyright (C) 2017-2018 Daniel Espinosa <esodan@gmail.com>
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
#include <string.h>
#include "gda-vconnection-hub.h"
#include "gda-virtual-provider.h"
#include <libgda/sql-parser/gda-sql-parser.h>
#include <libgda/gda-util.h>
#include <libgda/gda-data-select.h>
#include <libgda/gda-sql-builder.h>
#include "../gda-sqlite.h"
#include <libgda/gda-connection-internal.h>
#include <libgda/gda-log.h>

typedef struct {
	GdaVconnectionHub *hub;
	GdaConnection     *cnc;
	gchar             *ns; /* can be NULL in one case only among all the HubConnection structs */
} HubConnection;

static void hub_connection_free (HubConnection *hc);
static gboolean attach_hub_connection (GdaVconnectionHub *hub, HubConnection *hc, GError **error);
static void detach_hub_connection (GdaVconnectionHub *hub, HubConnection *hc);

typedef struct {
	GSList        *hub_connections; /* list of HubConnection structures */
	GdaSqlParser *internal_parser;
} GdaVconnectionHubPrivate;

static void gda_vconnection_hub_dispose   (GObject *object);

G_DEFINE_TYPE_WITH_PRIVATE (GdaVconnectionHub, gda_vconnection_hub, GDA_TYPE_VCONNECTION_DATA_MODEL)


static HubConnection *get_hub_cnc_by_ns (GdaVconnectionHub *hub, const gchar *ns);
static HubConnection *get_hub_cnc_by_cnc (GdaVconnectionHub *hub, GdaConnection *cnc);

/*
 * GdaVconnectionHub class implementation
 */
static void
gda_vconnection_hub_class_init (GdaVconnectionHubClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->dispose = gda_vconnection_hub_dispose;
}

static void
gda_vconnection_hub_init (GdaVconnectionHub *cnc)
{
	GdaVconnectionHubPrivate *priv = gda_vconnection_hub_get_instance_private (cnc);
	priv->hub_connections = NULL;
	priv->internal_parser = gda_sql_parser_new ();;
}

static void
gda_vconnection_hub_dispose (GObject *object)
{
	GdaVconnectionHub *cnc = (GdaVconnectionHub *) object;
	GdaVconnectionHubPrivate *priv = gda_vconnection_hub_get_instance_private (cnc);

	g_return_if_fail (GDA_IS_VCONNECTION_HUB (cnc));

	/* free memory */
	gda_connection_close ((GdaConnection *) cnc, NULL);
	if (priv->hub_connections) {
		g_slist_free_full (priv->hub_connections, (GDestroyNotify) hub_connection_free);
		priv->hub_connections = NULL;
	}
	if (priv->internal_parser) {
		g_object_unref (priv->internal_parser);
		priv->internal_parser = NULL;
	}

	/* chain to parent class */
	G_OBJECT_CLASS (gda_vconnection_hub_parent_class)->dispose (object);
}

/**
 * gda_vconnection_hub_add:
 * @hub: a #GdaVconnectionHub connection
 * @cnc: a #GdaConnection
 * @ns: a namespace, or %NULL
 * @error: a place to store errors, or %NULL
 *
 * Make all the tables of @cnc appear as tables (of the same name) in the @hub connection.
 * If the @ns is not %NULL, then within @hub, the tables will be accessible using the '@ns.@table_name'
 * notation.
 *
 * Within any instance of @hub, there can be only one added connection where @ns is %NULL.
 *
 * Returns: TRUE if no error occurred
 */
gboolean
gda_vconnection_hub_add (GdaVconnectionHub *hub,
			 GdaConnection *cnc, const gchar *ns, GError **error)
{
	HubConnection *hc;

	g_return_val_if_fail (GDA_IS_VCONNECTION_HUB (hub), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);

	if (ns == NULL) {
		g_set_error (error, GDA_SERVER_PROVIDER_ERROR,
			     GDA_SERVER_PROVIDER_MISUSE_ERROR,
			     "%s", _("Namespace must be specified"));
		return FALSE;
	}

	/* check for constraints */
	hc = get_hub_cnc_by_ns (hub, ns);
	if (hc && (hc->cnc != cnc)) {
		g_set_error (error, GDA_SERVER_PROVIDER_ERROR,
			     GDA_SERVER_PROVIDER_MISUSE_ERROR,
			     _("There is already a namespace named '%s' in use with another connection"), ns);
		return FALSE;
	}

	if (hc)
		return TRUE;

	if (!gda_connection_is_opened (cnc)) {
		g_set_error (error, GDA_SERVER_PROVIDER_ERROR,
			     GDA_SERVER_PROVIDER_MISUSE_ERROR,
			     _("Connection was not added to virtual connection because it is closed"));
		return FALSE;
	}

	/* actually adding @cnc */
	hc = g_new (HubConnection, 1);
	hc->hub = hub;
	hc->cnc = GDA_CONNECTION (g_object_ref (cnc));
	hc->ns = g_strdup (ns);

	if (!attach_hub_connection (hub, hc, error)) {
		hub_connection_free (hc);
		return FALSE;
	}

	return TRUE;
}

/**
 * gda_vconnection_hub_remove:
 * @hub: a #GdaVconnectionHub connection
 * @cnc: a #GdaConnection
 * @error: a place to store errors, or %NULL
 *
 * Remove all the tables in @hub representing @cnc's tables.
 *
 * Returns: TRUE if no error occurred
 */
gboolean
gda_vconnection_hub_remove (GdaVconnectionHub *hub, GdaConnection *cnc, GError **error)
{
	HubConnection *hc;

	g_return_val_if_fail (GDA_IS_VCONNECTION_HUB (hub), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);

	hc = get_hub_cnc_by_cnc (hub, cnc);

	if (!hc) {
		g_set_error (error, GDA_SERVER_PROVIDER_ERROR,
			     GDA_SERVER_PROVIDER_MISUSE_ERROR,
			     "%s", _("Connection was not represented in hub"));
		return FALSE;
	}

	/* clean the priv->hub_connections list */
	detach_hub_connection (hub, hc);
	return TRUE;
}

static  HubConnection*
get_hub_cnc_by_ns (GdaVconnectionHub *hub, const gchar *ns)
{
	g_return_val_if_fail (ns != NULL, NULL);
	GSList *list;
	GdaVconnectionHubPrivate *priv = gda_vconnection_hub_get_instance_private (hub);
	for (list = priv->hub_connections; list; list = list->next) {
		if ((!ns && !((HubConnection*) list->data)->ns)||
		    (ns && ((HubConnection*) list->data)->ns && !strcmp (((HubConnection*) list->data)->ns, ns)))
			return (HubConnection*) list->data;
	}
	return NULL;
}

static HubConnection *
get_hub_cnc_by_cnc (GdaVconnectionHub *hub, GdaConnection *cnc)
{
	GSList *list;
	GdaVconnectionHubPrivate *priv = gda_vconnection_hub_get_instance_private (hub);
	for (list = priv->hub_connections; list; list = list->next) {
		if (((HubConnection*) list->data)->cnc == cnc)
			return (HubConnection*) list->data;
	}
	return NULL;
}

/**
 * gda_vconnection_hub_get_connection:
 * @hub: a #GdaVconnectionHub connection
 * @ns: (nullable): a name space, or %NULL
 *
 * Find the #GdaConnection object in @hub associated to the @ns name space
 *
 * Returns: the #GdaConnection, or %NULL if no connection is associated to @ns
 */
GdaConnection *
gda_vconnection_hub_get_connection (GdaVconnectionHub *hub, const gchar *ns)
{
	HubConnection *hc;
	g_return_val_if_fail (GDA_IS_VCONNECTION_HUB (hub), NULL);

	hc = get_hub_cnc_by_ns (hub, ns);
	if (hc)
		return hc->cnc;
	else
		return NULL;
}

/**
 * gda_vconnection_hub_foreach:
 * @hub: a #GdaVconnectionHub connection
 * @func: a #GdaVconnectionDataModelFunc function pointer
 * @data: data to pass to @func calls
 *
 * Call @func for each #GdaConnection represented in @hub.
 */
void
gda_vconnection_hub_foreach (GdaVconnectionHub *hub,
			     GdaVConnectionHubFunc func, gpointer data)
{
	GSList *list, *next;
	g_return_if_fail (GDA_IS_VCONNECTION_HUB (hub));
	GdaVconnectionHubPrivate *priv = gda_vconnection_hub_get_instance_private (hub);

	if (!func)
		return;

	list = priv->hub_connections;
	while (list) {
		HubConnection *hc = (HubConnection*) list->data;
		next = list->next;
		func (hc->cnc, hc->ns, data);
		list = next;
	}
}

static void meta_changed_cb (GdaMetaStore *store, GSList *changes, HubConnection *hc);

typedef struct {
	GdaVconnectionDataModelSpec spec;
	GValue        *table_name;
	HubConnection *hc;

	GError        *cols_error;
	gint           ncols;
	gchar        **col_names; /* free using g_strfreev */
	GType         *col_gtypes;/* free using g_free */
	gchar        **col_dtypes;/* free using g_strfreev */

	GHashTable    *filters_hash; /* key = string; value = a ComputedFilter pointer */
} LocalSpec;

static void local_spec_free (LocalSpec *spec)
{
	gda_value_free (spec->table_name);
	if (spec->col_names)
		g_strfreev (spec->col_names);
	if (spec->col_gtypes)
		g_free (spec->col_gtypes);
	if (spec->col_dtypes)
		g_strfreev (spec->col_dtypes);
	g_clear_error (&(spec->cols_error));
	if (spec->filters_hash)
		g_hash_table_destroy (spec->filters_hash);
	g_free (spec);
}

static void
compute_column_specs (GdaVconnectionDataModelSpec *spec)
{
	LocalSpec *lspec = (LocalSpec *) spec;
	gint i, nrows;
	GdaDataModel *model;

	if (lspec->col_names)
		return;

	model = gda_connection_get_meta_store_data (lspec->hc->cnc,
						    GDA_CONNECTION_META_FIELDS, NULL, 1, "name", lspec->table_name);
	if (!model)
		return;

	nrows = gda_data_model_get_n_rows (model);
	lspec->col_names = g_new0 (gchar *, nrows+1);
	lspec->col_gtypes = g_new0 (GType, nrows);
	lspec->col_dtypes = g_new0 (gchar *, nrows+1);

	for (i = 0; i < nrows; i++) {
		const GValue *v0, *v1, *v2;

		v0 = gda_data_model_get_value_at (model, 0, i, NULL);
		v1 = gda_data_model_get_value_at (model, 1, i, NULL);
		v2 = gda_data_model_get_value_at (model, 2, i, NULL);

		if (!v0 || !v1 || !v2) {
			break;
		}

		lspec->col_names[i] = g_value_dup_string (v0);
		lspec->col_gtypes[i] = gda_g_type_from_string (g_value_get_string (v2));
		if (lspec->col_gtypes[i] == G_TYPE_INVALID)
			lspec->col_gtypes[i] = GDA_TYPE_NULL;
		lspec->col_dtypes[i] = g_value_dup_string (v1);
	}
	g_object_unref (model);

	if (i != nrows) {
		/* there has been an error */
		g_strfreev (lspec->col_names);
		lspec->col_names = NULL;
		g_free (lspec->col_gtypes);
		lspec->col_gtypes = NULL;
		g_strfreev (lspec->col_dtypes);
		lspec->col_dtypes = NULL;
		g_set_error (&(lspec->cols_error), GDA_META_STORE_ERROR, GDA_META_STORE_INTERNAL_ERROR,
			     _("Unable to get information about table '%s'"),
			     g_value_get_string (lspec->table_name));
	}
	else
		lspec->ncols = nrows;
}

static GList *
dict_table_create_columns_func (GdaVconnectionDataModelSpec *spec, GError **error)
{
	LocalSpec *lspec = (LocalSpec *) spec;
	GList *columns = NULL;
	gint i;

	compute_column_specs (spec);
	if (lspec->cols_error) {
		if (error)
			*error = g_error_copy (lspec->cols_error);
		return NULL;
	}

	for (i = 0; i < lspec->ncols; i++) {
		GdaColumn *col;

		col = gda_column_new ();
		gda_column_set_name (col, lspec->col_names[i]);
		gda_column_set_g_type (col, lspec->col_gtypes[i]);
		gda_column_set_dbms_type (col, lspec->col_dtypes[i]);
		columns = g_list_prepend (columns, col);
	}

	return g_list_reverse (columns);
}

static gchar *
make_string_for_filter (GdaVconnectionDataModelFilter *info)
{
	GString *string;
	gint i;

	string = g_string_new ("");
	for (i = 0; i < info->nConstraint; i++) {
		const struct GdaVirtualConstraint *cons;
		cons = &(info->aConstraint [i]);
		g_string_append_printf (string, "|%d,%d", cons->iColumn, cons->op);
	}
	g_string_append_c (string, '/');
	for (i = 0; i < info->nOrderBy; i++) {
		struct GdaVirtualOrderby *order;
		order = &(info->aOrderBy[i]);
		g_string_append_printf (string, "|%d,%d", order->iColumn, order->desc ? 1 : 0);
	}
	return g_string_free (string, FALSE);
}

typedef struct {
	GdaStatement *stmt;
	int orderByConsumed;
	struct GdaVirtualConstraintUsage *out_const;
} ComputedFilter;

static void
computed_filter_free (ComputedFilter *filter)
{
	g_object_unref (filter->stmt);
	g_free (filter->out_const);
	g_free (filter);
}

static void
dict_table_create_filter (GdaVconnectionDataModelSpec *spec, GdaVconnectionDataModelFilter *info)
{
	LocalSpec *lspec = (LocalSpec *) spec;
	GdaSqlBuilder *b;
	gint i;
	gchar *hash;

	compute_column_specs (spec);
	if (lspec->cols_error)
		return;

	hash = make_string_for_filter (info);
	if (lspec->filters_hash) {
		ComputedFilter *filter;
		filter = g_hash_table_lookup (lspec->filters_hash, hash);
		if (filter) {
			info->idxPointer = filter->stmt;
			info->orderByConsumed = filter->orderByConsumed;
			memcpy (info->aConstraintUsage,
				filter->out_const,
				sizeof (struct GdaVirtualConstraintUsage) * info->nConstraint);
			/*g_print ("Reusing filter %p, hash=[%s]\n", filter, hash);*/
			g_free (hash);
			return;
		}
	}

	/* SELECT core */
	b = gda_sql_builder_new (GDA_SQL_STATEMENT_SELECT);
	for (i = 0; i < lspec->ncols; i++)
		gda_sql_builder_select_add_field (b, lspec->col_names[i], NULL, NULL);
	gda_sql_builder_select_add_target_id (b,
					      gda_sql_builder_add_id (b, g_value_get_string (lspec->table_name)),
					      NULL);

	/* WHERE part */
	gint argpos;
	GdaSqlBuilderId *op_ids;
	op_ids = g_new (GdaSqlBuilderId, info->nConstraint);
	for (i = 0, argpos = 0; i < info->nConstraint; i++) {
		const struct GdaVirtualConstraint *cons;
		cons = &(info->aConstraint [i]);
		if (cons->iColumn >= lspec->ncols) {
			g_warning ("Internal error: column known by SQLite's virtual table %d is not known for "
				   "table '%s', which has %d column(s)", cons->iColumn,
				   g_value_get_string (lspec->table_name), lspec->ncols);
			continue;
		}

		GdaSqlBuilderId fid, pid, eid;
		gchar *pname;

		if (lspec->col_gtypes[cons->iColumn] == GDA_TYPE_BLOB) /* ignore BLOBs */
			continue;

		fid = gda_sql_builder_add_id (b, lspec->col_names[cons->iColumn]);
		pname = g_strdup_printf ("param%d", argpos);
		pid = gda_sql_builder_add_param (b, pname, lspec->col_gtypes[cons->iColumn], TRUE);
		g_free (pname);
		eid = gda_sql_builder_add_cond (b, cons->op, fid, pid, 0);
		op_ids[argpos] = eid;

		/* update info->aConstraintUsage */
		info->aConstraintUsage [i].argvIndex = argpos+1;
		info->aConstraintUsage [i].omit = 1;
		argpos++;
	}
	if (argpos > 0) {
		GdaSqlBuilderId whid;
		whid = gda_sql_builder_add_cond_v (b, GDA_SQL_OPERATOR_TYPE_AND, op_ids, argpos);
		gda_sql_builder_set_where (b, whid);
	}
	g_free (op_ids);

	/* ORDER BY part */
	info->orderByConsumed = FALSE;
	for (i = 0; i < info->nOrderBy; i++) {
		struct GdaVirtualOrderby *ao;
		GdaSqlBuilderId fid;
		info->orderByConsumed = TRUE;
		ao = &(info->aOrderBy [i]);
		if (ao->iColumn >= lspec->ncols) {
			g_warning ("Internal error: column known by SQLite's virtual table %d is not known for "
				   "table '%s', which has %d column(s)", ao->iColumn,
				   g_value_get_string (lspec->table_name), lspec->ncols);
			info->orderByConsumed = FALSE;
			continue;
		}
		fid = gda_sql_builder_add_id (b, lspec->col_names[ao->iColumn]);
		gda_sql_builder_select_order_by (b, fid, ao->desc ? FALSE : TRUE, NULL);
	}

	GdaStatement *stmt;
	stmt = gda_sql_builder_get_statement (b, NULL);
	g_object_unref (b);
	if (stmt) {
		ComputedFilter *filter;

		filter = g_new0 (ComputedFilter, 1);
		filter->stmt = stmt;
		filter->orderByConsumed = info->orderByConsumed;
		filter->out_const = g_new (struct GdaVirtualConstraintUsage,  info->nConstraint);
		memcpy (filter->out_const,
			info->aConstraintUsage,
			sizeof (struct GdaVirtualConstraintUsage) * info->nConstraint);

		gchar *sql;
		sql = gda_statement_to_sql (stmt, NULL, NULL);
		/*g_print ("Filter %p for [%s] hash=[%s]\n", filter, sql, hash);*/
		g_free (sql);

		if (! lspec->filters_hash)
			lspec->filters_hash = g_hash_table_new_full (g_str_hash, g_str_equal,
								     g_free,
								     (GDestroyNotify) computed_filter_free);

		g_hash_table_insert (lspec->filters_hash, hash, filter);
		info->idxPointer = filter->stmt;
		/*g_print ("There are now %d statements in store...\n", g_hash_table_size (lspec->filters_hash));*/
	}
	else {
		for (i = 0, argpos = 0; i < info->nConstraint; i++) {
			info->aConstraintUsage[i].argvIndex = 0;
			info->aConstraintUsage[i].omit = FALSE;
		}
		info->idxPointer = NULL;
		info->orderByConsumed = FALSE;
		g_free (hash);
	}
}


/*
 * Takes as input #GValue created when calling spec->create_filtered_model_func()
 * and creates a GValue with the correct requested type
 */
static GValue *
create_value_from_sqlite3_gvalue (GType type, GValue *svalue, GError **error)
{
	GValue *value;
	gboolean allok = TRUE;
	value = g_new0 (GValue, 1);

	g_value_init (value, type);

	if (type == GDA_TYPE_NULL)
		;
	else if (type == G_TYPE_INT) {
		if (G_VALUE_TYPE (svalue) != G_TYPE_INT64)
			allok = FALSE;
		else {
			gint64 i;
			i = g_value_get_int64 (svalue);
			if ((i > G_MAXINT) || (i < G_MININT)) {
				g_set_error (error, GDA_SERVER_PROVIDER_ERROR,
					     GDA_SERVER_PROVIDER_DATA_ERROR,
					     "%s", _("Integer value is out of bounds"));
				allok = FALSE;
			}
			else
				g_value_set_int (value, (gint) i);
		}
	}
	else if (type == G_TYPE_UINT) {
		if (G_VALUE_TYPE (svalue) != G_TYPE_INT64)
			allok = FALSE;
		else {
			gint64 i;
			i = g_value_get_int64 (svalue);
			if ((i < 0) || (i > G_MAXUINT)) {
				g_set_error (error, GDA_SERVER_PROVIDER_ERROR,
					     GDA_SERVER_PROVIDER_DATA_ERROR,
					     "%s", _("Integer value is out of bounds"));
				allok = FALSE;
			}
			else
				g_value_set_uint (value, (guint) i);
		}
	}
	else if (type == G_TYPE_INT64) {
		if (G_VALUE_TYPE (svalue) != G_TYPE_INT64)
			allok = FALSE;
		else
			g_value_set_int64 (value, g_value_get_int64 (svalue));
	}
	else if (type == G_TYPE_UINT64) {
		if (G_VALUE_TYPE (svalue) != G_TYPE_INT64)
			allok = FALSE;
		else
			g_value_set_uint64 (value, (guint64) g_value_get_int64 (svalue));
	}
	else if (type == G_TYPE_DOUBLE) {
		if (G_VALUE_TYPE (svalue) != G_TYPE_DOUBLE)
			allok = FALSE;
		else
			g_value_set_double (value, g_value_get_double (svalue));
	}
	else if (type == G_TYPE_STRING) {
		if (G_VALUE_TYPE (svalue) != G_TYPE_STRING)
			allok = FALSE;
		else
			g_value_set_string (value, g_value_get_string (svalue));
	}
	else if (type == GDA_TYPE_BINARY) {
		if (G_VALUE_TYPE (svalue) != GDA_TYPE_BINARY)
			allok = FALSE;
		else
			gda_value_set_binary (value, gda_value_get_binary (svalue));
	}
	else if (type == GDA_TYPE_BLOB) {
		g_set_error (error, GDA_SERVER_PROVIDER_ERROR,
			     GDA_SERVER_PROVIDER_DATA_ERROR,
			     "%s", _("Blob constraints are not handled in virtual table condition"));
		allok = FALSE;
	}
	else if (type == G_TYPE_BOOLEAN) {
		if (G_VALUE_TYPE (svalue) != G_TYPE_INT64)
			allok = FALSE;
		else
			g_value_set_boolean (value, g_value_get_int64 (svalue) == 0 ? FALSE : TRUE);
	}
	else if (type == G_TYPE_DATE) {
		if (G_VALUE_TYPE (svalue) != G_TYPE_STRING)
			allok = FALSE;
		else {
			GDate date;
			if (!gda_parse_iso8601_date (&date, g_value_get_string (svalue))) {
				g_set_error (error, GDA_SERVER_PROVIDER_ERROR,
					     GDA_SERVER_PROVIDER_DATA_ERROR,
					     _("Invalid date '%s' (date format should be YYYY-MM-DD)"),
					     g_value_get_string (svalue));
				allok = FALSE;
			}
			else
				g_value_set_boxed (value, &date);
		}
	}
	else if (type == GDA_TYPE_TIME) {
		if (G_VALUE_TYPE (svalue) != G_TYPE_STRING)
			allok = FALSE;
		else {
			GdaTime* timegda = gda_parse_iso8601_time (g_value_get_string (svalue));
			if (!timegda) {
				g_set_error (error, GDA_SERVER_PROVIDER_ERROR,
					     GDA_SERVER_PROVIDER_DATA_ERROR,
					     _("Invalid time '%s' (time format should be HH:MM:SS[.ms])"),
					     g_value_get_string (svalue));
				allok = FALSE;
			}
			else
				gda_value_set_time (value, timegda);
			gda_time_free (timegda);
		}
	}
	else if (g_type_is_a (type, G_TYPE_DATE_TIME)) {
		if (!g_type_is_a (G_VALUE_TYPE (svalue), G_TYPE_STRING))
			allok = FALSE;
		else {
		    GTimeZone *tz = g_time_zone_new_utc ();
			GDateTime* timestamp = g_date_time_new_from_iso8601 (g_value_get_string (svalue), tz);
			g_time_zone_unref (tz);
			if (timestamp == NULL) {
				g_set_error (error, GDA_SERVER_PROVIDER_ERROR,
					     GDA_SERVER_PROVIDER_DATA_ERROR,
					     _("Invalid timestamp '%s' (format should be YYYY-MM-DDTHH:MM:SS[.ms])"),
					     g_value_get_string (svalue));
				allok = FALSE;
			}
			else {
        g_value_set_boxed (value, timestamp);
			  g_date_time_unref (timestamp);
      }
		}
	}
	else
		g_error ("Unhandled GDA type %s in SQLite recordset",
			 gda_g_type_to_string (type));

	if (! allok) {
		g_free (value);
		return NULL;
	}
	else
		return value;
}

static GdaDataModel *
dict_table_create_model_func (GdaVconnectionDataModelSpec *spec, G_GNUC_UNUSED int idxNum, const char *idxStr,
			      int argc, GValue **argv)
{
	GdaDataModel *model;
	GdaStatement *stmt = NULL;
	GdaSet *params = NULL;
	LocalSpec *lspec = (LocalSpec *) spec;

	if (idxStr) {
		gint i;
		GSList *list;
		stmt = GDA_STATEMENT ((GdaStatement*) idxStr);
		g_assert (GDA_IS_STATEMENT (stmt));
		if (! gda_statement_get_parameters (stmt, &params, NULL))
			return NULL;
		if (argc > 0) {
			g_assert (params && ((guint)argc == g_slist_length (gda_set_get_holders (params))));
			for (i = 0, list = gda_set_get_holders (params); i < argc; i++, list = list->next) {
				GdaHolder *holder = GDA_HOLDER (list->data);
				GValue *value;
				value = create_value_from_sqlite3_gvalue (gda_holder_get_g_type (holder),
									  argv [i], NULL);
				if (value)
					g_assert (gda_holder_take_value (holder, value, NULL));
				else {
					g_object_ref (params);
					return NULL;
				}
			}
		}
		g_object_ref (stmt);
	}
	else {
		GdaSqlBuilder *b;
		b = gda_sql_builder_new (GDA_SQL_STATEMENT_SELECT);
		if (lspec->ncols > 0) {
			gint i;
			for (i = 0; i < lspec->ncols; i++)
				gda_sql_builder_select_add_field (b, lspec->col_names[i], NULL, NULL);
		}
		else
			gda_sql_builder_add_field_value_id (b,
							    gda_sql_builder_add_id (b, "*"), 0);
		gda_sql_builder_select_add_target_id (b,
						      gda_sql_builder_add_id (b, g_value_get_string (lspec->table_name)),
						      NULL);
		stmt = gda_sql_builder_get_statement (b, NULL);
		g_object_unref (b);
		g_assert (stmt);
	}
	GError *lerror = NULL;
#ifdef GDA_DEBUG_NO
	gchar *sql;
	sql = gda_statement_to_sql (stmt, params, NULL);
	g_print ("Executed: [%s]\n", sql);
	g_free (sql);
#endif
	model = gda_connection_statement_execute_select_full (lspec->hc->cnc, stmt, params,
							      GDA_STATEMENT_MODEL_CURSOR_FORWARD, NULL,
							      &lerror);
	g_object_unref (stmt);
	if (params)
		g_object_unref (params);
	if (model)
		gda_data_select_compute_modification_statements (GDA_DATA_SELECT (model), NULL);
	else {
		gda_log_message ("Virtual table: data model error: %s",
			       lerror && lerror->message ? lerror->message : "no detail");
		g_clear_error (&lerror);
	}

	return model;
}
static gboolean table_add (HubConnection *hc, const GValue *table_name, GError **error);
static void table_remove (HubConnection *hc, const GValue *table_name);
static gchar *get_complete_table_name (HubConnection *hc, const GValue *table_name);

static gboolean
attach_hub_connection (GdaVconnectionHub *hub, HubConnection *hc, GError **error)
{
	gchar *tmp;
	GdaMetaStore *store;
	GdaMetaContext context;
	GdaConnectionOptions options;
	GdaVconnectionHubPrivate *priv = gda_vconnection_hub_get_instance_private (hub);

	store = gda_connection_get_meta_store (hc->cnc);
	g_assert (store);
	g_object_get ((GObject*) hc->cnc, "options", &options, NULL);
	if (! (options & GDA_CONNECTION_OPTIONS_AUTO_META_DATA)) {
		/* make sure the meta store is up to date */
		context.table_name = "_tables";
		context.size = 0;
		if (!gda_connection_update_meta_store (hc->cnc, &context, error))
			return FALSE;
	}

	/* add a :memory: database */
	if (hc->ns) {
		GdaStatement *stmt;
		tmp = g_strdup_printf ("ATTACH ':memory:' AS %s", hc->ns);
		stmt = gda_sql_parser_parse_string (priv->internal_parser, tmp, NULL, NULL);
		g_free (tmp);
		g_assert (stmt);
		if (gda_connection_statement_execute_non_select (GDA_CONNECTION (hub), stmt, NULL, NULL, error) == -1) {
			g_object_unref (stmt);
			return FALSE;
		}
		g_object_unref (stmt);
	}

	/* add virtual tables */
	GdaDataModel *model;
	gint i, nrows;
	model = gda_connection_get_meta_store_data (hc->cnc, GDA_CONNECTION_META_TABLES, error, 0);
	if (!model)
		return FALSE;

	nrows = gda_data_model_get_n_rows (model);
	for (i = 0; i < nrows; i++) {
		const GValue *cv = gda_data_model_get_value_at (model, 0, i, error);
		const GValue *cv1 = gda_data_model_get_value_at (model, 2, i, error);
		if (!cv || !cv1) {
			g_object_unref (model);
			return FALSE;
		}

		/* ignore tables which require a complete name <schema>.<name> */
		if (!gda_value_differ (cv, cv1))
			continue;

		if (!table_add (hc, cv, error)) {
			g_object_unref (model);
			return FALSE;
		}
	}
	g_object_unref (model);

	/* monitor changes */
	g_signal_connect (store, "meta-changed", G_CALLBACK (meta_changed_cb), hc);

	priv->hub_connections = g_slist_append (priv->hub_connections, hc);
	return TRUE;
}

static gchar *
get_complete_table_name (HubConnection *hc, const GValue *table_name)
{
	if (hc->ns)
		return g_strdup_printf ("%s.%s", hc->ns, g_value_get_string (table_name));
	else
		return g_strdup (g_value_get_string (table_name));
}

static void
meta_changed_cb (G_GNUC_UNUSED GdaMetaStore *store, GSList *changes, HubConnection *hc)
{
	GSList *list;
	for (list = changes; list; list = list->next) {
		GdaMetaStoreChange *ch = (GdaMetaStoreChange*) list->data;
		GValue *tsn, *tn;

		/* we are only interested in changes occurring in the "_tables" table */
		if (!strcmp (gda_meta_store_change_get_table_name (ch), "_tables")) {
			switch (gda_meta_store_change_get_change_type (ch)) {
			case GDA_META_STORE_ADD: {
				/* we only want tables where table_short_name = table_name */
				tsn = g_hash_table_lookup (gda_meta_store_change_get_keys (ch), "+6");
				tn = g_hash_table_lookup (gda_meta_store_change_get_keys (ch), "+2");
				if (tn && tsn && !gda_value_compare (tsn, tn))
					table_add (hc, tn, NULL);
				break;
			}
			case GDA_META_STORE_REMOVE: {
				/* we only want tables where table_short_name = table_name */
				tsn = g_hash_table_lookup (gda_meta_store_change_get_keys (ch), "-6");
				tn = g_hash_table_lookup (gda_meta_store_change_get_keys (ch), "-2");
				if (tn && tsn && !gda_value_compare (tsn, tn))
					table_remove (hc, tn);
				break;
			}
			case GDA_META_STORE_MODIFY: {
				/* we only want tables where table_short_name = table_name */
				tsn = g_hash_table_lookup (gda_meta_store_change_get_keys (ch), "-6");
				tn = g_hash_table_lookup (gda_meta_store_change_get_keys (ch), "-2");
				if (tn && tsn && !gda_value_compare (tsn, tn))
					table_remove (hc, tn);
				tsn = g_hash_table_lookup (gda_meta_store_change_get_keys (ch), "+6");
				tn = g_hash_table_lookup (gda_meta_store_change_get_keys (ch), "+2");
				if (tn && tsn && !gda_value_compare (tsn, tn))
					table_add (hc, tn, NULL);
				break;
			}
			}
		}
		else if (!strcmp (gda_meta_store_change_get_table_name (ch), "_columns")) {
			/* TODO */
		}
	}
}

static gboolean
table_add (HubConnection *hc, const GValue *table_name, GError **error)
{
	LocalSpec *lspec;
	gchar *tmp;

	lspec = g_new0 (LocalSpec, 1);
	GDA_VCONNECTION_DATA_MODEL_SPEC (lspec)->data_model = NULL;
	GDA_VCONNECTION_DATA_MODEL_SPEC (lspec)->create_columns_func = (GdaVconnectionDataModelCreateColumnsFunc) dict_table_create_columns_func;
	GDA_VCONNECTION_DATA_MODEL_SPEC (lspec)->create_model_func = NULL;
	GDA_VCONNECTION_DATA_MODEL_SPEC (lspec)->create_filter_func = dict_table_create_filter;
	GDA_VCONNECTION_DATA_MODEL_SPEC (lspec)->create_filtered_model_func = dict_table_create_model_func;
	lspec->table_name = gda_value_copy (table_name);
	lspec->hc = hc;
	tmp = get_complete_table_name (hc, lspec->table_name);
	/*g_print ("%s (HC=%p, table_name=%s) name=%s\n", __FUNCTION__, hc, g_value_get_string (table_name), tmp);*/
	if (!gda_vconnection_data_model_add (GDA_VCONNECTION_DATA_MODEL (hc->hub), (GdaVconnectionDataModelSpec*) lspec,
					     (GDestroyNotify) local_spec_free, tmp, error)) {
		g_free (tmp);
		return FALSE;
	}
	g_free (tmp);
	return TRUE;
}

static void
table_remove (HubConnection *hc, const GValue *table_name)
{
	gchar *name;

	name = get_complete_table_name (hc, table_name);
	/*g_print ("%s (HC=%p, table_name=%s) name=%s\n", __FUNCTION__, hc, g_value_get_string (table_name), name);*/
	gda_vconnection_data_model_remove (GDA_VCONNECTION_DATA_MODEL (hc->hub), name, NULL);
	g_free (name);
}

static void
detach_hub_connection (GdaVconnectionHub *hub, HubConnection *hc)
{
	GdaMetaStore *store;
	GdaDataModel *model;
	gint i, nrows;
	GdaVconnectionHubPrivate *priv = gda_vconnection_hub_get_instance_private (hub);

	/* un-monitor changes */
	g_object_get (G_OBJECT (hc->cnc), "meta-store", &store, NULL);
	g_assert (store);
	g_signal_handlers_disconnect_by_func (store, G_CALLBACK (meta_changed_cb), hc);

	/* remove virtual tables */
	model = gda_connection_get_meta_store_data (hc->cnc, GDA_CONNECTION_META_TABLES, NULL, 0);
	if (!model)
		return;
	nrows = gda_data_model_get_n_rows (model);
	for (i = 0; i < nrows; i++) {
		const GValue *cv = gda_data_model_get_value_at (model, 0, i, NULL);
		if (cv)
			table_remove (hc, cv);
	}
	g_object_unref (model);

	/* remove the :memory: database */
	if (hc->ns) {
		GdaStatement *stmt;
		gchar *tmp;
		tmp = g_strdup_printf ("DETACH %s", hc->ns);
		stmt = gda_sql_parser_parse_string (priv->internal_parser, tmp, NULL, NULL);
		g_free (tmp);
		g_assert (stmt);
		gda_connection_statement_execute_non_select (GDA_CONNECTION (hub), stmt, NULL, NULL, NULL);
		g_object_unref (stmt);
	}

	priv->hub_connections = g_slist_remove (priv->hub_connections, hc);
	hub_connection_free (hc);
}

static void
hub_connection_free (HubConnection *hc)
{
	g_object_unref (hc->cnc);
	g_free (hc->ns);
	g_free (hc);
}
