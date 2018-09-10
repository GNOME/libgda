/*
 * Copyright (C) 2009 - 2014 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2010 David King <davidk@openismus.com>
 * Copyright (C) 2013 Daniel Espinosa <esodan@gmail.com>
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
#include <libgda/binreloc/gda-binreloc.h>
#include "gdaui-data-selector.h"
#include  <glib/gi18n-lib.h>
#include <libgda-ui/gdaui-raw-form.h>
#include <libgda-ui/gdaui-raw-grid.h>

/* signals */
enum
{
        SELECTION_CHANGED,
        LAST_SIGNAL
};

static gint gdaui_data_selector_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_INTERFACE(GdauiDataSelector, gdaui_data_selector, G_TYPE_OBJECT)

static void
gdaui_data_selector_default_init (GdauiDataSelectorInterface *iface)
{
		gdaui_data_selector_signals[SELECTION_CHANGED] =
			g_signal_new ("selection-changed",
                                      GDAUI_TYPE_DATA_SELECTOR,
                                      G_SIGNAL_RUN_FIRST,
                                      G_STRUCT_OFFSET (GdauiDataSelectorInterface, selection_changed),
                                      NULL, NULL,
                                      g_cclosure_marshal_VOID__VOID, G_TYPE_NONE,
                                      0);

}

/**
 * gdaui_data_selector_get_model:
 * @iface: an object which implements the #GdauiDataSelector interface
 *
 * Queries the #GdaDataModel from which the data displayed by the widget implementing @iface
 * are. Beware that the returned data model may be different than the one used when the
 * widget was created in case it internally uses a #GdaDataProxy.
 *
 * Returns: (transfer none): the #GdaDataModel
 *
 * Since: 4.2
 */
GdaDataModel *
gdaui_data_selector_get_model (GdauiDataSelector *iface)
{
	g_return_val_if_fail (GDAUI_IS_DATA_SELECTOR (iface), NULL);
	
	if (GDAUI_DATA_SELECTOR_GET_IFACE (iface)->get_model)
		return (GDAUI_DATA_SELECTOR_GET_IFACE (iface)->get_model) (iface);
	return NULL;
}

/**
 * gdaui_data_selector_set_model:
 * @iface: an object which implements the #GdauiDataSelector interface
 * @model: a #GdaDataModel to use
 *
 * Sets the data model from which the data being displayed are. Also see gdaui_data_selector_get_model()
 *
 * Since: 4.2
 */
void
gdaui_data_selector_set_model (GdauiDataSelector *iface, GdaDataModel *model)
{
	g_return_if_fail (GDAUI_IS_DATA_SELECTOR (iface));
	g_return_if_fail (!model || GDA_IS_DATA_MODEL (model));
	
	if (GDAUI_DATA_SELECTOR_GET_IFACE (iface)->set_model)
		(GDAUI_DATA_SELECTOR_GET_IFACE (iface)->set_model) (iface, model);
}

/**
 * gdaui_data_selector_get_selected_rows:
 * @iface: an object which implements the #GdauiDataSelector interface
 *
 * Gat an array of selected rows. If no row is selected, the the returned value is %NULL.
 *
 * Please note that rows refers to the "visible" rows
 * at the time it's being called, which may change if the widget implementing this interface
 * uses a #GdaDataProxy (as is the case for example for the #GdauiRawForm, #GdauiForm, #GdauiRawGrid
 * and #GdauiGrid).
 *
 * Returns: (transfer full) (element-type gint): an array of #gint values, one for each selected row. Use g_array_free() when finished (passing %TRUE as the last argument)
 *
 * Since: 4.2
 */
GArray *
gdaui_data_selector_get_selected_rows (GdauiDataSelector *iface)
{
	g_return_val_if_fail (GDAUI_IS_DATA_SELECTOR (iface), NULL);
	
	if (GDAUI_DATA_SELECTOR_GET_IFACE (iface)->get_selected_rows)
		return (GDAUI_DATA_SELECTOR_GET_IFACE (iface)->get_selected_rows) (iface);
	else
		return NULL;
}

