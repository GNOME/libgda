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
#include "gda-ddl-modifiable.h"
#include <glib/gi18n-lib.h>

G_DEFINE_QUARK (gda_db_table_error, gda_db_table_error)

typedef struct
{
  gchar *mp_comment;

  gboolean m_istemp;
  GList *mp_columns; /* Type GdaDbColumn*/
  GList *mp_fkeys; /* List of all fkeys, GdaDbFkey */
  GSList *mp_constraint; /* A list of constraints for the table as strings, a la gchar* */
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
 *
 * #GdaDbTable can be used as a container to hold other objects, e.g. #GdaDbColumn, #GdaDbFkey and
 * as soon as populated with all needed objects the table can be created using
 * gda_ddl_modifiable_create(),
 *
 * To delete the table a method gda_ddl_modifiable_drop() can be called.
 * 
 * A simple example of how to create a table with 3 columns being the last column of the foreign key.
 * 
 * |[<!-- language="C" -->
 * 
 * GdaConnection *cnc;
 * //Open connection here
 * 
 * GError        *error = NULL;
 * 
 * GdaDbTable *tproject = gda_db_table_new ();
 * gda_db_base_set_name (GDA_DB_BASE (tproject), "Project");
 *
 * GdaDbColumn *pid   = gda_db_column_new ();
 * GdaDbColumn *pname = gda_db_column_new ();
 * GdaDbColumn *pfkey = gda_db_column_new ();
 * GdaDbFkey   *fkey  = gda_db_fkey_new ();
 * 
 * gda_db_column_set_name (pid, "id");
 * gda_db_column_set_type (pid, G_TYPE_INT);
 * gda_db_column_set_nnul (pid, TRUE);
 * gda_db_column_set_autoinc (pid, TRUE);
 * gda_db_column_set_unique (pid, TRUE);
 * gda_db_column_set_pkey (pid, TRUE);
 *
 * gda_db_column_set_name (pname, "name");
 * gda_db_column_set_type (pname, G_TYPE_STRING);
 * gda_db_column_set_size (pname, 30);
 * gda_db_column_set_nnul (pname, TRUE);
 * gda_db_column_set_unique (pname, TRUE);
 * 
 * gda_db_column_set_name (pfkey, "fkey");
 * gda_db_column_set_type (pfkey, G_TYPE_INT);
 * gda_db_column_set_nnul (pfkey, TRUE);
 * 
 * gda_db_fkey_set_ref_table (fkey, "AnotherTable");
 * gda_db_fkey_set_ondelete (fkey, GDA_DB_FKEY_RESTRICT);
 * gda_db_fkey_set_onupdate (fkey, GDA_DB_FKEY_RESTRICT);
 * gda_db_fkey_set_field (fkey, "fkey", "another_table_id");
 * 
 * gda_db_table_append_column (tproject, pid);
 * g_object_unref (pid);
 * 
 * gda_db_table_append_column (tproject, pname);
 * g_object_unref (pname);
 * 
 * gda_db_table_append_column (tproject, pfkey);
 * g_object_unref (pfkey);
 * 
 * gda_db_table_append_fkey (tproject, fkey);
 * g_object_unref (fkey);
 * 
 * if (!gda_ddl_modifiable_create (GDA_DDL_MODIFIABLE (tproject), cnc, NULL, &error))
 * {
 *   g_error ("It was not possible to create the table in the database: %s\n",
 *            error && error->message ? error->message : "No detail");
 * }
 * 
 * g_object_unref (tproject);
 *  
 * ]|

 */

static void gda_db_table_buildable_interface_init (GdaDbBuildableInterface *iface);
static void gda_ddl_modifiable_interface_init (GdaDdlModifiableInterface *iface);

static gboolean gda_db_table_create (GdaDdlModifiable *self, GdaConnection *cnc,
                                     gpointer user_data, GError **error);
static gboolean gda_db_table_drop (GdaDdlModifiable *self, GdaConnection *cnc,
                                   gpointer user_data, GError **error);
static gboolean gda_db_table_rename (GdaDdlModifiable *old_name, GdaConnection *cnc,
                                     gpointer new_name, GError **error);

G_DEFINE_TYPE_WITH_CODE (GdaDbTable, gda_db_table, GDA_TYPE_DB_BASE,
                         G_ADD_PRIVATE (GdaDbTable)
                         G_IMPLEMENT_INTERFACE (GDA_TYPE_DB_BUILDABLE,
                                                gda_db_table_buildable_interface_init)
                         G_IMPLEMENT_INTERFACE (GDA_TYPE_DDL_MODIFIABLE,
                                                gda_ddl_modifiable_interface_init))

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
    GDA_DB_TABLE_CONSTRAINT,

    GDA_DB_TABLE_N_NODES
};

