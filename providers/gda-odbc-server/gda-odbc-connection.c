/* GNOME DB ODBC Provider
 * Copyright (C) 2000 Nick Gorham
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

#include "gda-odbc.h"
#include <gda.h>
#include <ctype.h>

typedef Gda_ServerRecordset* (*schema_ops_fn)(Gda_ServerRecordset*,
					     Gda_ServerConnection*,
					     GDA_Connection_Constraint* constraints,
					     gint length);

/*
 * schema opts prototypes
 */

static Gda_ServerRecordset*  schema_tables(Gda_ServerRecordset*  recset,
					  Gda_ServerConnection* cnc,
					  GDA_Connection_Constraint* constraints,
					  gint length);

static Gda_ServerRecordset*  schema_columns(Gda_ServerRecordset*  recset,
					  Gda_ServerConnection* cnc,
					  GDA_Connection_Constraint* constraints,
					  gint length);

static Gda_ServerRecordset*  schema_types(Gda_ServerRecordset*   recset,
					 Gda_ServerConnection*  cnc,
					 GDA_Connection_Constraint*  constraint,
					 gint length);

static Gda_ServerRecordset*  schema_views(Gda_ServerRecordset* recset,
					 Gda_ServerConnection* cnc,
					 GDA_Connection_Constraint* constraint,
					 gint length);

static Gda_ServerRecordset*  schema_indexes(Gda_ServerRecordset* recset,
					 Gda_ServerConnection* cnc,
					 GDA_Connection_Constraint* constraint,
					 gint length);

static Gda_ServerRecordset*  schema_procedures(Gda_ServerRecordset* recset,
					 Gda_ServerConnection* cnc,
					 GDA_Connection_Constraint* constraint,
					 gint length);

schema_ops_fn schema_ops[GDA_Connection_GDCN_SCHEMA_LAST] =
{
  0,
};

static void
initialize_schema_ops(void)
{
  schema_ops[GDA_Connection_GDCN_SCHEMA_TABLES] = schema_tables;
  schema_ops[GDA_Connection_GDCN_SCHEMA_COLS]   = schema_columns;
  schema_ops[GDA_Connection_GDCN_SCHEMA_PROV_TYPES] = schema_types;
  schema_ops[GDA_Connection_GDCN_SCHEMA_VIEWS] = schema_views;
  schema_ops[GDA_Connection_GDCN_SCHEMA_INDEXES] = schema_indexes;
  schema_ops[GDA_Connection_GDCN_SCHEMA_PROCS] = schema_procedures;
}

gboolean
gda_odbc_connection_new (Gda_ServerConnection *cnc)
{
  ODBC_Connection* od_cnc;

  g_return_val_if_fail(cnc != NULL, FALSE);

  od_cnc = g_new0(ODBC_Connection, 1);
  od_cnc->connected = 0;

  initialize_schema_ops();

  gda_server_connection_set_user_data(cnc, (gpointer) od_cnc);
  return TRUE;
}

gint
gda_odbc_connection_open (Gda_ServerConnection *cnc,
			  const gchar *dsn,
			  const gchar *user,
			  const gchar *password)
{
  ODBC_Connection* od_cnc;
  Gda_ServerError* error;
  SQLRETURN rc;

  g_return_val_if_fail(cnc != NULL, -1);

  od_cnc = (ODBC_Connection *) gda_server_connection_get_user_data(cnc);

  if (od_cnc)
    {
      rc = SQLAllocEnv(&od_cnc->henv);
      if (!SQL_SUCCEEDED( rc ))
      {
	    gda_server_error_make(gda_server_error_new(), NULL, cnc, __PRETTY_FUNCTION__);
        return -1;
      }

      rc = SQLAllocConnect(od_cnc->henv, &od_cnc->hdbc);
      if (!SQL_SUCCEEDED( rc ))
      {
	    gda_server_error_make(gda_server_error_new(), NULL, cnc, __PRETTY_FUNCTION__);
        SQLFreeEnv(od_cnc->henv);
        return -1;
      }

      g_warning("gda_odbc_connection_open: dsn = '%s'", dsn);
      g_warning("gda_odbc_connection_open: user = '%s'", user);
      g_warning("gda_odbc_connection_open: passwd = '%s'", password);

      rc = SQLConnect(od_cnc->hdbc, 
              (SQLCHAR*)dsn, SQL_NTS, 
              (SQLCHAR*)user, SQL_NTS,
		      (SQLCHAR*)password, SQL_NTS);

      if (!SQL_SUCCEEDED( rc ))
      {
        error = gda_server_error_new();
        gda_server_error_make(error, 0, cnc, __PRETTY_FUNCTION__);

        SQLFreeConnect(od_cnc->hdbc);
	    SQLFreeEnv(od_cnc->henv);
        return -1;
      }

      od_cnc->connected = 0;
      return 0;
    }

  return -1;
}

