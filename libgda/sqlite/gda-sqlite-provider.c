/*
 * Copyright (C) 2001 - 2002 Carlos Perelló Marín <carlos@gnome-db.org>
 * Copyright (C) 2001 - 2003 Rodrigo Moya <rodrigo@gnome-db.org>
 * Copyright (C) 2002 - 2003 Gonzalo Paniagua Javier <gonzalo@src.gnome.org>
 * Copyright (C) 2004 Benjamin Otte <in7y118@public.uni-hamburg.de>
 * Copyright (C) 2004 J.H.M. Dassen (Ray) <jdassen@debian.org>
 * Copyright (C) 2004 Julio M. Merino Vidal <jmmv@menta.net>
 * Copyright (C) 2004 Jürg Billeter <j@bitron.ch>
 * Copyright (C) 2004 Nikolai Weibull <ruby-gnome2-devel-en-list@pcppopper.org>
 * Copyright (C) 2005 Denis Fortin <denis.fortin@free.fr>
 * Copyright (C) 2005 - 2015 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2005 Álvaro Peña <alvaropg@telefonica.net>
 * Copyright (C) 2008 - 2009 Bas Driessen <bas.driessen@xobas.com>
 * Copyright (C) 2008 - 2014 Murray Cumming <murrayc@murrayc.com>
 * Copyright (C) 2009 Armin Burgmeier <armin@openismus.com>
 * Copyright (C) 2010 David King <davidk@openismus.com>
 * Copyright (C) 2011,2018 Daniel Espinosa <despinosa@src.gnome.org>
 * Copyright (C) 2011 Marek Černocký <marek@manet.cz>
 * Copyright (C) 2012 Marco Ciampa <ciampix@libero.it>
 * Copyright (C) 2017-2018 Daniel Espinosa <esodan@gmail.com>
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

#undef GDA_DISABLE_DEPRECATED
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <glib/gi18n-lib.h>
#include <glib/gstdio.h>
#include <gio/gio.h>
#include <libgda/gda-data-model-array.h>
#include <libgda/gda-data-model-private.h>
#include <libgda/gda-util.h>
#include <libgda/gda-server-provider-extra.h>
#include <libgda/gda-server-provider-impl.h>
#include <libgda/gda-server-operation-private.h>
#include "gda-sqlite.h"
#include "gda-sqlite-provider.h"
#include "gda-sqlite-recordset.h"
#include "gda-sqlite-ddl.h"
#include "gda-sqlite-util.h"
#include "gda-sqlite-meta.h"
#include "gda-sqlite-handler-bin.h"
#include "gda-sqlite-handler-boolean.h"
#include "gda-sqlite-blob-op.h"
#include <libgda/gda-connection-private.h>
#include <libgda/binreloc/gda-binreloc.h>
#include <libgda/gda-set.h>
#include <libgda/gda-statement-extra.h>
#include <libgda/sql-parser/gda-sql-parser.h>
#include <stdio.h>
#define _GDA_PSTMT(x) ((GdaPStmt*)(x))
#include <libgda/gda-debug-macros.h>
#include <libgda/gda-provider-meta.h>
#include <libgda/gda-provider.h>
#include <libgda/gda-server-provider.h>

#define FILE_EXTENSION ".db"
static gchar *get_table_nth_column_name (GdaServerProvider *prov, GdaConnection *cnc, const gchar *table_name, gint pos);

/* TMP */
typedef struct {
	GdaSqlStatement *stmt;
	gchar   *db;
	gchar   *table;
	gchar   *column;
	gboolean free_column; /* set to %TRUE if @column has been dynamically allocated */
	GdaBlob *blob;
} PendingBlob;

static PendingBlob*
make_pending_blob (GdaServerProvider *provider, GdaConnection *cnc, GdaStatement *stmt, GdaHolder *holder, GError **error)
{
	PendingBlob *pb = NULL;
	GdaSqlStatement *sqlst;
	const gchar *hname;

	g_return_val_if_fail (GDA_IS_STATEMENT (stmt), NULL);
	g_return_val_if_fail (GDA_IS_HOLDER (holder), NULL);
	hname = gda_holder_get_id (holder);
	g_return_val_if_fail (hname && *hname, NULL);

	g_object_get (G_OBJECT (stmt), "structure", &sqlst, NULL);
	g_return_val_if_fail (sqlst, NULL);

	switch (sqlst->stmt_type) {
	case GDA_SQL_STATEMENT_INSERT: {
		GdaSqlStatementInsert *istmt = (GdaSqlStatementInsert*) sqlst->contents;
		gint pos = -1;
		GSList *vll;
		for (vll = istmt->values_list; vll && (pos == -1); vll = vll->next) {
			GSList *vlist;
			gint p;
			for (p = 0, vlist = (GSList *) vll->data;
			     vlist;
			     p++, vlist = vlist->next) {
				GdaSqlExpr *expr = (GdaSqlExpr *) vlist->data;
				if (!expr->param_spec)
					continue;
				if (expr->param_spec->name &&
				    !strcmp (expr->param_spec->name, hname)) {
					pos = p;
					break;
				}
			}
		}
		if (pos == -1) {
			g_set_error (error, GDA_SERVER_PROVIDER_ERROR,
				     GDA_SERVER_PROVIDER_INTERNAL_ERROR,
				     _("Parameter '%s' not found in statement"), hname);
			goto out;
		}
		GdaSqlField *field;
		field = g_slist_nth_data (istmt->fields_list, pos);
		if (field) {
			pb = g_new0 (PendingBlob, 1);
			pb->table = istmt->table->table_name;
			pb->column = field->field_name;
			pb->free_column = FALSE;
		}
		else if (istmt->fields_list) {
			g_set_error (error, GDA_SERVER_PROVIDER_ERROR,
				     GDA_SERVER_PROVIDER_INTERNAL_ERROR,
				     _("No column name to associate to parameter '%s'"),
				     hname);
			goto out;
		}
		else {
			gchar *fname;
			fname = get_table_nth_column_name (provider, cnc, istmt->table->table_name, pos);
			if (fname) {
				pb = g_new0 (PendingBlob, 1);
				pb->table = istmt->table->table_name;
				pb->column = fname;
				pb->free_column = TRUE;
			}
			else {
				g_set_error (error, GDA_SERVER_PROVIDER_ERROR,
					     GDA_SERVER_PROVIDER_INTERNAL_ERROR,
					     _("No column name to associate to parameter '%s'"),
					     hname);
				goto out;
			}
		}

		break;
	}
	case GDA_SQL_STATEMENT_UPDATE: {
		GdaSqlStatementUpdate *ustmt = (GdaSqlStatementUpdate*) sqlst->contents;
		gint pos = -1;
		GSList *vlist;
		gint p;
		for (p = 0, vlist = ustmt->expr_list;
		     vlist;
		     p++, vlist = vlist->next) {
			GdaSqlExpr *expr = (GdaSqlExpr *) vlist->data;
			if (!expr->param_spec)
				continue;
			if (expr->param_spec->name &&
			    !strcmp (expr->param_spec->name, hname)) {
				pos = p;
				break;
			}
		}
		if (pos == -1) {
			g_set_error (error, GDA_SERVER_PROVIDER_ERROR,
				     GDA_SERVER_PROVIDER_INTERNAL_ERROR,
				     _("Parameter '%s' not found is statement"), hname);
			goto out;
		}
		GdaSqlField *field;
		field = g_slist_nth_data (ustmt->fields_list, pos);
		if (field) {
			pb = g_new0 (PendingBlob, 1);
			pb->table = ustmt->table->table_name;
			pb->column = field->field_name;
			pb->free_column = FALSE;
		}
		if (ustmt->fields_list) {
			g_set_error (error, GDA_SERVER_PROVIDER_ERROR,
				     GDA_SERVER_PROVIDER_INTERNAL_ERROR,
				     _("No column name to associate to parameter '%s'"),
				     hname);
			goto out;
		}
		else {
			gchar *fname;
			fname = get_table_nth_column_name (provider, cnc, ustmt->table->table_name, pos);
			if (fname) {
				pb = g_new0 (PendingBlob, 1);
				pb->table = ustmt->table->table_name;
				pb->column = fname;
				pb->free_column = TRUE;
			}
			else {
				g_set_error (error, GDA_SERVER_PROVIDER_ERROR,
					     GDA_SERVER_PROVIDER_INTERNAL_ERROR,
					     _("No column name to associate to parameter '%s'"),
					     hname);
				goto out;
			}
		}
		break;
	}
	default:
		g_set_error (error, GDA_SERVER_PROVIDER_ERROR,
			     GDA_SERVER_PROVIDER_NON_SUPPORTED_ERROR,
			     "%s",_("Binding a BLOB for this type of statement is not supported"));
		goto out;
	}

 out:
	if (pb)
		pb->stmt = sqlst;
	else
		gda_sql_statement_free (sqlst);
	return pb;
}

static void
pending_blobs_free_list (GSList *blist)
{
	if (!blist)
		return;
	GSList *l;
	for (l = blist; l; l = l->next) {
		PendingBlob *pb = (PendingBlob*) l->data;
		if (pb->stmt)
			gda_sql_statement_free (pb->stmt);
		if (pb->free_column)
			g_free (pb->column);
		g_free (pb);
	}

	g_slist_free (blist);
}


static void
gda_sqlite_provider_meta_iface_init (GdaProviderMetaInterface *iface);

static void
gda_sqlite_provider_iface_init (GdaProviderInterface *iface);

// API routines from library
Sqlite3ApiRoutines *s3r;

/**
 * call it if you are implementing a new derived provider using
 * a different SQLite library, like SQLCipher
 */
static gpointer
gda_sqlite_provider_get_api_internal (GdaSqliteProvider *prov) {
  return s3r;
}
/* properties */
enum
{
	PROP_0,
	PROP_IS_DEFAULT,
	PROP_CONNECTION
};

/*
 * GObject methods
 */
typedef struct {
	GPtrArray       *internal_stmt;
	gboolean         is_default;
} GdaSqliteProviderPrivate;


G_DEFINE_TYPE_WITH_CODE (GdaSqliteProvider, gda_sqlite_provider, GDA_TYPE_SERVER_PROVIDER,
                         G_ADD_PRIVATE (GdaSqliteProvider)
                         G_IMPLEMENT_INTERFACE (GDA_TYPE_PROVIDER_META, gda_sqlite_provider_meta_iface_init)
                         G_IMPLEMENT_INTERFACE (GDA_TYPE_PROVIDER, gda_sqlite_provider_iface_init))


/*
 * GdaServerProvider's virtual methods
 */
/* connection management */
static gboolean            gda_sqlite_provider_open_connection (GdaServerProvider *provider, GdaConnection *cnc,
								GdaQuarkList *params, GdaQuarkList *auth);
static gboolean            gda_sqlite_provider_prepare_connection (GdaServerProvider *provider,
								   GdaConnection *cnc, GdaQuarkList *params, GdaQuarkList *auth);
static gboolean            gda_sqlite_provider_close_connection (GdaServerProvider *provider, GdaConnection *cnc);
static const gchar        *gda_sqlite_provider_get_server_version (GdaServerProvider *provider, GdaConnection *cnc);

/* DDL operations */
static gboolean            gda_sqlite_provider_supports_operation (GdaServerProvider *provider, GdaConnection *cnc,
								   GdaServerOperationType type, GdaSet *options);
static GdaServerOperation *gda_sqlite_provider_create_operation (GdaServerProvider *provider, GdaConnection *cnc,
								 GdaServerOperationType type,
								 GdaSet *options, GError **error);
static gchar              *gda_sqlite_provider_render_operation (GdaServerProvider *provider, GdaConnection *cnc,
								 GdaServerOperation *op, GError **error);

static gboolean            gda_sqlite_provider_perform_operation (GdaServerProvider *provider, GdaConnection *cnc,
								  GdaServerOperation *op, GError **error);
/* transactions */
static gboolean            gda_sqlite_provider_begin_transaction (GdaServerProvider *provider, GdaConnection *cnc,
								  const gchar *name, GdaTransactionIsolation level,
								  GError **error);
static gboolean            gda_sqlite_provider_commit_transaction (GdaServerProvider *provider, GdaConnection *cnc,
								   const gchar *name, GError **error);
static gboolean            gda_sqlite_provider_rollback_transaction (GdaServerProvider *provider, GdaConnection * cnc,
								     const gchar *name, GError **error);
static gboolean            gda_sqlite_provider_add_savepoint (GdaServerProvider *provider, GdaConnection *cnc,
							      const gchar *name, GError **error);
static gboolean            gda_sqlite_provider_rollback_savepoint (GdaServerProvider *provider, GdaConnection *cnc,
								   const gchar *name, GError **error);
static gboolean            gda_sqlite_provider_delete_savepoint (GdaServerProvider *provider, GdaConnection *cnc,
								 const gchar *name, GError **error);

/* information retrieval */
static const gchar        *gda_sqlite_provider_get_version (GdaServerProvider *provider);
static gboolean            gda_sqlite_provider_supports_feature (GdaServerProvider *provider, GdaConnection *cnc,
								 GdaConnectionFeature feature);

static GdaWorker          *gda_sqlite_provider_create_worker (GdaServerProvider *provider, gboolean for_cnc);
static const gchar        *gda_sqlite_provider_get_name (GdaServerProvider *provider);

static GdaDataHandler     *gda_sqlite_provider_get_data_handler (GdaServerProvider *provider, GdaConnection *cnc,
								 GType g_type, const gchar *dbms_type);

static const gchar*        gda_sqlite_provider_get_default_dbms_type (GdaServerProvider *provider, GdaConnection *cnc,
								      GType type);
/* statements */
static GdaSqlParser        *gda_sqlite_provider_create_parser (GdaServerProvider *provider, GdaConnection *cnc);
static gchar               *gda_sqlite_provider_statement_to_sql  (GdaServerProvider *provider, GdaConnection *cnc,
								   GdaStatement *stmt, GdaSet *params,
								   GdaStatementSqlFlag flags,
								   GSList **params_used, GError **error);
static gboolean             gda_sqlite_provider_statement_prepare (GdaServerProvider *provider, GdaConnection *cnc,
								   GdaStatement *stmt, GError **error);
static GObject             *gda_sqlite_provider_statement_execute (GdaServerProvider *provider, GdaConnection *cnc,
								   GdaStatement *stmt, GdaSet *params,
								   GdaStatementModelUsage model_usage,
								   GType *col_types, GdaSet **last_inserted_row, GError **error);
static GdaSqlStatement     *gda_sqlite_provider_statement_rewrite (GdaServerProvider *provider, GdaConnection *cnc,
								   GdaStatement *stmt, GdaSet *params, GError **error);

/* string escaping */
static gchar               *gda_sqlite_provider_escape_string (GdaServerProvider *provider, GdaConnection *cnc,
							       const gchar *string);
static gchar               *gda_sqlite_provider_unescape_string (GdaServerProvider *provider, GdaConnection *cnc,
								 const gchar *string);
GdaSet                     *gda_sqlite_provider_get_last_inserted (GdaServerProvider *provider,
                 GdaConnection *cnc, GError **error);

/*
 * private connection data destroy
 */
static void gda_sqlite_free_cnc_data (SqliteConnectionData *cdata);

/*
 * extending SQLite with our own functions  and collations
 */
static void scalar_gda_file_exists_func (sqlite3_context *context, int argc, sqlite3_value **argv);
static void scalar_gda_hex_print_func (sqlite3_context *context, int argc, sqlite3_value **argv);
static void scalar_gda_hex_print_func2 (sqlite3_context *context, int argc, sqlite3_value **argv);
static void scalar_gda_hex_func (sqlite3_context *context, int argc, sqlite3_value **argv);
static void scalar_gda_hex_func2 (sqlite3_context *context, int argc, sqlite3_value **argv);
static void scalar_rmdiacr (sqlite3_context *context, int argc, sqlite3_value **argv);
static void scalar_lower (sqlite3_context *context, int argc, sqlite3_value **argv);
static void scalar_upper (sqlite3_context *context, int argc, sqlite3_value **argv);
typedef struct {
	char     *name;
	int       nargs;
	gpointer  user_data; /* retrieve it in func. implementations using sqlite3_user_data() */
	void    (*xFunc)(sqlite3_context*,int,sqlite3_value**);
} ScalarFunction;
static ScalarFunction scalars[] = {
	{"gda_file_exists", 1, NULL, scalar_gda_file_exists_func},
	{"gda_hex_print", 1, NULL, scalar_gda_hex_print_func},
	{"gda_hex_print", 2, NULL, scalar_gda_hex_print_func2},
	{"gda_hex", 1, NULL, scalar_gda_hex_func},
	{"gda_hex", 2, NULL, scalar_gda_hex_func2},
	{"gda_rmdiacr", 1, NULL, scalar_rmdiacr},
	{"gda_rmdiacr", 2, NULL, scalar_rmdiacr},
	{"gda_lower", 1, NULL, scalar_lower},
	{"gda_upper", 1, NULL, scalar_upper},
};

/* Regexp handling */
static void scalar_regexp_func (sqlite3_context *context, int argc, sqlite3_value **argv);
static void scalar_regexp_match_func (sqlite3_context *context, int argc, sqlite3_value **argv);

static ScalarFunction regexp_functions[] = {
	{"regexp", 2, NULL, scalar_regexp_func},
	{"regexp", 3, NULL, scalar_regexp_func},
	{"regexp_match", 2, NULL, scalar_regexp_match_func},
	{"regexp_match", 3, NULL, scalar_regexp_match_func}
};

/* Collations */
static int locale_collate_func (G_GNUC_UNUSED void *pArg, int nKey1, const void *pKey1,
				int nKey2, const void *pKey2);
static int dcase_collate_func (G_GNUC_UNUSED void *pArg, int nKey1, const void *pKey1,
			       int nKey2, const void *pKey2);
typedef struct {
	char     *name;
	int     (*xFunc)(void *, int, const void *, int, const void *);
} CollationFunction;
static CollationFunction collation_functions[] = {
	{"LOCALE", locale_collate_func},
	{"DCASE", dcase_collate_func}
};


/* GdaProviderMeta methods declarations */
static GdaDataModel *gda_sqlite_provider_meta_btypes                (GdaProviderMeta *prov,
                              GError **error);
static GdaDataModel *gda_sqlite_provider_meta_udts                  (GdaProviderMeta *prov,
                              GError **error);
static GdaRow       *gda_sqlite_provider_meta_udt                   (GdaProviderMeta *prov,
                              const gchar *udt_catalog, const gchar *udt_schema,
                              GError **error);
static GdaDataModel *gda_sqlite_provider_meta_udt_cols              (GdaProviderMeta *prov,
                              GError **error);
static GdaRow       *gda_sqlite_provider_meta_udt_col               (GdaProviderMeta *prov,
                              const gchar *udt_catalog, const gchar *udt_schema,
                              const gchar *udt_name, GError **error);
static GdaDataModel *gda_sqlite_provider_meta_enums_type            (GdaProviderMeta *prov,
                              GError **error);
static GdaRow       *gda_sqlite_provider_meta_enum_type             (GdaProviderMeta *prov,
                              const gchar *udt_catalog, const gchar *udt_schema, const gchar *udt_name, GError **error);
static GdaDataModel *gda_sqlite_provider_meta_domains               (GdaProviderMeta *prov,
                              GError **error);
static GdaRow       *gda_sqlite_provider_meta_domain                (GdaProviderMeta *prov,
                              const gchar *domain_catalog, const gchar *domain_schema, GError **error);
static GdaDataModel *gda_sqlite_provider_meta_domains_constraints   (GdaProviderMeta *prov,
                              GError **error);
static GdaDataModel *gda_sqlite_provider_meta_domain_constraints    (GdaProviderMeta *prov,
                              const gchar *domain_catalog, const gchar *domain_schema,
                              const gchar *domain_name, GError **error);
static GdaRow       *gda_sqlite_provider_meta_domain_constraint     (GdaProviderMeta *prov,
                              const gchar *domain_catalog, const gchar *domain_schema,
                              const gchar *domain_name, const gchar *contraint_name,
                              GError **error);
static GdaDataModel *gda_sqlite_provider_meta_element_types         (GdaProviderMeta *prov,
                              GError **error);
static GdaRow       *gda_sqlite_provider_meta_element_type          (GdaProviderMeta *prov,
                              const gchar *specific_name, GError **error);
static GdaDataModel *gda_sqlite_provider_meta_collations            (GdaProviderMeta *prov,
                              GError **error);
static GdaRow       *gda_sqlite_provider_meta_collation             (GdaProviderMeta *prov,
                              const gchar *collation_catalog, const gchar *collation_schema,
                              const gchar *collation_name_n, GError **error);
static GdaDataModel *gda_sqlite_provider_meta_character_sets        (GdaProviderMeta *prov,
                              GError **error);
static GdaRow       *gda_sqlite_provider_meta_character_set         (GdaProviderMeta *prov,
                              const gchar *chset_catalog, const gchar *chset_schema,
                              const gchar *chset_name_n, GError **error);
static GdaDataModel *gda_sqlite_provider_meta_schematas             (GdaProviderMeta *prov,
                              GError **error);
static GdaRow       *gda_sqlite_provider_meta_schemata              (GdaProviderMeta *prov,
                              const gchar *catalog_name, const gchar *schema_name_n, GError **error);
static GdaDataModel *gda_sqlite_provider_meta_tables          (GdaProviderMeta *prov,
                              GError **error);
static GdaRow       *gda_sqlite_provider_meta_table            (GdaProviderMeta *prov,
                              const gchar *table_catalog, const gchar *table_schema,
                              const gchar *table_name_n, GError **error);
static GdaDataModel *gda_sqlite_provider_meta_views          (GdaProviderMeta *prov,
                              GError **error);
static GdaRow       *gda_sqlite_provider_meta_view            (GdaProviderMeta *prov,
                              const gchar *table_catalog, const gchar *table_schema,
                              const gchar *table_name_n, GError **error);
static GdaDataModel *gda_sqlite_provider_meta_tables_columns               (GdaProviderMeta *prov,
                              GError **error);
static GdaDataModel *gda_sqlite_provider_meta_table_columns         (GdaProviderMeta *prov,
                              const gchar *table_catalog, const gchar *table_schema,
                              const gchar *table_name, GError **error);
static GdaRow       *gda_sqlite_provider_meta_table_column          (GdaProviderMeta *prov,
                              const gchar *table_catalog, const gchar *table_schema,
                              const gchar *table_name,
                              const gchar *column_name, GError **error);
static GdaDataModel *gda_sqlite_provider_meta_views_columns            (GdaProviderMeta *prov,
                              GError **error);
static GdaDataModel *gda_sqlite_provider_meta_view_columns             (GdaProviderMeta *prov,
                              const gchar *view_catalog, const gchar *view_schema,
                              const gchar *view_name, GError **error);
static GdaRow       *gda_sqlite_provider_meta_view_column   (GdaProviderMeta *prov,
                              const gchar *view_catalog, const gchar *view_schema,
                              const gchar *view_name,
                              const gchar *column_name,
                              GError **error);
static GdaDataModel *gda_sqlite_provider_meta_constraints_tables    (GdaProviderMeta *prov, GError **error);
static GdaDataModel *gda_sqlite_provider_meta_constraints_table     (GdaProviderMeta *prov,
                             const gchar *table_catalog, const gchar *table_schema,
                             const gchar *table_name,
                             GError **error);
static GdaRow       *gda_sqlite_provider_meta_constraint_table      (GdaProviderMeta *prov,
                             const gchar *table_catalog, const gchar *table_schema,
                             const gchar *table_name,
                             const gchar *constraint_name_n, GError **error);
static GdaDataModel *gda_sqlite_provider_meta_columns (GdaProviderMeta *prov,
                             GError **error);
static GdaDataModel *gda_sqlite_provider_meta_constraints_ref       (GdaProviderMeta *prov,
                             GError **error);
static GdaDataModel *gda_sqlite_provider_meta_constraints_ref_table (GdaProviderMeta *prov,
                             const gchar *table_catalog,
                             const gchar *table_schema, const gchar *table_name,
                              GError **error);
static GdaRow       *gda_sqlite_provider_meta_constraint_ref  (GdaProviderMeta *prov,
                             const gchar *table_catalog,
                             const gchar *table_schema, const gchar *table_name,
                             const gchar *constraint_name, GError **error);
static GdaDataModel *gda_sqlite_provider_meta_key_columns           (GdaProviderMeta *prov,
                            GError **error);
static GdaRow       *gda_sqlite_provider_meta_key_column            (GdaProviderMeta *prov,
                              const gchar *table_catalog, const gchar *table_schema,
                              const gchar *table_name,
                              const gchar *constraint_name, GError **error);
static GdaDataModel *gda_sqlite_provider_meta_check_columns         (GdaProviderMeta *prov,
                              GError **error);
static GdaRow       *gda_sqlite_provider_meta_check_column          (GdaProviderMeta *prov,
                              const gchar *table_catalog,
                              const gchar *table_schema,
                              const gchar *table_name,
                              const gchar *constraint_name, GError **error);
static GdaDataModel *gda_sqlite_provider_meta_triggers              (GdaProviderMeta *prov,
                              GError **error);
static GdaRow       *gda_sqlite_provider_meta_trigger               (GdaProviderMeta *prov,
                              const gchar *table_catalog,
                              const gchar *table_schema,
                              const gchar *table_name, GError **error);
static GdaDataModel *gda_sqlite_provider_meta_routines              (GdaProviderMeta *prov,
                              GError **error);
static GdaRow       *gda_sqlite_provider_meta_routine               (GdaProviderMeta *prov,
                              const gchar *routine_catalog, const gchar *routine_schema,
                              const gchar *routine_name_n, GError **error);
static GdaDataModel *gda_sqlite_provider_meta_routines_col          (GdaProviderMeta *prov, GError **error);
static GdaRow       *gda_sqlite_provider_meta_routine_col           (GdaProviderMeta *prov,
                              const gchar *rout_catalog, const gchar *rout_schema,
                              const gchar *rout_name, GError **error);
static GdaDataModel *gda_sqlite_provider_meta_routines_pars         (GdaProviderMeta *prov,
                              GError **error);
static GdaRow       *gda_sqlite_provider_meta_routine_pars          (GdaProviderMeta *prov,
                              const gchar *rout_catalog, const gchar *rout_schema,
                              const gchar *rout_name, GError **error);
static GdaDataModel *gda_sqlite_provider_meta_indexes_tables        (GdaProviderMeta *prov,
                              GError **error);
static GdaDataModel *gda_sqlite_provider_meta_indexes_table         (GdaProviderMeta *prov,
                              const gchar *table_catalog, const gchar *table_schema,
                              const gchar *table_name, GError **error);
static GdaRow       *gda_sqlite_provider_meta_index_table (GdaProviderMeta *prov,
                              const gchar *table_catalog,
                              const gchar *table_schema,
                              const gchar *table_name,
                              const gchar *index_name_n, GError **error);
static GdaDataModel *gda_sqlite_provider_meta_index_cols            (GdaProviderMeta *prov,
                              GError **error);
static GdaRow       *gda_sqlite_provider_meta_index_col             (GdaProviderMeta *prov,
                              const gchar *table_catalog,
                              const gchar *table_schema,
                              const gchar *table_name,
                              const gchar *index_name, GError **error);

