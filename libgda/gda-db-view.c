/* gda-db-view.c
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
#define G_LOG_DOMAIN "GDA-db-view"

#include <glib/gi18n-lib.h>
#include "gda-db-view.h"
#include "gda-db-buildable.h"
#include "gda-db-base.h"
#include "gda-lockable.h"
#include "gda-server-provider.h"
#include "gda-ddl-modifiable.h"

typedef struct
{
  gchar *mp_defstring;
  gboolean m_istemp;
  gboolean m_ifnoexist;
  gboolean m_replace;
} GdaDbViewPrivate;

/**
 * SECTION:gda-db-view
 * @short_description: Object to represent view database object
 * @see_also: #GdaDbTable, #GdaDbCatalog
 * @stability: Stable
 * @include: libgda/libgda.h
 *
 * This object represents a view of a database. The view can be constracted manually
 * using API or generated from xml file together with other databse objects. See #GdaDbCatalog.
 * #GdaDbView implements #GdaDbBuildable interface for parsing xml file. This is typical example
 * how #GdaDbView can be used
 *
 * |[<!-- language="C" -->
 *
 *  GdaDbView *myview = gda_db_view_new ();
 *  gda_db_base_set_name (GDA_DB_BASE (myview), "MyView");
 *  gda_db_view_set_istemp (myview, FALSE);
 *  gda_db_view_set_defstring (myview, "SELECT name, project_id FROM NewEmployee");
 *
 *  res = gda_db_view_create (myview, fixture->cnc, TRUE, &error);
 *
 *  if (!res)
 *    GDA_PGSQL_ERROR_HANDLE (error);
 *  ]|
 */

static void gda_db_view_buildable_interface_init (GdaDbBuildableInterface *iface);
static void gda_ddl_modifiable_interface_init (GdaDdlModifiableInterface *iface);

static gboolean gda_db_view_create (GdaDdlModifiable *self, GdaConnection *cnc,
                                    gpointer user_data, GError **error);
static gboolean gda_db_view_drop (GdaDdlModifiable *self, GdaConnection *cnc,
                                  gpointer user_data, GError **error);
static gboolean gda_db_view_rename (GdaDdlModifiable *old_name, GdaConnection *cnc,
                                    gpointer new_name, GError **error);

G_DEFINE_TYPE_WITH_CODE (GdaDbView, gda_db_view, GDA_TYPE_DB_BASE,
                         G_ADD_PRIVATE (GdaDbView)
                         G_IMPLEMENT_INTERFACE (GDA_TYPE_DB_BUILDABLE,
                                                gda_db_view_buildable_interface_init)
                         G_IMPLEMENT_INTERFACE (GDA_TYPE_DDL_MODIFIABLE,
                                                gda_ddl_modifiable_interface_init))

/* This is convenient way to name all nodes from xml file */
enum {
    GDA_DB_VIEW_NODE,
    GDA_DB_VIEW_IFNOEXIST,
    GDA_DB_VIEW_TEMP,
    GDA_DB_VIEW_REPLACE,
    GDA_DB_VIEW_NAME,
    GDA_DB_VIEW_DEFSTR,
    GDA_DB_VIEW_N_NODES
};

