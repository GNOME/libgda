/*
 *  Copyright © 2002 Jorn Baayen
 *  Copyright © 2009 Vivien Malerba <malerba@nome-db.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef BROWSER_STOCK_ICONS_H
#define BROWSER_STOCK_ICONS_H

G_BEGIN_DECLS

#define BROWSER_STOCK_HISTORY         "history-view"
#define BROWSER_STOCK_BOOKMARKS       "bookmark-view"
#define BROWSER_STOCK_BEGIN           "transaction-begin"
#define BROWSER_STOCK_COMMIT          "transaction-commit"
#define BROWSER_STOCK_ROLLBACK        "transaction-rollback"

/* Named icons defined in fd.o Icon Naming Spec */
#define STOCK_NEW_WINDOW           "window-new"
#define STOCK_ADD_BOOKMARK         "bookmark-new"
#define STOCK_PRINT_SETUP          "document-page-setup"

#define STOCK_CONSOLE              "accessories-text-editor"

void browser_stock_icons_init (void);

G_END_DECLS

#endif
