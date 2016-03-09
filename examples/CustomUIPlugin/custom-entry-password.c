/*
 * Copyright (C) 2011 - 2012 Vivien Malerba <malerba@gnome-db.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */
#include "custom-entry-password.h"
#include <libgda/gda-data-handler.h>
#include <string.h>

/* 
 * Main static functions 
 */
static void custom_entry_password_class_init (CustomEntryPasswordClass * class);
static void custom_entry_password_init (CustomEntryPassword * srv);
static void custom_entry_password_finalize (GObject   * object);

/* virtual functions */
static GtkWidget *create_entry (GdauiEntryWrapper *mgwrap);
static void       real_set_value (GdauiEntryWrapper *mgwrap, const GValue *value);
static GValue    *real_get_value (GdauiEntryWrapper *mgwrap);
static void       connect_signals(GdauiEntryWrapper *mgwrap, GCallback modify_cb, GCallback activate_cb);
static gboolean   can_expand (GdauiEntryWrapper *mgwrap, gboolean horiz);


/* get a pointer to the parents to be able to call their destructor */
static GObjectClass  *parent_class = NULL;

/* private structure */
struct _CustomEntryPasswordPrivate
{
	GtkWidget    *entry;
	guint         minlength; /* 0 for no limit */
};

GType
custom_entry_password_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo info = {
			sizeof (CustomEntryPasswordClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) custom_entry_password_class_init,
			NULL,
			NULL,
			sizeof (CustomEntryPassword),
			0,
			(GInstanceInitFunc) custom_entry_password_init,
			0
		};
		
		type = g_type_register_static (GDAUI_TYPE_ENTRY_WRAPPER, "CustomEntryPassword", &info, 0);
	}
	return type;
}

static void
custom_entry_password_class_init (CustomEntryPasswordClass * class)
{
	GObjectClass   *object_class = G_OBJECT_CLASS (class);

	parent_class = g_type_class_peek_parent (class);

	object_class->finalize = custom_entry_password_finalize;

	GDAUI_ENTRY_WRAPPER_CLASS (class)->create_entry = create_entry;
	GDAUI_ENTRY_WRAPPER_CLASS (class)->real_set_value = real_set_value;
	GDAUI_ENTRY_WRAPPER_CLASS (class)->real_get_value = real_get_value;
	GDAUI_ENTRY_WRAPPER_CLASS (class)->connect_signals = connect_signals;
	GDAUI_ENTRY_WRAPPER_CLASS (class)->can_expand = can_expand;
}

static void
custom_entry_password_init (CustomEntryPassword * custom_entry_password)
{
	custom_entry_password->priv = g_new0 (CustomEntryPasswordPrivate, 1);
	custom_entry_password->priv->entry = NULL;
	custom_entry_password->priv->minlength = 0;
}

/*
 * custom_entry_password_new:
 * @dh: the data handler to be used by the new widget
 * @type: the requested data type (compatible with @dh)
 *
 * Creates a new widget which is mainly a GtkEntry
 *
 * Returns: the new widget
 */
GtkWidget *
custom_entry_password_new (GdaDataHandler *dh, GType type, const gchar *options)
{
	GObject *obj;
	CustomEntryPassword *mgtxt;

	g_return_val_if_fail (dh && GDA_IS_DATA_HANDLER (dh), NULL);
	g_return_val_if_fail (type != G_TYPE_INVALID, NULL);
	g_return_val_if_fail (gda_data_handler_accepts_g_type (dh, type), NULL);

	obj = g_object_new (CUSTOM_ENTRY_PASSWORD_TYPE, "handler", dh, NULL);
	mgtxt = CUSTOM_ENTRY_PASSWORD (obj);
	gdaui_data_entry_set_value_type (GDAUI_DATA_ENTRY (mgtxt), type);

	if (options && *options) {
                GdaQuarkList *params;
                const gchar *str;

                params = gda_quark_list_new_from_string (options);
                str = gda_quark_list_find (params, "MINLENGTH");
                if (str) {
			mgtxt->priv->minlength = atoi (str);
			if (mgtxt->priv->minlength < 0)
				mgtxt->priv->minlength = 0;
                }
                gda_quark_list_free (params);
        }

	return GTK_WIDGET (obj);
}

