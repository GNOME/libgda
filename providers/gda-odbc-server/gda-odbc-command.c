/* GNOME DB ODBC Provider
 * Copyright (C) Nick Gorham
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

/*
 * Private functions
 */

static void *
value_2_ptr (GDA_Value * v)
{

	switch (v->_d) {
	case GDA_TypeNull:
		g_warning ("GOT GDA_TYPE_NULL");
		abort ();
		break;
	case GDA_TypeBigint:
		return v->_u.lvc;	/* unsigned char */
		break;
	case GDA_TypeBinary:
		return v->_u.lvb._buffer;	/* unsigned char*  */
		break;
	case GDA_TypeBoolean:	/* unsigned char */
		return &v->_u.b;
		break;
	case GDA_TypeBstr:
		return v->_u.lvb._buffer;	/* unsigned char* */
		break;
	case GDA_TypeChar:
		return v->_u.lvc;	/* unsigned char*  */
		break;
	case GDA_TypeCurrency:
		return v->_u.lvc;	/* unsigned char* */
		break;
	case GDA_TypeDate:
		return &v->_u.d;	/* struct SQLDATE */
		break;
	case GDA_TypeDbDate:
		return &v->_u.dbd;	/* struct SQLDATE */
		break;
	case GDA_TypeDbTime:
		return &v->_u.dbt;	/* struct SQLTIME */
		break;
	case GDA_TypeDbTimestamp:
		return &v->_u.dbtstamp;	/* struct SQLTIMESTAMP */
		break;
	case GDA_TypeDecimal:
		return v->_u.lvc;	/* unsigned char* */
		break;
	case GDA_TypeDouble:
		return &v->_u.dp;	/* double */
	case GDA_TypeError:
		return &v->_u.lvc;	/* unsigned char* */
		break;
	case GDA_TypeInteger:
		return &v->_u.i;	/* CORBA_long */
	case GDA_TypeLongvarbin:
		return v->_u.lvb._buffer;	/* unsigned char* */
	case GDA_TypeLongvarchar:
		return v->_u.lvc;	/* unsigned char* */
	case GDA_TypeLongvarwchar:
		return v->_u.lvc;	/* unsigned char* */
	case GDA_TypeNumeric:
		return v->_u.lvc;	/* unsigned char* */
	case GDA_TypeSingle:
		return &v->_u.f;	/* float */
	case GDA_TypeSmallint:
		return &v->_u.si;	/* short */
	case GDA_TypeTinyint:
		return &v->_u.c;	/* unsigned char */
	case GDA_TypeUBigint:
		return v->_u.lvc;	/* unsigned char* */
	case GDA_TypeUSmallint:
		return &v->_u.us;	/* unsigned short */
	case GDA_TypeVarchar:
		return v->_u.lvc;	/* unsigned char* */
	case GDA_TypeVarbin:
		return v->_u.lvb._buffer;	/* unsigned char* */
	case GDA_TypeVarwchar:
		return v->_u.lvc;	/* unsigned char* */
	case GDA_TypeFixchar:
		return v->_u.lvc;	/* unsigned char* */
	case GDA_TypeFixbin:
		return v->_u.lvb._buffer;	/* unsigned char* */
	case GDA_TypeFixwchar:
		return v->_u.lvc;	/* unsigned char* */
	default:
		g_warning
			("value_2_ptr: reached end of function, no matching GDA type found\n");
		abort ();
	}
	return (void *) 0;
}

