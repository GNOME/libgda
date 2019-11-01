/*
 * Copyright (C) 2011 Murray Cumming <murrayc@murrayc.com>
 * Copyright (C) 2011 - 2014 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2019 Daniel Espinosa <esodan@gmail.com>
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

#undef GDA_DISABLE_DEPRECATED
#include "gda-data-pivot.h"

#include <string.h>
#include <glib/gi18n-lib.h>
#include <libgda/gda-enums.h>
#include <libgda/gda-holder.h>
#include <libgda/gda-data-model.h>
#include <libgda/gda-data-model-array.h>
#include <libgda/gda-data-model-iter.h>
#include <libgda/gda-row.h>
#include <libgda/gda-util.h>
#include <libgda/gda-blob-op.h>
#include <libgda/gda-sql-builder.h>
#include <virtual/libgda-virtual.h>
#include <sql-parser/gda-sql-parser.h>
#include <libgda/sql-parser/gda-sql-statement.h>
#include <libgda/gda-debug-macros.h>

typedef struct {
	GValue *value;
	gint    column_fields_index;
	gint    data_pos; /* index in priv->data_fields, or -1 if no index */
} ColumnData;
static guint column_data_hash (gconstpointer key);
static gboolean column_data_equal (gconstpointer a, gconstpointer b);
static void column_data_free (ColumnData *cdata);


typedef struct {
	gint    row;
	gint    col;
	GType   gtype;
	GArray *values; /* array of #GValue, none will be GDA_TYPE_NULL, and there is at least always 1 GValue */
	GdaDataPivotAggregate aggregate;

	gint    nvalues;
	GValue *data_value;
	GError *error;
} CellData;
static guint cell_data_hash (gconstpointer key);
static gboolean cell_data_equal (gconstpointer a, gconstpointer b);
static void cell_data_free (CellData *cdata);
static void cell_data_compute_aggregate (CellData *cdata);

static GValue *aggregate_get_empty_value (GdaDataPivotAggregate aggregate);
static gboolean aggregate_handle_new_value (CellData *cdata, const GValue *new_value);


#define TABLE_NAME "data"

static void gda_data_pivot_class_init (GdaDataPivotClass *klass);
static void gda_data_pivot_init       (GdaDataPivot *model);
static void gda_data_pivot_dispose    (GObject *object);

static void gda_data_pivot_set_property (GObject *object,
					 guint param_id,
					 const GValue *value,
					 GParamSpec *pspec);
static void gda_data_pivot_get_property (GObject *object,
					 guint param_id,
					 GValue *value,
					 GParamSpec *pspec);

static guint _gda_value_hash (gconstpointer key);
static guint _gda_row_hash (gconstpointer key);
static gboolean _gda_row_equal (gconstpointer a, gconstpointer b);
static gboolean create_vcnc (GdaDataPivot *pivot, GError **error);
static gboolean bind_source_model (GdaDataPivot *pivot, GError **error);
static void clean_previous_population (GdaDataPivot *pivot);

/* GdaDataModel interface */
static void                 gda_data_pivot_data_model_init (GdaDataModelInterface *iface);
static gint                 gda_data_pivot_get_n_rows      (GdaDataModel *model);
static gint                 gda_data_pivot_get_n_columns   (GdaDataModel *model);
static GdaColumn           *gda_data_pivot_describe_column (GdaDataModel *model, gint col);
static GdaDataModelAccessFlags gda_data_pivot_get_access_flags(GdaDataModel *model);
static const GValue        *gda_data_pivot_get_value_at    (GdaDataModel *model, gint col, gint row, GError **error);

typedef struct {
	GdaDataModel  *model; /* data to analyse */

	GdaConnection *vcnc; /* to use data in @model for row and column fields */

	GArray        *row_fields; /* array of (gchar *) field specifications */
	GArray        *column_fields; /* array of (gchar *) field specifications */
	GArray        *data_fields; /* array of (gchar *) field specifications */
	GArray        *data_aggregates; /* array of GdaDataPivotAggregate, corresponding to @data_fields */

	/* computed data */
	GArray        *columns; /* Array of GdaColumn objects, for ALL columns! */
	GdaDataModel  *results;
} GdaDataPivotPrivate;

G_DEFINE_TYPE_WITH_CODE (GdaDataPivot, gda_data_pivot,G_TYPE_OBJECT,
                         G_ADD_PRIVATE (GdaDataPivot)
                         G_IMPLEMENT_INTERFACE(GDA_TYPE_DATA_MODEL,gda_data_pivot_data_model_init))
/* properties */
enum
{
  PROP_0,
	PROP_MODEL,
};

static void 
gda_data_pivot_class_init (GdaDataPivotClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	/* properties */
	object_class->set_property = gda_data_pivot_set_property;
	object_class->get_property = gda_data_pivot_get_property;
	g_object_class_install_property (object_class, PROP_MODEL,
                                         g_param_spec_object ("model", NULL, "Data model from which data is analysed",
                                                              GDA_TYPE_DATA_MODEL,
							      G_PARAM_READABLE | G_PARAM_WRITABLE));

	/* virtual functions */
	object_class->dispose = gda_data_pivot_dispose;
}

static void
gda_data_pivot_data_model_init (GdaDataModelInterface *iface)
{
	iface->get_n_rows = gda_data_pivot_get_n_rows;
	iface->get_n_columns = gda_data_pivot_get_n_columns;
	iface->describe_column = gda_data_pivot_describe_column;
        iface->get_access_flags = gda_data_pivot_get_access_flags;
	iface->get_value_at = gda_data_pivot_get_value_at;
	iface->get_attributes_at = NULL;

	iface->create_iter = NULL;

	iface->set_value_at = NULL;
	iface->set_values = NULL;
        iface->append_values = NULL;
	iface->append_row = NULL;
	iface->remove_row = NULL;
	iface->find_row = NULL;
	
	iface->freeze = NULL;
	iface->thaw = NULL;
	iface->get_notify = NULL;
	iface->send_hint = NULL;
}

static void
gda_data_pivot_init (GdaDataPivot *model)
{
	GdaDataPivotPrivate *priv = gda_data_pivot_get_instance_private (model);
	priv->model = NULL;
}

static void
clean_previous_population (GdaDataPivot *pivot)
{
	GdaDataPivotPrivate *priv = gda_data_pivot_get_instance_private (pivot);
	if (priv->results) {
		g_object_unref ((GObject*) priv->results);
		priv->results = NULL;
	}

	if (priv->columns) {
		guint i;
		for (i = 0; i < priv->columns->len; i++) {
			GObject *obj;
			obj = g_array_index (priv->columns, GObject*, i);
			g_object_unref (obj);
		}
		g_array_free (priv->columns, TRUE);
		priv->columns = NULL;
	}
}

static void
gda_data_pivot_dispose (GObject *object)
{
	GdaDataPivot *model = (GdaDataPivot *) object;
	GdaDataPivotPrivate *priv = gda_data_pivot_get_instance_private (model);

	g_return_if_fail (GDA_IS_DATA_PIVOT (model));

	/* free memory */
	clean_previous_population (model);

	if (priv->row_fields) {
		guint i;
		for (i = 0; i < priv->row_fields->len; i++) {
			gchar *tmp;
			tmp = g_array_index (priv->row_fields, gchar*, i);
			g_free (tmp);
		}
		g_array_free (priv->row_fields, TRUE);
		priv->row_fields = NULL;
	}

	if (priv->column_fields) {
		guint i;
		for (i = 0; i < priv->column_fields->len; i++) {
			gchar *tmp;
			tmp = g_array_index (priv->column_fields, gchar*, i);
			g_free (tmp);
		}
		g_array_free (priv->column_fields, TRUE);
		priv->column_fields = NULL;
	}

	if (priv->data_fields) {
		guint i;
		for (i = 0; i < priv->data_fields->len; i++) {
			gchar *tmp;
			tmp = g_array_index (priv->data_fields, gchar*, i);
			g_free (tmp);
		}
		g_array_free (priv->data_fields, TRUE);
		priv->data_fields = NULL;
	}

	if (priv->data_aggregates) {
		g_array_free (priv->data_aggregates, TRUE);
		priv->data_aggregates = NULL;
	}

	if (priv->vcnc) {
		g_object_unref (priv->vcnc);
		priv->vcnc = NULL;
	}

	if (priv->model) {
		g_object_unref (priv->model);
		priv->model = NULL;
	}

	/* chain to parent class */
	G_OBJECT_CLASS (gda_data_pivot_parent_class)->dispose (object);
}

/* module error */
GQuark gda_data_pivot_error_quark (void)
{
        static GQuark quark;
        if (!quark)
                quark = g_quark_from_static_string ("gda_data_pivot_error");
        return quark;
}

static void
gda_data_pivot_set_property (GObject *object,
			     guint param_id,
			     const GValue *value,
			     GParamSpec *pspec)
{
	GdaDataPivot *model;

	model = GDA_DATA_PIVOT (object);
	GdaDataPivotPrivate *priv = gda_data_pivot_get_instance_private (model);
	switch (param_id) {
	case PROP_MODEL: {
		GdaDataModel *mod = g_value_get_object (value);

		clean_previous_population (model);

		if (mod) {
			g_return_if_fail (GDA_IS_DATA_MODEL (mod));

                              if (priv->model) {
				if (priv->vcnc)
					gda_vconnection_data_model_remove (GDA_VCONNECTION_DATA_MODEL (priv->vcnc),
									   TABLE_NAME, NULL);
				g_object_unref (priv->model);
			}

			priv->model = mod;
			g_object_ref (mod);
		}
		break;
	}
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}
}

