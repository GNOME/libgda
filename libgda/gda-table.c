/* GDA library
 * Copyright (C) 1998-2002 The GNOME Foundation.
 *
 * AUTHORS:
 * 	Rodrigo Moya <rodrigo@gnome-db.org>
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

#include <config.h>
#include <libgda/gda-intl.h>
#include <libgda/gda-log.h>
#include <libgda/gda-table.h>

#define PARENT_TYPE GDA_TYPE_DATA_MODEL_ARRAY

struct _GdaTablePrivate {
	gchar *name;
	GHashTable *fields;
};

static void gda_table_class_init (GdaTableClass *klass);
static void gda_table_init       (GdaTable *table, GdaTableClass *klass);
static void gda_table_finalize   (GObject *object);

static GObjectClass *parent_class = NULL;

/*
 * GdaTable class implementation
 */

static GdaFieldAttributes *
gda_table_describe_column (GdaDataModel *model, gint col)
{
	GdaFieldAttributes *fa = NULL, *new_fa;
	GdaTable *table = (GdaTable *) model;

	g_return_val_if_fail (GDA_IS_TABLE (table), NULL);

	if (col >= g_hash_table_size (table->priv->fields))
		return NULL;

	/* FIXME: obtain 'fa' from hash table */
	new_fa = gda_field_attributes_new ();
	gda_field_attributes_set_defined_size (new_fa, fa->defined_size);
	gda_field_attributes_set_name (new_fa, fa->name);
	gda_field_attributes_set_scale (new_fa, fa->scale);
	gda_field_attributes_set_gdatype (new_fa, fa->gda_type);
	gda_field_attributes_set_allow_null (new_fa, fa->allow_null);

	return new_fa;
}

static void
gda_table_class_init (GdaTableClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GdaDataModelClass *model_class = GDA_DATA_MODEL_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = gda_table_finalize;
	model_class->describe_column = gda_table_describe_column;
}

static void
gda_table_init (GdaTable *table, GdaTableClass *klass)
{
	g_return_if_fail (GDA_IS_TABLE (table));

	/* allocate internal structure */
	table->priv = g_new0 (GdaTablePrivate, 1);
	table->priv->name = NULL;
	table->priv->fields = g_hash_table_new (g_str_hash, g_str_equal);
}

static gboolean
remove_field_hash (gpointer key, gpointer value, gpointer user_data)
{
	g_free (key);
	gda_field_attributes_free (value);
	return TRUE;
}

static void
gda_table_finalize (GObject *object)
{
	GdaTable *table = (GdaTable *) object;

	g_return_if_fail (GDA_IS_TABLE (table));

	/* free memory */
	if (table->priv->name) {
		g_free (table->priv->name);
		table->priv->name = NULL;
	}

	g_hash_table_foreach_remove (table->priv->fields, remove_field_hash, NULL);
	g_hash_table_destroy (table->priv->fields);
	table->priv->fields = NULL;

	g_free (table->priv);
	table->priv = NULL;

	/* chain to parent class */
	parent_class->finalize (object);
}

GType
gda_table_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (GdaTableClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_table_class_init,
			NULL,
			NULL,
			sizeof (GdaTable),
			0,
			(GInstanceInitFunc) gda_table_init
		};
		type = g_type_register_static (PARENT_TYPE, "GdaTable", &info, 0);
	}
	return type;
}

/**
 * gda_table_new
 * @name: Name for the new table.
 *
 * Create a new #GdaTable object, which is an in-memory representation
 * of an entire table. It is mainly used by the #GdaXmlDatabase class,
 * but you can also use it in your applications for whatever you may need
 * it.
 *
 * Returns: the newly created object.
 */
GdaTable *
gda_table_new (const gchar *name)
{
	GdaTable *table;

	g_return_val_if_fail (name != NULL, NULL);

	table = g_object_new (GDA_TYPE_TABLE, NULL);
	table->priv->name = g_strdup (name);

	return table;
}

/**
 * gda_table_new_from_model
 * @name: Name for the new table.
 * @model: Model to create the table from.
 * @add_data: Whether to add model's data or not.
 *
 * Create a #GdaTable object from the given #GdaDataModel. This
 * is very useful to maintain an in-memory copy of a given
 * recordset obtained from a database. This is also used when
 * exporting data to a #GdaXmlDatabase object.
 *
 * Returns: the newly created object.
 */
GdaTable *
gda_table_new_from_model (const gchar *name, const GdaDataModel *model, gboolean add_data)
{
	GdaTable *table;
	gint n;
	gint cols;

	g_return_val_if_fail (name != NULL, NULL);
	g_return_val_if_fail (GDA_IS_DATA_MODEL (model), NULL);

	table = gda_table_new (name);
	if (!table)
		return NULL;

	/* add the columns description */
	cols = gda_data_model_get_n_columns (GDA_DATA_MODEL (model));
	for (n = 0; n < cols; n++) {
		GdaFieldAttributes *fa;

		fa = gda_data_model_describe_column (GDA_DATA_MODEL (model), n);
		gda_table_add_field (table, (const GdaFieldAttributes *) fa);
		gda_field_attributes_free (fa);
	}

	/* add the data */
	if (add_data)
		gda_table_add_data_from_model (table, model);

	return table;
}

/**
 * gda_table_add_field
 * @table: A #GdaTable object.
 * @fa: Attributes for the new field.
 *
 * Adds a field to the given #GdaTable.
 */
void
gda_table_add_field (GdaTable *table, const GdaFieldAttributes *fa)
{
	const gchar *name;
	GdaFieldAttributes *new_fa;

	g_return_if_fail (GDA_IS_TABLE (table));
	g_return_if_fail (fa != NULL);

	name = gda_field_attributes_get_name ((GdaFieldAttributes *) fa);
	if (!name || !*name)
		return;

	/* we first look for a field with the same name */
	if (g_hash_table_lookup (table->priv->fields, name)) {
		gda_log_error (_("There is already a field called %s"), name);
		return;
	}

	/* add the new field to the table */
	new_fa = gda_field_attributes_new ();
	gda_field_attributes_set_defined_size (new_fa, gda_field_attributes_get_defined_size ((GdaFieldAttributes *) fa));
	gda_field_attributes_set_name (new_fa, name);
	gda_field_attributes_set_scale (new_fa, gda_field_attributes_get_scale ((GdaFieldAttributes *) fa));
	gda_field_attributes_set_gdatype (new_fa, gda_field_attributes_get_gdatype ((GdaFieldAttributes *) fa));
	gda_field_attributes_set_allow_null (new_fa, gda_field_attributes_get_allow_null ((GdaFieldAttributes *) fa));

	g_hash_table_insert (table->priv->fields, g_strdup (name), new_fa);
	gda_data_model_array_set_n_columns (GDA_DATA_MODEL_ARRAY (table),
					    g_hash_table_size (table->priv->fields));
}

/**
 * gda_table_add_data_from_model
 */
void
gda_table_add_data_from_model (GdaTable *table, const GdaDataModel *model)
{
	g_return_if_fail (GDA_IS_TABLE (table));
	g_return_if_fail (GDA_IS_DATA_MODEL (model));
}
