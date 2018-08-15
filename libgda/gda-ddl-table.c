/* gda-ddl-table.c
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

#include "gda-ddl-table.h"
#include "gda-ddl-fkey.h"
#include "gda-ddl-column.h"
#include <libgda/sql-parser/gda-sql-statement.h>
#include <libgda/gda-lockable.h>
#include "gda-server-provider.h"
#include "gda-connection.h"
#include "gda-meta-struct.h"
#include <glib/gi18n-lib.h>

G_DEFINE_QUARK (gda_ddl_table_error,gda_ddl_table_error)

typedef struct
{
  gchar *mp_comment;

  gboolean m_istemp;
  GList *mp_columns; /* Type GdaDdlColumn*/
  GList *mp_fkeys; /* List of all fkeys, GdaDdlFkey */
} GdaDdlTablePrivate;

/**
 * SECTION:gda-ddl-table
 * @title: GdaDdlTable
 * @short_description: Object to represent table database object
 * @see_also: #GdaDdlView, #GdaDdlCreator
 * @stability: Stable
 * @include: libgda/libgda.h
 *
 * This object represents a table of a database. The table view can be constracted manually
 * using API or generated from xml file together with other databse objects. See #GdaDdlCreator.
 * #GdaDdlTable implements #GdaDdlBuildable interface for parsing xml file.
 */

static void gda_ddl_table_buildable_interface_init (GdaDdlBuildableInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GdaDdlTable, gda_ddl_table, GDA_TYPE_DDL_BASE,
                         G_ADD_PRIVATE (GdaDdlTable)
                         G_IMPLEMENT_INTERFACE (GDA_TYPE_DDL_BUILDABLE,
                                                gda_ddl_table_buildable_interface_init))

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
  GDA_DDL_TABLE_NODE,
  GDA_DDL_TABLE_NAME,
  GDA_DDL_TABLE_TEMP,
  GDA_DDL_TABLE_SPACE,
  GDA_DDL_TABLE_COMMENT,

  GDA_DDL_TABLE_N_NODES
};

const gchar *gdaddltablenodes[GDA_DDL_TABLE_N_NODES] = {
  "table",
  "name",
  "temptable",
  "space",
  "comment"
};

/**
 * gda_ddl_table_new:
 *
 * Returns: New instance of #GdaDdlTable.
 */
GdaDdlTable *
gda_ddl_table_new (void)
{
  return g_object_new (GDA_TYPE_DDL_TABLE, NULL);
}

static void
gda_ddl_table_finalize (GObject *object)
{
  GdaDdlTable *self = (GdaDdlTable *)object;
  GdaDdlTablePrivate *priv = gda_ddl_table_get_instance_private (self);

  if (priv->mp_fkeys)
    g_list_free_full (priv->mp_fkeys, (GDestroyNotify) g_object_unref);
  if (priv->mp_columns)
    g_list_free_full (priv->mp_columns, (GDestroyNotify)gda_ddl_column_free);

  g_free (priv->mp_comment);

  G_OBJECT_CLASS (gda_ddl_table_parent_class)->finalize (object);
}

static void
gda_ddl_table_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  GdaDdlTable *self = GDA_DDL_TABLE (object);
  GdaDdlTablePrivate *priv = gda_ddl_table_get_instance_private (self);

  switch (prop_id) {
    case PROP_COMMENT:
      g_value_set_string (value,priv->mp_comment);
      break;
    case PROP_ISTEMP:
      g_value_set_boolean (value,priv->m_istemp);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }

}

static void
gda_ddl_table_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  GdaDdlTable *self = GDA_DDL_TABLE (object);
  GdaDdlTablePrivate *priv = gda_ddl_table_get_instance_private (self);

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
gda_ddl_table_class_init (GdaDdlTableClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gda_ddl_table_finalize;
  object_class->get_property = gda_ddl_table_get_property;
  object_class->set_property = gda_ddl_table_set_property;

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

  g_object_class_install_properties (object_class,N_PROPS,properties);
}

static void
gda_ddl_table_init (GdaDdlTable *self)
{
  GdaDdlTablePrivate *priv = gda_ddl_table_get_instance_private (self);

  priv->mp_columns = NULL;
  priv->m_istemp = FALSE;
  priv->mp_comment = NULL;
  priv->mp_fkeys = NULL;
}

