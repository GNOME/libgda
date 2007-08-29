/* GDA LDAP provider
 * Copyright (C) 1998 - 2005 The GNOME Foundation.
 *
 * AUTHORS:
 *      Michael Lausch <michael@lausch.at>
 *      Rodrigo Moya <rodrigo@gnome-db.org>
 *      Vivien Malerba <malerba@gnome-db.org>
 *      German Poo-Caaman~o <gpoo@ubiobio.cl>
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
#include <stdlib.h>
#include "gda-ldap.h"
#include "gda-ldap-recordset.h"
#include <libgda/gda-data-model-private.h>

#define PARENT_TYPE GDA_TYPE_DATA_MODEL_ROW

static void gda_ldap_recordset_class_init (GdaLdapRecordsetClass *klass);
static void gda_ldap_recordset_init       (GdaLdapRecordset *recset,
					    GdaLdapRecordsetClass *klass);
static void gda_ldap_recordset_finalize   (GObject *object);

static GObjectClass *parent_class = NULL;

/*
 * Private functions
 */

static GdaRow *
fetch_row (GdaDataModel *model, GdaLdapRecordset *recset, gulong rownum)
{
	GdaRow *row;
	gint field_count = 0;
	gint row_count = 0;
/*	gint i;
	unsigned long *lengths;*/
/*	LDAP_FIELD *ldap_fields;
	LDAP_ROW ldap_row;*/

	g_return_val_if_fail (GDA_IS_LDAP_RECORDSET (recset), NULL);

	if (!recset->ldap_res) {
		gda_connection_add_event_string (recset->cnc, _("Invalid LDAP handle"));
		return NULL;
	}

	/* move to the corresponding row */
/*	row_count = ldap_num_rows (recset->ldap_res);
	if (row_count == 0)
		return NULL;
	field_count = ldap_num_fields (recset->ldap_res);
*/
	if (rownum < 0 || rownum >= row_count) {
		gda_connection_add_event_string (recset->cnc, _("Row number out of range"));
		return NULL;
	}

/*	ldap_data_seek (recset->ldap_res, rownum);*/
	row = gda_row_new (model, field_count);

/*	lengths = recset->ldap_res->lengths;
	ldap_fields = ldap_fetch_fields (recset->ldap_res);

	ldap_row = ldap_fetch_row (recset->ldap_res);
	if (!ldap_row)
		return NULL;

	for (i = 0; i < field_count; i++) {
		GValue *field;
		gchar *thevalue;

		field = gda_row_get_value (row, i);

		thevalue = ldap_row[i];

		switch (ldap_fields[i].type) {
		case FIELD_TYPE_DECIMAL :
		case FIELD_TYPE_DOUBLE :
			g_value_set_double (field, atof (thevalue));
			break;
		case FIELD_TYPE_FLOAT :
			g_value_set_float (field, atof (thevalue));
			break;
		case FIELD_TYPE_LONG :
		case FIELD_TYPE_YEAR :
			g_value_set_int (field, atol (thevalue));
			break;
		case FIELD_TYPE_LONGLONG :
		case FIELD_TYPE_INT24 :
			g_value_set_int64 (field, atoll (thevalue));
			break;
		case FIELD_TYPE_SHORT :
			gda_value_set_short (field, atoi (thevalue));
			break;
		case FIELD_TYPE_TINY :
			g_value_set_char (field, atoi (thevalue));
			break;
		case FIELD_TYPE_TINY_BLOB :
		case FIELD_TYPE_MEDIUM_BLOB :
		case FIELD_TYPE_LONG_BLOB :
		case FIELD_TYPE_BLOB :
			gda_value_set_binary (field, thevalue, lengths[i]);
			break;
		case FIELD_TYPE_VAR_STRING :
		case FIELD_TYPE_STRING :
			g_value_set_string (field, thevalue ? thevalue : "");
			break;
		case FIELD_TYPE_DATE :
		case FIELD_TYPE_NULL :
		case FIELD_TYPE_NEWDATE :
		case FIELD_TYPE_ENUM :
		case FIELD_TYPE_TIMESTAMP :
		case FIELD_TYPE_DATETIME :
		case FIELD_TYPE_TIME :
		case FIELD_TYPE_SET :
			g_value_set_string (field, thevalue ? thevalue : "");
			break;
		default :
			g_value_set_string (field, thevalue ? thevalue : "");
		}
	}
*/
	return row;
}

/*
 * GdaLdapRecordset class implementation
 */

static gint
gda_ldap_recordset_get_n_rows (GdaDataModelRow *model)
{
	GdaLdapRecordset *recset = (GdaLdapRecordset *) model;

	g_return_val_if_fail (GDA_IS_LDAP_RECORDSET (recset), -1);
/*	return ldap_num_rows (recset->ldap_res);*/
	return 1; /* FIXME */
}

