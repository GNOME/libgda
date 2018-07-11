/*
 * Copyright (C) 2009 - 2015 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2010 David King <davidk@openismus.com>
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

#include <glib/gi18n-lib.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>
#include <gdk-pixbuf/gdk-pixdata.h>
#include "gdaui-entry-pict.h"
#include "gdaui-entry-filesel.h"
#include <libgda/gda-data-handler.h>
#include <libgda/gda-blob-op.h>
#include <string.h>
#include <unistd.h>
#include <gdk/gdk.h>

#include "common-pict.h"

/* 
 * Main static functions 
 */
static void gdaui_entry_pict_class_init (GdauiEntryPictClass * class);
static void gdaui_entry_pict_init (GdauiEntryPict *srv);
static void gdaui_entry_pict_dispose (GObject *object);
static void gdaui_entry_pict_finalize (GObject *object);

/* virtual functions */
static GtkWidget *create_entry (GdauiEntryWrapper *mgwrap);
static void       real_set_value (GdauiEntryWrapper *mgwrap, const GValue *value);
static GValue    *real_get_value (GdauiEntryWrapper *mgwrap);
static void       connect_signals(GdauiEntryWrapper *mgwrap, GCallback modify_cb, GCallback activate_cb);
static void       set_editable (GdauiEntryWrapper *mgwrap, gboolean editable);
static gboolean   value_is_equal_to (GdauiEntryWrapper *mgwrap, const GValue *value);
static gboolean   value_is_null (GdauiEntryWrapper *mgwrap);

/* get a pointer to the parents to be able to call their destructor */
static GObjectClass  *parent_class = NULL;

/* private structure */
struct _GdauiEntryPictPrivate
{
	GtkWidget     *sw;
	GtkWidget     *pict;
	gboolean       editable;
	
	PictBinData    bindata;
	PictOptions    options;
	PictMenu       popup_menu;

	PictAllocation size;
};


GType
gdaui_entry_pict_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo info = {
			sizeof (GdauiEntryPictClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gdaui_entry_pict_class_init,
			NULL,
			NULL,
			sizeof (GdauiEntryPict),
			0,
			(GInstanceInitFunc) gdaui_entry_pict_init,
			0
		};
		
		type = g_type_register_static (GDAUI_TYPE_ENTRY_WRAPPER, "GdauiEntryPict", &info, 0);
	}
	return type;
}

static void
gdaui_entry_pict_class_init (GdauiEntryPictClass * class)
{
	GObjectClass   *object_class = G_OBJECT_CLASS (class);

	parent_class = g_type_class_peek_parent (class);

	object_class->dispose = gdaui_entry_pict_dispose;
	object_class->finalize = gdaui_entry_pict_finalize;

	GDAUI_ENTRY_WRAPPER_CLASS (class)->create_entry = create_entry;
	GDAUI_ENTRY_WRAPPER_CLASS (class)->real_set_value = real_set_value;
	GDAUI_ENTRY_WRAPPER_CLASS (class)->real_get_value = real_get_value;
	GDAUI_ENTRY_WRAPPER_CLASS (class)->connect_signals = connect_signals;
	GDAUI_ENTRY_WRAPPER_CLASS (class)->set_editable = set_editable;
	GDAUI_ENTRY_WRAPPER_CLASS (class)->value_is_equal_to = value_is_equal_to;
	GDAUI_ENTRY_WRAPPER_CLASS (class)->value_is_null = value_is_null;
}

static void
gdaui_entry_pict_init (GdauiEntryPict *gdaui_entry_pict)
{
	gdaui_entry_pict->priv = g_new0 (GdauiEntryPictPrivate, 1);
	gdaui_entry_pict->priv->pict = NULL;
	gdaui_entry_pict->priv->bindata.data = NULL;
	gdaui_entry_pict->priv->bindata.data_length = 0;
	gdaui_entry_pict->priv->options.encoding = ENCODING_NONE;
	common_pict_init_cache (&gdaui_entry_pict->priv->options);
	gdaui_entry_pict->priv->editable = TRUE;
	gdaui_entry_pict->priv->size.width = 0;
	gdaui_entry_pict->priv->size.height = 0;
	gtk_widget_set_vexpand (GTK_WIDGET (gdaui_entry_pict), TRUE);
}

