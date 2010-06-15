/*
 * Copyright (C) 2009 - 2010 Vivien Malerba
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

#ifndef BROWSER_SPINNER_H
#define BROWSER_SPINNER_H

#define BROWSER_SPINNER(x) GTK_SPINNER(x)
#define BrowserSpinner GtkSpinner
#define browser_spinner_new() gtk_spinner_new()
void	browser_spinner_start	 (BrowserSpinner *throbber);
void	browser_spinner_stop	 (BrowserSpinner *throbber);
void    browser_spinner_set_size (BrowserSpinner *spinner, GtkIconSize size);

#endif /* BROWSER_SPINNER_H */
