/* GDA libary
 * Copyright (C) 1998-2001 The Free Software Foundation
 *
 * AUTHORS:
 *	Michael Lausch <michael@lausch.at>
 *	Rodrigo Moya <rodrigo@gnome-db.org>
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
#include <libgda/gda-recordset.h>
#include <libgda/gda-row.h>
#include <glib-object.h>
#include <bonobo/bonobo-exception.h>

#define PARENT_TYPE GDA_TYPE_DATA_MODEL_ARRAY

struct _GdaRecordsetPrivate {
	GdaConnection *cnc;
	GNOME_Database_Recordset corba_recset;
	GNOME_Database_RowAttributes *attributes;
	guint timeout_id;
};

static void gda_recordset_class_init (GdaRecordsetClass *klass);
static void gda_recordset_init       (GdaRecordset *recset, GdaRecordsetClass *klass);
static void gda_recordset_finalize   (GObject *object);

enum {
	LAST_SIGNAL
};

static gint gda_recordset_signals[LAST_SIGNAL] = { 0, };
static GObjectClass *parent_class = NULL;

/*
 * Private functions
 */

static void
get_data (GdaRecordset *recset)
{
	g_return_if_fail (GDA_IS_RECORDSET (recset));

	/* remove timeout handler */
	if (recset->priv->timeout_id != -1)
		g_source_remove (recset->priv->timeout_id);

	gda_data_model_array_clear (GDA_DATA_MODEL_ARRAY (recset));
}

/*
 * GdaRecordset class implementation
 */

static gint
gda_recordset_get_n_rows (GdaDataModel *model)
{
	g_return_val_if_fail (parent_class != NULL, -1);
	GDA_DATA_MODEL_CLASS (parent_class)->get_n_rows (model);
}

static gint
gda_recordset_get_n_columns (GdaDataModel *model)
{
	GdaRecordset *recset = (GdaRecordset *) model;

	g_return_val_if_fail (GDA_IS_RECORDSET (recset), -1);

	return recset->priv->attributes->_length;
}

static const GdaValue *
gda_recordset_get_value_at (GdaDataModel *model, gint col, gint row)
{
	g_return_val_if_fail (parent_class != NULL, NULL);
	return GDA_DATA_MODEL_CLASS (parent_class)->get_value_at (model, col, row);
}

GType
gda_recordset_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (GdaRecordsetClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_recordset_class_init,
			NULL,
			NULL,
			sizeof (GdaRecordset),
			0,
			(GInstanceInitFunc) gda_recordset_init
		};
		type = g_type_register_static (PARENT_TYPE, "GdaRecordset", &info, 0);
	}
	return type;
}

static void
gda_recordset_class_init (GdaRecordsetClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GdaDataModelClass *model_class = GDA_DATA_MODEL_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = gda_recordset_finalize;
	model_class->get_n_rows = gda_recordset_get_n_rows;
	model_class->get_n_columns = gda_recordset_get_n_columns;
	model_class->get_value_at = gda_recordset_get_value_at;
}

static void
gda_recordset_init (GdaRecordset *recset, GdaRecordsetClass *klass)
{
	g_return_if_fail (GDA_IS_RECORDSET (recset));

	recset->priv = g_new0 (GdaRecordsetPrivate, 1);

	recset->priv->cnc = NULL;
	recset->priv->corba_recset = CORBA_OBJECT_NIL;
	recset->priv->attributes = NULL;
	recset->priv->timeout_id = -1;
}

static void
gda_recordset_finalize (GObject * object)
{
	GdaRecordset *recset = (GdaRecordset *) object;

	/* remove timeout handler */
	if (recset->priv->timeout_id != -1) {
		g_source_remove (recset->priv->timeout_id);
		recset->priv->timeout_id = -1;
	}

	/* free memory */
	if (recset->priv->corba_recset != CORBA_OBJECT_NIL) {
		CORBA_Environment ev;

		CORBA_exception_init (&ev);
		CORBA_Object_release (recset->priv->corba_recset, &ev);
		CORBA_exception_free (&ev);
	}
	if (recset->priv->attributes != NULL)
		CORBA_free (recset->priv->attributes);

	g_free (recset->priv);
	recset->priv = NULL;

	parent_class->finalize (object);
}

/**
 * gda_recordset_new:
 * @corba_recset: a GNOME_Database_Recordset object
 *
 * Allocates space for a new recordset.
 *
 * Returns: the allocated recordset object
 */
GdaRecordset *
gda_recordset_new (GdaConnection *cnc, GNOME_Database_Recordset corba_recset)
{
	GdaRecordset *recset;
	CORBA_Environment ev;
	gint i;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (corba_recset != CORBA_OBJECT_NIL, NULL);

	recset = g_object_new (GDA_TYPE_RECORDSET, NULL);

	recset->priv->cnc = cnc;
	recset->priv->corba_recset = corba_recset;

	/* get recordset description */
	CORBA_exception_init (&ev);

	recset->priv->attributes = GNOME_Database_Recordset_describe (
		recset->priv->corba_recset, &ev);
	if (BONOBO_EX (&ev)) {
		g_object_unref (G_OBJECT (recset));
		return NULL;
	}

	gda_data_model_array_set_n_columns (GDA_DATA_MODEL_ARRAY (recset),
					    recset->priv->attributes->_length);

	for (i = 0; i < recset->priv->attributes->_length; i++) {
		gda_data_model_set_column_title (
			GDA_DATA_MODEL (recset), i,
			gda_field_attributes_get_name (
				&recset->priv->attributes->_buffer[i]));
	}

	/* retrieve all data from the underlying recordset */
	get_data (recset);

	return recset;
}