static void
gda_sqlite_provider_meta_iface_init (GdaProviderMetaInterface *iface) {
  iface->btypes = gda_sqlite_provider_meta_btypes;
  iface->udts = gda_sqlite_provider_meta_udts;
  iface->udt = gda_sqlite_provider_meta_udt;
  iface->udt_cols = gda_sqlite_provider_meta_udt_cols;
  iface->udt_col = gda_sqlite_provider_meta_udt_col;
  iface->enums_type = gda_sqlite_provider_meta_enums_type;
  iface->enum_type = gda_sqlite_provider_meta_enum_type;
  iface->domains = gda_sqlite_provider_meta_domains;
  iface->domain = gda_sqlite_provider_meta_domain;
  iface->domains_constraints = gda_sqlite_provider_meta_domains_constraints;
  iface->domain_constraints = gda_sqlite_provider_meta_domain_constraints;
  iface->domain_constraint = gda_sqlite_provider_meta_domain_constraint;
  iface->element_types = gda_sqlite_provider_meta_element_types;
  iface->element_type = gda_sqlite_provider_meta_element_type;
  iface->collations = gda_sqlite_provider_meta_collations;
  iface->collation = gda_sqlite_provider_meta_collation;
  iface->character_sets = gda_sqlite_provider_meta_character_sets;
  iface->character_set = gda_sqlite_provider_meta_character_set;
  iface->schematas = gda_sqlite_provider_meta_schematas;
  iface->schemata = gda_sqlite_provider_meta_schemata;
  iface->tables_columns = gda_sqlite_provider_meta_tables_columns;
  iface->tables = gda_sqlite_provider_meta_tables;
  iface->table = gda_sqlite_provider_meta_table;
  iface->views = gda_sqlite_provider_meta_views;
  iface->view = gda_sqlite_provider_meta_view;
  iface->columns = gda_sqlite_provider_meta_columns;
  iface->table_columns = gda_sqlite_provider_meta_table_columns;
  iface->table_column = gda_sqlite_provider_meta_table_column;
  iface->views_columns = gda_sqlite_provider_meta_views_columns;
  iface->view_columns = gda_sqlite_provider_meta_view_columns;
  iface->view_column = gda_sqlite_provider_meta_view_column;
  iface->constraints_tables = gda_sqlite_provider_meta_constraints_tables;
  iface->constraints_table = gda_sqlite_provider_meta_constraints_table;
  iface->constraint_table = gda_sqlite_provider_meta_constraint_table;
  iface->constraints_ref = gda_sqlite_provider_meta_constraints_ref;
  iface->constraints_ref_table = gda_sqlite_provider_meta_constraints_ref_table;
  iface->constraint_ref = gda_sqlite_provider_meta_constraint_ref;
  iface->key_columns = gda_sqlite_provider_meta_key_columns;
  iface->key_column = gda_sqlite_provider_meta_key_column;
  iface->check_columns = gda_sqlite_provider_meta_check_columns;
  iface->check_column = gda_sqlite_provider_meta_check_column;
  iface->triggers = gda_sqlite_provider_meta_triggers;
  iface->trigger = gda_sqlite_provider_meta_trigger;
  iface->routines = gda_sqlite_provider_meta_routines;
  iface->routine = gda_sqlite_provider_meta_routine;
  iface->routines_col = gda_sqlite_provider_meta_routines_col;
  iface->routine_col = gda_sqlite_provider_meta_routine_col;
  iface->routines_pars = gda_sqlite_provider_meta_routines_pars;
  iface->routine_pars = gda_sqlite_provider_meta_routine_pars;
  iface->indexes_tables = gda_sqlite_provider_meta_indexes_tables;
  iface->indexes_table = gda_sqlite_provider_meta_indexes_table;
  iface->index_table = gda_sqlite_provider_meta_index_table;
  iface->index_cols = gda_sqlite_provider_meta_index_cols;
  iface->index_col = gda_sqlite_provider_meta_index_col;
}

static const gchar        *gda_sqlite_provider_iface_get_name              (GdaProvider *provider);
static const gchar        *gda_sqlite_provider_iface_get_version           (GdaProvider *provider);
static const gchar        *gda_sqlite_provider_iface_get_server_version    (GdaProvider *provider, GdaConnection *cnc);
static gboolean            gda_sqlite_provider_iface_supports_feature      (GdaProvider *provider, GdaConnection *cnc,
                                                        GdaConnectionFeature feature);
static GdaSqlParser       *gda_sqlite_provider_iface_create_parser         (GdaProvider *provider, GdaConnection *cnc); /* may be NULL */
static GdaConnection      *gda_sqlite_provider_iface_create_connection     (GdaProvider *provider);
static GdaDataHandler     *gda_sqlite_provider_iface_get_data_handler      (GdaProvider *provider, GdaConnection *cnc, /* may be NULL */
                                                        GType g_type, const gchar *dbms_type);
static const gchar        *gda_sqlite_provider_iface_get_def_dbms_type     (GdaProvider *provider, GdaConnection *cnc,
                                                        GType g_type); /* may be NULL */
static gboolean            gda_sqlite_provider_iface_supports_operation    (GdaProvider *provider, GdaConnection *cnc,
                                                        GdaServerOperationType type, GdaSet *options);
static GdaServerOperation *gda_sqlite_provider_iface_create_operation      (GdaProvider *provider, GdaConnection *cnc,
                                                        GdaServerOperationType type, GdaSet *options,
                                                        GError **error);
static gchar              *gda_sqlite_provider_iface_render_operation      (GdaProvider *provider, GdaConnection *cnc,
                                                        GdaServerOperation *op, GError **error);
static gchar              *gda_sqlite_provider_iface_statement_to_sql      (GdaProvider *provider, GdaConnection *cnc,
                                                        GdaStatement *stmt, GdaSet *params,
                                                        GdaStatementSqlFlag flags,
                                                        GSList **params_used, GError **error);
static gchar              *gda_sqlite_provider_iface_identifier_quote      (GdaProvider *provider, GdaConnection *cnc, /* may be NULL */
                                                        const gchar *id,
                                                        gboolean for_meta_store, gboolean force_quotes);
static GdaSqlStatement    *gda_sqlite_provider_iface_statement_rewrite     (GdaProvider *provider, GdaConnection *cnc,
                                                        GdaStatement *stmt, GdaSet *params, GError **error);
static gboolean            gda_sqlite_provider_iface_open_connection       (GdaProvider *provider, GdaConnection *cnc,
                                                        GdaQuarkList *params, GdaQuarkList *auth);
static gboolean            gda_sqlite_provider_iface_prepare_connection          (GdaProvider *provider, GdaConnection *cnc,
                                                        GdaQuarkList *params, GdaQuarkList *auth);
static gboolean            gda_sqlite_provider_iface_close_connection      (GdaProvider *provider, GdaConnection *cnc);
static gchar              *gda_sqlite_provider_iface_escape_string         (GdaProvider *provider, GdaConnection *cnc,
                                                        const gchar *str); /* may be NULL */
static gchar              *gda_sqlite_provider_iface_unescape_string       (GdaProvider *provider, GdaConnection *cnc,
                                                        const gchar *str); /* may be NULL */
static gboolean            gda_sqlite_provider_iface_perform_operation     (GdaProvider *provider, GdaConnection *cnc, /* may be NULL */
                                                        GdaServerOperation *op, GError **error);
static gboolean            gda_sqlite_provider_iface_begin_transaction     (GdaProvider *provider, GdaConnection *cnc,
                                                        const gchar *name, GdaTransactionIsolation level,
                                                        GError **error);
static gboolean            gda_sqlite_provider_iface_commit_transaction    (GdaProvider *provider, GdaConnection *cnc,
                                                        const gchar *name, GError **error);
static gboolean            gda_sqlite_provider_iface_rollback_transaction  (GdaProvider *provider, GdaConnection *cnc,
                                                        const gchar *name, GError **error);
static gboolean            gda_sqlite_provider_iface_add_savepoint         (GdaProvider *provider, GdaConnection *cnc,
                                                        const gchar *name, GError **error);
static gboolean            gda_sqlite_provider_iface_rollback_savepoint    (GdaProvider *provider, GdaConnection *cnc,
                                                        const gchar *name, GError **error);
static gboolean            gda_sqlite_provider_iface_delete_savepoint      (GdaProvider *provider, GdaConnection *cnc,
                                                        const gchar *name, GError **error);
static gboolean            gda_sqlite_provider_iface_statement_prepare     (GdaProvider *provider, GdaConnection *cnc,
                                                        GdaStatement *stmt, GError **error);
static GObject            *gda_sqlite_provider_iface_statement_execute     (GdaProvider *provider, GdaConnection *cnc,
                                                        GdaStatement *stmt, GdaSet *params,
                                                        GdaStatementModelUsage model_usage,
                                                        GType *col_types, GdaSet **last_inserted_row,
                                                        GError **error);
static GdaSet             *gda_sqlite_provider_iface_get_last_inserted     (GdaProvider *provider, GdaConnection *cnc,
                                                        GError **error);


static void
gda_sqlite_provider_iface_init (GdaProviderInterface *iface)
{
  iface->get_name = gda_sqlite_provider_iface_get_name;
  iface->get_version = gda_sqlite_provider_iface_get_version;
  iface->get_server_version = gda_sqlite_provider_iface_get_server_version;
  iface->supports_feature = gda_sqlite_provider_iface_supports_feature;
  iface->create_parser = gda_sqlite_provider_iface_create_parser;
  iface->create_connection = gda_sqlite_provider_iface_create_connection;
  iface->get_data_handler = gda_sqlite_provider_iface_get_data_handler;
  iface->get_def_dbms_type = gda_sqlite_provider_iface_get_def_dbms_type;
  iface->supports_operation = gda_sqlite_provider_iface_supports_operation;
  iface->create_operation = gda_sqlite_provider_iface_create_operation;
  iface->render_operation = gda_sqlite_provider_iface_render_operation;
  iface->statement_to_sql = gda_sqlite_provider_iface_statement_to_sql;
  iface->identifier_quote = gda_sqlite_provider_iface_identifier_quote;
  iface->statement_rewrite = gda_sqlite_provider_iface_statement_rewrite;
  iface->open_connection = gda_sqlite_provider_iface_open_connection;
  iface->prepare_connection = gda_sqlite_provider_iface_prepare_connection;
  iface->close_connection = gda_sqlite_provider_iface_close_connection;
  iface->escape_string = gda_sqlite_provider_iface_escape_string;
  iface->unescape_string = gda_sqlite_provider_iface_unescape_string;
  iface->perform_operation = gda_sqlite_provider_iface_perform_operation;
  iface->begin_transaction = gda_sqlite_provider_iface_begin_transaction;
  iface->commit_transaction = gda_sqlite_provider_iface_commit_transaction;
  iface->rollback_transaction = gda_sqlite_provider_iface_rollback_transaction;
  iface->add_savepoint = gda_sqlite_provider_iface_add_savepoint;
  iface->rollback_savepoint = gda_sqlite_provider_iface_rollback_savepoint;
  iface->delete_savepoint = gda_sqlite_provider_iface_delete_savepoint;
  iface->statement_prepare = gda_sqlite_provider_iface_statement_prepare;
  iface->statement_execute = gda_sqlite_provider_iface_statement_execute;
  iface->get_last_inserted = gda_sqlite_provider_iface_get_last_inserted;
}

/*
 * Prepared internal statements
 */
typedef enum {
	INTERNAL_PRAGMA_INDEX_LIST,
	INTERNAL_PRAGMA_INDEX_INFO,
	INTERNAL_PRAGMA_FK_LIST,
	INTERNAL_PRAGMA_TABLE_INFO,
	INTERNAL_SELECT_A_TABLE_ROW,
	INTERNAL_SELECT_ALL_TABLES,
	INTERNAL_SELECT_A_TABLE,
	INTERNAL_PRAGMA_PROC_LIST,
	INTERNAL_PRAGMA_EMPTY_RESULT,

	INTERNAL_BEGIN,
	INTERNAL_BEGIN_NAMED,
	INTERNAL_COMMIT,
	INTERNAL_COMMIT_NAMED,
	INTERNAL_ROLLBACK,
	INTERNAL_ROLLBACK_NAMED,

	INTERNAL_ADD_SAVEPOINT,
	INTERNAL_ROLLBACK_SAVEPOINT,
	INTERNAL_RELEASE_SAVEPOINT
} InternalStatementItem;

static gchar *internal_sql[] = {
	"PRAGMA index_list(##tblname::string)",
	"PRAGMA index_info(##idxname::string)",
	"PRAGMA foreign_key_list (##tblname::string)",
	"PRAGMA table_info (##tblname::string)",
	"SELECT * FROM ##tblname::string LIMIT 1",
	"SELECT name as 'Table', 'system' as 'Owner', ' ' as 'Description', sql as 'Definition' FROM (SELECT * FROM sqlite_master UNION ALL SELECT * FROM sqlite_temp_master) WHERE type = ##type::string AND name not like 'sqlite_%%' ORDER BY name",
	"SELECT name as 'Table', 'system' as 'Owner', ' ' as 'Description', sql as 'Definition' FROM (SELECT * FROM sqlite_master UNION ALL SELECT * FROM sqlite_temp_master) WHERE type = ##type::string AND name = ##tblname::string AND name not like 'sqlite_%%' ORDER BY name",
	"PRAGMA proc_list",
	"PRAGMA empty_result_callbacks = ON",

	"BEGIN TRANSACTION",
	"BEGIN TRANSACTION ##name::string",
	"COMMIT TRANSACTION",
	"COMMIT TRANSACTION ##name::string",
	"ROLLBACK TRANSACTION",
	"ROLLBACK TRANSACTION ##name::string",

	"SAVEPOINT ##name::string",
	"ROLLBACK TO ##name::string",
	"RELEASE ##name::string"
};

static gchar *
get_table_nth_column_name (GdaServerProvider *provider, GdaConnection *cnc, const gchar *table_name, gint pos)
{
	GdaSet *params_set = NULL;
	GdaDataModel *model;
	gchar *fname = NULL;
	GdaStatement *stm;
  GdaSqliteProviderPrivate *priv = gda_sqlite_provider_get_instance_private (GDA_SQLITE_PROVIDER (provider));

	g_assert (table_name);
	params_set = gda_set_new_inline (1, "tblname", G_TYPE_STRING, table_name);
	stm = (GdaStatement*) g_ptr_array_index (priv->internal_stmt, INTERNAL_PRAGMA_TABLE_INFO);
	model = gda_connection_statement_execute_select (cnc,
							 stm,
							 params_set, NULL);
	g_object_unref (params_set);
	if (model) {
		const GValue *cvalue;
		cvalue = gda_data_model_get_value_at (model, 1, pos, NULL);
		if (cvalue)
			fname = g_value_dup_string (cvalue);
		g_object_unref (model);
	}
	return fname;
}

/*
 * GdaSqliteProvider class implementation
 */
GdaServerProviderBase sqlite_base_functions = {
	gda_sqlite_provider_get_name,
	gda_sqlite_provider_get_version,
	gda_sqlite_provider_get_server_version,
	gda_sqlite_provider_supports_feature,
	gda_sqlite_provider_create_worker,
	NULL,
	gda_sqlite_provider_create_parser,
	gda_sqlite_provider_get_data_handler,
	gda_sqlite_provider_get_default_dbms_type,
	gda_sqlite_provider_supports_operation,
	gda_sqlite_provider_create_operation,
	gda_sqlite_provider_render_operation,
	gda_sqlite_provider_statement_to_sql,
	NULL,
	gda_sqlite_provider_statement_rewrite,
	gda_sqlite_provider_open_connection,
	gda_sqlite_provider_prepare_connection,
	gda_sqlite_provider_close_connection,
	gda_sqlite_provider_escape_string,
	gda_sqlite_provider_unescape_string,
	gda_sqlite_provider_perform_operation,
	gda_sqlite_provider_begin_transaction,
	gda_sqlite_provider_commit_transaction,
	gda_sqlite_provider_rollback_transaction,
	gda_sqlite_provider_add_savepoint,
	gda_sqlite_provider_rollback_savepoint,
	gda_sqlite_provider_delete_savepoint,
	gda_sqlite_provider_statement_prepare,
	gda_sqlite_provider_statement_execute,

	NULL, NULL, NULL, NULL, /* padding */
};

GdaServerProviderMeta sqlite_meta_functions = {
	_gda_sqlite_meta__info,
	_gda_sqlite_meta__btypes,
	_gda_sqlite_meta__udt,
	_gda_sqlite_meta_udt,
	_gda_sqlite_meta__udt_cols,
	_gda_sqlite_meta_udt_cols,
	_gda_sqlite_meta__enums,
	_gda_sqlite_meta_enums,
	_gda_sqlite_meta__domains,
	_gda_sqlite_meta_domains,
	_gda_sqlite_meta__constraints_dom,
	_gda_sqlite_meta_constraints_dom,
	_gda_sqlite_meta__el_types,
	_gda_sqlite_meta_el_types,
	_gda_sqlite_meta__collations,
	_gda_sqlite_meta_collations,
	_gda_sqlite_meta__character_sets,
	_gda_sqlite_meta_character_sets,
	_gda_sqlite_meta__schemata,
	_gda_sqlite_meta_schemata,
	_gda_sqlite_meta__tables_views,
	_gda_sqlite_meta_tables_views,
	_gda_sqlite_meta__columns,
	_gda_sqlite_meta_columns,
	_gda_sqlite_meta__view_cols,
	_gda_sqlite_meta_view_cols,
	_gda_sqlite_meta__constraints_tab,
	_gda_sqlite_meta_constraints_tab,
	_gda_sqlite_meta__constraints_ref,
	_gda_sqlite_meta_constraints_ref,
	_gda_sqlite_meta__key_columns,
	_gda_sqlite_meta_key_columns,
	_gda_sqlite_meta__check_columns,
	_gda_sqlite_meta_check_columns,
	_gda_sqlite_meta__triggers,
	_gda_sqlite_meta_triggers,
	_gda_sqlite_meta__routines,
	_gda_sqlite_meta_routines,
	_gda_sqlite_meta__routine_col,
	_gda_sqlite_meta_routine_col,
	_gda_sqlite_meta__routine_par,
	_gda_sqlite_meta_routine_par,
	_gda_sqlite_meta__indexes_tab,
        _gda_sqlite_meta_indexes_tab,
        _gda_sqlite_meta__index_cols,
        _gda_sqlite_meta_index_cols,

	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, /* padding */
};