static gint
gda_ldap_recordset_get_n_columns (GdaDataModelRow *model)
{
	GdaLdapRecordset *recset = (GdaLdapRecordset *) model;

	g_return_val_if_fail (GDA_IS_LDAP_RECORDSET (recset), -1);
	/* return ldap_num_fields (recset->ldap_res);*/
	return 1; /* FIXME */
}

static void
gda_ldap_recordset_describe_column (GdaDataModel *model, gint col)
{
	gint field_count = 0;
	GdaColumn *attrs;
/*	LDAP_FIELD *ldap_fields;*/
	GdaLdapRecordset *recset = (GdaLdapRecordset *) model;

	g_return_val_if_fail (GDA_IS_LDAP_RECORDSET (recset), NULL);

	if (!recset->ldap_res) {
		gda_connection_add_event_string (recset->cnc, _("Invalid LDAP handle"));
		return NULL;
	}

	/* create the GdaColumn to be returned */
	/*field_count = ldap_num_fields (recset->ldap_res);*/
	if (col >= field_count)
		return NULL;

	attrs = gda_data_model_describe_column (model, col);

	/*ldap_fields = ldap_fetch_field (recset->ldap_res);*/
/*	if (!ldap_fields)
		return NULL;
*/
/*	if (ldap_fields[col].name)
		gda_column_set_name (attrs, ldap_fields[col].name);
	gda_column_set_defined_size (attrs, ldap_fields[col].max_length);
	gda_column_set_scale (attrs, ldap_fields[col].decimals);
	gda_column_set_g_type (attrs, gda_ldap_type_to_gda (ldap_fields[col].type));
*/
	return attrs;
}

static const GdaRow *
gda_ldap_recordset_get_row (GdaDataModelRow *model, gint row, GError **error)
{
	gint rows = 0;
	gint fetched_rows = 0;
	gint i;
	GdaRow *fields = NULL;
	GdaLdapRecordset *recset = (GdaLdapRecordset *) model;

	g_return_val_if_fail (GDA_IS_LDAP_RECORDSET (recset), NULL);

/*	rows = ldap_num_rows (recset->ldap_res);
	fetched_rows = recset->rows->len;*/

	if (row >= rows)
		return NULL;
	if (row < fetched_rows)
		return (const GdaRow *) g_ptr_array_index (recset->rows, row);

	gda_data_model_freeze (GDA_DATA_MODEL (recset));

	for (i = fetched_rows; i <= row; i++) {
		fields = fetch_row (GDA_DATA_MODEL (model), recset, i);
		if (!fields)
			return NULL;

		g_ptr_array_add (recset->rows, fields);
	}

	gda_data_model_thaw (GDA_DATA_MODEL (recset));

	return (const GdaRow *) fields;
}

static const GValue *
gda_ldap_recordset_get_value_at (GdaDataModelRow *model, gint col, gint row)
{
	gint cols = 0;
	const GdaRow *fields;
	GdaLdapRecordset *recset = (GdaLdapRecordset *) model;

	g_return_val_if_fail (GDA_IS_LDAP_RECORDSET (recset), NULL);
	
/*	cols = ldap_num_fields (recset->ldap_res);*/
	if (col >= cols)
		return NULL;

	fields = gda_ldap_recordset_get_row (model, row, NULL);
	return fields != NULL ? gda_row_get_value ((GdaRow *) fields, col) : NULL;
}

static gboolean
gda_ldap_recordset_is_updatable (GdaDataModelRow *model)
{
	GdaCommandType cmd_type;
	GdaLdapRecordset *recset = (GdaLdapRecordset *) model;

	g_return_val_if_fail (GDA_IS_LDAP_RECORDSET (recset), FALSE);
	
	g_object_get (G_OBJECT (model), "command_type", &cmd_type, NULL);
	return FALSE;
	/*return cmd_type == GDA_COMMAND_TYPE_TABLE ? TRUE : FALSE;*/
}

