/* 
 * Copyright © 2000 Eazel, Inc.
 * Copyright © 2002-2004 Marco Pesenti Gritti
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
 * Ephy port by Marco Pesenti Gritti <marco@it.gnome.org>
 * 
 * $Id$
 */

#ifdef GSEAL_ENABLE
  #define KEEP_GSEAL_ENABLE
  #undef GSEAL_ENABLE
#endif
#include <gtk/gtk.h>
#ifdef KEEP_GSEAL_ENABLE
  #define GSEAL_ENABLE
  #undef KEEP_GSEAL_ENABLE
#endif

#include "browser-spinner.h"
/**
 * browser_spinner_start:
 * @spinner: a #BrowserSpinner
 *
 * Start the spinner animation.
 **/
void
browser_spinner_start (BrowserSpinner *spinner)
{
	gtk_widget_show (GTK_WIDGET (spinner));
	gtk_spinner_start (spinner);
}

/**
 * browser_spinner_stop:
 * @spinner: a #BrowserSpinner
 *
 * Stop the spinner animation.
 **/
void
browser_spinner_stop (BrowserSpinner *spinner)
{
	gtk_widget_hide (GTK_WIDGET (spinner));
	gtk_spinner_stop (spinner);
}

/*
 * browser_spinner_set_size:
 * @spinner: a #BrowserSpinner
 * @size: the size of type %GtkIconSize
 *
 * Set the size of the spinner.
 **/
void
browser_spinner_set_size (BrowserSpinner *spinner,
			  GtkIconSize size)
{
	gint width, height;
	if (size == GTK_ICON_SIZE_INVALID)
		size = GTK_ICON_SIZE_DIALOG;
	gtk_icon_size_lookup_for_settings (gtk_widget_get_settings ((GtkWidget*) spinner),
					   size, &width, &height);
	gtk_widget_set_size_request (GTK_WIDGET (spinner), width, height);
}
