/* 
 * GDA common library
 * Copyright (C) 1998 - 2005 The GNOME Foundation.
 *
 * AUTHORS:
 *	Rodrigo Moya <rodrigo@gnome-db.org>
 *	Gonzalo Paniagua Javier <gonzalo@gnome-db.org>
 *      Vivien Malerba <malerba@gnome-db.org>
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
#include <libgda/gda-data-model.h>
#include <libgda/gda-data-model-row.h>
#include <libgda/gda-data-model-iter.h>
#include <libgda/gda-column.h>
#include <libgda/gda-log.h>
#include <libgda/gda-util.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <string.h>

#define PARENT_TYPE GDA_TYPE_OBJECT
#define CLASS(model) (GDA_DATA_MODEL_ROW_CLASS (G_OBJECT_GET_CLASS (model)))

struct _GdaDataModelRowPrivate {
	gboolean       notify_changes;
	GHashTable    *column_spec;

	/* to be removed */
	gchar         *command_text;
	GdaCommandType command_type;
};

/* properties */
enum
{
        PROP_0,
        PROP_COMMAND_TEXT,
        PROP_COMMAND_TYPE,
};

static void gda_data_model_row_class_init (GdaDataModelRowClass *klass);
static void gda_data_model_row_init       (GdaDataModelRow *model, GdaDataModelRowClass *klass);
static void gda_data_model_row_finalize   (GObject *object);
static void gda_data_model_row_set_property (GObject *object,
					     guint param_id,
					     const GValue *value,
					     GParamSpec *pspec);
static void gda_data_model_row_get_property (GObject *object,
					     guint param_id,
					     GValue *value,
					     GParamSpec *pspec);

/* GdaDataModel interface */
static void                 gda_data_model_row_data_model_init (GdaDataModelClass *iface);
static gint                 gda_data_model_row_get_n_rows      (GdaDataModel *model);
static gint                 gda_data_model_row_get_n_columns   (GdaDataModel *model);
static GdaColumn           *gda_data_model_row_describe_column (GdaDataModel *model, gint col);
static const GdaValue      *gda_data_model_row_get_value_at    (GdaDataModel *model, gint col, gint row);
static guint                gda_data_model_row_get_access_flags(GdaDataModel *model);

static gboolean             gda_data_model_row_set_value_at    (GdaDataModel *model, gint col, gint row, 
								 const GdaValue *value, GError **error);
static gboolean             gda_data_model_row_set_values      (GdaDataModel *model, gint row, 
								 GList *values, GError **error);
static gint                 gda_data_model_row_append_values   (GdaDataModel *model, const GList *values, GError **error);
static gint                 gda_data_model_row_append_row      (GdaDataModel *model, GError **error);
static gboolean             gda_data_model_row_remove_row      (GdaDataModel *model, gint row, GError **error);

static void                 gda_data_model_row_set_notify      (GdaDataModel *model, gboolean do_notify_changes);
static gboolean             gda_data_model_row_get_notify      (GdaDataModel *model);

static GObjectClass *parent_class = NULL;

/*
 * GdaDataModelRow class implementation
 */

static void
gda_data_model_row_class_init (GdaDataModelRowClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = gda_data_model_row_finalize;

	/* class attributes */
	GDA_OBJECT_CLASS (klass)->id_unique_enforced = FALSE;

	/* Properties */
        object_class->set_property = gda_data_model_row_set_property;
        object_class->get_property = gda_data_model_row_get_property;
        g_object_class_install_property (object_class, PROP_COMMAND_TEXT,
                                         g_param_spec_string ("command_text", NULL, NULL,
							      NULL, G_PARAM_READABLE | G_PARAM_WRITABLE));
        g_object_class_install_property (object_class, PROP_COMMAND_TYPE,
                                         g_param_spec_int ("command_type", NULL, NULL,
                                                            GDA_COMMAND_TYPE_SQL, GDA_COMMAND_TYPE_INVALID, 
							   GDA_COMMAND_TYPE_INVALID, 
							   G_PARAM_READABLE | G_PARAM_WRITABLE));
}

