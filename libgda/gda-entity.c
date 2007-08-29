/* gda-entity.c
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

#include "gda-entity.h"
#include <libgda/gda-object.h>
#include <libgda/gda-entity-field.h>
#include "gda-parameter-list.h"
#include "gda-marshal.h"


/* signals */
enum
{
	FIELD_ADDED,
	FIELD_REMOVED,
	FIELD_UPDATED,
	FIELDS_ORDER_CHANGED,
	LAST_SIGNAL
};

static gint gda_entity_signals[LAST_SIGNAL] = { 0, 0, 0, 0 };

static void gda_entity_iface_init (gpointer g_class);

GType
gda_entity_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo info = {
			sizeof (GdaEntityIface),
			(GBaseInitFunc) gda_entity_iface_init,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) NULL,
			NULL,
			NULL,
			0,
			0,
			(GInstanceInitFunc) NULL
		};
		
		type = g_type_register_static (G_TYPE_INTERFACE, "GdaEntity", &info, 0);
		g_type_interface_add_prerequisite (type, GDA_TYPE_OBJECT);
	}
	return type;
}


static void
gda_entity_iface_init (gpointer g_class)
{
	static gboolean initialized = FALSE;

	if (! initialized) {
		gda_entity_signals[FIELD_ADDED] =
			g_signal_new ("field_added",
				      GDA_TYPE_ENTITY,
				      G_SIGNAL_RUN_FIRST,
				      G_STRUCT_OFFSET (GdaEntityIface, field_added),
				      NULL, NULL,
				      gda_marshal_VOID__OBJECT, G_TYPE_NONE,
				      1, GDA_TYPE_ENTITY_FIELD);
		gda_entity_signals[FIELD_REMOVED] =
			g_signal_new ("field_removed",
				      GDA_TYPE_ENTITY,
				      G_SIGNAL_RUN_FIRST,
				      G_STRUCT_OFFSET (GdaEntityIface, field_removed),
				      NULL, NULL,
				      gda_marshal_VOID__OBJECT, G_TYPE_NONE,
				      1, GDA_TYPE_ENTITY_FIELD);
		gda_entity_signals[FIELD_UPDATED] =
			g_signal_new ("field_updated",
				      GDA_TYPE_ENTITY,
				      G_SIGNAL_RUN_FIRST,
				      G_STRUCT_OFFSET (GdaEntityIface, field_updated),
				      NULL, NULL,
				      gda_marshal_VOID__OBJECT, G_TYPE_NONE,
				      1, GDA_TYPE_ENTITY_FIELD);
		gda_entity_signals[FIELDS_ORDER_CHANGED] =
			g_signal_new ("fields_order_changed",
				      GDA_TYPE_ENTITY,
				      G_SIGNAL_RUN_FIRST,
				      G_STRUCT_OFFSET (GdaEntityIface, fields_order_changed),
				      NULL, NULL,
				      gda_marshal_VOID__VOID, G_TYPE_NONE,
				      0);		
		initialized = TRUE;
	}
}

/**
 * gda_entity_has_field
 * @iface: an object implementing the #GdaEntity interface
 * @field: an object implementing the #GdaEntityField interface
 *
 * Tells if @field belongs to the @iface entity
 *
 * Returns: TRUE if @field belongs to the @iface entity
 */
gboolean
gda_entity_has_field (GdaEntity *iface, GdaEntityField *field)
{
	g_return_val_if_fail (iface && GDA_IS_ENTITY (iface), FALSE);

	if (GDA_ENTITY_GET_IFACE (iface)->has_field)
		return (GDA_ENTITY_GET_IFACE (iface)->has_field) (iface, field);
	else
		return FALSE;
}

/**
 * gda_entity_get_fields
 * @iface: an object implementing the #GdaEntity interface
 *
 * Get a new list containing all the #GdaEntityField objects held within the object
 * implementing the #GdaEntity interface.
 *
 * The returned list nodes are in the order in which the fields are within the entity.
 *
 * Returns: the new list.
 */
GSList *
gda_entity_get_fields (GdaEntity *iface)
{
	g_return_val_if_fail (iface && GDA_IS_ENTITY (iface), NULL);

	if (GDA_ENTITY_GET_IFACE (iface)->get_fields)
		return (GDA_ENTITY_GET_IFACE (iface)->get_fields) (iface);
	
	return NULL;
}

/**
 * gda_entity_get_n_fields
 * @iface: an object implementing the #GdaEntity interface
 *
 * Get the number of fields in @iface
 *
 * Returns: the number of fields, or -1 if an error occurred
 */
gint
gda_entity_get_n_fields (GdaEntity *iface)
{
	GSList *list;
	gint retval;

	g_return_val_if_fail (iface && GDA_IS_ENTITY (iface), -1);
	list = gda_entity_get_fields (iface);
	retval = g_slist_length (list);
	g_slist_free (list);
	return retval;
}

/**
 * gda_entity_get_field_by_name
 * @iface: an object implementing the #GdaEntity interface
 * @name:
 *
 * Get a #GdaEntityField using its name. The notion of "field name" is the
 * string returned by gda_entity_field_get_name() on each of the fields composing @iface.
 * However, if that definition does not return any field, then each particular
 * implementation of @iface may try to give an extra definition to the notion of 
 * "field name".
 *
 * For instance, in the case of the #GdaQuery object, the  gda_entity_field_get_name() is used
 * as a first try to find a field, and if that fails, then the object tries to find
 * fields from their SQL naming.
 *
 * In the case where there can be more than one field with the same name (depending on
 * @iface's implementation), then the returned value is %NULL.
 *
 * Returns: the requested #GdaEntityField or %NULL if the field cannot be found, or if
 *          more than one field has been found.
 */
