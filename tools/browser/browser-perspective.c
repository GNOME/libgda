/* 
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

#include "browser-perspective.h"

static GStaticRecMutex init_mutex = G_STATIC_REC_MUTEX_INIT;
static void browser_perspective_class_init (gpointer g_class);

GType
browser_perspective_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo info = {
			sizeof (BrowserPerspectiveIface),
			(GBaseInitFunc) browser_perspective_class_init,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) NULL,
			NULL,
			NULL,
			0,
			0,
			(GInstanceInitFunc) NULL
		};
		
		g_static_rec_mutex_lock (&init_mutex);
		if (type == 0) {
			type = g_type_register_static (G_TYPE_INTERFACE, "BrowserPerspective", &info, 0);
			g_type_interface_add_prerequisite (type, GTK_TYPE_WIDGET);
		}
		g_static_rec_mutex_unlock (&init_mutex);
	}
	return type;
}

static void
browser_perspective_class_init (gpointer g_class)
{
	static gboolean initialized = FALSE;

	g_static_rec_mutex_lock (&init_mutex);
	if (! initialized) {
		initialized = TRUE;
	}
	g_static_rec_mutex_unlock (&init_mutex);
}

/**
 * browser_perspective_get_actions_group
 * @pers:
 * @instance: a GtkWidget which has been returned by a previous call to browser_perspective_create()
 *
 * Returns: a new #GtkActionGroup
 */
GtkActionGroup *
browser_perspective_get_actions_group (BrowserPerspective *pers)
{
	g_return_val_if_fail (IS_BROWSER_PERSPECTIVE (pers), NULL);
	
	if (BROWSER_PERSPECTIVE_GET_CLASS (pers)->i_get_actions_group)
		return (BROWSER_PERSPECTIVE_GET_CLASS (pers)->i_get_actions_group) (pers);
	else
		return NULL;
}

/**
 * browser_perspective_get_actions_ui
 *
 */
const gchar *
browser_perspective_get_actions_ui (BrowserPerspective *pers)
{
	g_return_val_if_fail (IS_BROWSER_PERSPECTIVE (pers), NULL);
	
	if (BROWSER_PERSPECTIVE_GET_CLASS (pers)->i_get_actions_ui)
		return (BROWSER_PERSPECTIVE_GET_CLASS (pers)->i_get_actions_ui) (pers);
	else
		return NULL;
}
