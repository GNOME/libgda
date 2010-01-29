/* gdaui-entry-text.c
 *
 * Copyright (C) 2003 - 2005 Vivien Malerba
 *
 * This Library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

#include "gdaui-entry-text.h"
#include <libgda/gda-data-handler.h>

/* 
 * Main static functions 
 */
static void gdaui_entry_text_class_init (GdauiEntryTextClass * class);
static void gdaui_entry_text_init (GdauiEntryText * srv);
static void gdaui_entry_text_dispose (GObject   * object);
static void gdaui_entry_text_finalize (GObject   * object);

/* virtual functions */
static GtkWidget *create_entry (GdauiEntryWrapper *mgwrap);
static void       real_set_value (GdauiEntryWrapper *mgwrap, const GValue *value);
static GValue    *real_get_value (GdauiEntryWrapper *mgwrap);
static void       connect_signals(GdauiEntryWrapper *mgwrap, GCallback modify_cb, GCallback activate_cb);
static gboolean   expand_in_layout (GdauiEntryWrapper *mgwrap);
static void       set_editable (GdauiEntryWrapper *mgwrap, gboolean editable);

/* get a pointer to the parents to be able to call their destructor */
static GObjectClass  *parent_class = NULL;

/* private structure */
struct _GdauiEntryTextPrivate
{
	GtkTextBuffer *buffer;
	GtkWidget     *view;
};


GType
gdaui_entry_text_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo info = {
			sizeof (GdauiEntryTextClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gdaui_entry_text_class_init,
			NULL,
			NULL,
			sizeof (GdauiEntryText),
			0,
			(GInstanceInitFunc) gdaui_entry_text_init
		};
		
		type = g_type_register_static (GDAUI_TYPE_ENTRY_WRAPPER, "GdauiEntryText", &info, 0);
	}
	return type;
}

static void
gdaui_entry_text_class_init (GdauiEntryTextClass * class)
{
	GObjectClass   *object_class = G_OBJECT_CLASS (class);

	parent_class = g_type_class_peek_parent (class);

	object_class->dispose = gdaui_entry_text_dispose;
	object_class->finalize = gdaui_entry_text_finalize;

	GDAUI_ENTRY_WRAPPER_CLASS (class)->create_entry = create_entry;
	GDAUI_ENTRY_WRAPPER_CLASS (class)->real_set_value = real_set_value;
	GDAUI_ENTRY_WRAPPER_CLASS (class)->real_get_value = real_get_value;
	GDAUI_ENTRY_WRAPPER_CLASS (class)->connect_signals = connect_signals;
	GDAUI_ENTRY_WRAPPER_CLASS (class)->expand_in_layout = expand_in_layout;
	GDAUI_ENTRY_WRAPPER_CLASS (class)->set_editable = set_editable;
}

static void
gdaui_entry_text_init (GdauiEntryText * gdaui_entry_text)
{
	gdaui_entry_text->priv = g_new0 (GdauiEntryTextPrivate, 1);
	gdaui_entry_text->priv->buffer = NULL;
	gdaui_entry_text->priv->view = NULL;
}

/**
 * gdaui_entry_text_new
 * @dh: the data handler to be used by the new widget
 * @type: the requested data type (compatible with @dh)
 *
 * Creates a new widget which is mainly a GtkEntry
 *
 * Returns: the new widget
 */
GtkWidget *
gdaui_entry_text_new (GdaDataHandler *dh, GType type)
{
	GObject *obj;
	GdauiEntryText *mgtxt;

	g_return_val_if_fail (dh && GDA_IS_DATA_HANDLER (dh), NULL);
	g_return_val_if_fail (type != G_TYPE_INVALID, NULL);
	g_return_val_if_fail (gda_data_handler_accepts_g_type (dh, type), NULL);

	obj = g_object_new (GDAUI_TYPE_ENTRY_TEXT, "handler", dh, NULL);
	mgtxt = GDAUI_ENTRY_TEXT (obj);
	gdaui_data_entry_set_value_type (GDAUI_DATA_ENTRY (mgtxt), type);

	return GTK_WIDGET (obj);
}


static void
gdaui_entry_text_dispose (GObject   * object)
{
	GdauiEntryText *gdaui_entry_text;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GDAUI_IS_ENTRY_TEXT (object));

	gdaui_entry_text = GDAUI_ENTRY_TEXT (object);
	if (gdaui_entry_text->priv) {

	}

	/* parent class */
	parent_class->dispose (object);
}

static void
gdaui_entry_text_finalize (GObject   * object)
{
	GdauiEntryText *gdaui_entry_text;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GDAUI_IS_ENTRY_TEXT (object));

	gdaui_entry_text = GDAUI_ENTRY_TEXT (object);
	if (gdaui_entry_text->priv) {

		g_free (gdaui_entry_text->priv);
		gdaui_entry_text->priv = NULL;
	}

	/* parent class */
	parent_class->finalize (object);
}

static GtkWidget *
create_entry (GdauiEntryWrapper *mgwrap)
{
	GdauiEntryText *mgtxt;
	GtkWidget *sw;

	g_return_val_if_fail (mgwrap && GDAUI_IS_ENTRY_TEXT (mgwrap), NULL);
	mgtxt = GDAUI_ENTRY_TEXT (mgwrap);
	g_return_val_if_fail (mgtxt->priv, NULL);

	mgtxt->priv->view = gtk_text_view_new ();
	mgtxt->priv->buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (mgtxt->priv->view));
	sw = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw), GTK_SHADOW_IN);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);
	gtk_container_add (GTK_CONTAINER (sw), mgtxt->priv->view);
	gtk_widget_show (mgtxt->priv->view);
	
	return sw;
}

