/* gda-db-column.c
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
#define G_LOG_DOMAIN "GDA-db-column"

#include <stdlib.h>
#include "gda-db-column.h"
#include <glib/gi18n-lib.h>
#include "gda-util.h"
#include "gda-db-buildable.h"
#include "gda-server-provider.h"
#include "gda-db-column-private.h"
#include "gda-ddl-modifiable.h"
#include "gda-lockable.h"

G_DEFINE_QUARK (gda-db-column-error, gda_db_column_error)

typedef struct
{
  gchar *mp_name; /* property */
  gchar *mp_type;
  gchar *mp_comment;
  gchar *mp_default; /* property */
  gchar *mp_check;
  GdaDbTable *mp_table; /* Property */

  GType m_gtype;

  guint m_size; /* Property */
  guint m_scale;

  gboolean m_nnul; /* Property */
  gboolean m_autoinc; /* property */
  gboolean m_unique; /* property */
  gboolean m_pkey; /* property */
} GdaDbColumnPrivate;

/**
 * SECTION:gda-db-column
 * @short_description: Object to represent table column
 * @see_also: #GdaDbTable, #GdaDbView, #GdaDbBuildable
 * @stability: Stable
 * @include: libgda/libgda.h
 *
 * This object represents a column of a table or a view. The column can be constracted manually
 * using API or generated from xml file together with other databse objects. See #GdaDbCatalog.
 * #GdaDbColumn implements #GdaDbBuildable interface for parsing xml file. This is a typical example
 * how the #GdaDbColumn API can be used.
 *
 * |[<!-- language="C" -->
 * GdaDbTable *tproject = gda_db_table_new ();
 * gda_db_base_set_name (GDA_DB_BASE (tproject), "Project");
 *
 * GdaDbColumn *pid = gda_db_column_new ();
 * gda_db_column_set_name (pid, "id");
 * gda_db_column_set_type (pid, G_TYPE_INT);
 * gda_db_column_set_nnul (pid, TRUE);
 * gda_db_column_set_autoinc (pid, TRUE);
 * gda_db_column_set_unique (pid, TRUE);
 * gda_db_column_set_pkey (pid, TRUE);
 *
 * gda_db_table_append_column (tproject, pid);
 *
 * g_object_unref (pid);
 * ]|
 */

/* All nodes in xml should be accessed using enum below */
enum {
    GDA_DB_COLUMN_NODE,
    GDA_DB_COLUMN_NAME,
    GDA_DB_COLUMN_ATYPE,
    GDA_DB_COLUMN_PKEY,
    GDA_DB_COLUMN_UNIQUE,
    GDA_DB_COLUMN_AUTOINC,
    GDA_DB_COLUMN_NNUL,
    GDA_DB_COLUMN_COMMENT,
    GDA_DB_COLUMN_SIZE,
    GDA_DB_COLUMN_CHECK,
    GDA_DB_COLUMN_VALUE,
    GDA_DB_COLUMN_SCALE,
    GDA_DB_COLUMN_N_NODES
};

static const gchar *gdadbcolumnnode[GDA_DB_COLUMN_N_NODES] = {
  "column",
  "name",
  "type",
  "pkey",
  "unique",
  "autoinc",
  "nnul",
  "comment",
  "size",
  "check",
  "value",
  "scale"
};

static void
gda_db_column_buildable_interface_init (GdaDbBuildableInterface *iface);
static void gda_ddl_modifiable_interface_init (GdaDdlModifiableInterface *iface);

static gboolean gda_db_column_create (GdaDdlModifiable *self, GdaConnection *cnc,
				     gpointer user_data, GError **error);
static gboolean gda_db_column_drop (GdaDdlModifiable *self, GdaConnection *cnc,
				   gpointer user_data, GError **error);
static gboolean gda_db_column_rename (GdaDdlModifiable *old_name, GdaConnection *cnc,
				     gpointer new_name, GError **error);

G_DEFINE_TYPE_WITH_CODE (GdaDbColumn, gda_db_column, G_TYPE_OBJECT,
                         G_ADD_PRIVATE (GdaDbColumn)
                         G_IMPLEMENT_INTERFACE (GDA_TYPE_DB_BUILDABLE,
                                                gda_db_column_buildable_interface_init)
                         G_IMPLEMENT_INTERFACE (GDA_TYPE_DDL_MODIFIABLE,
                                                gda_ddl_modifiable_interface_init))

enum {
    PROP_0,
    PROP_COLUMN_NAME,
    PROP_COLUMN_COMMENT,
    PROP_COLUMN_SIZE,
    PROP_COLUMN_NNUL,
    PROP_COLUMN_AUTOINC,
    PROP_COLUMN_UNIQUE,
    PROP_COLUMN_PKEY,
    PROP_COLUMN_DEFAULT,
    PROP_COLUMN_CHECK,
    PROP_COLUMN_SCALE,
    PROP_COLUMN_TABLE,
    /*<private>*/
    N_PROPS
};

static void
_gda_db_column_set_type (GdaDbColumn *self, const gchar *type, GError **error);

static GParamSpec *properties [N_PROPS] = {NULL};

/**
 * gda_db_column_new:
 *
 * Returns: New instance of #GdaDbColumn, to free with g_object_unref () once not needed anymore
 *
 * Stability: Stable
 * Since: 6.0
 *
 */
GdaDbColumn *
gda_db_column_new (void)
{
  return g_object_new (GDA_TYPE_DB_COLUMN, NULL);
}

static void
gda_db_column_finalize (GObject *object)
{
  GdaDbColumn *self = GDA_DB_COLUMN (object);
  GdaDbColumnPrivate *priv = gda_db_column_get_instance_private (self);

  g_free (priv->mp_name);
  g_free (priv->mp_type);
  g_free (priv->mp_default);
  g_free (priv->mp_check);
  g_free (priv->mp_comment);

  G_OBJECT_CLASS (gda_db_column_parent_class)->finalize (object);
}

