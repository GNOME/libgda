/* gda-meta-struct.c
 *
 * Copyright (C) 2008 Vivien Malerba
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

#include <string.h>
#include <glib/gi18n-lib.h>
#include <libgda/gda-meta-store.h>
#include <libgda/gda-meta-struct.h>
#include <libgda/gda-util.h>
#include <sql-parser/gda-sql-parser.h>
#include <sql-parser/gda-sql-statement.h>

/* module error */
GQuark gda_meta_struct_error_quark (void) {
        static GQuark quark;
        if (!quark)
                quark = g_quark_from_static_string ("gda_meta_struct_error");
        return quark;
}

static void gda_meta_db_object_free (GdaMetaDbObject *dbo);
static void gda_meta_table_free_contents (GdaMetaTable *table);
static void gda_meta_view_free_contents (GdaMetaView *view);
static void gda_meta_table_column_free (GdaMetaTableColumn *tcol);
static void gda_meta_table_foreign_key_free (GdaMetaTableForeignKey *tfk);

/**
 * gda_meta_struct_new
 *
 * Creates a new initialized #GdaMetaStruct structure
 */
GdaMetaStruct *
gda_meta_struct_new (void)
{
	GdaMetaStruct *mstruct;
	mstruct = g_new0 (GdaMetaStruct, 1);
	mstruct->db_objects = NULL;
	mstruct->index = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

	return mstruct;
}

static void
compute_view_dependencies (GdaMetaStruct *mstruct, GdaMetaDbObject *view_dbobj, GdaSqlStatement *sqlst) {	
	if (sqlst->stmt_type == GDA_SQL_STATEMENT_SELECT) {
		GdaSqlStatementSelect *selst;
		selst = (GdaSqlStatementSelect*) (sqlst->contents);
		GSList *targets;
		for (targets = selst->from->targets; targets; targets = targets->next) {
			GdaSqlSelectTarget *t = (GdaSqlSelectTarget *) targets->data;
			GValue *catalog, *schema, *name;
			GdaMetaDbObject *ref_obj;

			if (!t->table_name)
				continue;
			/* FIXME: compute catalog, schema, name using t->table_name */
			/*
			ref_obj = gda_meta_struct_get_db_object (mstruct, catalog, schema, name);
			if (!ref_obj) {
				gchar *str;
				ref_obj = g_new0 (GdaMetaDbObject, 1);
				ref_obj->obj_type = GDA_META_DB_UNKNOWN;
				ref_obj->obj_catalog = g_strdup (g_value_get_string (fk_catalog));
				ref_obj->obj_schema = g_strdup (g_value_get_string (fk_schema));
				ref_obj->obj_name = g_strdup (g_value_get_string (fk_name));
				mstruct->db_objects = g_slist_append (mstruct->db_objects, ref_obj);
				str = g_strdup_printf ("%s.%s.%s", g_value_get_string (fk_catalog), 
						       g_value_get_string (fk_schema), 
						       g_value_get_string (fk_name));
				g_hash_table_insert (mstruct->index, str, ref_obj);
			}
			view_dbobj->depend_list = g_slist_append (view_dbobj->depend_list, ref_obj);
			*/
		}
	}
	else if (sqlst->stmt_type == GDA_SQL_STATEMENT_COMPOUND) {
		GdaSqlStatementCompound *cst;
		GSList *list;
		cst = (GdaSqlStatementCompound*) (sqlst->contents);
		for (list = cst->stmt_list; list; list = list->next)
			compute_view_dependencies (mstruct, view_dbobj, (GdaSqlStatement*) list->data);
	}
	else
		g_assert_not_reached ();
}

/**
 * gda_meta_struct_complement
 * @mstruct: a #GdaMetaStruct structure
 * @store: the #GdaMetaStore to use
 * @type: the type of object to add
 * @catalog: the catalog the object belongs to (as a G_TYPE_STRING GValue)
 * @schema: the schema the object belongs to (as a G_TYPE_STRING GValue)
 * @name: the object's name (as a G_TYPE_STRING GValue)
 * @error: a place to store errors, or %NULL
 *
 * Creates a new #GdaMetaDbObject structure in @mstruct to represent the database object (of type @type)
 * which can be uniquely identified as @catalog.@schema.@name.
 *
 * Returns: the #GdaMetaDbObject corresponding to the database object if no error occurred, or %NULL
 */
