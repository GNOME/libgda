/* 
 * $Id$
 *
 * GNOME DB Sybase Provider
 * Copyright (C) 2000 Rodrigo Moya
 * Copyright (C) 2000 Stephan Heinze
 * Copyright (C) 2000 Holger Thon
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

// $Log$
// Revision 1.11.4.1  2001/09/16 20:12:39  rodrigo
// 2001-09-16  Mike <wingert.3@postbox.acs.ohio-state.edu>
//
// 	* adapted to new GdaError API
//
// Revision 1.11  2001/07/18 23:05:42  vivien
// Ran indent -br -i8 on the files
//
// Revision 1.10  2001/07/05 22:42:35  rodrigo
// 2001-07-06  Rodrigo Moya <rodrigo@gnome-db.org>
//
//      * merged libgda-client's GdaError and libgda-server's GdaServerError
//      into libgda-common's GdaError, as both classes were exactly the
//      same and are useful for both clients and providers
//
// Revision 1.9  2001/04/07 08:49:31  rodrigo
// 2001-04-07  Rodrigo Moya <rodrigo@gnome-db.org>
//
//      * objects renaming (Gda_* to Gda*) to conform to the GNOME
//      naming standards
//
// Revision 1.8  2001/02/14 17:31:59  rodrigo
// 2001-02-14   Rodrigo Moya <rodrigo@gnome-db.org>
//
//      * idl/GDA_Connection.idl: added new conversion methods
//      (xml2sql and sql2xml)
//      * lib/gda-client/gda-connection.[ch],
//      lib/gda-server/*, providers/*: added new IDL methods
//
// Revision 1.7  2000/11/27 17:50:27  rodrigo
// 2000-11-27   Rodrigo Moya <rodrigo@gnome-db.org>
//
//      * added modifySchema method to GDA::Connection interface
//
// Revision 1.6  2000/11/27 12:09:13  holger
// 2000-11-27   Holger Thon <holger.thon@gnome-db.org>
//
//      * added primebase server skeleton
//
// Revision 1.5  2000/11/21 19:57:13  holger
// 2000-11-21 Holger Thon <holger.thon@gnome-db.org>
//
//      - Fixed missing parameter in gda-sybase-server
//      - Removed ct_diag/cs_diag stuff completly from tds server
//
// Revision 1.4  2000/10/06 19:24:45  menthos
// Added Swedish entry to configure.in and changed some C++-style comments causing problems to
// C-style.
//
// Revision 1.3  2000/10/04 08:43:14  rodrigo
// Fixed untranslatable strings
//
// Revision 1.2  2000/10/04 08:40:05  rodrigo
// Fixed untranslatable strings
//
// Revision 1.1.1.1  2000/08/10 09:32:38  rodrigo
// First version of libgda separated from GNOME-DB
//
// Revision 1.2  2000/08/04 10:20:37  rodrigo
// New Sybase provider
//
//

#include "gda-sybase.h"
#include "ctype.h"

typedef GdaServerRecordset *(*schema_ops_fn) (GdaError *,
					      GdaServerConnection *,
					      GDA_Connection_Constraint *,
					      gint);

schema_ops_fn schema_ops[GDA_Connection_GDCN_SCHEMA_LAST] = {
	NULL,
};

// schema definitions
static GdaServerRecordset *schema_cols (GdaError *,
					GdaServerConnection *,
					GDA_Connection_Constraint *, gint);
static GdaServerRecordset *schema_procs (GdaError *,
					 GdaServerConnection *,
					 GDA_Connection_Constraint *, gint);
static GdaServerRecordset *schema_tables (GdaError *,
					  GdaServerConnection *,
					  GDA_Connection_Constraint *, gint);
static GdaServerRecordset *schema_types (GdaError *,
					 GdaServerConnection *,
					 GDA_Connection_Constraint *, gint);



// Further non-public definitions
static gint
gda_sybase_init_dsn_properties (GdaServerConnection *, const gchar *);
static gboolean gda_sybase_set_locale (GdaServerConnection *, const gchar *);

//  utility functions
static gchar *get_value (gchar * ptr);
static gchar *get_option (const gchar * dsn, const gchar * opt_name);


gboolean
gda_sybase_connection_new (GdaServerConnection * cnc)
{
	static gboolean initialized;
	sybase_Connection *scnc = NULL;
	gint i;

	if (!initialized) {
		for (i = GDA_Connection_GDCN_SCHEMA_AGGREGATES;
		     i <= GDA_Connection_GDCN_SCHEMA_LAST; i++) {
			schema_ops[i] = NULL;
		}


		schema_ops[GDA_Connection_GDCN_SCHEMA_COLS] = schema_cols;
		schema_ops[GDA_Connection_GDCN_SCHEMA_PROCS] = schema_procs;
		schema_ops[GDA_Connection_GDCN_SCHEMA_TABLES] = schema_tables;
		schema_ops[GDA_Connection_GDCN_SCHEMA_PROV_TYPES] =
			schema_types;
	}
	initialized = TRUE;

	scnc = g_new0 (sybase_Connection, 1);
	g_return_val_if_fail (scnc != NULL, FALSE);

	if (scnc) {
		// Initialize connection struct
		scnc->ctx = (CS_CONTEXT *) NULL;
		scnc->cnc = (CS_CONNECTION *) NULL;
		scnc->ret = CS_SUCCEED;
		scnc->cs_diag = FALSE;
		scnc->ct_diag = FALSE;

		// Rest is covered by gda_sybase_connection_clear_user_data()
		// Note that this force sets all remaining pointers to NULL!
		gda_sybase_connection_clear_user_data (cnc, TRUE);

		// Further initialization, bail out on error
		//          scnc->error = gda_error_new();
		//          if (!scnc->error) {
		//                  g_free((gpointer) scnc);
		//                  gda_log_error(_("Error allocating error structure"));
		//                  return FALSE;
		//          }               

		// Succeeded
		gda_server_connection_set_user_data (cnc, (gpointer) scnc);
		return TRUE;
	}
	else {
		gda_log_error (_("Not enough memory for sybase_Connection"));
		return FALSE;
	}

	return FALSE;
}

gint
gda_sybase_connection_open (GdaServerConnection * cnc,
			    const gchar * dsn,
			    const gchar * user, const gchar * passwd)
{
	sybase_Connection *scnc = NULL;

	g_return_val_if_fail (cnc != NULL, -1);
	gda_server_connection_set_username (cnc, user);
	gda_server_connection_set_password (cnc, passwd);
	gda_server_connection_set_dsn (cnc, dsn);
	scnc = (sybase_Connection *)
		gda_server_connection_get_user_data (cnc);
	g_return_val_if_fail (scnc != NULL, -1);

	gda_sybase_connection_clear_user_data (cnc, FALSE);

	if (gda_sybase_connection_reopen (cnc) != TRUE) {
		return -1;
	}

	return 0;
}

void
gda_sybase_connection_close (GdaServerConnection * cnc)
{
	sybase_Connection *scnc;

	g_return_if_fail (cnc != NULL);
	scnc = (sybase_Connection *)
		gda_server_connection_get_user_data (cnc);
	g_return_if_fail (scnc != NULL);

	// Close connection
	if (scnc->cnc) {
		if (SYB_CHK
		    ((CS_INT *) & scnc->ret, ct_close (scnc->cnc, CS_UNUSED),
		     NULL, NULL, cnc, NULL) != CS_SUCCEED) {
			gda_sybase_cleanup (scnc, scnc->ret,
					    "Could not close connection\n");
			return;
		}
		if (SYB_CHK ((CS_INT *) & scnc->ret, ct_con_drop (scnc->cnc),
			     NULL, NULL, cnc, NULL) != CS_SUCCEED) {
			gda_sybase_cleanup (scnc, scnc->ret,
					    "Could not drop connection structure\n");
			return;
		}
		scnc->cnc = (CS_CONNECTION *) NULL;
	}

	if (scnc->ctx) {
		// Exit client library and drop context structure
		if ((scnc->ret =
		     ct_exit (scnc->ctx, CS_UNUSED)) != CS_SUCCEED) {
			gda_sybase_cleanup (scnc, scnc->ret,
					    "ct_exit failed");
			return;
		}

		if ((scnc->ret = cs_ctx_drop (scnc->ctx)) != CS_SUCCEED) {
			gda_sybase_cleanup (scnc, scnc->ret,
					    "cs_ctx_drop failed");
			return;
		}
		scnc->ctx = (CS_CONTEXT *) NULL;
	}
}

gint
gda_sybase_connection_begin_transaction (GdaServerConnection * cnc)
{
	// FIXME: Implement transaction code
	return -1;
}

gint
gda_sybase_connection_commit_transaction (GdaServerConnection * cnc)
{
	// FIXME: Implement transaction code
	return -1;
}

gint
gda_sybase_connection_rollback_transaction (GdaServerConnection * cnc)
{
	// FIXME: Implement transaction code
	return -1;
}

GdaServerRecordset *
gda_sybase_connection_open_schema (GdaServerConnection * cnc,
				   GdaError * error,
				   GDA_Connection_QType t,
				   GDA_Connection_Constraint * constraints,
				   gint length)
{
	schema_ops_fn fn;

	g_return_val_if_fail (cnc != NULL, NULL);

	fn = schema_ops[(gint) t];
	if (fn) {
		return fn (error, cnc, constraints, length);
	}
	else {
		gda_log_error (_("Unhandled SCHEMA_QTYPE %d"), (gint) t);
	}

	return NULL;
}

glong
gda_sybase_connection_modify_schema (GdaServerConnection * cnc,
				     GDA_Connection_QType t,
				     GDA_Connection_Constraint * constraints,
				     gint length)
{
	return -1;
}

gint
gda_sybase_connection_start_logging (GdaServerConnection * cnc,
				     const gchar * filename)
{
	// FIXME: Implement gda logging facility
	return -1;
}

gint
gda_sybase_connection_stop_logging (GdaServerConnection * cnc)
{
	// FIXME: Implement gda logging facility
	return -1;
}

gchar *
gda_sybase_connection_create_table (GdaServerConnection * cnc,
				    GDA_RowAttributes * columns)
{
	// FIXME: Implement creation of tables
	return NULL;
}

gboolean
gda_sybase_connection_supports (GdaServerConnection * cnc,
				GDA_Connection_Feature feature)
{
	g_return_val_if_fail (cnc != NULL, FALSE);

	switch (feature) {
	case GDA_Connection_FEATURE_SQL:
		return TRUE;
	case GDA_Connection_FEATURE_FOREIGN_KEYS:
	case GDA_Connection_FEATURE_INHERITANCE:
	case GDA_Connection_FEATURE_OBJECT_ID:
	case GDA_Connection_FEATURE_PROCS:
	case GDA_Connection_FEATURE_SEQUENCES:
	case GDA_Connection_FEATURE_SQL_SUBSELECT:
	case GDA_Connection_FEATURE_TRANSACTIONS:
	case GDA_Connection_FEATURE_TRIGGERS:
	case GDA_Connection_FEATURE_VIEWS:
	case GDA_Connection_FEATURE_XML_QUERIES:
	default:
		return FALSE;
	}

	return FALSE;
}

const GDA_ValueType
gda_sybase_connection_get_gda_type (GdaServerConnection * cnc,
				    gulong sql_type)
{
	return sybase_get_gda_type ((CS_INT) sql_type);
}

const CS_INT
gda_sybase_connection_get_sql_type (GdaServerConnection * cnc,
				    GDA_ValueType gda_type)
{
	return sybase_get_sql_type (gda_type);
}

const gshort
gda_sybase_connection_get_c_type (GdaServerConnection * cnc,
				  GDA_ValueType gda_type)
{
	return sybase_get_c_type (gda_type);
}

gchar *
gda_sybase_connection_sql2xml (GdaServerConnection * cnc, const gchar * sql)
{
	return NULL;
}

gchar *
gda_sybase_connection_xml2sql (GdaServerConnection * cnc, const gchar * xml)
{
	return NULL;
}

void
gda_sybase_connection_free (GdaServerConnection * cnc)
{
	sybase_Connection *scnc;

	g_return_if_fail (cnc != NULL);

	scnc = (sybase_Connection *)
		gda_server_connection_get_user_data (cnc);
	if (scnc) {
		gda_sybase_connection_clear_user_data (cnc, FALSE);
		g_free ((gpointer) scnc);
		gda_server_connection_set_user_data (cnc, (gpointer) NULL);
	}
}

void
gda_sybase_connection_clear_user_data (GdaServerConnection * cnc,
				       gboolean force_set_null_pointers)
{
	sybase_Connection *scnc = NULL;

	g_return_if_fail (cnc != NULL);
	scnc = (sybase_Connection *)
		gda_server_connection_get_user_data (cnc);
	g_return_if_fail (scnc != NULL);

	// we do not touch CS_* variables and *_diag variables,
	// as they are essential for the connection

	scnc->serr.err_type = SYBASE_ERR_NONE;
	if (force_set_null_pointers) {
		scnc->serr.udeferr_msg = (gchar *) NULL;
		scnc->database = (gchar *) NULL;
	}
	else {
		if (scnc->serr.udeferr_msg) {
			g_free ((gpointer) scnc->serr.udeferr_msg);
		}
		if (scnc->database) {
			g_free ((gpointer) scnc->database);
		}
	}
}

/*
 * Schema functions
 */