static void
gda_data_pivot_get_property (GObject *object,
			     guint param_id,
			     GValue *value,
			     GParamSpec *pspec)
{
	GdaDataPivot *model;

	model = GDA_DATA_PIVOT (object);
	GdaDataPivotPrivate *priv = gda_data_pivot_get_instance_private (model);
	switch (param_id) {
	case PROP_MODEL:
		g_value_set_object (value, G_OBJECT (priv->model));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}
}

/**
 * gda_data_pivot_new:
 * @model: (nullable): a #GdaDataModel to analyse data from, or %NULL
 *
 * Creates a new #GdaDataModel which will contain analysed data from @model.
 *
 * Returns: (transfer full): a pointer to the newly created #GdaDataModel.
 */
GdaDataModel *
gda_data_pivot_new (GdaDataModel *model)
{
	GdaDataPivot *retmodel;

	g_return_val_if_fail (!model || GDA_IS_DATA_MODEL (model), NULL);
	
	retmodel = g_object_new (GDA_TYPE_DATA_PIVOT,
				 "model", model, NULL);

	return GDA_DATA_MODEL (retmodel);
}

/*
 * GdaDataModel interface implementation
 */
static gint
gda_data_pivot_get_n_rows (GdaDataModel *model)
{
	GdaDataPivot *pivot;
	g_return_val_if_fail (GDA_IS_DATA_PIVOT (model), -1);
	pivot = GDA_DATA_PIVOT (model);
	GdaDataPivotPrivate *priv = gda_data_pivot_get_instance_private (pivot);
	g_return_val_if_fail (priv, -1);

	if (priv->results)
		return gda_data_model_get_n_rows (priv->results);
	return -1;
}

static gint
gda_data_pivot_get_n_columns (GdaDataModel *model)
{
	GdaDataPivot *pivot;
	g_return_val_if_fail (GDA_IS_DATA_PIVOT (model), 0);
	pivot = GDA_DATA_PIVOT (model);
	GdaDataPivotPrivate *priv = gda_data_pivot_get_instance_private (pivot);
	g_return_val_if_fail (priv, 0);
	
	if (priv->columns)
		return (gint) priv->columns->len;
	return 0;
}

static GdaColumn *
gda_data_pivot_describe_column (GdaDataModel *model, gint col)
{
	GdaDataPivot *pivot;
	GdaColumn *column = NULL;
	g_return_val_if_fail (GDA_IS_DATA_PIVOT (model), NULL);
	pivot = GDA_DATA_PIVOT (model);
	GdaDataPivotPrivate *priv = gda_data_pivot_get_instance_private (pivot);
	g_return_val_if_fail (priv, NULL);

	if (priv->columns && (col < (gint) priv->columns->len))
		column = g_array_index (priv->columns, GdaColumn*, col);
	else {
		if (priv->columns->len > 0)
			g_warning ("Column %d out of range (0-%d)", col,
				   (gint) priv->columns->len);
		else
			g_warning ("No column defined");
	}

	return column;
}

static GdaDataModelAccessFlags
gda_data_pivot_get_access_flags (GdaDataModel *model)
{
	GdaDataPivot *pivot;

	g_return_val_if_fail (GDA_IS_DATA_PIVOT (model), 0);
	pivot = GDA_DATA_PIVOT (model);
	GdaDataPivotPrivate *priv = gda_data_pivot_get_instance_private (pivot);
	g_return_val_if_fail (priv, 0);
	
	return GDA_DATA_MODEL_ACCESS_RANDOM;
}

static const GValue *
gda_data_pivot_get_value_at (GdaDataModel *model, gint col, gint row, GError **error)
{
	GdaDataPivot *pivot;

	g_return_val_if_fail (GDA_IS_DATA_PIVOT (model), NULL);
	pivot = GDA_DATA_PIVOT (model);
	GdaDataPivotPrivate *priv = gda_data_pivot_get_instance_private (pivot);
	g_return_val_if_fail (priv, NULL);
	g_return_val_if_fail (priv->model, NULL);
	g_return_val_if_fail (row >= 0, NULL);
	g_return_val_if_fail (col >= 0, NULL);

	if (! priv->results) {
		g_set_error (error, GDA_DATA_PIVOT_ERROR, GDA_DATA_PIVOT_USAGE_ERROR,
			     "%s", _("Pivot model not populated"));
		return NULL;
	}
	return gda_data_model_get_value_at (priv->results, col, row, error);
}

static GValue *
aggregate_get_empty_value (GdaDataPivotAggregate aggregate)
{
	GValue *value;
	switch (aggregate) {
	case GDA_DATA_PIVOT_MIN:
	case GDA_DATA_PIVOT_MAX:
	case GDA_DATA_PIVOT_AVG:
	case GDA_DATA_PIVOT_SUM:
		return gda_value_new_null ();
	case GDA_DATA_PIVOT_COUNT:
		value = gda_value_new (G_TYPE_UINT);
		g_value_set_uint (value, 0);
		return value;
		break;
	default:
		return gda_value_new_null ();
	}
}

static gboolean
aggregate_handle_gint (CellData *cdata, gint val)
{
	if (cdata->data_value) {
		gint eval = 0;
		if (G_VALUE_TYPE (cdata->data_value) == G_TYPE_INT)
			eval = g_value_get_int (cdata->data_value);
		switch (cdata->aggregate) {
		case GDA_DATA_PIVOT_MIN:
			if (eval > val)
				g_value_set_int (cdata->data_value, val);
			break;
		case GDA_DATA_PIVOT_MAX:
			if (eval < val)
				g_value_set_int (cdata->data_value, val);
			break;
		case GDA_DATA_PIVOT_SUM: {
			gint64 tmp;
			tmp = eval + val;
			if ((tmp > G_MAXINT) || (tmp < G_MININT))
				g_set_error (&(cdata->error),
					     GDA_DATA_PIVOT_ERROR, GDA_DATA_PIVOT_OVERFLOW_ERROR,
					     "%s", _("Integer overflow"));
			else
				g_value_set_int (cdata->data_value, (gint) tmp);
			break;
		}
		case GDA_DATA_PIVOT_AVG: {
			gint64 tmp;
			tmp = g_value_get_int64 (cdata->data_value) + val;
			if ((tmp > G_MAXINT64) || (tmp < G_MININT64))
				/* FIXME: how to handle overflow detection ? */
				g_set_error (&(cdata->error),
					     GDA_DATA_PIVOT_ERROR, GDA_DATA_PIVOT_OVERFLOW_ERROR,
					     "%s", _("Integer overflow"));
			else
				g_value_set_int64 (cdata->data_value, tmp);
			break;
		}
		default:
			return FALSE;
		}
	}
	else {
		/* new initial value */
		switch (cdata->aggregate) {
		case GDA_DATA_PIVOT_MIN:
		case GDA_DATA_PIVOT_MAX:
		case GDA_DATA_PIVOT_SUM:
			cdata->data_value = gda_value_new (G_TYPE_INT);
			g_value_set_int (cdata->data_value, val);
			break;
		case GDA_DATA_PIVOT_AVG:
			cdata->data_value = gda_value_new (G_TYPE_INT64);
			g_value_set_int64 (cdata->data_value, (gint64) val);
			break;
		default:
			return FALSE;
		}
	}
	cdata->nvalues ++;
	return TRUE;
}

static gboolean
aggregate_handle_guint (CellData *cdata, guint val)
{
	if (cdata->data_value) {
		guint eval = 0;
		if (G_VALUE_TYPE (cdata->data_value) == G_TYPE_UINT)
			eval = g_value_get_uint (cdata->data_value);
		switch (cdata->aggregate) {
		case GDA_DATA_PIVOT_MIN:
			if (eval > val)
				g_value_set_uint (cdata->data_value, val);
			break;
		case GDA_DATA_PIVOT_MAX:
			if (eval < val)
				g_value_set_uint (cdata->data_value, val);
			break;
		case GDA_DATA_PIVOT_SUM: {
			guint64 tmp;
			tmp = eval + val;
			if (tmp > G_MAXUINT)
				g_set_error (&(cdata->error),
					     GDA_DATA_PIVOT_ERROR, GDA_DATA_PIVOT_OVERFLOW_ERROR,
					     "%s", _("Integer overflow"));
			else
				g_value_set_uint (cdata->data_value, (guint) tmp);
			break;
		}
		case GDA_DATA_PIVOT_AVG: {
			guint64 tmp;
			tmp = g_value_get_uint64 (cdata->data_value) + val;
			if (tmp > G_MAXUINT64)
				/* FIXME: how to handle overflow detection ? */
				g_set_error (&(cdata->error),
					     GDA_DATA_PIVOT_ERROR, GDA_DATA_PIVOT_OVERFLOW_ERROR,
					     "%s", _("Integer overflow"));
			else
				g_value_set_uint64 (cdata->data_value, tmp);
			break;
		}
		default:
			return FALSE;
		}
	}
	else {
		/* new initial value */
		switch (cdata->aggregate) {
		case GDA_DATA_PIVOT_MIN:
		case GDA_DATA_PIVOT_MAX:
		case GDA_DATA_PIVOT_SUM:
			cdata->data_value = gda_value_new (G_TYPE_UINT);
			g_value_set_uint (cdata->data_value, val);
			break;
		case GDA_DATA_PIVOT_AVG:
			cdata->data_value = gda_value_new (G_TYPE_UINT64);
			g_value_set_uint64 (cdata->data_value, (guint64) val);
			break;
		default:
			return FALSE;
		}
	}
	cdata->nvalues ++;
	return TRUE;
}

