/*
 * Copyright (C) 2008 - 2012 Murray Cumming <murrayc@murrayc.com>
 * Copyright (C) 2008 - 2014 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2009 Bas Driessen <bas.driessen@xobas.com>
 * Copyright (C) 2010 David King <davidk@openismus.com>
 * Copyright (C) 2010 Jonh Wendell <jwendell@gnome.org>
 * Copyright (C) 2012, 2018 Daniel Espinosa <esodan@gmail.com>
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
#define G_LOG_DOMAIN "GDA-meta-struct"

#include <string.h>
#include <glib/gi18n-lib.h>
#include <libgda/gda-meta-struct.h>
#include <libgda/gda-util.h>
#include <sql-parser/gda-sql-parser.h>
#include <sql-parser/gda-sql-statement.h>
#include <sql-parser/gda-statement-struct-util.h>
#include <libgda/gda-debug-macros.h>

/*
 * Main static functions
 */
static void gda_meta_struct_dispose (GObject *object);

static void gda_meta_struct_set_property (GObject *object,
                                         guint param_id,
                                         const GValue *value,
                                         GParamSpec *pspec);
static void gda_meta_struct_get_property (GObject *object,
                                         guint param_id,
                                         GValue *value,
                                         GParamSpec *pspec);


static void meta_store_changed_cb (GdaMetaStore *store, GSList *changes, GdaMetaStruct *mstruct);
static void meta_store_reset_cb (GdaMetaStore *store, GdaMetaStruct *mstruct);

typedef struct {
	GdaMetaStore *store;
	GSList       *db_objects; /* list of GdaMetaDbObject structures */
	GHashTable   *index; /* key = [catalog].[schema].[name], value = a GdaMetaDbObject. Note: catalog, schema and name 
			      * are case sensitive (and don't have any double quote around them) */
	guint         features;
	gboolean      auto_incement;
	gchar        *desc;
	GdaSqlParser *parser;
} GdaMetaStructPrivate;
G_DEFINE_TYPE_WITH_PRIVATE (GdaMetaStruct, gda_meta_struct, G_TYPE_OBJECT)


static GdaMetaDbObject *_meta_struct_get_db_object (GdaMetaStruct *mstruct, 
						    const GValue *catalog, const GValue *schema, const GValue *name);

static void gda_meta_db_object_free (GdaMetaDbObject *dbo);
static void gda_meta_db_object_free_contents (GdaMetaDbObject *dbo);
static void gda_meta_table_free_contents (GdaMetaTable *table);
static void gda_meta_view_free_contents (GdaMetaView *view);
static void gda_meta_table_column_free (GdaMetaTableColumn *tcol);
static void gda_meta_table_foreign_key_free (GdaMetaTableForeignKey *tfk);

/* properties */
enum {
	PROP_0,
	PROP_STORE,
	PROP_FEATURES
};

/* module error */
GQuark gda_meta_struct_error_quark (void) {
	static GQuark quark;
	if (!quark)
		quark = g_quark_from_static_string ("gda_meta_struct_error");
	return quark;
}


static void
gda_meta_struct_class_init (GdaMetaStructClass *klass) {
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	/* Properties */
	object_class->set_property = gda_meta_struct_set_property;
	object_class->get_property = gda_meta_struct_get_property;
	g_object_class_install_property (object_class, PROP_STORE,
					 g_param_spec_object ("meta-store", NULL,
							      /* To translators: GdaMetaStore is an object holding meta data information
							       * which will be used as an information source */
							      _ ("GdaMetaStore object to fetch information from"),  
							      GDA_TYPE_META_STORE, 
							      (G_PARAM_WRITABLE | G_PARAM_READABLE | 
							       G_PARAM_CONSTRUCT_ONLY)));
        g_object_class_install_property (object_class, PROP_FEATURES,
					 /* To translators: The GdaMetaStruct object computes lists of tables, views,
					  * and some other information, the "Features to compute" allows one to avoid
					  * computing some features which won't be used */
					 g_param_spec_uint ("features", _ ("Features to compute"), NULL, 
							    GDA_META_STRUCT_FEATURE_NONE, G_MAXINT,
							    GDA_META_STRUCT_FEATURE_ALL,
							    (G_PARAM_WRITABLE | G_PARAM_READABLE | 
							     G_PARAM_CONSTRUCT_ONLY)));

	/* virtual methods */
	object_class->dispose = gda_meta_struct_dispose;

}


static void
gda_meta_struct_init (GdaMetaStruct *mstruct) {
	GdaMetaStructPrivate *priv = gda_meta_struct_get_instance_private (mstruct);
	priv->store = NULL;
	priv->db_objects = NULL;
	priv->index = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
	priv->auto_incement = FALSE;
	priv->desc = NULL;
	priv->parser = gda_sql_parser_new ();
}

static void
gda_meta_struct_dispose (GObject *object)
{
	GdaMetaStruct *mstruct;

	g_assert_nonnull (object);
	g_assert_true (GDA_IS_META_STRUCT (object));
	
	mstruct = GDA_META_STRUCT (object);
	GdaMetaStructPrivate *priv = gda_meta_struct_get_instance_private (mstruct);
	if (priv->store) {
		g_signal_handlers_disconnect_by_func (G_OBJECT (priv->store),
						      G_CALLBACK (meta_store_changed_cb), mstruct);
		g_signal_handlers_disconnect_by_func (G_OBJECT (priv->store),
						      G_CALLBACK (meta_store_reset_cb), mstruct);
		g_object_unref (priv->store);
		priv->store = NULL;
	}
	if (priv->db_objects) {
		g_slist_free_full (priv->db_objects, (GDestroyNotify) gda_meta_db_object_free);
		priv->db_objects = NULL;
	}
	if (priv->index) {
		g_hash_table_destroy (priv->index);
		priv->index = NULL;
	}

	if (priv->parser)
		g_clear_object (&priv->parser);

	/* parent class */
	G_OBJECT_CLASS (gda_meta_struct_parent_class)->dispose (object);
}

static void
gda_meta_struct_set_property (GObject *object,
			      guint param_id,
			      const GValue *value,
			      GParamSpec *pspec)
{
	GdaMetaStruct *mstruct;

	mstruct = GDA_META_STRUCT (object);
	GdaMetaStructPrivate *priv = gda_meta_struct_get_instance_private (mstruct);
	switch (param_id) {
		case PROP_STORE:
			priv->store = g_value_get_object (value);
			if (priv->store) {
				g_object_ref (priv->store);
				g_signal_connect (G_OBJECT (priv->store), "meta-changed",
						  G_CALLBACK (meta_store_changed_cb), mstruct);
				g_signal_connect (G_OBJECT (priv->store), "meta-reset",
						  G_CALLBACK (meta_store_reset_cb), mstruct);
			}
			break;
		case PROP_FEATURES:
			priv->features = g_value_get_uint (value);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
			break;
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
	GdaMetaStructPrivate *priv = gda_meta_struct_get_instance_private (mstruct);

	switch (param_id) {
		case PROP_STORE:
			g_value_set_object (value, priv->store);
			break;
		case PROP_FEATURES:
			g_value_set_uint (value, priv->features);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
			break;
	}
}

static void
meta_store_changed_cb (GdaMetaStore *store, G_GNUC_UNUSED GSList *changes, GdaMetaStruct *mstruct)
{
	/* for now we mark ALL the db objects as outdated */
	meta_store_reset_cb (store, mstruct);
}

static void
meta_store_reset_cb (G_GNUC_UNUSED GdaMetaStore *store, GdaMetaStruct *mstruct)
{
	GdaMetaStructPrivate *priv = gda_meta_struct_get_instance_private (mstruct);
	/* mark ALL the db objects as outdated */
	GSList *list;
	for (list = priv->db_objects; list; list = list->next)
		GDA_META_DB_OBJECT (list->data)->outdated = TRUE;
}


static void
compute_view_dependencies (GdaMetaStruct *mstruct, GdaMetaDbObject *view_dbobj, GdaSqlStatement *sqlst) 
{	
	GdaMetaStructPrivate *priv = gda_meta_struct_get_instance_private (mstruct);
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
			
			m2 = (GdaMetaStruct*) g_object_new (GDA_TYPE_META_STRUCT, "meta-store", priv->store, "features", GDA_META_STRUCT_FEATURE_NONE, NULL);
			g_value_set_string ((vname = gda_value_new (G_TYPE_STRING)), t->table_name);
			if (! (tmp_obj = gda_meta_struct_complement (m2, 
								     GDA_META_DB_TABLE, NULL, NULL, vname, NULL)))
				tmp_obj = gda_meta_struct_complement (m2,
								      GDA_META_DB_VIEW, NULL, NULL, vname, NULL);
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
			ref_obj = _meta_struct_get_db_object (mstruct, catalog, schema, name);
			if (!ref_obj) {
				gchar *str;
				ref_obj = g_new0 (GdaMetaDbObject, 1);
				ref_obj->obj_type = GDA_META_DB_UNKNOWN;
				ref_obj->obj_catalog = g_strdup (tmp_obj->obj_catalog);
				ref_obj->obj_schema = g_strdup (tmp_obj->obj_schema);
				ref_obj->obj_name = g_strdup (tmp_obj->obj_name);

				priv->db_objects = g_slist_append (priv->db_objects, ref_obj);
				str = g_strdup_printf ("%s.%s.%s", tmp_obj->obj_catalog,
						       tmp_obj->obj_schema, tmp_obj->obj_name);
				g_hash_table_insert (priv->index, str, ref_obj);
			}
			g_object_unref (m2);
			g_assert (ref_obj);
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
			compute_view_dependencies (mstruct, view_dbobj, (GdaSqlStatement*) list->data);
	}
	else
		g_assert_not_reached ();
}

static GdaMetaDbObject * _meta_struct_complement (GdaMetaStruct *mstruct, GdaMetaDbObjectType type,
						  const GValue *icatalog, const GValue *ischema, const GValue *iname, 
						  const GValue *short_name, const GValue *full_name, 
						  const GValue *owner, GError **error);
static gboolean determine_db_object_from_schema_and_name (GdaMetaStruct *mstruct,
							  GdaMetaDbObjectType *int_out_type, GValue **out_catalog,
							  GValue **out_short_name, GValue **out_full_name,
							  GValue **out_owner, const GValue *schema, const GValue *name);
static gboolean determine_db_object_from_short_name (GdaMetaStruct *mstruct,
						     GdaMetaDbObjectType *int_out_type, GValue **out_catalog,
						     GValue **out_schema, GValue **out_name, GValue **out_short_name, 
						     GValue **out_full_name, GValue **out_owner, const GValue *name);
static gboolean determine_db_object_from_missing_type (GdaMetaStruct *mstruct,
						       GdaMetaDbObjectType *out_type, GValue **out_short_name, 
						       GValue **out_full_name, GValue **out_owner, const GValue *catalog, 
						       const GValue *schema, const GValue *name);
static gchar *array_type_to_sql (GdaMetaStore *store, const GValue *specific_name);
static gchar *
get_user_obj_name (const GValue *catalog, const GValue *schema, const GValue *name)
{
	GString *string = NULL;
	gchar *ret;
	if (catalog && (G_VALUE_TYPE (catalog) != GDA_TYPE_NULL))
		string = g_string_new (g_value_get_string (catalog));
	if (schema && (G_VALUE_TYPE (schema) != GDA_TYPE_NULL)) {
		if (string) {
			g_string_append_c (string, '.');
			g_string_append (string, g_value_get_string (schema));
		}
		else
			string = g_string_new (g_value_get_string (schema));
	}
	if (name && (G_VALUE_TYPE (name) != GDA_TYPE_NULL)) {
		if (string) {
			g_string_append_c (string, '.');
			g_string_append (string, g_value_get_string (name));
		}
		else
			string = g_string_new (g_value_get_string (name));
	}
	ret = string->str;
	g_string_free (string, FALSE);
	return ret;
}

