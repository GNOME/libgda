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

#include <libgda/gda-intl.h>
#include <libgda/gda-data-model.h>
#include <libgda/gda-data-model-base.h>
#include <libgda/gda-column.h>
#include <libgda/gda-log.h>
#include <libgda/gda-util.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <string.h>

#define PARENT_TYPE G_TYPE_OBJECT
#define CLASS(model) (GDA_DATA_MODEL_BASE_CLASS (G_OBJECT_GET_CLASS (model)))

struct _GdaDataModelBasePrivate {
	gboolean       notify_changes;
	GHashTable    *column_spec;
	gchar         *cmd_text;
	GdaCommandType cmd_type;

	/* update mode */
	gboolean       updating;
};

static void gda_data_model_base_class_init (GdaDataModelBaseClass *klass);
static void gda_data_model_base_init       (GdaDataModelBase *model, GdaDataModelBaseClass *klass);
static void gda_data_model_base_finalize   (GObject *object);

/* GdaDataModel interface */
static void                 gda_data_model_base_data_model_init (GdaDataModelClass *iface);
static gint                 gda_data_model_base_get_n_rows      (GdaDataModel *model);
static gint                 gda_data_model_base_get_n_columns   (GdaDataModel *model);
static GdaColumn           *gda_data_model_base_describe_column (GdaDataModel *model, gint col);
static const gchar         *gda_data_model_base_get_column_title(GdaDataModel *model, gint col);
static void                 gda_data_model_base_set_column_title(GdaDataModel *model, gint col, const gchar *title);
static gint                 gda_data_model_base_get_column_pos  (GdaDataModel *model, const gchar *title);
static GdaRow              *gda_data_model_base_get_row         (GdaDataModel *model, gint row);
static const GdaValue      *gda_data_model_base_get_value_at    (GdaDataModel *model, gint col, gint row);
static gboolean             gda_data_model_base_is_updatable    (GdaDataModel *model);
static gboolean             gda_data_model_base_has_changed     (GdaDataModel *model);
static void                 gda_data_model_base_begin_changes   (GdaDataModel *model);
static gboolean             gda_data_model_base_commit_changes  (GdaDataModel *model);
static gboolean             gda_data_model_base_cancel_changes  (GdaDataModel *model);
static GdaRow              *gda_data_model_base_append_values   (GdaDataModel *model, const GList *values);
static gboolean             gda_data_model_base_append_row      (GdaDataModel *model, GdaRow *row);
static gboolean             gda_data_model_base_update_row      (GdaDataModel *model, GdaRow *row);
static gboolean             gda_data_model_base_remove_row      (GdaDataModel *model, GdaRow *row);
static GdaColumn           *gda_data_model_base_append_column   (GdaDataModel *model);
static gboolean             gda_data_model_base_remove_column   (GdaDataModel *model, gint col);
static void                 gda_data_model_base_set_notify      (GdaDataModel *model, gboolean do_notify_changes);
static gboolean             gda_data_model_base_get_notify      (GdaDataModel *model);
static gboolean             gda_data_model_base_set_command     (GdaDataModel *model, const gchar *txt, GdaCommandType type);
static const gchar         *gda_data_model_base_get_command     (GdaDataModel *model, GdaCommandType *type);

static GObjectClass *parent_class = NULL;

/*
 * GdaDataModelBase class implementation
 */

static void
gda_data_model_base_class_init (GdaDataModelBaseClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);


	object_class->finalize = gda_data_model_base_finalize;
}

