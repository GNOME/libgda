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

#include <glib/gmessages.h>
#include <libgda/gda-server-operation.h>
#include <string.h>
#include <libxml/tree.h>
#include <glib/gi18n-lib.h>

#define PARENT_TYPE G_TYPE_OBJECT
#define CLASS(operation) (GDA_SERVER_OPERATION_CLASS (G_OBJECT_GET_CLASS (operation)))

static void gda_server_operation_class_init (GdaServerOperationClass *klass);
static void gda_server_operation_init       (GdaServerOperation *operation,
					    GdaServerOperationClass *klass);
static void gda_server_operation_dispose   (GObject *object);

static GObjectClass *parent_class = NULL;

typedef struct {
	GdaServerOperationNodeType type;
	union {
		GdaParameterList *plist;
		GdaDataModel     *model;
		GdaParameter     *param;
		GSList           *seq_list; /* list of Node */
	} d;
} Node;
#define NODE(x) ((Node*)(x))

static void node_destroy (Node *node);

struct _GdaServerOperationPrivate {
	xmlDocPtr    xml_spec;

	GSList      *allnodes; /* list of all the Node structures, referenced here only */
	GHashTable  *hashnodes; /* key = path, value = Node. Not referenced here */
};

/*
 * GdaServerOperation class implementation
 */
static void
gda_server_operation_class_init (GdaServerOperationClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->dispose = gda_server_operation_dispose;
}

static void
gda_server_operation_init (GdaServerOperation *operation,
			  GdaServerOperationClass *klass)
{
	g_return_if_fail (GDA_IS_SERVER_OPERATION (operation));

	operation->priv = g_new0 (GdaServerOperationPrivate, 1);
	operation->priv->allnodes = NULL;
	operation->priv->hashnodes = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
}

static void
gda_server_operation_dispose (GObject *object)
{
	GdaServerOperation *operation = (GdaServerOperation *) object;

	g_return_if_fail (GDA_IS_SERVER_OPERATION (operation));

	/* free memory */
	if (operation->priv) {
		g_hash_table_destroy (operation->priv->hashnodes);

		g_slist_foreach (operation->priv->allnodes, (GFunc) node_destroy, NULL);
		
		g_free (operation->priv);
		operation->priv = NULL;
	}

	/* chain to parent class */
	parent_class->dispose (object);
}

GType
gda_server_operation_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (GdaServerOperationClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_server_operation_class_init,
			NULL,
			NULL,
			sizeof (GdaServerOperation),
			0,
			(GInstanceInitFunc) gda_server_operation_init
		};
		type = g_type_register_static (PARENT_TYPE, "GdaServerOperation", &info, G_TYPE_FLAG_ABSTRACT);
	}
	return type;
}

static void
node_destroy (Node *node)
{
	switch (node->type) {
	case GDA_SERVER_OPERATION_NODE_PARAMLIST:
		break;
	case GDA_SERVER_OPERATION_NODE_DATA_MODEL:
		break;
	case GDA_SERVER_OPERATION_NODE_PARAM:
		break;
	case GDA_SERVER_OPERATION_NODE_SEQUENCE:
		break;
	}
}