static void
gda_sqlite_provider_get_property (G_GNUC_UNUSED GObject    *object,
                          guint       property_id,
                          GValue     *value,
                          G_GNUC_UNUSED GParamSpec *pspec)
{
	GdaSqliteProviderPrivate *priv = gda_sqlite_provider_get_instance_private (GDA_SQLITE_PROVIDER (object));
	switch (property_id) {
	case PROP_IS_DEFAULT:
		g_value_set_boolean (value, priv->is_default);
		break;
	case PROP_CONNECTION:
		g_value_set_object (value, NULL);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

static void
gda_sqlite_provider_set_property (G_GNUC_UNUSED GObject      *object,
                          guint         property_id,
                          G_GNUC_UNUSED const GValue *value,
                          G_GNUC_UNUSED GParamSpec   *pspec)
{
	GdaSqliteProviderPrivate *priv = gda_sqlite_provider_get_instance_private (GDA_SQLITE_PROVIDER (object));
	switch (property_id) {
	case PROP_IS_DEFAULT:
		priv->is_default = g_value_get_boolean (value);
		break;
	case PROP_CONNECTION:
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

static void
gda_sqlite_provider_dispose (GObject *object)
{
  GdaSqliteProvider *prov = GDA_SQLITE_PROVIDER (object);
  GdaSqliteProviderPrivate *priv = gda_sqlite_provider_get_instance_private (prov);
  g_ptr_array_unref (priv->internal_stmt);
  priv->internal_stmt = NULL;
}

static void
gda_sqlite_provider_class_init (GdaSqliteProviderClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->get_property = gda_sqlite_provider_get_property;
	object_class->set_property = gda_sqlite_provider_set_property;
	object_class->dispose = gda_sqlite_provider_dispose;


	g_object_class_install_property (object_class, PROP_IS_DEFAULT,
		g_param_spec_boolean ("is-default", "Is Default Provider",
													_("Set to TRUE if the provider is the internal default"),
													TRUE,
													(G_PARAM_WRITABLE | G_PARAM_EXPLICIT_NOTIFY)));
	/* set virtual functions */
	gda_server_provider_set_impl_functions (GDA_SERVER_PROVIDER_CLASS (klass),
						GDA_SERVER_PROVIDER_FUNCTIONS_BASE,
						(gpointer) &sqlite_base_functions);
	gda_server_provider_set_impl_functions (GDA_SERVER_PROVIDER_CLASS (klass),
						GDA_SERVER_PROVIDER_FUNCTIONS_META,
						(gpointer) &sqlite_meta_functions);
	gda_server_provider_set_impl_functions (GDA_SERVER_PROVIDER_CLASS (klass),
						GDA_SERVER_PROVIDER_FUNCTIONS_XA,
						NULL);
	g_object_class_override_property (G_OBJECT_CLASS (klass), PROP_CONNECTION, "connection");

#ifdef HAVE_SQLITE
	GModule *module2;

	module2 = gda_sqlite_find_library ("libsqlite3");
	if (module2)
		gda_sqlite_load_symbols (module2, &s3r);
	if (s3r == NULL) {
		g_warning (_("Can't find libsqlite3." G_MODULE_SUFFIX " file."));
	}
	klass->get_api = gda_sqlite_provider_get_api_internal;
#endif
}

static void
gda_sqlite_provider_init (GdaSqliteProvider *sqlite_prv)
{
  GdaSqliteProviderPrivate *priv = gda_sqlite_provider_get_instance_private (sqlite_prv);
	priv->internal_stmt = g_ptr_array_new_full (1, (GDestroyNotify) g_object_unref);

	InternalStatementItem i;
	GdaSqlParser *parser;
	parser = gda_server_provider_internal_get_parser ((GdaServerProvider*) sqlite_prv);

	for (i = INTERNAL_PRAGMA_INDEX_LIST; i < sizeof (internal_sql) / sizeof (gchar*); i++) {
		GdaStatement *stm;
		stm = gda_sql_parser_parse_string (parser, internal_sql[i], NULL, NULL);
		if (stm != NULL) {
			g_ptr_array_add (priv->internal_stmt, stm);
		} else {
			g_error ("Could not parse internal statement: %s\n", internal_sql[i]);
		}
	}
	/* Set as defualt */
	priv->is_default = TRUE;

	/* meta data init */
	_gda_sqlite_provider_meta_init ((GdaServerProvider*) sqlite_prv);
}

static GdaWorker *
gda_sqlite_provider_create_worker (GdaServerProvider *provider, gboolean for_cnc)
{
	/* see http://www.sqlite.org/threadsafe.html */
	GdaSqliteProvider *prov = GDA_SQLITE_PROVIDER (provider);

	static GdaWorker *unique_worker = NULL;
	if (SQLITE3_CALL (prov, sqlite3_threadsafe) ()) {
		if (for_cnc)
			return gda_worker_new ();
		else
			return gda_worker_new_unique (&unique_worker, TRUE);
	}
	else
		return gda_worker_new_unique (&unique_worker, TRUE);
}

/*
 * Get provider name request
 */
static const gchar *
gda_sqlite_provider_get_name (G_GNUC_UNUSED GdaServerProvider *provider)
{
	return PNAME;
}

/*
 * Get provider's version
 */
static const gchar *
gda_sqlite_provider_get_version (G_GNUC_UNUSED GdaServerProvider *provider)
{
	return PACKAGE_VERSION;
}

typedef enum {
	CASE_UP,
	CASE_DOWN,
	CASE_UNCHANGED
} CaseModif;

gchar *
remove_diacritics_and_change_case (const gchar *str, gssize len, CaseModif cmod)
{
	gchar *retval = NULL;

	if (str) {
		GString* string;
		gchar *normstr, *ptr;

		normstr = g_utf8_normalize (str, len, G_NORMALIZE_NFD);
		string = g_string_new ("");
		ptr = normstr;
		while (ptr) {
			gunichar c;
			c = g_utf8_get_char (ptr);
			if (c != '\0') {
				if (!g_unichar_ismark(c)) {
					switch (cmod) {
					case CASE_UP:
						c = g_unichar_toupper (c);
						break;
					case CASE_DOWN:
						c = g_unichar_tolower (c);
						break;
					case CASE_UNCHANGED:
						break;
					}
					g_string_append_unichar (string, c);
				}
				ptr = g_utf8_next_char (ptr);
			}
			else
				break;
		}

		retval = g_string_free (string, FALSE);
		g_free (normstr);
	}

	return retval;
}


/*
 * Open connection request
 */
static gboolean
gda_sqlite_provider_open_connection (GdaServerProvider *provider, GdaConnection *cnc,
				     GdaQuarkList *params, G_GNUC_UNUSED GdaQuarkList *auth)
{
	gchar *filename = NULL;
	const gchar *dirname = NULL, *dbname = NULL;
	const gchar *is_virtual = NULL;
	const gchar *append_extension = NULL;
	gint errmsg;
	SqliteConnectionData *cdata;
	gchar *dup = NULL;
#ifdef SQLITE_HAS_CODEC
	const gchar *passphrase = NULL;
#endif

	g_return_val_if_fail (GDA_IS_SQLITE_PROVIDER (provider), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);

	GdaSqliteProvider *prov = GDA_SQLITE_PROVIDER (provider);

	/* get all parameters received */
	dirname = gda_quark_list_find (params, "DB_DIR");
	if (!dirname)
		dirname="."; /* default to current directory */
	dbname = gda_quark_list_find (params, "DB_NAME");
	append_extension = gda_quark_list_find (params, "APPEND_DB_EXTENSION");
	is_virtual = gda_quark_list_find (params, "_IS_VIRTUAL");

	if (! is_virtual) {
		if (!dbname) {
			const gchar *str;

			str = gda_quark_list_find (params, "URI");
			if (!str) {
				gda_connection_add_event_string (cnc,
								 _("The connection string must contain DB_DIR and DB_NAME values"));
				return FALSE;
			}
			else {
				gint len = strlen (str);
				gint elen = strlen (FILE_EXTENSION);

				if (g_str_has_suffix (str, FILE_EXTENSION)) {
					gchar *ptr;

					dup = g_strdup (str);
					dup [len-elen] = 0;
					for (ptr = dup + (len - elen - 1); (ptr >= dup) && (*ptr != G_DIR_SEPARATOR); ptr--);
					dbname = ptr;
					if (*ptr == G_DIR_SEPARATOR)
						dbname ++;

					if ((*ptr == G_DIR_SEPARATOR) && (ptr > dup)) {
						dirname = dup;
						while ((ptr >= dup) && (*ptr != G_DIR_SEPARATOR))
							ptr--;
						*ptr = 0;
					}
				}
				if (!dbname) {
					gda_connection_add_event_string (cnc,
									 _("The connection string format has changed: replace URI with "
									   "DB_DIR (the path to the database file) and DB_NAME "
									   "(the database file without the '%s' at the end)."), FILE_EXTENSION);
					g_free (dup);
					return FALSE;
				}
				else
					g_warning (_("The connection string format has changed: replace URI with "
						     "DB_DIR (the path to the database file) and DB_NAME "
						     "(the database file without the '%s' at the end)."), FILE_EXTENSION);
			}
		}

		if (! g_ascii_strcasecmp (dbname,":memory:"))
			/* we have an in memory database */
			filename = g_strdup (":memory:");
		else if (! g_ascii_strcasecmp (dbname,"__gda_tmp"))
			/* we have an in memory database */
			filename = NULL/*g_strdup ("")*/;
		else {
			if (!g_file_test (dirname, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR)) {
				gda_connection_add_event_string (cnc,
								 _("The DB_DIR part of the connection string must point "
								   "to a valid directory"));
				g_free (dup);
				return FALSE;
			}

			/* try first with the file extension */
			gchar *tmp, *f1, *f2;
			if (!append_extension ||
			    (append_extension && ((*append_extension == 't') || (*append_extension == 'T'))))
				tmp = g_strdup_printf ("%s%s", dbname, FILE_EXTENSION);
			else
				tmp = g_strdup (dbname);
			f1 = g_build_filename (dirname, tmp, NULL);
			g_free (tmp);
			f2 = g_build_filename (dirname, dbname, NULL);
			if (g_file_test (f1, G_FILE_TEST_EXISTS)) {
				filename = f1;
				f1 = NULL;
			}
			else if (g_file_test (f2, G_FILE_TEST_EXISTS)) {
				filename = f2;
				f2 = NULL;
			}
			else {
				filename = f1;
				f1 = NULL;
			}
			g_free (f1);
			g_free (f2);
			g_free (dup);
		}
	}

	cdata = g_new0 (SqliteConnectionData, 1);
	g_weak_ref_init (&cdata->provider, prov);

	if (filename)
		cdata->file = filename;

	errmsg = SQLITE3_CALL (prov, sqlite3_open_v2) (filename, &cdata->connection, SQLITE_OPEN_FULLMUTEX | SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL);

	if (errmsg != SQLITE_OK) {
		gda_connection_add_event_string (cnc, SQLITE3_CALL (prov, sqlite3_errmsg) (cdata->connection));
		gda_sqlite_free_cnc_data (cdata);

		return FALSE;
	}

#ifdef SQLITE_HAS_CODEC
  /* TODO Fix this compiler warning: the address of 'sqlite3_key' will always evaluate as 'true' */
  /* We disable this warning in the meantime with -Wno-address */

	if (auth)
		passphrase = gda_quark_list_find (auth, "PASSWORD");

	if (passphrase != NULL) {
		errmsg = SQLITE3_CALL (prov,sqlite3_key_v2) (cdata->connection, filename,
		                                       (void*) passphrase, strlen (passphrase));
		if (errmsg != SQLITE_OK) {
			gda_connection_add_event_string (cnc, _("Wrong encryption passphrase"));
			gda_sqlite_free_cnc_data (cdata);
			return FALSE;
		}
	}
#endif

	gda_connection_internal_set_provider_data (cnc, (GdaServerProviderConnectionData*) cdata,
						   (GDestroyNotify) gda_sqlite_free_cnc_data);
	return TRUE;
}

static gboolean
gda_sqlite_provider_prepare_connection (GdaServerProvider *provider, GdaConnection *cnc, GdaQuarkList *params, G_GNUC_UNUSED GdaQuarkList *auth)
{
	SqliteConnectionData *cdata;
	GdaStatement *stm;
  GdaSqliteProviderPrivate *priv = gda_sqlite_provider_get_instance_private (GDA_SQLITE_PROVIDER (provider));

	cdata = (SqliteConnectionData*) gda_connection_internal_get_provider_data_error (cnc, NULL);
	if (!cdata)
		return FALSE;

	GdaSqliteProvider *prov = GDA_SQLITE_PROVIDER (provider);

	const gchar *use_extra_functions = NULL, *with_fk = NULL, *regexp, *locale_collate, *extensions;
	with_fk = gda_quark_list_find (params, "FK");
	use_extra_functions = gda_quark_list_find (params, "EXTRA_FUNCTIONS");
	if (!use_extra_functions)
		use_extra_functions = gda_quark_list_find (params, "LOAD_GDA_FUNCTIONS");
	regexp = gda_quark_list_find (params, "REGEXP");
	locale_collate = gda_quark_list_find (params, "EXTRA_COLLATIONS");
	extensions = gda_quark_list_find (params, "EXTENSIONS");

	/* use extended result codes */
	SQLITE3_CALL (prov, sqlite3_extended_result_codes) (cdata->connection, 1);

	/* allow a busy timeout of 500 ms */
	SQLITE3_CALL (prov, sqlite3_busy_timeout) (cdata->connection, 500);

	/* allow loading extensions using SELECT load_extension ()... */
	if (extensions && ((*extensions == 't') || (*extensions == 'T'))) {
		sqlite3_db_config (cdata->connection, SQLITE_DBCONFIG_ENABLE_LOAD_EXTENSION, 1, NULL);
		g_message (_("SECURITY WARNING: Load Extension is enable for SQL commands, this means an attacker can access extension load capabilities"));
	}

	/* try to prepare all the internal statements */
	InternalStatementItem i;
	for (i = INTERNAL_PRAGMA_INDEX_LIST; i < sizeof (internal_sql) / sizeof (gchar*); i++) {
		stm = (GdaStatement*) g_ptr_array_index (priv->internal_stmt, i);
		gda_connection_statement_prepare (cnc, stm, NULL); /* Note: some may fail because
											   * the SQL cannot be "prepared" */
	}

	/* set SQLite library options */
	GObject *obj;
	GError *lerror = NULL;
	stm = (GdaStatement*) g_ptr_array_index (priv->internal_stmt, INTERNAL_PRAGMA_EMPTY_RESULT);
	obj = gda_connection_statement_execute (cnc, stm,
						NULL, GDA_STATEMENT_MODEL_RANDOM_ACCESS, NULL, &lerror);
	if (!obj) {
		gda_connection_add_event_string (cnc,
						 _("Could not set empty_result_callbacks SQLite option: %s"),
						 lerror && lerror->message ? lerror->message : _("no detail"));
		g_clear_error (&lerror);
	}
	else
		g_object_unref (obj);

	/* make sure the internals are completely initialized now */
	{
		gchar **data = NULL;
		gint ncols;
		gint nrows;
		gint status;
		gchar *errmsg;

		status = SQLITE3_CALL (prov, sqlite3_get_table) (cdata->connection, "SELECT name"
							   " FROM (SELECT * FROM sqlite_master UNION ALL "
							   "       SELECT * FROM sqlite_temp_master)",
							   &data,
							   &nrows,
							   &ncols,
							   &errmsg);
		if (status == SQLITE_OK)
			SQLITE3_CALL (prov, sqlite3_free_table) (data);
		else {
			gda_connection_add_event_string (cnc, errmsg);
			SQLITE3_CALL (prov, sqlite3_free) (errmsg);
			gda_sqlite_free_cnc_data (cdata);
			gda_connection_internal_set_provider_data (cnc, NULL, NULL);
			return FALSE;
		}
	}

	/* set connection parameters */
	gboolean enforce_fk = TRUE;
	if (with_fk && ((*with_fk == 'f') || (*with_fk == 'F')))
		enforce_fk = FALSE;
	int res;
	sqlite3_stmt *pStmt;
	if (enforce_fk)
		res = SQLITE3_CALL (prov, sqlite3_prepare_v2) (cdata->connection,
						      "PRAGMA foreign_keys = ON", -1,
						      &pStmt, NULL);
	else
		res = SQLITE3_CALL (prov, sqlite3_prepare_v2) (cdata->connection,
						      "PRAGMA foreign_keys = OFF", -1,
						      &pStmt, NULL);

	if (res != SQLITE_OK) {
		if (with_fk) {
			gda_sqlite_free_cnc_data (cdata);
			gda_connection_internal_set_provider_data (cnc, NULL, NULL);
			return FALSE;
		}
	}
	else {
		res = SQLITE3_CALL (prov, sqlite3_step) (pStmt);
		SQLITE3_CALL (prov, sqlite3_reset) (pStmt);
		SQLITE3_CALL (prov, sqlite3_finalize) (pStmt);
		if (res != SQLITE_DONE) {
			if (with_fk) {
				gda_sqlite_free_cnc_data (cdata);
				gda_connection_internal_set_provider_data (cnc, NULL, NULL);
				return FALSE;
			}
		}
#ifdef GDA_DEBUG_NO
		else {
			if (enforce_fk)
				g_print ("SQLite provider enforces foreign keys.\n");
			else
				g_print ("SQLite provider does not enforce foreign keys.\n");
		}
#endif
	}

	if (priv->is_default) {
		if (!use_extra_functions || ((*use_extra_functions == 't') || (*use_extra_functions == 'T'))) {
			gsize i;

			for (i = 0; i < sizeof (scalars) / sizeof (ScalarFunction); i++) {
				ScalarFunction *func = (ScalarFunction *) &(scalars [i]);
				g_object_ref (prov);
				gint res = (s3r->sqlite3_create_function_v2) (cdata->connection,
											 func->name, func->nargs,
											 SQLITE_UTF8, prov,
											 func->xFunc, NULL, NULL, g_object_unref);
				if (res != SQLITE_OK) {
					gda_connection_add_event_string (cnc, _("Could not register function '%s'"),
									 func->name);
					gda_sqlite_free_cnc_data (cdata);
					gda_connection_internal_set_provider_data (cnc, NULL, NULL);
					return FALSE;
				}
			}
		}

		if (!regexp || ((*regexp == 't') || (*regexp == 'T'))) {
			gsize i;

			for (i = 0; i < sizeof (regexp_functions) / sizeof (ScalarFunction); i++) {
				ScalarFunction *func = (ScalarFunction *) &(regexp_functions [i]);
				g_object_ref (prov);
				gint res = (s3r->sqlite3_create_function_v2) (cdata->connection,
											 func->name, func->nargs,
											 SQLITE_UTF8, prov,
											 func->xFunc, NULL, NULL, g_object_unref);
				if (res != SQLITE_OK) {
					gda_connection_add_event_string (cnc, _("Could not register function '%s'"),
									 func->name);
					gda_sqlite_free_cnc_data (cdata);
					gda_connection_internal_set_provider_data (cnc, NULL, NULL);
					return FALSE;
				}
			}
		}

		if (! locale_collate || ((*locale_collate == 't') || (*locale_collate == 'T'))) {
			gsize i;
			for (i = 0; i < sizeof (collation_functions) / sizeof (CollationFunction); i++) {
				CollationFunction *func = (CollationFunction*) &(collation_functions [i]);
				gint res;
				res = (s3r->sqlite3_create_collation) (cdata->connection, func->name,
										     SQLITE_UTF8, prov, func->xFunc);
				if (res != SQLITE_OK) {
					gda_connection_add_event_string (cnc,
									 _("Could not define the %s collation"),
									 func->name);
					gda_sqlite_free_cnc_data (cdata);
					gda_connection_internal_set_provider_data (cnc, NULL, NULL);
					return FALSE;
				}
			}
		}
	}

	return TRUE;
}

static int
locale_collate_func (G_GNUC_UNUSED void *pArg,
		     int nKey1, const void *pKey1,
		     int nKey2, const void *pKey2)
{
	gchar *tmp1, *tmp2;
	int res;
	tmp1 = g_utf8_collate_key ((gchar*) pKey1, nKey1);
	tmp2 = g_utf8_collate_key ((gchar*) pKey2, nKey2);
	res = strcmp (tmp1, tmp2);
	g_free (tmp1);
	g_free (tmp2);
	return res;
}

static int
dcase_collate_func (G_GNUC_UNUSED void *pArg,
		    int nKey1, const void *pKey1,
		    int nKey2, const void *pKey2)
{
	gchar *tmp1, *tmp2;
	int res;
	tmp1 = remove_diacritics_and_change_case ((gchar*) pKey1, nKey1, CASE_DOWN);
	tmp2 = remove_diacritics_and_change_case ((gchar*) pKey2, nKey2, CASE_DOWN);
	res = strcmp (tmp1, tmp2);
	g_free (tmp1);
	g_free (tmp2);
	return res;
}

/*
 * Close connection request
 */
static gboolean
gda_sqlite_provider_close_connection (GdaServerProvider *provider, GdaConnection *cnc)
{
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);

	return TRUE;
}

/*
 * Server version request
 */
static const gchar *
gda_sqlite_provider_get_server_version (GdaServerProvider *provider, GdaConnection *cnc)
{
	static gchar *version_string = NULL;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, NULL);

	gda_lockable_lock (GDA_LOCKABLE(provider));
	if (!version_string)
		version_string = g_strdup_printf ("SQLite version %s", SQLITE_VERSION);
	gda_lockable_unlock (GDA_LOCKABLE(provider));

	return (const gchar *) version_string;
}

/*
 * Support operation request
 */
static gboolean
gda_sqlite_provider_supports_operation (G_GNUC_UNUSED GdaServerProvider *provider, G_GNUC_UNUSED GdaConnection *cnc,
					GdaServerOperationType type, G_GNUC_UNUSED GdaSet *options)
{
        switch (type) {
        case GDA_SERVER_OPERATION_CREATE_DB:
        case GDA_SERVER_OPERATION_DROP_DB:

        case GDA_SERVER_OPERATION_CREATE_TABLE:
        case GDA_SERVER_OPERATION_DROP_TABLE:
        case GDA_SERVER_OPERATION_RENAME_TABLE:

        case GDA_SERVER_OPERATION_ADD_COLUMN:
        case GDA_SERVER_OPERATION_RENAME_COLUMN:

        case GDA_SERVER_OPERATION_CREATE_INDEX:
        case GDA_SERVER_OPERATION_DROP_INDEX:

        case GDA_SERVER_OPERATION_CREATE_VIEW:
        case GDA_SERVER_OPERATION_DROP_VIEW:
		return TRUE;
        default:
                return FALSE;
        }
}

/*
 * Create operation request
 */
static GdaServerOperation *
gda_sqlite_provider_create_operation (GdaServerProvider *provider, GdaConnection *cnc,
				      GdaServerOperationType type,
				      G_GNUC_UNUSED GdaSet *options, G_GNUC_UNUSED GError **error)
{
  gchar *file;
  GdaServerOperation *op;
  gchar *str;
  
  file = g_strdup_printf (PNAME "_specs_%s", gda_server_operation_op_type_to_string (type));
  str = g_utf8_strdown (file, -1);
 
  g_free (file);
  file = NULL;

  gchar *lpname;
  lpname = g_utf8_strdown (PNAME, -1);
  file = g_strdup_printf ("/spec/%s/%s.raw.xml", lpname, str);
  g_free (lpname);

  op = GDA_SERVER_OPERATION (g_object_new (GDA_TYPE_SERVER_OPERATION, 
                                           "op-type", type, 
                                           "spec-resource", file, 
                                           "provider", provider, 
                                           "connection", cnc,
                                           NULL));

  g_free (file);
  g_free (str);

  return op;
}

/*
 * Render operation request
 */
static gchar *
gda_sqlite_provider_render_operation (GdaServerProvider *provider, GdaConnection *cnc,
				      GdaServerOperation *op, GError **error)
{
  gchar *sql = NULL;
  gchar *str;
	str = g_strdup_printf ("/spec/" PNAME "/" PNAME "_specs_%s.raw.xml",
			gda_server_operation_op_type_to_string (gda_server_operation_get_op_type (op)));
	gchar *res = g_utf8_strdown (str, -1);
	g_free (str);
	GBytes *contents;
	contents = g_resources_lookup_data (res,
					    G_RESOURCE_LOOKUP_FLAGS_NONE,
					    error);
	if (contents == NULL) {
		g_assert (*error != NULL);
#ifdef GDA_DEBUG
		g_print ("Error at getting specs '%s': %s\n", res, (*error)->message);
#endif
		g_free (str);
		g_free (res);
		return NULL;
	}
	g_free (res);
	/* else: TO_IMPLEMENT */
	g_bytes_unref (contents);

	/* actual rendering */
        switch (gda_server_operation_get_op_type (op)) {
        case GDA_SERVER_OPERATION_CREATE_DB:
        case GDA_SERVER_OPERATION_DROP_DB:
		sql = NULL;
                break;
        case GDA_SERVER_OPERATION_CREATE_TABLE:
                sql = _gda_sqlite_render_CREATE_TABLE (provider, cnc, op, error);
                break;
        case GDA_SERVER_OPERATION_DROP_TABLE:
                sql = _gda_sqlite_render_DROP_TABLE (provider, cnc, op, error);
                break;
        case GDA_SERVER_OPERATION_RENAME_TABLE:
                sql = _gda_sqlite_render_RENAME_TABLE (provider, cnc, op, error);
                break;
        case GDA_SERVER_OPERATION_ADD_COLUMN:
                sql = _gda_sqlite_render_ADD_COLUMN (provider, cnc, op, error);
                break;
        case GDA_SERVER_OPERATION_RENAME_COLUMN:
                sql = _gda_sqlite_render_RENAME_COLUMN (provider, cnc, op, error);
                break;
        case GDA_SERVER_OPERATION_CREATE_INDEX:
                sql = _gda_sqlite_render_CREATE_INDEX (provider, cnc, op, error);
                break;
        case GDA_SERVER_OPERATION_DROP_INDEX:
                sql = _gda_sqlite_render_DROP_INDEX (provider, cnc, op, error);
                break;
        case GDA_SERVER_OPERATION_CREATE_VIEW:
                sql = _gda_sqlite_render_CREATE_VIEW (provider, cnc, op, error);
                break;
        case GDA_SERVER_OPERATION_DROP_VIEW:
                sql = _gda_sqlite_render_DROP_VIEW (provider, cnc, op, error);
                break;

        default:
                g_assert_not_reached ();
        }
        return sql;
}

/*
 * Perform operation request
 */
static gboolean
gda_sqlite_provider_perform_operation (GdaServerProvider *provider, GdaConnection *cnc,
				       GdaServerOperation *op, GError **error)
{
        GdaServerOperationType optype;
        optype = gda_server_operation_get_op_type (op);
	switch (optype) {
	case GDA_SERVER_OPERATION_CREATE_DB: {
		const GValue *value;
		const gchar *dbname = NULL, *append_extension = NULL;
		const gchar *dir = NULL;
		SqliteConnectionData *cdata;
		gint errmsg;
		gchar *filename, *tmp;
		gboolean retval = TRUE;
		GdaSqliteProvider *prov = GDA_SQLITE_PROVIDER (provider);

		value = gda_server_operation_get_value_at (op, "/DB_DEF_P/DB_NAME");
                if (value && G_VALUE_HOLDS (value, G_TYPE_STRING) && g_value_get_string (value))
                        dbname = g_value_get_string (value);
		value = gda_server_operation_get_value_at (op, "/DB_DEF_P/APPEND_DB_EXTENSION");
                if (value && G_VALUE_HOLDS (value, G_TYPE_STRING) && g_value_get_string (value))
                        append_extension = g_value_get_string (value);
		value = gda_server_operation_get_value_at (op, "/DB_DEF_P/DB_DIR");
                if (value && G_VALUE_HOLDS (value, G_TYPE_STRING) && g_value_get_string (value))
                        dir = g_value_get_string (value);

		if (!append_extension ||
		    (append_extension && ((*append_extension == 't') || (*append_extension == 'T'))))
			tmp = g_strdup_printf ("%s%s", dbname, FILE_EXTENSION);
		else
			tmp = g_strdup (dbname);
		filename = g_build_filename (dir, tmp, NULL);
		g_free (tmp);

		cdata = g_new0 (SqliteConnectionData, 1);
		errmsg = SQLITE3_CALL (prov, sqlite3_open) (filename, &cdata->connection);
		g_free (filename);

		if (errmsg != SQLITE_OK) {
			g_set_error (error, GDA_SERVER_PROVIDER_ERROR,
				     GDA_SERVER_PROVIDER_PREPARE_STMT_ERROR,
				     "%s", SQLITE3_CALL (prov, sqlite3_errmsg) (cdata->connection));
			retval = FALSE;
		}

#ifdef SQLITE_HAS_CODEC
		value = gda_server_operation_get_value_at (op, "/DB_DEF_P/PASSWORD");
		if (value && G_VALUE_HOLDS (value, G_TYPE_STRING) &&
		    g_value_get_string (value) &&
		    *g_value_get_string (value)) {
			const gchar *passphrase = g_value_get_string (value);
			errmsg = SQLITE3_CALL (prov, sqlite3_key_v2) (cdata->connection, cdata->file, (void*) passphrase,
							     strlen (passphrase));
			if (errmsg != SQLITE_OK) {
				g_set_error (error, GDA_SERVER_PROVIDER_ERROR,
					     GDA_SERVER_PROVIDER_INTERNAL_ERROR,
					     "%s", SQLITE3_CALL (prov, sqlite3_errmsg) (cdata->connection));
				retval = FALSE;
			}
			else {
				/* create some contents */
				int res;
				sqlite3_stmt *pStmt;
				res = SQLITE3_CALL (prov, sqlite3_prepare_v2) (cdata->connection,
								      "CREATE TABLE data (id int)", -1,
								      &pStmt, NULL);

				if (res != SQLITE_OK) {
					g_set_error (error, GDA_SERVER_PROVIDER_ERROR,
						     GDA_SERVER_PROVIDER_INTERNAL_ERROR,
						     "%s",
						     _("Error initializing database with passphrase"));
					retval = FALSE;
					goto outcontents;
				}
				res = SQLITE3_CALL (prov, sqlite3_step) (pStmt);
				SQLITE3_CALL (prov, sqlite3_reset) (pStmt);
				SQLITE3_CALL (prov, sqlite3_finalize) (pStmt);
				if (res != SQLITE_DONE) {
					g_set_error (error, GDA_SERVER_PROVIDER_ERROR,
						     GDA_SERVER_PROVIDER_INTERNAL_ERROR,
						     "%s",
						     _("Error initializing database with passphrase"));
					retval = FALSE;
					goto outcontents;
					/* end */
				}

				res = SQLITE3_CALL (prov, sqlite3_prepare_v2) (cdata->connection,
								      "DROP TABLE data", -1,
								      &pStmt, NULL);

				if (res != SQLITE_OK) {
					g_set_error (error, GDA_SERVER_PROVIDER_ERROR,
						     GDA_SERVER_PROVIDER_INTERNAL_ERROR,
						     "%s",
						     _("Error initializing database with passphrase"));
					retval = FALSE;
					goto outcontents;
				}
				res = SQLITE3_CALL (prov, sqlite3_step) (pStmt);
				SQLITE3_CALL (prov, sqlite3_reset) (pStmt);
				SQLITE3_CALL (prov, sqlite3_finalize) (pStmt);
				if (res != SQLITE_DONE) {
					g_set_error (error, GDA_SERVER_PROVIDER_ERROR,
						     GDA_SERVER_PROVIDER_INTERNAL_ERROR,
						     "%s",
						     _("Error initializing database with passphrase"));
					retval = FALSE;
					goto outcontents;
					/* end */
				}
			outcontents:
				;
			}
		}
#endif
		gda_sqlite_free_cnc_data (cdata);

		return retval;
        }
	case GDA_SERVER_OPERATION_DROP_DB: {
		const GValue *value;
		const gchar *dbname = NULL;
		const gchar *dir = NULL;
		gboolean retval = TRUE;

		value = gda_server_operation_get_value_at (op, "/DB_DESC_P/DB_NAME");
                if (value && G_VALUE_HOLDS (value, G_TYPE_STRING) && g_value_get_string (value))
                        dbname = g_value_get_string (value);
		value = gda_server_operation_get_value_at (op, "/DB_DESC_P/DB_DIR");
                if (value && G_VALUE_HOLDS (value, G_TYPE_STRING) && g_value_get_string (value))
                        dir = g_value_get_string (value);

		if (dbname && dir) {
			gchar *filename, *tmp;
			tmp = g_strdup_printf ("%s%s", dbname, FILE_EXTENSION);
			filename = (gchar *) g_build_filename (dir, tmp, NULL);
			g_free (tmp);

			if (g_unlink (filename)) {
				g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_OPERATION_ERROR,
					      "%s", g_strerror (errno));
				retval = FALSE;
			}
			g_free (filename);
		}
		else {
			g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_OPERATION_ERROR,
				      "%s", _("Missing database name or directory"));
			retval = FALSE;
		}

		return retval;
	}
        default:
                /* use the SQL from the provider to perform the action */
		return gda_server_provider_perform_operation_default (provider, cnc, op, error);
	}
}

/*
 * Begin transaction request
 */
static gboolean
gda_sqlite_provider_begin_transaction (GdaServerProvider *provider, GdaConnection *cnc,
				       const gchar *name, G_GNUC_UNUSED GdaTransactionIsolation level,
				       GError **error)
{
	gboolean status = TRUE;
	GdaStatement *stm;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);
  GdaSqliteProviderPrivate *priv = gda_sqlite_provider_get_instance_private (GDA_SQLITE_PROVIDER (provider));

	if (gda_connection_get_options (cnc) & GDA_CONNECTION_OPTIONS_READ_ONLY) {
		gda_connection_add_event_string (cnc, _("Transactions are not supported in read-only mode"));
		return FALSE;
	}

	if (name) {
		GdaSet *params_set = NULL;
		gda_lockable_lock (GDA_LOCKABLE(provider));
		if (!params_set)
			params_set = gda_set_new_inline (1, "name", G_TYPE_STRING, name);
		else if (! gda_set_set_holder_value (params_set, error, "name", name))
			status = FALSE;
		stm = (GdaStatement*) g_ptr_array_index (priv->internal_stmt, INTERNAL_BEGIN_NAMED);
		if (status && gda_connection_statement_execute_non_select (cnc, stm,
									   params_set, NULL, error) == -1)
			status = FALSE;
		g_object_unref (params_set);
		gda_lockable_unlock (GDA_LOCKABLE(provider));
	}
	else {
		stm = (GdaStatement*) g_ptr_array_index (priv->internal_stmt, INTERNAL_BEGIN);
		if (gda_connection_statement_execute_non_select (cnc, stm,
								 NULL, NULL, error) == -1)
			status = FALSE;
	}

	/*g_print ("%s(%p) => %s\n", __FUNCTION__, cnc, status ? "TRUE" : "FALSE");*/
	return status;
}

/*
 * Commit transaction request
 */
static gboolean
gda_sqlite_provider_commit_transaction (GdaServerProvider *provider, GdaConnection *cnc,
					const gchar *name, GError **error)
{
	gboolean status = TRUE;
	GdaStatement *stm;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);
  GdaSqliteProviderPrivate *priv = gda_sqlite_provider_get_instance_private (GDA_SQLITE_PROVIDER (provider));

	if (name) {
		GdaSet *params_set = NULL;
		gda_lockable_lock (GDA_LOCKABLE(provider));
		if (!params_set)
			params_set = gda_set_new_inline (1, "name", G_TYPE_STRING, name);
		else if (!gda_set_set_holder_value (params_set, error, "name", name))
			status = FALSE;
		stm = (GdaStatement*) g_ptr_array_index (priv->internal_stmt, INTERNAL_COMMIT_NAMED);
		if (status && gda_connection_statement_execute_non_select (cnc, stm,
									   params_set, NULL, error) == -1)
			status = FALSE;
		g_object_unref (params_set);
		gda_lockable_unlock (GDA_LOCKABLE(provider));
	}
	else {
		stm = (GdaStatement*) g_ptr_array_index (priv->internal_stmt, INTERNAL_COMMIT);
		if (gda_connection_statement_execute_non_select (cnc, stm,
								 NULL, NULL, error) == -1)
			status = FALSE;
	}

	/*g_print ("%s(%p) => %s\n", __FUNCTION__, cnc, status ? "TRUE" : "FALSE");*/
	return status;
}

/*
 * Rollback transaction request
 */
static gboolean
gda_sqlite_provider_rollback_transaction (GdaServerProvider *provider,
					  GdaConnection *cnc,
					  const gchar *name, GError **error)
{
	gboolean status = TRUE;
	GdaStatement *stm;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);
  GdaSqliteProviderPrivate *priv = gda_sqlite_provider_get_instance_private (GDA_SQLITE_PROVIDER (provider));

	if (name) {
		GdaSet *params_set = NULL;
		gda_lockable_lock (GDA_LOCKABLE(provider));
		if (!params_set)
			params_set = gda_set_new_inline (1, "name", G_TYPE_STRING, name);
		else if (! gda_set_set_holder_value (params_set, error, "name", name))
			status = FALSE;
		stm = (GdaStatement*) g_ptr_array_index (priv->internal_stmt, INTERNAL_ROLLBACK_NAMED);
		if (status && gda_connection_statement_execute_non_select (cnc, stm,
									   params_set, NULL, error) == -1)
			status = FALSE;
		g_object_unref (params_set);
		gda_lockable_unlock (GDA_LOCKABLE(provider));
	}
	else {
		stm = (GdaStatement*) g_ptr_array_index (priv->internal_stmt, INTERNAL_ROLLBACK);
		if (gda_connection_statement_execute_non_select (cnc, stm,
								 NULL, NULL, error) == -1)
			status = FALSE;
	}

	/*g_print ("%s(%p) => %s\n", __FUNCTION__, cnc, status ? "TRUE" : "FALSE");*/
	return status;
}

