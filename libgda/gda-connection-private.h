/* GDA library
 * Copyright (C) 2006 - 2008 The GNOME Foundation.
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

#ifndef __GDA_CONNECTION_PRIVATE_H_
#define __GDA_CONNECTION_PRIVATE_H_

G_BEGIN_DECLS

/*
 * Provider's specific connection data management
 */
void     gda_connection_internal_set_provider_data (GdaConnection *cnc, gpointer data, GDestroyNotify destroy_func);
gpointer gda_connection_internal_get_provider_data (GdaConnection *cnc);

/*
 * Transaction related
 */
void gda_connection_internal_transaction_started (GdaConnection *cnc, const gchar *parent_trans, const gchar *trans_name, 
						  GdaTransactionIsolation isol_level);
void gda_connection_internal_transaction_rolledback (GdaConnection *cnc, const gchar *trans_name);
void gda_connection_internal_transaction_committed (GdaConnection *cnc, const gchar *trans_name);

void gda_connection_internal_sql_executed (GdaConnection *cnc, const gchar *sql, GdaConnectionEvent *error);
void gda_connection_internal_statement_executed (GdaConnection *cnc, GdaStatement *stmt, GdaConnectionEvent *error);

void gda_connection_internal_savepoint_added (GdaConnection *cnc, const gchar *parent_trans, const gchar *svp_name);
void gda_connection_internal_savepoint_rolledback (GdaConnection *cnc, const gchar *svp_name);
void gda_connection_internal_savepoint_removed (GdaConnection *cnc, const gchar *svp_name);
void gda_connection_internal_change_transaction_state (GdaConnection *cnc,
						       GdaTransactionStatusState newstate);

/* helper function, fuzzy analysis of "standard" SQL for transactions */
void gda_connection_internal_treat_sql (GdaConnection *cnc, const gchar *sql, GdaConnectionEvent *error);

void gda_connection_force_status (GdaConnection *cnc, gboolean opened);

/* 
 * prepared statements support
 */
void     gda_connection_add_prepared_statement (GdaConnection *cnc, GdaStatement *gda_stmt, gpointer prepared_stmt); 
void     gda_connection_del_prepared_statement (GdaConnection *cnc, GdaStatement *gda_stmt); 
gpointer gda_connection_get_prepared_statement (GdaConnection *cnc, GdaStatement *gda_stmt);

G_END_DECLS

#endif
