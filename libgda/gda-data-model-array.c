/*
 * Copyright (C) 2001 Cleber Rodrigues <cleberrrjr@bol.com.br>
 * Copyright (C) 2001 - 2005 Rodrigo Moya <rodrigo@gnome-db.org>
 * Copyright (C) 2002 Gonzalo Paniagua Javier <gonzalo@gnome-db.org>
 * Copyright (C) 2003 Laurent Sansonetti <lrz@gnome.org>
 * Copyright (C) 2004 Szalai Ferenc <szferi@einstein.ki.iif.hu>
 * Copyright (C) 2005 Bas Driessen <bas.driessen@xobas.com>
 * Copyright (C) 2005 - 2015 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2005 Álvaro Peña <alvaropg@telefonica.net>
 * Copyright (C) 2006 - 2008 Murray Cumming <murrayc@murrayc.com>
 * Copyright (C) 2008 Phil Longstaff <plongstaff@rogers.com>
 * Copyright (C) 2008 Przemysław Grzegorczyk <pgrzegorczyk@gmail.com>
 * Copyright (C) 2010 David King <davidk@openismus.com>
 * Copyright (C) 2010 Jonh Wendell <jwendell@gnome.org>
 * Copyright (C) 2011 Daniel Espinosa <esodan@gmail.com>
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

#include <string.h>
#include <glib.h>
#include <libgda/gda-data-model-array.h>
#include <glib/gi18n-lib.h>
#include <libgda/gda-data-model.h>
#include <libgda/gda-data-model-extra.h>
#include <libgda/gda-util.h>

struct _GdaDataModelArrayPrivate {
	gboolean       notify_changes;
        GHashTable    *column_spec;

        gboolean       read_only;

	/* number of columns in each row */
	gint           number_of_columns;

	/* the array of rows, each item is a GdaRow */
	GArray        *rows;
};

enum {
	PROP_0,
	PROP_READ_ONLY,
	PROP_N_COLUMNS
};

static void gda_data_model_array_class_init   (GdaDataModelArrayClass *klass);
static void gda_data_model_array_init         (GdaDataModelArray *model,
					       GdaDataModelArrayClass *klass);
static void gda_data_model_array_finalize     (GObject *object);
static void gda_data_model_array_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec);
static void gda_data_model_array_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec);

static GObjectClass *parent_class = NULL;

/* GdaDataModel interface */
static void                 gda_data_model_array_data_model_init (GdaDataModelIface *iface);
static gint                 gda_data_model_array_get_n_rows      (GdaDataModel *model);
static gint                 gda_data_model_array_get_n_columns   (GdaDataModel *model);
static GdaColumn           *gda_data_model_array_describe_column (GdaDataModel *model, gint col);
static const GValue        *gda_data_model_array_get_value_at    (GdaDataModel *model, gint col, gint row, GError **error);
static GdaValueAttribute    gda_data_model_array_get_attributes_at (GdaDataModel *model, gint col, gint row);
static GdaDataModelAccessFlags gda_data_model_array_get_access_flags(GdaDataModel *model);

static gboolean             gda_data_model_array_set_value_at    (GdaDataModel *model, gint col, gint row,
                                                                 const GValue *value, GError **error);
static gboolean             gda_data_model_array_set_values      (GdaDataModel *model, gint row,
                                                                 GList *values, GError **error);
static gint                 gda_data_model_array_append_values   (GdaDataModel *model, const GList *values, GError **error);
static gint                 gda_data_model_array_append_row      (GdaDataModel *model, GError **error);
static gboolean             gda_data_model_array_remove_row      (GdaDataModel *model, gint row, GError **error);

static void                 gda_data_model_array_set_notify      (GdaDataModel *model, gboolean do_notify_changes);
static gboolean             gda_data_model_array_get_notify      (GdaDataModel *model);

/*
 * GdaDataModelArray class implementation
 */

