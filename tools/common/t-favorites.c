/*
 * Copyright (C) 2009 - 2010 Murray Cumming <murrayc@murrayc.com>
 * Copyright (C) 2009 - 2014 Vivien Malerba <malerba@gnome-db.org>
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

#include <string.h>
#include <glib/gi18n-lib.h>
#include "t-favorites.h"
#include "t-errors.h"
#include <libgda/gda-sql-builder.h>
#include <sql-parser/gda-sql-parser.h>

struct _TFavoritesPrivate {
	GdaMetaStore  *store;
	GdaConnection *store_cnc;
};


/*
 * Main static functions
 */
static void t_favorites_class_init (TFavoritesClass *klass);
static void t_favorites_init (TFavorites *bfav);
static void t_favorites_dispose (GObject *object);

/* get a pointer to the parents to be able to call their destructor */
static GObjectClass  *parent_class = NULL;

/* signals */
enum {
	FAV_CHANGED,
	LAST_SIGNAL
};

gint t_favorites_signals [LAST_SIGNAL] = { 0 };

GType
t_favorites_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static GMutex registering;
		static const GTypeInfo info = {
			sizeof (TFavoritesClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) t_favorites_class_init,
			NULL,
			NULL,
			sizeof (TFavorites),
			0,
			(GInstanceInitFunc) t_favorites_init,
			0
		};


		g_mutex_lock (&registering);
		if (type == 0)
			type = g_type_register_static (G_TYPE_OBJECT, "TFavorites", &info, 0);
		g_mutex_unlock (&registering);
	}
	return type;
}

static void
t_favorites_class_init (TFavoritesClass *klass)
{
	GObjectClass   *object_class = G_OBJECT_CLASS (klass);
	parent_class = g_type_class_peek_parent (klass);

	t_favorites_signals [FAV_CHANGED] =
		g_signal_new ("favorites-changed",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_FIRST | G_SIGNAL_DETAILED,
                              G_STRUCT_OFFSET (TFavoritesClass, favorites_changed),
                              NULL, NULL,
                              g_cclosure_marshal_VOID__VOID, G_TYPE_NONE,
                              0);

	klass->favorites_changed = NULL;

	object_class->dispose = t_favorites_dispose;
}

static void
t_favorites_init (TFavorites *bfav)
{
	bfav->priv = g_new0 (TFavoritesPrivate, 1);
	bfav->priv->store = NULL;
	bfav->priv->store_cnc = NULL;
}

static void
t_favorites_dispose (GObject *object)
{
	TFavorites *bfav;

	g_return_if_fail (object != NULL);
	g_return_if_fail (T_IS_FAVORITES (object));

	bfav = T_FAVORITES (object);
	if (bfav->priv) {
		if (bfav->priv->store)
			g_object_unref (bfav->priv->store);
		if (bfav->priv->store_cnc)
			g_object_unref (bfav->priv->store_cnc);

		g_free (bfav->priv);
		bfav->priv = NULL;
	}

	/* parent class */
	parent_class->dispose (object);
}

/**
 * t_favorites_new
 *
 * Creates a new #TFavorites object
 *
 * Returns: the new object
 */
TFavorites*
t_favorites_new (GdaMetaStore *store)
{
	TFavorites *bfav;

	g_return_val_if_fail (GDA_IS_META_STORE (store), NULL);

	bfav = T_FAVORITES (g_object_new (T_TYPE_FAVORITES, NULL));
	bfav->priv->store = g_object_ref (store);

	return bfav;
}

#define FAVORITES_TABLE_NAME "gda_sql_favorites"
#define FAVORITES_TABLE_DESC						\
        "<table name=\"" FAVORITES_TABLE_NAME "\"> "			\
        "   <column name=\"id\" type=\"gint\" pkey=\"TRUE\" autoinc=\"TRUE\"/>"	\
        "   <column name=\"session\" type=\"gint\"/>"			\
        "   <column name=\"type\"/>"					\
        "   <column name=\"name\" nullok=\"TRUE\"/>"			\
        "   <column name=\"contents\"/>"				\
        "   <column name=\"descr\" nullok=\"TRUE\"/>"			\
        "   <unique>"							\
        "     <column name=\"session\"/>"				\
        "     <column name=\"type\"/>"					\
        "     <column name=\"contents\"/>"				\
        "   </unique>"							\
        "</table>"
#define FAVORDER_TABLE_NAME "gda_sql_favorder"
#define FAVORDER_TABLE_DESC						\
        "<table name=\"" FAVORDER_TABLE_NAME "\"> "			\
        "   <column name=\"order_key\" type=\"gint\" pkey=\"TRUE\"/>"	\
        "   <column name=\"fav_id\" type=\"gint\" pkey=\"TRUE\"/>"	\
        "   <column name=\"rank\" type=\"gint\"/>"			\
        "</table>"

static gboolean
meta_store_addons_init (TFavorites *bfav, GError **error)
{
	GError *lerror = NULL;
	if (bfav->priv->store_cnc)
		return TRUE;

	if (!gda_meta_store_schema_add_custom_object (bfav->priv->store, FAVORITES_TABLE_DESC, &lerror)) {
                g_set_error (error, T_ERROR, T_STORED_DATA_ERROR,
			     "%s", _("Can't initialize dictionary to store favorites"));
		g_warning (_("Can't initialize dictionary to store favorites :%s"),
			   lerror && lerror->message ? lerror->message : "No detail");
		if (lerror)
			g_error_free (lerror);
                return FALSE;
        }
	if (!gda_meta_store_schema_add_custom_object (bfav->priv->store, FAVORDER_TABLE_DESC, &lerror)) {
                g_set_error (error, T_ERROR, T_STORED_DATA_ERROR,
			     "%s", _("Can't initialize dictionary to store favorites"));
		g_warning ("Can't initialize dictionary to store favorites :%s",
			   lerror && lerror->message ? lerror->message : "No detail");
		if (lerror)
			g_error_free (lerror);
                return FALSE;
        }

	bfav->priv->store_cnc = g_object_ref (gda_meta_store_get_internal_connection (bfav->priv->store));
	return TRUE;
}

/**
 * t_favorites_type_to_string:
 * @type: a #TFavoritesType
 *
 * Returns: a string representing @type
 */