static gchar *
prepare_sql_identifier_for_compare (gchar *str)
{
	if (!str || (*str == '"'))
		return str;
	else {
		gchar *ptr;
		for (ptr = str; *ptr; ptr++)
			*ptr = g_ascii_tolower (*ptr);
		return str;
	}
}

/**
 * gda_meta_struct_complement:
 * @mstruct: a #GdaMetaStruct object
 * @type: the type of object to add (which can be GDA_META_DB_UNKNOWN)
 * @catalog: (nullable): the catalog the object belongs to (as a G_TYPE_STRING GValue), or %NULL
 * @schema: (nullable): the schema the object belongs to (as a G_TYPE_STRING GValue), or %NULL
 * @name: the object's name (as a G_TYPE_STRING GValue), not %NULL
 * @error: (nullable): a place to store errors, or %NULL
 *
 * Creates a new #GdaMetaDbObject structure in @mstruct to represent the database object (of type @type)
 * which can be uniquely identified as @catalog.@schema.@name.
 *
 * If @catalog is not %NULL, then @schema should not be %NULL.
 *
 * If both @catalog and @schema are %NULL, then the database object will be the one which is
 * "visible" by default (that is which can be accessed only by its short @name name).
 *
 * If @catalog is %NULL and @schema is not %NULL, then the database object will be the one which 
 * can be accessed by its @schema.@name name.
 *
 * Important note: @catalog, @schema and @name will be used using the following convention:
 * <itemizedlist>
 *   <listitem><para>be surrounded by double quotes for a case sensitive search</para></listitem>
 *   <listitem><para>otherwise for case insensitive search</para></listitem>
 * </itemizedlist>
 *
 * For more information, see the <link linkend="information_schema:sql_identifiers">
 * meta data section about SQL identifiers</link>.
 *
 * Returns: (transfer none) (nullable): the #GdaMetaDbObject corresponding to the database object if no error occurred, or %NULL
 */
GdaMetaDbObject *
gda_meta_struct_complement (GdaMetaStruct *mstruct, GdaMetaDbObjectType type,
			    const GValue *catalog, const GValue *schema, const GValue *name, 
			    GError **error)
{
	GdaMetaDbObject *dbo = NULL;
	GdaMetaDbObjectType real_type = type;
	GValue *short_name = NULL, *full_name=NULL, *owner=NULL;
	GValue *icatalog = NULL, *ischema = NULL, *iname = NULL; /* GValue with identifiers ready to be compared */

	g_return_val_if_fail (GDA_IS_META_STRUCT (mstruct), NULL);

	GdaMetaStructPrivate *priv = gda_meta_struct_get_instance_private (mstruct);

	/* checks */
	g_return_val_if_fail (priv->store, NULL);
	g_return_val_if_fail (name && (G_VALUE_TYPE (name) == G_TYPE_STRING), NULL);

	if (catalog && (gda_value_is_null (catalog) || !g_value_get_string (catalog)))
		catalog = NULL;
	if (schema && (gda_value_is_null (schema) || !g_value_get_string (schema)))
		schema = NULL;
	g_return_val_if_fail (!catalog || (catalog && schema), NULL);
	g_return_val_if_fail (!catalog || (G_VALUE_TYPE (catalog) == G_TYPE_STRING), NULL);
	g_return_val_if_fail (!schema || (G_VALUE_TYPE (schema) == G_TYPE_STRING), NULL);

	/* create ready to compare strings for catalog, schema and name */
	gchar *schema_s, *name_s;
	if (_split_identifier_string (g_value_dup_string (name), &schema_s, &name_s)) {
		g_value_take_string ((iname = gda_value_new (G_TYPE_STRING)), prepare_sql_identifier_for_compare (name_s));
		if (schema_s)
			g_value_take_string ((ischema = gda_value_new (G_TYPE_STRING)), prepare_sql_identifier_for_compare (schema_s));
	}
	else
		g_value_take_string ((iname = gda_value_new (G_TYPE_STRING)), 
				     prepare_sql_identifier_for_compare (g_value_dup_string (name)));
	if (catalog)
		g_value_take_string ((icatalog = gda_value_new (G_TYPE_STRING)), 
				     prepare_sql_identifier_for_compare (g_value_dup_string (catalog)));
	if (schema && !ischema) 
		g_value_take_string ((ischema = gda_value_new (G_TYPE_STRING)), 
				     prepare_sql_identifier_for_compare (g_value_dup_string (schema)));

	if (!icatalog) {
		if (ischema) {
			g_return_val_if_fail (ischema && (G_VALUE_TYPE (ischema) == G_TYPE_STRING), NULL);
			if (! determine_db_object_from_schema_and_name (mstruct, &real_type, &icatalog, 
									&short_name, &full_name, &owner,
									ischema, iname)) {
				gchar *tmp;
				tmp = get_user_obj_name (catalog, schema, name);
				g_set_error (error, GDA_META_STRUCT_ERROR, GDA_META_STRUCT_UNKNOWN_OBJECT_ERROR,
					     _("Could not find object named '%s'"), tmp);
				g_free (tmp);
				gda_value_free (ischema);
				gda_value_free (iname);
				return NULL;
			}
		}
		else {
			GValue *real_name = NULL;
			if (! determine_db_object_from_short_name (mstruct, &real_type, &icatalog, 
								   &ischema, &real_name, 
								   &short_name, &full_name, &owner, iname)) {
				gchar *tmp;
				tmp = get_user_obj_name (catalog, schema, name);
				g_set_error (error, GDA_META_STRUCT_ERROR, GDA_META_STRUCT_UNKNOWN_OBJECT_ERROR,
					     _("Could not find object named '%s'"), tmp);
				g_free (tmp);
				gda_value_free (iname);
				return NULL;
			}
			if (real_name) {
				gda_value_free (iname);
				iname = real_name;
			}
		}
	}
	else if (type == GDA_META_DB_UNKNOWN) {
		if (! determine_db_object_from_missing_type (mstruct, &real_type, &short_name, &full_name, &owner,
							     icatalog, ischema, iname)) {
			gchar *tmp;
			tmp = get_user_obj_name (catalog, schema, name);
			g_set_error (error, GDA_META_STRUCT_ERROR, GDA_META_STRUCT_UNKNOWN_OBJECT_ERROR,
				     _("Could not find object named '%s'"), tmp);
			g_free (tmp);
			gda_value_free (icatalog);
			gda_value_free (ischema);
			gda_value_free (iname);
			return NULL;
		}
	}
	type = real_type;
	dbo = _meta_struct_complement (mstruct, type, icatalog, ischema, iname, short_name, full_name, owner, error);

	gda_value_free (icatalog);
	gda_value_free (ischema);
	gda_value_free (iname);

	if (short_name)	gda_value_free (short_name);
	if (full_name)	gda_value_free (full_name);
	if (owner) gda_value_free (owner);
	return dbo;
}

static GdaMetaForeignKeyPolicy
policy_string_to_value (const gchar *string)
{
	if (!string)
		return GDA_META_FOREIGN_KEY_UNKNOWN;
	if (*string == 'C')
		return GDA_META_FOREIGN_KEY_CASCADE;
	else if (*string == 'R')
		return GDA_META_FOREIGN_KEY_RESTRICT;
	else if (*string == 'N') {
		if (!strcmp (string + 1, "ONE"))
			return GDA_META_FOREIGN_KEY_NONE;
		else
			return GDA_META_FOREIGN_KEY_NO_ACTION;
	}
	else if (*string == 'S') {
		if (!strcmp (string + 1, "ET NULL"))
			return GDA_META_FOREIGN_KEY_SET_NULL;
		else
			return GDA_META_FOREIGN_KEY_SET_DEFAULT;
	} 
	return GDA_META_FOREIGN_KEY_UNKNOWN;
}

/*
 * Find if there is already a declared foreign for @dbo to @ref_dbo using the columns
 * listed in @columns
 *
 * @columns contains 4 columns, with no NULL value, and with nrows>0
 *  - @dbo's column name (string)
 *  - @dbo's column name ordinal position (int)
 *  - @ref_dbo's column name (string)
 *  - @ref_dbo's column name ordinal position (int)
 */
static GdaMetaTableForeignKey *
find_real_foreign_key (GdaMetaTable *table, GdaMetaDbObject *ref_dbo, GdaDataModel *columns)
{
	GSList *list;
	gint nrows = -1;
	for (list = table->fk_list; list; list = list->next) {
		GdaMetaTableForeignKey *fk = (GdaMetaTableForeignKey*) list->data;
		gint i;
		gboolean *mapping;
		if (fk->depend_on != ref_dbo)
			continue;

		if (nrows == -1)
			nrows = gda_data_model_get_n_rows (columns);

		if (nrows != fk->cols_nb)
			continue;

		mapping = g_new0 (gboolean, nrows);
		for (i = 0; i < nrows; i++) {
			const GValue *cvalues[4];
			const gchar *fk_colname, *ref_colname;
			gint fk_pos, ref_pos;
			gint j;
			for (j = 0; j < 4; j++) {
				cvalues[j] = gda_data_model_get_value_at (columns, j, i, NULL);
				if (!cvalues[j]) {
					g_free (mapping);
					goto out;
				}
			}
			fk_colname = g_value_get_string (cvalues[0]);
			fk_pos = g_value_get_int (cvalues[1]);
			ref_colname = g_value_get_string (cvalues[2]);
			ref_pos = g_value_get_int (cvalues[3]);
			if (!fk_colname || !*fk_colname ||
			    !ref_colname || !*ref_colname) {
				g_free (mapping);
				goto out;
			}

			for (j = 0; j < nrows; j++) {
				if (mapping [j]) /* row already mapped? */
					continue;
				if ((fk_pos == fk->fk_cols_array[j]) &&
				    (ref_pos == fk->ref_pk_cols_array[j]) &&
				    !strcmp (fk_colname, fk->fk_names_array[j]) &&
				    !strcmp (ref_colname, fk->ref_pk_names_array[j])) {
					mapping [j] = TRUE;
					break;
				}
			}
			if (j == nrows)
				/* i'th row has not been mapped */
				break;
		}
		g_free (mapping);
		if (i == nrows)
			/* all the rows have been mapped */
			return fk;
		else
			continue;
	}
 out:
	return NULL;
}

