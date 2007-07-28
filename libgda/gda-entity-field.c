/* gda-entity-field.c
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

#include "gda-entity-field.h"
#include "gda-dict-type.h"
#include <libgda/gda-object.h>

static void gda_entity_field_iface_init (gpointer g_class);

GType
gda_entity_field_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (GdaEntityFieldIface),
			(GBaseInitFunc) gda_entity_field_iface_init,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) NULL,
			NULL,
			NULL,
			0,
			0,
			(GInstanceInitFunc) NULL
		};
		
		type = g_type_register_static (G_TYPE_INTERFACE, "GdaEntityField", &info, 0);
		g_type_interface_add_prerequisite (type, GDA_TYPE_OBJECT);
	}
	return type;
}


static void
gda_entity_field_iface_init (gpointer g_class)
{
	static gboolean initialized = FALSE;

	if (! initialized) {
		initialized = TRUE;
	}
}


/**
 * gda_entity_field_get_entity
 * @iface: an object which implements the #GdaEntityField interface
 *
 * Get a reference to the object implementing the #GdaEntity interface to which
 * the object implementing the #GdaEntityField is attached to.
 *
 * Returns: the object implementing the #GdaEntity interface
 */
GdaEntity *
gda_entity_field_get_entity (GdaEntityField *iface)
{
	g_return_val_if_fail (iface && GDA_IS_ENTITY_FIELD (iface), NULL);

	if (GDA_ENTITY_FIELD_GET_IFACE (iface)->get_entity)
		return (GDA_ENTITY_FIELD_GET_IFACE (iface)->get_entity) (iface);
	
	return NULL;
}

/**
 * gda_entity_field_get_dict_type
 * @iface: an object which implements the #GdaEntityField interface
 *
 * Get the dict type of the object implementing the #GdaEntityField interface
 *
 * Returns: the corresponding #GdaDictType
 */
GdaDictType *
gda_entity_field_get_dict_type (GdaEntityField *iface)
{
	g_return_val_if_fail (iface && GDA_IS_ENTITY_FIELD (iface), NULL);

	if (GDA_ENTITY_FIELD_GET_IFACE (iface)->get_dict_type)
		return (GDA_ENTITY_FIELD_GET_IFACE (iface)->get_dict_type) (iface);
	
	return NULL;
}

/**
 * gda_entity_field_set_dict_type
 * @iface: an object which implements the #GdaEntityField interface
 * @type: a GdaDictType to set to.
 *
 * Set the dict type of the object implementing the #GdaEntityField interface
 */
void
gda_entity_field_set_dict_type (GdaEntityField *iface, GdaDictType *type)
{
	g_return_if_fail (iface && GDA_IS_ENTITY_FIELD (iface));

	if (GDA_ENTITY_FIELD_GET_IFACE (iface)->set_dict_type)
		(GDA_ENTITY_FIELD_GET_IFACE (iface)->set_dict_type) (iface, type);
}

/**
 * gda_entity_field_get_g_type
 * @iface: an object which implements the #GdaEntityField interface
 *
 * Get the gda type of the object implementing the #GdaEntityField interface
 *
 * Returns: the corresponding #GType or #G_TYPE_INVALID if the gda type is unknown
 */
GType
gda_entity_field_get_g_type (GdaEntityField *iface)
{
	g_return_val_if_fail (iface && GDA_IS_ENTITY_FIELD (iface), G_TYPE_INVALID);

	if (GDA_ENTITY_FIELD_GET_IFACE (iface)->get_dict_type) {
		GdaDictType *type;	
		type = (GDA_ENTITY_FIELD_GET_IFACE (iface)->get_dict_type) (iface);

		if (type)
			return gda_dict_type_get_g_type (type);
		else {
			if (GDA_ENTITY_FIELD_GET_IFACE (iface)->get_g_type)
				return GDA_ENTITY_FIELD_GET_IFACE (iface)->get_g_type (iface);
			else
				return G_TYPE_INVALID;
		}
	}
	
	return G_TYPE_INVALID;
}


/**
 * gda_entity_field_get_name
 * @iface: an object which implements the #GdaEntityField interface
 *
 * Get the name of the object implementing the #GdaEntityField interface
 *
 * Returns: the name
 */
const gchar *
gda_entity_field_get_name (GdaEntityField *iface)
{
	g_return_val_if_fail (iface && GDA_IS_ENTITY_FIELD (iface), NULL);

	return gda_object_get_name (GDA_OBJECT (iface));
}

/**
 * gda_entity_field_get_description
 * @iface: an object which implements the #GdaEntityField interface
 *
 * Get the description of the object implementing the #GdaEntityField interface
 *
 * Returns: the description
 */
const gchar *
gda_entity_field_get_description (GdaEntityField *iface)
{
	g_return_val_if_fail (iface && GDA_IS_ENTITY_FIELD (iface), NULL);

	return gda_object_get_description (GDA_OBJECT (iface));
}



