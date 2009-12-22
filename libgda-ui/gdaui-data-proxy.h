/* gdaui-data-proxy.h
 *
 * Copyright (C) 2004 - 2009 Vivien Malerba <malerba@gnome-db.org>
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


#ifndef __GDAUI_DATA_PROXY_H_
#define __GDAUI_DATA_PROXY_H_

#include <glib-object.h>
#include <gtk/gtk.h>
#include <libgda/gda-decl.h>
#include "gdaui-decl.h"
#include "gdaui-enums.h"

G_BEGIN_DECLS

#define GDAUI_TYPE_DATA_PROXY          (gdaui_data_proxy_get_type())
#define GDAUI_DATA_PROXY(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, GDAUI_TYPE_DATA_PROXY, GdauiDataProxy)
#define GDAUI_IS_DATA_PROXY(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, GDAUI_TYPE_DATA_PROXY)
#define GDAUI_DATA_PROXY_GET_IFACE(obj)  (G_TYPE_INSTANCE_GET_INTERFACE ((obj), GDAUI_TYPE_DATA_PROXY, GdauiDataProxyIface))

typedef enum {
	GDAUI_DATA_PROXY_WRITE_ON_DEMAND           = 0, /* write only when explicitly requested */
	GDAUI_DATA_PROXY_WRITE_ON_ROW_CHANGE       = 1, /* write when the current selected row changes */
	GDAUI_DATA_PROXY_WRITE_ON_VALUE_ACTIVATED  = 2, /* write when user activates a value change */
	GDAUI_DATA_PROXY_WRITE_ON_VALUE_CHANGE     = 3  /* write when a parameters's value changes */
} GdauiDataProxyWriteMode;

/* struct for the interface */
struct _GdauiDataProxyIface
{
	GTypeInterface           g_iface;

	/* virtual table */
	GdaDataProxy        *(* get_proxy)           (GdauiDataProxy *iface);
	void                 (* set_column_editable) (GdauiDataProxy *iface, gint column, gboolean editable);
	void                 (* show_column_actions) (GdauiDataProxy *iface, gint column, gboolean show_actions);
	GtkActionGroup      *(* get_actions_group)   (GdauiDataProxy *iface);
	gboolean             (* set_write_mode)      (GdauiDataProxy *iface, GdauiDataProxyWriteMode mode);
	GdauiDataProxyWriteMode (* get_write_mode)(GdauiDataProxy *iface);

	/* signals */
	void                 (* proxy_changed)       (GdauiDataProxy *iface, GdaDataProxy *proxy);
};

GType             gdaui_data_proxy_get_type                  (void) G_GNUC_CONST;

GdaDataProxy     *gdaui_data_proxy_get_proxy                 (GdauiDataProxy *iface);
GtkActionGroup   *gdaui_data_proxy_get_actions_group         (GdauiDataProxy *iface);
void              gdaui_data_proxy_perform_action            (GdauiDataProxy *iface, GdauiAction action);

void              gdaui_data_proxy_column_set_editable       (GdauiDataProxy *iface, gint column, gboolean editable);
void              gdaui_data_proxy_column_show_actions       (GdauiDataProxy *iface, gint column, gboolean show_actions);

gboolean          gdaui_data_proxy_set_write_mode            (GdauiDataProxy *iface, GdauiDataProxyWriteMode mode);
GdauiDataProxyWriteMode gdaui_data_proxy_get_write_mode   (GdauiDataProxy *iface);

G_END_DECLS

#endif
