/* GNOME DB MYSQL Provider
 * Copyright (C) 1998 Michael Lausch
 * Copyright (C) 2000 Rodrigo Moya
 * Copyright (C) 2001 Vivien Malerba
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

#include "gda-mysql.h"
#include <ctype.h>

typedef GdaServerRecordset* (*schema_ops_fn)(GdaError *,
                                              GdaServerConnection *,
                                              GDA_Connection_Constraint *,
                                              gint );

static GdaServerRecordset* schema_tables (GdaError *error,
                                           GdaServerConnection *cnc,
                                           GDA_Connection_Constraint *constraints,
                                           gint length);
static GdaServerRecordset* schema_columns (GdaError *error,
                                            GdaServerConnection *cnc,
                                            GDA_Connection_Constraint *constraints,
                                            gint length);

static GdaServerRecordset *schema_types (GdaError *error,
					 GdaServerConnection *cnc,
					 GDA_Connection_Constraint *constraint,
					 gint length);
     
schema_ops_fn schema_ops[GDA_Connection_GDCN_SCHEMA_LAST] = { 0, };

#define MYSQL_Types_Array_Nb 18
MYSQL_Types_Array types_array[MYSQL_Types_Array_Nb+1] = { /* +1 for the dummy BOOL data type */
        {"bigint", FIELD_TYPE_LONGLONG, GDA_TypeUBigint, SQL_C_ULONG, "Large integer: the signed range is `-9223372036854775808' to `9223372036854775807' and the unsigned one is `0' to `18446744073709551615'", "dbowner"}, 
        {"blob", FIELD_TYPE_BLOB, GDA_TypeVarchar, SQL_C_BINARY, "`BLOB' or `TEXT' column with a maximum length of 65535 (2^16 - 1) characters", "dbowner"}, 
        {"char", FIELD_TYPE_TINY, GDA_TypeChar, SQL_C_CHAR, "Fixed-length string that is always right-padded with spaces to the specified length", "dbowner"}, 
        {"date", FIELD_TYPE_DATE, GDA_TypeDbDate, SQL_C_DATE, "Date: the supported range is `'1000-01-01'' to `'9999-12-31''", "dbowner"}, 
        {"datetime", FIELD_TYPE_DATETIME, GDA_TypeDbTimestamp, SQL_C_TIMESTAMP, "Date and time combination: the supported range is `'1000-01-01 00:00:00'' to `'9999-12-31 23:59:59''", "dbowner"}, 
        {"decimal", FIELD_TYPE_DECIMAL, GDA_TypeDecimal, SQL_C_FLOAT, "floating-point number", "dbowner"}, 
        {"double", FIELD_TYPE_DOUBLE, GDA_TypeDouble, SQL_C_DOUBLE, "Normal-size (double-precision) floating-point number cannot be unsigned", "dbowner"}, 
        {"enum", FIELD_TYPE_ENUM, GDA_TypeVarchar, SQL_C_CHAR, "Enumeration: a string object that can have only one value, chosen from the list of values", "dbowner"}, 
        {"float", FIELD_TYPE_FLOAT, GDA_TypeSingle, SQL_C_FLOAT, "A floating-point number, cannot be unsigned", "dbowner"}, 
        {"integer", FIELD_TYPE_LONG, GDA_TypeBigint, SQL_C_SLONG, "Normal-size integer: the signed range is `-2147483648' to `2147483647' and the unsigned one is `0' to `4294967295'", "dbowner"}, 
        {"mediumint", FIELD_TYPE_INT24, GDA_TypeBigint, SQL_C_SLONG, "Medium-size integer: the signed range is `-8388608' to `8388607' and the unsigned one is `0' to `16777215'", "dbowner"}, 
        {"set", FIELD_TYPE_SET, GDA_TypeVarchar, SQL_C_BINARY, "Set: a string object that can have zero or more values, each of which must be chosen from the list of values", "dbowner"}, 
        {"smallint", FIELD_TYPE_SHORT, GDA_TypeSmallint, SQL_C_SSHORT, "Small integer: the signed range is `-32768' to `32767' and the unsigned one is `0' to `65535'", "dbowner"}, 
        {"time", FIELD_TYPE_TIME, GDA_TypeDbTime, SQL_C_TIME, "Time: the range is `'-838:59:59'' to `'838:59:59''", "dbowner"}, 
        {"timestamp", FIELD_TYPE_TIMESTAMP, GDA_TypeDbTimestamp, SQL_C_TIMESTAMP, "Timestamp: the range is `'1970-01-01 00:00:00'' to sometime in the year `2037'", "dbowner"}, 
        {"varchar", FIELD_TYPE_STRING, GDA_TypeVarchar, SQL_C_CHAR, "Variable-length string", "dbowner"},
        {"year", FIELD_TYPE_YEAR, GDA_TypeInteger, SQL_C_SLONG, "Year in 2- or 4-digit format (default is 4-digit): the allowable values are `1901' to `2155', `0000' in the 4-digit year format, and 1970-2069 if you use the 2-digit format (70-69)", "dbowner"}, 
        {"null", FIELD_TYPE_NULL, GDA_TypeNull, SQL_C_CHAR, "Unknown", "dbowner"},
	{"bool", FIELD_TYPE_BOOL, GDA_TypeBoolean, SQL_C_BIT, "Does not go to the user!", "GDA only"}
};

