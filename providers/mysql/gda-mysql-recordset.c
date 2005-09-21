/* GDA MySQL provider
 * Copyright (C) 1998 - 2005 The GNOME Foundation.
 *
 * AUTHORS:
 *      Michael Lausch <michael@lausch.at>
 *	Rodrigo Moya <rodrigo@gnome-db.org>
 *      Vivien Malerba <malerba@gnome-db.org>
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
#include <stdlib.h>
#include <string.h>
#include "gda-mysql.h"
#include "gda-mysql-recordset.h"
#include <libgda/gda-data-model-private.h>

#define PARENT_TYPE GDA_TYPE_DATA_MODEL_ARRAY

struct _GdaMysqlRecordsetPrivate {
	MYSQL_RES *mysql_res;
	gint mysql_res_rows;
	GdaConnection *cnc;
	gint ncolumns;
	gchar *table_name;
	gboolean row_sync;
};

static void gda_mysql_recordset_class_init (GdaMysqlRecordsetClass *klass);
static void gda_mysql_recordset_init       (GdaMysqlRecordset *recset,
					    GdaMysqlRecordsetClass *klass);
static void gda_mysql_recordset_finalize   (GObject *object);

static gint gda_mysql_recordset_get_n_rows			     (GdaDataModelBase *model);
static GdaColumn *gda_mysql_recordset_describe_column (GdaDataModelBase *model, gint col);
static const GdaRow *gda_mysql_recordset_get_row		     (GdaDataModelBase *model, gint row);
static const GdaValue *gda_mysql_recordset_get_value_at		     (GdaDataModelBase *model, gint col, gint row);
static gboolean gda_mysql_recordset_is_updatable		     (GdaDataModelBase *model);
static gboolean gda_mysql_recordset_append_row			     (GdaDataModelBase *model, GdaRow *row);
static gboolean gda_mysql_recordset_remove_row			     (GdaDataModelBase *model, const GdaRow *row);
static gboolean gda_mysql_recordset_update_row			     (GdaDataModelBase *model, const GdaRow *row);

static GObjectClass *parent_class = NULL;

/*
 * Private functions
 */

/*
 * Object init and finalize
 */
static void
gda_mysql_recordset_class_init (GdaMysqlRecordsetClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GdaDataModelBaseClass *model_class = GDA_DATA_MODEL_BASE_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = gda_mysql_recordset_finalize;
	model_class->get_n_rows = gda_mysql_recordset_get_n_rows;
	model_class->get_row = gda_mysql_recordset_get_row;
	model_class->get_value_at = gda_mysql_recordset_get_value_at;
	model_class->is_updatable = gda_mysql_recordset_is_updatable;
	model_class->append_row = gda_mysql_recordset_append_row;
	model_class->remove_row = gda_mysql_recordset_remove_row;
	model_class->update_row = gda_mysql_recordset_update_row;
}

static void
gda_mysql_recordset_init (GdaMysqlRecordset *recset, GdaMysqlRecordsetClass *klass)
{
	g_return_if_fail (GDA_IS_MYSQL_RECORDSET (recset));

	recset->priv = g_new0 (GdaMysqlRecordsetPrivate, 1);
}

static void
gda_mysql_recordset_finalize (GObject *object)
{
	GdaMysqlRecordset *recset = (GdaMysqlRecordset *) object;

	g_return_if_fail (GDA_IS_MYSQL_RECORDSET (recset));

	if (recset->priv->mysql_res != NULL) {
		mysql_free_result (recset->priv->mysql_res);
		recset->priv->mysql_res = NULL;
	}

	g_free (recset->priv->table_name);
	recset->priv->table_name = NULL;

	g_free (recset->priv);
	recset->priv = NULL;

	parent_class->finalize (object);
}

