/* gdaui-entry-bin.c
 *
 * Copyright (C) 2009 Vivien Malerba
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

#include <glib/gi18n-lib.h>
#include <libgda/binreloc/gda-binreloc.h>
#include "gdaui-entry-bin.h"
#include "common-bin.h"

/* 
 * Main static functions 
 */
static void gdaui_entry_bin_class_init (GdauiEntryBinClass * class);
static void gdaui_entry_bin_init (GdauiEntryBin * srv);
static void gdaui_entry_bin_dispose (GObject   * object);
static void gdaui_entry_bin_finalize (GObject   * object);

/* virtual functions */
static GtkWidget *create_entry (GdauiEntryWrapper *mgwrap);
static void       real_set_value (GdauiEntryWrapper *mgwrap, const GValue *value);
static GValue    *real_get_value (GdauiEntryWrapper *mgwrap);
static void       connect_signals(GdauiEntryWrapper *mgwrap, GCallback modify_cb, GCallback activate_cb);
static gboolean   can_expand (GdauiEntryWrapper *mgwrap, gboolean horiz);
static void       set_editable (GdauiEntryWrapper *mgwrap, gboolean editable);
static void       grab_focus (GdauiEntryWrapper *mgwrap);

static void       show (GtkWidget *widget);

/* get a pointer to the parents to be able to call their destructor */
static GObjectClass  *parent_class = NULL;
static GdkPixbuf *attach_pixbuf = NULL;

/* private structure */
struct _GdauiEntryBinPrivate
{
	GtkWidget *button;
	GtkWidget *button_hbox;
	GtkWidget *button_label; /* ref held! */
	GtkWidget *button_image; /* ref held! */

	BinMenu    menu;
	gboolean   editable;

	GValue    *current_data;
};


GType
gdaui_entry_bin_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo info = {
			sizeof (GdauiEntryBinClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gdaui_entry_bin_class_init,
			NULL,
			NULL,
			sizeof (GdauiEntryBin),
			0,
			(GInstanceInitFunc) gdaui_entry_bin_init
		};
		
		type = g_type_register_static (GDAUI_TYPE_ENTRY_WRAPPER, "GdauiEntryBin", &info, 0);
	}
	return type;
}

static void
gdaui_entry_bin_class_init (GdauiEntryBinClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);

	parent_class = g_type_class_peek_parent (class);

	object_class->dispose = gdaui_entry_bin_dispose;
	object_class->finalize = gdaui_entry_bin_finalize;

	GDAUI_ENTRY_WRAPPER_CLASS (class)->create_entry = create_entry;
	GDAUI_ENTRY_WRAPPER_CLASS (class)->real_set_value = real_set_value;
	GDAUI_ENTRY_WRAPPER_CLASS (class)->real_get_value = real_get_value;
	GDAUI_ENTRY_WRAPPER_CLASS (class)->connect_signals = connect_signals;
	GDAUI_ENTRY_WRAPPER_CLASS (class)->can_expand = can_expand;
	GDAUI_ENTRY_WRAPPER_CLASS (class)->set_editable = set_editable;
	GDAUI_ENTRY_WRAPPER_CLASS (class)->grab_focus = grab_focus;

	GTK_WIDGET_CLASS (class)->show = show;

	if (! attach_pixbuf) {
		gchar *tmp;
		tmp = gda_gbr_get_file_path (GDA_DATA_DIR, LIBGDA_ABI_NAME, "pixmaps", "bin-attachment-16x16.png", NULL);
		attach_pixbuf = gdk_pixbuf_new_from_file (tmp, NULL);
		if (!attach_pixbuf)
			g_warning ("Could not find icon file %s", tmp);
		g_free (tmp);
	}
}

static void
gdaui_entry_bin_init (GdauiEntryBin * gdaui_entry_bin)
{
	gdaui_entry_bin->priv = g_new0 (GdauiEntryBinPrivate, 1);
	gdaui_entry_bin->priv->button = NULL;
	gdaui_entry_bin->priv->current_data = NULL;
	gdaui_entry_bin->priv->editable = TRUE;
}

static void
show (GtkWidget *widget)
{
	GValue *value;
	GdauiEntryBin *dbin;

	((GtkWidgetClass *)parent_class)->show (widget);

	dbin = GDAUI_ENTRY_BIN (widget);
	value = dbin->priv->current_data;
	if (value && (G_VALUE_TYPE (value) != GDA_TYPE_NULL)) {
		gtk_widget_show (dbin->priv->button_image);
		gtk_widget_hide (dbin->priv->button_label);
	}
	else {
		gtk_widget_hide (dbin->priv->button_image);
		gtk_widget_show (dbin->priv->button_label);
	}
}

