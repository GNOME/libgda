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

int map_cols( ODBC_Recordset *odbc_recset, int col )
{
    if ( odbc_recset -> mapped_cols )
    {
        return odbc_recset -> map[ col ];
    }
    else
    {
        return col;
    }
}

static void
fill_field_values (Gda_ServerRecordset *recset, ODBC_Recordset *odbc_recset)
{
  SQLSMALLINT   i;
  GList*        node;
  SQLINTEGER    len;
  SQLRETURN     rc;

  g_return_if_fail(recset != NULL);
  g_return_if_fail(odbc_recset != NULL);

  for ( i = 0; i < ( odbc_recset -> mapped_cols ? odbc_recset -> mapped_cols : odbc_recset -> ncols ); i ++ )
  {
    node = g_list_nth(gda_server_recordset_get_fields(recset), i);
    if ( node )
    {
      Gda_ServerField* field = (Gda_ServerField *) node->data;

      switch (gda_server_field_get_sql_type(field))
      {
        case SQL_VARCHAR:
          {
              char txt[ 1024 ];
              rc = SQLGetData( odbc_recset->hstmt, map_cols( odbc_recset, i ) + 1, 
                      SQL_C_CHAR, txt, sizeof( txt ), &len );
              gda_server_field_set_varchar(field, txt);
              if ( len >= 0 )
                gda_server_field_set_actual_length((Gda_ServerField *) node->data, len );
          }
          break;

        case SQL_LONGVARCHAR:
          {
              char txt[ 1024 ];
              int total = 0;

              do
              {
                rc = SQLGetData( odbc_recset->hstmt, map_cols( odbc_recset, i ) + 1, 
                      SQL_C_CHAR, txt, sizeof( txt ), &len );

                if ( SQL_SUCCEEDED( rc ))
                {
                    gda_server_field_set_longvarchar(field, txt);
                    if ( len < 0 )
                    {
                        break;
                    }
                    else if ( rc == SQL_SUCCESS )
                    {
                        total += len;
                        break;
                    }
                    else
                    {
                        total += sizeof( txt );;
                    }
                }
              }
              while( SQL_SUCCEEDED( rc ));
              if ( total >= 0 )
                gda_server_field_set_actual_length((Gda_ServerField *) node->data, total );
          }
          break;

        case SQL_BINARY:
        case SQL_VARBINARY:
        case SQL_LONGVARBINARY:
          {
              char txt[ 1024 ];
              int total = 0;

              do
              {
                rc = SQLGetData( odbc_recset->hstmt, map_cols( odbc_recset, i ) + 1, 
                      SQL_C_BINARY, txt, sizeof( txt ), &len );

                if ( SQL_SUCCEEDED( rc ))
                {
                    if ( len < 0 )
                    {
                        break;
                    }
                    else if ( rc == SQL_SUCCESS )
                    {
                        total += len;
                        gda_server_field_set_varbin(field, txt, len);
                        break;
                    }
                    else
                    {
                        total += sizeof( txt );;
                        gda_server_field_set_varbin(field, txt, sizeof(txt));
                    }
                }
              }
              while( SQL_SUCCEEDED( rc ));
              if ( total >= 0 )
                gda_server_field_set_actual_length((Gda_ServerField *) node->data, total );
          }
          break;

        case SQL_DECIMAL:
        case SQL_NUMERIC:
          {
              char txt[ 1024 ];
              rc = SQLGetData( odbc_recset->hstmt, map_cols( odbc_recset, i ) + 1, 
                      SQL_C_CHAR, txt, sizeof( txt ), &len );
              gda_server_field_set_varchar(field, txt);
              if ( len >= 0 )
                gda_server_field_set_actual_length((Gda_ServerField *) node->data, len );
          }
          break;

        case SQL_BIT:
          {
              char cv;
              rc = SQLGetData( odbc_recset->hstmt, map_cols( odbc_recset, i ) + 1, 
                      SQL_C_BIT, &cv, sizeof( cv ), &len );
              gda_server_field_set_boolean(field, cv);
          }
          break;
    
        case SQL_TINYINT:
        case SQL_INTEGER:
        case SQL_BIGINT:
          {
              int cv;
              rc = SQLGetData( odbc_recset->hstmt, map_cols( odbc_recset, i ) + 1 , 
                      SQL_C_LONG, &cv, sizeof( cv ), &len );
              gda_server_field_set_integer(field, cv);
          }
          break;

        case SQL_SMALLINT:
          {
              short cv;
              rc = SQLGetData( odbc_recset->hstmt, map_cols( odbc_recset, i ) + 1, 
                      SQL_C_SHORT, &cv, sizeof( cv ), &len );
              gda_server_field_set_smallint(field, cv);
          }
          break;

        case SQL_REAL:
          {
              float cv;
              rc = SQLGetData( odbc_recset->hstmt, map_cols( odbc_recset, i ) + 1, 
                      SQL_C_FLOAT, &cv, sizeof( cv ), &len );
              gda_server_field_set_single(field, cv);
          }
          break;

        case SQL_FLOAT:
        case SQL_DOUBLE:
          {
              double cv;
              rc = SQLGetData( odbc_recset->hstmt, map_cols( odbc_recset, i ) + 1, 
                      SQL_C_FLOAT, &cv, sizeof( cv ), &len );
              gda_server_field_set_double(field, cv);
          }

#if (ODBCVER >= 0x0300)
        case SQL_TYPE_DATE:
        case SQL_DATE:
          {
              rc = SQLGetData( odbc_recset->hstmt, map_cols( odbc_recset, i ) + 1, 
                      SQL_C_TYPE_DATE, &field->value->_u.dbd, sizeof( SQL_TIME_STRUCT ), &len );
              gda_server_field_set_actual_length((Gda_ServerField *) node->data, 
                      sizeof( SQL_DATE_STRUCT ));
              field->value->_d = GDA_TypeDbDate;
          }
          break;

        case SQL_TYPE_TIME:
        case SQL_TIME:
          {
              rc = SQLGetData( odbc_recset->hstmt, map_cols( odbc_recset, i ) + 1, 
                      SQL_C_TYPE_TIME, &field->value->_u.dbt, sizeof( SQL_TIME_STRUCT ), &len );
              gda_server_field_set_actual_length((Gda_ServerField *) node->data, 
                      sizeof( SQL_TIME_STRUCT ));
              field->value->_d = GDA_TypeDbTime;
          }
          break;

        case SQL_TYPE_TIMESTAMP:
        case SQL_TIMESTAMP:
          {
              rc = SQLGetData( odbc_recset->hstmt, map_cols( odbc_recset, 1 ) + 1, 
                      SQL_C_TYPE_TIMESTAMP, &field->value->_u.dbtstamp, sizeof( SQL_TIMESTAMP_STRUCT ), &len );
              gda_server_field_set_actual_length((Gda_ServerField *) node->data, 
                      sizeof( SQL_TIMESTAMP_STRUCT ));
              field->value->_d = GDA_TypeDbTimestamp;
          }
          break;
#else
        case SQL_DATE:
          {
              rc = SQLGetData( odbc_recset->hstmt, map_cols( odbc_recset, i ) + 1, 
                      SQL_C_DATE, &field->value->_u.dbd, sizeof( SQL_TIME_STRUCT ), &len );
              gda_server_field_set_actual_length((Gda_ServerField *) node->data, 
                      sizeof( SQL_DATE_STRUCT ));
              field->value->_d = GDA_TypeDbDate;
          }
          break;

        case SQL_TIME:
          {
              rc = SQLGetData( odbc_recset->hstmt, map_cols( odbc_recset, i ) + 1, 
                      SQL_C_TIME, &field->value->_u.dbt, sizeof( SQL_TIME_STRUCT ), &len );
              gda_server_field_set_actual_length((Gda_ServerField *) node->data, 
                      sizeof( SQL_TIME_STRUCT ));
              field->value->_d = GDA_TypeDbTime;
          }
          break;

        case SQL_TIMESTAMP:
          {
              rc = SQLGetData( odbc_recset->hstmt, map_cols( odbc_recset, 1 ) + 1, 
                      SQL_C_TIMESTAMP, &field->value->_u.dbtstamp, sizeof( SQL_TIMESTAMP_STRUCT ), &len );
              gda_server_field_set_actual_length((Gda_ServerField *) node->data, 
                      sizeof( SQL_TIMESTAMP_STRUCT ));
              field->value->_d = GDA_TypeDbTimestamp;
          }
          break;
#endif

        default:
        case SQL_CHAR:
          {
              char txt[ 1024 ];
              rc = SQLGetData( odbc_recset->hstmt, map_cols( odbc_recset, i ) + 1, SQL_C_CHAR, txt, sizeof( txt ), &len );
              gda_server_field_set_char(field, txt);
              gda_server_field_set_actual_length((Gda_ServerField *) node->data, len );
          }
      }
      if ( len == SQL_NULL_DATA )
      {
        gda_server_field_set_actual_length((Gda_ServerField *) node->data, 0);
      }
    }
    else
    {
      gda_server_field_set_actual_length((Gda_ServerField *) node->data, 0);
    }
  }
}