static void
fill_gda_value (GdaValue *gda_value, enum enum_field_types type, gchar *value,
		unsigned long length, gboolean is_unsigned)
{
	if (!value) {
		gda_value_set_null (gda_value);
		return;
	}

	switch (type) {
	case FIELD_TYPE_DECIMAL :
	case FIELD_TYPE_DOUBLE :
		gda_value_set_double (gda_value, atof (value));
		break;
	case FIELD_TYPE_FLOAT :
		gda_value_set_single (gda_value, atof (value));
		break;
	case FIELD_TYPE_LONG :
		if (is_unsigned) {
			gda_value_set_uinteger (gda_value, strtoul (value, NULL, 0));
			break;
		}
	case FIELD_TYPE_YEAR :
		gda_value_set_integer (gda_value, atol (value));
		break;
	case FIELD_TYPE_LONGLONG :
	case FIELD_TYPE_INT24 :
		if (is_unsigned) {
			gda_value_set_biguint (gda_value, strtoull (value, NULL, 0));
		} else {
			gda_value_set_bigint (gda_value, atoll (value));
		}
		break;
	case FIELD_TYPE_SHORT :
		if (is_unsigned) {
			gda_value_set_smalluint (gda_value, atoi (value));
		} else {
			gda_value_set_smallint (gda_value, atoi (value));
		}
		break;
	case FIELD_TYPE_TINY :
		if (is_unsigned) {
			gda_value_set_tinyuint (gda_value, atoi (value));
		} else {
			gda_value_set_tinyint (gda_value, atoi (value));
		}
		break;
	case FIELD_TYPE_TINY_BLOB :
	case FIELD_TYPE_MEDIUM_BLOB :
	case FIELD_TYPE_LONG_BLOB :
	case FIELD_TYPE_BLOB : {
		GdaBinary bin;
		bin.data = value;
		bin.binary_length = length;
		gda_value_set_binary (gda_value, &bin);
		break;
	}
	case FIELD_TYPE_VAR_STRING :
	case FIELD_TYPE_STRING :
		/* FIXME: we might get "[VAR]CHAR(20) BINARY" type with \0 inside
		   We should check for BINARY flag and treat it like a BLOB
		 */
		gda_value_set_string (gda_value, value);
		break;
	case FIELD_TYPE_DATE :
		gda_value_set_from_string (gda_value, value, GDA_VALUE_TYPE_DATE);
		break;
	case FIELD_TYPE_TIME :
		gda_value_set_from_string (gda_value, value, GDA_VALUE_TYPE_TIME);
		break;
	case FIELD_TYPE_TIMESTAMP :
	case FIELD_TYPE_DATETIME :
		gda_value_set_from_string (gda_value, value, GDA_VALUE_TYPE_TIMESTAMP);
		break;
	case FIELD_TYPE_NULL :
	case FIELD_TYPE_NEWDATE :
	case FIELD_TYPE_ENUM :
	case FIELD_TYPE_SET : /* FIXME */
		gda_value_set_string (gda_value, value);
		break;
	default :
		g_printerr ("Unknown MySQL datatype.  This is fishy, but continuing anyway.\n");
		gda_value_set_string (gda_value, value);
	}
}


static GdaRow *
fetch_row (GdaMysqlRecordset *recset, gulong rownum)
{
	GdaRow *row;
	gint field_count;
	gint row_count;
	gint i;
	MYSQL_FIELD *mysql_fields;
	MYSQL_ROW mysql_row;
	unsigned long *mysql_lengths;


	g_return_val_if_fail (GDA_IS_MYSQL_RECORDSET (recset), NULL);
	g_return_val_if_fail (recset->priv != NULL, 0);

	if (!recset->priv->mysql_res) {
		gda_connection_add_event_string (recset->priv->cnc, _("Invalid MySQL handle"));
		return NULL;
	}

	/* move to the corresponding row */
	row_count = mysql_num_rows (recset->priv->mysql_res);
	if (row_count == 0)
		return NULL;
	field_count = mysql_num_fields (recset->priv->mysql_res);

	if (rownum < 0 || rownum >= row_count) {
		gda_connection_add_event_string (recset->priv->cnc, _("Row number out of range"));
		return NULL;
	}

	mysql_data_seek (recset->priv->mysql_res, rownum);
	mysql_fields = mysql_fetch_fields (recset->priv->mysql_res);
	mysql_row = mysql_fetch_row (recset->priv->mysql_res);
	mysql_lengths = mysql_fetch_lengths (recset->priv->mysql_res);
	if (!mysql_row || !mysql_lengths)
		return NULL;
	
	row = gda_row_new (GDA_DATA_MODEL (recset), field_count);

	for (i = 0; i < field_count; i++) {
		fill_gda_value ((GdaValue*) gda_row_get_value (row, i),
				mysql_fields[i].type,
				mysql_row[i],
				mysql_lengths[i],
				mysql_fields[i].flags & UNSIGNED_FLAG);
	}

	return row;
}