void
gda_odbc_connection_close (Gda_ServerConnection *cnc)
{
  ODBC_Connection* od_cnc;

  g_return_if_fail(cnc != NULL);

  od_cnc = (ODBC_Connection *) gda_server_connection_get_user_data(cnc);

  g_return_if_fail(od_cnc != NULL);

  if ( od_cnc->connected )
  {
      SQLDisconnect(od_cnc->hdbc);
  }
  SQLFreeConnect(od_cnc->hdbc);
  SQLFreeEnv(od_cnc->henv);
}

gint
gda_odbc_connection_begin_transaction (Gda_ServerConnection *cnc)
{
  ODBC_Connection* od_cnc;

  g_return_val_if_fail(cnc != NULL, -1);

  od_cnc = (ODBC_Connection *) gda_server_connection_get_user_data(cnc);

  if (od_cnc)
  {
    SQLRETURN rc;

    rc = SQLSetConnectAttr( od_cnc->hdbc, SQL_ATTR_AUTOCOMMIT, SQL_AUTOCOMMIT_OFF, 0 );
    if ( SQL_SUCCEEDED( rc ))
    {
        return 0;
    }
    else
    {
        gda_server_error_make(gda_server_error_new(), NULL, cnc, __PRETTY_FUNCTION__);
        return -1;
    }
  }
      
  return -1;
}

gint
gda_odbc_connection_commit_transaction (Gda_ServerConnection *cnc)
{
  ODBC_Connection* od_cnc;

  g_return_val_if_fail(cnc != NULL, -1);

  od_cnc = (ODBC_Connection *) gda_server_connection_get_user_data(cnc);

  if (od_cnc)
  {
    SQLRETURN rc;

    rc = SQLTransact( od_cnc->henv, od_cnc->hdbc, SQL_COMMIT );
    if ( SQL_SUCCEEDED( rc ))
    {
        return 0;
    }
    else
    {
        gda_server_error_make(gda_server_error_new(), NULL, cnc, __PRETTY_FUNCTION__);
        return -1;
    }
  }
      
  return -1;
}

gint
gda_odbc_connection_rollback_transaction (Gda_ServerConnection *cnc)
{
  ODBC_Connection* od_cnc;

  g_return_val_if_fail(cnc != NULL, -1);

  od_cnc = (ODBC_Connection *) gda_server_connection_get_user_data(cnc);

  if (od_cnc)
  {
    SQLRETURN rc;

    rc = SQLTransact( od_cnc->henv, od_cnc->hdbc, SQL_COMMIT );
    if ( SQL_SUCCEEDED( rc ))
    {
        return 0;
    }
    else
    {
        gda_server_error_make(gda_server_error_new(), NULL, cnc, __PRETTY_FUNCTION__);
        return -1;
    }
  }
      
  return -1;
}

Gda_ServerRecordset *
gda_odbc_connection_open_schema (Gda_ServerConnection *cnc,
				 Gda_ServerError *error,
				 GDA_Connection_QType t,
				 GDA_Connection_Constraint *constraints,
				 gint length)
{
  Gda_ServerRecordset* rs;
  schema_ops_fn fn = schema_ops[t];

  g_return_val_if_fail(cnc != NULL, NULL);

  rs = gda_server_recordset_new(cnc);

  g_return_val_if_fail(rs != NULL, NULL);

  if (fn)
    {
      return fn(rs, cnc, constraints, length);
    }
  else
    {
      gda_log_error( _("gda_odbc_connection_open_schema: Unhandled SCHEMA_QTYPE %d\n"), t);
    }

  return NULL;
}