static long
value_2_inputsize (GDA_Value * v)
{

	switch (v->_d) {
	case GDA_TypeNull:
		g_warning ("GOT GDA_TYPE_NULL");
		abort ();
		break;
	case GDA_TypeBigint:
		return SQL_NTS;	/* unsigned char */
		break;
	case GDA_TypeBinary:
		return v->_u.lvb._length;	/* unsigned char*  */
		break;
	case GDA_TypeBoolean:	/* unsigned char */
		return sizeof (v->_u.b);
		break;
	case GDA_TypeBstr:
		return v->_u.lvb._length;	/* unsigned char* */
		break;
	case GDA_TypeChar:
		return SQL_NTS;	/* unsigned char*  */
		break;
	case GDA_TypeCurrency:
		return SQL_NTS;	/* unsigned char* */
		break;
	case GDA_TypeDate:
		return sizeof (v->_u.d);	/* struct SQLDATE */
		break;
	case GDA_TypeDbDate:
		return sizeof (v->_u.dbd);	/* struct SQLDATE */
		break;
	case GDA_TypeDbTime:
		return sizeof (v->_u.dbt);	/* struct SQLTIME */
		break;
	case GDA_TypeDbTimestamp:
		return sizeof (v->_u.dbtstamp);	/* struct SQLTIMESTAMP */
		break;
	case GDA_TypeDecimal:
		return SQL_NTS;	/* unsigned char* */
		break;
	case GDA_TypeDouble:
		return sizeof (v->_u.dp);	/* double */
	case GDA_TypeError:
		return SQL_NTS;	/* unsigned char* */
		break;
	case GDA_TypeInteger:
		return sizeof (v->_u.i);	/* CORBA_long */
	case GDA_TypeLongvarbin:
		return v->_u.lvb._length;	/* unsigned char* */
	case GDA_TypeLongvarchar:
		return SQL_NTS;	/* unsigned char* */
	case GDA_TypeLongvarwchar:
		return SQL_NTS;	/* unsigned char* */
	case GDA_TypeNumeric:
		return SQL_NTS;	/* unsigned char* */
	case GDA_TypeSingle:
		return sizeof (v->_u.f);	/* float */
	case GDA_TypeSmallint:
		return sizeof (v->_u.si);	/* short */
	case GDA_TypeTinyint:
		return sizeof (v->_u.c);	/* unsigned char */
	case GDA_TypeUBigint:
		return SQL_NTS;	/* unsigned char* */
	case GDA_TypeUSmallint:
		return sizeof (v->_u.us);	/* unsigned short */
	case GDA_TypeVarchar:
		return SQL_NTS;	/* unsigned char* */
	case GDA_TypeVarbin:
		return v->_u.lvb._length;	/* unsigned char* */
	case GDA_TypeVarwchar:
		return SQL_NTS;	/* unsigned char* */
	case GDA_TypeFixchar:
		return SQL_NTS;	/* unsigned char* */
	case GDA_TypeFixbin:
		return v->_u.lvb._length;	/* unsigned char* */
	case GDA_TypeFixwchar:
		return SQL_NTS;	/* unsigned char* */
	default:
		g_warning
			("value_2_inputsize: reached end of function, no matching GDA type found\n");
		abort ();
	}
	return 0;
}

/*
 * Public functions
 */
gboolean
gda_odbc_command_new (GdaServerCommand * cmd)
{
	return TRUE;
}

