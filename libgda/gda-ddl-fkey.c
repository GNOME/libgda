/* gda-ddl-fkey.c
 *
 * Copyright (C) 2018 Pavlo Solntsev <p.sun.fun@gmail.com>
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
#include "gda-ddl-fkey.h"
#include <glib/gi18n-lib.h>
#include "gda-ddl-buildable.h"
#include "gda-meta-struct.h"

typedef struct
{
  /* /FKEY_S/%d/FKEY_REF_TABLE */
  gchar *mp_ref_table;
  /* /FKEY_S/%d/FKEY_FIELDS_A/@FK_FIELD */

  GList  *mp_field;
  /* /FKEY_S/FKEY_FIELDS_A/@FK_REF_PK_FIELD */
  GList  *mp_ref_field;
  /* /FKEY_S/FKEY_ONUPDATE This action is reserved for ONUPDATE */
  GdaDdlFkeyReferenceAction m_onupdate;
  /* /FKEY_S/FKEY_ONDELETE This action is reserved for ONDELETE */
  GdaDdlFkeyReferenceAction m_ondelete;
}GdaDdlFkeyPrivate;

static void gda_ddl_fkey_buildable_interface_init (GdaDdlBuildableInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GdaDdlFkey, gda_ddl_fkey, G_TYPE_OBJECT,
                         G_ADD_PRIVATE (GdaDdlFkey)
                         G_IMPLEMENT_INTERFACE (GDA_TYPE_DDL_BUILDABLE,
                                                gda_ddl_fkey_buildable_interface_init))

static const gchar *OnAction[] = {
    "NO ACTION",
    "SET NULL",
    "RESTRICT",
    "SET DEFAULT",
    "CASCADE"
};

/* This is convenient way to name all nodes from xml file */
enum {
  GDA_DDL_FKEY_NODE,
  GDA_DDL_FKEY_REFTABLE,
  GDA_DDL_FKEY_ONUPDATE,
  GDA_DDL_FKEY_ONDELETE,
  GDA_DDL_FKEY_FKFIELD,
  GDA_DDL_FKEY_FKFIELD_NAME,
  GDA_DDL_FKEY_FKFIELD_REFIELD,
  GDA_DDL_FKEY_N_NODES
};

const gchar *gdaddlfkeynodes[GDA_DDL_FKEY_N_NODES] = {
  "fkey",
  "reftable",
  "onupdate",
  "ondelete",
  "fk_field",
  "name",
  "reffield"
};

/**
 * gda_ddl_fkey_new:
 *
 * Create a new #GdaDdlFkey object.
 *
 * Since: 6.0
 */
GdaDdlFkey *
gda_ddl_fkey_new (void)
{
  return g_object_new (GDA_TYPE_DDL_FKEY, NULL);
}

/**
 * gda_ddl_fkey_new_from_meta:
 * @metafkey: a #GdaMetaTableForeignKey instance
 * 
 * Create a new instance from the corresponding meta object. If @metafkey is %NULL, 
 * this function is identical to gda_ddl_fkey_new()
 *
 */
