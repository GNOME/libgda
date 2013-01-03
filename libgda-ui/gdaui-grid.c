/*
 * Copyright (C) 2009 - 2011 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2010 David King <davidk@openismus.com>
 * Copyright (C) 2011 Murray Cumming <murrayc@murrayc.com>
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
#include "gdaui-grid.h"
#include "gdaui-data-proxy.h"
#include "gdaui-data-selector.h"
#include "gdaui-raw-grid.h"
#include "gdaui-data-proxy-info.h"
#include "gdaui-enum-types.h"

static void gdaui_grid_class_init (GdauiGridClass * class);
static void gdaui_grid_init (GdauiGrid *wid);
static void gdaui_grid_dispose (GObject *object);

static void gdaui_grid_set_property (GObject *object,
				     guint param_id,
				     const GValue *value,
				     GParamSpec *pspec);
static void gdaui_grid_get_property (GObject *object,
				     guint param_id,
				     GValue *value,
				     GParamSpec *pspec);

/* GdauiDataProxy interface */
static void            gdaui_grid_widget_init         (GdauiDataProxyIface *iface);
static GdaDataProxy   *gdaui_grid_get_proxy           (GdauiDataProxy *iface);
static void            gdaui_grid_set_column_editable (GdauiDataProxy *iface, gint column, gboolean editable);
static void            gdaui_grid_show_column_actions (GdauiDataProxy *iface, gint column, gboolean show_actions);
static GtkActionGroup *gdaui_grid_get_actions_group   (GdauiDataProxy *iface);
static gboolean        gdaui_grid_widget_set_write_mode (GdauiDataProxy *iface, GdauiDataProxyWriteMode mode);
static GdauiDataProxyWriteMode gdaui_grid_widget_get_write_mode (GdauiDataProxy *iface);

/* GdauiDataSelector interface */
static void              gdaui_grid_selector_init (GdauiDataSelectorIface *iface);
static GdaDataModel     *gdaui_grid_selector_get_model (GdauiDataSelector *iface);
static void              gdaui_grid_selector_set_model (GdauiDataSelector *iface, GdaDataModel *model);
static GArray           *gdaui_grid_selector_get_selected_rows (GdauiDataSelector *iface);
static GdaDataModelIter *gdaui_grid_selector_get_data_set (GdauiDataSelector *iface);
static gboolean          gdaui_grid_selector_select_row (GdauiDataSelector *iface, gint row);
static void              gdaui_grid_selector_unselect_row (GdauiDataSelector *iface, gint row);
static void              gdaui_grid_selector_set_column_visible (GdauiDataSelector *iface, gint column, gboolean visible);

struct _GdauiGridPriv
{
	GtkWidget *raw_grid;
	GtkWidget *info;
};

/* get a pointer to the parents to be able to call their destructor */
static GObjectClass *parent_class = NULL;

/* properties */
enum {
	PROP_0,
	PROP_RAW_GRID,
	PROP_INFO,
	PROP_MODEL,
	PROP_INFO_FLAGS
};

GType
gdaui_grid_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo info = {
			sizeof (GdauiGridClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gdaui_grid_class_init,
			NULL,
			NULL,
			sizeof (GdauiGrid),
			0,
			(GInstanceInitFunc) gdaui_grid_init,
			0
		};

		static const GInterfaceInfo proxy_info = {
                        (GInterfaceInitFunc) gdaui_grid_widget_init,
                        NULL,
                        NULL
                };

		static const GInterfaceInfo selector_info = {
                        (GInterfaceInitFunc) gdaui_grid_selector_init,
                        NULL,
                        NULL
                };

		type = g_type_register_static (GTK_TYPE_BOX, "GdauiGrid", &info, 0);
		g_type_add_interface_static (type, GDAUI_TYPE_DATA_PROXY, &proxy_info);
		g_type_add_interface_static (type, GDAUI_TYPE_DATA_SELECTOR, &selector_info);

	}

	return type;
}

static void
gdaui_grid_widget_init (GdauiDataProxyIface *iface)
{
	iface->get_proxy = gdaui_grid_get_proxy;
	iface->set_column_editable = gdaui_grid_set_column_editable;
	iface->show_column_actions = gdaui_grid_show_column_actions;
	iface->get_actions_group = gdaui_grid_get_actions_group;
	iface->set_write_mode = gdaui_grid_widget_set_write_mode;
	iface->get_write_mode = gdaui_grid_widget_get_write_mode;
}

static void
gdaui_grid_selector_init (GdauiDataSelectorIface *iface)
{
	iface->get_model = gdaui_grid_selector_get_model;
	iface->set_model = gdaui_grid_selector_set_model;
	iface->get_selected_rows = gdaui_grid_selector_get_selected_rows;
	iface->get_data_set = gdaui_grid_selector_get_data_set;
	iface->select_row = gdaui_grid_selector_select_row;
	iface->unselect_row = gdaui_grid_selector_unselect_row;
	iface->set_column_visible = gdaui_grid_selector_set_column_visible;
}

