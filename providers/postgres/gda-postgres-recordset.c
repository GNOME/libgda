/* GDA Postgres provider
 * Copyright (C) 1998-2005 The GNOME Foundation
 *
 * AUTHORS:
 *      Michael Lausch <michael@lausch.at>
 *	Rodrigo Moya <rodrigo@gnome-db.org>
 *      Vivien Malerba <malerba@gnome-db.org>
 *      Gonzalo Paniagua Javier <gonzalo@gnome-db.org>
 *	Bas Driessen <bas.driessen@xobas.com>
 *
 * This Library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this Library; see the file COPYING.LIB.  If not,
 * write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <libgda/gda-intl.h>
#include <string.h>
#include "gda-postgres.h"
#include "gda-postgres-recordset.h"

#ifdef PARENT_TYPE
#undef PARENT_TYPE
#endif

#define PARENT_TYPE GDA_TYPE_DATA_MODEL_HASH

struct _GdaPostgresRecordsetPrivate {
	PGresult *pg_res;
	GdaConnection *cnc;
	GdaValueType *column_types;
	gchar *table_name;
	gint ncolumns;
	gint nrows;

	gint ntypes;
	GdaPostgresTypeOid *type_data;
	GHashTable *h_table;
};

static void gda_postgres_recordset_class_init (GdaPostgresRecordsetClass *klass);
static void gda_postgres_recordset_init       (GdaPostgresRecordset *recset,
					       GdaPostgresRecordsetClass *klass);
static void gda_postgres_recordset_finalize   (GObject *object);

static const GdaValue *gda_postgres_recordset_get_value_at    (GdaDataModel *model, gint col, gint row);
static GdaFieldAttributes *gda_postgres_recordset_describe    (GdaDataModel *model, gint col);
static gint gda_postgres_recordset_get_n_rows 		      (GdaDataModel *model);
static const GdaRow *gda_postgres_recordset_get_row 	      (GdaDataModel *model, gint rownum);
static const GdaRow *gda_postgres_recordset_append_row        (GdaDataModel *model, const GList *values);
static gboolean gda_postgres_recordset_remove_row 	      (GdaDataModel *model, const GdaRow *row);
static gboolean gda_postgres_recordset_update_row 	      (GdaDataModel *model, const GdaRow *row);

static GObjectClass *parent_class = NULL;

/*
 * Private functions
 */

/*
 * Object init and finalize
 */
static void
gda_postgres_recordset_init (GdaPostgresRecordset *recset,
			     GdaPostgresRecordsetClass *klass)
{
	g_return_if_fail (GDA_IS_POSTGRES_RECORDSET (recset));

	recset->priv = g_new0 (GdaPostgresRecordsetPrivate, 1);
}

static void
gda_postgres_recordset_class_init (GdaPostgresRecordsetClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GdaDataModelClass *model_class = GDA_DATA_MODEL_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = gda_postgres_recordset_finalize;
	model_class->get_n_rows = gda_postgres_recordset_get_n_rows;
	model_class->describe_column = gda_postgres_recordset_describe;
	model_class->get_value_at = gda_postgres_recordset_get_value_at;
	model_class->get_row = gda_postgres_recordset_get_row;
	model_class->append_row = gda_postgres_recordset_append_row;
	model_class->remove_row = gda_postgres_recordset_remove_row;
	model_class->update_row = gda_postgres_recordset_update_row;
}

static void
gda_postgres_recordset_finalize (GObject * object)
{
	GdaPostgresRecordset *recset = (GdaPostgresRecordset *) object;

	g_return_if_fail (GDA_IS_POSTGRES_RECORDSET (recset));

	if (recset->priv->pg_res != NULL) {
		PQclear (recset->priv->pg_res);
		recset->priv->pg_res = NULL;
	}

	g_free (recset->priv->column_types);
	recset->priv->column_types = NULL;
	g_free (recset->priv);
	recset->priv = NULL;

	parent_class->finalize (object);
}

static GdaValueType *
get_column_types (GdaPostgresRecordsetPrivate *priv)
{
	gint i;
	GdaValueType *types;

	types = g_new (GdaValueType, priv->ncolumns);
	for (i = 0; i < priv->ncolumns; i++)
		types [i] = gda_postgres_type_oid_to_gda (priv->type_data,
							  priv->ntypes, 
							  PQftype (priv->pg_res, i));

	return types;
}