gint
gda_odbc_connection_start_logging (Gda_ServerConnection *cnc,
				   const gchar *filename)
{
  ODBC_Connection* od_cnc;

  g_return_val_if_fail(cnc != NULL, -1);

  od_cnc = (ODBC_Connection *) gda_server_connection_get_user_data(cnc);

  if (od_cnc)
  {
    SQLRETURN rc;

    rc = SQLSetConnectAttr( od_cnc->hdbc, SQL_ATTR_TRACE, (SQLPOINTER)SQL_OPT_TRACE_ON, 0 );
    if ( SQL_SUCCEEDED( rc ))
    {
        return 0;
    }
    else
    {
        gda_server_error_make(gda_server_error_new(), NULL, cnc, __PRETTY_FUNCTION__);
        return -1;
    }
  }
      
  return -1;
}

gint
gda_odbc_connection_stop_logging (Gda_ServerConnection *cnc)
{
  ODBC_Connection* od_cnc;

  g_return_val_if_fail(cnc != NULL, -1);

  od_cnc = (ODBC_Connection *) gda_server_connection_get_user_data(cnc);

  if (od_cnc)
  {
    SQLRETURN rc;

    rc = SQLSetConnectAttr( od_cnc->hdbc, SQL_ATTR_TRACE, SQL_OPT_TRACE_OFF, 0 );
    if ( SQL_SUCCEEDED( rc ))
    {
        return 0;
    }
    else
    {
        gda_server_error_make(gda_server_error_new(), NULL, cnc, __PRETTY_FUNCTION__);
        return -1;
    }
  }
      
  return -1;
}

gchar *
gda_odbc_connection_create_table (Gda_ServerConnection *cnc,
				       GDA_RowAttributes *columns)
{
  g_return_val_if_fail(cnc != NULL, NULL);
  return NULL;
}

gboolean
gda_odbc_connection_supports (Gda_ServerConnection *cnc,
				   GDA_Connection_Feature feature)
{
  g_return_val_if_fail(cnc != NULL, FALSE);

  if (feature == GDA_Connection_FEATURE_TRANSACTIONS) 
  {
    ODBC_Connection* od_cnc;

    od_cnc = (ODBC_Connection *) gda_server_connection_get_user_data(cnc);
    if (od_cnc)
    {
      SQLRETURN rc;
      SQLUSMALLINT val;
      
      rc = SQLGetInfo( od_cnc -> hdbc, SQL_TXN_CAPABLE, &val, sizeof( val ), NULL );
      if ( SQL_SUCCEEDED( rc ))
      {
          if ( val == SQL_TC_NONE )
              return FALSE;
          else
              return TRUE;
      }
      else gda_server_error_make(gda_server_error_new(), NULL, cnc, __PRETTY_FUNCTION__);
    }
  }
  else if (feature == GDA_Connection_FEATURE_PROCS) 
  {
    ODBC_Connection* od_cnc;

    od_cnc = (ODBC_Connection *) gda_server_connection_get_user_data(cnc);
    if (od_cnc)
    {
      SQLRETURN rc;
      char sval[ 2 ];
      
      rc = SQLGetInfo( od_cnc -> hdbc, SQL_PROCEDURES, sval, sizeof( sval ), NULL );
      if ( SQL_SUCCEEDED( rc ))
      {
          if ( strcmp( sval, "Y" ) == 0 )
              return TRUE;
          else
              return FALSE;
      }
      else gda_server_error_make(gda_server_error_new(), NULL, cnc, __PRETTY_FUNCTION__);
    }
  }
  return FALSE; /* not supported or know nothing about it */
}

