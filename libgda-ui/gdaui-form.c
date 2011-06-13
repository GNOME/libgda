/*
 * Copyright (C) 2009 - 2010 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2010 David King <davidk@openismus.com>
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
#include <glib/gi18n-lib.h>
#include <libgda/libgda.h>
#include "gdaui-form.h"
#include "gdaui-data-proxy.h"
#include "gdaui-data-selector.h"
#include "gdaui-raw-form.h"
#include "gdaui-data-proxy-info.h"
#include "gdaui-enum-types.h"

static void gdaui_form_class_init (GdauiFormClass * class);
static void gdaui_form_init (GdauiForm *wid);

static void gdaui_form_set_property (GObject *object,
				     guint param_id,
				     const GValue *value,
				     GParamSpec *pspec);
static void gdaui_form_get_property (GObject *object,
				     guint param_id,
				     GValue *value,
				     GParamSpec *pspec);

/* GdauiDataProxy interface */
static void            gdaui_form_widget_init         (GdauiDataProxyIface *iface);
static GdaDataProxy   *gdaui_form_get_proxy           (GdauiDataProxy *iface);
static void            gdaui_form_set_column_editable (GdauiDataProxy *iface, gint column, gboolean editable);
static void            gdaui_form_show_column_actions (GdauiDataProxy *iface, gint column, gboolean show_actions);
static GtkActionGroup *gdaui_form_get_actions_group   (GdauiDataProxy *iface);
static gboolean        gdaui_form_widget_set_write_mode (GdauiDataProxy *iface, GdauiDataProxyWriteMode mode);
static GdauiDataProxyWriteMode gdaui_form_widget_get_write_mode (GdauiDataProxy *iface);

/* GdauiDataSelector interface */
static void              gdaui_form_selector_init (GdauiDataSelectorIface *iface);
static GdaDataModel     *gdaui_form_selector_get_model (GdauiDataSelector *iface);
static void              gdaui_form_selector_set_model (GdauiDataSelector *iface, GdaDataModel *model);
static GArray           *gdaui_form_selector_get_selected_rows (GdauiDataSelector *iface);
static GdaDataModelIter *gdaui_form_selector_get_data_set (GdauiDataSelector *iface);
static gboolean          gdaui_form_selector_select_row (GdauiDataSelector *iface, gint row);
static void              gdaui_form_selector_unselect_row (GdauiDataSelector *iface, gint row);
static void              gdaui_form_selector_set_column_visible (GdauiDataSelector *iface, gint column, gboolean visible);

struct _GdauiFormPriv
{
	GtkWidget *raw_form;
	GtkWidget *info;
};

/* get a pointer to the parents to be able to call their destructor */
static GObjectClass *parent_class = NULL;

/* properties */
enum {
	PROP_0,
	PROP_RAW_FORM,
	PROP_INFO,
	PROP_MODEL,
	PROP_INFO_FLAGS
};

GType
gdaui_form_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo info = {
			sizeof (GdauiFormClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gdaui_form_class_init,
			NULL,
			NULL,
			sizeof (GdauiForm),
			0,
			(GInstanceInitFunc) gdaui_form_init,
			0
		};

		static const GInterfaceInfo proxy_info = {
                        (GInterfaceInitFunc) gdaui_form_widget_init,
                        NULL,
                        NULL
                };

		static const GInterfaceInfo selector_info = {
                        (GInterfaceInitFunc) gdaui_form_selector_init,
                        NULL,
                        NULL
                };

		type = g_type_register_static (GTK_TYPE_VBOX, "GdauiForm", &info, 0);
		g_type_add_interface_static (type, GDAUI_TYPE_DATA_PROXY, &proxy_info);
		g_type_add_interface_static (type, GDAUI_TYPE_DATA_SELECTOR, &selector_info);
	}

	return type;
}

static void
gdaui_form_widget_init (GdauiDataProxyIface *iface)
{
	iface->get_proxy = gdaui_form_get_proxy;
	iface->set_column_editable = gdaui_form_set_column_editable;
	iface->show_column_actions = gdaui_form_show_column_actions;
	iface->get_actions_group = gdaui_form_get_actions_group;
	iface->set_write_mode = gdaui_form_widget_set_write_mode;
	iface->get_write_mode = gdaui_form_widget_get_write_mode;
}