static void
gda_data_model_row_data_model_init (GdaDataModelClass *iface)
{
	iface->i_get_n_rows = gda_data_model_row_get_n_rows;
	iface->i_get_n_columns = gda_data_model_row_get_n_columns;
	iface->i_describe_column = gda_data_model_row_describe_column;
        iface->i_get_access_flags = gda_data_model_row_get_access_flags;
	iface->i_get_value_at = gda_data_model_row_get_value_at;

	iface->i_create_iter = NULL;
	iface->i_iter_at_row = NULL;
	iface->i_iter_next = NULL;
	iface->i_iter_prev = NULL;

	iface->i_set_value_at = gda_data_model_row_set_value_at;
	iface->i_set_values = gda_data_model_row_set_values;
        iface->i_append_values = gda_data_model_row_append_values;
	iface->i_append_row = gda_data_model_row_append_row;
	iface->i_remove_row = gda_data_model_row_remove_row;
	iface->i_find_row = NULL;

	iface->i_set_notify = gda_data_model_row_set_notify;
	iface->i_get_notify = gda_data_model_row_get_notify;
	iface->i_send_hint = NULL;
}

static void
gda_data_model_row_init (GdaDataModelRow *model, GdaDataModelRowClass *klass)
{
	g_return_if_fail (GDA_IS_DATA_MODEL (model));

	model->priv = g_new (GdaDataModelRowPrivate, 1);
	model->priv->notify_changes = TRUE;
	model->priv->column_spec = g_hash_table_new (g_direct_hash, g_direct_equal);
	model->priv->command_text = NULL;
	model->priv->command_type = GDA_COMMAND_TYPE_INVALID;
}

static void column_gda_type_changed_cb (GdaColumn *column, GdaValueType old, GdaValueType new, GdaDataModelRow *model);

static void
hash_free_column (gpointer key, GdaColumn *column, GdaDataModelRow *model)
{
	g_signal_handlers_disconnect_by_func (G_OBJECT (column),
					      G_CALLBACK (column_gda_type_changed_cb), model);
	g_object_unref (column);
}

static void
gda_data_model_row_finalize (GObject *object)
{
	GdaDataModelRow *model = (GdaDataModelRow *) object;

	g_return_if_fail (GDA_IS_DATA_MODEL (model));

	/* free memory */
	g_hash_table_foreach (model->priv->column_spec, (GHFunc) hash_free_column, model);
	g_hash_table_destroy (model->priv->column_spec);
	model->priv->column_spec = NULL;

	g_free (model->priv->command_text);
	model->priv->command_text = NULL;

	g_free (model->priv);
	model->priv = NULL;

	/* chain to parent class */
	parent_class->finalize (object);
}

GType
gda_data_model_row_get_type (void)
{
	static GType type = 0;

	if (!type) {
		if (type == 0) {
			static GTypeInfo info = {
				sizeof (GdaDataModelRowClass),
				(GBaseInitFunc) NULL,
				(GBaseFinalizeFunc) NULL,
				(GClassInitFunc) gda_data_model_row_class_init,
				NULL, NULL,
				sizeof (GdaDataModelRow),
				0,
				(GInstanceInitFunc) gda_data_model_row_init
			};
			
			static const GInterfaceInfo data_model_info = {
				(GInterfaceInitFunc) gda_data_model_row_data_model_init,
				NULL,
				NULL
			};

			type = g_type_register_static (PARENT_TYPE, "GdaDataModelRow", &info, G_TYPE_FLAG_ABSTRACT);
			g_type_add_interface_static (type, GDA_TYPE_DATA_MODEL, &data_model_info);
		}
	}

	return type;
}

static void
gda_data_model_row_set_property (GObject *object,
				 guint param_id,
				 const GValue *value,
				 GParamSpec *pspec)
{
        GdaDataModelRow *row;

        row = GDA_DATA_MODEL_ROW (object);
        if (row->priv) {
                switch (param_id) {
                case PROP_COMMAND_TEXT:
			if (row->priv->command_text) {
				g_free (row->priv->command_text);
				row->priv->command_text = NULL;
			}
			
                        row->priv->command_text = g_strdup (g_value_get_string (value));
                        break;
                case PROP_COMMAND_TYPE:
			row->priv->command_type = g_value_get_int (value);
			break;
                }
        }
}

static void
gda_data_model_row_get_property (GObject *object,
				 guint param_id,
				 GValue *value,
				 GParamSpec *pspec)
{
        GdaDataModelRow *row;

        row = GDA_DATA_MODEL_ROW (object);
        if (row->priv) {
                switch (param_id) {
                case PROP_COMMAND_TEXT:
			g_value_set_string (value, row->priv->command_text);
                        break;
                case PROP_COMMAND_TYPE:
			g_value_set_int (value, row->priv->command_type);
                        break;
                }
        }
}