static gboolean
aggregate_handle_gint64 (CellData *cdata, gint64 val)
{
	if (cdata->data_value) {
		gint64 eval = 0;
		if (G_VALUE_TYPE (cdata->data_value) == G_TYPE_INT64)
			eval = g_value_get_int64 (cdata->data_value);
		switch (cdata->aggregate) {
		case GDA_DATA_PIVOT_MIN:
			if (eval > val)
				g_value_set_int64 (cdata->data_value, val);
			break;
		case GDA_DATA_PIVOT_MAX:
			if (eval < val)
				g_value_set_int64 (cdata->data_value, val);
			break;
		case GDA_DATA_PIVOT_SUM:
		case GDA_DATA_PIVOT_AVG: {
			gint64 tmp;
			tmp = eval + val;
			if ((tmp > G_MAXINT64) || (tmp < G_MININT64))
				/* FIXME: how to handle overflow detection ? */
				g_set_error (&(cdata->error),
					     GDA_DATA_PIVOT_ERROR, GDA_DATA_PIVOT_OVERFLOW_ERROR,
					     "%s", _("Integer overflow"));
			else
				g_value_set_int64 (cdata->data_value, (gint64) tmp);
			break;
		}
		default:
			return FALSE;
		}
	}
	else {
		/* new initial value */
		switch (cdata->aggregate) {
		case GDA_DATA_PIVOT_MIN:
		case GDA_DATA_PIVOT_MAX:
		case GDA_DATA_PIVOT_SUM:
		case GDA_DATA_PIVOT_AVG:
			cdata->data_value = gda_value_new (G_TYPE_INT64);
			g_value_set_int64 (cdata->data_value, val);
			break;
		default:
			return FALSE;
		}
	}
	cdata->nvalues ++;
	return TRUE;
}

static gboolean
aggregate_handle_guint64 (CellData *cdata, guint val)
{
	if (cdata->data_value) {
		guint64 eval = 0;
		if (G_VALUE_TYPE (cdata->data_value) == G_TYPE_UINT64)
			eval = g_value_get_uint64 (cdata->data_value);
		switch (cdata->aggregate) {
		case GDA_DATA_PIVOT_MIN:
			if (eval > val)
				g_value_set_uint64 (cdata->data_value, val);
			break;
		case GDA_DATA_PIVOT_MAX:
			if (eval < val)
				g_value_set_uint64 (cdata->data_value, val);
			break;
		case GDA_DATA_PIVOT_SUM:
		case GDA_DATA_PIVOT_AVG: {
			guint64 tmp;
			tmp = eval + val;
			if (tmp > G_MAXUINT64)
				/* FIXME: how to handle overflow detection ? */
				g_set_error (&(cdata->error),
					     GDA_DATA_PIVOT_ERROR, GDA_DATA_PIVOT_OVERFLOW_ERROR,
					     "%s", _("Integer overflow"));
			else
				g_value_set_uint64 (cdata->data_value, tmp);
			break;
		}
		default:
			return FALSE;
		}
	}
	else {
		/* new initial value */
		switch (cdata->aggregate) {
		case GDA_DATA_PIVOT_MIN:
		case GDA_DATA_PIVOT_MAX:
		case GDA_DATA_PIVOT_SUM:
		case GDA_DATA_PIVOT_AVG:
			cdata->data_value = gda_value_new (G_TYPE_UINT64);
			g_value_set_uint64 (cdata->data_value, val);
			break;
		default:
			return FALSE;
		}
	}
	cdata->nvalues ++;
	return TRUE;
}

static gboolean
aggregate_handle_float (CellData *cdata, gfloat val)
{
	if (cdata->data_value) {
		gfloat eval = 0;
		if (G_VALUE_TYPE (cdata->data_value) == G_TYPE_FLOAT)
			eval = g_value_get_float (cdata->data_value);
		switch (cdata->aggregate) {
		case GDA_DATA_PIVOT_MIN:
			if (eval > val)
				g_value_set_float (cdata->data_value, val);
			break;
		case GDA_DATA_PIVOT_MAX:
			if (eval < val)
				g_value_set_float (cdata->data_value, val);
			break;
		case GDA_DATA_PIVOT_SUM: {
			gdouble tmp;
			tmp = eval + val;
			if ((tmp > G_MAXDOUBLE) || (tmp < G_MINDOUBLE))
				g_set_error (&(cdata->error),
					     GDA_DATA_PIVOT_ERROR, GDA_DATA_PIVOT_OVERFLOW_ERROR,
					     "%s", _("Float value overflow"));
			else
				g_value_set_float (cdata->data_value, (gfloat) tmp);
			break;
		}
		case GDA_DATA_PIVOT_AVG: {
			gdouble tmp;
			tmp = g_value_get_double (cdata->data_value) + val;
			if ((tmp > G_MAXDOUBLE) || (tmp < G_MINDOUBLE))
				g_set_error (&(cdata->error),
					     GDA_DATA_PIVOT_ERROR, GDA_DATA_PIVOT_OVERFLOW_ERROR,
					     "%s", _("Float value overflow"));
			else
				g_value_set_double (cdata->data_value, tmp);
			break;
		}
		default:
			return FALSE;
		}
	}
	else {
		/* new initial value */
		switch (cdata->aggregate) {
		case GDA_DATA_PIVOT_MIN:
		case GDA_DATA_PIVOT_MAX:
		case GDA_DATA_PIVOT_SUM:
			cdata->data_value = gda_value_new (G_TYPE_FLOAT);
			g_value_set_float (cdata->data_value, val);
			break;
		case GDA_DATA_PIVOT_AVG:
			cdata->data_value = gda_value_new (G_TYPE_DOUBLE);
			g_value_set_double (cdata->data_value, (gdouble) val);
			break;
		default:
			return FALSE;
		}
	}
	cdata->nvalues ++;
	return TRUE;
}

static gboolean
aggregate_handle_double (CellData *cdata, gdouble val)
{
	if (cdata->data_value) {
		gdouble eval = 0;
		if (G_VALUE_TYPE (cdata->data_value) == G_TYPE_DOUBLE)
			eval = g_value_get_double (cdata->data_value);
		switch (cdata->aggregate) {
		case GDA_DATA_PIVOT_MIN:
			if (eval > val)
				g_value_set_double (cdata->data_value, val);
			break;
		case GDA_DATA_PIVOT_MAX:
			if (eval < val)
				g_value_set_double (cdata->data_value, val);
			break;
		case GDA_DATA_PIVOT_SUM:
		case GDA_DATA_PIVOT_AVG: {
			gdouble tmp;
			tmp = eval + val;
			if ((tmp > G_MAXDOUBLE) || (tmp < G_MINDOUBLE))
				/* FIXME: how to handle overflow detection ? */
				g_set_error (&(cdata->error),
					     GDA_DATA_PIVOT_ERROR, GDA_DATA_PIVOT_OVERFLOW_ERROR,
					     "%s", _("Double value overflow"));
			else
				g_value_set_double (cdata->data_value, (gdouble) tmp);
			break;
		}
		default:
			return FALSE;
		}
	}
	else {
		/* new initial value */
		switch (cdata->aggregate) {
		case GDA_DATA_PIVOT_MIN:
		case GDA_DATA_PIVOT_MAX:
		case GDA_DATA_PIVOT_SUM:
		case GDA_DATA_PIVOT_AVG:
			cdata->data_value = gda_value_new (G_TYPE_DOUBLE);
			g_value_set_double (cdata->data_value, val);
			break;
		default:
			return FALSE;
		}
	}
	cdata->nvalues ++;
	return TRUE;
}

static gboolean
aggregate_handle_char (CellData *cdata,
		       gint8 val
		       )
{
	if (cdata->data_value) {
		gint8 eval = 0;
		if (G_VALUE_TYPE (cdata->data_value) == G_TYPE_CHAR)
			eval = g_value_get_schar (cdata->data_value);
		switch (cdata->aggregate) {
		case GDA_DATA_PIVOT_MIN:
			if (eval > val)
				g_value_set_schar (cdata->data_value, val);
			break;
		case GDA_DATA_PIVOT_MAX:
			if (eval < val)
				g_value_set_schar (cdata->data_value, val);
			break;
		case GDA_DATA_PIVOT_SUM: {
			gint tmp;
			tmp = eval + val;
			if ((tmp > G_MAXINT8) || (tmp < G_MININT8))
				g_set_error (&(cdata->error),
					     GDA_DATA_PIVOT_ERROR, GDA_DATA_PIVOT_OVERFLOW_ERROR,
					     "%s", _("Integer overflow"));
			else
				g_value_set_schar (cdata->data_value, (gint8) tmp);
			break;
		}
		case GDA_DATA_PIVOT_AVG: {
			gint64 tmp;
			tmp = g_value_get_int64 (cdata->data_value) + val;
			if ((tmp > G_MAXINT64) || (tmp < G_MININT64))
				/* FIXME: how to handle overflow detection ? */
				g_set_error (&(cdata->error),
					     GDA_DATA_PIVOT_ERROR, GDA_DATA_PIVOT_OVERFLOW_ERROR,
					     "%s", _("Integer overflow"));
			else
				g_value_set_int64 (cdata->data_value, tmp);
			break;
		}
		default:
			return FALSE;
		}
	}
	else {
		/* new initial value */
		switch (cdata->aggregate) {
		case GDA_DATA_PIVOT_MIN:
		case GDA_DATA_PIVOT_MAX:
		case GDA_DATA_PIVOT_SUM:
			cdata->data_value = gda_value_new (G_TYPE_CHAR);
			g_value_set_schar (cdata->data_value, val);
			break;
		case GDA_DATA_PIVOT_AVG:
			cdata->data_value = gda_value_new (G_TYPE_INT64);
			g_value_set_int64 (cdata->data_value, (gint64) val);
			break;
		default:
			return FALSE;
		}
	}
	cdata->nvalues ++;
	return TRUE;
}