static GdaRow *
get_row (GdaDataModel *model, GdaPostgresRecordsetPrivate *priv, gint rownum)
{
	gchar *thevalue;
	GdaValueType ftype;
	gboolean isNull;
	GdaValue *value;
	GdaRow *row;
	gint i;
	gchar *id;
	gint length;
	
	row = gda_row_new (model, priv->ncolumns);
	for (i = 0; i < priv->ncolumns; i++) {
		thevalue = PQgetvalue(priv->pg_res, rownum, i);
		length = PQgetlength (priv->pg_res, rownum, i);
		ftype = priv->column_types [i];
		isNull = *thevalue != '\0' ? FALSE : PQgetisnull (priv->pg_res, rownum, i);
		value = (GdaValue *) gda_row_get_value (row, i);
		gda_postgres_set_value (value, ftype, thevalue, isNull, length);
		if (gda_value_isa (value, GDA_VALUE_TYPE_BLOB)) {
			GdaBlob *blob = (GdaBlob *) gda_value_get_blob (value);
			gda_postgres_blob_set_connection (blob, priv->cnc);
		}
	}

	gda_row_set_number (row, rownum);
	id = g_strdup_printf ("%d", rownum);
   	/* Use oid or figure out primary keys ? could use libsql to add oid to every query. */
	gda_row_set_id (row, id); /* FIXME: by now, the rowid is just the row number */
	g_free (id);
	return row;
}

/*
 * Overrides
 */

static const GdaRow *
gda_postgres_recordset_get_row (GdaDataModel *model, gint row)
{
	GdaPostgresRecordset *recset = (GdaPostgresRecordset *) model;
	GdaPostgresRecordsetPrivate *priv_data;
	GdaRow *row_list;
	
	g_return_val_if_fail (GDA_IS_POSTGRES_RECORDSET (recset), NULL);
	g_return_val_if_fail (recset->priv != NULL, 0);

	row_list = (GdaRow *) GDA_DATA_MODEL_CLASS (parent_class)->get_row (model, row);
	if (row_list != NULL)
		return (const GdaRow *)row_list;

	priv_data = recset->priv;
	if (!priv_data->pg_res) {
		gda_connection_add_error_string (priv_data->cnc,
						 _("Invalid PostgreSQL handle"));
		return NULL;
	}

	if (row == priv_data->nrows)
		return NULL; /* For the last row don't add an error. */

	if (row < 0 || row > priv_data->nrows) {
		gda_connection_add_error_string (priv_data->cnc,
						_("Row number out of range"));
		return NULL;
	}

	row_list = get_row (model, priv_data, row);
	gda_data_model_hash_insert_row (GDA_DATA_MODEL_HASH (model),
					row, row_list);

	return (const GdaRow *) row_list;
}

