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

#include <glib/gdataset.h>
#include <glib-object.h>
#include <libsql/sql_parser.h>
#include <libgda/gda-intl.h>
#include <libgda/gda-log.h>
#include <libgda/gda-select.h>
#include <libgda/gda-value.h>
#include <string.h>

#define PARENT_TYPE GDA_TYPE_DATA_MODEL_ARRAY

struct _GdaSelectPrivate {
	GList *field_descriptions;
	GHashTable *source_models;
	gchar *sql;
	gboolean changed;
	gboolean run_result;
};

static void gda_select_class_init (GdaSelectClass *klass);
static void gda_select_init       (GdaSelect *sel, GdaSelectClass *klass);
static void gda_select_finalize   (GObject *object);

static GObjectClass *parent_class = NULL;

/*
 * Private functions
 */

static void
data_model_changed_cb (GdaDataModel *model, gpointer user_data)
{
	GdaSelect *sel = GDA_SELECT (user_data);

	sel->priv->changed = TRUE;
}

/*
 * GdaSelect class implementation
 */

static GdaFieldAttributes *
gda_select_describe_column (GdaDataModel *model, gint col)
{
	GList *l;
	GdaSelect *sel = (GdaSelect *) model;

	g_return_val_if_fail (GDA_IS_SELECT (sel), NULL);
	g_return_val_if_fail (sel->priv->field_descriptions != NULL, NULL);

	l = g_list_nth (sel->priv->field_descriptions, col);
	if (!l)
		return NULL;

	return gda_field_attributes_copy ((GdaFieldAttributes *) l->data);
}

static const GdaRow *
gda_select_get_row (GdaDataModel *model, gint row)
{
	GdaSelect *sel = (GdaSelect *) model;

	g_return_val_if_fail (GDA_IS_SELECT (sel), NULL);

	/* FIXME: identify this row, so that it can be updated and changes
	   proxied to the source_model */

	return GDA_DATA_MODEL_CLASS (parent_class)->get_row (model, row);
}

static gboolean
gda_select_is_editable (GdaDataModel *model)
{
	GdaSelect *sel = (GdaSelect *) model;

	g_return_val_if_fail (GDA_IS_SELECT (sel), FALSE);

	return FALSE;
}

static const GdaRow *
gda_select_append_row (GdaDataModel *model, const GList *values)
{
	const GdaRow *row;
	GdaSelect *sel = (GdaSelect *) model;

	g_return_val_if_fail (GDA_IS_SELECT (sel), NULL);

	return NULL;
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
	model_class->get_row = gda_select_get_row;
	// we use the get_value_at of the base class
	model_class->is_editable = gda_select_is_editable;
	model_class->append_row = gda_select_append_row;
}

static void
gda_select_init (GdaSelect *sel, GdaSelectClass *klass)
{
	gint i;

	g_return_if_fail (GDA_IS_SELECT (sel));

	/* allocate internal structure */
	sel->priv = g_new0 (GdaSelectPrivate, 1);
	sel->priv->field_descriptions = NULL;
	sel->priv->source_models = g_hash_table_new (g_str_hash, g_str_equal);
	sel->priv->sql = NULL;
	sel->priv->changed = FALSE;
	sel->priv->run_result = TRUE;
}

static void
free_source_model (gpointer key, gpointer value, GdaSelect *sel)
{
	g_signal_handlers_disconnect_by_func (G_OBJECT (value), data_model_changed_cb, sel);

	g_free (key);
	g_object_unref (G_OBJECT (value));
}

static void
gda_select_finalize (GObject *object)
{
	GList *l;
	GdaSelect *sel = (GdaSelect *) object;

	g_return_if_fail (GDA_IS_SELECT (sel));

	/* free memory */
	if (sel->priv->field_descriptions) {
		g_list_foreach (sel->priv->field_descriptions, (GFunc) gda_field_attributes_free, NULL);
		g_list_free (sel->priv->field_descriptions);
		sel->priv->field_descriptions = NULL;
	}

	g_hash_table_foreach (sel->priv->source_models, (GHFunc) free_source_model, sel);
	g_hash_table_destroy (sel->priv->source_models);
	sel->priv->source_models = NULL;

	if (sel->priv->sql) {
		g_free (sel->priv->sql);
		sel->priv->sql = NULL;
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
 * #GdaDataModel's based on a given SQL SELECT command..
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

/**
 * gda_select_add_source
 * @sel: a #GdaSelect object.
 * @name: name to identify the data model (usually a table name).
 * @source: a #GdaDataModel from which to get data.
 *
 * Add a data model as a source of data for the #GdaSelect object. When
 * the select object is run (via #gda_select_run), it will parse the SQL
 * and get the required data from the source data models.
 */
void
gda_select_add_source (GdaSelect *sel, const gchar *name, const GdaDataModel *source)
{
	gpointer key, value;

	g_return_if_fail (GDA_IS_SELECT (sel));
	g_return_if_fail (GDA_IS_DATA_MODEL (source));

	/* search for a data model with the same name */
	if (g_hash_table_lookup_extended (sel->priv->source_models, name, &key, &value)) {
		g_hash_table_remove (sel->priv->source_models, name);

		free_source_model (key, value, sel);
	}

	g_signal_connect (G_OBJECT (source), "changed",
			  G_CALLBACK (data_model_changed_cb), sel);

	g_object_ref (G_OBJECT (source));
	g_hash_table_insert (sel->priv->source_models, g_strdup (name), source);

	sel->priv->changed = TRUE;
}

/**
 * gda_select_set_sql
 * @sel: a #GdaSelect object.
 * @sql: the SQL command to be used for filtering rows.
 *
 * Set the SQL command to be used on the given #GdaSelect object
 * for filtering rows from the source data model (which is
 * set with #gda_select_set_source).
 */
void
gda_select_set_sql (GdaSelect *sel, const gchar *sql)
{
	g_return_if_fail (GDA_IS_SELECT (sel));

	if (sel->priv->sql)
		g_free (sel->priv->sql);
	sel->priv->sql = g_strdup (sql);

	sel->priv->changed = TRUE;
}

/**
 * gda_select_run
 * @sel: a #GdaSelect object.
 *
 * Run the query and fill in the #GdaSelect object with the
 * rows that matched the SQL command (which can be set with
 * #gda_select_set_sql) associated with this #GdaSelect
 * object.
 *
 * After calling this function, if everything is successful,
 * the #GdaSelect object will contain the matched rows, which
 * can then be accessed like a normal #GdaDataModel.
 *
 * Returns: TRUE if successful, FALSE if there was an error.
 */
gboolean
gda_select_run (GdaSelect *sel)
{
	sql_statement *sqlst;

	g_return_val_if_fail (GDA_IS_SELECT (sel), FALSE);
	g_return_val_if_fail (GDA_IS_DATA_MODEL (sel->priv->source_models), FALSE);

	if (!sel->priv->changed)
		return sel->priv->run_result;

	gda_data_model_array_clear (GDA_DATA_MODEL_ARRAY (sel));

	/* parse the SQL command */
	sqlst = sql_parse (sel->priv->sql);
	if (!sqlst) {
		gda_log_error (_("Could not parse SQL string '%s'"), sel->priv->sql);
		return FALSE;
	}
	if (sqlst->type != SQL_select) {
		gda_log_error (_("SQL command is not a SELECT (is '%s'"), sel->priv->sql);
		//sql_statement_destroy (sqlst);
		return FALSE;
	}

	/* FIXME */

	//sql_statement_destroy (sqlst);
	sel->priv->changed = FALSE;

	return sel->priv->run_result;
}