/*
 * Public functions
 */
gboolean
gda_mysql_connection_new (GdaServerConnection *cnc)
{
	static gboolean   initialized = FALSE;
	MYSQL_Connection* mysql_cnc;
	
	/* initialize schema functions */
	if (!initialized) {
		schema_ops[GDA_Connection_GDCN_SCHEMA_TABLES] = schema_tables;
		schema_ops[GDA_Connection_GDCN_SCHEMA_COLS] = schema_columns;
		schema_ops[GDA_Connection_GDCN_SCHEMA_PROV_TYPES] = schema_types;
	}
	
	mysql_cnc = g_new0(MYSQL_Connection, 1);
	mysql_cnc->types_array = types_array;
	gda_server_connection_set_user_data(cnc, (gpointer) mysql_cnc);
	
	return TRUE;
}

static gchar*
get_value (gchar* ptr)
{
	while (*ptr && *ptr != '=') ptr++;
	if (!*ptr)
		return 0;
	ptr++;
	if (!*ptr)
		return 0;

	while (*ptr && isspace(*ptr)) ptr++;

	return (g_strdup(ptr));
}

gint
gda_mysql_connection_open (GdaServerConnection *cnc,
                           const gchar *dsn,
                           const gchar *user,
                           const gchar *password)
{
	MYSQL_Connection* mysql_cnc;
	gchar*            ptr_s;
	gchar*            ptr_e;
	MYSQL*            rc;
	gchar*            t_host = NULL;
	gchar*            t_db = NULL;
	gchar*            t_user = NULL;
	gchar*            t_password = NULL;
	gchar*            t_port = NULL;
	gchar*            t_unix_socket = NULL;
	gchar*            t_flags = NULL;
#if MYSQL_VERSION_ID < 32200
	gint              err;
#endif

	g_return_val_if_fail(cnc != NULL, -1);
	
	mysql_cnc = (MYSQL_Connection *) gda_server_connection_get_user_data(cnc);
	if (mysql_cnc) {
		/* parse connection string */
		ptr_s = (gchar *) dsn;
		while (ptr_s && *ptr_s) {
			ptr_e = strchr(ptr_s, ';');
			
			if (ptr_e)
				*ptr_e = '\0';
			if (strncasecmp(ptr_s, "HOST", strlen("HOST")) == 0)
				t_host = get_value(ptr_s);
			else if (strncasecmp(ptr_s, "DATABASE", strlen("DATABASE")) == 0)
				t_db = get_value(ptr_s);
			else if (strncasecmp(ptr_s, "USERNAME", strlen("USERNAME")) == 0)
				t_user = get_value(ptr_s);
			else if (strncasecmp(ptr_s, "PASSWORD", strlen("PASSWORD")) == 0)
				t_password = get_value(ptr_s);
			else if (strncasecmp(ptr_s, "PORT", strlen("PORT")) == 0)
				t_port = get_value(ptr_s);
			else if (strncasecmp(ptr_s, "UNIX_SOCKET", strlen("UNIX_SOCKET")) == 0)
				t_unix_socket = get_value(ptr_s);
			else if (strncasecmp(ptr_s, "FLAGS", strlen("FLAGS")) == 0)
				t_flags = get_value(ptr_s);

			ptr_s = ptr_e;
			if (ptr_s)
				ptr_s++;
		}

		/* okay, now let's copy the overriding user and/or password if applicable */
		if (user) {
			if (t_user) g_free((gpointer) t_user);
			t_user = g_strdup(user);
		}
		if (password) {
			if (t_password) g_free((gpointer) t_password);
			t_password = g_strdup(password);
		}

		/* we can't have both a host/pair AND a unix_socket */
		/* if both are provided, error out now... */
		if ((t_host || t_port) && t_unix_socket) {
			gda_log_error(_("%s:%d: You cannot provide a UNIX_SOCKET if you also provide"
			                " either a HOST or a PORT. Please remove the UNIX_SOCKET, or"
			                " remove both the HOST and PORT options"),
			                __FILE__, __LINE__);
			return -1;
		}

		/* set the default of localhost:3306 if neither is provided */
		if (!t_unix_socket) {
			if (!t_port) t_port = g_strdup("3306");
			if (!t_host) t_host = g_strdup("localhost");
		}

		gda_log_message(_("Opening connection with user=%s, password=%s, "
		                  "host=%s, port=%s, unix_socket=%s"), t_user, t_password,
		                  t_host, t_port, t_unix_socket);
		mysql_cnc->mysql = g_new0(MYSQL, 1);

		rc = mysql_real_connect(mysql_cnc->mysql,
		                        t_host,
		                        t_user,
		                        t_password,
#if MYSQL_VERSION_ID >= 32200
		                        t_db,
#endif
		                        t_port ? atoi(t_port) : 0,
		                        t_unix_socket,
		                        t_flags ? atoi(t_flags) : 0);
		if (!rc) {
			gda_server_error_make(gda_error_new(), 0, cnc, __PRETTY_FUNCTION__);
			return -1;
		}
#if MYSQL_VERSION_ID < 32200
		err = mysql_select_db(mysql_cnc->mysql, t_db);
		if (err != 0) {
			gda_server_error_make(gda_error_new(), 0, cnc, __PRETTY_FUNCTION__);
			return -1;
		}
#endif

		/* free memory */
		g_free((gpointer) t_host);
		g_free((gpointer) t_db);
		g_free((gpointer) t_user);
		g_free((gpointer) t_password);
		g_free((gpointer) t_port);
		g_free((gpointer) t_unix_socket);
		g_free((gpointer) t_flags);

		return 0;
	}
	return -1;
}

