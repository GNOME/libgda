/*
 * Copyright (C) 2009 - 2012 Vivien Malerba <malerba@gnome-db.org>
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

#include <glib/gi18n-lib.h>
#include "gdaui-entry-timestamp.h"
#include <libgda/gda-data-handler.h>
#include <gdk/gdkkeysyms.h>
#include <string.h>

/* 
 * Main static functions 
 */
G_DEFINE_TYPE (GdauiEntryTimestamp, gdaui_entry_timestamp, GDAUI_TYPE_ENTRY_COMMON_TIME)
static void
gdaui_entry_timestamp_class_init (GdauiEntryTimestampClass * class)
{
}

static void
gdaui_entry_timestamp_init (G_GNUC_UNUSED GdauiEntryTimestamp * gdaui_entry_timestamp)
{
}

/**
 * gdaui_entry_timestamp_new:
 * @dh: the data handler to be used by the new widget
 *
 * Creates a new data entry widget
 *
 * Returns: (transfer full): the new widget
 */
GtkWidget *
gdaui_entry_timestamp_new (GdaDataHandler *dh)
{
	GObject *obj;

	g_return_val_if_fail (dh && GDA_IS_DATA_HANDLER (dh), NULL);
	g_return_val_if_fail (gda_data_handler_accepts_g_type (dh, G_TYPE_DATE_TIME), NULL);

	obj = g_object_new (GDAUI_TYPE_ENTRY_TIMESTAMP, "handler", dh, "type", G_TYPE_DATE_TIME, NULL);

	return GTK_WIDGET (obj);
}
