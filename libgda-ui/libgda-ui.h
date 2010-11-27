/* GDA library
 * Copyright (C) 2009 - 2010 The GNOME Foundation.
 *
 * AUTHORS:
 *      Vivien Malerba <malerba@gnome-db.org>
 *
 * This Library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this Library; see the file COPYING.LIB.  If not,
 * write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
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

void gdaui_init (void);

G_END_DECLS

#endif
