  /*
 * Copyright (C) 2010 - 2015 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2011 Murray Cumming <murrayc@murrayc.com>
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

#include <string.h>
#include "gdaui-entry-rt.h"
#include <libgda-ui/gdaui-rt-editor.h>
#include <libgda/gda-data-handler.h>
#include <libgda/gda-blob-op.h>

/* 
 * Main static functions 
 */
static void gdaui_entry_rt_class_init (GdauiEntryRtClass * class);
static void gdaui_entry_rt_init (GdauiEntryRt * srv);
static void gdaui_entry_rt_dispose (GObject   * object);
static void gdaui_entry_rt_finalize (GObject   * object);

/* virtual functions */
static GtkWidget *create_entry (GdauiEntryWrapper *mgwrap);
static void       real_set_value (GdauiEntryWrapper *mgwrap, const GValue *value);
static GValue    *real_get_value (GdauiEntryWrapper *mgwrap);
static void       connect_signals(GdauiEntryWrapper *mgwrap, GCallback modify_cb, GCallback activate_cb);
static void       set_editable (GdauiEntryWrapper *mgwrap, gboolean editable);

/* get a pointer to the parents to be able to call their destructor */
static GObjectClass  *parent_class = NULL;

/* private structure */
struct _GdauiEntryRtPrivate
{
	GtkWidget     *view;
};


GType
gdaui_entry_rt_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo info = {
			sizeof (GdauiEntryRtClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gdaui_entry_rt_class_init,
			NULL,
			NULL,
			sizeof (GdauiEntryRt),
			0,
			(GInstanceInitFunc) gdaui_entry_rt_init,
			0
		};
		
		type = g_type_register_static (GDAUI_TYPE_ENTRY_WRAPPER, "GdauiEntryRt", &info, 0);
	}
	return type;
}

static void
gdaui_entry_rt_class_init (GdauiEntryRtClass * class)
{
	GObjectClass   *object_class = G_OBJECT_CLASS (class);

	parent_class = g_type_class_peek_parent (class);

	object_class->dispose = gdaui_entry_rt_dispose;
	object_class->finalize = gdaui_entry_rt_finalize;

	GDAUI_ENTRY_WRAPPER_CLASS (class)->create_entry = create_entry;
	GDAUI_ENTRY_WRAPPER_CLASS (class)->real_set_value = real_set_value;
	GDAUI_ENTRY_WRAPPER_CLASS (class)->real_get_value = real_get_value;
	GDAUI_ENTRY_WRAPPER_CLASS (class)->connect_signals = connect_signals;
	GDAUI_ENTRY_WRAPPER_CLASS (class)->set_editable = set_editable;
}

static void
gdaui_entry_rt_init (GdauiEntryRt *gdaui_entry_rt)
{
	gdaui_entry_rt->priv = g_new0 (GdauiEntryRtPrivate, 1);
	gdaui_entry_rt->priv->view = NULL;
	gtk_widget_set_vexpand (GTK_WIDGET (gdaui_entry_rt), TRUE);
}

/**
 * gdaui_entry_rt_new
 * @dh: the data handler to be used by the new widget
 * @type: the requested data type (compatible with @dh)
 * @options: the options
 *
 * Creates a new widget which is mainly a GtkEntry
 *
 * Returns: the new widget
 */
GtkWidget *
gdaui_entry_rt_new (GdaDataHandler *dh, GType type, G_GNUC_UNUSED const gchar *options)
{
	GObject *obj;

	g_return_val_if_fail (dh && GDA_IS_DATA_HANDLER (dh), NULL);
	g_return_val_if_fail (gda_data_handler_accepts_g_type (dh, type), NULL);

	obj = g_object_new (GDAUI_TYPE_ENTRY_RT, "handler", dh, NULL);
	gdaui_data_entry_set_value_type (GDAUI_DATA_ENTRY (obj), type);

	return GTK_WIDGET (obj);
}


static void
gdaui_entry_rt_dispose (GObject   * object)
{
	GdauiEntryRt *gdaui_entry_rt;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GDAUI_IS_ENTRY_RT (object));

	gdaui_entry_rt = GDAUI_ENTRY_RT (object);
	if (gdaui_entry_rt->priv) {
	}

	/* parent class */
	parent_class->dispose (object);
}

static void
gdaui_entry_rt_finalize (GObject   * object)
{
	GdauiEntryRt *gdaui_entry_rt;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GDAUI_IS_ENTRY_RT (object));

	gdaui_entry_rt = GDAUI_ENTRY_RT (object);
	if (gdaui_entry_rt->priv) {
		g_free (gdaui_entry_rt->priv);
		gdaui_entry_rt->priv = NULL;
	}

	/* parent class */
	parent_class->finalize (object);
}

static GtkWidget *
create_entry (GdauiEntryWrapper *mgwrap)
{
	GdauiEntryRt *mgtxt;

	g_return_val_if_fail (mgwrap && GDAUI_IS_ENTRY_RT (mgwrap), NULL);
	mgtxt = GDAUI_ENTRY_RT (mgwrap);
	g_return_val_if_fail (mgtxt->priv, NULL);

	mgtxt->priv->view = gdaui_rt_editor_new ();
	
	return mgtxt->priv->view;
}

