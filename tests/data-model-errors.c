/*
 * Copyright (C) 2010 David King <davidk@openismus.com>
 * Copyright (C) 2010 - 2013 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2011 Murray Cumming <murrayc@murrayc.com>
 * Copyright (C) 2019 Daniel Espinosa <esodan@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <string.h>
#include <glib/gstdio.h>
#include <libgda/gda-data-model.h>
#include <libgda/gda-data-model-extra.h>
#include <libgda/gda-row.h>
#include "data-model-errors.h"
#include <libgda/gda-debug-macros.h>
#include "test-errors.h"

#define NCOLS 4
typedef struct {
	gchar *col0;
	gchar *col1;
	gchar *col2;
	gchar *col3;
} ARow;

ARow data[] = {
	{"-",        "Cell 0,1", "Cell 0,2", "Cell 0,3"},
	{"Cell 1,0", "-",        NULL,       "Cell 1,3"},
	{"Cell 2,0", "Cell 2,1", NULL,       "Cell 2,3"},
	{"Cell 3,0", "Cell 3,1", "-",        NULL      },
};


static void gda_data_model_errors_class_init (GdaDataModelErrorsClass *klass);
static void gda_data_model_errors_init       (GdaDataModelErrors *model);
static void gda_data_model_errors_dispose    (GObject *object);

/* GdaDataModel interface */
static void                 gda_data_model_errors_data_model_init (GdaDataModelInterface *iface);
static gint                 gda_data_model_errors_get_n_rows      (GdaDataModel *model);
static gint                 gda_data_model_errors_get_n_columns   (GdaDataModel *model);
static GdaColumn           *gda_data_model_errors_describe_column (GdaDataModel *model, gint col);
static GdaDataModelAccessFlags gda_data_model_errors_get_access_flags(GdaDataModel *model);
static const GValue        *gda_data_model_errors_get_value_at    (GdaDataModel *model, gint col, gint row, GError **error);
static GdaValueAttribute    gda_data_model_errors_get_attributes_at (GdaDataModel *model, gint col, gint row);

static gboolean             gda_data_model_errors_set_value_at (GdaDataModel *model, gint col, gint row, const GValue *value, GError **error);
static gint                 gda_data_model_errors_append_values (GdaDataModel *model, const GList *values, GError **error);
static gboolean             gda_data_model_errors_remove_row (GdaDataModel *model, gint row, GError **error);


typedef struct {
	GSList    *columns; /* list of GdaColumn objects */
	GPtrArray *rows; /* array of GdaRow pointers */
} GdaDataModelErrorsPrivate;

G_DEFINE_TYPE_WITH_CODE (GdaDataModelErrors, gda_data_model_errors, G_TYPE_OBJECT,
                         G_ADD_PRIVATE (GdaDataModelErrors)
                         G_IMPLEMENT_INTERFACE(GDA_TYPE_DATA_MODEL,gda_data_model_errors_data_model_init))

/*
 * Object init and dispose
 */
static void
gda_data_model_errors_data_model_init (GdaDataModelInterface *iface)
{
        iface->get_n_rows = gda_data_model_errors_get_n_rows;
        iface->get_n_columns = gda_data_model_errors_get_n_columns;
        iface->describe_column = gda_data_model_errors_describe_column;
        iface->get_access_flags = gda_data_model_errors_get_access_flags;
        iface->get_value_at = gda_data_model_errors_get_value_at;
        iface->get_attributes_at = gda_data_model_errors_get_attributes_at;

        iface->create_iter = NULL;

        iface->set_value_at = gda_data_model_errors_set_value_at;
        iface->set_values = NULL;
        iface->append_values = gda_data_model_errors_append_values;
        iface->append_row = NULL;
        iface->remove_row = gda_data_model_errors_remove_row;
        iface->find_row = NULL;

        iface->freeze = NULL;
        iface->thaw = NULL;
        iface->get_notify = NULL;
        iface->send_hint = NULL;
}