static void
gda_data_model_base_data_model_init (GdaDataModelClass *iface)
{
	iface->i_get_n_rows = gda_data_model_base_get_n_rows;
	iface->i_get_n_columns = gda_data_model_base_get_n_columns;
	iface->i_describe_column = gda_data_model_base_describe_column;
        iface->i_get_row = gda_data_model_base_get_row;
	iface->i_get_value_at = gda_data_model_base_get_value_at;
	iface->i_is_updatable = gda_data_model_base_is_updatable;
	iface->i_has_changed = gda_data_model_base_has_changed;
	iface->i_begin_changes = gda_data_model_base_begin_changes;
	iface->i_cancel_changes = gda_data_model_base_cancel_changes;
	iface->i_commit_changes = gda_data_model_base_commit_changes;
        iface->i_append_values = gda_data_model_base_append_values;
	iface->i_append_row = gda_data_model_base_append_row;
	iface->i_update_row = gda_data_model_base_update_row;
	iface->i_remove_row = gda_data_model_base_remove_row;
	iface->i_append_column = gda_data_model_base_append_column;
	iface->i_remove_column = gda_data_model_base_remove_column;
	iface->i_set_notify = gda_data_model_base_set_notify;
	iface->i_get_notify = gda_data_model_base_get_notify;
	iface->i_set_command = gda_data_model_base_set_command;
	iface->i_get_command = gda_data_model_base_get_command;
}

static void
gda_data_model_base_init (GdaDataModelBase *model, GdaDataModelBaseClass *klass)
{
	g_return_if_fail (GDA_IS_DATA_MODEL (model));

	model->priv = g_new (GdaDataModelBasePrivate, 1);
	model->priv->notify_changes = TRUE;
	model->priv->column_spec = g_hash_table_new (g_direct_hash, g_direct_equal);
	model->priv->updating = FALSE;
	model->priv->cmd_text = NULL;
	model->priv->cmd_type = GDA_COMMAND_TYPE_INVALID;
}

static void column_gda_type_changed_cb (GdaColumn *column, GdaValueType old, GdaValueType new, GdaDataModelBase *model);

static void
hash_free_column (gpointer key, GdaColumn *column, GdaDataModelBase *model)
{
	g_signal_handlers_disconnect_by_func (G_OBJECT (column),
					      G_CALLBACK (column_gda_type_changed_cb), model);
	g_object_unref (column);
}

static void
gda_data_model_base_finalize (GObject *object)
{
	GdaDataModelBase *model = (GdaDataModelBase *) object;

	g_return_if_fail (GDA_IS_DATA_MODEL (model));

	/* free memory */
	g_hash_table_foreach (model->priv->column_spec, (GHFunc) hash_free_column, model);
	g_hash_table_destroy (model->priv->column_spec);
	model->priv->column_spec = NULL;

	g_free (model->priv->cmd_text);
	model->priv->cmd_text = NULL;

	g_free (model->priv);
	model->priv = NULL;

	/* chain to parent class */
	parent_class->finalize (object);
}

GType
gda_data_model_base_get_type (void)
{
	static GType type = 0;

	if (!type) {
		if (type == 0) {
			static GTypeInfo info = {
				sizeof (GdaDataModelBaseClass),
				(GBaseInitFunc) NULL,
				(GBaseFinalizeFunc) NULL,
				(GClassInitFunc) gda_data_model_base_class_init,
				NULL, NULL,
				sizeof (GdaDataModelBase),
				0,
				(GInstanceInitFunc) gda_data_model_base_init
			};
			
			static const GInterfaceInfo data_model_info = {
				(GInterfaceInitFunc) gda_data_model_base_data_model_init,
				NULL,
				NULL
			};

			type = g_type_register_static (PARENT_TYPE, "GdaDataModelBase", &info, G_TYPE_FLAG_ABSTRACT);
			g_type_add_interface_static (type, GDA_TYPE_DATA_MODEL, &data_model_info);
		}
	}

	return type;
}
static gint
gda_data_model_base_get_n_rows (GdaDataModel *model)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL_BASE (model), -1);
	g_return_val_if_fail (CLASS (model)->get_n_rows != NULL, -1);
	
	return CLASS (model)->get_n_rows (GDA_DATA_MODEL_BASE (model));
}

static gint
gda_data_model_base_get_n_columns (GdaDataModel *model)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL_BASE (model), -1);
	g_return_val_if_fail (CLASS (model)->get_n_columns != NULL, -1);
	
	return CLASS (model)->get_n_columns (GDA_DATA_MODEL_BASE (model));
}

