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
#include <glib.h>
#include "gda-ddl-buildable.h"

G_DEFINE_QUARK (gda-ddl-fkey-error, gda_ddl_fkey_error)

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
                         G_IMPLEMENT_INTERFACE (GDA_TYPE_DDL_BUILDABLE,
                                                gda_ddl_fkey_buildable_interface_init))

static const gchar *OnAction[] = {
    "NO ACTION",
    "SET NULL",
    "RESTRICT",
    "SET DEFAULT",
    "CASCADE"
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
 * this method is called from #GdaDdlCreator suring parsing the imput xml file.
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
 * Returns: %FALSE if an error occures, %TRUE otherwise
 *
 * Since: 6.0
 */
static gboolean
gda_ddl_fkey_parse_node (GdaDdlBuildable *buildable,
                         xmlNodePtr       node,
                         GError         **error)
{
  GdaDdlFkey *self = GDA_DDL_FKEY (buildable);
  g_return_val_if_fail (node, FALSE);
  //    g_return_val_if_fail(!*error, FALSE);
  /*
   *    <fkey reftable="products" onupdate="NO_ACTION" ondelete="NO_ACTION">
   *        <fk_field name="column_name" reffield="column_name"/>
   *        <fk_field name="column_id" reffield="column_"/>
   *    </fkey>
   */
  GdaDdlFkeyPrivate *priv = gda_ddl_fkey_get_instance_private (self);
  xmlChar *prop = NULL;

  prop = xmlGetProp (node,(xmlChar *)"reftable");

  g_assert (prop); /* Bug with xml valdation */

  priv->mp_ref_table = g_strdup ((gchar *)prop);
  xmlFree (prop);
  prop = NULL;

  prop = xmlGetProp (node,(xmlChar *)"onupdate");

  g_assert(prop);

  priv->m_onupdate = GDA_DDL_FKEY_NO_ACTION;

  for (guint i = 0; i < G_N_ELEMENTS(OnAction);i++) {
      if (!g_strcmp0 (prop,OnAction[i]))
        priv->m_onupdate = (GdaDdlFkeyReferenceAction)i;
  }

  xmlFree (prop);
  prop = NULL;

  prop = xmlGetProp (node,(xmlChar *)"ondelete");

  g_assert(prop);

  for (guint i = 0; i < G_N_ELEMENTS(OnAction);i++) {
      if (!g_strcmp0 (prop,OnAction[i]))
        priv->m_ondelete = (GdaDdlFkeyReferenceAction)i;
  }

  xmlFree (prop);
  prop = NULL;

  xmlChar *name = NULL;
  xmlChar *reffield = NULL;

  for (xmlNodePtr it = node->children; it; it = it->next) {
      if (!g_strcmp0 ((gchar *)it->name, "fk_field")) {
          name = xmlGetProp (it,(xmlChar *)"name");

          g_assert(name);
          priv->mp_field = g_list_append (priv->mp_field,
                                          g_strdup ((const gchar *)name));
          xmlFree (name);

          reffield = xmlGetProp (it, (xmlChar *)"reffield");
          g_assert(reffield);
          priv->mp_ref_field = g_list_append (priv->mp_ref_field,
                                              g_strdup ((const gchar *)reffield));
          xmlFree (reffield);
      } /* end of if */
  } /* end of for loop */
  return TRUE;
}

/**
 * gda_ddl_fkey_write_xml:
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
                         xmlTextWriterPtr  writer,
                         GError          **error)
{
  GdaDdlFkey *self = GDA_DDL_FKEY (buildable);
  g_return_val_if_fail (writer, FALSE);

  GdaDdlFkeyPrivate *priv = gda_ddl_fkey_get_instance_private (self);

  int res = 0;

  res = xmlTextWriterStartElement(writer, BAD_CAST "fkey");
  if (res < 0) {
      g_set_error (error,
                   GDA_DDL_FKEY_ERROR,
                   GDA_DDL_FKEY_ERROR_START_ELEMENT,
                   _("Can't set start element <fkey> in xml tree\n"));
      return FALSE;
  }

  res = xmlTextWriterWriteAttribute (writer,"reftable",
                                     (xmlChar*)priv->mp_ref_table);

  if (res < 0) {
      g_set_error (error,
                   GDA_DDL_FKEY_ERROR,
                   GDA_DDL_FKEY_ERROR_ATTRIBUTE,
                   _("Can't set reftable attribute to element <fkey>\n"));
      return FALSE;
  }

  res = xmlTextWriterWriteAttribute (writer,"onupdate",
                                     (xmlChar*)OnAction[priv->m_onupdate]);

  if (res < 0) {
      g_set_error (error,
                   GDA_DDL_FKEY_ERROR,
                   GDA_DDL_FKEY_ERROR_ATTRIBUTE,
                   _("Can't set onupdate attribute to element <fkey>\n"));
      return FALSE;
  }

  res = xmlTextWriterWriteAttribute (writer,"ondelete",
                                     (xmlChar*)OnAction[priv->m_ondelete]);

  if (res < 0) {
      g_set_error (error,
                   GDA_DDL_FKEY_ERROR,
                   GDA_DDL_FKEY_ERROR_ATTRIBUTE,
                   _("Can't set ondelete attribute to element <fkey>\n"));
      return FALSE;
  }

  GList *it = priv->mp_field;
  GList *jt = priv->mp_ref_field;

  for (; it && jt; it = it->next, jt=jt->next ) {
      res = xmlTextWriterStartElement(writer, BAD_CAST "fk_field");
      if (res < 0) {
          g_set_error (error,
                       GDA_DDL_FKEY_ERROR,
                       GDA_DDL_FKEY_ERROR_START_ELEMENT,
                       _("Can't set start element <fk_field> in xml tree\n"));
          return FALSE;
      }

      res = xmlTextWriterWriteAttribute (writer,"name",(xmlChar*)it->data);

      if (res < 0) {
          g_set_error (error,
                       GDA_DDL_FKEY_ERROR,
                       GDA_DDL_FKEY_ERROR_ATTRIBUTE,
                       _("Can't set ondelete attribute to element <fk_field>\n"));
          return FALSE;
      }

      res = xmlTextWriterWriteAttribute (writer,"reffield",(xmlChar*)jt->data);

      if (res < 0) {
          g_set_error (error,
                       GDA_DDL_FKEY_ERROR,
                       GDA_DDL_FKEY_ERROR_ATTRIBUTE,
                       _("Can't set ondelete attribute to element <fk_field>\n"));
          return FALSE;
      }

      res = xmlTextWriterEndElement (writer);

      if (res < 0) {
          g_set_error (error,
                       GDA_DDL_FKEY_ERROR,
                       GDA_DDL_FKEY_ERROR_END_ELEMENT,
                       _("Can't close element <fk_field>\n"));
          return FALSE;
      }
  }

  res = xmlTextWriterEndElement (writer);

  if (res < 0) {
      g_set_error (error,
                   GDA_DDL_FKEY_ERROR,
                   GDA_DDL_FKEY_ERROR_END_ELEMENT,
                   _("Can't close element <fkey>\n"));
      return FALSE;
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
 * Return: ON DELETE action as a string or %NULL
 * if the action hasn't been set
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
 * Return: ON DELETE action as a #GdaDdlFkeyReferenceAction
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
 * Set action for ON UPDATE
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
 * Set action for ON DELETE
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
 * Return: ON UPDATE action as a string. Never %NULL
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
 * Return: ON UPDATE action as a #GdaDdlFkeyReferenceAction
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
 * Returns: (transfer none): A const #GList of strings where each string
 * corresponds to a foregin key field or %NULL.
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
 * Returns: (transfer none): A #GList of strings where each string corresponds
 * to a foregin key reference field or %NULL.
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



