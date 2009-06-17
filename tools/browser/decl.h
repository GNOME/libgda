/* 
 * Copyright (C) 2009 The GNOME Foundation.
 *
 * AUTHORS:
 * 	Vivien Malerba <malerba@gnome-db.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef __DECL_H_
#define __DECL_H_

G_BEGIN_DECLS

typedef struct _BrowserCore BrowserCore;

typedef struct _BrowserVariable BrowserVariable;
typedef struct _BrowserConnection BrowserConnection;
typedef struct _BrowserWindow BrowserWindow;

typedef struct _BrowserData BrowserData;

typedef struct _BrowserPerspectiveIface   BrowserPerspectiveIface;
typedef struct _BrowserPerspective        BrowserPerspective;

typedef struct {
	const gchar          *perspective_name;
	BrowserPerspective *(*perspective_create) (BrowserWindow *);
} BrowserPerspectiveFactory;
#define BROWSER_PERSPECTIVE_FACTORY(x) ((BrowserPerspectiveFactory*)(x))

typedef enum {
        BROWSER_FAVORITES_TABLES
} BrowserFavoritesType;

G_END_DECLS

#endif