/**
 * gdaui_entry_bin_new:
 * @dh: the data handler to be used by the new widget
 * @type: the requested data type (compatible with @dh)
 *
 * Creates a new widget which is mainly a #GtkEntry
 *
 * Returns: (transfer full): the new widget
 */
GtkWidget *
gdaui_entry_bin_new (GdaDataHandler *dh, GType type)
{
	GObject *obj;
	GdauiEntryBin *dbin;

	g_return_val_if_fail (GDA_IS_DATA_HANDLER (dh), NULL);
	g_return_val_if_fail (type != G_TYPE_INVALID, NULL);
	g_return_val_if_fail (gda_data_handler_accepts_g_type (dh, type), NULL);

	obj = g_object_new (GDAUI_TYPE_ENTRY_BIN, "handler", dh, NULL);
	dbin = GDAUI_ENTRY_BIN (obj);
	gdaui_data_entry_set_value_type (GDAUI_DATA_ENTRY (dbin), type);

	return GTK_WIDGET (obj);
}


static void
gdaui_entry_bin_dispose (GObject   * object)
{
	GdauiEntryBin *gdaui_entry_bin;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GDAUI_IS_ENTRY_BIN (object));

	gdaui_entry_bin = GDAUI_ENTRY_BIN (object);
	if (gdaui_entry_bin->priv) {
		if (gdaui_entry_bin->priv->current_data) {
			gda_value_free (gdaui_entry_bin->priv->current_data);
			gdaui_entry_bin->priv->current_data = NULL;
		}
		common_bin_reset (&(gdaui_entry_bin->priv->menu));

		if (gdaui_entry_bin->priv->button_label) {
			g_object_unref (gdaui_entry_bin->priv->button_label);
			gdaui_entry_bin->priv->button_label = NULL;
		}

		if (gdaui_entry_bin->priv->button_image) {
			g_object_unref (gdaui_entry_bin->priv->button_image);
			gdaui_entry_bin->priv->button_image = NULL;
		}
	}

	/* parent class */
	parent_class->dispose (object);
}

static void
gdaui_entry_bin_finalize (GObject   * object)
{
	GdauiEntryBin *gdaui_entry_bin;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GDAUI_IS_ENTRY_BIN (object));

	gdaui_entry_bin = GDAUI_ENTRY_BIN (object);
	if (gdaui_entry_bin->priv) {

		g_free (gdaui_entry_bin->priv);
		gdaui_entry_bin->priv = NULL;
	}

	/* parent class */
	parent_class->finalize (object);
}

static GtkWidget *
create_entry (GdauiEntryWrapper *mgwrap)
{
	GtkWidget *button, *arrow, *label, *img;
	GdauiEntryBin *dbin;
	GtkWidget *hbox;

	g_return_val_if_fail (GDAUI_IS_ENTRY_BIN (mgwrap), NULL);
	dbin = GDAUI_ENTRY_BIN (mgwrap);
	g_return_val_if_fail (dbin->priv, NULL);

	button = gtk_button_new ();
	dbin->priv->button = button;

	hbox = gtk_hbox_new (FALSE, 0);
        gtk_container_add (GTK_CONTAINER (button), hbox);
	dbin->priv->button_hbox = hbox;

	label = gtk_label_new ("");
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
	dbin->priv->button_label = g_object_ref (G_OBJECT (label));

	img = gtk_image_new_from_pixbuf (attach_pixbuf);
	gtk_box_pack_start (GTK_BOX (hbox), img, FALSE, FALSE, 0);
	dbin->priv->button_image = g_object_ref (G_OBJECT (img));

        arrow = gtk_arrow_new (GTK_ARROW_DOWN, GTK_SHADOW_NONE);
	gtk_misc_set_alignment (GTK_MISC (arrow), 1.0, -1);
	gtk_box_pack_start (GTK_BOX (hbox), arrow, TRUE, TRUE, 0);

        gtk_widget_show_all (hbox);
	gtk_widget_hide (dbin->priv->button_label);

	return button;
}

/*
 * WARNING:
 * Does NOT emit any signal 
 */
static void
take_current_value (GdauiEntryBin *dbin, GValue *value)
{
	/* clear previous situation */
	if (dbin->priv->current_data) {
		gda_value_free (dbin->priv->current_data);
		dbin->priv->current_data = NULL;
	}

	/* new situation */
	dbin->priv->current_data = value;
	if (value && (G_VALUE_TYPE (value) != GDA_TYPE_NULL)) {
		gtk_widget_show (dbin->priv->button_image);
		gtk_widget_hide (dbin->priv->button_label);
	}
	else {
		gtk_widget_hide (dbin->priv->button_image);
		gtk_widget_show (dbin->priv->button_label);
	}

	common_bin_adjust_menu (&(dbin->priv->menu), dbin->priv->editable, value);
}