static gboolean
gda_sqlite_provider_add_savepoint (GdaServerProvider *provider, GdaConnection *cnc,
				   const gchar *name, GError **error)
{
	gboolean status = TRUE;
	GdaStatement *stm;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);
	g_return_val_if_fail (name && *name, FALSE);
  GdaSqliteProviderPrivate *priv = gda_sqlite_provider_get_instance_private (GDA_SQLITE_PROVIDER (provider));

	GdaSet *params_set = NULL;
	gda_lockable_lock (GDA_LOCKABLE(provider));
	if (!params_set)
		params_set = gda_set_new_inline (1, "name", G_TYPE_STRING, name);
	else if (! gda_set_set_holder_value (params_set, error, "name", name))
		status = FALSE;
	stm = (GdaStatement*) g_ptr_array_index (priv->internal_stmt, INTERNAL_ADD_SAVEPOINT);
	if (status && gda_connection_statement_execute_non_select (cnc, stm,
								   params_set, NULL, error) == -1)
		status = FALSE;
	g_object_unref (params_set);
	gda_lockable_unlock (GDA_LOCKABLE(provider));

	/*g_print ("%s(%p) => %s\n", __FUNCTION__, cnc, status ? "TRUE" : "FALSE");*/
	return status;
}

static gboolean
gda_sqlite_provider_rollback_savepoint (GdaServerProvider *provider, GdaConnection *cnc,
					const gchar *name, GError **error)
{
	gboolean status = TRUE;
	GdaStatement *stm;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);
	g_return_val_if_fail (name && *name, FALSE);
  GdaSqliteProviderPrivate *priv = gda_sqlite_provider_get_instance_private (GDA_SQLITE_PROVIDER (provider));

	GdaSet *params_set = NULL;
	gda_lockable_lock (GDA_LOCKABLE(provider));
	if (!params_set)
		params_set = gda_set_new_inline (1, "name", G_TYPE_STRING, name);
	else if (! gda_set_set_holder_value (params_set, error, "name", name))
		status = FALSE;
	stm = (GdaStatement*) g_ptr_array_index (priv->internal_stmt, INTERNAL_ROLLBACK_SAVEPOINT);
	if (status && gda_connection_statement_execute_non_select (cnc, stm,
								   params_set, NULL, error) == -1)
		status = FALSE;
	g_object_unref (params_set);
	gda_lockable_unlock (GDA_LOCKABLE(provider));

	/*g_print ("%s(%p) => %s\n", __FUNCTION__, cnc, status ? "TRUE" : "FALSE");*/
	return status;
}

static gboolean
gda_sqlite_provider_delete_savepoint (GdaServerProvider *provider, GdaConnection *cnc,
				      const gchar *name, GError **error)
{
	gboolean status = TRUE;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);
	g_return_val_if_fail (name && *name, FALSE);
  GdaSqliteProviderPrivate *priv = gda_sqlite_provider_get_instance_private (GDA_SQLITE_PROVIDER (provider));

	GdaSet *params_set = NULL;
	GdaStatement *stm;
	gda_lockable_lock (GDA_LOCKABLE(provider));
	if (!params_set)
		params_set = gda_set_new_inline (1, "name", G_TYPE_STRING, name);
	else if (! gda_set_set_holder_value (params_set, error, "name", name))
		status = FALSE;
	stm = (GdaStatement*) g_ptr_array_index (priv->internal_stmt, INTERNAL_RELEASE_SAVEPOINT);
	if (status && gda_connection_statement_execute_non_select (cnc, stm,
								   params_set, NULL, error) == -1) {
		status = FALSE;
	}
	g_object_unref (params_set);
	gda_lockable_unlock (GDA_LOCKABLE(provider));

	/*g_print ("%s(%p) => %s\n", __FUNCTION__, cnc, status ? "TRUE" : "FALSE");*/
	return status;
}

/*
 * Feature support request
 */
static gboolean
gda_sqlite_provider_supports_feature (GdaServerProvider *provider,
				      GdaConnection *cnc,
				      GdaConnectionFeature feature)
{
	if (cnc) {
		g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
		g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);
	}

	switch (feature) {
	case GDA_CONNECTION_FEATURE_SQL :
	case GDA_CONNECTION_FEATURE_TRANSACTIONS :
	case GDA_CONNECTION_FEATURE_AGGREGATES :
	case GDA_CONNECTION_FEATURE_INDEXES :
	case GDA_CONNECTION_FEATURE_TRIGGERS :
	case GDA_CONNECTION_FEATURE_VIEWS :
	case GDA_CONNECTION_FEATURE_PROCEDURES :
		return TRUE;
	default:
		return FALSE;
	}
}

/*
 * Get data handler request
 */
static GdaDataHandler *
gda_sqlite_provider_get_data_handler (GdaServerProvider *provider, GdaConnection *cnc,
				      GType type, G_GNUC_UNUSED const gchar *dbms_type)
{
	GdaDataHandler *dh = NULL;

	if (cnc) {
		g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
		g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);
	}

	if (type == G_TYPE_INVALID) {
		TO_IMPLEMENT; /* use @dbms_type */
		dh = NULL;
	}
	else if (type == G_TYPE_STRING) {
		dh = gda_server_provider_handler_find (provider, cnc, type, NULL);
		if (!dh) {
			dh = gda_handler_string_new_with_provider (provider, cnc);
			if (dh) {
				gda_server_provider_handler_declare (provider, dh, cnc, G_TYPE_STRING, NULL);
				g_object_unref (dh);
			}
		}
	}
	else if (type == GDA_TYPE_BINARY) {
		dh = gda_server_provider_handler_find (provider, cnc, type, NULL);
		if (!dh) {
			dh = _gda_sqlite_handler_bin_new ();
			if (dh) {
				gda_server_provider_handler_declare (provider, dh, cnc, GDA_TYPE_BINARY, NULL);
				g_object_unref (dh);
			}
		}
	}
	else if ((type == GDA_TYPE_TIME) ||
		 (type == G_TYPE_DATE_TIME) ||
		 (type == G_TYPE_DATE)) {
		dh = gda_server_provider_handler_find (provider, NULL, type, NULL);
		if (!dh) {
			dh = gda_handler_time_new ();
			/* SQLite format is 'YYYY-MM-DD HH:MM:SS.SSS' */
			gda_handler_time_set_sql_spec (GDA_HANDLER_TIME (dh), G_DATE_YEAR, G_DATE_MONTH,
						       G_DATE_DAY, '-', FALSE);
			gda_handler_time_set_str_spec (GDA_HANDLER_TIME (dh), G_DATE_YEAR, G_DATE_MONTH,
						       G_DATE_DAY, '-', FALSE);
			gda_server_provider_handler_declare (provider, dh, NULL, GDA_TYPE_TIME, NULL);
			gda_server_provider_handler_declare (provider, dh, NULL, G_TYPE_DATE_TIME, NULL);
			gda_server_provider_handler_declare (provider, dh, NULL, G_TYPE_DATE, NULL);
			g_object_unref (dh);
		}
	}
	else if (type == G_TYPE_BOOLEAN) {
		dh = gda_server_provider_handler_find (provider, cnc, type, NULL);
		if (!dh) {
			dh = _gda_sqlite_handler_boolean_new ();
			if (dh) {
				gda_server_provider_handler_declare (provider, dh, cnc, G_TYPE_BOOLEAN, NULL);
				g_object_unref (dh);
			}
		}
	}
	else
		dh = gda_server_provider_handler_use_default (provider, type);

	return dh;
}

/*
 * Get default DBMS type request
 */
static const gchar*
gda_sqlite_provider_get_default_dbms_type (G_GNUC_UNUSED GdaServerProvider *provider, G_GNUC_UNUSED GdaConnection *cnc, GType type)
{
	if ((type == G_TYPE_INT64) ||
	    (type == G_TYPE_INT) ||
	    (type == GDA_TYPE_SHORT) ||
	    (type == GDA_TYPE_USHORT) ||
	    (type == G_TYPE_CHAR) ||
	    (type == G_TYPE_UCHAR) ||
	    (type == G_TYPE_ULONG) ||
	    (type == G_TYPE_LONG) ||
	    (type == G_TYPE_UINT) ||
	    (type == G_TYPE_UINT64))
		return "integer";

	if (type == GDA_TYPE_BINARY)
		return "blob";

	if (type == G_TYPE_BOOLEAN)
		return "boolean";

	if ((type == GDA_TYPE_GEOMETRIC_POINT) ||
	    (type == G_TYPE_OBJECT) ||
	    (type == G_TYPE_STRING) ||
	    (type == GDA_TYPE_TEXT) ||
	    (type == G_TYPE_INVALID))
		return "text";

	if ((type == G_TYPE_DOUBLE) ||
	    (type == GDA_TYPE_NUMERIC) ||
	    (type == G_TYPE_FLOAT))
		return "real";

	if (type == GDA_TYPE_TIME)
		return "time";
	if (type == G_TYPE_DATE_TIME)
		return "timestamp";
	if (type == G_TYPE_DATE)
		return "date";

	if ((type == GDA_TYPE_NULL) ||
	    (type == G_TYPE_GTYPE))
		return NULL;

	return "text";
}

/*
 * Create parser request
 */
static GdaSqlParser *
gda_sqlite_provider_create_parser (G_GNUC_UNUSED GdaServerProvider *provider, G_GNUC_UNUSED GdaConnection *cnc)
{
	return (GdaSqlParser*) g_object_new (GDA_TYPE_SQL_PARSER, "tokenizer-flavour", GDA_SQL_PARSER_FLAVOUR_SQLITE, NULL);
}


/*
 * GdaStatement to SQL request
 */
static gchar *sqlite_render_compound (GdaSqlStatementCompound *stmt, GdaSqlRenderingContext *context, GError **error);

static gchar *sqlite_render_operation (GdaSqlOperation *op, GdaSqlRenderingContext *context, GError **error);
static gchar *sqlite_render_expr (GdaSqlExpr *expr, GdaSqlRenderingContext *context,
				  gboolean *is_default, gboolean *is_null,
				  GError **error);
static gchar *sqlite_render_distinct (GdaSqlStatementSelect *stmt,
				      GdaSqlRenderingContext *context, GError **error);

static gchar *
gda_sqlite_provider_statement_to_sql (GdaServerProvider *provider, GdaConnection *cnc,
				      GdaStatement *stmt, GdaSet *params, GdaStatementSqlFlag flags,
				      GSList **params_used, GError **error)
{
	gchar *str;
	GdaSqlRenderingContext context;

	g_return_val_if_fail (GDA_IS_STATEMENT (stmt), NULL);
	if (cnc) {
		g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
		g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, NULL);
	}

	memset (&context, 0, sizeof (context));
	context.provider = provider;
	context.cnc = cnc;
	context.params = params;
	context.flags = flags;
	context.render_operation = (GdaSqlRenderingFunc) sqlite_render_operation; /* specific REGEXP rendering */
	context.render_compound = (GdaSqlRenderingFunc) sqlite_render_compound; /* don't surround each SELECT with parenthesis, EXCEPT ALL and INTERSECT ALL are not supported */
	context.render_expr = sqlite_render_expr; /* render "FALSE" as 0 and TRUE as 1 */
	context.render_distinct = (GdaSqlRenderingFunc) sqlite_render_distinct; /* DISTINCT ON (...) is not supported */

	str = gda_statement_to_sql_real (stmt, &context, error);

	if (str) {
		if (params_used)
			*params_used = context.params_used;
		else
			g_slist_free (context.params_used);
	}
	else {
		if (params_used)
			*params_used = NULL;
		g_slist_free (context.params_used);
	}
	return str;
}

/*
 * The difference with the default implementation is that DISTINCT ON (...) is not supported
 */
static gchar *
sqlite_render_distinct (GdaSqlStatementSelect *stmt, GdaSqlRenderingContext *context, GError **error)
{
	if (stmt->distinct) {
		if (stmt->distinct_expr) {
			g_set_error (error, GDA_SERVER_PROVIDER_ERROR,
				     GDA_SERVER_PROVIDER_NON_SUPPORTED_ERROR,
				     "%s", _("SQLite does not support specifying fields to apply DISTINCT clause on"));
			return NULL;
		}
		else {
			gchar *tmp;
			tmp = g_strdup ("DISTINCT\n");
			if (! (context->flags & GDA_STATEMENT_SQL_PRETTY))
				tmp [8] = 0;
			return tmp;
		}
	}
	else
		return NULL;
}

/*
 * The difference with the default implementation is to avoid surrounding each SELECT with parenthesis
 * and that EXCEPT ALL and INTERSECT ALL are not supported
 */
static gchar *
sqlite_render_compound (GdaSqlStatementCompound *stmt, GdaSqlRenderingContext *context, GError **error)
{
	GString *string;
	gchar *str;
	GSList *list;

	g_return_val_if_fail (stmt, NULL);
	g_return_val_if_fail (GDA_SQL_ANY_PART (stmt)->type == GDA_SQL_ANY_STMT_COMPOUND, NULL);

	string = g_string_new ("");

	for (list = stmt->stmt_list; list; list = list->next) {
		GdaSqlStatement *sqlstmt = (GdaSqlStatement*) list->data;
		if (list != stmt->stmt_list) {
			switch (stmt->compound_type) {
			case GDA_SQL_STATEMENT_COMPOUND_UNION:
				g_string_append (string, " UNION ");
				break;
			case GDA_SQL_STATEMENT_COMPOUND_UNION_ALL:
				g_string_append (string, " UNION ALL ");
				break;
			case GDA_SQL_STATEMENT_COMPOUND_INTERSECT:
				g_string_append (string, " INTERSECT ");
				break;
			case GDA_SQL_STATEMENT_COMPOUND_INTERSECT_ALL:
				g_set_error (error, GDA_SERVER_PROVIDER_ERROR,
					     GDA_SERVER_PROVIDER_NON_SUPPORTED_ERROR,
					     _("'%s' compound not supported by SQLite"),
					     "INTERSECT ALL");
				goto err;
				break;
			case GDA_SQL_STATEMENT_COMPOUND_EXCEPT:
				g_string_append (string, " EXCEPT ");
				break;
			case GDA_SQL_STATEMENT_COMPOUND_EXCEPT_ALL:
				g_set_error (error, GDA_SERVER_PROVIDER_ERROR,
					     GDA_SERVER_PROVIDER_NON_SUPPORTED_ERROR,
					     _("'%s' compound not supported by SQLite"),
					     "EXCEPT ALL");
				goto err;
				break;
			default:
				g_assert_not_reached ();
			}
		}
		switch (sqlstmt->stmt_type) {
		case GDA_SQL_ANY_STMT_SELECT:
			str = context->render_select (GDA_SQL_ANY_PART (sqlstmt->contents), context, error);
			if (!str) goto err;
			g_string_append (string, str);
			g_free (str);
			break;
		case GDA_SQL_ANY_STMT_COMPOUND:
			str = context->render_compound (GDA_SQL_ANY_PART (sqlstmt->contents), context, error);
			if (!str) goto err;
			g_string_append (string, str);
			g_free (str);
			break;
		default:
			g_assert_not_reached ();
		}
	}

	str = string->str;
	g_string_free (string, FALSE);
	return str;

 err:
	g_string_free (string, TRUE);
	return NULL;
}

/* The difference with the default implementation is specific REGEXP rendering */
static gchar *
sqlite_render_operation (GdaSqlOperation *op, GdaSqlRenderingContext *context, GError **error)
{
	gchar *str;
	GSList *list;
	GSList *sql_list; /* list of SqlOperand */
	GString *string;
	gchar *multi_op = NULL;

	typedef struct {
		gchar    *sql;
		gboolean  is_null;
		gboolean  is_default;
		gboolean  is_composed;
	} SqlOperand;
#define SQL_OPERAND(x) ((SqlOperand*)(x))

	g_return_val_if_fail (op, NULL);
	g_return_val_if_fail (GDA_SQL_ANY_PART (op)->type == GDA_SQL_ANY_SQL_OPERATION, NULL);

	/* can't have:
	 *  - op->operands == NULL
	 *  - incorrect number of operands
	 */
	if (!gda_sql_any_part_check_structure (GDA_SQL_ANY_PART (op), error)) {
		g_assert (*error != NULL);
		return NULL;
	}

	/* render operands */
	for (list = op->operands, sql_list = NULL; list; list = list->next) {
		SqlOperand *sqlop = g_new0 (SqlOperand, 1);
		GdaSqlExpr *expr = (GdaSqlExpr*) list->data;
		str = context->render_expr (expr, context, &(sqlop->is_default), &(sqlop->is_null), error);
		if (!str) {
			g_free (sqlop);
			g_assert (*error != NULL);
			goto out;
		}
		sqlop->sql = str;
		if (expr->cond || expr->case_s || expr->select)
			sqlop->is_composed = TRUE;
		sql_list = g_slist_prepend (sql_list, sqlop);
	}
	sql_list = g_slist_reverse (sql_list);

	str = NULL;
	switch (op->operator_type) {
	case GDA_SQL_OPERATOR_TYPE_EQ:
		if (SQL_OPERAND (sql_list->next->data)->is_null)
			str = g_strdup_printf ("%s IS NULL", SQL_OPERAND (sql_list->data)->sql);
		else
			str = g_strdup_printf ("%s = %s", SQL_OPERAND (sql_list->data)->sql, SQL_OPERAND (sql_list->next->data)->sql);
		break;
	case GDA_SQL_OPERATOR_TYPE_IS:
		str = g_strdup_printf ("%s IS %s", SQL_OPERAND (sql_list->data)->sql, SQL_OPERAND (sql_list->next->data)->sql);
		break;
	case GDA_SQL_OPERATOR_TYPE_LIKE:
		str = g_strdup_printf ("%s LIKE %s", SQL_OPERAND (sql_list->data)->sql, SQL_OPERAND (sql_list->next->data)->sql);
		break;
	case GDA_SQL_OPERATOR_TYPE_NOTLIKE:
		str = g_strdup_printf ("%s NOT LIKE %s", SQL_OPERAND (sql_list->data)->sql, SQL_OPERAND (sql_list->next->data)->sql);
		break;
	case GDA_SQL_OPERATOR_TYPE_ILIKE:
	case GDA_SQL_OPERATOR_TYPE_NOTILIKE:
		g_set_error (error, GDA_STATEMENT_ERROR, GDA_STATEMENT_SYNTAX_ERROR,
			     "%s", _("ILIKE operation not supported"));
		goto out;
		break;
	case GDA_SQL_OPERATOR_TYPE_GT:
		str = g_strdup_printf ("%s > %s", SQL_OPERAND (sql_list->data)->sql, SQL_OPERAND (sql_list->next->data)->sql);
		break;
	case GDA_SQL_OPERATOR_TYPE_LT:
		str = g_strdup_printf ("%s < %s", SQL_OPERAND (sql_list->data)->sql, SQL_OPERAND (sql_list->next->data)->sql);
		break;
	case GDA_SQL_OPERATOR_TYPE_GEQ:
		str = g_strdup_printf ("%s >= %s", SQL_OPERAND (sql_list->data)->sql, SQL_OPERAND (sql_list->next->data)->sql);
		break;
	case GDA_SQL_OPERATOR_TYPE_LEQ:
		str = g_strdup_printf ("%s <= %s", SQL_OPERAND (sql_list->data)->sql, SQL_OPERAND (sql_list->next->data)->sql);
		break;
	case GDA_SQL_OPERATOR_TYPE_DIFF:
		str = g_strdup_printf ("%s != %s", SQL_OPERAND (sql_list->data)->sql, SQL_OPERAND (sql_list->next->data)->sql);
		break;
	case GDA_SQL_OPERATOR_TYPE_REGEXP:
		str = g_strdup_printf ("regexp (%s, %s)", SQL_OPERAND (sql_list->next->data)->sql, SQL_OPERAND (sql_list->data)->sql);
		break;
	case GDA_SQL_OPERATOR_TYPE_REGEXP_CI:
		str = g_strdup_printf ("regexp (%s, %s, 'i')", SQL_OPERAND (sql_list->next->data)->sql,
				       SQL_OPERAND (sql_list->data)->sql);
		break;
	case GDA_SQL_OPERATOR_TYPE_NOT_REGEXP_CI:
		str = g_strdup_printf ("NOT regexp (%s, %s, 'i')", SQL_OPERAND (sql_list->next->data)->sql,
				       SQL_OPERAND (sql_list->data)->sql);
    break;
	case GDA_SQL_OPERATOR_TYPE_SIMILAR:
		/* does not exist in SQLite => error */
		g_set_error (error, GDA_STATEMENT_ERROR, GDA_STATEMENT_SYNTAX_ERROR,
			     "%s", _("SIMILAR operation not supported"));
		goto out;
		break;
	case GDA_SQL_OPERATOR_TYPE_NOT_REGEXP:
		str = g_strdup_printf ("NOT regexp (%s, %s)", SQL_OPERAND (sql_list->next->data)->sql,
				       SQL_OPERAND (sql_list->data)->sql);
		break;
	case GDA_SQL_OPERATOR_TYPE_REM:
		str = g_strdup_printf ("%s %% %s", SQL_OPERAND (sql_list->data)->sql, SQL_OPERAND (sql_list->next->data)->sql);
		break;
	case GDA_SQL_OPERATOR_TYPE_DIV:
		str = g_strdup_printf ("%s / %s", SQL_OPERAND (sql_list->data)->sql, SQL_OPERAND (sql_list->next->data)->sql);
		break;
	case GDA_SQL_OPERATOR_TYPE_BITAND:
		str = g_strdup_printf ("%s & %s", SQL_OPERAND (sql_list->data)->sql, SQL_OPERAND (sql_list->next->data)->sql);
		break;
	case GDA_SQL_OPERATOR_TYPE_BITOR:
		str = g_strdup_printf ("%s | %s", SQL_OPERAND (sql_list->data)->sql, SQL_OPERAND (sql_list->next->data)->sql);
		break;
	case GDA_SQL_OPERATOR_TYPE_BETWEEN:
		str = g_strdup_printf ("%s BETWEEN %s AND %s", SQL_OPERAND (sql_list->data)->sql,
				       SQL_OPERAND (sql_list->next->data)->sql,
				       SQL_OPERAND (sql_list->next->next->data)->sql);
		break;
	case GDA_SQL_OPERATOR_TYPE_ISNULL:
		str = g_strdup_printf ("%s IS NULL", SQL_OPERAND (sql_list->data)->sql);
		break;
	case GDA_SQL_OPERATOR_TYPE_ISNOTNULL:
		str = g_strdup_printf ("%s IS NOT NULL", SQL_OPERAND (sql_list->data)->sql);
		break;
	case GDA_SQL_OPERATOR_TYPE_BITNOT:
		str = g_strdup_printf ("~ %s", SQL_OPERAND (sql_list->data)->sql);
		break;
	case GDA_SQL_OPERATOR_TYPE_NOT:
		str = g_strdup_printf ("NOT %s", SQL_OPERAND (sql_list->data)->sql);
		break;
	case GDA_SQL_OPERATOR_TYPE_IN:
	case GDA_SQL_OPERATOR_TYPE_NOTIN: {
		gboolean add_p = TRUE;
		if (sql_list->next && !(sql_list->next->next) &&
		    *(SQL_OPERAND (sql_list->next->data)->sql)=='(')
			add_p = FALSE;
	  string = g_string_new ("");
		string = g_string_append (string, SQL_OPERAND (sql_list->data)->sql);
		if (op->operator_type == GDA_SQL_OPERATOR_TYPE_IN)
			g_string_append (string, " IN ");
		else
			g_string_append (string, " NOT IN ");
		if (add_p)
			g_string_append_c (string, '(');
		for (list = sql_list->next; list; list = list->next) {
			if (list != sql_list->next)
				g_string_append (string, ", ");
			g_string_append (string, SQL_OPERAND (list->data)->sql);
		}
		if (add_p)
			g_string_append_c (string, ')');
		str = string->str;
		g_string_free (string, FALSE);
    string = NULL;
		break;
	}
	case GDA_SQL_OPERATOR_TYPE_CONCAT:
		multi_op = "||";
		break;
	case GDA_SQL_OPERATOR_TYPE_PLUS:
		multi_op = "+";
		break;
	case GDA_SQL_OPERATOR_TYPE_MINUS:
		multi_op = "-";
		break;
	case GDA_SQL_OPERATOR_TYPE_STAR:
		multi_op = "*";
		break;
	case GDA_SQL_OPERATOR_TYPE_AND:
		multi_op = "AND";
		break;
	case GDA_SQL_OPERATOR_TYPE_OR:
		multi_op = "OR";
		break;
	default:
		g_assert_not_reached ();
		break;
	}

	if (multi_op) {
		if (!sql_list->next) {
			/* 1 operand only */
			string = g_string_new ("");
			g_string_append_printf (string, "%s %s", multi_op, SQL_OPERAND (sql_list->data)->sql);
		}
		else {
			/* 2 or more operands */
			if (SQL_OPERAND (sql_list->data)->is_composed) {
				string = g_string_new ("(");
				g_string_append (string, SQL_OPERAND (sql_list->data)->sql);
				g_string_append_c (string, ')');
			}
			else
				string = g_string_new (SQL_OPERAND (sql_list->data)->sql);
			for (list = sql_list->next; list; list = list->next) {
				g_string_append_printf (string, " %s ", multi_op);
				if (SQL_OPERAND (list->data)->is_composed) {
					g_string_append_c (string, '(');
					g_string_append (string, SQL_OPERAND (list->data)->sql);
					g_string_append_c (string, ')');
				}
				else
					g_string_append (string, SQL_OPERAND (list->data)->sql);
			}
		}
		str = string->str;
		g_string_free (string, FALSE);
	}

 out:
	for (list = sql_list; list; list = list->next) {
		g_free (((SqlOperand*)list->data)->sql);
		g_free (list->data);
	}
	g_slist_free (sql_list);

	if (str == NULL && *error == NULL)
		g_set_error (error, GDA_STATEMENT_ERROR, GDA_STATEMENT_SYNTAX_ERROR,
			     "%s", _("Operator type not supported"));
	g_assert (str != NULL);
	return str;
}

/*
 * The difference with the default implementation is to render TRUE and FALSE as 0 and 1
 */