static const gchar *gdadbtablenodes[GDA_DB_TABLE_N_NODES] = {
    "table",
    "name",
    "temptable",
    "space",
    "comment",
    "constraint"
};

/**
 * gda_db_table_new:
 *
 * Returns: New instance of #GdaDbTable, to free with g_object_unref () once not needed anymore
 *
 * Stability: Stable
 * Since: 6.0
 */
GdaDbTable *
gda_db_table_new (void)
{
  G_DEBUG_HERE();
  return g_object_new (GDA_TYPE_DB_TABLE, NULL);
}

static void
gda_db_table_finalize (GObject *object)
{
  G_DEBUG_HERE();
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
  if (priv->mp_constraint)
    g_slist_free_full (priv->mp_constraint, (GDestroyNotify)g_free);

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
  priv->mp_constraint = NULL;
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
  G_DEBUG_HERE();
  g_return_val_if_fail (buildable, FALSE);
  g_return_val_if_fail (node, FALSE);

  GdaDbTable *self = GDA_DB_TABLE (buildable);

  GdaDbTablePrivate *priv = gda_db_table_get_instance_private (self);

  xmlChar *name = NULL;
  name = xmlGetProp (node, (xmlChar *)gdadbtablenodes[GDA_DB_TABLE_NAME]);
  g_assert (name); /* If here bug with xml validation */
  gda_db_base_set_name(GDA_DB_BASE(self), (gchar*)name);
  xmlFree(name);

  xmlChar *tempt = xmlGetProp (node, (xmlChar*)gdadbtablenodes[GDA_DB_TABLE_TEMP]);

  if (tempt)
    {
      if (*tempt == 't' || *tempt == 't')
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
        }
      else if (!g_strcmp0((gchar*)it->name, gdadbtablenodes[GDA_DB_TABLE_CONSTRAINT]))
        {
          xmlChar *constraint = xmlNodeGetContent (it);

          if (constraint)
            {
              gda_db_table_append_constraint(self, (gchar*)constraint);
              xmlFree (constraint);
            }

        } /* end of else if */
    } /* End of for loop */
  return TRUE;
} /* End of gda_db_column_parse_node */