static GdaServerRecordset *
schema_cols (GdaError * error,
	     GdaServerConnection * cnc,
	     GDA_Connection_Constraint * constraints, gint length)
{
	gchar *query = NULL;
	GdaServerCommand *cmd = NULL;
	GdaServerRecordset *recset = NULL;
	GDA_Connection_Constraint *ptr = NULL;
	gint cnt;
	gulong affected = 0;
	gchar *table_name = NULL;

	/* process constraints */
	ptr = constraints;
	for (cnt = 0; cnt < length && ptr != NULL; cnt++) {
		switch (ptr->ctype) {
		case GDA_Connection_OBJECT_NAME:
			if (table_name) {
				g_free ((gpointer) table_name);
			}
#ifdef SYBASE_DEBUG
			gda_log_message (_("schema_cols: table_name '%s'"),
					 ptr->value);
#endif
			table_name = g_strdup_printf ("%s", ptr->value);
			break;
		default:
		}
	}

	/* build command object and query */
	if (!table_name) {
		gda_log_error (_("No table name given for SCHEMA_COLS"));
		return NULL;
	}
	cmd = gda_server_command_new (cnc);
	if (!cmd) {
		g_free ((gpointer) table_name);
		gda_log_error (_("Could not allocate command structure"));
		return NULL;
	}
	query = g_strdup_printf ("SELECT c.name AS \"Name\", "
#ifdef SYBASE_DEBUG
				 "       c.colid, "
#endif
				 "       t.name AS \"Type\", "
				 "       c.length AS \"Size\", "
				 "       c.prec AS \"Precision\", "
				 "       t.allownulls AS \"Nullable\", "
				 "       null AS \"Comments\", "
				 "       c.domain, "
				 "       c.printfmt "
				 "  FROM syscolumns c, sysobjects o "
				 "       ,systypes t "
				 "    WHERE c.id = o.id "
				 "      AND c.usertype = t.usertype "
				 "    HAVING o.name = '%s' "
				 "  ORDER BY c.colid ASC", table_name);

	if (!query) {
		gda_log_error (_
			       ("Could not allocate enough memory for query"));
		gda_server_command_free (cmd);
		g_free ((gpointer) table_name);
		return NULL;
	}
	gda_server_command_set_text (cmd, query);
	recset = gda_server_command_execute (cmd, error, NULL, &affected, 0);
	g_free ((gpointer) query);
	g_free ((gpointer) table_name);

	if (recset) {
		return recset;
	}
	else {
		//          gda_server_error_make(error, NULL, cnc, __PRETTY_FUNCTION__);
		gda_log_error (_("Could not get recordset in schema_cols"));
	}

	return NULL;
}