static void
real_set_value (GdauiEntryWrapper *mgwrap, const GValue *value)
{
	GdauiEntryRt *mgtxt;

	g_return_if_fail (mgwrap && GDAUI_IS_ENTRY_RT (mgwrap));
	mgtxt = GDAUI_ENTRY_RT (mgwrap);
	g_return_if_fail (mgtxt->priv);

	gdaui_rt_editor_set_contents (GDAUI_RT_EDITOR (mgtxt->priv->view), "", -1);
	if (value) {
		if (! gda_value_is_null ((GValue *) value)) {
			GdaDataHandler *dh;		
			gchar *str;
			gboolean done = FALSE;

			if (G_VALUE_TYPE (value) == GDA_TYPE_BLOB) {
				GdaBlob *blob;
				GdaBinary *bin;
				blob = g_value_get_boxed (value);
				bin = gda_blob_get_binary (blob);
				if (gda_blob_get_op (blob) &&
				    (gda_binary_get_size (bin) != gda_blob_op_get_length (gda_blob_get_op (blob))))
                                        gda_blob_op_read_all (gda_blob_get_op (blob), (GdaBlob*) blob);
				if (g_utf8_validate ((gchar*) gda_binary_get_data (bin), gda_binary_get_size (bin), NULL)) {
					gdaui_rt_editor_set_contents (GDAUI_RT_EDITOR (mgtxt->priv->view),
								      (gchar*) gda_binary_get_data (bin),
								      gda_binary_get_size (bin));
					done = TRUE;
				}
			}
			else  if (G_VALUE_TYPE (value) == GDA_TYPE_BINARY) {
				GdaBinary *bin;
				bin = g_value_get_boxed (value);
				if (g_utf8_validate ((gchar*) gda_binary_get_data (bin), gda_binary_get_size (bin), NULL)) {
					gdaui_rt_editor_set_contents (GDAUI_RT_EDITOR (mgtxt->priv->view),
								      (gchar*) gda_binary_get_data (bin), 
								      gda_binary_get_size (bin));
					done = TRUE;
				}
			}

			if (!done) {
				dh = gdaui_data_entry_get_handler (GDAUI_DATA_ENTRY (mgwrap));
				str = gda_data_handler_get_str_from_value (dh, value);
				if (str) {
					gdaui_rt_editor_set_contents (GDAUI_RT_EDITOR (mgtxt->priv->view),
								      str, -1);
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
	GdauiEntryRt *mgtxt;
	GdaDataHandler *dh;
	gchar *str;

	g_return_val_if_fail (mgwrap && GDAUI_IS_ENTRY_RT (mgwrap), NULL);
	mgtxt = GDAUI_ENTRY_RT (mgwrap);
	g_return_val_if_fail (mgtxt->priv, NULL);

	dh = gdaui_data_entry_get_handler (GDAUI_DATA_ENTRY (mgwrap));
	str = gdaui_rt_editor_get_contents (GDAUI_RT_EDITOR (mgtxt->priv->view));
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
focus_out_cb (GtkWidget *widget, GdkEventFocus *event, GdauiEntryRt *mgtxt)
{
	GCallback activate_cb;
	activate_cb = g_object_get_data (G_OBJECT (widget), "_activate_cb");
	g_assert (activate_cb);
	((Callback2)activate_cb) (widget, mgtxt);

	return gtk_widget_event (GTK_WIDGET (mgtxt), (GdkEvent*) event);
}

static void
connect_signals(GdauiEntryWrapper *mgwrap, GCallback modify_cb, GCallback activate_cb)
{
	GdauiEntryRt *mgtxt;

	g_return_if_fail (mgwrap && GDAUI_IS_ENTRY_RT (mgwrap));
	mgtxt = GDAUI_ENTRY_RT (mgwrap);
	g_return_if_fail (mgtxt->priv);

	g_object_set_data (G_OBJECT (mgtxt->priv->view), "_activate_cb", activate_cb);
	g_signal_connect_swapped (G_OBJECT (GDAUI_RT_EDITOR (mgtxt->priv->view)), "changed",
				  modify_cb, mgwrap);
	g_signal_connect_swapped (G_OBJECT (mgtxt->priv->view), "focus-out-event",
				  G_CALLBACK (focus_out_cb), mgtxt);
	/* FIXME: how does the user "activates" the GtkRtView widget ? */
}

static void
set_editable (GdauiEntryWrapper *mgwrap, gboolean editable)
{
	GdauiEntryRt *mgtxt;

	g_return_if_fail (mgwrap && GDAUI_IS_ENTRY_RT (mgwrap));
	mgtxt = GDAUI_ENTRY_RT (mgwrap);

	gdaui_rt_editor_set_editable (GDAUI_RT_EDITOR (mgtxt->priv->view), editable);
}