/**
 * gdaui_data_selector_select_row:
 * @iface: an object which implements the #GdauiDataSelector interface
 * @row: the row to select
 *
 * Force the selection of a specific row.
 *
 * Please note that @row refers to the "visible" row
 * at the time it's being called, which may change if the widget implementing this interface
 * uses a #GdaDataProxy (as is the case for example for the #GdauiRawForm, #GdauiForm, #GdauiRawGrid
 * and #GdauiGrid).
 *
 * Returns: %TRUE if the row has been selected
 *
 * Since: 4.2
 */
gboolean
gdaui_data_selector_select_row (GdauiDataSelector *iface, gint row)
{
	g_return_val_if_fail (GDAUI_IS_DATA_SELECTOR (iface), FALSE);
	
	if (GDAUI_DATA_SELECTOR_GET_IFACE (iface)->select_row)
		return (GDAUI_DATA_SELECTOR_GET_IFACE (iface)->select_row) (iface, row);
	else
		return FALSE;
}

/**
 * gdaui_data_selector_unselect_row:
 * @iface: an object which implements the #GdauiDataSelector interface
 * @row: the row to unselect
 *
 * Please note that @row refers to the "visible" row
 * at the time it's being called, which may change if the widget implementing this interface
 * uses a #GdaDataProxy (as is the case for example for the #GdauiRawForm, #GdauiForm, #GdauiRawGrid
 * and #GdauiGrid).
 *
 * Since: 4.2
 */
void
gdaui_data_selector_unselect_row (GdauiDataSelector *iface, gint row)
{
	g_return_if_fail (GDAUI_IS_DATA_SELECTOR (iface));
	
	if (GDAUI_DATA_SELECTOR_GET_IFACE (iface)->unselect_row)
		(GDAUI_DATA_SELECTOR_GET_IFACE (iface)->unselect_row) (iface, row);
}

/**
 * gdaui_data_selector_set_column_visible:
 * @iface: an object which implements the #GdauiDataSelector interface
 * @column: a column number, starting at %0, or -1 to apply to all the columns
 * @visible: required visibility of the data in the @column column
 *
 * Shows or hides the data at column @column
 *
 * Since: 4.2
 */
void
gdaui_data_selector_set_column_visible (GdauiDataSelector *iface, gint column, gboolean visible)
{
	g_return_if_fail (GDAUI_IS_DATA_SELECTOR (iface));
	
	if (!GDAUI_DATA_SELECTOR_GET_IFACE (iface)->set_column_visible)
		return;

	if (column >= 0)
		(GDAUI_DATA_SELECTOR_GET_IFACE (iface)->set_column_visible) (iface, column, visible);
	else if (column == -1) {
		gint i, ncols;
		GdaDataModelIter *iter;
		iter = gdaui_data_selector_get_data_set (iface);
		if (!iter)
			return;
		ncols = g_slist_length (gda_set_get_holders (GDA_SET (iter)));
		for (i = 0; i < ncols; i++)
			(GDAUI_DATA_SELECTOR_GET_IFACE (iface)->set_column_visible) (iface, i, visible);
	}
	else
		g_warning (_("Invalid column number %d"), column);
}

/**
 * gdaui_data_selector_get_data_set:
 * @iface: an object which implements the #GdauiDataSelector interface
 *
 * Get the #GdaDataModelIter object represented the current selected row in @iface. This
 * function may return either %NULL or an invalid iterator (see gda_data_model_iter_is_valid()) if
 * the selection cannot be represented by a single selected row.
 *
 * Note that the returned #GdaDataModelIter is actually an iterator iterating on the #GdaDataModel
 * returned by the gdaui_data_selector_get_model() method.
 *
 * Returns: (transfer none): a pointer to a #GdaDataModelIter object, or %NULL
 *
 * Since: 4.2
 */
GdaDataModelIter *
gdaui_data_selector_get_data_set (GdauiDataSelector *iface)
{
	g_return_val_if_fail (GDAUI_IS_DATA_SELECTOR (iface), NULL);

	if (GDAUI_DATA_SELECTOR_GET_IFACE (iface)->get_data_set)
		return (GDAUI_DATA_SELECTOR_GET_IFACE (iface)->get_data_set) (iface);
	return NULL;
}
