/* GDA common library
 * Copyright (C) 1998-2002 The GNOME Foundation.
 *
 * AUTHORS:
 *	Rodrigo Moya <rodrigo@gnome-db.org>
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

#include <libgda/gda-select.h>

#define PARENT_TYPE GDA_TYPE_DATA_MODEL_ARRAY

struct _GdaSelectPrivate {
	GdaDataModel *source_model;
	gchar *expression;
};

static void gda_select_class_init (GdaSelectClass *klass);
static void gda_select_init       (GdaSelect *sel, GdaSelectClass *klass);
static void gda_select_finalize   (GObject *object);

static GObjectClass *parent_class = NULL;

/*
 * Private functions
 */

static void
fill_data (GdaSelect *sel)
{
}

/*
 * GdaSelect class implementation
 */

static GdaFieldAttributes *
gda_select_describe_column (GdaDataModel *model, gint col)
{
	GdaSelect *sel = (GdaSelect *) model;

	g_return_val_if_fail (GDA_IS_SELECT (sel), NULL);
	g_return_val_if_fail (GDA_IS_DATA_MODEL (sel->priv->source_model), NULL);

	return gda_data_model_describe_column (sel->priv->source_model, col);
}

static gboolean
gda_select_is_editable (GdaDataModel *model)
{
	GdaSelect *sel = (GdaSelect *) model;

	g_return_val_if_fail (GDA_IS_SELECT (sel), FALSE);
	g_return_val_if_fail (GDA_IS_DATA_MODEL (sel->priv->source_model), FALSE);

	return gda_data_model_is_editable (sel->priv->source_model);
}

static GdaRow *
gda_select_append_row (GdaDataModel *model, const GList *values)
{
	GdaRow *row;
	GdaSelect *sel = (GdaSelect *) model;

	g_return_val_if_fail (GDA_IS_SELECT (sel), NULL);
	g_return_val_if_fail (GDA_IS_DATA_MODEL (sel->priv->source_model), NULL);

	/* add the row to the source data model */
	row = gda_data_model_append_row (sel->priv->source_model, values);
	if (!row)
		return NULL;

	/* re-fill the select object from the source data model */
	fill_data (sel);
	
	return row;
}

static void
gda_select_class_init (GdaSelectClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GdaDataModelClass *model_class = GDA_DATA_MODEL_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = gda_select_finalize;
	// we use the get_n_rows and get_n_columns of the base class
	model_class->describe_column = gda_select_describe_column;
	// we use the get_value_at of the base class
	model_class->is_editable = gda_select_is_editable;
	model_class->append_row = gda_select_append_row;
}

static void
gda_select_init (GdaSelect *sel, GdaSelectClass *klass)
{
	g_return_if_fail (GDA_IS_SELECT (sel));

	/* allocate internal structure */
	sel->priv = g_new0 (GdaSelectPrivate, 1);
	sel->priv->source_model = NULL;
	sel->priv->expression = NULL;
}

static void
gda_select_finalize (GObject *object)
{
	GdaSelect *sel = (GdaSelect *) object;

	g_return_if_fail (GDA_IS_SELECT (sel));

	/* free memory */
	if (sel->priv->source_model) {
		g_object_unref (G_OBJECT (sel->priv->source_model));
		sel->priv->source_model = NULL;
	}

	if (sel->priv->expression) {
		g_free (sel->priv->expression);
		sel->priv->expression = NULL;
	}

	g_free (sel->priv);
	sel->priv = NULL;

	/* chain to parent class */
	parent_class->finalize (object);
}

GType
gda_select_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (GdaSelectClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_select_class_init,
			NULL,
			NULL,
			sizeof (GdaSelect),
			0,
			(GInstanceInitFunc) gda_select_init
		};
		type = g_type_register_static (PARENT_TYPE, "GdaSelect", &info, 0);
	}
	return type;
}

/**
 * gda_select_new
 *
 * Create a new #GdaSelect object, which allows programs to filter
 * #GdaDataModel's based on a given expression.
 *
 * A #GdaSelect is just another #GdaDataModel-based class, so it
 * can be used in the same way any other data model class is.
 *
 * Returns: the newly created object.
 */
GdaDataModel *
gda_select_new (void)
{
	GdaDataModel *model;

	model = g_object_new (GDA_TYPE_SELECT, NULL);
	return model;
}