GdaDdlFkey*
gda_ddl_fkey_new_from_meta (GdaMetaTableForeignKey *metafkey)
{
  if (!metafkey)
    return gda_ddl_fkey_new();

  GdaMetaDbObject *refobject = GDA_META_DB_OBJECT (metafkey->meta_table);

  GdaDdlFkey *fkey = gda_ddl_fkey_new ();
  
  gda_ddl_fkey_set_ref_table (fkey,refobject->obj_full_name);

  for (gint i = 0; i < metafkey->cols_nb; i++)
      gda_ddl_fkey_set_field (fkey,
                              metafkey->fk_names_array[i],
                              metafkey->ref_pk_names_array[i]);

  GdaMetaForeignKeyPolicy policy = GDA_META_TABLE_FOREIGN_KEY_ON_UPDATE_POLICY(metafkey);

  switch (policy)
    {
    case GDA_META_FOREIGN_KEY_NONE:
      gda_ddl_fkey_set_onupdate (fkey,GDA_DDL_FKEY_NO_ACTION);
      break;
    case GDA_META_FOREIGN_KEY_CASCADE:
      gda_ddl_fkey_set_onupdate (fkey,GDA_DDL_FKEY_CASCADE);
      break;
    case GDA_META_FOREIGN_KEY_RESTRICT:
      gda_ddl_fkey_set_onupdate (fkey,GDA_DDL_FKEY_RESTRICT);
      break;
    case GDA_META_FOREIGN_KEY_SET_DEFAULT:
      gda_ddl_fkey_set_onupdate (fkey,GDA_DDL_FKEY_SET_DEFAULT);
      break;
    default:
      break;
    }
  
  policy = GDA_META_TABLE_FOREIGN_KEY_ON_DELETE_POLICY(metafkey);
  switch (policy)
    {
    case GDA_META_FOREIGN_KEY_NONE:
      gda_ddl_fkey_set_ondelete (fkey,GDA_DDL_FKEY_NO_ACTION);
      break;
    case GDA_META_FOREIGN_KEY_CASCADE:
      gda_ddl_fkey_set_ondelete (fkey,GDA_DDL_FKEY_CASCADE);
      break;
    case GDA_META_FOREIGN_KEY_RESTRICT:
      gda_ddl_fkey_set_ondelete (fkey,GDA_DDL_FKEY_RESTRICT);
      break;
    case GDA_META_FOREIGN_KEY_SET_DEFAULT:
      gda_ddl_fkey_set_ondelete (fkey,GDA_DDL_FKEY_SET_DEFAULT);
      break;
    default:
      break;
    }

  return fkey;
}

static void
gda_ddl_fkey_finalize (GObject *object)
{
  GdaDdlFkey *self = GDA_DDL_FKEY(object);
  GdaDdlFkeyPrivate *priv = gda_ddl_fkey_get_instance_private (self);

  if (priv->mp_field)     g_list_free_full (priv->mp_field,     g_free);
  if (priv->mp_ref_field) g_list_free_full (priv->mp_ref_field, g_free);

  g_free(priv->mp_ref_table);

  G_OBJECT_CLASS (gda_ddl_fkey_parent_class)->finalize (object);
}

static void
gda_ddl_fkey_class_init (GdaDdlFkeyClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gda_ddl_fkey_finalize;
}

static void
gda_ddl_fkey_init (GdaDdlFkey *self)
{
  GdaDdlFkeyPrivate *priv = gda_ddl_fkey_get_instance_private (self);

  priv->mp_field = NULL;
  priv->mp_ref_field = NULL;
  priv->mp_ref_table = NULL;

  priv->m_onupdate = GDA_DDL_FKEY_NO_ACTION;
  priv->m_ondelete = GDA_DDL_FKEY_NO_ACTION;
}

