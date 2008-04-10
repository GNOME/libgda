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
#include <libgda/gda-meta-struct.h>
#include <libgda/gda-util.h>
#include <sql-parser/gda-sql-parser.h>
#include <sql-parser/gda-sql-statement.h>

/*
 * Main static functions
 */
static void gda_meta_struct_class_init (GdaMetaStructClass *klass);
static void gda_meta_struct_init (GdaMetaStruct *mstruct);
static void gda_meta_struct_finalize (GObject *object);

static void gda_meta_struct_set_property (GObject *object,
                                         guint param_id,
                                         const GValue *value,
                                         GParamSpec *pspec);
static void gda_meta_struct_get_property (GObject *object,
                                         guint param_id,
                                         GValue *value,
                                         GParamSpec *pspec);


struct _GdaMetaStructPrivate {
	GHashTable *index; /* key = [catalog].[schema].[name], value = a GdaMetaDbObject */
	guint       features;
};

static void gda_meta_db_object_free (GdaMetaDbObject *dbo);
static void gda_meta_table_free_contents (GdaMetaTable *table);
static void gda_meta_view_free_contents (GdaMetaView *view);
static void gda_meta_table_column_free (GdaMetaTableColumn *tcol);
static void gda_meta_table_foreign_key_free (GdaMetaTableForeignKey *tfk);

/* get a pointer to the parents to be able to call their destructor */
static GObjectClass  *parent_class = NULL;

/* properties */
enum {
        PROP_0,
        PROP_FEATURES
};

/* module error */
GQuark gda_meta_struct_error_quark (void) {
	static GQuark quark;
	if (!quark)
		quark = g_quark_from_static_string ("gda_meta_struct_error");
	return quark;
}

GType
gda_meta_struct_get_type (void) {
	static GType type = 0;
	
	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo info = {
			sizeof (GdaMetaStructClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_meta_struct_class_init,
			NULL,
			NULL,
			sizeof (GdaMetaStruct),
			0,
			(GInstanceInitFunc) gda_meta_struct_init
		};
		
		type = g_type_register_static (G_TYPE_OBJECT, "GdaMetaStruct", &info, 0);
	}
	return type;
}


static void
gda_meta_struct_class_init (GdaMetaStructClass *klass) {
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	
	parent_class = g_type_class_peek_parent (klass);
	
	/* Properties */
        object_class->set_property = gda_meta_struct_set_property;
        object_class->get_property = gda_meta_struct_get_property;
        g_object_class_install_property (object_class, PROP_FEATURES,
					 g_param_spec_uint ("features", _ ("Features to compute"), NULL, 
							    GDA_META_STRUCT_FEATURE_NONE, G_MAXINT,
							    GDA_META_STRUCT_FEATURE_ALL,
							    (G_PARAM_WRITABLE | G_PARAM_READABLE | 
							     G_PARAM_CONSTRUCT_ONLY)));

	/* virtual methods */
	object_class->finalize = gda_meta_struct_finalize;
}


static void
gda_meta_struct_init (GdaMetaStruct *mstruct) {
	mstruct->priv = g_new0 (GdaMetaStructPrivate, 1);
	mstruct->priv->index = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
}


/**
 * gda_meta_struct_new
 * @features: the kind of extra information the new #GdaMetaStruct object will compute
 *
 * Creates a new #GdaMetaStruct object. The @features specifies the extra features which will also be computed:
 * the more features, the more time it takes to run. Features such as table's columns, each column's attributes, etc
 * are not optional and will always be computed.
 *
 * Returns: the newly created #GdaMetaStruct object
 */
GdaMetaStruct *
gda_meta_struct_new (GdaMetaStructFeature features) 
{
	return (GdaMetaStruct*) g_object_new (GDA_TYPE_META_STRUCT, "features", features, NULL);
}

static void
gda_meta_struct_finalize (GObject   * object) {
	GdaMetaStruct *mstruct;
	
	g_return_if_fail (object != NULL);
	g_return_if_fail (GDA_IS_META_STRUCT (object));
	
	mstruct = GDA_META_STRUCT (object);
	if (mstruct->priv) {
		g_slist_foreach (mstruct->db_objects, (GFunc) gda_meta_db_object_free, NULL);
		g_slist_free (mstruct->db_objects);
		g_hash_table_destroy (mstruct->priv->index);
		g_free (mstruct->priv);
		mstruct->priv = NULL;
	}
	
	/* parent class */
	parent_class->finalize (object);
}

static void
gda_meta_struct_set_property (GObject *object,
			      guint param_id,
			      const GValue *value,
			      GParamSpec *pspec)
{
        GdaMetaStruct *mstruct;

        mstruct = GDA_META_STRUCT (object);
        if (mstruct->priv) {
                switch (param_id) {
		case PROP_FEATURES:
			mstruct->priv->features = g_value_get_uint (value);
			break;
		default:
			break;
		}
	}
}

static void
gda_meta_struct_get_property (GObject *object,
			      guint param_id,
			      GValue *value,
			      GParamSpec *pspec)
{
        GdaMetaStruct *mstruct;
        mstruct = GDA_META_STRUCT (object);

        if (mstruct->priv) {
                switch (param_id) {
		case PROP_FEATURES:
			g_value_set_uint (value, mstruct->priv->features);
			break;
		default:
			break;
		}
	}
}