void
gda_mysql_connection_close (GdaServerConnection *cnc)
{
	MYSQL_Connection* mysql_cnc;
	
	g_return_if_fail(cnc != NULL);
	
	mysql_cnc = (MYSQL_Connection *) gda_server_connection_get_user_data(cnc);
	if (mysql_cnc) {
	        if (mysql_cnc->mysql) {
		        mysql_close(mysql_cnc->mysql);
			mysql_cnc->mysql=NULL;
			mysql_cnc->types_array=NULL;
		}
	}
}

gint
gda_mysql_connection_begin_transaction (GdaServerConnection *cnc)
{
	return -1;
}

gint
gda_mysql_connection_commit_transaction (GdaServerConnection *cnc)
{
	return -1;
}

gint
gda_mysql_connection_rollback_transaction (GdaServerConnection *cnc)
{
	return -1;
}

GdaServerRecordset *
gda_mysql_connection_open_schema (GdaServerConnection *cnc,
                                  GdaError *error,
                                  GDA_Connection_QType t,
                                  GDA_Connection_Constraint *constraints,
                                  gint length)
{
	schema_ops_fn fn;
	
	g_return_val_if_fail(cnc != NULL, NULL);
	
	fn = schema_ops[(gint) t];
	if (fn)
		return fn(error, cnc, constraints, length);
	else gda_log_error(_("Unhandled SCHEMA_QTYPE %d"), (gint) t);
	return NULL;
}

