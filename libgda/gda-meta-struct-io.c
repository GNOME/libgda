/*
 * Copyright (C) 2008 Murray Cumming <murrayc@murrayc.com>
 * Copyright (C) 2008 - 2013 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2013 Miguel Angel Cabrera Moya <madmac2501@gmail.com>
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
#define G_LOG_DOMAIN "GDA-meta-struct-io"

#include <string.h>
#include <glib/gi18n-lib.h>
#include <libgda/gda-util.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libgda/gda-meta-struct-private.h>
#include <libgda/gda-attributes-manager.h>
#include <libgda/gda-debug-macros.h>
#include <libgda/sql-parser/gda-statement-struct-util.h>


static GdaMetaDbObject *create_table_object (GdaMetaStruct *mstruct, const GValue *catalog, const gchar *quoted_catalog,
					     const GValue *schema, const gchar *quoted_schema, xmlNodePtr node, GError **error);
/*
static GdaMetaDbObject *create_view_object (GdaMetaStruct *mstruct, const GValue *catalog, const gchar *quoted_catalog,
					    const GValue *schema, const gchar *quoted_schema, xmlNodePtr node, GError **error);
*/



/**
 * gda_meta_struct_load_from_xml_file:
 * @mstruct: a #GdaMetaStruct object
 * @catalog: (nullable): the catalog name, or %NULL
 * @schema: (nullable): the schema name, or %NULL
 * @xml_spec_file: the specifications as the name of an XML file
 * @error: (nullable): a place to store errors, or %NULL
 *
 * Loads an XML description into @mstruct. This method is still experimental and no description
 * the XML file structure is given, and no guarantee that it will remain as it is given.
 *
 * returns: TRUE if no error has occurred
 */
gboolean
gda_meta_struct_load_from_xml_file (GdaMetaStruct *mstruct, const gchar *catalog, const gchar *schema, 
				    const gchar *xml_spec_file, GError **error)
{
	xmlNodePtr node;
	xmlDocPtr doc;

	GValue *catalog_value = NULL;
	gchar *quoted_catalog = NULL;
	GValue *schema_value = NULL;
	gchar *quoted_schema = NULL;

	g_return_val_if_fail (GDA_IS_META_STRUCT (mstruct), FALSE);
	g_return_val_if_fail (xml_spec_file && *xml_spec_file, FALSE);

	if (catalog) {
		g_value_set_string ((catalog_value = gda_value_new (G_TYPE_STRING)), catalog);
		quoted_catalog = gda_sql_identifier_quote (catalog, NULL, NULL, FALSE, FALSE);
	}
	if (schema) {
		g_value_set_string ((schema_value = gda_value_new (G_TYPE_STRING)), schema);
		quoted_schema = gda_sql_identifier_quote (schema, NULL, NULL, FALSE, FALSE);
	}

	/* load information schema's structure XML file */
	doc = xmlParseFile (xml_spec_file);
	if (!doc) {
		g_set_error (error, GDA_META_STRUCT_ERROR, GDA_META_STRUCT_XML_ERROR,
			     _("Could not load file '%s'"), xml_spec_file);
		return FALSE;
	}
	
	node = xmlDocGetRootElement (doc);
	if (!node || strcmp ((gchar *) node->name, "schema")) {
		g_set_error (error, GDA_META_STRUCT_ERROR, GDA_META_STRUCT_XML_ERROR,
			     _("Root node of file '%s' should be <schema>."), xml_spec_file);
		xmlFreeDoc (doc);
		return FALSE;
	}
	
	/* walk through the xmlDoc */
	for (node = node->children; node; node = node->next) {
		/* <table> tag for table creation */
		if (!strcmp ((gchar *) node->name, "table")) {
			GdaMetaDbObject *dbo;
			dbo = create_table_object (mstruct, catalog_value, quoted_catalog,
						   schema_value, quoted_schema, node, error);
			if (!dbo) {
				xmlFreeDoc (doc);
				return FALSE;
			}
		}
		/* <view> tag for view creation */
		else if (!strcmp ((gchar *) node->name, "view")) {
			TO_IMPLEMENT;
			/*
			GdaMetaDbObject *dbo;
			dbo = create_view_object (mstruct, catalog_value, quoted_catalog,
			schema_value, quoted_schema, node, error);
			if (!dbo) {
				xmlFreeDoc (doc);
				return FALSE;
			}
			*/
		}
	}
	xmlFreeDoc (doc);

	/* sort database objects */
	if (! gda_meta_struct_sort_db_objects (mstruct, GDA_META_SORT_DEPENDENCIES, error)) 
		return FALSE;

#ifdef GDA_DEBUG
	GSList *objlist, *list;
	objlist = gda_meta_struct_get_all_db_objects (mstruct);

	for (list = objlist; list; list = list->next) {
		GSList *dl;
		g_print ("Database object: %s\n", GDA_META_DB_OBJECT (list->data)->obj_name);
		for (dl = GDA_META_DB_OBJECT (list->data)->depend_list; dl; dl = dl->next) {
			g_print ("   depend on %s\n", GDA_META_DB_OBJECT (dl->data)->obj_name);
		}
	}
	g_slist_free (objlist);
#endif

	/* dump as a graph */
#ifdef GDA_DEBUG
	gchar *graph;
	GError *lerror = NULL;
	graph = gda_meta_struct_dump_as_graph (mstruct, GDA_META_GRAPH_COLUMNS, &lerror);
	if (graph) {
		if (g_file_set_contents ("graph.dot", graph, -1, &lerror))
			g_print ("Graph written to 'graph.dot'\n");
		else {
			g_print ("Could not save graph to file: %s\n", lerror && lerror->message ? lerror->message : "No detail");
			g_error_free (lerror);
		}
		g_free (graph);
	}
	else {
		g_print ("Could not create a graph: %s\n", lerror && lerror->message ? lerror->message : "No detail");
		g_error_free (lerror);
	}
#endif

	return TRUE;
}

