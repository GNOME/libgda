/* gda-db-table.c
 *
 * Copyright (C) 2018-2019 Pavlo Solntsev <p.sun.fun@gmail.com>
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
#define G_LOG_DOMAIN "GDA-db-table"

#include "gda-db-table.h"
#include "gda-db-fkey.h"
#include "gda-db-column.h"
#include <libgda/sql-parser/gda-sql-statement.h>
#include <libgda/gda-lockable.h>
#include "gda-server-provider.h"
#include "gda-connection.h"
#include "gda-meta-struct.h"
#include <glib/gi18n-lib.h>

G_DEFINE_QUARK (gda_db_table_error, gda_db_table_error)

typedef struct
{
  gchar *mp_comment;

  gboolean m_istemp;
  GList *mp_columns; /* Type GdaDbColumn*/
  GList *mp_fkeys; /* List of all fkeys, GdaDbFkey */
} GdaDbTablePrivate;

/**
 * SECTION:gda-db-table
 * @title: GdaDbTable
 * @short_description: Object to represent table database object
 * @see_also: #GdaDbView, #GdaDbCatalog
 * @stability: Stable
 * @include: libgda/libgda.h
 *
 * This object represents a table of a database. The table view can be constracted manually
 * using API or generated from xml file together with other databse objects. See #GdaDbCatalog.
 * #GdaDbTable implements #GdaDbBuildable interface for parsing xml file.
 */

static void gda_db_table_buildable_interface_init (GdaDbBuildableInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GdaDbTable, gda_db_table, GDA_TYPE_DB_BASE,
                         G_ADD_PRIVATE (GdaDbTable)
                         G_IMPLEMENT_INTERFACE (GDA_TYPE_DB_BUILDABLE,
                                                gda_db_table_buildable_interface_init))

enum {
    PROP_0,
    PROP_COMMENT,
    PROP_ISTEMP,
    /* <private> */
    N_PROPS
};

static GParamSpec *properties [N_PROPS] = {NULL};

/* This is a convenient way to name all nodes from xml file */
enum {
    GDA_DB_TABLE_NODE,
    GDA_DB_TABLE_NAME,
    GDA_DB_TABLE_TEMP,
    GDA_DB_TABLE_SPACE,
    GDA_DB_TABLE_COMMENT,

    GDA_DB_TABLE_N_NODES
};

static const gchar *gdadbtablenodes[GDA_DB_TABLE_N_NODES] = {
    "table",
    "name",
    "temptable",
    "space",
    "comment"
};

/**
 * gda_db_table_new:
 *
 * Returns: New instance of #GdaDbTable.
 *
 * Since: 6.0
 */
GdaDbTable *
gda_db_table_new (void)
{
  return g_object_new (GDA_TYPE_DB_TABLE, NULL);
}

static void
gda_db_table_finalize (GObject *object)
{
  GdaDbTable *self = (GdaDbTable *)object;
  GdaDbTablePrivate *priv = gda_db_table_get_instance_private (self);

  g_free (priv->mp_comment);

  G_OBJECT_CLASS (gda_db_table_parent_class)->finalize (object);
}

static void
gda_db_table_dispose (GObject *object)
{
  GdaDbTable *self = (GdaDbTable *)object;
  GdaDbTablePrivate *priv = gda_db_table_get_instance_private (self);

  if (priv->mp_fkeys)
    g_list_free_full (priv->mp_fkeys, (GDestroyNotify) g_object_unref);
  if (priv->mp_columns)
    g_list_free_full (priv->mp_columns, (GDestroyNotify)g_object_unref);

  G_OBJECT_CLASS (gda_db_table_parent_class)->dispose (object);
}