glong
gda_mysql_connection_modify_schema (GdaServerConnection *cnc,
                                    GDA_Connection_QType t,
                                    GDA_Connection_Constraint *constraints,
                                    gint length)
{
	return -1;
}

gint
gda_mysql_connection_start_logging (GdaServerConnection *cnc, const gchar *filename)
{
	return -1;
}

gint
gda_mysql_connection_stop_logging (GdaServerConnection *cnc)
{
	return -1;
}

gchar *
gda_mysql_connection_create_table (GdaServerConnection *cnc,
                                   GDA_RowAttributes *columns)
{
  return NULL;
}

gboolean
gda_mysql_connection_supports (GdaServerConnection *cnc,
                               GDA_Connection_Feature feature)
{
	g_return_val_if_fail(cnc != NULL, FALSE);
	
	switch (feature) {
		case GDA_Connection_FEATURE_SQL :
		case GDA_Connection_FEATURE_XML_QUERIES :
			return TRUE;
		default :
			return FALSE;
	}

	return FALSE; /* not supported or know nothing about it */
}

gulong
gda_mysql_connection_get_sql_type(MYSQL_Connection *cnc, 
				  gchar *mysql_type)
{
	gulong oid = 0; /* default return value */
	gboolean found=FALSE;
	gint i=0;

	g_return_val_if_fail((cnc != NULL), 0);
	g_return_val_if_fail((cnc->types_array != NULL), 0);

	while ((i< MYSQL_Types_Array_Nb-1) && !found) {
		if (!strcmp(cnc->types_array[i].mysql_type, mysql_type)) {
			oid = cnc->types_array[i].oid;
			found = TRUE;
		}
		i++;
	}

	return oid;
}


GDA_ValueType
gda_mysql_connection_get_gda_type_mysql(MYSQL_Connection *mycnc, gulong sql_type)
{
        GDA_ValueType gda_type = GDA_TypeVarchar; /* default value */
	gboolean found=FALSE;
	gint i=0;

	g_return_val_if_fail((mycnc != NULL), GDA_TypeNull);
	g_return_val_if_fail((mycnc->types_array != NULL), GDA_TypeNull);
	
	while ((i< MYSQL_Types_Array_Nb-1) && !found) {
	        if (mycnc->types_array[i].oid == sql_type) {
	                gda_type = mycnc->types_array[i].gda_type;
			found = TRUE;
		}
		i++;
	}
	return gda_type;
}

gchar *
gda_mysql_connection_get_charsql_type_mysql(MYSQL_Connection *mycnc, gulong sql_type)
{
        gchar *retval="varchar";
	gboolean found=FALSE;
	gint i=0;

	g_return_val_if_fail((mycnc != NULL), GDA_TypeNull);
	g_return_val_if_fail((mycnc->types_array != NULL), GDA_TypeNull);
	
	while ((i< MYSQL_Types_Array_Nb-1) && !found) {
	        if (mycnc->types_array[i].oid == sql_type) {
	                retval = mycnc->types_array[i].mysql_type;
			found = TRUE;
		}
		i++;
	}
	return retval;
}


GDA_ValueType
gda_mysql_connection_get_gda_type (GdaServerConnection *cnc, gulong sql_type)
{
        MYSQL_Connection *mycnc;
        g_return_val_if_fail(cnc != NULL, GDA_TypeNull);

	mycnc = (MYSQL_Connection *) gda_server_connection_get_user_data(cnc);
	return gda_mysql_connection_get_gda_type_mysql(mycnc, sql_type);
}

gshort
gda_mysql_connection_get_c_type_mysql (MYSQL_Connection *mycnc, 
				       GDA_ValueType gda_type)
{
	MYSQL_CType c_type = SQL_C_CHAR; /* default value */
	gboolean found=FALSE;
	gint i=0;

	g_return_val_if_fail(mycnc != NULL, SQL_C_CHAR);
	g_return_val_if_fail((mycnc->types_array != NULL), SQL_C_CHAR);
	while ((i< MYSQL_Types_Array_Nb-1) && !found) {
	        if (mycnc->types_array[i].gda_type == gda_type) {
			c_type = mycnc->types_array[i].c_type;
			found = TRUE;
		}
		i++;
	}

	return c_type;
}