/**
 * gda_ddl_table_parse_node:
 * @self: a #GdaDdlTable object
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
gda_ddl_table_parse_node (GdaDdlBuildable *buildable,
                          xmlNodePtr	node,
                          GError      **error)
{
  g_return_val_if_fail (buildable,FALSE);
  g_return_val_if_fail (node, FALSE);

  GdaDdlTable *self = GDA_DDL_TABLE (buildable);

  GdaDdlTablePrivate *priv = gda_ddl_table_get_instance_private (self);

  xmlChar *name = NULL;
  name = xmlGetProp (node,(xmlChar *)gdaddltablenodes[GDA_DDL_TABLE_NAME]);
  g_assert (name); /* If here bug with xml validation */
  gda_ddl_base_set_name(GDA_DDL_BASE(self),(gchar *)name);

  xmlChar *tempt = xmlGetProp (node,(xmlChar*)gdaddltablenodes[GDA_DDL_TABLE_TEMP]);

  if (tempt && (*tempt == 't' || *tempt == 't'))
    {
      g_object_set (G_OBJECT(self),"istemp",TRUE,NULL);
      xmlFree (tempt);
    }

  for (xmlNodePtr it = node->children; it ; it = it->next)
    {
      if (!g_strcmp0 ((gchar *) it->name,gdaddltablenodes[GDA_DDL_TABLE_COMMENT]))
        {
          xmlChar *comment = xmlNodeGetContent (it);

          if (comment)
            {
              g_object_set (G_OBJECT(self),"comment", (char *)comment,NULL);
              xmlFree (comment);
            }
        } 
      else if (!g_strcmp0 ((gchar *) it->name, "column"))
        {
          GdaDdlColumn *column = gda_ddl_column_new ();

          if (!gda_ddl_buildable_parse_node(GDA_DDL_BUILDABLE (column),
                                            it, error))
            {
              gda_ddl_column_free (column);
              return FALSE;
            }
          else
            priv->mp_columns = g_list_append (priv->mp_columns,column);
        }
      else if (!g_strcmp0 ((gchar *) it->name, "fkey"))
        {
          GdaDdlFkey *fkey;
          fkey = gda_ddl_fkey_new ();

          if (!gda_ddl_buildable_parse_node (GDA_DDL_BUILDABLE(fkey), it, error)) {
              g_object_unref (fkey);
              return FALSE;
          } else
            priv->mp_fkeys = g_list_append (priv->mp_fkeys,fkey);
        } /* end of else if */
    } /* End of for loop */
  return TRUE;
} /* End of gda_ddl_column_parse_node */

static gboolean
gda_ddl_table_write_node (GdaDdlBuildable *buildable,
                          xmlNodePtr       rootnode,
                          GError         **error)
{
  g_return_val_if_fail (buildable,FALSE);
  g_return_val_if_fail (rootnode,FALSE);

  GdaDdlTable *self = GDA_DDL_TABLE (buildable);
  GdaDdlTablePrivate *priv = gda_ddl_table_get_instance_private (self);

  xmlNodePtr node = NULL;
  node  = xmlNewChild (rootnode,NULL,(xmlChar*)gdaddltablenodes[GDA_DDL_TABLE_NODE],NULL);
  xmlNewProp (node,BAD_CAST gdaddltablenodes[GDA_DDL_TABLE_NAME],
              BAD_CAST gda_ddl_base_get_name (GDA_DDL_BASE(self)));

  xmlNewProp (node,BAD_CAST gdaddltablenodes[GDA_DDL_TABLE_TEMP], 
              BAD_CAST GDA_BOOL_TO_STR (priv->m_istemp)); 

  xmlNewChild (node,NULL,
               (xmlChar*)gdaddltablenodes[GDA_DDL_TABLE_COMMENT],
               (xmlChar*)priv->mp_comment);

  GList *it = NULL;

  for (it = priv->mp_columns; it; it=it->next)
    if(!gda_ddl_buildable_write_node (GDA_DDL_BUILDABLE(GDA_DDL_COLUMN(it->data)),
                                      node,error))
      return FALSE;

  return TRUE;
}

static void
gda_ddl_table_buildable_interface_init (GdaDdlBuildableInterface *iface)
{
  iface->parse_node = gda_ddl_table_parse_node;
  iface->write_node = gda_ddl_table_write_node;
}

/**
 * gda_ddl_table_set_comment:
 * @self: an #GdaDdlTable object
 * @tablecomment: Comment to set as a string
 *
 * If @tablecomment is %NULL nothing is happened. @tablecomment will not be set
 * to %NULL.
 *
 * Since: 6.0
 */
