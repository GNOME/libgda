/*
 * Copyright (C) 2009 - 2012 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2010 David King <davidk@openismus.com>
 * Copyright (C) 2012 Daniel Espinosa <esodan@gmail.com>
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
#include <gtk/gtk.h>
#include "gdaui-combo.h"
#include "gdaui-data-store.h"
#include "gdaui-data-selector.h"

typedef struct {
	GdaDataModel     *model; /* proxied model (the one when _set_model() is called) */
	GdaDataModelIter *iter; /* for @model, may be NULL */
	GdauiDataStore   *store; /* model proxy */

	/* columns of the model to display */
	gint              n_cols;
	gint             *cols_index;

	/* columns' chars width if computed, for all the model's columns */
	gint             *cols_width;

	gulong            changed_id; /* signal handler ID for the "changed" signal */
} GdauiComboPrivate;

static void gdaui_combo_set_property (GObject *object,
				      guint paramid,
				      const GValue *value,
				      GParamSpec *pspec);
static void gdaui_combo_get_property (GObject *object,
				      guint param_id,
				      GValue *value,
				      GParamSpec *pspec);
static void gdaui_combo_dispose      (GObject *object);
static void gdaui_combo_finalize     (GObject *object);

static void gdaui_combo_get_preferred_width (GtkWidget *widget,
					     gint *minimum_size,
					     gint *natural_size);

/* GdauiDataSelector interface */
static void              gdaui_combo_selector_init (GdauiDataSelectorInterface *iface);
static GdaDataModel     *combo_selector_get_model (GdauiDataSelector *iface);
static void              combo_selector_set_model (GdauiDataSelector *iface, GdaDataModel *model);
static GArray           *combo_selector_get_selected_rows (GdauiDataSelector *iface);
static GdaDataModelIter *combo_selector_get_data_set (GdauiDataSelector *iface);
static gboolean          combo_selector_select_row (GdauiDataSelector *iface, gint row);
static void              combo_selector_unselect_row (GdauiDataSelector *iface, gint row);
static void              combo_selector_set_column_visible (GdauiDataSelector *iface, gint column, gboolean visible);

enum {
	PROP_0,
	PROP_MODEL,
	PROP_AS_LIST
};

G_DEFINE_TYPE_WITH_CODE(GdauiCombo, gdaui_combo, GTK_TYPE_COMBO_BOX,
                        G_ADD_PRIVATE (GdauiCombo)
                        G_IMPLEMENT_INTERFACE (GDAUI_TYPE_DATA_SELECTOR, gdaui_combo_selector_init))


static void
gdaui_combo_class_init (GdauiComboClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->set_property = gdaui_combo_set_property;
	object_class->get_property = gdaui_combo_get_property;
	object_class->dispose = gdaui_combo_dispose;
	object_class->finalize = gdaui_combo_finalize;

	GTK_WIDGET_CLASS (klass)->get_preferred_width = gdaui_combo_get_preferred_width;

	/* add class properties */
	g_object_class_install_property (object_class, PROP_MODEL,
					 g_param_spec_object ("model", _("The data model to display"), NULL, 
							      GDA_TYPE_DATA_MODEL,
							      (G_PARAM_READABLE | G_PARAM_WRITABLE)));
	g_object_class_install_property (object_class, PROP_AS_LIST,
					 g_param_spec_boolean ("as-list", _("Display popup as list"), NULL, 
							       FALSE,
							      (G_PARAM_READABLE | G_PARAM_WRITABLE)));
}

static void
gdaui_combo_selector_init (GdauiDataSelectorInterface *iface)
{
	iface->get_model = combo_selector_get_model;
	iface->set_model = combo_selector_set_model;
	iface->get_selected_rows = combo_selector_get_selected_rows;
	iface->get_data_set = combo_selector_get_data_set;
	iface->select_row = combo_selector_select_row;
	iface->unselect_row = combo_selector_unselect_row;
	iface->set_column_visible = combo_selector_set_column_visible;
}

static void selection_changed_cb (GtkComboBox *widget, gpointer data);