static void
gdaui_grid_class_init (GdauiGridClass *class)
{
	GObjectClass   *object_class = G_OBJECT_CLASS (class);

	parent_class = g_type_class_peek_parent (class);
	object_class->dispose = gdaui_grid_dispose;

	/* Properties */
        object_class->set_property = gdaui_grid_set_property;
        object_class->get_property = gdaui_grid_get_property;
	g_object_class_install_property (object_class, PROP_RAW_GRID,
                                         g_param_spec_object ("raw-grid", NULL, NULL,
							      GDAUI_TYPE_RAW_GRID,
							      G_PARAM_READABLE));
	g_object_class_install_property (object_class, PROP_INFO,
                                         g_param_spec_object ("info", NULL, NULL,
							      GDAUI_TYPE_DATA_PROXY_INFO,
							      G_PARAM_READABLE));
	g_object_class_install_property (object_class, PROP_INFO_FLAGS,
                                         g_param_spec_flags ("info-flags", NULL, NULL,
							     GDAUI_TYPE_DATA_PROXY_INFO_FLAG,
							     GDAUI_DATA_PROXY_INFO_CURRENT_ROW,
							     G_PARAM_READABLE | G_PARAM_WRITABLE));
	g_object_class_install_property (object_class, PROP_MODEL,
	                                 g_param_spec_object ("model", NULL, NULL,
	                                                      GDA_TYPE_DATA_MODEL,
	                                                      G_PARAM_READWRITE));
}

static void
raw_grid_selection_changed_cb (G_GNUC_UNUSED GdauiRawGrid *rawgrid, GdauiGrid *grid)
{
	g_signal_emit_by_name (G_OBJECT (grid), "selection-changed");
}

static void
gdaui_grid_init (GdauiGrid *grid)
{
	GtkWidget *sw;

	grid->priv = g_new0 (GdauiGridPriv, 1);
	grid->priv->raw_grid = NULL;
	grid->priv->info = NULL;

	gtk_orientable_set_orientation (GTK_ORIENTABLE (grid), GTK_ORIENTATION_VERTICAL);

	sw = gtk_scrolled_window_new (NULL, NULL);
        gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
        gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw), GTK_SHADOW_IN);
	gtk_box_pack_start (GTK_BOX (grid), sw, TRUE, TRUE, 0);
	gtk_widget_show (sw);

	grid->priv->raw_grid = gdaui_raw_grid_new (NULL);
	gtk_container_add (GTK_CONTAINER (sw), grid->priv->raw_grid);
	gtk_widget_show (grid->priv->raw_grid);
	g_signal_connect (grid->priv->raw_grid, "selection-changed",
			  G_CALLBACK (raw_grid_selection_changed_cb), grid);

	grid->priv->info = gdaui_data_proxy_info_new (GDAUI_DATA_PROXY (grid->priv->raw_grid),
						      GDAUI_DATA_PROXY_INFO_CURRENT_ROW);
	gtk_box_pack_start (GTK_BOX (grid), grid->priv->info, FALSE, TRUE, 0);
	gtk_widget_show (grid->priv->info);
}

static void
gdaui_grid_dispose (GObject *object)
{
	GdauiGrid *grid;

	g_return_if_fail (GDAUI_IS_GRID (object));
	grid = GDAUI_GRID (object);

	if (grid->priv) {
		g_signal_handlers_disconnect_by_func (grid->priv->raw_grid,
						      G_CALLBACK (raw_grid_selection_changed_cb), grid);

		/* the private area itself */
		g_free (grid->priv);
		grid->priv = NULL;
	}

	/* for the parent class */
	parent_class->dispose (object);
}

/**
 * gdaui_grid_new:
 * @model: (allow-none): a #GdaDataModel, or %NULL
 *
 * Creates a new #GdauiGrid widget suitable to display the data in @model
 *
 * Returns: (transfer full): the new widget
 *
 * Since: 4.2
 */
GtkWidget *
gdaui_grid_new (GdaDataModel *model)
{
	GdauiGrid *grid;

	g_return_val_if_fail (!model || GDA_IS_DATA_MODEL (model), NULL);

	grid = (GdauiGrid *) g_object_new (GDAUI_TYPE_GRID,
					   "model", model, NULL);

	return (GtkWidget *) grid;
}


static void
gdaui_grid_set_property (GObject *object,
			 guint param_id,
			 const GValue *value,
			 GParamSpec *pspec)
{
	GdauiGrid *grid;
	GdaDataModel *model;

	grid = GDAUI_GRID (object);

	switch (param_id) {
	case PROP_MODEL:
		model = GDA_DATA_MODEL (g_value_get_object (value));
		g_object_set (G_OBJECT (grid->priv->raw_grid), "model", model, NULL);
		break;
	case PROP_INFO_FLAGS:
		g_object_set (G_OBJECT (grid->priv->info), "flags", g_value_get_flags (value), NULL);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}
}