static gint
gda_data_model_row_get_n_rows (GdaDataModel *model)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL_ROW (model), -1);
	g_return_val_if_fail (CLASS (model)->get_n_rows != NULL, -1);
	
	return CLASS (model)->get_n_rows (GDA_DATA_MODEL_ROW (model));
}

static gint
gda_data_model_row_get_n_columns (GdaDataModel *model)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL_ROW (model), -1);
	g_return_val_if_fail (CLASS (model)->get_n_columns != NULL, -1);
	
	return CLASS (model)->get_n_columns (GDA_DATA_MODEL_ROW (model));
}

static GdaColumn *
gda_data_model_row_describe_column (GdaDataModel *model, gint col)
{
	GdaColumn *column;

	g_return_val_if_fail (GDA_IS_DATA_MODEL_ROW (model), NULL);

	if (col >= gda_data_model_get_n_columns (model)) {
		g_warning ("Column %d out of range 0 - %d", col, gda_data_model_get_n_columns (model) - 1);
		return NULL;
	}

	column = g_hash_table_lookup (GDA_DATA_MODEL_ROW (model)->priv->column_spec,
				      GINT_TO_POINTER (col));
	if (!column) {
		column = gda_column_new ();
		g_signal_connect (G_OBJECT (column), "gda_type_changed",
				  G_CALLBACK (column_gda_type_changed_cb), model);
		gda_column_set_position (column, col);
		g_hash_table_insert (GDA_DATA_MODEL_ROW (model)->priv->column_spec,
				     GINT_TO_POINTER (col), column);
	}

	return column;
}

static void
column_gda_type_changed_cb (GdaColumn *column, GdaValueType old, GdaValueType new, GdaDataModelRow *model)
{
	/* emit a warning if there are GdaValues which are not compatible with the new type */
	gint i, nrows, col;
	const GdaValue *value;
	gchar *str;
	gint nb_warnings = 0;
#define max_warnings 5

	if ((new == GDA_VALUE_TYPE_NULL) ||
	    (new == GDA_VALUE_TYPE_UNKNOWN))
		return;

	col = gda_column_get_position (column);
	nrows = gda_data_model_row_get_n_rows (GDA_DATA_MODEL (model));
	for (i = 0; (i < nrows) && (nb_warnings < max_warnings); i++) {
		GdaValueType vtype;

		value = gda_data_model_row_get_value_at (GDA_DATA_MODEL (model), col, i);
		vtype = gda_value_get_type ((GdaValue *) value);
		if ((vtype != GDA_VALUE_TYPE_NULL) && (vtype != new)) {
			nb_warnings ++;
			if (nb_warnings < max_warnings) {
				if (nb_warnings == max_warnings)
					g_warning ("Max number of warning reached, "
						   "more incompatible types...");
				else {
					str = gda_value_stringify ((GdaValue *) value);
					g_warning ("Value of type %s not compatible with new"
						   " column type %s (value=%s)",
						   gda_type_to_string (gda_value_get_type ((GdaValue *) value)), 
						   gda_type_to_string (new), str);
					g_free (str);
				}
			}
		}
			
	}
}

/**
 * gda_data_model_row_get_row
 * @model: a #GdaDataModelRow object
 * @row: the row number to fetch
 *
 * Finds the #GdaRow object corresponding to the @row row number.
 *
 * Returns: the #GdaRow, or %NULL if not found.
 */
GdaRow *
gda_data_model_row_get_row (GdaDataModelRow *model, gint row, GError **error)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL_ROW (model), NULL);
	g_return_val_if_fail (CLASS (model)->get_value_at != NULL, NULL);
	
	return CLASS (model)->get_row (model, row, error);
}

/*
 * GdaDataModel interface implementation
 */

static const GdaValue *
gda_data_model_row_get_value_at (GdaDataModel *model, gint col, gint row)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL_ROW (model), NULL);
	g_return_val_if_fail (CLASS (model)->get_value_at != NULL, NULL);
	
	return CLASS (model)->get_value_at (GDA_DATA_MODEL_ROW (model), col, row);
}

static guint
gda_data_model_row_get_access_flags (GdaDataModel *model)
{
	guint flags = GDA_DATA_MODEL_ACCESS_RANDOM | 
		GDA_DATA_MODEL_ACCESS_CURSOR_FORWARD |
		GDA_DATA_MODEL_ACCESS_CURSOR_BACKWARD;
	g_return_val_if_fail (GDA_IS_DATA_MODEL_ROW (model), FALSE);
	g_return_val_if_fail (CLASS (model)->is_updatable != NULL, FALSE);

	if ( CLASS (model)->is_updatable (GDA_DATA_MODEL_ROW (model)))
		flags |= GDA_DATA_MODEL_ACCESS_WRITE;
	
	return flags;
}

