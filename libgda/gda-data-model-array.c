/* GDA library
 * Copyright (C) 1998 - 2006 The GNOME Foundation.
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
#include <libgda/gda-row.h>
#include <libgda/gda-data-model.h>
#include <libgda/gda-data-model-extra.h>
#include <libgda/gda-util.h>

#define PARENT_TYPE GDA_TYPE_DATA_MODEL_ROW

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
gda_data_model_array_get_n_rows (GdaDataModelRow *model)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL_ARRAY (model), -1);
	return GDA_DATA_MODEL_ARRAY (model)->priv->rows->len;
}

static gint
gda_data_model_array_get_n_columns (GdaDataModelRow *model)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL_ARRAY (model), -1);
	return GDA_DATA_MODEL_ARRAY (model)->priv->number_of_columns;
}

static GdaRow *
gda_data_model_array_get_row (GdaDataModelRow *model, gint row, GError **error)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL_ARRAY (model), NULL);

	if (row >= GDA_DATA_MODEL_ARRAY (model)->priv->rows->len) {
		g_set_error (error, 0, 0,
			     _("Row out %d of range 0 - %d"), row,
			     GDA_DATA_MODEL_ARRAY (model)->priv->rows->len- 1);
		return NULL;
	}

	return GDA_ROW (g_ptr_array_index (GDA_DATA_MODEL_ARRAY (model)->priv->rows, row));
}

static const GdaValue *
gda_data_model_array_get_value_at (GdaDataModelRow *model, gint col, gint row)
{
	GdaRow *fields;

	g_return_val_if_fail (GDA_IS_DATA_MODEL_ARRAY (model), NULL);

	if (row >= GDA_DATA_MODEL_ARRAY (model)->priv->rows->len) {
		g_warning (_("Row out %d of range 0 - %d"), row, 
			   GDA_DATA_MODEL_ARRAY (model)->priv->rows->len);
		return NULL;
	}

	if (col >= GDA_DATA_MODEL_ARRAY (model)->priv->number_of_columns) {
		g_warning (_("Column out %d of range 0 - %d"), col, 
			   GDA_DATA_MODEL_ARRAY (model)->priv->number_of_columns);
		return NULL;
	}

	fields = g_ptr_array_index (GDA_DATA_MODEL_ARRAY (model)->priv->rows, row);
	if (fields != NULL) {
		GdaValue *field;

		field = gda_row_get_value (fields, col);
		return (const GdaValue *) field;
	}

	return NULL;
}

static gboolean
gda_data_model_array_is_updatable (GdaDataModelRow *model)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL_ARRAY (model), FALSE);
	return TRUE;
}

static GdaRow *
gda_data_model_array_append_values (GdaDataModelRow *model, const GList *values, GError **error)
{
	gint len;
	GdaRow *row = NULL;

	g_return_val_if_fail (GDA_IS_DATA_MODEL_ARRAY (model), NULL);
	g_return_val_if_fail (values != NULL, NULL);

	len = g_list_length ((GList *) values);
	if (len != GDA_DATA_MODEL_ARRAY (model)->priv->number_of_columns) {
		g_set_error (error, 0, GDA_DATA_MODEL_VALUES_LIST_ERROR,
			     _("Wrong number of values in values list"));
		return NULL;
	}

	row = gda_row_new_from_list (GDA_DATA_MODEL (model), values);
	if (!GDA_DATA_MODEL_ROW_CLASS (G_OBJECT_GET_CLASS (model))->append_row (model, row, error)) {
		g_object_unref (row);
		return NULL;
	}

	g_object_unref (row);
	return (GdaRow *) row;
}

static gboolean
gda_data_model_array_append_row (GdaDataModelRow *model, GdaRow *row, GError **error)
{
	g_return_val_if_fail (GDA_IS_DATA_MODEL_ARRAY (model), FALSE);
	g_return_val_if_fail (row != NULL, FALSE);

	g_ptr_array_add (GDA_DATA_MODEL_ARRAY (model)->priv->rows, (GdaRow *) row);
	g_object_ref (row);
	gda_row_set_number ((GdaRow *) row, GDA_DATA_MODEL_ARRAY (model)->priv->rows->len - 1);
	gda_data_model_row_inserted ((GdaDataModel *) model, GDA_DATA_MODEL_ARRAY (model)->priv->rows->len - 1);

	return TRUE;
}

static gboolean
gda_data_model_array_update_row (GdaDataModelRow *model, GdaRow *row, GError **error)
{
        gint i;
        GdaDataModelArrayPrivate *priv;
	gint num;

        g_return_val_if_fail (GDA_IS_DATA_MODEL_ARRAY (model), FALSE);
        g_return_val_if_fail (row != NULL, FALSE);

	num = gda_row_get_number (row);
        priv = GDA_DATA_MODEL_ARRAY (model)->priv;

        for (i = 0; i < priv->rows->len; i++) {
                if (gda_row_get_number (priv->rows->pdata[i]) == num) {
			GdaRow *tmp;

			if (row == priv->rows->pdata[i]) {
				gda_data_model_row_updated ((GdaDataModel *) model, i);
				return TRUE;
			}

			tmp = gda_row_copy (row);
			g_object_unref (priv->rows->pdata[i]);
			priv->rows->pdata[i] = tmp;
			gda_data_model_row_updated ((GdaDataModel *) model, i);

                        return TRUE;
                }
        }

	g_set_error (error, 0, GDA_DATA_MODEL_ROW_NOT_FOUND_ERROR,
		     _("Row not found in data model"));
        return FALSE;
}

static gboolean
gda_data_model_array_remove_row (GdaDataModelRow *model, GdaRow *row, GError **error)
{
	gint i, rownum;
	
	g_return_val_if_fail (GDA_IS_DATA_MODEL_ARRAY (model), FALSE);
	g_return_val_if_fail (row != NULL, FALSE);

	if (g_ptr_array_remove (GDA_DATA_MODEL_ARRAY (model)->priv->rows, (gpointer) row)) {
		/* renumber following rows */
		rownum = gda_row_get_number ((GdaRow *) row);
		for (i = (rownum + 1); i < GDA_DATA_MODEL_ARRAY (model)->priv->rows->len; i++)
			gda_row_set_number ((GdaRow *) gda_data_model_array_get_row (model, i, error), (i-1));

		/* tag the row as being removed */
		gda_row_set_id ((GdaRow *) row, "R");
		gda_row_set_number ((GdaRow *) row, -1);

		gda_data_model_row_removed ((GdaDataModel *) model, rownum);
		g_object_unref (row);
		return TRUE;
	}

	g_set_error (error, 0, GDA_DATA_MODEL_ROW_NOT_FOUND_ERROR,
		     _("Row not found in data model"));
	return FALSE;
}