const gchar *
t_favorites_type_to_string (TFavoritesType type)
{
	switch (type) {
	case T_FAVORITES_TABLES:
		return "TABLE";
	case T_FAVORITES_DIAGRAMS:
		return "DIAGRAM";
	case T_FAVORITES_QUERIES:
		return "QUERY";
	case T_FAVORITES_DATA_MANAGERS:
		return "DATAMAN";
	case T_FAVORITES_ACTIONS:
		return "ACTION";
	case T_FAVORITES_LDAP_DN:
		return "LDAP_DN";
	case T_FAVORITES_LDAP_CLASS:
		return "LDAP_CLASS";
	default:
		g_warning ("Unknown type of favorite");
	}

	return "";
}

static TFavoritesType
favorite_string_to_type (const gchar *str)
{
	if (*str == 'T')
		return T_FAVORITES_TABLES;
	else if (*str == 'D') {
		if (str[1] == 'I')
			return T_FAVORITES_DIAGRAMS;
		else
			return T_FAVORITES_DATA_MANAGERS;
	}
	else if (*str == 'Q')
		return T_FAVORITES_QUERIES;
	else if (*str == 'A')
		return T_FAVORITES_ACTIONS;
	else if (*str == 'L') {
		if (strlen (str) == 7)
			return T_FAVORITES_LDAP_DN;
		else
			return T_FAVORITES_LDAP_CLASS;
	}
	else
		g_warning ("Unknown type '%s' of favorite", str);
	return 0;
}

static gint
find_favorite_position (TFavorites *bfav, gint fav_id, gint order_key)
{
	GdaSqlBuilder *b;
	GdaStatement *stmt;
	GdaSet *params = NULL;
	GdaDataModel *model;
	gint pos = -1;

	g_return_val_if_fail (fav_id >= 0, -1);

	b = gda_sql_builder_new (GDA_SQL_STATEMENT_SELECT);
	gda_sql_builder_add_field_value_id (b,
					    gda_sql_builder_add_id (b, "rank"), 0);
	gda_sql_builder_select_add_target (b, FAVORDER_TABLE_NAME, NULL);
	gda_sql_builder_set_where (b,
				   gda_sql_builder_add_cond (b, GDA_SQL_OPERATOR_TYPE_AND,
							     gda_sql_builder_add_cond (b, GDA_SQL_OPERATOR_TYPE_EQ,
										       gda_sql_builder_add_id (b, "fav_id"),
										       gda_sql_builder_add_param (b, "favid", G_TYPE_INT, FALSE), 0),
							     gda_sql_builder_add_cond (b, GDA_SQL_OPERATOR_TYPE_EQ,
										       gda_sql_builder_add_id (b, "order_key"),
										       gda_sql_builder_add_param (b, "okey", G_TYPE_INT, FALSE), 0),
							     0));

	stmt = gda_sql_builder_get_statement (b, NULL);
	g_object_unref (G_OBJECT (b));
	if (!stmt)
		return -1;
	params = gda_set_new_inline (2,
				     "favid", G_TYPE_INT, fav_id,
				     "okey", G_TYPE_INT, order_key);
	model = gda_connection_statement_execute_select (bfav->priv->store_cnc, stmt, params, NULL);
	g_object_unref (stmt);
	g_object_unref (params);

	if (!model)
		return -1;

	gint nrows;
	nrows = gda_data_model_get_n_rows (model);
	if (nrows == 1) {
		const GValue *cvalue;
		cvalue = gda_data_model_get_value_at (model, 0, 0, NULL);
		if (cvalue)
			pos = g_value_get_int (cvalue);
	}

	g_object_unref (G_OBJECT (model));
	return pos;	
}

static gint
find_favorite_by_name (TFavorites *bfav, guint session_id, const gchar *name, TFavoritesType type,
		       TFavoritesAttributes *out_existing_fav, GError **error)
{
	GdaSqlBuilder *b;
	GdaStatement *stmt;
	GdaSet *params = NULL;
	GdaDataModel *model;
	gint favid = -1;

	if (out_existing_fav)
		memset (out_existing_fav, 0, sizeof (TFavoritesAttributes));
	g_return_val_if_fail (name, -1);

	b = gda_sql_builder_new (GDA_SQL_STATEMENT_SELECT);
	gda_sql_builder_add_field_value_id (b,
					    gda_sql_builder_add_id (b, "id"), 0);
	gda_sql_builder_add_field_value_id (b,
					    gda_sql_builder_add_id (b, "type"), 0);
	gda_sql_builder_add_field_value_id (b,
					    gda_sql_builder_add_id (b, "name"), 0);
	gda_sql_builder_add_field_value_id (b,
					    gda_sql_builder_add_id (b, "descr"), 0);
	gda_sql_builder_add_field_value_id (b,
					    gda_sql_builder_add_id (b, "contents"), 0);
	gda_sql_builder_select_add_target (b, FAVORITES_TABLE_NAME, NULL);
	gda_sql_builder_set_where (b,
				   gda_sql_builder_add_cond (b, GDA_SQL_OPERATOR_TYPE_AND,
							     gda_sql_builder_add_cond (b, GDA_SQL_OPERATOR_TYPE_EQ,
										       gda_sql_builder_add_id (b, "session"),
										       gda_sql_builder_add_param (b, "session", G_TYPE_INT, FALSE), 0),
							     gda_sql_builder_add_cond (b, GDA_SQL_OPERATOR_TYPE_EQ,
										       gda_sql_builder_add_id (b, "name"),
										       gda_sql_builder_add_param (b, "name", G_TYPE_INT, FALSE), 0),
							     gda_sql_builder_add_cond (b, GDA_SQL_OPERATOR_TYPE_EQ,
										       gda_sql_builder_add_id (b, "type"),
										       gda_sql_builder_add_param (b, "type", G_TYPE_STRING, FALSE), 0)));

 	stmt = gda_sql_builder_get_statement (b, error);
	g_object_unref (G_OBJECT (b));
	if (!stmt)
		return -1;
	params = gda_set_new_inline (3,
				     "session", G_TYPE_INT, session_id,
				     "type", G_TYPE_STRING, t_favorites_type_to_string (type),
				     "name", G_TYPE_STRING, name);
	model = gda_connection_statement_execute_select (bfav->priv->store_cnc, stmt, params, error);
	g_object_unref (stmt);
	g_object_unref (params);

	if (model && gda_data_model_get_n_rows (model) == 1) {
		const GValue *cvalue;
		cvalue = gda_data_model_get_value_at (model, 0, 0, NULL);
		if (cvalue && (G_VALUE_TYPE (cvalue) == G_TYPE_INT))
			favid = g_value_get_int (cvalue);

		if (out_existing_fav) {
			out_existing_fav->id = favid;
			cvalue = gda_data_model_get_value_at (model, 1, 0, error);
			if (cvalue)
				out_existing_fav->type = favorite_string_to_type (g_value_get_string (cvalue));
			cvalue = gda_data_model_get_value_at (model, 2, 0, error);
			if (cvalue && (G_VALUE_TYPE (cvalue) == G_TYPE_STRING))
				out_existing_fav->name = g_value_dup_string (cvalue);
			cvalue = gda_data_model_get_value_at (model, 3, 0, error);
			if (cvalue && (G_VALUE_TYPE (cvalue) == G_TYPE_STRING))
				out_existing_fav->descr = g_value_dup_string (cvalue);
			cvalue = gda_data_model_get_value_at (model, 4, 0, error);
			if (cvalue && (G_VALUE_TYPE (cvalue) == G_TYPE_STRING))
				out_existing_fav->contents = g_value_dup_string (cvalue);
		}
	}
	if (model)
		g_object_unref (model);
	return favid;
}