/**
 * gda_ddl_fkey_parse_node:
 * @self: #GdaDdlFkey object
 * @node: xml node to parse as #xmlNodePtr
 * @error: #GError object to store error
 *
 * Use this method to populate corresponding #GdaDdlFkey object from xml node. Usually,
 * this method is called from #GdaDdlCreator during parsing the input xml file.
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
gda_ddl_fkey_parse_node (GdaDdlBuildable *buildable,
                         xmlNodePtr       node,
                         GError         **error)
{
  g_return_val_if_fail (buildable,FALSE);
  g_return_val_if_fail (node, FALSE);
  
  /*
   *    <fkey reftable="products" onupdate="NO_ACTION" ondelete="NO_ACTION">
   *        <fk_field name="column_name" reffield="column_name"/>
   *        <fk_field name="column_id" reffield="column_"/>
   *    </fkey>
   */
  xmlChar *name = NULL;
  if (g_strcmp0 ((gchar*)node->name,gdaddlfkeynodes[GDA_DDL_FKEY_NODE]))
    return FALSE;

  GdaDdlFkey *self = GDA_DDL_FKEY (buildable);
  GdaDdlFkeyPrivate *priv = gda_ddl_fkey_get_instance_private (self);

  xmlChar *prop = NULL;

  prop = xmlGetProp (node,(xmlChar *)gdaddlfkeynodes[GDA_DDL_FKEY_REFTABLE]);

  g_assert (prop); /* Bug with xml valdation */

  priv->mp_ref_table = g_strdup ((gchar *)prop);
  xmlFree (prop);
  prop = NULL;

  prop = xmlGetProp (node,(xmlChar *)gdaddlfkeynodes[GDA_DDL_FKEY_ONUPDATE]);

  g_assert(prop);

  priv->m_onupdate = GDA_DDL_FKEY_NO_ACTION;

  for (guint i = 0; i < G_N_ELEMENTS(OnAction);i++)
    {
      if (!g_strcmp0 ((gchar *)prop,OnAction[i]))
        priv->m_onupdate = (GdaDdlFkeyReferenceAction)i;
    }

  xmlFree (prop);
  prop = NULL;

  prop = xmlGetProp (node,(xmlChar *)gdaddlfkeynodes[GDA_DDL_FKEY_ONDELETE]);

  g_assert(prop);

  for (guint i = 0; i < G_N_ELEMENTS(OnAction);i++)
    {
      if (!g_strcmp0 ((gchar *)prop,OnAction[i]))
        priv->m_ondelete = (GdaDdlFkeyReferenceAction)i;
    }

  xmlFree (prop);
  prop = NULL;

  name = NULL;
  xmlChar *reffield = NULL;

  for (xmlNodePtr it = node->children; it; it = it->next)
    {
      if (!g_strcmp0 ((gchar *)it->name,gdaddlfkeynodes[GDA_DDL_FKEY_FKFIELD]))
        {
          name = xmlGetProp (it,(xmlChar *)gdaddlfkeynodes[GDA_DDL_FKEY_FKFIELD_NAME]);

          g_assert(name);
          priv->mp_field = g_list_append (priv->mp_field,g_strdup ((const gchar *)name));
          xmlFree (name);

          reffield = xmlGetProp (it,(xmlChar *)gdaddlfkeynodes[GDA_DDL_FKEY_FKFIELD_REFIELD]);
          g_assert(reffield);
          priv->mp_ref_field = g_list_append (priv->mp_ref_field,
                                              g_strdup ((const gchar *)reffield));
          xmlFree (reffield);
        } /* end of if */
    } /* end of for loop */
  return TRUE;
}

/**
 * gda_ddl_fkey_write_node:
 * @self: An object #GdaDdlFkey
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
gda_ddl_fkey_write_node (GdaDdlBuildable  *buildable,
                         xmlNodePtr rootnode,
                         GError **error)
{
  g_return_val_if_fail (buildable, FALSE);
  g_return_val_if_fail (rootnode,FALSE);

  GdaDdlFkey *self = GDA_DDL_FKEY (buildable);
  GdaDdlFkeyPrivate *priv = gda_ddl_fkey_get_instance_private (self);

  xmlNodePtr node = NULL;
  node  = xmlNewChild (rootnode,NULL,(xmlChar*)gdaddlfkeynodes[GDA_DDL_FKEY_NODE],NULL);

  xmlNewProp (node,BAD_CAST gdaddlfkeynodes[GDA_DDL_FKEY_REFTABLE],
              BAD_CAST priv->mp_ref_table);
  
  xmlNewProp (node,BAD_CAST gdaddlfkeynodes[GDA_DDL_FKEY_ONUPDATE],
              BAD_CAST priv->m_onupdate);

  xmlNewProp (node,BAD_CAST gdaddlfkeynodes[GDA_DDL_FKEY_ONDELETE],
              BAD_CAST priv->m_ondelete);

  GList *it = priv->mp_field;
  GList *jt = priv->mp_ref_field;

  for (; it && jt; it = it->next, jt=jt->next )
    {
      xmlNodePtr tnode = NULL;
      tnode = xmlNewChild (node,NULL,(xmlChar*)gdaddlfkeynodes[GDA_DDL_FKEY_FKFIELD],NULL);

      xmlNewProp (tnode,(xmlChar*)gdaddlfkeynodes[GDA_DDL_FKEY_FKFIELD_NAME],
                  BAD_CAST it->data);

      xmlNewProp (tnode,(xmlChar*)gdaddlfkeynodes[GDA_DDL_FKEY_FKFIELD_REFIELD],
                  BAD_CAST jt->data);
    }

  return TRUE;
}

static void
gda_ddl_fkey_buildable_interface_init (GdaDdlBuildableInterface *iface)
{
    iface->parse_node = gda_ddl_fkey_parse_node;
    iface->write_node = gda_ddl_fkey_write_node;
}

/**
 * gda_ddl_fkey_get_ondelete:
 * @self: An object #GdaDdlFkey
 *
 * Return: ON DELETE action as a string. If the action is not set then the string corresponding to
 * NO_ACTION is returned. 
 *
 * Since: 6.0
 */