static void
compute_view_dependencies (GdaMetaStruct *mstruct, GdaMetaStore *store, GdaMetaDbObject *view_dbobj, GdaSqlStatement *sqlst) 
{	
	if (sqlst->stmt_type == GDA_SQL_STATEMENT_SELECT) {
		GdaSqlStatementSelect *selst;
		selst = (GdaSqlStatementSelect*) (sqlst->contents);
		GSList *targets = NULL;
		if (selst->from)
			targets = selst->from->targets;
		for (; targets; targets = targets->next) {
			GdaSqlSelectTarget *t = (GdaSqlSelectTarget *) targets->data;
			GValue *catalog, *schema, *name;
			GdaMetaDbObject *ref_obj, *tmp_obj;
			GdaMetaStruct *m2;
			GValue *vname;

			if (!t->table_name)
				continue;
			
			m2 = gda_meta_struct_new (GDA_META_STRUCT_FEATURE_NONE);
			g_value_set_string ((vname = gda_value_new (G_TYPE_STRING)), t->table_name);
			if (! (tmp_obj = gda_meta_struct_complement (m2, store, GDA_META_DB_TABLE, NULL, NULL, vname, NULL)))
				tmp_obj = gda_meta_struct_complement (m2, store, GDA_META_DB_VIEW, NULL, NULL, vname, NULL);
			gda_value_free (vname);

			if (!tmp_obj) {
				/* could not find dependency */
				g_object_unref (m2);
				continue;
			}

			/* the dependency exists, and is identified by tmp_obj->obj_catalog, tmp_obj->obj_schema 
			 * and tmp_obj->obj_name */
			g_value_set_string ((catalog = gda_value_new (G_TYPE_STRING)), tmp_obj->obj_catalog);
			g_value_set_string ((schema = gda_value_new (G_TYPE_STRING)), tmp_obj->obj_schema);
			g_value_set_string ((name = gda_value_new (G_TYPE_STRING)), tmp_obj->obj_name);
			ref_obj = gda_meta_struct_get_db_object (mstruct, catalog, schema, name);
			if (!ref_obj) {
				gchar *str;
				ref_obj = g_new0 (GdaMetaDbObject, 1);
				ref_obj->obj_type = GDA_META_DB_UNKNOWN;
				ref_obj->obj_catalog = g_strdup (tmp_obj->obj_catalog);
				ref_obj->obj_schema = g_strdup (tmp_obj->obj_schema);
				ref_obj->obj_name = g_strdup (tmp_obj->obj_name);

				mstruct->db_objects = g_slist_append (mstruct->db_objects, ref_obj);
				str = g_strdup_printf ("%s.%s.%s", tmp_obj->obj_catalog,
						       tmp_obj->obj_schema, tmp_obj->obj_name);
				g_hash_table_insert (mstruct->priv->index, str, ref_obj);
			}
			g_assert (ref_obj);
			g_object_unref (m2);
			gda_value_free (catalog);
			gda_value_free (schema);
			gda_value_free (name);

			view_dbobj->depend_list = g_slist_append (view_dbobj->depend_list, ref_obj);
		}
	}
	else if (sqlst->stmt_type == GDA_SQL_STATEMENT_COMPOUND) {
		GdaSqlStatementCompound *cst;
		GSList *list;
		cst = (GdaSqlStatementCompound*) (sqlst->contents);
		for (list = cst->stmt_list; list; list = list->next)
			compute_view_dependencies (mstruct, store, view_dbobj, (GdaSqlStatement*) list->data);
	}
	else
		g_assert_not_reached ();
}

static gboolean determine_db_object_from_schema_and_name (GdaMetaStruct *mstruct, GdaMetaStore *store, 
							  GdaMetaDbObjectType *int_out_type, GValue **out_catalog,
							  GValue **out_short_name, GValue **out_full_name,
							  GValue **out_owner, const GValue *schema, const GValue *name);
static gboolean determine_db_object_from_short_name (GdaMetaStruct *mstruct, GdaMetaStore *store, 
						     GdaMetaDbObjectType *int_out_type, GValue **out_catalog,
						     GValue **out_schema, GValue **out_name, GValue **out_short_name, 
						     GValue **out_full_name, GValue **out_owner, const GValue *name);
static gboolean determine_db_object_from_missing_type (GdaMetaStruct *mstruct, GdaMetaStore *store, 
						       GdaMetaDbObjectType *out_type, GValue **out_short_name, 
						       GValue **out_full_name, GValue **out_owner, const GValue *catalog, 
						       const GValue *schema, const GValue *name);
static gchar *array_type_to_sql (GdaMetaStore *store, const GValue *specific_name);

/**
 * gda_meta_struct_complement
 * @mstruct: a #GdaMetaStruct object
 * @store: the #GdaMetaStore to use
 * @type: the type of object to add (which can be GDA_META_DB_UNKNOWN)
 * @catalog: the catalog the object belongs to (as a G_TYPE_STRING GValue), or %NULL
 * @schema: the schema the object belongs to (as a G_TYPE_STRING GValue), or %NULL
 * @name: the object's name (as a G_TYPE_STRING GValue), not %NULL
 * @error: a place to store errors, or %NULL
 *
 * Creates a new #GdaMetaDbObject structure in @mstruct to represent the database object (of type @type)
 * which can be uniquely identified as @catalog.@schema.@name.
 *
 * If @catalog is %NULL and @schema is not %NULL, then the database object will be the one which is
 * "visible" by default (that is which can be accessed only by its short @name name).
 *
 * If both @catalog and @schema are %NULL, then the database object will be the one which 
 * can be accessed by its @schema.@name name.
 *
 * Returns: the #GdaMetaDbObject corresponding to the database object if no error occurred, or %NULL
 */
