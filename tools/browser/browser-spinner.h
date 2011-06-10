/*
 * Copyright (C) 2009 - 2011 Vivien Malerba <malerba@gnome-db.org>
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

#ifndef BROWSER_SPINNER_H
#define BROWSER_SPINNER_H

#define BROWSER_SPINNER(x) GTK_SPINNER(x)
#define BrowserSpinner GtkSpinner
#define browser_spinner_new() gtk_spinner_new()
void	browser_spinner_start	 (BrowserSpinner *throbber);
void	browser_spinner_stop	 (BrowserSpinner *throbber);
void    browser_spinner_set_size (BrowserSpinner *spinner, GtkIconSize size);

#endif /* BROWSER_SPINNER_H */
