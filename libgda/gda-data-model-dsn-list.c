/*
 * Copyright (C) 2008 - 2013 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2010 David King <davidk@openismus.com>
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
#define G_LOG_DOMAIN "GDA-data-model-dsn-list"

#include <stdarg.h>
#include <string.h>
#include <glib/gi18n-lib.h>
#include <libgda/gda-data-model.h>
#include <libgda/gda-quark-list.h>
#include <libgda/gda-data-model-extra.h>
#include <libgda/gda-data-model-dsn-list.h>
#include <libgda/gda-config.h>

typedef struct {
	gint    nb_dsn;
	GSList *columns;
	gint    row_to_remove;
	GValue *tmp_value;
} GdaDataModelDsnListPrivate;

static void                 gda_data_model_dsn_list_data_model_init (GdaDataModelInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GdaDataModelDsnList, gda_data_model_dsn_list,G_TYPE_OBJECT,
                         G_ADD_PRIVATE (GdaDataModelDsnList)
                         G_IMPLEMENT_INTERFACE(GDA_TYPE_DATA_MODEL,gda_data_model_dsn_list_data_model_init))

static void gda_data_model_dsn_list_dispose    (GObject *object);

/* GdaDataModel interface */
static gint                 gda_data_model_dsn_list_get_n_rows      (GdaDataModel *model);
static gint                 gda_data_model_dsn_list_get_n_columns   (GdaDataModel *model);
static GdaColumn           *gda_data_model_dsn_list_describe_column (GdaDataModel *model, gint col);
static GdaDataModelAccessFlags gda_data_model_dsn_list_get_access_flags(GdaDataModel *model);
static const GValue        *gda_data_model_dsn_list_get_value_at    (GdaDataModel *model, gint col, gint row, GError **error);
static GdaValueAttribute    gda_data_model_dsn_list_get_attributes_at (GdaDataModel *model, gint col, gint row);

static void dsn_added_cb (GdaConfig *conf, GdaDsnInfo *info, GdaDataModelDsnList *model);
static void dsn_to_be_removed_cb (GdaConfig *conf, GdaDsnInfo *info, GdaDataModelDsnList *model);
static void dsn_removed_cb (GdaConfig *conf, GdaDsnInfo *info, GdaDataModelDsnList *model);
static void dsn_changed_cb (GdaConfig *conf, GdaDsnInfo *info, GdaDataModelDsnList *model);

/*
 * Object init and finalize
 */