static gchar *
sqlite_render_expr (GdaSqlExpr *expr, GdaSqlRenderingContext *context, gboolean *is_default,
		    gboolean *is_null, GError **error)
{
	GString *string;
	gchar *str = NULL;

	g_return_val_if_fail (expr, NULL);
	g_return_val_if_fail (GDA_SQL_ANY_PART (expr)->type == GDA_SQL_ANY_EXPR, NULL);

	if (is_default)
		*is_default = FALSE;
	if (is_null)
		*is_null = FALSE;

	/* can't have:
	 *  - expr->cast_as && expr->param_spec
	 */
	if (!gda_sql_any_part_check_structure (GDA_SQL_ANY_PART (expr), error)) return NULL;

	string = g_string_new ("");
	if (expr->param_spec) {
		str = context->render_param_spec (expr->param_spec, expr, context, is_default, is_null, error);
		if (!str) goto err;
	}
	else if (expr->value) {
		if (G_VALUE_TYPE (expr->value) == G_TYPE_STRING) {
			/* specific treatment for strings, see documentation about GdaSqlExpr's value attribute */
			const gchar *vstr;
			vstr = g_value_get_string (expr->value);
			if (vstr) {
				if (expr->value_is_ident) {
					gchar **ids_array;
					gint i;
					GString *string = NULL;
					GdaConnectionOptions cncoptions = 0;
					if (context->cnc)
						g_object_get (G_OBJECT (context->cnc), "options", &cncoptions, NULL);
					ids_array = gda_sql_identifier_split (vstr);
					if (!ids_array)
						str = g_strdup (vstr);
					else if (!(ids_array[0])) goto err;
					else {
						for (i = 0; ids_array[i]; i++) {
							gchar *tmp;
							if (!string)
								string = g_string_new ("");
							else
								g_string_append_c (string, '.');
							tmp = gda_sql_identifier_quote (ids_array[i], context->cnc,
											context->provider, FALSE,
											cncoptions & GDA_CONNECTION_OPTIONS_SQL_IDENTIFIERS_CASE_SENSITIVE);
							g_string_append (string, tmp);
							g_free (tmp);
						}
						g_strfreev (ids_array);
						str = g_string_free (string, FALSE);
					}
				}
				else {
					/* we don't have an identifier */
					if (!g_ascii_strcasecmp (vstr, "default")) {
						if (is_default)
							*is_default = TRUE;
						str = g_strdup ("DEFAULT");
					}
					else if (!g_ascii_strcasecmp (vstr, "FALSE")) {
						g_free (str);
						str = g_strdup ("0");
					}
					else if (!g_ascii_strcasecmp (vstr, "TRUE")) {
						g_free (str);
						str = g_strdup ("1");
					}
					else
						str = g_strdup (vstr);
				}
			}
			else {
				str = g_strdup ("NULL");
				if (is_null)
					*is_null = TRUE;
			}
		}
		if (!str) {
			/* use a GdaDataHandler to render the value as valid SQL */
			GdaDataHandler *dh;
			if (context->cnc) {
				GdaServerProvider *prov;
				prov = gda_connection_get_provider (context->cnc);
				dh = gda_server_provider_get_data_handler_g_type (prov, context->cnc,
										  G_VALUE_TYPE (expr->value));
				if (!dh)
				  goto err;
				else
				  g_object_ref (dh);
			}
			else
				dh = gda_data_handler_get_default (G_VALUE_TYPE (expr->value));

			if (dh) {
				str = gda_data_handler_get_sql_from_value (dh, expr->value);
				g_object_unref (dh);
			}
			else
				str = gda_value_stringify (expr->value);
			if (!str) goto err;
		}
	}
	else if (expr->func) {
		str = context->render_function (GDA_SQL_ANY_PART (expr->func), context, error);
		if (!str) goto err;
	}
	else if (expr->cond) {
		gchar *tmp;
		tmp = context->render_operation (GDA_SQL_ANY_PART (expr->cond), context, error);
		if (!tmp) goto err;
		str = NULL;
		if (GDA_SQL_ANY_PART (expr)->parent) {
			if (GDA_SQL_ANY_PART (expr)->parent->type == GDA_SQL_ANY_STMT_SELECT) {
				GdaSqlStatementSelect *selst;
				selst = (GdaSqlStatementSelect*) (GDA_SQL_ANY_PART (expr)->parent);
				if ((expr == selst->where_cond) ||
				    (expr == selst->having_cond))
					str = tmp;
			}
			else if (GDA_SQL_ANY_PART (expr)->parent->type == GDA_SQL_ANY_STMT_DELETE) {
				GdaSqlStatementDelete *delst;
				delst = (GdaSqlStatementDelete*) (GDA_SQL_ANY_PART (expr)->parent);
				if (expr == delst->cond)
					str = tmp;
			}
			else if (GDA_SQL_ANY_PART (expr)->parent->type == GDA_SQL_ANY_STMT_UPDATE) {
				GdaSqlStatementUpdate *updst;
				updst = (GdaSqlStatementUpdate*) (GDA_SQL_ANY_PART (expr)->parent);
				if (expr == updst->cond)
					str = tmp;
			}
		}

		if (!str) {
			str = g_strconcat ("(", tmp, ")", NULL);
			g_free (tmp);
		}
	}
	else if (expr->select) {
		gchar *str1;
		if (GDA_SQL_ANY_PART (expr->select)->type == GDA_SQL_ANY_STMT_SELECT)
			str1 = context->render_select (GDA_SQL_ANY_PART (expr->select), context, error);
		else if (GDA_SQL_ANY_PART (expr->select)->type == GDA_SQL_ANY_STMT_COMPOUND)
			str1 = context->render_compound (GDA_SQL_ANY_PART (expr->select), context, error);
		else
			g_assert_not_reached ();
		if (!str1) goto err;

		if (! GDA_SQL_ANY_PART (expr)->parent ||
		    (GDA_SQL_ANY_PART (expr)->parent->type != GDA_SQL_ANY_SQL_FUNCTION)) {
			str = g_strconcat ("(", str1, ")", NULL);
			g_free (str1);
		}
		else
			str = str1;
	}
	else if (expr->case_s) {
		str = context->render_case (GDA_SQL_ANY_PART (expr->case_s), context, error);
		if (!str) goto err;
	}
	else {
		if (is_null)
			*is_null = TRUE;
		str = g_strdup ("NULL");
	}

	if (!str) goto err;

	if (expr->cast_as)
		g_string_append_printf (string, "CAST (%s AS %s)", str, expr->cast_as);
	else
		g_string_append (string, str);
	g_free (str);

	str = string->str;
	g_string_free (string, FALSE);
	return str;

 err:
	g_string_free (string, TRUE);
	return NULL;
}

/*
 * Statement prepare request
 */
static GdaSqlitePStmt *real_prepare (GdaServerProvider *provider, GdaConnection *cnc, GdaStatement *stmt, GError **error);
static gboolean
gda_sqlite_provider_statement_prepare (GdaServerProvider *provider, GdaConnection *cnc,
				       GdaStatement *stmt, GError **error)
{
	GdaSqlitePStmt *ps;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, FALSE);
	g_return_val_if_fail (GDA_IS_STATEMENT (stmt), FALSE);

	/* fetch prepares stmt if already done */
	ps = (GdaSqlitePStmt *) gda_connection_get_prepared_statement (cnc, stmt);
	if (ps)
		return TRUE;

	ps = real_prepare (provider, cnc, stmt, error);
	if (!ps)
		return FALSE;
	else {
		gda_connection_add_prepared_statement (cnc, stmt, (GdaPStmt *) ps);
		g_object_unref (ps);
		return TRUE;
	}
}

/*
 * Creates a new GdaStatement, as a copy of @stmt, and adds new OID columns, one for each table involved
 * in the SELECT
 * The GHashTable created (as @out_hash) contains:
 *  - key = table name
 *  - value = column number in the SELECT (use GPOINTER_TO_INT)
 *
 * This step is required for BLOBs which needs to table's ROWID to be opened. The extra columns
 * won't appear in the final result set.
 */
static GdaStatement *
add_oid_columns (GdaStatement *stmt, GHashTable **out_hash, gint *out_nb_cols_added)
{
	GHashTable *hash = NULL;
	GdaStatement *nstmt;
	GdaSqlStatement *sqlst;
	GdaSqlStatementSelect *sst;
	gint nb_cols_added = 0;
	gint add_index;
	GSList *list;

	*out_hash = NULL;
	*out_nb_cols_added = 0;

	GdaSqlStatementType type;
	type = gda_statement_get_statement_type (stmt);
	if (type == GDA_SQL_STATEMENT_COMPOUND) {
		return g_object_ref (stmt);
	}
	else if (type != GDA_SQL_STATEMENT_SELECT) {
		return g_object_ref (stmt);
	}

	g_object_get (G_OBJECT (stmt), "structure", &sqlst, NULL);
	g_assert (sqlst);
	hash = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
	sst = (GdaSqlStatementSelect*) sqlst->contents;

	if (!sst->from || sst->distinct) {
		gda_sql_statement_free (sqlst);
		return g_object_ref (stmt);
	}

	/* if there is an ORDER BY test if we can alter it */
	if (sst->order_by) {
		for (list = sst->order_by; list; list = list->next) {
			GdaSqlSelectOrder *order = (GdaSqlSelectOrder*) list->data;
			if (order->expr && order->expr->value &&
			    (G_VALUE_TYPE (order->expr->value) != G_TYPE_STRING)) {
				gda_sql_statement_free (sqlst);
				return g_object_ref (stmt);
			}
		}
	}

	add_index = 0;
	for (list = sst->from->targets; list; list = list->next) {
		GdaSqlSelectTarget *target = (GdaSqlSelectTarget*) list->data;
		GdaSqlSelectField *field;

		if (!target->table_name)
			continue;

		/* add a field */
		field = gda_sql_select_field_new (GDA_SQL_ANY_PART (sst));
		sst->expr_list = g_slist_insert (sst->expr_list, field, add_index);

		field->expr = gda_sql_expr_new (GDA_SQL_ANY_PART (field));

		gchar *str, *tmp;
		const gchar *name;
		if (target->as)
			name = target->as;
		else
			name = target->table_name;

		tmp = gda_sql_identifier_quote (name, NULL, NULL, FALSE, FALSE);
		str = g_strdup_printf ("%s.rowid", tmp);
		g_free (tmp);
		g_value_take_string ((field->expr->value = gda_value_new (G_TYPE_STRING)), str);

		/* add to hash table */
		add_index++;
		g_hash_table_insert (hash, gda_sql_identifier_prepare_for_compare (g_strdup (name)),
				     GINT_TO_POINTER (add_index)); /* ADDED 1 to column number,
								    * don't forget to remove 1 when using */
		if (target->as)
			g_hash_table_insert (hash, gda_sql_identifier_prepare_for_compare (g_strdup (target->table_name)),
					     GINT_TO_POINTER (add_index)); /* ADDED 1 to column number,
									    * don't forget to remove 1 when using */
		nb_cols_added ++;
	}

	/* if there is an ORDER BY which uses numbers, then also alter that */
	if (sst->order_by) {
		for (list = sst->order_by; list; list = list->next) {
			GdaSqlSelectOrder *order = (GdaSqlSelectOrder*) list->data;
			if (order->expr && order->expr->value) {
				long i;
				const gchar *cstr;
				gchar *endptr = NULL;
				cstr = g_value_get_string (order->expr->value);
				i = strtol (cstr, (char **) &endptr, 10);
				if (!endptr || !(*endptr)) {
					i += nb_cols_added;
					endptr = g_strdup_printf ("%ld", i);
					g_value_take_string (order->expr->value, endptr);
				}
			}
		}
	}

	/* prepare return */
	nstmt = (GdaStatement*) g_object_new (GDA_TYPE_STATEMENT, "structure", sqlst, NULL);
	gda_sql_statement_free (sqlst);
	/*g_print ("%s(): Added %d columns: %s\n", __FUNCTION__, nb_cols_added, gda_statement_to_sql (nstmt, NULL, NULL));*/

	*out_hash = hash;
	*out_nb_cols_added = nb_cols_added;

	return nstmt;
}

static GdaSqlitePStmt *
real_prepare (GdaServerProvider *provider, GdaConnection *cnc, GdaStatement *stmt, GError **error)
{
	int status;
	SqliteConnectionData *cdata;
	sqlite3_stmt *sqlite_stmt;
	gchar *sql;
	const char *left;
	GdaSqlitePStmt *ps;
	GdaSet *params = NULL;
	GSList *used_params = NULL;

	GdaStatement *real_stmt;
	GHashTable *hash;
	gint nb_rows_added;
	GdaSqliteProvider *prov = GDA_SQLITE_PROVIDER (provider);

	/* get SQLite's private data */
	cdata = (SqliteConnectionData*) gda_connection_internal_get_provider_data_error (cnc, error);
	if (!cdata)
		return NULL;

	/* render as SQL understood by SQLite */
	if (! gda_statement_get_parameters (stmt, &params, error))
		return NULL;

	real_stmt = add_oid_columns (stmt, &hash, &nb_rows_added);
	sql = gda_sqlite_provider_statement_to_sql (provider, cnc, real_stmt, params, GDA_STATEMENT_SQL_PARAMS_AS_QMARK,
						    &used_params, error);
	if (!sql)
		goto out_err;

	/* prepare statement */
	status = SQLITE3_CALL (prov, sqlite3_prepare_v2) (cdata->connection, sql, -1, &sqlite_stmt, &left);
	if (status != SQLITE_OK) {
		g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_PREPARE_STMT_ERROR,
			     "%s", SQLITE3_CALL (prov, sqlite3_errmsg) (cdata->connection));
		goto out_err;
	}

	if (left && (*left != 0))
		g_warning ("SQlite SQL: %s (REMAIN:%s)\n", sql, left);

	/* make a list of the parameter names used in the statement */
	GSList *param_ids = NULL;
	if (used_params) {
		GSList *list;
		for (list = used_params; list; list = list->next) {
			const gchar *cid;
			cid = gda_holder_get_id (GDA_HOLDER (list->data));
			if (cid) {
				param_ids = g_slist_append (param_ids, g_strdup (cid));
				/*g_print ("PREPARATION: param ID: %s\n", cid);*/
			}
			else {
				g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_PREPARE_STMT_ERROR,
					      "%s", _("Unnamed parameter is not allowed in prepared statements"));
				g_slist_free_full (param_ids, (GDestroyNotify) g_free);
				goto out_err;
			}
		}
		g_slist_free (used_params);
	}
	if (params)
		g_object_unref (params);

	/* create a prepared statement object */
	ps = _gda_sqlite_pstmt_new (GDA_SQLITE_PROVIDER (provider), sqlite_stmt);
	gda_pstmt_set_gda_statement (_GDA_PSTMT (ps), stmt);
	gda_pstmt_set_param_ids (_GDA_PSTMT (ps), param_ids);
	gda_pstmt_set_sql (_GDA_PSTMT (ps), sql);
	_gda_sqlite_pstmt_set_rowid_hash (ps, hash);
	_gda_sqlite_pstmt_set_nb_rowid_columns (ps, nb_rows_added);
	g_object_unref (real_stmt);
	/*g_print ("%s(%s) => GdaSqlitePStmt %p\n", __FUNCTION__, sql, ps);*/
  g_free (sql);
	if (hash)
		g_hash_table_destroy (hash);
	return ps;

 out_err:
	if (hash)
		g_hash_table_destroy (hash);
	g_object_unref (real_stmt);
	if (used_params)
		g_slist_free (used_params);
	if (params)
		g_object_unref (params);
  if (sql != NULL) {
    g_free (sql);
  }
	return NULL;
}

#define MAKE_LAST_INSERTED_SET_ID "__gda"
static void
lir_stmt_reset_cb (GdaStatement *stmt, G_GNUC_UNUSED gpointer data)
{
	/* get rid of the SELECT statement used in make_last_inserted_set() */
	g_object_set_data ((GObject*) stmt, MAKE_LAST_INSERTED_SET_ID, NULL);
}

static GdaSet *
make_last_inserted_set (GdaConnection *cnc, GdaStatement *stmt, sqlite3_int64 last_id)
{
	GError *lerror = NULL;

	GdaStatement *statement;
	statement = g_object_get_data ((GObject*) stmt, MAKE_LAST_INSERTED_SET_ID);
	if (!statement) {
		/* analyze @stmt */
		GdaSqlStatement *sql_insert;
		GdaSqlStatementInsert *insert;
		if (gda_statement_get_statement_type (stmt) != GDA_SQL_STATEMENT_INSERT)
			/* unable to compute anything */
			return NULL;
		g_object_get (G_OBJECT (stmt), "structure", &sql_insert, NULL);
		g_assert (sql_insert);
		insert = (GdaSqlStatementInsert *) sql_insert->contents;

		/* build corresponding SELECT statement */
		GdaSqlStatementSelect *select;
		GdaSqlSelectTarget *target;
		GdaSqlStatement *sql_statement = gda_sql_statement_new (GDA_SQL_STATEMENT_SELECT);

		select = (GdaSqlStatementSelect*) sql_statement->contents;

		/* FROM */
		select->from = gda_sql_select_from_new (GDA_SQL_ANY_PART (select));
		target = gda_sql_select_target_new (GDA_SQL_ANY_PART (select->from));
		gda_sql_select_from_take_new_target (select->from, target);

		/* Filling in the target */
		GValue *value;
		g_value_set_string ((value = gda_value_new (G_TYPE_STRING)), insert->table->table_name);
		gda_sql_select_target_take_table_name (target, value);
		gda_sql_statement_free (sql_insert);

		/* selected fields */
		GdaSqlSelectField *field;
		GSList *fields_list = NULL;

		field = gda_sql_select_field_new (GDA_SQL_ANY_PART (select));
		g_value_set_string ((value = gda_value_new (G_TYPE_STRING)), "*");
		gda_sql_select_field_take_star_value (field, value);
		fields_list = g_slist_append (fields_list, field);

		gda_sql_statement_select_take_expr_list (sql_statement, fields_list);

		/* WHERE */
		GdaSqlExpr *where, *expr;
		GdaSqlOperation *cond;
		where = gda_sql_expr_new (GDA_SQL_ANY_PART (select));
		cond = gda_sql_operation_new (GDA_SQL_ANY_PART (where));
		where->cond = cond;
		cond->operator_type = GDA_SQL_OPERATOR_TYPE_EQ;
		expr = gda_sql_expr_new (GDA_SQL_ANY_PART (cond));
		g_value_set_string ((value = gda_value_new (G_TYPE_STRING)), "rowid");
		expr->value = value;
		cond->operands = g_slist_append (NULL, expr);

		GdaSqlParamSpec *pspec = g_new0 (GdaSqlParamSpec, 1);
		pspec->name = g_strdup ("lid");
		pspec->g_type = G_TYPE_INT64;
		pspec->nullok = TRUE;
		pspec->is_param = TRUE;
		expr = gda_sql_expr_new (GDA_SQL_ANY_PART (cond));
		expr->param_spec = pspec;
		cond->operands = g_slist_append (cond->operands, expr);

		gda_sql_statement_select_take_where_cond (sql_statement, where);

		if (gda_sql_statement_check_structure (sql_statement, &lerror) == FALSE) {
			g_warning (_("Can't build SELECT statement to get last inserted row: %s"),
				   lerror && lerror->message ? lerror->message : _("No detail"));
			if (lerror)
				g_error_free (lerror);
			gda_sql_statement_free (sql_statement);
			return NULL;
		}
		statement = g_object_new (GDA_TYPE_STATEMENT, "structure", sql_statement, NULL);
		gda_sql_statement_free (sql_statement);

		GdaSet *params;
		if (! gda_statement_get_parameters (statement, &params, &lerror)) {
			g_warning (_("Can't build SELECT statement to get last inserted row: %s"),
				   lerror && lerror->message ? lerror->message : _("No detail"));
			if (lerror)
				g_error_free (lerror);
			g_object_unref (statement);
			return NULL;
		}

		g_object_set_data_full ((GObject*) stmt, MAKE_LAST_INSERTED_SET_ID, statement, g_object_unref);
		g_object_set_data_full ((GObject*) stmt, MAKE_LAST_INSERTED_SET_ID "P", params, g_object_unref);
		g_signal_connect (stmt, "reset",
				  G_CALLBACK (lir_stmt_reset_cb), NULL);
	}

	/* execute SELECT statement */
	GdaDataModel *model;
	GdaSet *params;
	params = g_object_get_data ((GObject*) stmt, MAKE_LAST_INSERTED_SET_ID "P");
	g_assert (params);
	g_assert (gda_set_set_holder_value (params, NULL, "lid", last_id));
        model = gda_connection_statement_execute_select (cnc, statement, params, NULL);
        if (!model) {
		/* may have failed if the table has the WITHOUT ROWID optimization
		 * see: https://www.sqlite.org/withoutrowid.html
		 */
		return NULL;
        }
	else {
		GdaSet *set = NULL;
		GSList *holders = NULL;
		gint nrows, ncols, i;

		nrows = gda_data_model_get_n_rows (model);
		if (nrows <= 0) {
			g_warning (_("SELECT statement to get last inserted row did not return any row"));
			return NULL;
		}
		else if (nrows > 1) {
			g_warning (_("SELECT statement to get last inserted row returned too many (%d) rows"),
				   nrows);
			return NULL;
		}
		ncols = gda_data_model_get_n_columns (model);
		for (i = 0; i < ncols; i++) {
			GdaHolder *h;
			GdaColumn *col;
			gchar *id;
			const GValue *cvalue;
			col = gda_data_model_describe_column (model, i);
			id = g_strdup_printf ("+%d", i);
			h = gda_holder_new (gda_column_get_g_type (col), id);
			g_object_set (G_OBJECT (h),
				      "name", gda_column_get_name (col), NULL);
			g_free (id);
			cvalue = gda_data_model_get_value_at (model, i, 0, NULL);
			if (!cvalue || !gda_holder_set_value (h, cvalue, NULL)) {
				if (holders) {
					g_slist_free_full (holders, (GDestroyNotify) g_object_unref);
					holders = NULL;
				}
				break;
			}
			holders = g_slist_prepend (holders, h);
		}
		g_object_unref (model);

		if (holders) {
			holders = g_slist_reverse (holders);
			set = gda_set_new_read_only (holders);
			g_slist_free_full (holders, (GDestroyNotify) g_object_unref);
		}

		return set;
	}
}

/*
 * This function opens any blob which has been inserted or updated as a ZERO blob (the blob currently
 * only contains zeros) and fills its contents with the blob passed as parameter when executing the statement.
 *
 * @blobs_list is freed here
 */
static GdaConnectionEvent *
fill_blob_data (GdaConnection *cnc, GdaSet *params,
		SqliteConnectionData *cdata, GdaSqlitePStmt *pstmt, GSList *blobs_list, GError **error)
{
	if (!blobs_list)
		/* nothing to do */
		return NULL;

	const gchar *cstr = NULL;
	GdaStatement *stmt;
	sqlite3_int64 rowid = -1;
	GdaDataModel *model = NULL;
	GError *lerror = NULL;
	GdaSqliteProvider *prov = GDA_SQLITE_PROVIDER (gda_connection_get_provider (cnc));

	/* get single ROWID or a list of ROWID */
	stmt = gda_pstmt_get_gda_statement (GDA_PSTMT (pstmt));
	if (!stmt) {
		g_set_error (&lerror, GDA_SERVER_PROVIDER_ERROR,
			     GDA_SERVER_PROVIDER_INTERNAL_ERROR, "%s",
			     _("Prepared statement has no associated GdaStatement"));
		goto blobs_out;
	}
	if (gda_statement_get_statement_type (stmt) == GDA_SQL_STATEMENT_INSERT) {
		rowid = SQLITE3_CALL (prov, sqlite3_last_insert_rowid) (cdata->connection);
	}
	else if (gda_statement_get_statement_type (stmt) == GDA_SQL_STATEMENT_UPDATE) {
		GdaSqlStatement *sel_stmt;
		GdaSqlStatementSelect *sst;
		sel_stmt = gda_compute_select_statement_from_update (stmt, &lerror);
		if (!sel_stmt)
			goto blobs_out;
		sst = (GdaSqlStatementSelect*) sel_stmt->contents;
		GdaSqlSelectField *oidfield;
		oidfield = gda_sql_select_field_new (GDA_SQL_ANY_PART (sst));
		sst->expr_list = g_slist_prepend (NULL, oidfield);
		oidfield->expr = gda_sql_expr_new (GDA_SQL_ANY_PART (oidfield));
		g_value_set_string ((oidfield->expr->value = gda_value_new (G_TYPE_STRING)), "rowid");

		GdaStatement *select;
		select = (GdaStatement *) g_object_new (GDA_TYPE_STATEMENT, "structure", sel_stmt, NULL);
		gda_sql_statement_free (sel_stmt);
		model = gda_connection_statement_execute_select (cnc, select, params, &lerror);
		if (!model) {
			g_object_unref (select);
			goto blobs_out;
		}
		g_object_unref (select);
	}
	g_object_unref (stmt);

	/* actual blob filling */
	GSList *list;
	for (list = blobs_list; list; list = list->next) {
		PendingBlob *pb = (PendingBlob*) list->data;

		if (rowid >= 0) {
			/*g_print ("Filling BLOB @ %s.%s.%s, rowID %ld\n", pb->db, pb->table, pb->column, rowid);*/
			GdaSqliteBlobOp *bop;
			bop = (GdaSqliteBlobOp*) _gda_sqlite_blob_op_new (cnc, pb->db, pb->table, pb->column, rowid);
			if (!bop) {
				cstr =  _("Can't create SQLite BLOB handle");
				goto blobs_out;
			}
			if (gda_blob_op_write (GDA_BLOB_OP (bop), pb->blob, 0) < 0) {
				cstr =  _("Can't write to SQLite's BLOB");
				g_object_unref (bop);
				goto blobs_out;
			}
			g_object_unref (bop);
		}
		else if (model) {
			gint nrows, i;
			nrows = gda_data_model_get_n_rows (model);
			for (i = 0; i < nrows; i++) {
				const GValue *cvalue;
				cvalue = gda_data_model_get_value_at (model, 0, i, &lerror);
				if (!cvalue) {
					g_object_unref (model);
					goto blobs_out;
				}
				if (G_VALUE_TYPE (cvalue) == G_TYPE_INT64)
					rowid = g_value_get_int64 (cvalue);
				else if (G_VALUE_TYPE (cvalue) == G_TYPE_INT)
					rowid = g_value_get_int (cvalue);
				else {
					g_set_error (&lerror, GDA_SERVER_PROVIDER_ERROR,
						     GDA_SERVER_PROVIDER_INTERNAL_ERROR,
						     _("Can't obtain SQLite BLOB handle (reported type is '%s'), "
						       "please report this bug to "
						       "http://gitlab.gnome.org/GNOME/libgda/issues"),
						       g_type_name (G_VALUE_TYPE (cvalue)));
					g_object_unref (model);
					goto blobs_out;
				}
				GdaSqliteBlobOp *bop;
				bop = (GdaSqliteBlobOp*) _gda_sqlite_blob_op_new (cnc, pb->db, pb->table, pb->column, rowid);
				if (!bop) {
					cstr =  _("Can't create SQLite BLOB handle");
					g_object_unref (model);
					goto blobs_out;
				}
				if (gda_blob_op_write (GDA_BLOB_OP (bop), pb->blob, 0) < 0) {
					cstr =  _("Can't write to SQLite's BLOB");
					g_object_unref (bop);
					g_object_unref (model);
					goto blobs_out;
				}
				g_object_unref (bop);
			}
			g_object_unref (model);
		}
		else
			cstr = _("Can't identify the ROWID of the blob to fill");
	}

 blobs_out:
	pending_blobs_free_list (blobs_list);

	if (cstr) {
		GdaConnectionEvent *event;
		event = gda_connection_point_available_event (cnc, GDA_CONNECTION_EVENT_ERROR);
		gda_connection_event_set_description (event, cstr);
		g_set_error (error, GDA_SERVER_PROVIDER_ERROR,
			     GDA_SERVER_PROVIDER_DATA_ERROR, "%s", cstr);
		return event;
	}
	if (lerror) {
		GdaConnectionEvent *event;
		event = gda_connection_point_available_event (cnc, GDA_CONNECTION_EVENT_ERROR);
		gda_connection_event_set_description (event, lerror->message ? lerror->message : _("No detail"));
		g_propagate_error (error, lerror);
		return event;
	}
	return NULL;
}

/*
 * Execute statement request
 */
