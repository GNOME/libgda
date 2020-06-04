/* gda-db-fkey.c
 *
 * Copyright (C) 2018-2019 Pavlo Solntsev <p.sun.fun@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */
#define G_LOG_DOMAIN "GDA-db-fkey"

#include "gda-db-fkey.h"
#include <glib/gi18n-lib.h>
#include "gda-db-buildable.h"
#include "gda-db-fkey-private.h"

typedef struct
{
  /* /FKEY_S/%d/FKEY_REF_TABLE */
  gchar *mp_ref_table;
  /* /FKEY_S/%d/FKEY_FIELDS_A/@FK_FIELD */

  GList  *mp_field;
  /* /FKEY_S/FKEY_FIELDS_A/@FK_REF_PK_FIELD */
  GList  *mp_ref_field;
  /* /FKEY_S/FKEY_ONUPDATE This action is reserved for ONUPDATE */
  GdaDbFkeyReferenceAction m_onupdate;
  /* /FKEY_S/FKEY_ONDELETE This action is reserved for ONDELETE */
  GdaDbFkeyReferenceAction m_ondelete;
}GdaDbFkeyPrivate;

/**
 * SECTION:gda-db-fkey
 * @short_description: Object to hold information for foregn key.
 * @stability: Stable
 * @include: libgda/libgda.h
 *
 * For generating database from xml file or for mapping
 * database to an xml file #GdaDbFkey holds information about
 * foregn keys with a convenient set of methods to manipulate them.
 * #GdaDbFkey implements #GdaDbBuildable interface for parsing xml file. This is an example how
 * #GdaDbFkey can be used:
 *
 * |[<!-- language="C" -->
 * GdaDbFkey *fkey = gda_db_fkey_new ();
 * gda_db_fkey_set_ref_table (fkey, "Project");
 * gda_db_fkey_set_ondelete (fkey, GDA_DB_FKEY_RESTRICT);
 * gda_db_fkey_set_onupdate (fkey, GDA_DB_FKEY_RESTRICT);
 * gda_db_fkey_set_field (fkey, "project_id", "id");
 *
 * gda_db_table_append_fkey (temployee, fkey);
 * ]|
 */

static void gda_db_fkey_buildable_interface_init (GdaDbBuildableInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GdaDbFkey, gda_db_fkey, G_TYPE_OBJECT,
                         G_ADD_PRIVATE (GdaDbFkey)
                         G_IMPLEMENT_INTERFACE (GDA_TYPE_DB_BUILDABLE,
                                                gda_db_fkey_buildable_interface_init))

static const gchar *OnAction[] = {
    "NO ACTION",
    "SET NULL",
    "RESTRICT",
    "SET DEFAULT",
    "CASCADE"
};

/* This is convenient way to name all nodes from xml file */
enum {
    GDA_DB_FKEY_NODE,
    GDA_DB_FKEY_REFTABLE,
    GDA_DB_FKEY_ONUPDATE,
    GDA_DB_FKEY_ONDELETE,
    GDA_DB_FKEY_FKFIELD,
    GDA_DB_FKEY_FKFIELD_NAME,
    GDA_DB_FKEY_FKFIELD_REFIELD,
    GDA_DB_FKEY_N_NODES
};

static const gchar *gdadbfkeynodes[GDA_DB_FKEY_N_NODES] = {
    "fkey",
    "reftable",
    "onupdate",
    "ondelete",
    "fk_field",
    "name",
    "reffield"
};

/**
 * gda_db_fkey_new:
 *
 * Create a new #GdaDbFkey object.
 *
 * Stability: Stable
 * Since: 6.0
 */
GdaDbFkey *
gda_db_fkey_new (void)
{
  return g_object_new (GDA_TYPE_DB_FKEY, NULL);
}

/**
 * gda_db_fkey_new_from_meta:
 * @metafkey: a #GdaMetaTableForeignKey instance
 *
 * Create a new instance from the corresponding meta object. If @metafkey is %NULL,
 * this function is identical to gda_db_fkey_new().
 *
 * Stability: Stable
 * Since: 6.0
 */
