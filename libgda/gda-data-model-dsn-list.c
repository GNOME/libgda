/* GDA SQLite provider
 * Copyright (C) 2007 The GNOME Foundation.
 *
 * AUTHORS:
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

#include <stdarg.h>
#include <string.h>
#include <glib/gi18n-lib.h>
#include <libgda/gda-data-model.h>
#include <libgda/gda-data-model-extra.h>
#include <libgda/gda-data-model-dsn-list.h>
#include <libgda/gda-config.h>

struct _GdaDataModelDsnListPrivate {
	gint nb_dsn;
	GSList *columns;
	gint row_to_remove;
};

static void gda_data_model_dsn_list_class_init (GdaDataModelDsnListClass *klass);
static void gda_data_model_dsn_list_init       (GdaDataModelDsnList *model,
						GdaDataModelDsnListClass *klass);
static void gda_data_model_dsn_list_dispose    (GObject *object);

/* GdaDataModel interface */
static void                 gda_data_model_dsn_list_data_model_init (GdaDataModelClass *iface);
static gint                 gda_data_model_dsn_list_get_n_rows      (GdaDataModel *model);
static gint                 gda_data_model_dsn_list_get_n_columns   (GdaDataModel *model);
static GdaColumn           *gda_data_model_dsn_list_describe_column (GdaDataModel *model, gint col);
static GdaDataModelAccessFlags gda_data_model_dsn_list_get_access_flags(GdaDataModel *model);
static const GValue      *gda_data_model_dsn_list_get_value_at    (GdaDataModel *model, gint col, gint row);
static GdaValueAttribute    gda_data_model_dsn_list_get_attributes_at (GdaDataModel *model, gint col, gint row);

static GObjectClass *parent_class = NULL;

static void dsn_added_cb (GdaConfig *conf, GdaDataSourceInfo *info, GdaDataModelDsnList *model);
static void dsn_to_be_removed_cb (GdaConfig *conf, GdaDataSourceInfo *info, GdaDataModelDsnList *model);
static void dsn_removed_cb (GdaConfig *conf, GdaDataSourceInfo *info, GdaDataModelDsnList *model);
static void dsn_changed_cb (GdaConfig *conf, GdaDataSourceInfo *info, GdaDataModelDsnList *model);

/*
 * Object init and finalize
 */
static void
gda_data_model_dsn_list_data_model_init (GdaDataModelClass *iface)
{
        iface->i_get_n_rows = gda_data_model_dsn_list_get_n_rows;
        iface->i_get_n_columns = gda_data_model_dsn_list_get_n_columns;
        iface->i_describe_column = gda_data_model_dsn_list_describe_column;
        iface->i_get_access_flags = gda_data_model_dsn_list_get_access_flags;
        iface->i_get_value_at = gda_data_model_dsn_list_get_value_at;
        iface->i_get_attributes_at = gda_data_model_dsn_list_get_attributes_at;

        iface->i_create_iter = NULL;
        iface->i_iter_at_row = NULL;
        iface->i_iter_next = NULL;
        iface->i_iter_prev = NULL;

        iface->i_set_value_at = NULL;
        iface->i_set_values = NULL;
        iface->i_append_values = NULL;
        iface->i_append_row = NULL;
        iface->i_remove_row = NULL;
        iface->i_find_row = NULL;

        iface->i_set_notify = NULL;
        iface->i_get_notify = NULL;
        iface->i_send_hint = NULL;
}

static void
gda_data_model_dsn_list_init (GdaDataModelDsnList *model,
			      GdaDataModelDsnListClass *klass)
{
	GdaConfig *config;
	GdaColumn *col;

	g_return_if_fail (GDA_IS_DATA_MODEL_DSN_LIST (model));

	model->priv = g_new0 (GdaDataModelDsnListPrivate, 1);
	model->priv->nb_dsn = gda_config_get_nb_dsn ();
	model->priv->row_to_remove = -1;
	
	col = gda_column_new ();
	gda_column_set_name (col, _("DSN"));
	gda_column_set_title (col, _("DSN"));
	gda_column_set_g_type (col, G_TYPE_STRING);
	model->priv->columns = g_slist_append (NULL, col);

	col = gda_column_new ();
	gda_column_set_name (col, _("Provider"));
	gda_column_set_title (col, _("Provider"));
	gda_column_set_g_type (col, G_TYPE_STRING);
	model->priv->columns = g_slist_append (model->priv->columns, col);

	col = gda_column_new ();
	gda_column_set_name (col, _("Description"));
	gda_column_set_title (col, _("Description"));
	gda_column_set_g_type (col, G_TYPE_STRING);
	model->priv->columns = g_slist_append (model->priv->columns, col);

	col = gda_column_new ();
	gda_column_set_name (col, _("Connection string"));
	gda_column_set_title (col, _("Connection string"));
	gda_column_set_g_type (col, G_TYPE_STRING);
	model->priv->columns = g_slist_append (model->priv->columns, col);

	col = gda_column_new ();
	gda_column_set_name (col, _("Username"));
	gda_column_set_title (col, _("Username"));
	gda_column_set_g_type (col, G_TYPE_STRING);
	model->priv->columns = g_slist_append (model->priv->columns, col);

	col = gda_column_new ();
	gda_column_set_name (col, _("Global"));
	gda_column_set_title (col, _("Global"));
	gda_column_set_g_type (col, G_TYPE_BOOLEAN);
	model->priv->columns = g_slist_append (model->priv->columns, col);

	gda_object_set_name (GDA_OBJECT (model), _("List of defined data sources"));

	config = gda_config_get ();
	g_signal_connect (G_OBJECT (config), "dsn_added",
			  G_CALLBACK (dsn_added_cb), model);
	g_signal_connect (G_OBJECT (config), "dsn_to_be_removed",
			  G_CALLBACK (dsn_to_be_removed_cb), model);
	g_signal_connect (G_OBJECT (config), "dsn_removed",
			  G_CALLBACK (dsn_removed_cb), model);
	g_signal_connect (G_OBJECT (config), "dsn_changed",
			  G_CALLBACK (dsn_changed_cb), model);
}