static void
gdaui_combo_init (GdauiCombo *combo)
{
	g_return_if_fail (GDAUI_IS_COMBO (combo));
	GdauiComboPrivate *priv = gdaui_combo_get_instance_private (combo);

	priv->model = NULL;
	priv->store = NULL;
	priv->iter = NULL;

	gtk_combo_box_set_wrap_width (GTK_COMBO_BOX (combo), 0);
	priv->changed_id = g_signal_connect (combo, "changed",
						    G_CALLBACK (selection_changed_cb), NULL);
}

static void
sync_iter_with_selection (GdauiCombo *combo)
{
	g_return_if_fail (GDAUI_IS_COMBO (combo));
	gint selrow = -1;
	GtkTreeIter iter;
	GdauiComboPrivate *priv = gdaui_combo_get_instance_private (combo);

	if (! priv->iter)
		return;

	/* there is at most one selected row */
	if (gtk_combo_box_get_active_iter (GTK_COMBO_BOX (combo), &iter))
		selrow = gdaui_data_store_get_row_from_iter (priv->store, &iter);
	
	/* update iter */
	if ((selrow == -1) || !gda_data_model_iter_move_to_row (priv->iter, selrow)) {
		gda_data_model_iter_invalidate_contents (priv->iter);
		g_object_set (G_OBJECT (priv->iter), "current-row", -1, NULL);
	}
}

static void
selection_changed_cb (GtkComboBox *widget, G_GNUC_UNUSED gpointer data)
{
	sync_iter_with_selection ((GdauiCombo *)widget);
	g_signal_emit_by_name (widget, "selection-changed");
}

static void
gdaui_combo_set_property (GObject *object,
			  guint param_id,
			  const GValue *value,
			  GParamSpec *pspec)
{
	GdauiCombo *combo = (GdauiCombo *) object;

	g_return_if_fail (GDAUI_IS_COMBO (combo));

	switch (param_id) {
	case PROP_MODEL :
		gdaui_combo_set_data (combo,
				       GDA_DATA_MODEL (g_value_get_object (value)),
				       0, NULL);
		break;
	case PROP_AS_LIST: {
		GtkStyleContext *style;
		style = gtk_widget_get_style_context (GTK_WIDGET (combo));
		if (g_value_get_boolean (value)) {
			gtk_style_context_remove_class (style, "gdaui-combo-normal");
			gtk_style_context_add_class (style, "gdaui-combo-as-list");
		}
		else {
			gtk_style_context_remove_class (style, "gdaui-combo-as-list");
			gtk_style_context_add_class (style, "gdaui-combo-normal");
		}
		break;
	}
	default :
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}
}

static void
gdaui_combo_get_property (GObject *object,
			  guint param_id,
			  GValue *value,
			  GParamSpec *pspec)
{
	GdauiCombo *combo = (GdauiCombo *) object;

	g_return_if_fail (GDAUI_IS_COMBO (combo));
	GdauiComboPrivate *priv = gdaui_combo_get_instance_private (combo);

	switch (param_id) {
	case PROP_MODEL :
		g_value_set_object (value, G_OBJECT (priv->model));
		break;
	case PROP_AS_LIST: {
		const gchar *name;
		name = gtk_widget_get_name ((GtkWidget*) combo);
		g_value_set_boolean (value, name && !strcmp (name, "gdaui-combo-as-list") ? TRUE : FALSE);
		break;
	}
	default :
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}
}

static void
gdaui_combo_get_preferred_width (GtkWidget *widget,
				 gint *minimum_size,
				 gint *natural_size)
{
#define MINSIZE 50
	GTK_WIDGET_CLASS (gdaui_combo_parent_class)->get_preferred_width (widget, minimum_size, natural_size);
	if (minimum_size && (*minimum_size > MINSIZE))
		*minimum_size = MINSIZE;
	if (natural_size && (*natural_size > MINSIZE))
		*natural_size = MINSIZE;
}

static void
gdaui_combo_dispose (GObject *object)
{
	GdauiCombo *combo = (GdauiCombo *) object;

	g_return_if_fail (GDAUI_IS_COMBO (combo));
	GdauiComboPrivate *priv = gdaui_combo_get_instance_private (combo);
	/* free objects references */
	if (priv->store) {
		g_signal_handler_disconnect (combo, priv->changed_id);
		if (priv->iter)
			g_object_unref (priv->iter);
		g_object_unref (G_OBJECT (priv->store));
		priv->store = NULL;
	}

	/* chain to parent class */
	G_OBJECT_CLASS (gdaui_combo_parent_class)->dispose (object);
}

