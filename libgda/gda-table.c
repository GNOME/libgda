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

#include <bonobo/bonobo-i18n.h>
#include <libgda/gda-table.h>

#define PARENT_TYPE GDA_TYPE_DATA_MODEL

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

static void
gda_table_class_init (GdaTableClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = gda_table_finalize;
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

static void
remove_field_hash (gpointer key, gpointer value, gpointer user_data)
{
	g_free (key);
	gda_field_attributes_free (value);
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
 * gda_table_add_field
 * @table: A #GdaTable object.
 * @fa: Attributes for the new field.
 *
 * Adds a field to the given #GdaTable.
 */
void
gda_table_add_field (GdaTable *table, const GdaFieldAttributes *fa)
{
	gchar *name;
	GdaFieldAttributes *new_fa;

	g_return_if_fail (GDA_IS_TABLE (table));
	g_return_if_fail (fa != NULL);

	name = gda_field_attributes_get_name (fa);
	if (!name || !*name)
		return;

	/* we first look for a field with the same name */
	if (g_hash_table_lookup (table->priv->fields, name)) {
		gda_log_error (_("There is already a field called %s"), name);
		return;
	}

	/* add the new field to the table */
	new_fa = gda_field_attributes_new ();
	gda_field_attributes_set_defined_size (new_fa, gda_field_attributes_get_defined_size (fa));
	gda_field_attributes_set_name (new_fa, name);
	gda_field_attributes_set_scale (new_fa, gda_field_attributes_get_scale (fa));
	gda_field_attributes_set_gdatype (new_fa, gda_field_attributes_get_gdatype (fa));
	gda_field_attributes_set_allow_null (new_fa, gda_field_attributes_get_allow_null (fa));

	g_hash_table_insert (table->priv->fields, g_strdup (name), new_fa);
}