static void
gda_db_table_get_property (GObject    *object,
                           guint       prop_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  GdaDbTable *self = GDA_DB_TABLE (object);
  GdaDbTablePrivate *priv = gda_db_table_get_instance_private (self);

  switch (prop_id) {
    case PROP_COMMENT:
      g_value_set_string (value, priv->mp_comment);
      break;
    case PROP_ISTEMP:
      g_value_set_boolean (value, priv->m_istemp);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
gda_db_table_set_property (GObject      *object,
                           guint         prop_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  GdaDbTable *self = GDA_DB_TABLE (object);
  GdaDbTablePrivate *priv = gda_db_table_get_instance_private (self);

  switch (prop_id) {
    case PROP_COMMENT:
      g_free (priv->mp_comment);
      priv->mp_comment = g_value_dup_string (value);
      break;
    case PROP_ISTEMP:
      priv->m_istemp = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
gda_db_table_class_init (GdaDbTableClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gda_db_table_finalize;
  object_class->dispose = gda_db_table_dispose;
  object_class->get_property = gda_db_table_get_property;
  object_class->set_property = gda_db_table_set_property;

  properties[PROP_COMMENT] = g_param_spec_string ("comment",
                                                  "Comment",
                                                  "Table comment",
                                                  NULL,
                                                  G_PARAM_READWRITE);
  properties[PROP_ISTEMP] = g_param_spec_string ("istemp",
                                                 "temporary-table",
                                                 "Is table temporary",
                                                 NULL,
                                                 G_PARAM_READWRITE);

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gda_db_table_init (GdaDbTable *self)
{
  GdaDbTablePrivate *priv = gda_db_table_get_instance_private (self);

  priv->mp_columns = NULL;
  priv->m_istemp = FALSE;
  priv->mp_comment = NULL;
  priv->mp_fkeys = NULL;
}

/**
 * gda_db_table_parse_node:
 * @self: a #GdaDbTable object
 * @node: instance of #xmlNodePtr to parse
 * @error: #GError to handle an error
 *
 * Parse @node wich should point to the <table> node
 *
 * Returns: %TRUE if no error, %FALSE otherwise
 *
 * Since: 6.0
 */
static gboolean
gda_db_table_parse_node (GdaDbBuildable *buildable,
                         xmlNodePtr	node,
                         GError      **error)
{
  g_return_val_if_fail (buildable, FALSE);
  g_return_val_if_fail (node, FALSE);

  GdaDbTable *self = GDA_DB_TABLE (buildable);

  GdaDbTablePrivate *priv = gda_db_table_get_instance_private (self);

  xmlChar *name = NULL;
  name = xmlGetProp (node, (xmlChar *)gdadbtablenodes[GDA_DB_TABLE_NAME]);
  g_assert (name); /* If here bug with xml validation */
  gda_db_base_set_name(GDA_DB_BASE(self), (gchar*)name);

  xmlChar *tempt = xmlGetProp (node, (xmlChar*)gdadbtablenodes[GDA_DB_TABLE_TEMP]);

  if (tempt && (*tempt == 't' || *tempt == 't'))
    {
      g_object_set (G_OBJECT(self), "istemp", TRUE, NULL);
      xmlFree (tempt);
    }

  for (xmlNodePtr it = node->children; it ; it = it->next)
    {
      if (!g_strcmp0 ((gchar *) it->name, gdadbtablenodes[GDA_DB_TABLE_COMMENT]))
        {
          xmlChar *comment = xmlNodeGetContent (it);

          if (comment)
            {
              g_object_set (G_OBJECT(self), "comment", (char*)comment, NULL);
              xmlFree (comment);
            }
        }
      else if (!g_strcmp0 ((gchar *)it->name, "column"))
        {
          GdaDbColumn *column = gda_db_column_new ();

          if (!gda_db_buildable_parse_node(GDA_DB_BUILDABLE (column), it, error))
            {
              g_object_unref(column);
              return FALSE;
            }
          else
            priv->mp_columns = g_list_append (priv->mp_columns, column);
        }
      else if (!g_strcmp0 ((gchar*)it->name, "fkey"))
        {
          GdaDbFkey *fkey;
          fkey = gda_db_fkey_new ();

          if (!gda_db_buildable_parse_node (GDA_DB_BUILDABLE(fkey), it, error))
            {
              g_object_unref (fkey);
              return FALSE;
            }
          else
            priv->mp_fkeys = g_list_append (priv->mp_fkeys, fkey);
        } /* end of else if */
    } /* End of for loop */
  return TRUE;
} /* End of gda_db_column_parse_node */

static gboolean
gda_db_table_write_node (GdaDbBuildable *buildable,
                         xmlNodePtr       rootnode,
                         GError         **error)
{
  g_return_val_if_fail (buildable, FALSE);
  g_return_val_if_fail (rootnode, FALSE);

  GdaDbTable *self = GDA_DB_TABLE (buildable);
  GdaDbTablePrivate *priv = gda_db_table_get_instance_private (self);

  xmlNodePtr node = NULL;
  node  = xmlNewChild (rootnode, NULL, (xmlChar*)gdadbtablenodes[GDA_DB_TABLE_NODE], NULL);
  xmlNewProp (node, BAD_CAST gdadbtablenodes[GDA_DB_TABLE_NAME],
              BAD_CAST gda_db_base_get_name (GDA_DB_BASE(self)));

  xmlNewProp (node, BAD_CAST gdadbtablenodes[GDA_DB_TABLE_TEMP],
              BAD_CAST GDA_BOOL_TO_STR (priv->m_istemp));

  xmlNewChild (node, NULL,
               (xmlChar*)gdadbtablenodes[GDA_DB_TABLE_COMMENT],
               (xmlChar*)priv->mp_comment);

  GList *it = NULL;

  for (it = priv->mp_columns; it; it=it->next)
    if(!gda_db_buildable_write_node (GDA_DB_BUILDABLE(GDA_DB_COLUMN(it->data)), node,error))
      return FALSE;

  return TRUE;
}

static void
gda_db_table_buildable_interface_init (GdaDbBuildableInterface *iface)
{
  iface->parse_node = gda_db_table_parse_node;
  iface->write_node = gda_db_table_write_node;
}

/**
 * gda_db_table_set_comment:
 * @self: an #GdaDbTable object
 * @tablecomment: Comment to set as a string
 *
 * If @tablecomment is %NULL nothing is happened. @tablecomment will not be set
 * to %NULL.
 *
 * Since: 6.0
 */
void
gda_db_table_set_comment (GdaDbTable *self,
                          const char  *tablecomment)
{
  g_return_if_fail (self);

  if (tablecomment)
    {
      GdaDbTablePrivate *priv = gda_db_table_get_instance_private (self);
      g_free (priv->mp_comment);
      priv->mp_comment = g_utf8_strup (tablecomment, g_utf8_strlen (tablecomment, -1));
    }
}

/**
 * gda_db_table_set_is_temp:
 * @self: a #GdaDbTable object
 * @istemp: Set if the table should be temporary
 *
 * Set if the table should be temporary or not.  %FALSE is set by default.
 *
 * Since: 6.0
 */
void
gda_db_table_set_is_temp (GdaDbTable *self,
                           gboolean istemp)
{
  g_return_if_fail (self);
  GdaDbTablePrivate *priv = gda_db_table_get_instance_private (self);
  priv->m_istemp = istemp;
}

/**
 * gda_db_table_get_comment:
 * @self: a #GdaDbTable object
 *
 * Returns: A comment string or %NULL if comment wasn't set.
 *
 * Since: 6.0
 */
const char*
gda_db_table_get_comment (GdaDbTable *self)
{
  g_return_val_if_fail (self, NULL);
  GdaDbTablePrivate *priv = gda_db_table_get_instance_private (self);
  return priv->mp_comment;
}

/**
 * gda_db_table_get_temp:
 * @self: a #GdaDbTable object
 *
 * Returns temp key. If %TRUE table should be temporary, %FALSE otherwise.
 * If @self is %NULL, this function aborts. So check @self before calling this
 * method.
 *
 * Since: 6.0
 */
gboolean
gda_db_table_get_temp (GdaDbTable *self)
{
  g_return_val_if_fail (self, FALSE);

  GdaDbTablePrivate *priv = gda_db_table_get_instance_private (self);
  return priv->m_istemp;
}

/**
 * gda_db_table_is_valid:
 * @self: a #GdaDbTable instance
 *
 * This method returns %TRUE if at least one column is added to the table. It ruturns %FALSE if the
 * table has no columns.
 *
 * Returns: %TRUE or %FALSE
 *
 * Since: 6.0
 */
gboolean
gda_db_table_is_valid (GdaDbTable *self)
{
  g_return_val_if_fail (self, FALSE);

  GdaDbTablePrivate *priv = gda_db_table_get_instance_private (self);

/* TODO: It may be worthwhile to have the same method for all children elements and
 * call this method on each children member to make sure whole table is valid
 * */
  if (priv->mp_columns)
    return TRUE;
  else
    return FALSE;
}

/**
 * gda_db_table_get_columns:
 * @self: an #GdaDbTable object
 *
 * Use this method to obtain internal list of all columns. The internal list
 * should not be freed.
 *
 * Returns: (element-type Gda.DbColumn) (transfer none): A list of #GdaDbColumn objects or %NULL
 * if the internal list is not set or if %NULL is passed.
 *
 * Since: 6.0
 *
 */
GList*
gda_db_table_get_columns (GdaDbTable *self)
{
  g_return_val_if_fail (self, NULL);
  GdaDbTablePrivate *priv = gda_db_table_get_instance_private (self);

  return priv->mp_columns;
}

/**
 * gda_db_table_get_fkeys:
 * @self: A #GdaDbTable object
 *
 * Use this method to obtain internal list of all fkeys. The internal list
 * should not be freed.
 *
 * Returns: (transfer none) (element-type Gda.DbFkey): A list of #GdaDbFkey objects or %NULL if
 * the internal list is not set or %NULL is passed
 *
 * Since: 6.0
 */
GList*
gda_db_table_get_fkeys (GdaDbTable *self)
{
  g_return_val_if_fail (self, NULL);
  GdaDbTablePrivate *priv = gda_db_table_get_instance_private (self);

  return priv->mp_fkeys;
}

/**
 * gda_db_table_is_temp:
 * @self: a #GdaDbTable object
 *
 * Checks if the table is temporary
 *
 * Returns: %TRUE if table is temporary, %FALSE otherwise.
 *
 * Since: 6.0
 */
gboolean
gda_db_table_get_is_temp (GdaDbTable *self)
{
  g_return_val_if_fail (GDA_IS_DB_TABLE(self), FALSE);

  GdaDbTablePrivate *priv = gda_db_table_get_instance_private (self);
  return priv->m_istemp;
}

/**
 * gda_db_table_prepare_create:
 * @self: a #GdaDbTable instance
 * @op: an instance of #GdaServerOperation to populate.
 * @error: error container
 *
 * Populate @op with information stored in @self. This method sets @op to execute CREATE_TABLE
 * operation.
 *
 * Returns: %TRUE if no error occured and %FALSE otherwise.
 *
 * Since: 6.0
 */
gboolean
gda_db_table_prepare_create (GdaDbTable *self,
                             GdaServerOperation *op,
                             gboolean ifnotexists,
                             GError **error)
{
  GdaDbTablePrivate *priv = gda_db_table_get_instance_private (self);

  if (!gda_server_operation_set_value_at (op,
                                          gda_db_base_get_name (GDA_DB_BASE(self)),
                                          error,
                                          "/TABLE_DEF_P/TABLE_NAME"))
    return FALSE;

  if (!gda_server_operation_set_value_at (op,
                                          GDA_BOOL_TO_STR(priv->m_istemp),
                                          error,
                                          "/TABLE_DEF_P/TABLE_TEMP"))
    return FALSE;

  if (!gda_server_operation_set_value_at (op,
                                          priv->mp_comment,
                                          error,
                                          "/TABLE_DEF_P/TABLE_COMMENT"))
    return FALSE;

  if (!gda_server_operation_set_value_at (op,
                                          GDA_BOOL_TO_STR (ifnotexists),
                                          error,
                                          "/TABLE_DEF_P/TABLE_IFNOTEXISTS"))
    return FALSE;

  GList *it = NULL;
  gint i = 0; /* column order counter */

  for (it = priv->mp_columns;it;it=it->next)
    if(!gda_db_column_prepare_create (GDA_DB_COLUMN (it->data), op, i++, error))
      return FALSE;

  i = 0;
  for (it = priv->mp_fkeys;it;it=it->next)
    if(!gda_db_fkey_prepare_create (GDA_DB_FKEY(it->data), op, i++, error))
      return FALSE;

  return TRUE;
}

static gint
_gda_db_compare_column_meta (GdaMetaTableColumn *a,
                             GdaDbColumn *b)
{
  if (a && !b)
    return 1;
  else if (!a && b)
    return -1;
  else if (!a && !b)
    return 0;

  const gchar *namea = a->column_name;
  const gchar *nameb = gda_db_column_get_name (b);

	g_debug("Checking %s and %s\n", namea, nameb);
  return g_strcmp0 (namea,nameb);
}

/**
 * gda_db_table_update:
 * @self: a #GdaDbTable instance
 * @obj: The corresponding meta object to take data from
 * @cnc: opened connection
 * @error: error container
 *
 * With this method object @obj in the database available through @cnc will be updated using
 * ADD_COLUMN operation with information stored in @self.
 *
 * Returns: %TRUE if no error and %FALSE otherwise
 */
gboolean
gda_db_table_update (GdaDbTable *self,
                     GdaMetaTable *obj,
                     GdaConnection *cnc,
                     GError **error)
{
  g_return_val_if_fail (self, FALSE);
  g_return_val_if_fail (obj, FALSE);
  g_return_val_if_fail (cnc, FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  if (!gda_connection_is_opened (cnc))
    return FALSE;

  GdaDbTablePrivate *priv = gda_db_table_get_instance_private (self);

  if (!obj->columns)
    {
      g_set_error (error, GDA_DB_TABLE_ERROR, GDA_DB_TABLE_COLUMN_EMPTY, _("Empty column list"));
      return FALSE;
    }

  GSList *dbcolumns = NULL;
  GList *it = NULL;

  GdaServerProvider *provider = NULL;
  GdaServerOperation *op = NULL;

  dbcolumns = obj->columns;

  gda_lockable_lock ((GdaLockable*)cnc);

  provider = gda_connection_get_provider (cnc);

  op = gda_server_provider_create_operation (provider,
                                             cnc,
                                             GDA_SERVER_OPERATION_ADD_COLUMN,
                                             NULL,
                                             error);
  if (!op)
    goto on_error;

  if(!gda_server_operation_set_value_at (op,
                                         gda_db_base_get_full_name (GDA_DB_BASE(self)),
                                         error,
                                         "/COLUMN_DEF_P/TABLE_NAME"))
    goto on_error;

  gint newcolumncount = 0;
  for (it = priv->mp_columns;it;it=it->next)
    {
      GSList *res = g_slist_find_custom (dbcolumns,
                                         it->data,
                                         (GCompareFunc)_gda_db_compare_column_meta);

      if (res) /* If object is present we need go to next element */
        continue;
      else
        newcolumncount++; /* We need to count new column. See below */

      if(!gda_db_column_prepare_add (it->data, op, error))
        {
          g_slist_free (res);
          goto on_error;
        }
    }

  if (newcolumncount != 0) /* Run operation only if at least on column is present*/
    if(!gda_server_provider_perform_operation (provider, cnc, op, error))
      goto on_error;

  g_object_unref(op);
  gda_lockable_unlock ((GdaLockable*)cnc);
  return TRUE;

on_error:
  g_object_unref(op);
  gda_lockable_unlock ((GdaLockable*)cnc);
  return FALSE;
}

/**
 * gda_db_table_create:
 * @self: a #GdaDbTable object
 * @cnc: a #GdaConnection object
 * @ifnotexists: Set to %TRUE if table should be created with "IFNOTEXISTS" option
 * @error: container for error storage
 *
 * Execute a full set of steps to create tabe in the database.
 * This method is called with "IFNOTEXISTS" option.
 *
 * Returns: %TRUE if successful, %FALSE otherwise
 *
 * Since: 6.0
 */
gboolean
gda_db_table_create (GdaDbTable *self,
                     GdaConnection *cnc,
                     gboolean ifnotexists,
                     GError **error)
{
  g_return_val_if_fail (self, FALSE);
  g_return_val_if_fail (cnc, FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

 if (!gda_connection_is_opened (cnc))
    {
      g_set_error (error,
                   GDA_DB_CATALOG_ERROR,
                   GDA_DB_CATALOG_CONNECTION_CLOSED,
                   _("Connection is not opened"));
      return FALSE;
    }
  gda_lockable_lock (GDA_LOCKABLE(cnc));

  GdaServerProvider *provider = NULL;
  GdaServerOperation *op = NULL;
  gboolean res = FALSE;

  provider = gda_connection_get_provider (cnc);

  op = gda_server_provider_create_operation (provider,
                                             cnc,
                                             GDA_SERVER_OPERATION_CREATE_TABLE,
                                             NULL,
                                             error);
  if (op) 
    {
      g_object_set_data_full (G_OBJECT (op), "connection", g_object_ref (cnc), g_object_unref);

      if (gda_db_table_prepare_create (self, op, ifnotexists, error)) 
        {
#ifdef GDA_DEBUG_NO
          gchar* str = gda_server_operation_render (op, error);
          g_message ("Operation: %s", str);
          g_free (str);
#endif
          res = gda_server_provider_perform_operation (provider, cnc, op, error);
        }
      g_object_unref (op);
    }
  gda_lockable_unlock (GDA_LOCKABLE(cnc));

  return res;
}

/**
 * gda_db_table_append_column:
 * @self: a #GdaDbTable instance
 * @column: column to add
 *
 * Append @column to the internal list of columns
 *
 * Since: 6.0
 */
void
gda_db_table_append_column (GdaDbTable *self,
                            GdaDbColumn *column)
{
  g_return_if_fail (self);
  g_return_if_fail (column);

  GdaDbTablePrivate *priv = gda_db_table_get_instance_private (self);

  priv->mp_columns = g_list_append (priv->mp_columns, g_object_ref (column));
}

/**
 * gda_db_table_append_fkey:
 * @self: a #GdaDbTable instance
 * @fkey: fkry to add
 *
 * Append @fkey to the internal list of columns
 *
 * Since: 6.0
 */
void
gda_db_table_append_fkey (GdaDbTable *self,
                          GdaDbFkey *fkey)
{
  g_return_if_fail (self);

  GdaDbTablePrivate *priv = gda_db_table_get_instance_private (self);

  priv->mp_fkeys = g_list_append (priv->mp_fkeys, g_object_ref (fkey));
}