void
gda_ddl_table_set_comment (GdaDdlTable *self,
                           const char  *tablecomment)
{
  g_return_if_fail (self);

  if (tablecomment) {
      GdaDdlTablePrivate *priv = gda_ddl_table_get_instance_private (self);
      g_free (priv->mp_comment);
      priv->mp_comment = g_utf8_strup (tablecomment,g_utf8_strlen (tablecomment,-1));
  }
}

/**
 * gda_ddl_table_set_temp:
 * @self: a #GdaDdlTable object
 * @istemp: Set if the table should be temporary
 *
 * Set if the table should be temporary or not.  False is set by default.
 *
 * Since: 6.0
 */
void
gda_ddl_table_set_temp (GdaDdlTable *self,
                        gboolean istemp)
{
  g_return_if_fail (self);
  GdaDdlTablePrivate *priv = gda_ddl_table_get_instance_private (self);
  priv->m_istemp = istemp;
}

/**
 * gda_ddl_table_get_comment:
 * @self: a #GdaDdlTable object
 *
 * Returns: A comment string or %NULL if comment wasn't set.
 */
const char*
gda_ddl_table_get_comment (GdaDdlTable *self)
{
  g_return_val_if_fail (self, NULL);
  GdaDdlTablePrivate *priv = gda_ddl_table_get_instance_private (self);
  return priv->mp_comment;
}

/**
 * gda_ddl_table_get_temp:
 * @self: a #GdaDdlTable object
 *
 * Returns temp key. If %TRUE table should be temporary, %FALSE otherwise.
 * If @self is %NULL, this function aborts. So check @self before calling this
 * method.
 *
 * Since: 6.0
 */
gboolean
gda_ddl_table_get_temp (GdaDdlTable *self)
{
  g_return_val_if_fail (self,FALSE);

  GdaDdlTablePrivate *priv = gda_ddl_table_get_instance_private (self);
  return priv->m_istemp;
}

gboolean
gda_ddl_table_is_valid (GdaDdlTable *self)
{
  g_return_val_if_fail (self,FALSE);

  GdaDdlTablePrivate *priv = gda_ddl_table_get_instance_private (self);

/* TODO: It may be worthwhile to have the same method for all children elements and
 * call this method on each children member to make sure whole table is valid
 * */
  if (priv->mp_columns)
    return TRUE;
  else
    return FALSE;
}

/**
 * gda_ddl_table_get_columns:
 * @self: an #GdaDdlTable object
 *
 * Use this method to obtain internal list of all columns. The internal list
 * should not be freed.
 *
 * Returns: (element-type Gda.DdlColumn) (transfer none): A list of #GdaDdlColumn objects or %NULL if the internal list is
 * not set or if %NULL is passed.
 *
 * Since: 6.0
 */
const GList*
gda_ddl_table_get_columns (GdaDdlTable *self)
{
  g_return_val_if_fail (self, NULL);
  GdaDdlTablePrivate *priv = gda_ddl_table_get_instance_private (self);

  return priv->mp_columns;
}

/**
 * gda_ddl_table_get_fkeys:
 * @self: A #GdaDdlTable object
 *
 * Use this method to obtain internal list of all fkeys. The internal list
 * should not be freed.
 *
 * Returns: (element-type Gda.DdlFkey): A list of #GdaDdlFkey objects or %NULL if the internal list is not
 * set or %NULL is passed
 *
 * Since: 6.0
 */
const GList*
gda_ddl_table_get_fkeys (GdaDdlTable *self)
{
  g_return_val_if_fail (self, NULL);
  GdaDdlTablePrivate *priv = gda_ddl_table_get_instance_private (self);

  return priv->mp_fkeys;
}

/**
 * gda_ddl_table_is_temp:
 * @self: a #GdaDdlTable object
 *
 * Checks if the table is temporary
 *
 * Returns: %TRUE if table is temporary, %FALSE otherwise.
 *
 * Since: 6.0
 */
gboolean
gda_ddl_table_is_temp (GdaDdlTable *self)
{
  g_return_val_if_fail (GDA_IS_DDL_TABLE(self), FALSE);

  GdaDdlTablePrivate *priv = gda_ddl_table_get_instance_private (self);
  return priv->m_istemp;
}