static GdaServerRecordset *
schema_procs (GdaError * error,
	      GdaServerConnection * cnc,
	      GDA_Connection_Constraint * constraints, gint length)
{
	GString *query = NULL;
	GdaServerCommand *cmd = NULL;
	GdaServerRecordset *recset = NULL;
	GDA_Connection_Constraint *ptr = NULL;
	gboolean extra_info = FALSE;
	gint cnt;
	GString *condition = NULL;
	gulong affected = 0;

	/* process constraints */
	ptr = constraints;
	for (cnt = 0; cnt < length && ptr != NULL; cnt++) {
		switch (ptr->ctype) {
			//                        case GDA_Connection_EXTRA_INFO :
			//                                        extra_info = TRUE;
			//                                break;
			//                        case GDA_Connection_OBJECT_NAME :
			//                                        if (!condition) condition = g_string_new("");
			//                                        g_string_sprintfa(condition, "AND a.relname='%s' ", ptr->value);
			//#ifdef SYBASE_DEBUG
			//                                        gda_log_message(_("schema_tables: table name = '%s'"),
			//                                                        ptr->value);
			//#endif
			//                                break;
			//                        case GDA_Connection_OBJECT_SCHEMA :
			//                                        if (!condition) condition = g_string_new("");
			//                                        g_string_sprintfa(condition, "AND b.usename='%s' ", ptr->value);
			//#ifdef SYBASE_DEBUG
			//                                        gda_log_message(_("schema_tables: table_schema = '%s'"),
			//                                                        ptr->value);
			//#endif
			//                                break;
			//                        case GDA_Connection_OBJECT_CATALOG :
			//                                        gda_log_error(_("schema_procedures: proc_catalog = '%s' UNUSED!\n"), 
			//                                                ptr->value);
			//                                break;
		default:
			gda_log_error (_
				       ("schema_procs: invalid constraint type %d"),
				       ptr->ctype);
			g_string_free (condition, TRUE);
			return NULL;
		}
		ptr++;
	}

	cmd = gda_server_command_new (cnc);
	if (!cmd) {
		gda_log_error (_("Could not allocate cmd structure"));
		return NULL;
	}

	// Get all usertables and systables
	query = g_string_new ("SELECT o.name AS \"Name\" "
			      //                            "       null   AS \"Comment\" "
			      "FROM sysobjects o "
			      "  WHERE o.type = \"P\" "
			      "ORDER BY o.type DESC, " "         o.name ASC");

	if (!query) {
		gda_server_command_free (cmd);
		return NULL;
	}
	gda_server_command_set_text (cmd, query->str);
	recset = gda_server_command_execute (cmd, error, NULL, &affected, 0);
	g_string_free (query, TRUE);
	//    gda_server_command_free(cmd);

	return recset;
}