static const gchar *gdadbviewnodes[GDA_DB_VIEW_N_NODES] = {
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

/**
 * gda_db_view_new:
 *
 * Returns: (transfer full): A new instance of #GdaDbView.
 *
 * Stability: Stable
 * Since: 6.0
 */
GdaDbView*
gda_db_view_new (void)
{
  return g_object_new (GDA_TYPE_DB_VIEW, NULL);
}

static void
gda_db_view_finalize (GObject *object)
{
  GdaDbView *self = GDA_DB_VIEW(object);
  GdaDbViewPrivate *priv = gda_db_view_get_instance_private (self);

  g_free (priv->mp_defstring);

  G_OBJECT_CLASS (gda_db_view_parent_class)->finalize (object);
}

static void
gda_db_view_get_property (GObject    *object,
                          guint       prop_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
  GdaDbView *self = GDA_DB_VIEW (object);
  GdaDbViewPrivate *priv = gda_db_view_get_instance_private (self);

  switch (prop_id)
    {
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
gda_db_view_set_property (GObject      *object,
                          guint         prop_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
  GdaDbView *self = GDA_DB_VIEW (object);
  GdaDbViewPrivate *priv = gda_db_view_get_instance_private (self);

  switch (prop_id)
    {
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
gda_db_view_class_init (GdaDbViewClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gda_db_view_finalize;
  object_class->get_property = gda_db_view_get_property;
  object_class->set_property = gda_db_view_set_property;

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

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gda_db_view_init (GdaDbView *self)
{
  GdaDbViewPrivate *priv = gda_db_view_get_instance_private (self);

  priv->mp_defstring = NULL;
  priv->mp_defstring = NULL;
  priv->m_istemp = TRUE;
}

static gboolean
gda_db_view_parse_node (GdaDbBuildable *buildable,
                        xmlNodePtr node,
                        GError **error)
{
  g_return_val_if_fail (node, FALSE);
  g_return_val_if_fail (node, FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  GdaDbView *self = GDA_DB_VIEW (buildable);

/*
 * <view name="Total" replace="TRUE" temp="TRUE" ifnoexists="TRUE">
 *   <definition>SELECT id,name FROM CUSTOMER</definition>
 * </view>
 */

  xmlChar *prop = NULL;

  prop = xmlGetProp (node, (xmlChar *)gdadbviewnodes[GDA_DB_VIEW_NAME]);
  g_assert (prop); /* Bug with xml valdation */

  gda_db_base_set_name (GDA_DB_BASE(self), (gchar*)prop);
  xmlFree (prop);
  prop = NULL;

  prop = xmlGetProp (node, (xmlChar *)gdadbviewnodes[GDA_DB_VIEW_REPLACE]);
  if (prop)
    g_object_set (G_OBJECT(self), "replace", *prop == 't' || *prop == 'T' ? TRUE : FALSE, NULL);

  xmlFree (prop);
  prop = NULL;

  prop = xmlGetProp (node, (xmlChar *)gdadbviewnodes[GDA_DB_VIEW_TEMP]);
  if (prop)
    g_object_set (G_OBJECT(self), "istemp", *prop == 't' || *prop == 'T' ? TRUE : FALSE, NULL);

  xmlFree (prop);
  prop = NULL;

  prop = xmlGetProp (node, (xmlChar *)gdadbviewnodes[GDA_DB_VIEW_IFNOEXIST]);
  if (prop)
    g_object_set (G_OBJECT(self), "ifnoexist", *prop == 't' || *prop == 'T' ? TRUE : FALSE, NULL);

  xmlFree (prop);
  prop = NULL;
/* Loop here is for the following expension if we need to add more children
 * nodes it will be easy to add
 */
  for (xmlNodePtr it = node->children; it ; it = it->next)
    {
      if (!g_strcmp0 ((char *)it->name,gdadbviewnodes[GDA_DB_VIEW_DEFSTR]))
        {
          xmlChar *def = xmlNodeGetContent (it);
          gda_db_view_set_defstring (self, (gchar*)def);
          xmlFree (def);
        }
    }

  return TRUE;
}

static gboolean
gda_db_view_write_node (GdaDbBuildable *buildable,
                        xmlNodePtr rootnode,
                        GError **error)
{
  g_return_val_if_fail (buildable, FALSE);
  g_return_val_if_fail (rootnode, FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  GdaDbView *self = GDA_DB_VIEW (buildable);
  GdaDbViewPrivate *priv = gda_db_view_get_instance_private (self);

  xmlNodePtr node = NULL;
  node  = xmlNewChild (rootnode,
                       NULL,
                       BAD_CAST gdadbviewnodes[GDA_DB_VIEW_NODE],
                       NULL);

  xmlNewProp (node,
              BAD_CAST gdadbviewnodes[GDA_DB_VIEW_NODE],
              BAD_CAST gda_db_base_get_name (GDA_DB_BASE(self)));

  xmlNewProp (node,
              BAD_CAST gdadbviewnodes[GDA_DB_VIEW_REPLACE],
              BAD_CAST GDA_BOOL_TO_STR(priv->m_replace));

  xmlNewProp (node,
              BAD_CAST gdadbviewnodes[GDA_DB_VIEW_TEMP],
              BAD_CAST GDA_BOOL_TO_STR(priv->m_istemp));

  xmlNewProp (node,
              BAD_CAST gdadbviewnodes[GDA_DB_VIEW_IFNOEXIST],
              BAD_CAST GDA_BOOL_TO_STR(priv->m_ifnoexist));

  xmlNewChild (node,
               NULL,
               BAD_CAST gdadbviewnodes[GDA_DB_VIEW_DEFSTR],
               BAD_CAST priv->mp_defstring);

  return TRUE;
}

static void
gda_db_view_buildable_interface_init (GdaDbBuildableInterface *iface)
{
  iface->parse_node = gda_db_view_parse_node;
  iface->write_node = gda_db_view_write_node;
}

static void gda_ddl_modifiable_interface_init (GdaDdlModifiableInterface *iface)
{
  iface->create = gda_db_view_create;
  iface->drop   = gda_db_view_drop;
  iface->rename = gda_db_view_rename;
}
/**
 * gda_db_view_get_istemp:
 * @self: a #GdaDbView object
 *
 * Returns: %TRUE if the view is temporary, %FALSE otherwise
 *
 * Stability: Stable
 * Since: 6.0
 */
gboolean
gda_db_view_get_istemp (GdaDbView *self)
{
  g_return_val_if_fail (self, FALSE);
  GdaDbViewPrivate *priv = gda_db_view_get_instance_private (self);
  return priv->m_istemp;
}

/**
 * gda_db_view_set_istemp:
 * @self: a #GdaDbView object
 * @temp: value to set
 *
 * Stability: Stable
 * Since: 6.0
 */
void
gda_db_view_set_istemp (GdaDbView *self,
                        gboolean temp)
{
  g_return_if_fail (self);
  GdaDbViewPrivate *priv = gda_db_view_get_instance_private (self);
  priv->m_istemp = temp;
}

/**
 * gda_db_view_get_ifnoexist:
 * @self: a #GdaDbView object
 *
 * Returns: %TRUE if th view should be created with "IF NOT EXISTS" key, %FALSE
 * otherwise
 *
 * Stability: Stable
 * Since: 6.0
 */
gboolean
gda_db_view_get_ifnoexist (GdaDbView *self)
{
  g_return_val_if_fail (self, FALSE);
  GdaDbViewPrivate *priv = gda_db_view_get_instance_private (self);
  return priv->m_ifnoexist;
}

/**
 * gda_db_view_set_ifnoexist:
 * @self: a #GdaDbView object
 * @noexist: a value to set
 *
 * Stability: Stable
 * Since: 6.0
 */
void
gda_db_view_set_ifnoexist (GdaDbView *self,
                           gboolean noexist)
{
  g_return_if_fail (self);
  GdaDbViewPrivate *priv = gda_db_view_get_instance_private (self);
  priv->m_ifnoexist = noexist;
}

/**
 * gda_db_view_get_defstring:
 * @self: a #GdaDbView object
 *
 * Returns: view definition string
 *
 * Stability: Stable
 * Since: 6.0
 */
const gchar*
gda_db_view_get_defstring (GdaDbView *self)
{
  g_return_val_if_fail (self, FALSE);
  GdaDbViewPrivate *priv = gda_db_view_get_instance_private (self);
  return priv->mp_defstring;
}

/**
 * gda_db_view_set_defstring:
 * @self: a #GdaDbView object
 * @str: view definition string to set. Should be valid SQL string
 *
 * Stability: Stable
 * Since: 6.0
 */
void
gda_db_view_set_defstring (GdaDbView *self,
                            const gchar *str)
{
  g_return_if_fail (self);
  GdaDbViewPrivate *priv = gda_db_view_get_instance_private (self);
  g_free (priv->mp_defstring);
  priv->mp_defstring = g_strdup (str);
}

/**
 * gda_db_view_get_replace:
 * @self: a #GdaDbView object
 *
 * Returns: %TRUE if the current view should replace the existing one in the
 * database, %FALSE otherwise.
 *
 * Stability: Stable
 * Since: 6.0
 */
gboolean
gda_db_view_get_replace (GdaDbView *self)
{
  g_return_val_if_fail (self, FALSE);
  GdaDbViewPrivate *priv = gda_db_view_get_instance_private (self);

  return priv->m_replace;
}

/**
 * gda_db_view_set_replace:
 * @self: a #GdaDbView object
 * @replace: a value to set
 *
 * Stability: Stable
 * Since: 6.0
 */
void
gda_db_view_set_replace (GdaDbView *self,
			 gboolean replace)
{
  g_return_if_fail (self);
  GdaDbViewPrivate *priv = gda_db_view_get_instance_private (self);

  priv->m_replace = replace;
}

/**
 * gda_db_view_create:
 * @self: a #GdaDbView instance
 * @cnc: open connection for the operation
 * @ifnotexists: if set to %TRUE, the view will be created using IF NOT EXISTS flag
 * @error: error container
 *
 * This method performs CREATE_VIEW operation over @cnc using data stored in @self
 * It is a convenient method to perform operation. See gda_db_view_prepare_create() if better
 * flexibility is needed.
 *
 * For convenience, @ifnotexists should be used to specify if the view should be created if exist.
 * The internal property "ifnoexists" will be ignored.
 *
 * Returns: %TRUE if no error, %FASLE otherwise
 *
 * Stability: Stable
 * Since: 6.0
 */
static gboolean
gda_db_view_create (GdaDdlModifiable *self,
                    GdaConnection *cnc,
                    gpointer user_data,
                    GError **error)
{
  GdaServerProvider *provider = NULL;
  GdaServerOperation *op = NULL;
  GdaDbView *view = GDA_DB_VIEW (self);

  gda_lockable_lock ((GdaLockable*)cnc);

  provider = gda_connection_get_provider (cnc);
  GdaDbViewPrivate *priv = gda_db_view_get_instance_private (view);

  if (!gda_db_base_get_name (GDA_DB_BASE (view)))
    {
      g_set_error (error, GDA_DDL_MODIFIABLE_ERROR, GDA_DDL_MODIFIABLE_MISSED_DATA,
                   _("View name is not set"));
      return FALSE;
    }

  if (!priv->mp_defstring)
    {
      g_set_error (error, GDA_DDL_MODIFIABLE_ERROR, GDA_DDL_MODIFIABLE_MISSED_DATA,
                   _("View definition is not set"));
      return FALSE;
    }

  op = gda_server_provider_create_operation (provider,
                                             cnc,
                                             GDA_SERVER_OPERATION_CREATE_VIEW,
                                             NULL,
                                             error);
  if (!op)
    goto on_error;

  if (!gda_server_operation_set_value_at (op,
                                          gda_db_base_get_name (GDA_DB_BASE(self)),
                                          error,
                                          "/VIEW_DEF_P/VIEW_NAME"))
    goto on_error;

  if (!gda_server_operation_set_value_at (op,
                                          GDA_BOOL_TO_STR(priv->m_replace),
                                          error,
                                          "/VIEW_DEF_P/VIEW_OR_REPLACE"))
    goto on_error;

  if (!gda_server_operation_set_value_at (op,
                                          GDA_BOOL_TO_STR(priv->m_istemp),
                                          error,
                                          "/VIEW_DEF_P/VIEW_TEMP"))
    goto on_error;

  if (!gda_server_operation_set_value_at (op,
                                          priv->mp_defstring,
                                          error,
                                          "/VIEW_DEF_P/VIEW_DEF"))
    goto on_error;

  if(!gda_server_provider_perform_operation (provider, cnc, op, error))
    goto on_error;

  g_object_unref (op);
  gda_lockable_unlock ((GdaLockable*)cnc);
  return TRUE;

on_error:
  if (op) g_object_unref (op);
  gda_lockable_unlock ((GdaLockable*)cnc);
  return FALSE;
}

/**
 * gda_db_view_prepare_create:
 * @self: a #GdaDbView instance
 * @op: #GdaServerOperation instance to populate
 * @error: error container
 *
 * Populate @op with information needed to perform CREATE_VIEW operation. This method was desgned
 * for internal use and will be obsolete in the future. Do not use it for the new code.
 *
 * Stability: Stable
 * Returns: %TRUE if succeeded and %FALSE otherwise.
 */
gboolean
gda_db_view_prepare_create (GdaDbView *self,
                            GdaServerOperation *op,
                            GError **error)
{
  g_return_val_if_fail (self, FALSE);
  g_return_val_if_fail (op, FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  GdaDbViewPrivate *priv = gda_db_view_get_instance_private (self);

  if (!gda_server_operation_set_value_at (op,
                                          gda_db_base_get_name (GDA_DB_BASE(self)),
                                          error,
                                          "/VIEW_DEF_P/VIEW_NAME"))
    return FALSE;

  if (!gda_server_operation_set_value_at (op,
                                          GDA_BOOL_TO_STR(priv->m_replace),
                                          error,
                                          "/VIEW_DEF_P/VIEW_OR_REPLACE"))
    return FALSE;


  if (!gda_server_operation_set_value_at (op,
                                          GDA_BOOL_TO_STR(priv->m_istemp),
                                          error,
                                          "/VIEW_DEF_P/VIEW_TEMP"))
    return FALSE;

  if (!gda_server_operation_set_value_at (op,
                                          priv->mp_defstring,
                                          error,
                                          "/VIEW_DEF_P/VIEW_DEF"))
    return FALSE;

  return  TRUE;
}

/**
 * gda_db_view_drop:
 * @self: #GdaDbView instance to drop form the database
 * @cnc: Connection to the database to perform the operation
 * @ifexists: if %TRUE IF NOT EXISTS will be added to the SQL command
 * @action: actions for referencing objecrs. See #GdaDbViewRefAction
 * @error: instance of #GError to place the error
 *
 * This method call DDL comands to drop the view. 
 *
 * Returns: %TRUE if everything is ok and %FALSE otherwise.
 *
 * Stability: Stable
 * Since: 6.0
 */
static gboolean
gda_db_view_drop (GdaDdlModifiable *self,
                  GdaConnection *cnc,
                  gpointer user_data,
                  GError **error)
{
  gda_lockable_lock ((GdaLockable*)cnc);

  GdaServerProvider *provider = NULL;
  GdaServerOperation *op = NULL;
  gchar *action_str = NULL;
  GdaDbViewRefAction *action = (GdaDbViewRefAction*)user_data;

  provider = gda_connection_get_provider (cnc);

  op = gda_server_provider_create_operation (provider,
                                             cnc,
                                             GDA_SERVER_OPERATION_DROP_VIEW,
                                             NULL,
                                             error);
  if (!op)
    goto on_error;

  if (!gda_server_operation_set_value_at (op,
                                          gda_db_base_get_full_name (GDA_DB_BASE (self)),
                                          error,
                                          "/VIEW_DESC_P/VIEW_NAME"))
    goto on_error;

  if (!gda_server_operation_set_value_at (op, GDA_BOOL_TO_STR(TRUE), error,
                                          "/VIEW_DESC_P/VIEW_IFEXISTS"))
    goto on_error;

  switch (*action)
    {
    case GDA_DB_VIEW_RESTRICT:
      action_str = g_strdup ("RESTRICT");
      break;
    case GDA_DB_VIEW_CASCADE:
      action_str = g_strdup ("CASCADE");
      break;
    default:
      g_debug("Wrong value for action. It is %d\n", *action);
      break;
    }

  if (!gda_server_operation_set_value_at (op, action_str, error,
                                          "/VIEW_DESC_P/REFERENCED_ACTION"))
    goto on_error;

  if (!gda_server_provider_perform_operation (provider, cnc, op, error))
    goto on_error;

  g_object_unref (op);
  g_free (action_str);
  gda_lockable_unlock ((GdaLockable*)cnc);

  return TRUE;

on_error:
  if (op) g_object_unref (op);
  gda_lockable_unlock ((GdaLockable*)cnc);
  g_free (action_str);
  return FALSE;
}

static gboolean
gda_db_view_rename (GdaDdlModifiable *old_name,
                    GdaConnection *cnc,
                    gpointer new_name,
                    GError **error)
{
  g_set_error (error, GDA_DDL_MODIFIABLE_ERROR,
               GDA_DDL_MODIFIABLE_NOT_IMPLEMENTED,
               _("Operation is not implemented for the used provider"));
  return FALSE;
}