static void
gda_db_column_dispose (GObject *object)
{
  GdaDbColumn *self = GDA_DB_COLUMN (object);
  GdaDbColumnPrivate *priv = gda_db_column_get_instance_private (self);

  if (priv->mp_table) g_object_unref (priv->mp_table);

  G_OBJECT_CLASS (gda_db_column_parent_class)->dispose (object);
}

static void
gda_db_column_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  GdaDbColumn *self = GDA_DB_COLUMN (object);
  GdaDbColumnPrivate *priv = gda_db_column_get_instance_private (self);

  switch (prop_id) {
    case PROP_COLUMN_NAME:
      g_value_set_string (value, priv->mp_name);
      break;
    case PROP_COLUMN_SIZE:
      g_value_set_uint (value, priv->m_size);
      break;
    case PROP_COLUMN_NNUL:
      g_value_set_boolean (value, priv->m_nnul);
      break;
    case PROP_COLUMN_UNIQUE:
      g_value_set_boolean (value, priv->m_unique);
      break;
    case PROP_COLUMN_PKEY:
      g_value_set_boolean (value, priv->m_pkey);
      break;
    case PROP_COLUMN_AUTOINC:
      g_value_set_boolean (value, priv->m_autoinc);
      break;
    case PROP_COLUMN_DEFAULT:
      g_value_set_string (value, priv->mp_default);
      break;
    case PROP_COLUMN_CHECK:
      g_value_set_string (value, priv->mp_check);
      break;
    case PROP_COLUMN_COMMENT:
      g_value_set_string (value, priv->mp_comment);
      break;
    case PROP_COLUMN_SCALE:
      g_value_set_uint (value, priv->m_scale);
      break;
    case PROP_COLUMN_TABLE:
      g_value_set_object (value, priv->mp_table);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gda_db_column_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  GdaDbColumn *self = GDA_DB_COLUMN (object);
  GdaDbColumnPrivate *priv = gda_db_column_get_instance_private (self);

  switch (prop_id){
    case PROP_COLUMN_NAME:
      g_free (priv->mp_name);
      priv->mp_name = g_value_dup_string (value);
      break;
    case PROP_COLUMN_SIZE:
      priv->m_size = g_value_get_uint (value);
      break;
    case PROP_COLUMN_NNUL:
      priv->m_nnul = g_value_get_boolean (value);
      break;
    case PROP_COLUMN_UNIQUE:
      priv->m_unique = g_value_get_boolean (value);
      break;
    case PROP_COLUMN_AUTOINC:
      priv->m_autoinc = g_value_get_boolean (value);
      break;
    case PROP_COLUMN_PKEY:
      priv->m_pkey = g_value_get_boolean (value);
      break;
    case PROP_COLUMN_DEFAULT:
      g_free (priv->mp_default);
      priv->mp_default = g_value_dup_string (value);
      break;
    case PROP_COLUMN_CHECK:
      g_free (priv->mp_check);
      priv->mp_check = g_value_dup_string (value);
      break;
    case PROP_COLUMN_COMMENT:
      g_free (priv->mp_comment);
      priv->mp_comment = g_value_dup_string (value);
      break;
    case PROP_COLUMN_SCALE:
      priv->m_scale = g_value_get_uint (value);
      break;
    case PROP_COLUMN_TABLE:
      if (priv->mp_table)
        g_object_unref (priv->mp_table);
      priv->mp_table = g_value_dup_object (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gda_db_column_class_init (GdaDbColumnClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gda_db_column_finalize;
  object_class->dispose = gda_db_column_dispose;
  object_class->get_property = gda_db_column_get_property;
  object_class->set_property = gda_db_column_set_property;

  properties[PROP_COLUMN_NAME] =
    g_param_spec_string ("name", "Name", "Column name", NULL, G_PARAM_READWRITE);

  properties[PROP_COLUMN_SIZE] =
    g_param_spec_uint ("size", "Size", "Column size", 0, 9999, 80, G_PARAM_READWRITE);

  properties[PROP_COLUMN_NNUL] =
    g_param_spec_boolean ("nnul", "NotNULL", "Can value be NULL", TRUE, G_PARAM_READWRITE);

  properties[PROP_COLUMN_AUTOINC] =
    g_param_spec_boolean ("autoinc", "Autoinc", "Can value be autoincremented", FALSE,
                          G_PARAM_READWRITE);

  properties[PROP_COLUMN_UNIQUE] =
    g_param_spec_boolean ("unique", "Unique", "Can value be unique", FALSE, G_PARAM_READWRITE);

  properties[PROP_COLUMN_PKEY] =
    g_param_spec_boolean ("pkey", "Pkey", "Is is primery key", FALSE, G_PARAM_READWRITE);

  properties[PROP_COLUMN_DEFAULT] =
    g_param_spec_string ("default", "Default", "Default value", NULL,G_PARAM_READWRITE);

  properties[PROP_COLUMN_CHECK] =
    g_param_spec_string ("check", "Check", "Column check string", NULL, G_PARAM_READWRITE);

  properties[PROP_COLUMN_COMMENT] =
    g_param_spec_string ("comment", "Comment", "Column comment", NULL, G_PARAM_READWRITE);

  properties[PROP_COLUMN_SCALE] =
    g_param_spec_uint ("scale", "Scale", "Number of decimal for numeric type", 0, 64, 2,
                         G_PARAM_READWRITE);

  properties[PROP_COLUMN_TABLE] =
    g_param_spec_object ("table", "Table", "Parent table", GDA_TYPE_DB_TABLE, G_PARAM_READWRITE);

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gda_db_column_init (GdaDbColumn *self)
{
  GdaDbColumnPrivate *priv = gda_db_column_get_instance_private (self);

  priv->mp_type = NULL;
  priv->m_scale = 0;
  priv->mp_check = NULL;
  priv->mp_name = NULL;
  priv->m_gtype = G_TYPE_INVALID;
  priv->m_nnul = FALSE;
  priv->m_autoinc = FALSE;
  priv->m_unique = FALSE;
  priv->m_pkey = FALSE;
  priv->mp_default = NULL;
  priv->mp_comment = NULL;
  priv->mp_table = NULL;
}

/**
 * gda_db_column_parse_node: (skip)
 * @self: #GdaDbColumn object to store parsed data
 * @node: instance of #xmlNodePtr to parse
 * @error: error container
 *
 * Parse @node wich should point to <column> node
 *
 * Returns: %TRUE if no error, %FALSE otherwise
 *
 * Since: 6.0
 */
static gboolean
gda_db_column_parse_node (GdaDbBuildable *buildable,
                          xmlNodePtr   node,
                          GError       **error)
{
  g_return_val_if_fail (buildable,FALSE);
  g_return_val_if_fail (node,FALSE);

  /*TODO: Appropriate error should be set an returned.
   * It should be added to the header file first
   */
  if (g_strcmp0 ((gchar *)node->name, gdadbcolumnnode[GDA_DB_COLUMN_NODE]))
    return FALSE;

  GdaDbColumn *self = GDA_DB_COLUMN (buildable);
  GdaDbColumnPrivate *priv = gda_db_column_get_instance_private (self);

  xmlChar *name = xmlGetProp (node, (xmlChar *)gdadbcolumnnode[GDA_DB_COLUMN_NAME]);
  g_assert (name); /* If here bug with xml validation */
  g_object_set (G_OBJECT(self),"name",(gchar*)name,NULL);
  xmlFree (name);

  xmlChar *type = xmlGetProp (node,(xmlChar *)gdadbcolumnnode[GDA_DB_COLUMN_ATYPE]);
  g_assert (type); /* If here buf with validation */
  _gda_db_column_set_type (self, (const gchar *)type,error);
  xmlFree (type);

  xmlChar *cprop = NULL;
  cprop = xmlGetProp (node,(xmlChar *)gdadbcolumnnode[GDA_DB_COLUMN_PKEY]);
  if (cprop)
    {
      g_object_set (G_OBJECT(self),
                    "pkey", *cprop == 't' || *cprop == 'T' ? TRUE : FALSE,
                    NULL);
      xmlFree (cprop);
    }

  /* We need scale only for NUMBERIC data type, e.g. float, double  */
  cprop = xmlGetProp (node, (xmlChar *)gdadbcolumnnode[GDA_DB_COLUMN_UNIQUE]);

  if (cprop)
    {
      g_object_set (G_OBJECT(self),
                    "unique", *cprop == 't' || *cprop == 'T' ? TRUE : FALSE,
                    NULL);
      xmlFree (cprop);
    }

  cprop = xmlGetProp (node, (xmlChar *)gdadbcolumnnode[GDA_DB_COLUMN_AUTOINC]);

  if (cprop)
    {
      g_object_set (G_OBJECT(self),
                    "autoinc", *cprop == 't' || *cprop == 'T' ? TRUE : FALSE,
                    NULL);
      xmlFree (cprop);
    }

  cprop = xmlGetProp (node, (xmlChar *)gdadbcolumnnode[GDA_DB_COLUMN_NNUL]);

  if (cprop)
    {
      g_object_set (G_OBJECT(self),
                    "nnul",*cprop == 't' || *cprop == 'T' ? TRUE : FALSE,
                    NULL);
      xmlFree (cprop);
    }

  for (xmlNodePtr it = node->children; it; it = it->next)
    {
      if (!g_strcmp0 ((char *)it->name,gdadbcolumnnode[GDA_DB_COLUMN_VALUE]))
        {
          xmlChar *value = xmlNodeGetContent (it);

          if (value)
            {
              g_object_set (G_OBJECT (self), "default", (gchar *)value, NULL);
              xmlFree (value);
            }

          xmlChar *nprop = xmlGetProp (it, BAD_CAST gdadbcolumnnode[GDA_DB_COLUMN_SIZE]);
          guint64 tint = 0;
          if (nprop)
            {
              tint = g_ascii_strtoull ((gchar*)nprop, NULL, 10);
              xmlFree (nprop);
              g_object_set (G_OBJECT (self), "size", tint, NULL);
            }

          if (priv->m_gtype == G_TYPE_FLOAT ||
              priv->m_gtype == G_TYPE_DOUBLE ||
              priv->m_gtype == GDA_TYPE_NUMERIC)
            {
              nprop = xmlGetProp (it, (xmlChar *)gdadbcolumnnode[GDA_DB_COLUMN_SCALE]);
              if (nprop)
                {
                  guint64 tint = g_ascii_strtoull ((gchar*)nprop, NULL, 10);
                  g_object_set (G_OBJECT(self), "scale", tint, NULL);
                  xmlFree (nprop);
                }
            }
        }

      if (!g_strcmp0 ((char *)it->name, gdadbcolumnnode[GDA_DB_COLUMN_CHECK]))
        {
          xmlChar *check = xmlNodeGetContent (it);
          g_object_set (G_OBJECT (self), "check", check,NULL);
          xmlFree (check);
        }

      if (!g_strcmp0 ((char *)it->name, gdadbcolumnnode[GDA_DB_COLUMN_COMMENT]))
        {
          xmlChar *comment = xmlNodeGetContent (it);
          g_object_set (G_OBJECT (self), "comment", comment,NULL);
          xmlFree (comment);
        }
    } /* End of for loop */
  return TRUE;
} /* End of gda_db_column_parse_node */

static gboolean
gda_db_column_write_node (GdaDbBuildable *buildable,
                          xmlNodePtr rootnode,
                          GError       **error)
{
  g_return_val_if_fail (buildable, FALSE);
  g_return_val_if_fail (rootnode, FALSE);

  GdaDbColumn *self = GDA_DB_COLUMN (buildable);

  GdaDbColumnPrivate *priv = gda_db_column_get_instance_private (self);

  xmlNodePtr node = NULL;
  node  = xmlNewChild (rootnode,
                       NULL,
                       BAD_CAST gdadbcolumnnode[GDA_DB_COLUMN_NODE],
                       NULL);

  xmlNewProp (node,
              BAD_CAST gdadbcolumnnode[GDA_DB_COLUMN_NAME],
              BAD_CAST gda_db_column_get_name (self));

  xmlNewProp (node,
              BAD_CAST gdadbcolumnnode[GDA_DB_COLUMN_ATYPE],
              BAD_CAST priv->mp_type);

  xmlNewProp (node,
              BAD_CAST gdadbcolumnnode[GDA_DB_COLUMN_PKEY],
              BAD_CAST GDA_BOOL_TO_STR(priv->m_pkey));

  xmlNewProp (node,
              BAD_CAST gdadbcolumnnode[GDA_DB_COLUMN_UNIQUE],
              BAD_CAST GDA_BOOL_TO_STR(priv->m_unique));

  xmlNewProp (node,
              BAD_CAST gdadbcolumnnode[GDA_DB_COLUMN_AUTOINC],
              BAD_CAST GDA_BOOL_TO_STR(priv->m_autoinc));

  xmlNewProp (node,
              BAD_CAST gdadbcolumnnode[GDA_DB_COLUMN_NNUL],
              BAD_CAST GDA_BOOL_TO_STR(priv->m_nnul));

  if (priv->mp_comment)
    xmlNewChild (node, NULL,
                 BAD_CAST gdadbcolumnnode[GDA_DB_COLUMN_COMMENT],
                 BAD_CAST priv->mp_comment);

  xmlNodePtr nodevalue = NULL;

  if (priv->mp_default)
    {
      nodevalue = xmlNewChild (node,
                               NULL,
                               BAD_CAST gdadbcolumnnode[GDA_DB_COLUMN_VALUE],
                               BAD_CAST priv->mp_default);

      if (priv->m_size > 0)
        {
          /* temp string to convert gint to gchar* */
          GString *sizestr = g_string_new (NULL);
          g_string_printf (sizestr, "%d", priv->m_size);

          xmlNewProp (nodevalue,
                      BAD_CAST gdadbcolumnnode[GDA_DB_COLUMN_SIZE],
                      BAD_CAST sizestr->str);

          g_string_free (sizestr,TRUE);

          /* We need to write down scale if column type is numeric */
          if (priv->m_gtype == G_TYPE_FLOAT ||
              priv->m_gtype == G_TYPE_DOUBLE ||
              priv->m_gtype == GDA_TYPE_NUMERIC)
            {
              GString *scale_str = g_string_new (NULL);
              g_string_printf (scale_str, "%d", priv->m_scale);

              xmlNewProp (nodevalue,
                          BAD_CAST gdadbcolumnnode[GDA_DB_COLUMN_SCALE],
                          BAD_CAST scale_str->str);

              g_string_free (scale_str, TRUE);
            }
        }
    }

  if (priv->mp_check)
    xmlNewChild (node,NULL,
                 BAD_CAST gdadbcolumnnode[GDA_DB_COLUMN_CHECK],
                 BAD_CAST priv->mp_check);

  return TRUE;
}

static void
gda_db_column_buildable_interface_init (GdaDbBuildableInterface *iface)
{
    iface->parse_node = gda_db_column_parse_node;
    iface->write_node = gda_db_column_write_node;
}

static void
gda_ddl_modifiable_interface_init (GdaDdlModifiableInterface *iface)
{
  iface->create = gda_db_column_create;
  iface->drop   = gda_db_column_drop;
  iface->rename = gda_db_column_rename;
}

static void
_gda_db_column_set_type (GdaDbColumn *self,
                         const char *type,
                         GError **error)
{
  g_return_if_fail (self);
  g_return_if_fail (type);
  //	g_return_if_fail (!*error);
  GdaDbColumnPrivate *priv = gda_db_column_get_instance_private (self);
  GType ttype = gda_g_type_from_string (type);
  if (ttype != G_TYPE_INVALID){
      priv->mp_type = g_strdup (type);
      priv->m_gtype = ttype;
  }
  else {
      g_set_error (error,
                   GDA_DB_COLUMN_ERROR,
                   GDA_DB_COLUMN_ERROR_TYPE,
                   _("Invalid column type %s. See API for gda_g_type_from_string ().\n"),
                   type);
  }
}

/**
 * gda_db_column_get_name:
 * @self: a #GdaDbColumn instance
 *
 * Returns name of the column
 *
 * Returns: Column name as a string or %NULL.
 *
 * Stability: Stable
 * Since: 6.0
 */
const gchar*
gda_db_column_get_name (GdaDbColumn *self)
{
  g_return_val_if_fail (self, NULL);
  GdaDbColumnPrivate *priv = gda_db_column_get_instance_private (self);

  return priv->mp_name;
}

/**
 * gda_db_column_set_name:
 * @self: a #GdaDbColumn object
 * @name: name as a string
 *
 * Set column name.
 *
 * Stability: Stable
 * Since: 6.0
 */
void
gda_db_column_set_name (GdaDbColumn *self,
                        const gchar *name)
{
  g_return_if_fail (self);
  GdaDbColumnPrivate *priv = gda_db_column_get_instance_private (self);

  g_free (priv->mp_name);
  priv->mp_name = g_strdup (name);
}

/**
 * gda_db_column_get_gtype:
 * @self: a #GdaDbColumn object
 *
 * Return of column type as #GType
 *
 * Stability: Stable
 * Since: 6.0
 */
GType
gda_db_column_get_gtype (GdaDbColumn *self)
{
  g_return_val_if_fail (self, GDA_TYPE_NULL);
  GdaDbColumnPrivate *priv = gda_db_column_get_instance_private (self);

  return priv->m_gtype;
}

/**
 * gda_db_column_get_ctype:
 * @self: a #GdaDbColumn object
 *
 * Returns column type as a string derivied from #GType
 *
 * Returns: column type as a string or %NULL
 *
 * Stability: Stable
 * Since: 6.0
 */
const gchar*
gda_db_column_get_ctype (GdaDbColumn *self)
{
  g_return_val_if_fail (self, NULL);
  GdaDbColumnPrivate *priv = gda_db_column_get_instance_private (self);
  return priv->mp_type;
}

/**
 * gda_db_column_set_type:
 * @self: a #GdaDbColumn instance
 * @type: #GType for column
 *
 * Set type of the column as a #GType. For numeric type, #GDA_TYPE_NUMERIC should be used. Other
 * types, e.g. %G_TYPE_FLOAT or %G_TYPE_DOUBLE can also be used but precision and scale should not be
 * set. In this case appropriate types for DB implementation will be used, e.g. float4.
 *
 * Stability: Stable
 * Since: 6.0
 */
void
gda_db_column_set_type (GdaDbColumn *self,
                        GType type)
{
  g_return_if_fail (self);
  GdaDbColumnPrivate *priv = gda_db_column_get_instance_private (self);
  priv->m_gtype = type;
  g_free (priv->mp_type);
  priv->mp_type = g_strdup (gda_g_type_to_string (type));
}

/**
 * gda_db_column_get_scale:
 * @self: a #GdaDbColumn object
 *
 * Scale is used for float number representation to specify a number of decimal digits.
 * This value is ignore for column types except float or double.
 *
 * Returns: Current scale value
 *
 * Stability: Stable
 * Since: 6.0
 */
guint
gda_db_column_get_scale (GdaDbColumn *self)
{
  g_return_val_if_fail (self,0);
  GdaDbColumnPrivate *priv = gda_db_column_get_instance_private (self);
  return priv->m_scale;
}

/**
 * gda_db_column_set_scale:
 * @self: a #GdaDbColumn
 * @scale: scale value to set
 *
 * Scale is used for float number representation to specify a number of decimal digits.
 * This value is ignore for column types except float or double.
 *
 * Stability: Stable
 * Since: 6.0
 */
void
gda_db_column_set_scale (GdaDbColumn *self,
                         guint scale)
{
  g_return_if_fail (self);
  GdaDbColumnPrivate *priv = gda_db_column_get_instance_private (self);
  priv->m_scale = scale;
}

/**
 * gda_db_column_get_pkey:
 * @self: a #GdaDbColumn object
 *
 * Returns a primary key flag
 *
 * Returns: %TRUE if the column is primary key, %FALSE otherwise
 *
 * Stability: Stable
 * Since: 6.0
 */
gboolean
gda_db_column_get_pkey (GdaDbColumn *self)
{
  g_return_val_if_fail (self, FALSE);
  GdaDbColumnPrivate *priv = gda_db_column_get_instance_private (self);
  return priv->m_pkey;
}

/**
 * gda_db_column_set_pkey:
 * @self: a #GdaDbColumn object
 * @pkey: value to set
 *
 * If @pkey is %TRUE, the given column will be marked with PRIMERY KEY flag
 *
 * Stability: Stable
 * Since: 6.0
 */
void
gda_db_column_set_pkey (GdaDbColumn *self,
                        gboolean pkey)
{
  g_return_if_fail (self);
  GdaDbColumnPrivate *priv = gda_db_column_get_instance_private (self);

  priv->m_pkey = pkey;
}
/**
 * gda_db_column_get_nnul:
 * @self: a @GdaDbColumn object
 *
 * Specify if the column's value can be NULL.
 *
 * Returns: %TRUE if value can be %NULL, %FALSE otherwise.
 *
 * Stability: Stable
 * Since: 6.0
 */
gboolean
gda_db_column_get_nnul (GdaDbColumn *self)
{
  g_return_val_if_fail (self, FALSE);
  GdaDbColumnPrivate *priv = gda_db_column_get_instance_private (self);
  return priv->m_nnul;
}

/**
 * gda_db_column_set_nnul:
 * @self: a GdaDbColumn object
 * @nnul: value to set for nnul
 * If @nnul is %TRUE the column will be marked with NON NULL flag
 * Stability: Stable
 * Since: 6.0
 */
void
gda_db_column_set_nnul (GdaDbColumn *self,
                        gboolean nnul)
{
  g_return_if_fail (self);
  GdaDbColumnPrivate *priv = gda_db_column_get_instance_private (self);
  priv->m_nnul = nnul;
}

/**
 * gda_db_column_get_autoinc:
 * @self: a #GdaDbColumn object
 *
 * Get value for autoinc key
 *
 * Returns: %TRUE if column should be auto-incremented, %FALSE otherwise.
 *
 * Stability: Stable
 * Since: 6.0
 */
gboolean
gda_db_column_get_autoinc (GdaDbColumn *self)
{
  g_return_val_if_fail (self, FALSE);
  GdaDbColumnPrivate *priv = gda_db_column_get_instance_private (self);
  return priv->m_autoinc;
}

/**
 * gda_db_column_set_autoinc:
 * @self: a #GdaDbColumn object
 * @autoinc: value to set
 *
 * Set value for auto-incremented key.
 *
 * Stability: Stable
 * Since: 6.0
 */
void
gda_db_column_set_autoinc (GdaDbColumn *self,
                           gboolean autoinc)
{
  g_return_if_fail (self);
  GdaDbColumnPrivate *priv = gda_db_column_get_instance_private (self);
  priv->m_autoinc = autoinc;
}

/**
 * gda_db_column_get_unique:
 * @self: a #GdaDbColumn object
 *
 * Get value for unique key
 *
 * Returns: %TRUE if column should have a unique value, %FALSE otherwise.
 *
 * Stability: Stable
 * Since: 6.0
 */
gboolean
gda_db_column_get_unique (GdaDbColumn *self)
{
  g_return_val_if_fail (self, FALSE);
  GdaDbColumnPrivate *priv = gda_db_column_get_instance_private (self);
  return priv->m_unique;
}

/**
 * gda_db_column_set_unique:
 * @self: a #GdaDbColumn object
 * @unique: value to set
 *
 * Set value for unique key.
 *
 * Stability: Stable
 * Since: 6.0
 */
void
gda_db_column_set_unique (GdaDbColumn *self,
                          gboolean unique)
{
  g_return_if_fail (self);
  GdaDbColumnPrivate *priv = gda_db_column_get_instance_private (self);
  priv->m_unique = unique;
}

/**
 * gda_db_column_get_comment:
 * @self: a #GdaDbColumn object
 *
 * Get value for column comment.
 *
 * Returns: Column comment as a string.
 * %NULL is returned if comment is not set.
 *
 * Stability: Stable
 * Since: 6.0
 */
const gchar*
gda_db_column_get_comment (GdaDbColumn *self)
{
  g_return_val_if_fail (self, NULL);
  GdaDbColumnPrivate *priv = gda_db_column_get_instance_private (self);

  return priv->mp_comment;
}

/**
 * gda_db_column_set_comment:
 * @self: a #GdaDbColumn object
 * @comnt: comment to set
 *
 * Set value for column comment.
 *
 * Stability: Stable
 * Since: 6.0
 */
void
gda_db_column_set_comment (GdaDbColumn *self,
                           const gchar *comnt)
{
  g_return_if_fail (self);
  GdaDbColumnPrivate *priv = gda_db_column_get_instance_private (self);
  g_free (priv->mp_comment);
  priv->mp_comment = g_strdup (comnt);
}

/**
 * gda_db_column_get_size:
 * @self: a #GdaDbColumn instance
 *
 * Returns: Current value for column size.
 *
 * Stability: Stable
 * Since: 6.0
 */
guint
gda_db_column_get_size (GdaDbColumn *self)
{
  g_return_val_if_fail (self, 0);
  GdaDbColumnPrivate *priv = gda_db_column_get_instance_private (self);
  return priv->m_size;
}

/**
 * gda_db_column_set_size:
 * @self: a #GdaDbColumn instance
 * @size: value to set
 *
 * Set value for column size. This is relevant only for string column type.
 *
 * Stability: Stable
 * Since: 6.0
 */
void
gda_db_column_set_size (GdaDbColumn *self,
                        guint size)
{
  g_return_if_fail (self);
  GdaDbColumnPrivate *priv = gda_db_column_get_instance_private (self);
  priv->m_size = size;
}

/**
 * gda_db_column_get_default:
 * @self: a #GdaDbColumn instance
 *
 * Returns default value for the column. Can be %NULL if the default value hasn't been set.
 *
 * Returns: Default value for the column as a string.
 *
 * Stability: Stable
 * Since: 6.0
 */
const gchar*
gda_db_column_get_default (GdaDbColumn *self)
{
  g_return_val_if_fail (self, NULL);
  GdaDbColumnPrivate *priv = gda_db_column_get_instance_private (self);
  return priv->mp_default;
}

/**
 * gda_db_column_set_default:
 * @self: a #GdaDbColumn instance
 * @value: default value to set for column as a string
 *
 * Stability: Stable
 * Since: 6.0
 */
void
gda_db_column_set_default (GdaDbColumn *self,
                           const gchar *value)
{
  g_return_if_fail (self);
  GdaDbColumnPrivate *priv = gda_db_column_get_instance_private (self);
  g_free(priv->mp_default);
  priv->mp_default = g_strdup (value);
}

/**
 * gda_db_column_get_check:
 * @self: a #GdaDbColumn instance
 *
 * Returns value of the check field.
 *
 * Returns: Column check string
 *
 * Stability: Stable
 * Since: 6.0
 */
const gchar*
gda_db_column_get_check (GdaDbColumn *self)
{
  g_return_val_if_fail (self,NULL);
  GdaDbColumnPrivate *priv = gda_db_column_get_instance_private (self);
  return priv->mp_check;
}

/**
 * gda_db_column_set_check:
 * @self: a #GdaDbColumn instance
 * @value: value to set
 *
 * Sets check string to the column.
 *
 * Stability: Stable
 * Since: 6.0
 */
void
gda_db_column_set_check (GdaDbColumn *self,
                         const gchar *value)
{
  g_return_if_fail (self);
  GdaDbColumnPrivate *priv = gda_db_column_get_instance_private (self);
  g_free(priv->mp_check);
  priv->mp_check = g_strdup (value);
}

/**
 * gda_db_column_prepare_create:
 * @self: a #GdaDbColumn instance
 * @op: a #GdaServerOperation instance to update for TABLE_CREATE operation
 * @order: Order number for the column
 * @error: a #GError container
 *
 * This method populate @op with information stored in @self.
 *
 * Returns: %TRUE if successful, %FALSE otherwise.
 *
 * Stability: Stable
 * Since: 6.0
 */
gboolean
gda_db_column_prepare_create  (GdaDbColumn *self,
                               GdaServerOperation *op,
                               guint order,
                               GError **error)
{
  GdaConnection *cnc;
  const gchar *strtype;

  g_return_val_if_fail(self,FALSE);
  g_return_val_if_fail(op,FALSE);

  GdaDbColumnPrivate *priv = gda_db_column_get_instance_private (self);

  if(!gda_server_operation_set_value_at (op, priv->mp_name, error, "/FIELDS_A/@COLUMN_NAME/%d", order))
    return FALSE;

  cnc = (GdaConnection*) g_object_get_data (G_OBJECT (op), "connection");

  if (cnc == NULL)
    {
      g_set_error (error, GDA_DB_COLUMN_ERROR, GDA_DB_COLUMN_ERROR_TYPE,
                   _("Internal error: Operation should be prepared, setting a connection data"));
      return FALSE;
    }

  strtype = gda_server_provider_get_default_dbms_type (gda_connection_get_provider (cnc),
                                                       cnc, priv->m_gtype);

  if(!gda_server_operation_set_value_at (op, strtype, error, "/FIELDS_A/@COLUMN_TYPE/%d", order))
    return FALSE;

  gchar *numstr = NULL;
  numstr = g_strdup_printf ("%d", priv->m_size);

  if (g_type_is_a (priv->m_gtype, G_TYPE_STRING))
    {
      if(!gda_server_operation_set_value_at (op, numstr, error, "/FIELDS_A/@COLUMN_SIZE/%d", order))
        {
          g_free (numstr);
          return FALSE;
        }
    }

  g_free (numstr);

/* We need to set scale only for numeric column type */
  if (priv->m_gtype == G_TYPE_FLOAT ||
      priv->m_gtype == G_TYPE_DOUBLE ||
      priv->m_gtype == GDA_TYPE_NUMERIC)
    {
      numstr = g_strdup_printf ("%d", priv->m_scale);

      if(!gda_server_operation_set_value_at (op, numstr, error, "/FIELDS_A/@COLUMN_SCALE/%d", order))
        {
          g_free (numstr);
          return FALSE;
        }

      g_free (numstr);
    }

  if(!gda_server_operation_set_value_at (op, GDA_BOOL_TO_STR (priv->m_nnul), error,
                                        "/FIELDS_A/@COLUMN_NNUL/%d",order))
    return FALSE;

  if(!gda_server_operation_set_value_at (op, GDA_BOOL_TO_STR (priv->m_autoinc), error,
                                        "/FIELDS_A/@COLUMN_AUTOINC/%d",order))
    return FALSE;

  if(!gda_server_operation_set_value_at (op, GDA_BOOL_TO_STR (priv->m_unique), error,
                                        "/FIELDS_A/@COLUMN_UNIQUE/%d",order))
    return FALSE;

  if(!gda_server_operation_set_value_at (op, GDA_BOOL_TO_STR (priv->m_pkey), error,
                                        "/FIELDS_A/@COLUMN_PKEY/%d",order))
    return FALSE;

  if(!gda_server_operation_set_value_at (op, priv->mp_default, error,
                                        "/FIELDS_A/@COLUMN_DEFAULT/%d", order))
    return FALSE;

  if(!gda_server_operation_set_value_at (op, priv->mp_check,error,
                                        "/FIELDS_A/@COLUMN_CHECK/%d", order))
    return FALSE;

  return TRUE;
}

/**
 * gda_db_column_prepare_add:
 * @self: a #GdaDbColumn instance
 * @op: #GdaServerOperation to add information
 * @error: error storage container
 *
 * Populate @op with information stored in @self. This method is used to
 * prepare @op for %GDA_SERVER_OPERATION_ADD_COLUMN operation.
 *
 * Returns: %TRUE if success, %FALSE otherwise.
 *
 * Stability: Stable
 * Since: 6.0
 */
gboolean
gda_db_column_prepare_add (GdaDbColumn *self,
                           GdaServerOperation *op,
                           GError **error)
{
  g_return_val_if_fail(GDA_IS_DB_COLUMN(self), FALSE);
  g_return_val_if_fail(GDA_IS_SERVER_OPERATION(op), FALSE);
  g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

  GdaConnection *cnc;
  const gchar *strtype;

  GdaServerOperationType sotype = gda_server_operation_get_op_type(op);

  if (sotype != GDA_SERVER_OPERATION_ADD_COLUMN)
    {
      g_set_error(error, GDA_DB_COLUMN_ERROR, GDA_DB_COLUMN_ERROR_WRONG_OPERATION,
                  "Wrong ServerOperation type");
      return FALSE;
    }

  cnc = (GdaConnection*) g_object_get_data (G_OBJECT (op), "connection");

  if (!cnc)
    {
      g_set_error (error, GDA_DB_COLUMN_ERROR, GDA_DB_COLUMN_ERROR_TYPE,
                   _("Internal error: Operation should be prepared, setting a connection data"));
      return FALSE;
    }

  GdaDbColumnPrivate *priv = gda_db_column_get_instance_private (self);

  if (!gda_server_operation_set_value_at (op, priv->mp_name, error,
                                          "/COLUMN_DEF_P/COLUMN_NAME"))
    return FALSE;

  strtype = gda_server_provider_get_default_dbms_type (gda_connection_get_provider (cnc),
                                                       cnc, priv->m_gtype);

  if (!gda_server_operation_set_value_at (op, strtype, error,
                                          "/COLUMN_DEF_P/COLUMN_TYPE"))
    return FALSE;

  gchar *sizestr = NULL;
  sizestr = g_strdup_printf ("%d", priv->m_size);

  if (!gda_server_operation_set_value_at (op, sizestr, error,
                                          "/COLUMN_DEF_P/COLUMN_SIZE"))
    {
      g_free (sizestr);
      return FALSE;
    }
  else
    g_free (sizestr);

  if (!gda_server_operation_set_value_at (op, GDA_BOOL_TO_STR (priv->m_nnul), error,
                                          "/COLUMN_DEF_P/COLUMN_NNUL"))
    return FALSE;

  if (!gda_server_operation_set_value_at (op, GDA_BOOL_TO_STR (priv->m_autoinc), error,
                                          "/COLUMN_DEF_P/COLUMN_AUTOINC"))
    return FALSE;

  sizestr = g_strdup_printf("%d",priv->m_scale);

  if (!gda_server_operation_set_value_at (op, sizestr, error,
                                          "/COLUMN_DEF_P/COLUMN_SCALE"))
    {
      g_free (sizestr);
      return FALSE;
    }
  else
    g_free (sizestr);

  if (!gda_server_operation_set_value_at (op, priv->mp_default, error,
                                          "/COLUMN_DEF_P/COLUMN_DEFAULT"))
    return FALSE;

  if (!gda_server_operation_set_value_at (op, priv->mp_check, error,
                                          "/COLUMN_DEF_P/COLUMN_CHECK"))
    return FALSE;

  return TRUE;
}

/**
 * gda_db_column_new_from_meta:
 * @column: a #GdaMetaTableColumn instance
 *
 * Create new #GdaDbColumn instance from the corresponding #GdaMetaTableColumn
 * object. If %NULL is passed this function works exactly as
 * gda_db_column_new().
 *
 * Returns: New object that should be freed with g_object_unref()
 */
GdaDbColumn*
gda_db_column_new_from_meta (GdaMetaTableColumn *column)
{
  if (!column)
    return gda_db_column_new ();

  GdaDbColumn *gdacolumn = gda_db_column_new ();

  g_object_set (gdacolumn,
                "name", column->column_name,
                "pkey", column->pkey,
                "nnul", column->nullok,
                "default", column->default_value,
                NULL);

  gda_db_column_set_type (gdacolumn, column->gtype);
  return gdacolumn;
}

static gboolean
gda_db_column_create (GdaDdlModifiable *self,
                      GdaConnection *cnc,
                      gpointer user_data,
                      GError **error)
{

  G_DEBUG_HERE();
  GdaServerOperation *op = NULL;
  GdaServerProvider *provider = NULL;
  gchar *buffer_str = NULL;
  GdaDbTable *table = NULL;
  GdaDbColumn *column = GDA_DB_COLUMN (self);

  const gchar *strtype = NULL;

  gda_lockable_lock (GDA_LOCKABLE (cnc));

  provider = gda_connection_get_provider (cnc);

  op = gda_server_provider_create_operation (provider, cnc, GDA_SERVER_OPERATION_ADD_COLUMN,
                                             NULL, error);

  if (!op)
    goto on_error;

  g_object_get (column, "table", &table, NULL);

  if (!table)
    goto on_error;

  if (!gda_server_operation_set_value_at (op, gda_db_base_get_full_name (GDA_DB_BASE (table)),
                                          error, "/COLUMN_DEF_P/TABLE_NAME"))
    goto on_error;

  g_object_unref (table);

  if (!gda_server_operation_set_value_at (op, gda_db_column_get_name (column),
                                          error, "/COLUMN_DEF_P/COLUMN_NAME"))
    goto on_error;

  strtype = gda_server_provider_get_default_dbms_type (gda_connection_get_provider (cnc),
                                                       cnc, gda_db_column_get_gtype(column));

  if (!gda_server_operation_set_value_at (op, strtype,
                                          error, "/COLUMN_DEF_P/COLUMN_TYPE"))
    goto on_error;

  guint size = gda_db_column_get_size (column);

  buffer_str = g_strdup_printf("%d", size);

  if (!gda_server_operation_set_value_at (op, buffer_str,
                                          error, "/COLUMN_DEF_P/COLUMN_SIZE"))
    goto on_error;

  g_free (buffer_str);
  buffer_str = NULL;

  guint scale = gda_db_column_get_scale (column);

  buffer_str = g_strdup_printf("%d", scale);

  if (!gda_server_operation_set_value_at (op, buffer_str,
                                          error, "/COLUMN_DEF_P/COLUMN_SCALE"))
    goto on_error;

  g_free (buffer_str);
  buffer_str = NULL;

  if (!gda_server_operation_set_value_at (op, GDA_BOOL_TO_STR (gda_db_column_get_nnul (column)),
                                          error, "/COLUMN_DEF_P/COLUMN_NNUL"))
    goto on_error;

  if (!gda_server_provider_perform_operation (provider, cnc, op, error))
    goto on_error;

  g_object_unref (op);

  gda_lockable_unlock (GDA_LOCKABLE (cnc));

  return TRUE;

on_error:
  if (op)
    g_object_unref (op);

  if (buffer_str)
    g_free (buffer_str);

  if (table)
    g_object_unref (table);

  gda_lockable_unlock (GDA_LOCKABLE (cnc));

  return FALSE;
}

static gboolean
gda_db_column_drop (GdaDdlModifiable *self,
                    GdaConnection *cnc,
                    gpointer user_data,
                    GError **error)
{
  g_set_error (error, GDA_DDL_MODIFIABLE_ERROR,
               GDA_DDL_MODIFIABLE_NOT_IMPLEMENTED,
               _("Operation is not implemented for the used provider"));

  return FALSE;
}

static gboolean
gda_db_column_rename (GdaDdlModifiable *self,
                      GdaConnection *cnc,
                      gpointer user_data,
                      GError **error)
{
  G_DEBUG_HERE();
  GdaServerOperation *op = NULL;
  GdaServerProvider *provider = NULL;
  GdaDbTable *table = NULL;
  GdaDbColumn *column = GDA_DB_COLUMN (self);
  GdaDbColumn *column_new = GDA_DB_COLUMN (user_data);

  gda_lockable_lock (GDA_LOCKABLE (cnc));

  provider = gda_connection_get_provider (cnc);

  op = gda_server_provider_create_operation (provider, cnc, GDA_SERVER_OPERATION_RENAME_COLUMN,
                                             NULL, error);

  if (!op)
    goto on_error;

  g_object_get (column, "table", &table, NULL);

  if (!table)
    goto on_error;

  if (!gda_server_operation_set_value_at (op, gda_db_base_get_name (GDA_DB_BASE (table)),
                                          error, "/COLUMN_DEF_P/TABLE_NAME"))
    goto on_error;

  g_object_unref (table);

  if (!gda_server_operation_set_value_at (op, gda_db_column_get_name (column),
                                          error, "/COLUMN_DEF_P/COLUMN_NAME"))
    goto on_error;

  if (!gda_server_operation_set_value_at (op, gda_db_column_get_name (column_new),
                                          error, "/COLUMN_DEF_P/COLUMN_NAME_NEW"))
    goto on_error;

  if (!gda_server_provider_perform_operation (provider, cnc, op, error))
    goto on_error;

  g_object_unref (op);

  gda_lockable_unlock (GDA_LOCKABLE (cnc));

  return TRUE;

on_error:
  if (op)
    g_object_unref (op);

  if (table)
    g_object_unref (table);

  gda_lockable_unlock (GDA_LOCKABLE (cnc));

  return FALSE;
}