static GObject *
gda_sqlite_provider_statement_execute (GdaServerProvider *provider, GdaConnection *cnc,
				       GdaStatement *stmt, GdaSet *params,
				       GdaStatementModelUsage model_usage,
				       GType *col_types, GdaSet **last_inserted_row, GError **error)
{
	GdaSqlitePStmt *ps;
	SqliteConnectionData *cdata;
	gboolean new_ps = FALSE;
	gboolean allow_noparam;
	gboolean empty_rs = FALSE; /* TRUE when @allow_noparam is TRUE and there is a problem with @params
				      => resulting data model will be empty (0 row) */
	GdaSqliteProvider *prov = GDA_SQLITE_PROVIDER (provider);

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, NULL);
	g_return_val_if_fail (GDA_IS_STATEMENT (stmt), NULL);

	allow_noparam = (model_usage & GDA_STATEMENT_MODEL_ALLOW_NOPARAM) &&
		(gda_statement_get_statement_type (stmt) == GDA_SQL_STATEMENT_SELECT);

	if (last_inserted_row)
		*last_inserted_row = NULL;

	/* get SQLite's private data */
	cdata = (SqliteConnectionData*) gda_connection_internal_get_provider_data_error (cnc, error);
	if (!cdata)
		return NULL;

	/* get/create new prepared statement */
	ps = (GdaSqlitePStmt *) gda_connection_get_prepared_statement (cnc, stmt);
	if (!ps) {
		if (!gda_sqlite_provider_statement_prepare (provider, cnc, stmt, NULL)) {
			/* try to use the SQL when parameters are rendered with their values, using GMT for timezones  */
			gchar *sql;
			int status;
			sqlite3_stmt *sqlite_stmt;
			char *left;

			sql = gda_sqlite_provider_statement_to_sql (provider, cnc, stmt, params,
								    GDA_STATEMENT_SQL_TIMEZONE_TO_GMT, NULL, error);
			if (!sql)
				return NULL;

			status = SQLITE3_CALL (prov, sqlite3_prepare_v2) (cdata->connection, sql, -1,
								    &sqlite_stmt, (const char**) &left);
			if (status != SQLITE_OK) {
				g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_PREPARE_STMT_ERROR,
					     "%s", SQLITE3_CALL (prov, sqlite3_errmsg) (cdata->connection));
				g_free (sql);
				return NULL;
			}

			if (left && (*left != 0)) {
				g_warning ("SQlite SQL: %s (REMAIN:%s)\n", sql, left);
				*left = 0;
			}

			/* create a SQLitePreparedStatement */
			ps = _gda_sqlite_pstmt_new (GDA_SQLITE_PROVIDER (provider), sqlite_stmt);
			gda_pstmt_set_sql (_GDA_PSTMT (ps), sql);
      g_free (sql);

			new_ps = TRUE;
		}
		else
			ps = (GdaSqlitePStmt *) gda_connection_get_prepared_statement (cnc, stmt);
	}
	else if (_gda_sqlite_pstmt_get_is_used (ps)) {
		/* Don't use @ps => prepare stmt again */
		GdaSqlitePStmt *nps;
		nps = real_prepare (provider, cnc, stmt, error);
		if (!nps)
			return NULL;
		gda_pstmt_copy_contents ((GdaPStmt *) ps, (GdaPStmt *) nps);
		ps = nps;
		new_ps = TRUE;
	}

	/* check that prepared stmt is not NULL, to avoid a crash */
	if (!_gda_sqlite_pstmt_get_stmt (ps)) {
		GdaConnectionEvent *event;
		const char *errmsg;

		errmsg = _("Empty statement");
		event = gda_connection_point_available_event (cnc, GDA_CONNECTION_EVENT_ERROR);
		gda_connection_event_set_description (event, errmsg);
		gda_connection_add_event (cnc, event);
		gda_connection_del_prepared_statement (cnc, stmt);
		g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_EMPTY_STMT_ERROR,
			      "%s", errmsg);
		if (new_ps)
			g_object_unref (ps);
		return NULL;
	}

	/* reset prepared stmt */
	if ((SQLITE3_CALL (prov, sqlite3_reset) (_gda_sqlite_pstmt_get_stmt (ps)) != SQLITE_OK) ||
	    (SQLITE3_CALL (prov, sqlite3_clear_bindings) (_gda_sqlite_pstmt_get_stmt (ps)) != SQLITE_OK)) {
		GdaConnectionEvent *event;
		const char *errmsg;

		errmsg = SQLITE3_CALL (prov, sqlite3_errmsg) (cdata->connection);
		event = gda_connection_point_available_event (cnc, GDA_CONNECTION_EVENT_ERROR);
		gda_connection_event_set_description (event, errmsg);
		gda_connection_add_event (cnc, event);
		gda_connection_del_prepared_statement (cnc, stmt);
		g_set_error (error, GDA_SERVER_PROVIDER_ERROR, GDA_SERVER_PROVIDER_PREPARE_STMT_ERROR,
			      "%s", errmsg);
		if (new_ps)
			g_object_unref (ps);
		return NULL;
	}

	/* bind statement's parameters */
	GSList *list;
	GdaConnectionEvent *event = NULL;
	int i;
	GSList *blobs_list = NULL; /* list of PendingBlob structures */

	for (i = 1, list = gda_pstmt_get_param_ids (_GDA_PSTMT (ps)); list; list = list->next, i++) {
		const gchar *pname = (gchar *) list->data;
		GdaHolder *h = NULL;

		if (!params) {
			event = gda_connection_point_available_event (cnc, GDA_CONNECTION_EVENT_ERROR);
			gda_connection_event_set_description (event, _("Missing parameter(s) to execute query"));
			g_set_error (error, GDA_SERVER_PROVIDER_ERROR,
				     GDA_SERVER_PROVIDER_MISSING_PARAM_ERROR,
				      "%s", _("Missing parameter(s) to execute query"));
			break;
		}

		h = gda_set_get_holder (params, pname);
		if (!h) {
			gchar *tmp = gda_alphanum_to_text (g_strdup (pname + 1));
			if (tmp) {
				h = gda_set_get_holder (params, tmp);
				g_free (tmp);
			}
		}
		if (!h) {
			if (allow_noparam) {
				/* bind param to NULL */
				SQLITE3_CALL (prov, sqlite3_bind_null) (_gda_sqlite_pstmt_get_stmt (ps), i);
				empty_rs = TRUE;
				continue;
			}
			else {

				gchar *str;
				str = g_strdup_printf (_("Missing parameter '%s' to execute query"), pname);
				event = gda_connection_point_available_event (cnc, GDA_CONNECTION_EVENT_ERROR);
				gda_connection_event_set_description (event, str);
				g_set_error (error, GDA_SERVER_PROVIDER_ERROR,
					     GDA_SERVER_PROVIDER_MISSING_PARAM_ERROR, "%s", str);
				g_free (str);
				break;
			}
		}

		if (!gda_holder_is_valid (h)) {
			if (allow_noparam) {
				/* bind param to NULL */
				SQLITE3_CALL (prov, sqlite3_bind_null) (_gda_sqlite_pstmt_get_stmt (ps), i);
				empty_rs = TRUE;
				continue;
			}
			else {
				gchar *str;
				str = g_strdup_printf (_("Parameter '%s' is invalid"), pname);
				event = gda_connection_point_available_event (cnc, GDA_CONNECTION_EVENT_ERROR);
				gda_connection_event_set_description (event, str);
				g_set_error (error, GDA_SERVER_PROVIDER_ERROR,
					     GDA_SERVER_PROVIDER_MISSING_PARAM_ERROR, "%s", str);
				g_free (str);
				break;
			}
		}
		else if (gda_holder_value_is_default (h) && !gda_holder_get_value (h)) {
			/* create a new GdaStatement to handle all default values and execute it instead */
			GdaSqlStatement *sqlst = NULL;
			GError *lerror = NULL;
			sqlst = gda_statement_rewrite_for_default_values (stmt, params, TRUE, &lerror);
			if (!sqlst) {
				event = gda_connection_point_available_event (cnc,
									      GDA_CONNECTION_EVENT_ERROR);
				gda_connection_event_set_description (event, lerror && lerror->message ?
								      lerror->message :
								      _("Can't rewrite statement handle default values"));
				g_propagate_error (error, lerror);
				break;
			}

			GdaStatement *rstmt = NULL;
			GObject *res;
			rstmt = g_object_new (GDA_TYPE_STATEMENT, "structure", sqlst, NULL);
			gda_sql_statement_free (sqlst);
			if (new_ps)
				g_object_unref (ps);
			pending_blobs_free_list (blobs_list);
			res = gda_sqlite_provider_statement_execute (provider, cnc,
								     rstmt, params,
								     model_usage,
								     col_types, last_inserted_row, error);
			g_object_unref (rstmt);
			return res;
		}

		const GValue *value = gda_holder_get_value (h);
		/*g_print ("BINDING param '%s' to GdaHolder %p, valued to [%s]\n", pname, h, gda_value_stringify (value));*/
		if (!value || gda_value_is_null (value)) {
			GdaStatement *rstmt = NULL;
			if (! gda_rewrite_statement_for_null_parameters (stmt, params, &rstmt, error))
				SQLITE3_CALL (prov, sqlite3_bind_null) (_gda_sqlite_pstmt_get_stmt (ps), i);
			else if (rstmt == NULL) {
				return NULL;
      } else {
				/* The strategy here is to execute @rstmt using its prepared
				 * statement, but with common data from @ps. Beware that
				 * the @param_ids attribute needs to be retained (i.e. it must not
				 * be the one copied from @ps) */
				GObject *obj;
				GdaSqlitePStmt *tps;
				GdaPStmt *gtps;
				GSList *prep_param_ids, *copied_param_ids;
				if (!gda_sqlite_provider_statement_prepare (provider, cnc,
									    rstmt, error))
					return NULL;
				tps = (GdaSqlitePStmt *)
					gda_connection_get_prepared_statement (cnc, rstmt);
				gtps = (GdaPStmt *) tps;

				/* keep @param_ids to avoid being cleared by gda_pstmt_copy_contents() */
				prep_param_ids = gda_pstmt_get_param_ids (gtps);
				gda_pstmt_set_param_ids (gtps, NULL);
				
				/* actual copy */
				gda_pstmt_copy_contents ((GdaPStmt *) ps, (GdaPStmt *) tps);

				/* restore previous @param_ids */
				copied_param_ids = gda_pstmt_get_param_ids (gtps);
				gda_pstmt_set_param_ids (gtps, prep_param_ids);

				/* execute */
				obj = gda_sqlite_provider_statement_execute (provider, cnc,
									     rstmt, params,
									     model_usage,
									     col_types,
									     last_inserted_row, error);
				/* clear original @param_ids and restore copied one */
				g_slist_free_full (prep_param_ids, (GDestroyNotify) g_free);

				gda_pstmt_set_param_ids (gtps, copied_param_ids);

				/*if (GDA_IS_DATA_MODEL (obj))
				  gda_data_model_dump ((GdaDataModel*) obj, NULL);*/

				g_object_unref (rstmt);

				if (new_ps)
					g_object_unref (ps);
				pending_blobs_free_list (blobs_list);
				return obj;
			}
		}
		else if (G_VALUE_TYPE (value) == G_TYPE_STRING)
			SQLITE3_CALL (prov, sqlite3_bind_text) (_gda_sqlite_pstmt_get_stmt (ps), i,
							  g_value_get_string (value), -1, SQLITE_TRANSIENT);
		else if (G_VALUE_TYPE (value) == GDA_TYPE_TEXT) {
			GdaText *text = (GdaText*) g_value_get_boxed (value);
			const gchar *tstr = gda_text_get_string (text);
			SQLITE3_CALL (prov, sqlite3_bind_text) (_gda_sqlite_pstmt_get_stmt (ps), i,
							  tstr, -1, SQLITE_TRANSIENT);
    }
		else if (G_VALUE_TYPE (value) == G_TYPE_INT)
			SQLITE3_CALL (prov, sqlite3_bind_int) (_gda_sqlite_pstmt_get_stmt (ps), i, g_value_get_int (value));
		else if (G_VALUE_TYPE (value) == G_TYPE_LONG)
			SQLITE3_CALL (prov, sqlite3_bind_int) (_gda_sqlite_pstmt_get_stmt (ps), i, g_value_get_long (value));
		else if (G_VALUE_TYPE (value) == G_TYPE_DOUBLE)
			SQLITE3_CALL (prov, sqlite3_bind_double) (_gda_sqlite_pstmt_get_stmt (ps), i, g_value_get_double (value));
		else if (G_VALUE_TYPE (value) == G_TYPE_FLOAT)
			SQLITE3_CALL (prov, sqlite3_bind_double) (_gda_sqlite_pstmt_get_stmt (ps), i, g_value_get_float (value));
		else if (G_VALUE_TYPE (value) == G_TYPE_UINT)
			SQLITE3_CALL (prov, sqlite3_bind_int) (_gda_sqlite_pstmt_get_stmt (ps), i, g_value_get_uint (value));
		else if (G_VALUE_TYPE (value) == G_TYPE_BOOLEAN)
			SQLITE3_CALL (prov, sqlite3_bind_int) (_gda_sqlite_pstmt_get_stmt (ps), i, g_value_get_boolean (value) ? 1 : 0);
		else if (G_VALUE_TYPE (value) == G_TYPE_INT64)
			SQLITE3_CALL (prov, sqlite3_bind_int64) (_gda_sqlite_pstmt_get_stmt (ps), i, g_value_get_int64 (value));
		else if (G_VALUE_TYPE (value) == G_TYPE_UINT64)
			SQLITE3_CALL (prov, sqlite3_bind_int64) (_gda_sqlite_pstmt_get_stmt (ps), i, g_value_get_uint64 (value));
		else if (G_VALUE_TYPE (value) == GDA_TYPE_SHORT)
			SQLITE3_CALL (prov, sqlite3_bind_int) (_gda_sqlite_pstmt_get_stmt (ps), i, gda_value_get_short (value));
		else if (G_VALUE_TYPE (value) == GDA_TYPE_USHORT)
			SQLITE3_CALL (prov, sqlite3_bind_int) (_gda_sqlite_pstmt_get_stmt (ps), i, gda_value_get_ushort (value));
		else if (G_VALUE_TYPE (value) == G_TYPE_CHAR)
			SQLITE3_CALL (prov, sqlite3_bind_int) (_gda_sqlite_pstmt_get_stmt (ps), i, g_value_get_schar (value));
		else if (G_VALUE_TYPE (value) == G_TYPE_UCHAR)
			SQLITE3_CALL (prov, sqlite3_bind_int) (_gda_sqlite_pstmt_get_stmt (ps), i, g_value_get_uchar (value));
		else if (G_VALUE_TYPE (value) == GDA_TYPE_BLOB) {
			glong blob_len;
			GdaBlob *blob = (GdaBlob*) gda_value_get_blob (value);
			const gchar *str = NULL;

			/* force reading the complete BLOB into memory */
			if (gda_blob_get_op (blob))
				blob_len = gda_blob_op_get_length (gda_blob_get_op (blob));
			else
				blob_len = gda_binary_get_size (gda_blob_get_binary (blob));
			if (blob_len < 0)
				str = _("Can't get BLOB's length");
			else if (blob_len >= G_MAXINT)
				str = _("BLOB is too big");

			if (str) {
				event = gda_connection_point_available_event (cnc, GDA_CONNECTION_EVENT_ERROR);
				gda_connection_event_set_description (event, str);
				g_set_error (error, GDA_SERVER_PROVIDER_ERROR,
					     GDA_SERVER_PROVIDER_DATA_ERROR, "%s", str);
				break;
			}

			PendingBlob *pb;
			GError *lerror = NULL;
			pb = make_pending_blob (provider, cnc, stmt, h, &lerror);
			if (!pb) {
				event = gda_connection_point_available_event (cnc, GDA_CONNECTION_EVENT_ERROR);
				gda_connection_event_set_description (event,
				   lerror && lerror->message ? lerror->message : _("No detail"));

				g_propagate_error (error, lerror);
				break;
			}
			pb->blob = blob;
			blobs_list = g_slist_prepend (blobs_list, pb);

			if (SQLITE3_CALL (prov, sqlite3_bind_zeroblob) (_gda_sqlite_pstmt_get_stmt (ps), i, (int) blob_len) !=
			    SQLITE_OK) {
				event = gda_connection_point_available_event (cnc, GDA_CONNECTION_EVENT_ERROR);
				gda_connection_event_set_description (event,
				   lerror && lerror->message ? lerror->message : _("No detail"));

				g_propagate_error (error, lerror);
				break;
			}
		}
		else if (G_VALUE_TYPE (value) == GDA_TYPE_BINARY) {
			GdaBinary *bin = (GdaBinary *) gda_value_get_binary (value);
			SQLITE3_CALL (prov, sqlite3_bind_blob) (_gda_sqlite_pstmt_get_stmt (ps), i,
							  gda_binary_get_data (bin), gda_binary_get_size (bin), SQLITE_TRANSIENT);
		}
		else if (G_VALUE_TYPE (value) == GDA_TYPE_TIME) {
			gchar *string;
			GdaTime *gtime;

			gtime = (GdaTime *) gda_value_get_time (value);
      string = gda_time_to_string_utc (gtime);

			SQLITE3_CALL (prov, sqlite3_bind_text) (_gda_sqlite_pstmt_get_stmt (ps), i, string, -1, g_free);
		}
		else if (G_VALUE_TYPE (value) == G_TYPE_DATE) {
			gchar *str;
			const GDate *ts;

			ts = g_value_get_boxed (value);
			str = g_strdup_printf ("%4d-%02d-%02d", g_date_get_year (ts),
					       g_date_get_month (ts), g_date_get_day (ts));
			SQLITE3_CALL (prov, sqlite3_bind_text) (_gda_sqlite_pstmt_get_stmt (ps), i, str, -1, g_free);
		}
		else if (g_type_is_a (G_VALUE_TYPE (value), G_TYPE_DATE_TIME)) {
			GDateTime *timestamp;
			gboolean tofree = FALSE;

			timestamp = (GDateTime *) g_value_get_boxed (value);

			if (g_date_time_get_utc_offset (timestamp) != 0) { // FIXME: This should be supported now
				/* SQLite cant' store timezone information, so if timezone information is
				 * provided, we do our best and convert it to UTC */
				GTimeZone *tz = g_time_zone_new ("Z");
				timestamp = g_date_time_to_timezone (timestamp, tz);
				tofree = TRUE;
			}
			gchar *string = g_date_time_format (timestamp, "%F %H:%M:%S");

			if (tofree)
				g_date_time_unref (timestamp);

			SQLITE3_CALL (prov, sqlite3_bind_text) (_gda_sqlite_pstmt_get_stmt (ps), i, string, -1, g_free);
		}
		else if (G_VALUE_TYPE (value) == GDA_TYPE_NUMERIC) {
			const GdaNumeric *gdan;

			gdan = gda_value_get_numeric (value);
			SQLITE3_CALL (prov, sqlite3_bind_text) (_gda_sqlite_pstmt_get_stmt (ps), i, gda_numeric_get_string((GdaNumeric*)gdan), -1, SQLITE_TRANSIENT);
		}
		else {
			gchar *str;
			str = g_strdup_printf (_("Non handled data type '%s'"),
					       g_type_name (G_VALUE_TYPE (value)));
			event = gda_connection_point_available_event (cnc, GDA_CONNECTION_EVENT_ERROR);
			gda_connection_event_set_description (event, str);
			g_set_error (error, GDA_SERVER_PROVIDER_ERROR,
				     GDA_SERVER_PROVIDER_MISSING_PARAM_ERROR, "%s", str);
			g_free (str);
			break;
		}
	}

	if (event) {
		gda_connection_add_event (cnc, event);
		if (new_ps)
			g_object_unref (ps);
		pending_blobs_free_list (blobs_list);
		return NULL;
	}

	/* add a connection event */
	event = gda_connection_point_available_event (cnc, GDA_CONNECTION_EVENT_COMMAND);
	gda_connection_event_set_description (event, gda_pstmt_get_sql (_GDA_PSTMT (ps)));
	gda_connection_add_event (cnc, event);

	/* treat prepared and bound statement */
	if (! g_ascii_strncasecmp (gda_pstmt_get_sql (_GDA_PSTMT (ps)), "SELECT", 6) ||
		! g_ascii_strncasecmp (gda_pstmt_get_sql (_GDA_PSTMT (ps)), "PRAGMA", 6) ||
		! g_ascii_strncasecmp (gda_pstmt_get_sql (_GDA_PSTMT (ps)), "EXPLAIN", 7)) {
		GObject *data_model;
		GdaDataModelAccessFlags flags;

		if (model_usage & GDA_STATEMENT_MODEL_RANDOM_ACCESS)
			flags = GDA_DATA_MODEL_ACCESS_RANDOM;
		else if (model_usage & GDA_DATA_MODEL_ACCESS_CURSOR_BACKWARD)
			flags = GDA_DATA_MODEL_ACCESS_RANDOM;
		else
			flags = GDA_DATA_MODEL_ACCESS_CURSOR_FORWARD;

                data_model = (GObject *) _gda_sqlite_recordset_new (cnc, ps, params, flags, col_types, empty_rs);
		GError **exceptions;
		exceptions = gda_data_model_get_exceptions (GDA_DATA_MODEL (data_model));
		if (exceptions && exceptions[0]) {
			GError *e;
			e = g_error_copy (exceptions[0]);
			event = gda_connection_point_available_event (cnc, GDA_CONNECTION_EVENT_ERROR);
			gda_connection_event_set_description (event, e->message ? e->message : _("No detail"));
			g_propagate_error (error, e);
			g_object_unref (data_model);
			return NULL;
		}
		else {
			gda_connection_internal_statement_executed (cnc, stmt, params, NULL);
			if (new_ps)
				g_object_unref (ps);
			pending_blobs_free_list (blobs_list);
			return data_model;
		}
        }
	else {
                int status, changes;
                sqlite3 *handle;
		gboolean transaction_started = FALSE;

		if (blobs_list) {
			GError *lerror = NULL;
			if (! _gda_sqlite_check_transaction_started (cnc,
								     &transaction_started, &lerror)) {
				const gchar *errmsg = _("Could not start transaction to create BLOB");
				event = gda_connection_point_available_event (cnc,
									      GDA_CONNECTION_EVENT_ERROR);
				if (lerror) {
					gda_connection_event_set_description (event,
									      lerror && lerror->message ?
									      lerror->message : errmsg);
					g_propagate_error (error, lerror);
				}
				else {
					gda_connection_event_set_description (event, errmsg);
					g_set_error (error, GDA_SERVER_PROVIDER_ERROR,
						     GDA_SERVER_PROVIDER_STATEMENT_EXEC_ERROR, "%s", errmsg);
				}
				if (new_ps)
					g_object_unref (ps);
				pending_blobs_free_list (blobs_list);
				return NULL;
			}
		}

                /* actually execute the command */
                handle = SQLITE3_CALL (prov, sqlite3_db_handle) (_gda_sqlite_pstmt_get_stmt (ps));
                status = SQLITE3_CALL (prov, sqlite3_step) (_gda_sqlite_pstmt_get_stmt (ps));
                guint tries = 0;
                while (status == SQLITE_BUSY) {
                        if (gda_statement_get_statement_type (stmt) == GDA_SQL_STATEMENT_COMMIT) {
                                break;
                        }
                        status = SQLITE3_CALL (prov, sqlite3_step) (_gda_sqlite_pstmt_get_stmt (ps));
                        if (tries == 10) {
                                break;
                        }
                        tries++;
                }
                changes = SQLITE3_CALL (prov, sqlite3_changes) (handle);
                if (status != SQLITE_DONE) {
                        if (SQLITE3_CALL (prov, sqlite3_errcode) (handle) != SQLITE_OK) {
				const char *errmsg;
                                SQLITE3_CALL (prov, sqlite3_reset) (_gda_sqlite_pstmt_get_stmt (ps));

				errmsg = SQLITE3_CALL (prov, sqlite3_errmsg) (handle);
                                event = gda_connection_point_available_event (cnc, GDA_CONNECTION_EVENT_ERROR);
                                gda_connection_event_set_description (event, errmsg);
				g_set_error (error, GDA_SERVER_PROVIDER_ERROR,
					     GDA_SERVER_PROVIDER_STATEMENT_EXEC_ERROR, "%s", errmsg);

                                gda_connection_add_event (cnc, event);
				gda_connection_internal_statement_executed (cnc, stmt, params, event);
				if (new_ps)
					g_object_unref (ps);
				pending_blobs_free_list (blobs_list);
				if (transaction_started)
					gda_connection_rollback_transaction (cnc, NULL, NULL);
				return NULL;
                        }
			else {
				/* could be SQLITE_SCHEMA if database schema has changed and
				 * changes are incompatible with statement */
				TO_IMPLEMENT;
				if (new_ps)
					g_object_unref (ps);
				pending_blobs_free_list (blobs_list);
				if (transaction_started)
					gda_connection_rollback_transaction (cnc, NULL, NULL);
				return NULL;
			}
                }
                else {
			/* fill blobs's data */
			event = fill_blob_data (cnc, params, cdata, ps, blobs_list, error);
			if (event) {
				/* an error occurred */
				SQLITE3_CALL (prov, sqlite3_reset) (_gda_sqlite_pstmt_get_stmt (ps));
				if (new_ps)
					g_object_unref (ps);
				if (transaction_started)
					gda_connection_rollback_transaction (cnc, NULL, NULL);
				return NULL;
			}
			else if (transaction_started)
				gda_connection_commit_transaction (cnc, NULL, NULL);

			gchar *str = NULL;
			gboolean count_changes = FALSE;

			if (! g_ascii_strncasecmp (gda_pstmt_get_sql (_GDA_PSTMT (ps)), "DELETE", 6)) {
				count_changes = TRUE;
				str = g_strdup_printf ("DELETE %d (see SQLite documentation for a \"DELETE * FROM table\" query)",
				                      changes);
			}
			else if (! g_ascii_strncasecmp (gda_pstmt_get_sql (_GDA_PSTMT (ps)), "INSERT", 6)) {
				sqlite3_int64 last_id;
				count_changes = TRUE;
				last_id = SQLITE3_CALL (prov, sqlite3_last_insert_rowid) (handle);
				str = g_strdup_printf ("INSERT %lld %d", last_id, changes);
				if (last_inserted_row)
					*last_inserted_row = make_last_inserted_set (cnc, stmt, last_id);
			}
			else if (!g_ascii_strncasecmp (gda_pstmt_get_sql (_GDA_PSTMT (ps)), "UPDATE", 6)) {
				count_changes = TRUE;
				str = g_strdup_printf ("UPDATE %d", changes);
			}
			else if (*(gda_pstmt_get_sql (_GDA_PSTMT (ps)))) {
				gchar *tmp = g_ascii_strup (gda_pstmt_get_sql (_GDA_PSTMT (ps)), -1);
				for (str = tmp; *str && (*str != ' ') && (*str != '\t') &&
					     (*str != '\n'); str++);
				*str = 0;
				if (changes > 0) {
					str = g_strdup_printf ("%s %d", tmp,
							       changes);
					g_free (tmp);
				}
				else
					str = tmp;
			}

			if (str) {
				event = gda_connection_point_available_event (cnc, GDA_CONNECTION_EVENT_NOTICE);
				                                             gda_connection_event_set_description (event, str);
				                                             g_free (str);
				                                             gda_connection_add_event (cnc, event);
			}
			gda_connection_internal_statement_executed (cnc, stmt, params, event);
			SQLITE3_CALL (prov, sqlite3_reset) (_gda_sqlite_pstmt_get_stmt (ps));
			if (new_ps)
				g_object_unref (ps);

			GObject *set;
			if (count_changes) {
				GdaHolder *holder;
				GValue *value;
				GSList *list;

				holder = gda_holder_new (G_TYPE_INT, "IMPACTED_ROWS");
				g_value_set_int ((value = gda_value_new (G_TYPE_INT)), changes);
				gda_holder_take_value (holder, value, NULL);
				list = g_slist_append (NULL, holder);
				set = (GObject*) gda_set_new_read_only (list);
				g_slist_free (list);
				g_object_unref (holder);
			}
			else
				set = (GObject*) gda_set_new_read_only (NULL);

			return set;
		}
	}
}

/*
 * Rewrites a statement in case some parameters in @params are set to DEFAULT, for INSERT or UPDATE statements
 *
 * Removes any default value inserted or updated
 */
static GdaSqlStatement *
gda_sqlite_provider_statement_rewrite (GdaServerProvider *provider, GdaConnection *cnc,
				       GdaStatement *stmt, GdaSet *params, GError **error)
{
	if (cnc) {
		g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
		g_return_val_if_fail (gda_connection_get_provider (cnc) == provider, NULL);
	}
	return gda_statement_rewrite_for_default_values (stmt, params, TRUE, error);
}

/*
 * SQLite's extra functions' implementations
 */
static void
scalar_gda_file_exists_func (sqlite3_context *context, int argc, sqlite3_value **argv)
{
	const gchar *path;

	if (argc != 1) {
		(s3r->sqlite3_result_error) (context, _("Function requires one argument"), -1);
		return;
	}

	path = (gchar *) (s3r->sqlite3_value_text) (argv [0]);
	if (g_file_test (path, G_FILE_TEST_EXISTS))
		(s3r->sqlite3_result_int) (context, 1);
	else
		(s3r->sqlite3_result_int) (context, 0);
}


