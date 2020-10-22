/* gda-db-base.c
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
#define G_LOG_DOMAIN "GDA-db-base"

#include "gda-db-base.h"
#include <glib/gi18n.h>

typedef struct
{
  gchar *m_catalog;
  gchar *m_schema;
  gchar *m_name;
  gchar *m_fullname;
} GdaDbBasePrivate;

/**
 * SECTION:gda-db-base
 * @short_description: The basic class for all database objects
 * @see_also: #GdaDbTable, #GdaDbView
 * @stability: Stable
 * @include: libgda/libgda.h
 *
 * This is a basic class for database objects, e.g. #GdaDbTable and #GdaDbView. It is not common to
 * use it directly.
 */

G_DEFINE_TYPE_WITH_PRIVATE (GdaDbBase, gda_db_base, G_TYPE_OBJECT)

/**
 * gda_db_base_new:
 *
 * Create a new #GdaDbBase instance
 *
 * Returns: a new #GdaDbBase instance
 */
GdaDbBase*
gda_db_base_new (void)
{
  return g_object_new (GDA_TYPE_DB_BASE, NULL);
}

static void
gda_db_base_finalize (GObject *object)
{
  GdaDbBase *self = (GdaDbBase *)object;
  GdaDbBasePrivate *priv = gda_db_base_get_instance_private (self);

  g_free (priv->m_catalog);
  g_free (priv->m_schema);
  g_free (priv->m_name);
  g_free (priv->m_fullname);

  G_OBJECT_CLASS (gda_db_base_parent_class)->finalize (object);
}

static void
gda_db_base_class_init (GdaDbBaseClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gda_db_base_finalize;
}

static void
gda_db_base_init (GdaDbBase *self)
{
  GdaDbBasePrivate *priv = gda_db_base_get_instance_private (self);

  priv->m_catalog = NULL;
  priv->m_schema  = NULL;
  priv->m_name    = NULL;
  priv->m_fullname = NULL;
}

/**
 * gda_db_base_set_names:
 * @self: a #GdaDbBase object
 * @catalog: (nullable): a catalog name associated with the table
 * @schema: (nullable): a schema name associated with the table
 * @name: a table name associated with the table
 *
 * Sets database object names. @catalog and @schema can be %NULL but
 * @name always should be a valid, not %NULL string. The @name must be
 * set. If @catalog is %NULL @schema may not be %NULL but if @schema is
 * %NULL @catalog also should be %NULL.
 *
 * Since: 6.0
 */
void
gda_db_base_set_names (GdaDbBase *self,
                       const gchar* catalog,
                       const gchar* schema,
                       const gchar* name)
{
  g_return_if_fail (self);
  g_return_if_fail (name);

  GdaDbBasePrivate *priv = gda_db_base_get_instance_private (self);

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
    g_string_printf (fullnamestr, "%s.%s.%s", priv->m_catalog,
                     priv->m_schema, priv->m_name);
  else if (priv->m_schema)
    g_string_printf (fullnamestr, "%s.%s", priv->m_schema, priv->m_name);
  else
    g_string_printf (fullnamestr, "%s", priv->m_name);

  priv->m_fullname = g_string_free (fullnamestr, FALSE);
}

/**
 * gda_db_base_get_full_name:
 * @self: an instance of #GdaDbBase
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
gda_db_base_get_full_name (GdaDbBase *self)
{
  g_return_val_if_fail (self, NULL);

  GdaDbBasePrivate *priv = gda_db_base_get_instance_private (self);

  GString *fullnamestr = NULL;
  g_free (priv->m_fullname);

  fullnamestr = g_string_new (NULL);

  if (priv->m_catalog && priv->m_schema && priv->m_name)
    g_string_printf (fullnamestr, "%s.%s.%s", priv->m_catalog,
                     priv->m_schema, priv->m_name);
  else if (priv->m_schema && priv->m_name)
    g_string_printf (fullnamestr, "%s.%s", priv->m_schema, priv->m_name);
  else if (priv->m_name)
    g_string_printf (fullnamestr, "%s", priv->m_name);
  else
  {
    g_string_free (fullnamestr, TRUE);
    return NULL;
  }

  priv->m_fullname = g_string_free (fullnamestr, FALSE);

  return priv->m_fullname;
}

/**
 * gda_db_base_get_catalog:
 * @self: a #GdaDbBase object
 *
 * Returns current catalog name. The returned string should not be freed.
 *
 * Returns: Current catalog or %NULL
 *
 * Since: 6.0
 */