static void
real_set_value (GdauiEntryWrapper *mgwrap, const GValue *value)
{
	GdauiEntryBin *dbin;

	g_return_if_fail (GDAUI_IS_ENTRY_BIN (mgwrap));
	dbin = GDAUI_ENTRY_BIN (mgwrap);
	g_return_if_fail (dbin->priv);

	take_current_value (dbin, value ? gda_value_copy (value) : NULL);
}

static GValue *
real_get_value (GdauiEntryWrapper *mgwrap)
{
	GdauiEntryBin *dbin;

	g_return_val_if_fail (GDAUI_IS_ENTRY_BIN (mgwrap), NULL);
	dbin = GDAUI_ENTRY_BIN (mgwrap);
	g_return_val_if_fail (dbin->priv, NULL);

	if (dbin->priv->current_data)
		return gda_value_copy (dbin->priv->current_data);
	else
		return gda_value_new_null ();
}


static void
value_loaded_cb (GdauiEntryBin *dbin, GValue *new_value)
{
	take_current_value (dbin, new_value);

	/* signal changes */
	gdaui_entry_wrapper_contents_changed (GDAUI_ENTRY_WRAPPER (dbin));
	gdaui_entry_wrapper_contents_activated (GDAUI_ENTRY_WRAPPER (dbin));
}

static void
popup_position (PopupContainer *container, gint *out_x, gint *out_y)
{
	GtkWidget *poswidget;
	poswidget = g_object_get_data (G_OBJECT (container), "__poswidget");

	gint x, y;
        GtkRequisition req;

        gtk_widget_size_request (poswidget, &req);

#if GTK_CHECK_VERSION(2,18,0)
        gdk_window_get_origin (gtk_widget_get_window (poswidget), &x, &y);
	GtkAllocation alloc;
	gtk_widget_get_allocation (poswidget, &alloc);
        x += alloc.x;
        y += alloc.y;
        y += alloc.height;
#else
	gdk_window_get_origin (poswidget->window, &x, &y);
	x += poswidget->allocation.x;
        y += poswidget->allocation.y;
	y += poswidget->allocation.height;
#endif

        if (x < 0)
                x = 0;

        if (y < 0)
                y = 0;

	*out_x = x;
	*out_y = y;
}

static void
button_clicked_cb (GtkWidget *button, GdauiEntryBin *dbin)
{
	if (!dbin->priv->menu.popup) {
		common_bin_create_menu (&(dbin->priv->menu), popup_position,
					gdaui_data_entry_get_value_type (GDAUI_DATA_ENTRY (dbin)),
					(BinCallback) value_loaded_cb, dbin);
		g_object_set_data (G_OBJECT (dbin->priv->menu.popup), "__poswidget", button);
	}

	common_bin_adjust_menu (&(dbin->priv->menu), dbin->priv->editable, dbin->priv->current_data);
	gtk_widget_show (dbin->priv->menu.popup);
}

static void
connect_signals (GdauiEntryWrapper *mgwrap, GCallback modify_cb, GCallback activate_cb)
{
	GdauiEntryBin *dbin;

	g_return_if_fail (GDAUI_IS_ENTRY_BIN (mgwrap));
	dbin = GDAUI_ENTRY_BIN (mgwrap);
	g_return_if_fail (dbin->priv);

	g_signal_connect (G_OBJECT (dbin->priv->button), "clicked",
			  G_CALLBACK (button_clicked_cb), dbin);
}

static gboolean
can_expand (GdauiEntryWrapper *mgwrap, gboolean horiz)
{
	return FALSE;
}

static void
set_editable (GdauiEntryWrapper *mgwrap, gboolean editable)
{
	GdauiEntryBin *dbin;

	g_return_if_fail (GDAUI_IS_ENTRY_BIN (mgwrap));
	dbin = GDAUI_ENTRY_BIN (mgwrap);
	g_return_if_fail (dbin->priv);

	dbin->priv->editable = editable;
	common_bin_adjust_menu (&(dbin->priv->menu), editable, dbin->priv->current_data);
}

static void
grab_focus (GdauiEntryWrapper *mgwrap)
{
	GdauiEntryBin *dbin;

	g_return_if_fail (GDAUI_IS_ENTRY_BIN (mgwrap));
	dbin = GDAUI_ENTRY_BIN (mgwrap);
	g_return_if_fail (dbin->priv);

	gtk_widget_grab_focus (dbin->priv->button);
}