GdaMetaDbObject *
gda_meta_struct_complement (GdaMetaStruct *mstruct, GdaMetaStore *store, GdaMetaDbObjectType type,
			    const GValue *catalog, const GValue *schema, const GValue *name, 
			    GError **error)
{
	GdaMetaDbObject *dbo = NULL;

	g_return_val_if_fail (GDA_IS_META_STORE (store), NULL);
	g_return_val_if_fail (mstruct, NULL);
	g_return_val_if_fail (catalog && (G_VALUE_TYPE (catalog) == G_TYPE_STRING), NULL);
	g_return_val_if_fail (schema && (G_VALUE_TYPE (schema) == G_TYPE_STRING), NULL);
	g_return_val_if_fail (name && (G_VALUE_TYPE (name) == G_TYPE_STRING), NULL);
	
	/* create new GdaMetaDbObject or get already existing one */
	dbo = gda_meta_struct_get_db_object (mstruct, catalog, schema, name);
	if (!dbo) {
		dbo = g_new0 (GdaMetaDbObject, 1);
		dbo->obj_catalog = g_strdup (g_value_get_string (catalog));
		dbo->obj_schema = g_strdup (g_value_get_string (schema));
		dbo->obj_name = g_strdup (g_value_get_string (name));
		dbo->obj_short_name = NULL;
		dbo->obj_full_name = NULL;
	}
	else if (dbo->obj_type == type)
		return dbo; /* nothing to do */
	else if (dbo->obj_type != GDA_META_DB_UNKNOWN) {
		/* DB Object already exists, return an error */
		g_set_error (error, GDA_META_STRUCT_ERROR, GDA_META_STRUCT_DUPLICATE_OBJECT_ERROR,
			     _("Object %s.%s.%s already exists in GdaMetaStruct and has a different object type"), 
			     g_value_get_string (catalog), g_value_get_string (schema), 
			     g_value_get_string (name));
		dbo = NULL;
		goto onerror;
	}
	dbo->obj_type = type;

	switch (type) {
	case GDA_META_DB_VIEW: {
		gchar *sql = "SELECT view_definition, is_updatable FROM _views "
			"WHERE table_catalog = ##tc::string "
			"AND table_schema = ##ts::string AND table_name = ##tname::string";
		GdaDataModel *model;
		gint nrows;
		GdaMetaView *mv;

		model = gda_meta_store_extract (store, sql, error, "tc", catalog, "ts", schema, "tname", name, NULL);
		if (!model) 
			goto onerror;
		nrows = gda_data_model_get_n_rows (model);
		if (nrows < 1) {
			g_object_unref (model);
			g_set_error (error, GDA_META_STRUCT_ERROR, GDA_META_STRUCT_UNKNOWN_OBJECT_ERROR,
				     _("View %s.%s.%s not found in meta store object"), 
				     g_value_get_string (catalog), g_value_get_string (schema), 
				     g_value_get_string (name));
			goto onerror;
		}
		
		mv = GDA_META_DB_OBJECT_GET_VIEW (dbo);
		mv->view_def = g_strdup (g_value_get_string (gda_data_model_get_value_at (model, 0, 0)));
		mv->is_updatable = g_value_get_boolean (gda_data_model_get_value_at (model, 1, 0));

		/* view's dependencies, from its definition */
		if (mv->view_def && *mv->view_def) {
			static GdaSqlParser *parser = NULL;
			GdaStatement *stmt;
			const gchar *remain;
			if (!parser)
				parser = gda_sql_parser_new ();
			stmt = gda_sql_parser_parse_string (parser, mv->view_def, &remain, NULL);
			if (stmt &&
			    ((gda_statement_get_statement_type (stmt) == GDA_SQL_STATEMENT_SELECT) ||
			     (gda_statement_get_statement_type (stmt) == GDA_SQL_STATEMENT_COMPOUND))) {
				GdaSqlStatement *sqlst;
				g_object_get (G_OBJECT (stmt), "structure", &sqlst, NULL);
				compute_view_dependencies (mstruct, dbo, sqlst);
				gda_sql_statement_free (sqlst);
				g_object_unref (stmt);
				
#ifdef GDA_DEBUG_NO
				g_print ("View %s depends on: ", dbo->obj_name);
				GSList *list;
				for (list = dbo->depend_list; list; list = list->next) 
					g_print ("%s ", GDA_META_DB_OBJECT (list->data)->obj_name);
				g_print ("\n");
#endif
			}
		}
	}
	case GDA_META_DB_TABLE: {
		/* columns */
		gchar *sql = "SELECT c.column_name, c.data_type, c.gtype, c.is_nullable, t.table_short_name, t.table_full_name, "
			"c.column_default FROM _columns as c NATURAL JOIN _tables as t WHERE table_catalog = ##tc::string "
			"AND table_schema = ##ts::string AND table_name = ##tname::string "
			"ORDER BY ordinal_position";
		GdaMetaTable *mt;
		GdaDataModel *model;
		gint i, nrows;

		model = gda_meta_store_extract (store, sql, error, "tc", catalog, "ts", schema, "tname", name, NULL);
		if (!model) 
			goto onerror;

		nrows = gda_data_model_get_n_rows (model);
		if (nrows < 1) {
			g_object_unref (model);
			g_set_error (error, GDA_META_STRUCT_ERROR, GDA_META_STRUCT_UNKNOWN_OBJECT_ERROR,
				     _("Table %s.%s.%s not found in meta store object"), 
				     g_value_get_string (catalog), g_value_get_string (schema), 
				     g_value_get_string (name));
			goto onerror;
		}

		mt = GDA_META_DB_OBJECT_GET_TABLE (dbo);
		for (i = 0; i < nrows; i++) {
			GdaMetaTableColumn *tcol;
			const GValue *val;
			tcol = g_new0 (GdaMetaTableColumn, 1);
			tcol->column_name = g_strdup (g_value_get_string (gda_data_model_get_value_at (model, 0, i)));
			tcol->column_type = g_strdup (g_value_get_string (gda_data_model_get_value_at (model, 1, i)));
			tcol->gtype = gda_g_type_from_string (g_value_get_string (gda_data_model_get_value_at (model, 2, i)));
			tcol->nullok = g_value_get_boolean (gda_data_model_get_value_at (model, 3, i));
			val = gda_data_model_get_value_at (model, 1, 6);
			if (val && !gda_value_is_null (val))
				tcol->default_value = g_strdup (g_value_get_string (val));

			/* Note: tcol->pkey is not determined here */
			mt->columns = g_slist_prepend (mt->columns, tcol);

			if (i == 0) {
				dbo->obj_short_name = g_strdup (g_value_get_string (gda_data_model_get_value_at (model, 4, i)));
				dbo->obj_full_name = g_strdup (g_value_get_string (gda_data_model_get_value_at (model, 5, i)));
			}
		}
		mt->columns = g_slist_reverse (mt->columns);
		g_object_unref (model);

		/* primary key */
		sql = "SELECT constraint_catalog, constraint_schema, constraint_name FROM _table_constraints WHERE constraint_type='PRIMARY KEY' AND table_catalog = ##tc::string AND table_schema = ##ts::string AND table_name = ##tname::string";
		model = gda_meta_store_extract (store, sql, error, "tc", catalog, "ts", schema, "tname", name, NULL);
		if (!model) 
			goto onerror;

		nrows = gda_data_model_get_n_rows (model);
		if (nrows >= 1) {
			GdaDataModel *pkmodel;
			sql = "SELECT column_name FROM _key_column_usage WHERE constraint_catalog = ##cc::string AND constraint_schema = ##cs::string AND constraint_name = ##cname::string ORDER BY ordinal_position";
			pkmodel = gda_meta_store_extract (store, sql, error, 
							  "cc", gda_data_model_get_value_at (model, 0, 0),
							  "cs", gda_data_model_get_value_at (model, 1, 0),
							  "cname", gda_data_model_get_value_at (model, 2, 0), NULL);
			if (!pkmodel) {
				g_object_unref (model);
				goto onerror;
			}
			nrows = gda_data_model_get_n_rows (pkmodel);
			mt->pk_cols_nb = nrows;
			mt->pk_cols_array = g_new0 (gint, mt->pk_cols_nb);
			for (i = 0; i < nrows; i++) {
				GdaMetaTableColumn *tcol;
				tcol = gda_meta_struct_get_table_column (mstruct, mt, 
									 gda_data_model_get_value_at (model, 0, i));
				if (!tcol) {
					mt->pk_cols_array [i] = -1;
					g_warning (_("Internal GdaMetaStore error: column %s not found"),
						   g_value_get_string (gda_data_model_get_value_at (model, 0, i)));
				}
				else {
					mt->pk_cols_array [i] = g_slist_index (mt->columns, tcol);
					tcol->pkey = TRUE;
				}
			}

			g_object_unref (pkmodel);
		}
		g_object_unref (model);

		/* foreign keys */
		sql = "SELECT t.table_catalog, t.table_schema, t.table_name FROM _tables as f INNER JOIN _table_constraints as tc ON (tc.table_catalog=f.table_catalog AND tc.table_schema=f.table_schema AND tc.table_name=f.table_name) INNER JOIN _referential_constraints as rc ON (rc.constraint_catalog = tc.constraint_catalog AND rc.constraint_schema=tc.constraint_schema AND rc.constraint_name=tc.constraint_name) INNER JOIN _table_constraints as tc2 ON (rc.unique_constraint_catalog = tc2.constraint_catalog AND rc.unique_constraint_schema=tc2.constraint_schema AND rc.unique_constraint_name=tc2.constraint_name) INNER JOIN _tables as t ON (tc2.table_catalog=t.table_catalog AND tc2.table_schema=t.table_schema AND tc2.table_name=t.table_name) WHERE f.table_catalog = ##tc::string AND f.table_schema = ##ts::string AND f.table_name = ##tname::string";
		model = gda_meta_store_extract (store, sql, error, "tc", catalog, "ts", schema, "tname", name, NULL);
		if (!model) 
			goto onerror;

		nrows = gda_data_model_get_n_rows (model);
		for (i = 0; i < nrows; i++) {
			GdaMetaTableForeignKey *tfk;
			const GValue *fk_catalog, *fk_schema, *fk_name;

			fk_catalog = gda_data_model_get_value_at (model, 0, i);
			fk_schema = gda_data_model_get_value_at (model, 1, i);
			fk_name = gda_data_model_get_value_at (model, 2, i);
			tfk = g_new0 (GdaMetaTableForeignKey, 1);
			tfk->meta_table = dbo;
			tfk->depend_on = gda_meta_struct_get_db_object (mstruct, fk_catalog, fk_schema, fk_name);
			if (!tfk->depend_on) {
				gchar *str;
				tfk->depend_on = g_new0 (GdaMetaDbObject, 1);
				tfk->depend_on->obj_type = GDA_META_DB_UNKNOWN;
				tfk->depend_on->obj_catalog = g_strdup (g_value_get_string (fk_catalog));
				tfk->depend_on->obj_schema = g_strdup (g_value_get_string (fk_schema));
				tfk->depend_on->obj_name = g_strdup (g_value_get_string (fk_name));
				mstruct->db_objects = g_slist_append (mstruct->db_objects, tfk->depend_on);
				str = g_strdup_printf ("%s.%s.%s", g_value_get_string (fk_catalog), 
						       g_value_get_string (fk_schema), 
						       g_value_get_string (fk_name));
				g_hash_table_insert (mstruct->index, str, tfk->depend_on);
			}
			dbo->depend_list = g_slist_append (dbo->depend_list, tfk->depend_on);

			/* FIXME: compute @cols_nb, and all the @*_array members (ref_pk_cols_array must be
			 * initialized with -1 values everywhere */

			mt->fk_list = g_slist_prepend (mt->fk_list, tfk);
		}
		mt->fk_list = g_slist_reverse (mt->fk_list);
		g_object_unref (model);
		/* Note: mt->reverse_fk_list is not determined here */
		
		break;
	}
	default:
		TO_IMPLEMENT;
	}

	if (dbo && !g_slist_find (mstruct->db_objects, dbo)) {
		gchar *str;
		mstruct->db_objects = g_slist_append (mstruct->db_objects, dbo);
		str = g_strdup_printf ("%s.%s.%s", g_value_get_string (catalog), 
				       g_value_get_string (schema), 
				       g_value_get_string (name));
		g_hash_table_insert (mstruct->index, str, dbo);
	}
	if (dbo) {
		/* compute GdaMetaTableForeignKey's @ref_pk_cols_array arrays and GdaMetaTable' @reverse_fk_list lists*/
		GSList *list;
		for (list = mstruct->db_objects; list; list = list->next) {
			GdaMetaDbObject *tmpdbo;
			tmpdbo = GDA_META_DB_OBJECT (list->data);
			if (tmpdbo->obj_type != GDA_META_DB_TABLE)
				continue;
			GdaMetaTable *mt = GDA_META_DB_OBJECT_GET_TABLE (tmpdbo);
			GSList *klist;
			for (klist = mt->fk_list; klist; klist = klist->next) {
				GdaMetaTableForeignKey *tfk = GDA_META_TABLE_FOREIGN_KEY (klist->data);
				GdaMetaTable *ref_mt = GDA_META_DB_OBJECT_GET_TABLE (tfk->depend_on);
				gint i;
				for (i = 0; i < tfk->cols_nb; i++) {
					gint col;
					GdaMetaTableColumn *r_tcol;
					GValue *r_val;

					if (tfk->ref_pk_cols_array[i] != -1) /* already correctly set */
						continue;

					if (tfk->depend_on->obj_type != GDA_META_DB_TABLE)
						continue; /* can't be set now */

					g_value_set_string ((r_val = gda_value_new (G_TYPE_STRING)), tfk->ref_pk_names_array[i]);
					r_tcol = gda_meta_struct_get_table_column (mstruct, ref_mt, r_val);
					gda_value_free (r_val);
					col = g_slist_index (ref_mt->columns, r_tcol);
					if (!r_tcol || (col < 0)) {
						g_set_error (error, GDA_META_STRUCT_ERROR, GDA_META_STRUCT_INCOHERENCE_ERROR,
							     _("Foreign key column '%s' not found in table '%s'"),
							     tfk->ref_pk_names_array[i], tfk->depend_on->obj_name);
						dbo = NULL;
						goto onerror;
					}
					tfk->ref_pk_cols_array[i] = col;
				}
				ref_mt->reverse_fk_list = g_slist_append (ref_mt->reverse_fk_list, tmpdbo);
			}
		}
	}
	return dbo;

 onerror:
	if (dbo)
		gda_meta_db_object_free (dbo);
	return NULL;
}