static const GdaRow *
gda_postgres_recordset_append_row (GdaDataModel *model, const GList *values)
{
	GdaPostgresRecordset *recset = (GdaPostgresRecordset *) model;
	GdaPostgresRecordsetPrivate *priv_data;
	GString *sql;
	gint cols, i;
	GList *l;
	PGresult *pg_res;
	GdaPostgresConnectionData *cnc_priv_data;
	PGconn *pg_conn;
	const GdaRow *row;

	g_return_val_if_fail (GDA_IS_POSTGRES_RECORDSET (recset), NULL);
	g_return_val_if_fail (values != NULL, NULL);
	g_return_val_if_fail (gda_data_model_is_updatable (model), NULL);
	//g_return_val_if_fail (gda_data_model_hash_changed (model), NULL);
	g_return_val_if_fail (recset->priv != NULL, NULL);
	priv_data = recset->priv;
	pg_res = priv_data->pg_res;
	cnc_priv_data = g_object_get_data (G_OBJECT (priv_data->cnc),
					   OBJECT_DATA_POSTGRES_HANDLE);
	pg_conn = cnc_priv_data->pconn;

	/* checks if the table name has been guessed */
	if (priv_data->table_name == NULL) {
		gda_connection_add_error_string (priv_data->cnc,
						 _("Table name could not be guessed."));
		return NULL;
	}

	cols = gda_data_model_get_n_columns (model);
	if (cols != g_list_length ((GList *) values)) {
		gda_connection_add_error_string (
			priv_data->cnc,
			_("Attempt to insert a row with an invalid number of columns"));
		return NULL;
	}

	/* Prepare the SQL command */
	sql = g_string_new ("INSERT INTO ");
	g_string_append_printf (sql, "%s(",
				priv_data->table_name);
	for (i = 0; i < cols; i++) {
		GdaFieldAttributes *fa;

		fa = gda_data_model_describe_column (model, i);
		if (!fa) {
			gda_connection_add_error_string (
				priv_data->cnc,
				_("Could not retrieve column's information"));
			g_string_free (sql, TRUE);
			return NULL;
		}

		if (i != 0)
			sql = g_string_append (sql, ", ");
		sql = g_string_append (sql, "\"");
		sql = g_string_append (sql, gda_field_attributes_get_name (fa));
		sql = g_string_append (sql, "\"");
	}
	sql = g_string_append (sql, ") VALUES (");

	for (l = (GList *) values, i = 0; i < cols; i++, l = g_list_next (l)) {
		gchar *val_str;
		const GdaValue *val = (const GdaValue *) l->data;
	
		if (!val) {
			gda_connection_add_error_string (
				priv_data->cnc,
				_("Could not retrieve column's value"));
			g_string_free (sql, TRUE);
			return NULL;
		}

		if (i != 0)
			sql = g_string_append (sql, ", ");
		val_str = gda_postgres_value_to_sql_string ((GdaValue *)val);
		if (val_str == NULL) {
			gda_connection_add_error_string (
				priv_data->cnc,
				_("Could not retrieve column's value"));
			g_string_free (sql, TRUE);
			return NULL;
		}
		sql = g_string_append (sql, val_str);
		g_free (val_str);
	}
	sql = g_string_append (sql, ")");

	/* execute the SQL query */
	pg_res = PQexec (pg_conn, sql->str);
	g_string_free (sql, TRUE);

	if (pg_res != NULL) {
		/* update ok! */
		if (PQresultStatus (pg_res) != PGRES_COMMAND_OK) {
			gda_connection_add_error (priv_data->cnc,
						  gda_postgres_make_error (pg_conn, pg_res));
			PQclear (pg_res);
			return NULL;
		}
		PQclear (pg_res);
	}
	else
		gda_connection_add_error (priv_data->cnc,
					  gda_postgres_make_error (pg_conn, NULL));

	/* append row in hash table */
	row = GDA_DATA_MODEL_CLASS (parent_class)->append_row (model, values);

	return row;
}