gshort
gda_mysql_connection_get_c_type (GdaServerConnection *cnc, GDA_ValueType type)
{
        MYSQL_Connection *mycnc;

	g_return_val_if_fail(mycnc != NULL, -1);
	mycnc = (MYSQL_Connection *) gda_server_connection_get_user_data(cnc);
	return gda_mysql_connection_get_c_type_mysql(mycnc, type);
}

gchar *
gda_mysql_connection_sql2xml (GdaServerConnection *cnc, const gchar *sql)
{
	return NULL;
}

gchar *
gda_mysql_connection_xml2sql (GdaServerConnection *cnc, const gchar *xml)
{
	return NULL;
}

void
gda_mysql_connection_free (GdaServerConnection *cnc)
{
	MYSQL_Connection* mysql_cnc;
	
	g_return_if_fail(cnc != NULL);
	gda_mysql_connection_close(cnc);

	mysql_cnc = (MYSQL_Connection *) gda_server_connection_get_user_data(cnc);
	if (mysql_cnc) {
		g_free((gpointer) mysql_cnc);
		gda_server_connection_set_user_data(cnc, NULL);
	}
}

void
gda_mysql_error_make (GdaError *error,
		      GdaServerRecordset *recset,
		      GdaServerConnection *cnc,
		      gchar *where)
{
	MYSQL_Connection* mysql_cnc;
	
	g_return_if_fail(cnc != NULL);
	
	mysql_cnc = (MYSQL_Connection *) gda_server_connection_get_user_data(cnc);
	if (mysql_cnc) {
		gda_error_set_description(error, mysql_error(mysql_cnc->mysql));
		gda_log_error(_("error '%s' at %s"), gda_error_get_description(error), where);
		
		gda_error_set_number(error, mysql_errno(mysql_cnc->mysql));
		gda_error_set_source(error, "[gda-mysql]");
		gda_error_set_help_url(error, _("Not available"));
		gda_error_set_help_context(error, _("Not available"));
		gda_error_set_sqlstate(error, _("error"));
		gda_error_set_native(error, gda_error_get_description(error));
	}
}

/*
 * Schema functions
 */
