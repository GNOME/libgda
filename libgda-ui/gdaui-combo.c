/* GNOME DB library
 *
 * Copyright (C) 1999 - 2009 The Free Software Foundation
 *
 * AUTHORS:
 *      Rodrigo Moya <rodrigo@gnome-db.org>
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

#include <string.h>
#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>
#include "gdaui-combo.h"
#include "gdaui-data-store.h"

struct _GdauiComboPrivate {
	GdaDataModel     *model; /* proxied model (the one when _set_model() is called) */
	GdauiDataStore   *store; /* model proxy */

	/* columns of the model to display */
	gint              n_cols;
	gint             *cols_index;
};

static void gdaui_combo_class_init   (GdauiComboClass *klass);
static void gdaui_combo_init         (GdauiCombo *combo,
					 GdauiComboClass *klass);
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

enum {
	PROP_0,
	PROP_MODEL
};

/* get a pointer to the parents to be able to call their destructor */
static GObjectClass *parent_class = NULL;

/*
 * GdauiCombo class implementation
 */

GType
gdaui_combo_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo info = {
			sizeof (GdauiComboClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gdaui_combo_class_init,
			NULL,
			NULL,
			sizeof (GdauiCombo),
			0,
			(GInstanceInitFunc) gdaui_combo_init
		};
		type = g_type_register_static (GTK_TYPE_COMBO_BOX, "GdauiCombo", &info, 0);
	}
	return type;
}

static void
gdaui_combo_class_init (GdauiComboClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->set_property = gdaui_combo_set_property;
	object_class->get_property = gdaui_combo_get_property;
	object_class->dispose = gdaui_combo_dispose;
	object_class->finalize = gdaui_combo_finalize;

	/* add class properties */
	g_object_class_install_property (object_class, PROP_MODEL,
					 g_param_spec_object ("model", _("The data model to display"), NULL, 
							      GDA_TYPE_DATA_MODEL,
							      (G_PARAM_READABLE | G_PARAM_WRITABLE)));
}

static void
gdaui_combo_init (GdauiCombo *combo, GdauiComboClass *klass)
{
	g_return_if_fail (GDAUI_IS_COMBO (combo));

	/* allocate private structure */
	combo->priv = g_new0 (GdauiComboPrivate, 1);
	combo->priv->model = NULL;
	combo->priv->store = NULL;

	gtk_combo_box_set_wrap_width (GTK_COMBO_BOX (combo), 0);
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
		gdaui_combo_set_model (combo,
					  GDA_DATA_MODEL (g_value_get_object (value)),
					  0, NULL);
		break;
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

	switch (param_id) {
	case PROP_MODEL :
		g_value_set_object (value, G_OBJECT (combo->priv->model));
		break;
	default :
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}
}

static void
gdaui_combo_dispose (GObject *object)
{
	GdauiCombo *combo = (GdauiCombo *) object;

	g_return_if_fail (GDAUI_IS_COMBO (combo));

	/* free objects references */
	if (combo->priv->store) {
		g_object_unref (G_OBJECT (combo->priv->store));
		combo->priv->store = NULL;
	}

	/* chain to parent class */
	parent_class->dispose (object);
}

static void
gdaui_combo_finalize (GObject *object)
{
	GdauiCombo *combo = (GdauiCombo *) object;

	g_return_if_fail (GDAUI_IS_COMBO (combo));

	/* free memory */
	if (combo->priv->cols_index)
		g_free (combo->priv->cols_index);

	g_free (combo->priv);
	combo->priv = NULL;

	/* chain to parent class */
	parent_class->finalize (object);
}

/**
 * gdaui_combo_new
 *
 * Create a new GdauiCombo widget.
 *
 * Returns: the newly-created widget.
 */
GtkWidget *
gdaui_combo_new ()
{
	GdauiCombo *combo;

	combo = g_object_new (GDAUI_TYPE_COMBO, NULL);

	return GTK_WIDGET (combo);
}

/**
 * gdaui_combo_new_with_model
 * @model: a #GdaDataModel object.
 * @n_cols: number of columns in the model to be shown
 * @cols_index: index of each column to be shown
 *
 * Create a new GdauiCombo widget with a model. See gdaui_combo_set_model() for
 * more information about the @n_cols and @cols_index usage.
 *
 * Returns: the newly-created widget.
 */
GtkWidget *
gdaui_combo_new_with_model (GdaDataModel *model, gint n_cols, gint *cols_index)
{
	GdauiCombo *combo;
	
	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), NULL);

	combo = GDAUI_COMBO (gdaui_combo_new ()); 
	gdaui_combo_set_model (GDAUI_COMBO (combo), GDA_DATA_MODEL (model), n_cols, cols_index);

	return GTK_WIDGET (combo);
}

static void cell_layout_data_func (GtkCellLayout *cell_layout, GtkCellRenderer *cell,
				   GtkTreeModel *tree_model, GtkTreeIter *iter, GdauiCombo *combo);

