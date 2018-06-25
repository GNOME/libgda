/*
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

G_DEFINE_QUARK (gda_ddl_table_error,gda_ddl_table_error)

typedef struct
{
  gchar *mp_comment;

  gboolean m_istemp;
//  gchar *mp_name;
  GList *mp_columns; /* Type GdaDdlColumn*/
  GList *mp_fkeys; /* List of all fkeys, GdaDdlFkey */
} GdaDdlTablePrivate;

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
    g_list_free_full (priv->mp_fkeys, (GDestroyNotify)gda_ddl_fkey_free);
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
 * Parse #xmlNodePtr wich should point to the <table> node
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
//  g_return_val_if_fail (error != NULL && *error == NULL,FALSE);

  GdaDdlTable *self = GDA_DDL_TABLE (buildable);

  GdaDdlTablePrivate *priv = gda_ddl_table_get_instance_private (self);

  xmlChar *name = NULL;
  name = xmlGetProp (node,(xmlChar *)"name");
  g_assert (name); /* If here bug with xml validation */
  gda_ddl_base_set_name(GDA_DDL_BASE(self),(gchar *)name);

  xmlChar *tempt = xmlGetProp (node,(xmlChar*)"temptable");

  if (tempt && (*tempt == 't' || *tempt == 't'))
    {
      g_object_set (G_OBJECT(self),"istemp",TRUE,NULL);
      xmlFree (tempt);
    }

  gint columnscale = 0;

  for (xmlNodePtr it = node->children; it ; it = it->next)
    {
      g_print ("Table node name is : %s\n",it->name);
      if (!g_strcmp0 ((gchar *) it->name, "comment"))
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
          gda_ddl_column_set_scale (column,columnscale++);

          if (!gda_ddl_buildable_parse_node(GDA_DDL_BUILDABLE (column),
                                            it, error))
            {
              g_print ("Error parsing column node\n");
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
              gda_ddl_fkey_free (fkey);
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
  node  = xmlNewChild (rootnode,NULL,(xmlChar*)"table",NULL);
  xmlNewProp (node,BAD_CAST "name",
              BAD_CAST gda_ddl_base_get_name (GDA_DDL_BASE(self)));

  xmlNewProp (node,BAD_CAST "temptable",
              BAD_CAST GDA_BOOL_TO_STR (priv->m_istemp)); 

  xmlNewChild (node,NULL,(xmlChar*)"comment",(xmlChar*)priv->mp_comment);

  GList *it = NULL;

  for (it = priv->mp_columns; it; it=it->next)
    if(!gda_ddl_buildable_write_node (GDA_DDL_BUILDABLE(GDA_DDL_COLUMN(it->data)),
                                      node,error))
      goto on_error;

  return TRUE;
on_error:
  return FALSE;
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
 * If @tablecomment is %NULL nothing is happaning. tablecomment will not be set
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
 * Returns: A comment string or%NULL if comment wasn't set.
 */
const char *
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
  g_assert (self);

  GdaDdlTablePrivate *priv = gda_ddl_table_get_instance_private (self);
  return priv->m_istemp;
}

gboolean
gda_ddl_table_is_valid (GdaDdlTable *self)
{
  GdaDdlTablePrivate *priv = gda_ddl_table_get_instance_private (self);

/* TODO: It may be worthwile to have the save method for all children elements and
 * call this method on each children member to make sure whole table is valid
 * */
  if (!self || !priv->mp_columns)
    return FALSE;
  else
    return TRUE;
}

/**
 * gda_ddl_table_get_columns:
 * @self: an #GdaDdlTable object
 *
 * Use this method to obtain internal list of all columns. The internal list
 * should not be freed.
 *
 * Returns: A list of #GdaDdlColumn objects or %NULL if the internal list is
 * not set or if %NULL is passed.
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
 * Returns: A list of #GdaDdlFkey objects or %NULL if the internal list is not
 * set or %NULL is passed
 */
const GList*
gda_ddl_table_get_fkeys (GdaDdlTable *self)
{
  g_return_val_if_fail (self, NULL);
  GdaDdlTablePrivate *priv = gda_ddl_table_get_instance_private (self);

  return priv->mp_fkeys;
}


/**
 * gda_ddl_table_free:
 * @self: a #GdaDdlTable object
 *
 * A convenient method to free free the object
 *
 * Since: 6.0
 */
void
gda_ddl_table_free (GdaDdlTable *self)
{
  g_clear_object (&self);
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

gboolean
gda_ddl_table_prepare_create (GdaDdlTable *self,
                              GdaServerOperation *op,
                              GError **error)
{
  GdaDdlTablePrivate *priv = gda_ddl_table_get_instance_private (self);

  if (!gda_server_operation_set_value_at(op,
                                         gda_ddl_base_get_full_name(GDA_DDL_BASE(self)),
                                         error,
                                         "/TABLE_DEF_P/TABLE_NAME"))
    return FALSE;

  g_print ("%s:%d\n",__FILE__,__LINE__);
  if (!gda_server_operation_set_value_at(op,
                                         GDA_BOOL_TO_STR(priv->m_istemp),
                                         error,
                                         "/TABLE_DEF_P/TABLE_TEMP"))
    return FALSE;

  g_print ("%s:%d\n",__FILE__,__LINE__);
  if (!gda_server_operation_set_value_at(op,
                                         priv->mp_comment,
                                         error,
                                         "/TABLE_DEF_P/TABLE_COMMENT"))
    return FALSE;

  g_print ("%s:%d\n",__FILE__,__LINE__);
  if (!gda_server_operation_set_value_at(op,
                                         priv->mp_comment,
                                         error,
                                         "/TABLE_DEF_P/TABLE_IFNOTEXISTS"))
    return FALSE;
  
  g_print ("%s:%d\n",__FILE__,__LINE__);
  GList *it = NULL;

  for (it = priv->mp_columns;it;it=it->next)
    {
      if(!gda_ddl_column_prepare_create (GDA_DDL_COLUMN(it->data),op,error))
        return FALSE;
      
      g_print ("%s:%d\n",__FILE__,__LINE__);
    }

  for (it = priv->mp_fkeys;it;it=it->next)
    {
      if(!gda_ddl_fkey_prepare_create (it->data,op,error))
        return FALSE;
      
      g_print ("%s:%d\n",__FILE__,__LINE__);
    }

  g_print ("%s:%d\n",__FILE__,__LINE__);
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

gboolean        
gda_ddl_table_update (GdaDdlTable *self,
                      GdaMetaTable *obj,
                      GdaConnection *cnc,
                      GError **error)
{
  g_print ("%s:%d\n",__FILE__,__LINE__);
  g_return_val_if_fail (self,FALSE);
  g_return_val_if_fail (obj,FALSE);
  g_return_val_if_fail (cnc,FALSE);
 
  g_print ("%s:%d\n",__FILE__,__LINE__);
  if (!gda_connection_is_opened(cnc))
    return FALSE;

  g_print ("%s:%d\n",__FILE__,__LINE__);
  GdaDdlTablePrivate *priv = gda_ddl_table_get_instance_private (self);
  
  if (!obj->columns)
    return FALSE;

  g_print ("%s:%d\n",__FILE__,__LINE__);
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
  g_print ("%s:%d\n",__FILE__,__LINE__);
  if (!op)
    goto on_error;
 
  g_print ("%s:%d\n",__FILE__,__LINE__);
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

      g_print ("%s:%d\n",__FILE__,__LINE__);
      if (res) /* If object is present we need go to next element */
        continue;
      else
        newcolumncount++; /* We need to count new column. See below */ 
          
      g_print ("%s:%d\n",__FILE__,__LINE__);
      if(!gda_ddl_column_prepare_add(it->data,op,error))
        {
          g_print ("%s:%d\n",__FILE__,__LINE__);
          g_slist_free (res);
          goto on_error;
        }
      g_print ("%s:%d\n",__FILE__,__LINE__);
    }

  g_print ("%s:%d\n",__FILE__,__LINE__);
  if (newcolumncount != 0) /* Run operation only if at least on column presents*/
    if(!gda_server_provider_perform_operation(provider,cnc,op,error))
      goto on_error;

  g_print ("%s:%d\n",__FILE__,__LINE__);
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
 * Returns: %TRUE if succesful, %FALSE otherwise
 *
 * Sicne: 6.0
 */
gboolean
gda_ddl_table_create (GdaDdlTable *self,
                      GdaConnection *cnc,
                      GError **error)
{
  g_return_val_if_fail (self,FALSE);
  g_return_val_if_fail (cnc,FALSE);

  g_print ("%s:%d\n",__FILE__,__LINE__);
  if (!gda_connection_is_opened(cnc))
    return FALSE;
  
  g_print ("%s:%d\n",__FILE__,__LINE__);
  gda_lockable_lock(GDA_LOCKABLE(cnc));

  g_print ("%s:%d\n",__FILE__,__LINE__);
  GdaServerProvider *provider = NULL;
  GdaServerOperation *op = NULL;
 
  provider = gda_connection_get_provider (cnc);

  g_print ("%s:%d\n",__FILE__,__LINE__);
  op = gda_server_provider_create_operation(provider,
                                            cnc,
                                            GDA_SERVER_OPERATION_CREATE_TABLE,
                                            NULL,
                                            error);
  if (!op)
    goto on_error;

  g_print ("%s:%d\n",__FILE__,__LINE__);
  if (!gda_ddl_table_prepare_create(self,op,error))
    goto on_error;

  gchar *sqlstr = NULL;
  sqlstr = gda_server_provider_render_operation (provider,
                                                 cnc,
                                                 op,
                                                 error);

  g_print ("SQL STRING:%s\n",sqlstr);
  g_print ("%s:%d\n",__FILE__,__LINE__);
  if(!gda_server_provider_perform_operation(provider,cnc,op,error))
    goto on_error;

  g_print ("%s:%d\n",__FILE__,__LINE__);
  g_object_unref (op);
  gda_lockable_unlock(GDA_LOCKABLE(cnc));

  return TRUE;

on_error:
  g_object_unref (op);
  gda_lockable_unlock(GDA_LOCKABLE(cnc));
  return FALSE;
}

/**
 * _gda_ddl_table_align_columns: (skip)
 * @self: a #GdaDdlTable object
 * 
 * This method asign new scale values to all columns to make  sure they are
 * ordered properly. 
 *
 */
static void
_gda_ddl_table_align_columns (GdaDdlTable *self)
{
  g_return_if_fail (self);

  GdaDdlTablePrivate *priv = gda_ddl_table_get_instance_private (self);
  
  gint ncolumn = 0;

  for (GList *it = priv->mp_columns; it; it = it->next)
    gda_ddl_column_set_scale(it->data,ncolumn++); 
}

/**
 * gda_ddl_table_new_from_meta:
 * @obj: a #GdaMetaDbObject
 *
 * Create new #GdaDdlTable instance from the corresponding #GdaMetaDbObject
 * object. If %NULL is passed this function works exactly as
 * gda_ddl_table_new()
 *
 * Returns: New object that should be freed with gda_ddl_table_free()
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

  _gda_ddl_table_align_columns (table);
  return table;
}

void
gda_ddl_table_append_column (GdaDdlTable *self,
                             GdaDdlColumn *column)
{
  g_return_if_fail (self);

  GdaDdlTablePrivate *priv = gda_ddl_table_get_instance_private (self);

  priv->mp_columns = g_list_append (priv->mp_columns,column);
}