static void
scalar_gda_hex_print_func (sqlite3_context *context, int argc, sqlite3_value **argv)
{
	GdaBinary *bin;
	GdaDataHandler *dh;
	GValue *value;
	gchar *str;

	if (argc != 1) {
		(s3r->sqlite3_result_error) (context, _("Function requires one argument"), -1);
		return;
	}

	bin = gda_binary_new ();
	guchar* buffer = (guchar*) (s3r->sqlite3_value_blob) (argv [0]);
	if (!buffer) {
		gda_binary_free (bin);
		(s3r->sqlite3_result_null) (context);
		return;
	}
	glong length = (s3r->sqlite3_value_bytes) (argv [0]);
	gda_binary_set_data (bin, buffer, length);
	g_free (free);
	gda_value_take_binary ((value = gda_value_new (GDA_TYPE_BINARY)), bin);
	dh = gda_data_handler_get_default (GDA_TYPE_BINARY);
	str = gda_data_handler_get_str_from_value (dh, value);
	g_object_unref (dh);

	gda_value_free (value);
	(s3r->sqlite3_result_text) (context, str, -1, g_free);
}

static void
scalar_gda_hex_print_func2 (sqlite3_context *context, int argc, sqlite3_value **argv)
{
	GdaBinary *bin;
	GdaDataHandler *dh;
	GValue *value;
	gchar *str;
	gint size;

	if (argc != 2) {
		(s3r->sqlite3_result_error) (context, _("Function requires two arguments"), -1);
		return;
	}

	bin = gda_binary_new ();
	guchar* buffer = (guchar*) (s3r->sqlite3_value_blob) (argv [0]);
	if (!buffer) {
		gda_binary_free (bin);
		(s3r->sqlite3_result_null) (context);
		return;
	}
	glong length = (s3r->sqlite3_value_bytes) (argv [0]);
	gda_binary_set_data (bin, buffer, length);
	gda_value_take_binary ((value = gda_value_new (GDA_TYPE_BINARY)), bin);
	dh = gda_data_handler_get_default (GDA_TYPE_BINARY);
	str = gda_data_handler_get_str_from_value (dh, value);
	g_object_unref (dh);

	gda_value_free (value);

	size = (s3r->sqlite3_value_int) (argv [1]);

	(s3r->sqlite3_result_text) (context, str, size, g_free);
}

static void
scalar_rmdiacr (sqlite3_context *context, int argc, sqlite3_value **argv)
{
	gchar *data, *tmp;
	CaseModif ncase = CASE_UNCHANGED;

	if (argc == 2) {
		data = (gchar*) (s3r->sqlite3_value_text) (argv [1]);
		if ((*data == 'u') || (*data == 'U'))
			ncase = CASE_UP;
		else if ((*data == 'l') || (*data == 'l'))
			ncase = CASE_DOWN;
	}
	else if (argc != 1) {
		(s3r->sqlite3_result_error) (context, _("Function requires one or two arguments"), -1);
		return;
	}

	data = (gchar*) (s3r->sqlite3_value_text) (argv [0]);
	if (!data) {
		(s3r->sqlite3_result_null) (context);
		return;
	}

	tmp = remove_diacritics_and_change_case (data, -1, ncase);
	(s3r->sqlite3_result_text) (context, tmp, -1, g_free);
}

static void
scalar_lower (sqlite3_context *context, int argc, sqlite3_value **argv)
{
	gchar *data, *tmp;

	if (argc != 1) {
		(s3r->sqlite3_result_error) (context, _("Function requires one argument"), -1);
		return;
	}

	data = (gchar*) (s3r->sqlite3_value_text) (argv [0]);
	if (!data) {
		(s3r->sqlite3_result_null) (context);
		return;
	}

	tmp = g_utf8_strdown (data, -1);
	(s3r->sqlite3_result_text) (context, tmp, -1, g_free);
}

static void
scalar_upper (sqlite3_context *context, int argc, sqlite3_value **argv)
{
	gchar *data, *tmp;

	if (argc != 1) {
		(s3r->sqlite3_result_error) (context, _("Function requires one argument"), -1);
		return;
	}

	data = (gchar*) (s3r->sqlite3_value_text) (argv [0]);
	if (!data) {
		(s3r->sqlite3_result_null) (context);
		return;
	}

	tmp = g_utf8_strup (data, -1);
	(s3r->sqlite3_result_text) (context, tmp, -1, g_free);
}

static void
scalar_gda_hex_func (sqlite3_context *context, int argc, sqlite3_value **argv)
{
	guchar *data;
	gint length;
	GString *string;
	gint i;

	if (argc != 1) {
		(s3r->sqlite3_result_error) (context, _("Function requires one argument"), -1);
		return;
	}

	data = (guchar*) (s3r->sqlite3_value_blob) (argv [0]);
	if (!data) {
		(s3r->sqlite3_result_null) (context);
		return;
	}

	length = (s3r->sqlite3_value_bytes) (argv [0]);
	string = g_string_new ("");
	for (i = 0; i < length; i++) {
		if ((i > 0) && (i % 4 == 0))
			g_string_append_c (string, ' ');
		g_string_append_printf (string, "%02x", data [i]);
	}

	(s3r->sqlite3_result_text) (context, string->str, -1, g_free);
	g_string_free (string, FALSE);
}

static void
scalar_gda_hex_func2 (sqlite3_context *context, int argc, sqlite3_value **argv)
{
	guchar *data;
	gint length;
	GString *string;
	gint i;
	guint size;

	if (argc != 2) {
		(s3r->sqlite3_result_error) (context, _("Function requires two arguments"), -1);
		return;
	}

	data = (guchar*) (s3r->sqlite3_value_blob) (argv [0]);
	if (!data) {
		(s3r->sqlite3_result_null) (context);
		return;
	}

	length = (s3r->sqlite3_value_bytes) (argv [0]);
	size = (s3r->sqlite3_value_int) (argv [1]);

	string = g_string_new ("");
	for (i = 0; (i < length) && (string->len < (size / 2) * 2 + 2); i++) {
		if ((i > 0) && (i % 4 == 0))
			g_string_append_c (string, ' ');
		g_string_append_printf (string, "%02x", data [i]);
	}

	if (string->len > size)
		string->str[size] = 0;
	(s3r->sqlite3_result_text) (context, string->str, -1, g_free);
	g_string_free (string, FALSE);
}

static void
scalar_regexp_func (sqlite3_context *context, int argc, sqlite3_value **argv)
{
	GRegex *regex = NULL;
	GError *error = NULL;
	const gchar *str, *pattern, *options = NULL;
	GRegexCompileFlags flags = G_REGEX_OPTIMIZE;
	gboolean as_boolean = TRUE;

#define MAX_DEFINED_REGEX 10
	static GArray *re_array = NULL; /* array of signatures (pattern+option) */
	static GHashTable *re_hash = NULL; /* hash of GRegex */

	if ((argc != 2) && (argc != 3)) {
		(s3r->sqlite3_result_error) (context, _("Function requires two or three arguments"), -1);
		return;
	}

	str = (gchar*) (s3r->sqlite3_value_text) (argv [1]);
	if (!str) {
		(s3r->sqlite3_result_null) (context);
		return;
	}

	pattern = (gchar*) (s3r->sqlite3_value_text) (argv [0]);
	if (!pattern) {
		(s3r->sqlite3_result_null) (context);
		return;
	}

	if (argc == 3)
		options = (gchar*) (s3r->sqlite3_value_text) (argv [2]);

	if (options) {
		const gchar *ptr;
		for (ptr = options; *ptr; ptr++) {
			switch (*ptr) {
			case 'i':
			case 'I':
				flags |= G_REGEX_CASELESS;
				break;
			case 'm':
			case 'M':
				flags |= G_REGEX_MULTILINE;
				break;
			case 'v':
			case 'V':
				as_boolean = FALSE;
				break;
			}
		}
	}

	GString *sig;
	sig = g_string_new (pattern);
	g_string_append_c (sig, 0x01);
	if (options && *options)
		g_string_append (sig, options);

	if (re_hash)
		regex = g_hash_table_lookup (re_hash, sig->str);
	if (regex) {
		/*g_print ("FOUND GRegex %p as [%s]\n", regex, sig->str);*/
		g_string_free (sig, TRUE);
	}
	else {
		regex = g_regex_new ((const gchar*) pattern, flags, 0, &error);
		if (! regex) {
			gda_log_error (_("SQLite regexp '%s' error:"), pattern,
				       error && error->message ? error->message : _("Invalid regular expression"));
			g_clear_error (&error);
			if (as_boolean)
				(s3r->sqlite3_result_int) (context, 0);
			else
				(s3r->sqlite3_result_null) (context);

			g_string_free (sig, TRUE);
			return;
		}

		if (!re_array) {
			re_array = g_array_new (FALSE, FALSE, sizeof (gchar*));
			re_hash = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify) g_regex_unref);
		}
		/*g_print ("ADDED new GRegex %p as [%s]\n", regex, sig->str);*/
		g_hash_table_insert (re_hash, sig->str, regex);
		g_array_prepend_val (re_array, sig->str);
		g_string_free (sig, FALSE);
		if (re_array->len > MAX_DEFINED_REGEX) {
			/* get rid of the 'oldest' GRexex */
			gchar *osig;
			osig = g_array_index (re_array, gchar*, re_array->len - 1);
			/*g_print ("REMOVED GRegex [%s]\n", osig);*/
			g_hash_table_remove (re_hash, osig);
			g_array_remove_index (re_array, re_array->len - 1);
		}
	}

	if (as_boolean) {
		if (g_regex_match (regex, str, 0, NULL))
			(s3r->sqlite3_result_int) (context, 1);
		else
			(s3r->sqlite3_result_int) (context, 0);
	}
	else {
		GMatchInfo *match_info;
		g_regex_match (regex, str, 0, &match_info);
		if (g_match_info_matches (match_info)) {
			gchar *word = g_match_info_fetch (match_info, 0);
			(s3r->sqlite3_result_text) (context, word, -1, g_free);
		}
		else
			(s3r->sqlite3_result_null) (context);
		g_match_info_free (match_info);
	}
}

static void
scalar_regexp_match_func (sqlite3_context *context, int argc, sqlite3_value **argv)
{
	if ((argc != 2) && (argc != 3)) {
		(s3r->sqlite3_result_error) (context, _("Function requires two or three arguments"), -1);
		return;
	}

	sqlite3_value **nargv;
	nargv = g_new (sqlite3_value*, argc);
	nargv[0] = argv[1];
	nargv[1] = argv[0];
	if (argc == 3)
		nargv[2] = argv[2];
	scalar_regexp_func (context, argc, nargv);
	g_free (nargv);
}

static void
gda_sqlite_free_cnc_data (SqliteConnectionData *cdata)
{
	if (!cdata)
		return;

	if (cdata->connection) {
		GdaSqliteProvider *prov = g_weak_ref_get (&cdata->provider);
		if (prov != NULL) {
			(s3r->sqlite3_close_v2) (cdata->connection);
			g_object_unref (prov);
		}
	}
	g_free (cdata->file);
	if (cdata->types_hash)
		g_hash_table_destroy (cdata->types_hash);
	if (cdata->types_array)
		g_free (cdata->types_array);
	g_free (cdata);
}

static gchar *
gda_sqlite_provider_escape_string (G_GNUC_UNUSED GdaServerProvider *provider, G_GNUC_UNUSED GdaConnection *cnc, const gchar *string)
{
	gchar *ptr, *ret, *retptr;
	gint size;

	if (!string)
		return NULL;

	/* determination of the new string size */
	ptr = (gchar *) string;
	size = 1;
	while (*ptr) {
		if (*ptr == '\'')
			size += 2;
		else
			size += 1;
		ptr++;
	}

	ptr = (gchar *) string;
	ret = g_new0 (gchar, size);
	retptr = ret;
	while (*ptr) {
		if (*ptr == '\'') {
			*retptr = '\'';
			*(retptr+1) = *ptr;
			retptr += 2;
		}
		else {
			*retptr = *ptr;
			retptr ++;
		}
		ptr++;
	}
	*retptr = '\0';

	return ret;
}

gchar *
gda_sqlite_provider_unescape_string (G_GNUC_UNUSED GdaServerProvider *provider, G_GNUC_UNUSED GdaConnection *cnc, const gchar *string)
{
	glong total;
	gchar *ptr;
	gchar *retval;
	glong offset = 0;

	if (!string)
		return NULL;

	total = strlen (string);
	retval = g_memdup (string, total+1);
	ptr = (gchar *) retval;
	while (offset < total) {
		if (*ptr == '\'') {
			if (*(ptr+1) == '\'') {
				memmove (ptr+1, ptr+2, total - offset);
				offset += 2;
			}
			else {
				g_free (retval);
				return NULL;
			}
		}
		else
			offset ++;

		ptr++;
	}

	return retval;
}

/**
 * gda_sqlite_provider_get_api:
 * @prov: a #GdaSqliteProvider to get API loaded from library in use
 *
 * Returns: a pointer to internal loaded API from library
 */
gpointer
gda_sqlite_provider_get_api (GdaSqliteProvider *prov) {
	GdaSqliteProviderClass *klass = GDA_SQLITE_PROVIDER_GET_CLASS (prov);
	if (klass->get_api != NULL) {
		return klass->get_api (prov);
	}
	return NULL;
}

/* GdaProviderMeta implementations */

static GdaDataModel*
gda_sqlite_provider_meta_btypes (GdaProviderMeta *prov,
                              G_GNUC_UNUSED GError **error)
{
  g_return_val_if_fail (prov, NULL);
  g_return_val_if_fail (GDA_IS_PROVIDER_META (prov), NULL);
  return NULL;
}
static GdaDataModel*
gda_sqlite_provider_meta_udts (GdaProviderMeta *prov,
                              G_GNUC_UNUSED GError **error)
{
  g_return_val_if_fail (prov, NULL);
  g_return_val_if_fail (GDA_IS_PROVIDER_META (prov), NULL);
  return NULL;
}
static GdaRow*
gda_sqlite_provider_meta_udt (GdaProviderMeta *prov,
                              G_GNUC_UNUSED const gchar *udt_catalog, G_GNUC_UNUSED const gchar *udt_schema,
                              G_GNUC_UNUSED GError **error)
{
  g_return_val_if_fail (prov, NULL);
  g_return_val_if_fail (GDA_IS_PROVIDER_META (prov), NULL);
  return NULL;
}
static GdaDataModel*
gda_sqlite_provider_meta_udt_cols (GdaProviderMeta *prov,
                              G_GNUC_UNUSED GError **error)
{
  g_return_val_if_fail (prov, NULL);
  g_return_val_if_fail (GDA_IS_PROVIDER_META (prov), NULL);
  return NULL;
}
static GdaRow*
gda_sqlite_provider_meta_udt_col (GdaProviderMeta *prov,
                              G_GNUC_UNUSED const gchar *udt_catalog, G_GNUC_UNUSED const gchar *udt_schema,
                              G_GNUC_UNUSED const gchar *udt_name, GError **error)
{
  g_return_val_if_fail (prov, NULL);
  g_return_val_if_fail (GDA_IS_PROVIDER_META (prov), NULL);
  return NULL;
}
static GdaDataModel*
gda_sqlite_provider_meta_enums_type (GdaProviderMeta *prov,
                              G_GNUC_UNUSED GError **error)
{
  g_return_val_if_fail (prov, NULL);
  g_return_val_if_fail (GDA_IS_PROVIDER_META (prov), NULL);
  return NULL;
}
static GdaRow*
gda_sqlite_provider_meta_enum_type (GdaProviderMeta *prov,
                                    G_GNUC_UNUSED const gchar *udt_catalog,
                                    G_GNUC_UNUSED const gchar *udt_schema, G_GNUC_UNUSED const gchar *udt_name, GError **error)
{
  g_return_val_if_fail (prov, NULL);
  g_return_val_if_fail (GDA_IS_PROVIDER_META (prov), NULL);
  return NULL;
}
static GdaDataModel*
gda_sqlite_provider_meta_domains (GdaProviderMeta *prov,
                              G_GNUC_UNUSED GError **error)
{
  g_return_val_if_fail (prov, NULL);
  g_return_val_if_fail (GDA_IS_PROVIDER_META (prov), NULL);
  return NULL;
}
static GdaRow*
gda_sqlite_provider_meta_domain (GdaProviderMeta *prov,
                              G_GNUC_UNUSED const gchar *domain_catalog, G_GNUC_UNUSED const gchar *domain_schema, GError **error)
{
  g_return_val_if_fail (prov, NULL);
  g_return_val_if_fail (GDA_IS_PROVIDER_META (prov), NULL);
  return NULL;
}
static GdaDataModel*
gda_sqlite_provider_meta_domains_constraints (GdaProviderMeta *prov,
                              G_GNUC_UNUSED GError **error)
{
  g_return_val_if_fail (prov, NULL);
  g_return_val_if_fail (GDA_IS_PROVIDER_META (prov), NULL);
  return NULL;
}
static GdaDataModel*
gda_sqlite_provider_meta_domain_constraints (GdaProviderMeta *prov,
                              G_GNUC_UNUSED const gchar *domain_catalog, G_GNUC_UNUSED const gchar *domain_schema,
                              G_GNUC_UNUSED const gchar *domain_name, GError **error)
{
  g_return_val_if_fail (prov, NULL);
  g_return_val_if_fail (GDA_IS_PROVIDER_META (prov), NULL);
  return NULL;
}
static GdaRow*
gda_sqlite_provider_meta_domain_constraint (GdaProviderMeta *prov,
                              G_GNUC_UNUSED const gchar *domain_catalog, G_GNUC_UNUSED const gchar *domain_schema,
                              G_GNUC_UNUSED const gchar *domain_name, G_GNUC_UNUSED const gchar *contraint_name,
                              G_GNUC_UNUSED GError **error)
{
  g_return_val_if_fail (prov, NULL);
  g_return_val_if_fail (GDA_IS_PROVIDER_META (prov), NULL);
  return NULL;
}
static GdaDataModel*
gda_sqlite_provider_meta_element_types (GdaProviderMeta *prov,
                              G_GNUC_UNUSED GError **error)
{
  g_return_val_if_fail (prov, NULL);
  g_return_val_if_fail (GDA_IS_PROVIDER_META (prov), NULL);
  return NULL;
}
static GdaRow*
gda_sqlite_provider_meta_element_type (GdaProviderMeta *prov,
                              G_GNUC_UNUSED const gchar *specific_name, GError **error)
{
  g_return_val_if_fail (prov, NULL);
  g_return_val_if_fail (GDA_IS_PROVIDER_META (prov), NULL);
  return NULL;
}
static GdaDataModel*
gda_sqlite_provider_meta_collations (GdaProviderMeta *prov,
                              GError **error)
{
  g_return_val_if_fail (prov, NULL);
  g_return_val_if_fail (GDA_IS_PROVIDER_META (prov), NULL);
  return NULL;
}
static GdaRow*
gda_sqlite_provider_meta_collation (GdaProviderMeta *prov,
                              const gchar *collation_catalog, const gchar *collation_schema,
                              const gchar *collation_name_n, GError **error)
{
  g_return_val_if_fail (prov, NULL);
  g_return_val_if_fail (GDA_IS_PROVIDER_META (prov), NULL);
  return NULL;
}
static GdaDataModel*
gda_sqlite_provider_meta_character_sets (GdaProviderMeta *prov,
                              GError **error)
{
  g_return_val_if_fail (prov, NULL);
  g_return_val_if_fail (GDA_IS_PROVIDER_META (prov), NULL);
  return NULL;
}
static GdaRow*
gda_sqlite_provider_meta_character_set (GdaProviderMeta *prov,
                              const gchar *chset_catalog, const gchar *chset_schema,
                              const gchar *chset_name_n, GError **error)
{
  g_return_val_if_fail (prov, NULL);
  g_return_val_if_fail (GDA_IS_PROVIDER_META (prov), NULL);
  return NULL;
}
static GdaDataModel*
gda_sqlite_provider_meta_schematas (GdaProviderMeta *prov,
                              GError **error)
{
  g_return_val_if_fail (prov, NULL);
  g_return_val_if_fail (GDA_IS_PROVIDER_META (prov), NULL);
  return NULL;
}
static GdaRow*
gda_sqlite_provider_meta_schemata (GdaProviderMeta *prov,
                              const gchar *catalog_name, const gchar *schema_name_n, GError **error)
{
  g_return_val_if_fail (prov, NULL);
  g_return_val_if_fail (GDA_IS_PROVIDER_META (prov), NULL);
  return NULL;
}
static GdaDataModel*
gda_sqlite_provider_meta_tables (GdaProviderMeta *prov,
                              GError **error)
{
  g_return_val_if_fail (prov, NULL);
  g_return_val_if_fail (GDA_IS_PROVIDER_META (prov), NULL);

  return gda_provider_meta_execute_query (prov,
        "SELECT name, 'system' as 'Owner',"
        " ' ' as 'Description', sql as 'Definition' "
        "FROM (SELECT * FROM sqlite_master "
        "UNION ALL SELECT * FROM sqlite_temp_master) "
        "WHERE name not like 'sqlite_%%' AND type='table' ORDER BY name",
        NULL, error);
}
static GdaRow*
gda_sqlite_provider_meta_table (GdaProviderMeta *prov,
                              const gchar *table_catalog, const gchar *table_schema,
                              const gchar *table_name_n, GError **error)
{
  g_return_val_if_fail (prov, NULL);
  g_return_val_if_fail (GDA_IS_PROVIDER_META (prov), NULL);
  GdaRow *row;
  GdaSet *params;

  params = gda_set_new_inline (1, "name", G_TYPE_STRING, NULL);
  gda_set_set_holder_value (params, error, "name", table_name_n, NULL);
  if (*error != NULL) {
    g_object_unref (params);
    return NULL;
  }

  row = gda_provider_meta_execute_query_row (prov,
          "SELECT name as 'Table', 'system' as 'Owner',"
          " ' ' as 'Description', sql as 'Definition' "
          "FROM (SELECT * FROM sqlite_master UNION ALL "
          "SELECT * FROM sqlite_temp_master) "
          "WHERE name = ##name::string name not like 'sqlite_%%' ORDER BY name",
          params, error);
  g_object_unref (params);
  return row;
}
static GdaDataModel*
gda_sqlite_provider_meta_views (GdaProviderMeta *prov,
                              GError **error)
{
  g_return_val_if_fail (prov, NULL);
  g_return_val_if_fail (GDA_IS_PROVIDER_META (prov), NULL);

  return gda_provider_meta_execute_query (prov,
        "SELECT name, 'system' as 'Owner',"
        " ' ' as 'Description', sql as 'Definition' "
        "FROM (SELECT * FROM sqlite_master "
        "UNION ALL SELECT * FROM sqlite_temp_master) "
        "WHERE name not like 'sqlite_%%' AND type='view' ORDER BY name",
        NULL, error);
}
static GdaRow*
gda_sqlite_provider_meta_view (GdaProviderMeta *prov,
                              const gchar *table_catalog, const gchar *table_schema,
                              const gchar *table_name_n, GError **error)
{
  g_return_val_if_fail (prov, NULL);
  g_return_val_if_fail (GDA_IS_PROVIDER_META (prov), NULL);
  GdaRow *row;
  GdaSet *params;

  params = gda_set_new_inline (1, "name", G_TYPE_STRING, NULL);
  gda_set_set_holder_value (params, error, "name", table_name_n, NULL);
  if (*error != NULL) {
    g_object_unref (params);
    return NULL;
  }

  row = gda_provider_meta_execute_query_row (prov,
          "SELECT name, 'system' as 'Owner',"
          " ' ' as 'Description', sql as 'Definition' "
          "FROM (SELECT * FROM sqlite_master UNION ALL "
          "SELECT * FROM sqlite_temp_master) "
          "WHERE name = ##name::string name not like 'sqlite_%%' AND type = 'view' ORDER BY name",
          params, error);
  g_object_unref (params);
  return row;
}
static GdaDataModel*
gda_sqlite_provider_meta_columns (GdaProviderMeta *prov,
                              GError **error)
{
  g_return_val_if_fail (prov, NULL);
  g_return_val_if_fail (GDA_IS_PROVIDER_META (prov), NULL);
  return NULL;
}
static GdaDataModel*
gda_sqlite_provider_meta_tables_columns (GdaProviderMeta *prov,
                                         GError **error)
{
  g_return_val_if_fail (prov, NULL);
  g_return_val_if_fail (GDA_IS_PROVIDER_META (prov), NULL);
  return NULL;
}
static GdaDataModel*
gda_sqlite_provider_meta_table_columns (GdaProviderMeta *prov,
                              const gchar *table_catalog, const gchar *table_schema,
                              const gchar *table_name, GError **error)
{
  g_return_val_if_fail (prov, NULL);
  g_return_val_if_fail (GDA_IS_PROVIDER_META (prov), NULL);
  return NULL;
}
static GdaRow*
gda_sqlite_provider_meta_table_column (GdaProviderMeta *prov,
                              const gchar *table_catalog, const gchar *table_schema,
                              const gchar *table_name,
                              const gchar *column_name, GError **error)
{
  g_return_val_if_fail (prov, NULL);
  g_return_val_if_fail (GDA_IS_PROVIDER_META (prov), NULL);
  return NULL;
}
static GdaDataModel*
gda_sqlite_provider_meta_views_columns (GdaProviderMeta *prov,
                              GError **error)
{
  g_return_val_if_fail (prov, NULL);
  g_return_val_if_fail (GDA_IS_PROVIDER_META (prov), NULL);
  return NULL;
}
static GdaDataModel*
gda_sqlite_provider_meta_view_columns (GdaProviderMeta *prov,
                              const gchar *view_catalog, const gchar *view_schema,
                              const gchar *view_name, GError **error)
{
  g_return_val_if_fail (prov, NULL);
  g_return_val_if_fail (GDA_IS_PROVIDER_META (prov), NULL);
  return NULL;
}
static GdaRow*
gda_sqlite_provider_meta_view_column   (GdaProviderMeta *prov,
                              const gchar *view_catalog, const gchar *view_schema,
                              const gchar *view_name,
                              const gchar *column_name,
                              GError **error)
{
  g_return_val_if_fail (prov, NULL);
  g_return_val_if_fail (GDA_IS_PROVIDER_META (prov), NULL);
  return NULL;
}
static GdaDataModel*
gda_sqlite_provider_meta_constraints_tables (GdaProviderMeta *prov, GError **error)
{
  g_return_val_if_fail (prov, NULL);
  g_return_val_if_fail (GDA_IS_PROVIDER_META (prov), NULL);
  return NULL;
}
static GdaDataModel*
gda_sqlite_provider_meta_constraints_table (GdaProviderMeta *prov,
                             const gchar *table_catalog, const gchar *table_schema,
                             const gchar *table_name,
                             GError **error)
{
  g_return_val_if_fail (prov, NULL);
  g_return_val_if_fail (GDA_IS_PROVIDER_META (prov), NULL);
  return NULL;
}
static GdaRow*
gda_sqlite_provider_meta_constraint_table (GdaProviderMeta *prov,
                             const gchar *table_catalog, const gchar *table_schema,
                             const gchar *table_name,
                             const gchar *constraint_name_n, GError **error)
{
  g_return_val_if_fail (prov, NULL);
  g_return_val_if_fail (GDA_IS_PROVIDER_META (prov), NULL);
  return NULL;
}
static GdaDataModel*
gda_sqlite_provider_meta_constraints_ref (GdaProviderMeta *prov,
                             GError **error)
{
  g_return_val_if_fail (prov, NULL);
  g_return_val_if_fail (GDA_IS_PROVIDER_META (prov), NULL);
  return NULL;
}
static GdaDataModel*
gda_sqlite_provider_meta_constraints_ref_table (GdaProviderMeta *prov,
                             const gchar *table_catalog,
                             const gchar *table_schema, const gchar *table_name,
                              GError **error)
{
  g_return_val_if_fail (prov, NULL);
  g_return_val_if_fail (GDA_IS_PROVIDER_META (prov), NULL);
  return NULL;
}
static GdaRow*
gda_sqlite_provider_meta_constraint_ref (GdaProviderMeta *prov,
                             const gchar *table_catalog,
                             const gchar *table_schema, const gchar *table_name,
                             const gchar *constraint_name, GError **error)
{
  g_return_val_if_fail (prov, NULL);
  g_return_val_if_fail (GDA_IS_PROVIDER_META (prov), NULL);
  return NULL;
}
static GdaDataModel*
gda_sqlite_provider_meta_key_columns (GdaProviderMeta *prov,
                            GError **error)
{
  g_return_val_if_fail (prov, NULL);
  g_return_val_if_fail (GDA_IS_PROVIDER_META (prov), NULL);
  return NULL;
}
static GdaRow*
gda_sqlite_provider_meta_key_column (GdaProviderMeta *prov,
                              const gchar *table_catalog, const gchar *table_schema,
                              const gchar *table_name,
                              const gchar *constraint_name, GError **error)
{
  g_return_val_if_fail (prov, NULL);
  g_return_val_if_fail (GDA_IS_PROVIDER_META (prov), NULL);
  return NULL;
}
static GdaDataModel*
gda_sqlite_provider_meta_check_columns (GdaProviderMeta *prov,
                              GError **error)
{
  g_return_val_if_fail (prov, NULL);
  g_return_val_if_fail (GDA_IS_PROVIDER_META (prov), NULL);
  return NULL;
}
static GdaRow*
gda_sqlite_provider_meta_check_column (GdaProviderMeta *prov,
                              const gchar *table_catalog,
                              const gchar *table_schema,
                              const gchar *table_name,
                              const gchar *constraint_name, GError **error)
{
  g_return_val_if_fail (prov, NULL);
  g_return_val_if_fail (GDA_IS_PROVIDER_META (prov), NULL);
  return NULL;
}
static GdaDataModel*
gda_sqlite_provider_meta_triggers (GdaProviderMeta *prov,
                              GError **error)
{
  g_return_val_if_fail (prov, NULL);
  g_return_val_if_fail (GDA_IS_PROVIDER_META (prov), NULL);
  return NULL;
}
static GdaRow*
gda_sqlite_provider_meta_trigger (GdaProviderMeta *prov,
                              const gchar *table_catalog,
                              const gchar *table_schema,
                              const gchar *table_name, GError **error)
{
  g_return_val_if_fail (prov, NULL);
  g_return_val_if_fail (GDA_IS_PROVIDER_META (prov), NULL);
  return NULL;
}
static GdaDataModel*
gda_sqlite_provider_meta_routines (GdaProviderMeta *prov,
                              GError **error)
{
  g_return_val_if_fail (prov, NULL);
  g_return_val_if_fail (GDA_IS_PROVIDER_META (prov), NULL);
  return NULL;
}
static GdaRow*
gda_sqlite_provider_meta_routine (GdaProviderMeta *prov,
                              const gchar *routine_catalog, const gchar *routine_schema,
                              const gchar *routine_name_n, GError **error)
{
  g_return_val_if_fail (prov, NULL);
  g_return_val_if_fail (GDA_IS_PROVIDER_META (prov), NULL);
  return NULL;
}
static GdaDataModel*
gda_sqlite_provider_meta_routines_col (GdaProviderMeta *prov, GError **error)
{
  g_return_val_if_fail (prov, NULL);
  g_return_val_if_fail (GDA_IS_PROVIDER_META (prov), NULL);
  return NULL;
}
static GdaRow*
gda_sqlite_provider_meta_routine_col (GdaProviderMeta *prov,
                              const gchar *rout_catalog, const gchar *rout_schema,
                              const gchar *rout_name, GError **error)
{
  g_return_val_if_fail (prov, NULL);
  g_return_val_if_fail (GDA_IS_PROVIDER_META (prov), NULL);
  return NULL;
}
static GdaDataModel*
gda_sqlite_provider_meta_routines_pars (GdaProviderMeta *prov,
                              GError **error)
{
  g_return_val_if_fail (prov, NULL);
  g_return_val_if_fail (GDA_IS_PROVIDER_META (prov), NULL);
  return NULL;
}
static GdaRow*
gda_sqlite_provider_meta_routine_pars (GdaProviderMeta *prov,
                              const gchar *rout_catalog, const gchar *rout_schema,
                              const gchar *rout_name, GError **error)
{
  g_return_val_if_fail (prov, NULL);
  g_return_val_if_fail (GDA_IS_PROVIDER_META (prov), NULL);
  return NULL;
}
static GdaDataModel*
gda_sqlite_provider_meta_indexes_tables (GdaProviderMeta *prov,
                              GError **error)
{
  g_return_val_if_fail (prov, NULL);
  g_return_val_if_fail (GDA_IS_PROVIDER_META (prov), NULL);
  return NULL;
}
static GdaDataModel*
gda_sqlite_provider_meta_indexes_table (GdaProviderMeta *prov,
                              const gchar *table_catalog, const gchar *table_schema,
                              const gchar *table_name, GError **error)
{
  g_return_val_if_fail (prov, NULL);
  g_return_val_if_fail (GDA_IS_PROVIDER_META (prov), NULL);
  return NULL;
}
static GdaRow*
gda_sqlite_provider_meta_index_table (GdaProviderMeta *prov,
                              const gchar *table_catalog,
                              const gchar *table_schema,
                              const gchar *table_name,
                              const gchar *index_name_n, GError **error)
{
  g_return_val_if_fail (prov, NULL);
  g_return_val_if_fail (GDA_IS_PROVIDER_META (prov), NULL);
  return NULL;
}
static GdaDataModel*
gda_sqlite_provider_meta_index_cols (GdaProviderMeta *prov,
                              GError **error)
{
  g_return_val_if_fail (prov, NULL);
  g_return_val_if_fail (GDA_IS_PROVIDER_META (prov), NULL);
  return NULL;
}
static GdaRow*
gda_sqlite_provider_meta_index_col (GdaProviderMeta *prov,
                              const gchar *table_catalog,
                              const gchar *table_schema,
                              const gchar *table_name,
                              const gchar *index_name, GError **error)
{
  g_return_val_if_fail (prov, NULL);
  g_return_val_if_fail (GDA_IS_PROVIDER_META (prov), NULL);
  return NULL;
}