GDA_ValueType
gda_odbc_connection_get_gda_type (Gda_ServerConnection *cnc, gulong sql_type)
{
  g_return_val_if_fail(cnc != NULL, GDA_TypeNull);

  switch (sql_type)
  {
    case SQL_CHAR:
      return GDA_TypeVarchar;

    case SQL_VARCHAR:
      return GDA_TypeVarchar;

    case SQL_LONGVARCHAR:
      return GDA_TypeLongvarchar;

    case SQL_BINARY:
      return GDA_TypeBinary;

    case SQL_VARBINARY:
      return GDA_TypeVarbin;

    case SQL_LONGVARBINARY:
      return GDA_TypeLongvarbin;

    case SQL_DECIMAL:
      return GDA_TypeVarchar;

    case SQL_NUMERIC:
      return GDA_TypeVarchar;

    case SQL_BIT:
      return GDA_TypeBoolean;

    case SQL_TINYINT:
      return GDA_TypeTinyint;

    case SQL_SMALLINT:
      return GDA_TypeSmallint;

    case SQL_INTEGER:
      return GDA_TypeInteger;

    case SQL_BIGINT:
      return GDA_TypeBigint;

    case SQL_REAL:
      return GDA_TypeDecimal;

    case SQL_FLOAT:
    case SQL_DOUBLE:
      return GDA_TypeDouble;

    case SQL_TYPE_DATE:
    case SQL_DATE:
      return GDA_TypeDbDate;

    case SQL_TYPE_TIME:
    case SQL_TIME:
      return GDA_TypeDbTime;

    case SQL_TYPE_TIMESTAMP:
    case SQL_TIMESTAMP:
      return GDA_TypeDbTimestamp;
  }

  return GDA_TypeNull;
}

gshort
gda_odbc_connection_get_c_type (Gda_ServerConnection *cnc, GDA_ValueType type)
{
  g_return_val_if_fail(cnc != NULL, -1);

  switch (type)
  {
    case  GDA_TypeFixchar:
        return SQL_C_CHAR;

    case GDA_TypeVarchar:
        return SQL_C_CHAR;

    case GDA_TypeLongvarchar:
        return SQL_C_CHAR;

    case GDA_TypeBinary:
        return SQL_C_BINARY;

    case GDA_TypeVarbin:
        return SQL_C_BINARY;

    case GDA_TypeLongvarbin:
        return SQL_C_BINARY;

    case GDA_TypeDecimal:
        return SQL_CHAR;

    case GDA_TypeNumeric:
        return SQL_CHAR;

    case GDA_TypeBoolean:
      return SQL_C_BIT;

    case GDA_TypeTinyint:
      return SQL_C_LONG;

    case GDA_TypeSmallint:
      return SQL_C_SHORT;

    case GDA_TypeInteger:
      return SQL_C_LONG;

    case GDA_TypeBigint:
      return SQL_C_LONG;

    case  GDA_TypeDouble:
      return SQL_C_DOUBLE;

    case GDA_TypeDbDate:
      return SQL_C_TYPE_DATE;

    case GDA_TypeDbTime:
      return SQL_C_TYPE_TIME;

    case GDA_TypeDbTimestamp:
      return SQL_C_TYPE_TIMESTAMP;

    default:
      return SQL_C_CHAR;
  }
  return -1;
}

void
gda_odbc_connection_free (Gda_ServerConnection *cnc)
{
  ODBC_Connection* od_cnc;

  g_return_if_fail(cnc != NULL);

  od_cnc = (ODBC_Connection *) gda_server_connection_get_user_data(cnc);
  if (od_cnc)
    {
      g_free((gpointer) od_cnc);
    }
}

