/*
 * Copyright (C) 2000 - 2003 Rodrigo Moya <rodrigo@gnome-db.org>
 * Copyright (C) 2001 Cleber Rodrigues <cleberrrjr@bol.com.br>
 * Copyright (C) 2002 Gonzalo Paniagua Javier <gonzalo@gnome-db.org>
 * Copyright (C) 2003 Laurent Sansonetti <laurent@datarescue.be>
 * Copyright (C) 2003 Paisa Seeluangsawat <paisa@users.sf.net>
 * Copyright (C) 2003 Xabier Rodríguez Calvar <xrcalvar@igalia.com>
 * Copyright (C) 2004 Paisa  Seeluangsawat <paisa@users.sf.net>
 * Copyright (C) 2004 Szalai Ferenc <szferi@einstein.ki.iif.hu>
 * Copyright (C) 2005 - 2013 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2006 - 2008 Murray Cumming <murrayc@murrayc.com>
 * Copyright (C) 2007 Leonardo Boshell <lb@kmc.com.co>
 * Copyright (C) 2010 David King <davidk@openismus.com>
 * Copyright (C) 2011, 2018 Daniel Espinosa <esodan@gmail.com>
 * Copyright (C) 2015 Corentin Noël <corentin@elementary.io>
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
#define G_LOG_DOMAIN "GDA-row"

#include "gda-row.h"
#include <string.h>
#include <glib/gi18n-lib.h>
#include "gda-data-model.h"

#define PARENT_TYPE G_TYPE_OBJECT

typedef struct {
  GdaDataModel *model; /* can be NULL */
  guint         model_row;

  GValue       *fields; /* GValues */
  GError      **errors; /* GError for each invalid value at the same position */
  guint         nfields;
} GdaRowPrivate;
G_DEFINE_TYPE_WITH_PRIVATE (GdaRow, gda_row, G_TYPE_OBJECT)

/* properties */
enum
{
  PROP_NB_VALUES = 1,
  PROP_MODEL,
  PROP_MODEL_ROW
};

static void gda_row_finalize   (GObject *object);
static void gda_row_dispose    (GObject *object);

static void gda_row_set_property (GObject *object,
				  guint param_id,
				  const GValue *value,
				  GParamSpec *pspec);
static void gda_row_get_property (GObject *object,
				  guint param_id,
				  GValue *value,
				  GParamSpec *pspec);