static gboolean
gda_data_model_row_set_value_at (GdaDataModel *model, gint col, gint row, 
				 const GdaValue *value, GError **error)
{
	GdaRow *gdarow;

	g_return_val_if_fail (GDA_IS_DATA_MODEL_ROW (model), FALSE);
	g_return_val_if_fail (row >= 0, FALSE);
	g_return_val_if_fail (CLASS (model)->update_row != NULL, FALSE);
	g_return_val_if_fail (CLASS (model)->get_row != NULL, FALSE);

	gdarow = CLASS (model)->get_row (GDA_DATA_MODEL_ROW (model), row, error);
	if (gdarow) {
		gda_row_set_value (gdarow, col, value);
		return CLASS (model)->update_row (GDA_DATA_MODEL_ROW (model), gdarow, error);
	}
	else
		return FALSE;
}

static gboolean
gda_data_model_row_set_values (GdaDataModel *model, gint row, GList *values, GError **error)
{
	GdaRow *gdarow;

	g_return_val_if_fail (GDA_IS_DATA_MODEL_ROW (model), FALSE);
	g_return_val_if_fail (row >= 0, FALSE);
	g_return_val_if_fail (CLASS (model)->update_row != NULL, FALSE);
	g_return_val_if_fail (CLASS (model)->get_row != NULL, FALSE);
	if (!values)
		return TRUE;
	if (g_list_length (values) > gda_data_model_get_n_columns (model)) {
		g_set_error (error, 0, GDA_DATA_MODEL_VALUES_LIST_ERROR,
			     _("Too many values in list"));
		return FALSE;
	}

	gdarow = CLASS (model)->get_row (GDA_DATA_MODEL_ROW (model), row, error);
	if (gdarow) {
		GList *list;
		gint col = 0;
		list = values;
		while (list) {
			gda_row_set_value (gdarow, col, (GdaValue*)(list->data));
			list = g_list_next (list);
			col++;
		}
		return CLASS (model)->update_row (GDA_DATA_MODEL_ROW (model), gdarow, error);
	}

	return FALSE;
}


static gint
gda_data_model_row_append_values (GdaDataModel *model, const GList *values, GError **error)
{
	GdaRow *row;

	g_return_val_if_fail (GDA_IS_DATA_MODEL_ROW (model), -1);
	g_return_val_if_fail (CLASS (model)->append_values != NULL, -1);
	
	row = CLASS (model)->append_values (GDA_DATA_MODEL_ROW (model), values, error);
	if (row)
		return gda_row_get_number (row);
	else
		return -1;
}

static gint
gda_data_model_row_append_row (GdaDataModel *model, GError **error)
{
	GdaRow *row;
	gint retval = -1;

	g_return_val_if_fail (GDA_IS_DATA_MODEL_ROW (model), -1);
	g_return_val_if_fail (row != NULL, -1);
	g_return_val_if_fail (CLASS (model)->append_row != NULL, -1);

	row = gda_row_new (model, gda_data_model_get_n_columns (model));
	if (CLASS (model)->append_row (GDA_DATA_MODEL_ROW (model), row, error))
		retval = gda_row_get_number (row);
	g_object_unref (row);

	return retval;
}

static gboolean
gda_data_model_row_remove_row (GdaDataModel *model, gint row, GError **error)
{
	GdaRow *gdarow;

	g_return_val_if_fail (GDA_IS_DATA_MODEL_ROW (model), FALSE);
	g_return_val_if_fail (row >= 0, FALSE);
	g_return_val_if_fail (CLASS (model)->remove_row != NULL, FALSE);
	g_return_val_if_fail (CLASS (model)->get_row != NULL, FALSE);

	gdarow = CLASS (model)->get_row (GDA_DATA_MODEL_ROW (model), row, error);
	if (gdarow)
		return CLASS (model)->remove_row (GDA_DATA_MODEL_ROW (model), gdarow, error);
	else
		return FALSE;
}

static void
gda_data_model_row_set_notify (GdaDataModel *model, gboolean do_notify_changes)
{
	g_return_if_fail (GDA_IS_DATA_MODEL_ROW (model));
	GDA_DATA_MODEL_ROW (model)->priv->notify_changes = do_notify_changes;
}

static gboolean
gda_data_model_row_get_notify (GdaDataModel *model)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL_ROW (model), FALSE);
	return GDA_DATA_MODEL_ROW (model)->priv->notify_changes;
}
