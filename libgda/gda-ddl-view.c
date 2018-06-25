/* gda-ddl-view.c
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

#include <glib/gi18n-lib.h>
#include "gda-ddl-view.h"
#include "gda-ddl-buildable.h"
#include "gda-ddl-base.h"
#include "gda-lockable.h"
#include "gda-server-provider.h"

typedef struct
{
  gchar *mp_defstring;
  gboolean m_istemp;
  gboolean m_ifnoexist;
  gboolean m_replace;
} GdaDdlViewPrivate;

static void gda_ddl_view_buildable_interface_init (GdaDdlBuildableInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GdaDdlView, gda_ddl_view, GDA_TYPE_DDL_BASE,
                         G_ADD_PRIVATE (GdaDdlView)
                         G_IMPLEMENT_INTERFACE (GDA_TYPE_DDL_BUILDABLE,
                                                gda_ddl_view_buildable_interface_init))

/* This is convenient way to name all nodes from xml file */
enum {
  GDA_DDL_VIEW_NODE,
  GDA_DDL_VIEW_IFNOEXIST,
  GDA_DDL_VIEW_TEMP,
  GDA_DDL_VIEW_REPLACE,
  GDA_DDL_VIEW_NAME,
  GDA_DDL_VIEW_DEFSTR,
  GDA_DDL_VIEW_N_NODES
};

const gchar *gdaddlviewnodes[GDA_DDL_VIEW_N_NODES] = {
  "view",
  "ifnoexist",
  "temp",
  "replace",
  "name",
  "definition"
};

enum {
  PROP_0,
  PROP_VIEW_TEMP,
  PROP_VIEW_REPLACE,
  PROP_VIEW_IFNOEXIST,
  PROP_VIEW_DEFSTR,
  N_PROPS 
};

static GParamSpec *properties [N_PROPS] = {NULL};

GdaDdlView*
gda_ddl_view_new (void)
{
  return g_object_new (GDA_TYPE_DDL_VIEW, NULL);
}

static void
gda_ddl_view_finalize (GObject *object)
{
  GdaDdlView *self = GDA_DDL_VIEW(object);
  GdaDdlViewPrivate *priv = gda_ddl_view_get_instance_private (self);

  g_free (priv->mp_defstring);

  G_OBJECT_CLASS (gda_ddl_view_parent_class)->finalize (object);
}