/**
 * gdaui_entry_pict_new
 * @dh: the data handler to be used by the new widget
 * @type: the requested data type (compatible with @dh)
 * @options: optional parameters
 *
 * Creates a new widget which is mainly a GtkEntry
 *
 * Returns: the new widget
 */
GtkWidget *
gdaui_entry_pict_new (GdaDataHandler *dh, GType type, const gchar *options)
{
	GObject *obj;
	GdauiEntryPict *mgpict;

	g_return_val_if_fail (GDA_IS_DATA_HANDLER (dh), NULL);
	g_return_val_if_fail (gda_data_handler_accepts_g_type (dh, type), NULL);

	obj = g_object_new (GDAUI_TYPE_ENTRY_PICT, "handler", dh, NULL);
	mgpict = GDAUI_ENTRY_PICT (obj);
	gdaui_data_entry_set_value_type (GDAUI_DATA_ENTRY (mgpict), type);

	common_pict_parse_options (&(mgpict->priv->options), options);

	return GTK_WIDGET (obj);
}


static void
gdaui_entry_pict_dispose (GObject   * object)
{
	GdauiEntryPict *mgpict;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GDAUI_IS_ENTRY_PICT (object));

	mgpict = GDAUI_ENTRY_PICT (object);
	if (mgpict->priv) {
		if (mgpict->priv->options.pixbuf_hash) {
			g_hash_table_destroy (mgpict->priv->options.pixbuf_hash);
			mgpict->priv->options.pixbuf_hash = NULL;
		}

		if (mgpict->priv->bindata.data) {
			g_free (mgpict->priv->bindata.data);
			mgpict->priv->bindata.data = NULL;
			mgpict->priv->bindata.data_length = 0;
		}

		if (mgpict->priv->popup_menu.menu) {
			gtk_widget_destroy (mgpict->priv->popup_menu.menu);
			mgpict->priv->popup_menu.menu = NULL;
		}
	}

	/* parent class */
	parent_class->dispose (object);
}

static void
gdaui_entry_pict_finalize (GObject   * object)
{
	GdauiEntryPict *mgpict;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GDAUI_IS_ENTRY_PICT (object));

	mgpict = GDAUI_ENTRY_PICT (object);
	if (mgpict->priv) {
		g_free (mgpict->priv);
		mgpict->priv = NULL;
	}

	/* parent class */
	parent_class->finalize (object);
}

static void display_image (GdauiEntryPict *mgpict, const GValue *value, const gchar *error_icon_name, const gchar *notice);
static gboolean popup_menu_cb (GtkWidget *button, GdauiEntryPict *mgpict);
static gboolean event_cb (GtkWidget *button, GdkEvent *event, GdauiEntryPict *mgpict);
static void size_allocate_cb (GtkWidget *wid, GtkAllocation *allocation, GdauiEntryPict *mgpict);

static void
realize_cb (GdauiEntryPict *mgpict, G_GNUC_UNUSED GdauiEntryWrapper *mgwrap)
{
	display_image (mgpict, NULL, NULL, NULL);
}