void
gda_odbc_error_make (Gda_ServerError *error,
		     Gda_ServerRecordset *recset,
		     Gda_ServerConnection *cnc,
		     gchar *where)
{
  ODBC_Connection*  od_cnc;
  SQLRETURN         rc;

  od_cnc = (ODBC_Connection *) gda_server_connection_get_user_data(cnc);
  if (od_cnc)
    {
      if ( recset )
        {
          gchar err_msg[ 1024 ];
          SQLINTEGER native;
          SQLSMALLINT len;
          gchar sqlstate[ 6 ];
          ODBC_Recordset* odbc_recset;

          odbc_recset = (ODBC_Recordset *) gda_server_recordset_get_user_data(recset);

          rc = SQLError( od_cnc->henv, od_cnc->hdbc, odbc_recset->hstmt,
                  sqlstate, &native, err_msg, sizeof( err_msg ), &len );

          gda_log_error(_("error '%s' at %s"), err_msg, where);
          gda_server_error_set_source(error, "[gda-odbc]");
          gda_server_error_set_help_file(error, _("Not available"));
          gda_server_error_set_help_context(error, _("Not available"));
          if ( SQL_SUCCEEDED( rc ))
          {
            char sznative[ 20 ];
            
            gda_server_error_set_description(error, err_msg);
            gda_server_error_set_sqlstate(error, sqlstate);

            sprintf( sznative, "%d", (int) native );
            gda_server_error_set_native(error, sznative);
            gda_server_error_set_number(error, native);
          }
          else
          {
            gda_server_error_set_description(error, _("no text"));
            gda_server_error_set_sqlstate(error, _("no text"));
            gda_server_error_set_native(error, _("no text"));
            gda_server_error_set_number(error, 0);
          }
        }
      else
        {
          gchar err_msg[ 1024 ];
          SQLINTEGER native;
          SQLSMALLINT len;
          gchar sqlstate[ 6 ];

          rc = SQLError( od_cnc->henv, od_cnc->hdbc, SQL_NULL_HSTMT,
                  sqlstate, &native, err_msg, sizeof( err_msg ), &len );

          gda_log_error(_("error '%s' at %s"), err_msg, where);
          gda_server_error_set_source(error, "[gda-odbc]");
          gda_server_error_set_help_file(error, _("Not available"));
          gda_server_error_set_help_context(error, _("Not available"));
          if ( SQL_SUCCEEDED( rc ))
          {
            char sznative[ 20 ];
            
            gda_server_error_set_description(error, err_msg);
            gda_server_error_set_sqlstate(error, sqlstate);

            sprintf( sznative, "%d", (int) native );
            gda_server_error_set_native(error, sznative);
            gda_server_error_set_number(error, native);
          }
          else
          {
            gda_server_error_set_description(error, _("no text"));
            gda_server_error_set_sqlstate(error, _("no text"));
            gda_server_error_set_native(error, _("no text"));
            gda_server_error_set_number(error, 0);
          }
        }
    }
}

static Gda_ServerRecordset*  schema_tables(Gda_ServerRecordset*  recset,
					  Gda_ServerConnection* cnc,
					  GDA_Connection_Constraint* constraints,
					  gint length)
{
  ODBC_Recordset*   odbc_recset;
  ODBC_Connection*  od_cnc;
  SQLRETURN         rc;
  Gda_ServerError*  error;
  gchar*            table_qualifier = NULL;
  gchar*            table_owner     = NULL;
  gchar*            table_name      = NULL;
  gchar*            table_type      = "TABLE";

  g_return_val_if_fail( recset != NULL, NULL );
  g_return_val_if_fail( cnc != NULL, NULL );

  odbc_recset = (ODBC_Recordset *) gda_server_recordset_get_user_data(recset);
  g_return_val_if_fail( odbc_recset != NULL, NULL );

  od_cnc = (ODBC_Connection *) gda_server_connection_get_user_data(cnc);
  g_return_val_if_fail( od_cnc != NULL, NULL );

  rc = SQLAllocStmt( od_cnc->hdbc, &odbc_recset->hstmt );
  if( !SQL_SUCCEEDED( rc ))
  {
    error = gda_server_error_new();
    gda_server_error_make(error, recset, cnc, __PRETTY_FUNCTION__);
    gda_server_recordset_free(recset);

    return NULL;
  }

  odbc_recset->allocated = 1;
  odbc_recset->mapped_cols = 3;
  odbc_recset->map[ 0 ] = 2;
  odbc_recset->map[ 1 ] = 1;
  odbc_recset->map[ 2 ] = 4;

  while(length)
  {
    switch (constraints->ctype)
	{
	case GDA_Connection_OBJECT_CATALOG:
	  table_qualifier = constraints->value;
	  break;
	case GDA_Connection_OBJECT_SCHEMA:
	  table_owner = constraints->value;
	  break;
	case GDA_Connection_OBJECT_NAME:
	  table_name = constraints->value;
	  break;
	case GDA_Connection_TABLE_TYPE:
	  table_type = constraints->value;
	  break;
	default :
	  fprintf(stderr,"schema_tables: invalid constraint type %d\n", constraints->ctype);
      odbc_recset->allocated = 0;
	  SQLFreeStmt(odbc_recset->hstmt, SQL_DROP);
	  return NULL;
	  break;
	}
    length--;
    constraints++;
  }

  rc = SQLTables( odbc_recset->hstmt,
		  table_qualifier, table_qualifier ? SQL_NTS : 0,
		  table_owner,     table_owner ? SQL_NTS : 0, 
		  table_name,      table_name ? SQL_NTS : 0,
		  table_type,      table_type ? SQL_NTS : 0 );

  if( !SQL_SUCCEEDED( rc ))
  {
    error = gda_server_error_new();
    gda_server_error_make(error, recset, cnc, __PRETTY_FUNCTION__);

    SQLFreeStmt( odbc_recset->hstmt, SQL_DROP );
    odbc_recset->allocated = 0;
    gda_server_recordset_free(recset);

    return NULL;
  }

  return gda_odbc_describe_recset(recset, odbc_recset);
}