static void
gdaui_combo_finalize (GObject *object)
{
	GdauiCombo *combo = (GdauiCombo *) object;

	g_return_if_fail (GDAUI_IS_COMBO (combo));
	GdauiComboPrivate *priv = gdaui_combo_get_instance_private (combo);

	/* free memory */
	if (priv->cols_index)
		g_free (priv->cols_index);
	if (priv->cols_width)
		g_free (priv->cols_width);

	/* chain to parent class */
	G_OBJECT_CLASS (gdaui_combo_parent_class)->finalize (object);
}

/**
 * gdaui_combo_new:
 *
 * Create a new GdauiCombo widget.
 *
 * Returns: (transfer full): the newly-created widget.
 *
 * Since: 4.2
 */
GtkWidget *
gdaui_combo_new ()
{
	GdauiCombo *combo;

	combo = g_object_new (GDAUI_TYPE_COMBO, NULL);

	return GTK_WIDGET (combo);
}

/**
 * gdaui_combo_new_with_model:
 * @model: a #GdaDataModel object.
 * @n_cols: number of columns in the model to be shown
 * @cols_index: an array of columns to be shown, its size must be @n_cols
 *
 * Create a new GdauiCombo widget with a model. See gdaui_combo_set_data() for
 * more information about the @n_cols and @cols_index usage.
 *
 * Returns: (transfer full): the newly-created widget.
 *
 * Since: 4.2
 */
GtkWidget *
gdaui_combo_new_with_model (GdaDataModel *model, gint n_cols, gint *cols_index)
{
	GdauiCombo *combo;
	
	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), NULL);

	combo = GDAUI_COMBO (gdaui_combo_new ()); 
	gdaui_combo_set_data (GDAUI_COMBO (combo), GDA_DATA_MODEL (model), n_cols, cols_index);

	return GTK_WIDGET (combo);
}

static void cell_layout_data_func (GtkCellLayout *cell_layout, GtkCellRenderer *cell,
				   GtkTreeModel *tree_model, GtkTreeIter *iter, GdauiCombo *combo);

/**
 * gdaui_combo_set_data:
 * @combo: a #GdauiCombo widget.
 * @model: a #GdaDataModel object to get data from.
 * @n_cols: number of columns in the model to be shown
 * @cols_index: (array length=n_cols): an array of columns to be shown, its size must be @n_cols
 *
 * Makes @combo display data stored in @model (makes the
 * combo widget refresh its list of values and display the values contained
 * in the model). A NULL @model will make the combo empty
 * and disassociate the previous model, if any.
 *
 * if @n_cols is %0, then all the columns of @model will be displayed in @combo.
 *
 * Since: 5.2
 */
