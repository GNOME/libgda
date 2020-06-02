/* gda-db-index-index.c
 *
 * Copyright (C) 2019 Pavlo Solntsev <p.sun.fun@gmail.com>
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

#include "gda-db-index-field.h"

typedef struct
{
  GdaDbColumn *mColumn;
  gchar *mCollate;
  GdaDbIndexSortOrder mSortOrder;
} GdaDbIndexFieldPrivate;


/**
 * SECTION:gda-db-index-field
 * @short_description: Object to represent table index
 * @see_also: #GdaDbTable, #GdaDbCatalog
 * @stability: Stable
 * @include: libgda/libgda.h
 *
 * This object is a container for information needed to create an index for a table. After
 * population with information, it should be passed to the #GdaDbIndex instance. See #GdaDbIndex
 * section for the example.
 *
 */

G_DEFINE_TYPE_WITH_PRIVATE(GdaDbIndexField, gda_db_index_field, G_TYPE_OBJECT)

/**
 * gda_db_index_field_new:
 *
 * Create a new instance of #GdaDbIndexField
 *
 * Returns: (transfer full): A new instance of #GdaDbIndexField
 *
 * Stability: Stable
 * Since: 6.0
 */
GdaDbIndexField *
gda_db_index_field_new (void)
{
  return g_object_new (GDA_TYPE_DB_INDEX_FIELD, NULL);
}
static void
gda_db_index_field_dispose (GObject *object)
{
  GdaDbIndexField *self = GDA_DB_INDEX_FIELD (object);
  GdaDbIndexFieldPrivate *priv = gda_db_index_field_get_instance_private (self);

  g_object_unref (priv->mColumn);

  if (priv->mCollate)
    g_free (priv->mCollate);

  G_OBJECT_CLASS (gda_db_index_field_parent_class)->dispose (object);
}

static void
gda_db_index_field_class_init (GdaDbIndexFieldClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gda_db_index_field_dispose;
}

static void
gda_db_index_field_init (GdaDbIndexField *self)
{
  GdaDbIndexFieldPrivate *priv = gda_db_index_field_get_instance_private (self);

  priv->mColumn = NULL;
  priv->mCollate = NULL;
  priv->mSortOrder = GDA_DB_INDEX_SORT_ORDER_ASC;
}

/**
 * gda_db_index_field_set_column:
 * @self: an instance of #GdaDbIndexField
 * @column: column to add index to
 *
 * Only full name will be extracted from @column. The @column instance should be freed using
 * g_object_unref(). The instance @self take a copy of the @column object by increasing its
 * referecne count.
 *
 * Stability: Stable
 * Since: 6.0
 */
void
gda_db_index_field_set_column (GdaDbIndexField *self,
                               GdaDbColumn *column)
{
  GdaDbIndexFieldPrivate *priv = gda_db_index_field_get_instance_private (self);

  if (priv->mColumn)
    g_object_unref (priv->mColumn);

  priv->mColumn = g_object_ref (column);
}

/**
 * gda_db_index_field_get_column:
 * @self: a #GdaDbIndexField instance
 *
 * Returns an active column that was asigned to #GdaDbIndexField instance
 *
 * Returns: (transfer none): A #GdaDbColumn where index should be added
 *
 * Stability: Stable
 * Since: 6.0
 */
GdaDbColumn *
gda_db_index_field_get_column (GdaDbIndexField *self)
{
  GdaDbIndexFieldPrivate *priv = gda_db_index_field_get_instance_private (self);
  return priv->mColumn;
}

/**
 * gda_db_index_field_set_collate:
 * @self: instance of #GdaDbIndexField
 * @collate: collate to set
 *
 * Unfortunately, collate can vary from provider to provider. This method accepts collate name as a
 * string but user should provide valid values. For instance, SQLite3 accepts only "BINARY",
 * "NOCASE", and "RTRIM" values. PostgreSQL, on the other hand expects a name of a callable object,
 * e.g. function.
 *
 * Stability: Stable
 * Since: 6.0
 */
void
gda_db_index_field_set_collate (GdaDbIndexField *self,
                                const gchar *collate)
{
  GdaDbIndexFieldPrivate *priv = gda_db_index_field_get_instance_private (self);

  if (priv->mCollate)
    g_free (priv->mCollate);

  priv->mCollate = g_strdup (collate);
}

/**
 * gda_db_index_field_get_collate:
 * @self: instance of #GdaDbIndexField
 *
 * Returns: Collate value
 *
 * Stability: Stable
 * Since: 6.0
 */
const gchar *
gda_db_index_field_get_collate (GdaDbIndexField *self)
{
  GdaDbIndexFieldPrivate *priv = gda_db_index_field_get_instance_private (self);
  return priv->mCollate;
}

/**
 * gda_db_index_field_set_sort_order:
 * @self: object to use
 * @sorder: sort order to set
 *
 * Stability: Stable
 * Since: 6.0
 */
void
gda_db_index_field_set_sort_order (GdaDbIndexField *self,
                                   GdaDbIndexSortOrder sorder)
{
  GdaDbIndexFieldPrivate *priv = gda_db_index_field_get_instance_private (self);
  priv->mSortOrder = sorder;
}

/**
 * gda_db_index_field_get_sort_order:
 * @self: object to use
 *
 * Returns: sort order as a #GdaDbIndexSortOrder object
 *
 * Stability: Stable
 * Since: 6.0
 */
GdaDbIndexSortOrder
gda_db_index_field_get_sort_order (GdaDbIndexField *self)
{
  GdaDbIndexFieldPrivate *priv = gda_db_index_field_get_instance_private (self);
  return priv->mSortOrder;
}

/**
 * gda_db_index_field_get_sort_order_str:
 * @self: an instance of #GdaDbIndexField
 *
 * Returns: SORT ORDER string or %NULL
 *
 * Stability: Stable
 * Since: 6.0
 */
const gchar *
gda_db_index_field_get_sort_order_str (GdaDbIndexField *self)
{
  g_return_val_if_fail (GDA_IS_DB_INDEX_FIELD (self), NULL);

  GdaDbIndexSortOrder sorder = gda_db_index_field_get_sort_order (self);

  if (sorder == GDA_DB_INDEX_SORT_ORDER_ASC)
    return "ASC";
  else if (sorder == GDA_DB_INDEX_SORT_ORDER_DESC)
    return "DESC";
  else
    return NULL;
}
