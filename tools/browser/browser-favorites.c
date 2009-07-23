/*
 * Copyright (C) 2009 Vivien Malerba
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
#include "browser-favorites.h"
#include <libgda/thread-wrapper/gda-thread-wrapper.h>
#include "support.h"
#include "marshal.h"
#include <libgda/gda-sql-builder.h>

struct _BrowserFavoritesPrivate {
	GdaMetaStore  *store;
	GdaConnection *store_cnc;
};


/*
 * Main static functions
 */
static void browser_favorites_class_init (BrowserFavoritesClass *klass);
static void browser_favorites_init (BrowserFavorites *bfav);
static void browser_favorites_dispose (GObject *object);

/* get a pointer to the parents to be able to call their destructor */
static GObjectClass  *parent_class = NULL;

/* signals */
enum {
	FAV_CHANGED,
	LAST_SIGNAL
};

gint browser_favorites_signals [LAST_SIGNAL] = { 0 };

GType
browser_favorites_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static GStaticMutex registering = G_STATIC_MUTEX_INIT;
		static const GTypeInfo info = {
			sizeof (BrowserFavoritesClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) browser_favorites_class_init,
			NULL,
			NULL,
			sizeof (BrowserFavorites),
			0,
			(GInstanceInitFunc) browser_favorites_init
		};


		g_static_mutex_lock (&registering);
		if (type == 0)
			type = g_type_register_static (G_TYPE_OBJECT, "BrowserFavorites", &info, 0);
		g_static_mutex_unlock (&registering);
	}
	return type;
}

static void
browser_favorites_class_init (BrowserFavoritesClass *klass)
{
	GObjectClass   *object_class = G_OBJECT_CLASS (klass);
	parent_class = g_type_class_peek_parent (klass);

	browser_favorites_signals [FAV_CHANGED] =
		g_signal_new ("favorites-changed",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_FIRST | G_SIGNAL_DETAILED,
                              G_STRUCT_OFFSET (BrowserFavoritesClass, favorites_changed),
                              NULL, NULL,
                              g_cclosure_marshal_VOID__VOID, G_TYPE_NONE,
                              0);

	klass->favorites_changed = NULL;

	object_class->dispose = browser_favorites_dispose;
}

static void
browser_favorites_init (BrowserFavorites *bfav)
{
	bfav->priv = g_new0 (BrowserFavoritesPrivate, 1);
	bfav->priv->store = NULL;
	bfav->priv->store_cnc = NULL;
}