const gchar*
gda_db_base_get_catalog (GdaDbBase *self)
{
  g_return_val_if_fail (self, NULL);

  GdaDbBasePrivate *priv = gda_db_base_get_instance_private (self);

  return priv->m_catalog;
}

/**
 * gda_db_base_get_schema:
 * @self: GdaDbBase object
 *
 * Returns current schema name. The returned string should not be freed.
 *
 * Returns: Current scheme or %NULL
 *
 * Since: 6.0
 */
const gchar*
gda_db_base_get_schema (GdaDbBase *self)
{
  g_return_val_if_fail (self, NULL);

  GdaDbBasePrivate *priv = gda_db_base_get_instance_private (self);

  return priv->m_schema;
}

/**
 * gda_db_base_get_name:
 * @self: GdaDbBase object
 *
 * Returns current object name. The returned string should not be freed.
 *
 * Returns: Current object name or %NULL
 *
 * Since: 6.0
 */
const gchar*
gda_db_base_get_name (GdaDbBase *self)
{
  g_return_val_if_fail (self, NULL);

  GdaDbBasePrivate *priv = gda_db_base_get_instance_private (self);

  return priv->m_name;
}

/**
 * gda_db_base_set_catalog:
 * @self: a #GdaDbBase instance
 * @catalog: Catalog name as a string
 *
 * Set catalog name
 *
 * Since: 6.0
 */
void
gda_db_base_set_catalog (GdaDbBase  *self,
                         const gchar *catalog)
{
  g_return_if_fail (self);

  GdaDbBasePrivate *priv = gda_db_base_get_instance_private (self);

  g_free (priv->m_catalog);
  priv->m_catalog = g_strdup (catalog);
}

/**
 * gda_db_base_set_schema:
 * @self: a #GdaDbBase instance
 * @schema: Schema name as a string
 *
 * Set object schema. If @schema is %NULL the function just returns.
 *
 * Since: 6.0
 */
void
gda_db_base_set_schema (GdaDbBase  *self,
                        const gchar *schema)
{
  g_return_if_fail (self);

  if (!schema)
    return;

  GdaDbBasePrivate *priv = gda_db_base_get_instance_private (self);

  g_free (priv->m_schema);
  priv->m_schema = g_strdup (schema);
}

/**
 * gda_db_base_set_name:
 * @self: a #GdaDbBase instance
 * @name: Object name as a string
 *
 * Set object name. If @name is %NULL the function just returns.
 *
 * Since: 6.0
 */
void
gda_db_base_set_name (GdaDbBase  *self,
                      const gchar *name)
{
  g_return_if_fail (self);

  if (!name)
    return;

  GdaDbBasePrivate *priv = gda_db_base_get_instance_private (self);

  g_free (priv->m_name);
  priv->m_name = g_strdup (name);
}

/**
 * gda_db_base_compare:
 * @a: first #GdaDbBase object
 * @b: second #GdaDbBase object
 *
 * Compares two objects similar to g_strcmp().
 *
 * Returns: 0 if catalog, schema and name are the same
 *
 * Since: 6.0
 */
gint
gda_db_base_compare (GdaDbBase *a,
                     GdaDbBase *b)
{
  if (!a && !b)
    return 0;
  else if (!a && b)
    return -1;
  else if (a && !b)
    return 1;

  gint res = g_strcmp0 (gda_db_base_get_name(a), gda_db_base_get_name(b));

  if (!res)
    {
      res = g_strcmp0 (gda_db_base_get_catalog(a), gda_db_base_get_catalog(b));

      if (!res)
        return g_strcmp0(gda_db_base_get_schema(a), gda_db_base_get_schema(b));
      else
        return res;
    }
  else
    return res;
}
