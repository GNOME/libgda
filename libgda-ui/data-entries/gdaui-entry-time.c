/*
 * Copyright (C) 2009 - 2011 Vivien Malerba <malerba@gnome-db.org>
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
#include "gdaui-entry-time.h"
#include <libgda/gda-data-handler.h>
#include <gdk/gdkkeysyms.h>
#include <string.h>

/* 
 * Main static functions 
 */
static void gdaui_entry_time_class_init (GdauiEntryTimeClass * class);
static void gdaui_entry_time_init (GdauiEntryTime * srv);
/* get a pointer to the parents to be able to call their destructor */
static GObjectClass  *parent_class = NULL;

GType
gdaui_entry_time_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo info = {
			sizeof (GdauiEntryTimeClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gdaui_entry_time_class_init,
			NULL,
			NULL,
			sizeof (GdauiEntryTime),
			0,
			(GInstanceInitFunc) gdaui_entry_time_init,
			0
		};
	
		type = g_type_register_static (GDAUI_TYPE_ENTRY_COMMON_TIME, "GdauiEntryTime", &info, 0);
	}
	return type;
}

static void
gdaui_entry_time_class_init (GdauiEntryTimeClass * class)
{
	parent_class = g_type_class_peek_parent (class);
}

static void
gdaui_entry_time_init (G_GNUC_UNUSED GdauiEntryTime * gdaui_entry_time)
{
}

/**
 * gdaui_entry_time_new
 * @dh: the data handler to be used by the new widget
 *
 * Creates a new widget which is mainly a GtkEntry
 *
 * Returns: the new widget
 */
GtkWidget *
gdaui_entry_time_new (GdaDataHandler *dh)
{
	GObject *obj;

	g_return_val_if_fail (dh && GDA_IS_DATA_HANDLER (dh), NULL);
	g_return_val_if_fail (gda_data_handler_accepts_g_type (dh, GDA_TYPE_TIME), NULL);

	obj = g_object_new (GDAUI_TYPE_ENTRY_TIME, "handler", dh, "type", GDA_TYPE_TIME, NULL);

	return GTK_WIDGET (obj);
}