static void
gda_data_model_array_data_model_init (GdaDataModelIface *iface)
{
        iface->i_get_n_rows = gda_data_model_array_get_n_rows;
        iface->i_get_n_columns = gda_data_model_array_get_n_columns;
        iface->i_describe_column = gda_data_model_array_describe_column;
        iface->i_get_access_flags = gda_data_model_array_get_access_flags;
        iface->i_get_value_at = gda_data_model_array_get_value_at;
        iface->i_get_attributes_at = gda_data_model_array_get_attributes_at;

        iface->i_create_iter = NULL;
        iface->i_iter_at_row = NULL;
        iface->i_iter_next = NULL;
        iface->i_iter_prev = NULL;

        iface->i_set_value_at = gda_data_model_array_set_value_at;
	iface->i_iter_set_value = NULL;
        iface->i_set_values = gda_data_model_array_set_values;
        iface->i_append_values = gda_data_model_array_append_values;
        iface->i_append_row = gda_data_model_array_append_row;
        iface->i_remove_row = gda_data_model_array_remove_row;
        iface->i_find_row = NULL;

        iface->i_set_notify = gda_data_model_array_set_notify;
        iface->i_get_notify = gda_data_model_array_get_notify;
        iface->i_send_hint = NULL;
}

GType
gda_data_model_array_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static GMutex registering;
		static const GTypeInfo info = {
			sizeof (GdaDataModelArrayClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_data_model_array_class_init,
			NULL,
			NULL,
			sizeof (GdaDataModelArray),
			0,
			(GInstanceInitFunc) gda_data_model_array_init,
			0
		};

		static const GInterfaceInfo data_model_info = {
                        (GInterfaceInitFunc) gda_data_model_array_data_model_init,
                        NULL,
                        NULL
                };


		g_mutex_lock (&registering);
		if (type == 0) {
			type = g_type_register_static (G_TYPE_OBJECT, "GdaDataModelArray", &info, 0);
			g_type_add_interface_static (type, GDA_TYPE_DATA_MODEL, &data_model_info);
		}
		g_mutex_unlock (&registering);
	}
	return type;
}

static void
gda_data_model_array_class_init (GdaDataModelArrayClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = gda_data_model_array_finalize;
	object_class->set_property = gda_data_model_array_set_property;
	object_class->get_property = gda_data_model_array_get_property;

	g_object_class_install_property (object_class,
	                                 PROP_N_COLUMNS,
	                                 g_param_spec_uint ("n-columns",
	                                                    "Number of columns",
	                                                    "The number of columns in the model",
	                                                    0,
	                                                    G_MAXUINT,
	                                                    0,
	                                                    G_PARAM_READWRITE));
	g_object_class_install_property (object_class, PROP_READ_ONLY,
                                         g_param_spec_boolean ("read-only", NULL, 
							       _("Whether data model can be modified"),
                                                               FALSE,
                                                               G_PARAM_READABLE | G_PARAM_WRITABLE));
}

static void
gda_data_model_array_init (GdaDataModelArray *model, G_GNUC_UNUSED GdaDataModelArrayClass *klass)
{
	g_return_if_fail (GDA_IS_DATA_MODEL_ARRAY (model));

	/* allocate internal structure */
	model->priv = g_new0 (GdaDataModelArrayPrivate, 1);
	model->priv->notify_changes = TRUE;
        model->priv->column_spec = g_hash_table_new_full (g_int_hash, g_int_equal, g_free, NULL);
        model->priv->read_only = FALSE;
	model->priv->number_of_columns = 0;
	model->priv->rows = g_array_new (FALSE, FALSE, sizeof (GdaRow *));
}

static void column_g_type_changed_cb (GdaColumn *column, GType old, GType new, GdaDataModelArray *model);

static void
hash_free_column (G_GNUC_UNUSED gpointer key, GdaColumn *column, GdaDataModelArray *model)
{
        g_signal_handlers_disconnect_by_func (G_OBJECT (column),
                                              G_CALLBACK (column_g_type_changed_cb), model);
        g_object_unref (column);
}