/**
 * gda_ddl_table_prepare_create:
 * @self: a #GdaDdlTable instance
 * @op: an instance of #GdaServerOperation to populate.
 * @error: error container 
 *
 * Populate @op with information stored in @self. This method sets @op to execute CREATE_TABLE
 * operation.
 *
 * Since: 6.0
 */
gboolean
gda_ddl_table_prepare_create (GdaDdlTable *self,
                              GdaServerOperation *op,
                              GError **error)
{
  GdaDdlTablePrivate *priv = gda_ddl_table_get_instance_private (self);

  if (!gda_server_operation_set_value_at(op,
                                         gda_ddl_base_get_name(GDA_DDL_BASE(self)),
                                         error,
                                         "/TABLE_DEF_P/TABLE_NAME"))
    return FALSE;

  if (!gda_server_operation_set_value_at(op,
                                         GDA_BOOL_TO_STR(priv->m_istemp),
                                         error,
                                         "/TABLE_DEF_P/TABLE_TEMP"))
    return FALSE;

  if (!gda_server_operation_set_value_at(op,
                                         priv->mp_comment,
                                         error,
                                         "/TABLE_DEF_P/TABLE_COMMENT"))
    return FALSE;

  if (!gda_server_operation_set_value_at(op,
                                         priv->mp_comment,
                                         error,
                                         "/TABLE_DEF_P/TABLE_IFNOTEXISTS"))
    return FALSE;
  
  GList *it = NULL;
  gint i = 0; /* column order counter */

  for (it = priv->mp_columns;it;it=it->next)
    if(!gda_ddl_column_prepare_create (GDA_DDL_COLUMN(it->data),op,i++,error))
      return FALSE;

  for (it = priv->mp_fkeys;it;it=it->next)
    if(!gda_ddl_fkey_prepare_create (it->data,op,error))
      return FALSE;

  return TRUE;
}

static gint
_gda_ddl_compare_column_meta(GdaMetaTableColumn *a,GdaDdlColumn *b)
{
  if (a && !b)
    return 1;
  else if (!a && b)
    return -1;
  else if (!a && !b)
    return 0;

  const gchar *namea = a->column_name;
  const gchar *nameb = gda_ddl_column_get_name(b); 

  return g_strcmp0(namea,nameb);
}

/**
 * gda_ddl_table_update:
 * @self: a #GdaDdlTable instance
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
gda_ddl_table_update (GdaDdlTable *self,
                      GdaMetaTable *obj,
                      GdaConnection *cnc,
                      GError **error)
{
  g_return_val_if_fail (self,FALSE);
  g_return_val_if_fail (obj,FALSE);
  g_return_val_if_fail (cnc,FALSE);
 
  if (!gda_connection_is_opened(cnc))
    return FALSE;

  GdaDdlTablePrivate *priv = gda_ddl_table_get_instance_private (self);
  
  if (!obj->columns)
    {
      g_set_error (error,GDA_DDL_TABLE_ERROR,GDA_DDL_TABLE_COLUMN_EMPTY,_("Empty column list"));
      return FALSE;
    }

  GSList *dbcolumns = NULL;
  GList *it = NULL;

  GdaServerProvider *provider = NULL;
  GdaServerOperation *op = NULL;

  dbcolumns = obj->columns;

  gda_lockable_lock((GdaLockable*)cnc);
 
  provider = gda_connection_get_provider (cnc);

  op = gda_server_provider_create_operation(provider,
                                            cnc,
                                            GDA_SERVER_OPERATION_ADD_COLUMN,
                                            NULL,
                                            error);
  if (!op)
    goto on_error;
 
  if(!gda_server_operation_set_value_at(op,
                                        gda_ddl_base_get_full_name(GDA_DDL_BASE(self)),
                                        error,
                                        "/COLUMN_DEF_P/TABLE_NAME"))
    goto on_error;
  
  gint newcolumncount = 0;
  for (it = priv->mp_columns;it;it=it->next)
    {
      GSList *res = g_slist_find_custom (dbcolumns,
                                         it->data,
                                         (GCompareFunc)_gda_ddl_compare_column_meta);

      if (res) /* If object is present we need go to next element */
        continue;
      else
        newcolumncount++; /* We need to count new column. See below */ 
          
      if(!gda_ddl_column_prepare_add(it->data,op,error))
        {
          g_slist_free (res);
          goto on_error;
        }
    }

  if (newcolumncount != 0) /* Run operation only if at least on column is present*/
    if(!gda_server_provider_perform_operation(provider,cnc,op,error))
      goto on_error;

  g_object_unref(op);
  gda_lockable_unlock((GdaLockable*)cnc);
  return TRUE;