static GdaServerRecordset *
schema_tables (GdaError *error,
               GdaServerConnection *cnc,
               GDA_Connection_Constraint *constraints,
               gint length)
{
	GdaServerRecordset* recset;
	MYSQL_Connection*    mycnc;
	
	mycnc = (MYSQL_Connection *) gda_server_connection_get_user_data(cnc);
	if (mycnc) {
	        gboolean                   extra_info = FALSE;
		gchar*                     table_name = NULL;
		GDA_Connection_Constraint* ptr = NULL;
		gint                       cnt;
		gchar*                     query;
		gint                       rc;
		gulong                     affected = 0;

		/* process constraints */
		ptr = constraints;
		for (cnt = 0; cnt < length && ptr != NULL; cnt++) {
			switch (ptr->ctype) {
				case GDA_Connection_OBJECT_NAME :
					table_name = ptr->value;
					break;
				case GDA_Connection_EXTRA_INFO :
				        extra_info = TRUE;
					break;
			        case GDA_Connection_OBJECT_SCHEMA :
			        case GDA_Connection_OBJECT_CATALOG :
				        /* N/A */
				        break;
				default :
			}
		}
		
		/* get list of tables from MySQL */
		if (table_name)
		  query = g_strdup_printf("SHOW TABLE STATUS LIKE '%s'", table_name);
		else
		  query = g_strdup("SHOW TABLE STATUS");
	
		rc = mysql_real_query(mycnc->mysql, query, strlen(query));
                g_free(query);
                if (rc) {
                        gda_server_error_make(error, NULL, cnc, __PRETTY_FUNCTION__);
                }
                else {
		        MYSQL_RES *res;

                        res = mysql_store_result(mycnc->mysql);
                        if (!res) {
                                gda_server_error_make(error, NULL, cnc, __PRETTY_FUNCTION__);
			}
                        else {
			        MYSQL_ROW                 myrow;
				GdaBuiltin_Result*        bres;
				gchar*                    row[4];

				if (extra_info) {
				        bres = GdaBuiltin_Result_new(4, "result", FIELD_TYPE_STRING, -1);
					GdaBuiltin_Result_set_att(bres, 0, "Name",
                                                          gda_mysql_connection_get_sql_type(mycnc, "varchar"),
                                                          -1);
					GdaBuiltin_Result_set_att(bres, 1, "Owner",
                                                          gda_mysql_connection_get_sql_type(mycnc, "varchar"),
                                                          -1);
					GdaBuiltin_Result_set_att(bres, 2, "Comments",
                                                          gda_mysql_connection_get_sql_type(mycnc, "varchar"),
                                                          -1);
					GdaBuiltin_Result_set_att(bres, 3, "SQL",
                                                          gda_mysql_connection_get_sql_type(mycnc, "varchar"),
                                                          -1);
				}
				else {
				        bres = GdaBuiltin_Result_new(2, "result", FIELD_TYPE_STRING, -1);
					GdaBuiltin_Result_set_att(bres, 0, "Name",
                                                          gda_mysql_connection_get_sql_type(mycnc, "varchar"),
                                                          -1);
					GdaBuiltin_Result_set_att(bres, 1, "Comments",
                                                          gda_mysql_connection_get_sql_type(mycnc, "varchar"),
                                                          -1);
				}
				while ((myrow = mysql_fetch_row(res))) {
				        row[0] = myrow[0];
					if (extra_info) {
					        row[1] = _("Unknown");
					        row[2] = myrow[14];
						/* TODO: if we really want the SQL for the table, 
						   use a query like "SHOW CREATE TABLE <table>" 
						   for now we leave it as "" 
						*/
					        row[3] = ""; 
					}
					else {
					        row[1] = myrow[14];
					}
					GdaBuiltin_Result_add_row(bres, row);
				}
				if(!mysql_eof(res)) { /* mysql_fetch_row() failed due to an error */
				        mysql_free_result(res);
				        gda_server_error_make(error, NULL, cnc, __PRETTY_FUNCTION__);
				}
				else {
				        mysql_free_result(res);
					recset = gda_mysql_command_build_recset_with_builtin(cnc,
											     bres,
											     &affected);
					fprintf(stderr, "Nb of tables: %ld\n", affected);
					return (recset);
				}
			}
		}
	}
	else 
	        gda_error_set_description(error, _("No MYSQL_Connection for GdaServerConnection"));
	return NULL;
}