/*
 * Find a favorite ID from its ID or from its contents
 *
 * Returns: the ID or -1 if not found (and sets ERROR).
 *
 * if @out_existing_fav is not %NULL, then its attributes are set, use t_favorites_reset_attributes()
 * to free.
 */
static gint
find_favorite (TFavorites *bfav, guint session_id, gint id, const gchar *contents,
	       TFavoritesAttributes *out_existing_fav, GError **error)
{
	GdaSqlBuilder *b;
	GdaStatement *stmt;
	GdaSet *params = NULL;
	GdaDataModel *model;
	gint favid = -1;

	if (out_existing_fav)
		memset (out_existing_fav, 0, sizeof (TFavoritesAttributes));
	g_return_val_if_fail ((id >= 0) || contents, -1);

	b = gda_sql_builder_new (GDA_SQL_STATEMENT_SELECT);
	gda_sql_builder_add_field_value_id (b,
					    gda_sql_builder_add_id (b, "id"), 0);
	gda_sql_builder_add_field_value_id (b,
					    gda_sql_builder_add_id (b, "type"), 0);
	gda_sql_builder_add_field_value_id (b,
					    gda_sql_builder_add_id (b, "name"), 0);
	gda_sql_builder_add_field_value_id (b,
					    gda_sql_builder_add_id (b, "descr"), 0);
	gda_sql_builder_add_field_value_id (b,
					    gda_sql_builder_add_id (b, "contents"), 0);
	gda_sql_builder_select_add_target (b, FAVORITES_TABLE_NAME, NULL);

	if (id >= 0) {
		/* lookup from ID */
		gda_sql_builder_set_where (b,
					   gda_sql_builder_add_cond (b, GDA_SQL_OPERATOR_TYPE_EQ,
								     gda_sql_builder_add_id (b, "id"),
								     gda_sql_builder_add_param (b, "id", G_TYPE_INT, FALSE), 0));
	}
	else {
		/* lookup using session and contents */
		gda_sql_builder_set_where (b,
					   gda_sql_builder_add_cond (b, GDA_SQL_OPERATOR_TYPE_AND,
								     gda_sql_builder_add_cond (b, GDA_SQL_OPERATOR_TYPE_EQ,
											       gda_sql_builder_add_id (b, "session"),
											       gda_sql_builder_add_param (b, "session", G_TYPE_INT, FALSE), 0),
								     gda_sql_builder_add_cond (b, GDA_SQL_OPERATOR_TYPE_EQ,
											       gda_sql_builder_add_id (b, "contents"),
											       gda_sql_builder_add_param (b, "contents", G_TYPE_INT, FALSE), 0), 0));
	}
 	stmt = gda_sql_builder_get_statement (b, error);
	g_object_unref (G_OBJECT (b));
	if (!stmt)
		return -1;
	params = gda_set_new_inline (3,
				     "session", G_TYPE_INT, session_id,
				     "id", G_TYPE_INT, id,
				     "contents", G_TYPE_STRING, contents);
	model = gda_connection_statement_execute_select (bfav->priv->store_cnc, stmt, params, error);
	g_object_unref (stmt);
	g_object_unref (params);

	if (!model)
		return -1;

	gint nrows;
	nrows = gda_data_model_get_n_rows (model);
	if (nrows == 1) {
		const GValue *cvalue;
		cvalue = gda_data_model_get_value_at (model, 0, 0, error);
		if (cvalue)
			favid = g_value_get_int (cvalue);
		if (out_existing_fav) {
			out_existing_fav->id = favid;
			cvalue = gda_data_model_get_value_at (model, 1, 0, error);
			if (cvalue)
				out_existing_fav->type = favorite_string_to_type (g_value_get_string (cvalue));
			cvalue = gda_data_model_get_value_at (model, 2, 0, error);
			if (cvalue && (G_VALUE_TYPE (cvalue) == G_TYPE_STRING))
				out_existing_fav->name = g_value_dup_string (cvalue);
			cvalue = gda_data_model_get_value_at (model, 3, 0, error);
			if (cvalue && (G_VALUE_TYPE (cvalue) == G_TYPE_STRING))
				out_existing_fav->descr = g_value_dup_string (cvalue);
			cvalue = gda_data_model_get_value_at (model, 4, 0, error);
			if (cvalue && (G_VALUE_TYPE (cvalue) == G_TYPE_STRING))
				out_existing_fav->contents = g_value_dup_string (cvalue);
		}
	}

	g_object_unref (G_OBJECT (model));
	return favid;
}

/*
 * Reorders the favorites for @order_key, making sure @id is at position @new_pos
 */
