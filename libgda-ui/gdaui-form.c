/*
 * Copyright (C) 2009 - 2015 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2010 David King <davidk@openismus.com>
 * Copyright (C) 2011 Murray Cumming <murrayc@murrayc.com>
 * Copyright (C) 2018 Daniel Espinosa <esodan@gmail.com>
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

static void gdaui_form_dispose (GObject *object);

static void gdaui_form_set_property (GObject *object,
				     guint param_id,
				     const GValue *value,
				     GParamSpec *pspec);
static void gdaui_form_get_property (GObject *object,
				     guint param_id,
				     GValue *value,
				     GParamSpec *pspec);

/* GdauiDataProxy interface */
static void            gdaui_form_widget_init         (GdauiDataProxyInterface *iface);
static GdaDataProxy   *gdaui_form_get_proxy           (GdauiDataProxy *iface);
static void            gdaui_form_set_column_editable (GdauiDataProxy *iface, gint column, gboolean editable);
static gboolean        gdaui_form_supports_action       (GdauiDataProxy *iface, GdauiAction action);
static void            gdaui_form_perform_action        (GdauiDataProxy *iface, GdauiAction action);
static gboolean        gdaui_form_widget_set_write_mode (GdauiDataProxy *iface, GdauiDataProxyWriteMode mode);
static GdauiDataProxyWriteMode gdaui_form_widget_get_write_mode (GdauiDataProxy *iface);

/* GdauiDataSelector interface */
static void              gdaui_form_selector_init (GdauiDataSelectorInterface *iface);
static GdaDataModel     *gdaui_form_selector_get_model (GdauiDataSelector *iface);
static void              gdaui_form_selector_set_model (GdauiDataSelector *iface, GdaDataModel *model);
static GArray           *gdaui_form_selector_get_selected_rows (GdauiDataSelector *iface);
static GdaDataModelIter *gdaui_form_selector_get_data_set (GdauiDataSelector *iface);
static gboolean          gdaui_form_selector_select_row (GdauiDataSelector *iface, gint row);
static void              gdaui_form_selector_unselect_row (GdauiDataSelector *iface, gint row);
static void              gdaui_form_selector_set_column_visible (GdauiDataSelector *iface, gint column, gboolean visible);

typedef struct {
	GtkWidget *raw_form;
	GtkWidget *info;
} GdauiFormPrivate;

G_DEFINE_TYPE_WITH_CODE (GdauiForm, gdaui_form, GTK_TYPE_BOX,
                         G_ADD_PRIVATE (GdauiForm)
                         G_IMPLEMENT_INTERFACE (GDAUI_TYPE_DATA_PROXY, gdaui_form_widget_init)
                         G_IMPLEMENT_INTERFACE (GDAUI_TYPE_DATA_SELECTOR, gdaui_form_selector_init))

/* properties */
enum {
	PROP_0,
	PROP_RAW_FORM,
	PROP_INFO,
	PROP_MODEL,
	PROP_INFO_FLAGS
};

static void
gdaui_form_widget_init (GdauiDataProxyInterface *iface)
{
	iface->get_proxy = gdaui_form_get_proxy;
	iface->set_column_editable = gdaui_form_set_column_editable;
	iface->supports_action = gdaui_form_supports_action;
	iface->perform_action = gdaui_form_perform_action;
	iface->set_write_mode = gdaui_form_widget_set_write_mode;
	iface->get_write_mode = gdaui_form_widget_get_write_mode;
}

static void
gdaui_form_selector_init (GdauiDataSelectorInterface *iface)
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

	object_class->dispose = gdaui_form_dispose;

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
	GdauiFormPrivate *priv = gdaui_form_get_instance_private (form);
	g_object_get (G_OBJECT (priv->raw_form), "can-expand-v", &expand, NULL);
	gtk_container_child_set (GTK_CONTAINER (form), priv->raw_form,
				 "expand", expand, "fill", expand, NULL);
	gtk_widget_queue_resize ((GtkWidget*) form);
}

static void
form_selection_changed_cb (G_GNUC_UNUSED GdauiRawForm *rawform, GdauiForm *form)
{
	g_signal_emit_by_name (G_OBJECT (form), "selection-changed");
}

