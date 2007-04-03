/* gda-referer.c
 *
 * Copyright (C) 2003 - 2005 Vivien Malerba
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

#include "gda-referer.h"
#include "gda-marshal.h"

/* signals */
enum
{
        ACTIVATED,
        DEACTIVATED,
        LAST_SIGNAL
};

static gint gda_referer_signals[LAST_SIGNAL] = { 0, 0 };

static void gda_referer_iface_init (gpointer g_class);

GType
gda_referer_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (GdaRefererIface),
			(GBaseInitFunc) gda_referer_iface_init,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) NULL,
			NULL,
			NULL,
			0,
			0,
			(GInstanceInitFunc) NULL
		};
		
		type = g_type_register_static (G_TYPE_INTERFACE, "GdaReferer", &info, 0);
		g_type_interface_add_prerequisite (type, G_TYPE_OBJECT);
	}
	return type;
}


static void
gda_referer_iface_init (gpointer g_class)
{
	static gboolean initialized = FALSE;

	if (! initialized) {
		gda_referer_signals[ACTIVATED] =
                        g_signal_new ("activated",
                                      GDA_TYPE_REFERER,
                                      G_SIGNAL_RUN_FIRST,
                                      G_STRUCT_OFFSET (GdaRefererIface, activated),
                                      NULL, NULL,
                                      gda_marshal_VOID__VOID, G_TYPE_NONE,
                                      0);
		gda_referer_signals[DEACTIVATED] =
                        g_signal_new ("deactivated",
                                      GDA_TYPE_REFERER,
                                      G_SIGNAL_RUN_FIRST,
                                      G_STRUCT_OFFSET (GdaRefererIface, deactivated),
                                      NULL, NULL,
                                      gda_marshal_VOID__VOID, G_TYPE_NONE,
                                      0);

		initialized = TRUE;
	}
}

/**
 * gda_referer_activate
 * @iface: an object which implements the #GdaReferer interface
 *
 * Tries to activate the object, does nothing if the object is already active.
 *
 * Returns: TRUE if the object is active after the call
 */
gboolean
gda_referer_activate (GdaReferer *iface)
{
	g_return_val_if_fail (iface && GDA_IS_REFERER (iface), FALSE);
	
	if (GDA_REFERER_GET_IFACE (iface)->activate)
		return (GDA_REFERER_GET_IFACE (iface)->activate) (iface);
	
	return TRUE;
}

/**
 * gda_referer_deactivate
 * @iface: an object which implements the #GdaReferer interface
 *
 * Deactivates the object. This is the opposite to function gda_referer_activate().
 * If the object is already non active, then nothing happens.
 */
void
gda_referer_deactivate (GdaReferer *iface)
{
	g_return_if_fail (iface && GDA_IS_REFERER (iface));
	
	if (GDA_REFERER_GET_IFACE (iface)->deactivate)
		(GDA_REFERER_GET_IFACE (iface)->deactivate) (iface);
}

/**
 * gda_referer_is_active
 * @iface: an object which implements the #GdaReferer interface
 *
 * Get the status of an object
 *
 * Returns: TRUE if the object is active
 */
gboolean
gda_referer_is_active (GdaReferer *iface)
{
	g_return_val_if_fail (iface && GDA_IS_REFERER (iface), FALSE);
	
	if (GDA_REFERER_GET_IFACE (iface)->is_active)
		return (GDA_REFERER_GET_IFACE (iface)->is_active) (iface);
	
	return TRUE;
}

/**
 * gda_referer_get_ref_objects
 * @iface: an object which implements the #GdaReferer interface
 *
 * Get the list of objects which are referenced by @iface. The returned list is a
 * new list. If @iface is not active, then the returned list is incomplete.
 *
 * Returns: a new list of referenced objects
 */
GSList *
gda_referer_get_ref_objects (GdaReferer *iface)
{
	g_return_val_if_fail (iface && GDA_IS_REFERER (iface), NULL);
	
	if (GDA_REFERER_GET_IFACE (iface)->get_ref_objects)
		return (GDA_REFERER_GET_IFACE (iface)->get_ref_objects) (iface);
	
	return NULL;
}

/**
 * gda_referer_replace_refs
 * @iface: an object which implements the #GdaReferer interface
 * @replacements: a #GHashTable
 *
 * Ask @iface to replace references to objects listed as keys in the @replacements hash table
 * with references to objects of the corresponding value.
 *
 * It's up to the caller to make sure each pair of (key, value) objects in @replacements are of the
 * same type, and that it makes sense to procede to the replacement.
 *
 * The object implementing this interface will accept to do any work only if it is
 * already active.
 */
void
gda_referer_replace_refs (GdaReferer *iface, GHashTable *replacements)
{
	g_return_if_fail (iface && GDA_IS_REFERER (iface));
	
	if (!replacements)
		return;

	if (GDA_REFERER_GET_IFACE (iface)->replace_refs)
		(GDA_REFERER_GET_IFACE (iface)->replace_refs) (iface, replacements);
}
