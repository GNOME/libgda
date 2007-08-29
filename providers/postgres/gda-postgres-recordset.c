/* GDA Postgres provider
 * Copyright (C) 1998 - 2007 The GNOME Foundation
 *
 * AUTHORS:
 *      Michael Lausch <michael@lausch.at>
 *	Rodrigo Moya <rodrigo@gnome-db.org>
 *      Vivien Malerba <malerba@gnome-db.org>
 *      Gonzalo Paniagua Javier <gonzalo@gnome-db.org>
 *      Bas Driessen <bas.driessen@xobas.com>
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

#include <glib/gi18n-lib.h>
#include <libgda/gda-data-model.h>
#include <libgda/gda-data-model-private.h>
#include <string.h>
#include "gda-postgres.h"
#include "gda-postgres-recordset.h"

#ifdef PARENT_TYPE
#undef PARENT_TYPE
#endif

#define PARENT_TYPE GDA_TYPE_DATA_MODEL_HASH

struct _GdaPostgresRecordsetPrivate {
	GdaConnection *cnc;
	PGresult      *pg_res;

	GType         *column_types;
	gint           ncolumns;
	gint           nrows;

	gchar         *table_name;
};

static void gda_postgres_recordset_class_init (GdaPostgresRecordsetClass *klass);
static void gda_postgres_recordset_init       (GdaPostgresRecordset *recset,
					       GdaPostgresRecordsetClass *klass);
static void gda_postgres_recordset_finalize   (GObject *object);

static const GValue *gda_postgres_recordset_get_value_at    (GdaDataModelRow *model, gint col, gint row);
static gint gda_postgres_recordset_get_n_rows 		      (GdaDataModelRow *model);
static GdaRow *gda_postgres_recordset_get_row 	      (GdaDataModelRow *model, gint rownum, GError **error);
static gboolean gda_postgres_recordset_append_row	      (GdaDataModelRow *model, GdaRow *row, GError **error);
static gboolean gda_postgres_recordset_remove_row 	      (GdaDataModelRow *model, GdaRow *row, GError **error);
static gboolean gda_postgres_recordset_update_row 	      (GdaDataModelRow *model, GdaRow *row, GError **error);

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
	GdaDataModelRowClass *model_class = GDA_DATA_MODEL_ROW_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = gda_postgres_recordset_finalize;
	model_class->get_n_rows = gda_postgres_recordset_get_n_rows;
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

static GdaRow *
get_row (GdaDataModel *model, GdaPostgresRecordsetPrivate *priv, gint rownum, GError **error)
{
	gchar *thevalue;
	GType ftype;
	gboolean isNull;
	GValue *value;
	GdaRow *row;
	gint i;
	gchar *id;
	gint length;
	
	row = gda_row_new (model, priv->ncolumns);
	for (i = 0; i < priv->ncolumns; i++) {
		thevalue = PQgetvalue (priv->pg_res, rownum, i);
		length = PQgetlength (priv->pg_res, rownum, i);
		ftype = priv->column_types [i];
		isNull = thevalue && *thevalue != '\0' ? FALSE : PQgetisnull (priv->pg_res, rownum, i);
		value = (GValue *) gda_row_get_value (row, i);
		gda_postgres_set_value (priv->cnc, value, ftype, thevalue, isNull, length);
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

static GdaRow *
gda_postgres_recordset_get_row (GdaDataModelRow *model, gint row, GError **error)
{
	GdaPostgresRecordset *recset = (GdaPostgresRecordset *) model;
	GdaPostgresRecordsetPrivate *priv_data;
	GdaRow *row_list;
	
	g_return_val_if_fail (GDA_IS_POSTGRES_RECORDSET (recset), NULL);
	g_return_val_if_fail (recset->priv != NULL, 0);

	row_list = (GdaRow *) GDA_DATA_MODEL_ROW_CLASS (parent_class)->get_row (model, row, 
										error);
	if (row_list != NULL)
		return row_list;

	priv_data = recset->priv;
	if (!priv_data->pg_res) {
		gda_connection_add_event_string (priv_data->cnc,
						 _("Invalid PostgreSQL handle"));
		return NULL;
	}

	if (row == priv_data->nrows)
		return NULL; /* For the last row don't add an error. */

	if (row < 0 || row > priv_data->nrows) {
		gda_connection_add_event_string (priv_data->cnc,
						_("Row number out of range"));
		return NULL;
	}

	row_list = get_row (GDA_DATA_MODEL (model), priv_data, row, error);
	gda_data_model_hash_insert_row (GDA_DATA_MODEL_HASH (model),
					row, row_list);

	return row_list;
}

