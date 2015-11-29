/*
 * Copyright (C) 2012 - 2015 Vivien Malerba <malerba@gnome-db.org>
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
#include "gdaui-entry-format.h"
#include <libgda/gda-data-handler.h>
#include <string.h>
#include "gdaui-formatted-entry.h"

/* 
 * Main static functions 
 */
static void gdaui_entry_format_class_init (GdauiEntryFormatClass * class);
static void gdaui_entry_format_init (GdauiEntryFormat * srv);
static void gdaui_entry_format_dispose (GObject   * object);
static void gdaui_entry_format_finalize (GObject   * object);

/* virtual functions */
static GtkWidget *create_entry (GdauiEntryWrapper *mgwrap);
static void       real_set_value (GdauiEntryWrapper *mgwrap, const GValue *value);
static GValue    *real_get_value (GdauiEntryWrapper *mgwrap);
static void       connect_signals(GdauiEntryWrapper *mgwrap, GCallback modify_cb, GCallback activate_cb);

/* get a pointer to the parents to be able to call their destructor */
static GObjectClass  *parent_class = NULL;

/* private structure */
struct _GdauiEntryFormatPrivate
{
	GtkWidget *entry;
	gchar     *format;
	gchar     *mask;
};


GType
gdaui_entry_format_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo info = {
			sizeof (GdauiEntryFormatClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gdaui_entry_format_class_init,
			NULL,
			NULL,
			sizeof (GdauiEntryFormat),
			0,
			(GInstanceInitFunc) gdaui_entry_format_init,
			0
		};
		
		type = g_type_register_static (GDAUI_TYPE_ENTRY_WRAPPER, "GdauiEntryFormat", &info, 0);
	}
	return type;
}

static void
gdaui_entry_format_class_init (GdauiEntryFormatClass * class)
{
	GObjectClass   *object_class = G_OBJECT_CLASS (class);

	parent_class = g_type_class_peek_parent (class);

	object_class->dispose = gdaui_entry_format_dispose;
	object_class->finalize = gdaui_entry_format_finalize;

	GDAUI_ENTRY_WRAPPER_CLASS (class)->create_entry = create_entry;
	GDAUI_ENTRY_WRAPPER_CLASS (class)->real_set_value = real_set_value;
	GDAUI_ENTRY_WRAPPER_CLASS (class)->real_get_value = real_get_value;
	GDAUI_ENTRY_WRAPPER_CLASS (class)->connect_signals = connect_signals;
}

static void
gdaui_entry_format_init (GdauiEntryFormat * gdaui_entry_format)
{
	gdaui_entry_format->priv = g_new0 (GdauiEntryFormatPrivate, 1);
	gdaui_entry_format->priv->entry = NULL;
	gdaui_entry_format->priv->format = NULL;
	gdaui_entry_format->priv->mask = NULL;
}

/**
 * gdaui_entry_format_new
 * @dh: the data handler to be used by the new widget
 * @type: the requested data type (compatible with @dh)
 * @options: the options
 *
 * Creates a new widget which is mainly a GtkEntry
 *
 * Returns: the new widget
 */
GtkWidget *
gdaui_entry_format_new (GdaDataHandler *dh, GType type, const gchar *options)
{
	GObject *obj;
	GdauiEntryFormat *mgformat;

	g_return_val_if_fail (dh && GDA_IS_DATA_HANDLER (dh), NULL);
	g_return_val_if_fail (type != G_TYPE_INVALID, NULL);
	g_return_val_if_fail (gda_data_handler_accepts_g_type (dh, type), NULL);

	obj = g_object_new (GDAUI_TYPE_ENTRY_FORMAT, "handler", dh, NULL);
	mgformat = GDAUI_ENTRY_FORMAT (obj);
	if (options && *options) {
                GdaQuarkList *params;
                const gchar *str;

                params = gda_quark_list_new_from_string (options);
                str = gda_quark_list_find (params, "FORMAT");
                if (str)
                        mgformat->priv->format = g_strdup (str);
                str = gda_quark_list_find (params, "MASK");
                if (str)
                        mgformat->priv->mask = g_strdup (str);

                gda_quark_list_free (params);
        }

	gdaui_data_entry_set_value_type (GDAUI_DATA_ENTRY (mgformat), type);

	return GTK_WIDGET (obj);
}