static gboolean
favorites_reorder (TFavorites *bfav, gint order_key, gint id, gint new_pos, GError **error)
{
	GdaSqlBuilder *b;
	GdaStatement *stmt;
	GdaSet *params = NULL;
	GdaDataModel *model;

	g_assert (id >= 0);
	g_assert (order_key >= 0);

	b = gda_sql_builder_new (GDA_SQL_STATEMENT_SELECT);
	gda_sql_builder_add_field_value_id (b, gda_sql_builder_add_id (b, "fav_id"), 0);

	gda_sql_builder_select_add_target (b, FAVORDER_TABLE_NAME, NULL);
	
	gda_sql_builder_set_where (b, gda_sql_builder_add_cond (b, GDA_SQL_OPERATOR_TYPE_EQ,
								gda_sql_builder_add_id (b, "order_key"),
								gda_sql_builder_add_param (b, "orderkey", G_TYPE_INT, FALSE), 0));
	gda_sql_builder_select_order_by (b,
					 gda_sql_builder_add_id (b, "rank"), TRUE, NULL);
	stmt = gda_sql_builder_get_statement (b, error);
	g_object_unref (G_OBJECT (b));
	if (!stmt)
		return FALSE;
	params = gda_set_new_inline (3,
				     "orderkey", G_TYPE_INT, order_key,
				     "rank", G_TYPE_INT, 0,
				     "id", G_TYPE_INT, id);
	model = gda_connection_statement_execute_select (bfav->priv->store_cnc, stmt, params, error);
	g_object_unref (stmt);
	if (!model) {
		g_object_unref (params);
		return FALSE;
	}

	gint i, nrows;
	gboolean retval = TRUE;
	nrows = gda_data_model_get_n_rows (model);
	if (new_pos < 0)
		new_pos = 0;
	else if (new_pos > nrows)
		new_pos = nrows;

	b = gda_sql_builder_new (GDA_SQL_STATEMENT_UPDATE);
	gda_sql_builder_set_table (b, FAVORDER_TABLE_NAME);
	gda_sql_builder_add_field_value_id (b,
					    gda_sql_builder_add_id (b, "rank"),
					    gda_sql_builder_add_param (b, "rank", G_TYPE_INT, FALSE));
	const GdaSqlBuilderId id_cond1 = gda_sql_builder_add_cond (b, GDA_SQL_OPERATOR_TYPE_EQ,
								   gda_sql_builder_add_id (b, "fav_id"),
								   gda_sql_builder_add_param (b, "id", G_TYPE_INT, FALSE),
								   0);
	const GdaSqlBuilderId id_cond2 = gda_sql_builder_add_cond (b, GDA_SQL_OPERATOR_TYPE_EQ,
								   gda_sql_builder_add_id (b, "order_key"),
								   gda_sql_builder_add_param (b, "orderkey", G_TYPE_INT, FALSE),
								   0);
	gda_sql_builder_set_where (b, gda_sql_builder_add_cond (b, GDA_SQL_OPERATOR_TYPE_AND, id_cond1, id_cond2, 0));
	stmt = gda_sql_builder_get_statement (b, error);
	if (!stmt) {
		retval = FALSE;
		goto out;
	}

	/* reodering the rows */
	for (i = 0; i < nrows; i++) {
		const GValue *v;
		v = gda_data_model_get_value_at (model, 0, i, error);
		if (v) {
			g_assert (gda_holder_set_value (gda_set_get_holder (params, "id"), v, NULL));
			if (g_value_get_int (v) == id)
				g_assert (gda_set_set_holder_value (params, NULL, "rank", new_pos));
			else
				g_assert (gda_set_set_holder_value (params, NULL, "rank", i < new_pos ? i : i + 1));
			if (gda_connection_statement_execute_non_select (bfav->priv->store_cnc, stmt,
									 params, NULL, error) == -1) {
				retval = FALSE;
				goto out;
			}
		}
		else {
			retval = FALSE;
			goto out;
		}
	}

 out:
	g_object_unref (b);
	g_object_unref (params);
	g_object_unref (model);
	if (stmt)
		g_object_unref (stmt);
	return retval;
}

/**
 * t_favorites_add
 * @bfav: a #TFavorites object
 * @session_id: session ID (0)
 * @fav: a pointer to a #TFavoritesAttributes structure
 * @order_key: ordering key or -1 for none
 * @pos: position (ignored if @order_key < 0)
 * @error: a place to store errors, or %NULL
 *
 * Add a new favorite, or replace an existing one.
 * NOTE:
 * <itemizedlist>
 *   <listitem><para>if @fav->id is < 0 then it's either an update or an insert (depending if fav->contents exists)
 *     and if it's not it is an INSERT </para></listitem>
 *   <listitem><para>@fav->type can't be 0</para></listitem>
 *   <listitem><para>@fav->contents can't be %NULL</para></listitem>
 * </itemizedlist>
 *
 * On success @fav->id contains the favorite's ID, otherwise it will contain -1.
 *
 * if @order_key is negative, then no ordering is done and @pos is ignored.
 */
