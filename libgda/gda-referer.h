/* gda-referer.h
 *
 * Copyright (C) 2003 - 2005 Vivien Malerba
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


#ifndef __GDA_REFERER_H_
#define __GDA_REFERER_H_

#include <glib-object.h>
#include "gda-decl.h"

G_BEGIN_DECLS

#define GDA_TYPE_REFERER          (gda_referer_get_type())
#define GDA_REFERER(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_REFERER, GdaReferer)
#define GDA_IS_REFERER(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, GDA_TYPE_REFERER)
#define GDA_REFERER_GET_IFACE(obj)  (G_TYPE_INSTANCE_GET_INTERFACE ((obj), GDA_TYPE_REFERER, GdaRefererIface))

struct _GdaRefererContext
{
	gpointer fill_later;
};

/* struct for the interface */
struct _GdaRefererIface
{
	GTypeInterface           g_iface;

	/* virtual table */
	gboolean    (* activate)        (GdaReferer *iface);
	void        (* deactivate)      (GdaReferer *iface);
	gboolean    (* is_active)       (GdaReferer *iface);
	GSList     *(* get_ref_objects) (GdaReferer *iface);
	void        (* replace_refs)    (GdaReferer *iface, GHashTable *replacements);

	/* signals */
	void        (* activated)       (GdaReferer *iface);
	void        (* deactivated)     (GdaReferer *iface);
};

GType       gda_referer_get_type            (void) G_GNUC_CONST;

gboolean    gda_referer_activate            (GdaReferer *iface);
void        gda_referer_deactivate          (GdaReferer *iface);
gboolean    gda_referer_is_active           (GdaReferer *iface);
GSList     *gda_referer_get_ref_objects     (GdaReferer *iface);
void        gda_referer_replace_refs        (GdaReferer *iface, GHashTable *replacements);

G_END_DECLS

#endif