static void
custom_entry_password_finalize (GObject   * object)
{
	CustomEntryPassword *custom_entry_password;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_CUSTOM_ENTRY_PASSWORD (object));

	custom_entry_password = CUSTOM_ENTRY_PASSWORD (object);
	if (custom_entry_password->priv) {
		g_free (custom_entry_password->priv);
		custom_entry_password->priv = NULL;
	}

	/* parent class */
	parent_class->finalize (object);
}

static GtkWidget *
create_entry (GdauiEntryWrapper *mgwrap)
{
	GtkWidget *entry;
        CustomEntryPassword *mgstr;

        g_return_val_if_fail (mgwrap && IS_CUSTOM_ENTRY_PASSWORD (mgwrap), NULL);
        mgstr = CUSTOM_ENTRY_PASSWORD (mgwrap);
        g_return_val_if_fail (mgstr->priv, NULL);

        entry = gtk_entry_new ();
        mgstr->priv->entry = entry;
	gtk_entry_set_visibility (GTK_ENTRY (entry), FALSE);

        return entry;
}

static void
real_set_value (GdauiEntryWrapper *mgwrap, const GValue *value)
{
	CustomEntryPassword *mgstr;

        g_return_if_fail (mgwrap && IS_CUSTOM_ENTRY_PASSWORD (mgwrap));
        mgstr = CUSTOM_ENTRY_PASSWORD (mgwrap);
        g_return_if_fail (mgstr->priv);

        if (value) {
                if (gda_value_is_null ((GValue *) value))
                        gtk_entry_set_text (GTK_ENTRY (mgstr->priv->entry), "");
                else {
                        GdaDataHandler *dh;
                        gchar *str;

                        dh = gdaui_data_entry_get_handler (GDAUI_DATA_ENTRY (mgwrap));
                        str = gda_data_handler_get_str_from_value (dh, value);
                        gtk_entry_set_text (GTK_ENTRY (mgstr->priv->entry), str);
			g_free (str);
                }
        }
        else
                gtk_entry_set_text (GTK_ENTRY (mgstr->priv->entry), "");
}

static GValue *
real_get_value (GdauiEntryWrapper *mgwrap)
{
	GValue *value = NULL;
        CustomEntryPassword *mgstr;
        GdaDataHandler *dh;
        const gchar *str;
	GType type;

        g_return_val_if_fail (mgwrap && IS_CUSTOM_ENTRY_PASSWORD (mgwrap), NULL);
        mgstr = CUSTOM_ENTRY_PASSWORD (mgwrap);
        g_return_val_if_fail (mgstr->priv, NULL);

        dh = gdaui_data_entry_get_handler (GDAUI_DATA_ENTRY (mgwrap));
        str = gtk_entry_get_text (GTK_ENTRY (mgstr->priv->entry));
	type = gdaui_data_entry_get_value_type (GDAUI_DATA_ENTRY (mgwrap));
	
	if (*str) {
		if (((mgstr->priv->minlength > 0) && (strlen (str) >= mgstr->priv->minlength)) ||
		    (mgstr->priv->minlength == 0))
			value = gda_data_handler_get_value_from_str (dh, str, type);
	}

        if (!value) {
                /* in case the gda_data_handler_get_value_from_str() returned an error because
                   the contents of the GtkEntry cannot be interpreted as a GValue */
                value = gda_value_new_null ();
        }

        return value;
}

static void
connect_signals(GdauiEntryWrapper *mgwrap, GCallback modify_cb, GCallback activate_cb)
{
	CustomEntryPassword *mgstr;

        g_return_if_fail (mgwrap && IS_CUSTOM_ENTRY_PASSWORD (mgwrap));
        mgstr = CUSTOM_ENTRY_PASSWORD (mgwrap);
        g_return_if_fail (mgstr->priv);

        g_signal_connect (G_OBJECT (mgstr->priv->entry), "changed",
                          modify_cb, mgwrap);
        g_signal_connect (G_OBJECT (mgstr->priv->entry), "activate",
                          activate_cb, mgwrap);
}

static gboolean
can_expand (G_GNUC_UNUSED GdauiEntryWrapper *mgwrap, G_GNUC_UNUSED gboolean horiz)
{
	return FALSE;
}