static gboolean
add_declared_foreign_keys (GdaMetaStruct *mstruct, GdaMetaTable *mt, GError **error)
{
	const gchar *sql1 = "SELECT DISTINCT constraint_name, ref_table_catalog, ref_table_schema, ref_table_name FROM __declared_fk WHERE table_catalog = ##tc::string AND table_schema = ##ts::string AND table_name = ##tname::string ORDER BY constraint_name, ref_table_catalog, ref_table_schema, ref_table_name";
	const gchar *sql2 = "select de.column_name, c.ordinal_position, de.ref_column_name, rc.ordinal_position FROM __declared_fk de LEFT JOIN _columns c ON (de.table_catalog=c.table_catalog AND de.table_schema=c.table_schema AND de.table_name=c.table_name AND de.column_name=c.column_name) LEFT JOIN _columns rc ON (de.ref_table_catalog=rc.table_catalog AND de.ref_table_schema=rc.table_schema AND de.ref_table_name=rc.table_name AND de.ref_column_name=rc.column_name) WHERE de.constraint_name=##cname::string AND de.table_catalog=##tc::string AND de.table_schema=##ts::string AND de.table_name=##tname::string";
	GdaDataModel *model, *cmodel = NULL;
	gint i, nrows;
	GValue *v1, *v2, *v3;
	GdaMetaDbObject *dbo;
	GdaMetaStructPrivate *priv = gda_meta_struct_get_instance_private (mstruct);

	dbo = (GdaMetaDbObject*) mt;
	g_value_set_string ((v1 = gda_value_new (G_TYPE_STRING)), dbo->obj_catalog);
	g_value_set_string ((v2 = gda_value_new (G_TYPE_STRING)), dbo->obj_schema);
	g_value_set_string ((v3 = gda_value_new (G_TYPE_STRING)), dbo->obj_name);
	model = gda_meta_store_extract (priv->store, sql1,
					error, "tc", v1, "ts", v2, "tname", v3, NULL);
	if (!model) 
		goto onerror;

	nrows = gda_data_model_get_n_rows (model);
	for (i = 0; i < nrows; i++) {
		GdaMetaDbObject *ref_dbo = NULL;
		GdaMetaTableForeignKey *tfk = NULL;
		const GValue *fk_catalog, *fk_schema, *fk_tname, *fk_name;
		gboolean ignore = FALSE;

		fk_name = gda_data_model_get_value_at (model, 0, i, error);
		if (!fk_name) goto onerror;		
		fk_catalog = gda_data_model_get_value_at (model, 1, i, error);
		if (!fk_catalog) goto onerror;
		fk_schema = gda_data_model_get_value_at (model, 2, i, error);
		if (!fk_schema) goto onerror;
		fk_tname = gda_data_model_get_value_at (model, 3, i, error);
		if (!fk_tname) goto onerror;

		/* get columns */
		cmodel = gda_meta_store_extract (priv->store, sql2,
						 error, "tc", v1, "ts", v2, "tname", v3,
						 "cname", fk_name, NULL);
		if (!cmodel) 
			goto onerror;

		/* create new FK structure */
		tfk = g_new0 (GdaMetaTableForeignKey, 1);
		tfk->meta_table = dbo;
		tfk->declared = TRUE;
		tfk->fk_name = g_value_dup_string (fk_name);
		tfk->on_update_policy = GDA_META_FOREIGN_KEY_NONE;
		tfk->on_delete_policy = GDA_META_FOREIGN_KEY_NONE;
		
		/* ignore if some columns are not found in the schema
		 * (maybe wrong or outdated FK decl) */
		gint j, cnrows;
		cnrows = gda_data_model_get_n_rows (cmodel);
		if (cnrows == 0)
			ignore = TRUE;
		else {
			tfk->cols_nb = cnrows;
			tfk->fk_cols_array = g_new0 (gint, cnrows);
			tfk->fk_names_array = g_new0 (gchar *, cnrows);
			tfk->ref_pk_cols_array = g_new0 (gint, cnrows);
			tfk->ref_pk_names_array = g_new0 (gchar *, cnrows);
		}
		for (j = 0; j < cnrows; j++) {
			const GValue *ov;
			ov = gda_data_model_get_value_at (cmodel, 0, j, error);
			if (!ov) goto onerror;
			if (G_VALUE_TYPE (ov) != G_TYPE_STRING) {
				ignore = TRUE;
				break;
			}
			tfk->fk_names_array [j] = g_value_dup_string (ov);

			ov = gda_data_model_get_value_at (cmodel, 1, j, error);
			if (!ov) goto onerror;
			if (G_VALUE_TYPE (ov) != G_TYPE_INT) {
				ignore = TRUE;
				break;
			}
			tfk->fk_cols_array [j] = g_value_get_int (ov);

			ov = gda_data_model_get_value_at (cmodel, 2, j, error);
			if (!ov) goto onerror;
			if (G_VALUE_TYPE (ov) != G_TYPE_STRING) {
				ignore = TRUE;
				break;
			}
			tfk->ref_pk_names_array [j] = g_value_dup_string (ov);

			ov = gda_data_model_get_value_at (cmodel, 3, j, error);
			if (!ov) goto onerror;
			if (G_VALUE_TYPE (ov) != G_TYPE_INT) {
				ignore = TRUE;
				break;
			}
			tfk->ref_pk_cols_array [j] = g_value_get_int (ov);
		}

		/* ignore if there is already an implemented FK */
		if (! ignore) {
			ref_dbo = _meta_struct_get_db_object (mstruct, fk_catalog,
							      fk_schema, fk_tname);
			if (ref_dbo && find_real_foreign_key (mt, ref_dbo, cmodel))
				ignore = TRUE;
		}
		if (ignore) {
			/* ignore this FK end move on to the next one */
			if (tfk)
				gda_meta_table_foreign_key_free (tfk);
			continue;
		}

		tfk->depend_on = ref_dbo;
		if (!tfk->depend_on) {
			gchar *str;
			tfk->depend_on = g_new0 (GdaMetaDbObject, 1);
			tfk->depend_on->obj_type = GDA_META_DB_UNKNOWN;
			tfk->depend_on->obj_catalog = g_strdup (g_value_get_string (fk_catalog));
			tfk->depend_on->obj_schema = g_strdup (g_value_get_string (fk_schema));
			tfk->depend_on->obj_name = g_strdup (g_value_get_string (fk_tname));
			priv->db_objects = g_slist_append (priv->db_objects, tfk->depend_on);
			str = g_strdup_printf ("%s.%s.%s", g_value_get_string (fk_catalog), 
					       g_value_get_string (fk_schema), 
					       g_value_get_string (fk_tname));
			g_hash_table_insert (priv->index, str, tfk->depend_on);
		}
		else if (tfk->depend_on->obj_type == GDA_META_DB_TABLE) {
			GdaMetaTable *dot = GDA_META_TABLE (tfk->depend_on);
			dot->reverse_fk_list = g_slist_prepend (dot->reverse_fk_list, tfk);
		}

		dbo->depend_list = g_slist_append (dbo->depend_list, tfk->depend_on);
		mt->fk_list = g_slist_prepend (mt->fk_list, tfk);
	}
	g_object_unref (model);
	if (cmodel)
		g_object_unref (cmodel);

	gda_value_free (v1);
	gda_value_free (v2);
	gda_value_free (v3);

	return TRUE;

 onerror:
	if (model)
		g_object_unref (model);
	if (cmodel)
		g_object_unref (cmodel);
	gda_value_free (v1);
	gda_value_free (v2);
	gda_value_free (v3);

	return FALSE;
}

