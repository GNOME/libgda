/* GDA library
 * Copyright (C) 2009 The GNOME Foundation.
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

#include <libgdaui/gdaui-data-entry.h>
#include <libgdaui/gdaui-basic-form.h>
#include <libgdaui/gdaui-combo.h>
#include <libgdaui/gdaui-data-store.h>
#include <libgdaui/gdaui-data-widget-filter.h>
#include <libgdaui/gdaui-data-widget.h>
#include <libgdaui/gdaui-easy.h>
#include <libgdaui/gdaui-enums.h>
#include <libgdaui/gdaui-raw-form.h>
#include <libgdaui/gdaui-form.h>
#include <libgdaui/gdaui-set.h>
#include <libgdaui/gdaui-raw-grid.h>
#include <libgdaui/gdaui-grid.h>
#include <libgdaui/gdaui-data-widget-info.h>
#include <libgdaui/gdaui-provider-selector.h>
#include <libgdaui/gdaui-server-operation.h>
#include <libgdaui/gdaui-login.h>
#include <libgdaui/gdaui-tree-store.h>

G_BEGIN_DECLS

void             gdaui_init (void);

G_END_DECLS

#endif