/*
 * Makes a list of all the GdaMetaDbObject structures listed in @objects
 * which are not present in @ordered_list and for which no dependency is in @ordered_list
 */
static GSList *
build_pass (GSList *objects, GSList *ordered_list)
{
	GSList *retlist = NULL, *list;

	for (list = objects; list; list = list->next) {
		gboolean has_dep = FALSE;
		GSList *dep_list;
		if (g_slist_find (ordered_list, list->data))
			continue;
		for (dep_list = GDA_META_DB_OBJECT (list->data)->depend_list; dep_list; dep_list = dep_list->next) {
			if (!g_slist_find (ordered_list, dep_list->data)) {
				has_dep = TRUE;
				break;
			}
		}
		if (has_dep)
			continue;
		retlist = g_slist_prepend (retlist, list->data);
	}

#ifdef GDA_DEBUG_NO
	g_print (">> PASS\n");
	for (list = retlist; list; list = list->next) 
		g_print ("--> %s\n", GDA_META_DB_OBJECT (list->data)->obj_name);
	g_print ("<<\n");
#endif

	return retlist;
}

/**
 * gda_meta_struct_order_db_objects
 * @mstruct: a #GdaMetaStruct structure
 * @error: a place to store errors, or %NULL
 *
 * Reorders the list of database objects within @mstruct in a way that for any given GdaMetaDbObject in the list, 
 * all the dependencies are _before_ it in the list.
 *
 * Returns: TRUE if no error occurred
 */ 
