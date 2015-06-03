/*
 * Copyright (C) 2009 - 2014 Vivien Malerba <malerba@gnome-db.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef __DECL_H_
#define __DECL_H_

G_BEGIN_DECLS

typedef struct _BrowserVariable BrowserVariable;
typedef struct _BrowserWindow BrowserWindow;

typedef struct _BrowserData BrowserData;

typedef struct _BrowserPerspectiveIface   BrowserPerspectiveIface;
typedef struct _BrowserPerspective        BrowserPerspective;

typedef struct _BrowserPageIface   BrowserPageIface;
typedef struct _BrowserPage        BrowserPage;

#define DEFAULT_FAVORITES_SIZE 150

#define DEFAULT_DATA_SELECT_LIMIT 500

G_END_DECLS

#endif
