/* GDA library
 * Copyright (C) 1998 - 2005 The GNOME Foundation.
 *
 * AUTHORS:
 *	Rodrigo Moya <rodrigo@gnome-db.org>
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
#include <glib/garray.h>
#include <libgda/gda-data-model-array.h>
#include <glib/gi18n-lib.h>
#include <libgda/gda-data-model-private.h>

#define PARENT_TYPE GDA_TYPE_DATA_MODEL_BASE

struct _GdaDataModelArrayPrivate {
	/* number of columns in each row */
	gint number_of_columns;

	/* the array of rows, each item is a GdaRow */
	GPtrArray *rows;
};

static void gda_data_model_array_class_init (GdaDataModelArrayClass *klass);
static void gda_data_model_array_init       (GdaDataModelArray *model,
					     GdaDataModelArrayClass *klass);
static void gda_data_model_array_finalize   (GObject *object);

static GObjectClass *parent_class = NULL;

/*
 * GdaDataModelArray class implementation
 */

static gint
gda_data_model_array_get_n_rows (GdaDataModelBase *model)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL_ARRAY (model), -1);
	return GDA_DATA_MODEL_ARRAY (model)->priv->rows->len;
}

static gint
gda_data_model_array_get_n_columns (GdaDataModelBase *model)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL_ARRAY (model), -1);
	return GDA_DATA_MODEL_ARRAY (model)->priv->number_of_columns;
}

static const GdaRow *
gda_data_model_array_get_row (GdaDataModelBase *model, gint row)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL_ARRAY (model), NULL);

	if (row >= GDA_DATA_MODEL_ARRAY (model)->priv->rows->len)
		return NULL;

	return (const GdaRow *) g_ptr_array_index (GDA_DATA_MODEL_ARRAY (model)->priv->rows, row);
}

static const GdaValue *
gda_data_model_array_get_value_at (GdaDataModelBase *model, gint col, gint row)
{
	GdaRow *fields;

	g_return_val_if_fail (GDA_IS_DATA_MODEL_ARRAY (model), NULL);

	if (row >= GDA_DATA_MODEL_ARRAY (model)->priv->rows->len)
		return NULL;

	fields = g_ptr_array_index (GDA_DATA_MODEL_ARRAY (model)->priv->rows, row);
	if (fields != NULL) {
		GdaValue *field;

		field = gda_row_get_value (fields, col);
		return (const GdaValue *) field;
	}

	return NULL;
}

static gboolean
gda_data_model_array_is_updatable (GdaDataModelBase *model)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL_ARRAY (model), FALSE);
	return TRUE;
}

static const GdaRow *
gda_data_model_array_append_values (GdaDataModelBase *model, const GList *values)
{
	gint len;
	GdaRow *row = NULL;

	g_return_val_if_fail (GDA_IS_DATA_MODEL_ARRAY (model), NULL);
	g_return_val_if_fail (values != NULL, NULL);

	len = g_list_length ((GList *) values);
	g_return_val_if_fail (len == GDA_DATA_MODEL_ARRAY (model)->priv->number_of_columns, NULL);

	row = gda_row_new_from_list (GDA_DATA_MODEL (model), values);
	if (row) {
		g_ptr_array_add (GDA_DATA_MODEL_ARRAY (model)->priv->rows, row);
		gda_row_set_number (row, GDA_DATA_MODEL_ARRAY (model)->priv->rows->len - 1);
		gda_data_model_row_inserted (GDA_DATA_MODEL (model), 
					     GDA_DATA_MODEL_ARRAY (model)->priv->rows->len - 1);
	}

	return (const GdaRow *) row;
}

static gboolean
gda_data_model_array_append_row (GdaDataModelBase *model, GdaRow *row)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL_ARRAY (model), FALSE);
	g_return_val_if_fail (row != NULL, FALSE);

	g_ptr_array_add (GDA_DATA_MODEL_ARRAY (model)->priv->rows, (GdaRow *) row);
	gda_row_set_number ((GdaRow *) row, GDA_DATA_MODEL_ARRAY (model)->priv->rows->len - 1);
	gda_data_model_row_inserted (GDA_DATA_MODEL (model), 
				     GDA_DATA_MODEL_ARRAY (model)->priv->rows->len - 1);

	return TRUE;
}