static void
gdaui_grid_get_property (GObject *object,
			 guint param_id,
			 GValue *value,
			 GParamSpec *pspec)
{
	GdauiGrid *grid;
	GdaDataModel *model;

	grid = GDAUI_GRID (object);

	switch (param_id) {
	case PROP_RAW_GRID:
		g_value_set_object (value, grid->priv->raw_grid);
		break;
	case PROP_INFO:
		g_value_set_object (value, grid->priv->info);
		break;
	case PROP_INFO_FLAGS: {
			GdauiDataProxyInfoFlag flags;
			g_object_get (G_OBJECT (grid->priv->info), "flags", &flags, NULL);
			g_value_set_flags (value, flags);
			break;
		}
	case PROP_MODEL:
		g_object_get (G_OBJECT (grid->priv->raw_grid), "model", &model, NULL);
		g_value_take_object (value, G_OBJECT (model));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}
}

/**
 * gdaui_grid_set_sample_size:
 * @grid: a #GdauiGrid widget
 * @sample_size: the size of the sample displayed in @grid
 *
 * Sets the size of each chunk of data to display: the maximum number of rows which
 * can be displayed at a time. See gdaui_raw_grid_set_sample_size() and gda_data_proxy_set_sample_size()
 *
 * Since: 4.2
 */
void
gdaui_grid_set_sample_size (GdauiGrid *grid, gint sample_size)
{
	g_return_if_fail (grid && GDAUI_IS_GRID (grid));
	g_return_if_fail (grid->priv);

	gdaui_raw_grid_set_sample_size (GDAUI_RAW_GRID (grid->priv->raw_grid), sample_size);
}

/* GdauiDataProxy interface */
static GdaDataProxy *
gdaui_grid_get_proxy (GdauiDataProxy *iface)
{
	return gdaui_data_proxy_get_proxy ((GdauiDataProxy*) GDAUI_GRID (iface)->priv->raw_grid);
}

static void
gdaui_grid_set_column_editable (GdauiDataProxy *iface, gint column, gboolean editable)
{
	gdaui_data_proxy_column_set_editable ((GdauiDataProxy*) GDAUI_GRID (iface)->priv->raw_grid,
					      column, editable);
}

static void
gdaui_grid_show_column_actions (GdauiDataProxy *iface, gint column, gboolean show_actions)
{
	gdaui_data_proxy_column_show_actions ((GdauiDataProxy*) GDAUI_GRID (iface)->priv->raw_grid,
					      column, show_actions);
}

static GtkActionGroup *
gdaui_grid_get_actions_group (GdauiDataProxy *iface)
{
	return gdaui_data_proxy_get_actions_group ((GdauiDataProxy*) GDAUI_GRID (iface)->priv->raw_grid);
}

static gboolean
gdaui_grid_widget_set_write_mode (GdauiDataProxy *iface, GdauiDataProxyWriteMode mode)
{
	return gdaui_data_proxy_set_write_mode ((GdauiDataProxy*) GDAUI_GRID (iface)->priv->raw_grid, mode);
}

static GdauiDataProxyWriteMode
gdaui_grid_widget_get_write_mode (GdauiDataProxy *iface)
{
	return gdaui_data_proxy_get_write_mode ((GdauiDataProxy*) GDAUI_GRID (iface)->priv->raw_grid);
}

/* GdauiDataSelector interface */
static GdaDataModel *
gdaui_grid_selector_get_model (GdauiDataSelector *iface)
{
	return gdaui_data_selector_get_model ((GdauiDataSelector*) GDAUI_GRID (iface)->priv->raw_grid);
}

static void
gdaui_grid_selector_set_model (GdauiDataSelector *iface, GdaDataModel *model)
{
	gdaui_data_selector_set_model ((GdauiDataSelector*) GDAUI_GRID (iface)->priv->raw_grid, model);
}

static GArray *
gdaui_grid_selector_get_selected_rows (GdauiDataSelector *iface)
{
	return gdaui_data_selector_get_selected_rows ((GdauiDataSelector*) GDAUI_GRID (iface)->priv->raw_grid);
}

static GdaDataModelIter *
gdaui_grid_selector_get_data_set (GdauiDataSelector *iface)
{
	return gdaui_data_selector_get_data_set ((GdauiDataSelector*) GDAUI_GRID (iface)->priv->raw_grid);
}

static gboolean
gdaui_grid_selector_select_row (GdauiDataSelector *iface, gint row)
{
	return gdaui_data_selector_select_row ((GdauiDataSelector*) GDAUI_GRID (iface)->priv->raw_grid, row);
}

static void
gdaui_grid_selector_unselect_row (GdauiDataSelector *iface, gint row)
{
	gdaui_data_selector_unselect_row ((GdauiDataSelector*) GDAUI_GRID (iface)->priv->raw_grid, row);
}

static void
gdaui_grid_selector_set_column_visible (GdauiDataSelector *iface, gint column, gboolean visible)
{
	gdaui_data_selector_set_column_visible ((GdauiDataSelector*) GDAUI_GRID (iface)->priv->raw_grid,
						column, visible);
}