static GdaColumn *
gda_data_model_base_describe_column (GdaDataModel *model, gint col)
{
	GdaColumn *column;

	g_return_val_if_fail (GDA_IS_DATA_MODEL_BASE (model), NULL);

	column = g_hash_table_lookup (GDA_DATA_MODEL_BASE (model)->priv->column_spec,
				      GINT_TO_POINTER (col));
	if (!column) {
		column = gda_column_new ();
		g_signal_connect (G_OBJECT (column), "gda_type_changed",
				  G_CALLBACK (column_gda_type_changed_cb), model);
		gda_column_set_position (column, col);
		g_hash_table_insert (GDA_DATA_MODEL_BASE (model)->priv->column_spec,
				     GINT_TO_POINTER (col), column);
	}

	return column;
}

static void
column_gda_type_changed_cb (GdaColumn *column, GdaValueType old, GdaValueType new, GdaDataModelBase *model)
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
	nrows = gda_data_model_base_get_n_rows (GDA_DATA_MODEL (model));
	for (i = 0; (i < nrows) && (nb_warnings < max_warnings); i++) {
		GdaValueType vtype;

		value = gda_data_model_base_get_value_at (GDA_DATA_MODEL (model), col, i);
		vtype = gda_value_get_type (value);
		if ((vtype != GDA_VALUE_TYPE_NULL) && (vtype != new)) {
			nb_warnings ++;
			if (nb_warnings >= max_warnings)
				g_warning ("Max number of warning reachedn more incompatible types...");
			else {
				str = gda_value_stringify (value);
				g_warning ("Value of type %s not compatible with new column type %s (value=%s)",
					   gda_type_to_string (gda_value_get_type (value)), 
					   gda_type_to_string (new), str);
				g_free (str);
			}
		}
			
	}
}

static GdaRow *
gda_data_model_base_get_row (GdaDataModel *model, gint row)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL_BASE (model), NULL);
	g_return_val_if_fail (CLASS (model)->get_row != NULL, NULL);

	return CLASS (model)->get_row (GDA_DATA_MODEL_BASE (model), row);
}

static const GdaValue *
gda_data_model_base_get_value_at (GdaDataModel *model, gint col, gint row)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL_BASE (model), NULL);
	g_return_val_if_fail (CLASS (model)->get_value_at != NULL, NULL);
	
	return CLASS (model)->get_value_at (GDA_DATA_MODEL_BASE (model), col, row);
}

static gboolean
gda_data_model_base_is_updatable (GdaDataModel *model)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL_BASE (model), FALSE);
	g_return_val_if_fail (CLASS (model)->is_updatable != NULL, FALSE);

	return CLASS (model)->is_updatable (GDA_DATA_MODEL_BASE (model));
}

static gboolean
gda_data_model_base_has_changed (GdaDataModel *model)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL_BASE (model), FALSE);
	return GDA_DATA_MODEL_BASE (model)->priv->updating;
}

static void
gda_data_model_base_begin_changes (GdaDataModel *model)
{
	GdaDataModelBase *mb;

	g_return_if_fail (GDA_IS_DATA_MODEL_BASE (model));
	mb = GDA_DATA_MODEL_BASE (model);
	g_return_if_fail (!mb->priv->updating);

	mb->priv->updating = TRUE;
}

static gboolean
gda_data_model_base_commit_changes (GdaDataModel *model)
{
	GdaDataModelBase *mb;

	g_return_val_if_fail (GDA_IS_DATA_MODEL_BASE (model), FALSE);
	mb = GDA_DATA_MODEL_BASE (model);
	g_return_val_if_fail (mb->priv->updating, FALSE);

	mb->priv->updating = FALSE;

	return TRUE;
}

