/* gda-renderer.c
 *
 * Copyright (C) 2003 - 2007 Vivien Malerba
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

#include "gda-renderer.h"

static void gda_renderer_iface_init (gpointer g_class);

GType
gda_renderer_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (GdaRendererIface),
			(GBaseInitFunc) gda_renderer_iface_init,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) NULL,
			NULL,
			NULL,
			0,
			0,
			(GInstanceInitFunc) NULL
		};
		
		type = g_type_register_static (G_TYPE_INTERFACE, "GdaRenderer", &info, 0);
		g_type_interface_add_prerequisite (type, G_TYPE_OBJECT);
	}
	return type;
}


static void
gda_renderer_iface_init (gpointer g_class)
{
	static gboolean initialized = FALSE;

	if (! initialized) {
		initialized = TRUE;
	}
}

/**
 * gda_renderer_render_as_sql
 * @iface: an object which implements the #GdaRenderer interface
 * @context: a #GdaParameterList containing values for @iface's parameters, or %NULL
 * @out_params_used: a place to append #GdaParameter which have been used during the rendering process, or %NULL
 * @options: OR'ed flags from #GdaRendererOptions to give some rendering options
 * @error: location to store error, or %NULL
 *
 * Build a SQL statement representing the object, in the specified context.
 *
 * If @context is %NULL, then no error related to missing parameters (which should be in the
 * context) is returned, and missing values are replaced by 'human readable' SQL.
 *
 * If @out_params_used is not %NULL, then pointers to the #GdaParameter objects used to actually
 * render the SQL statement are appended to the list (in case a parameter object is used several times,
 * it is only once listed in the resulting list).
 *
 * Returns: the new SQL statement (new string), or %NULL in case of error
 */
gchar *
gda_renderer_render_as_sql (GdaRenderer *iface, GdaParameterList *context, GSList **out_params_used,
			    GdaRendererOptions options, GError **error)
{
	g_return_val_if_fail (iface && GDA_IS_RENDERER (iface), NULL);

	if (GDA_RENDERER_GET_IFACE (iface)->render_as_sql)
		return (GDA_RENDERER_GET_IFACE (iface)->render_as_sql) (iface, context, out_params_used, 
									options, error);
	
	return NULL;
}


/**
 * gda_renderer_render_as_str
 * @iface: an object which implements the #GdaRenderer interface
 * @context: rendering context
 *
 * Build a human readable string representing the object, in the specified context.
 *
 * Returns: the new string
 */
gchar *
gda_renderer_render_as_str (GdaRenderer *iface, GdaParameterList *context)
{
	g_return_val_if_fail (iface && GDA_IS_RENDERER (iface), NULL);

	if (GDA_RENDERER_GET_IFACE (iface)->render_as_str)
		return (GDA_RENDERER_GET_IFACE (iface)->render_as_str) (iface, context);
	
	return NULL;
}

/**
 * gda_renderer_is_valid
 * @iface: an object which implements the #GdaRenderer interface
 * @context: rendering context
 * @error: location to store error, or %NULL
 *
 * Tells if @iface has all the necessary information in @context to be rendered
 * into a valid statement (which can be executed).
 *
 * Returns: TRUE if @iface can be rendered with @context
 */
gboolean
gda_renderer_is_valid (GdaRenderer *iface, GdaParameterList *context, GError **error)
{
	g_return_val_if_fail (iface && GDA_IS_RENDERER (iface), FALSE);

	if (GDA_RENDERER_GET_IFACE (iface)->is_valid)
		return (GDA_RENDERER_GET_IFACE (iface)->is_valid) (iface, context, error);
	
	return FALSE;
}