static void
gda_row_class_init (GdaRowClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = gda_row_finalize;
	object_class->dispose = gda_row_dispose;

	/* Properties */
        object_class->set_property = gda_row_set_property;
        object_class->get_property = gda_row_get_property;

	g_object_class_install_property (object_class, PROP_NB_VALUES,
                                         g_param_spec_int ("nb-values", "NumValues", "Number of values in the row",
							   1, G_MAXINT, 1, 
							   G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_MODEL,
                                         g_param_spec_object ("model", "Model", "Data model used to get values from",
							   GDA_TYPE_DATA_MODEL,
							   G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_MODEL_ROW,
                                         g_param_spec_int ("model-row", "ModelRow", "Row number in data model to get data from",
							   0, G_MAXINT, 0,
							   G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE));
}

static void
gda_row_init (GdaRow *row)
{
	g_return_if_fail (GDA_IS_ROW (row));

	GdaRowPrivate *priv = gda_row_get_instance_private (row);
	priv->model = NULL;
  priv->model_row = 0;
	priv->fields = NULL;
	priv->errors = NULL;
	priv->nfields = 0;
}

static void
gda_row_dispose (GObject *object)
{
	GdaRow *row = (GdaRow *) object;
	
	g_return_if_fail (GDA_IS_ROW (row));
		
	G_OBJECT_CLASS (gda_row_parent_class)->dispose (object);
}

static void
gda_row_finalize (GObject *object)
{
	GdaRow *row = (GdaRow *) object;

	g_return_if_fail (GDA_IS_ROW (row));
	GdaRowPrivate *priv = gda_row_get_instance_private (row);
	
	if (priv->model == NULL) {
		guint i;

		for (i = 0; i < priv->nfields; i++) {
			gda_value_set_null (&(priv->fields [i]));
			if (priv->errors && priv->errors [i])
				g_error_free (priv->errors [i]);
		}
		g_free (priv->fields);
		g_free (priv->errors);

	} else {
		g_object_unref (priv->model);
		priv->model = NULL;
	}

	G_OBJECT_CLASS (gda_row_parent_class)->finalize (object);
}

static void
gda_row_set_property (GObject *object,
                    guint param_id,
                    const GValue *value,
                    GParamSpec *pspec)
{
    GdaRow *row;

    row = GDA_ROW (object);
    GdaRowPrivate *priv = gda_row_get_instance_private (row);
    switch (param_id) {
    case PROP_NB_VALUES:
            if (priv->model != NULL) {
                    g_warning (_("Can't set a number of values, because a data model is in use"));
                    break;
            }
            guint i;
            g_return_if_fail (!priv->fields);

            priv->nfields = (guint) g_value_get_int (value);
            priv->fields = g_new0 (GValue, priv->nfields);
            for (i = 0; i < priv->nfields; i++)
                    gda_value_set_null (& (priv->fields [i]));
            break;
    case PROP_MODEL:
      if (priv->nfields != 0) {
        break;
      }
      priv->model = GDA_DATA_MODEL(g_object_ref (g_value_get_object (value)));
      break;
    case PROP_MODEL_ROW:
      if (priv->nfields != 0) {
        break;
      }
      priv->model_row = g_value_get_uint (value);
      break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        break;
    }
}

static void
gda_row_get_property (GObject *object,
		      guint param_id,
		      GValue *value,
		      GParamSpec *pspec)
{
	GdaRow *row;

	row = GDA_ROW (object);
	GdaRowPrivate *priv = gda_row_get_instance_private (row);
	switch (param_id) {
		case PROP_NB_VALUES:
			g_value_set_int (value, priv->nfields);
			break;
		case PROP_MODEL:
			g_value_set_object (value, priv->model);
			break;
		case PROP_MODEL_ROW:
			g_value_set_uint (value, priv->model_row);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
			break;
	}
}

/**
 * gda_row_new:
 * @count: number of #GValue in the new #GdaRow.
 *
 * Creates a #GdaRow which can hold @count #GValue values.
 *
 * Returns: a newly allocated #GdaRow object.
 */
GdaRow *
gda_row_new (gint count)
{
	g_return_val_if_fail (count > 0, NULL);
	return (GdaRow*) g_object_new (GDA_TYPE_ROW, "nb-values", count, NULL);
}

/**
 * gda_row_new_from_data_model:
 * @model: a #GdaDataModel to get data from.
 * @row: row at #GdaDataModel to get data from
 *
 * Creates a #GdaRow which represent a row in a #GdaDataModel
 *
 * Returns: (transfer full):a newly allocated #GdaRow object.
 */
GdaRow *
gda_row_new_from_data_model (GdaDataModel *model, guint row)
{
  g_return_val_if_fail (model, NULL);
  g_return_val_if_fail (GDA_IS_DATA_MODEL (model), NULL);
	return (GdaRow*) g_object_new (GDA_TYPE_ROW, "model", model, "model-row", row, NULL);
}

/**
 * gda_row_get_value:
 * @row: a #GdaRow
 * @num: field index.
 *
 * Gets a pointer to a #GValue stored in a #GdaRow.
 *
 * This is a pointer to the internal array of values. Don't try to free
 * or modify it (modifying is reserved to database provider's implementations).
 *
 * Returns: (nullable) (transfer none): a pointer to the #GValue in the position @num of @row.
 */
GValue *
gda_row_get_value (GdaRow *row, gint num)
{
  g_return_val_if_fail (GDA_IS_ROW (row), NULL);
  GdaRowPrivate *priv = gda_row_get_instance_private (row);
  if (priv->model == NULL) {
    g_return_val_if_fail ((num >= 0) && ((guint) num < priv->nfields), NULL);
    return & (priv->fields[num]);
  } else {
    GError *error = NULL;
    const GValue *value = NULL;
    g_return_val_if_fail ((num >= 0) && (num < gda_data_model_get_n_columns (priv->model)), NULL);
    value = gda_data_model_get_value_at (priv->model, num, priv->model_row, &error);
    if (value == NULL) {
      g_warning (_("No value can be retrieved from data model's row: %s"),
                 error ? (error->message ? error->message : _("No Detail")) : _("No error was set"));
    }
  }
  return NULL;
}

/**
 * gda_row_invalidate_value:
 * @row: a #GdaRow
 * @value: a #GValue belonging to @row (obtained with gda_row_get_value()).
 * 
 * Marks @value as being invalid. This method is mainly used by database
 * providers' implementations to report any error while reading a value from the database.
 */
void
gda_row_invalidate_value (G_GNUC_UNUSED GdaRow *row, GValue *value)
{
	return gda_row_invalidate_value_e (row, value, NULL);
}

/**
 * gda_row_invalidate_value_e:
 * @row: a #GdaRow
 * @value: a #GValue belonging to @row (obtained with gda_row_get_value()).
 * @error: (nullable) (transfer full): the error which lead to the invalidation
 * 
 * Marks @value as being invalid. This method is mainly used by database
 * providers' implementations to report any error while reading a value from the database.
 *
 * Since: 4.2.10
 */
void
gda_row_invalidate_value_e (GdaRow *row, GValue *value, GError *error)
{
	gda_value_set_null (value);
	G_VALUE_TYPE (value) = G_TYPE_INVALID;
	GdaRowPrivate *priv = gda_row_get_instance_private (row);
	if (error) {
		guint i;
		if (! priv->errors)
			priv->errors = g_new0 (GError*, priv->nfields);
		for (i = 0; i < priv->nfields; i++) {
			if (& (priv->fields[i]) == value) {
				if (priv->errors [i])
					g_error_free (priv->errors [i]);
				priv->errors [i] = error;
				break;
			}
		}
		if (i == priv->nfields) {
			g_error_free (error);
			g_warning (_("Value not found in row!"));
		}
	}
	else if (priv->errors) {
		guint i;
		for (i = 0; i < priv->nfields; i++) {
			if (& (priv->fields[i]) == value) {
				if (priv->errors [i]) {
					g_error_free (priv->errors [i]);
					priv->errors [i] = NULL;
				}
				break;
			}
		}
		if (i == priv->nfields)
			g_warning (_("Value not found in row!"));
	}
}

/**
 * gda_row_value_is_valid:
 * @row: a #GdaRow.
 * @value: a #GValue belonging to @row (obtained with gda_row_get_value()).
 *
 * Tells if @value has been marked as being invalid by gda_row_invalidate_value().
 * This method is mainly used by database
 * providers' implementations to report any error while reading a value from the database.
 *
 * Returns: %TRUE if @value is valid
 */
gboolean
gda_row_value_is_valid (GdaRow *row, GValue *value)
{
	return gda_row_value_is_valid_e (row, value, NULL);
}

/**
 * gda_row_value_is_valid_e:
 * @row: a #GdaRow.
 * @value: a #GValue belonging to @row (obtained with gda_row_get_value()).
 * @error: (nullable): a place to store the invalid error, or %NULL
 *
 * Tells if @value has been marked as being invalid by gda_row_invalidate_value().
 * This method is mainly used by database
 * providers' implementations to report any error while reading a value from the database.
 *
 * Returns: %TRUE if @value is valid
 *
 * Since: 4.2.10
 */
gboolean
gda_row_value_is_valid_e (GdaRow *row, GValue *value, GError **error)
{
	gboolean valid;
	valid = (G_VALUE_TYPE (value) == G_TYPE_INVALID) ? FALSE : TRUE;
	GdaRowPrivate *priv = gda_row_get_instance_private (row);
	if (!valid && priv->errors && error) {
		guint i;
		for (i = 0; i < priv->nfields; i++) {
			if (& (priv->fields[i]) == value) {
				if (priv->errors [i])
					g_propagate_error (error, g_error_copy (priv->errors [i]));
				break;
			}
		}
		if (i == priv->nfields)
			g_warning (_("Value not found in row!"));
	}
	return valid;
}

/**
 * gda_row_get_length:
 * @row: a #GdaRow.
 *
 * Returns: the number of columns that the @row has.
 */
gint
gda_row_get_length (GdaRow *row)
{
  g_return_val_if_fail (GDA_IS_ROW (row), 0);
  GdaRowPrivate *priv = gda_row_get_instance_private (row);
  if (priv->model == NULL)
    return priv->nfields;
  return gda_data_model_get_n_columns (priv->model);
}
