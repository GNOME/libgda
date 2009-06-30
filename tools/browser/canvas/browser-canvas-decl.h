/* browser-canvas-decl.h
 *
 * Copyright (C) 2009 Vivien Malerba
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

#ifndef __BROWSER_CANVAS_DECL_H_
#define __BROWSER_CANVAS_DECL_H_

typedef struct _BrowserCanvas        BrowserCanvas;
typedef struct _BrowserCanvasClass   BrowserCanvasClass;
typedef struct _BrowserCanvasPrivate BrowserCanvasPrivate;

typedef struct _BrowserCanvasItem        BrowserCanvasItem;
typedef struct _BrowserCanvasItemClass   BrowserCanvasItemClass;
typedef struct _BrowserCanvasItemPrivate BrowserCanvasItemPrivate;

typedef struct _BrowserCanvasTable        BrowserCanvasTable;
typedef struct _BrowserCanvasTableClass   BrowserCanvasTableClass;
typedef struct _BrowserCanvasTablePrivate BrowserCanvasTablePrivate;

typedef struct _BrowserCanvasColumn        BrowserCanvasColumn;
typedef struct _BrowserCanvasColumnClass   BrowserCanvasColumnClass;
typedef struct _BrowserCanvasColumnPrivate BrowserCanvasColumnPrivate;

#define BROWSER_CANVAS_ENTITY_COLOR      "yellow"
#define BROWSER_CANVAS_DB_TABLE_COLOR    "#b9b9b9"

#define BROWSER_CANVAS_OBJ_BG_COLOR      "#f8f8f8"
#define BROWSER_CANVAS_FONT              "Sans 10"

#endif