static GdaServerRecordset *
schema_tables (GdaError * error,
	       GdaServerConnection * cnc,
	       GDA_Connection_Constraint * constraints, gint length)
{
	GString *query = NULL;
	GdaServerCommand *cmd = NULL;
	GdaServerRecordset *recset = NULL;
	GDA_Connection_Constraint *ptr = NULL;
	gboolean extra_info = FALSE;
	gint cnt;
	GString *condition = NULL;
	gulong affected = 0;

	/* process constraints */
	ptr = constraints;
	for (cnt = 0; cnt < length && ptr != NULL; cnt++) {
		switch (ptr->ctype) {
			//                        case GDA_Connection_EXTRA_INFO :
			//                                        extra_info = TRUE;
			//                                break;
			//                        case GDA_Connection_OBJECT_NAME :
			//                                        if (!condition) condition = g_string_new("");
			//                                        g_string_sprintfa(condition, "AND a.relname='%s' ", ptr->value);
			//#ifdef SYBASE_DEBUG
			//                                        gda_log_message(_("schema_tables: table name = '%s'"),
			//                                                        ptr->value);
			//#endif
			//                                break;
			//                        case GDA_Connection_OBJECT_SCHEMA :
			//                                        if (!condition) condition = g_string_new("");
			//                                        g_string_sprintfa(condition, "AND b.usename='%s' ", ptr->value);
			//#ifdef SYBASE_DEBUG
			//                                        gda_log_message(_("schema_tables: table_schema = '%s'"),
			//                                                        ptr->value);
			//#endif
			//                                break;
			//                        case GDA_Connection_OBJECT_CATALOG :
			//                                        gda_log_error(_("schema_procedures: proc_catalog = '%s' UNUSED!"), 
			//                                                      ptr->value);
			//                                break;
		default:
			gda_log_error (_
				       ("schema_tables: invalid constraint type %d"),
				       ptr->ctype);
			g_string_free (condition, TRUE);
			return NULL;
		}
		ptr++;
	}

	cmd = gda_server_command_new (cnc);
	if (!cmd) {
		return NULL;
	}

	// Get all usertables and systables
	query = g_string_new ("SELECT o.name AS \"Name\" "
			      //                            "       null   AS \"Comment\" "
			      "FROM sysobjects o "
			      "  WHERE o.type = \"U\" "
			      "     OR o.type = \"S\" "
			      "ORDER BY o.type DESC, " "         o.name ASC");

	if (!query) {
		gda_server_command_free (cmd);
		return NULL;
	}
	gda_server_command_set_text (cmd, query->str);
	recset = gda_server_command_execute (cmd, error, NULL, &affected, 0);
	g_string_free (query, TRUE);
	//    gda_server_command_free(cmd);

	return recset;
}