gboolean
t_favorites_add (TFavorites *bfav, guint session_id,
		     TFavoritesAttributes *fav,
		     gint order_key, gint pos,
		     GError **error)
{
	GdaConnection *store_cnc;
	GdaSet *params = NULL;
	gint favid = -1;
	TFavoritesAttributes efav; /* existing favorite */

	g_return_val_if_fail (T_IS_FAVORITES (bfav), FALSE);
	g_return_val_if_fail (fav, FALSE);
	g_return_val_if_fail (fav->contents, FALSE);

	if (! meta_store_addons_init (bfav, error))
		return FALSE;

	store_cnc = bfav->priv->store_cnc;
	if (! gda_lockable_trylock (GDA_LOCKABLE (store_cnc))) {
		g_set_error (error, T_ERROR, T_STORED_DATA_ERROR,
			     "%s", _("Can't initialize transaction to access favorites"));
		return FALSE;
	}
	/* begin a transaction */
	if (! gda_connection_begin_transaction (store_cnc, NULL, GDA_TRANSACTION_ISOLATION_UNKNOWN, NULL)) {
		g_set_error (error, T_ERROR, T_STORED_DATA_ERROR,
			     "%s", _("Can't initialize transaction to access favorites"));
		gda_lockable_unlock (GDA_LOCKABLE (store_cnc));
                return FALSE;
	}

	gint rtype;
	favid = find_favorite (bfav, session_id, fav->id, fav->contents, &efav, NULL);
	rtype = fav->type;
	if (efav.type)
		rtype = efav.type;
	if ((favid != -1) && (pos == G_MAXINT)) {
		/* find current position */
		pos = find_favorite_position (bfav, favid, order_key);
	}
	params = gda_set_new_inline (8,
				     "session", G_TYPE_INT, session_id,
				     "id", G_TYPE_INT, fav->id,
				     "type", G_TYPE_STRING, t_favorites_type_to_string (rtype),
				     "name", G_TYPE_STRING, fav->name ? fav->name : efav.name,
				     "contents", G_TYPE_STRING, fav->contents,
				     "rank", G_TYPE_INT, pos,
				     "orderkey", G_TYPE_INT, order_key,
				     "descr", G_TYPE_STRING, fav->descr ? fav->descr : efav.descr);

	if (favid == -1) {
		/* insert a favorite */
		GdaSqlBuilder *builder;
		GdaStatement *stmt;

		g_return_val_if_fail (fav->type, FALSE);
		builder = gda_sql_builder_new (GDA_SQL_STATEMENT_INSERT);
		gda_sql_builder_set_table (builder, FAVORITES_TABLE_NAME);

		gda_sql_builder_add_field_value_id (builder,
						    gda_sql_builder_add_id (builder, "session"),
						    gda_sql_builder_add_param (builder, "session", G_TYPE_INT, FALSE));
		gda_sql_builder_add_field_value_id (builder,
						    gda_sql_builder_add_id (builder, "type"),
						    gda_sql_builder_add_param (builder, "type", G_TYPE_INT, FALSE));
		gda_sql_builder_add_field_value_id (builder,
						    gda_sql_builder_add_id (builder, "name"),
						    gda_sql_builder_add_param (builder, "name", G_TYPE_STRING, TRUE));
		gda_sql_builder_add_field_value_id (builder,
						    gda_sql_builder_add_id (builder, "contents"),
						    gda_sql_builder_add_param (builder, "contents", G_TYPE_STRING, FALSE));
		gda_sql_builder_add_field_value_id (builder,
						    gda_sql_builder_add_id (builder, "descr"),
						    gda_sql_builder_add_param (builder, "descr", G_TYPE_STRING, TRUE));
		stmt = gda_sql_builder_get_statement (builder, error);
		g_object_unref (G_OBJECT (builder));
		if (!stmt)
			goto err;
		if (gda_connection_statement_execute_non_select (store_cnc, stmt, params, NULL, error) == -1) {
			g_object_unref (stmt);
			goto err;
		}
		g_object_unref (stmt);

		favid = find_favorite (bfav, session_id, fav->id, fav->contents, &efav, NULL);
		fav->id = favid;
	}
	else {
		/* update favorite's contents */
		GdaSqlBuilder *builder;
		GdaStatement *stmt;

		gda_set_set_holder_value (params, NULL, "id", favid);
		builder = gda_sql_builder_new (GDA_SQL_STATEMENT_UPDATE);
		gda_sql_builder_set_table (builder, FAVORITES_TABLE_NAME);

		gda_sql_builder_add_field_value_id (builder,
						    gda_sql_builder_add_id (builder, "name"),
						    gda_sql_builder_add_param (builder, "name", G_TYPE_STRING, TRUE));
		gda_sql_builder_add_field_value_id (builder,
						    gda_sql_builder_add_id (builder, "contents"),
						    gda_sql_builder_add_param (builder, "contents", G_TYPE_STRING, FALSE));
		gda_sql_builder_add_field_value_id (builder,
						    gda_sql_builder_add_id (builder, "descr"),
						    gda_sql_builder_add_param (builder, "descr", G_TYPE_STRING, TRUE));

		gda_sql_builder_set_where (builder,
					   gda_sql_builder_add_cond (builder, GDA_SQL_OPERATOR_TYPE_EQ,
								     gda_sql_builder_add_id (builder, "id"),
								     gda_sql_builder_add_param (builder, "id", G_TYPE_INT, FALSE),
								     0));
		if (fav->id == favid) {
			/* alter name and description only if fav->id was OK */
			gda_sql_builder_add_field_value_id (builder,
							    gda_sql_builder_add_id (builder, "name"),
							    gda_sql_builder_add_param (builder, "name", G_TYPE_STRING,
										       TRUE));
			gda_sql_builder_add_field_value_id (builder,
							    gda_sql_builder_add_id (builder, "descr"),
							    gda_sql_builder_add_param (builder, "descr", G_TYPE_STRING,
										       TRUE));
		}

		stmt = gda_sql_builder_get_statement (builder, error);
		g_object_unref (G_OBJECT (builder));
		if (!stmt)
			goto err;
		if (gda_connection_statement_execute_non_select (store_cnc, stmt, params, NULL, error) == -1) {
			g_object_unref (stmt);
			goto err;
		}
		g_object_unref (stmt);
		fav->id = favid;
	}
	t_favorites_reset_attributes (&efav);

	if (order_key >= 0) {
		GdaSqlBuilder *builder;
		GdaStatement *stmt;

		/* delete and insert favorite in orders table */
		favid = find_favorite (bfav, session_id, fav->id, fav->contents, NULL, error);
		if (favid < 0) {
			g_warning ("Could not identify favorite by its ID, make sure it's correct");
			goto err;
		}
		
		gda_set_set_holder_value (params, NULL, "id", favid);

		builder = gda_sql_builder_new (GDA_SQL_STATEMENT_DELETE);
		gda_sql_builder_set_table (builder, FAVORDER_TABLE_NAME);
		gda_sql_builder_set_where (builder,
					   gda_sql_builder_add_cond (builder, GDA_SQL_OPERATOR_TYPE_AND,
								     gda_sql_builder_add_cond (builder, GDA_SQL_OPERATOR_TYPE_EQ,
											       gda_sql_builder_add_id (builder, "fav_id"),
											       gda_sql_builder_add_param (builder, "id", G_TYPE_INT, FALSE),
											       0),
								     gda_sql_builder_add_cond (builder, GDA_SQL_OPERATOR_TYPE_EQ,
											       gda_sql_builder_add_id (builder, "order_key"),
											       gda_sql_builder_add_param (builder, "orderkey", G_TYPE_INT, FALSE),
											       0), 0));
		stmt = gda_sql_builder_get_statement (builder, error);
		g_object_unref (G_OBJECT (builder));
		if (!stmt)
			goto err;
		if (gda_connection_statement_execute_non_select (store_cnc, stmt, params, NULL, error) == -1) {
			g_object_unref (stmt);
			goto err;
		}
		g_object_unref (stmt);

		builder = gda_sql_builder_new (GDA_SQL_STATEMENT_INSERT);
		gda_sql_builder_set_table (builder, FAVORDER_TABLE_NAME);
		gda_sql_builder_add_field_value_id (builder,
						    gda_sql_builder_add_id (builder, "fav_id"),
						    gda_sql_builder_add_param (builder, "id", G_TYPE_INT, FALSE));
		gda_sql_builder_add_field_value_id (builder,
						    gda_sql_builder_add_id (builder, "rank"),
						    gda_sql_builder_add_param (builder, "rank", G_TYPE_INT, FALSE));
		gda_sql_builder_add_field_value_id (builder,
						    gda_sql_builder_add_id (builder, "order_key"),
						    gda_sql_builder_add_param (builder, "orderkey", G_TYPE_STRING, TRUE));
		stmt = gda_sql_builder_get_statement (builder, error);
		g_object_unref (G_OBJECT (builder));
		if (!stmt)
			goto err;
		if (gda_connection_statement_execute_non_select (store_cnc, stmt, params, NULL, error) == -1) {
			g_object_unref (stmt);
			goto err;
		}
		g_object_unref (stmt);

		/* reorder */
		if (!favorites_reorder (bfav, order_key, favid, pos, error))
			goto err;
	}

	if (! gda_connection_commit_transaction (store_cnc, NULL, NULL)) {
		g_set_error (error, T_ERROR, T_STORED_DATA_ERROR,
			     "%s", _("Can't commit transaction to access favorites"));
		goto err;
	}

	if (params)
		g_object_unref (params);
	gda_lockable_unlock (GDA_LOCKABLE (store_cnc));
	g_signal_emit (bfav, t_favorites_signals [FAV_CHANGED],
		       g_quark_from_string (t_favorites_type_to_string (rtype)));
	return TRUE;

 err:
	if (params)
		g_object_unref (params);
	gda_lockable_unlock (GDA_LOCKABLE (store_cnc));
	gda_connection_rollback_transaction (store_cnc, NULL, NULL);
	return FALSE;
}

