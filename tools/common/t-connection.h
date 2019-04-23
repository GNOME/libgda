/*
 * Copyright (C) 2014 - 2015 Vivien Malerba <malerba@gnome-db.org>
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

#ifndef __T_CONNECTION_H_
#define __T_CONNECTION_H_

#include <libgda/libgda.h>
#include "t-favorites.h"
#include "t-decl.h"
#ifdef HAVE_LDAP
#include <providers/ldap/gda-ldap-connection.h>
#endif

G_BEGIN_DECLS

#define T_TYPE_CONNECTION          (t_connection_get_type())
#define T_CONNECTION(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, t_connection_get_type(), TConnection)
#define T_CONNECTION_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, t_connection_get_type (), TConnectionClass)
#define T_IS_CONNECTION(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, t_connection_get_type ())

typedef struct _TConnection TConnection;
typedef struct _TConnectionClass TConnectionClass;
typedef struct _TConnectionPrivate TConnectionPrivate;

/* struct for the object's data */
struct _TConnection
{
	GObject                  object;
	TConnectionPrivate      *priv;
};

/* struct for the object's class */
struct _TConnectionClass
{
	GObjectClass              parent_class;

	/* signals */
	void                    (*busy) (TConnection *tcnc, gboolean is_busy, const gchar *reason);
	void                    (*status_changed) (TConnection *tcnc, GdaConnectionStatus status);
	void                    (*meta_changed) (TConnection *tcnc, GdaMetaStruct *mstruct);
	void                    (*favorites_changed) (TConnection *tcnc);
	void                    (*transaction_status_changed) (TConnection *tcnc);
	void                    (*table_column_pref_changed) (TConnection *tcnc, GdaMetaTable *table,
							      GdaMetaTableColumn *column,
							      const gchar *attr_name, const gchar *value);
	void                    (*notice) (TConnection *tcnc, const gchar *notice);
};

/**
 * SECTION:t-connection
 * @short_description: An opened connection
 * @title: TConnection
 * @stability: Stable
 * @see_also:
 *
 * The #TConnection object wraps a #GdaConnection with some
 * additionnal features. The wrapped #GdaConnection is only accessible from within
 * the #TConnection object, so to use a connection, you have to use the
 * #TConnection's methods.
 */

gboolean            t_connection_name_is_valid          (const gchar *name);
GType               t_connection_get_type               (void) G_GNUC_CONST;

TConnection        *t_connection_open                   (const gchar *cnc_name, const gchar *cnc_string, const gchar *auth_string,
							 gboolean use_term, GError **error);
void                t_connection_close                  (TConnection *tcnc);
TConnection        *t_connection_new                    (GdaConnection *cnc);
TConnection        *t_connection_get_by_name            (const gchar *name);
GdaConnection      *t_connection_get_cnc                (TConnection *tcnc);
void                t_connection_set_name               (TConnection *tcnc, const gchar *name);
const gchar        *t_connection_get_name               (TConnection *tcnc);
const gchar        *t_connection_get_information        (TConnection *tcnc);
gchar              *t_connection_get_long_name          (TConnection *tcnc);
const GdaDsnInfo   *t_connection_get_dsn_information    (TConnection *tcnc);

gboolean            t_connection_is_busy                (TConnection *tcnc, gchar **out_reason);
gboolean            t_connection_is_virtual             (TConnection *tcnc);
void                t_connection_update_meta_data       (TConnection *tcnc, GError **error);
void                t_connection_meta_data_changed      (TConnection *tcnc);
GdaMetaStruct      *t_connection_get_meta_struct        (TConnection *tcnc);
GdaMetaStore       *t_connection_get_meta_store         (TConnection *tcnc);
const gchar        *t_connection_get_dictionary_file    (TConnection *tcnc);

TFavorites         *t_connection_get_favorites          (TConnection *tcnc);

gchar             **t_connection_get_completions        (TConnection *tcnc, const gchar *sql,
							 gint start, gint end);

void                t_connection_set_query_buffer       (TConnection *tcnc, const gchar *sql);
const gchar        *t_connection_get_query_buffer       (TConnection *tcnc);

/**
 * TConnectionJobCallback:
 * @tcnc: the #TConnection
 * @out_result: the execution result
 * @data: a pointer passed when calling the execution function such as t_connection_ldap_describe_entry()
 * @error: the error returned, if any
 *
 * Callback function called when a job (not a statement execution job) is finished.
 * the out_result is not used by the TConnection anymore after this function has been
 * called, so you need to keep it or free it.
 *
 * @error should not be modified.
 */
