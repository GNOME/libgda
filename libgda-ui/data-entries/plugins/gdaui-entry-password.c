/*
 * Copyright (C) 2009 - 2015 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2010 David King <davidk@openismus.com>
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

#include "gdaui-entry-password.h"
#include <libgda/gda-data-handler.h>
#include <gcrypt.h>
#include <string.h>

/* 
 * Main static functions 
 */
static void gdaui_entry_password_class_init (GdauiEntryPasswordClass * class);
static void gdaui_entry_password_init (GdauiEntryPassword * srv);
static void gdaui_entry_password_finalize (GObject   * object);

/* virtual functions */
static GtkWidget *create_entry (GdauiEntryWrapper *mgwrap);
static void       real_set_value (GdauiEntryWrapper *mgwrap, const GValue *value);
static GValue    *real_get_value (GdauiEntryWrapper *mgwrap);
static void       connect_signals(GdauiEntryWrapper *mgwrap, GCallback modify_cb, GCallback activate_cb);

static void signal_handlers_block (GdauiEntryPassword *mgstr);
static void signal_handlers_unblock (GdauiEntryPassword *mgstr);
static void entry_delete_text_cb (GtkEditable *editable, gint start_pos, gint end_pos, GdauiEntryPassword *mgstr);
static void entry_insert_text_cb (GtkEditable *editable, const gchar *text, gint length, gint *position, GdauiEntryPassword *mgstr);

/* get a pointer to the parents to be able to call their destructor */
static GObjectClass  *parent_class = NULL;

typedef enum {
	ENCODING_NONE,
	ENCODING_MD5
} EncodingType;

/* private structure */
struct _GdauiEntryPasswordPrivate
{
	GtkWidget    *entry;
	gboolean      needs_encoding;
	EncodingType  encoding_type;
};

GType
gdaui_entry_password_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo info = {
			sizeof (GdauiEntryPasswordClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gdaui_entry_password_class_init,
			NULL,
			NULL,
			sizeof (GdauiEntryPassword),
			0,
			(GInstanceInitFunc) gdaui_entry_password_init,
			0
		};
		
		type = g_type_register_static (GDAUI_TYPE_ENTRY_WRAPPER, "GdauiEntryPassword", &info, 0);
	}
	return type;
}

static void
gdaui_entry_password_class_init (GdauiEntryPasswordClass * class)
{
	GObjectClass   *object_class = G_OBJECT_CLASS (class);

	parent_class = g_type_class_peek_parent (class);

	object_class->finalize = gdaui_entry_password_finalize;

	GDAUI_ENTRY_WRAPPER_CLASS (class)->create_entry = create_entry;
	GDAUI_ENTRY_WRAPPER_CLASS (class)->real_set_value = real_set_value;
	GDAUI_ENTRY_WRAPPER_CLASS (class)->real_get_value = real_get_value;
	GDAUI_ENTRY_WRAPPER_CLASS (class)->connect_signals = connect_signals;
}

static void
gdaui_entry_password_init (GdauiEntryPassword * gdaui_entry_password)
{
	gdaui_entry_password->priv = g_new0 (GdauiEntryPasswordPrivate, 1);
	gdaui_entry_password->priv->entry = NULL;
	gdaui_entry_password->priv->encoding_type = ENCODING_MD5;
	gdaui_entry_password->priv->needs_encoding = FALSE;
}

/**
 * gdaui_entry_password_new
 * @dh: the data handler to be used by the new widget
 * @type: the requested data type (compatible with @dh)
 *
 * Creates a new widget which is mainly a GtkEntry
 *
 * Returns: the new widget
 */
GtkWidget *
gdaui_entry_password_new (GdaDataHandler *dh, GType type, const gchar *options)
{
	GObject *obj;
	GdauiEntryPassword *mgtxt;

	g_return_val_if_fail (dh && GDA_IS_DATA_HANDLER (dh), NULL);
	g_return_val_if_fail (gda_data_handler_accepts_g_type (dh, type), NULL);

	obj = g_object_new (GDAUI_ENTRY_PASSWORD_TYPE, "handler", dh, NULL);
	mgtxt = GDAUI_ENTRY_PASSWORD (obj);
	gdaui_data_entry_set_value_type (GDAUI_DATA_ENTRY (mgtxt), type);

	if (options && *options) {
                GdaQuarkList *params;
                const gchar *str;

                params = gda_quark_list_new_from_string (options);
                str = gda_quark_list_find (params, "ENCODING");
                if (str) {
			mgtxt->priv->encoding_type = ENCODING_NONE;
                        if ((*str == 'M') || (*str == 'm'))
				mgtxt->priv->encoding_type = ENCODING_MD5;
                }
                gda_quark_list_free (params);
        }

	return GTK_WIDGET (obj);
}

static void
gdaui_entry_password_finalize (GObject   * object)
{
	GdauiEntryPassword *gdaui_entry_password;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GDAUI_IS_ENTRY_PASSWORD (object));

	gdaui_entry_password = GDAUI_ENTRY_PASSWORD (object);
	if (gdaui_entry_password->priv) {
		g_free (gdaui_entry_password->priv);
		gdaui_entry_password->priv = NULL;
	}

	/* parent class */
	parent_class->finalize (object);
}