static gboolean
aggregate_handle_uchar (CellData *cdata, guchar val)
{
	if (cdata->data_value) {
		guchar eval = 0;
		if (G_VALUE_TYPE (cdata->data_value) == G_TYPE_UCHAR)
			eval = g_value_get_uchar (cdata->data_value);
		switch (cdata->aggregate) {
		case GDA_DATA_PIVOT_MIN:
			if (eval > val)
				g_value_set_uchar (cdata->data_value, val);
			break;
		case GDA_DATA_PIVOT_MAX:
			if (eval < val)
				g_value_set_uchar (cdata->data_value, val);
			break;
		case GDA_DATA_PIVOT_SUM: {
			guint tmp;
			tmp = eval + val;
			if (tmp > G_MAXUINT8)
				g_set_error (&(cdata->error),
					     GDA_DATA_PIVOT_ERROR, GDA_DATA_PIVOT_OVERFLOW_ERROR,
					     "%s", _("Integer overflow"));
			else
				g_value_set_uchar (cdata->data_value, (guchar) tmp);
			break;
		}
		case GDA_DATA_PIVOT_AVG: {
			guint64 tmp;
			tmp = g_value_get_uint64 (cdata->data_value) + val;
			if (tmp > G_MAXUINT64)
				/* FIXME: how to handle overflow detection ? */
				g_set_error (&(cdata->error),
					     GDA_DATA_PIVOT_ERROR, GDA_DATA_PIVOT_OVERFLOW_ERROR,
					     "%s", _("Integer overflow"));
			else
				g_value_set_uint64 (cdata->data_value, tmp);
			break;
		}
		default:
			return FALSE;
		}
	}
	else {
		/* new initial value */
		switch (cdata->aggregate) {
		case GDA_DATA_PIVOT_MIN:
		case GDA_DATA_PIVOT_MAX:
		case GDA_DATA_PIVOT_SUM:
			cdata->data_value = gda_value_new (G_TYPE_UCHAR);
			g_value_set_uchar (cdata->data_value, val);
			break;
		case GDA_DATA_PIVOT_AVG:
			cdata->data_value = gda_value_new (G_TYPE_UINT64);
			g_value_set_uint64 (cdata->data_value, (guint64) val);
			break;
		default:
			return FALSE;
		}
	}
	cdata->nvalues ++;
	return TRUE;
}

static gboolean
aggregate_handle_short (CellData *cdata, gshort val)
{
	if (cdata->data_value) {
		gshort eval = 0;
		if (G_VALUE_TYPE (cdata->data_value) == GDA_TYPE_SHORT)
			eval = gda_value_get_short (cdata->data_value);
		switch (cdata->aggregate) {
		case GDA_DATA_PIVOT_MIN:
			if (eval > val)
				gda_value_set_short (cdata->data_value, val);
			break;
		case GDA_DATA_PIVOT_MAX:
			if (eval < val)
				gda_value_set_short (cdata->data_value, val);
			break;
		case GDA_DATA_PIVOT_SUM: {
			gint tmp;
			tmp = eval + val;
			if ((tmp > G_MAXSHORT) || (tmp < G_MINSHORT))
				g_set_error (&(cdata->error),
					     GDA_DATA_PIVOT_ERROR, GDA_DATA_PIVOT_OVERFLOW_ERROR,
					     "%s", _("Integer overflow"));
			else
				gda_value_set_short (cdata->data_value, (gshort) tmp);
			break;
		}
		case GDA_DATA_PIVOT_AVG: {
			gint64 tmp;
			tmp = g_value_get_int64 (cdata->data_value) + val;
			if ((tmp > G_MAXINT64) || (tmp < G_MININT64))
				/* FIXME: how to handle overflow detection ? */
				g_set_error (&(cdata->error),
					     GDA_DATA_PIVOT_ERROR, GDA_DATA_PIVOT_OVERFLOW_ERROR,
					     "%s", _("Integer overflow"));
			else
				g_value_set_int64 (cdata->data_value, tmp);
			break;
		}
		default:
			return FALSE;
		}
	}
	else {
		/* new initial value */
		switch (cdata->aggregate) {
		case GDA_DATA_PIVOT_MIN:
		case GDA_DATA_PIVOT_MAX:
		case GDA_DATA_PIVOT_SUM:
			cdata->data_value = gda_value_new (GDA_TYPE_SHORT);
			gda_value_set_short (cdata->data_value, val);
			break;
		case GDA_DATA_PIVOT_AVG:
			cdata->data_value = gda_value_new (G_TYPE_INT64);
			g_value_set_int64 (cdata->data_value, (gint64) val);
			break;
		default:
			return FALSE;
		}
	}
	cdata->nvalues ++;
	return TRUE;
}

static gboolean
aggregate_handle_ushort (CellData *cdata, gushort val)
{
	if (cdata->data_value) {
		gushort eval = 0;
		if (G_VALUE_TYPE (cdata->data_value) == GDA_TYPE_USHORT)
			eval = gda_value_get_ushort (cdata->data_value);
		switch (cdata->aggregate) {
		case GDA_DATA_PIVOT_MIN:
			if (eval > val)
				gda_value_set_ushort (cdata->data_value, val);
			break;
		case GDA_DATA_PIVOT_MAX:
			if (eval < val)
				gda_value_set_ushort (cdata->data_value, val);
			break;
		case GDA_DATA_PIVOT_SUM: {
			guint tmp;
			tmp = eval + val;
			if (tmp > G_MAXUSHORT)
				g_set_error (&(cdata->error),
					     GDA_DATA_PIVOT_ERROR, GDA_DATA_PIVOT_OVERFLOW_ERROR,
					     "%s", _("Integer overflow"));
			else
				gda_value_set_ushort (cdata->data_value, (gushort) tmp);
			break;
		}
		case GDA_DATA_PIVOT_AVG: {
			guint64 tmp;
			tmp = g_value_get_uint64 (cdata->data_value) + val;
			if (tmp > G_MAXUINT64)
				/* FIXME: how to handle overflow detection ? */
				g_set_error (&(cdata->error),
					     GDA_DATA_PIVOT_ERROR, GDA_DATA_PIVOT_OVERFLOW_ERROR,
					     "%s", _("Integer overflow"));
			else
				g_value_set_uint64 (cdata->data_value, tmp);
			break;
		}
		default:
			return FALSE;
		}
	}
	else {
		/* new initial value */
		switch (cdata->aggregate) {
		case GDA_DATA_PIVOT_MIN:
		case GDA_DATA_PIVOT_MAX:
		case GDA_DATA_PIVOT_SUM:
			cdata->data_value = gda_value_new (GDA_TYPE_USHORT);
			gda_value_set_ushort (cdata->data_value, val);
			break;
		case GDA_DATA_PIVOT_AVG:
			cdata->data_value = gda_value_new (G_TYPE_UINT64);
			g_value_set_uint64 (cdata->data_value, (guint64) val);
			break;
		default:
			return FALSE;
		}
	}
	cdata->nvalues ++;
	return TRUE;
}

/*
 * Returns: %TRUE if @new_value_has been handled (even if it created cdata->error),
 * or %FALSE if cdata's aggregate needs to store all the data
 */
static gboolean
aggregate_handle_new_value (CellData *cdata, const GValue *new_value)
{
	if (cdata->error)
		return TRUE;
	else if (cdata->values)
		return FALSE;

	/* chack data type */
	if (G_VALUE_TYPE (new_value) != cdata->gtype) {
		if (!cdata->error)
			g_set_error (&(cdata->error),
				     GDA_DATA_PIVOT_ERROR, GDA_DATA_PIVOT_INTERNAL_ERROR,
				     "%s", _("Inconsistent data type"));
		return TRUE;
	}

	if (cdata->aggregate == GDA_DATA_PIVOT_COUNT) {
		if (cdata->data_value) {
			guint64 tmp;
			tmp = g_value_get_uint (cdata->data_value) + 1;
			if (tmp >= G_MAXUINT)
				g_set_error (&(cdata->error),
					     GDA_DATA_PIVOT_ERROR, GDA_DATA_PIVOT_OVERFLOW_ERROR,
					     "%s", _("Integer overflow"));
			else
				g_value_set_uint (cdata->data_value, (guint) tmp);
		}
		else {
			cdata->data_value = gda_value_new (G_TYPE_UINT);
			g_value_set_uint (cdata->data_value, 1);
		}
		return TRUE;
	}
	else if (cdata->gtype == G_TYPE_INT)
		return aggregate_handle_gint (cdata, g_value_get_int (new_value));
	else if (cdata->gtype == G_TYPE_UINT)
		return aggregate_handle_guint (cdata, g_value_get_uint (new_value));
	else if (cdata->gtype == G_TYPE_INT64)
		return aggregate_handle_gint64 (cdata, g_value_get_int64 (new_value));
	else if (cdata->gtype == G_TYPE_UINT64)
		return aggregate_handle_guint64 (cdata, g_value_get_uint64 (new_value));
	else if (cdata->gtype == G_TYPE_FLOAT)
		return aggregate_handle_float (cdata, g_value_get_float (new_value));
	else if (cdata->gtype == G_TYPE_DOUBLE)
		return aggregate_handle_double (cdata, g_value_get_double (new_value));
	else if (cdata->gtype == G_TYPE_CHAR)
		return aggregate_handle_char (cdata, g_value_get_schar (new_value));
	else if (cdata->gtype == G_TYPE_UCHAR)
		return aggregate_handle_uchar (cdata, g_value_get_uchar (new_value));
	else if (cdata->gtype == GDA_TYPE_SHORT)
		return aggregate_handle_short (cdata, gda_value_get_short (new_value));
	else if (cdata->gtype == GDA_TYPE_USHORT)
		return aggregate_handle_ushort (cdata, gda_value_get_ushort (new_value));
	else {
		/* incompatible data type for this kind of operation */
		if (!cdata->error)
			g_set_error (&(cdata->error),
				     GDA_DATA_PIVOT_ERROR, GDA_DATA_PIVOT_INTERNAL_ERROR,
				     "%s", _("Data type does not support requested computation"));
		return TRUE;
	}
}