static gboolean
gda_data_model_array_remove_row (GdaDataModelBase *model, const GdaRow *row)
{
	gint i, rownum;
	
	g_return_val_if_fail (GDA_IS_DATA_MODEL_ARRAY (model), FALSE);
	g_return_val_if_fail (row != NULL, FALSE);

	if (g_ptr_array_remove (GDA_DATA_MODEL_ARRAY (model)->priv->rows, (gpointer) row)) {
		/* renumber following rows */
		rownum = gda_row_get_number ((GdaRow *) row);
		for (i = (rownum + 1); i < GDA_DATA_MODEL_ARRAY (model)->priv->rows->len; i++)
			gda_row_set_number ((GdaRow *) gda_data_model_get_row (GDA_DATA_MODEL (model), i), (i-1));

		/* tag the row as being removed */
		gda_row_set_id ((GdaRow *) row, "R");
		gda_row_set_number ((GdaRow *) row, -1);

		gda_data_model_row_removed (GDA_DATA_MODEL (model), rownum);
		return TRUE;
	}

	return FALSE;
}

static gboolean
gda_data_model_array_update_row (GdaDataModelBase *model, const GdaRow *row)
{
	gint i;
	GdaDataModelArrayPrivate *priv;

	g_return_val_if_fail (GDA_IS_DATA_MODEL_ARRAY (model), FALSE);
	g_return_val_if_fail (row != NULL, FALSE);

	priv = GDA_DATA_MODEL_ARRAY (model)->priv;

	for (i = 0; i < priv->rows->len; i++) {
		if (priv->rows->pdata[i] == row) {
			gda_row_free (priv->rows->pdata[i]);
			priv->rows->pdata[i] = gda_row_copy ((GdaRow *) row);
			gda_data_model_row_updated (GDA_DATA_MODEL (model), i);

			return TRUE;
		}
	}

	return FALSE; /* row not found in this data model */
}

static gboolean
gda_data_model_array_append_column (GdaDataModelBase *model, const GdaColumn *attrs)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL_ARRAY (model), FALSE);
	g_return_val_if_fail (attrs != NULL, FALSE);

	return FALSE;
}

static gboolean
gda_data_model_array_update_column (GdaDataModelBase *model, gint col, const GdaColumn *attrs)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL_ARRAY (model), FALSE);
	g_return_val_if_fail (attrs != NULL, FALSE);

	return FALSE;
}

static gboolean
gda_data_model_array_remove_column (GdaDataModelBase *model, gint col)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL_ARRAY (model), FALSE);

	return FALSE;
}

static void
gda_data_model_array_class_init (GdaDataModelArrayClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GdaDataModelBaseClass *model_class = GDA_DATA_MODEL_BASE_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = gda_data_model_array_finalize;
	model_class->get_n_rows = gda_data_model_array_get_n_rows;
	model_class->get_n_columns = gda_data_model_array_get_n_columns;
	model_class->get_row = gda_data_model_array_get_row;
	model_class->get_value_at = gda_data_model_array_get_value_at;
	model_class->is_updatable = gda_data_model_array_is_updatable;
	model_class->append_values = gda_data_model_array_append_values;
	model_class->append_row = gda_data_model_array_append_row;
	model_class->remove_row = gda_data_model_array_remove_row;
	model_class->update_row = gda_data_model_array_update_row;
	model_class->append_column = gda_data_model_array_append_column;
	model_class->update_column = gda_data_model_array_update_column;
	model_class->remove_column = gda_data_model_array_remove_column;
}

