/* GDA common library
 * Copyright (C) 2008 The GNOME Foundation.
 *
 * AUTHORS:
 *      Vivien Malerba <malerba@gnome-db.org>
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

#ifndef __GDA_ATTRIBUTES_MANAGER_H__
#define __GDA_ATTRIBUTES_MANAGER_H__

#include <glib-object.h>

G_BEGIN_DECLS

/* main struct */
typedef struct _GdaAttributesManager GdaAttributesManager;
typedef void (*GdaAttributesManagerFunc) (const gchar *att_name, const GValue *value, gpointer data);
typedef void (*GdaAttributesManagerSignal) (GObject *obj, const gchar *att_name, const GValue *value, gpointer data);

GdaAttributesManager *gda_attributes_manager_new         (gboolean for_objects, 
							  GdaAttributesManagerSignal signal_func, gpointer signal_data);
void                  gda_attributes_manager_free        (GdaAttributesManager *mgr);

void                  gda_attributes_manager_set         (GdaAttributesManager *mgr, gpointer ptr,
							  const gchar *att_name, const GValue *value);
void                  gda_attributes_manager_set_full    (GdaAttributesManager *mgr, gpointer ptr,
							  const gchar *att_name, const GValue *value, GDestroyNotify destroy);
const GValue         *gda_attributes_manager_get         (GdaAttributesManager *mgr, gpointer ptr, const gchar *att_name);
void                  gda_attributes_manager_copy        (GdaAttributesManager *from_mgr, gpointer *from, 
							  GdaAttributesManager *to_mgr, gpointer *to);
void                  gda_attributes_manager_clear       (GdaAttributesManager *mgr, gpointer ptr);
void                  gda_attributes_manager_foreach     (GdaAttributesManager *mgr, gpointer ptr, 
							  GdaAttributesManagerFunc func, gpointer data);

/**
 * GDA_ATTRIBUTE_TREE_NODE_UNKNOWN_CHILDREN:
 * This attribute, if %TRUE specifies that a tree node may or may not have any children nodes (value has a G_TYPE_BOOLEAN type).
 */
#define GDA_ATTRIBUTE_TREE_NODE_UNKNOWN_CHILDREN "__gda_attr_tnuchild"


/* possible predefined attribute names for gda_holder_get_attribute() or gda_column_get_attribute() */
#define GDA_ATTRIBUTE_DESCRIPTION "__gda_attr_descr" /* G_TYPE_STRING */
#define GDA_ATTRIBUTE_NAME "__gda_attr_name" /* G_TYPE_STRING */
#define GDA_ATTRIBUTE_NUMERIC_PRECISION "__gda_attr_numeric_precision" /* G_TYPE_INT */
#define GDA_ATTRIBUTE_NUMERIC_SCALE "__gda_attr_numeric_scale" /* G_TYPE_INT */
#define GDA_ATTRIBUTE_AUTO_INCREMENT "__gda_attr_autoinc" /* G_TYPE_BOOLEAN */
#define GDA_ATTRIBUTE_IS_DEFAULT "__gda_attr_is_default" /* G_TYPE_BOOLEAN */

G_END_DECLS

#endif