static void
gda_data_model_array_finalize (GObject *object)
{
	GdaDataModelArray *model = (GdaDataModelArray *) object;

	g_return_if_fail (GDA_IS_DATA_MODEL_ARRAY (model));

	/* free memory */
	gda_data_model_freeze (GDA_DATA_MODEL(model));
	gda_data_model_array_clear (model);
	g_array_free (model->priv->rows, TRUE);
	g_hash_table_foreach (model->priv->column_spec, (GHFunc) hash_free_column, model);
        g_hash_table_destroy (model->priv->column_spec);
        model->priv->column_spec = NULL;

	g_free (model->priv);
	model->priv = NULL;

	/* chain to parent class */
	parent_class->finalize (object);
}

static void
gda_data_model_array_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	GdaDataModelArray *model;

	g_return_if_fail (GDA_IS_DATA_MODEL_ARRAY (object));
	model = GDA_DATA_MODEL_ARRAY (object);

	switch (prop_id) {
	case PROP_READ_ONLY:
		model->priv->read_only = g_value_get_boolean (value);
		break;
	case PROP_N_COLUMNS:
		gda_data_model_array_set_n_columns (model, g_value_get_uint (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
gda_data_model_array_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	GdaDataModelArray *model;

	g_return_if_fail (GDA_IS_DATA_MODEL_ARRAY (object));
	model = GDA_DATA_MODEL_ARRAY (object);

	switch (prop_id) {
	case PROP_READ_ONLY:
		g_value_set_boolean (value, model->priv->read_only);
		break;
	case PROP_N_COLUMNS:
		g_value_set_uint (value, model->priv->number_of_columns);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}



/**
 * gda_data_model_array_new:
 * @cols: number of columns for rows in this data model.
 *
 * Creates a new #GdaDataModel object without initializing the column
 * types. Using gda_data_model_array_new_with_g_types() is usually better.
 *
 * Returns: (transfer full): a pointer to the newly created #GdaDataModel.
 */
GdaDataModel *
gda_data_model_array_new (gint cols)
{
	GdaDataModel *model;

	model = g_object_new (GDA_TYPE_DATA_MODEL_ARRAY, "n-columns", cols, NULL);
	return model;
}

/**
 * gda_data_model_array_new_with_g_types:
 * @cols: number of columns for rows in this data model.
 * @...: types of the columns of the model to create as #GType, as many as indicated by @cols
 * 
 * Creates a new #GdaDataModel object with the column types as
 * specified.
 *
 * Returns: (transfer full): a pointer to the newly created #GdaDataModel.
 */
GdaDataModel *
gda_data_model_array_new_with_g_types (gint cols, ...)
{
	GdaDataModel *model;
	va_list args;
	gint i;

	model = gda_data_model_array_new (cols);
	va_start (args, cols);
	i = 0;
	while (i < cols) {
		GType argtype;

		argtype = va_arg (args, GType);

		gda_column_set_g_type (gda_data_model_describe_column (model, i),
					 argtype);
		i++;
	}
	va_end (args);
	return model;
}

/**
 * gda_data_model_array_new_with_g_types_v: (rename-to gda_data_model_array_new_with_g_types)
 * @cols: number of columns for rows in this data model.
 * @types: (array): array of types of the columns of the model to create as #GType, as many as indicated by @cols
 *
 * Creates a new #GdaDataModel object with the column types as
 * specified.
 *
 * Returns: (transfer full): a pointer to the newly created #GdaDataModel.
 *
 * Since: 4.2.6
 */
GdaDataModel *
gda_data_model_array_new_with_g_types_v (gint cols, GType *types)
{
	GdaDataModel *model;
	gint i;

	model = gda_data_model_array_new (cols);
	i = 0;
	while (i < cols) {
		GType type = types [i];
		gda_column_set_g_type (gda_data_model_describe_column (model, i), type);
		i++;
	}
	return model;
}

/**
 * gda_data_model_array_copy_model:
 * @src: a #GdaDataModel to copy data from
 * @error: a place to store errors, or %NULL
 *
 * Makes a copy of @src into a new #GdaDataModelArray object
 *
 * Returns: (transfer full) (allow-none): a new data model, or %NULL if an error occurred
 */
GdaDataModelArray *
gda_data_model_array_copy_model (GdaDataModel *src, GError **error)
{
	GdaDataModel *model;
	gint nbfields, i;

	g_return_val_if_fail (GDA_IS_DATA_MODEL (src), NULL);

	nbfields = gda_data_model_get_n_columns (src);
	model = gda_data_model_array_new (nbfields);

	if (g_object_get_data (G_OBJECT (src), "name"))
		g_object_set_data_full (G_OBJECT (model), "name", g_strdup (g_object_get_data (G_OBJECT (src), "name")), g_free);
	if (g_object_get_data (G_OBJECT (src), "descr"))
		g_object_set_data_full (G_OBJECT (model), "descr", g_strdup (g_object_get_data (G_OBJECT (src), "descr")), g_free);
	for (i = 0; i < nbfields; i++) {
		GdaColumn *copycol, *srccol;
		gchar *colid;

		srccol = gda_data_model_describe_column (src, i);
		copycol = gda_data_model_describe_column (model, i);

		g_object_get (G_OBJECT (srccol), "id", &colid, NULL);
		g_object_set (G_OBJECT (copycol), "id", colid, NULL);
		g_free (colid);
		gda_column_set_description (copycol, gda_column_get_description (srccol));
		gda_column_set_name (copycol, gda_column_get_name (srccol));
		gda_column_set_dbms_type (copycol, gda_column_get_dbms_type (srccol));
		gda_column_set_g_type (copycol, gda_column_get_g_type (srccol));
		gda_column_set_position (copycol, gda_column_get_position (srccol));
		gda_column_set_allow_null (copycol, gda_column_get_allow_null (srccol));
	}

	if (! gda_data_model_import_from_model (model, src, FALSE, NULL, error)) {
		g_object_unref (model);
		model = NULL;
	}
	/*else
	  gda_data_model_dump (model, stdout);*/

	return (GdaDataModelArray*) model;
}

/**
 * gda_data_model_array_copy_model_ext:
 * @src: a #GdaDataModel to copy data from
 * @ncols: size of @cols
 * @cols: (array length=ncols): array of @src's columns to copy into the new array, not %NULL
 * @error: a place to store errors, or %NULL
 *
 * Like gda_data_model_array_copy_model(), makes a copy of @src, but copies only some
 * columns.
 *
 * Returns: (transfer full) (allow-none): a new data model, or %NULL if an error occurred
 *
 * Since: 5.2.0
 */
GdaDataModelArray *
gda_data_model_array_copy_model_ext (GdaDataModel *src, gint ncols, gint *cols, GError **error)
{
	GdaDataModel *model;
	gint nbfields, i;

	g_return_val_if_fail (GDA_IS_DATA_MODEL (src), NULL);
	g_return_val_if_fail (cols, NULL);
	g_return_val_if_fail (ncols > 0, NULL);

	/* check columns' validity */
	nbfields = gda_data_model_get_n_columns (src);
	for (i = 0; i < ncols; i++) {
		if ((cols[i] < 0) || (cols[i] >= nbfields)) {
			g_set_error (error, GDA_DATA_MODEL_ERROR,
				     GDA_DATA_MODEL_COLUMN_OUT_OF_RANGE_ERROR,
                                     _("Column %d out of range (0-%d)"), cols[i], nbfields - 1);
			return NULL;
		}
	}

	/* initialize new model */
	model = gda_data_model_array_new (ncols);
	if (g_object_get_data (G_OBJECT (src), "name"))
		g_object_set_data_full (G_OBJECT (model), "name", g_strdup (g_object_get_data (G_OBJECT (src), "name")), g_free);
	if (g_object_get_data (G_OBJECT (src), "descr"))
		g_object_set_data_full (G_OBJECT (model), "descr", g_strdup (g_object_get_data (G_OBJECT (src), "descr")), g_free);


	/* map new columns */
	GHashTable *hash;
	hash = g_hash_table_new_full (g_int_hash, g_int_equal, g_free, NULL);
	for (i = 0; i < ncols; i++) {
		gint *ptr;
		ptr = g_new (gint, 1);
		*ptr = i;
		g_hash_table_insert (hash, ptr, GINT_TO_POINTER (cols[i]));

		GdaColumn *copycol, *srccol;
		gchar *colid;

		srccol = gda_data_model_describe_column (src, cols[i]);
		copycol = gda_data_model_describe_column (model, i);

		g_object_get (G_OBJECT (srccol), "id", &colid, NULL);
		g_object_set (G_OBJECT (copycol), "id", colid, NULL);
		g_free (colid);
		gda_column_set_description (copycol, gda_column_get_description (srccol));
		gda_column_set_name (copycol, gda_column_get_name (srccol));
		gda_column_set_dbms_type (copycol, gda_column_get_dbms_type (srccol));
		gda_column_set_g_type (copycol, gda_column_get_g_type (srccol));
		gda_column_set_position (copycol, gda_column_get_position (srccol));
		gda_column_set_allow_null (copycol, gda_column_get_allow_null (srccol));
	}

	if (! gda_data_model_import_from_model (model, src, FALSE, hash, error)) {
		g_hash_table_destroy (hash);
		g_object_unref (model);
		model = NULL;
	}
	/*else
	  gda_data_model_dump (model, stdout);*/

	g_hash_table_destroy (hash);
	return (GdaDataModelArray*) model;
}

/**
 * gda_data_model_array_get_row:
 * @model: a #GdaDataModelArray object
 * @row: row number (starting from 0)
 * @error: a place to store errors, or %NULL
 *
 * Get a pointer to a row in @model
 *
 * Returns: (transfer none): the #GdaRow, or %NULL if an error occurred
 */
GdaRow *
gda_data_model_array_get_row (GdaDataModelArray *model, gint row, GError **error)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL_ARRAY (model), NULL);
	g_return_val_if_fail (row >= 0, NULL);

	if ((guint)row >= model->priv->rows->len) {
		if (model->priv->rows->len > 0)
			g_set_error (error, GDA_DATA_MODEL_ERROR, GDA_DATA_MODEL_ROW_OUT_OF_RANGE_ERROR,
				     _("Row %d out of range (0-%d)"), row,
				     model->priv->rows->len - 1);
		else
			g_set_error (error, GDA_DATA_MODEL_ERROR, GDA_DATA_MODEL_ROW_OUT_OF_RANGE_ERROR,
				     _("Row %d not found (empty data model)"), row);
		return NULL;
	}

	return g_array_index (model->priv->rows, GdaRow*, row);
}

/**
 * gda_data_model_array_set_n_columns:
 * @model: the #GdaDataModelArray.
 * @cols: number of columns for rows this data model should use.
 *
 * Sets the number of columns for rows inserted in this model. 
 * @cols must be greated than or equal to 0.
 *
 * Also clears @model's contents.
 */
void
gda_data_model_array_set_n_columns (GdaDataModelArray *model, gint cols)
{
	g_return_if_fail (GDA_IS_DATA_MODEL_ARRAY (model));

	gda_data_model_array_clear (model);
	model->priv->number_of_columns = cols;

	g_object_notify (G_OBJECT (model), "n-columns");
}

/**
 * gda_data_model_array_clear:
 * @model: the model to clear.
 *
 * Frees all the rows in @model.
 */
void
gda_data_model_array_clear (GdaDataModelArray *model)
{
	g_return_if_fail (GDA_IS_DATA_MODEL_ARRAY (model));

	while (model->priv->rows->len > 0) 
		gda_data_model_array_remove_row ((GdaDataModel*) model, 0, NULL);
}


/*
 * GdaDataModel interface
 */
static gint
gda_data_model_array_get_n_rows (GdaDataModel *model)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL_ARRAY (model), -1);
	return GDA_DATA_MODEL_ARRAY (model)->priv->rows->len;
}