GdaDbFkey*
gda_db_fkey_new_from_meta (GdaMetaTableForeignKey *metafkey)
{
  if (!metafkey)
    return gda_db_fkey_new ();

  GdaMetaDbObject *refobject = GDA_META_DB_OBJECT (metafkey->meta_table);

  GdaDbFkey *fkey = gda_db_fkey_new ();

  gda_db_fkey_set_ref_table (fkey, refobject->obj_full_name);

  for (gint i = 0; i < metafkey->cols_nb; i++)
      gda_db_fkey_set_field (fkey,
                              metafkey->fk_names_array[i],
                              metafkey->ref_pk_names_array[i]);

  GdaMetaForeignKeyPolicy policy = GDA_META_TABLE_FOREIGN_KEY_ON_UPDATE_POLICY(metafkey);

  switch (policy)
    {
    case GDA_META_FOREIGN_KEY_NONE:
      gda_db_fkey_set_onupdate (fkey, GDA_DB_FKEY_NO_ACTION);
      break;
    case GDA_META_FOREIGN_KEY_CASCADE:
      gda_db_fkey_set_onupdate (fkey, GDA_DB_FKEY_CASCADE);
      break;
    case GDA_META_FOREIGN_KEY_RESTRICT:
      gda_db_fkey_set_onupdate (fkey, GDA_DB_FKEY_RESTRICT);
      break;
    case GDA_META_FOREIGN_KEY_SET_DEFAULT:
      gda_db_fkey_set_onupdate (fkey, GDA_DB_FKEY_SET_DEFAULT);
      break;
    default:
      break;
    }

  policy = GDA_META_TABLE_FOREIGN_KEY_ON_DELETE_POLICY(metafkey);
  switch (policy)
    {
    case GDA_META_FOREIGN_KEY_NONE:
      gda_db_fkey_set_ondelete (fkey, GDA_DB_FKEY_NO_ACTION);
      break;
    case GDA_META_FOREIGN_KEY_CASCADE:
      gda_db_fkey_set_ondelete (fkey, GDA_DB_FKEY_CASCADE);
      break;
    case GDA_META_FOREIGN_KEY_RESTRICT:
      gda_db_fkey_set_ondelete (fkey, GDA_DB_FKEY_RESTRICT);
      break;
    case GDA_META_FOREIGN_KEY_SET_DEFAULT:
      gda_db_fkey_set_ondelete (fkey, GDA_DB_FKEY_SET_DEFAULT);
      break;
    default:
      break;
    }

  return fkey;
}

static void
gda_db_fkey_finalize (GObject *object)
{
  GdaDbFkey *self = GDA_DB_FKEY(object);
  GdaDbFkeyPrivate *priv = gda_db_fkey_get_instance_private (self);

  if (priv->mp_field)     g_list_free_full (priv->mp_field,     g_free);
  if (priv->mp_ref_field) g_list_free_full (priv->mp_ref_field, g_free);

  g_free (priv->mp_ref_table);

  G_OBJECT_CLASS (gda_db_fkey_parent_class)->finalize (object);
}

static void
gda_db_fkey_class_init (GdaDbFkeyClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gda_db_fkey_finalize;
}

static void
gda_db_fkey_init (GdaDbFkey *self)
{
  GdaDbFkeyPrivate *priv = gda_db_fkey_get_instance_private (self);

  priv->mp_field = NULL;
  priv->mp_ref_field = NULL;
  priv->mp_ref_table = NULL;

  priv->m_onupdate = GDA_DB_FKEY_NO_ACTION;
  priv->m_ondelete = GDA_DB_FKEY_NO_ACTION;
}