const gchar *
gda_ddl_fkey_get_ondelete (GdaDdlFkey *self)
{
  g_return_val_if_fail (self,NULL);

  GdaDdlFkeyPrivate *priv = gda_ddl_fkey_get_instance_private (self);

  return OnAction[priv->m_ondelete];
}

/**
 * gda_ddl_fkey_get_ondelete_id:
 * @self: a #GdaDdlFkey object
 * 
 * The default value is %NO_ACTION
 *
 * Return: ON DELETE action as a #GdaDdlFkeyReferenceAction.
 *
 * Since: 6.0
 */
GdaDdlFkeyReferenceAction
gda_ddl_fkey_get_ondelete_id (GdaDdlFkey *self)
{
  GdaDdlFkeyPrivate *priv = gda_ddl_fkey_get_instance_private (self);

  return priv->m_ondelete;
}

/**
 * gda_ddl_fkey_set_onupdate:
 * @self: An object #GdaDdlFkey
 * @id: #GdaDdlFkeyReferenceAction action to set
 *
 * Set action for ON_UPDATE
 *
 * Since: 6.0
 */
void
gda_ddl_fkey_set_onupdate (GdaDdlFkey *self,
                           GdaDdlFkeyReferenceAction id)
{
  g_return_if_fail (self);

  GdaDdlFkeyPrivate *priv = gda_ddl_fkey_get_instance_private (self);

  priv->m_onupdate = id;
}

/**
 * gda_ddl_fkey_set_ondelete:
 * @self: An object #GdaDdlFkey
 * @id: #GdaDdlFkeyReferenceAction action to set
 *
 * Set action for ON_DELETE
 *
 * Since: 6.0
 */
void
gda_ddl_fkey_set_ondelete (GdaDdlFkey *self,
                           GdaDdlFkeyReferenceAction id)
{
  g_return_if_fail (self);

  GdaDdlFkeyPrivate *priv = gda_ddl_fkey_get_instance_private (self);

  priv->m_ondelete = id;
}

/**
 * gda_ddl_fkey_get_onupdate:
 *
 * Return: ON_UPDATE action as a string. Never %NULL
 *
 * Since: 6.0
 */
const gchar *
gda_ddl_fkey_get_onupdate (GdaDdlFkey *self)
{
  g_return_val_if_fail (self,NULL);

  GdaDdlFkeyPrivate *priv = gda_ddl_fkey_get_instance_private (self);

  return OnAction[priv->m_onupdate];
}

/**
 * gda_ddl_fkey_get_onupdate_id:
 *
 * Return: ON_UPDATE action as a #GdaDdlFkeyReferenceAction
 *
 * Since: 6.0
 */
GdaDdlFkeyReferenceAction
gda_ddl_fkey_get_onupdate_id (GdaDdlFkey *self)
{
  GdaDdlFkeyPrivate *priv = gda_ddl_fkey_get_instance_private (self);

  return priv->m_onupdate;
}

/**
 * gda_ddl_fkey_get_ref_table:
 * @self: a #GdaDdlFkey object
 *
 * Return: Returns reference table name as a string or %NULL if table name
 * hasn't been set.
 *
 * Since: 6.0
 */
const gchar *
gda_ddl_fkey_get_ref_table (GdaDdlFkey *self)
{
  g_return_val_if_fail (self, NULL);

  GdaDdlFkeyPrivate *priv = gda_ddl_fkey_get_instance_private (self);

  return priv->mp_ref_table;
}

/**
 * gda_ddl_fkey_set_ref_table:
 * @self: #GdaDdlFkey object
 * @rtable: reference table name
 *
 * Set reference table
 *
 * Since: 6.0
 */
void
gda_ddl_fkey_set_ref_table (GdaDdlFkey *self,
                            const gchar *rtable)
{
  g_return_if_fail (self);

  GdaDdlFkeyPrivate *priv = gda_ddl_fkey_get_instance_private (self);
  g_free (priv->mp_ref_table);

  priv->mp_ref_table = g_strdup (rtable);
}