void
gdaui_combo_set_data (GdauiCombo *combo, GdaDataModel *model, gint n_cols, gint *cols_index)
{
	gint ln_cols, model_ncols = -1;
	gint *lcols_index;
	
	g_return_if_fail (GDAUI_IS_COMBO (combo));
	g_return_if_fail (model == NULL || GDA_IS_DATA_MODEL (model));
	GdauiComboPrivate *priv = gdaui_combo_get_instance_private (combo);

	/* reset all */
	if (priv->store) {
		g_object_unref (G_OBJECT (priv->store));
		priv->store = NULL;
		gtk_combo_box_set_model (GTK_COMBO_BOX (combo), NULL);
		gtk_combo_box_set_active (GTK_COMBO_BOX (combo), -1);
	}
	if (priv->model) {
		g_object_unref (priv->model);
		priv->model = NULL;
	}
	if (priv->cols_index) {
		g_free (priv->cols_index);
		priv->cols_index = NULL;
	}
	priv->n_cols = 0;
	gtk_cell_layout_clear (GTK_CELL_LAYOUT (combo));

	if (priv->cols_width) {
		g_free (priv->cols_width);
		priv->cols_width = NULL;
	}

	/* set model */
	if (model) {
		priv->model = model;
		g_object_ref (model);
		
		priv->store = GDAUI_DATA_STORE (gdaui_data_store_new (priv->model));
		gtk_combo_box_set_model (GTK_COMBO_BOX (combo), GTK_TREE_MODEL (priv->store));
		model_ncols = gda_data_model_get_n_columns (model);
		priv->cols_width = g_new (gint, model_ncols);
		gint i;
		for (i = 0; i < model_ncols; i++)
			priv->cols_width [i] = -1;
	}
	
	if (!n_cols && model) {
		gint i;
		ln_cols = model_ncols;
		lcols_index = g_new (gint, ln_cols);
		for (i = 0; i < ln_cols; i++)
			lcols_index [i] = i;	
	}
	else {
		ln_cols = n_cols;
		lcols_index = cols_index;
	}

	/* set columns with cell renderers*/
	if (ln_cols && model) {
		gint i, index;
		GtkCellRenderer *renderer;
		GdaDataHandler *dh;

		/* local copy */
		priv->cols_index = g_new0 (gint, ln_cols);
		priv->n_cols = ln_cols;
		memcpy (priv->cols_index, lcols_index, sizeof (gint) * ln_cols);

		/* compute cell renderers' widths in chars */
		gint j, nrows;
		const GValue *cvalue;
		nrows = gda_data_model_get_n_rows (model);
		for (j = 0; j < nrows; j++) {
			for (i = 0; i < ln_cols; i++) {
				cvalue = gda_data_model_get_value_at (model, priv->cols_index [i], j, NULL);
				if (cvalue && (G_VALUE_TYPE (cvalue) != GDA_TYPE_NULL)) {
					gchar *str;
					gint len;
					dh = gda_data_handler_get_default (G_VALUE_TYPE (cvalue));
					str = gda_data_handler_get_str_from_value (dh, cvalue);
					len = strlen (str);
					g_free (str);
					if (len > priv->cols_width [priv->cols_index [i]])
						priv->cols_width [priv->cols_index [i]] = len;
				}
			}
		}

		/* create cell renderers */
		for (i = 0; i < ln_cols; i++) {
			GdaColumn *column;
			GType type;

			index = priv->cols_index [i];

			column = gda_data_model_describe_column (model, index);
			type = gda_column_get_g_type (column);
			dh = gda_data_handler_get_default (type);
			
			renderer = gtk_cell_renderer_text_new ();
			g_object_set_data (G_OBJECT (renderer), "data-handler", dh);
			g_object_set_data (G_OBJECT (renderer), "colnum", GINT_TO_POINTER (index));
			
			gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo), renderer, FALSE);
			gtk_cell_layout_set_cell_data_func (GTK_CELL_LAYOUT (combo), renderer,
							    (GtkCellLayoutDataFunc) cell_layout_data_func, combo, NULL);
			/* Don't unref the renderer! */
		}
	}
	
	if (!n_cols && model)
		g_free (lcols_index);
}

static void
cell_layout_data_func (G_GNUC_UNUSED GtkCellLayout *cell_layout, GtkCellRenderer *cell,
		       GtkTreeModel *tree_model, GtkTreeIter *iter, G_GNUC_UNUSED GdauiCombo *combo)
{
	GdaDataHandler *dh;
	gint colnum;
	const GValue *value;
	gchar *str;
	
	dh = g_object_get_data (G_OBJECT (cell), "data-handler");
	colnum = GPOINTER_TO_INT (g_object_get_data  (G_OBJECT (cell), "colnum"));

	gtk_tree_model_get (tree_model, iter, colnum, &value, -1);
	str = gda_data_handler_get_str_from_value (dh, value);
	g_object_set (G_OBJECT (cell), "text", str, NULL);
	g_free (str);
}

/*
 * _gdaui_combo_set_selected
 * @combo: a #GdauiCombo widget
 * @values: a list of #GValue
 *
 * Sets the currently selected row of @combo from the values stored in @values. 
 *
 * WARNING: @values must contain one value for each column set to be displayed when the
 * data model was associated to @combo.
 *
 * Returns: TRUE if a row in the model was found to match the list of values.
 */