static void
real_set_value (GdauiEntryWrapper *mgwrap, const GValue *value)
{
	GdauiEntryText *mgtxt;

	g_return_if_fail (mgwrap && GDAUI_IS_ENTRY_TEXT (mgwrap));
	mgtxt = GDAUI_ENTRY_TEXT (mgwrap);
	g_return_if_fail (mgtxt->priv);

	gtk_text_buffer_set_text (mgtxt->priv->buffer, "", -1);
	if (value) {
		if (! gda_value_is_null ((GValue *) value)) {
			GdaDataHandler *dh;		
			gchar *str;
			gboolean done = FALSE;

			if (G_VALUE_TYPE (value) == GDA_TYPE_BLOB) {
				const GdaBlob *blob;
				GdaBinary *bin;
				blob = gda_value_get_blob (value);
				bin = (GdaBinary *) blob;
				if (blob->op &&
				    (bin->binary_length != gda_blob_op_get_length (blob->op)))
                                        gda_blob_op_read_all (blob->op, blob);
				if (g_utf8_validate (bin->data, bin->binary_length, NULL)) {
					gtk_text_buffer_set_text (mgtxt->priv->buffer, (gchar*) bin->data, 
								  bin->binary_length);
					done = TRUE;
				}
			}
			else  if (G_VALUE_TYPE (value) == GDA_TYPE_BINARY) {
				const GdaBinary *bin;
				bin = gda_value_get_binary (value);
				if (g_utf8_validate (bin->data, bin->binary_length, NULL)) {
					gtk_text_buffer_set_text (mgtxt->priv->buffer, (gchar*) bin->data, 
								  bin->binary_length);
					done = TRUE;
				}
			}

			if (!done) {
				dh = gdaui_data_entry_get_handler (GDAUI_DATA_ENTRY (mgwrap));
				str = gda_data_handler_get_str_from_value (dh, value);
				if (str) {
					gtk_text_buffer_set_text (mgtxt->priv->buffer, str, -1);
					g_free (str);
				}
			}
		}
	}
}

static GValue *
real_get_value (GdauiEntryWrapper *mgwrap)
{
	GValue *value;
	GdauiEntryText *mgtxt;
	GdaDataHandler *dh;
	gchar *str;
	GtkTextIter start, end;

	g_return_val_if_fail (mgwrap && GDAUI_IS_ENTRY_TEXT (mgwrap), NULL);
	mgtxt = GDAUI_ENTRY_TEXT (mgwrap);
	g_return_val_if_fail (mgtxt->priv, NULL);

	dh = gdaui_data_entry_get_handler (GDAUI_DATA_ENTRY (mgwrap));
	gtk_text_buffer_get_start_iter (mgtxt->priv->buffer, &start);
	gtk_text_buffer_get_end_iter (mgtxt->priv->buffer, &end);
	str = gtk_text_buffer_get_text (mgtxt->priv->buffer, &start, &end, FALSE);
	value = gda_data_handler_get_value_from_str (dh, str, 
						     gdaui_data_entry_get_value_type (GDAUI_DATA_ENTRY (mgwrap)));
	g_free (str);
	if (!value) {
		/* in case the gda_data_handler_get_value_from_sql() returned an error because
		   the contents of the GtkEntry cannot be interpreted as a GValue */
		value = gda_value_new_null ();
	}

	return value;
}

typedef void (*Callback2) (gpointer, gpointer);
static gboolean
focus_out_cb (GtkWidget *widget, GdkEventFocus *event, GdauiEntryText *mgtxt)
{
	GCallback activate_cb;
	activate_cb = g_object_get_data (G_OBJECT (widget), "_activate_cb");
	g_assert (activate_cb);
	((Callback2)activate_cb) (widget, mgtxt);

	return FALSE;
}

static void
connect_signals(GdauiEntryWrapper *mgwrap, GCallback modify_cb, GCallback activate_cb)
{
	GdauiEntryText *mgtxt;

	g_return_if_fail (mgwrap && GDAUI_IS_ENTRY_TEXT (mgwrap));
	mgtxt = GDAUI_ENTRY_TEXT (mgwrap);
	g_return_if_fail (mgtxt->priv);

	g_object_set_data (G_OBJECT (mgtxt->priv->view), "_activate_cb", activate_cb);
	g_signal_connect (G_OBJECT (mgtxt->priv->buffer), "changed",
			  modify_cb, mgwrap);
	g_signal_connect (G_OBJECT (mgtxt->priv->view), "focus-out-event",
			  G_CALLBACK (focus_out_cb), mgtxt);
	/* FIXME: how does the user "activates" the GtkTextView widget ? */
}

static gboolean
expand_in_layout (GdauiEntryWrapper *mgwrap)
{
	return TRUE;
}

static void
set_editable (GdauiEntryWrapper *mgwrap, gboolean editable)
{
	GdauiEntryText *mgtxt;

	g_return_if_fail (mgwrap && GDAUI_IS_ENTRY_TEXT (mgwrap));
	mgtxt = GDAUI_ENTRY_TEXT (mgwrap);

	gtk_widget_set_sensitive (mgtxt->priv->view, editable);
}