static void
gda_data_model_errors_init (GdaDataModelErrors *model)
{
	gsize i;
	g_return_if_fail (GDA_IS_DATA_MODEL_ERRORS (model));

	GdaDataModelErrorsPrivate *priv = gda_data_model_errors_get_instance_private (model);
	
	/* columns */
	priv->columns = NULL;
	for (i = 0; i < NCOLS; i++) {
		GdaColumn *col;
		gchar *str;
		col = gda_column_new ();
		gda_column_set_g_type (col, G_TYPE_STRING);
		str = g_strdup_printf ("col%" G_GSIZE_FORMAT, i);
		gda_column_set_name (col, str);
		gda_column_set_description (col, str);
		g_object_set (G_OBJECT (col), "id", str, NULL);
		g_free (str);

		priv->columns = g_slist_append (priv->columns, col);
	}

	/* rows */
	priv->rows = g_ptr_array_new (); /* array of GdaRow pointers */
	for (i = 0; i < (sizeof (data) / sizeof (ARow)); i++) {
		ARow *arow = &(data[i]);
		GdaRow *row = gda_row_new (NCOLS);
		GValue *value;
		value = gda_row_get_value (row, 0);
		if (arow->col0) {
			if (*arow->col0 == '-')
				G_VALUE_TYPE (value) = G_TYPE_INVALID;
			else {
				gda_value_reset_with_type (value, G_TYPE_STRING);
				g_value_set_string (value, arow->col0);
			}
		}

		value = gda_row_get_value (row, 1);
		if (arow->col1) {
			if (*arow->col1 == '-')
				G_VALUE_TYPE (value) = G_TYPE_INVALID;
			else {
				gda_value_reset_with_type (value, G_TYPE_STRING);
				g_value_set_string (value, arow->col1);
			}
		}

		value = gda_row_get_value (row, 2);
		if (arow->col2) {
			if (*arow->col2 == '-')
				G_VALUE_TYPE (value) = G_TYPE_INVALID;
			else {
				gda_value_reset_with_type (value, G_TYPE_STRING);
				g_value_set_string (value, arow->col2);
			}
		}

		value = gda_row_get_value (row, 3);
		if (arow->col3) {
			if (*arow->col3 == '-')
				G_VALUE_TYPE (value) = G_TYPE_INVALID;
			else {
				gda_value_reset_with_type (value, G_TYPE_STRING);
				g_value_set_string (value, arow->col3);
			}
		}

		g_ptr_array_add (priv->rows, row);
	}
}

static void
gda_data_model_errors_class_init (GdaDataModelErrorsClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	/* virtual functions */
	object_class->dispose = gda_data_model_errors_dispose;
}

static void
gda_data_model_errors_dispose (GObject * object)
{
	GdaDataModelErrors *model = (GdaDataModelErrors *) object;

	g_return_if_fail (GDA_IS_DATA_MODEL_ERRORS (model));
	GdaDataModelErrorsPrivate *priv = gda_data_model_errors_get_instance_private (model);

  if (priv->columns) {
    g_slist_free_full (priv->columns, (GDestroyNotify) g_object_unref);
    priv->columns = NULL;
  }
	/* DONT: g_ptr_array_foreach (priv->rows, (GFunc) g_object_unref, NULL);
	 * because we use the convention that G_VALUE_TYPE() == G_TYPE_INVALID for errors */
  if (priv->rows != NULL) {
	  g_ptr_array_free (priv->rows, TRUE);
    priv->rows = NULL;
  }

	G_OBJECT_CLASS (gda_data_model_errors_parent_class)->dispose (object);
}

/*
 * gda_data_model_errors_new
 *
 * Creates a new #GdaDataModel object
 *
 * Returns: a new #GdaDataModel
 */
GdaDataModel *
gda_data_model_errors_new (void)
{
	GdaDataModel *model;
	model = (GdaDataModel *) g_object_new (GDA_TYPE_DATA_MODEL_ERRORS, NULL);

	return model;
}

static gint
gda_data_model_errors_get_n_rows (GdaDataModel *model)
{
	GdaDataModelErrors *imodel = (GdaDataModelErrors *) model;

	g_return_val_if_fail (GDA_IS_DATA_MODEL_ERRORS (imodel), 0);
	GdaDataModelErrorsPrivate *priv = gda_data_model_errors_get_instance_private (imodel);

	return priv->rows->len;
}

static gint
gda_data_model_errors_get_n_columns (G_GNUC_UNUSED GdaDataModel *model)
{
	return NCOLS;
}

static GdaColumn *
gda_data_model_errors_describe_column (GdaDataModel *model, gint col)
{
	GdaDataModelErrors *imodel;
	g_return_val_if_fail (GDA_IS_DATA_MODEL_ERRORS (model), NULL);
	imodel = GDA_DATA_MODEL_ERRORS (model);
	GdaDataModelErrorsPrivate *priv = gda_data_model_errors_get_instance_private (imodel);

	return g_slist_nth_data (priv->columns, col);
}

static GdaDataModelAccessFlags
gda_data_model_errors_get_access_flags (G_GNUC_UNUSED GdaDataModel *model)
{
	GdaDataModelAccessFlags flags;

	flags = GDA_DATA_MODEL_ACCESS_CURSOR_FORWARD | 
		GDA_DATA_MODEL_ACCESS_CURSOR_BACKWARD |
		GDA_DATA_MODEL_ACCESS_RANDOM |
		GDA_DATA_MODEL_ACCESS_WRITE;

	return flags;
}

