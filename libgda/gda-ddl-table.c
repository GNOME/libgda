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
#include <glib/gi18n.h>

G_DEFINE_QUARK (gda_ddl_table_error,gda_ddl_table_error)

typedef struct
{
  gchar *mp_comment;

  gboolean m_istemp;
  gchar *mp_name;
  GList *mp_columns; /* Type GdaDdlColumn*/
  GList *mp_fkeys; /* List of all fkeys, GdaDdlFkey */

} GdaDdlTablePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GdaDdlTable, gda_ddl_table, GDA_TYPE_DDL_BASE)

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
                                                  G_PARAM_READABLE);
  properties[PROP_ISTEMP] = g_param_spec_string ("istemp",
                                                 "temporary-table",
                                                 "Is table temporary",
                                                 NULL,
                                                 G_PARAM_READABLE);

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
gboolean
gda_ddl_table_parse_node (GdaDdlTable  *self,
                          xmlNodePtr	node,
                          GError      **error)
{
  g_return_val_if_fail (self,FALSE);
  g_return_val_if_fail (node,FALSE);
  g_return_val_if_fail (error != NULL && *error == NULL,FALSE);

  GdaDdlTablePrivate *priv = gda_ddl_table_get_instance_private (self);

  xmlChar *name = NULL;
  name = xmlGetProp (node,(xmlChar *)"name");
  g_assert (name); /* If here bug with xml validation */
  g_object_set (GDA_DDL_BASE(priv->mp_name),"name",(char*)name,NULL);

  xmlChar *tempt = xmlGetProp (node,(xmlChar*)"temptable");

  if (tempt && (*tempt == 't' || *tempt == 't')) {
      g_object_set (G_OBJECT(self),"istemp",TRUE,NULL);
      xmlFree (tempt);
  }

  for (xmlNodePtr it = node->children; it ; it = it->next) {
      if (!g_strcmp0 ((gchar *) it->name, "comment")) {
          xmlChar *comment = xmlNodeGetContent (it);

          if (comment) {
              g_object_set (G_OBJECT(self),"comment", (char *)comment,NULL);
              xmlFree (comment);
          }
      } else if (!g_strcmp0 ((gchar *) it->name, "column")) {
          GdaDdlColumn *column;
          column = gda_ddl_column_new ();

          if (!gda_ddl_column_parse_node (column, it, error)) {
              gda_ddl_column_free (column);
              return FALSE;
          } else
            priv->mp_columns = g_list_append (priv->mp_columns,column);
      } else if (!g_strcmp0 ((gchar *) it->name, "fkey")) {
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

