/* gda-db-index.c
 *
 * Copyright (C) 2019-2020 Pavlo Solntsev <p.sun.fun@gmail.com>
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

#include "gda-db-index.h"
#include "gda-connection.h"
#include "gda-db-index-field.h"
#include "gda-server-operation.h"
#include "gda-server-provider.h"
#include "gda-lockable.h"
#include "gda-ddl-modifiable.h"
#include <glib/gi18n-lib.h>

G_DEFINE_QUARK (gda_db_index_error, gda_db_index_error)

typedef struct
{
  gboolean mUnique;
  GSList *mFieldList; /* A list of GdaDbIndexField */
  GdaDbTable *mTable; /* A table associated with the index */
} GdaDbIndexPrivate;

/**
 * SECTION:gda-db-index
 * @short_description: Object to represent table index
 * @see_also: #GdaDbTable, #GdaDbCatalog
 * @stability: Stable
 * @include: libgda/libgda.h
 *
 * The object #GdaDbIndex holds information about index in a table. Just populate the information
 * using index API, set table as property, execute gda_ddl_modifiable_create() method. This
 * method executes all needed DB manopulations to add the target index to the DB. This can be
 * illustarted by the following example:
 *
 * |[<!-- language="C" -->
 * GdaDbTable *table = gda_db_table_new();
 * // Populate table as needed.
 * GdaDbIndex      *index = gda_db_index_new ();
 * GdaDbIndexField *field = gda_db_index_field_new ();
 * GdaDbColumn     *fcol  = gda_db_column_new ();
 * GError *error = NULL;
 *
 * gda_db_index_set_unique (index, TRUE);
 * gda_db_base_set_name (GDA_DB_BASE (index), "MyIndex");
 *
 * gda_db_column_set_name (fcol, "name");
 *
 * gda_db_index_field_set_column (field, fcol);
 * gda_db_index_field_set_sort_order (field, GDA_DB_INDEX_SORT_ORDER_ASC);
 * gda_db_index_append_field (index, field);

 * g_object_unref (fcol);
 * g_object_unref (field);
 *
 * g_object_set (index, "table", table, NULL);
 *
 * res = gda_ddl_modifiable_create (GDA_DDL_MODIFIABLE(index), cnc, NULL, &error);
 *
 * if (!res)
 *   g_print("Error during index addition\n");
 * ]|
 */

static void gda_ddl_modifiable_interface_init (GdaDdlModifiableInterface *iface);

static gboolean gda_db_index_create (GdaDdlModifiable *self, GdaConnection *cnc,
                                     gpointer user_data, GError **error);
static gboolean gda_db_index_drop (GdaDdlModifiable *self, GdaConnection *cnc,
                                   gpointer user_data, GError **error);
static gboolean gda_db_index_rename (GdaDdlModifiable *old_name, GdaConnection *cnc,
                                     gpointer new_name, GError **error);


G_DEFINE_TYPE_WITH_CODE (GdaDbIndex, gda_db_index, GDA_TYPE_DB_BASE,
                         G_ADD_PRIVATE(GdaDbIndex)
                         G_IMPLEMENT_INTERFACE (GDA_TYPE_DDL_MODIFIABLE,
                                                gda_ddl_modifiable_interface_init))

enum {
    PROP_0,
    PROP_TABLE,
    /* <private> */
    N_PROPS
};

static GParamSpec *properties [N_PROPS] = {NULL};

GdaDbIndex *
gda_db_index_new (void)
{
  return g_object_new (GDA_TYPE_DB_INDEX, NULL);
}

static void
gda_db_index_init (GdaDbIndex *self)
{
  GdaDbIndexPrivate *priv = gda_db_index_get_instance_private (self);

  priv->mFieldList = NULL;
  priv->mUnique = TRUE;
  priv->mTable = NULL;
}