GdaMetaDbObject *
gda_meta_struct_complement (GdaMetaStruct *mstruct, GdaMetaStore *store, GdaMetaDbObjectType type,
			    const GValue *catalog, const GValue *schema, const GValue *name, 
			    GError **error)
{
	GdaMetaDbObject *dbo = NULL;
	GdaMetaDbObjectType real_type = type;
	GValue *real_catalog = NULL, *real_schema = NULL, *real_name = NULL;
	GValue *short_name = NULL, *full_name=NULL, *owner=NULL;

	g_return_val_if_fail (GDA_IS_META_STORE (store), NULL);
	g_return_val_if_fail (GDA_IS_META_STRUCT (mstruct), NULL);
	g_return_val_if_fail (name && (G_VALUE_TYPE (name) == G_TYPE_STRING), NULL);

	if (!catalog) {
		if (schema) {
			g_return_val_if_fail (schema && (G_VALUE_TYPE (schema) == G_TYPE_STRING), NULL);
			if (! determine_db_object_from_schema_and_name (mstruct, store, &real_type, &real_catalog, 
									&short_name, &full_name, &owner,
									schema, name)) {
				g_set_error (error, GDA_META_STRUCT_ERROR, GDA_META_STRUCT_UNKNOWN_OBJECT_ERROR,
					     _("Could not find object named '%s.%s'"), 
					     g_value_get_string (schema), g_value_get_string (name));
				return NULL;
			}
			catalog = real_catalog;
		}
		else {
			if (! determine_db_object_from_short_name (mstruct, store, &real_type, &real_catalog, 
								   &real_schema, &real_name, 
								   &short_name, &full_name, &owner, name)) {
				g_set_error (error, GDA_META_STRUCT_ERROR, GDA_META_STRUCT_UNKNOWN_OBJECT_ERROR,
					     _("Could not find object named '%s'"), g_value_get_string (name));
				return NULL;
			}
			if (real_name)
				name = real_name;
			catalog = real_catalog;
			schema = real_schema;
		}
	}
	else if (type == GDA_META_DB_UNKNOWN) {
		g_return_val_if_fail (catalog && (G_VALUE_TYPE (catalog) == G_TYPE_STRING), NULL);
		g_return_val_if_fail (schema && (G_VALUE_TYPE (schema) == G_TYPE_STRING), NULL);

		if (! determine_db_object_from_missing_type (mstruct, store, &real_type, &short_name, &full_name, &owner,
							     catalog, schema, name)) {
			g_set_error (error, GDA_META_STRUCT_ERROR, GDA_META_STRUCT_UNKNOWN_OBJECT_ERROR,
				     _("Could not find object named '%s.%s.%s'"), g_value_get_string (catalog),
				     g_value_get_string (schema), g_value_get_string (name));
			return NULL;
		}
	}
	type = real_type;
	
	/* create new GdaMetaDbObject or get already existing one */
	dbo = gda_meta_struct_get_db_object (mstruct, catalog, schema, name);
	if (!dbo) {
		dbo = g_new0 (GdaMetaDbObject, 1);
		dbo->obj_catalog = g_strdup (g_value_get_string (catalog));
		dbo->obj_schema = g_strdup (g_value_get_string (schema));
		dbo->obj_name = g_strdup (g_value_get_string (name));
		if (short_name)
			dbo->obj_short_name = g_strdup (g_value_get_string (short_name));
		if (full_name)
			dbo->obj_full_name = g_strdup (g_value_get_string (full_name));
		if (owner)
			dbo->obj_owner = g_strdup (g_value_get_string (owner));
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
		gchar *sql = "SELECT view_definition, is_updatable, table_short_name, table_full_name, table_owner "
			"FROM _views NATURAL JOIN _tables "
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
		
		if (!dbo->obj_short_name)
			dbo->obj_short_name = g_strdup (g_value_get_string (gda_data_model_get_value_at (model, 2, 0)));
		if (!dbo->obj_full_name)
			dbo->obj_full_name = g_strdup (g_value_get_string (gda_data_model_get_value_at (model, 3, 0)));
		if (!dbo->obj_owner)
			dbo->obj_owner = g_strdup (g_value_get_string (gda_data_model_get_value_at (model, 4, 0)));

		mv = GDA_META_DB_OBJECT_GET_VIEW (dbo);
		mv->view_def = g_strdup (g_value_get_string (gda_data_model_get_value_at (model, 0, 0)));
		mv->is_updatable = g_value_get_boolean (gda_data_model_get_value_at (model, 1, 0));

		/* view's dependencies, from its definition */
		if ((mstruct->priv->features & GDA_META_STRUCT_FEATURE_VIEW_DEPENDENCIES) && 
		    mv->view_def && *mv->view_def) {
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
				compute_view_dependencies (mstruct, store, dbo, sqlst);
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
		gchar *sql = "SELECT c.column_name, c.data_type, c.gtype, c.is_nullable, t.table_short_name, t.table_full_name, c.column_default, t.table_owner, c.array_spec FROM _columns as c NATURAL JOIN _tables as t WHERE table_catalog = ##tc::string AND table_schema = ##ts::string AND table_name = ##tname::string ORDER BY ordinal_position";
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
		if (!dbo->obj_short_name)
			dbo->obj_short_name = g_strdup (g_value_get_string (gda_data_model_get_value_at (model, 4, 0)));
		if (!dbo->obj_full_name)
			dbo->obj_full_name = g_strdup (g_value_get_string (gda_data_model_get_value_at (model, 5, 0)));
		if (!dbo->obj_owner)
			dbo->obj_owner = g_strdup (g_value_get_string (gda_data_model_get_value_at (model, 7, 0)));

		mt = GDA_META_DB_OBJECT_GET_TABLE (dbo);
		for (i = 0; i < nrows; i++) {
			GdaMetaTableColumn *tcol;
			const GValue *val;
			const gchar *cstr = NULL;
			tcol = g_new0 (GdaMetaTableColumn, 1);
			tcol->column_name = g_strdup (g_value_get_string (gda_data_model_get_value_at (model, 0, i)));
			val = gda_data_model_get_value_at (model, 1, i);
			if (val && !gda_value_is_null (val))
				cstr = g_value_get_string (val);
			if (cstr && *cstr)
				tcol->column_type = g_strdup (cstr);
			else 
				tcol->column_type = array_type_to_sql (store, 
								       gda_data_model_get_value_at (model, 8, i));
			tcol->gtype = gda_g_type_from_string (g_value_get_string (gda_data_model_get_value_at (model, 2, i)));
			tcol->nullok = g_value_get_boolean (gda_data_model_get_value_at (model, 3, i));
			val = gda_data_model_get_value_at (model, 6, i);
			if (val && !gda_value_is_null (val))
				tcol->default_value = g_strdup (g_value_get_string (val));

			/* Note: tcol->pkey is not determined here */
			mt->columns = g_slist_prepend (mt->columns, tcol);
		}
		mt->columns = g_slist_reverse (mt->columns);
		g_object_unref (model);

		/* primary key */
		sql = "SELECT constraint_name FROM _table_constraints WHERE constraint_type='PRIMARY KEY' AND table_catalog = ##tc::string AND table_schema = ##ts::string AND table_name = ##tname::string";
		model = gda_meta_store_extract (store, sql, error, "tc", catalog, "ts", schema, "tname", name, NULL);
		if (!model) 
			goto onerror;

		nrows = gda_data_model_get_n_rows (model);
		if (nrows >= 1) {
			GdaDataModel *pkmodel;
			sql = "SELECT column_name FROM _key_column_usage WHERE table_catalog = ##cc::string AND table_schema = ##cs::string AND table_name = ##tname::string AND constraint_name = ##cname::string ORDER BY ordinal_position";
			pkmodel = gda_meta_store_extract (store, sql, error, 
							  "cc", catalog,
							  "cs", schema,
							  "tname", name,
							  "cname", gda_data_model_get_value_at (model, 0, 0), NULL);
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
									 gda_data_model_get_value_at (pkmodel, 0, i));
				if (!tcol) {
					mt->pk_cols_array [i] = -1;
					g_warning (_("Internal GdaMetaStore error: column %s not found"),
						   g_value_get_string (gda_data_model_get_value_at (pkmodel, 0, i)));
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
		if (mstruct->priv->features & GDA_META_STRUCT_FEATURE_FOREIGN_KEYS) { 
			sql = "SELECT ref_table_catalog, ref_table_schema, ref_table_name FROM _referential_constraints WHERE table_catalog = ##tc::string AND table_schema = ##ts::string AND table_name = ##tname::string";
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
					g_hash_table_insert (mstruct->priv->index, str, tfk->depend_on);
				}
				dbo->depend_list = g_slist_append (dbo->depend_list, tfk->depend_on);
				
				/* FIXME: compute @cols_nb, and all the @*_array members (ref_pk_cols_array must be
				 * initialized with -1 values everywhere */
				
				mt->fk_list = g_slist_prepend (mt->fk_list, tfk);
			}
			mt->fk_list = g_slist_reverse (mt->fk_list);
			g_object_unref (model);
			/* Note: mt->reverse_fk_list is not determined here */
		}
		
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
		g_hash_table_insert (mstruct->priv->index, str, dbo);
	}
	if (dbo && (mstruct->priv->features & GDA_META_STRUCT_FEATURE_FOREIGN_KEYS)) {
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
	if (real_catalog) gda_value_free (real_catalog);
	if (real_schema) gda_value_free (real_schema);
	if (real_name) gda_value_free (real_name);
	if (short_name)	gda_value_free (short_name);
	if (full_name)	gda_value_free (full_name);
	if (owner) gda_value_free (owner);
	return dbo;

 onerror:
	if (dbo)
		gda_meta_db_object_free (dbo);
	if (real_catalog) gda_value_free (real_catalog);
	if (real_schema) gda_value_free (real_schema);
	if (real_name) gda_value_free (real_name);
	if (short_name)	gda_value_free (short_name);
	if (full_name)	gda_value_free (full_name);
	if (owner) gda_value_free (owner);

	return NULL;
}

static gchar *
array_type_to_sql (GdaMetaStore *store, const GValue *specific_name)
{
	gchar *str;
	GdaDataModel *model;
	gint nrows;
	const GValue *cvalue;

	if (!specific_name || gda_value_is_null (specific_name)) 
		return g_strdup ("[]");

	model = gda_meta_store_extract (store, 	"SELECT data_type, array_spec FROM _element_types WHERE specific_name = ##name::string",
					NULL, "name", specific_name, NULL);
	if (!model) 
		return g_strdup ("[]");
	nrows = gda_data_model_get_n_rows (model);
	if (nrows != 1) {
		g_object_unref (model);
		return g_strdup ("[]");
	}
	
	cvalue = gda_data_model_get_value_at (model, 0, 0);
	if (!cvalue || gda_value_is_null (cvalue) || !g_value_get_string (cvalue)) {
		/* use array_spec */
		gchar *str2;
		cvalue = gda_data_model_get_value_at (model, 1, 0);
		str2 = array_type_to_sql (store, cvalue);
		str = g_strdup_printf ("%s[]", str2);
		g_free (str2);
	}
	else
		str = g_strdup_printf ("%s[]", g_value_get_string (gda_data_model_get_value_at (model, 0, 0)));
	g_object_unref (model);
	return str;
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

static gint
db_object_sort_func (GdaMetaDbObject *dbo1, GdaMetaDbObject *dbo2)
{
	gint retval;
	retval = strcmp (dbo1->obj_schema, dbo2->obj_schema);
	if (retval)
		return retval;
	return strcmp (dbo1->obj_name, dbo2->obj_name);
}

/**
 * gda_meta_struct_sort_db_objects
 * @mstruct: a #GdaMetaStruct object
 * @sort_type: the kind of sorting requested
 * @error: a place to store errors, or %NULL
 *
 * Reorders the list of database objects within @mstruct in a way specified by @sort_type.
 *
 * Returns: TRUE if no error occurred
 */ 
gboolean
gda_meta_struct_sort_db_objects (GdaMetaStruct *mstruct, GdaMetaSortType sort_type, GError **error)
{
	GSList *pass_list;
	GSList *ordered_list = NULL;

	g_return_val_if_fail (GDA_IS_META_STRUCT (mstruct), FALSE);

	switch (sort_type) {
	case GDA_META_SORT_ALHAPETICAL:
		mstruct->db_objects = g_slist_sort (mstruct->db_objects, (GCompareFunc) db_object_sort_func);
		ordered_list = mstruct->db_objects;
		break;
	case GDA_META_SORT_DEPENDENCIES:
		g_return_val_if_fail (mstruct, FALSE);
		for (pass_list = build_pass (mstruct->db_objects, ordered_list); 
		     pass_list; 
		     pass_list = build_pass (mstruct->db_objects, ordered_list)) 
			ordered_list = g_slist_concat (ordered_list, pass_list);
		g_slist_free (mstruct->db_objects);
		mstruct->db_objects = ordered_list;
		break;
	default:
		TO_IMPLEMENT;
		break;
	}

#ifdef GDA_DEBUG_NO
	GSList *list;
	for (list = ordered_list; list; list = list->next) 
		g_print ("--> %s\n", GDA_META_DB_OBJECT (list->data)->obj_name);
#endif
	return TRUE;
}

/**
 * gda_meta_struct_get_db_object
 * @mstruct: a #GdaMetaStruct object
 * @catalog: the catalog the object belongs to (as a G_TYPE_STRING GValue), or %NULL
 * @schema: the schema the object belongs to (as a G_TYPE_STRING GValue), or %NULL
 * @name: the object's name (as a G_TYPE_STRING GValue), not %NULL
 *
 * Tries to locate the #GdaMetaDbObject structure representing the database object named after
 * @catalog, @schema and @name.
 *
 * If one or both of @catalog and @schema are %NULL, and more than one database object matches the name, then
 * the return value is also %NULL.
 *
 * Returns: the #GdaMetaDbObject or %NULL if not found
 */
GdaMetaDbObject *
gda_meta_struct_get_db_object (GdaMetaStruct *mstruct, const GValue *catalog, const GValue *schema, const GValue *name)
{
	gchar *key;
	GdaMetaDbObject *dbo;

	g_return_val_if_fail (GDA_IS_META_STRUCT (mstruct), NULL);
	g_return_val_if_fail (name && (G_VALUE_TYPE (name) == G_TYPE_STRING), NULL);

	if (catalog && schema) {
		g_return_val_if_fail (G_VALUE_TYPE (catalog) == G_TYPE_STRING, NULL);
		g_return_val_if_fail (G_VALUE_TYPE (schema) == G_TYPE_STRING, NULL);

		key = g_strdup_printf ("%s.%s.%s", g_value_get_string (catalog), g_value_get_string (schema), 
				       g_value_get_string (name));
		dbo = g_hash_table_lookup (mstruct->priv->index, key);
		g_free (key);
		return dbo;
	}
	else {
		/* walk through all the objects, and pick the ones with a matching name */
		GSList *list;
		GSList *matching = NULL;
		const gchar *obj_name = g_value_get_string (name);
		const gchar *obj_schema = NULL, *obj_catalog = NULL;
		if (catalog) {
			g_return_val_if_fail (G_VALUE_TYPE (catalog) == G_TYPE_STRING, NULL);
			obj_catalog = g_value_get_string (catalog);
		}
		if (schema) {
			g_return_val_if_fail (G_VALUE_TYPE (schema) == G_TYPE_STRING, NULL);
			obj_schema = g_value_get_string (schema);
		}

		for (list = mstruct->db_objects; list; list = list->next) {
			GdaMetaDbObject *dbo;
			dbo = GDA_META_DB_OBJECT (list->data);
			if (gda_identifier_equal (dbo->obj_name, obj_name) &&
			    (!obj_schema || gda_identifier_equal (dbo->obj_schema, obj_schema)) &&
			    (!obj_catalog || gda_identifier_equal (dbo->obj_catalog, obj_catalog)))
				matching = g_slist_prepend (matching, dbo);
		}

		if (matching && !matching->next) {
			GdaMetaDbObject *dbo = GDA_META_DB_OBJECT (matching->data);
			g_slist_free (matching);
			return dbo;
		}
		else {
			/* none or more than one found => return NULL */
			if (matching)
				g_slist_free (matching);
			return NULL;
		}
	}
}

/**
 * gda_meta_struct_get_table_column
 * @mstruct: a #GdaMetaStruct object
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

	g_return_val_if_fail (GDA_IS_META_STRUCT (mstruct), NULL);
	g_return_val_if_fail (table, NULL);
	g_return_val_if_fail (col_name && (G_VALUE_TYPE (col_name) == G_TYPE_STRING), NULL);
	cname = g_value_get_string (col_name);

	for (list = table->columns; list; list = list->next) {
		GdaMetaTableColumn *tcol = GDA_META_TABLE_COLUMN (list->data);
		if (gda_identifier_equal (tcol->column_name, cname))
			return tcol;
	}
	return NULL;
}

/**
 * gda_meta_struct_dump_as_graph
 * @mstruct: a #GdaMetaStruct object
 * @info: informs what kind of information to show in the resulting graph
 * @error: a place to store errors, or %NULL
 *
 * Creates a new graph (in the GraphViz syntax) representation of @mstruct.
 *
 * Returns: a new string, or %NULL if an error occurred.
 */
gchar *
gda_meta_struct_dump_as_graph (GdaMetaStruct *mstruct, GdaMetaGraphInfo info, GError **error)
{
	GString *string;
	gchar *result;

	g_return_val_if_fail (GDA_IS_META_STRUCT (mstruct), NULL);
	
	string = g_string_new ("digraph G {\nrankdir = BT;\nnode [shape = plaintext];\n");
	GSList *dbo_list;
	for (dbo_list = mstruct->db_objects; dbo_list; dbo_list = dbo_list->next) {
		gchar *objname, *fullname;
		GdaMetaDbObject *dbo = GDA_META_DB_OBJECT (dbo_list->data);
		GSList *list;
		gboolean use_html = (info & GDA_META_GRAPH_COLUMNS) ? TRUE : FALSE;

		/* obj human readable name, and full name */
		fullname = g_strdup_printf ("%s.%s.%s", dbo->obj_catalog, dbo->obj_schema, dbo->obj_name);
		if (dbo->obj_short_name) 
			objname = g_strdup (dbo->obj_short_name);
		else
			objname = g_strdup_printf ("%s.%s", dbo->obj_schema, dbo->obj_name);

		/* node */
		switch (dbo->obj_type) {
		case GDA_META_DB_UNKNOWN:
			break;
		case GDA_META_DB_TABLE:
			if (use_html) {
				g_string_append_printf (string, "\"%s\" [label=<<TABLE BORDER=\"1\" CELLBORDER=\"0\" CELLSPACING=\"0\">", fullname);
				g_string_append_printf (string, "<TR><TD COLSPAN=\"2\" BGCOLOR=\"grey\" BORDER=\"1\">%s</TD></TR>", objname);
			}
			else
				g_string_append_printf (string, "\"%s\" [ shape = box label = \"%s\" ]", fullname, objname);
			break;
		case GDA_META_DB_VIEW:
			if (use_html) {
				g_string_append_printf (string, "\"%s\" [label=<<TABLE BORDER=\"1\" CELLBORDER=\"0\" CELLSPACING=\"0\">", fullname);
				g_string_append_printf (string, "<TR><TD BGCOLOR=\"yellow\" BORDER=\"1\">%s</TD></TR>", objname);
			}
			else
				g_string_append_printf (string, "\"%s\" [ shape = ellipse, label = \"%s\" ]", fullname, objname);
			break;
		default:
			TO_IMPLEMENT;
			g_string_append_printf (string, "\"%s\" [ shape = note label = \"%s\" ]", fullname, objname);
			break;
		}

		/* columns, only for tables */
		if (dbo->obj_type == GDA_META_DB_TABLE) {
			GdaMetaTable *mt = GDA_META_DB_OBJECT_GET_TABLE (dbo);
			GSList *depend_dbo_list = NULL;
			if (info & GDA_META_GRAPH_COLUMNS) {
				for (list = mt->columns; list; list = list->next) {
					GdaMetaTableColumn *tcol = GDA_META_TABLE_COLUMN (list->data);
					GString *extra = g_string_new ("");
					if (tcol->pkey)
						g_string_append_printf (extra, "key");
					g_string_append_printf (string, "<TR><TD ALIGN=\"left\">%s</TD><TD ALIGN=\"right\">%s</TD></TR>", 
								tcol->column_name, extra->str);
					g_string_free (extra, TRUE);
				}
			}
			if (use_html)
				g_string_append (string, "</TABLE>>];\n");
			/* foreign keys */
			for (list = mt->fk_list; list; list = list->next) {
				GdaMetaTableForeignKey *tfk = GDA_META_TABLE_FOREIGN_KEY (list->data);
				if (tfk->depend_on->obj_type != GDA_META_DB_UNKNOWN) {
					g_string_append_printf (string, "\"%s\" -> \"%s.%s.%s\";\n", fullname,
								tfk->depend_on->obj_catalog, tfk->depend_on->obj_schema, 
								tfk->depend_on->obj_name);
					depend_dbo_list = g_slist_prepend (depend_dbo_list, tfk->depend_on);
				}
			}

			/* dependencies other than foreign keys */
			for (list = dbo->depend_list; list; list = list->next) {
				if (!g_slist_find (depend_dbo_list, list->data)) {
					GdaMetaDbObject *dep_dbo = GDA_META_DB_OBJECT (list->data);
					if (dep_dbo->obj_type != GDA_META_DB_UNKNOWN) 
						g_string_append_printf (string, "\"%s\" -> \"%s.%s.%s\";\n", 
									fullname,
									dep_dbo->obj_catalog, dep_dbo->obj_schema, 
									dep_dbo->obj_name);
				}
			}

			g_slist_free (depend_dbo_list);
		}
		else if (dbo->obj_type == GDA_META_DB_VIEW) {
			GdaMetaTable *mt = GDA_META_DB_OBJECT_GET_TABLE (dbo);
			if (info & GDA_META_GRAPH_COLUMNS) {
				for (list = mt->columns; list; list = list->next) {
					GdaMetaTableColumn *tcol = GDA_META_TABLE_COLUMN (list->data);
					g_string_append_printf (string, "<TR><TD ALIGN=\"left\">%s</TD></TR>", tcol->column_name);
				}
			}
			if (use_html)
				g_string_append (string, "</TABLE>>];\n");
			/* dependencies */
			for (list = dbo->depend_list; list; list = list->next) {
				GdaMetaDbObject *ddbo = GDA_META_DB_OBJECT (list->data);
				if (ddbo->obj_type != GDA_META_DB_UNKNOWN) 
					g_string_append_printf (string, "\"%s\" -> \"%s.%s.%s\";\n", fullname,
								ddbo->obj_catalog, ddbo->obj_schema, 
								ddbo->obj_name);
			}
		}

		g_free (objname);
		g_free (fullname);
	}
	g_string_append_c (string, '}');

	result = string->str;
	g_string_free (string, FALSE);
	return result;
}

static void
gda_meta_db_object_free (GdaMetaDbObject *dbo)
{
	g_free (dbo->obj_catalog);
	g_free (dbo->obj_schema);
	g_free (dbo->obj_name);
	g_free (dbo->obj_short_name);
	g_free (dbo->obj_full_name);
	g_free (dbo->obj_owner);
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

static gboolean 
determine_db_object_from_schema_and_name (GdaMetaStruct *mstruct, GdaMetaStore *store, 
					  GdaMetaDbObjectType *in_out_type, GValue **out_catalog,
					  GValue **out_short_name, 
					  GValue **out_full_name, GValue **out_owner, 
					  const GValue *schema, const GValue *name)
{
	*out_catalog = NULL;
	*out_short_name = NULL;
	*out_full_name = NULL;
	*out_owner = NULL;

	switch (*in_out_type) {
	case GDA_META_DB_UNKNOWN: {
		GdaMetaDbObjectType type = GDA_META_DB_TABLE;
		if (determine_db_object_from_schema_and_name (mstruct, store, &type, out_catalog,
							      out_short_name, out_full_name, out_owner,
							      schema, name)) {
			*in_out_type = GDA_META_DB_TABLE;
			return TRUE;
		}
		type = GDA_META_DB_VIEW;
		if (determine_db_object_from_schema_and_name (mstruct, store, &type, out_catalog,
							      out_short_name, out_full_name, out_owner,
							      schema, name)) {
			*in_out_type = GDA_META_DB_VIEW;
			return TRUE;
		}
		return FALSE;
		break;
	}

	case GDA_META_DB_TABLE: {
		const gchar *sql = "SELECT table_catalog, table_short_name, table_full_name, table_owner FROM _tables as t WHERE table_schema = ##ts::string AND table_short_name = ##tname::string AND table_name NOT IN (SELECT v.table_name FROM _views as v WHERE v.table_catalog=t.table_catalog AND v.table_schema=t.table_schema)";
		GdaDataModel *model;
		gint nrows;
		model = gda_meta_store_extract (store, sql, NULL, "ts", schema, "tname", name, NULL);
		if (!model) 
			return FALSE;

		nrows = gda_data_model_get_n_rows (model);
		if (nrows != 1) {
			g_object_unref (model);
			return FALSE;
		}
		*out_catalog = gda_value_copy (gda_data_model_get_value_at (model, 0, 0));
		*out_short_name = gda_value_copy (gda_data_model_get_value_at (model, 1, 0));
		*out_full_name = gda_value_copy (gda_data_model_get_value_at (model, 2, 0));
		*out_owner = gda_value_copy (gda_data_model_get_value_at (model, 3, 0));
		g_object_unref (model);
		return TRUE;
	}

	case GDA_META_DB_VIEW:{
		const gchar *sql = "SELECT table_catalog, table_short_name, table_full_name, table_owner FROM _tables NATURAL JOIN _views WHERE table_schema = ##ts::string AND table_short_name = ##tname::string";
		GdaDataModel *model;
		gint nrows;
		model = gda_meta_store_extract (store, sql, NULL, "ts", schema, "tname", name, NULL);
		if (!model) 
			return FALSE;

		nrows = gda_data_model_get_n_rows (model);
		if (nrows != 1) {
			g_object_unref (model);
			return FALSE;
		}
		*out_catalog = gda_value_copy (gda_data_model_get_value_at (model, 0, 0));
		*out_short_name = gda_value_copy (gda_data_model_get_value_at (model, 1, 0));
		*out_full_name = gda_value_copy (gda_data_model_get_value_at (model, 2, 0));
		*out_owner = gda_value_copy (gda_data_model_get_value_at (model, 3, 0));
		g_object_unref (model);
		return TRUE;
	}
	default:
		TO_IMPLEMENT;
	}

	return FALSE;
}

static gboolean
determine_db_object_from_short_name (GdaMetaStruct *mstruct, GdaMetaStore *store, 
				     GdaMetaDbObjectType *in_out_type, GValue **out_catalog,
				     GValue **out_schema, GValue **out_name, GValue **out_short_name, 
				     GValue **out_full_name, GValue **out_owner, const GValue *name)
{
	*out_name = NULL;
	*out_schema = NULL;
	*out_catalog = NULL;
	*out_short_name = NULL;
	*out_full_name = NULL;
	*out_owner = NULL;

	/* general lookup */
	switch (*in_out_type) {
	case GDA_META_DB_UNKNOWN: {
		GdaMetaDbObjectType type = GDA_META_DB_TABLE;
		if (determine_db_object_from_short_name (mstruct, store, &type, out_catalog, out_schema, out_name,
							 out_short_name, out_full_name, out_owner, name)) {
			*in_out_type = GDA_META_DB_TABLE;
			return TRUE;
		}
		type = GDA_META_DB_VIEW;
		if (determine_db_object_from_short_name (mstruct, store, &type, out_catalog, out_schema, out_name,
							 out_short_name, out_full_name, out_owner, name)) {
			*in_out_type = GDA_META_DB_VIEW;
			return TRUE;
		}
		return FALSE;
		break;
	}

	case GDA_META_DB_TABLE: {
		const gchar *sql = "SELECT table_catalog, table_schema, table_name, table_short_name, table_full_name, table_owner FROM _tables as t WHERE table_short_name = ##tname::string AND table_name NOT IN (SELECT v.table_name FROM _views as v WHERE v.table_catalog=t.table_catalog AND v.table_schema=t.table_schema)";
		GdaDataModel *model;
		gint nrows;
		model = gda_meta_store_extract (store, sql, NULL, "tname", name, NULL);
		if (!model) 
			return FALSE;

		nrows = gda_data_model_get_n_rows (model);
		if (nrows != 1) {
			g_object_unref (model);
			goto next;
		}
		*out_catalog = gda_value_copy (gda_data_model_get_value_at (model, 0, 0));
		*out_schema = gda_value_copy (gda_data_model_get_value_at (model, 1, 0));
		*out_name = gda_value_copy (gda_data_model_get_value_at (model, 2, 0));
		*out_short_name = gda_value_copy (gda_data_model_get_value_at (model, 3, 0));
		*out_full_name = gda_value_copy (gda_data_model_get_value_at (model, 4, 0));
		*out_owner = gda_value_copy (gda_data_model_get_value_at (model, 5, 0));
		g_object_unref (model);
		return TRUE;
	}

	case GDA_META_DB_VIEW:{
		const gchar *sql = "SELECT table_catalog, table_schema, table_name, table_short_name, table_full_name, table_owner FROM _tables NATURAL JOIN _views WHERE table_short_name = ##tname::string";
		GdaDataModel *model;
		gint nrows;
		model = gda_meta_store_extract (store, sql, NULL, "tname", name, NULL);
		if (!model) 
			return FALSE;

		nrows = gda_data_model_get_n_rows (model);
		if (nrows != 1) {
			g_object_unref (model);
			goto next;
		}
		*out_catalog = gda_value_copy (gda_data_model_get_value_at (model, 0, 0));
		*out_schema = gda_value_copy (gda_data_model_get_value_at (model, 1, 0));
		*out_name = gda_value_copy (gda_data_model_get_value_at (model, 2, 0));
		*out_short_name = gda_value_copy (gda_data_model_get_value_at (model, 3, 0));
		*out_full_name = gda_value_copy (gda_data_model_get_value_at (model, 4, 0));
		*out_owner = gda_value_copy (gda_data_model_get_value_at (model, 5, 0));
		g_object_unref (model);
		return TRUE;
	}
	default:
		TO_IMPLEMENT;
	}

 next:
	{
		/* treat the case where name is in fact <schema>.<name> */
		const gchar *sname;
		gchar *ptr;
		sname = g_value_get_string (name);
		for (ptr = (gchar *) sname; *ptr && (*ptr != '.'); ptr++);
		if (*ptr == '.') {
			gchar *tmp = g_strdup (sname);
			GValue *sv, *nv;
			gboolean retval;
			ptr = tmp + (ptr - sname);
			*ptr = 0;
			g_value_set_string ((sv = gda_value_new (G_TYPE_STRING)), tmp);
			g_value_set_string ((nv = gda_value_new (G_TYPE_STRING)), ptr + 1);
			retval = determine_db_object_from_schema_and_name (mstruct, store, in_out_type, out_catalog, 
									   out_short_name, out_full_name, out_owner, 
									   sv, nv);
			if (retval) {
				*out_schema = sv;
				*out_name = nv;
			}
			else {
				gda_value_free (sv);
				gda_value_free (nv);
			}
			g_free (tmp);
			return retval;
		}
	}

	return FALSE;
}

static gboolean
determine_db_object_from_missing_type (GdaMetaStruct *mstruct, GdaMetaStore *store, 
				       GdaMetaDbObjectType *out_type, GValue **out_short_name, 
				       GValue **out_full_name, GValue **out_owner, const GValue *catalog, 
				       const GValue *schema, const GValue *name)
{
	/* try as a view first */
	const gchar *sql = "SELECT table_short_name, table_full_name, table_owner FROM _tables NATURAL JOIN _views WHERE table_catalog = ##tc::string AND table_schema = ##ts::string AND table_name = ##tname::string";
	GdaDataModel *model;

	model = gda_meta_store_extract (store, sql, NULL, "tc", catalog, "ts", schema, "tname", name, NULL);
	if (model && (gda_data_model_get_n_rows (model) == 1)) {
		*out_type = GDA_META_DB_VIEW;
		*out_short_name = gda_value_copy (gda_data_model_get_value_at (model, 0, 0));
		*out_full_name = gda_value_copy (gda_data_model_get_value_at (model, 1, 0));
		*out_owner = gda_value_copy (gda_data_model_get_value_at (model, 2, 0));
		g_object_unref (model);
		return TRUE;
	}
	if (model) g_object_unref (model);

	/* try as a table */
	sql = "SELECT table_short_name, table_full_name, table_owner FROM _tables WHERE table_catalog = ##tc::string AND table_schema = ##ts::string AND table_name = ##tname::string";
	model = gda_meta_store_extract (store, sql, NULL, "tc", catalog, "ts", schema, "tname", name, NULL);
	if (model && (gda_data_model_get_n_rows (model) == 1)) {
		*out_type = GDA_META_DB_TABLE;
		*out_short_name = gda_value_copy (gda_data_model_get_value_at (model, 0, 0));
		*out_full_name = gda_value_copy (gda_data_model_get_value_at (model, 1, 0));
		*out_owner = gda_value_copy (gda_data_model_get_value_at (model, 2, 0));
		g_object_unref (model);
		return TRUE;
	}
	if (model) g_object_unref (model);
	
	return FALSE;
}
