/*
 * Copyright (C) 2009 - 2012 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2010 David King <davidk@openismus.com>
 * Copyright (C) 2018 Daniel Espinosa <esodan@gmail.com>
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

#include "gdaui-entry-none.h"
#include <libgda/gda-data-handler.h>
#include <glib/gi18n-lib.h>

/* 
 * Main static functions 
 */
static void gdaui_entry_none_dispose (GObject *object);

/* virtual functions */
static GtkWidget *create_entry (GdauiEntryWrapper *mgwrap);
static void       real_set_value (GdauiEntryWrapper *mgwrap, const GValue *value);
static GValue  *real_get_value (GdauiEntryWrapper *mgwrap);
static void       connect_signals(GdauiEntryWrapper *mgwrap, GCallback modify_cb, GCallback activate_cb);

/* private structure */
typedef struct
{
	GValue *stored_value; /* this value is never modified */
} GdauiEntryNonePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GdauiEntryNone, gdaui_entry_none, GDAUI_TYPE_ENTRY_WRAPPER)


static void
gdaui_entry_none_class_init (GdauiEntryNoneClass * class)
{
	GObjectClass   *object_class = G_OBJECT_CLASS (class);

	object_class->dispose = gdaui_entry_none_dispose;

	GDAUI_ENTRY_WRAPPER_CLASS (class)->create_entry = create_entry;
	GDAUI_ENTRY_WRAPPER_CLASS (class)->real_set_value = real_set_value;
	GDAUI_ENTRY_WRAPPER_CLASS (class)->real_get_value = real_get_value;
	GDAUI_ENTRY_WRAPPER_CLASS (class)->connect_signals = connect_signals;
}

static void
gdaui_entry_none_init (GdauiEntryNone * entry)
{
	GdauiEntryNonePrivate *priv = gdaui_entry_none_get_instance_private (entry);
	priv->stored_value = NULL;
}

/**
 * gdaui_entry_none_new:
 * @type: the requested data type (compatible with @dh)
 *
 * Creates a new data entry widget
 *
 * Returns: (transfer full): the new widget
 */
GtkWidget *
gdaui_entry_none_new (GType type)
{
	GObject *obj;
	GdauiEntryNone *entry;

	obj = g_object_new (GDAUI_TYPE_ENTRY_NONE, NULL);
	entry = GDAUI_ENTRY_NONE (obj);
	gdaui_data_entry_set_value_type (GDAUI_DATA_ENTRY (entry), type);

	return GTK_WIDGET (obj);
}


static void
gdaui_entry_none_dispose (GObject   * object)
{
	GdauiEntryNone *entry;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GDAUI_IS_ENTRY_NONE (object));

	entry = GDAUI_ENTRY_NONE (object);
	GdauiEntryNonePrivate *priv = gdaui_entry_none_get_instance_private (entry);
	if (priv->stored_value) {
		gda_value_free (priv->stored_value);
		priv->stored_value = NULL;
	}

	/* parent class */
	G_OBJECT_CLASS (gdaui_entry_none_parent_class)->dispose (object);
}

static GtkWidget *
create_entry (GdauiEntryWrapper *mgwrap)
{
	GtkWidget *evbox, *label;

	g_return_val_if_fail (mgwrap && GDAUI_IS_ENTRY_NONE (mgwrap), NULL);

	evbox = gtk_event_box_new ();
	gtk_widget_add_events (evbox, GDK_FOCUS_CHANGE_MASK);
	gtk_widget_set_can_focus (evbox, TRUE);
	label = gtk_label_new ("");
	gtk_widget_set_name (label, "invalid-label");
	gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_CENTER);

	gtk_container_add (GTK_CONTAINER (evbox), label);
	gtk_widget_show (label);

	return evbox;
}

static void
real_set_value (GdauiEntryWrapper *mgwrap, const GValue *value)
{
	GdauiEntryNone *entry;

	g_return_if_fail (mgwrap && GDAUI_IS_ENTRY_NONE (mgwrap));
	entry = GDAUI_ENTRY_NONE (mgwrap);
	GdauiEntryNonePrivate *priv = gdaui_entry_none_get_instance_private (entry);

	if (priv->stored_value) {
		gda_value_free (priv->stored_value);
		priv->stored_value = NULL;
	}

	if (value)
		priv->stored_value = gda_value_copy ((GValue *) value);
}

static GValue *
real_get_value (GdauiEntryWrapper *mgwrap)
{
	GdauiEntryNone *entry;

	g_return_val_if_fail (mgwrap && GDAUI_IS_ENTRY_NONE (mgwrap), NULL);
	entry = GDAUI_ENTRY_NONE (mgwrap);
	GdauiEntryNonePrivate *priv = gdaui_entry_none_get_instance_private (entry);

	if (priv->stored_value)
		return gda_value_copy (priv->stored_value);
	else
		return gda_value_new_null ();
}

static void
connect_signals(GdauiEntryWrapper *mgwrap, G_GNUC_UNUSED GCallback modify_cb,
		G_GNUC_UNUSED GCallback activate_cb)
{
	/* GdauiEntryNone *entry; */

	/* g_return_if_fail (mgwrap && GDAUI_IS_ENTRY_NONE (mgwrap)); */
	/* entry = GDAUI_ENTRY_NONE (mgwrap); */
	/* GdauiEntryNonePrivate *priv = gdaui_entry_none_get_instance_private (entry); */
}
