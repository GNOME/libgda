/*
 * Copyright (C) 2019 Daniel Espinosa <esodan@gmail.com>
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

#include "gda-handler-text.h"
#include <string.h>
#include <glib/gi18n-lib.h>
#include <libgda/gda-util.h>
#include <libgda/gda-server-provider.h>

typedef struct
{
  GWeakRef cnc;
} GdaHandlerTextPrivate;

static void data_handler_iface_init (GdaDataHandlerInterface *iface);

G_DEFINE_TYPE_WITH_CODE(GdaHandlerText, gda_handler_text, G_TYPE_OBJECT,
                         G_ADD_PRIVATE(GdaHandlerText)
                         G_IMPLEMENT_INTERFACE (GDA_TYPE_DATA_HANDLER, data_handler_iface_init))

/**
 * gda_handler_text_new:
 *
 * Creates a data handler for large strings
 *
 * Returns: (transfer full): the new object
 */
GdaDataHandler*
gda_handler_text_new (void)
{
  GObject *obj;

  obj = g_object_new (GDA_TYPE_HANDLER_TEXT, NULL);

  return (GdaDataHandler *) obj;
}

/**
 * gda_handler_text_new_with_connection:
 * @cnc: (nullable): a #GdaConnection object
 *
 * Creates a data handler for strings, which will use some specific methods implemented
 * by the provider object associated with @cnc.
 *
 * Returns: (type GdaHandlerText) (transfer full): the new object
 */
GdaDataHandler *
gda_handler_text_new_with_connection (GdaConnection *cnc)
{
  GObject *obj;
  GdaHandlerText *dh;

  g_return_val_if_fail (cnc != NULL, NULL);

  obj = g_object_new (GDA_TYPE_HANDLER_TEXT, NULL);
  dh = (GdaHandlerText*) obj;

  GdaHandlerTextPrivate *priv = gda_handler_text_get_instance_private (dh);

  g_weak_ref_set (&priv->cnc, cnc);

  return (GdaDataHandler *) obj;
}

static gchar *
gda_handler_text_get_sql_from_value (GdaDataHandler *iface, const GValue *value)
{
  g_return_val_if_fail (GDA_IS_HANDLER_TEXT (iface), NULL);
  g_return_val_if_fail (value != NULL, NULL);

  gchar *str, *retval;
  GdaHandlerText *hdl;
  GdaConnection *cnc;

  hdl = (GdaHandlerText*) (iface);

  GdaHandlerTextPrivate *priv = gda_handler_text_get_instance_private (hdl);

  str = gda_value_stringify ((GValue *) value);
  if (str) {
    gchar *str2;
    cnc = g_weak_ref_get (&priv->cnc);
    if (cnc != NULL) {
      str2 = gda_server_provider_escape_string (gda_connection_get_provider (cnc), cnc, str);
    } else {
      str2 = gda_default_escape_string (str);
    }
    retval = g_strdup_printf ("'%s'", str2);
    g_free (str2);
    g_free (str);
  } else {
    retval = g_strdup ("NULL");
  }

  return retval;
}

static gchar *
gda_handler_text_get_str_from_value (G_GNUC_UNUSED GdaDataHandler *iface, const GValue *value)
{
  g_assert (value);
  return gda_value_stringify ((GValue *) value);
}

static GValue *
gda_handler_text_get_value_from_sql (GdaDataHandler *iface, const gchar *sql, G_GNUC_UNUSED GType type)
{
  GValue *value = NULL;

  g_return_val_if_fail (iface != NULL, NULL);
  g_return_val_if_fail (GDA_IS_HANDLER_TEXT (iface), NULL);
  g_return_val_if_fail (g_utf8_validate (sql, -1, NULL), NULL);

  if (sql != NULL) {
    gchar *unstr = NULL;
    if (g_str_has_prefix (sql, "'") && g_str_has_suffix (sql, "'")) {
      glong len = g_utf8_strlen (sql, -1);
      unstr = g_utf8_substring (sql, 1, len - 1);
    } else {
      unstr = g_strdup (sql);
    }
    GdaText *text = gda_text_new ();
    gda_text_set_string (text, unstr);
    value = gda_value_new (GDA_TYPE_TEXT);
    g_value_take_boxed (value, text);
    g_free (unstr);
  }
  else
    value = gda_value_new_null ();

  return value;
}

static GValue *
gda_handler_text_get_value_from_str (G_GNUC_UNUSED GdaDataHandler *iface, const gchar *str, G_GNUC_UNUSED GType type)
{
  GValue *value;

  GdaText *text = gda_text_new ();
  gda_text_set_string (text, str);
  value = gda_value_new (GDA_TYPE_TEXT);
  g_value_take_boxed (value, text);

  return value;
}

static GValue *
gda_handler_text_get_sane_init_value (G_GNUC_UNUSED GdaDataHandler *iface, G_GNUC_UNUSED GType type)
{
  GValue *value;

  GdaText *text = gda_text_new ();
  gda_text_set_string (text, "");
  value = gda_value_new (GDA_TYPE_TEXT);
  g_value_take_boxed (value, text);

  return value;
}

static gboolean
gda_handler_text_accepts_g_type (GdaDataHandler *iface, GType type)
{
  g_return_val_if_fail (iface != NULL, FALSE);

  gboolean ret = FALSE;

  if (g_type_is_a (type, GDA_TYPE_TEXT)) {
    ret = TRUE;
  } else if (g_type_is_a (type, G_TYPE_STRING)) {
    ret = TRUE;
  } else if (g_value_type_compatible (type, GDA_TYPE_TEXT)) {
    ret = TRUE;
  }
  return ret;
}

static const gchar *
gda_handler_text_get_descr (GdaDataHandler *iface)
{
  g_return_val_if_fail (GDA_IS_HANDLER_TEXT (iface), NULL);
  return g_object_get_data (G_OBJECT (iface), "descr");
}

static void
gda_handler_text_init (GdaHandlerText * hdl)
{
  GdaHandlerTextPrivate *priv = gda_handler_text_get_instance_private (hdl);
  g_weak_ref_init (&priv->cnc, NULL);
  /* Handler support */
  g_object_set_data (G_OBJECT (hdl), "name", "InternalLargeString");
  g_object_set_data (G_OBJECT (hdl), "descr", _("Large String representation"));
}

static void
gda_handler_text_dispose (GObject   *object)
{
  GdaHandlerText *hdl = (GdaHandlerText *) object;

  g_return_if_fail (object != NULL);
  g_return_if_fail (GDA_IS_HANDLER_TEXT (object));

  GdaHandlerTextPrivate *priv = gda_handler_text_get_instance_private (hdl);
  g_weak_ref_clear (&priv->cnc);

  /* for the parent class */
  G_OBJECT_CLASS (gda_handler_text_parent_class)->dispose (object);
}

static void
data_handler_iface_init (GdaDataHandlerInterface *iface)
{
	iface->get_sql_from_value = gda_handler_text_get_sql_from_value;
	iface->get_str_from_value = gda_handler_text_get_str_from_value;
	iface->get_value_from_sql = gda_handler_text_get_value_from_sql;
	iface->get_value_from_str = gda_handler_text_get_value_from_str;
	iface->get_sane_init_value = gda_handler_text_get_sane_init_value;
	iface->accepts_g_type = gda_handler_text_accepts_g_type;
	iface->get_descr = gda_handler_text_get_descr;
}

static void
gda_handler_text_class_init (GdaHandlerTextClass * class)
{
	GObjectClass   *object_class = G_OBJECT_CLASS (class);

	object_class->dispose = gda_handler_text_dispose;
}