static Gda_ServerRecordset*  schema_columns(Gda_ServerRecordset*  recset,
					 Gda_ServerConnection* cnc,
					 GDA_Connection_Constraint* constraints,
					 gint length)
{
  ODBC_Recordset*   odbc_recset;
  ODBC_Connection*  od_cnc;
  SQLRETURN         rc;
  Gda_ServerError*  error;
  gchar*            table_qualifier = NULL;
  gchar*            table_owner     = NULL;
  gchar*            table_name      = NULL;
  gchar*            column_name     = NULL;

  g_return_val_if_fail( recset != NULL, NULL );
  g_return_val_if_fail( cnc != NULL, NULL );

  odbc_recset = (ODBC_Recordset *) gda_server_recordset_get_user_data(recset);
  g_return_val_if_fail( odbc_recset != NULL, NULL );

  od_cnc = (ODBC_Connection *) gda_server_connection_get_user_data(cnc);
  g_return_val_if_fail( od_cnc != NULL, NULL );

  rc = SQLAllocStmt( od_cnc->hdbc, &odbc_recset->hstmt );
  if( !SQL_SUCCEEDED( rc ))
  {
    error = gda_server_error_new();
    gda_server_error_make(error, recset, cnc, __PRETTY_FUNCTION__);
    gda_server_recordset_free(recset);

    return NULL;
  }

  odbc_recset->allocated = 1;
  odbc_recset->mapped_cols = 4;
  odbc_recset->map[ 0 ] = 2;
  odbc_recset->map[ 1 ] = 5;
  odbc_recset->map[ 2 ] = 6;
  odbc_recset->map[ 3 ] = 9;
  
  while(length)
  {
    switch (constraints->ctype)
	{
	case GDA_Connection_OBJECT_CATALOG :
	  table_qualifier = constraints->value;
	  break;

	case GDA_Connection_OBJECT_SCHEMA :
	  table_owner = constraints->value;
	  break;

	case GDA_Connection_OBJECT_NAME :
	  table_name = constraints->value;
	  break;

	case GDA_Connection_COLUMN_NAME :
	  column_name = constraints->value;
	  break;

	default :
	  fprintf(stderr,"schema_columns: invalid constraint type %d\n", constraints->ctype);
      odbc_recset->allocated = 0;
	  SQLFreeStmt(odbc_recset->hstmt, SQL_DROP);
      gda_server_recordset_free(recset);
	  return NULL;
	  break;
	}
    length--;
    constraints++;
  }

  rc = SQLColumns( odbc_recset->hstmt,
		  table_qualifier, table_qualifier ? SQL_NTS : 0,
		  table_owner,     table_owner ? SQL_NTS : 0, 
		  table_name,      table_name ? SQL_NTS : 0,
		  column_name,     column_name ? SQL_NTS : 0 );

  if( !SQL_SUCCEEDED( rc ))
  {
    error = gda_server_error_new();
    gda_server_error_make(error, recset, cnc, __PRETTY_FUNCTION__);

    SQLFreeStmt( odbc_recset->hstmt, SQL_DROP );
    odbc_recset->allocated = 0;
    gda_server_recordset_free(recset);

    return NULL;
  }

  return gda_odbc_describe_recset(recset, odbc_recset);
}