static void
gda_data_model_array_init (GdaDataModelArray *model, GdaDataModelArrayClass *klass)
{
	g_return_if_fail (GDA_IS_DATA_MODEL_ARRAY (model));

	/* allocate internal structure */
	model->priv = g_new0 (GdaDataModelArrayPrivate, 1);
	model->priv->number_of_columns = 0;
	model->priv->rows = g_ptr_array_new ();
}

static void
gda_data_model_array_finalize (GObject *object)
{
	GdaDataModelArray *model = (GdaDataModelArray *) object;

	g_return_if_fail (GDA_IS_DATA_MODEL_ARRAY (model));

	/* free memory */
	gda_data_model_array_clear (model);
	g_ptr_array_free (model->priv->rows, TRUE);

	g_free (model->priv);
	model->priv = NULL;

	/* chain to parent class */
	parent_class->finalize (object);
}

GType
gda_data_model_array_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (GdaDataModelArrayClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_data_model_array_class_init,
			NULL,
			NULL,
			sizeof (GdaDataModelArray),
			0,
			(GInstanceInitFunc) gda_data_model_array_init
		};
		type = g_type_register_static (PARENT_TYPE, "GdaDataModelArray", &info, 0);
	}
	return type;
}

/**
 * gda_data_model_array_new
 * @cols: number of columns for rows in this data model.
 *
 * Returns: a pointer to the newly created #GdaDataModel.
 */
GdaDataModel *
gda_data_model_array_new (gint cols)
{
	GdaDataModel *model;

	model = g_object_new (GDA_TYPE_DATA_MODEL_ARRAY, NULL);
	gda_data_model_array_set_n_columns (GDA_DATA_MODEL_ARRAY (model), cols);
	return model;
}

typedef struct {
	gchar        *name;
	gchar        *title;
	gchar        *caption;
	gchar        *dbms_type;
	GdaValueType  gdatype;
	gint          size;
	gint          scale;
	gboolean      pkey;
	gboolean      unique;
	gboolean      nullok;
	gboolean      autoinc;
} XmlColumnSpec;

static void
clean_field_specs (GSList *fields)
{
	GSList *list;
	XmlColumnSpec *spec;

	list = fields;
	while (list) {
		spec = (XmlColumnSpec*)(list->data);
		if (spec->name)
			g_free (spec->name);
		if (spec->title)
			g_free (spec->title);
		if (spec->caption)
			g_free (spec->caption);
		if (spec->dbms_type)
			g_free (spec->dbms_type);
		g_free (spec);

		list = g_slist_next (list);
	}
	g_slist_free (fields);
}

/**
 * gda_data_model_array_new_from_xml_node
 * @node: an XML node representing a &lt;data-array&gt; tag
 * @error: a place to store errors or %NULL
 *
 * Creates a new #GdaDataModel with the data stored in @node
 *
 * Returns: a pointer to the newly created #GdaDataModel, or %NULL if an error occured
 */
