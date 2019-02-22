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

#include "gdaui-entry-boolean.h"

/* virtual functions */
static GtkWidget *create_entry (GdauiEntryWrapper *mgwrap);
static void       real_set_value (GdauiEntryWrapper *mgwrap, const GValue *value);
static GValue    *real_get_value (GdauiEntryWrapper *mgwrap);
static void       connect_signals(GdauiEntryWrapper *mgwrap, GCallback modify_cb, GCallback activate_cb);
static void       set_editable (GdauiEntryWrapper *mgwrap, gboolean editable);
static void       grab_focus (GdauiEntryWrapper *mgwrap);

/* private structure */
typedef struct
{
	GtkWidget *switchw;
} GdauiEntryBooleanPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GdauiEntryBoolean, gdaui_entry_boolean, GDAUI_TYPE_ENTRY_WRAPPER)


static void
gdaui_entry_boolean_class_init (GdauiEntryBooleanClass * class)
{
	GDAUI_ENTRY_WRAPPER_CLASS (class)->create_entry = create_entry;
	GDAUI_ENTRY_WRAPPER_CLASS (class)->real_set_value = real_set_value;
	GDAUI_ENTRY_WRAPPER_CLASS (class)->real_get_value = real_get_value;
	GDAUI_ENTRY_WRAPPER_CLASS (class)->connect_signals = connect_signals;
	GDAUI_ENTRY_WRAPPER_CLASS (class)->set_editable = set_editable;
	GDAUI_ENTRY_WRAPPER_CLASS (class)->grab_focus = grab_focus;
}

static void
gdaui_entry_boolean_init (GdauiEntryBoolean * entry)
{
	GdauiEntryBooleanPrivate *priv = gdaui_entry_boolean_get_instance_private (entry);
	priv->switchw = NULL;
}

/**
 * gdaui_entry_boolean_new:
 * @dh: the data handler to be used by the new widget
 * @type: the requested data type (compatible with @dh)
 *
 * Creates a new data entry widget
 *
 * Returns: (transfer full): the new widget
 */
GtkWidget *
gdaui_entry_boolean_new (GdaDataHandler *dh, GType type)
{
	GObject *obj;
	GdauiEntryBoolean *mgbool;

	g_return_val_if_fail (GDA_IS_DATA_HANDLER (dh), NULL);
	g_return_val_if_fail (gda_data_handler_accepts_g_type (dh, type), NULL);

	obj = g_object_new (GDAUI_TYPE_ENTRY_BOOLEAN, "handler", dh, NULL);
	mgbool = GDAUI_ENTRY_BOOLEAN (obj);
	gdaui_data_entry_set_value_type (GDAUI_DATA_ENTRY (mgbool), type);

	return GTK_WIDGET (obj);
}

static void
event_after_cb (GtkWidget *widget, GdkEvent *event, gpointer data)
{
	g_signal_emit_by_name (widget, "event-after", event);
}

static GtkWidget *
create_entry (GdauiEntryWrapper *mgwrap)
{
	GdauiEntryBoolean *mgbool;

	g_return_val_if_fail (GDAUI_IS_ENTRY_BOOLEAN (mgwrap), NULL);
	mgbool = GDAUI_ENTRY_BOOLEAN (mgwrap);
	GdauiEntryBooleanPrivate *priv = gdaui_entry_boolean_get_instance_private (mgbool);

	GtkWidget *wid, *box;
	box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
	wid = gtk_switch_new ();
	gtk_box_pack_start (GTK_BOX (box), wid, FALSE, FALSE, 0);
	gtk_widget_show (wid);
	priv->switchw = wid;
	g_signal_connect_swapped (wid, "event-after",
				  G_CALLBACK (event_after_cb), box);

	return box;
}

static void
real_set_value (GdauiEntryWrapper *mgwrap, const GValue *value)
{
	GdauiEntryBoolean *mgbool;

	g_return_if_fail (GDAUI_IS_ENTRY_BOOLEAN (mgwrap));
	mgbool = GDAUI_ENTRY_BOOLEAN (mgwrap);
	GdauiEntryBooleanPrivate *priv = gdaui_entry_boolean_get_instance_private (mgbool);

	if (value) {
		if (gda_value_is_null ((GValue *) value))
			gtk_switch_set_active (GTK_SWITCH (priv->switchw), FALSE);
		else
			gtk_switch_set_active (GTK_SWITCH (priv->switchw), g_value_get_boolean ((GValue *) value));
	}
	else
		gtk_switch_set_active (GTK_SWITCH (priv->switchw), FALSE);
}

static GValue *
real_get_value (GdauiEntryWrapper *mgwrap)
{
	GValue *value;
	GdauiEntryBoolean *mgbool;
	GdaDataHandler *dh;
	const gchar *str;

	g_return_val_if_fail (GDAUI_IS_ENTRY_BOOLEAN (mgwrap), NULL);
	mgbool = GDAUI_ENTRY_BOOLEAN (mgwrap);
	GdauiEntryBooleanPrivate *priv = gdaui_entry_boolean_get_instance_private (mgbool);

	dh = gdaui_data_entry_get_handler (GDAUI_DATA_ENTRY (mgwrap));
	if (gtk_switch_get_active (GTK_SWITCH (priv->switchw)))
		str = "TRUE";
	else
		str = "FALSE";
	value = gda_data_handler_get_value_from_sql (dh, str, gdaui_data_entry_get_value_type (GDAUI_DATA_ENTRY (mgwrap)));

	return value;
}

static void
connect_signals(GdauiEntryWrapper *mgwrap, GCallback modify_cb, GCallback activate_cb)
{
	GdauiEntryBoolean *mgbool;

	g_return_if_fail (GDAUI_IS_ENTRY_BOOLEAN (mgwrap));
	mgbool = GDAUI_ENTRY_BOOLEAN (mgwrap);
	GdauiEntryBooleanPrivate *priv = gdaui_entry_boolean_get_instance_private (mgbool);

	g_signal_connect_swapped (G_OBJECT (priv->switchw), "notify::active",
				  modify_cb, mgwrap);
	g_signal_connect_swapped (G_OBJECT (priv->switchw), "notify::active",
				  activate_cb, mgwrap);
}

static void
set_editable (GdauiEntryWrapper *mgwrap, gboolean editable)
{
	GdauiEntryBoolean *mgbool;

	g_return_if_fail (GDAUI_IS_ENTRY_BOOLEAN (mgwrap));
	mgbool = GDAUI_ENTRY_BOOLEAN (mgwrap);
	GdauiEntryBooleanPrivate *priv = gdaui_entry_boolean_get_instance_private (mgbool);

	gtk_widget_set_sensitive (priv->switchw, editable);
}

static void
grab_focus (GdauiEntryWrapper *mgwrap)
{
	GdauiEntryBoolean *mgbool;

	g_return_if_fail (GDAUI_IS_ENTRY_BOOLEAN (mgwrap));
	mgbool = GDAUI_ENTRY_BOOLEAN (mgwrap);
	GdauiEntryBooleanPrivate *priv = gdaui_entry_boolean_get_instance_private (mgbool);

	gtk_widget_grab_focus (priv->switchw);
}