gboolean
_gdaui_combo_set_selected (GdauiCombo *combo, const GSList *values)
{
	g_return_val_if_fail (GDAUI_IS_COMBO (combo), FALSE);
	GdauiComboPrivate *priv = gdaui_combo_get_instance_private (combo);
	g_return_val_if_fail (priv->cols_index, FALSE);
	g_return_val_if_fail (g_slist_length ((GSList *) values) == (guint)priv->n_cols, FALSE);

	return _gdaui_combo_set_selected_ext (combo, values, priv->cols_index);
}

/*
 * _gdaui_combo_get_selected
 * @combo: a #GdauiCombo widget
 *
 * Get a list of the currently selected values in @combo. The list itself must be free'd using g_slist_free(), 
 * but not the values it contains.
 *
 * WARNING: @values will contain one value for each column set to be displayed when the
 * data model was associated to @combo.
 *
 * Returns: a new list of values, or %NULL if there is no displayed data in @combo.
 *
 * Since: 4.2
 */
GSList *
_gdaui_combo_get_selected (GdauiCombo *combo)
{
	g_return_val_if_fail (GDAUI_IS_COMBO (combo), NULL);
	GdauiComboPrivate *priv = gdaui_combo_get_instance_private (combo);
	if (!priv->store)
		return NULL;
	g_return_val_if_fail (priv->n_cols, NULL);
	g_return_val_if_fail (priv->cols_index, NULL);
	
	return _gdaui_combo_get_selected_ext (combo, priv->n_cols, priv->cols_index);
}

/*
 * _gdaui_combo_set_selected_ext
 * @combo: a #GdauiCombo widget
 * @values: (element-type GObject.Value): a list of #GValue objects
 * @cols_index: (nullable) (array): array of gint, index of column to which each value in @values corresponds, or %NULL
 *
 * Sets the currently selected row of @combo from the values stored in @values, assuming that
 * these values correspond to the columns listed in @cols_index. @cols_index must contain at least as
 * many #gint as there are elements in @values;
 *
 * if @cols_index is %NULL, then it is assumed that @values has the same number of columns 
 * than @combo's data
 * model and that the values in @values are ordered in the same way as the columns of 
 * @combo's data model.
 *
 * Returns: TRUE if a row in the model was found to match the list of values.
 */
gboolean
_gdaui_combo_set_selected_ext (GdauiCombo *combo, const GSList *values, gint *cols_index)
{
	gint row;
	GdaDataProxy *proxy;

	g_return_val_if_fail (GDAUI_IS_COMBO (combo), FALSE);
	GdauiComboPrivate *priv = gdaui_combo_get_instance_private (combo);
	g_return_val_if_fail (priv->store, FALSE);
	g_return_val_if_fail (values, FALSE);

	proxy = gdaui_data_store_get_proxy (priv->store);
	row = gda_data_model_get_row_from_values (GDA_DATA_MODEL (proxy), (GSList *) values, cols_index);
	gtk_combo_box_set_active (GTK_COMBO_BOX (combo), row);

	return (row >= 0) ? TRUE : FALSE;
}

/*
 * _gdaui_combo_get_selected_ext
 * @combo: a #GdauiCombo widget
 * @n_cols: the number of columns for which values are requested
 * @cols_index: (array) (nullable) (array length=n_cols): an array of @n_cols #gint indicating which column to get a value for, or %NULL
 *
 * Get a list of the currently selected values in @combo. The list itself must be free'd using g_slist_free(), 
 * but not the values it contains. If there is no selected value in @combo, then %NULL is returned.
 *
 * if n_cols equals 0 and @cols_index is %NULL, then a #GValue will be returned for each column of @combo's data model.
 *
 * Returns: a new list of values, or %NULL if there is no displayed data in @combo.
 *
 * Since: 4.2
 */
GSList *
_gdaui_combo_get_selected_ext (GdauiCombo *combo, gint n_cols, gint *cols_index)
{
	GtkTreeIter iter;
	GSList *retval = NULL;
	gint index, nbcols;
	GValue *value;

	g_return_val_if_fail (GDAUI_IS_COMBO (combo), NULL);
	GdauiComboPrivate *priv = gdaui_combo_get_instance_private (combo);
	if (! priv->store)
		return NULL;
	if (!n_cols) {
		GdaDataProxy *proxy;

		g_return_val_if_fail (!cols_index, NULL);
		proxy = gdaui_data_store_get_proxy (priv->store);
		nbcols = gda_data_model_get_n_columns ((GdaDataModel *) proxy);
	}
	else {
		g_return_val_if_fail (n_cols > 0, NULL);
		nbcols = n_cols;
	}

	if (gtk_combo_box_get_active_iter (GTK_COMBO_BOX (combo), &iter)) {
		for (index = 0;	index < nbcols; index++) {
			gtk_tree_model_get (GTK_TREE_MODEL (priv->store), &iter,
					    cols_index ? cols_index[index] : index, &value, -1);
			retval = g_slist_append (retval, value);
		}
	}

	return retval;
}

