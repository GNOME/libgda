/* GDA library
 * Copyright (C) 2006 The GNOME Foundation.
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

#ifndef __GDA_SERVER_OPERATION_H__
#define __GDA_SERVER_OPERATION_H__

#include <glib-object.h>
#include <libgda/gda-decl.h>

G_BEGIN_DECLS

#define GDA_TYPE_SERVER_OPERATION            (gda_server_operation_get_type())
#define GDA_SERVER_OPERATION(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_SERVER_OPERATION, GdaServerOperation))
#define GDA_SERVER_OPERATION_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_SERVER_OPERATION, GdaServerOperationClass))
#define GDA_IS_SERVER_OPERATION(obj)         (G_TYPE_CHECK_INSTANCE_TYPE(obj, GDA_TYPE_SERVER_OPERATION))
#define GDA_IS_SERVER_OPERATION_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GDA_TYPE_SERVER_OPERATION))

typedef enum {
	GDA_SERVER_OPERATION_CREATE_TABLE,
	GDA_SERVER_OPERATION_DROP_TABLE,
	GDA_SERVER_OPERATION_CREATE_INDEX,
	GDA_SERVER_OPERATION_DROP_INDEX,
} GdaServerOperationType;

typedef enum {
	GDA_SERVER_OPERATION_NODE_PARAMLIST,
	GDA_SERVER_OPERATION_NODE_DATA_MODEL,
	GDA_SERVER_OPERATION_NODE_PARAM,
	GDA_SERVER_OPERATION_NODE_SEQUENCE
} GdaServerOperationNodeType;

struct _GdaServerOperation {
	GObject                   object;
	GdaServerOperationPrivate *priv;
};

struct _GdaServerOperationClass {
	GObjectClass               parent_class;
};

/* implements XML storage */

GType                  gda_server_operation_get_type (void);
GdaServerOperation    *gda_server_operation_new      (const gchar *xml_spec);
gboolean               gda_server_operation_is_valid (GdaServerOperation *op, GError **error);

GdaServerOperationNodeType gda_server_operation_get_node_type (GdaServerOperation *op, const gchar *path);
GdaServerOperationNodeType gda_server_operation_get_node_type_v (GdaServerOperation *op, const gchar **path_array);

GdaParameterList *gda_server_operation_get_node_plist (GdaServerOperation *op, const gchar *path);
GdaParameterList *gda_server_operation_get_node_plist_v (GdaServerOperation *op, const gchar **path_array);

GdaParameter *gda_server_operation_get_node_param (GdaServerOperation *op, const gchar *path);
GdaParameter *gda_server_operation_get_node_param_v (GdaServerOperation *op, const gchar **path_array);

GdaDataModel *gda_server_operation_get_node_datamodel (GdaServerOperation *op, const gchar *path);
GdaDataModel *gda_server_operation_get_node_datamodel_v (GdaServerOperation *op, const gchar **path_array);

gint gda_server_operation_get_node_seq_size (GdaServerOperation *op, const gchar *path);
gint gda_server_operation_get_node_seq_size_v (GdaServerOperation *op, const gchar **path_array);

G_END_DECLS

#endif