/**
 * gda_db_fkey_parse_node:
 * @self: #GdaDbFkey object
 * @node: xml node to parse as #xmlNodePtr
 * @error: #GError object to store error
 *
 * Use this method to populate corresponding #GdaDbFkey object from xml node. Usually,
 * this method is called from #GdaDbCatalog during parsing the input xml file.
 *
 * The corresponding DTD section suitable for parsing by this method should correspond
 * th the following code:
 *
 * |[<!-- language="dtd" -->
 * <!ELEMENT fkey (fk_field?)>
 * <!ATTLIST fkey reftable IDREF #IMPLIED>
 * <!ATTLIST fkey onupdate (RESTRICT|CASCADE|SET_NULL|NO_ACTION|SET_DEFAULT) #IMPLIED>
 * <!ATTLIST fkey ondelete (RESTRICT|CASCADE|SET_NULL|NO_ACTION|SET_DEFAULT) #IMPLIED>
 *
 * <!ELEMENT fk_field (#PCDATA)>
 * <!ATTLIST fk_field name      IDREF #REQUIRED>
 * <!ATTLIST fk_field reffield  IDREF #REQUIRED>
 * ]|
 *
 * Returns: %FALSE if an error occurs, %TRUE otherwise
 *
 * Since: 6.0
 */
static gboolean
gda_db_fkey_parse_node (GdaDbBuildable *buildable,
                        xmlNodePtr       node,
                        GError         **error)
{
  g_return_val_if_fail (buildable, FALSE);
  g_return_val_if_fail (node, FALSE);

  /*
   *    <fkey reftable="products" onupdate="NO_ACTION" ondelete="NO_ACTION">
   *        <fk_field name="column_name" reffield="column_name"/>
   *        <fk_field name="column_id" reffield="column_"/>
   *    </fkey>
   */
  xmlChar *name = NULL;
  if (g_strcmp0 ((gchar*)node->name, gdadbfkeynodes[GDA_DB_FKEY_NODE]))
    return FALSE;

  GdaDbFkey *self = GDA_DB_FKEY (buildable);
  GdaDbFkeyPrivate *priv = gda_db_fkey_get_instance_private (self);

  xmlChar *prop = NULL;

  prop = xmlGetProp (node, (xmlChar *)gdadbfkeynodes[GDA_DB_FKEY_REFTABLE]);

  g_assert (prop); /* Bug with xml valdation */

  priv->mp_ref_table = g_strdup ((gchar *)prop);
  xmlFree (prop);
  prop = NULL;

  prop = xmlGetProp (node, (xmlChar *)gdadbfkeynodes[GDA_DB_FKEY_ONUPDATE]);

  if (prop)
    {
      priv->m_onupdate = GDA_DB_FKEY_NO_ACTION;

      for (guint i = 0; i < G_N_ELEMENTS(OnAction); i++)
        {
          if (!g_strcmp0 ((gchar *)prop, OnAction[i]))
            priv->m_onupdate = (GdaDbFkeyReferenceAction)i;
        }

      xmlFree (prop);
      prop = NULL;
    }

  prop = xmlGetProp (node, (xmlChar *)gdadbfkeynodes[GDA_DB_FKEY_ONDELETE]);

  if (prop)
    {
      for (guint i = 0; i < G_N_ELEMENTS(OnAction); i++)
        {
          if (!g_strcmp0 ((gchar *)prop, OnAction[i]))
            priv->m_ondelete = (GdaDbFkeyReferenceAction)i;
        }

      xmlFree (prop);
      prop = NULL;

    }

  name = NULL;
  xmlChar *reffield = NULL;

  for (xmlNodePtr it = node->children; it; it = it->next)
    {
      if (!g_strcmp0 ((gchar *)it->name, gdadbfkeynodes[GDA_DB_FKEY_FKFIELD]))
        {
          name = xmlGetProp (it, (xmlChar *)gdadbfkeynodes[GDA_DB_FKEY_FKFIELD_NAME]);

          g_assert(name);
          priv->mp_field = g_list_append (priv->mp_field, g_strdup ((const gchar *)name));
          xmlFree (name);

          reffield = xmlGetProp (it, (xmlChar *)gdadbfkeynodes[GDA_DB_FKEY_FKFIELD_REFIELD]);
          g_assert(reffield);
          priv->mp_ref_field = g_list_append (priv->mp_ref_field,
                                              g_strdup ((const gchar *)reffield));
          xmlFree (reffield);
        } /* end of if */
    } /* end of for loop */
  return TRUE;
}

