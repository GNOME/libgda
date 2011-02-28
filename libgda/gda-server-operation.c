/* GDA library
 * Copyright (C) 2006 - 2010 The GNOME Foundation.
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

#include <stdlib.h>
#include <glib.h>
#include <libgda/gda-marshal.h>
#include <libgda/gda-server-provider.h>
#include <libgda/gda-server-operation.h>
#include <libgda/gda-set.h>
#include <libgda/gda-holder.h>
#include <libgda/gda-connection.h>
#include <libgda/gda-config.h>
#include <libgda/gda-data-model-private.h>
#include <libgda/gda-data-model-import.h>
#include <libgda/gda-data-model-array.h>
#include "gda-util.h"
#include <string.h>
#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif
#include <glib/gi18n-lib.h>

extern gchar *gda_lang_locale;

#define CLASS(operation) (GDA_SERVER_OPERATION_CLASS (G_OBJECT_GET_CLASS (operation)))

static void gda_server_operation_class_init (GdaServerOperationClass *klass);
static void gda_server_operation_init       (GdaServerOperation *operation,
					    GdaServerOperationClass *klass);
static void gda_server_operation_dispose   (GObject *object);

static void gda_server_operation_set_property (GObject *object,
					       guint param_id,
					       const GValue *value,
					       GParamSpec *pspec);
static void gda_server_operation_get_property (GObject *object,
					       guint param_id,
					       GValue *value,
					       GParamSpec *pspec);

/* signals */
enum
{
	SEQUENCE_ITEM_ADDED,
	SEQUENCE_ITEM_REMOVE,
	LAST_SIGNAL
};

static gint gda_server_operation_signals[LAST_SIGNAL] = { 0, 0 };

/* properties */
enum
{
	PROP_0,
	PROP_CNC,
	PROP_PROV,
	PROP_OP_TYPE,
	PROP_SPEC_FILE
};

extern xmlDtdPtr gda_server_op_dtd;
static GObjectClass *parent_class = NULL;

typedef struct _Node {
	struct _Node                 *parent;
	GdaServerOperationNodeType    type;
	GdaServerOperationNodeStatus  status;
	gchar                        *path_name; /* NULL for GDA_SERVER_OPERATION_NODE_SEQUENCE_ITEM nodes */
	union {
		GdaSet               *plist;
		GdaDataModel         *model;
		GdaHolder            *param; 
		struct {
			GSList       *seq_tmpl; /* list of Node templates */
			guint         min_items;
			guint         max_items;
			GSList       *seq_items; /* list of Node of type GDA_SERVER_OPERATION_NODE_SEQUENCE_ITEM */
			gchar        *name;
			gchar        *descr;
			xmlNodePtr    xml_spec; /* references a op->priv->xml_spec_doc node,
						   for future instantiation of nodes */
		}                     seq;
		GSList               *seq_item_nodes; /* list of Node structures composing the item */
	}                             d;
} Node;
#define NODE(x) ((Node*)(x))

static void   node_destroy (GdaServerOperation *op, Node *node);
static Node  *node_new (Node *parent, GdaServerOperationNodeType type, const gchar *path);
static void   sequence_add_item (GdaServerOperation *op, Node *node);
static Node  *node_find (GdaServerOperation *op, const gchar *path);
static Node  *node_find_or_create (GdaServerOperation *op, const gchar *path);
static gchar *node_get_complete_path (GdaServerOperation *op, Node *node);
static void   clean_nodes_info_cache (GdaServerOperation *operation); 
struct _GdaServerOperationPrivate {
	GdaServerOperationType  op_type;
	gboolean                cnc_set;
	GdaConnection          *cnc;
	gboolean                prov_set;
	GdaServerProvider      *prov;

	xmlDocPtr               xml_spec_doc;
	GSList                 *sources; /* list of GdaDataModel as sources for the parameters */

	GSList                 *allnodes; /* list of all the Node structures, referenced here only */
	GSList                 *topnodes; /* list of the "/(*)" named nodes, not referenced here  */
	GHashTable             *info_hash; /* key = path, value = a GdaServerOperationNode */
};



/*
 * GdaServerOperation class implementation
 */