static void
gda_data_model_dsn_list_data_model_init (GdaDataModelInterface *iface)
{
        iface->get_n_rows = gda_data_model_dsn_list_get_n_rows;
        iface->get_n_columns = gda_data_model_dsn_list_get_n_columns;
        iface->describe_column = gda_data_model_dsn_list_describe_column;
        iface->get_access_flags = gda_data_model_dsn_list_get_access_flags;
        iface->get_value_at = gda_data_model_dsn_list_get_value_at;
        iface->get_attributes_at = gda_data_model_dsn_list_get_attributes_at;

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
gda_data_model_dsn_list_init (GdaDataModelDsnList *model)
{
	GdaConfig *config;
	GdaColumn *col;
	GdaDataModelDsnListPrivate *priv = gda_data_model_dsn_list_get_instance_private (model);

	priv->nb_dsn = gda_config_get_nb_dsn ();
	priv->row_to_remove = -1;
	
	col = gda_column_new ();
	gda_column_set_name (col, _("DSN"));
	gda_column_set_description (col, _("DSN"));
	gda_column_set_g_type (col, G_TYPE_STRING);
	priv->columns = g_slist_append (NULL, col);

	col = gda_column_new ();
	gda_column_set_name (col, _("Provider"));
	gda_column_set_description (col, _("Provider"));
	gda_column_set_g_type (col, G_TYPE_STRING);
	priv->columns = g_slist_append (priv->columns, col);

	col = gda_column_new ();
	gda_column_set_name (col, _("Description"));
	gda_column_set_description (col, _("Description"));
	gda_column_set_g_type (col, G_TYPE_STRING);
	priv->columns = g_slist_append (priv->columns, col);

	col = gda_column_new ();
	/* To translators: a "Connection string" is a semi-colon delimited list of key=value pairs which
	 * define the parameters for a connection, such as "DB_NAME=thedb;HOSTNAME=moon */
	gda_column_set_name (col, _("Connection string"));
	gda_column_set_description (col, _("Connection string"));
	gda_column_set_g_type (col, G_TYPE_STRING);
	priv->columns = g_slist_append (priv->columns, col);

	col = gda_column_new ();
	gda_column_set_name (col, _("Username"));
	gda_column_set_description (col, _("Username"));
	gda_column_set_g_type (col, G_TYPE_STRING);
	priv->columns = g_slist_append (priv->columns, col);

	col = gda_column_new ();
	gda_column_set_name (col, _("Global"));
	gda_column_set_description (col, _("Global"));
	gda_column_set_g_type (col, G_TYPE_BOOLEAN);
	priv->columns = g_slist_append (priv->columns, col);

	g_object_set_data (G_OBJECT (model), "name", _("List of defined data sources"));

	config = gda_config_get ();
	g_signal_connect (G_OBJECT (config), "dsn-added",
			  G_CALLBACK (dsn_added_cb), model);
	g_signal_connect (G_OBJECT (config), "dsn-to-be-removed",
			  G_CALLBACK (dsn_to_be_removed_cb), model);
	g_signal_connect (G_OBJECT (config), "dsn-removed",
			  G_CALLBACK (dsn_removed_cb), model);
	g_signal_connect (G_OBJECT (config), "dsn-changed",
			  G_CALLBACK (dsn_changed_cb), model);
	g_object_unref (config);

	priv->tmp_value = NULL;
}

static void
gda_data_model_dsn_list_class_init (GdaDataModelDsnListClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->dispose = gda_data_model_dsn_list_dispose;
}

static void
gda_data_model_dsn_list_dispose (GObject *object)
{
	GdaDataModelDsnList *model = (GdaDataModelDsnList *) object;

	g_return_if_fail (GDA_IS_DATA_MODEL_DSN_LIST (model));
	GdaDataModelDsnListPrivate *priv = gda_data_model_dsn_list_get_instance_private (model);

	GdaConfig *config = gda_config_get ();
	g_signal_handlers_disconnect_by_func (G_OBJECT (config),
					      G_CALLBACK (dsn_added_cb), model);
	g_signal_handlers_disconnect_by_func (G_OBJECT (config),
					      G_CALLBACK (dsn_removed_cb), model);
	g_signal_handlers_disconnect_by_func (G_OBJECT (config),
					      G_CALLBACK (dsn_to_be_removed_cb), model);
	g_signal_handlers_disconnect_by_func (G_OBJECT (config),
					      G_CALLBACK (dsn_changed_cb), model);
	g_object_unref (config);
	if (priv->tmp_value) {
		gda_value_free (priv->tmp_value);
		priv->tmp_value = NULL;
	}

	G_OBJECT_CLASS (gda_data_model_dsn_list_parent_class)->dispose (object);
}

static void 
dsn_added_cb (G_GNUC_UNUSED GdaConfig *conf, GdaDsnInfo *info, GdaDataModelDsnList *model)
{
	GdaDataModelDsnListPrivate *priv = gda_data_model_dsn_list_get_instance_private (model);
	priv->nb_dsn++;
	gda_data_model_row_inserted ((GdaDataModel *) model, gda_config_get_dsn_info_index (info->name));
}

static void
dsn_to_be_removed_cb (G_GNUC_UNUSED GdaConfig *conf, GdaDsnInfo *info, GdaDataModelDsnList *model)
{
	GdaDataModelDsnListPrivate *priv = gda_data_model_dsn_list_get_instance_private (model);
	priv->row_to_remove = gda_config_get_dsn_info_index (info->name);
}

static void
dsn_removed_cb (G_GNUC_UNUSED GdaConfig *conf, G_GNUC_UNUSED GdaDsnInfo *info, GdaDataModelDsnList *model)
{
	GdaDataModelDsnListPrivate *priv = gda_data_model_dsn_list_get_instance_private (model);
	priv->nb_dsn--;
	gda_data_model_row_removed ((GdaDataModel *) model, priv->row_to_remove);
	priv->row_to_remove = -1;
}

static void 
dsn_changed_cb (G_GNUC_UNUSED GdaConfig *conf, GdaDsnInfo *info, GdaDataModelDsnList *model)
{
	gda_data_model_row_updated ((GdaDataModel *) model, gda_config_get_dsn_info_index (info->name));
}

/*
 * GdaDataModel implementation
 */

static gint
gda_data_model_dsn_list_get_n_rows (GdaDataModel *model)
{
	GdaDataModelDsnList *dmodel = GDA_DATA_MODEL_DSN_LIST (model);
	GdaDataModelDsnListPrivate *priv = gda_data_model_dsn_list_get_instance_private (dmodel);

	return priv->nb_dsn;
}

static gint
gda_data_model_dsn_list_get_n_columns (GdaDataModel *model)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), -1);
	GdaDataModelDsnList *dmodel = GDA_DATA_MODEL_DSN_LIST (model);
	GdaDataModelDsnListPrivate *priv = gda_data_model_dsn_list_get_instance_private (dmodel);
	g_return_val_if_fail (priv->columns != NULL, -1);

	return g_slist_length (priv->columns);
}