/*
 * Sets cdata->computed_value to a #GValue
 * if an error occurs then cdata->computed_value is set to %NULL
 *
 * Also frees cdata->values.
 */
static void
cell_data_compute_aggregate (CellData *cdata)
{
	if (cdata->data_value) {
		/* finish computation */
		if (cdata->error) {
			gda_value_free (cdata->data_value);
			cdata->data_value = NULL;
		}
		else if (cdata->aggregate == GDA_DATA_PIVOT_AVG) {
			gdouble dval;
			if ((cdata->gtype == G_TYPE_INT) || (cdata->gtype == G_TYPE_INT64) ||
			    (cdata->gtype == G_TYPE_CHAR) || (cdata->gtype == GDA_TYPE_SHORT)) {
				gint64 val;
				g_assert (G_VALUE_TYPE (cdata->data_value) == G_TYPE_INT64);
				val = g_value_get_int64 (cdata->data_value);
				dval = val / (gdouble) cdata->nvalues;
			}
			else if ((cdata->gtype == G_TYPE_UINT) || (cdata->gtype == G_TYPE_UINT64) ||
				 (cdata->gtype == G_TYPE_UCHAR) || (cdata->gtype == GDA_TYPE_USHORT)) {
				guint64 val;
				g_assert (G_VALUE_TYPE (cdata->data_value) == G_TYPE_UINT64);
				val = g_value_get_uint64 (cdata->data_value);
				dval = val / (gdouble) cdata->nvalues;
			}
			else if (cdata->gtype == G_TYPE_FLOAT)
				dval = g_value_get_float (cdata->data_value) / (gdouble) cdata->nvalues;
			else if (cdata->gtype == G_TYPE_DOUBLE)
				dval = g_value_get_double (cdata->data_value) / (gdouble) cdata->nvalues;
			else
				g_assert_not_reached ();
				
			gda_value_free (cdata->data_value);
			cdata->data_value = gda_value_new (G_TYPE_DOUBLE);
			g_value_set_double (cdata->data_value, dval);
		}
	}
	else if (cdata->values) {
		TO_IMPLEMENT;
		guint i;
		for (i = 0; i < cdata->values->len; i++) {
			GValue *value;
			value = g_array_index (cdata->values, GValue*, i);
			gda_value_free (value);
		}
		g_array_free (cdata->values, TRUE);
		cdata->values = NULL;
	}
}

static GdaSqlStatement *
parse_field_spec (GdaDataPivot *pivot, const gchar *field, const gchar *alias, GError **error)
{
	GdaSqlParser *parser;
	gchar *sql;
	GdaStatement *stmt;
	const gchar *remain;
	GError *lerror = NULL;
	GdaDataPivotPrivate *priv = gda_data_pivot_get_instance_private (pivot);

	g_return_val_if_fail (field, FALSE);

	if (! bind_source_model (pivot, error))
		return NULL;

	parser = gda_connection_create_parser (priv->vcnc);
	g_assert (parser);
	if (alias && *alias) {
		gchar *tmp, *ptr;
		tmp = g_strdup (alias);
		for (ptr = tmp; *ptr; ptr++) {
			if (g_ascii_isdigit (*ptr)) {
				if (ptr == tmp)
					*ptr = '_';
			}
			else if (! g_ascii_isalpha (*ptr))
				*ptr = '_';
		}
		sql = g_strdup_printf ("SELECT %s AS %s FROM " TABLE_NAME, field, tmp);
		g_free (tmp);
	}
	else
		sql = g_strdup_printf ("SELECT %s FROM " TABLE_NAME, field);
	stmt = gda_sql_parser_parse_string (parser, sql, &remain, &lerror);
	g_free (sql);
	g_object_unref (parser);
	if (!stmt || remain) {
		g_set_error (error, GDA_DATA_PIVOT_ERROR, GDA_DATA_PIVOT_FIELD_FORMAT_ERROR,
			     _("Wrong field format error: %s"),
			     lerror && lerror->message ? lerror->message : _("No detail"));
		g_clear_error (&lerror);
		return NULL;
	}
	
	/* test syntax: a valid SQL expression is required */
	GdaSqlStatement *sqlst = NULL;
	g_object_get ((GObject*) stmt, "structure", &sqlst, NULL);

	if (sqlst->stmt_type != GDA_SQL_STATEMENT_SELECT) {
		g_object_unref (stmt);
		gda_sql_statement_free (sqlst);
		g_set_error (error, GDA_DATA_PIVOT_ERROR, GDA_DATA_PIVOT_FIELD_FORMAT_ERROR,
			     "%s", _("Wrong field format"));
		return NULL;
	}
	
	/*
	  gchar *tmp = gda_sql_statement_serialize (sqlst);
	  g_print ("SQLST [%p, %s]\n", sqlst, tmp);
	  g_free (tmp);
	*/

	/* further tests */
	GdaDataModel *model;
	model = gda_connection_statement_execute_select (priv->vcnc, stmt, NULL, &lerror);
	g_object_unref (stmt);
	if (!model) {
		g_set_error (error, GDA_DATA_PIVOT_ERROR, GDA_DATA_PIVOT_FIELD_FORMAT_ERROR,
			     _("Wrong field format error: %s"),
			     lerror && lerror->message ? lerror->message : _("No detail"));
		g_clear_error (&lerror);
		return NULL;
	}
	/*
	  g_print ("================= Pivot source:\n");
	  gda_data_model_dump (model, NULL);
	*/

	g_object_unref (model);
	return sqlst;
}

/**
 * gda_data_pivot_add_field:
 * @pivot: a #GdaDataPivot object
 * @field_type: the type of field to add
 * @field: the field description, see below
 * @alias: (nullable): the field alias, or %NULL
 * @error: (nullable): ta place to store errors, or %NULL
 *
 * Specifies that @field has to be included in the analysis.
 * @field is a field specification with the following accepted syntaxes:
 * <itemizedlist>
 *   <listitem><para>a column name in the source data model (see <link linkend="gda-data-model-get-column-index">gda_data_model_get_column_index()</link>); or</para></listitem>
 *   <listitem><para>an SQL expression involving a column name in the source data model, for example:
 *   <programlisting>
 * price
 * firstname || ' ' || lastname 
 * nb BETWEEN 5 AND 10</programlisting>
 * </para></listitem>
 * </itemizedlist>
 *
 * It is also possible to specify several fields to be added, while separating them by a comma (in effect
 * still forming a valid SQL syntax).
 *
 * Returns: %TRUE if no error occurred
 *
 * Since: 5.0
 */
gboolean
gda_data_pivot_add_field (GdaDataPivot *pivot, GdaDataPivotFieldType field_type,
			  const gchar *field, const gchar *alias, GError **error)
{
	gchar *tmp;
	GError *lerror = NULL;

	g_return_val_if_fail (GDA_IS_DATA_PIVOT (pivot), FALSE);

	GdaDataPivotPrivate *priv = gda_data_pivot_get_instance_private (pivot);

	g_return_val_if_fail (field, FALSE);

	GdaSqlStatement *sqlst;
	sqlst = parse_field_spec (pivot, field, alias, error);
	if (! sqlst)
		return FALSE;
	
	GArray *array;
	if (field_type == GDA_DATA_PIVOT_FIELD_ROW) {
		if (! priv->row_fields)
			priv->row_fields = g_array_new (FALSE, FALSE, sizeof (gchar*));
		array = priv->row_fields;
	}
	else {
		if (! priv->column_fields)
			priv->column_fields = g_array_new (FALSE, FALSE, sizeof (gchar*));
		array = priv->column_fields;
	}

	GdaSqlStatementSelect *sel;
	GSList *sf_list;
	sel = (GdaSqlStatementSelect*) sqlst->contents;
	for (sf_list = sel->expr_list; sf_list; sf_list = sf_list->next) {
		GdaSqlBuilder *b;
		GdaSqlBuilderId bid;
		GdaSqlSelectField *sf;
		sf = (GdaSqlSelectField*) sf_list->data;
		b = gda_sql_builder_new (GDA_SQL_STATEMENT_SELECT);
		gda_sql_builder_select_add_target_id (b,
						      gda_sql_builder_add_id (b, "T"),
						      NULL);
		bid = gda_sql_builder_import_expression (b, sf->expr);

		if (bid == 0) {
			g_set_error (error, GDA_DATA_PIVOT_ERROR,
				     GDA_DATA_PIVOT_FIELD_FORMAT_ERROR,
				     _("Wrong field format"));
			gda_sql_statement_free (sqlst);
			return FALSE;
		}
		gda_sql_builder_add_field_value_id (b, bid, 0);
		gchar *sql;
		GdaStatement *stmt;
		stmt = gda_sql_builder_get_statement (b, &lerror);
		g_object_unref (b);
		if (!stmt) {
			gda_sql_statement_free (sqlst);
			g_set_error (error, GDA_DATA_PIVOT_ERROR,
				     GDA_DATA_PIVOT_FIELD_FORMAT_ERROR,
				     _("Wrong field format error: %s"),
				     lerror && lerror->message ? lerror->message : _("No detail"));
			g_clear_error (&lerror);
			return FALSE;
		}
		sql = gda_statement_to_sql (stmt, NULL, NULL);
		g_object_unref (stmt);
		if (!sql) {
			g_set_error (error, GDA_DATA_PIVOT_ERROR,
				     GDA_DATA_PIVOT_FIELD_FORMAT_ERROR,
				     _("Wrong field format"));
			gda_sql_statement_free (sqlst);
			return FALSE;
		}
		/*g_print ("PART [%s][%s]\n", sql, sf->as);*/
		tmp = sql + 7; /* remove the "SELECT " start */
		tmp [strlen (tmp) - 7] = 0; /* remove the " FROM T" end */
		if (sf->as && *(sf->as)) {
			gchar *tmp2;
			tmp2 = g_strdup_printf ("%s AS %s", tmp, sf->as);
			g_array_append_val (array, tmp2);
		}
		else {
			tmp = g_strdup (tmp);
			g_array_append_val (array, tmp);
		}
		g_free (sql);
	}
	
	gda_sql_statement_free (sqlst);

	clean_previous_population (pivot);

	return TRUE;
}

