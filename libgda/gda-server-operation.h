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
#include <libxml/tree.h>

G_BEGIN_DECLS

#define GDA_TYPE_SERVER_OPERATION            (gda_server_operation_get_type())
#define GDA_SERVER_OPERATION(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_SERVER_OPERATION, GdaServerOperation))
#define GDA_SERVER_OPERATION_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_SERVER_OPERATION, GdaServerOperationClass))
#define GDA_IS_SERVER_OPERATION(obj)         (G_TYPE_CHECK_INSTANCE_TYPE(obj, GDA_TYPE_SERVER_OPERATION))
#define GDA_IS_SERVER_OPERATION_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GDA_TYPE_SERVER_OPERATION))

/*
 * Types of identified operations
 * all the providers don't implement them completely, though
 */
typedef enum {
	GDA_SERVER_OPERATION_CREATE_DB,
	GDA_SERVER_OPERATION_DROP_DB,
		
	GDA_SERVER_OPERATION_CREATE_TABLE,
	GDA_SERVER_OPERATION_DROP_TABLE,
	GDA_SERVER_OPERATION_CREATE_INDEX,
	GDA_SERVER_OPERATION_DROP_INDEX,

	GDA_SERVER_OPERATION_NB
} GdaServerOperationType;

typedef enum {
	GDA_SERVER_OPERATION_NODE_PARAMLIST,
	GDA_SERVER_OPERATION_NODE_DATA_MODEL,
	GDA_SERVER_OPERATION_NODE_PARAM,
	GDA_SERVER_OPERATION_NODE_SEQUENCE,
	GDA_SERVER_OPERATION_NODE_SEQUENCE_ITEM,

	GDA_SERVER_OPERATION_NODE_DATA_MODEL_COLUMN,
	GDA_SERVER_OPERATION_NODE_UNKNOWN
} GdaServerOperationNodeType;

typedef enum {
	GDA_SERVER_OPERATION_STATUS_OPTIONAL,
	GDA_SERVER_OPERATION_STATUS_REQUIRED,
	GDA_SERVER_OPERATION_STATUS_UNKNOWN
} GdaServerOperationNodeStatus;

typedef struct _GdaServerOperationNode {
	GdaServerOperationNodeType    type;
	GdaServerOperationNodeStatus  status;
	
	GdaParameterList             *plist;
	GdaDataModel                 *model;
	GdaColumn                    *column;
	GdaParameter                 *param; 
	gpointer                      priv;
} GdaServerOperationNode;

struct _GdaServerOperation {
	GObject                    object;
	GdaServerOperationPrivate *priv;
};

struct _GdaServerOperationClass {
	GObjectClass               parent_class;

	/* signals */
	void                     (*seq_item_added) (GdaServerOperation *op, const gchar *seq_path, gint item_index);
	void                     (*seq_item_remove) (GdaServerOperation *op, const gchar *seq_path, gint item_index);

};

GType                      gda_server_operation_get_type                (void);
GdaServerOperation        *gda_server_operation_new                     (GdaServerOperationType op_type, const gchar *xml_file);
GdaServerOperationType     gda_server_operation_get_op_type             (GdaServerOperation *op);
const gchar               *gda_server_operation_op_type_to_string       (GdaServerOperationType type);
GdaServerOperationNode    *gda_server_operation_get_node_info           (GdaServerOperation *op, const gchar *path_format, ...);

xmlNodePtr                 gda_server_operation_save_data_to_xml        (GdaServerOperation *op, GError **error);
gboolean                   gda_server_operation_load_data_from_xml      (GdaServerOperation *op, 
									 xmlNodePtr node, GError **error);

gchar**                    gda_server_operation_get_root_nodes          (GdaServerOperation *op);
GdaServerOperationNodeType gda_server_operation_get_node_type           (GdaServerOperation *op, const gchar *path,
								         GdaServerOperationNodeStatus *status);
gchar                     *gda_server_operation_get_node_parent         (GdaServerOperation *op, const gchar *path);
gchar                     *gda_server_operation_get_node_path_portion   (GdaServerOperation *op, const gchar *path);

const gchar               *gda_server_operation_get_sequence_name       (GdaServerOperation *op, const gchar *path);
guint                      gda_server_operation_get_sequence_size       (GdaServerOperation *op, const gchar *path);
guint                      gda_server_operation_get_sequence_max_size   (GdaServerOperation *op, const gchar *path);
guint                      gda_server_operation_get_sequence_min_size   (GdaServerOperation *op, const gchar *path);
gchar                    **gda_server_operation_get_sequence_item_names (GdaServerOperation *op, const gchar *path); 

guint                      gda_server_operation_add_item_to_sequence    (GdaServerOperation *op, const gchar *path);
gboolean                   gda_server_operation_del_item_from_sequence  (GdaServerOperation *op, const gchar *item_path);

const GValue              *gda_server_operation_get_value_at            (GdaServerOperation *op, const gchar *path_format, ...);
gboolean                   gda_server_operation_is_valid                (GdaServerOperation *op, const gchar *xml_file, GError **error);

G_END_DECLS

#endif