/*
 * GdaMysqlRecordset class implementation
 */

static gint
gda_mysql_recordset_get_n_rows (GdaDataModelBase *model)
{
	GdaMysqlRecordset *recset = (GdaMysqlRecordset *) model;

	g_return_val_if_fail (GDA_IS_MYSQL_RECORDSET (recset), -1);
	g_return_val_if_fail (recset->priv != NULL, -1);

	if (recset->priv->row_sync == FALSE)
		return recset->priv->mysql_res_rows;
	else
		return GDA_DATA_MODEL_BASE_CLASS (parent_class)->get_n_rows (model);
}

static const GdaRow *
gda_mysql_recordset_get_row (GdaDataModelBase *model, gint row)
{
	GdaMysqlRecordset *recset = (GdaMysqlRecordset *) model;
	GdaMysqlRecordsetPrivate *priv_data;
	gint fetched_rows;
	gint i;
	GdaRow *row_list, *fields = NULL;

	g_return_val_if_fail (GDA_IS_MYSQL_RECORDSET (recset), NULL);
	g_return_val_if_fail (recset->priv != NULL, 0);

	row_list = (GdaRow *) GDA_DATA_MODEL_BASE_CLASS (parent_class)->get_row (model, row);
	if (row_list != NULL)
		return (const GdaRow *)row_list;	

	priv_data = recset->priv;
	if (!priv_data->mysql_res) {
		gda_connection_add_event_string (priv_data->cnc,
						_("Invalid MySQL handle"));
		return NULL;
	}
	
	if (row < 0 || row > priv_data->mysql_res_rows) {
		gda_connection_add_event_string (priv_data->cnc,
						_("Row number out of range"));
		return NULL;
	}

	fetched_rows = GDA_DATA_MODEL_BASE_CLASS (parent_class)->get_n_rows (model);

	gda_data_model_freeze (GDA_DATA_MODEL (recset));

	for (i = fetched_rows; i <= row; i++) {
		row_list = fetch_row (recset, i);
		if (!row_list)
			return NULL;

		if (GDA_DATA_MODEL_BASE_CLASS (parent_class)->append_row (model, row_list) == FALSE)
			return NULL;
	}

	gda_data_model_thaw (GDA_DATA_MODEL (recset));


	return (const GdaRow *) row_list;
}

static const GdaValue *
gda_mysql_recordset_get_value_at (GdaDataModelBase *model, gint col, gint row)
{
	GdaMysqlRecordset *recset = (GdaMysqlRecordset *) model;
	GdaMysqlRecordsetPrivate *priv_data;
	const GdaValue *value;
	const GdaRow *fields;

	g_return_val_if_fail (GDA_IS_MYSQL_RECORDSET (recset), NULL);
	g_return_val_if_fail (recset->priv != NULL, 0);

	value = GDA_DATA_MODEL_BASE_CLASS (parent_class)->get_value_at (model, col, row);
	if (value != NULL)
		return value;

	priv_data = recset->priv;
        if (!priv_data->mysql_res) {
		gda_connection_add_event_string (priv_data->cnc,
						 _("Invalid MySQL handle"));
		return NULL;
	}

	if (row < 0 || row > priv_data->mysql_res_rows) {
		gda_connection_add_event_string (priv_data->cnc,
						 _("Row number out of range"));
		return NULL;
	}

	if (col >= priv_data->ncolumns) {
		gda_connection_add_event_string (priv_data->cnc,
						 _("Column number out of range"));
		return NULL;
	}	

	fields = gda_mysql_recordset_get_row (model, row);
	return fields != NULL ? gda_row_get_value ((GdaRow *) fields, col) : NULL;
}

static gboolean
gda_mysql_recordset_is_updatable (GdaDataModelBase *model)
{
	GdaCommandType cmd_type;
	GdaMysqlRecordset *recset = (GdaMysqlRecordset *) model;

	g_return_val_if_fail (GDA_IS_MYSQL_RECORDSET (recset), FALSE);
	
	cmd_type = gda_data_model_get_command_type (GDA_DATA_MODEL (model));
	return cmd_type == GDA_COMMAND_TYPE_TABLE ? TRUE : FALSE;
}