/**
 * gda_data_pivot_add_data:
 * @pivot: a #GdaDataPivot object
 * @aggregate_type: the type of aggregate operation to perform
 * @field: the field description, see below
 * @alias: (nullable): the field alias, or %NULL
 * @error: (nullable): ta place to store errors, or %NULL
 *
 * Specifies that @field has to be included in the analysis.
 * @field is a field specification with the following accepted syntaxes:
 * <itemizedlist>
 *   <listitem><para>a column name in the source data model (see <link linkend="gda-data-model-get-column-index">gda_data_model_get_column_index()</link>); or</para></listitem>
 *   <listitem><para>an SQL expression involving a column name in the source data model, for examples:
 *   <programlisting>
 * price
 * firstname || ' ' || lastname 
 * nb BETWEEN 5 AND 10</programlisting>
 * </para></listitem>
 * </itemizedlist>
 *
 * It is also possible to specify several fields to be added, while separating them by a comma (in effect
 * still forming a valid SQL syntax).
 *
 * Returns: %TRUE if no error occurred
 *
 * Since: 5.0
 */
gboolean
gda_data_pivot_add_data (GdaDataPivot *pivot, GdaDataPivotAggregate aggregate_type,
			 const gchar *field, const gchar *alias, GError **error)
{
	gchar *tmp;
	GError *lerror = NULL;

	g_return_val_if_fail (GDA_IS_DATA_PIVOT (pivot), FALSE);
	g_return_val_if_fail (field, FALSE);
	GdaDataPivotPrivate *priv = gda_data_pivot_get_instance_private (pivot);

	GdaSqlStatement *sqlst;
	sqlst = parse_field_spec (pivot, field, alias, error);
	if (! sqlst)
		return FALSE;
	
	if (! priv->data_fields)
		priv->data_fields = g_array_new (FALSE, FALSE, sizeof (gchar*));
	if (! priv->data_aggregates)
		priv->data_aggregates = g_array_new (FALSE, FALSE, sizeof (GdaDataPivotAggregate));

	GdaSqlStatementSelect *sel;
	GSList *sf_list;
	sel = (GdaSqlStatementSelect*) sqlst->contents;
	for (sf_list = sel->expr_list; sf_list; sf_list = sf_list->next) {
		GdaSqlBuilder *b;
		GdaSqlBuilderId bid;
		GdaSqlSelectField *sf;
		sf = (GdaSqlSelectField*) sf_list->data;
		b = gda_sql_builder_new (GDA_SQL_STATEMENT_SELECT);
		gda_sql_builder_select_add_target_id (b,
						      gda_sql_builder_add_id (b, "T"),
						      NULL);
		bid = gda_sql_builder_import_expression (b, sf->expr);

		if (bid == 0) {
			g_set_error (error, GDA_DATA_PIVOT_ERROR,
				     GDA_DATA_PIVOT_FIELD_FORMAT_ERROR,
				     _("Wrong field format"));
			gda_sql_statement_free (sqlst);
			return FALSE;
		}
		gda_sql_builder_add_field_value_id (b, bid, 0);
		gchar *sql;
		GdaStatement *stmt;
		stmt = gda_sql_builder_get_statement (b, &lerror);
		g_object_unref (b);
		if (!stmt) {
			gda_sql_statement_free (sqlst);
			g_set_error (error, GDA_DATA_PIVOT_ERROR,
				     GDA_DATA_PIVOT_FIELD_FORMAT_ERROR,
				     _("Wrong field format error: %s"),
				     lerror && lerror->message ? lerror->message : _("No detail"));
			g_clear_error (&lerror);
			return FALSE;
		}
		sql = gda_statement_to_sql (stmt, NULL, NULL);
		g_object_unref (stmt);
		if (!sql) {
			g_set_error (error, GDA_DATA_PIVOT_ERROR,
				     GDA_DATA_PIVOT_FIELD_FORMAT_ERROR,
				     _("Wrong field format"));
			gda_sql_statement_free (sqlst);
			return FALSE;
		}
		/*g_print ("PART [%s][%s]\n", sql, sf->as);*/
		tmp = sql + 7; /* remove the "SELECT " start */
		tmp [strlen (tmp) - 7] = 0; /* remove the " FROM T" end */
		if (sf->as && *(sf->as)) {
			gchar *tmp2;
			tmp2 = g_strdup_printf ("%s AS %s", tmp, sf->as);
			g_array_append_val (priv->data_fields, tmp2);
		}
		else {
			tmp = g_strdup (tmp);
			g_array_append_val (priv->data_fields, tmp);
		}
		g_array_append_val (priv->data_aggregates, aggregate_type);
		g_free (sql);
	}
	
	gda_sql_statement_free (sqlst);

	clean_previous_population (pivot);

	return TRUE;
}

/**
 * gda_data_pivot_populate:
 * @pivot: a #GdaDataPivot object
 * @error: (nullable): ta place to store errors, or %NULL
 *
 * Acutally populates @pivot by analysing the data from the provided data model.
 *
 * Returns: %TRUE if no error occurred.
 *
 * Since: 5.0
 */