static GtkWidget *
create_entry (GdauiEntryWrapper *mgwrap)
{
	GtkWidget *entry;
        GdauiEntryPassword *mgstr;

        g_return_val_if_fail (mgwrap && GDAUI_IS_ENTRY_PASSWORD (mgwrap), NULL);
        mgstr = GDAUI_ENTRY_PASSWORD (mgwrap);
        g_return_val_if_fail (mgstr->priv, NULL);

        entry = gtk_entry_new ();
        mgstr->priv->entry = entry;
	gtk_entry_set_visibility (GTK_ENTRY (entry), FALSE);

        return entry;
}

static void
real_set_value (GdauiEntryWrapper *mgwrap, const GValue *value)
{
	GdauiEntryPassword *mgstr;

        g_return_if_fail (mgwrap && GDAUI_IS_ENTRY_PASSWORD (mgwrap));
        mgstr = GDAUI_ENTRY_PASSWORD (mgwrap);
        g_return_if_fail (mgstr->priv);

	signal_handlers_block (mgstr);
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
	mgstr->priv->needs_encoding = FALSE;
	signal_handlers_unblock (mgstr);
}

static GValue *
real_get_value (GdauiEntryWrapper *mgwrap)
{
	GValue *value = NULL;
        GdauiEntryPassword *mgstr;
        GdaDataHandler *dh;
        const gchar *str;
	GType type;

        g_return_val_if_fail (mgwrap && GDAUI_IS_ENTRY_PASSWORD (mgwrap), NULL);
        mgstr = GDAUI_ENTRY_PASSWORD (mgwrap);
        g_return_val_if_fail (mgstr->priv, NULL);

        dh = gdaui_data_entry_get_handler (GDAUI_DATA_ENTRY (mgwrap));
        str = gtk_entry_get_text (GTK_ENTRY (mgstr->priv->entry));
	type = gdaui_data_entry_get_value_type (GDAUI_DATA_ENTRY (mgwrap));
	
	if (*str) {
		if (mgstr->priv->needs_encoding) {
			switch (mgstr->priv->encoding_type) {
			case ENCODING_MD5: {
				gcry_md_hd_t mdctx = NULL;
				unsigned char *md5str;
				int i;
				GString *md5pass;
				
				/* MD5 computation */
				gcry_md_open (&mdctx, GCRY_MD_MD5, 0);
				if (!mdctx) {
					value = NULL;
					break;
				}

				gcry_md_write (mdctx, str, strlen(str));
				md5str = gcry_md_read (mdctx, 0);
				
				md5pass = g_string_new ("");
				for (i = 0; i < 16; i++)
					g_string_append_printf (md5pass, "%02x", md5str[i]);
				value = gda_data_handler_get_value_from_str (dh, md5pass->str, type);

				g_string_free (md5pass, TRUE);
				gcry_md_close(mdctx);

				break;
			}
			case ENCODING_NONE:
				value = gda_data_handler_get_value_from_str (dh, str, type);
				break;
			default:
				g_assert_not_reached ();
				break;
			}
		}
		else 
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
	GdauiEntryPassword *mgstr;

        g_return_if_fail (mgwrap && GDAUI_IS_ENTRY_PASSWORD (mgwrap));
        mgstr = GDAUI_ENTRY_PASSWORD (mgwrap);
        g_return_if_fail (mgstr->priv);

        g_signal_connect (G_OBJECT (mgstr->priv->entry), "delete-text",
                          G_CALLBACK (entry_delete_text_cb), mgwrap);
        g_signal_connect (G_OBJECT (mgstr->priv->entry), "insert-text",
                          G_CALLBACK (entry_insert_text_cb), mgwrap);

        g_signal_connect_swapped (G_OBJECT (mgstr->priv->entry), "changed",
				modify_cb, mgwrap);
        g_signal_connect_swapped (G_OBJECT (mgstr->priv->entry), "activate",
				  activate_cb, mgwrap);
}

static void 
signal_handlers_block (GdauiEntryPassword *mgstr)
{
	g_signal_handlers_block_by_func (mgstr->priv->entry, G_CALLBACK (entry_insert_text_cb), mgstr);
	g_signal_handlers_block_by_func (mgstr->priv->entry, G_CALLBACK (entry_delete_text_cb), mgstr);
}

static void 
signal_handlers_unblock (GdauiEntryPassword *mgstr)
{
	g_signal_handlers_unblock_by_func (mgstr->priv->entry, G_CALLBACK (entry_insert_text_cb), mgstr);
	g_signal_handlers_unblock_by_func (mgstr->priv->entry, G_CALLBACK (entry_delete_text_cb), mgstr);
}

static void
entry_delete_text_cb (GtkEditable *editable, G_GNUC_UNUSED gint start_pos, G_GNUC_UNUSED gint end_pos,
		      GdauiEntryPassword *mgstr)
{
	if (!mgstr->priv->needs_encoding) {
		mgstr->priv->needs_encoding = TRUE;
		signal_handlers_block (mgstr);
		gtk_editable_delete_text (editable, 0, -1);
		signal_handlers_unblock (mgstr);
		g_signal_stop_emission_by_name (editable, "delete-text");
	}
}

static void
entry_insert_text_cb (GtkEditable *editable, const gchar *text, gint length, gint *position, GdauiEntryPassword *mgstr)
{
	if (!mgstr->priv->needs_encoding) {
		mgstr->priv->needs_encoding = TRUE;
		signal_handlers_block (mgstr);
		gtk_editable_delete_text (editable, 0, -1);
		gtk_editable_insert_text (editable, text, length, position);
		signal_handlers_unblock (mgstr);
		g_signal_stop_emission_by_name (editable, "insert-text");
	}
}