static gboolean
gda_postgres_recordset_append_row (GdaDataModelRow *model, GdaRow *row, GError **error)
{
	GdaPostgresRecordset *recset = (GdaPostgresRecordset *) model;
	GdaPostgresRecordsetPrivate *priv_data;
	GString *sql, *sql_value;
	gchar *cur_val;
	gint i;
	PGresult *pg_res;
	GdaPostgresConnectionData *cnc_priv_data;
	PGconn *pg_conn;

	g_return_val_if_fail (GDA_IS_POSTGRES_RECORDSET (recset), FALSE);
	g_return_val_if_fail (row != NULL, FALSE);
	g_return_val_if_fail (gda_data_model_is_updatable (GDA_DATA_MODEL (model)), FALSE);
	g_return_val_if_fail (recset->priv != NULL, FALSE);
	priv_data = recset->priv;
	pg_res = priv_data->pg_res;
	cnc_priv_data = g_object_get_data (G_OBJECT (priv_data->cnc),
					   OBJECT_DATA_POSTGRES_HANDLE);
	pg_conn = cnc_priv_data->pconn;

	/* checks if the table name has been guessed */
	if (priv_data->table_name == NULL) {
		gda_connection_add_event_string (priv_data->cnc,
						 _("Table name could not be guessed."));
		return FALSE;
	}

	/* checks for correct number of columns */
        if (priv_data->ncolumns != gda_row_get_length (row)) {
                gda_connection_add_event_string (priv_data->cnc,
                                                _("Attempt to insert a row with an invalid number of columns"));
                return FALSE;
        }

	/* Prepare the SQL command */
	sql = g_string_new ("INSERT INTO ");
	g_string_append_printf (sql, "%s (",  priv_data->table_name);

	sql_value = g_string_new ("VALUES (");

	for (i = 0; i < priv_data->ncolumns; i++) {

		if (i != 0)
		{
			sql = g_string_append (sql, ", ");
			sql_value = g_string_append (sql_value, ", ");
		}

		sql = g_string_append (sql, "\"");
		sql = g_string_append (sql, PQfname (priv_data->pg_res, i));
		sql = g_string_append (sql, "\"");

		cur_val = gda_value_stringify (gda_row_get_value ((GdaRow *) row, i));
		sql_value = g_string_append (sql_value, "'");
		sql_value = g_string_append (sql_value, cur_val);
		sql_value = g_string_append (sql_value, "'");
		g_free (cur_val);
	}

	/* concatenate SQL statement */
        sql = g_string_append (sql, ") ");
	sql = g_string_append (sql, sql_value->str);
	sql = g_string_append (sql, ")");

	/* execute the SQL query */
	pg_res = gda_postgres_PQexec_wrap (priv_data->cnc, pg_conn, sql->str);

	g_string_free (sql, TRUE);
        g_string_free (sql_value, TRUE);

	if (pg_res != NULL) {
		/* update ok! */
		if (PQresultStatus (pg_res) != PGRES_COMMAND_OK) {
			gda_postgres_make_error (priv_data->cnc, pg_conn, pg_res);
			PQclear (pg_res);
			return FALSE;
		}
		PQclear (pg_res);
	}
	else
		gda_postgres_make_error (priv_data->cnc, pg_conn, NULL);

	/* append row in hash table */
	if (! GDA_DATA_MODEL_ROW_CLASS (parent_class)->append_row (model, row, error)) {
		gda_postgres_make_error (priv_data->cnc, pg_conn, pg_res);
		return FALSE;
	}

	return TRUE;
}