/**
 * gdaui_combo_set_model
 * @combo: a #GdauiCombo widget.
 * @model: a #GdaDataModel object.
 * @n_cols: number of columns in the model to be shown
 * @cols_index: index of each column to be shown
 *
 * Makes @combo display data stored in @model (makes the
 * combo widget refresh its list of values and display the values contained
 * in the model). A NULL @model will make the combo empty
 * and disassociate the previous model, if any.
 *
 * if @n_cols is 0, then all the columns of @model will be displayed in @combo.
 */
void
gdaui_combo_set_model (GdauiCombo *combo, GdaDataModel *model, gint n_cols, gint *cols_index)
{
	gint ln_cols;
	gint *lcols_index;
	
	g_return_if_fail (GDAUI_IS_COMBO (combo));
	g_return_if_fail (model == NULL || GDA_IS_DATA_MODEL (model));

	/* reset all */
	if (combo->priv->store) {
		g_object_unref (G_OBJECT (combo->priv->store));
		combo->priv->store = NULL;
	}
	if (combo->priv->model) {
		g_object_unref (combo->priv->model);
		combo->priv->model = NULL;
	}
	if (combo->priv->cols_index) {
		g_free (combo->priv->cols_index);
		combo->priv->cols_index = NULL;
	}
	combo->priv->n_cols = 0;
	gtk_cell_layout_clear (GTK_CELL_LAYOUT (combo));

	/* set model */
	if (model) {
		combo->priv->model = model;
		g_object_ref (model);
		
		combo->priv->store = GDAUI_DATA_STORE (gdaui_data_store_new (combo->priv->model));
		gtk_combo_box_set_model (GTK_COMBO_BOX (combo), GTK_TREE_MODEL (combo->priv->store));
	} 
	
	if (!n_cols && model) {
		gint i;
		ln_cols = gda_data_model_get_n_columns (model);
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
		combo->priv->cols_index = g_new0 (gint, ln_cols);
		combo->priv->n_cols = ln_cols;
		memcpy (combo->priv->cols_index, lcols_index, sizeof (gint) * ln_cols);

		/* create cell renderers */
		for (i=0; i<ln_cols; i++) {
			GdaColumn *column;
			GType type;

			index = combo->priv->cols_index [i];

			column = gda_data_model_describe_column (model, index);
			type = gda_column_get_g_type (column);
			dh = gda_get_default_handler (type);
			
			renderer = gtk_cell_renderer_text_new ();
			g_object_set_data (G_OBJECT (renderer), "data_handler", dh);
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
cell_layout_data_func (GtkCellLayout *cell_layout, GtkCellRenderer *cell,
		       GtkTreeModel *tree_model, GtkTreeIter *iter, GdauiCombo *combo)
{
	GdaDataHandler *dh;
	gint colnum;
	const GValue *value;
	gchar *str;
	
	dh = g_object_get_data (G_OBJECT (cell), "data_handler");
	colnum = GPOINTER_TO_INT (g_object_get_data  (G_OBJECT (cell), "colnum"));

	gtk_tree_model_get (tree_model, iter, colnum, &value, -1);
	str = gda_data_handler_get_str_from_value (dh, value);
	g_object_set (G_OBJECT (cell), "text", str, NULL);
	g_free (str);
}

/**
 * gdaui_combo_get_model
 * @combo: a #GdauiCombo widget.
 *
 * This function returns the #GdaDataModel from which @combo displays values.
 *
 * Returns: a #GdaDataModel containing the data from the #GdauiCombo widget.
 */
GdaDataModel *
gdaui_combo_get_model (GdauiCombo *combo)
{
	g_return_val_if_fail (GDAUI_IS_COMBO (combo), NULL);
	g_return_val_if_fail (combo->priv, NULL);

	if (GDA_IS_DATA_MODEL (combo->priv->model)) 
		return GDA_DATA_MODEL (combo->priv->model);
	return NULL;
}

/**
 * gdaui_combo_set_values
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
gdaui_combo_set_values (GdauiCombo *combo, const GSList *values)
{
	g_return_val_if_fail (GDAUI_IS_COMBO (combo), FALSE);
	g_return_val_if_fail (combo->priv, FALSE);
	g_return_val_if_fail (combo->priv->cols_index, FALSE);
	g_return_val_if_fail (g_slist_length ((GSList *) values) == combo->priv->n_cols, FALSE);

	return gdaui_combo_set_values_ext (combo, values, combo->priv->cols_index);
}

/**
 * gdaui_combo_get_values
 * @combo: a #GdauiCombo widget
 *
 * Get a list of the currently selected values in @combo. The list itself must be free'd using g_slist_free(), 
 * but not the values it contains.
 *
 * WARNING: @values will contain one value for each column set to be displayed when the
 * data model was associated to @combo.
 *
 * Returns: a new list of values, or %NULL if there is no displayed data in @combo.
 */
GSList *
gdaui_combo_get_values (GdauiCombo *combo)
{
	g_return_val_if_fail (GDAUI_IS_COMBO (combo), NULL);
	g_return_val_if_fail (combo->priv, NULL);
	if (!combo->priv->store)
		return NULL;
	g_return_val_if_fail (combo->priv->n_cols, NULL);
	g_return_val_if_fail (combo->priv->cols_index, NULL);
	
	return gdaui_combo_get_values_ext (combo, combo->priv->n_cols, combo->priv->cols_index);
}

/**
 * gdaui_combo_set_values_ext
 * @combo: a #GdauiCombo widget
 * @values: a list of #GValue objects
 * @cols_index: array of gint, index of column to which each value in @values corresponds, or %NULL
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
gdaui_combo_set_values_ext (GdauiCombo *combo, const GSList *values, gint *cols_index)
{
	gint row;
	GdaDataProxy *proxy;

	g_return_val_if_fail (GDAUI_IS_COMBO (combo), FALSE);
	g_return_val_if_fail (combo->priv, FALSE);
	g_return_val_if_fail (combo->priv->store, FALSE);
	g_return_val_if_fail (values, FALSE);

	proxy = gdaui_data_store_get_proxy (combo->priv->store);
	row = gda_data_model_get_row_from_values (GDA_DATA_MODEL (proxy), (GSList *) values, cols_index);
	gtk_combo_box_set_active (GTK_COMBO_BOX (combo), row);

	return (row >= 0) ? TRUE : FALSE;
}

/**
 * gdaui_combo_get_values_ext
 * @combo: a #GdauiCombo widget
 * @n_cols: the number of columns for which values are requested
 * @cols_index: an array of @n_cols #gint indicating which column to get a value for, or %NULL
 *
 * Get a list of the currently selected values in @combo. The list itself must be free'd using g_slist_free(), 
 * but not the values it contains. If there is no selected value in @combo, then %NULL is returned.
 *
 * if n_cols equals 0 and @cols_index is %NULL, then a #GValue will be returned for each column of @combo's data model.
 *
 * Returns: a new list of values, or %NULL if there is no displayed data in @combo.
 */
GSList *
gdaui_combo_get_values_ext (GdauiCombo *combo, gint n_cols, gint *cols_index)
{
	GtkTreeIter iter;
	GSList *retval = NULL;
	gint index, nbcols;
	GValue *value;

	g_return_val_if_fail (GDAUI_IS_COMBO (combo), NULL);
	g_return_val_if_fail (combo->priv, NULL);
	if (! combo->priv->store)
		return NULL;
	if (!n_cols) {
		GdaDataProxy *proxy;

		g_return_val_if_fail (!cols_index, NULL);
		proxy = gdaui_data_store_get_proxy (combo->priv->store);
		nbcols = gda_data_model_get_n_columns ((GdaDataModel *) proxy);
	}
	else {
		g_return_val_if_fail (n_cols > 0, NULL);
		nbcols = n_cols;
	}

	if (gtk_combo_box_get_active_iter (GTK_COMBO_BOX (combo), &iter)) {
		for (index = 0;	index < nbcols; index++) {
			gtk_tree_model_get (GTK_TREE_MODEL (combo->priv->store), &iter, 
					    cols_index ? cols_index[index] : index, &value, -1);
			retval = g_slist_append (retval, value);
		}
	}

	return retval;
}

/**
 * gdaui_combo_add_undef_choice
 * @combo: a #GdauiCombo widget
 * @add_undef_choice:
 *
 * Tells if @combo should add a special entry representing an "undefined choice". The default is
 * that only the available choices in @combo's model are presented.
 */
void
gdaui_combo_add_undef_choice (GdauiCombo *combo, gboolean add_undef_choice)
{
	g_return_if_fail (GDAUI_IS_COMBO (combo));
	g_return_if_fail (combo->priv);

	g_object_set (G_OBJECT (combo->priv->store), "prepend_null_entry", add_undef_choice, NULL);
}

/**
 * gdaui_combo_undef_selected
 * @combo: a #GdauiCombo widget
 *
 * Tell if the currently selected entry represents the "undefined choice" entry.
 *
 * Returns:
 */
gboolean
gdaui_combo_undef_selected (GdauiCombo *combo)
{
	gint active_row;
	gboolean has_undef_choice;

	g_return_val_if_fail (GDAUI_IS_COMBO (combo), FALSE);
	g_return_val_if_fail (combo->priv, FALSE);

	active_row = gtk_combo_box_get_active (GTK_COMBO_BOX (combo));
	if (active_row == -1)
		return TRUE;
	
	g_object_get (G_OBJECT (combo->priv->store), "prepend_null_entry", &has_undef_choice, NULL);
	if (has_undef_choice && (active_row == 0))
		return TRUE;

	return FALSE;
}
