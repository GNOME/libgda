/*
 * Copyright (C) 2008 - 2012 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2012 Daniel Espinosa <despinosa@src.gnome.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#ifndef __GDA_ATTRIBUTES_MANAGER_H__
#define __GDA_ATTRIBUTES_MANAGER_H__

#include <glib-object.h>

G_BEGIN_DECLS

/* possible predefined attribute names for gda_holder_get_attribute() or gda_column_get_attribute() */
/**
 * GDA_ATTRIBUTE_DESCRIPTION:
 * The corresponding attribute is the description of the object it refers to (value has a G_TYPE_STRING type).
 */
#define GDA_ATTRIBUTE_DESCRIPTION "__gda_attr_descr"

/**
 * GDA_ATTRIBUTE_NAME:
 * The corresponding attribute is the name of the object it refers to (value has a G_TYPE_STRING type).
 */
#define GDA_ATTRIBUTE_NAME "__gda_attr_name"

/**
 * GDA_ATTRIBUTE_NUMERIC_PRECISION:
 * The corresponding attribute is the number of significant digits of the object it refers to (value has a G_TYPE_INT type).
 */
#define GDA_ATTRIBUTE_NUMERIC_PRECISION "__gda_attr_numeric_precision"

/**
 * GDA_ATTRIBUTE_NUMERIC_SCALE:
 * The corresponding attribute is the number of significant digits to the right of the decimal point of the object it refers to (value has a G_TYPE_INT type).
 */
#define GDA_ATTRIBUTE_NUMERIC_SCALE "__gda_attr_numeric_scale"

/**
 * GDA_ATTRIBUTE_AUTO_INCREMENT:
 * The corresponding attribute specifies if the object it refers to is auto incremented (value has a G_TYPE_BOOLEAN type).
 */
#define GDA_ATTRIBUTE_AUTO_INCREMENT "__gda_attr_autoinc"

/**
 * GDA_ATTRIBUTE_IS_DEFAULT:
 * The corresponding attribute specifies if the object it refers to has its value to default (value has a G_TYPE_BOOLEAN type).
 */
#define GDA_ATTRIBUTE_IS_DEFAULT "__gda_attr_is_default"

/**
 * GDA_ATTRIBUTE_TREE_NODE_UNKNOWN_CHILDREN:
 * This attribute, if %TRUE specifies that a tree node may or may not have any children nodes (value has a G_TYPE_BOOLEAN type).
 */
#define GDA_ATTRIBUTE_TREE_NODE_UNKNOWN_CHILDREN "__gda_attr_tnuchild"


/**
 * SECTION:gda-attributes-manager
 * @short_description: Manager for lists of attributes
 * @title: Attributes manager
 * @stability: Stable
 *
 * he #GdaAttributesManager manages lists of named values (attributes) for the benefit of
 * others (objects or resources for which only a pointer is known). It is used internally by &LIBGDA;
 * whenever an object or a simple structure may have several attributes.
 *
 * The features are similar to those of the <link linkend="g-object-set-data">g_object_set_data()</link> and similar
 * but with the following major differences:
 * <itemizedlist>
 *  <listitem><para>it works with GObject objects and also with simple pointers to data</para></listitem>
 *  <listitem><para>attributes names are considered static (they are not copied) and so they must either be static strings or allocated strings which exist (unchanged) while an attribute uses it as name</para></listitem>
 *  <listitem><para>it is possible to iterate through the attributes</para></listitem>
 *  <listitem><para>the associated values are expected to be #GValue values</para></listitem>
 * </itemizedlist>
 *
 * Attibute names can be any string, but &LIBGDA; reserves some for its own usage, see below.
 *
 * The #GdaAttributesManager implements its own locking mechanism so it is thread-safe.
 *
 * 
 */

G_END_DECLS

#endif