static GdaServerRecordset *
schema_types (GdaError * error,
	      GdaServerConnection * cnc,
	      GDA_Connection_Constraint * constraints, gint length)
{
	GdaServerCommand *cmd = NULL;
	GdaServerRecordset *rec = NULL;
	GDA_Connection_Constraint *ptr = NULL;
	gint i = 0;
	GString *query = NULL;
	GString *condition = NULL;
	gulong affected = 0;

	ptr = constraints;

	for (i = 0; i < length && ptr != NULL; i++) {
		switch (ptr->ctype) {
		case GDA_Connection_OBJECT_NAME:
			if (!condition) {
				condition = g_string_new ("  WHERE ");
			}
			else {
				g_string_sprintfa (condition, "    AND ");
			}
			g_string_sprintfa (condition, "a.name='%s' ",
					   ptr->value);
			break;
		default:
			gda_log_error (_
				       ("Invalid or unsupported constraint type: %d"),
				       ptr->ctype);
			break;
		}
		ptr++;
	}

	cmd = gda_server_command_new (cnc);
	if (!cmd) {
		g_string_free (condition, TRUE);
		return NULL;
	}

	query = g_string_new ("SELECT a.name"
			      //                            "       a.usertype,"
			      //                            "       a.variable,"
			      //                            "       a.allownulls,"
			      //                            "       a.type,"
			      //                            "       a.length,"
			      //                            "       a.tdefault,"
			      //                            "       a.domain,"
			      //                            "       a.printfmt"
			      "  FROM systypes a ");

	if (!query) {
		gda_server_command_free (cmd);
		g_string_free (condition, TRUE);
		return NULL;
	}

	if (condition) {
		g_string_free (condition, TRUE);
	}
	g_string_append (query, "  ORDER BY a.name");
	cmd = gda_server_command_new (cnc);
	if (cmd) {
		gda_server_command_set_text (cmd, query->str);
		g_string_free (query, TRUE);
		rec = gda_server_command_execute (cmd, error, NULL, &affected,
						  0);
		return rec;
	}
	else {
		gda_log_error (_("Could not allocate GdaServerCommand"));
		g_string_free (query, TRUE);
	}

	return rec;
}

/*
schema_views
{
select o.name AS Name 
from sysobjects o
where o.type = 'V'
}
*/

gboolean
gda_sybase_connection_dead (GdaServerConnection * cnc)
{
	sybase_Connection *scnc;
	CS_INT con_status = 0;

	g_return_val_if_fail (cnc != NULL, FALSE);
	scnc = (sybase_Connection *)
		gda_server_connection_get_user_data (cnc);
	g_return_val_if_fail (scnc != NULL, FALSE);
	g_return_val_if_fail (scnc->cnc != NULL, FALSE);

	if ((scnc->ret = ct_con_props (scnc->cnc, CS_GET, CS_CON_STATUS,
				       &con_status, CS_UNUSED,
				       NULL)) != CS_SUCCEED) {
		gda_log_message (_("connection status: %x"), con_status);
		gda_log_message (_("return code: %x"), scnc->ret);
		return TRUE;
	}

	gda_log_message (_("connection status: %x"), con_status);

	// check for CS_CONSTAT_CONNECTED bit
	if ((con_status && CS_CONSTAT_CONNECTED) == CS_CONSTAT_CONNECTED) {
		return FALSE;
	}

	return TRUE;
}

