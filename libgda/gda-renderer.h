/* gda-renderer.h
 *
 * Copyright (C) 2003 - 2006 Vivien Malerba
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


#ifndef __GDA_RENDERER_H_
#define __GDA_RENDERER_H_

#include <glib-object.h>
#include <libxml/tree.h>
#include "gda-decl.h"

G_BEGIN_DECLS

#define GDA_TYPE_RENDERER          (gda_renderer_get_type())
#define GDA_RENDERER(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_RENDERER, GdaRenderer)
#define GDA_IS_RENDERER(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, GDA_TYPE_RENDERER)
#define GDA_RENDERER_GET_IFACE(obj)  (G_TYPE_INSTANCE_GET_INTERFACE ((obj), GDA_TYPE_RENDERER, GdaRendererIface))

/* rendering options */
typedef enum {
	GDA_RENDERER_EXTRA_PRETTY_SQL       = 1 << 0,
	GDA_RENDERER_EXTRA_VAL_ATTRS        = 1 << 1,
	GDA_RENDERER_ERROR_IF_DEFAULT       = 1 << 2,
	GDA_RENDERER_FIELDS_NO_TARGET_ALIAS = 1 << 3
} GdaRendererOptions;

/* struct for the interface */
struct _GdaRendererIface
{
	GTypeInterface           g_iface;

	/* virtual table */
	gchar      *(* render_as_sql)   (GdaRenderer *iface, GdaParameterList *context, guint options, GError **error); 
	gchar      *(* render_as_str)   (GdaRenderer *iface, GdaParameterList *context);
	gboolean    (* is_valid)        (GdaRenderer *iface, GdaParameterList *context, GError **error);
};

GType           gda_renderer_get_type        (void) G_GNUC_CONST;

gchar          *gda_renderer_render_as_sql   (GdaRenderer *iface, GdaParameterList *context, guint options, GError **error);
gchar          *gda_renderer_render_as_str   (GdaRenderer *iface, GdaParameterList *context);
gboolean        gda_renderer_is_valid        (GdaRenderer *iface, GdaParameterList *context, GError **error);

G_END_DECLS

#endif
