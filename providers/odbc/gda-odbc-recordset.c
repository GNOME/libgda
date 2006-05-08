/* GDA Postgres provider
 * Copyright (C) 1998 - 2005 The GNOME Foundation
 *
 * AUTHORS:
 *         Nick Gorham <nick@lurcher.org>
 *         Vivien Malerba <malerba@gnome-db.org>
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
#include <string.h>
#include "gda-odbc.h"
#include "gda-odbc-recordset.h"

#ifdef PARENT_TYPE
#undef PARENT_TYPE
#endif

#define PARENT_TYPE GDA_TYPE_DATA_MODEL_HASH

struct _GdaOdbcRecordsetPrivate {
	SQLHANDLE *stmt;
	GdaConnection *cnc;
	GType *column_types;
	gint ncolumns;
	gint nrows;
	GHashTable *h_table;
};

static void gda_odbc_recordset_class_init (GdaOdbcRecordsetClass *klass);
static void gda_odbc_recordset_init       (GdaOdbcRecordset *recset,
					       GdaOdbcRecordsetClass *klass);
static void gda_odbc_recordset_finalize   (GObject *object);

static const GValue *gda_odbc_recordset_get_value_at    (GdaDataModelRow *model, gint col, gint row);
static void gda_odbc_recordset_describe    (GdaDataModel *model, gint col);
static gint gda_odbc_recordset_get_n_rows 		  (GdaDataModelRow *model);
static const GdaRow *gda_odbc_recordset_get_row 	  (GdaDataModelRow *model, 
							   gint rownum, GError **error);

static GObjectClass *parent_class = NULL;

/*
 * Private functions
 */

/*
 * Object init and finalize
 */
static void
gda_odbc_recordset_init (GdaOdbcRecordset *recset,
			     GdaOdbcRecordsetClass *klass)
{
	g_return_if_fail (GDA_IS_ODBC_RECORDSET (recset));

	recset->priv = g_new0 (GdaOdbcRecordsetPrivate, 1);
}

static void
gda_odbc_recordset_class_init (GdaOdbcRecordsetClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GdaDataModelRowClass *model_class = GDA_DATA_MODEL_ROW_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = gda_odbc_recordset_finalize;
	model_class->get_n_rows = gda_odbc_recordset_get_n_rows;
	model_class->get_value_at = gda_odbc_recordset_get_value_at;
	model_class->get_row = gda_odbc_recordset_get_row;
}

static void
gda_odbc_recordset_finalize (GObject * object)
{
	GdaOdbcRecordset *recset = (GdaOdbcRecordset *) object;

	g_return_if_fail (GDA_IS_ODBC_RECORDSET (recset));

	/* FIXME: Close the recordset... */

	parent_class->finalize (object);
}

static GType *
get_column_types (GdaOdbcRecordsetPrivate *priv)
{
	/* FIXME: Write this */

	return NULL;
}

static GdaRow *
get_row (GdaOdbcRecordsetPrivate *priv, gint rownum, GError **error)
{
	/* FIXME: Write this */

	return NULL;
}

/*
 * Overrides
 */

static const GdaRow *
gda_odbc_recordset_get_row (GdaDataModelRow *model, gint row, GError **error)
{
	/* FIXME: Write this */

	return NULL;
}

static const GValue *
gda_odbc_recordset_get_value_at (GdaDataModelRow *model, gint col, gint row)
{
	/* FIXME: Write this */

	return NULL;
}

static gint
gda_odbc_recordset_get_n_rows (GdaDataModelRow *model)
{
	/* FIXME: Write this */

	return -1;
}

/*
 * Public functions
 */

GType
gda_odbc_recordset_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (GdaOdbcRecordsetClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_odbc_recordset_class_init,
			NULL,
			NULL,
			sizeof (GdaOdbcRecordset),
			0,
			(GInstanceInitFunc) gda_odbc_recordset_init
		};
		type = g_type_register_static (PARENT_TYPE, "GdaOdbcRecordset", &info, 0);
	}
	return type;
}

static void
gda_odbc_recordset_describe (GdaDataModel *model, gint col)
{
	/* FIXME: Write this */

	return NULL;
}

GdaDataModel *
gda_odbc_recordset_new (GdaConnection *cnc, SQLHANDLE stmt )
{
	GdaOdbcRecordset *model;
	GdaOdbcConnectionData *cnc_priv_data;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (stmt != NULL, NULL);

	cnc_priv_data = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_ODBC_HANDLE);

	model = g_object_new (GDA_TYPE_ODBC_RECORDSET, NULL);
	model->priv->stmt = stmt;
	model->priv->cnc = cnc;

	/* FIXME: call gda_odbc_recordset_describe() for each column */

	return GDA_DATA_MODEL (model);
}