static gboolean
gda_postgres_recordset_remove_row (GdaDataModel *model, const GdaRow *row)
{
	GdaPostgresRecordset *recset = (GdaPostgresRecordset *) model;
	GdaPostgresRecordsetPrivate *priv_data;
	gint colnum, uk, i;
	PGresult *pg_res, *pg_rm_res;
	gchar *query, *query_where, *tmp;
	GdaPostgresConnectionData *cnc_priv_data;
	PGconn *pg_conn;
	gboolean status = FALSE;
	GdaValue *value;

	g_return_val_if_fail (GDA_IS_POSTGRES_RECORDSET (recset), FALSE);
	g_return_val_if_fail (recset->priv != NULL, FALSE);
	g_return_val_if_fail (row != NULL, FALSE);

	priv_data = recset->priv;
	pg_res = priv_data->pg_res;
	cnc_priv_data = g_object_get_data (G_OBJECT (priv_data->cnc),
					   OBJECT_DATA_POSTGRES_HANDLE);
	pg_conn = cnc_priv_data->pconn;

	/* checks if the given row belongs to the given model */
	if (gda_row_get_model ((GdaRow *) row) != GDA_DATA_MODEL (model)) {
		gda_connection_add_error_string (priv_data->cnc,
						_("Given row doesn't belong to the model."));
		return FALSE;
	}

	/* checks if the table name has been guessed */
	if (priv_data->table_name == NULL) {
		gda_connection_add_error_string (priv_data->cnc,
						_("Table name could not be guessed."));
		return FALSE;
	}

	query_where = g_strdup ("WHERE TRUE ");

	for (colnum = uk = 0;
	     colnum != gda_data_model_get_n_columns (GDA_DATA_MODEL(model));
	     colnum++)
	{
		GdaFieldAttributes *attrs = gda_data_model_describe_column (GDA_DATA_MODEL(model), colnum);
		const gchar *column_name = PQfname (pg_res, colnum);
		gchar *curval = gda_value_stringify (gda_row_get_value ((GdaRow *) row, colnum));

		/* unique column: we will use it as an index */
		if (gda_field_attributes_get_primary_key (attrs) ||
			gda_field_attributes_get_unique_key (attrs))
		{
			/* fills the 'where' part of the update command */
			tmp = g_strdup_printf ("AND %s = '%s' ",
					       column_name,
					       curval);
			query_where = g_strconcat (query_where, tmp, NULL);
			g_free (tmp);
			uk++;
		}

		g_free (curval);
		gda_field_attributes_free (attrs);
	}

	if (uk == 0) {
		gda_connection_add_error_string (priv_data->cnc,
						_("Model doesn't have at least one unique key."));
	}
	else {
		/* build the delete command */
		query = g_strdup_printf ("DELETE FROM %s %s",
					 priv_data->table_name,
					 query_where);

		/* remove the row */
		pg_rm_res = PQexec (pg_conn, query);
		g_free (query);

		if (pg_rm_res != NULL) {
		/* removal ok! */
			if (PQresultStatus (pg_rm_res) == PGRES_COMMAND_OK)
				status = TRUE;
			else
				gda_connection_add_error (priv_data->cnc,
							  gda_postgres_make_error (pg_conn, pg_rm_res));
			PQclear (pg_rm_res);
		}
		else
			gda_connection_add_error (priv_data->cnc,
						  gda_postgres_make_error (pg_conn, NULL));
	}

	g_free (query_where);

	/* remove entry from data model */
	if (status == TRUE)
		status = GDA_DATA_MODEL_CLASS (parent_class)->remove_row (model, row);

	return status;
}