static gboolean
gda_mysql_recordset_append_row (GdaDataModelBase *model, GdaRow *row)
{
	GdaMysqlRecordset *recset = (GdaMysqlRecordset *) model;
	GdaMysqlRecordsetPrivate *priv_data;
	GdaRow *row_list;
	MYSQL *mysql;
	MYSQL_FIELD *mysql_field;
	gint i, fetched_rows, rc;
	GString *sql, *sql_value;
	const gchar *column_name;

	g_return_val_if_fail (GDA_IS_MYSQL_RECORDSET (recset), FALSE);
	g_return_val_if_fail (row != NULL, FALSE);
	g_return_val_if_fail (gda_data_model_is_updatable (GDA_DATA_MODEL (model)), FALSE);
	g_return_val_if_fail (recset->priv != NULL, FALSE);

	priv_data = recset->priv;

	/* checks for valid MySQL handle */
        if (!priv_data->mysql_res) {
		gda_connection_add_event_string (priv_data->cnc, _("Invalid MySQL handle"));
		return FALSE;
	}

	/* get the mysql handle */
	mysql = g_object_get_data (G_OBJECT (priv_data->cnc), OBJECT_DATA_MYSQL_HANDLE);
	
	/* checks if the table name has been guessed */
	if (priv_data->table_name == NULL) {
		gda_connection_add_event_string (priv_data->cnc,
						_("Table name could not be guessed"));
		return FALSE;
	}

	/* checks for correct number of columns */
	if (priv_data->ncolumns != gda_row_get_length (row)) {
		gda_connection_add_event_string (priv_data->cnc,
						_("Attempt to insert a row with an invalid number of columns"));
		return FALSE;
	}

	/* check if all results are loaded in array, if not do so */
	if (priv_data->row_sync == FALSE)
	{
		fetched_rows = GDA_DATA_MODEL_BASE_CLASS (parent_class)->get_n_rows (model);

		gda_data_model_freeze (GDA_DATA_MODEL (recset));

		for (i = fetched_rows; i < priv_data->mysql_res_rows; i++) {
			row_list = fetch_row (recset, i);
			if (!row_list) {
				gda_connection_add_event_string (priv_data->cnc,
						_("Can not synchronize array with MySQL result set"));
				return FALSE;
			}

			if (GDA_DATA_MODEL_BASE_CLASS (parent_class)->append_row (model, row_list) == FALSE) {
				gda_connection_add_event_string (priv_data->cnc,
						_("Can not synchronize array with MySQL result set"));
				return FALSE;
			}
		}

		gda_data_model_thaw (GDA_DATA_MODEL (recset));

		/* set flag that all mysql result records are in array */
		priv_data->row_sync = TRUE;
	}	

	/* prepare the SQL statement */
	sql = g_string_new ("INSERT INTO ");
        g_string_append_printf (sql, "%s (", priv_data->table_name);

	sql_value = g_string_new ("VALUES (");

	for (i = 0; i < priv_data->ncolumns; i++) {

		if (i != 0) {
			sql = g_string_append (sql, ", ");
			sql_value = g_string_append (sql_value, ", ");
		}
		/* get column name */
		mysql_field = mysql_fetch_field_direct (priv_data->mysql_res, i);
		if (mysql_field)
			column_name = mysql_field->name;
		else
			column_name = gda_data_model_get_column_title (GDA_DATA_MODEL (model), i);

		sql = g_string_append (sql, "`");
		sql = g_string_append (sql, column_name);
		sql = g_string_append (sql, "`");

		sql_value = g_string_append (sql_value, 
			gda_mysql_provider_value_to_sql_string ( 	 
				NULL, 	 
				priv_data->cnc, 	 
				gda_row_get_value ((GdaRow *) row, i)) 	 
                 );
	}

	/* concatenate SQL statement */
	sql = g_string_append (sql, ") ");
	sql = g_string_append (sql, sql_value->str);
	sql = g_string_append (sql, ")");

	/* execute append command */
	rc = mysql_real_query (mysql, sql->str, strlen (sql->str));
	if (rc != 0) {
		gda_connection_add_event (
			priv_data->cnc, gda_mysql_make_error (mysql));
		return FALSE;
	}

	g_string_free (sql, TRUE);
	g_string_free (sql_value, TRUE);

	/* append row in the array */
	if (GDA_DATA_MODEL_BASE_CLASS (parent_class)->append_row (model, row) == FALSE) {
		gda_connection_add_event_string (priv_data->cnc,
			_("Can not append row to data model"));
		return FALSE;
	}

	return TRUE;
}