static GtkWidget *
create_entry (GdauiEntryWrapper *mgwrap)
{
	GtkWidget *vbox, *wid;
	GdauiEntryPict *mgpict;

	g_return_val_if_fail (GDAUI_IS_ENTRY_PICT (mgwrap), NULL);
	mgpict = GDAUI_ENTRY_PICT (mgwrap);
	g_return_val_if_fail (mgpict->priv, NULL);

	vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);

	/* sw */
	wid = gtk_scrolled_window_new (NULL, NULL);
	gtk_box_pack_start (GTK_BOX (vbox), wid, TRUE, TRUE, 0);
	gtk_widget_show (wid);
	mgpict->priv->sw = wid;
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (wid), 
					GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (wid), GTK_SHADOW_NONE);
	g_signal_connect (G_OBJECT (mgpict->priv->sw), "size-allocate",
			  G_CALLBACK (size_allocate_cb), mgpict);

	/* image */
	GtkWidget *evbox;
	evbox = gtk_event_box_new ();
	gtk_container_add (GTK_CONTAINER (mgpict->priv->sw), evbox);
	wid = gtk_image_new ();
	gtk_widget_set_valign (wid, GTK_ALIGN_CENTER);
	gtk_widget_set_halign (wid, GTK_ALIGN_START);
	gtk_container_add (GTK_CONTAINER (evbox), wid);
	gtk_widget_show_all (evbox);
	mgpict->priv->pict = wid;

	wid = gtk_bin_get_child (GTK_BIN (mgpict->priv->sw));
	gtk_viewport_set_shadow_type (GTK_VIEWPORT (wid), GTK_SHADOW_NONE);

	/* connect signals for popup menu */
	g_signal_connect (G_OBJECT (mgpict), "popup-menu",
			  G_CALLBACK (popup_menu_cb), mgpict);
	gtk_widget_add_events (evbox, GDK_BUTTON_PRESS_MASK);
	g_signal_connect (G_OBJECT (mgpict), "event",
			  G_CALLBACK (event_cb), mgpict);

	display_image (mgpict, NULL, "image-missing", _("No data to display"));

	g_signal_connect (G_OBJECT (mgpict), "realize",
			  G_CALLBACK (realize_cb), mgwrap);

	return vbox;
}

static void
size_allocate_cb (G_GNUC_UNUSED GtkWidget *wid, GtkAllocation *allocation, GdauiEntryPict *mgpict)
{
	if ((mgpict->priv->size.width != allocation->width) ||
	    (mgpict->priv->size.height != allocation->height)) {
		mgpict->priv->size.width = allocation->width;
		mgpict->priv->size.height = allocation->height;
		common_pict_clear_pixbuf_cache (&(mgpict->priv->options));
		display_image (mgpict, NULL, NULL, NULL);
	}
}

static void
pict_data_changed_cb (PictBinData *bindata, GdauiEntryPict *mgpict)
{
	g_free (mgpict->priv->bindata.data);
	mgpict->priv->bindata.data = bindata->data;
	mgpict->priv->bindata.data_length = bindata->data_length;
	display_image (mgpict, NULL, NULL, NULL);
	gdaui_entry_wrapper_contents_changed (GDAUI_ENTRY_WRAPPER (mgpict));
	gdaui_entry_wrapper_contents_activated (GDAUI_ENTRY_WRAPPER (mgpict));
}

static void
do_popup_menu (GtkWidget *widget, GdkEventButton *event, GdauiEntryPict *mgpict)
{
	if (mgpict->priv->popup_menu.menu) {
		gtk_widget_destroy (mgpict->priv->popup_menu.menu);
		mgpict->priv->popup_menu.menu = NULL;
	}
	common_pict_create_menu (&(mgpict->priv->popup_menu), widget, &(mgpict->priv->bindata), 
				 &(mgpict->priv->options), 
				 (PictCallback) pict_data_changed_cb, mgpict);

	common_pict_adjust_menu_sensitiveness (&(mgpict->priv->popup_menu), mgpict->priv->editable, 
						       &(mgpict->priv->bindata));

		gtk_menu_popup_at_pointer (GTK_MENU (mgpict->priv->popup_menu.menu), NULL);
}

static gboolean
popup_menu_cb (GtkWidget *widget, GdauiEntryPict *mgpict)
{
	do_popup_menu (widget, NULL, mgpict);
	return TRUE;
}

static gboolean
event_cb (GtkWidget *widget, GdkEvent *event, GdauiEntryPict *mgpict)
{
	if ((event->type == GDK_BUTTON_PRESS) && (((GdkEventButton *) event)->button == 3)) {
		do_popup_menu (widget, (GdkEventButton *) event, mgpict);
		return TRUE;
	}
	if ((event->type == GDK_2BUTTON_PRESS) && (((GdkEventButton *) event)->button == 1)) {
		if (mgpict->priv->editable) {
			if (mgpict->priv->popup_menu.menu) {
				gtk_widget_destroy (mgpict->priv->popup_menu.menu);
				mgpict->priv->popup_menu.menu = NULL;
			}
			common_pict_create_menu (&(mgpict->priv->popup_menu), widget, &(mgpict->priv->bindata), 
						 &(mgpict->priv->options), 
						 (PictCallback) pict_data_changed_cb, mgpict);
			
			common_pict_adjust_menu_sensitiveness (&(mgpict->priv->popup_menu), mgpict->priv->editable, 
							       &(mgpict->priv->bindata));
			
			gtk_menu_item_activate (GTK_MENU_ITEM (mgpict->priv->popup_menu.load_mitem));
			return TRUE;
		}
	}
	
	return FALSE;
}