gboolean
gda_data_pivot_populate (GdaDataPivot *pivot, GError **error)
{
	gboolean retval = FALSE;
	g_return_val_if_fail (GDA_IS_DATA_PIVOT (pivot), FALSE);
	GdaDataPivotPrivate *priv = gda_data_pivot_get_instance_private (pivot);

	if (!priv->row_fields || (priv->row_fields->len == 0)) {
		g_set_error (error, GDA_DATA_PIVOT_ERROR, GDA_DATA_PIVOT_USAGE_ERROR,
			     "%s", _("No row field defined"));
		return FALSE;
	}

	clean_previous_population (pivot);

	/*
	 * create data model extracted using the virtual connection.
	 * The resulting data model's columns are:
	 *   - for priv->row_fields: 0 to priv->row_fields->len - 1
	 *   - for priv->column_fields:
	 *     priv->row_fields->len to priv->row_fields->len + priv->column_fields->len - 1
	 *   - for priv->data_fields: priv->row_fields->len + priv->column_fields->len to priv->row_fields->len + priv->column_fields->len + priv->data_fields->len - 1
	 */
	GString *string;
	guint i;
	string = g_string_new ("SELECT ");
	for (i = 0; i < priv->row_fields->len; i++) {
		gchar *part;
		part = g_array_index (priv->row_fields, gchar *, i);
		if (i != 0)
			g_string_append (string, ", ");
		g_string_append (string, part);
	}
	if (priv->column_fields) {
		for (i = 0; i < priv->column_fields->len; i++) {
			gchar *part;
			part = g_array_index (priv->column_fields, gchar *, i);
			g_string_append (string, ", ");
			g_string_append (string, part);
		}
	}
	if (priv->data_fields) {
		for (i = 0; i < priv->data_fields->len; i++) {
			gchar *part;
			part = g_array_index (priv->data_fields, gchar *, i);
			g_string_append (string, ", ");
			g_string_append (string, part);
		}
	}
	g_string_append (string, " FROM " TABLE_NAME);
	
	GdaStatement *stmt;
	stmt = gda_connection_parse_sql_string (priv->vcnc, string->str, NULL, NULL);
	g_string_free (string, TRUE);
	if (!stmt) {
		g_set_error (error, GDA_DATA_PIVOT_ERROR, GDA_DATA_PIVOT_INTERNAL_ERROR,
			     "%s", _("Could not get information from source data model"));
		return FALSE;
	}

	GdaDataModel *model;
	model = gda_connection_statement_execute_select_full (priv->vcnc, stmt, NULL,
							      GDA_STATEMENT_MODEL_CURSOR_FORWARD,
							      //GDA_STATEMENT_MODEL_RANDOM_ACCESS,
							      NULL, NULL);
	g_object_unref (stmt);
	if (!model) {
		g_set_error (error, GDA_DATA_PIVOT_ERROR, GDA_DATA_PIVOT_INTERNAL_ERROR,
			     "%s", _("Could not get information from source data model"));
		return FALSE;
	}

	/*
	g_print ("========== Iterable data model\n");
	gda_data_model_dump (model, NULL);
	*/

	/* iterate through the data model */
	GdaDataModelIter *iter;
	gint col;
	iter = gda_data_model_create_iter (model);
	if (!iter) {
		g_object_unref (model);
		g_set_error (error, GDA_DATA_PIVOT_ERROR, GDA_DATA_PIVOT_INTERNAL_ERROR,
			     "%s", _("Could not get information from source data model"));
		return FALSE;
	}

	priv->columns = g_array_new (FALSE, FALSE, sizeof (GdaColumn*));
	for (col = 0; col < (gint) priv->row_fields->len; col++) {
		GdaColumn *column;
		GdaHolder *holder;
		column = gda_column_new ();
		holder = GDA_HOLDER (g_slist_nth_data (gda_set_get_holders (GDA_SET (iter)), col));
		
		gda_column_set_name (column, gda_holder_get_id (holder));
		gda_column_set_description (column, gda_holder_get_id (holder));
		gda_column_set_g_type (column, gda_holder_get_g_type (holder));
		gda_column_set_position (column, col);
		g_array_append_val (priv->columns, column);
	}

	/*
	 * actual computation
	 */
	GHashTable    *column_values_index; /* key = a #GValue (ref held in @column_values),
					     * value = pointer to a guint containing the position
					     * in @columns of the column */

	GArray        *first_rows; /* Array of GdaRow objects, the size of each GdaRow is
				    * the number of row fields defined */
	GHashTable    *first_rows_index; /* key = a #GdaRow (ref held in @first_rows),
					  * value = pointer to a guint containing the position
					  * in @first_rows of the row. */
	GHashTable    *data_hash; /* key = a CellData pointer, value = The CellData pointer
				   * as ref'ed in @data */

	first_rows = g_array_new (FALSE, FALSE, sizeof (GdaRow*));
	first_rows_index = g_hash_table_new_full (_gda_row_hash, _gda_row_equal, NULL, g_free);
	column_values_index = g_hash_table_new_full (column_data_hash, column_data_equal,
						     (GDestroyNotify) column_data_free, g_free);
	data_hash = g_hash_table_new_full (cell_data_hash, cell_data_equal,
					   (GDestroyNotify) cell_data_free, NULL);

	for (;gda_data_model_iter_move_next (iter);) {
		/*
		 * Row handling
		 */
		GdaRow *nrow;
#ifdef GDA_DEBUG_ROWS_HASH
		g_print ("read from iter: ");
#endif
		nrow = gda_row_new ((gint) priv->row_fields->len);
		for (col = 0; col < (gint) priv->row_fields->len; col++) {
			const GValue *ivalue;
			GValue *rvalue;
			GError *lerror = NULL;
			ivalue = gda_data_model_iter_get_value_at_e (iter, col, &lerror);
			if (!ivalue || lerror) {
				clean_previous_population (pivot);
				g_object_unref (nrow);
				g_propagate_error (error, lerror);
				goto out;
			}
			rvalue = gda_row_get_value (nrow, col);
			gda_value_reset_with_type (rvalue, G_VALUE_TYPE (ivalue));
			g_value_copy (ivalue, rvalue);
#ifdef GDA_DEBUG_ROWS_HASH
			gchar *tmp;
			tmp = gda_value_stringify (rvalue);
			g_print ("[%s]", tmp);
			g_free (tmp);
#endif
		}

		gint *rowindex; /* *rowindex will contain the actual row of the resulting data mode */
		rowindex = g_hash_table_lookup (first_rows_index, nrow);
		if (rowindex)
			g_object_unref (nrow);
		else {
			g_array_append_val (first_rows, nrow);
			rowindex = g_new (gint, 1);
			*rowindex = first_rows->len - 1;
			g_hash_table_insert (first_rows_index, nrow,
					     rowindex);
		}
#ifdef GDA_DEBUG_ROWS_HASH
		g_print (" => @row %d\n", *rowindex);
#endif

		/*
		 * Column handling
		 */
		gint colsmax;
		if (priv->column_fields)
			colsmax = (gint) priv->column_fields->len;
		else
			colsmax = 1;
		for (col = 0; col < colsmax; col++) {
			const GValue *ivalue = NULL;
			GError *lerror = NULL;
			if (priv->column_fields) {
				ivalue = gda_data_model_iter_get_value_at_e (iter,
									     col + priv->row_fields->len,
									     &lerror);
				if (!ivalue || lerror) {
					clean_previous_population (pivot);
					g_propagate_error (error, lerror);
					goto out;
				}
			}

			gint di, dimax;
			if (priv->data_fields && priv->data_fields->len > 0) {
				di = 0;
				dimax = priv->data_fields->len;
			}
			else {
				di = -1;
				dimax = 0;
			}
			for (; di < dimax; di++) {
				ColumnData coldata;
				gint *colindex;
				GdaDataPivotAggregate aggregate;
				coldata.value = (GValue*) ivalue;
				coldata.column_fields_index = col;
				coldata.data_pos = di;
				colindex = g_hash_table_lookup (column_values_index, &coldata);
				if (di >= 0)
					aggregate = g_array_index (priv->data_aggregates,
								   GdaDataPivotAggregate, di);
				else
					aggregate = GDA_DATA_PIVOT_SUM;

				if (!colindex) {
					/* create new column */
					GdaColumn *column;
					GString *name;

					name = g_string_new ("");
					if (priv->column_fields &&
					    priv->column_fields->len > 1) {
						GdaColumn *column;
						column = gda_data_model_describe_column (model,
											 priv->row_fields->len + col);
						g_string_append_printf (name, "[%s]",
									gda_column_get_name (column));
					}
					if (ivalue) {
						gchar *tmp;
						tmp = gda_value_stringify (ivalue);
						g_string_append (name, tmp);
						g_free (tmp);
					}
					if ((di >= 0) && (dimax > 0)) {
						GdaColumn *column;
						gint vcol;
						vcol = priv->row_fields->len + di;
						if (priv->column_fields)
							vcol += priv->column_fields->len;
						column = gda_data_model_describe_column (model, vcol);
						if (priv->column_fields)
							g_string_append_printf (name, "[%s]",
										gda_column_get_name (column));
						else
							g_string_append (name,
									 gda_column_get_name (column));	
					}

					column = gda_column_new ();
					g_object_set_data ((GObject*) column, "agg",
							   GINT_TO_POINTER (aggregate));
					gda_column_set_name (column, name->str);
					gda_column_set_description (column, name->str);
					g_array_append_val (priv->columns, column);
					gda_column_set_position (column, priv->columns->len - 1);
					/* don't set the column's type now */
					/*g_print ("New column [%s] @real column %d, type %s\n", name->str, priv->columns->len - 1, gda_g_type_to_string (gda_column_get_g_type (column)));*/
					g_string_free (name, TRUE);
					
					ColumnData *ncoldata;
					ncoldata = g_new (ColumnData, 1);
					ncoldata->value = ivalue ? gda_value_copy ((GValue*) ivalue) : NULL;
					ncoldata->column_fields_index = col;
					ncoldata->data_pos = di;
					colindex = g_new (gint, 1);
					*colindex = priv->columns->len - 1;
					g_hash_table_insert (column_values_index, ncoldata,
							     colindex);
				}
				
				/* compute value to take into account */
				GValue *value = NULL;
				if (di >= 0) {
					const GValue *cvalue;
					GError *lerror = NULL;
					gint vcol;
					vcol = priv->row_fields->len + di;
					if (priv->column_fields)
						vcol += priv->column_fields->len;
					cvalue = gda_data_model_iter_get_value_at_e (iter, vcol, &lerror);
					if (!cvalue || lerror) {
						g_propagate_error (error, lerror);
						goto out;
					}
					if (G_VALUE_TYPE (cvalue) != GDA_TYPE_NULL)
						value = gda_value_copy (cvalue);
				}
				else {
					value = gda_value_new (G_TYPE_INT);
					g_value_set_int (value, 1);
				}

				if (value) {
					/* accumulate data */
					CellData ccdata, *pcdata;
					ccdata.row = *rowindex;
					ccdata.col = *colindex;
					ccdata.values = NULL;
					ccdata.data_value = NULL;
					pcdata = g_hash_table_lookup (data_hash, &ccdata);
					if (!pcdata) {
						pcdata = g_new (CellData, 1);
						pcdata->row = *rowindex;
						pcdata->col = *colindex;
						pcdata->error = NULL;
						pcdata->values = NULL;
						pcdata->gtype = G_VALUE_TYPE (value);
						pcdata->nvalues = 0;
						pcdata->data_value = NULL;
						pcdata->aggregate = aggregate;
						g_hash_table_insert (data_hash, pcdata, pcdata);
					}
					if (!aggregate_handle_new_value (pcdata, value)) {
						if (!pcdata->values)
							pcdata->values = g_array_new (FALSE, FALSE,
										      sizeof (GValue*));
						g_array_append_val (pcdata->values, value);
					}
					/*g_print ("row %d col %d => [%s]\n", pcdata->row, pcdata->col,
					  gda_value_stringify (value));*/
				}
			}
		}
	}
	if (gda_data_model_iter_get_row (iter) != -1) {
		/* an error occurred! */
		goto out;
	}

	/* compute real data model's values from all the collected data */
	GdaDataModel *results;
	gint ncols, nrows;
	ncols = priv->columns->len;
	nrows = first_rows->len;
	results = gda_data_model_array_new (ncols);
	for (i = 0; i < priv->row_fields->len; i++) {
		GdaColumn *acolumn, *ecolumn;
		ecolumn = g_array_index (priv->columns, GdaColumn*, i);
		acolumn = gda_data_model_describe_column (results, i);
		gda_column_set_g_type (acolumn, gda_column_get_g_type (ecolumn));
		
		gint j;
		for (j = 0; j < nrows; j++) {
			GdaRow *arow, *erow;
			erow = g_array_index (first_rows, GdaRow*, j);
			arow = gda_data_model_array_get_row ((GdaDataModelArray*) results, j, NULL);
			if (!arow) {
				g_assert (gda_data_model_append_row (results, NULL) == j);
				arow = gda_data_model_array_get_row ((GdaDataModelArray*) results, j,
								     NULL);
				g_assert (arow);
			}
			GValue *av, *ev;
			av = gda_row_get_value (arow, i);
			ev = gda_row_get_value (erow, i);
			gda_value_reset_with_type (av, G_VALUE_TYPE (ev));
			g_value_copy (ev, av);
		}
	}
	for (; i < (guint) ncols; i++) {
		GdaColumn *ecolumn;
		ecolumn = g_array_index (priv->columns, GdaColumn*, i);

		gint j;
		for (j = 0; j < nrows; j++) {
			GdaRow *arow;
			GValue *av;
			arow = gda_data_model_array_get_row ((GdaDataModelArray*) results, j, NULL);
			av = gda_row_get_value (arow, i);

			CellData ccdata, *pcdata;
			GType coltype = GDA_TYPE_NULL;
			ccdata.row = j;
			ccdata.col = i;
			ccdata.values = NULL;
			ccdata.data_value = NULL;
			pcdata = g_hash_table_lookup (data_hash, &ccdata);
			if (pcdata) {
				cell_data_compute_aggregate (pcdata);
				if (pcdata->data_value) {
					coltype = G_VALUE_TYPE (pcdata->data_value);
					gda_value_reset_with_type (av, coltype);
					g_value_copy (pcdata->data_value, av);
				}
				else
					gda_row_invalidate_value (arow, av);
			}
			else {
				GValue *empty;
				GdaDataPivotAggregate agg;
				agg = GPOINTER_TO_INT (g_object_get_data ((GObject*) ecolumn, "agg"));
				empty = aggregate_get_empty_value (agg);
				coltype = G_VALUE_TYPE (empty);
				gda_value_reset_with_type (av, coltype);
				g_value_copy (empty, av);
			}
			if (coltype != GDA_TYPE_NULL)
				gda_column_set_g_type (ecolumn, coltype);
		}
	}
	priv->results = results;

	retval = TRUE;

 out:
	if (!retval)
		clean_previous_population (pivot);

	if (data_hash)
		g_hash_table_destroy (data_hash);

	if (first_rows) {
		guint i;
		for (i = 0; i < first_rows->len; i++) {
			GObject *obj;
			obj = g_array_index (first_rows, GObject*, i);
			if (obj)
				g_object_unref (obj);
		}
		g_array_free (first_rows, TRUE);
	}

	if (first_rows_index)
		g_hash_table_destroy (first_rows_index);

	g_hash_table_destroy (column_values_index);

	g_object_unref (iter);
	g_object_unref (model);

	return retval;
}