// do NOT call this before having freed GdaServerCommand structures
// associated to the given connection
gboolean
gda_sybase_connection_reopen (GdaServerConnection * cnc)
{
	sybase_Connection *scnc;
	gboolean first_connection = TRUE;
	CS_CHAR buf[CS_MAX_CHAR];
	CS_BYTE *mempool;
	CS_INT memsize = 131072;
	gchar *dsn;

	// At least we need valid connection structs
	g_return_val_if_fail (cnc != NULL, FALSE);
	scnc = (sybase_Connection *)
		gda_server_connection_get_user_data (cnc);
	g_return_val_if_fail (scnc != NULL, FALSE);

	////////////////////////////////////////////////////////
	// Drop old connection data, if any
	////////////////////////////////////////////////////////

	if (scnc->cnc) {
		first_connection = FALSE;
		gda_log_error (_("attempting reconnection"));
		gda_sybase_connection_close (cnc);
	}

	////////////////////////////////////////////////////////
	// Initialize connection
	////////////////////////////////////////////////////////

	// Create context info and initialize ctlib
	if ((scnc->ret =
	     cs_ctx_alloc (CS_VERSION_100, &scnc->ctx)) != CS_SUCCEED) {
		gda_sybase_cleanup (scnc, scnc->ret,
				    "Could not allocate context");
		return FALSE;
	}
	if ((scnc->ret = ct_init (scnc->ctx, CS_VERSION_100)) != CS_SUCCEED) {
		gda_sybase_cleanup (scnc, scnc->ret,
				    "Could not initialize client library");
		return FALSE;
	}
	mempool = (CS_BYTE *) g_new0 (CS_BYTE, memsize);
	if (!mempool) {
		gda_sybase_cleanup (scnc, scnc->ret,
				    "Could not allocate mempool");
		return FALSE;
	}
	if ((scnc->ret = ct_config (scnc->ctx, CS_SET, CS_MEM_POOL,
				    mempool, memsize, NULL)) != CS_SUCCEED) {
		gda_sybase_cleanup (scnc, scnc->ret,
				    "Could not initialize mempool");
		g_free ((gpointer) mempool);
		return FALSE;
	}

	// locales must be set up before we have any connection,
	// so get dsn and initialize locale if given
	dsn = gda_server_connection_get_dsn (cnc);
	if ((gda_sybase_set_locale (cnc, dsn)) != TRUE) {
		gda_sybase_cleanup (scnc, scnc->ret, "Could not set locale");
		return FALSE;
	}

	// Initialize connection
	if ((scnc->ret = ct_con_alloc (scnc->ctx, &scnc->cnc)) != CS_SUCCEED) {
		gda_sybase_cleanup (scnc, scnc->ret,
				    "Could not allocate connection info");
		return FALSE;
	}

	/* Now we've got an initialized CS_CONTEXT and an allocated CS_COMMAND
	   and maybe setup the locale, so we can setup error handling
	   For inline messages handling:
	   Uncomment all following gda_sybase_messages_install conditions
	   for handling messages inline.
	   Do comment all conditions with gda_sybase_install_error_messages then!

	   For callback messages handling:
	   Uncomment all following gda_sybase_install_error_messages conditions.
	   Do comment all conditions with gda_sybase_messages_install then!
	 */
	if (gda_sybase_messages_install (cnc) != TRUE) {
		return FALSE;
	}

	/*
	   if (gda_sybase_install_error_handlers(cnc) != 0) {
	   return FALSE;
	   }
	 */

	// We go on proceeding connection properties and dsn arguments
	// set up application name, username and password
	if (SYB_CHK ((CS_INT *) & scnc->ret,
		     ct_con_props (scnc->cnc, CS_SET, CS_APPNAME,
				   (CS_CHAR *) GDA_SYBASE_DEFAULT_APPNAME,
				   CS_NULLTERM, NULL),
		     NULL, NULL, cnc, NULL) != CS_SUCCEED) {
		gda_sybase_cleanup (scnc, scnc->ret,
				    "Could not set application name");
		return FALSE;
	}
	if (SYB_CHK ((CS_INT *) & scnc->ret,
		     ct_con_props (scnc->cnc, CS_SET, CS_USERNAME,
				   (CS_CHAR *)
				   gda_server_connection_get_username (cnc),
				   CS_NULLTERM, NULL), NULL, NULL, cnc,
		     NULL) != CS_SUCCEED) {
		gda_sybase_cleanup (scnc, scnc->ret,
				    "Could not set user name");
		return FALSE;
	}
	if (SYB_CHK ((CS_INT *) & scnc->ret,
		     ct_con_props (scnc->cnc, CS_SET, CS_PASSWORD,
				   (CS_CHAR *)
				   gda_server_connection_get_password (cnc),
				   CS_NULLTERM, NULL), NULL, NULL, cnc,
		     NULL) != CS_SUCCEED) {
		gda_sybase_cleanup (scnc, scnc->ret,
				    "Could not set password");
		return FALSE;
	}

	// Proceed further parameters given in dsn
	if (((dsn = gda_server_connection_get_dsn (cnc)) != NULL) &&
	    (gda_sybase_init_dsn_properties (cnc, dsn) != 0)) {
		return FALSE;
	}

	// Connect
	if (SYB_CHK ((CS_INT *) & scnc->ret,
		     ct_connect (scnc->cnc, (CS_CHAR *) NULL, 0),
		     NULL, NULL, cnc, NULL) != CS_SUCCEED) {
		gda_sybase_cleanup (scnc, scnc->ret,
				    (first_connection == FALSE)
				    ? "Could not reconnect"
				    : "Could not connect");
		return FALSE;
	}

	// To test the connection, we ask for the servers name
	if (SYB_CHK ((CS_INT *) & scnc->ret,
		     ct_con_props (scnc->cnc, CS_GET, CS_SERVERNAME,
				   &buf, CS_MAX_CHAR, NULL),
		     NULL, NULL, cnc, NULL) != CS_SUCCEED) {
		gda_sybase_cleanup (scnc, scnc->ret,
				    "Could not request servername");
		return FALSE;
	}
	else {
		if (first_connection == FALSE)
			gda_log_message (_("Reconnected to '%s'"), buf);
		else
			gda_log_message (_("Connected to %s"), buf);
		if (scnc->database) {
			scnc->ret =
				gda_sybase_connection_select_database (cnc,
								       scnc->
								       database);
			if (scnc->ret != CS_SUCCEED) {
				gda_sybase_cleanup (scnc, scnc->ret,
						    "Could not select database");
				return FALSE;
			}
			else {
				gda_log_message (_("Selected database '%s'"),
						 scnc->database);
			}
		}
	}


	// We have connected successfully
	return TRUE;
}