/**
 * gdaui_combo_add_null:
 * @combo: a #GdauiCombo widget
 * @add_null: set to %TRUE to add a NULL value to the combo box
 *
 * Tells if @combo should add a special entry representing an "undefined choice", as a %NULL entry. The default is
 * that only the available choices in @combo's model are presented.
 *
 * Since: 4.2
 */
void
gdaui_combo_add_null (GdauiCombo *combo, gboolean add_null)
{
	g_return_if_fail (GDAUI_IS_COMBO (combo));
	GdauiComboPrivate *priv = gdaui_combo_get_instance_private (combo);

	g_object_set (G_OBJECT (priv->store), "prepend-null-entry", add_null, NULL);
}

/**
 * gdaui_combo_is_null_selected:
 * @combo: a #GdauiCombo widget
 *
 * Tell if the currently selected entry represents the "undefined choice" entry.
 *
 * Returns: %TRUE if the %NULL value is selected
 *
 * Since: 4.2
 */
gboolean
gdaui_combo_is_null_selected (GdauiCombo *combo)
{
	gint active_row;
	gboolean has_undef_choice;

	g_return_val_if_fail (GDAUI_IS_COMBO (combo), FALSE);
	GdauiComboPrivate *priv = gdaui_combo_get_instance_private (combo);

	active_row = gtk_combo_box_get_active (GTK_COMBO_BOX (combo));
	if (active_row == -1)
		return TRUE;
	
	g_object_get (G_OBJECT (priv->store), "prepend-null-entry", &has_undef_choice, NULL);
	if (has_undef_choice && (active_row == 0))
		return TRUE;

	return FALSE;
}

/* GdauiDataSelector interface */
static GdaDataModel *
combo_selector_get_model (GdauiDataSelector *iface)
{
	GdauiCombo *combo;
	combo = GDAUI_COMBO (iface);
	GdauiComboPrivate *priv = gdaui_combo_get_instance_private (combo);
	return priv->model;
}

static void
combo_selector_set_model (GdauiDataSelector *iface, GdaDataModel *model)
{
	g_object_set (G_OBJECT (iface), "model", model, NULL);
}

static GArray *
combo_selector_get_selected_rows (GdauiDataSelector *iface)
{
	GtkTreeIter iter;
	GArray *retval = NULL;
	GdauiCombo *combo;

	combo = GDAUI_COMBO (iface);
	GdauiComboPrivate *priv = gdaui_combo_get_instance_private (combo);

	/* there is at most one selected row */
	if (gtk_combo_box_get_active_iter (GTK_COMBO_BOX (combo), &iter)) {
		gint row;
		row = gdaui_data_store_get_row_from_iter (priv->store, &iter);
		if (row >=0) {
			if (!retval)
				retval = g_array_new (FALSE, FALSE, sizeof (gint));
			g_array_append_val (retval, row);
		}
	}

	return retval;
}

static GdaDataModelIter *
combo_selector_get_data_set (GdauiDataSelector *iface)
{
	GdauiCombo *combo;

	combo = GDAUI_COMBO (iface);
	GdauiComboPrivate *priv = gdaui_combo_get_instance_private (combo);
	if (! priv->iter && priv->model) {
		GdaDataModel *proxy;
		proxy = (GdaDataModel*) gdaui_data_store_get_proxy (priv->store);
		priv->iter = gda_data_model_create_iter (proxy);
		sync_iter_with_selection (combo);
	}

	return priv->iter;
}