static gboolean
gda_postgres_recordset_update_row (GdaDataModel *model, const GdaRow *row)
{
	GdaPostgresRecordset *recset = (GdaPostgresRecordset *) model;
	GdaPostgresRecordsetPrivate *priv_data;
	gint colnum, uk, nuk;
	PGresult *pg_res, *pg_upd_res;
	gchar *query, *query_where, *query_set, *tmp;
	GdaPostgresConnectionData *cnc_priv_data;
	PGconn *pg_conn;
	gboolean status = FALSE;

	g_return_val_if_fail (GDA_IS_POSTGRES_RECORDSET (recset), FALSE);
	g_return_val_if_fail (recset->priv != NULL, FALSE);
	g_return_val_if_fail (row != NULL, FALSE);
	priv_data = recset->priv;
	pg_res = priv_data->pg_res;
	cnc_priv_data = g_object_get_data (G_OBJECT (priv_data->cnc),
					   OBJECT_DATA_POSTGRES_HANDLE);
	pg_conn = cnc_priv_data->pconn;
	
	/* checks if the given row belongs to the given model */
	if (gda_row_get_model ((GdaRow *) row) != model) {
		gda_connection_add_error_string (priv_data->cnc,
						_("Given row doesn't belong to the model."));
		return FALSE;
	}

	/* checks if the table name has been guessed */
	if (priv_data->table_name == NULL) {
		gda_connection_add_error_string (priv_data->cnc,
						_("Table name could not be guessed."));
		return FALSE;
	}

	query_where = g_strdup ("WHERE TRUE ");
	query_set = g_strdup ("SET ");

	for (colnum = uk = nuk = 0;
	     colnum != gda_data_model_get_n_columns (model);
	     colnum++) 
	{
		GdaFieldAttributes *attrs = gda_data_model_describe_column (model, colnum);
		const gchar *column_name = PQfname (pg_res, colnum);
		gchar *newval = gda_value_stringify (gda_row_get_value ((GdaRow *) row, colnum));
		const gchar *oldval = PQgetvalue (pg_res, gda_row_get_number ((GdaRow *) row), colnum);

		/* unique column: we won't update it, but we will use it as
		   an index */
		if (gda_field_attributes_get_primary_key (attrs) ||
		    gda_field_attributes_get_unique_key (attrs)) 
		{
			/* checks if it hasn't be modified anyway */
			if (oldval == NULL ||
	   		    newval == NULL ||
			    strcmp (oldval, newval) != 0)
			    	continue;

			/* fills the 'where' part of the update command */
			tmp = g_strdup_printf ("AND %s = '%s' ",
					       column_name,
					       newval);
			query_where = g_strconcat (query_where, tmp, NULL);
			g_free (tmp);
			uk++;
		}
		/* non-unique column: update it */
		else {
			/* fills the 'set' part of the update command */
			tmp = g_strdup_printf ("\"%s\" = '%s', ", 
					       column_name,
					       newval);
			query_set = g_strconcat (query_set, tmp, NULL);
			g_free (tmp);
			nuk++;
		}

		g_free (newval);
		gda_field_attributes_free (attrs);
	}

	if (uk == 0) {
		gda_connection_add_error_string (priv_data->cnc,
						_("Model doesn't have at least one non-modified unique key."));
	}
	else if (nuk == 0) {
		gda_connection_add_error_string (priv_data->cnc,
						_("Model doesn't have any non-unique values to update."));
	}
	else {
		/* remove the last ',' in the SET part */
		tmp = strrchr (query_set, ',');
		if (tmp != NULL)
			*tmp = ' ';
		
		/* build the update command */
		query = g_strdup_printf ("UPDATE %s %s %s", 
					 priv_data->table_name,
					 query_set,
					 query_where);
	
		/* update the row */
		pg_upd_res = PQexec (pg_conn, query);
		g_free (query);
	
		if (pg_upd_res != NULL) {
			/* update ok! */
			if (PQresultStatus (pg_upd_res) == PGRES_COMMAND_OK)
				status = TRUE;
			else
				gda_connection_add_error (priv_data->cnc,
			        	                  gda_postgres_make_error (pg_conn, pg_upd_res));
			PQclear (pg_upd_res);
		}
		else
			gda_connection_add_error (priv_data->cnc,
			       	                  gda_postgres_make_error (pg_conn, NULL));
	}
	
	g_free (query_set);
	g_free (query_where);

	/* emit update signals */
	gda_data_model_row_updated (GDA_DATA_MODEL (model), gda_row_get_number ((GdaRow *) row));
	gda_data_model_changed (GDA_DATA_MODEL (model));

	return status;
}

static const GdaValue *
gda_postgres_recordset_get_value_at (GdaDataModel *model, gint col, gint row)
{
	GdaPostgresRecordset *recset = (GdaPostgresRecordset *) model;
	GdaPostgresRecordsetPrivate *priv_data;
	PGresult *pg_res;
	GdaRow *row_list;
	const GdaValue *value;

	g_return_val_if_fail (GDA_IS_POSTGRES_RECORDSET (recset), NULL);
	g_return_val_if_fail (recset->priv != NULL, 0);
	
	value = gda_data_model_hash_get_value_at (model, col, row);
	if (value != NULL)
		return value;

	priv_data = recset->priv;
	pg_res = priv_data->pg_res;
	if (!pg_res) {
		gda_connection_add_error_string (priv_data->cnc,
						 _("Invalid PostgreSQL handle"));
		return NULL;
	}

	if (row == priv_data->nrows)
		return NULL; /* For the last row don't add an error. */

	if (row < 0 || row > priv_data->nrows) {
		gda_connection_add_error_string (priv_data->cnc,
						_("Row number out of range"));
		return NULL;
	}

	if (col >= priv_data->ncolumns) {
		gda_connection_add_error_string (priv_data->cnc,
						_("Column number out of range"));
		return NULL;
	}

	row_list = get_row (model, priv_data, row);
	gda_data_model_hash_insert_row (GDA_DATA_MODEL_HASH (model),
					 row, row_list);
	return gda_row_get_value (row_list, col);
}

/* Try to guess the table name involved in the given data model. 
 * It can fail on complicated queries, where several tables are
 * involved in the same time.
 */