CS_RETCODE
gda_sybase_connection_select_database (GdaServerConnection * cnc,
				       const gchar * database)
{
	sybase_Connection *scnc = NULL;
	CS_COMMAND *cmd = NULL;
	gchar *sqlcmd = NULL;
	CS_INT result_type;
	gboolean okay = FALSE;

	g_return_val_if_fail (cnc != NULL, CS_FAIL);
	scnc = (sybase_Connection *)
		gda_server_connection_get_user_data (cnc);
	g_return_val_if_fail (scnc != NULL, CS_FAIL);
	g_return_val_if_fail (scnc->cnc != NULL, CS_FAIL);

	if (SYB_CHK (NULL, ct_cmd_alloc (scnc->cnc, &cmd),
		     NULL, NULL, cnc, NULL) != CS_SUCCEED) {
		gda_log_error (_("%s: Could not allocate cmd structure"),
			       __PRETTY_FUNCTION__);
		return CS_FAIL;
	}

	// Prepare cmd string
	sqlcmd = g_strdup_printf ("use %s", database);
	if (!sqlcmd) {
		gda_log_error (_
			       ("%s: Could not allocate memory for cmd string"),
			       __PRETTY_FUNCTION__);
		SYB_CHK (NULL, ct_cmd_drop (cmd), NULL, NULL, cnc, NULL);
		return CS_FAIL;
	}
	// Pass cmd string to CS_COMMAND structure
	if (SYB_CHK (NULL, ct_command (cmd, CS_LANG_CMD, sqlcmd, CS_NULLTERM,
				       CS_UNUSED),
		     NULL, NULL, cnc, NULL) != CS_SUCCEED) {
		gda_log_error (_("%s: ct_command failed"),
			       __PRETTY_FUNCTION__);
		g_free ((gpointer) sqlcmd);
		SYB_CHK (NULL, ct_cmd_drop (cmd), NULL, NULL, cnc, NULL);
		return CS_FAIL;
	}
	g_free ((gpointer) sqlcmd);

	// Send the command
	if (SYB_CHK (NULL, ct_send (cmd), NULL, NULL, cnc, NULL) !=
	    CS_SUCCEED) {
		gda_log_error (_("%s: sending command failed"),
			       __PRETTY_FUNCTION__);
		SYB_CHK (NULL, ct_cmd_drop (cmd), NULL, NULL, cnc, NULL);
		return CS_FAIL;
	}

	while (CS_SUCCEED == SYB_CHK (NULL, ct_results (cmd, &result_type),
				      NULL, NULL, cnc, NULL)
		) {
		switch (result_type) {
		case CS_CMD_FAIL:
			gda_log_error (_("%s: selecting database failed"),
				       __PRETTY_FUNCTION__);
			break;
		case CS_CMD_SUCCEED:
			okay = TRUE;
			break;
		default:
			break;
		}
	}

	SYB_CHK (NULL, ct_cmd_drop (cmd), NULL, NULL, cnc, NULL);

	if (okay) {
		return CS_SUCCEED;
	}

	gda_log_error (_("%s: use command failed"), __PRETTY_FUNCTION__);
	return CS_FAIL;
}

// Select locale for connection; we just need a CS_CONTEXT at this time
static gboolean
gda_sybase_set_locale (GdaServerConnection * cnc, const gchar * dsn)
{
	sybase_Connection *scnc;
	CS_LOCALE *locale;
	CS_RETCODE ret = CS_FAIL;
	gchar *targ_locale = NULL;

	g_return_val_if_fail (cnc != NULL, FALSE);
	scnc = (sybase_Connection *)
		gda_server_connection_get_user_data (cnc);
	g_return_val_if_fail (scnc != NULL, FALSE);
	g_return_val_if_fail (scnc->ctx != NULL, FALSE);

	if ((targ_locale = get_option (dsn, "LOCALE=")) == NULL) {
		return TRUE;
	}

#ifdef SYBASE_DEBUG
	gda_log_message ("locale = '%s'", targ_locale);
#endif

	if ((ret = cs_loc_alloc (scnc->ctx, &locale)) != CS_SUCCEED) {
		gda_log_error (_("Could not allocate locale structure"));
		return FALSE;
	}
	if ((ret = cs_locale (scnc->ctx, CS_SET, locale, CS_LC_ALL,
			      (CS_CHAR *) targ_locale, CS_NULLTERM, NULL))
	    != CS_SUCCEED) {
		gda_log_error (_("Could not set locale to '%s'"),
			       targ_locale);
		cs_loc_drop (scnc->ctx, locale);
		return FALSE;
	}
	if ((ret = cs_config (scnc->ctx, CS_SET, CS_LOC_PROP, locale,
			      CS_UNUSED, NULL)) != CS_SUCCEED) {
		gda_log_error (_("Could not configure locale to be '%s'"),
			       targ_locale);
		cs_loc_drop (scnc->ctx, locale);
		return FALSE;
	}
	if ((ret = cs_loc_drop (scnc->ctx, locale)) != CS_SUCCEED) {
		gda_log_error (_("Could not drop locale structure"));
		return FALSE;
	}
	gda_log_message ("Selected locale '%s'", targ_locale);
	return TRUE;
}