gboolean
gda_odbc_recordset_new (Gda_ServerRecordset *recset)
{
  ODBC_Recordset* odbc_recset;

  odbc_recset = g_new0(ODBC_Recordset, 1);

  odbc_recset->allocated = 0;
  odbc_recset->sizes = NULL;
  
  gda_server_recordset_set_user_data(recset, (gpointer) odbc_recset);

  return TRUE;
}

gint
gda_odbc_recordset_move_next (Gda_ServerRecordset *recset)
{
  ODBC_Recordset* odbc_recset;

  g_return_val_if_fail( recset != NULL, -1 );

  odbc_recset = (ODBC_Recordset *) gda_server_recordset_get_user_data(recset);
  if ( odbc_recset )
  {
    SQLRETURN rc;

    rc = SQLFetch( odbc_recset->hstmt );

    if ( rc == SQL_NO_DATA )
    {
      gda_server_recordset_set_at_begin(recset, FALSE);
      gda_server_recordset_set_at_end(recset, TRUE);
      return 1;
    }
    else if ( SQL_SUCCEEDED( rc ))
    {
      fill_field_values( recset, odbc_recset );
      return 0;
    }
    else
    {
      return -1;
    }
  }
  return -1;
}

gint
gda_odbc_recordset_move_prev (Gda_ServerRecordset *recset)
{
  ODBC_Recordset* odbc_recset;

  g_return_val_if_fail( recset != NULL, -1 );

  odbc_recset = (ODBC_Recordset *) gda_server_recordset_get_user_data(recset);
  if ( odbc_recset )
  {
    SQLRETURN rc;

#if (ODBCVER >= 0x0300)
    rc = SQLFetchScroll( odbc_recset->hstmt, SQL_FETCH_PRIOR, 0 );
#else
    rc = SQLExtendedFetch( odbc_recset->hstmt, SQL_FETCH_PRIOR, 0, NULL );
#endif 

    if ( rc == SQL_NO_DATA )
    {
      gda_server_recordset_set_at_begin(recset, TRUE);
      gda_server_recordset_set_at_end(recset, FALSE);
      return 1;
    }
    else if ( SQL_SUCCEEDED( rc ))
    {
      fill_field_values( recset, odbc_recset );
      return 0;
    }
    else
    {
      return -1;
    }
  }
  return -1;
}