static GdaServerRecordset *
schema_columns (GdaError *error,
                GdaServerConnection *cnc,
                GDA_Connection_Constraint *constraints,
                gint length)
{
	GdaServerCommand*         cmd;
	GdaServerRecordset*       recset;
	gulong                    affected = 0;
	gchar*                     query;
	gchar*                     table_name = NULL;
	GDA_Connection_Constraint* ptr;
	gint                       cnt;
	MYSQL_Connection *         mycnc;

	mycnc = (MYSQL_Connection *) gda_server_connection_get_user_data(cnc);
	
	/* process constraints */
	ptr = constraints;
	for (cnt = 0; cnt < length && ptr != NULL; cnt++) {
		switch (ptr->ctype) {
			case GDA_Connection_OBJECT_NAME :
				table_name = ptr->value;
				break;
			case GDA_Connection_COLUMN_NAME :
			        break;
			default :
		}
	}
	
	/* build command object and query */
	if (table_name) {
	        gint rc;
		gchar *query;
		
		query = g_strdup_printf("SELECT * from %s LIMIT 0", table_name);
		rc = mysql_real_query(mycnc->mysql, query, strlen(query));
		g_free(query);
		if (rc) {
		        gda_server_error_make(error, NULL, cnc, __PRETTY_FUNCTION__);
		}
		else {
		        MYSQL_RES *res;
			
			res = mysql_store_result(mycnc->mysql);
			if (!res)
			        gda_server_error_make(error, NULL, cnc, __PRETTY_FUNCTION__);
			else {
			        GdaBuiltin_Result*        bres;
				MYSQL_FIELD*              field;
				gchar*                    row[8];

				bres = GdaBuiltin_Result_new(8, "result", FIELD_TYPE_STRING, -1);
				GdaBuiltin_Result_set_att(bres, 0, "Name",
							  gda_mysql_connection_get_sql_type(mycnc, "varchar"),
							  -1);
				GdaBuiltin_Result_set_att(bres, 1, "Type",
							  gda_mysql_connection_get_sql_type(mycnc, "varchar"),
							  -1);
				GdaBuiltin_Result_set_att(bres, 2, "Size",
							  gda_mysql_connection_get_sql_type(mycnc, "integer"),
							  -1);
				GdaBuiltin_Result_set_att(bres, 3, "Precision",
							  gda_mysql_connection_get_sql_type(mycnc, "integer"),
							  -1);
				GdaBuiltin_Result_set_att(bres, 4, "Nullable",
							  FIELD_TYPE_BOOL,
							  -1);
				GdaBuiltin_Result_set_att(bres, 5, "Key",
							  FIELD_TYPE_BOOL,
							  -1);
				GdaBuiltin_Result_set_att(bres, 6, "Default Value",
							  gda_mysql_connection_get_sql_type(mycnc, "varchar"),
							  -1);
				GdaBuiltin_Result_set_att(bres, 7, "Comments",
							  gda_mysql_connection_get_sql_type(mycnc, "varchar"),
							  -1);
				while((field = mysql_fetch_field(res))) {
				        MYSQL_RES *res2;

				        row[0] = field->name;
					row[1] = gda_mysql_connection_get_charsql_type_mysql(mycnc, field->type);
					row[2] = g_strdup_printf("%d", field->length);
					row[3] = g_strdup_printf("%d", field->decimals);
					row[6] = NULL;
					row[7] = NULL;
					
					/* execution of a "show columns from columns_priv like '..'" query
					   to get default values and nullable values */
					query = g_strdup_printf("show columns from %s like '%s'", table_name,
								field->name);
					rc = mysql_real_query(mycnc->mysql, query, strlen(query));
					g_free(query);
					if (rc) {
					        gda_server_error_make(error, NULL, cnc, __PRETTY_FUNCTION__);
					}
					else {
					        MYSQL_ROW myrow;

					        res2 = mysql_store_result(mycnc->mysql);
						myrow = mysql_fetch_row(res2);
						if (!myrow) {
						        gda_server_error_make(error, NULL, cnc, __PRETTY_FUNCTION__);
						}
						else {
						  row[4] = (myrow[2] && *(myrow[2])) ? "t" : "f";
						  row[5] = (myrow[3] && *(myrow[3])) ? "t" : "f";
						  if (myrow[4])
						          row[6] = g_strdup(myrow[4]);
						  else
						          row[6] = NULL;
						  if (myrow[1]) { /* fix some SET and ENUM problems */
						          row[7] = g_strdup(myrow[1]);
							  if (strstr(myrow[1], "enum"))
							    row[1] = gda_mysql_connection_get_charsql_type_mysql(mycnc, FIELD_TYPE_ENUM);
							  if (strstr(myrow[1], "set"))
							    row[1] = gda_mysql_connection_get_charsql_type_mysql(mycnc, FIELD_TYPE_SET);
						  }
						  GdaBuiltin_Result_add_row(bres, row);

						  if (row[6])
						          g_free(row[6]);
						  if (row[7])
						          g_free(row[7]);
						}
						mysql_free_result(res2);
					}
					g_free(row[2]);
					g_free(row[3]);
				}
				mysql_free_result(res);
				recset = gda_mysql_command_build_recset_with_builtin(cnc,
										     bres,
										     &affected);
				fprintf(stderr, "Nb of cols: %ld\n", affected);
				return (recset);
			}
		}
	}
	else gda_error_set_description(error, _("No table name given for SCHEMA_COLS"));
	return NULL;
}