static void
browser_favorites_dispose (GObject *object)
{
	BrowserFavorites *bfav;

	g_return_if_fail (object != NULL);
	g_return_if_fail (BROWSER_IS_FAVORITES (object));

	bfav = BROWSER_FAVORITES (object);
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
 * browser_favorites_new
 *
 * Creates a new #BrowserFavorites object
 *
 * Returns: the new object
 */
BrowserFavorites*
browser_favorites_new (GdaMetaStore *store)
{
	BrowserFavorites *bfav;

	g_return_val_if_fail (GDA_IS_META_STORE (store), NULL);

	bfav = BROWSER_FAVORITES (g_object_new (BROWSER_TYPE_FAVORITES, NULL));
	bfav->priv->store = g_object_ref (store);

	return bfav;
}

#define FAVORITES_TABLE_NAME "gda_sql_favorites"
#define FAVORITES_TABLE_DESC \
        "<table name=\"" FAVORITES_TABLE_NAME "\"> "                            \
        "   <column name=\"id\" type=\"gint\" pkey=\"TRUE\" autoinc=\"TRUE\"/>"             \
        "   <column name=\"session\" type=\"gint\"/>"             \
        "   <column name=\"type\"/>"                              \
        "   <column name=\"name\" nullok=\"TRUE\"/>"                              \
        "   <column name=\"contents\"/>"                          \
        "   <column name=\"descr\" nullok=\"TRUE\"/>"                           \
        "   <unique>"                             \
        "     <column name=\"session\"/>"                             \
        "     <column name=\"type\"/>"                             \
        "     <column name=\"contents\"/>"                             \
        "   </unique>"                             \
        "</table>"
#define FAVORDER_TABLE_NAME "gda_sql_favorder"
#define FAVORDER_TABLE_DESC \
        "<table name=\"" FAVORDER_TABLE_NAME "\"> "                            \
        "   <column name=\"order_key\" type=\"gint\" pkey=\"TRUE\"/>"          \
        "   <column name=\"fav_id\" type=\"gint\" pkey=\"TRUE\"/>"             \
        "   <column name=\"rank\" type=\"gint\"/>"             \
        "</table>"

static gboolean
meta_store_addons_init (BrowserFavorites *bfav, GError **error)
{
	GError *lerror = NULL;

	if (!gda_meta_store_schema_add_custom_object (bfav->priv->store, FAVORITES_TABLE_DESC, &lerror)) {
                g_set_error (error, 0, 0, "%s",
                             _("Can't initialize dictionary to store favorites"));
		g_warning ("Can't initialize dictionary to store favorites :%s",
			   lerror && lerror->message ? lerror->message : "No detail");
		if (lerror)
			g_error_free (lerror);
                return FALSE;
        }
	if (!gda_meta_store_schema_add_custom_object (bfav->priv->store, FAVORDER_TABLE_DESC, &lerror)) {
                g_set_error (error, 0, 0, "%s",
                             _("Can't initialize dictionary to store favorites"));
		g_warning ("Can't initialize dictionary to store favorites :%s",
			   lerror && lerror->message ? lerror->message : "No detail");
		if (lerror)
			g_error_free (lerror);
                return FALSE;
        }

	bfav->priv->store_cnc = g_object_ref (gda_meta_store_get_internal_connection (bfav->priv->store));
	return TRUE;
}

static const gchar *
favorite_type_to_string (BrowserFavoritesType type)
{
	switch (type) {
	case BROWSER_FAVORITES_TABLES:
		return "TABLE";
	case BROWSER_FAVORITES_DIAGRAMS:
		return "DIAGRAM";
	default:
		g_warning ("Unknow type of favorite");
	}

	return "";
}

static BrowserFavoritesType
favorite_string_to_type (const gchar *str)
{
	if (*str == 'T')
		return BROWSER_FAVORITES_TABLES;
	else if (*str == 'D')
		return BROWSER_FAVORITES_DIAGRAMS;
	else
		g_warning ("Unknow type '%s' of favorite", str);
	return 0;
}


/*
 * Find a favorite ID from its ID or from its contents
 *
 * Returns: the ID or -1 if not found (and sets ERROR)
 */
static gint
find_favorite (BrowserFavorites *bfav, guint session_id, gint id, const gchar *contents, GError **error)
{
	GdaSqlBuilder *b;
	GdaStatement *stmt;
	GdaSet *params = NULL;
	GdaDataModel *model;
	gint favid = -1;

	g_return_val_if_fail ((id >= 0) || contents, -1);

	b = gda_sql_builder_new (GDA_SQL_STATEMENT_SELECT);
	gda_sql_builder_add_field (b,
				   gda_sql_builder_ident (b, 0, "id"), 0);
	gda_sql_builder_select_add_target (b, 0,
					   gda_sql_builder_ident (b, 0, FAVORITES_TABLE_NAME),
					   NULL);

	if (id >= 0) {
		/* lookup from ID */
		gda_sql_builder_set_where (b,
		    gda_sql_builder_cond (b, 0, GDA_SQL_OPERATOR_TYPE_EQ,
					  gda_sql_builder_ident (b, 0, "id"),
					  gda_sql_builder_param (b, 0, "id", G_TYPE_INT, FALSE), 0));
	}
	else {
		/* lookup using session and contents */
		gda_sql_builder_set_where (b,
	            gda_sql_builder_cond (b, 0, GDA_SQL_OPERATOR_TYPE_AND,
					  gda_sql_builder_cond (b, 0, GDA_SQL_OPERATOR_TYPE_EQ,
						   gda_sql_builder_ident (b, 0, "session"),
						   gda_sql_builder_param (b, 0, "session", G_TYPE_INT, FALSE), 0),
					  gda_sql_builder_cond (b, 0, GDA_SQL_OPERATOR_TYPE_EQ,
						   gda_sql_builder_ident (b, 0, "contents"),
						   gda_sql_builder_param (b, 0, "contents", G_TYPE_INT, FALSE), 0), 0));
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
	}

	g_object_unref (G_OBJECT (model));
	return favid;
}

/*
 * Reorders the favorites for @order_key, making sure @id is at position @new_pos
 */
static gboolean
favorites_reorder (BrowserFavorites *bfav, gint order_key, gint id, gint new_pos, GError **error)
{
	GdaSqlBuilder *b;
	GdaStatement *stmt;
	GdaSet *params = NULL;
	GdaDataModel *model;

	g_assert (id >= 0);
	g_assert (order_key >= 0);

	b = gda_sql_builder_new (GDA_SQL_STATEMENT_SELECT);
	gda_sql_builder_add_field (b, gda_sql_builder_ident (b, 0, "fav_id"), 0);

	gda_sql_builder_select_add_target (b, 0,
					   gda_sql_builder_ident (b, 0, FAVORDER_TABLE_NAME),
					   NULL);
	
	gda_sql_builder_set_where (b, gda_sql_builder_cond (b, 1, GDA_SQL_OPERATOR_TYPE_EQ,
				    gda_sql_builder_ident (b, 0, "order_key"),
				    gda_sql_builder_param (b, 0, "orderkey", G_TYPE_INT, FALSE), 0));
	gda_sql_builder_select_order_by (b,
					 gda_sql_builder_ident (b, 0, "rank"), TRUE, NULL);
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
	gda_sql_builder_add_field (b,
				   gda_sql_builder_ident (b, 0, "rank"),
				   gda_sql_builder_param (b, 0, "rank", G_TYPE_INT, FALSE));
	gda_sql_builder_cond (b, 1, GDA_SQL_OPERATOR_TYPE_EQ,
			      gda_sql_builder_ident (b, 0, "fav_id"),
			      gda_sql_builder_param (b, 0, "id", G_TYPE_INT, FALSE),
			      0);
	gda_sql_builder_cond (b, 2, GDA_SQL_OPERATOR_TYPE_EQ,
			      gda_sql_builder_ident (b, 0, "order_key"),
			      gda_sql_builder_param (b, 0, "orderkey", G_TYPE_INT, FALSE),
			      0);
	gda_sql_builder_set_where (b, gda_sql_builder_cond (b, 0, GDA_SQL_OPERATOR_TYPE_AND, 1, 2, 0));
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
 * browser_favorites_add
 *
 * Add a new favorite, or replace an existing one.
 * NOTE:
 *   - if @fav->id is NULL then it's either an update or an insert (depending if fav->contents exists)
 *     and if it's not it is an UPDATE
 *   - @fav->type can't be 0
 *   - @fav->contents can't be %NULL
 *
 * if @order_key is negative, then no ordering is done and @pos is ignored.
 */
gint
browser_favorites_add (BrowserFavorites *bfav, guint session_id,
		       BrowserFavoritesAttributes *fav,
		       gint order_key, gint pos,
		       GError **error)
{
	GdaConnection *store_cnc;
	GdaSet *params = NULL;
	gint favid = -1;

	g_return_val_if_fail (BROWSER_IS_FAVORITES (bfav), FALSE);
	g_return_val_if_fail (fav, FALSE);
	g_return_val_if_fail (fav->contents, FALSE);
	g_return_val_if_fail (fav->type, FALSE);

	if (! bfav->priv->store_cnc &&
	    ! meta_store_addons_init (bfav, error))
		return FALSE;

	store_cnc = bfav->priv->store_cnc;
	if (! gda_lockable_trylock (GDA_LOCKABLE (store_cnc))) {
		g_set_error (error, 0, 0, "%s",
                             _("Can't initialize transaction to access favorites"));
		return FALSE;
	}
	/* begin a transaction */
	if (! gda_connection_begin_transaction (store_cnc, NULL, GDA_TRANSACTION_ISOLATION_UNKNOWN, NULL)) {
		g_set_error (error, 0, 0, "%s",
                             _("Can't initialize transaction to access favorites"));
		gda_lockable_unlock (GDA_LOCKABLE (store_cnc));
                return FALSE;
	}

	params = gda_set_new_inline (8,
				     "session", G_TYPE_INT, session_id,
				     "id", G_TYPE_INT, fav->id,
				     "type", G_TYPE_STRING, favorite_type_to_string (fav->type),
				     "name", G_TYPE_STRING, fav->name,
				     "contents", G_TYPE_STRING, fav->contents,
				     "rank", G_TYPE_INT, pos,
				     "orderkey", G_TYPE_INT, order_key,
				     "descr", G_TYPE_STRING, fav->descr);

	favid = find_favorite (bfav, session_id, fav->id, fav->contents, NULL);
	if (favid == -1) {
		/* insert a favorite */
		GdaSqlBuilder *builder;
		GdaStatement *stmt;
		builder = gda_sql_builder_new (GDA_SQL_STATEMENT_INSERT);
		gda_sql_builder_set_table (builder, FAVORITES_TABLE_NAME);

		gda_sql_builder_add_field (builder,
					   gda_sql_builder_ident (builder, 0, "session"),
					   gda_sql_builder_param (builder, 0, "session", G_TYPE_INT, FALSE));
		gda_sql_builder_add_field (builder,
					   gda_sql_builder_ident (builder, 0, "type"),
					   gda_sql_builder_param (builder, 0, "type", G_TYPE_INT, FALSE));
		gda_sql_builder_add_field (builder,
					   gda_sql_builder_ident (builder, 0, "name"),
					   gda_sql_builder_param (builder, 0, "name", G_TYPE_STRING, TRUE));
		gda_sql_builder_add_field (builder,
					   gda_sql_builder_ident (builder, 0, "contents"),
					   gda_sql_builder_param (builder, 0, "contents", G_TYPE_STRING, FALSE));
		gda_sql_builder_add_field (builder,
					   gda_sql_builder_ident (builder, 0, "descr"),
					   gda_sql_builder_param (builder, 0, "descr", G_TYPE_STRING, TRUE));
		stmt = gda_sql_builder_get_statement (builder, error);
		g_object_unref (G_OBJECT (builder));
		if (!stmt)
			goto err;
		if (gda_connection_statement_execute_non_select (store_cnc, stmt, params, NULL, error) == -1) {
			g_object_unref (stmt);
			goto err;
		}
		g_object_unref (stmt);
	}
	else {
		/* update favorite's contents */
		GdaSqlBuilder *builder;
		GdaStatement *stmt;

		gda_set_set_holder_value (params, NULL, "id", favid);
		builder = gda_sql_builder_new (GDA_SQL_STATEMENT_UPDATE);
		gda_sql_builder_set_table (builder, FAVORITES_TABLE_NAME);

		gda_sql_builder_add_field (builder,
					   gda_sql_builder_ident (builder, 0, "name"),
					   gda_sql_builder_param (builder, 0, "name", G_TYPE_STRING, TRUE));
		gda_sql_builder_add_field (builder,
					   gda_sql_builder_ident (builder, 0, "contents"),
					   gda_sql_builder_param (builder, 0, "contents", G_TYPE_STRING, FALSE));
		gda_sql_builder_add_field (builder,
					   gda_sql_builder_ident (builder, 0, "descr"),
					   gda_sql_builder_param (builder, 0, "descr", G_TYPE_STRING, TRUE));

		gda_sql_builder_set_where (builder,
					   gda_sql_builder_cond (builder, 0, GDA_SQL_OPERATOR_TYPE_EQ,
								 gda_sql_builder_ident (builder, 0, "id"),
								 gda_sql_builder_param (builder, 0, "id", G_TYPE_INT, FALSE),
								 0));
		stmt = gda_sql_builder_get_statement (builder, error);
		g_object_unref (G_OBJECT (builder));
		if (!stmt)
			goto err;
		if (gda_connection_statement_execute_non_select (store_cnc, stmt, params, NULL, error) == -1) {
			g_object_unref (stmt);
			goto err;
		}
		g_object_unref (stmt);
	}

	if (order_key >= 0) {
		GdaSqlBuilder *builder;
		GdaStatement *stmt;

		/* delete and insert favorite in orders table */
		favid = find_favorite (bfav, session_id, fav->id, fav->contents, error);
		if (favid < 0) {
			g_warning ("Could not identify favorite by its ID, make sure it's correct");
			goto err;
		}
		
		gda_set_set_holder_value (params, NULL, "id", favid);

		builder = gda_sql_builder_new (GDA_SQL_STATEMENT_DELETE);
		gda_sql_builder_set_table (builder, FAVORDER_TABLE_NAME);
		gda_sql_builder_set_where (builder,
		      gda_sql_builder_cond (builder, 0, GDA_SQL_OPERATOR_TYPE_AND,
			    gda_sql_builder_cond (builder, 0, GDA_SQL_OPERATOR_TYPE_EQ,
						  gda_sql_builder_ident (builder, 0, "fav_id"),
						  gda_sql_builder_param (builder, 0, "id", G_TYPE_INT, FALSE),
						  0),
			    gda_sql_builder_cond (builder, 0, GDA_SQL_OPERATOR_TYPE_EQ,
						  gda_sql_builder_ident (builder, 0, "order_key"),
						  gda_sql_builder_param (builder, 0, "orderkey", G_TYPE_INT, FALSE),
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
		gda_sql_builder_add_field (builder,
					   gda_sql_builder_ident (builder, 0, "fav_id"),
					   gda_sql_builder_param (builder, 0, "id", G_TYPE_INT, FALSE));
		gda_sql_builder_add_field (builder,
					   gda_sql_builder_ident (builder, 0, "rank"),
					   gda_sql_builder_param (builder, 0, "rank", G_TYPE_INT, FALSE));
		gda_sql_builder_add_field (builder,
					   gda_sql_builder_ident (builder, 0, "order_key"),
					   gda_sql_builder_param (builder, 0, "orderkey", G_TYPE_STRING, TRUE));
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
		g_set_error (error, 0, 0, "%s",
                             _("Can't commit transaction to access favorites"));
		goto err;
	}

	if (params)
		g_object_unref (params);
	gda_lockable_unlock (GDA_LOCKABLE (store_cnc));
	g_signal_emit (bfav, browser_favorites_signals [FAV_CHANGED],
		       g_quark_from_string (favorite_type_to_string (fav->type)));
	return TRUE;

 err:
	if (params)
		g_object_unref (params);
	gda_lockable_unlock (GDA_LOCKABLE (store_cnc));
	gda_connection_rollback_transaction (store_cnc, NULL, NULL);
	return FALSE;
}

/**
 * browser_favorites_free_list
 */
void
browser_favorites_free_list (GSList *fav_list)
{
	GSList *list;
	if (!fav_list)
		return;
	for (list = fav_list; list; list = list->next) {
		BrowserFavoritesAttributes *fav = (BrowserFavoritesAttributes*) list->data;
		g_free (fav->contents);
		g_free (fav->descr);
		g_free (fav);
	}
	g_slist_free (fav_list);
}

/**
 * browser_favorites_list
 *
 * Returns: a new list of #BrowserFavoritesAttributes pointers. The list has to
 *          be freed using browser_favorites_free_list()
 */
GSList *
browser_favorites_list (BrowserFavorites *bfav, guint session_id, BrowserFavoritesType type,
			gint order_key, GError **error)
{
	GdaSqlBuilder *b;
	GdaSet *params = NULL;
	GdaStatement *stmt;
	guint t1, t2;
	GdaDataModel *model = NULL;
	GSList *fav_list = NULL;

	guint and_cond_ids [3];
	int and_cond_size = 0;
	guint or_cond_ids [BROWSER_FAVORITES_NB_TYPES];
	int or_cond_size = 0;

	g_return_val_if_fail (BROWSER_IS_FAVORITES (bfav), NULL);
	g_return_val_if_fail ((type != 0) || (order_key >= 0), NULL);

	if (! bfav->priv->store_cnc &&
	    ! meta_store_addons_init (bfav, error))
		return NULL;

	b = gda_sql_builder_new (GDA_SQL_STATEMENT_SELECT);
	gda_sql_builder_add_field (b,
				   gda_sql_builder_ident (b, 0, "fav.contents"), 0);
	gda_sql_builder_add_field (b,
				   gda_sql_builder_ident (b, 0, "fav.descr"), 0);
	gda_sql_builder_add_field (b,
				   gda_sql_builder_ident (b, 0, "fav.name"), 0);
	gda_sql_builder_add_field (b,
				   gda_sql_builder_ident (b, 0, "fav.type"), 0);
	gda_sql_builder_add_field (b,
				   gda_sql_builder_ident (b, 0, "fav.id"), 0);

	t1 = gda_sql_builder_select_add_target (b, 0,
						gda_sql_builder_ident (b, 0, FAVORITES_TABLE_NAME),
						"fav");
	if (order_key > 0) {
		t2 = gda_sql_builder_select_add_target (b, 0,
							gda_sql_builder_ident (b, 0, FAVORDER_TABLE_NAME),
							"o");
		gda_sql_builder_select_join_targets (b, 0, t1, t2, GDA_SQL_SELECT_JOIN_LEFT,
						     gda_sql_builder_cond (b, 0, GDA_SQL_OPERATOR_TYPE_EQ,
									   gda_sql_builder_ident (b, 0, "fav.id"),
									   gda_sql_builder_ident (b, 0, "o.fav_id"),
									   0));
		gda_sql_builder_select_order_by (b,
						 gda_sql_builder_ident (b, 0, "o.rank"), TRUE, NULL);

		and_cond_ids [and_cond_size] = gda_sql_builder_cond (b, 0, GDA_SQL_OPERATOR_TYPE_EQ,
							     gda_sql_builder_ident (b, 0, "o.order_key"),
							     gda_sql_builder_param (b, 0, "okey", G_TYPE_INT, FALSE),
							     0);
		and_cond_size++;
	}

	and_cond_ids [and_cond_size] = gda_sql_builder_cond (b, 0, GDA_SQL_OPERATOR_TYPE_EQ,
						     gda_sql_builder_ident (b, 0, "fav.session"),
						     gda_sql_builder_param (b, 0, "session", G_TYPE_INT, FALSE), 0);
	and_cond_size++;

	gint i;
	gint flag;
	for (i = 0, flag = 1; i < BROWSER_FAVORITES_NB_TYPES; i++, flag <<= 1) {
		if (type & flag) {
			gchar *str;
			str = g_strdup_printf ("'%s'", favorite_type_to_string (flag));
			or_cond_ids [or_cond_size] = gda_sql_builder_cond (b, 0, GDA_SQL_OPERATOR_TYPE_EQ,
							     gda_sql_builder_ident (b, 0, "fav.type"),
							     gda_sql_builder_ident (b, 0, str),
							     0);
			g_free (str);
			or_cond_size++;
		}
	}
	if (or_cond_size >= 1) {
		and_cond_ids [and_cond_size] = gda_sql_builder_cond_v (b, 0, GDA_SQL_OPERATOR_TYPE_OR,
								       or_cond_ids, or_cond_size);
		and_cond_size++;
	}

	gda_sql_builder_set_where (b,
				   gda_sql_builder_cond_v (b, 0, GDA_SQL_OPERATOR_TYPE_AND, and_cond_ids, and_cond_size));

	{
		GdaSqlStatement *sqlst;
		sqlst = gda_sql_builder_get_sql_statement (b, TRUE);
		g_print ("=>%s\n", gda_sql_statement_serialize (sqlst));
	}

	stmt = gda_sql_builder_get_statement (b, error);
	g_object_unref (G_OBJECT (b));
	if (!stmt)
		goto out;

	params = gda_set_new_inline (2,
				     "session", G_TYPE_INT, session_id,
				     "okey", G_TYPE_INT, order_key);

	model = gda_connection_statement_execute_select (bfav->priv->store_cnc, stmt, params, error);
	g_object_unref (stmt);
	if (!model)
		goto out;

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
			BrowserFavoritesAttributes *fav;
			fav = g_new0 (BrowserFavoritesAttributes, 1);
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
			browser_favorites_free_list (fav_list);
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
 * browser_favorites_delete_favorite
 */
gboolean
browser_favorites_delete (BrowserFavorites *bfav, guint session_id,
			  BrowserFavoritesAttributes *fav, GError **error)
{
	GdaSqlBuilder *b;
	GdaSet *params = NULL;
	GdaStatement *stmt;
	gboolean retval = FALSE;
	gint favid;

	g_return_val_if_fail (BROWSER_IS_FAVORITES (bfav), FALSE);
	g_return_val_if_fail (fav, FALSE);
	g_return_val_if_fail ((fav->id >= 0) || fav->contents, FALSE);
	
	if (! bfav->priv->store_cnc &&
	    ! meta_store_addons_init (bfav, error))
		return FALSE;

	if (! gda_lockable_trylock (GDA_LOCKABLE (bfav->priv->store_cnc))) {
		g_set_error (error, 0, 0, "%s",
                             _("Can't initialize transaction to access favorites"));
		return FALSE;
	}
	/* begin a transaction */
	if (! gda_connection_begin_transaction (bfav->priv->store_cnc, NULL,
						GDA_TRANSACTION_ISOLATION_UNKNOWN, NULL)) {
		g_set_error (error, 0, 0, "%s",
                             _("Can't initialize transaction to access favorites"));
		gda_lockable_unlock (GDA_LOCKABLE (bfav->priv->store_cnc));
                return FALSE;
	}

	favid = find_favorite (bfav, session_id, fav->id, fav->contents, error);
	if (favid < 0)
		goto out;

	/* remove entry from favorites' list */
	b = gda_sql_builder_new (GDA_SQL_STATEMENT_DELETE);
	gda_sql_builder_set_table (b, FAVORITES_TABLE_NAME);
	gda_sql_builder_set_where (b, gda_sql_builder_cond (b, 0, GDA_SQL_OPERATOR_TYPE_EQ,
							    gda_sql_builder_ident (b, 0, "id"),
							    gda_sql_builder_param (b, 0, "id", G_TYPE_INT, FALSE),
							    0));

	stmt = gda_sql_builder_get_statement (b, error);
	g_object_unref (G_OBJECT (b));
	if (!stmt)
		goto out;

	params = gda_set_new_inline (1,
				     "id", G_TYPE_INT, favid);

	if (gda_connection_statement_execute_non_select (bfav->priv->store_cnc, stmt, params, NULL, error) == -1) {
		g_object_unref (stmt);
		goto out;
	}
	g_object_unref (stmt);

	/* remove entry from favorites' order */
	b = gda_sql_builder_new (GDA_SQL_STATEMENT_DELETE);
	gda_sql_builder_set_table (b, FAVORDER_TABLE_NAME);
	gda_sql_builder_set_where (b, gda_sql_builder_cond (b, 0, GDA_SQL_OPERATOR_TYPE_EQ,
							    gda_sql_builder_ident (b, 0, "fav_id"),
							    gda_sql_builder_param (b, 0, "id", G_TYPE_INT, FALSE),
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
		g_set_error (error, 0, 0, "%s",
                             _("Can't commit transaction to access favorites"));
		goto out;
	}
	retval = TRUE;

 out:
	if (!retval)
		gda_connection_rollback_transaction (bfav->priv->store_cnc, NULL, NULL);

	gda_lockable_unlock (GDA_LOCKABLE (bfav->priv->store_cnc));
	if (retval)
		g_signal_emit (bfav, browser_favorites_signals [FAV_CHANGED],
			       g_quark_from_string (favorite_type_to_string (fav->type)));
	if (params)
		g_object_unref (G_OBJECT (params));

	return retval;
}
