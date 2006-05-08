/* gda-entity-field.h
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


#ifndef __GDA_ENTITY_FIELD_H_
#define __GDA_ENTITY_FIELD_H_

#include <glib-object.h>
#include "gda-decl.h"
#include "gda-value.h"

G_BEGIN_DECLS

#define GDA_TYPE_ENTITY_FIELD          (gda_entity_field_get_type())
#define GDA_ENTITY_FIELD(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_ENTITY_FIELD, GdaEntityField)
#define GDA_IS_ENTITY_FIELD(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, GDA_TYPE_ENTITY_FIELD)
#define GDA_ENTITY_FIELD_GET_IFACE(obj)  (G_TYPE_INSTANCE_GET_INTERFACE ((obj), GDA_TYPE_ENTITY_FIELD, GdaEntityFieldIface))


/* struct for the interface */
struct _GdaEntityFieldIface
{
	GTypeInterface           g_iface;

	/* virtual table */
	GdaEntity         *(* get_entity)      (GdaEntityField *iface);
	GType       (* get_gda_type)    (GdaEntityField *iface);
	GdaDictType       *(* get_data_type)   (GdaEntityField *iface);
	const gchar       *(* get_alias)       (GdaEntityField *iface);
};

GType            gda_entity_field_get_type        (void) G_GNUC_CONST;

GdaEntity       *gda_entity_field_get_entity      (GdaEntityField *iface);
GType     gda_entity_field_get_gda_type    (GdaEntityField *iface);
GdaDictType     *gda_entity_field_get_data_type   (GdaEntityField *iface);
const gchar     *gda_entity_field_get_name        (GdaEntityField *iface);
const gchar     *gda_entity_field_get_description (GdaEntityField *iface);

G_END_DECLS

#endif