static gchar *guess_table_name (GdaPostgresRecordset *recset)
{
	GdaPostgresConnectionData *cnc_priv_data;
	PGresult *pg_res, *pg_name_res;
	PGconn *pg_conn;
	gchar *table_name = NULL;
	
   
   	/* This code should be changed to use libsql, its proberly faster
	 * in long run to store a parsed sql statement and then
	 * just grab the tablename from that. */
	pg_res = recset->priv->pg_res;
	cnc_priv_data = g_object_get_data (G_OBJECT (recset->priv->cnc),
					   OBJECT_DATA_POSTGRES_HANDLE);
	pg_conn = cnc_priv_data->pconn;

	if (PQnfields (pg_res) > 0) {
		gchar *query = g_strdup_printf ("SELECT c.relname FROM pg_catalog.pg_class c WHERE c.relkind = 'r' AND c.relnatts = '%d'", PQnfields (pg_res));
		gint i;
		for (i = 0; i < PQnfields (pg_res); i++) {
			const gchar *column_name = PQfname (pg_res, i);
			gchar *cond = g_strdup_printf (" AND '%s' IN (SELECT a.attname FROM pg_catalog.pg_attribute a WHERE a.attrelid = c.oid)", column_name);
			query = g_strconcat (query, cond, NULL);
			g_free (cond);
		}
		pg_name_res = PQexec (pg_conn, query);
		if (pg_name_res != NULL) {
			if (PQntuples (pg_name_res) == 1)
				table_name = g_strdup (PQgetvalue (pg_name_res, 0, 0));
			PQclear (pg_name_res);
		}
		g_free (query);
	}
	return table_name;
}

/* Checks if the given column number of the given data model
 * has the given constraint.
 *
 * contype may be one of the following:
 *    'f': foreign key
 *    'p': primary key
 *    'u': unique key
 *    etc...
 */
static gboolean check_constraint (const GdaDataModel *model,
				  const gchar *table_name,
				  const gint col,
				  const gchar contype)
{
	GdaPostgresRecordset *recset = (GdaPostgresRecordset *) model;
	GdaPostgresRecordsetPrivate *priv_data;
	GdaPostgresConnectionData *cnc_priv_data;
	PGresult *pg_res, *pg_con_res;
	gchar *column_name;
	gchar *query;
	gboolean state = FALSE;

	g_return_val_if_fail (table_name != NULL, FALSE);
	
	priv_data = recset->priv;
	pg_res = priv_data->pg_res;
	cnc_priv_data = g_object_get_data (G_OBJECT (priv_data->cnc),
					   OBJECT_DATA_POSTGRES_HANDLE);

	column_name = PQfname (pg_res, col);
	if (column_name != NULL) {
		query = g_strdup_printf ("SELECT 1 FROM pg_catalog.pg_class c, pg_catalog.pg_constraint c2, pg_catalog.pg_attribute a WHERE c.relname = '%s' AND c.oid = c2.conrelid and a.attrelid = c.oid and c2.contype = '%c' and c2.conkey[1] = a.attnum and a.attname = '%s'", table_name, contype, column_name);
		pg_con_res = PQexec (cnc_priv_data->pconn, query);
		if (pg_con_res != NULL) {
			state = PQntuples (pg_con_res) == 1;
			PQclear (pg_con_res);
		}
		g_free (query);
	}

	return state;
}

