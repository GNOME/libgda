/* gda-ddl-base.c
 *
 * Copyright © 2018 Pavlo Solntsev <p.sun.fun@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "gda-ddl-base.h"
#include <glib/gi18n.h>

typedef struct
{
	gchar *m_catalog;
	gchar *m_schema;
	gchar *m_name;
	gchar *m_fullname;
} GdaDdlBasePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GdaDdlBase, gda_ddl_base, G_TYPE_OBJECT)

/**
 * gda_ddl_base_new:
 *
 * Create a new #GdaDdlBase instance
 *
 * Returns: #GdaDdlBase instance
 */
GdaDdlBase *
gda_ddl_base_new (void)
{
	return g_object_new (GDA_TYPE_DDL_BASE, NULL);
}

static void
gda_ddl_base_finalize (GObject *object)
{
	GdaDdlBase *self = (GdaDdlBase *)object;
	GdaDdlBasePrivate *priv = gda_ddl_base_get_instance_private (self);

	g_free (priv->m_catalog);
	g_free (priv->m_schema);
	g_free (priv->m_name);
	g_free (priv->m_fullname);

	G_OBJECT_CLASS (gda_ddl_base_parent_class)->finalize (object);
}

static void
gda_ddl_base_class_init (GdaDdlBaseClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = gda_ddl_base_finalize;
}

static void
gda_ddl_base_init (GdaDdlBase *self)
{
	GdaDdlBasePrivate *priv = gda_ddl_base_get_instance_private (self);

	priv->m_catalog = NULL;
	priv->m_schema  = NULL;
	priv->m_name    = NULL;
	priv->m_fullname = NULL;
}

/**
 * gda_ddl_base_set_names:
 * @self: a #GdaDdlBase object
 * @catalog: a catalog associated with table
 * @schema: a schema associated with table
 * @name: a table name associated with table
 *
 * Sets database object name. @catalog and @schema can be %NULL but
 * @name always should be a valid, not %NULL string. The @name must be always
 * set. If @catalog is %NULL @schema may not be %NULL but if @schema is
 * %NULL @catalog also should be %NULL.
 *
 * Since: 6.0
 * */
void
gda_ddl_base_set_names (GdaDdlBase *self,
			const gchar* catalog,
			const gchar* schema,
			const gchar* name)
{
	g_return_if_fail (self);
	g_return_if_fail (name);

        GdaDdlBasePrivate *priv = gda_ddl_base_get_instance_private (self);

	g_free (priv->m_name);
	g_free (priv->m_schema);
	g_free (priv->m_catalog);
	g_free (priv->m_fullname);

	priv->m_name = g_strdup (name);
	priv->m_schema = NULL;
	priv->m_catalog = NULL;

	if (schema)
		priv->m_schema = g_strdup (schema);

	if (catalog)
		priv->m_catalog = g_strdup (catalog);

	GString *fullnamestr = NULL;

	fullnamestr = g_string_new (NULL);

	if (priv->m_catalog && priv->m_schema)
		g_string_printf (fullnamestr,"%s.%s.%s",priv->m_catalog,priv->m_schema,priv->m_name);
	else if (priv->m_schema)
		g_string_printf (fullnamestr,"%s.%s",priv->m_schema,priv->m_name);
	else
		g_string_printf (fullnamestr,"%s",priv->m_name);

	priv->m_fullname = g_strdup (fullnamestr->str);
	g_string_free (fullnamestr, TRUE);

        if (priv->m_catalog && priv->m_schema) {
		g_string_printf (fullnamestr,"%s.%s.%s",priv->m_catalog,priv->m_schema,priv->m_name);
	} else {
	/* In this block  catalog is NULL */
		if (priv->m_schema)
			g_string_printf (fullnamestr,"%s.%s",priv->m_schema,priv->m_name);
		else
			g_string_printf (fullnamestr,"%s",priv->m_name);
	}

	priv->m_fullname = g_strdup (fullnamestr->str);
}

 /**
 * gda_ddl_base_get_full_name:
 * @self: an instance of #GdaDdlBase
 *
 * This method returns full name in the format catalog.schema.name.
 * If schema is %NULL but catalog and name are not, then only name is
 * returned. If catalog is %NULL then full name will be in the format:
 * schema.name. If all three components are not set, then %NULL is returned.
 *
 * Returns: Full name of the database object.
 */