static GdaColumn *
gda_data_model_dsn_list_describe_column (GdaDataModel *model, gint col)
{
	GdaDataModelDsnList *dmodel = GDA_DATA_MODEL_DSN_LIST (model);
	GdaDataModelDsnListPrivate *priv = gda_data_model_dsn_list_get_instance_private (dmodel);

	return g_slist_nth_data (priv->columns, col);
}

static GdaDataModelAccessFlags
gda_data_model_dsn_list_get_access_flags (G_GNUC_UNUSED GdaDataModel *model)
{
	return GDA_DATA_MODEL_ACCESS_RANDOM;
}

static const GValue *
gda_data_model_dsn_list_get_value_at (GdaDataModel *model, gint col, gint row, GError **error)
{
	GdaDataModelDsnList *dmodel = GDA_DATA_MODEL_DSN_LIST (model);
	GdaDataModelDsnListPrivate *priv = gda_data_model_dsn_list_get_instance_private (dmodel);

	if (priv->tmp_value) {
		gda_value_free (priv->tmp_value);
		priv->tmp_value = NULL;
	}
		
	if ((col >= gda_data_model_dsn_list_get_n_columns (model)) ||
	    (col < 0)) {
		g_set_error (error, GDA_DATA_MODEL_ERROR, GDA_DATA_MODEL_COLUMN_OUT_OF_RANGE_ERROR,
                             _("Column %d out of range (0-%d)"), col, gda_data_model_dsn_list_get_n_columns (model) - 1);
                return NULL;
	}
	if ((row < 0) || (row >= gda_data_model_dsn_list_get_n_rows (model))) {
		gint n;
		n = gda_data_model_dsn_list_get_n_rows (model);
		if (n > 0)
			g_set_error (error, GDA_DATA_MODEL_ERROR, GDA_DATA_MODEL_ROW_OUT_OF_RANGE_ERROR,
				     _("Row %d out of range (0-%d)"), row, n - 1);
		else
			g_set_error (error, GDA_DATA_MODEL_ERROR, GDA_DATA_MODEL_ROW_OUT_OF_RANGE_ERROR,
				     _("Row %d not found (empty data model)"), row);
		return NULL;
	}

	GdaDsnInfo *info = gda_config_get_dsn_info_at_index (row);
	g_assert (info);
	if (col != 5)
		priv->tmp_value = gda_value_new (G_TYPE_STRING);
	else
		priv->tmp_value = gda_value_new (G_TYPE_BOOLEAN);
	switch (col) {
	case 0:
		g_value_set_string (priv->tmp_value, info->name);
		break;
	case 1:
		g_value_set_string (priv->tmp_value, info->provider);
		break;
	case 2:
		g_value_set_string (priv->tmp_value, info->description);
		break;
	case 3:
		g_value_set_string (priv->tmp_value, info->cnc_string);
		break;
	case 4: 
		if (info->auth_string) {
			GdaQuarkList* ql;
			ql = gda_quark_list_new_from_string (info->auth_string);
			
			g_value_set_string (priv->tmp_value, gda_quark_list_find (ql, "USERNAME"));
			gda_quark_list_free (ql);
		}
		else
			g_value_set_string (priv->tmp_value, "");
		break;
	case 5:
		g_value_set_boolean (priv->tmp_value, info->is_system);
		break;
	default:
		g_assert_not_reached ();
	}
	
	return priv->tmp_value;
}

static GdaValueAttribute
gda_data_model_dsn_list_get_attributes_at (G_GNUC_UNUSED GdaDataModel *model, G_GNUC_UNUSED gint col, G_GNUC_UNUSED gint row)
{
	return GDA_VALUE_ATTR_NO_MODIF;
}
