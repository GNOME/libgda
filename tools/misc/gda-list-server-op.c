/*
 * Copyright (C) 2007 - 2014 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2010 David King <davidk@openismus.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */
#include <stdio.h>
#include <string.h>
#include <glib.h>
#include <gmodule.h>
#include <libgda/libgda.h>
#include <libgda/binreloc/gda-binreloc.h>
#include <glib/gi18n-lib.h>
#include "gda-tree-mgr-xml.h"

gchar *prov = NULL;
gchar *op = NULL;
gboolean list_ops = FALSE;
gboolean out_tree = FALSE;
GdaServerProvider *prov_obj = NULL;

static GOptionEntry entries[] = {
        { "provider", 'p', 0, G_OPTION_ARG_STRING, &prov, "Provider name", "provider"},
        { "op", 'o', 0, G_OPTION_ARG_STRING, &op, "Operation", "operation name"},
        { "list-ops", 'l', 0, G_OPTION_ARG_NONE, &list_ops, "List existing operations", NULL },
        { "tree", 't', 0, G_OPTION_ARG_NONE, &out_tree, "Output results as a tree (default is as XML)", NULL },
        { NULL, 0, 0, 0, NULL, NULL, NULL }
};

static xmlDocPtr merge_specs (const gchar *xml_dir, GdaServerOperationType type, gboolean *op_supported, GError **error);

gint 
main (int argc, char **argv) {
	gchar *xml_dir;
	GOptionContext *context;
	GError *error = NULL;

	context = g_option_context_new (_("Gda server operations list"));
        g_option_context_add_main_entries (context, entries, GETTEXT_PACKAGE);
        if (!g_option_context_parse (context, &argc, &argv, &error)) {
                g_warning ("Can't parse arguments: %s", error->message);
		return 1;
        }
        g_option_context_free (context);

	gda_init ();
	xml_dir = gda_gbr_get_file_path (GDA_DATA_DIR, "libgda-6.0", NULL);
	g_print (_("Using XML descriptions in %s\n"), xml_dir);
	if (prov)
		g_print ("For provider %s\n", prov);

	if (prov) {
		prov_obj = gda_config_get_provider (prov, &error);
		if (!prov_obj) {
			g_print (_("Could not create provider object: %s\n"), 
				 error && error->message ? error->message : _("No detail"));
			return 1;
		}
	}

	if (list_ops) {
		GdaServerOperationType type;
		if (prov)
			g_print (_("Existing operation types for provider '%s':\n"), prov);
		else
			g_print (_("Existing operation types:\n"));
		for (type = GDA_SERVER_OPERATION_CREATE_DB; type < GDA_SERVER_OPERATION_LAST; type++) {
			if (! prov_obj ||
			    (prov_obj && gda_server_provider_supports_operation (prov_obj, NULL, type, NULL)))
				g_print ("%s\n", gda_server_operation_op_type_to_string (type));
		}
		return 0;
	}

	GdaServerOperationType type;
	for (type = GDA_SERVER_OPERATION_CREATE_DB; type != GDA_SERVER_OPERATION_LAST; type++) {
		xmlDocPtr doc;
		GError *error = NULL;
		gboolean op_supported;

		if (op && strcmp (op, gda_server_operation_op_type_to_string (type)))
			continue;

		g_print (_("Description for type: %s\n"), gda_server_operation_op_type_to_string (type));
		doc = merge_specs (xml_dir, type, &op_supported, &error);
		if (doc) {
			if (out_tree) {
				GdaTree *tree;
				GdaTreeManager *mgr;

				tree = gda_tree_new ();
				mgr = gda_tree_mgr_xml_new (xmlDocGetRootElement (doc), "prov_name|id|name|gdatype|node_type|descr");
				gda_tree_add_manager (tree,  mgr);
				gda_tree_manager_add_manager (mgr, mgr);
				g_object_unref (mgr);
				gda_tree_update_all (tree, NULL);
				gda_tree_dump (tree, NULL, NULL);
				g_object_unref (tree);
			}
			else {
				xmlChar *buf;
				gint len;
				xmlKeepBlanksDefault (0);
				xmlDocDumpFormatMemory (doc, &buf, &len, 1);
				g_print ("%s\n", buf);
				xmlFree (buf);
			}
			xmlFreeDoc (doc);
		}
		else {
			if (!op_supported)
				g_print (_("Operation not supported\n"));
			else
				g_print (_("Error: %s\n"), error && error->message ? error->message : _("No detail"));
			if (error)
				g_error_free (error);
		}
	}
	g_free (xml_dir);

	return 0;
}

