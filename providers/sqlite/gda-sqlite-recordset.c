/* GDA SQLite provider
 * Copyright (C) 1998-2002 The GNOME Foundation.
 *
 * AUTHORS:
 *	Rodrigo Moya <rodrigo@gnome-db.org>
 *      Carlos Perelló Marín <carlos@gnome-db.org>
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

#include <config.h>
#include <libgda/gda-intl.h>
#include "gda-sqlite.h"
#include "gda-sqlite-recordset.h"
#include "gda-sqlite-provider.h"

#ifdef PARENT_TYPE
#undef PARENT_TYPE
#endif

#define PARENT_TYPE GDA_TYPE_DATA_MODEL_HASH

struct _GdaSqliteRecordsetPrivate {
	SQLITEresult *sres;
	GdaConnection *cnc;
	gint ncolumns;
	gint nrows;
};

static void gda_sqlite_recordset_class_init (GdaSqliteRecordsetClass *klass);
static void gda_sqlite_recordset_init       (GdaSqliteRecordset *recset,
					     GdaSqliteRecordsetClass *klass);
static void gda_sqlite_recordset_finalize   (GObject *object);

static const GdaValue *gda_sqlite_recordset_get_value_at (GdaDataModel *model, gint col, gint row);
static GdaFieldAttributes *gda_sqlite_recordset_describe (GdaDataModel *model, gint col);
static gint gda_sqlite_recordset_get_n_rows (GdaDataModel *model);
static const GdaRow *gda_sqlite_recordset_get_row (GdaDataModel *model, gint row);

static GObjectClass *parent_class = NULL;

/*
 * Private functions
 */

/*
 * Object init and finalize
 */
static void
gda_sqlite_recordset_init (GdaSqliteRecordset *recset,
			   GdaSqliteRecordsetClass *klass)
{
	g_return_if_fail (GDA_IS_SQLITE_RECORDSET (recset));

	recset->priv = g_new0 (GdaSqliteRecordsetPrivate, 1);
}

static void
gda_sqlite_recordset_class_init (GdaSqliteRecordsetClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GdaDataModelClass *model_class = GDA_DATA_MODEL_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = gda_sqlite_recordset_finalize;
	model_class->get_n_rows = gda_sqlite_recordset_get_n_rows;
	model_class->describe_column = gda_sqlite_recordset_describe;
	model_class->get_value_at = gda_sqlite_recordset_get_value_at;
	model_class->get_row = gda_sqlite_recordset_get_row;
}

static void
gda_sqlite_recordset_finalize (GObject *object)
{
	GdaSqliteRecordset *recset = (GdaSqliteRecordset *) object;

	g_return_if_fail (GDA_IS_SQLITE_RECORDSET (recset));

	if (recset->priv->sres != NULL) {
		sqlite_free_table (recset->priv->sres->data);
		g_free (recset->priv->sres);
		recset->priv->sres = NULL;
	}

	g_free (recset->priv);
	recset->priv = NULL;
	
	parent_class->finalize (object);
}

static GdaRow *
get_row (GdaSqliteRecordsetPrivate *priv, gint rownum)
{
        gchar *thevalue;
        GdaValueType ftype;
        gboolean isNull;
        GdaValue *value;
        GdaRow *row;
        gint i;
        gchar *id;
	gulong rowpos;
        
        row = gda_row_new (priv->ncolumns);
	rowpos = (priv->ncolumns - 1) + (rownum * priv->ncolumns);
	
        for (i = 0; i < priv->ncolumns; i++) {
                thevalue = priv->sres->data[rowpos + i];
		/* FIXME: Add detection of types */
                ftype = GDA_VALUE_TYPE_STRING;
                isNull = *thevalue != '\0' ? FALSE : TRUE;
                value = gda_row_get_value (row, i);
                gda_sqlite_set_value (value, ftype, thevalue, isNull);
        }

        id = g_strdup_printf ("%d", rownum);

	/* FIXME: by now, the rowid is just the row number */
        gda_row_set_id (row, id);
        g_free (id);

        return row;
}

/*
 * Overrides
 */

static const GdaRow *
gda_sqlite_recordset_get_row (GdaDataModel *model, gint row)
{
	GdaSqliteRecordset *recset = (GdaSqliteRecordset *) model;
	GdaSqliteRecordsetPrivate *priv_data;
	const GdaRow *row_list;

	g_return_val_if_fail (GDA_IS_SQLITE_RECORDSET (recset), NULL);
	g_return_val_if_fail (recset->priv != NULL, NULL);

	row_list = GDA_DATA_MODEL_CLASS (model)->get_row (model, row);
	if (row_list != NULL)
		return row_list;

	priv_data = recset->priv;
	if (!priv_data->sres) {
		gda_connection_add_error_string (priv_data->cnc,
						 _("Invalid SQLite handle"));
		return NULL;
	}

	if (row == priv_data->nrows)
		return NULL;

	if (row < 0 || row > priv_data->nrows) {
		gda_connection_add_error_string (priv_data->cnc,
						 _("Row number out of range"));
		return NULL;
	}
	
	row_list = get_row (priv_data, row);

	return row_list;
}