static gboolean
gda_postgres_recordset_remove_row (GdaDataModelRow *model, GdaRow *row, GError **error)
{
	GdaPostgresRecordset *recset = (GdaPostgresRecordset *) model;
	GdaPostgresRecordsetPrivate *priv_data;
	gint colnum, uk;
	PGresult *pg_res, *pg_rm_res;
	gchar *query, *query_where, *tmp;
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
	if (gda_row_get_model ((GdaRow *) row) != GDA_DATA_MODEL (model)) {
		gda_connection_add_event_string (priv_data->cnc,
						_("Given row doesn't belong to the model."));
		return FALSE;
	}

	/* checks if the table name has been guessed */
	if (priv_data->table_name == NULL) {
		gda_connection_add_event_string (priv_data->cnc,
						_("Table name could not be guessed."));
		return FALSE;
	}

	query_where = g_strdup ("WHERE TRUE ");

	for (colnum = uk = 0;
	     colnum != gda_data_model_get_n_columns (GDA_DATA_MODEL(model));
	     colnum++)
	{
		GdaColumn *attrs = gda_data_model_describe_column (GDA_DATA_MODEL(model), colnum);
		const gchar *column_name = PQfname (pg_res, colnum);
		gchar *curval = gda_value_stringify (gda_row_get_value ((GdaRow *) row, colnum));

		/* unique column: we will use it as an index */
		if (gda_column_get_primary_key (attrs) ||
		    gda_column_get_unique_key (attrs))
		{
			/* fills the 'where' part of the update command */
			tmp = g_strdup_printf ("AND \"%s\" = '%s' ",
					       column_name,
					       curval);
			query_where = g_strconcat (query_where, tmp, NULL);
			g_free (tmp);
			uk++;
		}

		g_free (curval);
	}

	if (uk == 0) {
		gda_connection_add_event_string (priv_data->cnc,
						_("Model doesn't have at least one unique key."));
	}
	else {
		/* build the delete command */
		query = g_strdup_printf ("DELETE FROM %s %s",
					 priv_data->table_name,
					 query_where);

		/* remove the row */
		pg_rm_res = gda_postgres_PQexec_wrap (priv_data->cnc, pg_conn, query);
		g_free (query);

		if (pg_rm_res != NULL) {
			/* removal ok! */
			if (PQresultStatus (pg_rm_res) == PGRES_COMMAND_OK)
				status = TRUE;
			else
				gda_postgres_make_error (priv_data->cnc, pg_conn, pg_rm_res);
			PQclear (pg_rm_res);
		}
		else
			gda_postgres_make_error (priv_data->cnc, pg_conn, NULL);
	}

	g_free (query_where);

	/* remove entry from data model */
	if (status == TRUE)
		status = GDA_DATA_MODEL_ROW_CLASS (parent_class)->remove_row (model, row, error);

	return status;
}