static GdaServerRecordset *schema_types (GdaError *error,
					 GdaServerConnection *cnc,
					 GDA_Connection_Constraint *constraint,
					 gint length)
{
        GdaServerRecordset*       recset = 0;
	gulong                    affected = 0;
        GdaBuiltin_Result*        bres;
	gint                       cnt;
	GDA_Connection_Constraint* ptr;
        MYSQL_Connection *         mycnc;
	gboolean                   extra_info=FALSE;
	gchar*                     typename=NULL;

	mycnc = (MYSQL_Connection *) gda_server_connection_get_user_data(cnc);

        ptr = constraint;
        for (cnt = 0; cnt < length && ptr != 0; cnt++) {
	       switch (ptr->ctype) {
		case GDA_Connection_OBJECT_NAME :
		        typename = ptr->value;
                        break;
		case GDA_Connection_OBJECT_SCHEMA :
		        /* N/A */
                        break;
		case GDA_Connection_OBJECT_CATALOG :
		        /* N/A */
                        break;
		case GDA_Connection_EXTRA_INFO :
		extra_info=TRUE;
                        break;
		default :
                        fprintf(stderr, "schema_types: invalid constraint type %d\n",
				ptr->ctype);
                        return (0);
		}
                ptr++;
	}

	if (!extra_info) {
    	        gchar *row[2];
		gint i;

	        bres = GdaBuiltin_Result_new(2, "result", 0, -1);
		GdaBuiltin_Result_set_att(bres, 0, "Name",
					  gda_mysql_connection_get_sql_type(mycnc, "varchar"),
					  -1);
		GdaBuiltin_Result_set_att(bres, 1, "Comments",
					  gda_mysql_connection_get_sql_type(mycnc, "varchar"),
					  -1);

		for (i=0; i<MYSQL_Types_Array_Nb-1; i++) {
		        if (!typename || 
			    (typename && !strcmp(mycnc->types_array[i].mysql_type, typename))) {
		                row[0] = mycnc->types_array[i].mysql_type;
				row[1] = mycnc->types_array[i].description;
				GdaBuiltin_Result_add_row(bres, row);
			}
		}
	} else {
    	        gchar *row[5];
		gint i;

	        bres = GdaBuiltin_Result_new(5, "result", 0, -1);
		GdaBuiltin_Result_set_att(bres, 0, "Name",
					  gda_mysql_connection_get_sql_type(mycnc, "varchar"),
					  -1);
		GdaBuiltin_Result_set_att(bres, 1, "Owner",
					  gda_mysql_connection_get_sql_type(mycnc, "varchar"),
					  -1);
		GdaBuiltin_Result_set_att(bres, 2, "Comments",
					  gda_mysql_connection_get_sql_type(mycnc, "varchar"),
					  -1);
		GdaBuiltin_Result_set_att(bres, 3, "Gda type",
					  gda_mysql_connection_get_sql_type(mycnc, "smallint"),
					  -1);
		GdaBuiltin_Result_set_att(bres, 4, "Provider type",
					  gda_mysql_connection_get_sql_type(mycnc, "integer"),
					  -1);
		for (i=0; i<MYSQL_Types_Array_Nb-1; i++) {
		        if (!typename || 
			    (typename && !strcmp(mycnc->types_array[i].mysql_type, typename))) {
			        row[0] = mycnc->types_array[i].mysql_type;
				row[1] = mycnc->types_array[i].owner;
				row[2] = mycnc->types_array[i].description;
				row[3] = g_strdup_printf("%d", mycnc->types_array[i].gda_type);
				row[4] = g_strdup_printf("%d", mycnc->types_array[i].oid);
				GdaBuiltin_Result_add_row(bres, row);
				g_free(row[3]);
				g_free(row[4]);
			}
		}
	}

	recset = gda_mysql_command_build_recset_with_builtin(cnc,
							     bres,  
							     &affected);
	fprintf(stderr, "Nb of proc params: %ld\n", affected);
	return (recset);
}
