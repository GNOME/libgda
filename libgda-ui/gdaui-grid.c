/* gdaui-grid.c
 *
 * Copyright (C) 2002 - 2009 Vivien Malerba
 *
 * This Library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

#include <string.h>
#include <glib/gi18n-lib.h>
#include <libgda/libgda.h>
#include "gdaui-grid.h"
#include "gdaui-data-widget.h"
#include "gdaui-raw-grid.h"
#include "gdaui-data-widget-info.h"

static void gdaui_grid_class_init (GdauiGridClass * class);
static void gdaui_grid_init (GdauiGrid *wid);

static void gdaui_grid_set_property (GObject *object,
				     guint param_id,
				     const GValue *value,
				     GParamSpec *pspec);
static void gdaui_grid_get_property (GObject *object,
				     guint param_id,
				     GValue *value,
				     GParamSpec *pspec);

struct _GdauiGridPriv
{
	GtkWidget *raw_grid;
	GtkWidget *info;
};

/* get a pointer to the parents to be able to call their destructor */
static GObjectClass *parent_class = NULL;

/* properties */
enum
{
        PROP_0,
        PROP_RAW_GRID,
	PROP_INFO,
	PROP_MODEL
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
			(GInstanceInitFunc) gdaui_grid_init
		};		

		type = g_type_register_static (GTK_TYPE_VBOX, "GdauiGrid", &info, 0);
	}

	return type;
}

static void
gdaui_grid_class_init (GdauiGridClass *class)
{
	GObjectClass   *object_class = G_OBJECT_CLASS (class);
	
	parent_class = g_type_class_peek_parent (class);

	/* Properties */
        object_class->set_property = gdaui_grid_set_property;
        object_class->get_property = gdaui_grid_get_property;
	g_object_class_install_property (object_class, PROP_RAW_GRID,
                                         g_param_spec_object ("raw_grid", NULL, NULL, 
							      GDAUI_TYPE_RAW_GRID,
							      G_PARAM_READABLE));
	g_object_class_install_property (object_class, PROP_INFO,
                                         g_param_spec_object ("widget_info", NULL, NULL, 
							      GDAUI_TYPE_DATA_WIDGET_INFO,
							      G_PARAM_READABLE));
	g_object_class_install_property (object_class, PROP_MODEL,
	                                 g_param_spec_object ("model", NULL, NULL,
	                                                      GDA_TYPE_DATA_MODEL,
	                                                      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
}

static void
gdaui_grid_init (GdauiGrid *grid)
{
	GtkWidget *sw;
	
	grid->priv = g_new0 (GdauiGridPriv, 1);
	grid->priv->raw_grid = NULL;
	grid->priv->info = NULL;
	
	sw = gtk_scrolled_window_new (NULL, NULL);
        gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
        gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw), GTK_SHADOW_IN);
	gtk_box_pack_start (GTK_BOX (grid), sw, TRUE, TRUE, 0);
	gtk_widget_show (sw);

	grid->priv->raw_grid = gdaui_raw_grid_new (NULL);
	gtk_container_add (GTK_CONTAINER (sw), grid->priv->raw_grid);
	gtk_widget_show (grid->priv->raw_grid);

	grid->priv->info = gdaui_data_widget_info_new (GDAUI_DATA_WIDGET (grid->priv->raw_grid), 
							  GDAUI_DATA_WIDGET_INFO_CURRENT_ROW |
							  GDAUI_DATA_WIDGET_INFO_ROW_MODIFY_BUTTONS |
							  GDAUI_DATA_WIDGET_INFO_CHUNCK_CHANGE_BUTTONS);
	gtk_box_pack_start (GTK_BOX (grid), grid->priv->info, FALSE, TRUE, 0);
	gtk_widget_show (grid->priv->info);
}

/**
 * gdaui_grid_new
 * @model: a #GdaDataModel
 *
 * Creates a new #GdauiGrid widget suitable to display the data in @model
 *
 *  Returns: the new widget
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
		g_object_set(G_OBJECT (grid->priv->raw_grid),
		             "model", model, NULL);
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
	case PROP_MODEL:
		g_object_get (G_OBJECT (grid->priv->raw_grid),
		              "model", &model, NULL);
		g_value_take_object (value, G_OBJECT (model));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}	
}

/**
 * gdaui_grid_get_selection
 * @grid: a #GdauiGrid widget
 * 
 * Returns the list of the currently selected rows in a #GdauiGrid widget. 
 * The returned value is a list of integers, which represent each of the selected rows.
 *
 * If new rows have been inserted, then those new rows will have a row number equal to -1.
 * This function is a wrapper around the gdaui_raw_grid_get_selection() function.
 *
 * Returns: a new list, should be freed (by calling g_list_free) when no longer needed.
 */
GList *
gdaui_grid_get_selection (GdauiGrid *grid)
{
	g_return_val_if_fail (grid && GDAUI_IS_GRID (grid), NULL);
	g_return_val_if_fail (grid->priv, NULL);

	return gdaui_raw_grid_get_selection (GDAUI_RAW_GRID (grid->priv->raw_grid));
}

/**
 * gdaui_grid_set_sample_size
 * @grid: a #GdauiGrid widget
 * @sample_size:
 *
 *
 */
void
gdaui_grid_set_sample_size (GdauiGrid *grid, gint sample_size)
{
	g_return_if_fail (grid && GDAUI_IS_GRID (grid));
	g_return_if_fail (grid->priv);

	gdaui_raw_grid_set_sample_size (GDAUI_RAW_GRID (grid->priv->raw_grid), sample_size);
}