static void
gda_server_operation_class_init (GdaServerOperationClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	/* signals */
	/**
	 * GdaServerOperation::sequence-item-added
	 * @op: the #GdaServerOperation
	 * @seq_path: the path to the new sequence item
	 * @item_index: the index (starting from 0) of the new sequence item in the sequence
	 *
	 * Gets emitted whenever a new sequence item (from a sequence template) has been added
	 */
	gda_server_operation_signals[SEQUENCE_ITEM_ADDED] =
		g_signal_new ("sequence-item-added",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (GdaServerOperationClass, seq_item_added),
			      NULL, NULL,
			      _gda_marshal_VOID__STRING_INT, G_TYPE_NONE,
			      2, G_TYPE_STRING, G_TYPE_INT);
	/**
	 * GdaServerOperation::sequence-item-remove
	 * @op: the #GdaServerOperation
	 * @seq_path: the path to the sequence item to be removed
	 * @item_index: the index (starting from 0) of the sequence item in the sequence
	 *
	 * Gets emitted whenever a sequence item is about to be removed
	 */
	gda_server_operation_signals[SEQUENCE_ITEM_REMOVE] =
		g_signal_new ("sequence-item-remove",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (GdaServerOperationClass, seq_item_remove),
			      NULL, NULL,
			      _gda_marshal_VOID__STRING_INT, G_TYPE_NONE,
			      2, G_TYPE_STRING, G_TYPE_INT);

	klass->seq_item_added = NULL;
	klass->seq_item_remove = NULL;

	object_class->dispose = gda_server_operation_dispose;

	/* Properties */
	object_class->set_property = gda_server_operation_set_property;
	object_class->get_property = gda_server_operation_get_property;

	g_object_class_install_property (object_class, PROP_CNC,
					 g_param_spec_object ("connection", NULL, "Connection to use", 
							      GDA_TYPE_CONNECTION,
							      G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
	g_object_class_install_property (object_class, PROP_PROV,
					 g_param_spec_object ("provider", NULL,
							      "Database provider which created the object", 
							      GDA_TYPE_SERVER_PROVIDER,
							      G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
	g_object_class_install_property (object_class, PROP_SPEC_FILE,
					 g_param_spec_string ("spec-filename", NULL,
							      "XML file which contains the object's data structure", 
							      NULL, G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
	g_object_class_install_property (object_class, PROP_OP_TYPE,
					 g_param_spec_int ("op-type", NULL, "Type of operation to be done", 
							   0, GDA_SERVER_OPERATION_LAST - 1, 
							   0, G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}

static void
gda_server_operation_init (GdaServerOperation *operation,
			  G_GNUC_UNUSED GdaServerOperationClass *klass)
{
	g_return_if_fail (GDA_IS_SERVER_OPERATION (operation));

	operation->priv = g_new0 (GdaServerOperationPrivate, 1);
	operation->priv->allnodes = NULL;
	operation->priv->info_hash = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
}

static void
clean_nodes_info_cache (GdaServerOperation *operation)
{
	if (operation->priv->info_hash)
		g_hash_table_destroy (operation->priv->info_hash);

	operation->priv->info_hash = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
}


static void
gda_server_operation_dispose (GObject *object)
{
	GdaServerOperation *operation = (GdaServerOperation *) object;

	g_return_if_fail (GDA_IS_SERVER_OPERATION (operation));

	/* free memory */
	if (operation->priv) {
		if (operation->priv->info_hash)
			g_hash_table_destroy (operation->priv->info_hash);

		if (operation->priv->cnc)
			g_object_unref (operation->priv->cnc);
		if (operation->priv->prov)
			g_object_unref (operation->priv->prov);

		while (operation->priv->topnodes)
			node_destroy (operation, NODE (operation->priv->topnodes->data));
		g_assert (!operation->priv->allnodes);

		/* don't free operation->priv->xml_spec_doc */

		if (operation->priv->sources) {
			g_slist_foreach (operation->priv->sources, (GFunc) g_object_unref, NULL);
			g_slist_free (operation->priv->sources);
		}

		g_free (operation->priv);
		operation->priv = NULL;
	}

	/* chain to parent class */
	parent_class->dispose (object);
}

/* module error */
GQuark
gda_server_operation_error_quark (void)
{
        static GQuark quark;
        if (!quark)
                quark = g_quark_from_static_string ("gda_server_operation_error");
        return quark;
}

GType
gda_server_operation_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static GStaticMutex registering = G_STATIC_MUTEX_INIT;

		static const GTypeInfo info = {
			sizeof (GdaServerOperationClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_server_operation_class_init,
			NULL,
			NULL,
			sizeof (GdaServerOperation),
			0,
			(GInstanceInitFunc) gda_server_operation_init,
			0
		};

		g_static_mutex_lock (&registering);
		if (!type)
			type = g_type_register_static (G_TYPE_OBJECT, "GdaServerOperation", &info, 0);
		g_static_mutex_unlock (&registering);
	}
	return type;
}

/* create a new Node structure */
static Node *
node_new (Node *parent, GdaServerOperationNodeType type, const gchar *path)
{
	Node *node;

	node = g_new0 (Node, 1);
	node->parent = parent;
	node->type = type;
	node->status = GDA_SERVER_OPERATION_STATUS_REQUIRED;
	node->path_name = g_strdup (path);

	return node;
}

/* destroy a Node structure */
static void
node_destroy (GdaServerOperation *op, Node *node)
{
	switch (node->type) {
	case GDA_SERVER_OPERATION_NODE_PARAMLIST:
		g_object_unref (G_OBJECT (node->d.plist));
		break;
	case GDA_SERVER_OPERATION_NODE_DATA_MODEL:
		g_object_unref (G_OBJECT (node->d.model));
		break;
	case GDA_SERVER_OPERATION_NODE_PARAM:
		g_object_unref (G_OBJECT (node->d.param));
		break;
	case GDA_SERVER_OPERATION_NODE_SEQUENCE: {
		GSList *list;

		for (list = node->d.seq.seq_tmpl; list; list = list->next)
			node_destroy (op, NODE (list->data));
		g_slist_free (node->d.seq.seq_tmpl);

		for (list = node->d.seq.seq_items; list; list = list->next)
			node_destroy (op, NODE (list->data));
		g_slist_free (node->d.seq.seq_items);

		g_free (node->d.seq.name);
		g_free (node->d.seq.descr);
		break;
	}
	case GDA_SERVER_OPERATION_NODE_SEQUENCE_ITEM: {
		GSList *list;

		for (list = node->d.seq_item_nodes; list; list = list->next)
			node_destroy (op, NODE (list->data));
		g_slist_free (node->d.seq_item_nodes);
		break;
	}
	default:
		g_assert_not_reached ();
		break;
	}

	g_free (node->path_name);
	if (op) {
		op->priv->topnodes = g_slist_remove (op->priv->topnodes, node);
		op->priv->allnodes = g_slist_remove (op->priv->allnodes, node);
	}

	g_free (node);
}

/*
 * Find a Node from its full path
 */
static Node *
node_find (GdaServerOperation *op, const gchar *path)
{
	Node *node = NULL;
	GSList *list;

	if (!path || !*path || (*path != '/'))
		return NULL;

	for (list = op->priv->allnodes; list; list = list->next) {
		gchar *str;
		str = node_get_complete_path (op, NODE (list->data));
		if (!strcmp (str, path)) {
			node = NODE (list->data);
			g_free (str);
			break;
		}
		g_free (str);
	}
	/*g_print ("%s(%s) => %p\n", __FUNCTION__, path, node);*/
	return node;
}

/*
 * Find a node from its full path and if it does not exist, tries to 
 * create it (for sequences' items)
 */
static Node *
node_find_or_create (GdaServerOperation *op, const gchar *path)
{
	Node *node;
	
	if (!path || !*path || (*path != '/'))
		return NULL;

	node = node_find (op, path);
	if (!node) {
		gchar *cpath = g_strdup (path);
		gchar *ptr;
		gchar *root, *ext;
		
		/* separate @path to <root>/<ext> */
		ptr = cpath + strlen (cpath) - 1;
		while (*ptr && (*ptr != '/')) ptr--;
		*ptr = 0;

		root = cpath;
		ext = ptr+1;

		/* treatment */
		node = node_find_or_create (op, root);
		if (node) 
			switch (node->type) {
			case GDA_SERVER_OPERATION_NODE_SEQUENCE: {
				gint index;
				
				index = strtol (ext, &ptr, 10);
				if (ptr && *ptr)
					index = -1; /* could not convert array[i] to an int */
				if (index >= 0) {
					gint i;
					
					for (i = g_slist_length (node->d.seq.seq_items); i <= index; i++)
						sequence_add_item (op, node);
					node = node_find (op, path);
					g_assert (node);
				}
				break;
			}
			case GDA_SERVER_OPERATION_NODE_SEQUENCE_ITEM: {
				node = node_find (op, path);
				g_assert (node);
				break;
			}
			default:
				node = NULL;
				break;
		}
		g_free(cpath);
	}

	/*g_print ("# %s (%s) => %p\n", __FUNCTION__, path, node);*/

	return node;
}

/*
 * Computes the complete path of a node
 */
static gchar *
node_get_complete_path (G_GNUC_UNUSED GdaServerOperation *op, Node *node)
{
	GString *string;
	gchar *retval;
	Node *lnode;

	if (!node)
		return NULL;

	string = g_string_new ("");
	for (lnode = node; lnode; lnode = lnode->parent) {
		if (lnode->type == GDA_SERVER_OPERATION_NODE_SEQUENCE_ITEM) {
			gchar *str;

			g_assert (lnode->parent);
			g_assert (lnode->parent->type == GDA_SERVER_OPERATION_NODE_SEQUENCE);
			str = g_strdup_printf ("%d", g_slist_index (lnode->parent->d.seq.seq_items, lnode));
			g_string_prepend (string, str);
			g_free (str);
		}
		else 
			g_string_prepend (string, lnode->path_name);
		g_string_prepend_c (string, '/');
	}

	retval = string->str;
	g_string_free (string, FALSE);

	/*g_print ("%s(%p) => %s\n", __FUNCTION__, node, retval);*/
	return retval;
}

static GSList *load_xml_spec (GdaServerOperation *op, xmlNodePtr specnode, const gchar *root, GError **error);

/* add a new item to @node and inserts it into @op's private structures */
static void
sequence_add_item (GdaServerOperation *op, Node *node)
{
	gchar *path, *seq_path;
	Node *new_node;
	GSList *seq_item_nodes, *list;

	g_assert (node);
	g_assert (node->type == GDA_SERVER_OPERATION_NODE_SEQUENCE);

	seq_path = node_get_complete_path (op, node);
	path = g_strdup_printf ("%s/%d", seq_path, g_slist_length (node->d.seq.seq_items));

	seq_item_nodes = load_xml_spec (op, node->d.seq.xml_spec, path, NULL);
	g_assert (seq_item_nodes);

	new_node = node_new (node, GDA_SERVER_OPERATION_NODE_SEQUENCE_ITEM, NULL);
	op->priv->allnodes = g_slist_append (op->priv->allnodes, new_node);
	new_node->d.seq_item_nodes = NULL;
	new_node->status = node->status;
	node->d.seq.seq_items = g_slist_append (node->d.seq.seq_items, new_node);

	new_node->d.seq_item_nodes = seq_item_nodes;
	for (list = seq_item_nodes; list; list = list->next)
		((Node*) list->data)->parent = new_node;

	clean_nodes_info_cache (op);
#ifdef GDA_DEBUG_signal
	g_print (">> 'SEQUENCE_ITEM_ADDED' from %s\n", __FUNCTION__);
#endif
	g_signal_emit (G_OBJECT (op), gda_server_operation_signals [SEQUENCE_ITEM_ADDED], 0, 
		       seq_path, g_slist_length (node->d.seq.seq_items) - 1);
#ifdef GDA_DEBUG_signal
	g_print ("<< 'SEQUENCE_ITEM_ADDED' from %s\n", __FUNCTION__);
#endif	

	g_free (seq_path);
	g_free (path);
}

static void xml_validity_error_func (void *ctx, const char *msg, ...);
static gboolean use_xml_spec (GdaServerOperation *op, xmlDocPtr doc, const gchar *xmlfile);

static void
gda_server_operation_set_property (GObject *object,
				   guint param_id,
				   const GValue *value,
				   GParamSpec *pspec)
{
	GdaServerOperation *op;

	op = GDA_SERVER_OPERATION (object);
	if (op->priv) {
		switch (param_id) {
		case PROP_CNC:
			if (op->priv->cnc)
				g_object_unref (op->priv->cnc);

			op->priv->cnc = GDA_CONNECTION (g_value_get_object (value));
			op->priv->cnc_set = TRUE;

			if (op->priv->cnc) {
				g_object_ref (op->priv->cnc);

				if (gda_connection_get_provider (op->priv->cnc)) {
					if (op->priv->prov)
						g_object_unref (op->priv->prov);
					op->priv->prov = gda_connection_get_provider (op->priv->cnc);
					g_object_ref (op->priv->prov);
					op->priv->prov_set = TRUE;
				}
			}
			break;
		case PROP_PROV:
			if (g_value_get_object (value)) {
				if (op->priv->prov)
					g_object_unref (op->priv->prov);
				op->priv->prov = g_value_get_object(value);
				g_object_ref (op->priv->prov);
			}
			op->priv->prov_set = TRUE;
			break;
		case PROP_OP_TYPE:
			op->priv->op_type = g_value_get_int (value);
			break;
		case PROP_SPEC_FILE: {
			xmlDocPtr doc;
			const gchar *xmlfile;
			static GHashTable *doc_hash = NULL; /* key = file name, value = xmlDocPtr */

			xmlfile = g_value_get_string (value);
			if (!xmlfile)
				return;

			if (!doc_hash)
				doc_hash = g_hash_table_new_full (g_str_hash, g_str_equal, 
								  g_free, (GDestroyNotify) xmlFreeDoc);
			else {
				doc = g_hash_table_lookup (doc_hash, xmlfile);
				if (doc) {
					op->priv->xml_spec_doc = doc;
					break;
				}
			}

			if (! g_file_test (xmlfile, G_FILE_TEST_EXISTS)) {
				g_warning (_("GdaServerOperation: could not find file '%s'"), xmlfile);
				return;
			}
			
			doc = xmlParseFile (xmlfile);
			if (doc) {
				if (!use_xml_spec (op, doc, xmlfile))
					return;
				g_hash_table_insert (doc_hash, g_strdup (xmlfile), doc);
			}
			else {
				g_warning (_("GdaServerOperation: could not load file '%s'"), xmlfile);
				return;	
			}
			break;
		}
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
			break;
		}
	}

	if (!op->priv->topnodes && op->priv->xml_spec_doc && op->priv->cnc_set && op->priv->prov_set) {
		/* load XML file */
		GError *lerror = NULL;
		op->priv->topnodes = load_xml_spec (op, xmlDocGetRootElement (op->priv->xml_spec_doc), NULL, &lerror);
		if (!op->priv->topnodes) {
			g_warning (_("Could not load XML specifications: %s"),
				   lerror && lerror->message ? lerror->message : _("No detail"));
			if (lerror)
				g_error_free (lerror);
		}
	}
}
	
static void
gda_server_operation_get_property (GObject *object,
				   guint param_id,
				   GValue *value,
				   GParamSpec *pspec)
{
	GdaServerOperation *op;

	op = GDA_SERVER_OPERATION (object);
	if (op->priv) {
		switch (param_id) {
		case PROP_CNC:
			g_value_set_object (value, op->priv->cnc);
			break;
		case PROP_PROV:
			g_value_set_object (value, op->priv->prov);
			break;
		case PROP_OP_TYPE:
			g_value_set_int (value, op->priv->op_type);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
			break;
		}
	}
}

/*
 * Steals @doc (it is freed if necessary)
 */
static gboolean
use_xml_spec (GdaServerOperation *op, xmlDocPtr doc, const gchar *xmlfile)
{
	/* doc validation */
	xmlValidCtxtPtr validc;
	int xmlcheck;
	xmlDtdPtr old_dtd = NULL;
	
	validc = g_new0 (xmlValidCtxt, 1);
	validc->userData = op;
	validc->error = xml_validity_error_func;
	validc->warning = NULL;
	
	xmlcheck = xmlDoValidityCheckingDefaultValue;
	xmlDoValidityCheckingDefaultValue = 1;
	
	/* replace the DTD with ours */
	if (gda_server_op_dtd) {
		old_dtd = doc->intSubset;
		doc->intSubset = gda_server_op_dtd;
	}
#ifndef G_OS_WIN32
	if (doc->intSubset && !xmlValidateDocument (validc, doc)) {
		gchar *str;
		
		if (gda_server_op_dtd)
			doc->intSubset = old_dtd;
		xmlFreeDoc (doc);
		g_free (validc);
		str = g_object_get_data (G_OBJECT (op), "xmlerror");
		if (str) {
			if (xmlfile)
				g_warning (_("GdaServerOperation: file '%s' does not conform to DTD:\n%s"),
					   xmlfile, str);
			else
				g_warning (_("GdaServerOperation specification does not conform to DTD:\n%s"),
					   str);
			g_free (str);
			g_object_set_data (G_OBJECT (op), "xmlerror", NULL);
		}
		else {
			if (xmlfile)
				g_warning (_("GdaServerOperation: file '%s' does not conform to DTD"),
					   xmlfile);
			else
				g_warning (_("GdaServerOperation specification does not conform to DTD\n"));
		}
		
		xmlDoValidityCheckingDefaultValue = xmlcheck;
		xmlFreeDoc (doc);
		return FALSE;
	}
#endif
	
	xmlDoValidityCheckingDefaultValue = xmlcheck;
	g_free (validc);
	if (gda_server_op_dtd)
		doc->intSubset = old_dtd;
	op->priv->xml_spec_doc = doc;

	return TRUE;
}


/*
 * function called when an error occurred during the document validation
 */
static void
xml_validity_error_func (void *ctx, const char *msg, ...)
{
        va_list args;
        gchar *str, *str2, *newerr;
        GdaServerOperation *op;

        op = GDA_SERVER_OPERATION (ctx);
        str2 = g_object_get_data (G_OBJECT (op), "xmlerror");

        va_start (args, msg);
        str = g_strdup_vprintf (msg, args);
        va_end (args);

        if (str2) {
                newerr = g_strdup_printf ("%s\n%s", str2, str);
                g_free (str2);
        }
        else
                newerr = g_strdup (str);
        g_free (str);
        g_object_set_data (G_OBJECT (op), "xmlerror", newerr);
}

/*
 * Warning: the new nodes' parent is not set!
 */
static GSList *
load_xml_spec (GdaServerOperation *op, xmlNodePtr specnode, const gchar *root, GError **error)
{
	xmlNodePtr node;
	const gchar *lang = gda_lang_locale;
	GSList *retlist = NULL;
	Node *parent = NULL;

	if (root) 
		parent = node_find (op, root);
	
	g_assert (specnode);

	/* Parameters' sources, not mandatory: makes the op->priv->sources list */
	if (!op->priv->sources) {
		GSList *sources = NULL;
	
		node = specnode->children;
		while (node && (xmlNodeIsText (node) || strcmp ((gchar*)node->name, "sources"))) 
		node = node->next; 
		if (node && !strcmp ((gchar*)node->name, "sources")) {
			for (node = node->xmlChildrenNode; (node != NULL); node = node->next) {
				if (xmlNodeIsText (node)) 
					continue;
				
				if (!strcmp ((gchar*)node->name, "gda_array")) {
					GdaDataModel *model;
					GSList *errors;
					
					model = gda_data_model_import_new_xml_node (node);
					errors = gda_data_model_import_get_errors (GDA_DATA_MODEL_IMPORT (model));
					if (errors) {
						g_object_unref (model);
						if (sources) {
							g_slist_foreach (sources, (GFunc) g_object_unref, NULL);
							g_slist_free (sources);
							return NULL;
						}
					}
					else  {
						xmlChar *str;
						sources = g_slist_prepend (sources, model);
						str = xmlGetProp (node, (xmlChar*)"name");
						if (str) 
							g_object_set_data_full (G_OBJECT (model), "name", (gchar*)str, xmlFree);
					}
				}
			}
		}
		op->priv->sources = sources;
	}
	
	/* actual objects loading */
	node = specnode->children;
	while (node) {
		xmlChar *id, *this_lang;
		gchar *complete_path = NULL, *path_name = NULL;
		Node *opnode = NULL;
		Node *old_opnode;

		if (xmlNodeIsText (node)) {
			xmlNodePtr nextnode;
			nextnode = node->next;
			xmlUnlinkNode (node);
			xmlFreeNode (node);
			node = nextnode;
			continue;
		}

		/* don't care about entries for the wrong locale */
		this_lang = xmlGetProp(node, (xmlChar*)"lang");
		if (this_lang) {
			if (strncmp ((gchar*)this_lang, lang, strlen ((gchar*)this_lang))) {
				xmlNodePtr nextnode;
				xmlFree (this_lang);
				nextnode = node->next;
				xmlUnlinkNode (node);
				xmlFreeNode (node);
				node = nextnode;
				continue;
			}

			xmlFree (this_lang);
		}

		id = xmlGetProp (node, BAD_CAST "id");
		if (!id) {
			node = node->next;
			continue;
		}

		complete_path = g_strdup_printf ("%s/%s", root ? root : "", id);
		path_name = g_strdup ((gchar*)id);
		xmlFree (id);

		old_opnode = node_find (op, complete_path);
		if (old_opnode) {
			node_destroy (op, old_opnode);
			retlist = g_slist_remove (retlist, old_opnode);
		}

		/* GDA_SERVER_OPERATION_NODE_PARAMLIST */
		if (!strcmp ((gchar*)node->name, "parameters")) {
			GdaSet *plist;

			plist = gda_set_new_from_spec_node (node, error);
			if (plist) {
				opnode = node_new (parent, GDA_SERVER_OPERATION_NODE_PARAMLIST, path_name);
				opnode->d.plist = plist;
			}
		}
		/* GDA_SERVER_OPERATION_NODE_DATA_MODEL */
		else if (!strcmp ((gchar*)node->name, "gda_array")) {
			GdaDataModel *import;

			import = gda_data_model_import_new_xml_node (node);
			if (import) {
				GdaDataModel *model;
				model = (GdaDataModel*) gda_data_model_array_copy_model (import, NULL);
				opnode = node_new (parent, GDA_SERVER_OPERATION_NODE_DATA_MODEL, path_name);
				opnode->d.model = model;
				g_object_unref (import);
			}
		}

		/* GDA_SERVER_OPERATION_NODE_SEQUENCE */
		else if (!strcmp ((gchar*)node->name, "sequence")) {
			GSList *seq_tmpl = NULL;
			xmlChar *prop;

			opnode = node_new (parent, GDA_SERVER_OPERATION_NODE_SEQUENCE, path_name);
			opnode->d.seq.seq_tmpl = NULL;
			opnode->d.seq.min_items = 0;
			opnode->d.seq.max_items = G_MAXUINT;
			opnode->d.seq.seq_items = NULL;
			opnode->d.seq.xml_spec = node;
			
			prop = xmlGetProp(node, (xmlChar*)"name");
			if (prop) {
				opnode->d.seq.name = g_strdup ((gchar*)prop);
				xmlFree (prop);
			}
			
			prop = xmlGetProp(node, (xmlChar*)"descr");
			if (prop) {
				opnode->d.seq.descr = g_strdup ((gchar*)prop);
				xmlFree (prop);
			}

			
			prop = xmlGetProp(node, (xmlChar*)"minitems");
			if (prop) {
				opnode->d.seq.min_items = atoi ((gchar*)prop); /* Flawfinder: ignore */
				if (opnode->d.seq.min_items < 0)
					opnode->d.seq.min_items = 0;
				xmlFree (prop);
			}

			prop = xmlGetProp(node, (xmlChar*)"maxitems");
			if (prop) {
				opnode->d.seq.max_items = atoi ((gchar*)prop); /* Flawfinder: ignore */
				if (opnode->d.seq.max_items < opnode->d.seq.min_items)
					opnode->d.seq.max_items = opnode->d.seq.min_items;
				xmlFree (prop);
			}

			seq_tmpl = load_xml_spec (op, node, complete_path, error);
			if (! seq_tmpl) {
				node_destroy (NULL, opnode);
				opnode = NULL;
			}
			else
				opnode->d.seq.seq_tmpl = seq_tmpl;
		}

		/* GDA_SERVER_OPERATION_NODE_PARAM */
		else if (!strcmp ((gchar*)node->name, "parameter")) {
			GdaHolder *param = NULL;
			xmlChar *gdatype;

			/* find data type and create GdaHolder */
			gdatype = xmlGetProp (node, BAD_CAST "gdatype");
			param = GDA_HOLDER (g_object_new (GDA_TYPE_HOLDER,
							  "g-type", 
							  gdatype ? gda_g_type_from_string ((gchar*) gdatype) : G_TYPE_STRING,
							  NULL));
			if (gdatype)
				xmlFree (gdatype);
			
			/* set parameter's attributes */
			if (gda_utility_holder_load_attributes (param, node, op->priv->sources, error)) {
				opnode = node_new (parent, GDA_SERVER_OPERATION_NODE_PARAM, path_name);
				opnode->d.param = param;
			}
		}
		else {
			node = node->next;
			g_free (path_name);
			continue;
		}
		
		/* really insert the new Node, and set its status */
		if (opnode) {
			xmlChar *status;

			/* insert */
			op->priv->allnodes = g_slist_append (op->priv->allnodes, opnode);
			retlist = g_slist_append (retlist, opnode);
			/*g_print ("+ %s (node's path = %s) %p\n", complete_path, opnode->path_name, opnode);*/

			/* status */
			status = xmlGetProp (node, BAD_CAST "status");
			if (status) {
				if (!strcmp ((gchar*)status, "OPT"))
					opnode->status = GDA_SERVER_OPERATION_STATUS_OPTIONAL;
				xmlFree (status);
			}

			if (opnode->type == GDA_SERVER_OPERATION_NODE_SEQUENCE) {
				/* add sequence items if necessary */
				if (opnode->d.seq.min_items > 0) {
					guint i;
					
					for (i = 0; i < opnode->d.seq.min_items; i++)
						gda_server_operation_add_item_to_sequence (op, complete_path);
				}
			}
		}
		else {
			g_free (path_name);
			g_free (complete_path);
			g_slist_free (retlist);
			return NULL;
		}
		g_free (path_name);
		g_free (complete_path);
		node = node->next;
	}

	return retlist;
}

/*
 * @xml_spec: the specifications for the GdaServerOperation object to create as a string
 * Internal function
 */
GdaServerOperation *
_gda_server_operation_new_from_string (GdaServerOperationType op_type, 
				       const gchar *xml_spec)
{
	GObject *obj;
	xmlDocPtr doc;
	GdaServerOperation *op;

	doc = xmlParseMemory (xml_spec, strlen (xml_spec) + 1);
	if (!doc)
		return NULL;
	obj = g_object_new (GDA_TYPE_SERVER_OPERATION, "op-type", op_type, NULL);
	op = GDA_SERVER_OPERATION (obj);
	use_xml_spec (op, doc, NULL);

	if (!op->priv->topnodes && op->priv->xml_spec_doc && op->priv->cnc_set && op->priv->prov_set) {
		/* load XML file */
		GError *lerror = NULL;
		op->priv->topnodes = load_xml_spec (op, xmlDocGetRootElement (op->priv->xml_spec_doc), NULL, &lerror);
		if (!op->priv->topnodes) {
			g_warning (_("Could not load XML specifications: %s"),
				   lerror && lerror->message ? lerror->message : _("No detail"));
			if (lerror)
				g_error_free (lerror);
		}
	}

	return op;
}


/**
 * gda_server_operation_new:
 * @op_type: type of operation
 * @xml_file: a file which has the specifications for the GdaServerOperation object to create
 *
 * IMPORTANT NOTE: Using this funtion is not the recommended way of creating a #GdaServerOperation object, the
 * correct way is to use gda_server_provider_create_operation(); this method is reserved for the database provider's
 * implementation.
 *
 * Creates a new #GdaServerOperation object from the @xml_file specifications
 *
 * The @xml_file must respect the DTD described in the "libgda-server-operation.dtd" file: its top
 * node must be a &lt;serv_op&gt; tag.
 *
 * Returns: a new #GdaServerOperation object
 */
GdaServerOperation *
gda_server_operation_new (GdaServerOperationType op_type, const gchar *xml_file)
{
	GObject *obj;

	obj = g_object_new (GDA_TYPE_SERVER_OPERATION, "op-type", op_type, 
			    "spec-filename", xml_file, NULL);
#ifdef GDA_DEBUG_NO
	{
		g_print ("New GdaServerOperation:\n");
		xmlNodePtr node;
		node = gda_server_operation_save_data_to_xml ((GdaServerOperation *) obj, NULL);
		xmlDocPtr doc;
		doc = xmlNewDoc ("1.0");
		xmlDocSetRootElement (doc, node);
		xmlDocDump (stdout, doc);
		xmlFreeDoc (doc);
	}
#endif
	return (GdaServerOperation *) obj;
}

/**
 * gda_server_operation_get_node_info: (skip)
 * @op: a #GdaServerOperation object
 * @path_format: a complete path to a node (starting with "/") as a format string, similar to g_strdup_printf()'s argument
 * @...: the arguments to insert into the format string
 *
 * Get information about the node identified by @path. The returned #GdaServerOperationNode structure can be 
 * copied but not modified; it may change or cease to exist if @op changes
 *
 * Returns: (transfer none) (allow-none): a #GdaServerOperationNode structure, or %NULL if the node was not found
 */
GdaServerOperationNode *
gda_server_operation_get_node_info (GdaServerOperation *op, const gchar *path_format, ...)
{
	GdaServerOperationNode *info_node;
	Node *node;
	gchar *path;
	va_list args;

	g_return_val_if_fail (GDA_IS_SERVER_OPERATION (op), NULL);
	g_return_val_if_fail (op->priv, NULL);

	/* build path */
	va_start (args, path_format);
	path = g_strdup_vprintf (path_format, args);
	va_end (args);

	/* use path */
	info_node = g_hash_table_lookup (op->priv->info_hash, path);
	if (info_node) {
		g_free (path);
		return info_node;
	}

	/* compute a new GdaServerOperationNode */
	node = node_find (op, path);
	if (node) {
		info_node = g_new0 (GdaServerOperationNode, 1);
		info_node->priv = node;
		info_node->type = node->type;
		info_node->status = node->status;
		switch (node->type) {
		case GDA_SERVER_OPERATION_NODE_PARAMLIST:
			info_node->plist = node->d.plist;
			break;
		case GDA_SERVER_OPERATION_NODE_DATA_MODEL:
			info_node->model = node->d.model;
			break;
		case GDA_SERVER_OPERATION_NODE_PARAM:
			info_node->param = node->d.param;
			break;
		default:
			break;
		}
	}
	else {
		/* try to see if the "parent" is a real node */
		gchar *str;
		gchar *extension;
		
		str = gda_server_operation_get_node_parent (op, path);
		if (str) {
			node = node_find (op, str);
			if (node && (node->type != GDA_SERVER_OPERATION_NODE_PARAMLIST) &&
			    (node->type != GDA_SERVER_OPERATION_NODE_DATA_MODEL))
				node = NULL; /* ignore node */
			g_free (str);
		}
		if (node && (node->type == GDA_SERVER_OPERATION_NODE_PARAMLIST)) {
			GdaHolder *param;
			extension = gda_server_operation_get_node_path_portion (op, path);
			param = gda_set_get_holder (node->d.plist, extension);
			g_free (extension);

			if (param) {
				info_node = g_new0 (GdaServerOperationNode, 1);
				info_node->type = GDA_SERVER_OPERATION_NODE_PARAM;
				info_node->status = node->status;
				info_node->param = param;
			}
		}
		if (node && (node->type == GDA_SERVER_OPERATION_NODE_DATA_MODEL)) {
			GdaColumn *column = NULL;

			extension = gda_server_operation_get_node_path_portion (op, path);
			if (extension && (*extension == '@')) {
				gint i, nbcols;
				GdaDataModel *model;

				model = node->d.model;
				nbcols = gda_data_model_get_n_columns (model);
				for (i = 0; (i<nbcols) && !column; i++) {
					gchar *colid = NULL;
					column = gda_data_model_describe_column (model, i);
					g_object_get (G_OBJECT (column), "id", &colid, NULL);
					if (!colid || strcmp (colid, extension +1))
						column = NULL;
					g_free(colid);
				}
			}
			g_free (extension);
			if (column) {
				info_node = g_new0 (GdaServerOperationNode, 1);
				info_node->type = GDA_SERVER_OPERATION_NODE_DATA_MODEL_COLUMN;
				info_node->status = node->status;
				info_node->column = column;
				info_node->model = node->d.model;
			}
		}
	}

	if (info_node)
		g_hash_table_insert (op->priv->info_hash, g_strdup (path), info_node);

	g_free (path);
	return info_node;
}

/**
 * gda_server_operation_get_op_type:
 * @op: a #GdaServerOperation object
 * 
 * Get the type of operation @op is for
 *
 * Returns: a #GdaServerOperationType enum
 */
GdaServerOperationType
gda_server_operation_get_op_type (GdaServerOperation *op)
{
	g_return_val_if_fail (GDA_IS_SERVER_OPERATION (op), 0);
	g_return_val_if_fail (op->priv, 0);

	return op->priv->op_type;
}

/**
 * gda_server_operation_op_type_to_string:
 * @type: a #GdaServerOperationType value
 * 
 * Get a string version of @type
 *
 * Returns: (transfer none): a non %NULL string (do not free or modify)
 */
const gchar *
gda_server_operation_op_type_to_string (GdaServerOperationType type)
{
	switch (type) {
	case GDA_SERVER_OPERATION_CREATE_DB:
		return "CREATE_DB";
	case GDA_SERVER_OPERATION_DROP_DB:
		return "DROP_DB";
	case GDA_SERVER_OPERATION_CREATE_TABLE:
		return "CREATE_TABLE";
	case GDA_SERVER_OPERATION_DROP_TABLE:
		return "DROP_TABLE";
        case GDA_SERVER_OPERATION_CREATE_INDEX:
		return "CREATE_INDEX";
        case GDA_SERVER_OPERATION_DROP_INDEX:
		return "DROP_INDEX";
        case GDA_SERVER_OPERATION_RENAME_TABLE:
		return "RENAME_TABLE";
        case GDA_SERVER_OPERATION_COMMENT_TABLE:
		return "COMMENT_TABLE";
        case GDA_SERVER_OPERATION_ADD_COLUMN:
		return "ADD_COLUMN";
        case GDA_SERVER_OPERATION_DROP_COLUMN:
		return "DROP_COLUMN";
        case GDA_SERVER_OPERATION_COMMENT_COLUMN:
		return "COMMENT_COLUMN";
	case GDA_SERVER_OPERATION_CREATE_VIEW:
		return "CREATE_VIEW";
	case GDA_SERVER_OPERATION_DROP_VIEW:
		return "DROP_VIEW";
	case GDA_SERVER_OPERATION_CREATE_USER:
		return "CREATE_USER";
	case GDA_SERVER_OPERATION_DROP_USER:
		return "DROP_USER";
	case GDA_SERVER_OPERATION_ALTER_USER:
		return "ALTER_USER";
	default:
		g_error (_("Non handled GdaServerOperationType, please report error to "
			   "http://bugzilla.gnome.org/ for the \"libgda\" product"));
		return "";
	}
}

/**
 * gda_server_operation_string_to_op_type:
 * @str: a string
 *
 * Performs the reverse of gda_server_operation_op_type_to_string()
 *
 * Returns: the #GdaServerOperationType represented by @str, or #G_MAXINT if @str is not a valid representation
 * of a #GdaServerOperationType
 *
 * Since: 4.2
 */
GdaServerOperationType
gda_server_operation_string_to_op_type (const gchar *str)
{
	GdaServerOperationType operation_type = G_MAXINT;
	g_return_val_if_fail (str && *str, G_MAXINT);

	if (! g_ascii_strcasecmp (str, "CREATE_DB"))
		operation_type = GDA_SERVER_OPERATION_CREATE_DB;
	else if	(! g_ascii_strcasecmp (str, "DROP_DB"))
		operation_type = GDA_SERVER_OPERATION_DROP_DB;
	else if (! g_ascii_strcasecmp (str, "CREATE_TABLE"))
		operation_type = GDA_SERVER_OPERATION_CREATE_TABLE;
	else if (! g_ascii_strcasecmp (str, "DROP_TABLE"))
		operation_type = GDA_SERVER_OPERATION_DROP_TABLE;
	else if (! g_ascii_strcasecmp (str, "CREATE_INDEX"))
		operation_type = GDA_SERVER_OPERATION_CREATE_INDEX;
	else if (! g_ascii_strcasecmp (str, "DROP_INDEX"))
		operation_type = GDA_SERVER_OPERATION_DROP_INDEX;
	else if (! g_ascii_strcasecmp (str, "RENAME_TABLE"))
		operation_type = GDA_SERVER_OPERATION_RENAME_TABLE;
	else if (! g_ascii_strcasecmp (str, "COMMENT_TABLE"))
		operation_type = GDA_SERVER_OPERATION_COMMENT_TABLE;
	else if (! g_ascii_strcasecmp (str, "ADD_COLUMN"))
		operation_type = GDA_SERVER_OPERATION_ADD_COLUMN;
	else if (! g_ascii_strcasecmp (str, "DROP_COLUMN"))
		operation_type = GDA_SERVER_OPERATION_DROP_COLUMN;
	else if (! g_ascii_strcasecmp (str, "COMMENT_COLUMN"))
		operation_type = GDA_SERVER_OPERATION_COMMENT_COLUMN;
	else if (! g_ascii_strcasecmp (str, "CREATE_VIEW"))
		operation_type = GDA_SERVER_OPERATION_CREATE_VIEW;
	else if (! g_ascii_strcasecmp (str, "DROP_VIEW"))
		operation_type = GDA_SERVER_OPERATION_DROP_VIEW;
	else if (! g_ascii_strcasecmp (str, "CREATE_USER"))
		operation_type = GDA_SERVER_OPERATION_CREATE_USER;
	else if (! g_ascii_strcasecmp (str, "DROP_USER"))
		operation_type = GDA_SERVER_OPERATION_DROP_USER;
	else if (! g_ascii_strcasecmp (str, "ALTER_USER"))
		operation_type = GDA_SERVER_OPERATION_ALTER_USER;

	return operation_type;
}

static gboolean node_save (GdaServerOperation *op, Node *opnode, xmlNodePtr parent);

/**
 * gda_server_operation_save_data_to_xml: (skip)
 * @op: a #GdaServerOperation object
 * @error: (allow-none): a place to store errors or %NULL
 * 
 * Creates a new #xmlNodePtr tree which can be used to save the #op object. This
 * XML structure can then be saved to disk if necessary. Use xmlFreeNode to free
 * the associated memory when not needed anymore.
 *
 * Returns: (transfer full): a new #xmlNodePtr structure, or %NULL
 */
xmlNodePtr
gda_server_operation_save_data_to_xml (GdaServerOperation *op, G_GNUC_UNUSED GError **error)
{
	xmlNodePtr topnode = NULL;
	GSList *list;

	g_return_val_if_fail (GDA_IS_SERVER_OPERATION (op), NULL);
	g_return_val_if_fail (op->priv, NULL);

	topnode = xmlNewNode (NULL, BAD_CAST "serv_op_data");
	xmlSetProp (topnode, BAD_CAST "type", 
		    BAD_CAST gda_server_operation_op_type_to_string (gda_server_operation_get_op_type (op)));

	for (list = op->priv->topnodes; list; list = list->next) {
		if (!node_save (op, NODE (list->data), topnode)) {
			xmlFreeNode (topnode);
			topnode = NULL;
			break;
		}
	}

	return topnode;
}

static gboolean
node_save (GdaServerOperation *op, Node *opnode, xmlNodePtr parent)
{
	gboolean retval = TRUE;
	xmlNodePtr node;
	GSList *list;
	gchar *complete_path;
	g_assert (opnode);

	complete_path = node_get_complete_path (op, opnode);
	switch (opnode->type) {
	case GDA_SERVER_OPERATION_NODE_PARAMLIST:
		for (list = opnode->d.plist->holders; list; list = list->next) {
			gchar *path;
			const GValue *value;
			gchar *str;

			value = gda_holder_get_value (GDA_HOLDER (list->data));
			if (!value || gda_value_is_null ((GValue *) value))
				str = NULL;
			else {
				if (G_VALUE_TYPE (value) == G_TYPE_BOOLEAN)
					str = g_strdup (g_value_get_boolean (value) ? "TRUE" : "FALSE");
				else
					str = gda_value_stringify (value);
			}
			node = xmlNewTextChild (parent, NULL, BAD_CAST "op_data", (xmlChar*)str);
			g_free (str);

			path = g_strdup_printf ("%s/%s", complete_path, gda_holder_get_id (GDA_HOLDER (list->data)));
			xmlSetProp(node, (xmlChar*)"path", (xmlChar*)path);
			g_free (path);
		}
		break;
	case GDA_SERVER_OPERATION_NODE_DATA_MODEL:
		node = xmlNewChild (parent, NULL, BAD_CAST "op_data", NULL);
		xmlSetProp(node, (xmlChar*)"path", (xmlChar*)complete_path);
		if (!gda_utility_data_model_dump_data_to_xml (opnode->d.model, node, NULL, 0, NULL, 0, TRUE))
			retval = FALSE;
		break;
	case GDA_SERVER_OPERATION_NODE_PARAM: {
		const GValue *value;
		gchar *str;
		
		value = gda_holder_get_value (opnode->d.param);
		if (!value || gda_value_is_null ((GValue *) value))
			str = NULL;
		else {
			if (G_VALUE_TYPE (value) == G_TYPE_BOOLEAN)
				str = g_strdup (g_value_get_boolean (value) ? "TRUE" : "FALSE");
			else
				str = gda_value_stringify (value);
		}
		node = xmlNewTextChild (parent, NULL, BAD_CAST "op_data", (xmlChar*)str);
		g_free (str);
		xmlSetProp(node, (xmlChar*)"path", (xmlChar*)complete_path);
		break;
	}
	case GDA_SERVER_OPERATION_NODE_SEQUENCE: {
		GSList *list;
		
		for (list =  opnode->d.seq.seq_items; list; list = list->next) 
			if (!node_save (op, NODE (list->data), parent)) {
				retval = FALSE;
				break;
			}
		break;
	}
	case GDA_SERVER_OPERATION_NODE_SEQUENCE_ITEM: {
		GSList *list;
		
		for (list =  opnode->d.seq_item_nodes; list; list = list->next) 
			if (! node_save (op, NODE (list->data), parent)) {
				retval = FALSE;
				break;
			}
		break;
	}
	default:
		g_assert_not_reached ();
	}

	g_free (complete_path);
	return retval;
}

/**
 * gda_server_operation_load_data_from_xml:
 * @op: a #GdaServerOperation object
 * @node: a #xmlNodePtr
 * @error: (allow-none): a place to store errors or %NULL
 * 
 * Loads the contents of @node into @op. The XML tree passed through the @node
 * argument must correspond to an XML tree saved using gda_server_operation_save_data_to_xml().
 *
 * Returns: %TRUE if no error occurred
 */
gboolean
gda_server_operation_load_data_from_xml (GdaServerOperation *op, xmlNodePtr node, GError **error)
{
	xmlNodePtr cur;

	g_return_val_if_fail (GDA_IS_SERVER_OPERATION (op), FALSE);
	g_return_val_if_fail (op->priv, FALSE);
	if (!node)
		return FALSE;

	/* remove any sequence items */
	GSList *list;
	list = op->priv->allnodes;
	while (list) {
		Node *node = NODE (list->data);
		if ((node->type == GDA_SERVER_OPERATION_NODE_SEQUENCE) && node->d.seq.seq_items) {
			gchar *seq_path;
			
			seq_path = node_get_complete_path (op, node);
			while (node->d.seq.seq_items) {
#ifdef GDA_DEBUG_signal
				g_print (">> 'SEQUENCE_ITEM_REMOVE' from %s\n", __FUNCTION__);
#endif
				g_signal_emit (G_OBJECT (op), gda_server_operation_signals [SEQUENCE_ITEM_REMOVE], 0, 
					       seq_path, 0);
#ifdef GDA_DEBUG_signal
				g_print ("<< 'SEQUENCE_ITEM_REMOVE' from %s\n", __FUNCTION__);
#endif	
				node_destroy (op, NODE (node->d.seq.seq_items->data));
				node->d.seq.seq_items = g_slist_delete_link (node->d.seq.seq_items, node->d.seq.seq_items);
			}
			g_free (seq_path);
			list = op->priv->allnodes;
		}
		else
			list = list->next;
	}

	/* actual data loading */
	if (strcmp ((gchar*)node->name, "serv_op_data")) {
		g_set_error (error, 0, 0,
			     _("Expected tag <%s>, got <%s>"), "serv_op_data", node->name);
		return FALSE;
	}
	
	cur = node->children;
	while (cur) {
		xmlChar *prop;
		if (xmlNodeIsText (cur)) {
			cur = cur->next;
			continue;
		}

		if (strcmp ((gchar*)cur->name, "op_data")) {
			g_set_error (error, 0, 0,
				     _("Expected tag <%s>, got <%s>"), "op_data", cur->name);
			return FALSE;
		}

		prop = xmlGetProp(cur, (xmlChar*)"path");
		if (prop) {
			Node *opnode;
			gchar *extension = NULL;
			gboolean allok = TRUE;

			opnode = node_find_or_create (op, (gchar*)prop);
			if (!opnode) {
				/* try to see if the "parent" is a real node */
				gchar *str;

				str = gda_server_operation_get_node_parent (op, (gchar*)prop);
				if (str) {
					opnode = node_find (op, str);
					if (opnode && (opnode->type != GDA_SERVER_OPERATION_NODE_PARAMLIST))
						opnode = NULL; /* ignore opnode */
					g_free (str);
				}
				if (opnode)
					extension = gda_server_operation_get_node_path_portion (op, (gchar*)prop);
			}

			if (opnode) {
				switch (opnode->type) {
				case GDA_SERVER_OPERATION_NODE_PARAMLIST:
					if (!extension) {
						g_set_error (error, 0, 0, "%s", 
							     _("Parameterlist values can only be set for individual parameters within it"));
						allok = FALSE;
					}
					else {
						xmlNodePtr contents;
						
						contents = cur->children;
						if (contents && xmlNodeIsText (contents)) {
							GdaHolder *param;
							param = gda_set_get_holder (opnode->d.plist, extension);
							if (param) {
								GValue *v;
								v = gda_value_new_from_string ((gchar*)contents->content, 
											       gda_holder_get_g_type (param));
								if (!gda_holder_take_value (param, v, error)) 
									allok = FALSE;
							}
						}
					}
					break;
				case GDA_SERVER_OPERATION_NODE_DATA_MODEL:
					gda_data_model_array_clear (GDA_DATA_MODEL_ARRAY (opnode->d.model));
					if (cur->children && 
						 ! gda_data_model_add_data_from_xml_node (opnode->d.model, 
											  cur->children, error))
						allok = FALSE;
					break;
				case GDA_SERVER_OPERATION_NODE_PARAM: {
					xmlNodePtr contents;

					contents = cur->children;
					if (contents && xmlNodeIsText (contents)) {
						GValue *v;
						v = gda_value_new_from_string ((gchar*)contents->content, 
									       gda_holder_get_g_type (opnode->d.param));
						if (!gda_holder_take_value (opnode->d.param, v, error)) 
							allok = FALSE;
					}
					break;
				}
				case GDA_SERVER_OPERATION_NODE_SEQUENCE:
					break;
				case GDA_SERVER_OPERATION_NODE_SEQUENCE_ITEM:
					break;
				default:
					g_assert_not_reached ();
				}
			}

			g_free (extension);
			xmlFree (prop);

			if (!allok)
				return FALSE;
		}
		else {
			g_set_error (error, 0, 0, "%s", 
				     _("Missing attribute named 'path'"));
			return FALSE;
		}
		
		cur = cur->next;
	}
	
	return TRUE;
}

/**
 * gda_server_operation_get_root_nodes:
 * @op: a #GdaServerOperation object
 *
 * Get an array of strings containing the paths of nodes situated at the root of @op.
 * 
 * Returns: (transfer full): a new array, which must be freed with g_strfreev().
 */ 
gchar**
gda_server_operation_get_root_nodes (GdaServerOperation *op)
{
	gchar **retval;
	GSList *list;
	gint i = 0;

	g_return_val_if_fail (GDA_IS_SERVER_OPERATION (op), NULL);
	g_return_val_if_fail (op->priv, NULL);	

	retval = g_new0 (gchar *, g_slist_length (op->priv->topnodes) + 1);
	for (list = op->priv->topnodes; list; list = list->next)
		retval [i++] = node_get_complete_path (op, NODE (list->data));

	return retval;
}

/**
 * gda_server_operation_get_node_type:
 * @op: a #GdaServerOperation object
 * @path: a complete path to a node (starting with "/")
 * @status: (allow-none): a place to store the status of the node, or %NULL
 *
 * Convenience function to get the type of a node.
 *
 * Returns: the type of node, or GDA_SERVER_OPERATION_NODE_UNKNOWN if the node was not found
 */
GdaServerOperationNodeType 
gda_server_operation_get_node_type (GdaServerOperation *op, const gchar *path,
				    GdaServerOperationNodeStatus *status)
{
	GdaServerOperationNode *node_info;

	g_return_val_if_fail (GDA_IS_SERVER_OPERATION (op), GDA_SERVER_OPERATION_NODE_UNKNOWN);
	g_return_val_if_fail (op->priv, GDA_SERVER_OPERATION_NODE_UNKNOWN);

	node_info = gda_server_operation_get_node_info (op, path);
	if (node_info) {
		if (status)
			*status = node_info->status;
		return node_info->type;
	}
	return GDA_SERVER_OPERATION_NODE_UNKNOWN;
}

/**
 * gda_server_operation_get_node_parent:
 * @op: a #GdaServerOperation object
 * @path: a complete path to a node (starting with "/")
 *
 * Get the complete path to the parent of the node defined by @path
 *
 * Returns: (transfer full): a new string or %NULL if the node does not have any parent or does not exist.
 */
gchar *
gda_server_operation_get_node_parent (GdaServerOperation *op, const gchar *path)
{
	Node *node;

	g_return_val_if_fail (GDA_IS_SERVER_OPERATION (op), NULL);
	g_return_val_if_fail (op->priv, NULL);
	g_return_val_if_fail (path && (*path == '/'), NULL);

	node = node_find (op, path);

	if (node) {
		if (! node->parent)
			return NULL;
		else
			return node_get_complete_path (op, node->parent);
	}
	else {
		gchar *path2 = g_strdup (path);
		gchar *ptr;
		
		ptr = path2 + strlen (path2) - 1;
		while (*ptr != '/') {
			*ptr = 0;
			ptr --;
		}
		*ptr = 0;

		return path2;
	}
}

/**
 * gda_server_operation_get_node_path_portion:
 * @op: a #GdaServerOperation object
 * @path: a complete path to a node (starting with "/")
 *
 * Get the last part of @path
 *
 * Returns: (transfer full): a new string, or %NULL if an error occurred
 */
gchar *
gda_server_operation_get_node_path_portion (GdaServerOperation *op, const gchar *path)
{
	Node *node;

	g_return_val_if_fail (GDA_IS_SERVER_OPERATION (op), NULL);
	g_return_val_if_fail (op->priv, NULL);
	g_return_val_if_fail (path && (*path == '/'), NULL);

	node = node_find (op, path);
	if (!node) {
		gchar *path2 = g_strdup (path);
		gchar *ptr, *retval = NULL;
		
		ptr = path2 + strlen (path2) - 1;
		while (*ptr != '/')
			ptr --;
		retval = g_strdup (ptr + 1);
		g_free (path2);
		return retval;
	}
	else {
		if (node->type == GDA_SERVER_OPERATION_NODE_SEQUENCE_ITEM) {
			g_assert (node->parent);
			g_assert (node->parent->type == GDA_SERVER_OPERATION_NODE_SEQUENCE);
			return g_strdup_printf ("%d", g_slist_index (node->parent->d.seq.seq_items, node));
		}
		else
			return g_strdup (node->path_name);
	}
}

/**
 * gda_server_operation_get_sequence_item_names:
 * @op: a #GdaServerOperation object
 * @path: a complete path to a sequence node (starting with "/")
 *
 * Fetch the contents of a sequence. @path can describe either a sequence (for example "/SEQNAME") or an item in a sequence
 * (for example "/SEQNAME/3")
 *
 * Returns: (transfer full): a array of strings containing the complete paths of the nodes contained at @path (free with g_strfreev())
 */
gchar **
gda_server_operation_get_sequence_item_names (GdaServerOperation *op, const gchar *path)
{
	Node *node;
	gchar **retval;
	gint i;
	GSList *list;

	g_return_val_if_fail (GDA_IS_SERVER_OPERATION (op), NULL);
	g_return_val_if_fail (op->priv, NULL);

	node = node_find (op, path);
	if (!node || ((node->type != GDA_SERVER_OPERATION_NODE_SEQUENCE) && 
		      (node->type != GDA_SERVER_OPERATION_NODE_SEQUENCE_ITEM)))
		return NULL;

	if (node->type == GDA_SERVER_OPERATION_NODE_SEQUENCE)
		list = node->d.seq.seq_tmpl;
	else
		list = node->d.seq_item_nodes;
	i = 0;
	retval = g_new0 (gchar *, g_slist_length (list) + 1);
	for (; list; list = list->next, i++)
		retval [i] = node_get_complete_path (op, NODE (list->data));

	return retval;
}

/**
 * gda_server_operation_get_sequence_name:
 * @op: a #GdaServerOperation object
 * @path: a complete path to a sequence node (starting with "/")
 *
 * Returns: (transfer none): the name of the sequence at @path
 */
const gchar *
gda_server_operation_get_sequence_name (GdaServerOperation *op, const gchar *path)
{
	Node *node;

	g_return_val_if_fail (GDA_IS_SERVER_OPERATION (op), NULL);
	g_return_val_if_fail (op->priv, NULL);

	node = node_find (op, path);
	if (!node || (node->type != GDA_SERVER_OPERATION_NODE_SEQUENCE))
		return NULL;

	return node->d.seq.name;
}

/**
 * gda_server_operation_get_sequence_size:
 * @op: a #GdaServerOperation object
 * @path: a complete path to a sequence node (starting with "/")
 *
 * Returns: the number of items in the sequence at @path, or 0 if @path is not a sequence node
 */
guint
gda_server_operation_get_sequence_size (GdaServerOperation *op, const gchar *path)
{
	Node *node;

	g_return_val_if_fail (GDA_IS_SERVER_OPERATION (op), 0);
	g_return_val_if_fail (op->priv, 0);

	node = node_find (op, path);
	if (!node || (node->type != GDA_SERVER_OPERATION_NODE_SEQUENCE))
		return 0;

	return g_slist_length (node->d.seq.seq_items);
}

/**
 * gda_server_operation_get_sequence_max_size:
 * @op: a #GdaServerOperation object
 * @path: a complete path to a sequence node (starting with "/")
 *
 * Returns: the maximum number of items in the sequence at @path, or 0 if @path is not a sequence node
 */
guint
gda_server_operation_get_sequence_max_size (GdaServerOperation *op, const gchar *path)
{
	Node *node;

	g_return_val_if_fail (GDA_IS_SERVER_OPERATION (op), 0);
	g_return_val_if_fail (op->priv, 0);

	node = node_find (op, path);
	if (!node || (node->type != GDA_SERVER_OPERATION_NODE_SEQUENCE))
		return 0;

	return node->d.seq.max_items;
}

/**
 * gda_server_operation_get_sequence_min_size:
 * @op: a #GdaServerOperation object
 * @path: a complete path to a sequence node (starting with "/")
 *
 * Returns: the minimum number of items in the sequence at @path, or 0 if @path is not a sequence node
 */
guint
gda_server_operation_get_sequence_min_size (GdaServerOperation *op, const gchar *path)
{
	Node *node;

	g_return_val_if_fail (GDA_IS_SERVER_OPERATION (op), 0);
	g_return_val_if_fail (op->priv, 0);

	node = node_find (op, path);
	if (!node || (node->type != GDA_SERVER_OPERATION_NODE_SEQUENCE))
		return 0;

	return node->d.seq.min_items;
}


#ifdef GDA_DEBUG_NO
static void
dump (GdaServerOperation *op)
{
	xmlNodePtr node;
	node = gda_server_operation_save_data_to_xml (op, NULL);
	if (node) {
		xmlDocPtr doc;
		xmlChar *buffer;
		
		doc = xmlNewDoc ("1.0");
		xmlDocSetRootElement (doc, node);
		xmlIndentTreeOutput = 1;
		xmlKeepBlanksDefault (0);
		xmlDocDumpFormatMemory (doc, &buffer, NULL, 1);
		g_print ("%s\n", buffer);
		xmlFree (buffer);
		xmlFreeDoc (doc);
	}
	else 
		g_warning ("Saving to XML failed!");
}
#endif

/**
 * gda_server_operation_add_item_to_sequence:
 * @op: a #GdaServerOperation object
 * @seq_path: the path to the sequence to which an item must be added (like "/SEQ_NAME" for instance)
 *
 * Returns: the index of the new entry in the sequence (like 5 for example if a 6th item has
 *          been added to the sequence.
 */
guint
gda_server_operation_add_item_to_sequence (GdaServerOperation *op, const gchar *seq_path)
{
	Node *node;

	g_return_val_if_fail (GDA_IS_SERVER_OPERATION (op), 0);
	g_return_val_if_fail (op->priv, 0);
	
	node = node_find (op, seq_path);
	if (!node || (node->type != GDA_SERVER_OPERATION_NODE_SEQUENCE)) 
		return 0;

	if (g_slist_length (node->d.seq.seq_items) == node->d.seq.max_items)
		return 0;

	sequence_add_item (op, node);

#ifdef GDA_DEBUG_NO
	dump (op);
#endif

	return g_slist_length (node->d.seq.seq_items);
}

/**
 * gda_server_operation_del_item_from_sequence:
 * @op: a #GdaServerOperation object
 * @item_path: the path to the sequence's item to remove (like "/SEQ_NAME/5" for instance)
 *
 * Returns: TRUE if the specified node has been removed from the sequence
 */
gboolean
gda_server_operation_del_item_from_sequence (GdaServerOperation *op, const gchar *item_path)
{
	Node *node, *item_node;
	gchar *seq_path, *ptr;

	g_return_val_if_fail (GDA_IS_SERVER_OPERATION (op), FALSE);
	g_return_val_if_fail (op->priv, FALSE);
	
	seq_path = g_strdup (item_path);
	ptr = seq_path + strlen (seq_path) - 1;
	while ((ptr >= seq_path) && 
	       (((*ptr >= '0') && (*ptr <= '9')) || (*ptr == '/'))) {
		*ptr = 0;
		ptr--;
	}

	node = node_find (op, seq_path);
	if (!node || 
	    (node->type != GDA_SERVER_OPERATION_NODE_SEQUENCE) ||
	    (g_slist_length (node->d.seq.seq_items) == node->d.seq.min_items)) {
		g_free (seq_path);
		return FALSE;
	}

	item_node = node_find (op, item_path);
	if (!item_node || (item_node->type != GDA_SERVER_OPERATION_NODE_SEQUENCE_ITEM)) {
		g_free (seq_path);
		return FALSE;
	}
	
	clean_nodes_info_cache (op);
#ifdef GDA_DEBUG_signal
	g_print (">> 'SEQUENCE_ITEM_REMOVE' from %s\n", __FUNCTION__);
#endif
	g_signal_emit (G_OBJECT (op), gda_server_operation_signals [SEQUENCE_ITEM_REMOVE], 0, 
		       seq_path, g_slist_index (node->d.seq.seq_items, item_node));
#ifdef GDA_DEBUG_signal
	g_print ("<< 'SEQUENCE_ITEM_REMOVE' from %s\n", __FUNCTION__);
#endif	

	g_free (seq_path);
	node_destroy (op, item_node);
	node->d.seq.seq_items = g_slist_remove (node->d.seq.seq_items, item_node);

#ifdef GDA_DEBUG_NO
	dump (op);
#endif

	return FALSE;
}

/*
 * real_gda_server_operation_get_value_at
 * @op: a #GdaServerOperation object
 * @path: a complete path to a node (starting with "/")
 *
 * Get the value for the node at the @path path
 *
 * Returns: a constant #GValue if a value has been defined, or %NULL if the value is undefined or
 * if the @path is not defined or @path does not hold any value.
 */
static const GValue *
real_gda_server_operation_get_value_at (GdaServerOperation *op, const gchar *path)
{
	const GValue *value = NULL;
	GdaServerOperationNode *node_info;

	g_return_val_if_fail (GDA_IS_SERVER_OPERATION (op), NULL);
	g_return_val_if_fail (op->priv, NULL);
	g_return_val_if_fail (path && *path, NULL);

	/* use path */
	node_info = gda_server_operation_get_node_info (op, path);
	if (node_info) {
		switch (node_info->type) {
		case GDA_SERVER_OPERATION_NODE_PARAM:
			value = gda_holder_get_value (node_info->param);
			break;
		case GDA_SERVER_OPERATION_NODE_PARAMLIST:
		case GDA_SERVER_OPERATION_NODE_DATA_MODEL:
		case GDA_SERVER_OPERATION_NODE_SEQUENCE:
		case GDA_SERVER_OPERATION_NODE_SEQUENCE_ITEM:
		case GDA_SERVER_OPERATION_NODE_DATA_MODEL_COLUMN:
			break;
		default:
			g_assert_not_reached ();
		}
	}
	else {
		/* specific syntax which does not yield to a GdaServerOperationNode */
		gchar *str;
		str = gda_server_operation_get_node_parent (op, path);
		if (str) {
			node_info = gda_server_operation_get_node_info (op, str);
			if (node_info && (node_info->type == GDA_SERVER_OPERATION_NODE_DATA_MODEL_COLUMN)) {
				gchar *extension, *ptr;
				gint row;
				extension = gda_server_operation_get_node_path_portion (op, path);
				
				row = strtol (extension, &ptr, 10);
				if (ptr && *ptr)
					row = -1;
				if (row >= 0) 
					value = gda_data_model_get_value_at (node_info->model, 
									     gda_column_get_position (node_info->column), 
									     row, NULL);
				g_free(extension);
			}
			g_free (str);
		}		
	}

	return value;
}

/**
 * gda_server_operation_get_value_at:
 * @op: a #GdaServerOperation object
 * @path_format: a complete path to a node (starting with "/")
 * @...: arguments to use with @path_format to make a complete path
 *
 * Get the value for the node at the path formed using @path_format and ... (the rules are the same as
 * for g_strdup_printf())
 *
 * Returns: (transfer none): a constant #GValue if a value has been defined, or %NULL if the value is undefined or
 * if the @path is not defined or @path does not hold any value.
 */
const GValue *
gda_server_operation_get_value_at (GdaServerOperation *op, const gchar *path_format, ...)
{
	const GValue *value = NULL;
	gchar *path;
	va_list args;

	g_return_val_if_fail (GDA_IS_SERVER_OPERATION (op), NULL);
	g_return_val_if_fail (op->priv, NULL);

	/* build path */
	va_start (args, path_format);
	path = g_strdup_vprintf (path_format, args);
	va_end (args);

	value = real_gda_server_operation_get_value_at (op, path);
	g_free (path);

	return value;
}

/**
 * gda_server_operation_get_sql_identifier_at:
 * @op: a #GdaServerOperation object
 * @cnc: a #GdaConnection, or %NULL
 * @prov: a #GdaServerProvider, or %NULL
 * @path_format: a complete path to a node (starting with "/")
 * @...: arguments to use with @path_format to make a complete path
 *
 * This method is similar to gda_server_operation_get_value_at(), but for SQL identifiers: a new string
 * is returned instead of a #GValue. Also the returned string is assumed to represents an SQL identifier
 * and will correctly be quoted to be used with @cnc, or @prov if @cnc is %NULL (a generic quoting rule
 * will be applied if both are %NULL).
 *
 * Returns: (transfer full): a new string, or %NULL if the value is undefined or
 * if the @path is not defined or @path does not hold any value, or if the value held is not a string
 * (in that last case a warning is shown).
 *
 * Since: 5.0.3
 */
gchar *
gda_server_operation_get_sql_identifier_at (GdaServerOperation *op, GdaConnection *cnc, GdaServerProvider *prov,
					    const gchar *path_format, ...)
{
	const GValue *value = NULL;
	gchar *path;
	va_list args;
	GdaConnectionOptions cncoptions = 0;

	g_return_val_if_fail (GDA_IS_SERVER_OPERATION (op), NULL);

	/* build path */
	va_start (args, path_format);
	path = g_strdup_vprintf (path_format, args);
	va_end (args);

	value = real_gda_server_operation_get_value_at (op, path);
	g_free (path);

	if (!value || (G_VALUE_TYPE (value) == GDA_TYPE_NULL))
		return NULL;
	g_return_val_if_fail (G_VALUE_TYPE (value) == G_TYPE_STRING, NULL);

	if (cnc)
		g_object_get (G_OBJECT (cnc), "options", &cncoptions, NULL);
	return gda_sql_identifier_quote (g_value_get_string (value), cnc, prov, FALSE,
					 cncoptions & GDA_CONNECTION_OPTIONS_SQL_IDENTIFIERS_CASE_SENSITIVE);
}

/**
 * gda_server_operation_set_value_at:
 * @op: a #GdaServerOperation object
 * @value: a string
 * @error: a place to store errors or %NULL
 * @path_format: a complete path to a node (starting with "/")
 * @...: arguments to use with @path_format to make a complete path
 *
 * Set the value for the node at the path formed using @path_format and the ... ellipse (the rules are the same as
 * for g_strdup_printf()). 
 *
 * Note that trying to set a value for a path which is not used by the current
 * provider, such as "/TABLE_OPTIONS_P/TABLE_ENGINE" for a PostgreSQL connection (this option is only supported for MySQL), 
 * will <emphasis>not</emphasis> generate
 * any error; this allows one to give values to a superset of the parameters and thus use the same code for several providers.
 *
 * Here are the possible formats of @path_format:
 * <itemizedlist>
 *  <listitem><para>If the path corresponds to a #GdaHolder, then the parameter is set to <![CDATA["@value"]]></para></listitem>
 *  <listitem><para>If the path corresponds to a sequence item like for example "/SEQUENCE_NAME/5/NAME" for
 *     the "NAME" value of the 6th item of the "SEQUENCE_NAME" sequence then:
 *     <itemizedlist>
 *        <listitem><para>if the sequence already has 6 or more items, then the value is just set to the corresponding 
 *           value in the 6th item of the sequence</para></listitem>
 *        <listitem><para>if the sequence has less then 6 items, then items are added up to the 6th one before setting
 *           the value to the corresponding in the 6th item of the sequence</para></listitem>
 *     </itemizedlist>
 *  </para></listitem>
 *  <listitem><para>If the path corresponds to a #GdaDataModel, like for example "/ARRAY/@@COLUMN/5" for the value at the
 *     6th row of the "COLUMN" column of the "ARRAY" data model, then:
 *     <itemizedlist>
 *        <listitem><para>if the data model already contains 6 or more rows, then the value is just set</para></listitem>
 *        <listitem><para>if the data model has less than 6 rows, then rows are added up to the 6th one before setting
 *           the value</para></listitem>
 *     </itemizedlist>
 *  </para></listitem>
 * </itemizedlist>
 *
 * Returns: %TRUE if no error occurred
 */
gboolean
gda_server_operation_set_value_at (GdaServerOperation *op, const gchar *value, GError **error,
				   const gchar *path_format, ...)
{
	gchar *path;
	va_list args;

	Node *opnode;
	gchar *extension = NULL;
	gchar *colname = NULL;
	gboolean allok = TRUE;

	g_return_val_if_fail (GDA_IS_SERVER_OPERATION (op), FALSE);
	g_return_val_if_fail (op->priv, FALSE);

	/* build path */
	va_start (args, path_format);
	path = g_strdup_vprintf (path_format, args);
	va_end (args);

	/* set the value */
	opnode = node_find_or_create (op, path);
	if (!opnode) {
		/* try to see if the "parent" is a real node */
		gchar *str;
		
		str = gda_server_operation_get_node_parent (op, path);
		if (str) {
			opnode = node_find (op, str);
			if (opnode) {
				if (opnode->type != GDA_SERVER_OPERATION_NODE_PARAMLIST)
					opnode = NULL; /* ignore opnode */
			}
			else {
				gchar *str2;

				str2 = gda_server_operation_get_node_parent (op, str);
				opnode = node_find (op, str2);
				if (opnode) {
					if (opnode->type != GDA_SERVER_OPERATION_NODE_DATA_MODEL)
						opnode = NULL; /* ignore opnode */
					else 
						colname = gda_server_operation_get_node_path_portion (op, str);
				}
				g_free (str2);
			}
			g_free (str);
		}
		if (opnode)
			extension = gda_server_operation_get_node_path_portion (op, path);
	}
	
	if (opnode) {
		switch (opnode->type) {
		case GDA_SERVER_OPERATION_NODE_PARAMLIST:
			if (!extension) {
				g_set_error (error, 0, 0, "%s", 
					     _("Parameterlist values can only be set for individual parameters within it"));
				allok = FALSE;
			}
			else {
				GdaHolder *param;
				param = gda_set_get_holder (opnode->d.plist, extension);
				if (param) {
					GValue *v;
					if (value)
						v = gda_value_new_from_string (value, 
									       gda_holder_get_g_type (param));
					else
						v = gda_value_new_null ();
					if (!gda_holder_take_value (param, v, error))
						allok = FALSE;
				}
			}
			break;
		case GDA_SERVER_OPERATION_NODE_DATA_MODEL: {
			GdaColumn *column = NULL;

			if (colname && (*colname == '@')) {
				gint i, nbcols;

				nbcols = gda_data_model_get_n_columns (opnode->d.model);
				for (i = 0; (i<nbcols) && !column; i++) {
					gchar *colid = NULL;
					column = gda_data_model_describe_column (opnode->d.model, i);
					g_object_get (G_OBJECT (column), "id", &colid, NULL);
					if (!colid || strcmp (colid, colname +1))
						column = NULL;
					g_free(colid);
				}
				if (column) {
					gchar *ptr;
					gint row;
					row = strtol (extension, &ptr, 10);
					if (ptr && *ptr)
						row = -1;
					if (row >= 0) {
						gint i = gda_data_model_get_n_rows (opnode->d.model);
						
						if (i <= row) {
							for (; allok && (i <= row); i++) 
								if (gda_data_model_append_row (opnode->d.model, error) < 0)
									allok = FALSE;
						}
						
						if (allok) {
							GValue *gvalue;
							if (value)
								gvalue = gda_value_new_from_string (value, 
												    gda_column_get_g_type (column));
							else
								gvalue = gda_value_new_null ();
							allok = gda_data_model_set_value_at (opnode->d.model,
											     gda_column_get_position (column), 
											     row, gvalue, error);
							gda_value_free (gvalue);
						}
					}
				}
			}
			break;
		}
		case GDA_SERVER_OPERATION_NODE_PARAM: {
			GValue *v;
			if (value)
				v = gda_value_new_from_string (value, 
							       gda_holder_get_g_type (opnode->d.param));
			else
				v = gda_value_new_null ();
			if (!gda_holder_take_value (opnode->d.param, v, error))
				allok = FALSE;
			break;
		}
		case GDA_SERVER_OPERATION_NODE_SEQUENCE:
			break;
		case GDA_SERVER_OPERATION_NODE_SEQUENCE_ITEM:
			break;
		default:
			g_assert_not_reached ();
		}
	}

	g_free (extension);
	g_free (colname);
	g_free (path);
	return allok;
}

/**
 * gda_server_operation_is_valid:
 * @op: a #GdaServerOperation widget
 * @xml_file: an XML specification file (see gda_server_operation_new())
 * @error: a place to store an error, or %NULL
 *
 * Tells if all the required values in @op have been defined.
 *
 * if @xml_file is not %NULL, the validity of @op is tested against that specification, 
 * and not against the current @op's specification.
 *
 * Returns: %TRUE if @op is valid
 */
gboolean
gda_server_operation_is_valid (GdaServerOperation *op, const gchar *xml_file, GError **error)
{
	gboolean valid = TRUE;
	GSList *list;

	g_return_val_if_fail (GDA_IS_SERVER_OPERATION (op), FALSE);
	g_return_val_if_fail (op->priv, FALSE);

	if (!xml_file) {
		/* basic validity test */
		for (list = op->priv->allnodes; list; list = list->next) {
			Node *node;
			
			node = NODE (list->data);
			if (node->status == GDA_SERVER_OPERATION_STATUS_REQUIRED) {
				if (node->type == GDA_SERVER_OPERATION_NODE_PARAM) {
					const GValue *value;
					gchar *path;

					path = node_get_complete_path (op, node);
					value = gda_server_operation_get_value_at (op, path);
					if (!value) {
						valid = FALSE;
						g_set_error (error, 0, 0,
							     _("Missing required value for '%s'"), path);
						break;
					}
					g_free (path);
				}
				else if (node->type == GDA_SERVER_OPERATION_NODE_PARAMLIST) {
					valid = gda_set_is_valid (node->d.plist, error);
					if (!valid)
						break;
				}
			}
		}
	}
	else {
		/* use @xml_file */
		xmlNodePtr save;

		save = gda_server_operation_save_data_to_xml (op, error);
		if (save) {
			GdaServerOperation *op2;
			op2 = gda_server_operation_new (op->priv->op_type, xml_file);
			if (gda_server_operation_load_data_from_xml (op2, save, error))
				valid = gda_server_operation_is_valid (op2, NULL, error);
			else 
				valid = FALSE;
			xmlFreeNode (save);
			g_object_unref (op2);
		}
		else
			valid = FALSE;	
	}

	return valid;
}

/**
 * gda_server_operation_prepare_create_database:
 * @provider: the database provider to use
 * @db_name: the name of the database to create, or %NULL
 * @error: a place to store errors, or %NULL
 *
 * Creates a new #GdaServerOperation object which contains the specifications required
 * to create a database. Once these specifications provided, use
 * gda_server_operation_perform_create_database() to perform the database creation.
 *
 * If @db_name is left %NULL, then the name of the database to create will have to be set in the
 * returned #GdaServerOperation using gda_server_operation_set_value_at().
 *
 * Returns: (transfer full) (allow-none): new #GdaServerOperation object, or %NULL if the provider does not support database
 * creation
 *
 * Since: 4.2.3
 */
GdaServerOperation *
gda_server_operation_prepare_create_database (const gchar *provider, const gchar *db_name, GError **error)
{
	GdaServerProvider *prov;

	g_return_val_if_fail (provider && *provider, NULL);

	prov = gda_config_get_provider (provider, error);
	if (prov) {
		GdaServerOperation *op;
		op = gda_server_provider_create_operation (prov, NULL, GDA_SERVER_OPERATION_CREATE_DB,
							   NULL, error);
		if (op) {
			g_object_set_data_full (G_OBJECT (op), "_gda_provider_obj", g_object_ref (prov), g_object_unref);
			if (db_name)
				gda_server_operation_set_value_at (op, db_name, NULL, "/DB_DEF_P/DB_NAME");
		}
		return op;
	}
	else
		return NULL;
}

/**
 * gda_server_operation_perform_create_database:
 * @provider: the database provider to use, or %NULL if @op has been created using gda_server_operation_prepare_create_database()
 * @op: a #GdaServerOperation object obtained using gda_server_operation_prepare_create_database()
 * @error: a place to store en error, or %NULL
 *
 * Creates a new database using the specifications in @op. @op can be obtained using
 * gda_server_provider_create_operation(), or gda_server_operation_prepare_create_database().
 *
 * Returns: TRUE if no error occurred and the database has been created, FALSE otherwise
 *
 * Since: 4.2.3
 */
gboolean
gda_server_operation_perform_create_database (GdaServerOperation *op, const gchar *provider, GError **error)
{
	GdaServerProvider *prov;

	g_return_val_if_fail (GDA_IS_SERVER_OPERATION (op), FALSE);
	if (provider)
		prov = gda_config_get_provider (provider, error);
	else
		prov = g_object_get_data (G_OBJECT (op), "_gda_provider_obj");
	if (prov)
		return gda_server_provider_perform_operation (prov, NULL, op, error);
	else {
		g_warning ("Could not find operation's associated provider, "
			   "did you use gda_server_operation_prepare_create_database() ?");
		return FALSE;
	}
}

/**
 * gda_server_operation_prepare_drop_database:
 * @provider: the database provider to use
 * @db_name: the name of the database to drop, or %NULL
 * @error: a place to store errors, or %NULL
 *
 * Creates a new #GdaServerOperation object which contains the specifications required
 * to drop a database. Once these specifications provided, use
 * gda_server_operation_perform_drop_database() to perform the database creation.
 *
 * If @db_name is left %NULL, then the name of the database to drop will have to be set in the
 * returned #GdaServerOperation using gda_server_operation_set_value_at().
 *
 * Returns: (transfer full) (allow-none): new #GdaServerOperation object, or %NULL if the provider does not support database
 * destruction
 *
 * Since: 4.2.3
 */
GdaServerOperation *
gda_server_operation_prepare_drop_database (const gchar *provider, const gchar *db_name, GError **error)
{
	GdaServerProvider *prov;

	g_return_val_if_fail (provider && *provider, NULL);

	prov = gda_config_get_provider (provider, error);
	if (prov) {
		GdaServerOperation *op;
		op = gda_server_provider_create_operation (prov, NULL, GDA_SERVER_OPERATION_DROP_DB,
							   NULL, error);
		if (op) {
			g_object_set_data_full (G_OBJECT (op), "_gda_provider_obj", g_object_ref (prov), g_object_unref);
			if (db_name)
				gda_server_operation_set_value_at (op, db_name, NULL, "/DB_DESC_P/DB_NAME");
		}
		return op;
	}
	else
		return NULL;
}

/**
 * gda_server_operation_perform_drop_database:
 * @provider: the database provider to use, or %NULL if @op has been created using gda_server_operation_prepare_drop_database()
 * @op: a #GdaServerOperation object obtained using gda_prepare_drop_database()
 * @error: a place to store en error, or %NULL
 *
 * Destroys an existing database using the specifications in @op. @op can be obtained using
 * gda_server_provider_create_operation(), or gda_server_operation_prepare_drop_database().
 *
 * Returns: TRUE if no error occurred and the database has been destroyed
 *
 * Since: 4.2.3
 */
gboolean
gda_server_operation_perform_drop_database (GdaServerOperation *op, const gchar *provider, GError **error)
{
	GdaServerProvider *prov;

	g_return_val_if_fail (GDA_IS_SERVER_OPERATION (op), FALSE);
	if (provider)
		prov = gda_config_get_provider (provider, error);
	else
		prov = g_object_get_data (G_OBJECT (op), "_gda_provider_obj");
	if (prov)
		return gda_server_provider_perform_operation (prov, NULL, op, error);
	else {
		g_warning ("Could not find operation's associated provider, "
			   "did you use gda_server_operation_prepare_drop_database() ?");
		return FALSE;
	}
}

/**
 * gda_server_operation_prepare_create_table:
 * @cnc: an opened connection
 * @table_name: name of the table to create
 * @error: a place to store errors, or %NULL
 * @...: group of three arguments for column's name, column's #GType
 * and a #GdaEasyCreateTableFlag flag, finished with %NULL
 *
 * Add more arguments if the flag needs them:
 *
 * GDA_SERVER_OPERATION_CREATE_TABLE_FKEY_FLAG:
 * <itemizedlist>
 *   <listitem><para>string with the table's name referenced</para></listitem>
 *   <listitem><para>an integer with the number pairs "local_field", "referenced_field"
 *   used in the reference</para></listitem>
 *   <listitem><para>Pairs of "local_field", "referenced_field" to use, must match
 *    the number specified above.</para></listitem>
 *   <listitem><para>a string with the action for ON DELETE; can be: "RESTRICT", "CASCADE",
 *    "NO ACTION", "SET NULL" and "SET DEFAULT". Example: "ON UPDATE CASCADE".</para></listitem>
 *   <listitem><para>a string with the action for ON UPDATE (see above).</para></listitem>
 * </itemizedlist>
 *
 * Create a #GdaServerOperation object using an opened connection, taking three
 * arguments, a column's name the column's GType and #GdaEasyCreateTableFlag
 * flag, you need to finish the list using %NULL.
 *
 * You'll be able to modify the #GdaServerOperation object to add custom options * to the operation. When finished call #gda_server_operation_perform_create_table
 * or #gda_server_provider_perform_operation
 * in order to execute the operation.
 *
 * Returns: (transfer full) (allow-none): a #GdaServerOperation if no errors; NULL and set @error otherwise
 *
 * Since: 4.2.3
 */
G_GNUC_NULL_TERMINATED
GdaServerOperation*
gda_server_operation_prepare_create_table (GdaConnection *cnc, const gchar *table_name, GError **error, ...)
{
	GdaServerOperation *op;
	GdaServerProvider *server;

	g_return_val_if_fail (gda_connection_is_opened (cnc), FALSE);

	server = gda_connection_get_provider (cnc);

	if (!table_name) {
		g_set_error (error, GDA_SERVER_OPERATION_ERROR, GDA_SERVER_OPERATION_OBJECT_NAME_ERROR,
			     "%s", _("Unspecified table name"));
		return NULL;
	}

	if (gda_server_provider_supports_operation (server, cnc, GDA_SERVER_OPERATION_CREATE_TABLE, NULL)) {
		va_list  args;
		gchar   *arg;
		GType    type;
		gchar   *dbms_type;
		GdaServerOperationCreateTableFlag flag;
		gint i;
		gint refs;

		op = gda_server_provider_create_operation (server, cnc,
							   GDA_SERVER_OPERATION_CREATE_TABLE, NULL, error);
		if (!GDA_IS_SERVER_OPERATION(op))
			return NULL;
		if(!gda_server_operation_set_value_at (op, table_name, error, "/TABLE_DEF_P/TABLE_NAME")) {
			g_object_unref (op);
			return NULL;
		}

		va_start (args, error);
		type = 0;
		arg = NULL;
		i = 0;
		refs = -1;

		while ((arg = va_arg (args, gchar*))) {
			/* First argument for Column's name */
			if(!gda_server_operation_set_value_at (op, arg, error, "/FIELDS_A/@COLUMN_NAME/%d", i)){
				g_object_unref (op);
				return NULL;
			}

			/* Second to Define column's type */
			type = va_arg (args, GType);
			if (type == 0) {
				g_set_error (error, GDA_SERVER_OPERATION_ERROR, GDA_SERVER_OPERATION_INCORRECT_VALUE_ERROR,
					     "%s", _("Invalid type"));
				g_object_unref (op);
				return NULL;
			}
			dbms_type = (gchar *) gda_server_provider_get_default_dbms_type (server,
											 cnc, type);
			if (!gda_server_operation_set_value_at (op, dbms_type, error, "/FIELDS_A/@COLUMN_TYPE/%d", i)){
				g_object_unref (op);
				return NULL;
			}

			/* Third for column's flags */
			flag = va_arg (args, GdaServerOperationCreateTableFlag);
			if (flag & GDA_SERVER_OPERATION_CREATE_TABLE_PKEY_FLAG)
				if(!gda_server_operation_set_value_at (op, "TRUE", error, "/FIELDS_A/@COLUMN_PKEY/%d", i)){
					g_object_unref (op);
					return NULL;
				}
			if (flag & GDA_SERVER_OPERATION_CREATE_TABLE_NOT_NULL_FLAG)
				if(!gda_server_operation_set_value_at (op, "TRUE", error, "/FIELDS_A/@COLUMN_NNUL/%d", i)){
					g_object_unref (op);
					return NULL;
				}
			if (flag & GDA_SERVER_OPERATION_CREATE_TABLE_AUTOINC_FLAG)
				if (!gda_server_operation_set_value_at (op, "TRUE", error, "/FIELDS_A/@COLUMN_AUTOINC/%d", i)){
					g_object_unref (op);
					return NULL;
				}
			if (flag & GDA_SERVER_OPERATION_CREATE_TABLE_UNIQUE_FLAG)
				if(!gda_server_operation_set_value_at (op, "TRUE", error, "/FIELDS_A/@COLUMN_UNIQUE/%d", i)){
					g_object_unref (op);
					return NULL;
				}
			if (flag & GDA_SERVER_OPERATION_CREATE_TABLE_FKEY_FLAG) {
				gint j;
				gint fields;
				gchar *fkey_table;
				gchar *fkey_ondelete;
				gchar *fkey_onupdate;

				refs++;

				fkey_table = va_arg (args, gchar*);
				if (!gda_server_operation_set_value_at (op, fkey_table, error,
								   "/FKEY_S/%d/FKEY_REF_TABLE", refs)){
					g_object_unref (op);
					return NULL;
				}

				fields = va_arg (args, gint);

				for (j = 0; j < fields; j++) {
					gchar *field, *rfield;

					field = va_arg (args, gchar*);
					if(!gda_server_operation_set_value_at (op, field, error,
									   "/FKEY_S/%d/FKEY_FIELDS_A/@FK_FIELD/%d", refs, j)){
						g_object_unref (op);
						return NULL;
					}

					rfield = va_arg (args, gchar*);
					if(!gda_server_operation_set_value_at (op, rfield, error,
									   "/FKEY_S/%d/FKEY_FIELDS_A/@FK_REF_PK_FIELD/%d", refs, j)){
						g_object_unref (op);
						return NULL;
					}
				}

				fkey_ondelete = va_arg (args, gchar*);
				if (!gda_server_operation_set_value_at (op, fkey_ondelete, error,
								   "/FKEY_S/%d/FKEY_ONDELETE", refs)){
					g_object_unref (op);
					return NULL;
				}
				fkey_onupdate = va_arg (args, gchar*);
				if(!gda_server_operation_set_value_at (op, fkey_onupdate, error,
								   "/FKEY_S/%d/FKEY_ONUPDATE", refs)){
					g_object_unref (op);
					return NULL;
				}
			}

			i++;
		}

		va_end (args);

		g_object_set_data_full (G_OBJECT (op), "_gda_connection", g_object_ref (cnc), g_object_unref);

		return op;
	}
	else {
		g_set_error (error, GDA_SERVER_OPERATION_ERROR, GDA_SERVER_OPERATION_OBJECT_NAME_ERROR,
			     "%s", _("CREATE TABLE operation is not supported by the database server"));
		return NULL;
	}
}


/**
 * gda_server_operation_perform_create_table:
 * @op: a valid #GdaServerOperation
 * @error: a place to store errors, or %NULL
 *
 * Performs a prepared #GdaServerOperation to create a table. This could perform
 * an operation created by #gda_server_operation_prepare_create_table or any other using the
 * the #GdaServerOperation API.
 *
 * Returns: TRUE if the table was created; FALSE and set @error otherwise
 *
 * Since: 4.2.3
 */
gboolean
gda_server_operation_perform_create_table (GdaServerOperation *op, GError **error)
{
	GdaConnection *cnc;

	g_return_val_if_fail (GDA_IS_SERVER_OPERATION (op), FALSE);
	g_return_val_if_fail (gda_server_operation_get_op_type (op) == GDA_SERVER_OPERATION_CREATE_TABLE, FALSE);

	cnc = g_object_get_data (G_OBJECT (op), "_gda_connection");
	if (cnc)
		return gda_server_provider_perform_operation (gda_connection_get_provider (cnc), cnc, op, error);
	else {
		g_warning ("Could not find operation's associated connection, "
			   "did you use gda_connection_prepare_create_table() ?");
		return FALSE;
	}
}

/**
 * gda_server_operation_prepare_drop_table:
 * @cnc: an opened connection
 * @table_name: name of the table to drop
 * @error: a place to store errors, or %NULL
 *
 * This is just a convenient function to create a #GdaServerOperation to drop a
 * table in an opened connection.
 *
 * Returns: (transfer full) (allow-none): a new #GdaServerOperation or %NULL if couldn't create the opereration.
 *
 * Since: 4.2.3
 */
GdaServerOperation*
gda_server_operation_prepare_drop_table (GdaConnection *cnc, const gchar *table_name, GError **error)
{
	GdaServerOperation *op;
	GdaServerProvider *server;

	server = gda_connection_get_provider (cnc);

	op = gda_server_provider_create_operation (server, cnc,
						   GDA_SERVER_OPERATION_DROP_TABLE, NULL, error);

	if (GDA_IS_SERVER_OPERATION (op)) {
		g_return_val_if_fail (table_name != NULL
				      || GDA_IS_CONNECTION (cnc)
				      || !gda_connection_is_opened (cnc), NULL);

		if (gda_server_operation_set_value_at (op, table_name,
						       error, "/TABLE_DESC_P/TABLE_NAME")) {
			g_object_set_data_full (G_OBJECT (op), "_gda_connection", g_object_ref (cnc), g_object_unref);
			return op;
		}
		else
			return NULL;
	}
	else
		return NULL;
}


/**
 * gda_server_operation_perform_drop_table:
 * @op: a #GdaServerOperation object
 * @error: a place to store errors, or %NULL
 *
 * This is just a convenient function to perform a drop a table operation.
 *
 * Returns: TRUE if the table was dropped
 *
 * Since: 4.2.3
 */
gboolean
gda_server_operation_perform_drop_table (GdaServerOperation *op, GError **error)
{
	GdaConnection *cnc;

	g_return_val_if_fail (GDA_IS_SERVER_OPERATION (op), FALSE);
	g_return_val_if_fail (gda_server_operation_get_op_type (op) == GDA_SERVER_OPERATION_DROP_TABLE, FALSE);

	cnc = g_object_get_data (G_OBJECT (op), "_gda_connection");
	if (cnc)
		return gda_server_provider_perform_operation (gda_connection_get_provider (cnc), cnc, op, error);
	else {
		g_warning ("Could not find operation's associated connection, "
			   "did you use gda_server_operation_prepare_drop_table() ?");
		return FALSE;
	}
}