/**
 * t_favorites_free_list
 * @fav_list: a list of #TFavoritesAttributes
 *
 * Frees all the #TFavoritesAttributes of the @fav_list list, and frees the list
 * itself.
 */
void
t_favorites_free_list (GSList *fav_list)
{
	GSList *list;
	if (!fav_list)
		return;
	for (list = fav_list; list; list = list->next) {
		TFavoritesAttributes *fav = (TFavoritesAttributes*) list->data;
		t_favorites_reset_attributes (fav);
		g_free (fav);
	}
	g_slist_free (fav_list);
}

/**
 * t_favorites_reset_attributes
 * @fav: a pointer to a #TFavoritesAttributes
 *
 * Resets @fav with empty attributes; it does not free @fav.
 */
void
t_favorites_reset_attributes (TFavoritesAttributes *fav)
{
	g_free (fav->name);
	g_free (fav->descr);
	g_free (fav->contents);
	memset (fav, 0, sizeof (TFavoritesAttributes));
}

/**
 * t_favorites_list
 * @bfav: a #TFavorites
 * @session_id: 0 for now
 * @type: filter the type of attributes to be listed
 * @order_key: a key to order the listed favorites, such as #ORDER_KEY_SCHEMA
 * @error: a place to store errors, or %NULL
 *
 * Extract some favorites.
 *
 * Returns: a new list of #TFavoritesAttributes pointers. The list has to
 *          be freed using t_favorites_free_list()
 */
GSList *
t_favorites_list (TFavorites *bfav, guint session_id, TFavoritesType type,
		      gint order_key, GError **error)
{
	GdaSqlBuilder *b;
	GdaSet *params = NULL;
	GdaStatement *stmt;
	GdaSqlBuilderId t1, t2;
	GdaDataModel *model = NULL;
	GSList *fav_list = NULL;

	guint and_cond_ids [3];
	int and_cond_size = 0;
	guint or_cond_ids [T_FAVORITES_NB_TYPES];
	int or_cond_size = 0;

	g_return_val_if_fail (T_IS_FAVORITES (bfav), NULL);
	g_return_val_if_fail ((type != 0) || (order_key >= 0), NULL);

	if (! meta_store_addons_init (bfav, error))
		return NULL;

	b = gda_sql_builder_new (GDA_SQL_STATEMENT_SELECT);
	gda_sql_builder_add_field_value_id (b,
					    gda_sql_builder_add_id (b, "fav.contents"), 0);
	gda_sql_builder_add_field_value_id (b,
					    gda_sql_builder_add_id (b, "fav.descr"), 0);
	gda_sql_builder_add_field_value_id (b,
					    gda_sql_builder_add_id (b, "fav.name"), 0);
	gda_sql_builder_add_field_value_id (b,
					    gda_sql_builder_add_id (b, "fav.type"), 0);
	gda_sql_builder_add_field_value_id (b,
					    gda_sql_builder_add_id (b, "fav.id"), 0);

	t1 = gda_sql_builder_select_add_target (b, FAVORITES_TABLE_NAME, "fav");
	if (order_key > 0) {
		t2 = gda_sql_builder_select_add_target (b, FAVORDER_TABLE_NAME, "o");
		gda_sql_builder_select_join_targets (b, t1, t2, GDA_SQL_SELECT_JOIN_LEFT,
						     gda_sql_builder_add_cond (b, GDA_SQL_OPERATOR_TYPE_EQ,
									       gda_sql_builder_add_id (b, "fav.id"),
									       gda_sql_builder_add_id (b, "o.fav_id"),
									       0));
		gda_sql_builder_select_order_by (b,
						 gda_sql_builder_add_id (b, "o.rank"), TRUE, NULL);

		and_cond_ids [and_cond_size] = gda_sql_builder_add_cond (b, GDA_SQL_OPERATOR_TYPE_EQ,
									 gda_sql_builder_add_id (b, "o.order_key"),
									 gda_sql_builder_add_param (b, "okey", G_TYPE_INT, TRUE),
									 0);
		and_cond_size++;
	}

	and_cond_ids [and_cond_size] = gda_sql_builder_add_cond (b, GDA_SQL_OPERATOR_TYPE_EQ,
								 gda_sql_builder_add_id (b, "fav.session"),
								 gda_sql_builder_add_param (b, "session", G_TYPE_INT, FALSE), 0);
	and_cond_size++;

	gint i;
	gint flag;
	for (i = 0, flag = 1; i < T_FAVORITES_NB_TYPES; i++, flag <<= 1) {
		if (type & flag) {
			gchar *str;
			str = g_strdup_printf ("'%s'", t_favorites_type_to_string (flag));
			or_cond_ids [or_cond_size] = gda_sql_builder_add_cond (b, GDA_SQL_OPERATOR_TYPE_EQ,
									       gda_sql_builder_add_id (b, "fav.type"),
									       gda_sql_builder_add_id (b, str),
									       0);
			g_free (str);
			or_cond_size++;
		}
	}
	if (or_cond_size >= 1) {
		and_cond_ids [and_cond_size] = gda_sql_builder_add_cond_v (b, GDA_SQL_OPERATOR_TYPE_OR,
									   or_cond_ids, or_cond_size);
		and_cond_size++;
	}

	gda_sql_builder_set_where (b,
				   gda_sql_builder_add_cond_v (b, GDA_SQL_OPERATOR_TYPE_AND, and_cond_ids, and_cond_size));
#ifdef GDA_DEBUG_NO
	{
		GdaSqlStatement *sqlst;
		sqlst = gda_sql_builder_get_sql_statement (b);
                
		g_print ("=>%s\n", gda_sql_statement_serialize (sqlst));
	}
#endif

	stmt = gda_sql_builder_get_statement (b, error);
	g_object_unref (G_OBJECT (b));
	if (!stmt)
		goto out;

#ifdef GDA_DEBUG_NO
	{
		g_print ("=>%s\n", gda_statement_to_sql (stmt, NULL, NULL));
	}
#endif


	params = gda_set_new_inline (2,
				     "session", G_TYPE_INT, session_id,
				     "okey", G_TYPE_INT, order_key);

	model = gda_connection_statement_execute_select (bfav->priv->store_cnc, stmt, params, error);
	g_object_unref (stmt);
	if (!model) {
		g_warning ("Malformed dictionary database, cannot get favorites list (this should happen only while in dev.).");
		goto out;
	}

	gint nrows;
	nrows = gda_data_model_get_n_rows (model);
	for (i = 0; i < nrows; i++) {
		const GValue *contents, *descr = NULL, *name = NULL, *type = NULL, *id = NULL;

		contents = gda_data_model_get_value_at (model, 0, i, error);
		if (contents)
			descr = gda_data_model_get_value_at (model, 1, i, error);
		if (descr)
			name = gda_data_model_get_value_at (model, 2, i, error);
		if (name)
			type = gda_data_model_get_value_at (model, 3, i, error);
		if (type)
			id = gda_data_model_get_value_at (model, 4, i, error);
		if (id) {
			TFavoritesAttributes *fav;
			fav = g_new0 (TFavoritesAttributes, 1);
			fav->id = g_value_get_int (id);
			fav->type = favorite_string_to_type (g_value_get_string (type));
			if (G_VALUE_TYPE (descr) == G_TYPE_STRING)
				fav->descr = g_value_dup_string (descr);
			if (G_VALUE_TYPE (name) == G_TYPE_STRING)
				fav->name = g_value_dup_string (name);
			fav->contents = g_value_dup_string (contents);
			fav_list = g_slist_prepend (fav_list, fav);
		}
		else {
			t_favorites_free_list (fav_list);
			fav_list = NULL;
			goto out;
		}
	}

 out:
	if (params)
		g_object_unref (G_OBJECT (params));
	if (model)
		g_object_unref (G_OBJECT (model));

	return g_slist_reverse (fav_list);
}


