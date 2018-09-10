/* gda-ddl-base.c
 *
 * Copyright (C) 2018 Pavlo Solntsev <p.sun.fun@gmail.com>
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
#include "gda-ddl-base.h"
#include <glib/gi18n.h>

typedef struct
{
  gchar *m_catalog;
  gchar *m_schema;
  gchar *m_name;
  gchar *m_fullname;
} GdaDdlBasePrivate;

/**
 * SECTION:gda-ddl-base 
 * @short_description: The basic class for all database objects
 * @see_also: #GdaDdlTable, #GdaDdlView
 * @stability: Stable
 * @include: libgda/libgda.h
 *
 * This is a basic class for database objects, e.g. #GdaDdlTable and #GdaDdlView. It is not common to
 * use it directly. 
 */

G_DEFINE_TYPE_WITH_PRIVATE (GdaDdlBase, gda_ddl_base, G_TYPE_OBJECT)

/**
 * gda_ddl_base_new:
 *
 * Create a new #GdaDdlBase instance
 *
 * Returns: a new #GdaDdlBase instance 
 */
GdaDdlBase*
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
 * @catalog: (nullable): a catalog name associated with the table
 * @schema: (nullable): a schema name associated with the table
 * @name: a table name associated with the table
 *
 * Sets database object names. @catalog and @schema can be %NULL but
 * @name always should be a valid, not %NULL string. The @name must be always
 * set. If @catalog is %NULL @schema may not be %NULL but if @schema is
 * %NULL @catalog also should be %NULL.
 *
 * Since: 6.0
 */
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
    g_string_printf (fullnamestr,"%s.%s.%s",priv->m_catalog,
                     priv->m_schema,priv->m_name);
  else if (priv->m_schema)
    g_string_printf (fullnamestr,"%s.%s",priv->m_schema,priv->m_name);
  else
    g_string_printf (fullnamestr,"%s",priv->m_name);

  priv->m_fullname = g_strdup (fullnamestr->str);
  g_string_free (fullnamestr, TRUE);
}

/**
 * gda_ddl_base_get_full_name:
 * @self: an instance of #GdaDdlBase
 *
 * This method returns a full name in the format catalog.schema.name.
 * If schema is %NULL but catalog and name are not, then only name is
 * returned. If catalog is %NULL then full name will be in the format:
 * schema.name. If all three components are not set, then %NULL is returned.
 *
 * Returns: Full name of the database object or %NULL.
 *
 * Since: 6.0
 */
const gchar*
gda_ddl_base_get_full_name (GdaDdlBase *self)
{
  g_return_val_if_fail (self,NULL);

  GdaDdlBasePrivate *priv = gda_ddl_base_get_instance_private (self);

  GString *fullnamestr = NULL;

  fullnamestr = g_string_new (NULL);

  if (priv->m_catalog && priv->m_schema && priv->m_name)
    g_string_printf (fullnamestr,"%s.%s.%s",priv->m_catalog,
                     priv->m_schema,priv->m_name);
  else if (priv->m_schema && priv->m_name)
    g_string_printf (fullnamestr,"%s.%s",priv->m_schema,priv->m_name);
  else if (priv->m_name)
    g_string_printf (fullnamestr,"%s",priv->m_name);
  else
    return NULL;

  priv->m_fullname = g_strdup (fullnamestr->str);
  g_string_free (fullnamestr, TRUE);

  return priv->m_fullname;
}

/**
 * gda_ddl_base_get_catalog:
 * @self: a #GdaDdlBase object
 *
 * Returns current catalog name. The returned string should not be freed.
 *
 * Returns: Current catalog or %NULL
 *
 * Since: 6.0
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
 * @self: GdaDdlBase object
 *
 * Returns current schema name. The returned string should not be freed.
 *
 * Returns: Current catalog or %NULL
 *
 * Since: 6.0
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
 * @self: GdaDdlBase object
 *
 * Returns current object name. The returned string should not be freed.
 *
 * Returns: Current object name or %NULL
 *
 * Since: 6.0
 */
const gchar*
gda_ddl_base_get_name (GdaDdlBase  *self)
{
  g_return_val_if_fail (self,NULL);

  GdaDdlBasePrivate *priv = gda_ddl_base_get_instance_private (self);

  return priv->m_name;
}

/**
 * gda_ddl_base_set_catalog:
 * @self: a #GdaDdlBase instance
 * @catalog: Catalog name as a string
 * 
 * Set catalog name
 *
 * Since: 6.0
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
 * @self: a #GdaDdlBase instance
 * @schema: Schema name as a string
 *
 * Set object schema.
 *
 * Since: 6.0
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
 * @self: a #GdaDdlBase instance
 * @name: Object name as a string
 * 
 * Set object name
 *
 * Since: 6.0
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

/**
 * gda_ddl_base_compare:
 * @a: first #GdaDdlBase object
 * @b: second #GdaDdlBase object
 *
 * Compares two objects similar to g_strcmp(). 
 *
 * Returns: 0 if catalog, schema and name are the same
 *
 * Since: 6.0
 */
gint
gda_ddl_base_compare (GdaDdlBase *a, GdaDdlBase *b)
{
  if (!a && !b)
    return 0;
  else if (!a && b)
    return -1;
  else if (a && !b)
    return 1;
 
  gint res = g_strcmp0 (gda_ddl_base_get_name(a),gda_ddl_base_get_name(b));

  if (!res)
    {
      res = g_strcmp0 (gda_ddl_base_get_catalog(a),gda_ddl_base_get_catalog(b));
      
      if (!res)
        return g_strcmp0(gda_ddl_base_get_schema(a),gda_ddl_base_get_schema(b));  
      else
        return res;
    }
  else
    return res;

}