static gint
gda_data_model_array_get_n_columns (GdaDataModel *model)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL_ARRAY (model), -1);
	return GDA_DATA_MODEL_ARRAY (model)->priv->number_of_columns;
}

static GdaColumn *
gda_data_model_array_describe_column (GdaDataModel *model, gint col)
{
        GdaColumn *column;

        if (col >= gda_data_model_get_n_columns (model)) {
                g_warning ("Column %d out of range (0-%d)", col, gda_data_model_get_n_columns (model) - 1);
                return NULL;
        }

	gint tmp;
	tmp = col;
        column = g_hash_table_lookup (((GdaDataModelArray*) model)->priv->column_spec, &tmp);
        if (!column) {
                column = gda_column_new ();
                g_signal_connect (G_OBJECT (column), "g-type-changed",
                                  G_CALLBACK (column_g_type_changed_cb), model);
                gda_column_set_position (column, col);

		gint *ptr;
		ptr = g_new (gint, 1);
		*ptr = col;
                g_hash_table_insert (((GdaDataModelArray*) model)->priv->column_spec, ptr, column);
        }

        return column;
}

static void
column_g_type_changed_cb (GdaColumn *column, G_GNUC_UNUSED GType old, GType new, GdaDataModelArray *model)
{
        /* emit a warning if there are GValues which are not compatible with the new type */
        gint i, nrows, col;
        const GValue *value;
        gchar *str;
        gint nb_warnings = 0;
	const gint max_warnings = 5;

        if ((new == G_TYPE_INVALID) || (new == GDA_TYPE_NULL))
                return;

        col = gda_column_get_position (column);
	nrows = model->priv->rows->len;
        for (i = 0; (i < nrows) && (nb_warnings < max_warnings); i++) {
                GType vtype;

                value = gda_data_model_get_value_at ((GdaDataModel *) model, col, i, NULL);
                if (!value)
			continue;

		vtype = G_VALUE_TYPE ((GValue *) value);
                if ((vtype != GDA_TYPE_NULL) && (vtype != new)) {
                        nb_warnings ++;
                        if (nb_warnings < max_warnings) {
                                if (nb_warnings == max_warnings)
                                        g_warning ("Max number of warning reached, "
                                                   "more incompatible types...");
                                else {
                                        str = gda_value_stringify ((GValue *) value);
                                        g_warning ("Value of type %s not compatible with new"
                                                   " column type %s (value=%s)",
                                                   gda_g_type_to_string (G_VALUE_TYPE ((GValue *) value)),
                                                   gda_g_type_to_string (new), str);
                                        g_free (str);
                                }
                        }
                }
        }
}