static void
gda_db_index_finalize (GObject *object)
{
  GdaDbIndex *self = GDA_DB_INDEX (object);
  GdaDbIndexPrivate *priv = gda_db_index_get_instance_private (self);

  g_slist_free_full (priv->mFieldList, g_object_unref);

  G_OBJECT_CLASS (gda_db_index_parent_class)->finalize (object);
}

static void
gda_db_index_dispose (GObject *object)
{
  GdaDbIndex *self = GDA_DB_INDEX (object);
  GdaDbIndexPrivate *priv = gda_db_index_get_instance_private (self);

  if (priv->mTable) g_object_unref (priv->mTable);

  G_OBJECT_CLASS (gda_db_index_parent_class)->dispose (object);
}

static void
gda_db_index_get_property (GObject    *object,
                           guint       prop_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  GdaDbIndex *self = GDA_DB_INDEX(object);
  GdaDbIndexPrivate *priv = gda_db_index_get_instance_private (self);

  switch (prop_id) {
    case PROP_TABLE:
      g_value_set_object (value, priv->mTable);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
gda_db_index_set_property (GObject    *object,
                           guint       prop_id,
                           const GValue     *value,
                           GParamSpec *pspec)
{
  GdaDbIndex *self = GDA_DB_INDEX(object);
  GdaDbIndexPrivate *priv = gda_db_index_get_instance_private (self);

  switch (prop_id) {
    case PROP_TABLE:
      if (priv->mTable)
        g_object_unref (priv->mTable);

      priv->mTable = g_value_dup_object (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
gda_db_index_class_init (GdaDbIndexClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gda_db_index_finalize;
  object_class->dispose = gda_db_index_dispose;
  object_class->get_property = gda_db_index_get_property;
  object_class->set_property = gda_db_index_set_property;

  properties[PROP_TABLE] = g_param_spec_object ("table",
                                                  "Table",
                                                  "Table associated with index",
                                                  GDA_TYPE_DB_TABLE,
                                                  G_PARAM_READWRITE);

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gda_ddl_modifiable_interface_init (GdaDdlModifiableInterface *iface)
{
  iface->create = gda_db_index_create;
  iface->drop   = gda_db_index_drop;
  iface->rename = gda_db_index_rename;
}

/**
 * gda_db_index_set_unique:
 * @self: #GdaDbIndex instance
 * @val: if set to %TRUE UNIQUE index type will be used.
 *
 * If @val is %TRUE a "UNIQUE" will be added to the INDEX CREATE command, e.g.
 * CREATE UNIQUE INDEX ...
 *
 * Stability: Stable
 * Since: 6.0
 */
void
gda_db_index_set_unique (GdaDbIndex *self,
                         gboolean val)
{
  G_DEBUG_HERE();
  g_return_if_fail (self);

  GdaDbIndexPrivate *priv = gda_db_index_get_instance_private (self);
  priv->mUnique = val;
}

/**
 * gda_db_index_get_unique:
 * @self: instance os #GdaDbIndex to use
 *
 * Returns: state for UNIQUE. This method will abort if @self is %NULL
 *
 * Stability: Stable
 * Since: 6.0
 */
gboolean
gda_db_index_get_unique (GdaDbIndex *self)
{
  G_DEBUG_HERE();
  GdaDbIndexPrivate *priv = gda_db_index_get_instance_private (self);
  return priv->mUnique;
}

/**
 * gda_db_index_append_field:
 * @self: an instance of #GdaDbIndex
 * @field: a field to set
 *
 * Append to index filed to the current index instance, The @self object will recieve full
 * ownership of the field. After this call, the reference count for @field will be increased and
 * the instance of @fiels must be destroyed by calling g_object_unref()
 *
 * Stability: Stable
 * Since: 6.0
 */
void
gda_db_index_append_field  (GdaDbIndex *self,
                            GdaDbIndexField *field)
{
  G_DEBUG_HERE();
  g_return_if_fail (self);
  g_return_if_fail (field);

  GdaDbIndexPrivate *priv = gda_db_index_get_instance_private (self);
// Check if field name already exists, then update values, otherwise append

  if (!priv->mFieldList)
    priv->mFieldList = g_slist_append (priv->mFieldList, g_object_ref (field));
  else
    {
      GSList *it = priv->mFieldList;
      for (; it != NULL; it = it->next)
        {
          GdaDbColumn *current_col = gda_db_index_field_get_column (GDA_DB_INDEX_FIELD (it->data));
          GdaDbColumn *new_col = gda_db_index_field_get_column (field);

          if (gda_db_base_compare (GDA_DB_BASE (current_col), GDA_DB_BASE (new_col)))
            priv->mFieldList = g_slist_append (priv->mFieldList, g_object_ref (field));
          else
            {
              gda_db_index_field_set_collate (GDA_DB_INDEX_FIELD (it->data),
                                              gda_db_index_field_get_collate (field));
              gda_db_index_field_set_sort_order(GDA_DB_INDEX_FIELD (it->data),
                                                gda_db_index_field_get_sort_order (field));
            }
        }
    }
}

/**
 * gda_db_index_remove_field:
 * @self: instance of #GdaDbIndex
 * @name: Name of the column where field should be removed.
 *
 * Stability: Stable
 * Since: 6.0
 */
void
gda_db_index_remove_field (GdaDbIndex *self,
                           const gchar *name)
{
  G_DEBUG_HERE();
  g_return_if_fail (self);
  g_return_if_fail (name);

  GdaDbIndexPrivate *priv = gda_db_index_get_instance_private (self);

  GSList *it = priv->mFieldList;
  for (; it != NULL; it = it->next)
    {
      const gchar *full_name = gda_db_base_get_full_name (GDA_DB_BASE (it->data));

      if (!g_strcmp0 (full_name, name))
        priv->mFieldList = g_slist_remove (priv->mFieldList, it->data);
    }
}

/**
 * gda_db_index_get_fields:
 * @self: an instance of #GdaDbIndex
 *
 * This function is thread safe, that is, @cnc will be locked.
 *
 * Returns: (transfer none) (nullable) (element-type Gda.DbIndexField): A list of #GdaDbIndexField
 *
 * Stability: Stable
 * Since: 6.0
 */
GSList *
gda_db_index_get_fields (GdaDbIndex *self)
{
  G_DEBUG_HERE();
  GdaDbIndexPrivate *priv = gda_db_index_get_instance_private (self);

  return priv->mFieldList;
}

/**
 * gda_db_index_drop:
 * @self: an instance of #GdaDbIndex to work to drop
 * @cnc: opened connection as the #GdaConnection
 * @ifexists: flag for IF EXSISTS
 * @error: an error storage
 *
 * Drop the index from the DB.
 *
 * Stability: Stable
 * Since: 6.0
 */
static gboolean
gda_db_index_drop (GdaDdlModifiable *self,
                   GdaConnection *cnc,
                   gpointer user_data,
                   GError **error)
{
  G_DEBUG_HERE();
  g_return_val_if_fail (GDA_IS_DB_INDEX (self), FALSE);

  GdaServerProvider *provider = NULL;
  GdaServerOperation *op = NULL;

  gda_lockable_lock (GDA_LOCKABLE (cnc));

  provider = gda_connection_get_provider (cnc);

  op = gda_server_provider_create_operation (provider,
                                             cnc,
                                             GDA_SERVER_OPERATION_DROP_INDEX,
                                             NULL,
                                             error);
  if (!op)
    goto on_error;

  if (!gda_server_operation_set_value_at (op, gda_db_base_get_full_name (GDA_DB_BASE (self)),
                                          error, "/INDEX_DESC_P/INDEX_NAME"))
    goto on_error;

  if (!gda_server_operation_set_value_at (op, GDA_BOOL_TO_STR (TRUE), error,
                                          "/INDEX_DESC_P/INDEX_IFEXISTS"))
    goto on_error;

  if (!gda_server_provider_perform_operation (provider, cnc, op, error))
    goto on_error;

  g_object_unref (op);

  gda_lockable_unlock (GDA_LOCKABLE (cnc));

  return TRUE;

on_error:
  if (op) g_object_unref (op);

  gda_lockable_unlock (GDA_LOCKABLE (cnc));

  return FALSE;
}

/**
 * gda_db_index_create:
 * @self is a instance of GdaDbIndex
 * @cnc is a opened connection
 * @user_data is ignored
 * @error an error holder
 *
 */
static gboolean
gda_db_index_create (GdaDdlModifiable *self,
                     GdaConnection *cnc,
                     gpointer user_data,
                     GError **error)
{
  G_DEBUG_HERE();
  GdaServerProvider *provider = NULL;
  GdaServerOperation *op = NULL;

  GdaDbIndex *index = GDA_DB_INDEX (self);
  GdaDbTable *table = NULL;

  gda_lockable_lock (GDA_LOCKABLE (cnc));

  provider = gda_connection_get_provider (cnc);

  op = gda_server_provider_create_operation (provider, cnc, GDA_SERVER_OPERATION_CREATE_INDEX,
                                             NULL, error);

  if (!op)
    goto on_error;

  if (gda_db_index_get_unique (index))
    {
      if (!gda_server_operation_set_value_at (op, "UNIQUE", error,
                                              "/INDEX_DEF_P/INDEX_TYPE"))
        goto on_error;
    }


  if (!gda_server_operation_set_value_at (op, gda_db_base_get_full_name (GDA_DB_BASE (index)),
                                          error, "/INDEX_DEF_P/INDEX_NAME"))
    goto on_error;

  g_object_get (index, "table", &table, NULL);

  if (!table)
    goto on_error;

  if (!gda_server_operation_set_value_at (op, gda_db_base_get_name (GDA_DB_BASE (table)),
                                          error, "/INDEX_DEF_P/INDEX_ON_TABLE"))
    goto on_error;

  g_object_unref (table);

  if (!gda_server_operation_set_value_at (op, "TRUE",
                                          error, "/INDEX_DEF_P/INDEX_IFNOTEXISTS"))
    goto on_error;

  GSList *it = gda_db_index_get_fields (index);
  gint i = 0;

  for (; it != NULL; it = it->next, i++)
    {
      GdaDbColumn *col = gda_db_index_field_get_column (GDA_DB_INDEX_FIELD (it->data));

      if (!gda_server_operation_set_value_at (op,
                                              gda_db_column_get_name (col),
                                              error,
                                              "/INDEX_FIELDS_S/%d/INDEX_FIELD",
                                              i))
        goto on_error;

      if (!gda_server_operation_set_value_at (op,
                                              gda_db_index_field_get_collate (it->data),
                                              error,
                                              "/INDEX_FIELDS_S/%d/INDEX_COLLATE",
                                              i))
        goto on_error;

      if (!gda_server_operation_set_value_at (op,
                                              gda_db_index_field_get_sort_order_str (it->data),
                                              error,
                                              "/INDEX_FIELDS_S/%d/INDEX_SORT_ORDER",
                                              i))
        goto on_error;
    }

  if (!gda_server_provider_perform_operation (provider, cnc, op, error))
    goto on_error;

  g_object_unref (op);

  gda_lockable_unlock (GDA_LOCKABLE (cnc));

  return TRUE;

on_error:
  if (op) g_object_unref (op);

  if (table) g_object_unref (table);

  gda_lockable_unlock (GDA_LOCKABLE (cnc));

  return FALSE;
}

static gboolean
gda_db_index_rename (GdaDdlModifiable *old_name,
                     GdaConnection *cnc,
                     gpointer new_name,
                     GError **error)
{
  g_set_error (error, GDA_DDL_MODIFIABLE_ERROR,
               GDA_DDL_MODIFIABLE_NOT_IMPLEMENTED,
               _("Operation is not implemented for the used provider"));

  return FALSE;
}