static GdaFieldAttributes *
gda_postgres_recordset_describe (GdaDataModel *model, gint col)
{
	GdaPostgresRecordset *recset = (GdaPostgresRecordset *) model;
	GdaPostgresRecordsetPrivate *priv_data;
	PGresult *pg_res;
	GdaValueType ftype;
	gint scale;
	GdaFieldAttributes *field_attrs;
	gboolean ispk = FALSE, isuk = FALSE;

	g_return_val_if_fail (GDA_IS_POSTGRES_RECORDSET (recset), NULL);
	g_return_val_if_fail (recset->priv != NULL, 0);

	priv_data = recset->priv;
	pg_res = priv_data->pg_res;
	if (!pg_res) {
		gda_connection_add_error_string (priv_data->cnc,
						_("Invalid PostgreSQL handle"));
		return NULL;
	}

	if (col >= priv_data->ncolumns) {
		gda_connection_add_error_string (priv_data->cnc,
						_("Column out of range"));
		return NULL;
	}

	field_attrs = gda_field_attributes_new ();
	gda_field_attributes_set_name (field_attrs, PQfname (pg_res, col));

	ftype = gda_postgres_type_oid_to_gda (priv_data->type_data,
					      priv_data->ntypes, 
					      PQftype (pg_res, col));

	scale = (ftype == GDA_VALUE_TYPE_DOUBLE) ? DBL_DIG :
		(ftype == GDA_VALUE_TYPE_SINGLE) ? FLT_DIG : 0;

	gda_field_attributes_set_scale (field_attrs, scale);
	gda_field_attributes_set_gdatype (field_attrs, ftype);

	/* PQfsize() == -1 => variable length */
	gda_field_attributes_set_defined_size (field_attrs,
					       PQfsize (pg_res, col));
					       
	gda_field_attributes_set_references (field_attrs, "");

	gda_field_attributes_set_table (field_attrs,
					priv_data->table_name);

	if (priv_data->table_name) {
		ispk = check_constraint (model,
					 priv_data->table_name,
					 col, 
					 'p');
	
		isuk = check_constraint (model, 
					 priv_data->table_name,
					 col, 
					 'u');
	}
	
	gda_field_attributes_set_primary_key (field_attrs, ispk);
	gda_field_attributes_set_unique_key (field_attrs, isuk);
	/* FIXME: set_allow_null? */

	return field_attrs;
}

static gint
gda_postgres_recordset_get_n_rows (GdaDataModel *model)
{
	gint parent_row_num;
	GdaPostgresRecordset *recset = (GdaPostgresRecordset *) model;

	g_return_val_if_fail (GDA_IS_POSTGRES_RECORDSET (model), 0);
	g_return_val_if_fail (recset->priv != NULL, 0);

	parent_row_num = GDA_DATA_MODEL_CLASS (parent_class)->get_n_rows (model);

	/* if not initialized return number of PQ Tuples */
	if (parent_row_num < 0)
		return recset->priv->nrows;
	else
		return parent_row_num;
}

/*
 * Public functions
 */

GType
gda_postgres_recordset_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (GdaPostgresRecordsetClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_postgres_recordset_class_init,
			NULL,
			NULL,
			sizeof (GdaPostgresRecordset),
			0,
			(GInstanceInitFunc) gda_postgres_recordset_init
		};
		type = g_type_register_static (PARENT_TYPE, "GdaPostgresRecordset", &info, 0);
	}
	return type;
}

GdaDataModel *
gda_postgres_recordset_new (GdaConnection *cnc, PGresult *pg_res)
{
	GdaPostgresRecordset *model;
	GdaPostgresConnectionData *cnc_priv_data;
	gchar *cmd_tuples;
	gchar *endptr [1];

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (pg_res != NULL, NULL);

	cnc_priv_data = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_POSTGRES_HANDLE);

	model = g_object_new (GDA_TYPE_POSTGRES_RECORDSET, NULL);
	model->priv->pg_res = pg_res;
	model->priv->cnc = cnc;
	model->priv->ntypes = cnc_priv_data->ntypes;
	model->priv->type_data = cnc_priv_data->type_data;
	model->priv->h_table = cnc_priv_data->h_table;
	model->priv->ncolumns = PQnfields (pg_res);
	cmd_tuples = PQcmdTuples (pg_res);
	if (cmd_tuples == NULL || *cmd_tuples == '\0'){
		model->priv->nrows = PQntuples (pg_res);
	} else {
		model->priv->nrows = strtol (cmd_tuples, endptr, 10);
		if (**endptr != '\0')
			g_warning (_("Tuples:\"%s\""), cmd_tuples);

	}
	model->priv->column_types = get_column_types (model->priv);
	gda_data_model_hash_set_n_columns (GDA_DATA_MODEL_HASH (model),
					    model->priv->ncolumns);
	model->priv->table_name = guess_table_name (model);
	return GDA_DATA_MODEL (model);
}

PGresult *
gda_postgres_recordset_get_pgresult (GdaPostgresRecordset *recset)
{
	g_return_val_if_fail (GDA_IS_POSTGRES_RECORDSET (recset), NULL);

	return recset->priv->pg_res;
}