static const GdaValue *
gda_sqlite_recordset_get_value_at (GdaDataModel *model, gint col, gint row)
{
	GdaSqliteRecordset *recset = (GdaSqliteRecordset *) model;
	GdaSqliteRecordsetPrivate *priv_data;
	SQLITEresult *sres;
	GdaRow *row_list;
	const GdaValue *value;

	g_return_val_if_fail (GDA_IS_SQLITE_RECORDSET (recset), NULL);
	g_return_val_if_fail (recset->priv != NULL, NULL);

	value = gda_data_model_hash_get_value_at (model, col, row);
	if (value != NULL)
		return value;
	
	priv_data = recset->priv;
	sres = priv_data->sres;

	if (!sres) {
		gda_connection_add_error_string (priv_data->cnc,
						 _("Invalid SQLite handle"));
		return NULL;
	}

	if (row == priv_data->nrows)
		return NULL;

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

	row_list = get_row (priv_data, row);
	gda_data_model_hash_insert_row (GDA_DATA_MODEL_HASH (model),
					row, row_list);
	return gda_row_get_value (row_list, col);
}

static GdaFieldAttributes *
gda_sqlite_recordset_describe (GdaDataModel *model, gint col)
{
	GdaSqliteRecordset *recset = (GdaSqliteRecordset *) model;
	GdaSqliteRecordsetPrivate *priv_data;
	SQLITEresult *sres;
	GdaFieldAttributes *field_attrs;

	g_return_val_if_fail (GDA_IS_SQLITE_RECORDSET (recset), NULL);
	g_return_val_if_fail (recset->priv != NULL, NULL);

	priv_data = recset->priv;
	sres = priv_data->sres;
	
	if (!sres) {
		gda_connection_add_error_string (priv_data->cnc,
						 _("Invalid SQLite handle"));
		return NULL;
	}

	if (col >= priv_data->ncolumns) {
		gda_connection_add_error_string (priv_data->cnc,
						 _("Column number out of range"));
		return NULL;
	}

	field_attrs = gda_field_attributes_new ();
	gda_field_attributes_set_name (field_attrs, sres->data[col]);

	gda_field_attributes_set_scale (field_attrs, 0);
	gda_field_attributes_set_gdatype (field_attrs, GDA_VALUE_TYPE_STRING);
	
	gda_field_attributes_set_defined_size (field_attrs, strlen (sres->data[col]));
/*	gda_field_attributes_set_references (field_attrs, "");
	gda_field_attributes_set_primary_key (field_attrs, FALSE);
	gda_field_attributes_set_unique_key (field_attrs, FALSE);
*/

	return field_attrs;
}

static gint
gda_sqlite_recordset_get_n_rows (GdaDataModel *model)
{
	GdaSqliteRecordset *recset = (GdaSqliteRecordset *) model;

	g_return_val_if_fail (GDA_IS_SQLITE_RECORDSET (recset), 0);
	g_return_val_if_fail (recset->priv != NULL, 0);

	return recset->priv->nrows;
}

/*
 * Public functions
 */

GType
gda_sqlite_recordset_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (GdaSqliteRecordsetClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_sqlite_recordset_class_init,
			NULL,
			NULL,
			sizeof (GdaSqliteRecordset),
			0,
			(GInstanceInitFunc) gda_sqlite_recordset_init
		};
		type = g_type_register_static (PARENT_TYPE, "GdaSqliteRecordset", &info, 0);
	}

	return type;
}

GdaDataModel *
gda_sqlite_recordset_new (GdaConnection *cnc, SQLITEresult *sres)
{
	GdaSqliteRecordset *model;
	SQLITEcnc *scnc;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (sres != NULL, NULL);

	scnc = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_SQLITE_HANDLE);
	
	model = g_object_new (GDA_TYPE_SQLITE_RECORDSET, NULL);
	model->priv->sres = sres;
	model->priv->cnc = cnc;
	model->priv->ncolumns = sres->ncols;
	model->priv->nrows = sres->nrows;
	gda_data_model_hash_set_n_columns (GDA_DATA_MODEL_HASH (model),
					   model->priv->ncolumns);

	return GDA_DATA_MODEL (model);
}