static GdaMetaDbObject *
_meta_struct_complement (GdaMetaStruct *mstruct, GdaMetaDbObjectType type,
			 const GValue *icatalog, const GValue *ischema, const GValue *iname, 
			 const GValue *short_name, const GValue *full_name, const GValue *owner, GError **error)
{
	/* at this point icatalog, ischema and iname are NOT NULL */
	GdaMetaDbObject *dbo = NULL;
	const GValue *cvalue;
	GdaMetaStructPrivate *priv = gda_meta_struct_get_instance_private (mstruct);

	/* create new GdaMetaDbObject or get already existing one */
	dbo = _meta_struct_get_db_object (mstruct, icatalog, ischema, iname);
	if (!dbo) {
		dbo = g_new0 (GdaMetaDbObject, 1);
		dbo->obj_catalog = g_strdup (g_value_get_string (icatalog));
		dbo->obj_schema = g_strdup (g_value_get_string (ischema));
		dbo->obj_name = g_strdup (g_value_get_string (iname));
		if (short_name)
			dbo->obj_short_name = g_strdup (g_value_get_string (short_name));
		if (full_name)
			dbo->obj_full_name = g_strdup (g_value_get_string (full_name));
		if (owner && !gda_value_is_null (owner))
			dbo->obj_owner = g_strdup (g_value_get_string (owner));
	}
	else if (dbo->obj_type == type) 
		return dbo; /* nothing to do */
	else if (dbo->obj_type != GDA_META_DB_UNKNOWN) {
		/* DB Object already exists, return an error */
		g_set_error (error, GDA_META_STRUCT_ERROR, GDA_META_STRUCT_DUPLICATE_OBJECT_ERROR,
			     _("Object %s.%s.%s already exists in GdaMetaStruct and has a different object type"), 
			     g_value_get_string (icatalog), g_value_get_string (ischema), 
			     g_value_get_string (iname));
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

		model = gda_meta_store_extract (priv->store, sql,
						error, "tc", icatalog, "ts", ischema, "tname", iname, NULL);
		if (!model) 
			goto onerror;
		nrows = gda_data_model_get_n_rows (model);
		if (nrows < 1) {
			g_object_unref (model);
			g_set_error (error, GDA_META_STRUCT_ERROR, GDA_META_STRUCT_UNKNOWN_OBJECT_ERROR,
				     _("View %s.%s.%s not found in meta store object"), 
				     g_value_get_string (icatalog), g_value_get_string (ischema), 
				     g_value_get_string (iname));
			goto onerror;
		}
		
		if (!dbo->obj_short_name) {
			cvalue = gda_data_model_get_value_at (model, 2, 0, error);
			if (!cvalue) goto onerror;
			dbo->obj_short_name = g_value_dup_string (cvalue);
		}
		if (!dbo->obj_full_name) {
			cvalue = gda_data_model_get_value_at (model, 3, 0, error);
			if (!cvalue) goto onerror;
			dbo->obj_full_name = g_value_dup_string (cvalue);
		}
		if (!dbo->obj_owner) {
			cvalue = gda_data_model_get_value_at (model, 4, 0, error);
			if (!cvalue) goto onerror;
			if (!gda_value_is_null (cvalue))
				dbo->obj_owner = g_value_dup_string (cvalue);
		}

		mv = GDA_META_VIEW (dbo);
		cvalue = gda_data_model_get_value_at (model, 0, 0, error);
		if (!cvalue) goto onerror;
		if (G_VALUE_TYPE (cvalue) != GDA_TYPE_NULL)
			mv->view_def = g_value_dup_string (cvalue);
		else
			mv->view_def = g_strdup ("");

		cvalue = gda_data_model_get_value_at (model, 1, 0, error);
		if (!cvalue) goto onerror;
		if (G_VALUE_TYPE (cvalue) != GDA_TYPE_NULL)
			mv->is_updatable = g_value_get_boolean (cvalue);
		else
			mv->is_updatable = FALSE;

		/* view's dependencies, from its definition */
		if ((priv->features & GDA_META_STRUCT_FEATURE_VIEW_DEPENDENCIES) &&
		    mv->view_def && *mv->view_def) {
			GdaStatement *stmt;
			const gchar *remain;
			stmt = gda_sql_parser_parse_string (priv->parser, mv->view_def, &remain, NULL);
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
        	break;
	}
	case GDA_META_DB_TABLE: {
		/* columns */
		gchar *sql = "SELECT c.column_name, c.data_type, c.gtype, c.is_nullable, t.table_short_name, t.table_full_name, c.column_default, t.table_owner, c.array_spec, c.extra, c.column_comments, coalesce (c.character_maximum_length, c.character_octet_length) FROM _tables as t LEFT NATURAL JOIN _columns as c WHERE table_catalog = ##tc::string AND table_schema = ##ts::string AND table_name = ##tname::string ORDER BY ordinal_position";
		GdaMetaTable *mt;
		GdaDataModel *model;
		gint i, nrows;

		model = gda_meta_store_extract (priv->store, sql,
						error, "tc", icatalog, "ts", ischema, "tname", iname, NULL);
		if (!model) 
			goto onerror;

		nrows = gda_data_model_get_n_rows (model);
		if (nrows < 1) {
			g_object_unref (model);
			g_set_error (error, GDA_META_STRUCT_ERROR, GDA_META_STRUCT_UNKNOWN_OBJECT_ERROR,
				     _("Table %s.%s.%s not found (or missing columns information)"), 
				     g_value_get_string (icatalog), g_value_get_string (ischema), 
				     g_value_get_string (iname));
			goto onerror;
		}
		if (!dbo->obj_short_name) {
			cvalue = gda_data_model_get_value_at (model, 4, 0, error);
			if (!cvalue) goto onerror;
			dbo->obj_short_name = g_value_dup_string (cvalue);
		}
		if (!dbo->obj_full_name) {
			cvalue = gda_data_model_get_value_at (model, 5, 0, error);
			if (!cvalue) goto onerror;
			dbo->obj_full_name = g_value_dup_string (cvalue);
		}
		if (!dbo->obj_owner) {
			cvalue = gda_data_model_get_value_at (model, 7, 0, error);
			if (!cvalue) goto onerror;
			if (!gda_value_is_null (cvalue))
				dbo->obj_owner = g_value_dup_string (cvalue);
		}

		cvalue = gda_data_model_get_value_at (model, 0, 0, error);
		if (cvalue && (G_VALUE_TYPE (cvalue) == GDA_TYPE_NULL)) {
			if (type == GDA_META_DB_VIEW) {
				/* we don't have the list of columns for the view.
				 * This can sometimes happen in SQLite */
				nrows = 0;
			}
		}
		mt = GDA_META_TABLE (dbo);
		for (i = 0; i < nrows; i++) {
			GdaMetaTableColumn *tcol;
			const gchar *cstr = NULL;
			gint len = -1;
			tcol = g_new0 (GdaMetaTableColumn, 1); /* Note: tcol->pkey is not determined here */
			mt->columns = g_slist_prepend (mt->columns, tcol);

			cvalue = gda_data_model_get_value_at (model, 0, i, error);
			if (!cvalue) goto onerror;
			tcol->column_name = g_value_dup_string (cvalue);

			cvalue = gda_data_model_get_value_at (model, 1, i, error);
			if (!cvalue) goto onerror;
			if (!gda_value_is_null (cvalue))
				cstr = g_value_get_string (cvalue);

			cvalue = gda_data_model_get_value_at (model, 11, i, error);
			if (!cvalue) goto onerror;
			if (!gda_value_is_null (cvalue))
				len = g_value_get_int (cvalue);

			if (cstr && *cstr) {
				if (len >= 0)
					tcol->column_type = g_strdup_printf ("%s (%d)", cstr, len);
				else
					tcol->column_type = g_strdup (cstr);
			}
			else {
				cvalue = gda_data_model_get_value_at (model, 8, i, error);
				if (!cvalue) goto onerror;
				tcol->column_type = array_type_to_sql (priv->store, cvalue);
				if (!tcol->column_type)
					goto onerror;
			}
			
			cvalue = gda_data_model_get_value_at (model, 2, i, error);
			if (!cvalue) goto onerror;
			tcol->gtype = gda_g_type_from_string (g_value_get_string (cvalue));
			if (tcol->gtype == G_TYPE_INVALID)
				tcol->gtype = GDA_TYPE_NULL;

			cvalue = gda_data_model_get_value_at (model, 3, i, error);
			if (!cvalue) goto onerror;
			tcol->nullok = g_value_get_boolean (cvalue);

			cvalue = gda_data_model_get_value_at (model, 6, i, error);
			if (!cvalue) goto onerror;
			if (!gda_value_is_null (cvalue))
				tcol->default_value = g_value_dup_string (cvalue);

			cvalue = gda_data_model_get_value_at (model, 9, i, error);
			if (!cvalue) goto onerror;
			if (!gda_value_is_null (cvalue)) {
				gchar **array, *tmp;
				gint ai;

				cstr = g_value_get_string (cvalue);
				array = g_strsplit (cstr, ",", 0);
				for (ai = 0; array [ai]; ai++) {
					tmp = g_strstrip (array [ai]);
					if (!g_strcmp0 (tmp, GDA_EXTRA_AUTO_INCREMENT)) {
						tcol->auto_incement = TRUE;
					}
				}
				g_strfreev (array);
			}

			cvalue = gda_data_model_get_value_at (model, 10, i, error);
			if (!cvalue) goto onerror;
			if (!gda_value_is_null (cvalue)) {
				tcol->desc = g_value_dup_string (cvalue);
			}

		}
		mt->columns = g_slist_reverse (mt->columns);
		g_object_unref (model);

		/* primary key */
		sql = "SELECT constraint_name FROM _table_constraints WHERE constraint_type='PRIMARY KEY' AND table_catalog = ##tc::string AND table_schema = ##ts::string AND table_name = ##tname::string";
		model = gda_meta_store_extract (priv->store, sql,
						error, "tc", icatalog, "ts", ischema, "tname", iname, NULL);
		if (!model) 
			goto onerror;

		nrows = gda_data_model_get_n_rows (model);
		if (nrows >= 1) {
			GdaDataModel *pkmodel;
			sql = "SELECT column_name FROM _key_column_usage WHERE table_catalog = ##cc::string AND table_schema = ##cs::string AND table_name = ##tname::string AND constraint_name = ##cname::string ORDER BY ordinal_position";
			cvalue = gda_data_model_get_value_at (model, 0, 0, error);
			if (!cvalue) goto onerror;
			pkmodel = gda_meta_store_extract (priv->store, sql, error,
							  "cc", icatalog,
							  "cs", ischema,
							  "tname", iname,
							  "cname", cvalue, NULL);
			if (!pkmodel) {
				g_object_unref (model);
				goto onerror;
			}
			nrows = gda_data_model_get_n_rows (pkmodel);
			mt->pk_cols_nb = nrows;
			mt->pk_cols_array = g_new0 (gint, mt->pk_cols_nb);
			for (i = 0; i < nrows; i++) {
				GdaMetaTableColumn *tcol;
				cvalue = gda_data_model_get_value_at (pkmodel, 0, i, error);
				if (!cvalue) goto onerror;
				tcol = gda_meta_struct_get_table_column (mstruct, mt, cvalue);
				if (!tcol) {
					mt->pk_cols_array [i] = -1;
					g_set_error (error, GDA_META_STRUCT_ERROR, GDA_META_STRUCT_INCOHERENCE_ERROR,
						     _("Internal GdaMetaStore error: column %s not found"),
						     g_value_get_string (cvalue));
					goto onerror;
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
		if (priv->features & GDA_META_STRUCT_FEATURE_FOREIGN_KEYS) {
			sql = "SELECT ref_table_catalog, ref_table_schema, ref_table_name, constraint_name, ref_constraint_name, update_rule, delete_rule, constraint_name FROM _referential_constraints WHERE table_catalog = ##tc::string AND table_schema = ##ts::string AND table_name = ##tname::string";
			model = gda_meta_store_extract (priv->store, sql, error,
							"tc", icatalog, 
							"ts", ischema, 
							"tname", iname, NULL);
			if (!model) 
				goto onerror;
			
			nrows = gda_data_model_get_n_rows (model);
			for (i = 0; i < nrows; i++) {
				GdaMetaTableForeignKey *tfk = NULL;
				const GValue *fk_catalog, *fk_schema, *fk_tname, *fk_name;
				const GValue *upd_policy, *del_policy;

				GdaDataModel *fk_cols = NULL;
				GdaDataModel *ref_pk_cols = NULL;

				fk_catalog = gda_data_model_get_value_at (model, 0, i, error);
				if (!fk_catalog) goto onfkerror;
				fk_schema = gda_data_model_get_value_at (model, 1, i, error);
				if (!fk_schema) goto onfkerror;
				fk_tname = gda_data_model_get_value_at (model, 2, i, error);
				if (!fk_tname) goto onfkerror;
				upd_policy = gda_data_model_get_value_at (model, 5, i, error);
				if (!upd_policy) goto onfkerror;
				del_policy = gda_data_model_get_value_at (model, 6, i, error);
				if (!del_policy) goto onfkerror;
				fk_name = gda_data_model_get_value_at (model, 7, i, error);
				if (!fk_name) goto onfkerror;

				tfk = g_new0 (GdaMetaTableForeignKey, 1);
				tfk->meta_table = dbo;
				tfk->fk_name = g_value_dup_string (fk_name);
				tfk->depend_on = _meta_struct_get_db_object (mstruct, fk_catalog, fk_schema, fk_tname);
				if (!tfk->depend_on) {
					gchar *str;
					tfk->depend_on = g_new0 (GdaMetaDbObject, 1);
					tfk->depend_on->obj_type = GDA_META_DB_UNKNOWN;
					tfk->depend_on->obj_catalog = g_strdup (g_value_get_string (fk_catalog));
					tfk->depend_on->obj_schema = g_strdup (g_value_get_string (fk_schema));
					tfk->depend_on->obj_name = g_strdup (g_value_get_string (fk_tname));
					priv->db_objects = g_slist_append (priv->db_objects, tfk->depend_on);
					str = g_strdup_printf ("%s.%s.%s", g_value_get_string (fk_catalog), 
							       g_value_get_string (fk_schema), 
							       g_value_get_string (fk_tname));
					g_hash_table_insert (priv->index, str, tfk->depend_on);
				}
				else if (tfk->depend_on->obj_type == GDA_META_DB_TABLE) {
					GdaMetaTable *dot = GDA_META_TABLE (tfk->depend_on);
					dot->reverse_fk_list = g_slist_prepend (dot->reverse_fk_list, tfk);
				}
				dbo->depend_list = g_slist_append (dbo->depend_list, tfk->depend_on);

				if (G_VALUE_TYPE (upd_policy) == G_TYPE_STRING)
					tfk->on_update_policy = policy_string_to_value (g_value_get_string (upd_policy));
				else
					tfk->on_update_policy = GDA_META_FOREIGN_KEY_UNKNOWN;

				if (G_VALUE_TYPE (upd_policy) == G_TYPE_STRING)
					tfk->on_delete_policy = policy_string_to_value (g_value_get_string (del_policy));
				else
					tfk->on_delete_policy = GDA_META_FOREIGN_KEY_UNKNOWN;

				tfk->declared = FALSE;

				/* FIXME: compute @cols_nb, and all the @*_array members (ref_pk_cols_array must be
				 * initialized with -1 values everywhere */
				sql = "SELECT k.column_name, c.ordinal_position FROM _key_column_usage k INNER JOIN _columns c ON (c.table_catalog = k.table_catalog AND c.table_schema = k.table_schema AND c.table_name=k.table_name AND c.column_name=k.column_name) WHERE k.table_catalog = ##tc::string AND k.table_schema = ##ts::string AND k.table_name = ##tname::string AND k.constraint_name = ##cname::string ORDER BY k.ordinal_position";
				gboolean fkerror = FALSE;
				cvalue = gda_data_model_get_value_at (model, 3, i, error);
				if (!cvalue) goto onfkerror;
				fk_cols = gda_meta_store_extract (priv->store, sql, error,
								  "tc", icatalog, 
								  "ts", ischema, 
								  "tname", iname, 
								  "cname", cvalue, NULL);
				/*g_print ("tname=%s cvalue=%s\n", gda_value_stringify (iname),
				  gda_value_stringify (cvalue));*/

				cvalue = gda_data_model_get_value_at (model, 4, i, error);
				if (!cvalue) goto onfkerror;
				ref_pk_cols = gda_meta_store_extract (priv->store, sql, error,
								      "tc", fk_catalog,
								      "ts", fk_schema,
								      "tname", fk_tname,
								      "cname", cvalue, NULL);
				/*g_print ("tname=%s cvalue=%s\n", gda_value_stringify (fk_tname),
				  gda_value_stringify (cvalue));*/
				
				if (fk_cols && ref_pk_cols) {
					gint fk_nrows, ref_pk_nrows;
					fk_nrows = gda_data_model_get_n_rows (fk_cols);
					ref_pk_nrows = gda_data_model_get_n_rows (ref_pk_cols);
					if (fk_nrows != ref_pk_nrows) {
						/*gda_data_model_dump (fk_cols, stdout);
						  gda_data_model_dump (ref_pk_cols, stdout);*/
						if (ref_pk_nrows > 0)
							fkerror = TRUE;
						else {
							/* not an error, only the referenced table is not present
							 * in the meta store, which is possible if the meta
							 * store was only partially updated */
						}
					}
					else {
						gint n;
						tfk->cols_nb = fk_nrows;
						tfk->fk_cols_array = g_new0 (gint, fk_nrows);
						tfk->fk_names_array = g_new0 (gchar *, fk_nrows);
						tfk->ref_pk_cols_array = g_new0 (gint, fk_nrows);
						tfk->ref_pk_names_array = g_new0 (gchar *, fk_nrows);
						for (n = 0; n < fk_nrows; n++) {
							const GValue *cv;
							cv = gda_data_model_get_value_at (fk_cols, 1, n, error);
							if (!cv) goto onfkerror;
							tfk->fk_cols_array [n] = g_value_get_int (cv);

							cv = gda_data_model_get_value_at (fk_cols, 0, n, error);
							if (!cv) goto onfkerror;
							tfk->fk_names_array [n] = g_value_dup_string (cv);

							cv = gda_data_model_get_value_at (ref_pk_cols, 1, n, error);
							if (!cv) goto onfkerror;
							tfk->ref_pk_cols_array [n] = g_value_get_int (cv);

							cv = gda_data_model_get_value_at (ref_pk_cols, 0, n, error);
							if (!cv) goto onfkerror;
							tfk->ref_pk_names_array [n] = g_value_dup_string (cv);
						}
					}
				}
				else
					fkerror = TRUE;

				if (fkerror) {
					g_set_error (error, GDA_META_STRUCT_ERROR, GDA_META_STRUCT_INCOHERENCE_ERROR,
						     _("Meta data incoherence in foreign key constraint for table %s.%s.%s "
						       "referencing table %s.%s.%s"),
						     g_value_get_string (icatalog), 
						     g_value_get_string (ischema), 
						     g_value_get_string (iname),
						     g_value_get_string (fk_catalog), 
						     g_value_get_string (fk_schema), 
						     g_value_get_string (fk_tname));
					goto onfkerror;
				}
				if (fk_cols)
					g_object_unref (fk_cols);
				if (ref_pk_cols)
					g_object_unref (ref_pk_cols);
				
				mt->fk_list = g_slist_prepend (mt->fk_list, tfk);
				continue;

			onfkerror:
				if (fk_cols)
					g_object_unref (fk_cols);
				if (ref_pk_cols)
					g_object_unref (ref_pk_cols);
				if (tfk)
					mt->fk_list = g_slist_prepend (mt->fk_list, tfk);
				goto onerror;
			}
			mt->fk_list = g_slist_reverse (mt->fk_list);
			g_object_unref (model);
			/* Note: mt->reverse_fk_list is not determined here */

			add_declared_foreign_keys (mstruct, mt, NULL);
		}
		
		break;
	}
	default:
		TO_IMPLEMENT;
	}

	if (dbo && !g_slist_find (priv->db_objects, dbo)) {
		gchar *str;
		priv->db_objects = g_slist_append (priv->db_objects, dbo);
		str = g_strdup_printf ("%s.%s.%s", g_value_get_string (icatalog), 
				       g_value_get_string (ischema), 
				       g_value_get_string (iname));
		g_hash_table_insert (priv->index, str, dbo);
	}
	if (dbo && (dbo->obj_type == GDA_META_DB_TABLE) &&
	    (priv->features & GDA_META_STRUCT_FEATURE_FOREIGN_KEYS)) {
		/* compute GdaMetaTableForeignKey's @ref_pk_cols_array arrays and GdaMetaTable' @reverse_fk_list lists*/
		GSList *list;
		for (list = priv->db_objects; list; list = list->next) {
			GdaMetaDbObject *tmpdbo;
			tmpdbo = GDA_META_DB_OBJECT (list->data);
			if (tmpdbo->obj_type != GDA_META_DB_TABLE)
				continue;

			GdaMetaTable *mt = GDA_META_TABLE (tmpdbo);
			GSList *klist;
			for (klist = mt->fk_list; klist; klist = klist->next) {
				GdaMetaTableForeignKey *tfk = GDA_META_TABLE_FOREIGN_KEY (klist->data);
				GdaMetaTable *ref_mt = GDA_META_TABLE (tfk->depend_on);
				gint i;

				if (tfk->depend_on != dbo)
					continue;

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
				
				GDA_META_TABLE (dbo)->reverse_fk_list = g_slist_append (GDA_META_TABLE (dbo)->reverse_fk_list,
											tfk);
			}
		}
	}
	return dbo;

 onerror:
	if (dbo)
		dbo->obj_type = GDA_META_DB_UNKNOWN;

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
	
	cvalue = gda_data_model_get_value_at (model, 0, 0, NULL);
	if (!cvalue)
		return NULL;
	if (gda_value_is_null (cvalue) || !g_value_get_string (cvalue)) {
		/* use array_spec */
		gchar *str2;
		cvalue = gda_data_model_get_value_at (model, 1, 0, NULL);
		if (!cvalue)
			return NULL;
		str2 = array_type_to_sql (store, cvalue);
		str = g_strdup_printf ("%s[]", str2);
		g_free (str2);
	}
	else {
		cvalue = gda_data_model_get_value_at (model, 0, 0, NULL);
		if (!cvalue)
			return NULL;
		str = g_strdup_printf ("%s[]", g_value_get_string (cvalue));
	}
	g_object_unref (model);
	return str;
}


/**
 * gda_meta_struct_complement_schema:
 * @mstruct: a #GdaMetaStruct object
 * @catalog: (nullable): name of a catalog, or %NULL
 * @schema: (nullable): name of a schema, or %NULL
 * @error: (nullable): a place to store errors, or %NULL
 *
 * This method is similar to gda_meta_struct_complement() but creates #GdaMetaDbObject for all the
 * database object which are in the @schema schema (and in the @catalog catalog).
 * If @catalog is %NULL, then any catalog will be used, and
 * if @schema is %NULL then any schema will be used (if @schema is %NULL then catalog must also be %NULL).
 *
 * Please refer to gda_meta_struct_complement() form more information.
 *
 * Returns: TRUE if no error occurred
 */
gboolean
gda_meta_struct_complement_schema (GdaMetaStruct *mstruct, const GValue *catalog, const GValue *schema,
				   GError **error)
{
	GdaDataModel *tables_model = NULL, *views_model = NULL;
	gint i, nrows, k;
	const GValue *cvalues[6];
	
	/* schema and catalog are known */
	const gchar *sql1 = "SELECT table_name "
		"FROM _tables WHERE table_short_name, table_full_name, table_owner, table_catalog = ##cat::string AND table_schema = ##schema::string "
		"AND table_type LIKE '%TABLE%' "
		"ORDER BY table_schema, table_name";
	const gchar *sql2 = "SELECT table_short_name, table_full_name, table_owner, table_name "
		"FROM _tables WHERE table_catalog = ##cat::string AND table_schema = ##schema::string "
		"AND table_type LIKE '%VIEW%' "
		"ORDER BY table_schema, table_name";
	
	/* schema is known, catalog unknown */
	const gchar *sql3 = "SELECT table_short_name, table_full_name, table_owner, table_name, table_catalog, table_schema "
		"FROM _tables WHERE table_schema = ##schema::string AND table_type LIKE '%TABLE%' "
		"ORDER BY table_schema, table_name";
	const gchar *sql4 = "SELECT table_short_name, table_full_name, table_owner, table_name, table_catalog, table_schema "
		"FROM _tables WHERE table_schema = ##schema::string AND table_type LIKE '%VIEW%' "
		"ORDER BY table_schema, table_name";

	/* schema and catalog are unknown */
	const gchar *sql5 = "SELECT table_short_name, table_full_name, table_owner, table_name, table_catalog, table_schema "
		"FROM _tables WHERE table_type LIKE '%TABLE%' "
		"ORDER BY table_schema, table_name";
	const gchar *sql6 = "SELECT table_short_name, table_full_name, table_owner, table_name, table_catalog, table_schema "
		"FROM _tables WHERE table_type LIKE '%VIEW%' "
		"ORDER BY table_schema, table_name";

	g_return_val_if_fail (GDA_IS_META_STRUCT (mstruct), FALSE);
	GdaMetaStructPrivate *priv = gda_meta_struct_get_instance_private (mstruct);
	g_return_val_if_fail (priv->store, FALSE);
	g_return_val_if_fail (!catalog || (catalog && schema), FALSE);
	g_return_val_if_fail (!catalog || (G_VALUE_TYPE (catalog) == G_TYPE_STRING), FALSE);
	g_return_val_if_fail (!schema || (G_VALUE_TYPE (schema) == G_TYPE_STRING), FALSE);

	if (schema) {
		if (catalog) {
			tables_model = gda_meta_store_extract (priv->store, sql1, error,
							       "cat", catalog, "schema", schema, NULL);
			if (tables_model)
				views_model = gda_meta_store_extract (priv->store, sql2, error,
								      "cat", catalog, "schema", schema, NULL);
		}
		else {
			tables_model = gda_meta_store_extract (priv->store, sql3,
							       error, "schema", schema, NULL);
			if (tables_model)
				views_model = gda_meta_store_extract (priv->store, sql4,
								      error, "schema", schema, NULL);
		}
	}
	else {
		tables_model = gda_meta_store_extract (priv->store, sql5, error, NULL);
		if (tables_model)
			views_model = gda_meta_store_extract (priv->store, sql6, error, NULL);
	}

	if (!tables_model || !views_model)
		return FALSE;
	
	/* tables */
	nrows = gda_data_model_get_n_rows (tables_model);
	for (i = 0; i < nrows; i++) {
		for (k = 0; k <= 5; k++) {
			cvalues [k] = gda_data_model_get_value_at (tables_model, k, i, error);
			if (!cvalues [k]) {
				g_object_unref (tables_model);
				g_object_unref (views_model);
				return FALSE;
			}
		}
		if (!_meta_struct_complement (mstruct, GDA_META_DB_TABLE,
					      catalog ? catalog : cvalues [4],
					      schema ? schema : cvalues [5],
					      cvalues [3],
					      cvalues [0],
					      cvalues [1],
					      cvalues [2], error)) {
			g_object_unref (tables_model);
			g_object_unref (views_model);
			return FALSE;
		}
	}
	g_object_unref (tables_model);

	/* views */
	nrows = gda_data_model_get_n_rows (views_model);
	for (i = 0; i < nrows; i++) {
		for (k = 0; k <= 5; k++) {
			cvalues [k] = gda_data_model_get_value_at (views_model, k, i, error);
			if (!cvalues [k]) {
				g_object_unref (views_model);
				return FALSE;
			}
		}
		if (!_meta_struct_complement (mstruct, GDA_META_DB_VIEW,
					      catalog ? catalog : cvalues [4],
					      schema ? schema : cvalues [5],
					      cvalues [3],
					      cvalues [0],
					      cvalues [1],
					      cvalues [2], error)) {
			g_object_unref (views_model);
			return FALSE;
		}
	}
	g_object_unref (views_model);

	return TRUE;
}

static gboolean
real_gda_meta_struct_complement_all (GdaMetaStruct *mstruct, gboolean default_only, GError **error)
{
	GdaDataModel *model;
	gint i, nrows, k;
	const GValue *cvalues[6];
	const gchar *sql1 = "SELECT table_catalog, table_schema, table_name, table_short_name, table_full_name, table_owner "
		"FROM _tables WHERE table_short_name = table_name AND table_type LIKE '%TABLE%' "
		"ORDER BY table_schema, table_name";
	const gchar *sql2 = "SELECT table_catalog, table_schema, table_name, table_short_name, table_full_name, table_owner "
		"FROM _tables WHERE table_short_name = table_name AND table_type='VIEW' "
		"ORDER BY table_schema, table_name";
	const gchar *sql3 = "SELECT table_catalog, table_schema, table_name, table_short_name, table_full_name, table_owner "
		"FROM _tables WHERE table_type LIKE '%TABLE%' "
		"ORDER BY table_schema, table_name";
	const gchar *sql4 = "SELECT table_catalog, table_schema, table_name, table_short_name, table_full_name, table_owner "
		"FROM _tables WHERE table_type='VIEW' "
		"ORDER BY table_schema, table_name";

	g_return_val_if_fail (GDA_IS_META_STRUCT (mstruct), FALSE);
	GdaMetaStructPrivate *priv = gda_meta_struct_get_instance_private (mstruct);
	g_return_val_if_fail (priv->store, FALSE);

	/* tables */
	model = gda_meta_store_extract (priv->store, default_only ? sql1 : sql3, error, NULL);
	if (!model)
		return FALSE;
	nrows = gda_data_model_get_n_rows (model);
	for (i = 0; i < nrows; i++) {
		for (k = 0; k <= 5; k++) {
			cvalues [k] = gda_data_model_get_value_at (model, k, i, error);
			if (!cvalues [k]) {
				g_object_unref (model);
				return FALSE;
			}
		}
		if (!_meta_struct_complement (mstruct, GDA_META_DB_TABLE,
					      cvalues [0], cvalues [1],
					      cvalues [2], cvalues [3],
					      cvalues [4], cvalues [5], error)) {
			g_object_unref (model);
			return FALSE;
		}
	}
	g_object_unref (model);

	/* views */
	model = gda_meta_store_extract (priv->store, default_only ? sql2 : sql4, error, NULL);
	if (!model)
		return FALSE;
	nrows = gda_data_model_get_n_rows (model);
	for (i = 0; i < nrows; i++) {
		for (k = 0; k <= 5; k++) {
			cvalues [k] = gda_data_model_get_value_at (model, k, i, error);
			if (!cvalues [k]) {
				g_object_unref (model);
				return FALSE;
			}
		}
		if (!_meta_struct_complement (mstruct, GDA_META_DB_VIEW,
					      cvalues [0], cvalues [1],
					      cvalues [2], cvalues [3],
					      cvalues [4], cvalues [5], error)) {
			g_object_unref (model);
			return FALSE;
		}
	}
	g_object_unref (model);
	return TRUE;
}

/**
 * gda_meta_struct_complement_default:
 * @mstruct: a #GdaMetaStruct object
 * @error: (nullable): a place to store errors, or %NULL
 *
 * This method is similar to gda_meta_struct_complement() and gda_meta_struct_complement_all()
 * but creates #GdaMetaDbObject for all the
 * database object which are usable using only their short name (that is which do not need to be prefixed by 
 * the schema in which they are to be used).
 *
 * Please refer to gda_meta_struct_complement() form more information.
 *
 * Returns: TRUE if no error occurred
 */
gboolean
gda_meta_struct_complement_default (GdaMetaStruct *mstruct, GError **error)
{
	return real_gda_meta_struct_complement_all (mstruct, TRUE, error);
}

/**
 * gda_meta_struct_complement_all:
 * @mstruct: a #GdaMetaStruct object
 * @error: (nullable): a place to store errors, or %NULL
 *
 * This method is similar to gda_meta_struct_complement() and gda_meta_struct_complement_default()
 * but creates #GdaMetaDbObject for all the database object.
 * 
 * Please refer to gda_meta_struct_complement() form more information.
 *
 * Returns: TRUE if no error occurred
 */
gboolean
gda_meta_struct_complement_all (GdaMetaStruct *mstruct, GError **error)
{
	return real_gda_meta_struct_complement_all (mstruct, FALSE, error);
}

/**
 * gda_meta_struct_complement_depend:
 * @mstruct: a #GdaMetaStruct object
 * @dbo: a #GdaMetaDbObject part of @mstruct
 * @error: (nullable): a place to store errors, or %NULL
 *
 * This method is similar to gda_meta_struct_complement() but creates #GdaMetaDbObject for all the dependencies
 * of @dbo.
 *
 * Please refer to gda_meta_struct_complement() form more information.
 *
 * Returns: TRUE if no error occurred
 */
gboolean
gda_meta_struct_complement_depend (GdaMetaStruct *mstruct, GdaMetaDbObject *dbo,
				   GError **error)
{
	GSList *list;

	g_return_val_if_fail (GDA_IS_META_STRUCT (mstruct), FALSE);
	GdaMetaStructPrivate *priv = gda_meta_struct_get_instance_private (mstruct);
	g_return_val_if_fail (priv->store, FALSE);
	g_return_val_if_fail (dbo, FALSE);
	g_return_val_if_fail (g_slist_find (priv->db_objects, dbo), FALSE);

	for (list = dbo->depend_list; list; list = list->next) {
		GdaMetaDbObject *dep_dbo = GDA_META_DB_OBJECT (list->data);
		if (dep_dbo->obj_type != GDA_META_DB_UNKNOWN)
			continue;
		
		GValue *cat = NULL, *schema = NULL, *name = NULL;
		GdaMetaDbObject *tmpobj;
		g_return_val_if_fail (dep_dbo->obj_name, FALSE);
		if (dep_dbo->obj_catalog)
			g_value_take_string ((cat = gda_value_new (G_TYPE_STRING)), 
					     g_strdup_printf ("\"%s\"", dep_dbo->obj_catalog));
		if (dep_dbo->obj_schema)
			g_value_take_string ((schema = gda_value_new (G_TYPE_STRING)), 
					     g_strdup_printf ("\"%s\"", dep_dbo->obj_schema));
		g_value_take_string ((name = gda_value_new (G_TYPE_STRING)), 
				     g_strdup_printf ("\"%s\"", dep_dbo->obj_name));
		tmpobj = gda_meta_struct_complement (mstruct, GDA_META_DB_UNKNOWN, cat, schema, name, error);
		if (cat) gda_value_free (cat);
		if (schema) gda_value_free (schema);
		gda_value_free (name);
		if (!tmpobj)
			return FALSE;
	}
	return TRUE;
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
	gint retval = 0;
	if (dbo1->obj_schema && dbo2->obj_schema) {
		retval = strcmp (dbo1->obj_schema, dbo2->obj_schema);
		if (retval)
			return retval;
	}
	else if (dbo1->obj_schema)
		return 1;
	else if (dbo2->obj_schema)
		return -1;

	if (dbo1->obj_name && dbo2->obj_name)
		return strcmp (dbo1->obj_name, dbo2->obj_name);
	else if (dbo1->obj_name)
		return 1;
	else if (dbo2->obj_name)
		return -1;
	return 0;
}

/**
 * gda_meta_struct_sort_db_objects:
 * @mstruct: a #GdaMetaStruct object
 * @sort_type: the kind of sorting requested
 * @error: (nullable): a place to store errors, or %NULL
 *
 * Reorders the list of database objects within @mstruct in a way specified by @sort_type.
 *
 * Returns: TRUE if no error occurred
 */ 
gboolean
gda_meta_struct_sort_db_objects (GdaMetaStruct *mstruct, GdaMetaSortType sort_type,
				 G_GNUC_UNUSED GError **error)
{
	GSList *pass_list;
	GSList *ordered_list = NULL;

	g_return_val_if_fail (GDA_IS_META_STRUCT (mstruct), FALSE);
	GdaMetaStructPrivate *priv = gda_meta_struct_get_instance_private (mstruct);

	switch (sort_type) {
	case GDA_META_SORT_ALHAPETICAL:
		priv->db_objects = g_slist_sort (priv->db_objects, (GCompareFunc) db_object_sort_func);
		ordered_list = priv->db_objects;
		break;
	case GDA_META_SORT_DEPENDENCIES:
		if (mstruct == NULL) {
			g_set_error (error, GDA_META_STRUCT_ERROR, GDA_META_STORE_INTERNAL_ERROR,
			     _("No meta struct was provided for object sorting"));
			return FALSE;
		}
		for (pass_list = build_pass (priv->db_objects, ordered_list);
		     pass_list; 
		     pass_list = build_pass (priv->db_objects, ordered_list))
			ordered_list = g_slist_concat (ordered_list, pass_list);
		g_slist_free (priv->db_objects);
		priv->db_objects = ordered_list;
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

/*
 * Same as gda_meta_struct_get_db_object except that @catalog, @schema and @name are ready to be
 * compared (no need to double quote)
 */
static GdaMetaDbObject *
_meta_struct_get_db_object (GdaMetaStruct *mstruct, const GValue *catalog, const GValue *schema, const GValue *name)
{
	gchar *key;
	GdaMetaDbObject *dbo;
	GdaMetaStructPrivate *priv = gda_meta_struct_get_instance_private (mstruct);

	if (catalog && schema) {
		g_return_val_if_fail (G_VALUE_TYPE (catalog) == G_TYPE_STRING, NULL);
		g_return_val_if_fail (G_VALUE_TYPE (schema) == G_TYPE_STRING, NULL);

		key = g_strdup_printf ("%s.%s.%s", g_value_get_string (catalog), g_value_get_string (schema), 
				       g_value_get_string (name));
		dbo = g_hash_table_lookup (priv->index, key);
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

		for (list = priv->db_objects; list; list = list->next) {
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
 * gda_meta_struct_get_all_db_objects:
 * @mstruct: a #GdaMetaStruct object
 *
 * Get a list of all the #GdaMetaDbObject structures representing database objects in @mstruct. Note that
 * no #GdaMetaDbObject structure must not be modified.
 *
 * Returns: (transfer container) (element-type Gda.MetaDbObject) (nullable): a new #GSList list of pointers to
 * #GdaMetaDbObject structures which must be destroyed after usage using g_slist_free(). The individual
 * #GdaMetaDbObject must not be modified.
 */
GSList *
gda_meta_struct_get_all_db_objects (GdaMetaStruct *mstruct)
{
	g_return_val_if_fail (GDA_IS_META_STRUCT (mstruct), NULL);
	GdaMetaStructPrivate *priv = gda_meta_struct_get_instance_private (mstruct);
	if (priv->db_objects)
		return g_slist_copy (priv->db_objects);
	else
		return NULL;
}

/**
 * gda_meta_struct_get_db_object:
 * @mstruct: a #GdaMetaStruct object
 * @catalog: (nullable): the catalog the object belongs to (as a G_TYPE_STRING GValue), or %NULL
 * @schema: (nullable): the schema the object belongs to (as a G_TYPE_STRING GValue), or %NULL
 * @name: the object's name (as a G_TYPE_STRING GValue), not %NULL
 *
 * Tries to locate the #GdaMetaDbObject structure representing the database object named after
 * @catalog, @schema and @name.
 *
 * If one or both of @catalog and @schema are %NULL, and more than one database object matches the name, then
 * the return value is also %NULL.
 *
 * Returns: (transfer none) (nullable): the #GdaMetaDbObject or %NULL if not found
 */
GdaMetaDbObject *
gda_meta_struct_get_db_object (GdaMetaStruct *mstruct, const GValue *catalog, const GValue *schema, const GValue *name)
{
	GdaMetaDbObject *dbo;
	GValue *icatalog = NULL, *ischema = NULL, *iname = NULL; /* GValue with identifiers ready to be compared */

	g_return_val_if_fail (GDA_IS_META_STRUCT (mstruct), NULL);
	g_return_val_if_fail (name && (G_VALUE_TYPE (name) == G_TYPE_STRING), NULL);
	g_return_val_if_fail (!catalog || (catalog && schema), NULL);
	g_return_val_if_fail (!catalog || (G_VALUE_TYPE (catalog) == G_TYPE_STRING), NULL);
	g_return_val_if_fail (!schema || (G_VALUE_TYPE (schema) == G_TYPE_STRING), NULL);

	/* prepare identifiers */
	g_value_take_string ((iname = gda_value_new (G_TYPE_STRING)), prepare_sql_identifier_for_compare (g_value_dup_string (name)));
	if (catalog)
		g_value_take_string ((icatalog = gda_value_new (G_TYPE_STRING)), 
				     prepare_sql_identifier_for_compare (g_value_dup_string (catalog)));
	if (schema) 
		g_value_take_string ((ischema = gda_value_new (G_TYPE_STRING)), 
				     prepare_sql_identifier_for_compare (g_value_dup_string (schema)));

	dbo = _meta_struct_get_db_object (mstruct, icatalog, ischema, iname);
	if (icatalog) gda_value_free (icatalog);
	if (ischema) gda_value_free (ischema);
	gda_value_free (iname);

	return dbo;
}

/**
 * gda_meta_struct_get_table_column:
 * @mstruct: a #GdaMetaStruct object
 * @table: the #GdaMetaTable structure to find the column for
 * @col_name: the name of the column to find (as a G_TYPE_STRING GValue)
 *
 * Tries to find the #GdaMetaTableColumn representing the column named @col_name in @table.
 *
 * Returns: (transfer none) (nullable): the #GdaMetaTableColumn or %NULL if not found
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
 * gda_meta_struct_dump_as_graph:
 * @mstruct: a #GdaMetaStruct object
 * @info: informs what kind of information to show in the resulting graph
 * @error: (nullable): a place to store errors, or %NULL
 *
 * Creates a new graph (in the GraphViz syntax) representation of @mstruct.
 *
 * Returns: (transfer full) (nullable): a new string, or %NULL if an error occurred.
 */
gchar *
gda_meta_struct_dump_as_graph (GdaMetaStruct *mstruct, GdaMetaGraphInfo info, G_GNUC_UNUSED GError **error)
{
	GString *string;
	gchar *result;

	g_return_val_if_fail (GDA_IS_META_STRUCT (mstruct), NULL);
	GdaMetaStructPrivate *priv = gda_meta_struct_get_instance_private (mstruct);
	
	string = g_string_new ("digraph G {\nrankdir = BT;\nnode [shape = plaintext];\n");
	GSList *dbo_list;
	for (dbo_list = priv->db_objects; dbo_list; dbo_list = dbo_list->next) {
		gchar *objname, *fullname;
		GdaMetaDbObject *dbo = GDA_META_DB_OBJECT (dbo_list->data);
		GSList *list;
		gboolean use_html = (info & GDA_META_GRAPH_COLUMNS) ? TRUE : FALSE;

		/* obj human readable name, and full name */
		fullname = g_strdup_printf ("%s.%s.%s", dbo->obj_catalog, dbo->obj_schema, dbo->obj_name);
		if (dbo->obj_short_name) 
			objname = g_strdup (dbo->obj_short_name);
		else if (dbo->obj_schema)
			objname = g_strdup_printf ("%s.%s", dbo->obj_schema, dbo->obj_name);
		else
			objname = g_strdup (dbo->obj_name);

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
			GdaMetaTable *mt = GDA_META_TABLE (dbo);
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
			GdaMetaTable *mt = GDA_META_TABLE (dbo);
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
gda_meta_db_object_free_contents (GdaMetaDbObject *dbo)
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
		gda_meta_table_free_contents (GDA_META_TABLE (dbo));
		break;
	case GDA_META_DB_VIEW:
		gda_meta_view_free_contents (GDA_META_VIEW (dbo));
		break;
	default:
		TO_IMPLEMENT;
	}
	g_slist_free (dbo->depend_list);
	memset (dbo, 0, sizeof (GdaMetaDbObject));
}

static void
gda_meta_db_object_free (GdaMetaDbObject *dbo)
{
	gda_meta_db_object_free_contents (dbo);
	g_free (dbo);
}

static void
gda_meta_table_free_contents (GdaMetaTable *table)
{
	g_slist_free_full (table->columns, (GDestroyNotify) gda_meta_table_column_free);
	g_free (table->pk_cols_array);
	g_slist_free_full (table->fk_list, (GDestroyNotify) gda_meta_table_foreign_key_free);
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
	if (tcol->desc != NULL) {
		g_free (tcol->desc);
	}
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
	g_free (tfk->fk_name);
	g_free (tfk);
}

static gboolean 
determine_db_object_from_schema_and_name (GdaMetaStruct *mstruct, 
					  GdaMetaDbObjectType *in_out_type, GValue **out_catalog,
					  GValue **out_short_name, 
					  GValue **out_full_name, GValue **out_owner, 
					  const GValue *schema, const GValue *name)
{
	const GValue *cvalue;
	GdaDataModel *model = NULL;
	*out_catalog = NULL;
	*out_short_name = NULL;
	*out_full_name = NULL;
	*out_owner = NULL;
	GdaMetaStructPrivate *priv = gda_meta_struct_get_instance_private (mstruct);

	switch (*in_out_type) {
	case GDA_META_DB_UNKNOWN: {
		GdaMetaDbObjectType type = GDA_META_DB_TABLE;
		if (determine_db_object_from_schema_and_name (mstruct, &type, out_catalog,
							      out_short_name, out_full_name, out_owner,
							      schema, name)) {
			*in_out_type = GDA_META_DB_TABLE;
			return TRUE;
		}
		type = GDA_META_DB_VIEW;
		if (determine_db_object_from_schema_and_name (mstruct, &type, out_catalog,
							      out_short_name, out_full_name, out_owner,
							      schema, name)) {
			*in_out_type = GDA_META_DB_VIEW;
			return TRUE;
		}
		return FALSE;
		break;
	}

	case GDA_META_DB_TABLE: {
		const gchar *sql = "SELECT table_catalog, table_short_name, table_full_name, table_owner FROM _tables as t WHERE table_schema = ##ts::string AND table_name = ##tname::string AND table_name NOT IN (SELECT v.table_name FROM _views as v WHERE v.table_catalog=t.table_catalog AND v.table_schema=t.table_schema)";
		gint nrows;
		model = gda_meta_store_extract (priv->store, sql, NULL, "ts", schema, "tname", name, NULL);
		if (!model) 
			return FALSE;

		nrows = gda_data_model_get_n_rows (model);
		if (nrows != 1) {
			g_object_unref (model);
			return FALSE;
		}
		cvalue = gda_data_model_get_value_at (model, 0, 0, NULL);
		if (!cvalue) goto copyerror;
		*out_catalog = gda_value_copy (cvalue);

		cvalue = gda_data_model_get_value_at (model, 1, 0, NULL);
		if (!cvalue) goto copyerror;
		*out_short_name = gda_value_copy (cvalue);

		cvalue = gda_data_model_get_value_at (model, 2, 0, NULL);
		if (!cvalue) goto copyerror;
		*out_full_name = gda_value_copy (cvalue);

		cvalue = gda_data_model_get_value_at (model, 3, 0, NULL);
		if (!cvalue) goto copyerror;
		*out_owner = gda_value_copy (cvalue);

		g_object_unref (model);
		return TRUE;
	}

	case GDA_META_DB_VIEW:{
		const gchar *sql = "SELECT table_catalog, table_short_name, table_full_name, table_owner FROM _tables NATURAL JOIN _views WHERE table_schema = ##ts::string AND table_name = ##tname::string";
		gint nrows;
		model = gda_meta_store_extract (priv->store, sql, NULL, "ts", schema, "tname", name, NULL);
		if (!model) 
			return FALSE;

		nrows = gda_data_model_get_n_rows (model);
		if (nrows != 1) {
			g_object_unref (model);
			return FALSE;
		}
		cvalue = gda_data_model_get_value_at (model, 0, 0, NULL);
		if (!cvalue) goto copyerror;
		*out_catalog = gda_value_copy (cvalue);

		cvalue = gda_data_model_get_value_at (model, 1, 0, NULL);
		if (!cvalue) goto copyerror;
		*out_short_name = gda_value_copy (cvalue);

		cvalue = gda_data_model_get_value_at (model, 2, 0, NULL);
		if (!cvalue) goto copyerror;
		*out_full_name = gda_value_copy (cvalue);

		cvalue = gda_data_model_get_value_at (model, 3, 0, NULL);
		if (!cvalue) goto copyerror;
		*out_owner = gda_value_copy (cvalue);

		g_object_unref (model);
		return TRUE;
	}
	default:
		TO_IMPLEMENT;
	}

 copyerror:
	if (model)
		g_object_unref (model);
	if (*out_catalog) {
		gda_value_free (*out_catalog);
		*out_catalog = NULL;
	}
	if (*out_short_name) {
		gda_value_free (*out_short_name);
		*out_short_name = NULL;
	}
	if (*out_full_name) {
		gda_value_free (*out_full_name);
		*out_full_name = NULL;
	}
	if (*out_owner) {
		gda_value_free (*out_owner);
		*out_owner = NULL;
	}

	return FALSE;
}

static gboolean
determine_db_object_from_short_name (GdaMetaStruct *mstruct,
				     GdaMetaDbObjectType *in_out_type, GValue **out_catalog,
				     GValue **out_schema, GValue **out_name, GValue **out_short_name, 
				     GValue **out_full_name, GValue **out_owner, const GValue *name)
{
	const GValue *cvalue;
	GdaDataModel *model = NULL;
	*out_name = NULL;
	*out_schema = NULL;
	*out_catalog = NULL;
	*out_short_name = NULL;
	*out_full_name = NULL;
	*out_owner = NULL;
	GdaMetaStructPrivate *priv = gda_meta_struct_get_instance_private (mstruct);

	/* general lookup */
	switch (*in_out_type) {
	case GDA_META_DB_UNKNOWN: {
		GdaMetaDbObjectType type = GDA_META_DB_TABLE;
		if (determine_db_object_from_short_name (mstruct, &type, out_catalog, out_schema, out_name,
							 out_short_name, out_full_name, out_owner, name)) {
			*in_out_type = GDA_META_DB_TABLE;
			return TRUE;
		}
		type = GDA_META_DB_VIEW;
		if (determine_db_object_from_short_name (mstruct, &type, out_catalog, out_schema, out_name,
							 out_short_name, out_full_name, out_owner, name)) {
			*in_out_type = GDA_META_DB_VIEW;
			return TRUE;
		}
		return FALSE;
		break;
	}

	case GDA_META_DB_TABLE: {
		const gchar *sql = "SELECT table_catalog, table_schema, table_name, table_short_name, table_full_name, table_owner FROM _tables as t WHERE table_short_name = ##tname::string AND table_name NOT IN (SELECT v.table_name FROM _views as v WHERE v.table_catalog=t.table_catalog AND v.table_schema=t.table_schema)";
		gint nrows;
		model = gda_meta_store_extract (priv->store, sql, NULL, "tname", name, NULL);
		if (!model) 
			return FALSE;

		nrows = gda_data_model_get_n_rows (model);
		if (nrows != 1) {
			g_object_unref (model);
			goto next;
		}

		cvalue = gda_data_model_get_value_at (model, 0, 0, NULL);
		if (!cvalue) goto copyerror;
		*out_catalog = gda_value_copy (cvalue);

		cvalue = gda_data_model_get_value_at (model, 1, 0, NULL);
		if (!cvalue) goto copyerror;
		*out_schema = gda_value_copy (cvalue);

		cvalue = gda_data_model_get_value_at (model, 2, 0, NULL);
		if (!cvalue) goto copyerror;
		*out_name = gda_value_copy (cvalue);

		cvalue = gda_data_model_get_value_at (model, 3, 0, NULL);
		if (!cvalue) goto copyerror;
		*out_short_name = gda_value_copy (cvalue);

		cvalue = gda_data_model_get_value_at (model, 4, 0, NULL);
		if (!cvalue) goto copyerror;
		*out_full_name = gda_value_copy (cvalue);

		cvalue = gda_data_model_get_value_at (model, 5, 0, NULL);
		if (!cvalue) goto copyerror;
		*out_owner = gda_value_copy (cvalue);

		g_object_unref (model);
		return TRUE;
	}

	case GDA_META_DB_VIEW:{
		const gchar *sql = "SELECT table_catalog, table_schema, table_name, table_short_name, table_full_name, table_owner FROM _tables NATURAL JOIN _views WHERE table_short_name = ##tname::string";
		gint nrows;
		model = gda_meta_store_extract (priv->store, sql, NULL, "tname", name, NULL);
		if (!model) 
			return FALSE;

		nrows = gda_data_model_get_n_rows (model);
		if (nrows != 1) {
			g_object_unref (model);
			goto next;
		}

		cvalue = gda_data_model_get_value_at (model, 0, 0, NULL);
		if (!cvalue) goto copyerror;
		*out_catalog = gda_value_copy (cvalue);

		cvalue = gda_data_model_get_value_at (model, 1, 0, NULL);
		if (!cvalue) goto copyerror;
		*out_schema = gda_value_copy (cvalue);

		cvalue = gda_data_model_get_value_at (model, 2, 0, NULL);
		if (!cvalue) goto copyerror;
		*out_name = gda_value_copy (cvalue);

		cvalue = gda_data_model_get_value_at (model, 3, 0, NULL);
		if (!cvalue) goto copyerror;
		*out_short_name = gda_value_copy (cvalue);

		cvalue = gda_data_model_get_value_at (model, 4, 0, NULL);
		if (!cvalue) goto copyerror;
		*out_full_name = gda_value_copy (cvalue);

		cvalue = gda_data_model_get_value_at (model, 5, 0, NULL);
		if (!cvalue) goto copyerror;
		*out_owner = gda_value_copy (cvalue);

		g_object_unref (model);
		return TRUE;
	}
	default:
		TO_IMPLEMENT;
	}

 next:
	model = NULL;
	{
		/* treat the case where name is in fact <schema>.<name> */
		gchar *obj_schema;
		gchar *obj_name;
		if (_split_identifier_string (g_strdup ((gchar *) g_value_get_string (name)), &obj_schema, &obj_name)) {
			GValue *sv, *nv;
			gboolean retval;
			if (!obj_schema) {
				g_free (obj_name);
				return FALSE;
			}
			if (!obj_name) {
				g_free (obj_schema);
				return FALSE;
			}
			g_value_take_string ((sv = gda_value_new (G_TYPE_STRING)), prepare_sql_identifier_for_compare (obj_schema));
			g_value_take_string ((nv = gda_value_new (G_TYPE_STRING)), prepare_sql_identifier_for_compare (obj_name));
			retval = determine_db_object_from_schema_and_name (mstruct, in_out_type, out_catalog, 
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
			return retval;
		}
	}

 copyerror:
	if (model)
		g_object_unref (model);
	if (*out_catalog) {
		gda_value_free (*out_catalog);
		*out_catalog = NULL;
	}
	if (*out_schema) {
		gda_value_free (*out_schema);
		*out_schema = NULL;
	}
	if (*out_name) {
		gda_value_free (*out_name);
		*out_name = NULL;
	}
	if (*out_short_name) {
		gda_value_free (*out_short_name);
		*out_short_name = NULL;
	}
	if (*out_full_name) {
		gda_value_free (*out_full_name);
		*out_full_name = NULL;
	}
	if (*out_owner) {
		gda_value_free (*out_owner);
		*out_owner = NULL;
	}

	return FALSE;
}

static gboolean
determine_db_object_from_missing_type (GdaMetaStruct *mstruct, 
				       GdaMetaDbObjectType *out_type, GValue **out_short_name, 
				       GValue **out_full_name, GValue **out_owner, const GValue *catalog, 
				       const GValue *schema, const GValue *name)
{
	/* try as a view first */
	const gchar *sql = "SELECT table_short_name, table_full_name, table_owner FROM _tables NATURAL JOIN _views WHERE table_catalog = ##tc::string AND table_schema = ##ts::string AND table_name = ##tname::string";
	GdaDataModel *model = NULL;
	const GValue *cvalue;
	GdaMetaStructPrivate *priv = gda_meta_struct_get_instance_private (mstruct);

	*out_type = GDA_META_DB_UNKNOWN;
	*out_short_name = NULL;
	*out_full_name = NULL;
	*out_owner = NULL;

	model = gda_meta_store_extract (priv->store, sql, NULL, "tc", catalog, "ts", schema, "tname", name, NULL);
	if (model && (gda_data_model_get_n_rows (model) == 1)) {
		*out_type = GDA_META_DB_VIEW;
		
		cvalue = gda_data_model_get_value_at (model, 0, 0, NULL);
		if (!cvalue) goto copyerror;
		*out_short_name = gda_value_copy (cvalue);

		cvalue = gda_data_model_get_value_at (model, 1, 0, NULL);
		if (!cvalue) goto copyerror;
		*out_full_name = gda_value_copy (cvalue);

		cvalue = gda_data_model_get_value_at (model, 2, 0, NULL);
		if (!cvalue) goto copyerror;
		*out_owner = gda_value_copy (cvalue);

		g_object_unref (model);
		return TRUE;
	}
	if (model)
		g_object_unref (model);

	/* try as a table */
	sql = "SELECT table_short_name, table_full_name, table_owner FROM _tables WHERE table_catalog = ##tc::string AND table_schema = ##ts::string AND table_name = ##tname::string";
	model = gda_meta_store_extract (priv->store, sql, NULL, "tc", catalog, "ts", schema, "tname", name, NULL);
	if (model && (gda_data_model_get_n_rows (model) == 1)) {
		*out_type = GDA_META_DB_TABLE;

		cvalue = gda_data_model_get_value_at (model, 0, 0, NULL);
		if (!cvalue) goto copyerror;
		*out_short_name = gda_value_copy (cvalue);

		cvalue = gda_data_model_get_value_at (model, 1, 0, NULL);
		if (!cvalue) goto copyerror;
		*out_full_name = gda_value_copy (cvalue);

		cvalue = gda_data_model_get_value_at (model, 2, 0, NULL);
		if (!cvalue) goto copyerror;
		*out_owner = gda_value_copy (cvalue);

		g_object_unref (model);
		return TRUE;
	}
	if (model) {
		g_object_unref (model);
		model = NULL;
	}
	
copyerror:
	if (model)
		g_object_unref (model);

	*out_type = GDA_META_DB_UNKNOWN;
	if (*out_short_name) {
		gda_value_free (*out_short_name);
		*out_short_name = NULL;
	}
	if (*out_full_name) {
		gda_value_free (*out_full_name);
		*out_full_name = NULL;
	}
	if (*out_owner) {
		gda_value_free (*out_owner);
		*out_owner = NULL;
	}
	return FALSE;
}

/**
 * _gda_meta_struct_add_db_object:
 * @mstruct: a #GdaMetaStruct object
 * @dbo: a #GdaMetaDbObject structure
 * @error: (nullable): a place to store errors, or %NULL
 *
 * Adds @dbo to the database objects known to @mstruct. In any case (whether an error occured or not)
 * @dbo's ownership is then transferred to @smtruct and should
 * not be used after calling this function (it may have been destroyed). If you need a pointer to the #GdaMetaDbObject
 * for a database object, use gda_meta_struct_get_db_object().
 *
 * Returns: (transfer none) (nullable): a pointer to the #GdaMetaDbObject used in @mstruct to represent the added database object (may be @dbo or not)
 */
GdaMetaDbObject *
_gda_meta_struct_add_db_object (GdaMetaStruct *mstruct, GdaMetaDbObject *dbo, GError **error)
{
	GdaMetaDbObject *edbo;
	GValue *v1 = NULL, *v2 = NULL, *v3 = NULL;
	g_return_val_if_fail (GDA_IS_META_STRUCT (mstruct), NULL);
	g_return_val_if_fail (dbo, NULL);
	GdaMetaStructPrivate *priv = gda_meta_struct_get_instance_private (mstruct);

	if (!dbo->obj_name) {
		g_set_error (error, GDA_META_STRUCT_ERROR, GDA_META_STRUCT_INCOHERENCE_ERROR,
			     "%s", _("Missing object name in GdaMetaDbObject structure"));
		gda_meta_db_object_free (dbo);
		return NULL;
	}
	if (dbo->obj_catalog)
		g_value_set_string ((v1 = gda_value_new (G_TYPE_STRING)), dbo->obj_catalog);
	if (dbo->obj_schema)
		g_value_set_string ((v2 = gda_value_new (G_TYPE_STRING)), dbo->obj_schema);
	g_value_set_string ((v3 = gda_value_new (G_TYPE_STRING)), dbo->obj_name);

	edbo = gda_meta_struct_get_db_object (mstruct, v1, v2, v3);
	if (v1) gda_value_free (v1);
	if (v2) gda_value_free (v2);
	gda_value_free (v3);

	if (edbo) {
		if (edbo->obj_type == GDA_META_DB_UNKNOWN) {
			/* overwrite */
			gda_meta_db_object_free_contents (edbo);
			*edbo = *dbo;
			g_free (dbo);
			return edbo;
		}
		else {
			g_set_error (error, GDA_META_STRUCT_ERROR, GDA_META_STRUCT_DUPLICATE_OBJECT_ERROR,
				     _("Database object '%s' already exists"), edbo->obj_full_name);
			gda_meta_db_object_free (dbo);
			return NULL;
		}
	}
	else {
		priv->db_objects = g_slist_append (priv->db_objects, dbo);
		g_hash_table_insert (priv->index, g_strdup (dbo->obj_full_name), dbo);
		return dbo;
	}
}