/**
 * gda_db_fkey_write_node:
 * @self: An object #GdaDbFkey
 * @writer: An object to #xmlTextWriterPtr instance
 * @error: A place to store error
 *
 * An appropriate encoding and version should be placed to the writer before
 * it is used. This method will not add any header, just populate appropriate
 * fields in the xml tree. The new xml block should be something like:
 *
 * |[<!-- language="xml" -->
 * <fkey reftable="products" onupdate="NO_ACTION" ondelete="NO_ACTION">
 *     <fk_field name="column_namefk1" reffield="column_name1"/>
 *     <fk_field name="column_namefk2" reffield="column_name2"/>
 *     <fk_field name="column_namefk3" reffield="column_name3"/>
 * </fkey>
 *]|
 * Returns: TRUE if no error, FALSE otherwise
 */
static gboolean
gda_db_fkey_write_node (GdaDbBuildable  *buildable,
                        xmlNodePtr rootnode,
                        GError **error)
{
  g_return_val_if_fail (buildable, FALSE);
  g_return_val_if_fail (rootnode,FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  GdaDbFkey *self = GDA_DB_FKEY (buildable);
  GdaDbFkeyPrivate *priv = gda_db_fkey_get_instance_private (self);

  xmlNodePtr node = NULL;
  node  = xmlNewChild (rootnode, NULL, (xmlChar*)gdadbfkeynodes[GDA_DB_FKEY_NODE], NULL);

  xmlNewProp (node, BAD_CAST gdadbfkeynodes[GDA_DB_FKEY_REFTABLE],
              BAD_CAST priv->mp_ref_table);

  xmlNewProp (node, BAD_CAST gdadbfkeynodes[GDA_DB_FKEY_ONUPDATE],
              BAD_CAST priv->m_onupdate);

  xmlNewProp (node, BAD_CAST gdadbfkeynodes[GDA_DB_FKEY_ONDELETE],
              BAD_CAST priv->m_ondelete);

  GList *it = priv->mp_field;
  GList *jt = priv->mp_ref_field;

  for (; it && jt; it = it->next, jt=jt->next )
    {
      xmlNodePtr tnode = NULL;
      tnode = xmlNewChild (node, NULL,(xmlChar*)gdadbfkeynodes[GDA_DB_FKEY_FKFIELD], NULL);

      xmlNewProp (tnode, (xmlChar*)gdadbfkeynodes[GDA_DB_FKEY_FKFIELD_NAME],
                  BAD_CAST it->data);

      xmlNewProp (tnode, (xmlChar*)gdadbfkeynodes[GDA_DB_FKEY_FKFIELD_REFIELD],
                  BAD_CAST jt->data);
    }

  return TRUE;
}

static void
gda_db_fkey_buildable_interface_init (GdaDbBuildableInterface *iface)
{
    iface->parse_node = gda_db_fkey_parse_node;
    iface->write_node = gda_db_fkey_write_node;
}

/**
 * gda_db_fkey_get_ondelete:
 * @self: An object #GdaDbFkey
 *
 * Return: ON DELETE action as a string. If the action is not set then the string corresponding to
 * NO_ACTION is returned.
 *
 * Stability: Stable
 * Since: 6.0
 */
const gchar *
gda_db_fkey_get_ondelete (GdaDbFkey *self)
{
  g_return_val_if_fail (self, NULL);

  GdaDbFkeyPrivate *priv = gda_db_fkey_get_instance_private (self);

  return OnAction[priv->m_ondelete];
}

/**
 * gda_db_fkey_get_ondelete_id:
 * @self: a #GdaDbFkey object
 *
 * The default value is %NO_ACTION
 *
 * Return: ON DELETE action as a #GdaDbFkeyReferenceAction.
 *
 * Stability: Stable
 * Since: 6.0
 */
GdaDbFkeyReferenceAction
gda_db_fkey_get_ondelete_id (GdaDbFkey *self)
{
  g_return_val_if_fail (self, GDA_DB_FKEY_NO_ACTION);

  GdaDbFkeyPrivate *priv = gda_db_fkey_get_instance_private (self);

  return priv->m_ondelete;
}

/**
 * gda_db_fkey_set_onupdate:
 * @self: An object #GdaDbFkey
 * @id: #GdaDbFkeyReferenceAction action to set
 *
 * Set action for ON_UPDATE
 *
 * Stability: Stable
 * Since: 6.0
 */
void
gda_db_fkey_set_onupdate (GdaDbFkey *self,
                          GdaDbFkeyReferenceAction id)
{
  g_return_if_fail (self);

  GdaDbFkeyPrivate *priv = gda_db_fkey_get_instance_private (self);

  priv->m_onupdate = id;
}

/**
 * gda_db_fkey_set_ondelete:
 * @self: An object #GdaDbFkey
 * @id: #GdaDbFkeyReferenceAction action to set
 *
 * Set action for ON_DELETE
 *
 * Stability: Stable
 * Since: 6.0
 */
void
gda_db_fkey_set_ondelete (GdaDbFkey *self,
                          GdaDbFkeyReferenceAction id)
{
  g_return_if_fail (self);

  GdaDbFkeyPrivate *priv = gda_db_fkey_get_instance_private (self);

  priv->m_ondelete = id;
}

/**
 * gda_db_fkey_get_onupdate:
 * @self: a #GdaDbFkey instance
 *
 * Returns: ON_UPDATE action as a string. Never %NULL
 *
 * Stability: Stable
 * Since: 6.0
 */
const gchar *
gda_db_fkey_get_onupdate (GdaDbFkey *self)
{
  g_return_val_if_fail (self, NULL);

  GdaDbFkeyPrivate *priv = gda_db_fkey_get_instance_private (self);

  return OnAction[priv->m_onupdate];
}

/**
 * gda_db_fkey_get_onupdate_id:
 * @self: a #GdaDbFkey instance
 *
 * Return: ON_UPDATE action as a #GdaDbFkeyReferenceAction
 *
 * Stability: Stable
 * Since: 6.0
 */
GdaDbFkeyReferenceAction
gda_db_fkey_get_onupdate_id (GdaDbFkey *self)
{
  GdaDbFkeyPrivate *priv = gda_db_fkey_get_instance_private (self);

  return priv->m_onupdate;
}

/**
 * gda_db_fkey_get_ref_table:
 * @self: a #GdaDbFkey object
 *
 * Return: Returns reference table name as a string or %NULL if table name
 * hasn't been set.
 *
 * Stability: Stable
 * Since: 6.0
 */
const gchar *
gda_db_fkey_get_ref_table (GdaDbFkey *self)
{
  g_return_val_if_fail (self, NULL);

  GdaDbFkeyPrivate *priv = gda_db_fkey_get_instance_private (self);

  return priv->mp_ref_table;
}

/**
 * gda_db_fkey_set_ref_table:
 * @self: #GdaDbFkey object
 * @rtable: reference table name
 *
 * Set reference table
 *
 * Stability: Stable
 * Since: 6.0
 */
void
gda_db_fkey_set_ref_table (GdaDbFkey *self,
                           const gchar *rtable)
{
  g_return_if_fail (self);

  GdaDbFkeyPrivate *priv = gda_db_fkey_get_instance_private (self);
  g_free (priv->mp_ref_table);

  priv->mp_ref_table = g_strdup (rtable);
}

/**
 * gda_db_fkey_get_field_name:
 * @self: a #GdaDbFkey object
 *
 * Returns: (element-type utf8) (transfer none): A const #GList of strings where each string
 * corresponds to a foreign key field or %NULL.
 *
 * Stability: Stable
 * Since: 6.0
 */
const GList*
gda_db_fkey_get_field_name (GdaDbFkey *self)
{
  g_return_val_if_fail (self, NULL);

  GdaDbFkeyPrivate *priv = gda_db_fkey_get_instance_private (self);

  return priv->mp_field;
}

/**
 * gda_db_fkey_get_ref_field:
 * @self: a #GdaDbFkey object
 *
 * Returns: (element-type utf8) (transfer none): A #GList of strings where each string corresponds
 * to a foreign key reference field or %NULL.
 *
 * Stability: Stable
 * Since: 6.0
 */
const GList *
gda_db_fkey_get_ref_field (GdaDbFkey *self)
{
  g_return_val_if_fail (self, NULL);

  GdaDbFkeyPrivate *priv = gda_db_fkey_get_instance_private (self);

  return priv->mp_ref_field;
}

/**
 * gda_db_fkey_set_field:
 * @self: An object #GdaDbFkey
 * @field: Field name as a string
 * @reffield: A reference field name as a string
 *
 * All arguments should be valid strings.
 *
 * Stability: Stable
 * Since: 6.0
 */
void
gda_db_fkey_set_field (GdaDbFkey  *self,
                       const gchar *field,
                       const gchar *reffield)
{
  g_return_if_fail (self);
  g_return_if_fail (field);
  g_return_if_fail (reffield);

  GdaDbFkeyPrivate *priv = gda_db_fkey_get_instance_private (self);

  priv->mp_field = g_list_append (priv->mp_field, (gpointer)field);
  priv->mp_ref_field = g_list_append(priv->mp_ref_field, (gpointer)reffield);
}

/**
 * gda_db_fkey_prepare_create:
 * @self: a #GdaDbFkey instance
 * @op: a #GdaServerOperation to populate
 * @i: Order number
 * @error: error container
 *
 * Prepare @op object for execution by populating with information stored in @self.
 *
 * Returns: %TRUE if no error or %FALSE otherwise.
 *
 * Stability: Stable
 * Since: 6.0
 */
gboolean
gda_db_fkey_prepare_create  (GdaDbFkey *self,
                             GdaServerOperation *op,
                             gint i,
                             GError **error)
{
  GdaDbFkeyPrivate *priv = gda_db_fkey_get_instance_private (self);

  if (!gda_server_operation_set_value_at (op,
                                          priv->mp_ref_table,
                                          error,
                                          "/FKEY_S/%d/FKEY_REF_TABLE", i))
    return FALSE;

  if (!gda_server_operation_set_value_at (op,
                                          OnAction[priv->m_ondelete],
                                          error,
                                          "/FKEY_S/%d/FKEY_ONDELETE", i))
    return FALSE;

  if (!gda_server_operation_set_value_at (op,
                                          OnAction[priv->m_onupdate],
                                          error,
                                          "/FKEY_S/%d/FKEY_ONUPDATE", i))
    return FALSE;

  GList *itfield = NULL;
  GList *itreffield = NULL;
  gint fkeycount = 0;

  itreffield = priv->mp_ref_field;
  itfield = priv->mp_field;

  for (;itfield && itreffield;)
    {
      if (!gda_server_operation_set_value_at (op,
                                              itfield->data,
                                              error,
                                              "/FKEY_S/%d/FKEY_FIELDS_A/@FK_FIELD/%d",
                                              i, fkeycount))
        return FALSE;

      if (!gda_server_operation_set_value_at (op,
                                              itreffield->data,
                                              error,
                                              "/FKEY_S/%d/FKEY_FIELDS_A/@FK_REF_PK_FIELD/%d",
                                              i, fkeycount))
        return FALSE;

      fkeycount++;

      /* It will make for loop overloaded and hard to read. Therefore,
       * advancement to the next element will be performed here. It is
       * only one reason.
       */
      itfield = itfield->next;
      itreffield = itreffield->next;
    }

  return TRUE;
}