static const GValue *
gda_data_model_array_get_value_at (GdaDataModel *model, gint col, gint row, GError **error)
{
	GdaRow *fields;
	GdaDataModelArray *amodel = (GdaDataModelArray*) model;

	g_return_val_if_fail(row >= 0, NULL);

	if (amodel->priv->rows->len == 0) {
		g_set_error (error, GDA_DATA_MODEL_ERROR, GDA_DATA_MODEL_ROW_NOT_FOUND_ERROR,
			      "%s", _("No row in data model"));
		return NULL;
	}

	if ((guint)row >= amodel->priv->rows->len) {
		if (amodel->priv->rows->len > 0)
			g_set_error (error, GDA_DATA_MODEL_ERROR, GDA_DATA_MODEL_ROW_OUT_OF_RANGE_ERROR,
				     _("Row %d out of range (0-%d)"), row, amodel->priv->rows->len - 1);
		else
			g_set_error (error, GDA_DATA_MODEL_ERROR, GDA_DATA_MODEL_ROW_OUT_OF_RANGE_ERROR,
				     _("Row %d not found (empty data model)"), row);
		return NULL;
	}

	if (col >= amodel->priv->number_of_columns) {
		g_set_error (error, GDA_DATA_MODEL_ERROR, GDA_DATA_MODEL_COLUMN_OUT_OF_RANGE_ERROR,
			     _("Column %d out of range (0-%d)"), col, amodel->priv->number_of_columns - 1);
		return NULL;
	}

	fields = g_array_index (amodel->priv->rows, GdaRow*, row);
	if (fields) {
		GValue *field;

		field = gda_row_get_value (fields, col);
		if (gda_row_value_is_valid (fields, field))
			return (const GValue *) field;
		else
			return NULL;
	}
	else {
		g_set_error (error, GDA_DATA_MODEL_ERROR, GDA_DATA_MODEL_ROW_NOT_FOUND_ERROR,
			      "%s", _("Data model has no data"));
		return NULL;
	}
}

