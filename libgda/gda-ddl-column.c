/* gda-ddl-column.c
 *
 * Copyright © 2018 Pavlo Solntsev <pavlo.solntsev@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <stdlib.h>
#include "gda-ddl-column.h"
#include <glib/gi18n-lib.h>
#include "gda-util.h"

G_DEFINE_QUARK (gda-ddl-column-error, gda_ddl_column_error)

typedef struct
{
  gchar *mp_name; /* property */
  gchar *mp_type;
  gchar *mp_comment;
  gchar *mp_default; /* property */
  gchar *mp_check;

  GType m_gtype;

  guint m_size; /* Property */
  guint m_scale;

  gboolean m_nnul; /* Property */
  gboolean m_autoinc; /* property */
  gboolean m_unique; /* property */
  gboolean m_pkey; /* property */
}GdaDdlColumnPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GdaDdlColumn, gda_ddl_column, G_TYPE_OBJECT)

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
    /*<private>*/
    N_PROPS
};

static void
_gda_ddl_column_set_type (GdaDdlColumn *self,
                          const gchar *type,
                          GError **error);

static GParamSpec *properties [N_PROPS] = {NULL};

/**
 * gda_ddl_column_new:
 *
 * Returns: New instance of #GdaDdlColumn
 *
<<<<<<< HEAD
 * Since: 6.0
 *
=======
>>>>>>> GdaDdlColumn object implementation was added
 */
GdaDdlColumn *
gda_ddl_column_new (void)
{
  return g_object_new (GDA_TYPE_DDL_COLUMN, NULL);
}

static void
gda_ddl_column_finalize (GObject *object)
{
  GdaDdlColumn *self = GDA_DDL_COLUMN (object);
  GdaDdlColumnPrivate *priv = gda_ddl_column_get_instance_private (self);

  g_free (priv->mp_name);
  g_free (priv->mp_type);
  g_free (priv->mp_default);
  g_free (priv->mp_check);

  G_OBJECT_CLASS (gda_ddl_column_parent_class)->finalize (object);
}

