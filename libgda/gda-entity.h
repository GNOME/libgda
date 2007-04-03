/* gda-entity.h
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


#ifndef __GDA_ENTITY_H_
#define __GDA_ENTITY_H_

#include <glib-object.h>
#include "gda-decl.h"

G_BEGIN_DECLS

#define GDA_TYPE_ENTITY          (gda_entity_get_type())
#define GDA_ENTITY(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_ENTITY, GdaEntity)
#define GDA_IS_ENTITY(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, GDA_TYPE_ENTITY)
#define GDA_ENTITY_GET_IFACE(obj)  (G_TYPE_INSTANCE_GET_INTERFACE ((obj), GDA_TYPE_ENTITY, GdaEntityIface))


/* struct for the interface */
struct _GdaEntityIface
{
	GTypeInterface           g_iface;

	/* virtual table */
	gboolean         (*has_field)            (GdaEntity *iface, GdaEntityField *field);
	GSList          *(*get_fields)           (GdaEntity *iface);
	GdaEntityField  *(*get_field_by_name)    (GdaEntity *iface, const gchar *name);
	GdaEntityField  *(*get_field_by_xml_id)  (GdaEntity *iface, const gchar *xml_id);
	GdaEntityField  *(*get_field_by_index)   (GdaEntity *iface, gint index);
	gint             (*get_field_index)      (GdaEntity *iface, GdaEntityField *field);
	void             (*add_field)            (GdaEntity *iface, GdaEntityField *field);
	void             (*add_field_before)     (GdaEntity *iface, GdaEntityField *field, GdaEntityField *field_before);
	void             (*swap_fields)          (GdaEntity *iface, GdaEntityField *field1, GdaEntityField *field2);
	void             (*remove_field)         (GdaEntity *iface, GdaEntityField *field);
	gboolean         (*is_writable)          (GdaEntity *iface);

	/* signals */
	void             (*field_added)          (GdaEntity *iface, GdaEntityField *field);
	void             (*field_removed)        (GdaEntity *iface, GdaEntityField *field);
	void             (*field_updated)        (GdaEntity *iface, GdaEntityField *field);
	void             (*fields_order_changed) (GdaEntity *iface);
};

GType             gda_entity_get_type        (void) G_GNUC_CONST;

gboolean          gda_entity_has_field           (GdaEntity *iface, GdaEntityField *field);
GSList           *gda_entity_get_fields          (GdaEntity *iface);
gint              gda_entity_get_n_fields        (GdaEntity *iface);
GdaEntityField   *gda_entity_get_field_by_name   (GdaEntity *iface, const gchar *name);
GdaEntityField   *gda_entity_get_field_by_xml_id (GdaEntity *iface, const gchar *xml_id);
GdaEntityField   *gda_entity_get_field_by_index  (GdaEntity *iface, gint index);
gint              gda_entity_get_field_index     (GdaEntity *iface, GdaEntityField *field);
void              gda_entity_add_field           (GdaEntity *iface, GdaEntityField *field);
void              gda_entity_add_field_before    (GdaEntity *iface, GdaEntityField *field, GdaEntityField *field_before);
void              gda_entity_swap_fields         (GdaEntity *iface, GdaEntityField *field1, GdaEntityField *field2);
void              gda_entity_remove_field        (GdaEntity *iface, GdaEntityField *field);

gboolean          gda_entity_is_writable         (GdaEntity *iface);

G_END_DECLS

#endif