// drop remaining cs/ctlib structures and exit
void
gda_sybase_cleanup (sybase_Connection * scnc, CS_RETCODE ret,
		    const gchar * msg)
{
	g_return_if_fail (scnc != NULL);

	gda_log_error (_("Fatal error: %s\n"), msg);

	if (scnc->cnc != NULL) {
		ct_con_drop (scnc->cnc);
		scnc->cnc = NULL;
	}

	if (scnc->ctx != NULL) {
		ct_exit (scnc->ctx, CS_FORCE_EXIT);
		cs_ctx_drop (scnc->ctx);
		scnc->ctx = NULL;
	}
}

/*
 * Private functions
 *
 *
 */

static gint
gda_sybase_init_dsn_properties (GdaServerConnection * cnc, const gchar * dsn)
{
	sybase_Connection *scnc;
	gchar *ptr_s, *ptr_e;

	g_return_val_if_fail (cnc != NULL, -1);
	scnc = (sybase_Connection *)
		gda_server_connection_get_user_data (cnc);

	if (scnc) {
		/* parse connection string */
		ptr_s = (gchar *) dsn;
		while (ptr_s && *ptr_s) {
			ptr_e = strchr (ptr_s, ';');

			if (ptr_e)
				*ptr_e = '\0';

			// HOST implies HOSTNAME ;-)
			if (strncasecmp (ptr_s, "HOST", strlen ("HOST")) == 0)
				scnc->ret =
					ct_con_props (scnc->cnc, CS_SET,
						      CS_HOSTNAME,
						      (CS_CHAR *)
						      get_value (ptr_s),
						      CS_NULLTERM, NULL);
			else if (strncasecmp
				 (ptr_s, "USERNAME",
				  strlen ("USERNAME")) == 0)
				scnc->ret =
					ct_con_props (scnc->cnc, CS_SET,
						      CS_USERNAME,
						      (CS_CHAR *)
						      get_value (ptr_s),
						      CS_NULLTERM, NULL);
			else if (strncasecmp
				 (ptr_s, "APPNAME", strlen ("APPNAME")) == 0)
				scnc->ret =
					ct_con_props (scnc->cnc, CS_SET,
						      CS_APPNAME,
						      (CS_CHAR *)
						      get_value (ptr_s),
						      CS_NULLTERM, NULL);
			else if (strncasecmp
				 (ptr_s, "PASSWORD",
				  strlen ("PASSWORD")) == 0)
				scnc->ret =
					ct_con_props (scnc->cnc, CS_SET,
						      CS_PASSWORD,
						      (CS_CHAR *)
						      get_value (ptr_s),
						      CS_NULLTERM, NULL);
			else if (strncasecmp
				 (ptr_s, "DATABASE",
				  strlen ("DATABASE")) == 0) {
				scnc->ret = CS_SUCCEED;
				if (scnc->database) {
					g_free ((gpointer) scnc->database);
				}
				scnc->database =
					g_strdup_printf ("%s",
							 get_value (ptr_s));
			}

			sybase_chkerr (NULL, NULL, cnc, NULL,
				       __PRETTY_FUNCTION__);
			if (scnc->ret != CS_SUCCEED) {
				gda_sybase_cleanup (scnc, scnc->ret,
						    "Could not proceed dsn settings.\n");
				return -1;
			}

			ptr_s = ptr_e;
			if (ptr_s)
				ptr_s++;
		}
	}

	return 0;
}

/*
 * Utility functions
 */

static gchar *
get_value (gchar * ptr)
{
	while (*ptr && (*ptr != '='))
		ptr++;

	if (!*ptr) {
		return NULL;
	}
	ptr++;
	if (!*ptr) {
		return NULL;
	}
	while (*ptr && isspace (*ptr))
		ptr++;

	return (g_strdup (ptr));
}

static gchar *
get_option (const gchar * dsn, const gchar * opt_name)
{
	gchar *ptr = NULL;
	gchar *opt = NULL;

	if (!(opt = strstr (dsn, opt_name))) {
		return NULL;
	}
	opt += strlen (opt_name);
	ptr = strchr (opt, ';');

	if (ptr)
		*ptr = '\0';

	return (g_strdup (opt));
}
