/*
 * Copyright (C) 2000 Reinhard Müller <reinhard@src.gnome.org>
 * Copyright (C) 2000 - 2002 Rodrigo Moya <rodrigo@gnome-db.org>
 * Copyright (C) 2001 Carlos Perelló Marín <carlos@gnome-db.org>
 * Copyright (C) 2001 - 2015 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2002 Gonzalo Paniagua Javier <gonzalo@src.gnome.org>
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

#ifndef __LIBGDAUI_H__
#define __LIBGDAUI_H__

#include <libgda/libgda.h>

#include <libgda-ui/gdaui-data-entry.h>
#include <libgda-ui/gdaui-basic-form.h>
#include <libgda-ui/gdaui-combo.h>
#include <libgda-ui/gdaui-data-store.h>
#include <libgda-ui/gdaui-data-filter.h>
#include <libgda-ui/gdaui-data-proxy.h>
#include <libgda-ui/gdaui-easy.h>
#include <libgda-ui/gdaui-enums.h>
#include <libgda-ui/gdaui-raw-form.h>
#include <libgda-ui/gdaui-form.h>
#include <libgda-ui/gdaui-raw-grid.h>
#include <libgda-ui/gdaui-grid.h>
#include <libgda-ui/gdaui-data-proxy-info.h>
#include <libgda-ui/gdaui-provider-selector.h>
#include <libgda-ui/gdaui-server-operation.h>
#include <libgda-ui/gdaui-login.h>
#include <libgda-ui/gdaui-tree-store.h>
#include <libgda-ui/gdaui-cloud.h>
#include <libgda-ui/gdaui-data-selector.h>
#include <libgda-ui/gdaui-rt-editor.h>

G_BEGIN_DECLS

/**
 * SECTION:libgdaui
 * @short_description: Library initialization and information
 * @title: Library initialization
 * @stability: Stable
 * @see_also:
 */

void         gdaui_init (void);
const gchar *gdaui_get_default_path (void);
void         gdaui_set_default_path (const gchar *path);
GdkPixbuf   *gdaui_get_icon_for_db_engine (const gchar *engine);

G_END_DECLS

#endif