GdaServerRecordset *
gda_odbc_command_execute (GdaServerCommand * cmd,
			  GdaError * error,
			  const GDA_CmdParameterSeq * params,
			  gulong * affected, gulong options)
{
	GdaServerRecordset *recset = NULL;
	ODBC_Connection *od_cnc;
	ODBC_Recordset *odbc_recset;
	GdaServerConnection *cnc;
	SQLRETURN rc;
	gchar *cmd_string;
	SQLSMALLINT nparams;
	GDA_CmdParameter *current_parameter;

	g_return_val_if_fail (cmd != NULL, NULL);

	cnc = gda_server_command_get_connection (cmd);
	if (cnc) {
		od_cnc = (ODBC_Connection *)
			gda_server_connection_get_user_data (cnc);
		if (od_cnc) {
			cmd_string = gda_server_command_get_text (cmd);
			if (cmd_string) {
				recset = gda_server_recordset_new (cnc);
				odbc_recset =
					(ODBC_Recordset *)
					gda_server_recordset_get_user_data
					(recset);
				if (odbc_recset) {
					rc = SQLAllocStmt (od_cnc->hdbc,
							   &odbc_recset->
							   hstmt);
					if (!SQL_SUCCEEDED (rc)) {
						error = gda_error_new ();
						gda_server_error_make (error,
								       recset,
								       cnc,
								       __PRETTY_FUNCTION__);

						return NULL;
					}

					odbc_recset->allocated = 1;

					rc = SQLPrepare (odbc_recset->hstmt,
							 cmd_string, SQL_NTS);

					if (!SQL_SUCCEEDED (rc)) {
						error = gda_error_new ();
						gda_server_error_make (error,
								       recset,
								       cnc,
								       __PRETTY_FUNCTION__);

						SQLFreeStmt (odbc_recset->
							     hstmt, SQL_DROP);
						odbc_recset->allocated = 0;
						gda_server_recordset_free
							(recset);

						return NULL;
					}

					rc = SQLNumParams (odbc_recset->hstmt,
							   &nparams);
					if (!SQL_SUCCEEDED (rc)) {
						error = gda_error_new ();
						gda_server_error_make (error,
								       recset,
								       cnc,
								       __PRETTY_FUNCTION__);

						SQLFreeStmt (odbc_recset->
							     hstmt, SQL_DROP);
						odbc_recset->allocated = 0;
						gda_server_recordset_free
							(recset);

						return NULL;
					}
					if (params
					    &&
					    ((nparams && params->_length == 0)
					     || (!nparams
						 && params->_length))) {
						if (nparams) {
							g_log ("GDA ODBC",
							       G_LOG_LEVEL_ERROR,
							       "Statement needs parameters, but none passed\n");
						}
						else {
							g_log ("GDA ODBC",
							       G_LOG_LEVEL_ERROR,
							       "Statement doesn't need parameters, but params availble\n");
						}
					}
					if (nparams) {
						int i;

						if (odbc_recset->sizes) {
							free (odbc_recset->
							      sizes);
						}

						odbc_recset->sizes =
							malloc (nparams *
								sizeof
								(SQLINTEGER));
						if (!odbc_recset->sizes) {
							error = gda_error_new
								();
							gda_server_error_make
								(error,
								 recset, cnc,
								 __PRETTY_FUNCTION__);

							SQLFreeStmt
								(odbc_recset->
								 hstmt,
								 SQL_DROP);
							odbc_recset->
								allocated = 0;
							gda_server_recordset_free
								(recset);

							return NULL;
						}

						for (i = 0; i < nparams; i++) {
							gushort sqlType;
							gulong precision;
							gshort scale;
							gshort nullable;
							unsigned long ctype;

							current_parameter =
								&(params->
								  _buffer[i]);

							if (current_parameter->value._d) {
								g_warning
									("NULL PARAMETER NYI");
							}

							ctype = gda_odbc_connection_get_c_type (cnc, current_parameter->value._u.v._d);

							rc = SQLDescribeParam
								(odbc_recset->
								 hstmt, i + 1,
								 &sqlType,
								 &precision,
								 &scale,
								 &nullable);

							if (!SQL_SUCCEEDED
							    (rc)) {
								error = gda_error_new ();
								gda_server_error_make
									(error,
									 recset,
									 cnc,
									 __PRETTY_FUNCTION__);

								SQLFreeStmt
									(odbc_recset->
									 hstmt,
									 SQL_DROP);
								odbc_recset->
									allocated
									= 0;
								gda_server_recordset_free
									(recset);

								return NULL;
							}

							odbc_recset->
								sizes[i] =
								value_2_inputsize
								(&current_parameter->
								 value._u.v);
							rc = SQLBindParameter
								(odbc_recset->
								 hstmt, i + 1,
								 SQL_PARAM_INPUT,
								 ctype,
								 sqlType,
								 precision,
								 scale,
								 value_2_ptr
								 (&current_parameter->
								  value._u.v),
								 0,
								 &odbc_recset->
								 sizes[i]
								);

							if (!SQL_SUCCEEDED
							    (rc)) {
								error = gda_error_new ();
								gda_server_error_make
									(error,
									 recset,
									 cnc,
									 __PRETTY_FUNCTION__);

								SQLFreeStmt
									(odbc_recset->
									 hstmt,
									 SQL_DROP);
								odbc_recset->
									allocated
									= 0;
								gda_server_recordset_free
									(recset);

								return NULL;
							}
						}
					}

					rc = SQLExecute (odbc_recset->hstmt);

					if (!SQL_SUCCEEDED (rc)) {
						error = gda_error_new ();
						gda_server_error_make (error,
								       recset,
								       cnc,
								       __PRETTY_FUNCTION__);

						SQLFreeStmt (odbc_recset->
							     hstmt, SQL_DROP);
						odbc_recset->allocated = 0;
						gda_server_recordset_free
							(recset);

						return NULL;
					}

					return gda_odbc_describe_recset
						(recset, odbc_recset);
				}
			}
			else {
				gda_server_error_make (error, 0, cnc,
						       __PRETTY_FUNCTION__);
			}
		}
	}

	return recset;
}

void
gda_odbc_command_free (GdaServerCommand * cmd)
{
}