static void
real_set_value (GdauiEntryWrapper *mgwrap, const GValue *value)
{
	GdauiEntryPict *mgpict;
	const gchar *icon_name = NULL;
	gchar *notice_msg = NULL;
	GError *error = NULL;

	g_return_if_fail (GDAUI_IS_ENTRY_PICT (mgwrap));
	mgpict = GDAUI_ENTRY_PICT (mgwrap);
	g_return_if_fail (mgpict->priv);

	g_free (mgpict->priv->bindata.data);
	mgpict->priv->bindata.data = NULL;
	mgpict->priv->bindata.data_length = 0;

	/* fill in mgpict->priv->data */
	if (!common_pict_load_data (&(mgpict->priv->options), value, &(mgpict->priv->bindata), &icon_name, &error)) {
		notice_msg = g_strdup (error->message ? error->message : "");
		g_error_free (error);
	}

	/* create (if possible) a pixbuf from mgpict->priv->bindata.data */
	display_image (mgpict, value, icon_name, notice_msg);
	g_free (notice_msg);
}

static void 
display_image (GdauiEntryPict *mgpict, const GValue *value, const gchar *error_icon_name, const gchar *notice)
{
	const gchar *icon_name = error_icon_name;
	gchar *notice_msg = NULL;
	GdkPixbuf *pixbuf;
	PictAllocation alloc;
	GError *error = NULL;

	GtkAllocation walloc;
	gtk_widget_get_allocation (mgpict->priv->sw, &walloc);
	alloc.width = walloc.width;
	alloc.height = walloc.height;
	alloc.width = MAX (alloc.width, 10);
	alloc.height = MAX (alloc.height, 10);

	pixbuf = common_pict_fetch_cached_pixbuf (&(mgpict->priv->options), value);
	if (pixbuf)
		g_object_ref (pixbuf);
	else {
		pixbuf = common_pict_make_pixbuf (&(mgpict->priv->options), &(mgpict->priv->bindata), &alloc, 
						  &icon_name, &error);
		if (pixbuf) 
			common_pict_add_cached_pixbuf (&(mgpict->priv->options), value, pixbuf);
	}

	if (pixbuf) {
		gtk_image_set_from_pixbuf (GTK_IMAGE (mgpict->priv->pict), pixbuf);
		g_object_unref (pixbuf);
	}
	else {
		if (error) {
			notice_msg = g_strdup (error->message ? error->message : "");
			g_error_free (error);
		}
		else {
			icon_name = "image-missing";
			notice_msg = g_strdup (_("Unspecified"));
		}
	}

	if (icon_name)
		gtk_image_set_from_icon_name (GTK_IMAGE (mgpict->priv->pict), 
					      icon_name, GTK_ICON_SIZE_DIALOG);
	gtk_widget_set_tooltip_text (mgpict->priv->pict, notice ? notice : notice_msg);
	g_free (notice_msg);

	common_pict_adjust_menu_sensitiveness (&(mgpict->priv->popup_menu), mgpict->priv->editable, &(mgpict->priv->bindata));
	gtk_widget_queue_resize ((GtkWidget *) mgpict);
}

static GValue *
real_get_value (GdauiEntryWrapper *mgwrap)
{
	GdauiEntryPict *mgpict;

	g_return_val_if_fail (GDAUI_IS_ENTRY_PICT (mgwrap), NULL);
	mgpict = GDAUI_ENTRY_PICT (mgwrap);
	g_return_val_if_fail (mgpict->priv, NULL);

	return common_pict_get_value (&(mgpict->priv->bindata), &(mgpict->priv->options), 
				       gdaui_data_entry_get_value_type (GDAUI_DATA_ENTRY (mgpict)));
}