gboolean
gda_meta_struct_order_db_objects (GdaMetaStruct *mstruct, GError **error)
{
	GSList *pass_list;
	GSList *ordered_list = NULL;

	g_return_val_if_fail (mstruct, FALSE);
	for (pass_list = build_pass (mstruct->db_objects, ordered_list); 
	     pass_list; 
	     pass_list = build_pass (mstruct->db_objects, ordered_list)) 
		ordered_list = g_slist_concat (ordered_list, pass_list);

#ifdef GDA_DEBUG_NO
	GSList *list;
	for (list = ordered_list; list; list = list->next) 
		g_print ("--> %s\n", GDA_META_DB_OBJECT (list->data)->obj_name);
#endif

	g_slist_free (mstruct->db_objects);
	mstruct->db_objects = ordered_list;
	return TRUE;
}

/**
 * gda_meta_struct_get_db_object
 * @mstruct: a #GdaMetaStruct structure
 * @catalog: the catalog the object belongs to (as a G_TYPE_STRING GValue)
 * @schema: the schema the object belongs to (as a G_TYPE_STRING GValue)
 * @name: the object's name (as a G_TYPE_STRING GValue)
 *
 * Tries to locate the #GdaMetaDbObject structure representing the database object named after
 * @catalog, @schema and @name.
 *
 * Returns: the #GdaMetaDbObject or %NULL if not found
 */