static void
gda_data_model_dsn_list_class_init (GdaDataModelDsnListClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->dispose = gda_data_model_dsn_list_dispose;
}

static void
gda_data_model_dsn_list_dispose (GObject *object)
{
	GdaDataModelDsnList *model = (GdaDataModelDsnList *) object;

	g_return_if_fail (GDA_IS_DATA_MODEL_DSN_LIST (model));

	if (model->priv) {
		GdaConfig *config = gda_config_get ();
		g_signal_handlers_disconnect_by_func (G_OBJECT (config),
						      G_CALLBACK (dsn_added_cb), model);
		g_signal_handlers_disconnect_by_func (G_OBJECT (config),
						      G_CALLBACK (dsn_removed_cb), model);
		g_signal_handlers_disconnect_by_func (G_OBJECT (config),
						      G_CALLBACK (dsn_to_be_removed_cb), model);
		g_signal_handlers_disconnect_by_func (G_OBJECT (config),
						      G_CALLBACK (dsn_changed_cb), model);
		g_free (model->priv);
		model->priv = NULL;
	}
	
	parent_class->dispose (object);
}

static void 
dsn_added_cb (GdaConfig *conf, GdaDataSourceInfo *info, GdaDataModelDsnList *model)
{
	model->priv->nb_dsn++;
	gda_data_model_row_inserted ((GdaDataModel *) model, gda_config_get_dsn_index (info->name));
}

static void
dsn_to_be_removed_cb (GdaConfig *conf, GdaDataSourceInfo *info, GdaDataModelDsnList *model)
{
	model->priv->row_to_remove = gda_config_get_dsn_index (info->name);
}

static void
dsn_removed_cb (GdaConfig *conf, GdaDataSourceInfo *info, GdaDataModelDsnList *model)
{
	model->priv->nb_dsn--;
	gda_data_model_row_removed ((GdaDataModel *) model, model->priv->row_to_remove);
	model->priv->row_to_remove = -1;
}

static void 
dsn_changed_cb (GdaConfig *conf, GdaDataSourceInfo *info, GdaDataModelDsnList *model)
{
	gda_data_model_row_updated ((GdaDataModel *) model, gda_config_get_dsn_index (info->name));
}


/*
 * Public functions
 */

GType
gda_data_model_dsn_list_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo info = {
			sizeof (GdaDataModelDsnListClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_data_model_dsn_list_class_init,
			NULL,
			NULL,
			sizeof (GdaDataModelDsnList),
			0,
			(GInstanceInitFunc) gda_data_model_dsn_list_init
		};
		static const GInterfaceInfo data_model_info = {
                        (GInterfaceInitFunc) gda_data_model_dsn_list_data_model_init,
                        NULL,
                        NULL
                };

		type = g_type_register_static (GDA_TYPE_OBJECT, "GdaDataModelDsnList", &info, 0);
		g_type_add_interface_static (type, GDA_TYPE_DATA_MODEL, &data_model_info);
	}

	return type;
}

/*
 * GdaDataModel implementation
 */

static gint
gda_data_model_dsn_list_get_n_rows (GdaDataModel *model)
{
	GdaDataModelDsnList *dmodel = GDA_DATA_MODEL_DSN_LIST (model);

	return dmodel->priv->nb_dsn;
}

static gint
gda_data_model_dsn_list_get_n_columns (GdaDataModel *model)
{
	GdaDataModelDsnList *dmodel = GDA_DATA_MODEL_DSN_LIST (model);

	return g_slist_length (dmodel->priv->columns);
}

static GdaColumn *
gda_data_model_dsn_list_describe_column (GdaDataModel *model, gint col)
{
	GdaDataModelDsnList *dmodel = GDA_DATA_MODEL_DSN_LIST (model);

	return g_slist_nth_data (dmodel->priv->columns, col);
}

static GdaDataModelAccessFlags
gda_data_model_dsn_list_get_access_flags (GdaDataModel *model)
{
	return GDA_DATA_MODEL_ACCESS_RANDOM;
}

static const GValue *
gda_data_model_dsn_list_get_value_at (GdaDataModel *model, gint col, gint row)
{
	static GValue *val = NULL;

	if (val) {
		gda_value_free (val);
		val = NULL;
	}
		
	if ((col < gda_data_model_dsn_list_get_n_columns (model)) && 
	    (col >= 0) && (row >=0) && (row < gda_data_model_dsn_list_get_n_rows (model))) {
		GdaDataSourceInfo *info = gda_config_get_dsn_at_index (row);
		g_assert (info);
		if (col != 5)
			val = gda_value_new (G_TYPE_STRING);
		else
			val = gda_value_new (G_TYPE_BOOLEAN);
		switch (col) {
		case 0:
			g_value_set_string (val, info->name);
			break;
		case 1:
			g_value_set_string (val, info->provider);
			break;
		case 2:
			g_value_set_string (val, info->description);
			break;
		case 3:
			g_value_set_string (val, info->cnc_string);
			break;
		case 4:
			g_value_set_string (val, info->username);
			break;
		case 5:
			g_value_set_boolean (val, info->is_system);
			break;
		default:
			g_assert_not_reached ();
		}
	}
	
	return val;
}

static GdaValueAttribute
gda_data_model_dsn_list_get_attributes_at (GdaDataModel *model, gint col, gint row)
{
	return GDA_VALUE_ATTR_NO_MODIF;
}