static void
remove_all_extra (xmlNodePtr node)
{
	xmlNodePtr child;
	for (child = node->last; child; ) {
		xmlChar *lang = xmlGetProp (child, (xmlChar*)"lang");
		if (lang || !strcmp ((gchar *) child->name, "sources")) {
			/* remove this node */
			xmlNodePtr prev = child->prev;
			if (lang)
				xmlFree (lang);

			xmlUnlinkNode (child);
			xmlFreeNode (child);
			child = prev;
		}
		else {
			remove_all_extra (child);
			child = child->prev;
		}
	}
}

typedef struct {
	GdaServerOperationNodeType  node_type;
	gchar                      *path;
	gchar                      *name;
	gchar                      *type;
	gchar                      *descr;
} Path;
static void
path_free (Path *path)
{
	g_free (path->path);
	g_free (path->name);
	g_free (path->type);
	g_free (path->descr);
	g_free (path);
}

/* create a list of Path structures */
static GSList *
make_paths (xmlNodePtr node, const gchar *parent_path, GSList *exist_list)
{
	xmlChar *id;
	GSList *retlist = exist_list;
	id = xmlGetProp (node, BAD_CAST "id");
	if (id) {
		Path *path;
		gchar *pstr = g_strdup_printf ("%s/%s", parent_path, id);

		path = g_new0 (Path, 1);
		path->path = pstr;
		retlist = g_slist_append (retlist, path);

		if (!strcmp ((gchar*) node->name, "parameters"))
			path->node_type = GDA_SERVER_OPERATION_NODE_PARAMLIST;
		else if (!strcmp ((gchar*) node->name, "parameter"))
			path->node_type = GDA_SERVER_OPERATION_NODE_PARAM;
		else if (!strcmp ((gchar*) node->name, "sequence"))
			path->node_type = GDA_SERVER_OPERATION_NODE_SEQUENCE;
		else if (!strcmp ((gchar*) node->name, "gda_array"))
			path->node_type = GDA_SERVER_OPERATION_NODE_DATA_MODEL;
		else if (!strcmp ((gchar*) node->name, "gda_array_field")) {
			path->node_type = GDA_SERVER_OPERATION_NODE_DATA_MODEL_COLUMN;
			g_free (path->path);
			pstr = g_strdup_printf ("%s/@%s", parent_path, id);
			path->path = pstr;
		}
		else
			path->node_type = GDA_SERVER_OPERATION_NODE_UNKNOWN;

		xmlChar *prop;
		prop = xmlGetProp (node, BAD_CAST "name");
		if (prop) {
			path->name = g_strdup ((gchar*) prop);
			xmlFree (prop);
		}
		prop = xmlGetProp (node, BAD_CAST "descr");
		if (prop) {
			path->descr = g_strdup ((gchar*) prop);
			xmlFree (prop);
		}
		prop = xmlGetProp (node, BAD_CAST "gdatype");
		if (prop) {
			path->type = g_strdup ((gchar*) prop);
			xmlFree (prop);
		}

		/* children */
		xmlNodePtr child;
		for (child = node->children; child; child = child->next) 
			retlist = make_paths (child, pstr, retlist);

		xmlFree (id);
	}
	else {
		/* children */
		xmlNodePtr child;
		for (child = node->children; child; child = child->next) 
			retlist = make_paths (child, parent_path, retlist);
	}

	return retlist;
}

const gchar *
node_type_to_string (GdaServerOperationType node_type)
{
	switch (node_type) {
	case GDA_SERVER_OPERATION_NODE_PARAMLIST:
		return "PARAMLIST";
	case GDA_SERVER_OPERATION_NODE_DATA_MODEL:
		return "DATA_MODEL";
	case GDA_SERVER_OPERATION_NODE_PARAM:
		return "PARAMETER";
	case GDA_SERVER_OPERATION_NODE_SEQUENCE:
		return "SEQUENCE";
	case GDA_SERVER_OPERATION_NODE_SEQUENCE_ITEM:
		return "SEQUENCE_ITEM";
	default:
		break;
	}
	return "Unknown";
}