GdaMetaDbObject *
gda_meta_struct_get_db_object (GdaMetaStruct *mstruct, const GValue *catalog, const GValue *schema, const GValue *name)
{
	gchar *key;
	GdaMetaDbObject *dbo;

	g_return_val_if_fail (mstruct, NULL);
	g_return_val_if_fail (catalog && (G_VALUE_TYPE (catalog) == G_TYPE_STRING), NULL);
	g_return_val_if_fail (schema && (G_VALUE_TYPE (schema) == G_TYPE_STRING), NULL);
	g_return_val_if_fail (name && (G_VALUE_TYPE (name) == G_TYPE_STRING), NULL);

	key = g_strdup_printf ("%s.%s.%s", g_value_get_string (catalog), g_value_get_string (schema), 
			       g_value_get_string (name));
	dbo = g_hash_table_lookup (mstruct->index, key);
	g_free (key);
	return dbo;
}

/**
 * gda_meta_struct_get_table_column
 * @mstruct: a #GdaMetaStruct structure
 * @table: the #GdaMetaTable structure to find the column for
 * @col_name: the name of the column to find (as a G_TYPE_STRING GValue)
 *
 * Tries to find the #GdaMetaTableColumn representing the column named @col_name in @table.
 *
 * Returns: the #GdaMetaTableColumn or %NULL if not found
 */