static gboolean
gda_mysql_recordset_remove_row (GdaDataModelBase *model, const GdaRow *row)
{
	GdaMysqlRecordset *recset = (GdaMysqlRecordset *) model;
	GdaMysqlRecordsetPrivate *priv_data;
	GdaRow *row_list;
	GdaColumn *attrs;
	MYSQL *mysql;
	MYSQL_FIELD *mysql_field;
	gint i, fetched_rows, rc, colnum, uk;
	gchar *query, *query_where, *tmp;
	const gchar *column_name;

	g_return_val_if_fail (GDA_IS_MYSQL_RECORDSET (recset), FALSE);
	g_return_val_if_fail (row != NULL, FALSE);
	g_return_val_if_fail (gda_data_model_is_updatable (GDA_DATA_MODEL (model)), FALSE);
	g_return_val_if_fail (recset->priv != NULL, FALSE);

	priv_data = recset->priv;

	/* checks for valid MySQL handle */
        if (!priv_data->mysql_res) {
		gda_connection_add_event_string (priv_data->cnc, _("Invalid MySQL handle"));
		return FALSE;
	}

	/* get the mysql handle */
	mysql = g_object_get_data (G_OBJECT (priv_data->cnc), OBJECT_DATA_MYSQL_HANDLE);
	
	/* checks if the given row belongs to the given model */
	if (gda_row_get_model ((GdaRow *) row) != GDA_DATA_MODEL (model)) {
		gda_connection_add_event_string (priv_data->cnc,
						_("Given row doesn't belong to the model."));
		return FALSE;
	}

	/* checks if the table name has been guessed */
	if (priv_data->table_name == NULL) {
		gda_connection_add_event_string (priv_data->cnc,
						_("Table name could not be guessed"));
		return FALSE;
	}

	/* check if all results are loaded in array, if not do so */
	if (priv_data->row_sync == FALSE)
	{
		fetched_rows = GDA_DATA_MODEL_BASE_CLASS (parent_class)->get_n_rows (model);

		gda_data_model_freeze (GDA_DATA_MODEL (recset));

		for (i = fetched_rows; i < priv_data->mysql_res_rows; i++) {
			row_list = fetch_row (recset, i);
			if (!row_list) {
				gda_connection_add_event_string (priv_data->cnc,
						_("Can not synchronize array with MySQL result set"));
				return FALSE;
			}

			if (GDA_DATA_MODEL_BASE_CLASS (parent_class)->append_row (model, row_list) == FALSE) {
				gda_connection_add_event_string (priv_data->cnc,
						_("Can not synchronize array with MySQL result set"));
				return FALSE;
			}
		}

		gda_data_model_thaw (GDA_DATA_MODEL (recset));

		/* set flag that all mysql result records are in array */
		priv_data->row_sync = TRUE;
	}	

	query_where = g_strdup ("WHERE ");

	for (colnum = uk = 0;
	     colnum != gda_data_model_get_n_columns (GDA_DATA_MODEL(model));
	     colnum++)
	{
		attrs = gda_data_model_describe_column (GDA_DATA_MODEL(model), colnum);

		mysql_field = mysql_fetch_field_direct (priv_data->mysql_res, colnum);
		if (mysql_field)
			column_name = mysql_field->name;
		else
			column_name = gda_data_model_get_column_title (GDA_DATA_MODEL (model), colnum);

		gchar *curval = gda_mysql_provider_value_to_sql_string (
				NULL, 	 
				priv_data->cnc, 	 
				gda_row_get_value ((GdaRow *) row, colnum) 	 
     		);

		/* unique column: we will use it as an index */
		if (gda_column_get_primary_key (attrs) ||
		    gda_column_get_unique_key (attrs)) {
			/* fills the 'where' part of the update command */
			if (colnum != 0)
				query_where = g_strconcat (query_where, "AND ", NULL);

			/* fills the 'where' part of the remove command */
			tmp = g_strdup_printf ("`%s` = %s ",
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
		g_free (query_where);

		return FALSE;
	}

	/* build the delete command */
	query = g_strdup_printf ("DELETE FROM %s %s",
				 priv_data->table_name,
				 query_where);

	/* execute append command */
	rc = mysql_real_query (mysql, query, strlen (query));
	if (rc != 0) {
		gda_connection_add_event (
			priv_data->cnc, gda_mysql_make_error (mysql));
		g_free (query);
		g_free (query_where);

		return FALSE;
	}

	g_free (query);
	g_free (query_where);

	/* remove row from the array */
	if (GDA_DATA_MODEL_BASE_CLASS (parent_class)->remove_row (model, row) == FALSE) {
		gda_connection_add_event_string (priv_data->cnc,
			_("Can not remove row from data model"));
		return FALSE;
	}

	return TRUE;
}

static gboolean
gda_mysql_recordset_update_row (GdaDataModelBase *model, const GdaRow *row)
{
	GdaMysqlRecordset *recset = (GdaMysqlRecordset *) model;
	GdaMysqlRecordsetPrivate *priv_data;
	gint colnum, uk, nuk, rc, rownum;
	gchar *query, *query_where, *query_set, *tmp;
	gchar *oldval, *newval;
	const gchar *column_name;
	MYSQL *mysql;
	MYSQL_FIELD *mysql_field;
	MYSQL_ROW mysql_row;
	GdaColumn *attrs;

	g_return_val_if_fail (GDA_IS_MYSQL_RECORDSET (recset), FALSE);
	g_return_val_if_fail (row != NULL, FALSE);
	g_return_val_if_fail (gda_data_model_is_updatable (GDA_DATA_MODEL (model)), FALSE);
	g_return_val_if_fail (recset->priv != NULL, FALSE);

	priv_data = recset->priv;

	/* checks for valid MySQL handle */
        if (!priv_data->mysql_res) {
		gda_connection_add_event_string (priv_data->cnc, _("Invalid MySQL handle"));
		return FALSE;
	}

	/* get the mysql handle */
	mysql = g_object_get_data (G_OBJECT (priv_data->cnc), OBJECT_DATA_MYSQL_HANDLE);
	
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

	/* init query */
	query_where = g_strdup ("WHERE ");
	query_set = g_strdup ("SET ");

	/* get original data if from mysql result set */
	rownum = gda_row_get_number ((GdaRow *) row);
	if (rownum < priv_data->mysql_res_rows) {
		mysql_data_seek (recset->priv->mysql_res, rownum);
		mysql_row = mysql_fetch_row (recset->priv->mysql_res);
	}

	/* process columns */
	for (colnum = uk = nuk = 0;
	     colnum != gda_data_model_get_n_columns (GDA_DATA_MODEL (model));
	     colnum++) 
	{
		attrs = gda_data_model_describe_column (GDA_DATA_MODEL (model), colnum);
		mysql_field = mysql_fetch_field_direct (priv_data->mysql_res, colnum);
		if (mysql_field)
			column_name = mysql_field->name;
		else
			column_name = gda_data_model_get_column_title (GDA_DATA_MODEL (model), colnum);
		newval = gda_value_stringify (gda_row_get_value ((GdaRow *) row, colnum));

		/* for data from mysql result we can retrieve original values to avoid 
		   unique columns to be updated */
		if (rownum < priv_data->mysql_res_rows)
			oldval = mysql_row[colnum];
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
			if (colnum != 0)
				query_where = g_strconcat (query_where, "AND ", NULL);

			tmp = g_strdup_printf ("`%s` = '%s' ",
					       column_name,
					       newval);
			query_where = g_strconcat (query_where, tmp, NULL);
			g_free (tmp);
			uk++;
		}
		/* non-unique column: update it */
		else {
			/* fills the 'set' part of the update command */
			tmp = g_strdup_printf ("`%s` = '%s', ", 
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
						_("Model does not have at least one non-modified unique key."));
		g_free (query_set);
		g_free (query_where);

		return FALSE;
	}

	if (nuk == 0) {
		gda_connection_add_event_string (priv_data->cnc,
						_("Model does not have any non-unique values to update."));
		g_free (query_set);
		g_free (query_where);

		return FALSE;
	}

	/* remove the last ',' in the SET part */
	tmp = strrchr (query_set, ',');
	if (tmp != NULL)
		*tmp = ' ';
	
	/* build the update command */
	query = g_strdup_printf ("UPDATE %s %s %s", 
				 priv_data->table_name,
				 query_set,
				 query_where);
	
	/* execute update command */
	rc = mysql_real_query (mysql, query, strlen (query));
	if (rc != 0) {
		gda_connection_add_event (
			priv_data->cnc, gda_mysql_make_error (mysql));
		return FALSE;
	}
	
	/* emit update signals */
	gda_data_model_row_updated (GDA_DATA_MODEL (model), gda_row_get_number ((GdaRow *) row));

	/* clean up */
	g_free (query);
	g_free (query_set);
	g_free (query_where);

	return TRUE;
}

GType
gda_mysql_recordset_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (GdaMysqlRecordsetClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_mysql_recordset_class_init,
			NULL,
			NULL,
			sizeof (GdaMysqlRecordset),
			0,
			(GInstanceInitFunc) gda_mysql_recordset_init
		};
		type = g_type_register_static (PARENT_TYPE, "GdaMysqlRecordset", &info, 0);
	}

	return type;
}