/**
 * t_favorites_delete
 * @bfav: a #TFavorites
 * @session_id: 0 for now
 * @fav: a pointer to a #TFavoritesAttributes definning which favorite to delete
 * @error: a place to store errors, or %NULL
 *
 * Delete a favorite
 *
 * Returns: %TRUE if no error occurred.
 */
gboolean
t_favorites_delete (TFavorites *bfav, guint session_id,
			TFavoritesAttributes *fav, GError **error)
{
	GdaSqlBuilder *b;
	GdaSet *params = NULL;
	GdaStatement *stmt;
	gboolean retval = FALSE;
	gint favid = -1;
	TFavoritesAttributes efav;

	g_return_val_if_fail (T_IS_FAVORITES (bfav), FALSE);
	g_return_val_if_fail (fav, FALSE);
	g_return_val_if_fail ((fav->id >= 0) || fav->contents || fav->name , FALSE);
	
	memset (&efav, 0, sizeof (TFavoritesAttributes));
	if (! meta_store_addons_init (bfav, error))
		return FALSE;

	if (! gda_lockable_trylock (GDA_LOCKABLE (bfav->priv->store_cnc))) {
		g_set_error (error, T_ERROR, T_STORED_DATA_ERROR,
			     "%s", _("Can't initialize transaction to access favorites"));
		return FALSE;
	}
	/* begin a transaction */
	if (! gda_connection_begin_transaction (bfav->priv->store_cnc, NULL,
						GDA_TRANSACTION_ISOLATION_UNKNOWN, NULL)) {
		g_set_error (error, T_ERROR, T_STORED_DATA_ERROR,
			     "%s", _("Can't initialize transaction to access favorites"));
		gda_lockable_unlock (GDA_LOCKABLE (bfav->priv->store_cnc));
                return FALSE;
	}

	GError *lerror = NULL;
	if ((fav->id >= 0) || fav->contents) {
		favid = find_favorite (bfav, session_id, fav->id, fav->contents, &efav, &lerror);
		if (lerror) {
			g_propagate_error (error, lerror);
			goto out;
		}
	}
	if ((favid < 0) && fav->name) {
		favid = find_favorite_by_name (bfav, session_id, fav->name, fav->type,
					       &efav, &lerror);
		if (lerror) {
			g_propagate_error (error, lerror);
			goto out;
		}
	}
	if (favid < 0) {
		g_set_error (error, T_ERROR, T_INTERNAL_COMMAND_ERROR,
			     "%s", _("Could not find favorite"));
		goto out;
	}

	/* remove entry from favorites' list */
	b = gda_sql_builder_new (GDA_SQL_STATEMENT_DELETE);
	gda_sql_builder_set_table (b, FAVORITES_TABLE_NAME);
	gda_sql_builder_set_where (b, gda_sql_builder_add_cond (b, GDA_SQL_OPERATOR_TYPE_EQ,
								gda_sql_builder_add_id (b, "id"),
								gda_sql_builder_add_param (b, "id", G_TYPE_INT, FALSE),
								0));

	stmt = gda_sql_builder_get_statement (b, error);
	g_object_unref (G_OBJECT (b));
	if (!stmt)
		goto out;

	params = gda_set_new_inline (1, "id", G_TYPE_INT, favid);

	if (gda_connection_statement_execute_non_select (bfav->priv->store_cnc, stmt, params, NULL, error) == -1) {
		g_object_unref (stmt);
		goto out;
	}
	g_object_unref (stmt);

	/* remove entry from favorites' order */
	b = gda_sql_builder_new (GDA_SQL_STATEMENT_DELETE);
	gda_sql_builder_set_table (b, FAVORDER_TABLE_NAME);
	gda_sql_builder_set_where (b, gda_sql_builder_add_cond (b, GDA_SQL_OPERATOR_TYPE_EQ,
								gda_sql_builder_add_id (b, "fav_id"),
								gda_sql_builder_add_param (b, "id", G_TYPE_INT, FALSE),
								0));

	stmt = gda_sql_builder_get_statement (b, error);
	g_object_unref (G_OBJECT (b));
	if (!stmt)
		goto out;
	if (gda_connection_statement_execute_non_select (bfav->priv->store_cnc, stmt, params, NULL, error) == -1) {
		g_object_unref (stmt);
		goto out;
	}
	g_object_unref (stmt);

	if (! gda_connection_commit_transaction (bfav->priv->store_cnc, NULL, NULL)) {
		g_set_error (error, T_ERROR, T_STORED_DATA_ERROR,
			     "%s", _("Can't commit transaction to access favorites"));
		goto out;
	}
	retval = TRUE;

 out:
	if (!retval)
		gda_connection_rollback_transaction (bfav->priv->store_cnc, NULL, NULL);

	gda_lockable_unlock (GDA_LOCKABLE (bfav->priv->store_cnc));
	if (retval)
		g_signal_emit (bfav, t_favorites_signals [FAV_CHANGED],
			       g_quark_from_string (t_favorites_type_to_string (efav.type)));
	t_favorites_reset_attributes (&efav);
	if (params)
		g_object_unref (G_OBJECT (params));

	return retval;
}