/* GdaProvider Implementation */

const gchar*
gda_sqlite_provider_iface_get_name (GdaProvider *provider) {
  g_return_val_if_fail (provider, NULL);
  return NULL;
}
const gchar*
gda_sqlite_provider_iface_get_version (GdaProvider *provider) {
  g_return_val_if_fail (provider, NULL);
  return NULL;
}
const gchar*
gda_sqlite_provider_iface_get_server_version (GdaProvider *provider, GdaConnection *cnc) {
  g_return_val_if_fail (provider, NULL);
  return NULL;
}
gboolean
gda_sqlite_provider_iface_supports_feature (GdaProvider *provider, GdaConnection *cnc,
                               GdaConnectionFeature feature) {
  g_return_val_if_fail (provider, TRUE);
  return TRUE;
}
GdaSqlParser*
gda_sqlite_provider_iface_create_parser (GdaProvider *provider, GdaConnection *cnc) {
  g_return_val_if_fail (provider, NULL);
  return NULL;
} /* may be NULL */
GdaConnection*
gda_sqlite_provider_iface_create_connection (GdaProvider *provider) {
  g_return_val_if_fail (provider, NULL);
  return NULL;
}
GdaDataHandler*
gda_sqlite_provider_iface_get_data_handler (GdaProvider *provider, GdaConnection *cnc, /* may be NULL */
                                    GType g_type, const gchar *dbms_type) {
  g_return_val_if_fail (provider, NULL);
  return NULL;
}
const gchar*
gda_sqlite_provider_iface_get_def_dbms_type (GdaProvider *provider, GdaConnection *cnc,
                                    GType g_type) {
  g_return_val_if_fail (provider, NULL);
  return NULL;
} /* may be NULL */
gboolean
gda_sqlite_provider_iface_supports_operation (GdaProvider *provider, GdaConnection *cnc,
                                    GdaServerOperationType type, GdaSet *options) {
  g_return_val_if_fail (provider, FALSE);
  return FALSE;
}
GdaServerOperation*
gda_sqlite_provider_iface_create_operation (GdaProvider *provider, GdaConnection *cnc,
                                    GdaServerOperationType type, GdaSet *options,
                                    GError **error) {
  g_return_val_if_fail (provider, NULL);
  return NULL;
}
gchar*
gda_sqlite_provider_iface_render_operation (GdaProvider *provider, GdaConnection *cnc,
                                    GdaServerOperation *op, GError **error) {
  g_return_val_if_fail (provider, NULL);
  return NULL;
}
gchar*
gda_sqlite_provider_iface_statement_to_sql (GdaProvider *provider, GdaConnection *cnc,
                               GdaStatement *stmt, GdaSet *params,
                               GdaStatementSqlFlag flags,
                               GSList **params_used, GError **error) {
  g_return_val_if_fail (provider, NULL);
  return NULL;
}
gchar*
gda_sqlite_provider_iface_identifier_quote (GdaProvider *provider, GdaConnection *cnc, /* may be NULL */
                               const gchar *id,
                               gboolean for_meta_store, gboolean force_quotes) {
  g_return_val_if_fail (provider, NULL);
  return NULL;
}
GdaSqlStatement*
gda_sqlite_provider_iface_statement_rewrite (GdaProvider *provider, GdaConnection *cnc,
                                GdaStatement *stmt, GdaSet *params, GError **error) {
  g_return_val_if_fail (provider, NULL);
  return NULL;
}
gboolean
gda_sqlite_provider_iface_open_connection (GdaProvider *provider, GdaConnection *cnc,
                              GdaQuarkList *params, GdaQuarkList *auth) {
  g_return_val_if_fail (provider, FALSE);
  return FALSE;
}
gboolean
gda_sqlite_provider_iface_prepare_connection (GdaProvider *provider, GdaConnection *cnc,
                                 GdaQuarkList *params, GdaQuarkList *auth) {
  g_return_val_if_fail (provider, FALSE);
  return FALSE;
}
gboolean
gda_sqlite_provider_iface_close_connection (GdaProvider *provider, GdaConnection *cnc) {
  g_return_val_if_fail (provider, FALSE);
  return FALSE;
}
gchar*
gda_sqlite_provider_iface_escape_string (GdaProvider *provider, GdaConnection *cnc,
                            const gchar *str) {
  g_return_val_if_fail (provider, NULL);
  return NULL;
} /* may be NULL */
gchar*
gda_sqlite_provider_iface_unescape_string (GdaProvider *provider, GdaConnection *cnc,
                              const gchar *str) {
  g_return_val_if_fail (provider, NULL);
  return NULL;
} /* may be NULL */
gboolean
gda_sqlite_provider_iface_perform_operation (GdaProvider *provider, GdaConnection *cnc, /* may be NULL */
                                GdaServerOperation *op, GError **error) {
  g_return_val_if_fail (provider, FALSE);
  return FALSE;
}
gboolean
gda_sqlite_provider_iface_begin_transaction (GdaProvider *provider, GdaConnection *cnc,
                                const gchar *name, GdaTransactionIsolation level,
                                GError **error) {
  g_return_val_if_fail (provider, FALSE);
  return FALSE;
}
gboolean
gda_sqlite_provider_iface_commit_transaction (GdaProvider *provider, GdaConnection *cnc,
                                 const gchar *name, GError **error) {
  g_return_val_if_fail (provider, FALSE);
  return FALSE;
}
gboolean
gda_sqlite_provider_iface_rollback_transaction (GdaProvider *provider, GdaConnection *cnc,
                                   const gchar *name, GError **error) {
  g_return_val_if_fail (provider, FALSE);
  return FALSE;
}
gboolean
gda_sqlite_provider_iface_add_savepoint (GdaProvider *provider, GdaConnection *cnc,
                            const gchar *name, GError **error) {
  g_return_val_if_fail (provider, FALSE);
  return FALSE;
}
gboolean
gda_sqlite_provider_iface_rollback_savepoint (GdaProvider *provider, GdaConnection *cnc,
                                 const gchar *name, GError **error) {
  g_return_val_if_fail (provider, FALSE);
  return FALSE;
}
gboolean
gda_sqlite_provider_iface_delete_savepoint (GdaProvider *provider, GdaConnection *cnc,
                               const gchar *name, GError **error) {
  g_return_val_if_fail (provider, FALSE);
  return FALSE;
}
gboolean
gda_sqlite_provider_iface_statement_prepare (GdaProvider *provider, GdaConnection *cnc,
                                GdaStatement *stmt, GError **error) {
  g_return_val_if_fail (provider, FALSE);
  return FALSE;
}
GObject*
gda_sqlite_provider_iface_statement_execute (GdaProvider *provider, GdaConnection *cnc,
                                GdaStatement *stmt, GdaSet *params,
                                GdaStatementModelUsage model_usage,
                                GType *col_types, GdaSet **last_inserted_row,
                                GError **error) {
  g_return_val_if_fail (provider, NULL);
  return NULL;
}

GdaSet*
gda_sqlite_provider_iface_get_last_inserted (GdaProvider *provider,
                                GdaConnection *cnc,
                                GError **error)
{
  g_return_val_if_fail (provider, NULL);
  return NULL;
}

static GModule *
find_sqlite_in_dir (const gchar *dir_name, const gchar *name_part)
{
	GDir *dir;
	GError *err = NULL;
	const gchar *name;
	GModule *handle = NULL;

#ifdef GDA_DEBUG_NO
	g_print ("Looking for '%s' in %s\n", name_part, dir_name);
#endif
	dir = g_dir_open (dir_name, 0, &err);
	if (err) {
		gda_log_error (err->message);
		g_error_free (err);
		return FALSE;
	}

	while ((name = g_dir_read_name (dir))) {
		const gchar *ptr1, *ptr2;
		ptr1 = g_strrstr (name, "." G_MODULE_SUFFIX);
		if (! ptr1)
			continue;
		ptr2 = g_strrstr (name, name_part);
		if (!ptr2)
			continue;
		if (ptr1 < ptr2)
			continue;

		gchar *path;

		path = g_build_path (G_DIR_SEPARATOR_S, dir_name, name, NULL);
		handle = g_module_open (path, G_MODULE_BIND_LAZY | G_MODULE_BIND_LOCAL);
		g_free (path);
		if (!handle) {
			/*g_warning (_("Error: %s"), g_module_error ());*/
			continue;
		}

		gpointer func;
		if (g_module_symbol (handle, "sqlite3_open", &func)) {
#ifdef GDA_DEBUG_NO
			path = g_build_path (G_DIR_SEPARATOR_S, dir_name, name, NULL);
			g_print ("'%s' found as: '%s'\n", name_part, path);
			g_free (path);
#endif
			break;
		}
		else {
			g_module_close (handle);
			handle = NULL;
		}

	}
	/* free memory */
	g_dir_close (dir);
	return handle;
}

GModule *
gda_sqlite_find_library (const gchar *name_part)
{
	GModule *handle = NULL;
	const gchar *env;

	/* first use the compile time SEARCH_LIB_PATH */
	if (SEARCH_LIB_PATH) {
		gchar **array;
		gint i;
		array = g_strsplit (SEARCH_LIB_PATH, ":", 0);
		for (i = 0; array[i]; i++) {
			handle = find_sqlite_in_dir (array [i], name_part);
			if (handle)
				break;
		}
		g_strfreev (array);
		if (handle)
			return handle;
	}

	/* then try by 'normal' shared library finding */
	handle = g_module_open (name_part, G_MODULE_BIND_LAZY | G_MODULE_BIND_LOCAL);
	if (handle) {
		gpointer func;
		if (g_module_symbol (handle, "sqlite3_open", &func)) {
#ifdef GDA_DEBUG_NO
			g_print ("'%s' found using default shared library finding\n", name_part);
#endif
			return handle;
		}

		g_module_close (handle);
		handle = NULL;
	}

	/* lastly, use LD_LIBRARY_PATH */
	env = g_getenv ("LD_LIBRARY_PATH");
	if (env) {
		gchar **array;
		gint i;
		array = g_strsplit (env, ":", 0);
		for (i = 0; array[i]; i++) {
			handle = find_sqlite_in_dir (array [i], name_part);
			if (handle)
				break;
		}
		g_strfreev (array);
		if (handle)
			return handle;
	}

	return NULL;
}


void
gda_sqlite_load_symbols (GModule *module, Sqlite3ApiRoutines** apilib)
{
	g_assert (module);
	(*apilib) = g_new (Sqlite3ApiRoutines, 1);

	if (! g_module_symbol (module, "sqlite3_bind_blob", (gpointer*) &((*apilib)->sqlite3_bind_blob)))
		goto onerror;
	if (! g_module_symbol (module, "sqlite3_bind_double", (gpointer*) &((*apilib)->sqlite3_bind_double)))
		goto onerror;
	if (! g_module_symbol (module, "sqlite3_bind_int", (gpointer*) &((*apilib)->sqlite3_bind_int)))
		goto onerror;
	if (! g_module_symbol (module, "sqlite3_bind_int64", (gpointer*) &((*apilib)->sqlite3_bind_int64)))
		goto onerror;
	if (! g_module_symbol (module, "sqlite3_bind_null", (gpointer*) &((*apilib)->sqlite3_bind_null)))
		goto onerror;
	if (! g_module_symbol (module, "sqlite3_bind_text", (gpointer*) &((*apilib)->sqlite3_bind_text)))
		goto onerror;
	if (! g_module_symbol (module, "sqlite3_bind_zeroblob", (gpointer*) &((*apilib)->sqlite3_bind_zeroblob)))
		goto onerror;
	if (! g_module_symbol (module, "sqlite3_blob_bytes", (gpointer*) &((*apilib)->sqlite3_blob_bytes)))
		goto onerror;
	if (! g_module_symbol (module, "sqlite3_blob_close", (gpointer*) &((*apilib)->sqlite3_blob_close)))
		goto onerror;
	if (! g_module_symbol (module, "sqlite3_blob_open", (gpointer*) &((*apilib)->sqlite3_blob_open)))
		goto onerror;
	if (! g_module_symbol (module, "sqlite3_blob_read", (gpointer*) &((*apilib)->sqlite3_blob_read)))
		goto onerror;
	if (! g_module_symbol (module, "sqlite3_blob_write", (gpointer*) &((*apilib)->sqlite3_blob_write)))
		goto onerror;
	if (! g_module_symbol (module, "sqlite3_busy_timeout", (gpointer*) &((*apilib)->sqlite3_busy_timeout)))
		goto onerror;
	if (! g_module_symbol (module, "sqlite3_changes", (gpointer*) &((*apilib)->sqlite3_changes)))
		goto onerror;
	if (! g_module_symbol (module, "sqlite3_clear_bindings", (gpointer*) &((*apilib)->sqlite3_clear_bindings)))
		goto onerror;
	if (! g_module_symbol (module, "sqlite3_close", (gpointer*) &((*apilib)->sqlite3_close)))
		goto onerror;
	if (! g_module_symbol (module, "sqlite3_close_v2", (gpointer*) &((*apilib)->sqlite3_close_v2)))
		goto onerror;
	if (! g_module_symbol (module, "sqlite3_column_blob", (gpointer*) &((*apilib)->sqlite3_column_blob)))
		goto onerror;
	if (! g_module_symbol (module, "sqlite3_column_bytes", (gpointer*) &((*apilib)->sqlite3_column_bytes)))
		goto onerror;
	if (! g_module_symbol (module, "sqlite3_column_count", (gpointer*) &((*apilib)->sqlite3_column_count)))
		goto onerror;
	if (! g_module_symbol (module, "sqlite3_column_database_name", (gpointer*) &((*apilib)->sqlite3_column_database_name)))
		goto onerror;
	if (! g_module_symbol (module, "sqlite3_column_decltype", (gpointer*) &((*apilib)->sqlite3_column_decltype)))
		goto onerror;
	if (! g_module_symbol (module, "sqlite3_column_double", (gpointer*) &((*apilib)->sqlite3_column_double)))
		goto onerror;
	if (! g_module_symbol (module, "sqlite3_column_int", (gpointer*) &((*apilib)->sqlite3_column_int)))
		goto onerror;
	if (! g_module_symbol (module, "sqlite3_column_int64", (gpointer*) &((*apilib)->sqlite3_column_int64)))
		goto onerror;
	if (! g_module_symbol (module, "sqlite3_column_name", (gpointer*) &((*apilib)->sqlite3_column_name)))
		goto onerror;
	if (! g_module_symbol (module, "sqlite3_column_origin_name", (gpointer*) &((*apilib)->sqlite3_column_origin_name)))
		goto onerror;
	if (! g_module_symbol (module, "sqlite3_column_table_name", (gpointer*) &((*apilib)->sqlite3_column_table_name)))
		goto onerror;
	if (! g_module_symbol (module, "sqlite3_column_text", (gpointer*) &((*apilib)->sqlite3_column_text)))
		goto onerror;
	if (! g_module_symbol (module, "sqlite3_column_type", (gpointer*) &((*apilib)->sqlite3_column_type)))
		goto onerror;
	if (! g_module_symbol (module, "sqlite3_config", (gpointer*) &((*apilib)->sqlite3_config)))
		goto onerror;
	if (! g_module_symbol (module, "sqlite3_create_function", (gpointer*) &((*apilib)->sqlite3_create_function)))
		goto onerror;
	if (! g_module_symbol (module, "sqlite3_create_function_v2", (gpointer*) &((*apilib)->sqlite3_create_function_v2)))
		goto onerror;
	if (! g_module_symbol (module, "sqlite3_create_module", (gpointer*) &((*apilib)->sqlite3_create_module)))
		goto onerror;
	if (! g_module_symbol (module, "sqlite3_db_handle", (gpointer*) &((*apilib)->sqlite3_db_handle)))
		goto onerror;
	if (! g_module_symbol (module, "sqlite3_declare_vtab", (gpointer*) &((*apilib)->sqlite3_declare_vtab)))
		goto onerror;
	if (! g_module_symbol (module, "sqlite3_errcode", (gpointer*) &((*apilib)->sqlite3_errcode)))
		goto onerror;
	if (! g_module_symbol (module, "sqlite3_errmsg", (gpointer*) &((*apilib)->sqlite3_errmsg)))
		goto onerror;
	if (! g_module_symbol (module, "sqlite3_exec", (gpointer*) &((*apilib)->sqlite3_exec)))
		goto onerror;
	if (! g_module_symbol (module, "sqlite3_extended_result_codes", (gpointer*) &((*apilib)->sqlite3_extended_result_codes)))
		goto onerror;
	if (! g_module_symbol (module, "sqlite3_finalize", (gpointer*) &((*apilib)->sqlite3_finalize)))
		goto onerror;
	if (! g_module_symbol (module, "sqlite3_free", (gpointer*) &((*apilib)->sqlite3_free)))
		goto onerror;
	if (! g_module_symbol (module, "sqlite3_free_table", (gpointer*) &((*apilib)->sqlite3_free_table)))
		goto onerror;
	if (! g_module_symbol (module, "sqlite3_get_table", (gpointer*) &((*apilib)->sqlite3_get_table)))
		goto onerror;
	if (! g_module_symbol (module, "sqlite3_last_insert_rowid", (gpointer*) &((*apilib)->sqlite3_last_insert_rowid)))
		goto onerror;
	if (! g_module_symbol (module, "sqlite3_malloc", (gpointer*) &((*apilib)->sqlite3_malloc)))
		goto onerror;
	if (! g_module_symbol (module, "sqlite3_mprintf", (gpointer*) &((*apilib)->sqlite3_mprintf)))
		goto onerror;
	if (! g_module_symbol (module, "sqlite3_open", (gpointer*) &((*apilib)->sqlite3_open)))
		goto onerror;
	if (! g_module_symbol (module, "sqlite3_open_v2", (gpointer*) &((*apilib)->sqlite3_open_v2)))
		goto onerror;
	if (! g_module_symbol (module, "sqlite3_prepare", (gpointer*) &((*apilib)->sqlite3_prepare)))
		goto onerror;
	if (! g_module_symbol (module, "sqlite3_prepare_v2", (gpointer*) &((*apilib)->sqlite3_prepare_v2)))
		goto onerror;
	if (! g_module_symbol (module, "sqlite3_reset", (gpointer*) &((*apilib)->sqlite3_reset)))
		goto onerror;
	if (! g_module_symbol (module, "sqlite3_result_blob", (gpointer*) &((*apilib)->sqlite3_result_blob)))
		goto onerror;
	if (! g_module_symbol (module, "sqlite3_result_double", (gpointer*) &((*apilib)->sqlite3_result_double)))
		goto onerror;
	if (! g_module_symbol (module, "sqlite3_result_error", (gpointer*) &((*apilib)->sqlite3_result_error)))
		goto onerror;
	if (! g_module_symbol (module, "sqlite3_result_int", (gpointer*) &((*apilib)->sqlite3_result_int)))
		goto onerror;
	if (! g_module_symbol (module, "sqlite3_result_int64", (gpointer*) &((*apilib)->sqlite3_result_int64)))
		goto onerror;
	if (! g_module_symbol (module, "sqlite3_result_null", (gpointer*) &((*apilib)->sqlite3_result_null)))
		goto onerror;
	if (! g_module_symbol (module, "sqlite3_result_text", (gpointer*) &((*apilib)->sqlite3_result_text)))
		goto onerror;
	if (! g_module_symbol (module, "sqlite3_step", (gpointer*) &((*apilib)->sqlite3_step)))
		goto onerror;
	if (! g_module_symbol (module, "sqlite3_table_column_metadata", (gpointer*) &((*apilib)->sqlite3_table_column_metadata)))
		goto onerror;
	if (! g_module_symbol (module, "sqlite3_threadsafe", (gpointer*) &((*apilib)->sqlite3_threadsafe)))
		goto onerror;
	if (! g_module_symbol (module, "sqlite3_value_blob", (gpointer*) &((*apilib)->sqlite3_value_blob)))
		goto onerror;
	if (! g_module_symbol (module, "sqlite3_value_bytes", (gpointer*) &((*apilib)->sqlite3_value_bytes)))
		goto onerror;
	if (! g_module_symbol (module, "sqlite3_value_int", (gpointer*) &((*apilib)->sqlite3_value_int)))
		goto onerror;
	if (! g_module_symbol (module, "sqlite3_value_int64", (gpointer*) &((*apilib)->sqlite3_value_int64)))
		goto onerror;
	if (! g_module_symbol (module, "sqlite3_value_double", (gpointer*) &((*apilib)->sqlite3_value_double)))
		goto onerror;
	if (! g_module_symbol (module, "sqlite3_value_text", (gpointer*) &((*apilib)->sqlite3_value_text)))
		goto onerror;
	if (! g_module_symbol (module, "sqlite3_value_type", (gpointer*) &((*apilib)->sqlite3_value_type)))
		goto onerror;
	if (! g_module_symbol (module, "sqlite3_key", (gpointer*) &((*apilib)->sqlite3_key)))
		(*apilib)->sqlite3_key = NULL;
	if (! g_module_symbol (module, "sqlite3_key_v2", (gpointer*) &((*apilib)->sqlite3_key_v2)))
		(*apilib)->sqlite3_key_v2 = NULL;
	if (! g_module_symbol (module, "sqlite3_rekey", (gpointer*) &((*apilib)->sqlite3_key)))
		(*apilib)->sqlite3_rekey = NULL;

	if (! g_module_symbol (module, "sqlite3_create_collation", (gpointer*) &((*apilib)->sqlite3_create_collation)))
		goto onerror;
	if (! g_module_symbol (module, "sqlite3_enable_load_extension", (gpointer*) &((*apilib)->sqlite3_enable_load_extension)))
		(*apilib)->sqlite3_enable_load_extension = NULL;
	return;

 onerror:
	g_free ((*apilib));
	apilib = NULL;
	g_module_close (module);
}