static gboolean
gda_db_table_write_node (GdaDbBuildable *buildable,
                         xmlNodePtr       rootnode,
                         GError         **error)
{
  G_DEBUG_HERE();
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

static void
gda_ddl_modifiable_interface_init (GdaDdlModifiableInterface *iface)
{
  iface->create = gda_db_table_create;
  iface->drop   = gda_db_table_drop;
  iface->rename = gda_db_table_rename;
}
/**
 * gda_db_table_set_comment:
 * @self: an #GdaDbTable object
 * @tablecomment: Comment to set as a string
 *
 * If @tablecomment is %NULL nothing is happened. @tablecomment will not be set
 * to %NULL.
 *
 * Stability: Stable
 * Since: 6.0
 */
void
gda_db_table_set_comment (GdaDbTable *self,
                          const char  *tablecomment)
{
  G_DEBUG_HERE();
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
 * Stability: Stable
 * Since: 6.0
 */
void
gda_db_table_set_is_temp (GdaDbTable *self,
                           gboolean istemp)
{
  G_DEBUG_HERE();
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
 * Stability: Stable
 * Since: 6.0
 */
const char*
gda_db_table_get_comment (GdaDbTable *self)
{
  G_DEBUG_HERE();
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
 * Stability: Stable
 * Since: 6.0
 */
gboolean
gda_db_table_get_temp (GdaDbTable *self)
{
  G_DEBUG_HERE();
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
 * Stability: Stable
 * Since: 6.0
 */
gboolean
gda_db_table_is_valid (GdaDbTable *self)
{
  G_DEBUG_HERE();
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
 * Stability: Stable
 * Since: 6.0
 *
 */
GList*
gda_db_table_get_columns (GdaDbTable *self)
{
  G_DEBUG_HERE();
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
 * Stability: Stable
 * Since: 6.0
 */
GList*
gda_db_table_get_fkeys (GdaDbTable *self)
{
  G_DEBUG_HERE();
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
 * Stability: Stable
 * Since: 6.0
 */
gboolean
gda_db_table_get_is_temp (GdaDbTable *self)
{
  G_DEBUG_HERE();
  g_return_val_if_fail (GDA_IS_DB_TABLE(self), FALSE);

  GdaDbTablePrivate *priv = gda_db_table_get_instance_private (self);
  return priv->m_istemp;
}

/**
 * gda_db_table_prepare_create:
 * @self: a #GdaDbTable instance
 * @op: an instance of #GdaServerOperation to populate.
 * @ifnotexists: Set it to TRUE if "IF NOT EXISTS" should be added
 * @error: error container
 *
 * Populate @op with information stored in @self. This method sets @op to execute CREATE_TABLE
 * operation.
 *
 * Returns: %TRUE if no error occured and %FALSE otherwise.
 *
 * Stability: Stable
 * Since: 6.0
 */
gboolean
gda_db_table_prepare_create (GdaDbTable *self,
                             GdaServerOperation *op,
                             gboolean ifnotexists,
                             GError **error)
{
  G_DEBUG_HERE();
  g_return_val_if_fail(GDA_IS_DB_TABLE(self), FALSE);
  g_return_val_if_fail(GDA_IS_SERVER_OPERATION(op), FALSE);
  g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

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

  GSList *jt = NULL;
  int indexsec = 0; /* Index for sequence */

  for (jt = priv->mp_constraint; jt != NULL; jt = jt->next, indexsec++)
    {
      if (!gda_server_operation_set_value_at (op,
                                              (const gchar *)jt->data,
                                              error,
                                              "/TABLE_CONSTRAINTS_S/%d/CONSTRAINT_STRING", indexsec))
        return FALSE;
    }

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
 * ADD_COLUMN operation with information stored in @self. This method is designed for internal use
 * only and should not be used for the new code. It will be obsolete.
 *
 * Returns: %TRUE if no error and %FALSE otherwise
 */
gboolean
gda_db_table_update (GdaDbTable *self,
                     GdaMetaTable *obj,
                     GdaConnection *cnc,
                     GError **error)
{
  g_return_val_if_fail (GDA_IS_DB_TABLE(self), FALSE);
  g_return_val_if_fail (obj, FALSE);
  g_return_val_if_fail (GDA_IS_CONNECTION(cnc), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  if (!gda_connection_is_opened (cnc))
    {
      g_set_error (error, GDA_DB_TABLE_ERROR, GDA_DB_TABLE_CONNECTION_NOT_OPENED,
                   _("Connection is not opened"));
      return FALSE;
    }

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

      if(!gda_db_column_prepare_add (GDA_DB_COLUMN(it->data), op, error))
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

/*
 * @self: a #GdaDbTable object
 * @cnc: a #GdaConnection object
 * @ifnotexists: Set to %TRUE if table should be created with "IFNOTEXISTS" option
 * @error: container for error storage
 *
 */
static gboolean
gda_db_table_create (GdaDdlModifiable *self,
                     GdaConnection *cnc,
                     gpointer user_data,
                     GError **error)
{
  g_return_val_if_fail (GDA_IS_DB_TABLE (self), FALSE);

  gda_lockable_lock (GDA_LOCKABLE(cnc));

  GdaServerProvider *provider = NULL;
  GdaServerOperation *op = NULL;
  gboolean res = FALSE;

  GdaDbTable *table = GDA_DB_TABLE (self);

  provider = gda_connection_get_provider (cnc);

  op = gda_server_provider_create_operation (provider,
                                             cnc,
                                             GDA_SERVER_OPERATION_CREATE_TABLE,
                                             NULL,
                                             error);
  if (op)
    {
      g_object_set_data_full (G_OBJECT (op), "connection", g_object_ref (cnc), g_object_unref);

      if (gda_db_table_prepare_create (table, op, TRUE, error))
        {
#ifdef GDA_DEBUG
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
 * Stability: Stable
 * Since: 6.0
 */
void
gda_db_table_append_column (GdaDbTable *self,
                            GdaDbColumn *column)
{
  g_return_if_fail (self);
  g_return_if_fail (column);

  GdaDbTablePrivate *priv = gda_db_table_get_instance_private (self);

  g_object_set (column, "table", self, NULL);
  priv->mp_columns = g_list_append (priv->mp_columns, g_object_ref (column));
}

/**
 * gda_db_table_append_fkey:
 * @self: a #GdaDbTable instance
 * @fkey: fkry to add
 *
 * Append @fkey to the internal list of columns
 *
 * Stability: Stable
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

/*
 * @old_name: The originam table to rename.
 * @new_name: The new table to rename to
 * @cnc: Connection to use
 * @error: An error holder
 *
 * This method performs rename operation on the table
 *
 * Stability: Stable
 * Since: 6.0
 */
static gboolean
gda_db_table_rename (GdaDdlModifiable *old_name,
                     GdaConnection *cnc,
                     gpointer new_name,
                     GError **error)
{
  G_DEBUG_HERE();
  GdaServerOperation *op = NULL;
  GdaServerProvider *provider = NULL;
  GdaDbTable *new_name_table = NULL;

  g_return_val_if_fail(GDA_IS_DB_TABLE (old_name), FALSE);
  g_return_val_if_fail(new_name, FALSE);

  new_name_table = GDA_DB_TABLE (new_name);

  gda_lockable_lock (GDA_LOCKABLE (cnc));
  provider = gda_connection_get_provider (cnc);

  op = gda_server_provider_create_operation (provider, cnc, GDA_SERVER_OPERATION_RENAME_TABLE,
                                             NULL, error);

  if (!op)
    goto on_error;

  if (!gda_server_operation_set_value_at (op, gda_db_base_get_full_name (GDA_DB_BASE (old_name)),
                                          error, "/TABLE_DESC_P/TABLE_NAME"))
    goto on_error;

  if (!gda_server_operation_set_value_at (op, gda_db_base_get_full_name (GDA_DB_BASE (new_name_table)),
                                          error, "/TABLE_DESC_P/TABLE_NEW_NAME"))
    goto on_error;

  if (!gda_server_provider_perform_operation (provider, cnc, op, error))
    goto on_error;

  g_object_unref (op);

  gda_lockable_unlock (GDA_LOCKABLE (cnc));

  return TRUE;

on_error:
  if (op)
    g_object_unref (op);

  gda_lockable_unlock (GDA_LOCKABLE (cnc));

  return FALSE;
}

/*
 * @self: An instance of GdaDbTable
 * @cnc: Open connection to use for the operation
 * @ifexists: Set to %TRUE if the flag "IF EXISTS" should be added.
 * @error: A place to store the error
 *
 * Drop table from the database. This mehod will call "DROP TABLE ..." SQL command.
 *
 * Returns: %TRUE if no error and %FALSE otherwise.
 *
 * Stability: Stable
 * Since: 6.0
 */
static gboolean
gda_db_table_drop (GdaDdlModifiable *self,
                   GdaConnection *cnc,
                   gpointer user_data,
                   GError **error)
{
  G_DEBUG_HERE();
  GdaServerProvider *provider = NULL;
  GdaServerOperation *op = NULL;

  g_return_val_if_fail (GDA_IS_DB_TABLE (self), FALSE);

  gda_lockable_lock (GDA_LOCKABLE (cnc));

  provider = gda_connection_get_provider (cnc);

  op = gda_server_provider_create_operation (provider, cnc, GDA_SERVER_OPERATION_DROP_TABLE,
                                             NULL, error);

  if (!op)
    {
      g_warning("ServerOperation is NULL\n");
      goto on_error;
    }

  if (!gda_server_operation_set_value_at (op, gda_db_base_get_full_name(GDA_DB_BASE(self)), error,
                                          "/TABLE_DESC_P/TABLE_NAME"))
    goto on_error;

  if (!gda_server_operation_set_value_at (op, GDA_BOOL_TO_STR (TRUE), error,
                                          "/TABLE_DESC_P/TABLE_IFEXISTS"))
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
 * gda_db_table_append_constraint:
 * @self: a #GdaDbTable instance
 * @constr: a constraint string to append
 *
 * Adds global table constraint. It will be added to the sql string by the provider implementation
 * if it supports it. Usually, table constraint is very complex and the current method just append
 * a list of constraints to the sql string.
 *
 * Stability: Stable
 * Since: 6.0
 */
void
gda_db_table_append_constraint (GdaDbTable *self,
                                const gchar *constr)
{
  GdaDbTablePrivate *priv = gda_db_table_get_instance_private (self);

  priv->mp_constraint = g_slist_append (priv->mp_constraint, g_strdup (constr));
}
