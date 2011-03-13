/* 
 * Copyright (C) 2009 Vivien Malerba
 *
 * This Program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
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


#ifndef __CONNECTION_BINDING_PROPERTIES_H_
#define __CONNECTION_BINDING_PROPERTIES_H_

#include <gtk/gtk.h>
#include "browser-virtual-connection.h"
#include "decl.h"

G_BEGIN_DECLS

#define CONNECTION_TYPE_BINDING_PROPERTIES          (connection_binding_properties_get_type())
#define CONNECTION_BINDING_PROPERTIES(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, connection_binding_properties_get_type(), ConnectionBindingProperties)
#define CONNECTION_BINDING_PROPERTIES_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, connection_binding_properties_get_type (), ConnectionBindingPropertiesClass)
#define CONNECTION_IS_BINDING_PROPERTIES(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, connection_binding_properties_get_type ())

typedef struct _ConnectionBindingProperties ConnectionBindingProperties;
typedef struct _ConnectionBindingPropertiesPrivate ConnectionBindingPropertiesPrivate;
typedef struct _ConnectionBindingPropertiesClass ConnectionBindingPropertiesClass;

/* struct for the object's data */
struct _ConnectionBindingProperties
{
	GtkDialog                           object;
	ConnectionBindingPropertiesPrivate *priv;
};

/* struct for the object's class */
struct _ConnectionBindingPropertiesClass
{
	GtkDialogClass                      parent_class;
};

GType           connection_binding_properties_get_type               (void) G_GNUC_CONST;
GtkWidget      *connection_binding_properties_new_create             (BrowserConnection *bcnc);

GtkWidget      *connection_binding_properties_new_edit               (const BrowserVirtualConnectionSpecs *specs);

const BrowserVirtualConnectionSpecs *connection_binding_properties_get_specs (ConnectionBindingProperties *prop);

G_END_DECLS

#endif
