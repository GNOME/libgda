/* gda-entity-field.h
 *
 * Copyright (C) 2003 - 2006 Vivien Malerba
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
	GType              (* get_g_type)      (GdaEntityField *iface);
    void               (* set_dict_type)   (GdaEntityField *iface, GdaDictType *type);
	GdaDictType       *(* get_dict_type)   (GdaEntityField *iface);
	const gchar       *(* get_alias)       (GdaEntityField *iface);
};

GType            gda_entity_field_get_type        (void) G_GNUC_CONST;

GdaEntity       *gda_entity_field_get_entity      (GdaEntityField *iface);
GType            gda_entity_field_get_g_type      (GdaEntityField *iface);
void             gda_entity_field_set_dict_type   (GdaEntityField *iface, GdaDictType *type);
GdaDictType     *gda_entity_field_get_dict_type   (GdaEntityField *iface);
const gchar     *gda_entity_field_get_name        (GdaEntityField *iface);
const gchar     *gda_entity_field_get_description (GdaEntityField *iface);

G_END_DECLS

#endif