/**
 * t_favorites_find
 * @bfav: a #TFavorites
 * @session_id: 0 for now
 * @contents: the favorite's contents
 * @out_fav: (nullable): a #TFavoritesAttributes to be filled with the favorite's attributes, or %NULL
 * @error: a place to store errors, or %NULL
 *
 * Get all the information about a favorite from its id: fills the @out_fav
 * pointed structure. Use t_favorites_reset_attributes() to reset @out_fav's contents.
 *
 * Retuns: the requested's favorite ID, or -1 if not found
 */
gint
t_favorites_find (TFavorites *bfav, guint session_id, const gchar *contents,
		      TFavoritesAttributes *out_fav, GError **error)
{
	g_return_val_if_fail (T_IS_FAVORITES (bfav), -1);
	g_return_val_if_fail (contents, -1);

	if (! meta_store_addons_init (bfav, error))
		return -1;
	return find_favorite (bfav, session_id, -1, contents, out_fav, error);
}

/**
 * t_favorites_find_by_name:
 * @bfav: a #TFavorites
 * @session_id: 0 for now
 * @type: the favorite's type
 * @name: the favorite's name
 * @out_fav: (nullable): a #TFavoritesAttributes to be filled with the favorite's attributes, or %NULL
 * @error: a place to store errors, or %NULL
 *
 * Get all the information about a favorite from its id: fills the @out_fav
 * pointed structure. Use t_favorites_reset_attributes() to reset @out_fav's contents.
 *
 * Retuns: the requested's favorite ID, or -1 if not found
 */
gint
t_favorites_find_by_name (TFavorites *bfav, guint session_id, TFavoritesType type, const gchar *name,
			      TFavoritesAttributes *out_fav, GError **error)
{
	g_return_val_if_fail (T_IS_FAVORITES (bfav), -1);
	g_return_val_if_fail (name, -1);

	if (! meta_store_addons_init (bfav, error))
		return -1;
	return find_favorite_by_name (bfav, session_id, name, type, out_fav, error);
}



/**
 * t_favorites_get
 * @bfav: a #TFavorites
 * @fav_id: the favorite's ID
 * @out_fav: a #TFavoritesAttributes to be filled with the favorite's attributes
 * @error: a place to store errors, or %NULL
 *
 * Get all the information about a favorite from its id: fills the @out_fav
 * pointed structure. Use t_favorites_reset_attributes() to reset @out_fav's contents.
 *
 * Retuns: %TRUE if no error occurred.
 */
gboolean
t_favorites_get (TFavorites *bfav, gint fav_id,
		     TFavoritesAttributes *out_fav, GError **error)
{
	GdaSqlBuilder *b;
	GdaStatement *stmt;
	GdaSet *params = NULL;
	GdaDataModel *model;
	gboolean retval = FALSE;

	g_return_val_if_fail (T_IS_FAVORITES (bfav), FALSE);
	g_return_val_if_fail (out_fav, FALSE);
	g_return_val_if_fail (fav_id >= 0, FALSE);

	memset (out_fav, 0, sizeof (TFavoritesAttributes));

	b = gda_sql_builder_new (GDA_SQL_STATEMENT_SELECT);
	gda_sql_builder_add_field_value_id (b,
					    gda_sql_builder_add_id (b, "id"), 0);
	gda_sql_builder_add_field_value_id (b,
					    gda_sql_builder_add_id (b, "type"), 0);
	gda_sql_builder_add_field_value_id (b,
					    gda_sql_builder_add_id (b, "name"), 0);
	gda_sql_builder_add_field_value_id (b,
					    gda_sql_builder_add_id (b, "descr"), 0);
	gda_sql_builder_add_field_value_id (b,
					    gda_sql_builder_add_id (b, "contents"), 0);
	gda_sql_builder_select_add_target (b, FAVORITES_TABLE_NAME, NULL);

	gda_sql_builder_set_where (b,
				   gda_sql_builder_add_cond (b, GDA_SQL_OPERATOR_TYPE_EQ,
							     gda_sql_builder_add_id (b, "id"),
							     gda_sql_builder_add_param (b, "id", G_TYPE_INT, FALSE), 0));
	stmt = gda_sql_builder_get_statement (b, error);
	g_object_unref (G_OBJECT (b));
	if (!stmt)
		return FALSE;
	params = gda_set_new_inline (1,
				     "id", G_TYPE_INT, fav_id);
	model = gda_connection_statement_execute_select (bfav->priv->store_cnc, stmt, params, error);
	g_object_unref (stmt);
	g_object_unref (params);

	if (!model)
		return FALSE;

	gint nrows;
	nrows = gda_data_model_get_n_rows (model);
	if (nrows == 1) {
		gint i;
		const GValue *cvalues[5];
		for (i = 0; i < 5; i++) {
			cvalues [i] = gda_data_model_get_value_at (model, i, 0, error);
			if (!cvalues [i])
				goto out;
		}

		out_fav->id = g_value_get_int (cvalues [0]);
		out_fav->type = favorite_string_to_type (g_value_get_string (cvalues [1]));
		if (G_VALUE_TYPE (cvalues [2]) == G_TYPE_STRING)
			out_fav->name = g_value_dup_string (cvalues [2]);
		if (G_VALUE_TYPE (cvalues [3]) == G_TYPE_STRING)
			out_fav->descr = g_value_dup_string (cvalues [3]);
		out_fav->contents = g_value_dup_string (cvalues [4]);
		retval = TRUE;
	}
	
 out:
	g_object_unref (G_OBJECT (model));
	return retval;
}