/* Not implemented. See bug http://bugzilla.gnome.org/show_bug.cgi?id=411811: */
#if 0
static const GdaRow *
gda_ldap_recordset_append_values (GdaDataModelRow *model, const GList *values)
{
	GString *sql;
	GdaRow *row;
	gint i;
/*	gint rc;*/
	gint cols = 0;
	GList *l;
	GdaLdapRecordset *recset = (GdaLdapRecordset *) model;

	g_return_val_if_fail (GDA_IS_LDAP_RECORDSET (recset), NULL);
	g_return_val_if_fail (values != NULL, NULL);
	g_return_val_if_fail (gda_data_model_is_updatable (GDA_DATA_MODEL (model)), NULL);

/*	cols = ldap_num_fields (recset->ldap_res);*/
	if (cols != g_list_length ((GList *) values)) {
		gda_connection_add_event_string (
			recset->cnc,
			_("Attempt to insert a row with an invalid number of columns"));
		return NULL;
	}

	/* prepare the SQL command */
	sql = g_string_new ("INSERT INTO ");
	sql = g_string_append (sql, gda_data_model_get_command_text (GDA_DATA_MODEL (model)));
	sql = g_string_append (sql, "(");
	for (i = 0; i < cols; i++) {
		GdaColumn *fa;

		fa = gda_data_model_describe_column (GDA_DATA_MODEL (model), i);
		if (!fa) {
			gda_connection_add_event_string (
				recset->cnc,
				_("Could not retrieve column's information"));
			g_string_free (sql, TRUE);
			return NULL;
		}

		if (i != 0)
			sql = g_string_append (sql, ", ");
		sql = g_string_append (sql, gda_column_get_name (fa));
	}
	sql = g_string_append (sql, ") VALUES (");

	for (l = (GList *) values, i = 0; i < cols; i++, l = l->next) {
		gchar *val_str;
		const GValue *val = (const GValue *) l->data;

		if (!val) {
			gda_connection_add_event_string (
				recset->cnc,
				_("Could not retrieve column's value"));
			g_string_free (sql, TRUE);
			return NULL;
		}

		if (i != 0)
			sql = g_string_append (sql, ", ");
		val_str = gda_ldap_value_to_sql_string ((GValue *) val);
		sql = g_string_append (sql, val_str);

		g_free (val_str);
	}
	sql = g_string_append (sql, ")");

	/* execute the UPDATE command */
/*	rc = ldap_real_query (recset->ldap_res->handle, sql->str, strlen (sql->str));*/
	g_string_free (sql, TRUE);
/*	if (rc != 0) {
		gda_connection_add_event (
			recset->cnc, gda_ldap_make_error (recset->ldap_res->handle));
		return NULL;
	}
*/
	/* append the row to the data model */
	row = gda_row_new_from_list (GDA_DATA_MODEL (model), values);
	g_ptr_array_add (recset->rows, row);

	return (const GdaRow *) row;
}
#endif

static gboolean
gda_ldap_recordset_remove_row (GdaDataModelRow *model, const GdaRow *row)
{
	return FALSE;
}

static gboolean
gda_ldap_recordset_update_row (GdaDataModelRow *model, const GdaRow *row)
{
	return FALSE;
}

static void
gda_ldap_recordset_class_init (GdaLdapRecordsetClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GdaDataModelRowClass *model_class = GDA_DATA_MODEL_ROW_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = gda_ldap_recordset_finalize;
	model_class->get_n_rows = gda_ldap_recordset_get_n_rows;
	model_class->get_n_columns = gda_ldap_recordset_get_n_columns;
	model_class->get_row = gda_ldap_recordset_get_row;
	model_class->get_value_at = gda_ldap_recordset_get_value_at;
	model_class->is_updatable = gda_ldap_recordset_is_updatable;

        /* Not implemented. See bug http://bugzilla.gnome.org/show_bug.cgi?id=411811: */
	model_class->append_values = /* gda_ldap_recordset_append_values */ NULL;

	model_class->remove_row = gda_ldap_recordset_remove_row;
	model_class->update_row = gda_ldap_recordset_update_row;
}

static void
gda_ldap_recordset_init (GdaLdapRecordset *recset, GdaLdapRecordsetClass *klass)
{
	g_return_if_fail (GDA_IS_LDAP_RECORDSET (recset));

	recset->cnc = NULL;
	recset->ldap_res = NULL;
	recset->rows = g_ptr_array_new ();
}

static void
gda_ldap_recordset_finalize (GObject *object)
{
	GdaLdapRecordset *recset = (GdaLdapRecordset *) object;

	g_return_if_fail (GDA_IS_LDAP_RECORDSET (recset));

/*	ldap_free_result (recset->ldap_res);*/
	recset->ldap_res = NULL;

	while (recset->rows->len > 0) {
		GdaRow * row = (GdaRow *) g_ptr_array_index (recset->rows, 0);

		if (row != NULL)
			g_object_unref (row);
		g_ptr_array_remove_index (recset->rows, 0);
	}
	g_ptr_array_free (recset->rows, TRUE);
	recset->rows = NULL;

	parent_class->finalize (object);
}

GType
gda_ldap_recordset_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo info = {
			sizeof (GdaLdapRecordsetClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_ldap_recordset_class_init,
			NULL,
			NULL,
			sizeof (GdaLdapRecordset),
			0,
			(GInstanceInitFunc) gda_ldap_recordset_init
		};
		type = g_type_register_static (PARENT_TYPE, "GdaLdapRecordset", &info, 0);
	}

	return type;
}

GdaLdapRecordset *
gda_ldap_recordset_new (GdaConnection *cnc, LDAPMessage *ldap_res)
{
	GdaLdapRecordset *recset;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (ldap_res != NULL, NULL);

	recset = g_object_new (GDA_TYPE_LDAP_RECORDSET, NULL);
	recset->cnc = cnc;
	/*recset->ldap_res = ldap_res;*/

	return recset;
}