static void
gdaui_form_selector_init (GdauiDataSelectorIface *iface)
{
	iface->get_model = gdaui_form_selector_get_model;
	iface->set_model = gdaui_form_selector_set_model;
	iface->get_selected_rows = gdaui_form_selector_get_selected_rows;
	iface->get_data_set = gdaui_form_selector_get_data_set;
	iface->select_row = gdaui_form_selector_select_row;
	iface->unselect_row = gdaui_form_selector_unselect_row;
	iface->set_column_visible = gdaui_form_selector_set_column_visible;
}

static void
gdaui_form_class_init (GdauiFormClass *class)
{
	GObjectClass   *object_class = G_OBJECT_CLASS (class);

	parent_class = g_type_class_peek_parent (class);

	/* Properties */

        object_class->set_property = gdaui_form_set_property;
        object_class->get_property = gdaui_form_get_property;
	g_object_class_install_property (object_class, PROP_RAW_FORM,
                                         g_param_spec_object ("raw-form", NULL, NULL, GDAUI_TYPE_RAW_FORM,
							      G_PARAM_READABLE));
	g_object_class_install_property (object_class, PROP_INFO,
                                         g_param_spec_object ("info", NULL, NULL, GDAUI_TYPE_DATA_PROXY_INFO,
							      G_PARAM_READABLE));

	g_object_class_install_property (object_class, PROP_INFO_FLAGS,
                                         g_param_spec_flags ("info-flags", NULL, NULL,
							     GDAUI_TYPE_DATA_PROXY_INFO_FLAG,
							     GDAUI_DATA_PROXY_INFO_CURRENT_ROW,
							     G_PARAM_READABLE | G_PARAM_WRITABLE));

	g_object_class_install_property (object_class, PROP_MODEL,
					 g_param_spec_object ("model", NULL, NULL,
							      GDA_TYPE_DATA_MODEL,
							      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
}

static void
form_layout_changed_cb (G_GNUC_UNUSED GdauiBasicForm *raw_form, GdauiForm *form)
{
	gboolean expand;
	g_object_get (G_OBJECT (form->priv->raw_form), "can-expand-v", &expand, NULL);
	gtk_container_child_set (GTK_CONTAINER (form), form->priv->raw_form,
				 "expand", expand, "fill", expand, NULL);
	gtk_widget_queue_resize ((GtkWidget*) form);
}

static void
gdaui_form_init (GdauiForm *form)
{
	form->priv = g_new0 (GdauiFormPriv, 1);
	form->priv->raw_form = NULL;
	form->priv->info = NULL;

	form->priv->raw_form = gdaui_raw_form_new (NULL);
	gtk_box_pack_start (GTK_BOX (form), form->priv->raw_form, FALSE, FALSE, 0);
	gtk_widget_show (form->priv->raw_form);
	g_signal_connect (form->priv->raw_form, "layout-changed",
			  G_CALLBACK (form_layout_changed_cb), form);

	form->priv->info = gdaui_data_proxy_info_new (GDAUI_DATA_PROXY (form->priv->raw_form),
						      GDAUI_DATA_PROXY_INFO_CURRENT_ROW |
						      GDAUI_DATA_PROXY_INFO_ROW_MOVE_BUTTONS);
	gtk_box_pack_start (GTK_BOX (form), form->priv->info, FALSE, FALSE, 0);
	gtk_widget_show (form->priv->info);

}

/**
 * gdaui_form_new:
 * @model: a #GdaDataModel
 *
 * Creates a new #GdauiForm widget suitable to display the data in @model
 *
 * Returns: (transfer full): the new widget
 *
 * Since: 4.2
 */
GtkWidget *
gdaui_form_new (GdaDataModel *model)
{
	GdauiForm *form;

	g_return_val_if_fail (!model || GDA_IS_DATA_MODEL (model), NULL);

	form = (GdauiForm *) g_object_new (GDAUI_TYPE_FORM, "model", model, NULL);

	return (GtkWidget *) form;
}


static void
gdaui_form_set_property (GObject *object,
			 guint param_id,
			 const GValue *value,
			 GParamSpec *pspec)
{
	GdauiForm *form;
	GdaDataModel *model;

	form = GDAUI_FORM (object);

	if (form->priv) {
		switch (param_id) {
		case PROP_MODEL: {
			gboolean expand;
			model = GDA_DATA_MODEL (g_value_get_object (value));
			g_object_set (G_OBJECT (form->priv->raw_form), "model", model, NULL);
			g_object_get (G_OBJECT (form->priv->raw_form), "can-expand-v", &expand, NULL);
			gtk_container_child_set (GTK_CONTAINER (form), form->priv->raw_form,
						 "expand", expand, "fill", expand, NULL);
			gtk_widget_queue_resize ((GtkWidget*) form);
			break;
		}
		case PROP_INFO_FLAGS:
			g_object_set (G_OBJECT (form->priv->info), "flags", g_value_get_flags (value), NULL);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
			break;
		}
	}
}

static void
gdaui_form_get_property (GObject *object,
			 guint param_id,
			 GValue *value,
			 GParamSpec *pspec)
{
	GdauiForm *form;
	GdaDataModel *model;

        form = GDAUI_FORM (object);
        if (form->priv) {
		switch (param_id) {
		case PROP_RAW_FORM:
			g_value_set_object (value, form->priv->raw_form);
			break;
		case PROP_INFO:
			g_value_set_object (value, form->priv->info);
			break;
		case PROP_INFO_FLAGS: {
			GdauiDataProxyInfoFlag flags;
			g_object_get (G_OBJECT (form->priv->info), "flags", &flags, NULL);
			g_value_set_flags (value, flags);
			break;
		}
		case PROP_MODEL:
			g_object_get (G_OBJECT (form->priv->raw_form), "model", &model, NULL);
			g_value_take_object (value, G_OBJECT (model));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
			break;
		}
        }
}

/* GdauiDataProxy interface */
static GdaDataProxy *
gdaui_form_get_proxy (GdauiDataProxy *iface)
{
	return gdaui_data_proxy_get_proxy ((GdauiDataProxy*) GDAUI_FORM (iface)->priv->raw_form);
}

static void
gdaui_form_set_column_editable (GdauiDataProxy *iface, gint column, gboolean editable)
{
	gdaui_data_proxy_column_set_editable ((GdauiDataProxy*) GDAUI_FORM (iface)->priv->raw_form,
					      column, editable);
}

static void
gdaui_form_show_column_actions (GdauiDataProxy *iface, gint column, gboolean show_actions)
{
	gdaui_data_proxy_column_show_actions ((GdauiDataProxy*) GDAUI_FORM (iface)->priv->raw_form,
					      column, show_actions);
}

static GtkActionGroup *
gdaui_form_get_actions_group (GdauiDataProxy *iface)
{
	return gdaui_data_proxy_get_actions_group ((GdauiDataProxy*) GDAUI_FORM (iface)->priv->raw_form);
}

static gboolean
gdaui_form_widget_set_write_mode (GdauiDataProxy *iface, GdauiDataProxyWriteMode mode)
{
	return gdaui_data_proxy_set_write_mode ((GdauiDataProxy*) GDAUI_FORM (iface)->priv->raw_form, mode);
}

static GdauiDataProxyWriteMode
gdaui_form_widget_get_write_mode (GdauiDataProxy *iface)
{
	return gdaui_data_proxy_get_write_mode ((GdauiDataProxy*) GDAUI_FORM (iface)->priv->raw_form);
}

/* GdauiDataSelector interface */
static GdaDataModel *
gdaui_form_selector_get_model (GdauiDataSelector *iface)
{
	return gdaui_data_selector_get_model ((GdauiDataSelector*) GDAUI_FORM (iface)->priv->raw_form);
}

static void
gdaui_form_selector_set_model (GdauiDataSelector *iface, GdaDataModel *model)
{
	gdaui_data_selector_set_model ((GdauiDataSelector*) GDAUI_FORM (iface)->priv->raw_form, model);
}

static GArray *
gdaui_form_selector_get_selected_rows (GdauiDataSelector *iface)
{
	return gdaui_data_selector_get_selected_rows ((GdauiDataSelector*) GDAUI_FORM (iface)->priv->raw_form);
}

static GdaDataModelIter *
gdaui_form_selector_get_data_set (GdauiDataSelector *iface)
{
	return gdaui_data_selector_get_data_set ((GdauiDataSelector*) GDAUI_FORM (iface)->priv->raw_form);
}

static gboolean
gdaui_form_selector_select_row (GdauiDataSelector *iface, gint row)
{
	return gdaui_data_selector_select_row ((GdauiDataSelector*) GDAUI_FORM (iface)->priv->raw_form, row);
}

static void
gdaui_form_selector_unselect_row (GdauiDataSelector *iface, gint row)
{
	gdaui_data_selector_unselect_row ((GdauiDataSelector*) GDAUI_FORM (iface)->priv->raw_form, row);
}

static void
gdaui_form_selector_set_column_visible (GdauiDataSelector *iface, gint column, gboolean visible)
{
	gdaui_data_selector_set_column_visible ((GdauiDataSelector*) GDAUI_FORM (iface)->priv->raw_form,
						column, visible);
}