static void
gdaui_entry_format_dispose (GObject   * object)
{
	GdauiEntryFormat *gdaui_entry_format;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GDAUI_IS_ENTRY_FORMAT (object));

	gdaui_entry_format = GDAUI_ENTRY_FORMAT (object);
	if (gdaui_entry_format->priv) {
		g_free (gdaui_entry_format->priv->format);
		g_free (gdaui_entry_format->priv->mask);
		g_free (gdaui_entry_format->priv);
		gdaui_entry_format->priv = NULL;
	}

	/* parent class */
	parent_class->dispose (object);
}

static void
gdaui_entry_format_finalize (GObject   * object)
{
	GdauiEntryFormat *gdaui_entry_format;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GDAUI_IS_ENTRY_FORMAT (object));

	gdaui_entry_format = GDAUI_ENTRY_FORMAT (object);
	if (gdaui_entry_format->priv) {

		g_free (gdaui_entry_format->priv);
		gdaui_entry_format->priv = NULL;
	}

	/* parent class */
	parent_class->finalize (object);
}

static GtkWidget *
create_entry (GdauiEntryWrapper *mgwrap)
{
	GtkWidget *entry;
	GdauiEntryFormat *mgformat;

	g_return_val_if_fail (mgwrap && GDAUI_IS_ENTRY_FORMAT (mgwrap), NULL);
	mgformat = GDAUI_ENTRY_FORMAT (mgwrap);
	g_return_val_if_fail (mgformat->priv, NULL);

	entry = gdaui_formatted_entry_new (mgformat->priv->format, mgformat->priv->mask);

	mgformat->priv->entry = entry;
	if (mgformat->priv->format)
		gtk_entry_set_width_chars (GTK_ENTRY (entry), g_utf8_strlen (mgformat->priv->format, -1));

	return entry;
}

static void
real_set_value (GdauiEntryWrapper *mgwrap, const GValue *value)
{
	GdauiEntryFormat *mgformat;

	g_return_if_fail (mgwrap && GDAUI_IS_ENTRY_FORMAT (mgwrap));
	mgformat = GDAUI_ENTRY_FORMAT (mgwrap);
	g_return_if_fail (mgformat->priv);

	if (value) {
		if (gda_value_is_null ((GValue *) value))
			gdaui_entry_set_text (GDAUI_ENTRY (mgformat->priv->entry), NULL);
		else
			gdaui_entry_set_text (GDAUI_ENTRY (mgformat->priv->entry),
					      g_value_get_string (value));
	}
	else
		gdaui_entry_set_text (GDAUI_ENTRY (mgformat->priv->entry), NULL);
}

static GValue *
real_get_value (GdauiEntryWrapper *mgwrap)
{
	GValue *value = NULL;
	GdauiEntryFormat *mgformat;
	gchar *tmp;

	g_return_val_if_fail (mgwrap && GDAUI_IS_ENTRY_FORMAT (mgwrap), NULL);
	mgformat = GDAUI_ENTRY_FORMAT (mgwrap);
	g_return_val_if_fail (mgformat->priv, NULL);

	tmp = gdaui_entry_get_text (GDAUI_ENTRY (mgformat->priv->entry));
	if (tmp && *tmp)
		g_value_take_string ((value = gda_value_new (G_TYPE_STRING)), tmp);
	else {
		g_free (tmp);
		value = gda_value_new_null ();
	}

	return value;
}

static void
connect_signals (GdauiEntryWrapper *mgwrap, GCallback modify_cb, GCallback activate_cb)
{
	GdauiEntryFormat *mgformat;

	g_return_if_fail (mgwrap && GDAUI_IS_ENTRY_FORMAT (mgwrap));
	mgformat = GDAUI_ENTRY_FORMAT (mgwrap);
	g_return_if_fail (mgformat->priv);

	g_signal_connect_swapped (G_OBJECT (mgformat->priv->entry), "changed", modify_cb, mgwrap);
	g_signal_connect_swapped (G_OBJECT (mgformat->priv->entry), "activate", activate_cb, mgwrap);
}