static void
merge_spec_from_root (xmlNodePtr root, const gchar *provider, xmlDocPtr pdoc)
{
	/* make list of paths */
	GSList *paths, *list;

	paths = make_paths (xmlDocGetRootElement (pdoc), "", NULL);
	for (list = paths; list; list = list->next) {
		Path *path = (Path *) list->data;

		/* find node for this path ID, and create it if not yet present */
		xmlNodePtr node = NULL;
		for (node = root->children; node; node = node->next) {
			if (!strcmp ((gchar*) node->name, "path")) {
				xmlChar *pid;
				pid = xmlGetProp (node, BAD_CAST "id");
				if (pid) {
					if (!strcmp ((gchar*) pid, path->path)) {
						xmlFree (pid);
						break;
					}
					xmlFree (pid);
				}
			}
			    
		}
		if (!node) {
			node = xmlNewChild (root, NULL, BAD_CAST "path", NULL);
			xmlSetProp (node, BAD_CAST "id", BAD_CAST path->path);
			xmlSetProp (node, BAD_CAST "node_type", BAD_CAST node_type_to_string (path->node_type));
			if (path->type && *path->type)
				xmlSetProp (node, BAD_CAST "gdatype", BAD_CAST path->type);	
		}

		/* add this provider's specific information */
		xmlNodePtr pnode;
		if (!prov) {
			pnode = xmlNewChild (node, NULL, BAD_CAST "prov", NULL);
			xmlSetProp (pnode, BAD_CAST "prov_name", BAD_CAST provider);
		}
		else
			pnode = node;
		xmlSetProp (pnode, BAD_CAST "name", BAD_CAST path->name);
		if (path->descr && *path->descr)
			xmlSetProp (pnode, BAD_CAST "descr", BAD_CAST path->descr);

		if (!prov) {
			/* check node type is similar to "node->node_type" */
			xmlChar *node_type;
			node_type = xmlGetProp (node, BAD_CAST "node_type");
			g_assert (node_type);
			if (strcmp ((gchar*) node_type, 
				    node_type_to_string (path->node_type)))
				xmlSetProp (pnode, BAD_CAST "node_type", 
					    BAD_CAST node_type_to_string (path->node_type));
			xmlFree (node_type);
			
			/* check gdatype is similar to "node->gdatype" */
			node_type = xmlGetProp (node, BAD_CAST "gdatype");
			if ((node_type && path->type && 
			     strcmp ((gchar*) node_type, path->type)) ||
			    (!node_type && path->type) ||
			    (node_type && *node_type && !path->type))
				xmlSetProp (pnode, BAD_CAST "gdatype", BAD_CAST path->type);
			if (node_type) 
				xmlFree (node_type);
		}
	}
	
	/* mem free */
	g_slist_foreach (paths, (GFunc) path_free, NULL);
	g_slist_free (paths);
}

static xmlDocPtr
merge_specs (const gchar *xml_dir, GdaServerOperationType type, gboolean *op_supported, GError **error)
{
	xmlDocPtr doc = NULL;
	xmlNodePtr root = NULL;
	GDir *dir;
	const gchar *file;
	gchar *lcprov = NULL;
	dir = g_dir_open (xml_dir, 0, error);
	if (!dir)
		return NULL;

	if (prov)
		lcprov = g_ascii_strdown (prov, -1);
	*op_supported = FALSE;

	for (file = g_dir_read_name (dir); file; file = g_dir_read_name (dir)) {
		gchar *suffix, *lc;

		lc = g_ascii_strdown (gda_server_operation_op_type_to_string (type), -1);
		suffix = g_strdup_printf ("_specs_%s.xml", lc);
		g_free (lc);
		if (g_str_has_suffix (file, suffix)) {
			gchar *provider_end, *provider;

			xmlDocPtr pdoc;
			gchar *cfile = g_build_filename (xml_dir, file, NULL);

			provider = g_strdup (file);
			provider_end = g_strrstr (provider, suffix);
			*provider_end = 0;
			
			if (lcprov && strcmp (lcprov, provider)) {
				/* handle the name mismatch between the provider named "PostgreSQL" and
				 * the file names which start with "postgres" */
				if (strcmp (provider, "postgres") || strcmp (lcprov, "postgresql"))
					continue;
			}

			*op_supported = TRUE;
			pdoc = xmlParseFile (cfile);
			g_free (cfile);
			if (!pdoc) {
				g_warning (_("Can't parse file '%s', ignoring it"), file);
				continue;
			}

			remove_all_extra (xmlDocGetRootElement (pdoc));

			if (!doc) {
				doc = xmlNewDoc (BAD_CAST "1.0");
				root = xmlNewNode (NULL, BAD_CAST "server_op");
				xmlDocSetRootElement (doc, root);
			}
			merge_spec_from_root (root, provider, pdoc);
						
			xmlFreeDoc (pdoc);
			g_free (provider);
		}
		g_free (suffix);
	}
	g_dir_close (dir);
	g_free (lcprov);
	return doc;
}
