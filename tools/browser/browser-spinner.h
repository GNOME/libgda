/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Copyright © 2000 Eazel, Inc.
 * Copyright © 2004, 2006 Christian Persch
 *
 * Nautilus is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Nautilus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Author: Andy Hertzfeld <andy@eazel.com>
 *
 * $Id$
 */

#ifndef BROWSER_SPINNER_H
#define BROWSER_SPINNER_H

#if GTK_CHECK_VERSION(2,20,0)
#define BROWSER_SPINNER(x) GTK_SPINNER(x)
#define BrowserSpinner GtkSpinner
#define browser_spinner_new() gtk_spinner_new()
void		browser_spinner_start	 (BrowserSpinner *throbber);
void		browser_spinner_stop	 (BrowserSpinner *throbber);
void            browser_spinner_set_size (BrowserSpinner *spinner, GtkIconSize size);
#else
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define BROWSER_TYPE_SPINNER		(browser_spinner_get_type ())
#define BROWSER_SPINNER(o)			(G_TYPE_CHECK_INSTANCE_CAST ((o), BROWSER_TYPE_SPINNER, BrowserSpinner))
#define BROWSER_SPINNER_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), BROWSER_TYPE_SPINNER, BrowserSpinnerClass))
#define BROWSER_IS_SPINNER(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), BROWSER_TYPE_SPINNER))
#define BROWSER_IS_SPINNER_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), BROWSER_TYPE_SPINNER))
#define BROWSER_SPINNER_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), BROWSER_TYPE_SPINNER, BrowserSpinnerClass))

typedef struct _BrowserSpinner		BrowserSpinner;
typedef struct _BrowserSpinnerClass	BrowserSpinnerClass;
typedef struct _BrowserSpinnerPriv	BrowserSpinnerPriv;

struct _BrowserSpinner
{
	GtkWidget parent;

	/*< private >*/
	BrowserSpinnerPriv *priv;
};

struct _BrowserSpinnerClass
{
	GtkWidgetClass parent_class;
};

GType		browser_spinner_get_type (void);
GtkWidget      *browser_spinner_new	 (void);
void		browser_spinner_start	 (BrowserSpinner *throbber);
void		browser_spinner_stop	 (BrowserSpinner *throbber);
void		browser_spinner_set_size (BrowserSpinner *spinner,
					  GtkIconSize size);

G_END_DECLS
#endif /* GTK_CHECK_VERSION */

#endif /* BROWSER_SPINNER_H */