static gboolean
gda_postgres_recordset_update_row (GdaDataModelRow *model, GdaRow *row, GError **error)
{
	GdaPostgresRecordset *recset = (GdaPostgresRecordset *) model;
	GdaPostgresRecordsetPrivate *priv_data;
	gint colnum, rownum, uk, nuk;
	PGresult *pg_res, *pg_upd_res;
	gchar *query, *query_where, *query_set, *tmp;
	gchar *oldval, *newval;
	const gchar *column_name;
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
	if (gda_row_get_model ((GdaRow *) row) != GDA_DATA_MODEL (model)) {
		gda_connection_add_event_string (priv_data->cnc,
						_("Given row doesn't belong to the model."));
		return FALSE;
	}

	/* checks if the table name has been guessed */
	if (priv_data->table_name == NULL) {
		gda_connection_add_event_string (priv_data->cnc,
						_("Table name could not be guessed."));
		return FALSE;
	}

	rownum = gda_row_get_number ((GdaRow *) row);

	query_where = g_strdup ("WHERE TRUE ");
	query_set = g_strdup ("SET ");

	for (colnum = uk = nuk = 0;
	     colnum != gda_data_model_get_n_columns (GDA_DATA_MODEL (model));
	     colnum++) 
	{
		GdaColumn *attrs = gda_data_model_describe_column (GDA_DATA_MODEL (model), colnum);
		column_name = PQfname (pg_res, colnum);
		newval = gda_value_stringify (gda_row_get_value ((GdaRow *) row, colnum));

		/* for data from mysql result we can retrieve original values to avoid
		   unique columns to be updated */
		if (rownum < priv_data->nrows)
			oldval = PQgetvalue (pg_res, gda_row_get_number ((GdaRow *) row), colnum);
		else
			oldval = newval;

		/* unique column: we won't update it, but we will use it as
		   an index */
		if (gda_column_get_primary_key (attrs) ||
		    gda_column_get_unique_key (attrs)) 
		{
			/* checks if it hasn't be modified anyway */
			if (oldval == NULL ||
	   		    newval == NULL ||
			    strcmp (oldval, newval) != 0)
			    	continue;

			/* fills the 'where' part of the update command */
			tmp = g_strdup_printf ("AND \"%s\" = '%s' ",
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
	}

	if (uk == 0) {
		gda_connection_add_event_string (priv_data->cnc,
						_("Model doesn't have at least one non-modified unique key."));
	}
	else if (nuk == 0) {
		gda_connection_add_event_string (priv_data->cnc,
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
		pg_upd_res = gda_postgres_PQexec_wrap (priv_data->cnc, pg_conn, query);
		g_free (query);
	
		if (pg_upd_res != NULL) {
			/* update ok! */
			if (PQresultStatus (pg_upd_res) == PGRES_COMMAND_OK)
				status = TRUE;
			else
				gda_postgres_make_error (priv_data->cnc, pg_conn, pg_upd_res);
			PQclear (pg_upd_res);
		}
		else
			gda_postgres_make_error (priv_data->cnc, pg_conn, NULL);
	}
	
	g_free (query_set);
	g_free (query_where);

	/* emit update signals */
	gda_data_model_row_updated (GDA_DATA_MODEL (model), gda_row_get_number ((GdaRow *) row));

	return status;
}

static const GValue *
gda_postgres_recordset_get_value_at (GdaDataModelRow *model, gint col, gint row)
{
	GdaPostgresRecordset *recset = (GdaPostgresRecordset *) model;
	GdaPostgresRecordsetPrivate *priv_data;
	PGresult *pg_res;
	GdaRow *row_list;
	const GValue *value;

	g_return_val_if_fail (GDA_IS_POSTGRES_RECORDSET (recset), NULL);
	g_return_val_if_fail (recset->priv != NULL, 0);
	
	value = GDA_DATA_MODEL_ROW_CLASS (parent_class)->get_value_at (model, col, row);
	if (value != NULL)
		return value;

	priv_data = recset->priv;
	pg_res = priv_data->pg_res;
	if (!pg_res) {
		gda_connection_add_event_string (priv_data->cnc,
						 _("Invalid PostgreSQL handle"));
		return NULL;
	}

	if (row == priv_data->nrows)
		return NULL; /* For the last row don't add an error. */

	if (row < 0 || row > priv_data->nrows) {
		gda_connection_add_event_string (priv_data->cnc,
						_("Row number out of range"));
		return NULL;
	}

	if (col >= priv_data->ncolumns) {
		gda_connection_add_event_string (priv_data->cnc,
						_("Column number out of range"));
		return NULL;
	}
	
	row_list = get_row (GDA_DATA_MODEL (model), priv_data, row, NULL);
	gda_data_model_hash_insert_row (GDA_DATA_MODEL_HASH (model),
					 row, row_list);
	return gda_row_get_value (row_list, col);
}

static gint
gda_postgres_recordset_get_n_rows (GdaDataModelRow *model)
{
	gint parent_row_num;
	GdaPostgresRecordset *recset = (GdaPostgresRecordset *) model;

	g_return_val_if_fail (GDA_IS_POSTGRES_RECORDSET (model), 0);
	g_return_val_if_fail (recset->priv != NULL, 0);

	parent_row_num = GDA_DATA_MODEL_ROW_CLASS (parent_class)->get_n_rows (model);

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

	if (G_UNLIKELY (type == 0)) {
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
	gint i;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (pg_res != NULL, NULL);

	cnc_priv_data = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_POSTGRES_HANDLE);

	model = g_object_new (GDA_TYPE_POSTGRES_RECORDSET, NULL);
	model->priv->pg_res = pg_res;
	model->priv->cnc = cnc;
	model->priv->ncolumns = PQnfields (pg_res);
	cmd_tuples = PQcmdTuples (pg_res);
	if (cmd_tuples == NULL || *cmd_tuples == '\0')
		model->priv->nrows = PQntuples (pg_res);
	else {
		model->priv->nrows = strtol (cmd_tuples, endptr, 10);
		if (**endptr != '\0')
			g_warning (_("Tuples:\"%s\""), cmd_tuples);
		
	}
	model->priv->column_types = gda_postgres_get_column_types (pg_res, 
								   cnc_priv_data->type_data, 
								   cnc_priv_data->ntypes);
	gda_data_model_hash_set_n_columns (GDA_DATA_MODEL_HASH (model),
					   model->priv->ncolumns);
	model->priv->table_name = gda_postgres_guess_table_name (cnc, pg_res);

	/* set GdaColumn attributes */
	for (i = 0; i < model->priv->ncolumns; i++)
		gda_postgres_recordset_describe_column (GDA_DATA_MODEL (model), cnc, pg_res, 
							cnc_priv_data->type_data, 
							cnc_priv_data->ntypes,
							model->priv->table_name, i);

	return GDA_DATA_MODEL (model);
}

PGresult *
gda_postgres_recordset_get_pgresult (GdaPostgresRecordset *recset)
{
	g_return_val_if_fail (GDA_IS_POSTGRES_RECORDSET (recset), NULL);

	return recset->priv->pg_res;
}