/**
 * gda_ddl_fkey_get_field_name:
 * @self: a #GdaDdlFkey object
 *
 * Returns: (element-type utf8) (transfer none): A const #GList of strings where each string
 * corresponds to a foreign key field or %NULL.
 *
 * Since: 6.0
 */
const GList*
gda_ddl_fkey_get_field_name (GdaDdlFkey *self)
{
  g_return_val_if_fail (self, NULL);

  GdaDdlFkeyPrivate *priv = gda_ddl_fkey_get_instance_private (self);

  return priv->mp_field;
}

/**
 * gda_ddl_fkey_get_ref_field:
 * @self: a #GdaDdlFkey object
 *
 * Returns: (element-type utf8) (transfer none): A #GList of strings where each string corresponds
 * to a foreign key reference field or %NULL.
 *
 * Since: 6.0
 */
const GList *
gda_ddl_fkey_get_ref_field (GdaDdlFkey *self)
{
  g_return_val_if_fail (self,NULL);

  GdaDdlFkeyPrivate *priv = gda_ddl_fkey_get_instance_private (self);

  return priv->mp_ref_field;
}

/**
 * gda_ddl_fkey_free:
 * @self: a #GdaDdlFkey object
 *
 * Convenient method to free the object. It is a wrap around g_clear_object()
 *
 * Since: 6.0
 */
void
gda_ddl_fkey_free (GdaDdlFkey *self)
{
  g_clear_object (&self);
}

/**
 * gda_ddl_fkey_set_field:
 * @self: An object #GdaDdlFkey
 * @field: Field name as a string
 * @reffield: A reference field name as a string
 *
 * All arguments should be valid strings.
 *
 * Since: 6.0
 */
void
gda_ddl_fkey_set_field (GdaDdlFkey  *self,
                        const gchar *field,
                        const gchar *reffield)
{
  g_return_if_fail (self);
  g_return_if_fail (field);
  g_return_if_fail (reffield);

  GdaDdlFkeyPrivate *priv = gda_ddl_fkey_get_instance_private (self);

  priv->mp_field = g_list_append (priv->mp_field,(gpointer)field);
  priv->mp_ref_field = g_list_append(priv->mp_ref_field,(gpointer)reffield);
}

/**
 * gda_ddl_fkey_prepare_create:
 * @self: a #GdaDdlFkey instance
 * @op: a #GdaServerOperation to populate
 * @error: error container
 *
 * Prepare @op object for execution by populating with information stored in @self. 
 *
 * Returns: %TRUE if no error or %FALSE otherwise.
 *
 * Since: 6.0
 */
gboolean
gda_ddl_fkey_prepare_create  (GdaDdlFkey *self,
                              GdaServerOperation *op,
                              GError **error)
{
  GdaDdlFkeyPrivate *priv = gda_ddl_fkey_get_instance_private (self);

  if (!gda_server_operation_set_value_at(op,
                                         priv->mp_ref_table,
                                         error,
                                         "/FKEY_S/FKEY_REF_TABLE"))
    return FALSE;

  if (!gda_server_operation_set_value_at(op,
                                         OnAction[priv->m_ondelete],
                                         error,
                                         "/FKEY_S/FKEY_ONDELETE"))
    return FALSE;

  if (!gda_server_operation_set_value_at(op,
                                         OnAction[priv->m_onupdate],
                                         error,
                                         "/FKEY_S/FKEY_ONUPDATE"))
    return FALSE;

  GList *itfield = NULL;
  GList *itreffield = NULL;
  gint fkeycount = 0;

  itreffield = priv->mp_ref_field;
  itfield = priv->mp_field;

  for (;itfield && itreffield;)
    {
      if (!gda_server_operation_set_value_at(op,
                                             itfield->data,
                                             error,
                                             "/FKEY_S/FKEY_FIELDS_A/@FK_FIELD/%d",
                                             fkeycount))
        return FALSE;

      if (!gda_server_operation_set_value_at(op,
                                             itreffield->data,
                                             error,
                                             "/FKEY_S/FKEY_FIELDS_A/@FK_REF_PK_FIELD/%d",
                                             fkeycount))
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