GdaMetaTableColumn *
gda_meta_struct_get_table_column (GdaMetaStruct *mstruct, GdaMetaTable *table, const GValue *col_name)
{
	GSList *list;
	const gchar *cname;
	g_return_val_if_fail (mstruct, NULL);
	g_return_val_if_fail (table, NULL);
	g_return_val_if_fail (col_name && (G_VALUE_TYPE (col_name) == G_TYPE_STRING), NULL);
	cname = g_value_get_string (col_name);

	for (list = table->columns; list; list = list->next) {
		GdaMetaTableColumn *tcol = GDA_META_TABLE_COLUMN (list->data);
		if (!strcmp (tcol->column_name, cname))
			return tcol;
	}
	return NULL;
}

/**
 * gda_meta_struct_free
 * @mstruct: a #GdaMetaStruct structure
 * 
 * Releases any memory associated to @mstruct.
 */
void
gda_meta_struct_free (GdaMetaStruct *mstruct)
{
	if (!mstruct)
		return;

	g_slist_foreach (mstruct->db_objects, (GFunc) gda_meta_db_object_free, NULL);
	g_slist_free (mstruct->db_objects);
	g_hash_table_destroy (mstruct->index);
	g_free (mstruct);
}


static void
gda_meta_db_object_free (GdaMetaDbObject *dbo)
{
	g_free (dbo->obj_catalog);
	g_free (dbo->obj_schema);
	g_free (dbo->obj_name);
	g_free (dbo->obj_short_name);
	g_free (dbo->obj_full_name);
	switch (dbo->obj_type) {
	case GDA_META_DB_UNKNOWN:
		break;
	case GDA_META_DB_TABLE:
		gda_meta_table_free_contents (GDA_META_DB_OBJECT_GET_TABLE (dbo));
		break;
	case GDA_META_DB_VIEW:
		gda_meta_view_free_contents (GDA_META_DB_OBJECT_GET_VIEW (dbo));
		break;
	default:
		TO_IMPLEMENT;
	}
	g_slist_free (dbo->depend_list);
}

static void
gda_meta_table_free_contents (GdaMetaTable *table)
{
	g_slist_foreach (table->columns, (GFunc) gda_meta_table_column_free, NULL);
	g_slist_free (table->columns);
	g_free (table->pk_cols_array);
	g_slist_foreach (table->fk_list, (GFunc) gda_meta_table_foreign_key_free, NULL);
	g_slist_free (table->fk_list);
	g_slist_free (table->reverse_fk_list);
}

static void
gda_meta_view_free_contents (GdaMetaView *view)
{
	gda_meta_table_free_contents ((GdaMetaTable*) view);
	g_free (view->view_def);
}

static void
gda_meta_table_column_free (GdaMetaTableColumn *tcol)
{
	g_free (tcol->column_name);
	g_free (tcol->column_type);
	g_free (tcol->default_value);
	g_free (tcol);
}

static void
gda_meta_table_foreign_key_free (GdaMetaTableForeignKey *tfk)
{
	gint i;
	for (i = 0; i < tfk->cols_nb; i++) {
		g_free (tfk->fk_names_array[i]);
		g_free (tfk->ref_pk_names_array[i]);
	}
	g_free (tfk->fk_cols_array);
	g_free (tfk->fk_names_array);
	g_free (tfk->ref_pk_cols_array);
	g_free (tfk->ref_pk_names_array);
	g_free (tfk);
}