gint
gda_odbc_recordset_close (Gda_ServerRecordset *recset)
{
  ODBC_Recordset* odbc_recset;

  g_return_val_if_fail(recset != NULL, -1);

  odbc_recset = (ODBC_Recordset *) gda_server_recordset_get_user_data(recset);
  if (odbc_recset)
  {
    if ( odbc_recset -> allocated )
    {
      SQLFreeStmt( odbc_recset->hstmt, SQL_CLOSE );
      SQLFreeStmt( odbc_recset->hstmt, SQL_DROP );
    }
    odbc_recset -> allocated = 0;
    return 0;
  }

  if ( odbc_recset -> sizes )
  {
      free( odbc_recset -> sizes );
      odbc_recset -> sizes = 0;
  }

  return -1;
}

void
gda_odbc_recordset_free (Gda_ServerRecordset *recset)
{
  ODBC_Recordset* odbc_recset;

  g_return_if_fail(recset != NULL);

  odbc_recset = (ODBC_Recordset *) gda_server_recordset_get_user_data(recset);
  if (odbc_recset)
  {
    g_free((gpointer) odbc_recset);
  }
}

Gda_ServerRecordset*
gda_odbc_describe_recset(Gda_ServerRecordset* recset, ODBC_Recordset* odbc_recset)
{
  SQLRETURN         rc;
  SQLSMALLINT       ncols;
  gint              i;

  g_return_val_if_fail( odbc_recset != NULL, NULL);
    
  rc = SQLNumResultCols(odbc_recset->hstmt, &ncols);
  if (rc != SQL_SUCCESS)
  {
      return NULL;
  }
  if (ncols == 0)
  {
    return NULL;
  }

  odbc_recset -> ncols = ncols;

  for (i = 0; i < ( odbc_recset -> mapped_cols ? odbc_recset -> mapped_cols : ncols ); i++)
   {
    Gda_ServerField* field;
    SQLCHAR           colname[128];
    SQLSMALLINT       colnamelen;
    SQLSMALLINT       sql_type;
    SQLUINTEGER       defined_length;
    SQLSMALLINT       num_scale;
    SQLSMALLINT       nullable;
    

    field = gda_server_field_new();

    rc = SQLDescribeCol(odbc_recset->hstmt,
			  map_cols( odbc_recset, i ) + 1,
			  colname, sizeof(colname), &colnamelen,
			  &sql_type,
			  &defined_length,
			  &num_scale,
			  &nullable );

    if (!SQL_SUCCEEDED( rc ))
    {
      return NULL;
	}

    gda_server_field_set_name(field, colname);
    gda_server_field_set_sql_type(field, sql_type);
    gda_server_field_set_scale(field, num_scale);
    gda_server_field_set_actual_length(field, defined_length);
    gda_server_field_set_defined_length(field, defined_length);
    gda_server_field_set_user_data(field, NULL);

    gda_server_recordset_add_field( recset, field );
  }
  return recset;
}