on_error:
  g_object_unref(op);
  gda_lockable_unlock((GdaLockable*)cnc);
  return FALSE;
}

/**
 * gda_ddl_table_create:
 * @self: a #GdaDdlTable object
 * @cnc: a #GdaConnection object
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
gda_ddl_table_create (GdaDdlTable *self,
                      GdaConnection *cnc,
                      GError **error)
{
  g_return_val_if_fail (self,FALSE);
  g_return_val_if_fail (cnc,FALSE);

  if (!gda_connection_is_opened(cnc))
    return FALSE;
  
  gda_lockable_lock(GDA_LOCKABLE(cnc));

  GdaServerProvider *provider = NULL;
  GdaServerOperation *op = NULL;
 
  provider = gda_connection_get_provider (cnc);

  op = gda_server_provider_create_operation(provider,
                                            cnc,
                                            GDA_SERVER_OPERATION_CREATE_TABLE,
                                            NULL,
                                            error);
  if (!op)
    goto on_error;

  if (!gda_ddl_table_prepare_create(self,op,error))
    goto on_error;

  if(!gda_server_provider_perform_operation(provider,cnc,op,error))
    goto on_error;

  g_object_unref (op);
  gda_lockable_unlock(GDA_LOCKABLE(cnc));

  return TRUE;

on_error:
  g_object_unref (op);
  gda_lockable_unlock(GDA_LOCKABLE(cnc));
  return FALSE;
}

/**
 * gda_ddl_table_new_from_meta:
 * @obj: a #GdaMetaDbObject
 *
 * Create new #GdaDdlTable instance from the corresponding #GdaMetaDbObject
 * object. If %NULL is passed this function works exactly as
 * gda_ddl_table_new()
 *
 * Since: 6.0
 */
GdaDdlTable*
gda_ddl_table_new_from_meta (GdaMetaDbObject *obj)
{
  if (!obj)
    return gda_ddl_table_new();

  if (obj->obj_type != GDA_META_DB_TABLE)
    return NULL;

  GdaMetaTable *metatable = GDA_META_TABLE(obj);
  GdaDdlTable *table = gda_ddl_table_new();

  gda_ddl_base_set_names(GDA_DDL_BASE(table),
                         obj->obj_catalog,
                         obj->obj_schema,
                         obj->obj_name);

  GSList *it = NULL;
  for (it = metatable->columns; it; it = it->next)
    {
      GdaDdlColumn *column = gda_ddl_column_new_from_meta (GDA_META_TABLE_COLUMN(it->data));
      
      gda_ddl_table_append_column(table,column);
    }

  it = NULL;
  for (it = metatable->fk_list;it;it = it->next)
    {
      if (!GDA_META_TABLE_FOREIGN_KEY_IS_DECLARED(GDA_META_TABLE_FOREIGN_KEY(it->data)))
        continue;

      GdaDdlFkey *fkey = gda_ddl_fkey_new_from_meta (GDA_META_TABLE_FOREIGN_KEY(it->data));
      
      gda_ddl_table_append_fkey (table,fkey);

    } 

  return table;
}

/**
 * gda_ddl_table_append_column:
 * @self: a #GdaDdlTable instance
 * @column: column to add
 *
 * Append @column to the internal list of columns
 *
 * Since: 6.0
 */
void
gda_ddl_table_append_column (GdaDdlTable *self,
                             GdaDdlColumn *column)
{
  g_return_if_fail (self);

  GdaDdlTablePrivate *priv = gda_ddl_table_get_instance_private (self);

  priv->mp_columns = g_list_append (priv->mp_columns,column);
}

/**
 * gda_ddl_table_append_fkey:
 * @self: a #GdaDdlTable instance
 * @fkey: fkry to add
 *
 * Append @fkey to the internal list of columns
 *
 * Since: 6.0
 */
void
gda_ddl_table_append_fkey (GdaDdlTable *self,
                           GdaDdlFkey *fkey)
{
  g_return_if_fail (self);

  GdaDdlTablePrivate *priv = gda_ddl_table_get_instance_private (self);

  priv->mp_fkeys = g_list_append (priv->mp_fkeys,fkey);
}