static gboolean
combo_selector_select_row (GdauiDataSelector *iface, gint row)
{
	GdauiCombo *combo;
	GtkTreeIter iter;

	combo = GDAUI_COMBO (iface);
	GdauiComboPrivate *priv = gdaui_combo_get_instance_private (combo);
	gtk_combo_box_set_active (GTK_COMBO_BOX (combo), row);
	if (gtk_combo_box_get_active_iter (GTK_COMBO_BOX (combo), &iter)) {
		gint srow;
		srow = gdaui_data_store_get_row_from_iter (priv->store, &iter);
		if (srow == row)
			return TRUE;
	}
	return FALSE;
}

static void
combo_selector_unselect_row (GdauiDataSelector *iface, gint row)
{
	GdauiCombo *combo;
	GtkTreeIter iter;

	combo = GDAUI_COMBO (iface);
	GdauiComboPrivate *priv = gdaui_combo_get_instance_private (combo);
	if (gtk_combo_box_get_active_iter (GTK_COMBO_BOX (combo), &iter)) {
		gint srow;
		srow = gdaui_data_store_get_row_from_iter (priv->store, &iter);
		if (srow == row)
			gtk_combo_box_set_active (GTK_COMBO_BOX (combo), -1);
	}
}

static void
combo_selector_set_column_visible (GdauiDataSelector *iface, gint column, gboolean visible)
{
	GdauiCombo *combo;
	combo = GDAUI_COMBO (iface);
	GdauiComboPrivate *priv = gdaui_combo_get_instance_private (combo);

	/* analyse existing columns */
	GList *cells, *list;
	gint cellpos;
	cells = gtk_cell_layout_get_cells (GTK_CELL_LAYOUT (iface));
	for (cellpos = 0, list = cells;
	     list;
	     cellpos ++, list = list->next) {
		gint col;
		col = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (list->data), "colnum"));
		if (col == column) {
			g_object_set (G_OBJECT (list->data), "visible", visible, NULL);
			g_list_free (cells);
			return;
		}
		else if (col > column)
			break;
	}
	g_list_free (cells);

	/* column does not exist at this point */
	if (!visible || !priv->model)
		return;
	if ((column < 0) || (column >= gda_data_model_get_n_columns (priv->model))) {
		g_warning (_("Column %d out of range (0-%d)"), column, gda_data_model_get_n_columns (priv->model)-1);
		return;
	}

	GdaDataHandler *dh;
	if (priv->cols_width[column] == -1) {
		/* compute column width in chars */
		gint j, nrows;
		const GValue *cvalue;
		nrows = gda_data_model_get_n_rows (priv->model);
		for (j = 0; j < nrows; j++) {
			cvalue = gda_data_model_get_value_at (priv->model, column, j, NULL);
			if (cvalue && (G_VALUE_TYPE (cvalue) != GDA_TYPE_NULL)) {
				gchar *str;
				gint len;
				dh = gda_data_handler_get_default (G_VALUE_TYPE (cvalue));
				str = gda_data_handler_get_str_from_value (dh, cvalue);
				len = strlen (str);
				g_free (str);
				if (len > priv->cols_width [column])
					priv->cols_width [column] = len;
			}
		}
	}

	/* create cell renderer */
	GdaColumn *mcolumn;
	GType type;
	GtkCellRenderer *renderer;

	mcolumn = gda_data_model_describe_column (priv->model, column);
	type = gda_column_get_g_type (mcolumn);
	dh = gda_data_handler_get_default (type);
	
	renderer = gtk_cell_renderer_text_new ();
	g_object_set_data (G_OBJECT (renderer), "data-handler", dh);
	g_object_set_data (G_OBJECT (renderer), "colnum", GINT_TO_POINTER (column));
	
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo), renderer, FALSE);
	gtk_cell_layout_set_cell_data_func (GTK_CELL_LAYOUT (combo), renderer,
					    (GtkCellLayoutDataFunc) cell_layout_data_func, combo, NULL);
	gtk_cell_layout_reorder (GTK_CELL_LAYOUT (combo), renderer, cellpos);
	/* Don't unref the renderer! */

	gint ww;
	g_object_get ((GObject*) iface, "wrap-width", &ww, NULL);
	g_object_set ((GObject*) iface, "wrap-width", 1, NULL);
	g_object_set ((GObject*) iface, "wrap-width", ww, NULL);
}