static const GValue *
gda_data_model_errors_get_value_at (GdaDataModel *model, gint col, gint row, GError **error)
{
	GdaDataModelErrors *imodel;
	GValue *value = NULL;
	GdaRow *drow;

	g_return_val_if_fail (GDA_IS_DATA_MODEL_ERRORS (model), NULL);
	imodel = GDA_DATA_MODEL_ERRORS (model);
	GdaDataModelErrorsPrivate *priv = gda_data_model_errors_get_instance_private (imodel);

	if ((col < 0) || (col > NCOLS)) {
		gchar *tmp;
		tmp = g_strdup_printf ("Column %d out of range (0-%d)", col, NCOLS-1);
		g_set_error (error, GDA_DATA_MODEL_ERROR, GDA_DATA_MODEL_COLUMN_OUT_OF_RANGE_ERROR,
			      "%s", tmp);
		g_free (tmp);
		return NULL;
	}

	if (row >= (gint)priv->rows->len) {
		gchar *str;
		if (priv->rows->len > 0)
			str = g_strdup_printf ("Row %d out of range (0-%d)", row,
					       priv->rows->len - 1);
		else
			str = g_strdup_printf ("Row %d not found (empty data model)", row);
		g_set_error (error, GDA_DATA_MODEL_ERROR, GDA_DATA_MODEL_ROW_OUT_OF_RANGE_ERROR,
			      "%s", str);
		g_free (str);
                return NULL;
        }

	drow =  g_ptr_array_index (priv->rows, row);
	if (drow) {
		GValue *val = gda_row_get_value (drow, col);
		if (G_VALUE_TYPE (val) == G_TYPE_INVALID) {
			/* simulates an error */
			g_set_error (error, GDA_DATA_MODEL_ERROR, GDA_DATA_MODEL_ACCESS_ERROR,
				     "%s", "Simulated error");
		}
		else
			value = val;
	}
	else
		g_set_error (error, GDA_DATA_MODEL_ERROR, GDA_DATA_MODEL_ROW_NOT_FOUND_ERROR,
			      "%s", "Row not found");

	return value;
}

static GdaValueAttribute
gda_data_model_errors_get_attributes_at (G_GNUC_UNUSED GdaDataModel *model, gint col, G_GNUC_UNUSED gint row)
{
	GdaValueAttribute flags = 0;
	if ((col < 0) || (col > NCOLS)) {
		gchar *tmp;
		tmp = g_strdup_printf ("Column %d out of range (0-%d)", col, NCOLS-1);
		g_free (tmp);
		return 0;
	}

	flags = 0;
	return flags;
}

static gboolean
gda_data_model_errors_set_value_at (GdaDataModel *model, gint col, gint row, const GValue *value, GError **error)
{
	gboolean retval = TRUE;
	GdaDataModelErrors *imodel;

	g_return_val_if_fail (GDA_IS_DATA_MODEL_ERRORS (model), FALSE);
	imodel = GDA_DATA_MODEL_ERRORS (model);
	GdaDataModelErrorsPrivate *priv = gda_data_model_errors_get_instance_private (imodel);

	if ((col < 0) || (col > NCOLS)) {
		gchar *tmp;
		tmp = g_strdup_printf ("Column %d out of range (0-%d)", col, NCOLS-1);
		g_set_error (error, TEST_ERROR, TEST_ERROR_GENERIC, "%s", tmp);
		g_free (tmp);
		return FALSE;
	}

	GdaRow *drow;
	drow =  g_ptr_array_index (priv->rows, row);
	if (drow) {
		GValue *dvalue;
		dvalue = gda_row_get_value (drow, col);
		gda_value_reset_with_type (dvalue, G_VALUE_TYPE (value));
		g_value_copy (value, dvalue);
		gda_data_model_row_updated (model, row);
	}
	else {
		g_set_error (error, GDA_DATA_MODEL_ERROR, GDA_DATA_MODEL_ROW_NOT_FOUND_ERROR,
			      "%s", "Row not found");
		retval = FALSE;
	}

	return retval;
}



static gint
gda_data_model_errors_append_values (G_GNUC_UNUSED GdaDataModel *model, G_GNUC_UNUSED const GList *values, GError **error)
{
	TO_IMPLEMENT;
	g_set_error (error, GDA_DATA_MODEL_ERROR, GDA_DATA_MODEL_FEATURE_NON_SUPPORTED_ERROR,
			      "%s", "Not implemented");
	return -1;
}

static gboolean
gda_data_model_errors_remove_row (GdaDataModel *model, gint row, GError **error)
{
	GdaDataModelErrors *imodel;

	g_return_val_if_fail (GDA_IS_DATA_MODEL_ERRORS (model), FALSE);
	imodel = (GdaDataModelErrors *) model;
	GdaDataModelErrorsPrivate *priv = gda_data_model_errors_get_instance_private (imodel);

	if (row >= (gint)priv->rows->len) {
		gchar *str;
		if (priv->rows->len > 0)
			str = g_strdup_printf ("Row %d out of range (0-%d)", row,
					       priv->rows->len - 1);
		else
			str = g_strdup_printf ("Row %d not found (empty data model)", row);
		g_set_error (error, TEST_ERROR, TEST_ERROR_GENERIC, "%s", str);
		g_free (str);
                return FALSE;
        }

	GdaRow *drow;
	drow =  g_ptr_array_index (priv->rows, row);
	/* remove row from data model */
	g_object_unref (drow);
	g_ptr_array_remove_index (priv->rows, row);
	gda_data_model_row_removed (model, row);
	
	return TRUE;
}