static GdaMetaDbObject *
create_table_object (GdaMetaStruct *mstruct, const GValue *catalog, const gchar *quoted_catalog,
		     const GValue *schema, const gchar *quoted_schema, xmlNodePtr node, GError **error)
{
	GdaMetaDbObject *dbobj;
	xmlChar *table_name, *table_schema;
	GString *full_table_name;
	GValue *v1 = NULL, *v2 = NULL, *v3 = NULL;
	GArray *pk_cols_array = NULL;
	gchar *tmp;

	table_name = xmlGetProp (node, BAD_CAST "name");
	if (!table_name) {
		g_set_error (error, GDA_META_STRUCT_ERROR, GDA_META_STRUCT_XML_ERROR,
			     "%s", _("Missing table name from <table> node"));
		return NULL;
	}
	table_schema = xmlGetProp (node, BAD_CAST "schema");

	/* determine object's complete name */
	full_table_name = g_string_new ("");
	if (quoted_catalog) {
		v1 = gda_value_copy (catalog);
		g_string_append (full_table_name, quoted_catalog);
		g_string_append_c (full_table_name, '.');
	}
	if (table_schema) {
		g_value_set_string ((v2 = gda_value_new (G_TYPE_STRING)), (gchar *) table_schema);
		tmp = gda_sql_identifier_quote ((gchar *) table_schema, NULL, NULL, FALSE, FALSE);
		g_string_append (full_table_name, tmp);
		g_free (tmp);
		g_string_append_c (full_table_name, '.');
	}
	else if (quoted_schema) {
		v2 = gda_value_copy (schema);
		g_string_append (full_table_name, quoted_schema);
		g_string_append_c (full_table_name, '.');
	}
	g_value_set_string ((v3 = gda_value_new (G_TYPE_STRING)), (gchar *) table_name);
	
	tmp = gda_sql_identifier_quote ((gchar *) table_name, NULL, NULL, FALSE, FALSE);
	g_string_append (full_table_name, tmp);
	g_free (tmp);

	/* a new GdaMetaDbObject structure */
	dbobj = g_new0 (GdaMetaDbObject, 1);
	dbobj->obj_type = GDA_META_DB_TABLE;
	dbobj->obj_catalog = catalog ? g_value_dup_string (catalog) : NULL;
	dbobj->obj_schema = table_schema ? g_strdup ((gchar *) table_schema) : 
		(schema ? g_value_dup_string (schema) : NULL);
	dbobj->obj_name = g_strdup ((gchar *) table_name);
	dbobj->obj_full_name = full_table_name->str;
	g_string_free (full_table_name, FALSE);
	dbobj = _gda_meta_struct_add_db_object (mstruct, dbobj, error);
	if (!dbobj)
		goto onerror;
	
	/* walk through the columns and Fkey nodes */
	GdaMetaTable *mtable = GDA_META_TABLE (dbobj);
	xmlNodePtr cnode;
	pk_cols_array = g_array_new (FALSE, FALSE, sizeof (gint));
	gint colsindex = 0;
	for (cnode = node->children; cnode; cnode = cnode->next) {
		if (!strcmp ((gchar *) cnode->name, "column")) {
			xmlChar *cname, *ctype, *xstr, *extra;
                        gboolean pkey = FALSE;
                        gboolean nullok = FALSE;
 
                        if (strcmp ((gchar *) cnode->name, "column"))
                                continue;
                        cname = xmlGetProp (cnode, BAD_CAST "name");
                        if (!cname) {
				g_set_error (error, GDA_META_STRUCT_ERROR,
					     GDA_META_STRUCT_XML_ERROR,
					     _("Missing column name for table '%s'"),
					     dbobj->obj_full_name);
				goto onerror;
			}
			xstr = xmlGetProp (cnode, BAD_CAST "pkey");
                        if (xstr) {
                                if ((*xstr == 't') || (*xstr == 'T'))
                                        pkey = TRUE;
                                xmlFree (xstr);
                        }
                        xstr = xmlGetProp (cnode, BAD_CAST "nullok");
                        if (xstr) {
                                if ((*xstr == 't') || (*xstr == 'T'))
                                        nullok = TRUE;
                                xmlFree (xstr);
                        }
                        ctype = xmlGetProp (cnode, BAD_CAST "type");

                        /* a field */
			GdaMetaTableColumn *tcol;

			tcol = g_new0 (GdaMetaTableColumn, 1);
			tcol->column_name = g_strdup ((gchar *) cname);
			xmlFree (cname);
			tcol->column_type = g_strdup (ctype ? (gchar *) ctype : "string");
			tcol->gtype = ctype ? gda_g_type_from_string ((gchar *) ctype) : G_TYPE_STRING;
			if (ctype)
				xmlFree (ctype);
			if (tcol->gtype == G_TYPE_INVALID)
				tcol->gtype = GDA_TYPE_NULL;
			tcol->pkey = pkey;
			tcol->nullok = nullok;
			if (pkey) 
				g_array_append_val (pk_cols_array, colsindex);
			colsindex++;
				
			/* FIXME: handle default value */
			extra = xmlGetProp (cnode, BAD_CAST "autoinc");
			if (extra) {
				tcol->auto_incement = TRUE;
				xmlFree (extra);
			}

			mtable->columns = g_slist_append (mtable->columns, tcol);
		}
		else if (!strcmp ((gchar *) cnode->name, "fkey")) {
			xmlNodePtr fnode;
			xmlChar *ref_table;

			ref_table = xmlGetProp (cnode, BAD_CAST "ref_table");
			if (!ref_table) {
				g_set_error (error, GDA_META_STRUCT_ERROR,
					     GDA_META_STRUCT_XML_ERROR,
					     _("Missing foreign key's referenced table name for table '%s'"), 
					     dbobj->obj_full_name);
				goto onerror;
			}
			
			/* referenced GdaMetaDbObject */
			GdaMetaDbObject *ref_obj;
			gchar *name_part, *schema_part, *catalog_part = NULL;
			gchar *tmp = g_strdup ((gchar *) ref_table);
			if (!_split_identifier_string (tmp, &schema_part, &name_part)) {
				g_set_error (error, GDA_META_STRUCT_ERROR,
					     GDA_META_STRUCT_XML_ERROR,
					     _("Invalid referenced table name '%s'"), ref_table);
				xmlFree (ref_table);
				goto onerror;
			}
			if (schema_part && !_split_identifier_string (schema_part, &catalog_part, &schema_part)) {
				g_set_error (error, GDA_META_STRUCT_ERROR,
					     GDA_META_STRUCT_XML_ERROR,
					     _("Invalid referenced table name '%s'"), ref_table);
				xmlFree (ref_table);
				goto onerror;
			}
			if (catalog_part) {
				g_set_error (error, GDA_META_STRUCT_ERROR,
					     GDA_META_STRUCT_XML_ERROR,
					     _("Invalid referenced table name '%s'"), ref_table);
				xmlFree (ref_table);
				goto onerror;
			}
			GValue *rv2 = NULL, *rv3;
			if (schema_part)
				g_value_set_string ((rv2 = gda_value_new (G_TYPE_STRING)), schema_part);
			g_value_set_string ((rv3 = gda_value_new (G_TYPE_STRING)), name_part);
			ref_obj = gda_meta_struct_get_db_object (mstruct, NULL, rv2, rv3);
			if (rv2) gda_value_free (rv2);
			gda_value_free (rv3);

			if (!ref_obj) {
				ref_obj = g_new0 (GdaMetaDbObject, 1);
				ref_obj->obj_type = GDA_META_DB_UNKNOWN;
				ref_obj->obj_full_name = g_strdup ((gchar *) ref_table);
				ref_obj->obj_name = name_part;
				ref_obj->obj_schema = schema_part;
				xmlFree (ref_table);
				ref_obj = _gda_meta_struct_add_db_object (mstruct, ref_obj, error);
				if (! ref_obj) 
					goto onerror;
			}
			else
				xmlFree (ref_table);
		
			/* GdaMetaTableForeignKey structure */
			GdaMetaTableForeignKey *mfkey;
			GArray *fk_names_array = g_array_new (FALSE, FALSE, sizeof (gchar *));
			GArray *ref_pk_names_array = g_array_new (FALSE, FALSE, sizeof (gchar *));
			
			for (fnode = cnode->children; fnode; fnode = fnode->next) {
				xmlChar *col, *ref_col;
				gchar *tmp;
				if (strcmp ((gchar *) fnode->name, "part"))
					continue;
				col = xmlGetProp (fnode, BAD_CAST "column");
				if (!col) {
					g_set_error (error, GDA_META_STRUCT_ERROR,
						     GDA_META_STRUCT_XML_ERROR,
						     _("Missing foreign key's column name for table '%s'"), 
						     dbobj->obj_full_name);
					g_array_free (fk_names_array, TRUE);
					g_array_free (ref_pk_names_array, TRUE);
					goto onerror;
				}
				tmp = g_strdup ((gchar *) col);
				g_array_append_val (fk_names_array, tmp);
				ref_col = xmlGetProp (fnode, BAD_CAST "ref_column");
				if (ref_col) {
					tmp = g_strdup ((gchar *) ref_col);
					g_array_append_val (ref_pk_names_array, tmp);
					xmlFree (ref_col);
				}
				else {
					tmp = g_strdup ((gchar *) col);
					g_array_append_val (ref_pk_names_array, tmp);
				}
				xmlFree (col);
			}

			mfkey = g_new0 (GdaMetaTableForeignKey, 1);
			mfkey->meta_table = dbobj;
			mfkey->depend_on = ref_obj;
			mfkey->cols_nb = fk_names_array->len;
			mfkey->fk_cols_array = NULL;
			mfkey->fk_names_array = (gchar **) fk_names_array->data;
			mfkey->ref_pk_cols_array = NULL;
			mfkey->ref_pk_names_array = (gchar **) ref_pk_names_array->data;
			g_array_free (fk_names_array, FALSE);
			g_array_free (ref_pk_names_array, FALSE);
			mtable->fk_list = g_slist_append (mtable->fk_list, mfkey);

			if (mfkey->depend_on->obj_type == GDA_META_DB_TABLE) {
				GdaMetaTable *dot = GDA_META_TABLE (mfkey->depend_on);
				dot->reverse_fk_list = g_slist_prepend (dot->reverse_fk_list, mfkey);
			}
			dbobj->depend_list = g_slist_append (dbobj->depend_list, mfkey->depend_on);
		}
		/* FIXME: Add support for unique node */
		if (!strcmp ((gchar *) cnode->name, "unique")) {
			TO_IMPLEMENT;
		}
	}
	mtable->pk_cols_array = (gint*) pk_cols_array->data;
	mtable->pk_cols_nb = pk_cols_array->len;
	g_array_free (pk_cols_array, FALSE);

	return dbobj;

 onerror:
	if (v1) gda_value_free (v1);
	if (v2) gda_value_free (v2);
	gda_value_free (v3);

	if (pk_cols_array)
		g_array_free (pk_cols_array, TRUE);

	return NULL;
}

/*
static GdaMetaDbObject *
create_view_object (GdaMetaStruct *mstruct, const GValue *catalog, const gchar *quoted_catalog,
		    const GValue *schema, const gchar *quoted_schema, xmlNodePtr node, GError **error)
{
	TO_IMPLEMENT;
	return NULL;
}
*/

