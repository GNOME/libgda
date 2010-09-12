/* gdaui-entry-date.c
 *
 * Copyright (C) 2003 - 2006 Vivien Malerba
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
#include "gdaui-entry-date.h"
#include <libgda/gda-data-handler.h>
#include <gdk/gdkkeysyms.h>
#include <string.h>

/* 
 * Main static functions 
 */
static void gdaui_entry_date_class_init (GdauiEntryDateClass * class);
static void gdaui_entry_date_init (GdauiEntryDate * srv);
/* get a pointer to the parents to be able to call their destructor */
static GObjectClass  *parent_class = NULL;

GType
gdaui_entry_date_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo info = {
			sizeof (GdauiEntryDateClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gdaui_entry_date_class_init,
			NULL,
			NULL,
			sizeof (GdauiEntryDate),
			0,
			(GInstanceInitFunc) gdaui_entry_date_init
		};
	
		type = g_type_register_static (GDAUI_TYPE_ENTRY_COMMON_TIME, "GdauiEntryDate", &info, 0);
	}
	return type;
}

static void
gdaui_entry_date_class_init (GdauiEntryDateClass * class)
{
	parent_class = g_type_class_peek_parent (class);
}

static void
gdaui_entry_date_init (GdauiEntryDate * gdaui_entry_date)
{
}

/**
 * gdaui_entry_date_new
 * @dh: the data handler to be used by the new widget
 *
 * Creates a new widget which is mainly a GtkEntry
 *
 * Returns: the new widget
 */
GtkWidget *
gdaui_entry_date_new (GdaDataHandler *dh)
{
	GObject *obj;

	g_return_val_if_fail (dh && GDA_IS_DATA_HANDLER (dh), NULL);
	g_return_val_if_fail (gda_data_handler_accepts_g_type (dh, G_TYPE_DATE), NULL);

	obj = g_object_new (GDAUI_TYPE_ENTRY_DATE, "handler", dh, "type", G_TYPE_DATE, NULL);

	return GTK_WIDGET (obj);
}