GdaMysqlRecordset *
gda_mysql_recordset_new (GdaConnection *cnc, MYSQL_RES *mysql_res, MYSQL *mysql)
{
	GdaMysqlRecordset *model;
	MYSQL_FIELD *mysql_fields;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (mysql_res != NULL || mysql != NULL, NULL);

	model = g_object_new (GDA_TYPE_MYSQL_RECORDSET, NULL);
	model->priv->mysql_res = mysql_res;
	model->priv->cnc = cnc;
	model->priv->row_sync = FALSE;
	model->priv->ncolumns = 0;
	if (mysql_res == NULL) {
		model->priv->mysql_res_rows = mysql_affected_rows (mysql);
		return model;
	} 
	else 
		model->priv->mysql_res_rows = mysql_num_rows (model->priv->mysql_res);

	mysql_fields = mysql_fetch_fields (model->priv->mysql_res);
	if (mysql_fields != NULL) {
		gint i;

		model->priv->ncolumns = mysql_num_fields (model->priv->mysql_res);
		gda_data_model_array_set_n_columns (GDA_DATA_MODEL_ARRAY (model),
						    model->priv->ncolumns);
		for (i = 0; i < model->priv->ncolumns; i++) {
			GdaColumn *column;
			MYSQL_FIELD *mysql_field;

			/* determine table name */
			gboolean multi_table = FALSE;
			if (strcmp(mysql_fields[i].table, mysql_fields[0].table) != 0)
				multi_table = TRUE;

			if (multi_table == FALSE)
				model->priv->table_name = g_strdup (mysql_fields[0].table);
			else
				model->priv->table_name = NULL;

			/* set GdaColumn attributes */
			column = gda_data_model_describe_column (GDA_DATA_MODEL (model), i);
			mysql_field = &(mysql_fields[i]);
			gda_column_set_title (column, mysql_field->name);
			if (mysql_field->name)
				gda_column_set_name (column, mysql_field->name);
			gda_column_set_defined_size (column, mysql_field->length);
			gda_column_set_table (column, mysql_field->table);
			gda_column_set_scale (column, mysql_field->decimals);
			gda_column_set_gdatype (column, gda_mysql_type_to_gda (mysql_field->type,
									       mysql_field->flags & UNSIGNED_FLAG));
			gda_column_set_allow_null (column, !IS_NOT_NULL (mysql_field->flags));
			gda_column_set_primary_key (column, IS_PRI_KEY (mysql_field->flags));
			gda_column_set_unique_key (column, mysql_field->flags & UNIQUE_KEY_FLAG);
			gda_column_set_auto_increment (column, mysql_field->flags & AUTO_INCREMENT_FLAG);
		}
	}

	return model;
}