typedef void (*TConnectionJobCallback) (TConnection *tcnc,
					gpointer out_result, gpointer data, GError *error);
#define T_CONNECTION_JOB_CALLBACK(x) ((TConnectionJobCallback)(x))
void                t_connection_job_cancel             (TConnection *tcnc, guint job_id);

/*
 * statements's manipulations
 */
GdaSqlParser       *t_connection_get_parser             (TConnection *tcnc);
GdaSqlParser       *t_connection_create_parser          (TConnection *tcnc);
gchar              *t_connection_render_pretty_sql      (TConnection *tcnc,
							 GdaStatement *stmt);
GObject            *t_connection_execute_statement      (TConnection *tcnc,
							 GdaStatement *stmt,
							 GdaSet *params,
							 GdaStatementModelUsage model_usage,
							 GdaSet **last_insert_row,
							 GError **error);
void                t_connection_set_busy_state (TConnection *bcnc, gboolean busy, const gchar *busy_reason);
gboolean            t_connection_normalize_sql_statement(TConnection *tcnc,
							 GdaSqlStatement *sqlst, GError **error);
gboolean            t_connection_check_sql_statement_validify (TConnection *tcnc,
							       GdaSqlStatement *sqlst, GError **error);

/*
 * transactions
 */
GdaTransactionStatus *t_connection_get_transaction_status (TConnection *tcnc);
gboolean              t_connection_begin (TConnection *tcnc, GError **error);
gboolean              t_connection_commit (TConnection *tcnc, GError **error);
gboolean              t_connection_rollback (TConnection *tcnc, GError **error);

/*
 * preferences
 */
#define T_CONNECTION_COLUMN_PLUGIN "PLUGIN"
gboolean             t_connection_set_table_column_attribute (TConnection *tcnc,
							      GdaMetaTable *table,
							      GdaMetaTableColumn *column,
							      const gchar *attr_name,
							      const gchar *value, GError **error);
gchar               *t_connection_get_table_column_attribute (TConnection *tcnc,
							      GdaMetaTable *table,
							      GdaMetaTableColumn *column,
							      const gchar *attr_name,
							      GError **error);

void                 t_connection_define_ui_plugins_for_batch(TConnection *tcnc,
							      GdaBatch *batch, GdaSet *params);
void                 t_connection_define_ui_plugins_for_stmt (TConnection *tcnc,
							      GdaStatement *stmt, GdaSet *params);

/*
 * Variables used at various places and for which a copy of the last recent value
 * is stored in the TConnection object
 */
void                 t_connection_keep_variables (TConnection *tcnc, GdaSet *set);
void                 t_connection_load_variables (TConnection *tcnc, GdaSet *set);

/*
 * LDAP
 */
gboolean             t_connection_is_ldap        (TConnection *tcnc);
#ifdef HAVE_LDAP
const         gchar *t_connection_ldap_get_base_dn (TConnection *tcnc);
GdaDataModel        *t_connection_ldap_search (TConnection *tcnc,
					       const gchar *base_dn, const gchar *filter,
					       const gchar *attributes, GdaLdapSearchScope scope,
					       GError **error);
GdaLdapEntry        *t_connection_ldap_describe_entry (TConnection *tcnc, const gchar *dn, GError **error);
GdaLdapEntry       **t_connection_ldap_get_entry_children (TConnection *tcnc, const gchar *dn,
							   gchar **attributes, GError **error);
gboolean             t_connection_describe_table  (TConnection *tcnc, const gchar *table_name,
						   const gchar **out_base_dn, const gchar **out_filter,
						   const gchar **out_attributes,
						   GdaLdapSearchScope *out_scope, GError **error);

GdaLdapClass        *t_connection_get_class_info (TConnection *tcnc, const gchar *classname);
const GSList        *t_connection_get_top_classes (TConnection *tcnc);

gboolean             t_connection_declare_table   (TConnection *tcnc,
						   const gchar *table_name,
						   const gchar *base_dn,
						   const gchar *filter,
						   const gchar *attributes,
						   GdaLdapSearchScope scope,
						   GError **error);
gboolean             t_connection_undeclare_table   (TConnection *tcnc,
						     const gchar *table_name,
						     GError **error);

#endif /* HAVE_LDAP */

/* Information about the connection */
GdaSet              *t_connection_get_all_infos (TConnection *tcnc);

G_END_DECLS

#endif