static void
gda_ddl_view_get_property (GObject    *object,
                           guint       prop_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  GdaDdlView *self = GDA_DDL_VIEW (object);
  GdaDdlViewPrivate *priv = gda_ddl_view_get_instance_private (self);

  switch (prop_id) {
    case PROP_VIEW_TEMP:
      g_value_set_boolean (value,priv->m_istemp);
      break;
    case PROP_VIEW_REPLACE:
      g_value_set_boolean (value,priv->m_replace);
      break;
    case PROP_VIEW_IFNOEXIST:
      g_value_set_boolean (value,priv->m_ifnoexist);
      break;
    case PROP_VIEW_DEFSTR:
      g_value_set_string (value,priv->mp_defstring);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gda_ddl_view_set_property (GObject      *object,
                           guint         prop_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  GdaDdlView *self = GDA_DDL_VIEW (object);
  GdaDdlViewPrivate *priv = gda_ddl_view_get_instance_private (self);

  switch (prop_id){
    case PROP_VIEW_TEMP:
      priv->m_istemp = g_value_get_boolean (value);
      break;
    case PROP_VIEW_REPLACE:
      priv->m_replace = g_value_get_boolean (value);
      break;
    case PROP_VIEW_IFNOEXIST:
      priv->m_ifnoexist = g_value_get_boolean (value);
      break;
    case PROP_VIEW_DEFSTR:
      g_free (priv->mp_defstring);
      priv->mp_defstring = g_value_dup_string (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gda_ddl_view_class_init (GdaDdlViewClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gda_ddl_view_finalize;
  object_class->get_property = gda_ddl_view_get_property;
  object_class->set_property = gda_ddl_view_set_property;

  properties[PROP_VIEW_TEMP] =
    g_param_spec_boolean ("istemp",
                          "IsTemp",
                          "Set if view is temp",
                          FALSE,
                          G_PARAM_READWRITE);
  properties[PROP_VIEW_REPLACE] =
    g_param_spec_boolean ("replace",
                          "Replace",
                          "Set if view should be repalced",
                          TRUE,
                          G_PARAM_READWRITE);
  properties[PROP_VIEW_IFNOEXIST] =
    g_param_spec_boolean ("ifnoexist",
                          "IfNoExist",
                          "Create view if it doesn't exist",
                          FALSE,
                          G_PARAM_READWRITE);
  properties[PROP_VIEW_DEFSTR] =
    g_param_spec_string ("defstring",
                          "Definition",
                          "Define view",
                          FALSE,
                          G_PARAM_READWRITE);
}

static void
gda_ddl_view_init (GdaDdlView *self)
{
  GdaDdlViewPrivate *priv = gda_ddl_view_get_instance_private (self);

  priv->mp_defstring = NULL;
  priv->mp_defstring = NULL;
  priv->m_istemp = TRUE;
}

static gboolean
gda_ddl_view_parse_node (GdaDdlBuildable *buildable,
                         xmlNodePtr node,
                         GError **error)
{
  GdaDdlView *self = GDA_DDL_VIEW (buildable);
  g_return_val_if_fail (node, FALSE);

/*
 * <view name="Total" replace="TRUE" temp="TRUE" ifnotexists="TRUE">
 *   <definition>SELECT id,name FROM CUSTOMER</definition>
 * </view>
 */
  
  xmlChar *prop = NULL;

  prop = xmlGetProp (node,(xmlChar *)gdaddlviewnodes[GDA_DDL_VIEW_NAME]);
  g_assert (prop); /* Bug with xml valdation */

  gda_ddl_base_set_name(GDA_DDL_BASE(self),(gchar*)prop);
  xmlFree (prop);
  prop = NULL;

  prop = xmlGetProp (node,(xmlChar *)gdaddlviewnodes[GDA_DDL_VIEW_REPLACE]);
  if (prop)
    {
      gboolean tres = FALSE;
      if (*prop == 't' || *prop == 'T')
        tres = TRUE;
      else if (*prop == 'f' || *prop == 'F')
        tres = FALSE;
      else
        {
          /*
           * FIXME: this step should never happend
           */
        }
      gda_ddl_view_set_replace (self,tres);
    }

  xmlFree (prop);
  prop = NULL;

  prop = xmlGetProp (node,(xmlChar *)gdaddlviewnodes[GDA_DDL_VIEW_TEMP]);
  if (prop)
    {
      gboolean tres = FALSE;
      if (*prop == 't' || *prop == 'T')
        tres = TRUE;
      else if (*prop == 'f' || *prop == 'F')
        tres = FALSE;
      else
        {
          /*
           * FIXME: this step should never happend
           */
        }
      gda_ddl_view_set_istemp(self,tres);
    }

  xmlFree (prop);
  prop = NULL;

  prop = xmlGetProp (node,(xmlChar *)gdaddlviewnodes[GDA_DDL_VIEW_IFNOEXIST]);
  if (prop)
    {
      gboolean tres = FALSE;
      if (*prop == 't' || *prop == 'T')
        tres = TRUE;
      else if (*prop == 'f' || *prop == 'F')
        tres = FALSE;
      else
        {
          /*
           * FIXME: this step should never happend
           */
        }
      gda_ddl_view_set_ifnoexist(self,tres);
    }

  xmlFree (prop);
  prop = NULL;
/* Loop here is for the following expension if we need to add more children 
 * nodes it will be easy to add 
 */
  for (xmlNodePtr it = node->children; it ; it = it->next)
    {
      if (!g_strcmp0 ((char *)it->name,gdaddlviewnodes[GDA_DDL_VIEW_DEFSTR]))
        {
          xmlChar *def = xmlNodeGetContent (it);
          gda_ddl_view_set_defstring (self,(gchar*)def);
          xmlFree (def);
        }
    }
  
  return TRUE;
}

static gboolean
gda_ddl_view_write_node (GdaDdlBuildable *buildable,
                         xmlTextWriterPtr writer,
                         GError **error)
{
/* We should get something like this: 
 *
 * <view name="Total" replace="TRUE" temp="TRUE" ifnotexists="TRUE">
 *   <definition>SELECT id,name FROM CUSTOMER</definition>
 * </view>
 */
  GdaDdlView *self = GDA_DDL_VIEW (buildable);
  g_return_val_if_fail (writer, FALSE);

  GdaDdlViewPrivate *priv = gda_ddl_view_get_instance_private (self);

  int res = 0;

  res = xmlTextWriterStartElement(writer, 
                                  BAD_CAST gdaddlviewnodes[GDA_DDL_VIEW_NODE]);
  if (res < 0)
    {
      g_set_error (error,
                   GDA_DDL_BUILDABLE_ERROR,
                   GDA_DDL_BUILDABLE_ERROR_START_ELEMENT,
                   _("Can't set start element in xml tree\n"));
      return FALSE;
    }

  res = xmlTextWriterWriteAttribute (writer,
                    (const xmlChar*)gdaddlviewnodes[GDA_DDL_VIEW_NAME],
                    (xmlChar*)gda_ddl_base_get_name(GDA_DDL_BASE(self)));

  if (res < 0)
    {
      g_set_error (error,
                   GDA_DDL_BUILDABLE_ERROR,
                   GDA_DDL_BUILDABLE_ERROR_ATTRIBUTE,
                   _("Can't set reftable attribute to element\n"));
      return FALSE;
    }

  res = xmlTextWriterWriteAttribute (writer,
                (const xmlChar*)gdaddlviewnodes[GDA_DDL_VIEW_IFNOEXIST],
                (xmlChar*)GDA_BOOL_TO_STR(gda_ddl_view_get_ifnoexist(self)));

  if (res < 0)
    {
      g_set_error (error,
                   GDA_DDL_BUILDABLE_ERROR,
                   GDA_DDL_BUILDABLE_ERROR_ATTRIBUTE,
                   _("Can't set onupdate attribute to element %s\n"),
                   gdaddlviewnodes[GDA_DDL_VIEW_IFNOEXIST]);
      return FALSE;
    }

  res = xmlTextWriterWriteAttribute (writer,
                    (const xmlChar*)gdaddlviewnodes[GDA_DDL_VIEW_TEMP],
                    (xmlChar*)GDA_BOOL_TO_STR(gda_ddl_view_get_istemp(self)));

  if (res < 0)
    {
      g_set_error (error,
                   GDA_DDL_BUILDABLE_ERROR,
                   GDA_DDL_BUILDABLE_ERROR_ATTRIBUTE,
                   _("Can't set ondelete attribute to element %s\n"),
                   gdaddlviewnodes[GDA_DDL_VIEW_TEMP]);
      return FALSE;
    }

  res = xmlTextWriterWriteAttribute (writer,
                   (const xmlChar*)gdaddlviewnodes[GDA_DDL_VIEW_REPLACE],
                   (xmlChar*)GDA_BOOL_TO_STR(gda_ddl_view_get_replace(self)));

  if (res < 0)
    {
      g_set_error (error,
                   GDA_DDL_BUILDABLE_ERROR,
                   GDA_DDL_BUILDABLE_ERROR_ATTRIBUTE,
                   _("Can't set ondelete attribute to element %s\n"),
                   gdaddlviewnodes[GDA_DDL_VIEW_REPLACE]);
      return FALSE;
    }

  res = xmlTextWriterWriteElement(writer, 
                                BAD_CAST gdaddlviewnodes[GDA_DDL_VIEW_DEFSTR],
                                (xmlChar*)gda_ddl_view_get_defstring(self));
  if (res < 0)
    {
      g_set_error (error,
                   GDA_DDL_BUILDABLE_ERROR,
                   GDA_DDL_BUILDABLE_ERROR_START_ELEMENT,
                   _("Can't set start element %s in xml tree\n"),
                   gdaddlviewnodes[GDA_DDL_VIEW_DEFSTR]);
      return FALSE;
    }

  res = xmlTextWriterEndElement (writer);

  if (res < 0) {
      g_set_error (error,
                   GDA_DDL_BUILDABLE_ERROR,
                   GDA_DDL_BUILDABLE_ERROR_END_ELEMENT,
                   _("Can't close element %s\n"),
                   gdaddlviewnodes[GDA_DDL_VIEW_NODE]);
      return FALSE;
  }

  return TRUE;
}

static void
gda_ddl_view_buildable_interface_init (GdaDdlBuildableInterface *iface)
{
  iface->parse_node = gda_ddl_view_parse_node;
  iface->write_node = gda_ddl_view_write_node;
}

/**
 * gda_ddl_view_free:
 * @self: a #GdaDdlView object
 *
 * Convenient method to fdelete the object and free the memory.
 *
 * Since: 6.0
 */
void
gda_ddl_view_free (GdaDdlView *self)
{
  g_clear_object (&self);
}

/**
 * gda_ddl_view_get_istemp:
 * @self: a #GdaDdlView object
 *
 * Returns: %TRUE if the view is temporary, %FALSE otherwise
 * Since: 6.0
 */
gboolean
gda_ddl_view_get_istemp (GdaDdlView *self)
{
  g_return_val_if_fail (self, FALSE);
  GdaDdlViewPrivate *priv = gda_ddl_view_get_instance_private (self);
  return priv->m_istemp;
}

/**
 * gda_ddl_view_set_istemp:
 * @self: a #GdaDdlView object
 * @temp: value to set
 *
 * Since: 6.0
 */
void
gda_ddl_view_set_istemp (GdaDdlView *self,gboolean temp)
{
  g_return_if_fail (self);
  GdaDdlViewPrivate *priv = gda_ddl_view_get_instance_private (self);
  priv->m_istemp = temp;
}

/**
 * gda_ddl_view_get_ifnoexist:
 * @self: a #GdaDdlView object
 *
 * Returns: %TRUE if th view should be created with "IF NOT EXISTS" key, %FALSE
 * otherwise
 * Since: 6.0
 */
gboolean
gda_ddl_view_get_ifnoexist (GdaDdlView *self)
{
  g_return_val_if_fail (self,FALSE);
  GdaDdlViewPrivate *priv = gda_ddl_view_get_instance_private (self);
  return priv->m_ifnoexist;
}

/**
 * gda_ddl_view_set_ifnoexist:
 * @self: a #GdaDdlView object
 * @noexist: a value to set
 *
 * Since: 6.0
 */
void
gda_ddl_view_set_ifnoexist (GdaDdlView *self,
                            gboolean noexist)
{
  g_return_if_fail (self);
  GdaDdlViewPrivate *priv = gda_ddl_view_get_instance_private (self);
  priv->m_ifnoexist = noexist;
}

/**
 * gda_ddl_view_get_defstring:
 * @self: a #GdaDdlView object
 *
 * Returns: view definition string
 * Sinc: 6.0
 */
const gchar*
gda_ddl_view_get_defstring (GdaDdlView *self)
{
  g_return_val_if_fail (self,FALSE);
  GdaDdlViewPrivate *priv = gda_ddl_view_get_instance_private (self);
  return priv->mp_defstring;
}

/**
 * gda_ddl_view_set_defstring:
 * @self: a #GdaDdlView object
 * @str: view definition string to set. Should be valis SQL string
 *
 * Since: 6.0
 */
void
gda_ddl_view_set_defstring (GdaDdlView *self,
                            const gchar *str)
{
  g_return_if_fail (self);
  GdaDdlViewPrivate *priv = gda_ddl_view_get_instance_private (self);
  g_free (priv->mp_defstring);
  priv->mp_defstring = g_strdup (str);
}

/**
 * gda_ddl_view_get_replace:
 * @self: a #GdaDdlView object
 *
 * Returns: %TRUE if the current view should dreplace the existing one in the
 * databse, %FALSE otherwise.
 *
 * Since: 6.0
 */
gboolean
gda_ddl_view_get_replace (GdaDdlView *self)
{
  g_return_val_if_fail (self, FALSE);
  GdaDdlViewPrivate *priv = gda_ddl_view_get_instance_private (self);

  return priv->m_replace;
}

/**
 * gda_ddl_view_set_replace:
 * @self: a #GdaDdlView object
 * @replace: a value to set
 *
 * Since: 6.0
 */
void
gda_ddl_view_set_replace (GdaDdlView *self,
                          gboolean replace)
{
  g_return_if_fail (self);
  GdaDdlViewPrivate *priv = gda_ddl_view_get_instance_private (self);

  priv->m_replace = replace;
}


gboolean
gda_ddl_view_create (GdaDdlView *self,
                     GdaConnection *cnc,
                     GError **error)
{
  g_return_val_if_fail (self,FALSE);
  g_return_val_if_fail (cnc,FALSE);

  if (!gda_connection_is_opened(cnc))
    return FALSE;

  gda_lockable_lock((GdaLockable*)cnc);

  GdaServerProvider *provider = NULL;
  GdaServerOperation *op = NULL;
 
  provider = gda_connection_get_provider (cnc);

  g_print ("%s:%d\n",__FILE__,__LINE__);
  op = gda_server_provider_create_operation(provider,
                                            cnc,
                                            GDA_SERVER_OPERATION_CREATE_VIEW,
                                            NULL,
                                            error);
  if (!op)
    goto on_error;

  if (!gda_ddl_view_prepare_create(self,op,error))
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




  gda_lockable_unlock((GdaLockable*)cnc);
  return TRUE;

on_error:
  gda_lockable_unlock((GdaLockable*)cnc);
  return FALSE; 
}


gboolean
gda_ddl_view_prepare_create (GdaDdlView *self,
                             GdaServerOperation *op,
                             GError **error)
{
  g_return_val_if_fail (self,FALSE);
  g_return_val_if_fail (op,FALSE);

  GdaDdlViewPrivate *priv = gda_ddl_view_get_instance_private (self);

  if (!gda_server_operation_set_value_at(op,
                                         gda_ddl_base_get_full_name(GDA_DDL_BASE(self)),
                                         error,
                                         "/VIEW_DEF_P/VIEW_NAME"))
    return FALSE;

  if (!gda_server_operation_set_value_at(op,
                                         GDA_BOOL_TO_STR(priv->m_replace),
                                         error,
                                         "/VIEW_DEF_P/VIEW_OR_REPLACE"))
    return FALSE;

  if (!gda_server_operation_set_value_at(op,
                                         GDA_BOOL_TO_STR(priv->m_ifnoexist),
                                         error,
                                         "/VIEW_DEF_P/VIEW_IFNOTEXISTS"))
    return FALSE;

  if (!gda_server_operation_set_value_at(op,
                                         GDA_BOOL_TO_STR(priv->m_istemp),
                                         error,
                                         "/VIEW_DEF_P/VIEW_TEMP"))
    return FALSE;

  if (!gda_server_operation_set_value_at(op,
                                         priv->mp_defstring,
                                         error,
                                         "/VIEW_DEF_P/VIEW_DEF"))
    return FALSE;

  return  TRUE;
}

/**
 * gda_ddl_view_new_from_meta:
 * @view: a #GdaMetaView instance
 *
 * Create new #GdaDdlView object from the corresponding #GdaMetaView object
 *
 * Returns: New instance of #GdaDdlView 
 */
GdaDdlView*
gda_ddl_view_new_from_meta (GdaMetaView *view)
{
  if (!view)
    return gda_ddl_view_new();

  GdaDdlView *ddlview = gda_ddl_view_new();

  gda_ddl_view_set_defstring (ddlview,view->view_def);
  gda_ddl_view_set_replace (ddlview,view->is_updatable);

  return ddlview;
}