static gboolean
gda_data_model_base_cancel_changes (GdaDataModel *model)
{
	GdaDataModelBase *mb;

	g_return_val_if_fail (GDA_IS_DATA_MODEL_BASE (model), FALSE);
	mb = GDA_DATA_MODEL_BASE (model);
	g_return_val_if_fail (mb->priv->updating, FALSE);

	mb->priv->updating = FALSE;

	return TRUE;
}



static GdaRow *
gda_data_model_base_append_values (GdaDataModel *model, const GList *values)
{
	GdaRow *row;

	g_return_val_if_fail (GDA_IS_DATA_MODEL_BASE (model), NULL);
	g_return_val_if_fail (CLASS (model)->append_values != NULL, NULL);

	row = CLASS (model)->append_values (GDA_DATA_MODEL_BASE (model), values);
	return row;
}

static gboolean
gda_data_model_base_append_row (GdaDataModel *model, GdaRow *row)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL_BASE (model), FALSE);
	g_return_val_if_fail (row != NULL, FALSE);
	g_return_val_if_fail (CLASS (model)->append_row != NULL, FALSE);

	return CLASS (model)->append_row (GDA_DATA_MODEL_BASE (model), row);
}

static gboolean
gda_data_model_base_update_row (GdaDataModel *model, GdaRow *row)
{
        g_return_val_if_fail (GDA_IS_DATA_MODEL_BASE (model), FALSE);
        g_return_val_if_fail (row != NULL, FALSE);
        g_return_val_if_fail (CLASS (model)->update_row != NULL, FALSE);

        return CLASS (model)->update_row (GDA_DATA_MODEL_BASE (model), row);
}

static gboolean
gda_data_model_base_remove_row (GdaDataModel *model, GdaRow *row)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL_BASE (model), FALSE);
	g_return_val_if_fail (row != NULL, FALSE);
	g_return_val_if_fail (CLASS (model)->remove_row != NULL, FALSE);

	return CLASS (model)->remove_row (GDA_DATA_MODEL_BASE (model), row);
}

static GdaColumn *
gda_data_model_base_append_column (GdaDataModel *model)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL_BASE (model), FALSE);
	g_return_val_if_fail (CLASS (model)->append_column != NULL, FALSE);

	return CLASS (model)->append_column (GDA_DATA_MODEL_BASE (model));
}

static gboolean
gda_data_model_base_remove_column (GdaDataModel *model, gint col)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL_BASE (model), FALSE);
	g_return_val_if_fail (CLASS (model)->remove_column != NULL, FALSE);

	return CLASS (model)->remove_column (GDA_DATA_MODEL_BASE (model), col);
}


static void
gda_data_model_base_set_notify (GdaDataModel *model, gboolean do_notify_changes)
{
	g_return_if_fail (GDA_IS_DATA_MODEL_BASE (model));
	GDA_DATA_MODEL_BASE (model)->priv->notify_changes = do_notify_changes;
}

static gboolean
gda_data_model_base_get_notify (GdaDataModel *model)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL_BASE (model), FALSE);
	return GDA_DATA_MODEL_BASE (model)->priv->notify_changes;
}

static gboolean
gda_data_model_base_set_command (GdaDataModel *model, const gchar *txt, GdaCommandType type)
{
	GdaDataModelBase *mb;

	g_return_val_if_fail (GDA_IS_DATA_MODEL_BASE (model), FALSE);
	mb = GDA_DATA_MODEL_BASE (model);	

	if (txt) {
		if (mb->priv->cmd_text)
			g_free (mb->priv->cmd_text);
		mb->priv->cmd_text = g_strdup (txt);
	}
	mb->priv->cmd_type = type;

	return TRUE;
}

static const gchar *
gda_data_model_base_get_command (GdaDataModel *model, GdaCommandType *type)

{	g_return_val_if_fail (GDA_IS_DATA_MODEL_BASE (model), NULL);
	if (type)
		*type = GDA_DATA_MODEL_BASE (model)->priv->cmd_type;
	return GDA_DATA_MODEL_BASE (model)->priv->cmd_text;
}


