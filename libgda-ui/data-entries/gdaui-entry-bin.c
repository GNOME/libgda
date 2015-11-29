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
static void       set_editable (GdauiEntryWrapper *mgwrap, gboolean editable);
static void       grab_focus (GdauiEntryWrapper *mgwrap);

/* get a pointer to the parents to be able to call their destructor */
static GObjectClass  *parent_class = NULL;

/* private structure */
struct _GdauiEntryBinPrivate
{
	GtkWidget *label;

	BinMenu    commonbin;
	GtkWidget *entry_widget;
	gboolean   editable;
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
			(GInstanceInitFunc) gdaui_entry_bin_init,
			0
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
	GDAUI_ENTRY_WRAPPER_CLASS (class)->set_editable = set_editable;
	GDAUI_ENTRY_WRAPPER_CLASS (class)->grab_focus = grab_focus;
}

static void
gdaui_entry_bin_init (GdauiEntryBin * gdaui_entry_bin)
{
	gdaui_entry_bin->priv = g_new0 (GdauiEntryBinPrivate, 1);
	gdaui_entry_bin->priv->label = NULL;
	gdaui_entry_bin->priv->editable = TRUE;
	common_bin_init (&(gdaui_entry_bin->priv->commonbin));
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
	if (gdaui_entry_bin->priv)
		common_bin_reset (&(gdaui_entry_bin->priv->commonbin));

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

/*
 * Steals @value
 *
 * WARNING:
 * Does NOT emit any signal 
 */
static void
take_as_current_value (GdauiEntryBin *dbin, GValue *value)
{
	common_bin_adjust (&(dbin->priv->commonbin), dbin->priv->editable, value);

	gchar *str;
	str = common_bin_get_description (&(dbin->priv->commonbin));
	gchar *markup;
	if (str) {
		markup = g_markup_printf_escaped ("<a href=''>%s</a>", str);
		g_free (str);
	}
	else
		markup = g_markup_printf_escaped ("<a href=''><i>%s</i></a>", _("No data"));
	gtk_label_set_markup (GTK_LABEL (dbin->priv->label), markup);
	g_free (markup);
}

static void
event_after_cb (GtkWidget *widget, GdkEvent *event, GdauiEntryBin *dbin)
{
	/* don't "forward" event if popover is shown */ 
	if (!dbin->priv->commonbin.popover || !gtk_widget_is_visible (dbin->priv->commonbin.popover))
		g_signal_emit_by_name (dbin->priv->entry_widget, "event-after", event);
}

static GtkWidget *
create_entry (GdauiEntryWrapper *mgwrap)
{
	GtkWidget *label;
	GdauiEntryBin *dbin;
	GtkWidget *hbox;

	g_return_val_if_fail (GDAUI_IS_ENTRY_BIN (mgwrap), NULL);
	dbin = GDAUI_ENTRY_BIN (mgwrap);
	g_return_val_if_fail (dbin->priv, NULL);

	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
	dbin->priv->entry_widget = hbox;

	label = gtk_label_new ("");
	gtk_widget_set_halign (label, GTK_ALIGN_START);
	gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, TRUE, 0);
	gtk_widget_show (label);
	dbin->priv->label = label;

	g_signal_connect (label, "event-after",
			  G_CALLBACK (event_after_cb), dbin);

	take_as_current_value (dbin, NULL);

	return hbox;
}

static void
real_set_value (GdauiEntryWrapper *mgwrap, const GValue *value)
{
	GdauiEntryBin *dbin;

	g_return_if_fail (GDAUI_IS_ENTRY_BIN (mgwrap));
	dbin = GDAUI_ENTRY_BIN (mgwrap);
	g_return_if_fail (dbin->priv);

	take_as_current_value (dbin, value ? gda_value_copy (value) : NULL);
}

static GValue *
real_get_value (GdauiEntryWrapper *mgwrap)
{
	GdauiEntryBin *dbin;

	g_return_val_if_fail (GDAUI_IS_ENTRY_BIN (mgwrap), NULL);
	dbin = GDAUI_ENTRY_BIN (mgwrap);
	g_return_val_if_fail (dbin->priv, NULL);

	if (dbin->priv->commonbin.tmpvalue)
		return gda_value_copy (dbin->priv->commonbin.tmpvalue);
	else
		return gda_value_new_null ();
}


static void
value_loaded_cb (GdauiEntryBin *dbin, GValue *new_value)
{
	take_as_current_value (dbin, new_value);

	/* signal changes */
	gdaui_entry_wrapper_contents_changed (GDAUI_ENTRY_WRAPPER (dbin));
	gdaui_entry_wrapper_contents_activated (GDAUI_ENTRY_WRAPPER (dbin));
}

static void
link_activated_cb (GdauiEntryBin *dbin, const gchar *uri, GtkWidget *label)
{
	if (!dbin->priv->commonbin.popover)
		common_bin_create_menu (GTK_WIDGET (label), &(dbin->priv->commonbin),
					gdaui_data_entry_get_value_type (GDAUI_DATA_ENTRY (dbin)),
					(BinCallback) value_loaded_cb, dbin);

	common_bin_adjust (&(dbin->priv->commonbin), dbin->priv->editable, dbin->priv->commonbin.tmpvalue);
	gtk_widget_show (dbin->priv->commonbin.popover);
}

static void
connect_signals (GdauiEntryWrapper *mgwrap, G_GNUC_UNUSED GCallback modify_cb,
		 G_GNUC_UNUSED GCallback activate_cb)
{
	GdauiEntryBin *dbin;

	g_return_if_fail (GDAUI_IS_ENTRY_BIN (mgwrap));
	dbin = GDAUI_ENTRY_BIN (mgwrap);
	g_return_if_fail (dbin->priv);

	g_assert (dbin->priv->label);
	g_signal_connect_swapped (G_OBJECT (dbin->priv->label), "activate-link",
				  G_CALLBACK (link_activated_cb), dbin);
}

static void
set_editable (GdauiEntryWrapper *mgwrap, gboolean editable)
{
	GdauiEntryBin *dbin;

	g_return_if_fail (GDAUI_IS_ENTRY_BIN (mgwrap));
	dbin = GDAUI_ENTRY_BIN (mgwrap);
	g_return_if_fail (dbin->priv);

	dbin->priv->editable = editable;
	common_bin_adjust (&(dbin->priv->commonbin), editable, dbin->priv->commonbin.tmpvalue);
}

static void
grab_focus (GdauiEntryWrapper *mgwrap)
{
	GdauiEntryBin *dbin;

	g_return_if_fail (GDAUI_IS_ENTRY_BIN (mgwrap));
	dbin = GDAUI_ENTRY_BIN (mgwrap);
	g_return_if_fail (dbin->priv);

	gtk_widget_grab_focus (dbin->priv->label);
}
