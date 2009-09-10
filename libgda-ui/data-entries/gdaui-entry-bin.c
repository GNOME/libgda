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
static gboolean   expand_in_layout (GdauiEntryWrapper *mgwrap);
static void       set_editable (GdauiEntryWrapper *mgwrap, gboolean editable);
static void       grab_focus (GdauiEntryWrapper *mgwrap);

/* get a pointer to the parents to be able to call their destructor */
static GObjectClass  *parent_class = NULL;

/* private structure */
struct _GdauiEntryBinPrivate
{
	GtkWidget *button;

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
gdaui_entry_bin_class_init (GdauiEntryBinClass * class)
{
	GObjectClass   *object_class = G_OBJECT_CLASS (class);

	parent_class = g_type_class_peek_parent (class);

	object_class->dispose = gdaui_entry_bin_dispose;
	object_class->finalize = gdaui_entry_bin_finalize;

	GDAUI_ENTRY_WRAPPER_CLASS (class)->create_entry = create_entry;
	GDAUI_ENTRY_WRAPPER_CLASS (class)->real_set_value = real_set_value;
	GDAUI_ENTRY_WRAPPER_CLASS (class)->real_get_value = real_get_value;
	GDAUI_ENTRY_WRAPPER_CLASS (class)->connect_signals = connect_signals;
	GDAUI_ENTRY_WRAPPER_CLASS (class)->expand_in_layout = expand_in_layout;
	GDAUI_ENTRY_WRAPPER_CLASS (class)->set_editable = set_editable;
	GDAUI_ENTRY_WRAPPER_CLASS (class)->grab_focus = grab_focus;
}

static void
gdaui_entry_bin_init (GdauiEntryBin * gdaui_entry_bin)
{
	gdaui_entry_bin->priv = g_new0 (GdauiEntryBinPrivate, 1);
	gdaui_entry_bin->priv->button = NULL;
	gdaui_entry_bin->priv->current_data = NULL;
	gdaui_entry_bin->priv->editable = TRUE;
}

/**
 * gdaui_entry_bin_new
 * @dh: the data handler to be used by the new widget
 * @type: the requested data type (compatible with @dh)
 *
 * Creates a new widget which is mainly a GtkEntry
 *
 * Returns: the new widget
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
		if (gdaui_entry_bin->priv->menu.menu) {
			gtk_widget_destroy (gdaui_entry_bin->priv->menu.menu);
			gdaui_entry_bin->priv->menu.menu = NULL;
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
	GtkWidget *button, *arrow, *label;
	GdauiEntryBin *dbin;
	GtkWidget *hbox;

	g_return_val_if_fail (GDAUI_IS_ENTRY_BIN (mgwrap), NULL);
	dbin = GDAUI_ENTRY_BIN (mgwrap);
	g_return_val_if_fail (dbin->priv, NULL);

	button = gtk_button_new ();
	dbin->priv->button = button;

	hbox = gtk_hbox_new (FALSE, 0);
        gtk_container_add (GTK_CONTAINER (button), hbox);

	label = gtk_label_new (_("Attachement"));
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

        arrow = gtk_arrow_new (GTK_ARROW_DOWN, GTK_SHADOW_NONE);
	gtk_misc_set_alignment (GTK_MISC (arrow), 1.0, -1);
	gtk_box_pack_start (GTK_BOX (hbox), arrow, TRUE, TRUE, 0);

        gtk_widget_show_all (hbox);

	return button;
}

/*
 * WARNING:
 * Does NOT emit any signal 
 */
static void
take_current_value (GdauiEntryBin *dbin, GValue *value)
{
	if (dbin->priv->current_data) {
		gda_value_free (dbin->priv->current_data);
		dbin->priv->current_data = NULL;
	}

	dbin->priv->current_data = value;
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
button_clicked_cb (GtkWidget *button, GdauiEntryBin *dbin)
{
	if (!dbin->priv->menu.menu)
		common_bin_create_menu (&(dbin->priv->menu), button,
					gdaui_data_entry_get_value_type (GDAUI_DATA_ENTRY (dbin)),
					(BinCallback) value_loaded_cb, dbin);

	common_bin_adjust_menu (&(dbin->priv->menu), dbin->priv->editable, dbin->priv->current_data);

	int event_time;
	event_time = gtk_get_current_event_time ();
	gtk_menu_popup (GTK_MENU (dbin->priv->menu.menu), NULL, NULL, NULL, NULL,
                        0, event_time);
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
expand_in_layout (GdauiEntryWrapper *mgwrap)
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
	if (dbin->priv->menu.load_mitem)
		gtk_widget_set_sensitive (dbin->priv->menu.load_mitem, dbin->priv->editable);
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