static void
gda_ddl_column_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  GdaDdlColumn *self = GDA_DDL_COLUMN (object);
  GdaDdlColumnPrivate *priv = gda_ddl_column_get_instance_private (self);

  switch (prop_id) {
    case PROP_COLUMN_NAME:
      g_value_set_string (value,priv->mp_name);
      break;
    case PROP_COLUMN_SIZE:
      g_value_set_uint (value,priv->m_size);
      break;
    case PROP_COLUMN_NNUL:
      g_value_set_boolean (value,priv->m_nnul);
      break;
    case PROP_COLUMN_UNIQUE:
      g_value_set_boolean (value,priv->m_unique);
      break;
    case PROP_COLUMN_PKEY:
      g_value_set_boolean (value,priv->m_pkey);
      break;
    case PROP_COLUMN_AUTOINC:
      g_value_set_boolean (value,priv->m_autoinc);
      break;
    case PROP_COLUMN_DEFAULT:
      g_value_set_string (value,priv->mp_default);
      break;
    case PROP_COLUMN_CHECK:
      g_value_set_string (value,priv->mp_check);
      break;
    case PROP_COLUMN_COMMENT:
      g_value_set_string (value,priv->mp_comment);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gda_ddl_column_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  GdaDdlColumn *self = GDA_DDL_COLUMN (object);
  GdaDdlColumnPrivate *priv = gda_ddl_column_get_instance_private (self);

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
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gda_ddl_column_class_init (GdaDdlColumnClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gda_ddl_column_finalize;
  object_class->get_property = gda_ddl_column_get_property;
  object_class->set_property = gda_ddl_column_set_property;

  properties[PROP_COLUMN_NAME] =
    g_param_spec_string ("name",
                         "Name",
                         "Column name",
                         NULL,
                         G_PARAM_READWRITE);

  properties[PROP_COLUMN_SIZE] =
    g_param_spec_uint ("size",
                       "Size",
                       "Column size",
                       1,
                       9999, /* FIXME: should be reasonable value*/
                       80,
                       G_PARAM_READWRITE);

  properties[PROP_COLUMN_NNUL] =
    g_param_spec_boolean ("nnul",
                          "NotNULL",
                          "Can value be NULL",
                          TRUE,
                          G_PARAM_READWRITE);

  properties[PROP_COLUMN_AUTOINC] =
    g_param_spec_boolean ("autoinc",
                          "Autoinc",
                          "Can value be autoincremented",
                          FALSE,
                          G_PARAM_READWRITE);

  properties[PROP_COLUMN_UNIQUE] =
    g_param_spec_boolean ("unique",
                          "Unique",
                          "Can value be unique",
                          FALSE,
                          G_PARAM_READWRITE);

  properties[PROP_COLUMN_PKEY] =
    g_param_spec_boolean ("pkey",
                          "Pkey",
                          "Is is primery key",
                          FALSE,
                          G_PARAM_READWRITE);

  properties[PROP_COLUMN_DEFAULT] =
    g_param_spec_string ("default",
                         "Default",
                         "Default value",
                         NULL,
                         G_PARAM_READWRITE);

  properties[PROP_COLUMN_CHECK] =
    g_param_spec_string ("check",
                         "Check",
                         "Column check string",
                         NULL,
                         G_PARAM_READWRITE);

  properties[PROP_COLUMN_COMMENT] =
    g_param_spec_string ("comment",
                         "Comment",
                         "Column comment",
                         NULL,
                         G_PARAM_READWRITE);

  g_object_class_install_properties (object_class, N_PROPS, properties);

}

static void
gda_ddl_column_init (GdaDdlColumn *self)
{
  GdaDdlColumnPrivate *priv = gda_ddl_column_get_instance_private (self);

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
}

/**
 * gda_ddl_column_free:
 * @self: a #GdaDdlColumn object
<<<<<<< HEAD
 *
=======
>>>>>>> GdaDdlColumn object implementation was added
 * Convenient function to free the object
 *
 * Since: 6.0
 */
void gda_ddl_column_free (GdaDdlColumn *self)
{
  g_clear_object (&self);
}

/**
 * gda_ddl_column_parse_node:
<<<<<<< HEAD
 * @self: #GdaDdlColumn object to store parsed data
 * @node: instance of #xmlNodePtr to parse
 * @error: #GError to handle an error
=======
 * @self: @GdaDdlColumn object to store parsed data
 * @node: instance of @xmlNodePtr to parse
 * @error: @GError to handle an error
>>>>>>> GdaDdlColumn object implementation was added
 *
 * Parse #xmlNodePtr wich should point to <column> node
 *
 * Returns: %TRUE if no error, %FALSE otherwise
 *
 * Since: 6.0
 */
gboolean
<<<<<<< HEAD
gda_ddl_column_parse_node (GdaDdlColumn *self,
                           xmlNodePtr   node,
                           GError       **error)
=======
gda_ddl_column_parse_node (GdaDdlColumn  *self,
                           xmlNode		*node,
                           GError      **error)
>>>>>>> GdaDdlColumn object implementation was added
{
  g_return_val_if_fail (self,FALSE);
  g_return_val_if_fail (node,FALSE);

  xmlChar *name = xmlGetProp (node,(xmlChar *)"name");
  g_assert (name); /* If here bug with xml validation */
  g_object_set (G_OBJECT(self),"name",(gchar*)name,NULL);
  xmlFree (name);
  xmlChar *type = xmlGetProp (node,(xmlChar *)"type");
  g_assert (type); /* If here buf with validation */
  _gda_ddl_column_set_type (self,(const gchar *)type,error);
  xmlFree (type);

  xmlChar *cprop = NULL;
  cprop = xmlGetProp (node,(xmlChar *)"pkey");
  if (cprop) {
      if (*cprop == 't' || *cprop == 'T') {
          g_object_set (G_OBJECT(self),"pkey",TRUE,NULL);
      }
      else if (*cprop == 'f' || *cprop == 'F') {
          g_object_set (G_OBJECT(self),"pkey",FALSE,NULL);
      }
      else {
          /*
           * FIXME: this step should never happend
           */
      }

      xmlFree (cprop);
  }
  cprop = xmlGetProp (node,(xmlChar *)"unique");

  if (cprop) {
      if (*cprop == 't' || *cprop == 'T') {
          g_object_set (G_OBJECT(self),"unique",TRUE,NULL);
      }
      else if (*cprop == 'f' || *cprop == 'F') {
          g_object_set (G_OBJECT(self),"unique",FALSE,NULL);
      }
      else {
<<<<<<< HEAD
          /*
           * FIXME: this step should never happend
           */
=======

>>>>>>> GdaDdlColumn object implementation was added
      }

      xmlFree (cprop);
  }

  cprop = xmlGetProp (node,(xmlChar *)"autoinc");

  if (cprop) {
      if (*cprop == 't' || *cprop == 'T') {
          g_object_set (G_OBJECT(self),"autoinc",TRUE,NULL);
      }
      else if (*cprop == 'f' || *cprop == 'F') {
          g_object_set (G_OBJECT(self),"autoinc",FALSE,NULL);
      }
      else {

      }

      xmlFree (cprop);
  }

  cprop = xmlGetProp (node,(xmlChar *)"nnul");

  if (cprop) {
      if (*cprop == 't' || *cprop == 'T') {
          g_object_set (G_OBJECT(self),"nnul",TRUE,NULL);
      }
      else if (*cprop == 'f' || *cprop == 'F') {
          g_object_set (G_OBJECT(self),"nnul",FALSE,NULL);
      }
      else {

      }

      xmlFree (cprop);
  }

  for (xmlNodePtr it = node->children; it ; it = it->next) {
      if (!g_strcmp0 ((char *)it->name,"size")) {
          xmlChar *size = xmlNodeGetContent (it);
          guint64 tint = g_ascii_strtoull ((gchar*)size,NULL,10);

          g_object_set (G_OBJECT (self),"size",(guint)tint,NULL);
          xmlFree (size);
      }

      if (!g_strcmp0 ((char *)it->name,"pkey")) {
          xmlChar *pkey = xmlNodeGetContent (it);
          g_object_set (G_OBJECT (self),"pkey",pkey,NULL);
          xmlFree (pkey);
      }

      if (!g_strcmp0 ((char *)it->name,"default")) {
          xmlChar *def = xmlNodeGetContent (it);
          g_object_set (G_OBJECT (self),"default",def,NULL);
          xmlFree (def);
      }

      if (!g_strcmp0 ((char *)it->name,"check")) {
          xmlChar *check = xmlNodeGetContent (it);
          g_object_set (G_OBJECT (self),"check",check,NULL);
          xmlFree (check);
      }

      if (!g_strcmp0 ((char *)it->name,"comment")) {
          xmlChar *comment = xmlNodeGetContent (it);
          g_object_set (G_OBJECT (self),"comment",comment,NULL);
          xmlFree (comment);
      }
  } /* End of for loop */
  return TRUE;
} /* End of gda_ddl_column_parse_node */

static void
_gda_ddl_column_set_type (GdaDdlColumn *self,
                          const char *type,
                          GError **error)
{
  g_return_if_fail (self);
  g_return_if_fail (type);
  //	g_return_if_fail (!*error);
  GdaDdlColumnPrivate *priv = gda_ddl_column_get_instance_private (self);
  GType ttype = gda_g_type_from_string (type);
  if (ttype != G_TYPE_INVALID){
      priv->mp_type = g_strdup (type);
      priv->m_gtype = ttype;
  }
  else {
      g_set_error (error,
                   GDA_DDL_COLUMN_ERROR,
                   GDA_DDL_COLUMN_TYPE,
                   _("Invalid column type %s. See API for gda_g_type_from_string ().\n"),
                   type);
  }
}

/**
 * gda_ddl_column_get_name:
 * @self: a #GdaDdlColumn object
 *
<<<<<<< HEAD
 * Returns: Column name as a string or %NULL.
=======
 * Returns: Column name as a string.
>>>>>>> GdaDdlColumn object implementation was added
 *
 * Since: 6.0
 */
const gchar*
gda_ddl_column_get_name (GdaDdlColumn *self)
{
  g_return_val_if_fail (self,NULL);
  GdaDdlColumnPrivate *priv = gda_ddl_column_get_instance_private (self);

  return priv->mp_name;
}

/**
 * gda_ddl_column_set_name:
 * @self: a #GdaDdlColumn object
 * @name: name as a string
 *
 * Set column name.
 *
 * Since: 6.0
 */
void
gda_ddl_column_set_name (GdaDdlColumn *self,
                         const gchar *name)
{
  GdaDdlColumnPrivate *priv = gda_ddl_column_get_instance_private (self);

  g_free (priv->mp_name);
  priv->mp_name = g_strdup (name);
}

/**
 * gda_ddl_column_get_gtype:
 * @self: a #GdaDdlColumn object
 *
 * Return of column type as #GType
 *
 * Since: 6.0
 */
GType
gda_ddl_column_get_gtype (GdaDdlColumn *self)
{
  g_return_val_if_fail (self,GDA_TYPE_NULL);
  GdaDdlColumnPrivate *priv = gda_ddl_column_get_instance_private (self);

  return priv->m_gtype;
}

/**
 * gda_ddl_column_get_ctype:
 * @self: a #GdaDdlColumn object
 *
<<<<<<< HEAD
 * Returns: column type as a string or %NULL
=======
 * Returns: column type as a string
>>>>>>> GdaDdlColumn object implementation was added
 *
 * Since: 6.0
 */
const gchar*
gda_ddl_column_get_ctype (GdaDdlColumn *self)
{
  GdaDdlColumnPrivate *priv = gda_ddl_column_get_instance_private (self);
  return priv->mp_type;
}

/**
 * gda_ddl_column_set_type:
 * @self: a #GdaDdlColumn
 * @type: #GType for column
 *
 * Set type of the column as a #GType
 *
 * Since: 6.0
 */
void
gda_ddl_column_set_type (GdaDdlColumn *self,
                         GType type)
{
  GdaDdlColumnPrivate *priv = gda_ddl_column_get_instance_private (self);
  priv->m_gtype = type;
}

/**
 * gda_ddl_column_get_scale:
 * @self: a #GdaDdlColumn object
 *
 * Scale is an order of column in an internal storage.
 *
 * Returns: column sequence number
 * Since: 6.0
 */
guint
gda_ddl_column_get_scale (GdaDdlColumn *self)
{
  GdaDdlColumnPrivate *priv = gda_ddl_column_get_instance_private (self);
  return priv->m_scale;
}

/**
 * gda_ddl_column_set scale:
 * @self: a #GdaDdlColumn
 * @scale: Column order number
 *
 * Since: 6.0
 */
void
gda_ddl_column_set_scale (GdaDdlColumn *self,
                          guint scale)
{
  GdaDdlColumnPrivate *priv = gda_ddl_column_get_instance_private (self);
  priv->m_scale = scale;
}

/**
 * gda_ddl_column_get_pkey:
 * @self: a #GdaDdlColumn object
 *
 * Returns: %TRUE if the column is primary key, %FALSE otherwise
 *
 * Since: 6.0
 */
gboolean
gda_ddl_column_get_pkey (GdaDdlColumn *self)
{
  GdaDdlColumnPrivate *priv = gda_ddl_column_get_instance_private (self);
  return priv->m_pkey;
}

/**
 * gda_ddl_column_set_pkey:
 * @self: a #GdaDdlColumn object
 * @pkey: value to set
 *
 * Since: 6.0
 */
void
gda_ddl_column_set_pkey (GdaDdlColumn *self,
                         gboolean pkey)
{
  GdaDdlColumnPrivate *priv = gda_ddl_column_get_instance_private (self);

  priv->m_pkey = pkey;
}
<<<<<<< HEAD

=======
>>>>>>> GdaDdlColumn object implementation was added
/**
 * gda_ddl_column_get_nnul:
 * @self: a @GdaDdlColumn object
 *
 * Since: 6.0
 */
gboolean
gda_ddl_column_get_nnul (GdaDdlColumn *self)
{
  GdaDdlColumnPrivate *priv = gda_ddl_column_get_instance_private (self);
  return priv->m_nnul;
}

<<<<<<< HEAD
/**
 * gda_ddl_column_set_nnul:
 * @self: a GdaDdlColumn object
 * @nnul: value to set for nnul
 *
 * Since: 6.0
 */
=======
>>>>>>> GdaDdlColumn object implementation was added
void
gda_ddl_column_set_nnul (GdaDdlColumn *self,
                         gboolean nnul)
{
  GdaDdlColumnPrivate *priv = gda_ddl_column_get_instance_private (self);
  priv->m_nnul = nnul;
}

<<<<<<< HEAD
/**
 * gda_ddl_column_get_autoinc:
 * @self: a #GdaDdlColumn object
 *
 * Get value for autoinc key
 *
 * Since: 6.0
 */
=======
>>>>>>> GdaDdlColumn object implementation was added
gboolean
gda_ddl_column_get_autoinc (GdaDdlColumn *self)
{
  GdaDdlColumnPrivate *priv = gda_ddl_column_get_instance_private (self);
  return priv->m_autoinc;
}

void
gda_ddl_column_set_autoinc (GdaDdlColumn *self,
                            gboolean autoinc)
{
  GdaDdlColumnPrivate *priv = gda_ddl_column_get_instance_private (self);
  priv->m_autoinc = autoinc;
}

gboolean
gda_ddl_column_get_unique (GdaDdlColumn *self)
{
  GdaDdlColumnPrivate *priv = gda_ddl_column_get_instance_private (self);
  return priv->m_unique;
}

void
gda_ddl_column_set_unique (GdaDdlColumn *self,
                           gboolean unique)
{
  GdaDdlColumnPrivate *priv = gda_ddl_column_get_instance_private (self);
  priv->m_unique = unique;
}

const gchar*
gda_ddl_column_get_comment (GdaDdlColumn *self)
{
  GdaDdlColumnPrivate *priv = gda_ddl_column_get_instance_private (self);

  return priv->mp_comment;
}

void
gda_ddl_column_set_comment (GdaDdlColumn *self,
                            const gchar *comnt)
{
  GdaDdlColumnPrivate *priv = gda_ddl_column_get_instance_private (self);
  g_free (priv->mp_comment);
  priv->mp_comment = g_strdup (comnt);
}

guint
gda_ddl_column_get_size (GdaDdlColumn *self)
{
  GdaDdlColumnPrivate *priv = gda_ddl_column_get_instance_private (self);
  return priv->m_size;
}

void
gda_ddl_column_set_size (GdaDdlColumn *self,
                         guint size)
{
  GdaDdlColumnPrivate *priv = gda_ddl_column_get_instance_private (self);
  priv->m_size = size;
}

const gchar*
gda_ddl_column_get_default (GdaDdlColumn *self)
{
  GdaDdlColumnPrivate *priv = gda_ddl_column_get_instance_private (self);
  return priv->mp_default;
}

void
gda_ddl_column_set_default (GdaDdlColumn *self,
                            const gchar *value)
{
  GdaDdlColumnPrivate *priv = gda_ddl_column_get_instance_private (self);
  g_free(priv->mp_default);
  priv->mp_default = g_strdup(value);
}

const gchar*
gda_ddl_column_get_check (GdaDdlColumn *self)
{
  GdaDdlColumnPrivate *priv = gda_ddl_column_get_instance_private (self);
  return priv->mp_check;
}

void
gda_ddl_column_set_check (GdaDdlColumn *self,
                          const gchar *value)
{
  GdaDdlColumnPrivate *priv = gda_ddl_column_get_instance_private (self);
  g_free(priv->mp_check);
  priv->mp_check = g_strdup(value);
}