static Gda_ServerRecordset*  schema_types(Gda_ServerRecordset*   recset,
					 Gda_ServerConnection*  cnc,
					 GDA_Connection_Constraint*  constraint,
					 gint length)
{
  ODBC_Recordset*   odbc_recset;
  ODBC_Connection*  od_cnc;
  SQLRETURN         rc;
  Gda_ServerError*  error;

  g_return_val_if_fail( recset != NULL, NULL );
  g_return_val_if_fail( cnc != NULL, NULL );

  odbc_recset = (ODBC_Recordset *) gda_server_recordset_get_user_data(recset);
  g_return_val_if_fail( odbc_recset != NULL, NULL );

  od_cnc = (ODBC_Connection *) gda_server_connection_get_user_data(cnc);
  g_return_val_if_fail( od_cnc != NULL, NULL );

  rc = SQLAllocStmt( od_cnc->hdbc, &odbc_recset->hstmt );
  if( !SQL_SUCCEEDED( rc ))
  {
    error = gda_server_error_new();
    gda_server_error_make(error, recset, cnc, __PRETTY_FUNCTION__);
    gda_server_recordset_free(recset);

    return NULL;
  }

  odbc_recset->allocated = 1;
  odbc_recset->mapped_cols = 1;
  odbc_recset->map[ 0 ] = 0;

  rc = SQLGetTypeInfo( odbc_recset->hstmt, SQL_ALL_TYPES );

  if( !SQL_SUCCEEDED( rc ))
  {
    error = gda_server_error_new();
    gda_server_error_make(error, recset, cnc, __PRETTY_FUNCTION__);

    SQLFreeStmt( odbc_recset->hstmt, SQL_DROP );
    odbc_recset->allocated = 0;
    gda_server_recordset_free(recset);

    return NULL;
  }

  return gda_odbc_describe_recset(recset, odbc_recset);
}

static Gda_ServerRecordset*  schema_views(Gda_ServerRecordset* recset,
					 Gda_ServerConnection* cnc,
					 GDA_Connection_Constraint* constraint,
					 gint length)
{
  ODBC_Recordset*   odbc_recset;
  ODBC_Connection*  od_cnc;
  SQLRETURN         rc;
  Gda_ServerError*  error;

  g_return_val_if_fail( recset != NULL, NULL );
  g_return_val_if_fail( cnc != NULL, NULL );

  odbc_recset = (ODBC_Recordset *) gda_server_recordset_get_user_data(recset);
  g_return_val_if_fail( odbc_recset != NULL, NULL );

  od_cnc = (ODBC_Connection *) gda_server_connection_get_user_data(cnc);
  g_return_val_if_fail( od_cnc != NULL, NULL );

  rc = SQLAllocStmt( od_cnc->hdbc, &odbc_recset->hstmt );
  if( !SQL_SUCCEEDED( rc ))
  {
    error = gda_server_error_new();
    gda_server_error_make(error, recset, cnc, __PRETTY_FUNCTION__);
    gda_server_recordset_free(recset);

    return NULL;
  }

  odbc_recset->allocated = 1;
  odbc_recset->mapped_cols = 3;
  odbc_recset->map[ 0 ] = 2;
  odbc_recset->map[ 1 ] = 1;
  odbc_recset->map[ 2 ] = 4;

  rc = SQLTables( odbc_recset->hstmt,
          NULL, 0,
          NULL, 0,
          NULL, 0,
          "VIEW", SQL_NTS );

  if( !SQL_SUCCEEDED( rc ))
  {
    error = gda_server_error_new();
    gda_server_error_make(error, recset, cnc, __PRETTY_FUNCTION__);

    SQLFreeStmt( odbc_recset->hstmt, SQL_DROP );
    odbc_recset->allocated = 0;
    gda_server_recordset_free(recset);

    return NULL;
  }

  return gda_odbc_describe_recset(recset, odbc_recset);
}