static void
gdaui_form_init (GdauiForm *form)
{
	GdauiFormPrivate *priv = gdaui_form_get_instance_private (form);
	priv->raw_form = NULL;
	priv->info = NULL;

	gtk_orientable_set_orientation (GTK_ORIENTABLE (form), GTK_ORIENTATION_VERTICAL);

	GtkWidget *frame;
	frame = gtk_frame_new (NULL);
	gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_IN);
	gtk_box_pack_start (GTK_BOX (form), frame, TRUE, TRUE, 0);
	gtk_widget_show (frame);

	priv->raw_form = gdaui_raw_form_new (NULL);
	gtk_container_add (GTK_CONTAINER (frame), priv->raw_form);

	gtk_widget_show (priv->raw_form);
	g_signal_connect (priv->raw_form, "layout-changed",
			  G_CALLBACK (form_layout_changed_cb), form);
	g_signal_connect (priv->raw_form, "selection-changed",
			  G_CALLBACK (form_selection_changed_cb), form);

	priv->info = gdaui_data_proxy_info_new (GDAUI_DATA_PROXY (priv->raw_form),
						      GDAUI_DATA_PROXY_INFO_CURRENT_ROW |
						      GDAUI_DATA_PROXY_INFO_ROW_MOVE_BUTTONS);
	gtk_widget_set_halign (priv->info, GTK_ALIGN_START);
	gtk_style_context_add_class (gtk_widget_get_style_context (priv->info), "inline-toolbar");

	gtk_box_pack_start (GTK_BOX (form), priv->info, FALSE, FALSE, 0);
	gtk_widget_show (priv->info);

}