GdaDataModel *
gda_data_model_array_new_from_xml_node (xmlNodePtr node, GError **error)
{
	GdaDataModel *model;
	xmlNodePtr cur;
	gint nbfields = 0;
	GSList *fields = NULL;
	GSList *list;
	gint pos;

	g_return_val_if_fail (node, NULL);

	if (strcmp (node->name, "data-array")) {
		g_set_error (error, 0, 0, _("Node is not <data-array>: '%s'"), node->name);
		return NULL;
	}

	for (cur = node->children; cur; cur=cur->next) {
		if (!strcmp (cur->name, "field")) {
			gchar *str;
			XmlColumnSpec *spec;

			spec = g_new0 (XmlColumnSpec, 1);
			fields = g_slist_append (fields, spec);

			spec->title = xmlGetProp (cur, "name");
			if (!spec->title)
				spec->title = g_strdup_printf ("field_%d", nbfields);
			spec->title = xmlGetProp (cur, "title");
			spec->caption = xmlGetProp (cur, "caption");
			spec->dbms_type = xmlGetProp (cur, "dbms_type");
			str = xmlGetProp (cur, "gdatype");
			if (str) {
				spec->gdatype = gda_type_from_string (str);
				g_free (str);
			}
			else {
				g_set_error (error, 0, 0, _("No \"gdatype\" attribute specified in <field>"));
				clean_field_specs (fields);
				return NULL;
			}
			str = xmlGetProp (cur, "size");
			if (str) {
				spec->size = atoi (str);
				g_free (str);
			}
			str = xmlGetProp (cur, "scale");
			if (str) {
				spec->scale = atoi (str);
				g_free (str);
			}
			str = xmlGetProp (cur, "pkey");
			if (str) {
				spec->pkey = ((*str == 't') || (*str == 'T')) ? TRUE : FALSE;
				g_free (str);
			}
			str = xmlGetProp (cur, "unique");
			if (str) {
				spec->unique = ((*str == 't') || (*str == 'T')) ? TRUE : FALSE;
				g_free (str);
			}
			str = xmlGetProp (cur, "nullok");
			if (str) {
				spec->nullok = ((*str == 't') || (*str == 'T')) ? TRUE : FALSE;
				g_free (str);
			}
			str = xmlGetProp (cur, "auto_increment");
			if (str) {
				spec->autoinc = ((*str == 't') || (*str == 'T')) ? TRUE : FALSE;
				g_free (str);
			}

			nbfields ++;
			continue;
		}
		if (!strcmp (cur->name, "data"))
			break;
	}

	if (nbfields == 0) {
		g_set_error (error, 0, 0, _("No <field> specified in <data-array>"));
		clean_field_specs (fields);
		return NULL;
	}

	if (! cur) {
		g_set_error (error, 0, 0, _("No <data> specified in <data-array>"), node->name);
		clean_field_specs (fields);
		return NULL;
	}

	/* model creation */
	model = gda_data_model_array_new (nbfields);
	list = fields;
	pos = 0;
	while (list) {
		GdaColumn *column;
		XmlColumnSpec *spec;

		spec = (XmlColumnSpec *)(list->data);
		column = gda_data_model_describe_column (model, pos);
		gda_column_set_title (column, spec->title);
		gda_column_set_name (column, spec->name);
		gda_column_set_defined_size (column, spec->size);
		gda_column_set_caption (column, spec->caption);
		gda_column_set_dbms_type (column, spec->dbms_type);
		gda_column_set_scale (column, spec->scale);
		gda_column_set_gdatype (column, spec->gdatype);
		gda_column_set_allow_null (column, spec->nullok);
		gda_column_set_primary_key (column, spec->pkey);
		gda_column_set_unique_key (column, spec->unique);

		list = g_slist_next (list);
		pos++;
	}
	clean_field_specs (fields);

	if (! gda_data_model_add_data_from_xml_node (model, cur)) {
		g_object_unref (model);
		return NULL;
	}
		
	return model;
}

/**
 * gda_data_model_array_set_n_columns
 * @model: the #GdaDataModelArray.
 * @cols: number of columns for rows this data model should use.
 *
 * Sets the number of columns for rows inserted in this model. 
 * @cols must be greated than or equal to 0.
 */
void
gda_data_model_array_set_n_columns (GdaDataModelArray *model, gint cols)
{
	g_return_if_fail (GDA_IS_DATA_MODEL_ARRAY (model));
	model->priv->number_of_columns = cols;
	/* FIXME: should rebuild the internal array, removing/adding empty cols */
}

/**
 * gda_data_model_array_clear
 * @model: the model to clear.
 *
 * Frees all the rows inserted in @model.
 */
void
gda_data_model_array_clear (GdaDataModelArray *model)
{
	g_return_if_fail (GDA_IS_DATA_MODEL_ARRAY (model));

	while (model->priv->rows->len > 0) {
		GdaRow *row = (GdaRow *) g_ptr_array_index (model->priv->rows, 0);

		if (row != NULL)
			gda_row_free (row);
		g_ptr_array_remove_index (model->priv->rows, 0);
	}
}