static Gda_ServerRecordset*  schema_indexes(Gda_ServerRecordset* recset,
					 Gda_ServerConnection* cnc,
					 GDA_Connection_Constraint* constraint,
					 gint length)
{
  ODBC_Recordset*   odbc_recset;
  ODBC_Connection*  od_cnc;
  SQLRETURN         rc;
  Gda_ServerError*  error;

  g_return_val_if_fail( recset != NULL, NULL );
  g_return_val_if_fail( cnc != NULL, NULL );

  odbc_recset = (ODBC_Recordset *) gda_server_recordset_get_user_data(recset);
  g_return_val_if_fail( odbc_recset != NULL, NULL );

  od_cnc = (ODBC_Connection *) gda_server_connection_get_user_data(cnc);
  g_return_val_if_fail( od_cnc != NULL, NULL );

  rc = SQLAllocStmt( od_cnc->hdbc, &odbc_recset->hstmt );
  if( !SQL_SUCCEEDED( rc ))
  {
    error = gda_server_error_new();
    gda_server_error_make(error, recset, cnc, __PRETTY_FUNCTION__);
    gda_server_recordset_free(recset);

    return NULL;
  }

  odbc_recset->allocated = 1;

  rc = SQLStatistics( odbc_recset->hstmt,
          NULL, 0,
          NULL, 0,
          NULL, 0,
          SQL_INDEX_ALL,
          SQL_QUICK );

  if( !SQL_SUCCEEDED( rc ))
  {
    error = gda_server_error_new();
    gda_server_error_make(error, recset, cnc, __PRETTY_FUNCTION__);

    SQLFreeStmt( odbc_recset->hstmt, SQL_DROP );
    odbc_recset->allocated = 0;
    gda_server_recordset_free(recset);

    return NULL;
  }

  return gda_odbc_describe_recset(recset, odbc_recset);
}

static Gda_ServerRecordset*  schema_procedures(Gda_ServerRecordset* recset,
					 Gda_ServerConnection* cnc,
					 GDA_Connection_Constraint* constraint,
					 gint length)
{
  ODBC_Recordset*   odbc_recset;
  ODBC_Connection*  od_cnc;
  SQLRETURN         rc;
  Gda_ServerError*  error;

  g_return_val_if_fail( recset != NULL, NULL );
  g_return_val_if_fail( cnc != NULL, NULL );

  odbc_recset = (ODBC_Recordset *) gda_server_recordset_get_user_data(recset);
  g_return_val_if_fail( odbc_recset != NULL, NULL );

  od_cnc = (ODBC_Connection *) gda_server_connection_get_user_data(cnc);
  g_return_val_if_fail( od_cnc != NULL, NULL );

  rc = SQLAllocStmt( od_cnc->hdbc, &odbc_recset->hstmt );
  if( !SQL_SUCCEEDED( rc ))
  {
    error = gda_server_error_new();
    gda_server_error_make(error, recset, cnc, __PRETTY_FUNCTION__);
    gda_server_recordset_free(recset);

    return NULL;
  }

  odbc_recset->allocated = 1;
  odbc_recset->mapped_cols = 1;
  odbc_recset->map[ 0 ] = 2;

  rc = SQLProcedures( odbc_recset->hstmt,
          NULL, 0,
          NULL, 0,
          NULL, 0 );

  if( !SQL_SUCCEEDED( rc ))
  {
    error = gda_server_error_new();
    gda_server_error_make(error, recset, cnc, __PRETTY_FUNCTION__);

    SQLFreeStmt( odbc_recset->hstmt, SQL_DROP );
    odbc_recset->allocated = 0;
    gda_server_recordset_free(recset);

    return NULL;
  }

  return gda_odbc_describe_recset(recset, odbc_recset);
}