static void
gdaui_form_dispose (GObject *object)
{
	GdauiForm *form;

	g_return_if_fail (GDAUI_IS_FORM (object));
	form = GDAUI_FORM (object);
	GdauiFormPrivate *priv = gdaui_form_get_instance_private (form);

	if (priv->raw_form != NULL) {
		g_signal_handlers_disconnect_by_func (priv->raw_form,
		                                     G_CALLBACK (form_layout_changed_cb), form);
	  g_signal_handlers_disconnect_by_func (priv->raw_form,
		                                     G_CALLBACK (form_selection_changed_cb), form);
		priv->raw_form = NULL;
  }
	/* for the parent class */
	G_OBJECT_CLASS (gdaui_form_parent_class)->dispose (object);
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
	GdauiFormPrivate *priv = gdaui_form_get_instance_private (form);

	switch (param_id) {
		case PROP_MODEL: {
			model = GDA_DATA_MODEL (g_value_get_object (value));
			g_object_set (G_OBJECT (priv->raw_form), "model", model, NULL);
			gtk_container_child_set (GTK_CONTAINER (form), priv->raw_form,
						 "expand", TRUE, "fill", TRUE, NULL);
			gtk_widget_queue_resize ((GtkWidget*) form);
			break;
		}
		case PROP_INFO_FLAGS:
			g_object_set (G_OBJECT (priv->info), "flags", g_value_get_flags (value), NULL);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
			break;
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
	GdauiFormPrivate *priv = gdaui_form_get_instance_private (form);
	switch (param_id) {
		case PROP_RAW_FORM:
			g_value_set_object (value, priv->raw_form);
			break;
		case PROP_INFO:
			g_value_set_object (value, priv->info);
			break;
		case PROP_INFO_FLAGS: {
			GdauiDataProxyInfoFlag flags;
			g_object_get (G_OBJECT (priv->info), "flags", &flags, NULL);
			g_value_set_flags (value, flags);
			break;
		}
		case PROP_MODEL:
			g_object_get (G_OBJECT (priv->raw_form), "model", &model, NULL);
			g_value_take_object (value, G_OBJECT (model));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
			break;
	}
}

/* GdauiDataProxy interface */
static GdaDataProxy *
gdaui_form_get_proxy (GdauiDataProxy *iface)
{
	GdauiFormPrivate *priv = gdaui_form_get_instance_private (GDAUI_FORM (iface));
	return gdaui_data_proxy_get_proxy ((GdauiDataProxy*) priv->raw_form);
}

static void
gdaui_form_set_column_editable (GdauiDataProxy *iface, gint column, gboolean editable)
{
	GdauiFormPrivate *priv = gdaui_form_get_instance_private (GDAUI_FORM (iface));
	gdaui_data_proxy_column_set_editable ((GdauiDataProxy*) priv->raw_form,
					      column, editable);
}

static gboolean
gdaui_form_supports_action (GdauiDataProxy *iface, GdauiAction action)
{
	GdauiFormPrivate *priv = gdaui_form_get_instance_private (GDAUI_FORM (iface));
	return gdaui_data_proxy_supports_action ((GdauiDataProxy*) priv->raw_form, action);
}

static void
gdaui_form_perform_action (GdauiDataProxy *iface, GdauiAction action)
{
	GdauiFormPrivate *priv = gdaui_form_get_instance_private (GDAUI_FORM (iface));
	gdaui_data_proxy_perform_action ((GdauiDataProxy*) priv->raw_form, action);
}

static gboolean
gdaui_form_widget_set_write_mode (GdauiDataProxy *iface, GdauiDataProxyWriteMode mode)
{
	GdauiFormPrivate *priv = gdaui_form_get_instance_private (GDAUI_FORM (iface));
	return gdaui_data_proxy_set_write_mode ((GdauiDataProxy*) priv->raw_form, mode);
}

static GdauiDataProxyWriteMode
gdaui_form_widget_get_write_mode (GdauiDataProxy *iface)
{
	GdauiFormPrivate *priv = gdaui_form_get_instance_private (GDAUI_FORM (iface));
	return gdaui_data_proxy_get_write_mode ((GdauiDataProxy*) priv->raw_form);
}

/* GdauiDataSelector interface */
static GdaDataModel *
gdaui_form_selector_get_model (GdauiDataSelector *iface)
{
	GdauiFormPrivate *priv = gdaui_form_get_instance_private (GDAUI_FORM (iface));
	return gdaui_data_selector_get_model ((GdauiDataSelector*) priv->raw_form);
}

static void
gdaui_form_selector_set_model (GdauiDataSelector *iface, GdaDataModel *model)
{
	GdauiFormPrivate *priv = gdaui_form_get_instance_private (GDAUI_FORM (iface));
	gdaui_data_selector_set_model ((GdauiDataSelector*) priv->raw_form, model);
}

static GArray *
gdaui_form_selector_get_selected_rows (GdauiDataSelector *iface)
{
	GdauiFormPrivate *priv = gdaui_form_get_instance_private (GDAUI_FORM (iface));
	return gdaui_data_selector_get_selected_rows ((GdauiDataSelector*)priv->raw_form);
}

static GdaDataModelIter *
gdaui_form_selector_get_data_set (GdauiDataSelector *iface)
{
	GdauiFormPrivate *priv = gdaui_form_get_instance_private (GDAUI_FORM (iface));
	return gdaui_data_selector_get_data_set ((GdauiDataSelector*) priv->raw_form);
}

static gboolean
gdaui_form_selector_select_row (GdauiDataSelector *iface, gint row)
{
	GdauiFormPrivate *priv = gdaui_form_get_instance_private (GDAUI_FORM (iface));
	return gdaui_data_selector_select_row ((GdauiDataSelector*) priv->raw_form, row);
}

static void
gdaui_form_selector_unselect_row (GdauiDataSelector *iface, gint row)
{
	GdauiFormPrivate *priv = gdaui_form_get_instance_private (GDAUI_FORM (iface));
	gdaui_data_selector_unselect_row ((GdauiDataSelector*) priv->raw_form, row);
}

static void
gdaui_form_selector_set_column_visible (GdauiDataSelector *iface, gint column, gboolean visible)
{
	GdauiFormPrivate *priv = gdaui_form_get_instance_private (GDAUI_FORM (iface));
	gdaui_data_selector_set_column_visible ((GdauiDataSelector*) priv->raw_form,
						column, visible);
}