static GdaValueAttribute
gda_data_model_array_get_attributes_at (GdaDataModel *model, gint col, gint row)
{
        const GValue *gdavalue;
        GdaValueAttribute flags = 0;
        GdaColumn *column;

        column = gda_data_model_array_describe_column (model, col);
        if (gda_column_get_allow_null (column))
                flags |= GDA_VALUE_ATTR_CAN_BE_NULL;
        if (gda_column_get_default_value (column))
                flags |= GDA_VALUE_ATTR_CAN_BE_DEFAULT;

        if (row >= 0) {
                gdavalue = gda_data_model_get_value_at (model, col, row, NULL);
                if (!gdavalue || gda_value_is_null ((GValue *) gdavalue))
                        flags |= GDA_VALUE_ATTR_IS_NULL;
        }

        if (((GdaDataModelArray *)model)->priv->read_only)
                flags |= GDA_VALUE_ATTR_NO_MODIF;

        return flags;
}


static GdaDataModelAccessFlags
gda_data_model_array_get_access_flags (GdaDataModel *model)
{
        GdaDataModelAccessFlags flags = GDA_DATA_MODEL_ACCESS_RANDOM |
                GDA_DATA_MODEL_ACCESS_CURSOR_FORWARD |
                GDA_DATA_MODEL_ACCESS_CURSOR_BACKWARD;

        if (! ((GdaDataModelArray *)model)->priv->read_only)
                flags |= GDA_DATA_MODEL_ACCESS_WRITE;

        return flags;
}