static void
connect_signals(G_GNUC_UNUSED GdauiEntryWrapper *mgwrap, G_GNUC_UNUSED GCallback modify_cb,
		G_GNUC_UNUSED GCallback activate_cb)
{
	/* do nothing because we manually call gdaui_entry_wrapper_contents_changed() */
}

static void
set_editable (GdauiEntryWrapper *mgwrap, gboolean editable)
{
	GdauiEntryPict *mgpict;

	g_return_if_fail (GDAUI_IS_ENTRY_PICT (mgwrap));
	mgpict = GDAUI_ENTRY_PICT (mgwrap);
	g_return_if_fail (mgpict->priv);
	
	mgpict->priv->editable = editable;
	common_pict_adjust_menu_sensitiveness (&(mgpict->priv->popup_menu),
					       mgpict->priv->editable, &(mgpict->priv->bindata));
}

static gboolean
value_is_equal_to (GdauiEntryWrapper *mgwrap, const GValue *value)
{
	GdauiEntryPict *mgpict;

	g_return_val_if_fail (GDAUI_IS_ENTRY_PICT (mgwrap), FALSE);
	mgpict = GDAUI_ENTRY_PICT (mgwrap);
	g_return_val_if_fail (mgpict->priv, FALSE);
	
	if (value) {
		if (gda_value_is_null (value) && !mgpict->priv->bindata.data)
			return TRUE;
		if (G_VALUE_TYPE (value) == GDA_TYPE_BLOB) {
			GdaBlob *blob;
			GdaBinary *bin;

			blob = (GdaBlob*) gda_value_get_blob ((GValue *) value);
			g_assert (blob);
			bin = gda_blob_get_binary (blob);
			if (gda_blob_get_op (blob) &&
			    (gda_binary_get_size (bin) != gda_blob_op_get_length (gda_blob_get_op (blob))))
				gda_blob_op_read_all (gda_blob_get_op (blob), blob);
			if (mgpict->priv->bindata.data)
				return !memcmp (gda_binary_get_data (bin), mgpict->priv->bindata.data, MIN (mgpict->priv->bindata.data_length, gda_binary_get_size (bin)));
			else
				return FALSE;
		}
		if (G_VALUE_TYPE (value) == GDA_TYPE_BINARY) {
			GdaBinary *bin;

			bin = (GdaBinary *) gda_value_get_binary ((GValue *) value);
			if (bin && mgpict->priv->bindata.data)
				return !memcmp (gda_binary_get_data (bin), mgpict->priv->bindata.data, MIN (mgpict->priv->bindata.data_length, gda_binary_get_size (bin)));
			else
				return FALSE;
		}
		if (G_VALUE_TYPE (value) == G_TYPE_STRING) {
			const gchar *cmpstr;
			gchar *curstr = NULL;
			gboolean res;

			cmpstr = g_value_get_string (value);
			switch (mgpict->priv->options.encoding) {
			case ENCODING_NONE:
				curstr = g_strndup ((gchar *) mgpict->priv->bindata.data, 
						    mgpict->priv->bindata.data_length);
				break;
			case ENCODING_BASE64: 
				curstr = g_base64_encode (mgpict->priv->bindata.data,
							  mgpict->priv->bindata.data_length);
				break;
			default:
				g_assert_not_reached ();
			}
			res = strcmp (curstr, cmpstr) == 0 ? TRUE : FALSE;
			g_free (curstr);
			return res;
		}
		return FALSE;
	}
	else {
		if (mgpict->priv->bindata.data)
			return TRUE;
		else
			return FALSE;
	}
	
	return FALSE;
}

static gboolean
value_is_null (GdauiEntryWrapper *mgwrap)
{
	GdauiEntryPict *mgpict;

	g_return_val_if_fail (GDAUI_IS_ENTRY_PICT (mgwrap), TRUE);
	mgpict = GDAUI_ENTRY_PICT (mgwrap);
	g_return_val_if_fail (mgpict->priv, TRUE);

	return mgpict->priv->bindata.data ? FALSE : TRUE;
}