GdaEntityField *
gda_entity_get_field_by_name (GdaEntity *iface, const gchar *name)
{
	g_return_val_if_fail (iface && GDA_IS_ENTITY (iface), NULL);

	if (GDA_ENTITY_GET_IFACE (iface)->get_field_by_name)
		return (GDA_ENTITY_GET_IFACE (iface)->get_field_by_name) (iface, name);
	
	return NULL;
}


/**
 * gda_entity_get_field_by_xml_id
 * @iface: an object implementing the #GdaEntity interface
 * @xml_id:
 *
 *
 * Returns: the requested GdaEntityField
 */
GdaEntityField *
gda_entity_get_field_by_xml_id (GdaEntity *iface, const gchar *xml_id)
{
	g_return_val_if_fail (iface && GDA_IS_ENTITY (iface), NULL);

	if (GDA_ENTITY_GET_IFACE (iface)->get_field_by_xml_id)
		return (GDA_ENTITY_GET_IFACE (iface)->get_field_by_xml_id) (iface, xml_id);
	
	return NULL;
}

/**
 * gda_entity_get_field_by_index
 * @iface: an object implementing the #GdaEntity interface
 * @index:
 *
 *
 * Returns: the requested GdaEntityField or NULL if the index is out of bounds
 */
GdaEntityField *
gda_entity_get_field_by_index (GdaEntity *iface, gint index)
{
	g_return_val_if_fail (iface && GDA_IS_ENTITY (iface), NULL);

	if (GDA_ENTITY_GET_IFACE (iface)->get_field_by_index)
		return (GDA_ENTITY_GET_IFACE (iface)->get_field_by_index) (iface, index);
	
	return NULL;
}

/**
 * gda_entity_get_field_index
 * @iface: an object implementing the #GdaEntity interface
 * @field: an object implementing the #GdaEntityField interface
 *
 * Get the position of the field in the given entity. Positions start at 0.
 * @field MUST be a visible field.
 *
 * Returns: the position or -1 if the field is not in the entity or is not visible
 */
gint
gda_entity_get_field_index (GdaEntity *iface, GdaEntityField *field)
{
	g_return_val_if_fail (iface && GDA_IS_ENTITY (iface), FALSE);

	if (GDA_ENTITY_GET_IFACE (iface)->get_field_index)
		return (GDA_ENTITY_GET_IFACE (iface)->get_field_index) (iface, field);	

	return FALSE;
}

/**
 * gda_entity_add_field
 * @iface: an object implementing the #GdaEntity interface
 * @field: an object implementing the #GdaEntityField interface to add
 *
 * Add @field to @iface's fields (at the end of the list)
 */
void
gda_entity_add_field (GdaEntity *iface, GdaEntityField *field)
{
	g_return_if_fail (iface && GDA_IS_ENTITY (iface));

	if (GDA_ENTITY_GET_IFACE (iface)->add_field)
		(GDA_ENTITY_GET_IFACE (iface)->add_field) (iface, field);
}

/**
 * gda_entity_add_field_before
 * @iface: an object implementing the #GdaEntity interface
 * @field: an object implementing the #GdaEntityField interface to add
 * @field_before: an object implementing the #GdaEntityField interface before which @field will be added, or %NULL
 *
 * Add @field to @iface's fields, before @field_before if it is not %NULL, 
 * or at the end if @field_before is %NULL.
 */
void
gda_entity_add_field_before (GdaEntity *iface, GdaEntityField *field, GdaEntityField *field_before)
{
	g_return_if_fail (iface && GDA_IS_ENTITY (iface));

	if (GDA_ENTITY_GET_IFACE (iface)->add_field_before)
		(GDA_ENTITY_GET_IFACE (iface)->add_field_before) (iface, field, field_before);
}

/**
 * gda_entity_swap_fields
 * @iface: an object implementing the #GdaEntity interface
 * @field1: an object implementing the #GdaEntityField interface
 * @field2: an object implementing the #GdaEntityField interface
 */
void
gda_entity_swap_fields (GdaEntity *iface, GdaEntityField *field1, GdaEntityField *field2)
{
	g_return_if_fail (iface && GDA_IS_ENTITY (iface));

	if (GDA_ENTITY_GET_IFACE (iface)->swap_fields)
		(GDA_ENTITY_GET_IFACE (iface)->swap_fields) (iface, field1, field2);
}

/**
 * gda_entity_remove_field
 * @iface: an object implementing the #GdaEntity interface
 * @field: an object implementing the #GdaEntityField interface to remove
 */
void
gda_entity_remove_field (GdaEntity *iface, GdaEntityField *field)
{
	g_return_if_fail (iface && GDA_IS_ENTITY (iface));

	if (GDA_ENTITY_GET_IFACE (iface)->remove_field)
		(GDA_ENTITY_GET_IFACE (iface)->remove_field) (iface, field);
}

/**
 * gda_entity_is_writable
 * @iface: an object implementing the #GdaEntity interface
 *
 * Tells if the real entity (the corresponding DBMS object) represented by @iface can be written to.
 *
 * Returns: TRUE if it is possible to write to @iface
 */
gboolean
gda_entity_is_writable (GdaEntity *iface)
{
	g_return_val_if_fail (iface && GDA_IS_ENTITY (iface), FALSE);

	if (GDA_ENTITY_GET_IFACE (iface)->is_writable)
		return (GDA_ENTITY_GET_IFACE (iface)->is_writable) (iface);	

	return FALSE;
}