static guint
_gda_value_hash (gconstpointer key)
{
	GValue *v;
	GType vt;
	guint res = 0;
	v = (GValue *) key;
	vt = G_VALUE_TYPE (v);
	if ((vt == G_TYPE_BOOLEAN) || (vt == G_TYPE_INT) || (vt == G_TYPE_UINT) ||
	    (vt == G_TYPE_FLOAT) || (vt == G_TYPE_DOUBLE) ||
	    (vt == G_TYPE_INT64) || (vt == G_TYPE_INT64) ||
	    (vt == GDA_TYPE_SHORT) || (vt == GDA_TYPE_USHORT) ||
	    (vt == G_TYPE_CHAR) || (vt == G_TYPE_UCHAR) ||
	    (vt == GDA_TYPE_NULL) || (vt == G_TYPE_GTYPE) ||
	    (vt == G_TYPE_LONG) || (vt == G_TYPE_ULONG)) {
		const signed char *p, *data;
		data = (signed char*) v;
		res = 5381;
		for (p = data; p < data + sizeof (GValue); p++)
			res = (res << 5) + res + *p;
	}
	else if (vt == G_TYPE_STRING) {
		const gchar *tmp;
		tmp = g_value_get_string (v);
		if (tmp)
			res += g_str_hash (tmp);
	}
	else if ((vt == GDA_TYPE_BINARY) || (vt == GDA_TYPE_BLOB)) {
		GdaBinary *bin;
		if (vt == GDA_TYPE_BLOB) {
			GdaBlob *blob;
			blob = (GdaBlob*) gda_value_get_blob ((GValue *) v);
			bin = gda_blob_get_binary (blob);
			if (gda_blob_get_op (blob) &&
			    (gda_binary_get_size (bin) != gda_blob_op_get_length (gda_blob_get_op (blob))))
				gda_blob_op_read_all (gda_blob_get_op (blob), blob);
		}
		else
			bin = gda_value_get_binary ((GValue *) v);
		if (bin) {
			glong l;
			for (l = 0; l < gda_binary_get_size (bin); l++) {
			  guchar* p = gda_binary_get_data (bin);
				res += (guint) p[l];
			}
		}
	}
	else {
		gchar *tmp;
		tmp = gda_value_stringify (v);
		res += g_str_hash (tmp);
		g_free (tmp);
	}
	return res;
}

static guint
_gda_row_hash (gconstpointer key)
{
	gint i, len;
	GdaRow *r;
	guint res = 0;
	r = (GdaRow*) key;
	len = gda_row_get_length (r);
	for (i = 0; i < len ; i++) {
		GValue *v;
		v = gda_row_get_value (r, i);
		res = (res << 5) + res + _gda_value_hash (v);
	}
	return res;
}

static gboolean
_gda_row_equal (gconstpointer a, gconstpointer b)
{
	gint i, len;
	GdaRow *ra, *rb;
	ra = (GdaRow*) a;
	rb = (GdaRow*) b;
	len = gda_row_get_length (ra);
	g_assert (len == gda_row_get_length (rb));
	for (i = 0; i < len ; i++) {
		GValue *va, *vb;
		va = gda_row_get_value (ra, i);
		vb = gda_row_get_value (rb, i);
		if (gda_value_differ (va, vb))
			return FALSE;
	}
	return TRUE;
}

/*
 * Create a new virtual connection for @pivot
 */
static gboolean
create_vcnc (GdaDataPivot *pivot, GError **error)
{
	GdaConnection *vcnc;
	GError *lerror = NULL;
  static GMutex provider_mutex;
  static GdaVirtualProvider *virtual_provider;
  GdaDataPivotPrivate *      priv             = gda_data_pivot_get_instance_private (pivot);
  if (priv->vcnc)
    return TRUE;

  g_mutex_lock (&provider_mutex);
  if (!virtual_provider)
    virtual_provider = gda_vprovider_data_model_new ();
	g_mutex_unlock (&provider_mutex);

	vcnc = gda_virtual_connection_open (virtual_provider, GDA_CONNECTION_OPTIONS_NONE, &lerror);
	if (! vcnc) {
		g_print ("Virtual ERROR: %s\n", lerror && lerror->message ? lerror->message : "No detail");
		if (lerror)
			g_error_free (lerror);
		g_set_error (error, GDA_DATA_PIVOT_ERROR, GDA_DATA_PIVOT_INTERNAL_ERROR,
			     "%s", _("Could not create virtual connection"));
		return FALSE;
	}

	priv->vcnc = vcnc;
	return TRUE;
}

/*
 * Bind @priv->model as a table named TABLE_NAME in @pivot's virtual connection
 */
static gboolean
bind_source_model (GdaDataPivot *pivot, GError **error)
{
	GdaDataPivotPrivate *priv = gda_data_pivot_get_instance_private (pivot);
	if (! priv->model) {
		g_set_error (error, GDA_DATA_PIVOT_ERROR, GDA_DATA_PIVOT_SOURCE_MODEL_ERROR,
			     "%s", _("No source defined"));
		return FALSE;
	}
	if (! create_vcnc (pivot, error))
		return FALSE;

	if (gda_vconnection_data_model_get_model (GDA_VCONNECTION_DATA_MODEL (priv->vcnc),
						  TABLE_NAME) == priv->model) {
		/* already bound */
		return TRUE;
	}

	GError *lerror = NULL;
	if (!gda_vconnection_data_model_add_model (GDA_VCONNECTION_DATA_MODEL (priv->vcnc),
						   priv->model,
						   TABLE_NAME, &lerror)) {
		g_set_error (error, GDA_DATA_PIVOT_ERROR, GDA_DATA_PIVOT_SOURCE_MODEL_ERROR,
			     "%s",
			     _("Invalid source data model (may have incompatible column names)"));
		g_clear_error (&lerror);
		return FALSE;
	}

	return TRUE;
}

static
guint column_data_hash (gconstpointer key)
{
	ColumnData *cd;
	cd = (ColumnData*) key;
	if (cd->value)
		return _gda_value_hash (cd->value) + cd->column_fields_index + cd->data_pos;
	else
		return cd->column_fields_index + cd->data_pos;
}

static gboolean
column_data_equal (gconstpointer a, gconstpointer b)
{
	ColumnData *cda, *cdb;
	cda = (ColumnData*) a;
	cdb = (ColumnData*) b;
	if ((cda->column_fields_index != cdb->column_fields_index) ||
	    (cda->data_pos != cdb->data_pos))
		return FALSE;
	if (cda->value && cdb->value)
		return gda_value_differ (cda->value, cdb->value) ? FALSE : TRUE;
	else if (cda->value || cdb->value)
		return FALSE;
	else
		return TRUE;
}

static void
column_data_free (ColumnData *cdata)
{
	gda_value_free (cdata->value);
	g_free (cdata);
}

static guint
cell_data_hash (gconstpointer key)
{
	CellData *cd;
	cd = (CellData*) key;
	return g_int_hash (&(cd->row)) + g_int_hash (&(cd->col));
}

static gboolean
cell_data_equal (gconstpointer a, gconstpointer b)
{
	CellData *cda, *cdb;
	cda = (CellData*) a;
	cdb = (CellData*) b;
	if ((cda->row == cdb->row) && (cda->col == cdb->col))
		return TRUE;
	else
		return FALSE;
}

static void
cell_data_free (CellData *cdata)
{
	if (cdata->values) {
		guint i;
		for (i = 0; i < cdata->values->len; i++) {
			GValue *value;
			value = g_array_index (cdata->values, GValue*, i);
			gda_value_free (value);
		}
		g_array_free (cdata->values, TRUE);
	}
	gda_value_free (cdata->data_value);
	g_free (cdata);
}