static gboolean
gda_data_model_array_set_value_at (GdaDataModel *model, gint col, gint row,
				   const GValue *value, GError **error)
{
        GdaRow *gdarow;
	GdaDataModelArray *amodel = (GdaDataModelArray*) model;

        g_return_val_if_fail (row >= 0, FALSE);

	if (amodel->priv->read_only) {
		g_set_error (error, GDA_DATA_MODEL_ERROR, GDA_DATA_MODEL_ACCESS_ERROR,
                              "%s", _("Attempting to modify a read-only data model"));
                return FALSE;
        }

	if ((guint)row > amodel->priv->rows->len) {
		if (amodel->priv->rows->len > 0)
			g_set_error (error, GDA_DATA_MODEL_ERROR, GDA_DATA_MODEL_VALUES_LIST_ERROR,
				     _("Row %d out of range (0-%d)"), row, amodel->priv->rows->len - 1);
		else
			g_set_error (error, GDA_DATA_MODEL_ERROR, GDA_DATA_MODEL_ROW_OUT_OF_RANGE_ERROR,
				     _("Row %d not found (empty data model)"), row);
                return FALSE;
        }

	gdarow = gda_data_model_array_get_row ((GdaDataModelArray *) model, row, error);
        if (gdarow) {
		GValue *dest;
		dest = gda_row_get_value (gdarow, col);
		if (value) {
			gda_value_reset_with_type (dest, G_VALUE_TYPE ((GValue *) value));
			gda_value_set_from_value (dest, value);
		}
		else
			gda_value_set_null (dest);
		gda_data_model_row_updated ((GdaDataModel *) model, row);
		return TRUE;
        }
        else 
                return FALSE;
}

