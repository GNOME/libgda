/*
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
#define G_LOG_DOMAIN "GDA-data-model-select"

#include<gda-data-model-select.h>

// GdaDataModel Interface
static void gda_data_model_select_data_model_init (GdaDataModelInterface *iface);

// GdaDataModelSelect object definition

enum
{
  PROP_VALID = 1,
  N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

static void gda_data_model_select_dispose (GObject *object);
static void gda_data_model_select_set_property (GObject      *object,
                                                guint         property_id,
                                                const GValue *value,
                                                GParamSpec   *pspec);

static void gda_data_model_select_get_property (GObject    *object,
                                                guint       property_id,
                                                GValue     *value,
                                                GParamSpec *pspec);

typedef struct {
  GdaConnection          *cnc;
  GdaStatement           *stm;
  GdaDataModel           *model;
  GdaSet                 *params;
} GdaDataModelSelectPrivate;

G_DEFINE_TYPE_WITH_CODE (GdaDataModelSelect, gda_data_model_select, G_TYPE_OBJECT,
                         G_ADD_PRIVATE (GdaDataModelSelect)
                         G_IMPLEMENT_INTERFACE (GDA_TYPE_DATA_MODEL, gda_data_model_select_data_model_init))


/* signals */
enum {
	UPDATED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

static void gda_data_model_select_class_init (GdaDataModelSelectClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  object_class->dispose = gda_data_model_select_dispose;
  object_class->get_property = gda_data_model_select_get_property;
  object_class->set_property = gda_data_model_select_set_property;

  /**
   * GdaDataModelSelect::updated:
   * @model: a #GdaDataModelSelect
   *
   * Emmited when the data model has been updated due to parameters changes
   * in statement
   */
  signals[UPDATED] =  g_signal_newv ("updated",
                 G_TYPE_FROM_CLASS (object_class),
                 G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS,
                 NULL /* closure */,
                 NULL /* accumulator */,
                 NULL /* accumulator data */,
                 NULL /* C marshaller */,
                 G_TYPE_NONE /* return_type */,
                 0     /* n_params */,
                 NULL  /* param_types */);
  properties[PROP_VALID] =
    g_param_spec_boolean ("valid",
                       "Valid",
                       "If last statement execution was successful this is set to TRUE",
                       FALSE  /* default value */,
                       G_PARAM_READABLE);

  g_object_class_install_properties (object_class,
                                     N_PROPERTIES,
                                     properties);
}

static void gda_data_model_select_init (GdaDataModelSelect *model)
{
  GdaDataModelSelectPrivate *priv = gda_data_model_select_get_instance_private (GDA_DATA_MODEL_SELECT (model));
  priv->cnc = NULL;
  priv->stm = NULL;
  priv->params = NULL;
  priv->model = NULL;
}

static void gda_data_model_select_dispose (GObject *object)
{
  GdaDataModelSelectPrivate *priv = gda_data_model_select_get_instance_private (GDA_DATA_MODEL_SELECT (object));
  if (priv->cnc != NULL) {
    g_object_unref (priv->cnc);
    priv->cnc = NULL;
  }
  if (priv->stm != NULL) {
    g_object_unref (priv->stm);
    priv->stm = NULL;
  }
  if (priv->params != NULL) {
    g_object_unref (priv->params);
    priv->params = NULL;
  }
  if (priv->model != NULL) {
    g_object_unref (priv->model);
    priv->model = NULL;
  }
}


static void gda_data_model_select_set_property (GObject      *object,
                                                guint         property_id,
                                                const GValue *value,
                                                GParamSpec   *pspec)
{
  switch (property_id) {
  case PROP_VALID:
    break;
  default:
    /* We don't have any other property... */
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    break;
  }
}

static void gda_data_model_select_get_property (GObject    *object,
                                                guint       property_id,
                                                GValue     *value,
                                                GParamSpec *pspec)
{
  GdaDataModelSelectPrivate *priv = gda_data_model_select_get_instance_private (GDA_DATA_MODEL_SELECT (object));
  switch (property_id) {
  case PROP_VALID:
    g_value_set_boolean (value, priv->model != NULL);
    break;
  default:
    /* We don't have any other property... */
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    break;
  }
}

static void params_changed_cb (GdaSet *params, GdaHolder *holder, GdaDataModelSelect *model)
{
  GdaDataModelSelectPrivate *priv = gda_data_model_select_get_instance_private (GDA_DATA_MODEL_SELECT (model));
  if (priv->cnc != NULL && priv->stm != NULL) {
    if (priv->model != NULL) {
      g_object_unref (priv->model);
      priv->model = NULL;
    }
    priv->model = gda_connection_statement_execute_select (priv->cnc, priv->stm, priv->params, NULL);
    g_signal_emit (model, signals[UPDATED], 0);
  }
}

/**
 * gda_data_model_select_new:
 * @cnc: an opened #GdaConnection
 * @stm: a SELECT SQL #GdaStatement
 * @params: (nullable): a #GdaSet with the parameters to ejecute the SELECT SQL statement
 *
 * Returns: (transfer full): a new #GdaDataModelSelect object
 */
GdaDataModelSelect*
gda_data_model_select_new (GdaConnection *cnc, GdaStatement *stm, GdaSet *params)
{
  g_return_val_if_fail (cnc != NULL, NULL);
  g_return_val_if_fail (stm != NULL, NULL);
  g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
  g_return_val_if_fail (GDA_IS_STATEMENT (stm), NULL);

  GdaDataModelSelect* model = GDA_DATA_MODEL_SELECT (g_object_new (GDA_TYPE_DATA_MODEL_SELECT, NULL));
  GdaDataModelSelectPrivate *priv = gda_data_model_select_get_instance_private (GDA_DATA_MODEL_SELECT (model));

  priv->cnc = g_object_ref (cnc);
  priv->stm = g_object_ref (stm);
  if (params != NULL) {
    priv->params = g_object_ref (params);
    g_signal_connect (priv->params, "holder-changed", G_CALLBACK (params_changed_cb), model);
  } else {
    priv->params = NULL;
  }
  priv->model = gda_connection_statement_execute_select (priv->cnc, priv->stm, priv->params, NULL);
  return model;
}
/**
 * gda_data_model_select_new_from_string:
 * @cnc: an opened #GdaConnection
 * @sql: a string representing a SELECT SQL to execute
 *
 * Returns: (transfer full): a new #GdaDataModelSelect object
 */
GdaDataModelSelect*
gda_data_model_select_new_from_string (GdaConnection *cnc, const gchar *sql)
{
  g_return_val_if_fail (cnc != NULL, NULL);
  g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
  GdaStatement *stm = gda_connection_parse_sql_string (cnc, sql, NULL, NULL);
  if (stm == NULL) {
    return NULL;
  }

  GdaSet *params = NULL;
  gda_statement_get_parameters (stm, &params, NULL);

  return gda_data_model_select_new (cnc, stm, params);
}


/**
 * gda_data_model_select_is_valid:
 * @model: a #GdaDataModelSelect object
 *
 * If at creation or after parameters change has been set, a SELECT statement
 * is ejectuted, if unsuccess then this model is at invalid state.
 *
 * Returns: TRUE if a valid data model is present
 */
gboolean
gda_data_model_select_is_valid (GdaDataModelSelect *model)
{
  g_return_val_if_fail (model != NULL, FALSE);
  g_return_val_if_fail (GDA_IS_DATA_MODEL_SELECT (model), FALSE);
  GdaDataModelSelectPrivate *priv = gda_data_model_select_get_instance_private (GDA_DATA_MODEL_SELECT (model));
  return priv->model != NULL;
}

/**
 * gda_data_model_select_get_parameters:
 * @model: a #GdaDataModelSelect object
 *
 * Returns: (transfer full): current parameters used by internal statement
 */
GdaSet*
gda_data_model_select_get_parameters  (GdaDataModelSelect *model)
{
  g_return_val_if_fail (model != NULL, NULL);
  g_return_val_if_fail (GDA_IS_DATA_MODEL_SELECT (model), NULL);
  GdaDataModelSelectPrivate *priv = gda_data_model_select_get_instance_private (GDA_DATA_MODEL_SELECT (model));
  if (priv->params == NULL) {
    gda_statement_get_parameters (priv->stm, &priv->params, NULL);
    return priv->params;
  }
  return g_object_ref (priv->params);
}

/**
 * gda_data_model_select_set_parameters:
 * @model: a #GdaDataModelSelect object
 * @params: a #GdaSet with the parameters for the statement
 */
void
gda_data_model_select_set_parameters  (GdaDataModelSelect *model, GdaSet *params)
{
  g_return_if_fail (model != NULL);
  g_return_if_fail (GDA_IS_DATA_MODEL_SELECT (model));
  g_return_if_fail (params != NULL);
  GdaDataModelSelectPrivate *priv = gda_data_model_select_get_instance_private (GDA_DATA_MODEL_SELECT (model));
  if (priv->params != NULL) {
    g_object_unref (priv->params);
    priv->params = NULL;
  }
  priv->params = g_object_ref (priv->params);
  g_signal_connect (priv->params, "holder-changed", G_CALLBACK (params_changed_cb), model);
  params_changed_cb (priv->params, NULL, model);
}


// GdaDataModel Implementation



static gint
gda_data_model_select_get_n_rows (GdaDataModel *model)
{
  g_return_val_if_fail (model != NULL, -1);
  g_return_val_if_fail (GDA_IS_DATA_MODEL_SELECT (model), -1);
  GdaDataModelSelectPrivate *priv = gda_data_model_select_get_instance_private (GDA_DATA_MODEL_SELECT (model));
  g_return_val_if_fail (priv->model != NULL, 0);
  return gda_data_model_get_n_rows (priv->model);
}
static gint
gda_data_model_select_get_n_columns   (GdaDataModel *model)
{
  g_return_val_if_fail (model != NULL, -1);
  g_return_val_if_fail (GDA_IS_DATA_MODEL_SELECT (model), -1);
  GdaDataModelSelectPrivate *priv = gda_data_model_select_get_instance_private (GDA_DATA_MODEL_SELECT (model));
  g_return_val_if_fail (priv->model != NULL, 0);
  return gda_data_model_get_n_columns (priv->model);
}
static GdaColumn*
gda_data_model_select_describe_column (GdaDataModel *model,
                                       gint          col)
{
  g_return_val_if_fail (model != NULL, NULL);
  g_return_val_if_fail (GDA_IS_DATA_MODEL_SELECT (model), NULL);
  GdaDataModelSelectPrivate *priv = gda_data_model_select_get_instance_private (GDA_DATA_MODEL_SELECT (model));
  g_return_val_if_fail (priv->model != NULL, NULL);
  return gda_data_model_describe_column (priv->model, col);
}
static GdaDataModelAccessFlags
gda_data_model_select_get_access_flags (GdaDataModel *model)
{
  g_return_val_if_fail (model != NULL, 0);
  g_return_val_if_fail (GDA_IS_DATA_MODEL_SELECT (model), 0);
  GdaDataModelSelectPrivate *priv = gda_data_model_select_get_instance_private (GDA_DATA_MODEL_SELECT (model));
  g_return_val_if_fail (priv->model != NULL, 0);
  return gda_data_model_get_access_flags (priv->model);
}
static const GValue*
gda_data_model_select_get_value_at (GdaDataModel  *model,
                                    gint           col,
                                    gint           row,
                                    GError       **error)
{
  g_return_val_if_fail (model != NULL, NULL);
  g_return_val_if_fail (GDA_IS_DATA_MODEL_SELECT (model), NULL);
  GdaDataModelSelectPrivate *priv = gda_data_model_select_get_instance_private (GDA_DATA_MODEL_SELECT (model));
  g_return_val_if_fail (priv->model != NULL, NULL);
  return gda_data_model_get_value_at (priv->model, col, row, error);
}
static GdaValueAttribute
gda_data_model_select_get_attributes_at (GdaDataModel *model,
                                         gint          col,
                                         gint          row)
{
  g_return_val_if_fail (model != NULL, 0);
  g_return_val_if_fail (GDA_IS_DATA_MODEL_SELECT (model), 0);
  GdaDataModelSelectPrivate *priv = gda_data_model_select_get_instance_private (GDA_DATA_MODEL_SELECT (model));
  g_return_val_if_fail (priv->model != NULL, 0);
  return gda_data_model_get_attributes_at (priv->model, col, row);
}

static GdaDataModelIter*
gda_data_model_select_create_iter (GdaDataModel *model)
{
  g_return_val_if_fail (model != NULL, NULL);
  g_return_val_if_fail (GDA_IS_DATA_MODEL_SELECT (model), NULL);
  GdaDataModelSelectPrivate *priv = gda_data_model_select_get_instance_private (GDA_DATA_MODEL_SELECT (model));
  g_return_val_if_fail (priv->model != NULL, NULL);
  return gda_data_model_create_iter (priv->model);
}

static gboolean
gda_data_model_select_set_value_at (GdaDataModel  *model,
                                    gint           col,
                                    gint           row,
                                    const GValue  *value,
                                    GError       **error)
{
  g_return_val_if_fail (model != NULL, FALSE);
  g_return_val_if_fail (GDA_IS_DATA_MODEL_SELECT (model), FALSE);
  GdaDataModelSelectPrivate *priv = gda_data_model_select_get_instance_private (GDA_DATA_MODEL_SELECT (model));
  g_return_val_if_fail (priv->model != NULL, FALSE);
  return gda_data_model_set_value_at (priv->model, col, row, value, error);
}

static gboolean
gda_data_model_select_set_values (GdaDataModel  *model,
                                  gint           row,
                                  GList         *values,
                                  GError       **error)
{
  g_return_val_if_fail (model != NULL, FALSE);
  g_return_val_if_fail (GDA_IS_DATA_MODEL_SELECT (model), FALSE);
  GdaDataModelSelectPrivate *priv = gda_data_model_select_get_instance_private (GDA_DATA_MODEL_SELECT (model));
  g_return_val_if_fail (priv->model != NULL, FALSE);
  return gda_data_model_set_values (priv->model, row, values, error);
}
static gint
gda_data_model_select_append_values (GdaDataModel  *model,
                                     const GList   *values,
                                     GError       **error)
{
  g_return_val_if_fail (model != NULL, -1);
  g_return_val_if_fail (GDA_IS_DATA_MODEL_SELECT (model), -1);
  GdaDataModelSelectPrivate *priv = gda_data_model_select_get_instance_private (GDA_DATA_MODEL_SELECT (model));
  g_return_val_if_fail (priv->model != NULL, -1);
  return gda_data_model_append_values (priv->model, values, error);
}
static gboolean
gda_data_model_select_remove_row (GdaDataModel  *model,
                                  gint           row,
                                  GError       **error)
{
  g_return_val_if_fail (model != NULL, FALSE);
  g_return_val_if_fail (GDA_IS_DATA_MODEL_SELECT (model), FALSE);
  GdaDataModelSelectPrivate *priv = gda_data_model_select_get_instance_private (GDA_DATA_MODEL_SELECT (model));
  g_return_val_if_fail (priv->model != NULL, FALSE);
  return gda_data_model_remove_row (priv->model, row, error);
}

static void
gda_data_model_select_freeze (GdaDataModel *model)
{
  g_return_if_fail (model != NULL);
  g_return_if_fail (GDA_IS_DATA_MODEL_SELECT (model));
  GdaDataModelSelectPrivate *priv = gda_data_model_select_get_instance_private (GDA_DATA_MODEL_SELECT (model));
  g_return_if_fail (priv->model != NULL);
  return gda_data_model_freeze (priv->model);
}
static void
gda_data_model_select_thaw (GdaDataModel *model)
{
  g_return_if_fail (model != NULL);
  g_return_if_fail (GDA_IS_DATA_MODEL_SELECT (model));
  GdaDataModelSelectPrivate *priv = gda_data_model_select_get_instance_private (GDA_DATA_MODEL_SELECT (model));
  g_return_if_fail (priv->model != NULL);
  return gda_data_model_thaw (priv->model);
}
static gboolean
gda_data_model_select_get_notify (GdaDataModel *model)
{
  g_return_val_if_fail (model != NULL, FALSE);
  g_return_val_if_fail (GDA_IS_DATA_MODEL_SELECT (model), FALSE);
  GdaDataModelSelectPrivate *priv = gda_data_model_select_get_instance_private (GDA_DATA_MODEL_SELECT (model));
  g_return_val_if_fail (priv->model != NULL, FALSE);
  return gda_data_model_get_notify (priv->model);
}
static GError**
gda_data_model_select_get_exceptions (GdaDataModel *model)
{
  g_return_val_if_fail (model != NULL, NULL);
  g_return_val_if_fail (GDA_IS_DATA_MODEL_SELECT (model), NULL);
  GdaDataModelSelectPrivate *priv = gda_data_model_select_get_instance_private (GDA_DATA_MODEL_SELECT (model));
  g_return_val_if_fail (priv->model != NULL, FALSE);
  return gda_data_model_get_exceptions (priv->model);
}

static void
gda_data_model_select_data_model_init (GdaDataModelInterface *iface)
{
	iface->get_n_rows = gda_data_model_select_get_n_rows;
	iface->get_n_columns = gda_data_model_select_get_n_columns;
	iface->describe_column = gda_data_model_select_describe_column;
        iface->get_access_flags = gda_data_model_select_get_access_flags;
	iface->get_value_at = gda_data_model_select_get_value_at;
	iface->get_attributes_at = gda_data_model_select_get_attributes_at;

	iface->create_iter = gda_data_model_select_create_iter;

	iface->set_value_at = gda_data_model_select_set_value_at;
	iface->set_values = gda_data_model_select_set_values;
        iface->append_values = gda_data_model_select_append_values;
	iface->append_row = NULL;
	iface->remove_row = gda_data_model_select_remove_row;
	iface->find_row = NULL;

	iface->freeze = gda_data_model_select_freeze;
	iface->thaw = gda_data_model_select_thaw;
	iface->get_notify = gda_data_model_select_get_notify;
	iface->send_hint = NULL;

	iface->get_exceptions = gda_data_model_select_get_exceptions;
}