const gchar*
gda_ddl_base_get_full_name (GdaDdlBase *self)
{
	g_return_val_if_fail (self,NULL);

	GdaDdlBasePrivate *priv = gda_ddl_base_get_instance_private (self);

	GString *fullnamestr = NULL;

	fullnamestr = g_string_new (NULL);

	if (priv->m_catalog && priv->m_schema && priv->m_name)
		g_string_printf (fullnamestr,"%s.%s.%s",priv->m_catalog,priv->m_schema,priv->m_name);
	else if (priv->m_schema && priv->m_name)
		g_string_printf (fullnamestr,"%s.%s",priv->m_schema,priv->m_name);
	else if (priv->m_name)
		g_string_printf (fullnamestr,"%s",priv->m_name);
	else
		return NULL;

	priv->m_fullname = g_strdup (fullnamestr->str);
	g_string_free (fullnamestr, TRUE);

	/* In this block  catalog is NULL */
	priv->m_fullname = g_strdup (fullnamestr->str);
	g_string_free (fullnamestr, TRUE);

	return priv->m_fullname;
}

/**
 * gda_ddl_base_get_catalog:
 * * @self: GdaDdlBase object
 *
 * Returns current catalog name. The returned string should not be freed.
 * In case of error, the @error is set appropriatly and %NULL is returned.
 *
 * Returns: Current catalog or %NULL
 */
const gchar*
gda_ddl_base_get_catalog (GdaDdlBase  *self)
{
	g_return_val_if_fail (self,NULL);

	GdaDdlBasePrivate *priv = gda_ddl_base_get_instance_private (self);

	return priv->m_catalog;
}

/**
 * gda_ddl_base_get_schema:
 *
 * @self: GdaDdlBase object
 *
 * Returns current schema name. The returned string should not be freed.
 *
 * Returns: Current catalog or %NULL
 */
const gchar*
gda_ddl_base_get_schema (GdaDdlBase  *self)
{
	g_return_val_if_fail (self,NULL);

	GdaDdlBasePrivate *priv = gda_ddl_base_get_instance_private (self);

	return priv->m_schema;
}

/**
 * gda_ddl_base_get_name:
 *
 * @self: GdaDdlBase object
 * @error: an error holder
 *
 * Returns current object name. The returned string should not be freed.
 *
 * Returns: Current object name or %NULL
 */
const gchar*
gda_ddl_base_get_name (GdaDdlBase  *self)
{
	g_return_val_if_fail (self,NULL);

	GdaDdlBasePrivate *priv = gda_ddl_base_get_instance_private (self);

	return priv->m_name;
}

/**
 * gda_ddl_base_free:
 * @self: an instance of #GdaDdlBase to free.
 * A convenient method to free the memory.
 * It is a wrapper around g_clear_object().
 *
 */
void
gda_ddl_base_free (GdaDdlBase *self)
{
	g_clear_object (&self);
}

/**
 * gda_ddl_base_set_catalog:
 *
 * @self: a #GdaDdlBase instance
 * @catalog: Catalog name as a string
 *
 */
void
gda_ddl_base_set_catalog (GdaDdlBase  *self,
			  const gchar *catalog)
{
	g_return_if_fail (self);

	GdaDdlBasePrivate *priv = gda_ddl_base_get_instance_private (self);

	g_free (priv->m_catalog);
	priv->m_catalog = g_strdup (catalog);
}

/**
 * gda_ddl_base_set_schema:
 *
 * @self: a #GdaDdlBase instance
 * @schema: Schema name as a string
 *
 */
void
gda_ddl_base_set_schema (GdaDdlBase  *self,
			 const gchar *schema)
{
	g_return_if_fail (self);

	GdaDdlBasePrivate *priv = gda_ddl_base_get_instance_private (self);

	g_free (priv->m_schema);
	priv->m_schema = g_strdup (schema);
}

/**
 * gda_ddl_base_set_name:
 *
 * @self: a #GdaDdlBase instance
 * @name: Object name as a string
 *
 */
void
gda_ddl_base_set_name (GdaDdlBase  *self,
		       const gchar *name)
{
	g_return_if_fail (self);

	GdaDdlBasePrivate *priv = gda_ddl_base_get_instance_private (self);

	g_free (priv->m_name);
	priv->m_name = g_strdup (name);
}