static gboolean
gda_data_model_array_set_values (GdaDataModel *model, gint row, GList *values, GError **error)
{
        GdaRow *gdarow;
	GdaDataModelArray *amodel = (GdaDataModelArray*) model;

        g_return_val_if_fail (row >= 0, FALSE);

        if (!values)
                return TRUE;

	if (amodel->priv->read_only) {
		g_set_error (error, GDA_DATA_MODEL_ERROR, GDA_DATA_MODEL_ACCESS_ERROR,
                              "%s", _("Attempting to modify a read-only data model"));
                return FALSE;
        }

        if (g_list_length (values) > (guint)gda_data_model_get_n_columns (model)) {
                g_set_error (error, GDA_DATA_MODEL_ERROR, GDA_DATA_MODEL_VALUES_LIST_ERROR,
                              "%s", _("Too many values in list"));
                return FALSE;
        }

	gdarow = gda_data_model_array_get_row (amodel, row, error);
        if (gdarow) {
                GList *list;
                gint col;
		for (list = values, col = 0; list; list = list->next, col++) {
			GValue *dest;
			dest = gda_row_get_value (gdarow, col);
			if (list->data) {
				gda_value_reset_with_type (dest, G_VALUE_TYPE ((GValue *) list->data));
				gda_value_set_from_value (dest, (GValue *) list->data);
			}
		}
		gda_data_model_row_updated (model, row);
		return TRUE;
        }

        return FALSE;
}

static gint
gda_data_model_array_append_values (GdaDataModel *model, const GList *values, GError **error)
{
        GdaRow *row;
	const GList *list;
	gint i;
	GdaDataModelArray *amodel = (GdaDataModelArray *) model;
	
        if (amodel->priv->read_only) {
		g_set_error (error, GDA_DATA_MODEL_ERROR, GDA_DATA_MODEL_ACCESS_ERROR,
                              "%s", _("Attempting to modify a read-only data model"));
                return FALSE;
        }

	if (g_list_length ((GList *) values) > (guint)amodel->priv->number_of_columns) {
                g_set_error (error, GDA_DATA_MODEL_ERROR, GDA_DATA_MODEL_VALUES_LIST_ERROR,
                              "%s", _("Too many values in list"));
                return FALSE;
        }

	row = gda_row_new (amodel->priv->number_of_columns);
	for (i = 0, list = values; list; i++, list = list->next) {
		GValue *dest;
		dest = gda_row_get_value (row, i);
		if (list->data) {
			gda_value_reset_with_type (dest, G_VALUE_TYPE ((GValue *) list->data));
			g_assert (gda_value_set_from_value (dest, (GValue *) list->data));
		}
		else
			gda_value_set_null (dest);
	}

	g_array_append_val (amodel->priv->rows, row);
	gda_data_model_row_inserted (model, amodel->priv->rows->len - 1);
	return amodel->priv->rows->len - 1;
}

static gint
gda_data_model_array_append_row (GdaDataModel *model, GError **error)
{
	GdaRow *row;
	GdaDataModelArray *amodel = (GdaDataModelArray *) model;

	if (amodel->priv->read_only) {
		g_set_error (error, GDA_DATA_MODEL_ERROR, GDA_DATA_MODEL_ACCESS_ERROR,
                              "%s", _("Attempting to modify a read-only data model"));
                return FALSE;
        }

	row = gda_row_new (amodel->priv->number_of_columns);
	g_array_append_val (amodel->priv->rows, row);
	gda_data_model_row_inserted (model, amodel->priv->rows->len - 1);
	return amodel->priv->rows->len - 1;
}

static gboolean
gda_data_model_array_remove_row (GdaDataModel *model, gint row, GError **error)
{
	GdaRow *gdarow;
	GdaDataModelArray *amodel = (GdaDataModelArray *) model;

	gdarow = g_array_index (amodel->priv->rows, GdaRow*, row);
	if (gdarow) {
		amodel->priv->rows = g_array_remove_index (amodel->priv->rows, row);
		gda_data_model_row_removed ((GdaDataModel *) model, row);
		g_object_unref (gdarow);
		return TRUE;
	}

	g_set_error (error, GDA_DATA_MODEL_ERROR, GDA_DATA_MODEL_ROW_NOT_FOUND_ERROR,
		      "%s", _("Row not found in data model"));
	return FALSE;
}

static void
gda_data_model_array_set_notify (GdaDataModel *model, gboolean do_notify_changes)
{
	((GdaDataModelArray *) model)->priv->notify_changes = do_notify_changes;
}

static gboolean
gda_data_model_array_get_notify (GdaDataModel *model)
{
	return ((GdaDataModelArray *) model)->priv->notify_changes;
}