static void
gda_data_model_array_class_init (GdaDataModelArrayClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GdaDataModelRowClass *model_class = GDA_DATA_MODEL_ROW_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = gda_data_model_array_finalize;
	model_class->get_n_rows = gda_data_model_array_get_n_rows;
	model_class->get_n_columns = gda_data_model_array_get_n_columns;
	model_class->get_row = gda_data_model_array_get_row;
	model_class->get_value_at = gda_data_model_array_get_value_at;
	model_class->is_updatable = gda_data_model_array_is_updatable;
	model_class->append_values = gda_data_model_array_append_values;
	model_class->append_row = gda_data_model_array_append_row;
	model_class->update_row = gda_data_model_array_update_row;
	model_class->remove_row = gda_data_model_array_remove_row;
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
	gchar        *table;
	gchar        *ref;
} XmlColumnSpec;

static void
clean_field_specs (GSList *fields)
{
	GSList *list;
	XmlColumnSpec *spec;

	list = fields;
	while (list) {
		spec = (XmlColumnSpec*)(list->data);
		g_free (spec->name);
		g_free (spec->title);
		g_free (spec->caption);
		g_free (spec->dbms_type);
		g_free (spec->table);
		g_free (spec->ref);
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
	gchar *str;

	g_return_val_if_fail (node, NULL);

	if (strcmp (node->name, "gda_array")) {
		g_set_error (error, 0, 0, _("Node is not <gda_array> but <%s>"), node->name);
		return NULL;
	}

	for (cur = node->children; cur; cur=cur->next) {
		if (xmlNodeIsText (cur))
			continue;
		if (!strcmp (cur->name, "gda_array_field")) {
			XmlColumnSpec *spec;

			spec = g_new0 (XmlColumnSpec, 1);
			fields = g_slist_append (fields, spec);

			spec->name = xmlGetProp (cur, "name");
			spec->title = xmlGetProp (cur, "title");
			if (!spec->title && spec->name)
				spec->title = g_strdup (spec->name);

			spec->caption = xmlGetProp (cur, "caption");
			spec->dbms_type = xmlGetProp (cur, "dbms_type");
			str = xmlGetProp (cur, "gdatype");
			if (str) {
				spec->gdatype = gda_type_from_string (str);
				g_free (str);
			}
			else {
				g_set_error (error, 0, 0, _("No \"gdatype\" attribute specified in <gda_array_field>"));
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
			spec->nullok = TRUE;
			if (str) {
				spec->nullok = ((*str == 't') || (*str == 'T')) ? TRUE : FALSE;
				g_free (str);
			}
			str = xmlGetProp (cur, "auto_increment");
			if (str) {
				spec->autoinc = ((*str == 't') || (*str == 'T')) ? TRUE : FALSE;
				g_free (str);
			}
			spec->table = xmlGetProp (cur, "table");
			spec->ref = xmlGetProp (cur, "ref");

			nbfields ++;
			continue;
		}
		if (!strcmp (cur->name, "gda_array_data"))
			break;
	}

	if ((nbfields == 0) || !cur) {
		g_set_error (error, 0, 0, _("No <gda_array_field> specified in <gda_array>"));
		clean_field_specs (fields);
		return NULL;
	}

	/* model creation */
	model = gda_data_model_array_new (nbfields);
	str = xmlGetProp (node, "id");
	if (str) {
		gda_object_set_id (GDA_OBJECT (model), str+2);
		g_free (str);
	}
	str = xmlGetProp (node, "name");
	if (str) {
		gda_object_set_name (GDA_OBJECT (model), str);
		g_free (str);
	}

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
		gda_column_set_gda_type (column, spec->gdatype);
		gda_column_set_allow_null (column, spec->nullok);
		gda_column_set_primary_key (column, spec->pkey);
		gda_column_set_unique_key (column, spec->unique);
		gda_column_set_table (column, spec->table);
		gda_column_set_references (column, spec->ref);

		list = g_slist_next (list);
		pos++;
	}
	clean_field_specs (fields);

	if (! gda_data_model_add_data_from_xml_node (model, cur, error)) {
		g_object_unref (model);
		return NULL;
	}

#ifdef GDA_DEBUG_NO
	g_print ("==== Loaded data model ====\n");
	gda_data_model_dump (model, stdout);
	g_print ("===========================\n");
#endif

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
			g_object_unref (row);
		g_ptr_array_remove_index (model->priv->rows, 0);
	}
}
