/* GNOME DB ORACLE Provider
 * Copyright (C) 2000 Rodrigo Moya
 * Copyright (C) 2000 Stephan Heinze
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

#include "gda-oracle.h"

/*
 * Private functions
 */
static gboolean
fill_field_values (Gda_ServerRecordset *recset)
{
  GList* node;

  g_return_val_if_fail(recset != NULL, FALSE);

  /* traverse all fields */
  for (node = g_list_first(recset->fields); node != NULL; node = g_list_next(node))
    {
      Gda_ServerField* field = (Gda_ServerField *) node->data;
      ORACLE_Field*    ora_field;

      ora_field = (ORACLE_Field *) gda_server_field_get_user_data(field);
      if (ora_field)
	{
	  /* ora_field->real_value must have been allocated in init_recset_fields */
	  if (ora_field->real_value)
	    {
	      /* FIXME: change to use the gda_server_field_set* functions */
	      gda_server_field_set_actual_length(field, field->defined_length);
	      switch (field->sql_type)
		{
		case SQLT_CHR : /* VARCHAR2 */
                case SQLT_STR : /* null-terminated string */
                case SQLT_VCS : /* VARCHAR */
                case SQLT_RID : /* ROWID */
                case SQLT_LNG : /* LONG */
                case SQLT_LVC : /* LONG VARCHAR */
                case SQLT_AFC : /* CHAR */
                case SQLT_AVC : /* CHARZ */
                case SQLT_LAB : /* MSLABEL */
                  field->value->_d = GDA_TypeVarchar;
                  if (-1 == ora_field->indicator)
                    field->value->_u.lvc = CORBA_string_dup("<NULL>");
                  else
                    field->value->_u.lvc = CORBA_string_dup(ora_field->real_value);
		  break;
		case SQLT_VNU : /* VARNUM */
                case SQLT_NUM : /* NUMBER */
                  /* was mapped to string while Definining output fields */
		  break;
		case SQLT_FLT : /* FLOAT */
		  field->value->_d = GDA_TypeSingle;
		  if (-1 == ora_field->indicator)
		    field->value->_u.f = 0;
		  else
		    {
		      memcpy(&field->value->_u.f,
			     ora_field->real_value,
			     sizeof(gfloat));
		    }
                  break;
		case SQLT_INT : /* 8-bit, 16-bit and 32-bit integers */
                case SQLT_UIN : /* UNSIGNED INT */
                  field->value->_d = GDA_TypeInteger;
                  if (-1 == ora_field->indicator)
                    field->value->_u.i = 0;
                  else
                    {
                      memcpy(&field->value->_u.i,
                             ora_field->real_value,
                             sizeof(glong));
                    }
		  break;
		case SQLT_DAT : /* DATE */
                  field->value->_d = GDA_TypeDate;
                  if (-1 == ora_field->indicator)
                    {
                      memset(&field->value->_u.dbd,
                             0,
                             sizeof(GDA_DbDate));
                    }
                  else
                    {
                      memcpy(&field->value->_u.dbd,
                             ora_field->real_value,
                             sizeof(GDA_DbDate));
                    }
		  break;
		case SQLT_VBI : /* VARRAW */
                case SQLT_BIN : /* RAW */
                case SQLT_LBI : /* LONG RAW */
                case SQLT_LVB : /* LONG VARRAW */
                  field->value->_d = GDA_TypeBinary;
                  memcpy(&field->value->_u.fb,
                         ora_field->real_value,
                         sizeof(GDA_VarBinString));
		  break;
                default :
                  gda_log_error(_("Unknown ORACLE data type in fill_field_values"));
		  break;
                }
	    }
	  else
	    {
	      gda_server_field_set_actual_length(field, 0);
	      memset(&field->value->_u, 0, sizeof(field->value->_u));
	    }
	}
    }
  return TRUE;
}

/*
 * Public functions
 */
gboolean
gda_oracle_recordset_new (Gda_ServerRecordset *recset)
{
  Gda_ServerConnection* cnc;
  ORACLE_Recordset*  ora_recset;
  ORACLE_Connection* ora_cnc;

  g_return_val_if_fail(recset != NULL, FALSE);

  ora_recset = g_new0(ORACLE_Recordset, 1);
  gda_server_recordset_set_user_data(recset, (gpointer) ora_recset);

  cnc = gda_server_recordset_get_connection(recset);
  if (cnc)
    {
      ora_cnc = (ORACLE_Connection *) gda_server_connection_get_user_data(cnc);
      ora_recset->ora_cnc = ora_cnc;
    }
  else gda_log_message(_("recordset object being created without associated connection"));
  return TRUE;
}

gint
gda_oracle_recordset_move_next (Gda_ServerRecordset *recset)
{
  ub4               status;
  ORACLE_Recordset* ora_recset;

  g_return_val_if_fail(recset != NULL, -1);

  ora_recset = (ORACLE_Recordset *) gda_server_recordset_get_user_data(recset);
  if (ora_recset)
    {
      status = OCIStmtFetch(ora_recset->hstmt,
			    ora_recset->ora_cnc->herr,
			    1,
			    OCI_FETCH_NEXT,
			    OCI_DEFAULT);
      switch (status)
	{
	case OCI_SUCCESS :
	case OCI_SUCCESS_WITH_INFO :
	  gda_server_recordset_set_at_end(recset, FALSE);
	  gda_server_recordset_set_at_begin(recset, FALSE);
	  if (fill_field_values(recset)) return 0;
	  break;
	case OCI_NO_DATA :
	  gda_server_recordset_set_at_end(recset, TRUE);
	  return 1;
	}
    }
  gda_server_error_make(gda_server_error_new(),
			0,
			gda_server_recordset_get_connection(recset),
			__PRETTY_FUNCTION__);
  return -1;
}

gint
gda_oracle_recordset_move_prev (Gda_ServerRecordset *recset)
{
  ub4               status;
  ORACLE_Recordset* ora_recset;

  g_return_val_if_fail(recset != NULL, -1);

  ora_recset = (ORACLE_Recordset *) gda_server_recordset_get_user_data(recset);
  if (ora_recset)
    {
      status = OCIStmtFetch(ora_recset->hstmt,
			    ora_recset->ora_cnc->herr,
			    1,
			    OCI_FETCH_PRIOR,
			    OCI_DEFAULT);
      switch (status)
	{
	case OCI_SUCCESS :
	case OCI_SUCCESS_WITH_INFO :
	  gda_server_recordset_set_at_end(recset, FALSE);
	  gda_server_recordset_set_at_begin(recset, FALSE);
	  if (fill_field_values(recset)) return 0;
	  break;
	case OCI_NO_DATA :
	  gda_server_recordset_set_at_begin(recset, TRUE);
	  return 1;
	}
    }
  gda_server_error_make(gda_server_error_new(),
			0,
			gda_server_recordset_get_connection(recset),
			__PRETTY_FUNCTION__);
  return -1;
}

gint
gda_oracle_recordset_close (Gda_ServerRecordset *recset)
{
  g_return_val_if_fail(recset != NULL, -1);
  return 0;
}

void
gda_oracle_recordset_free (Gda_ServerRecordset *recset)
{
  ORACLE_Recordset* ora_recset;

  g_return_if_fail(recset != NULL);

  ora_recset = (ORACLE_Recordset *) gda_server_recordset_get_user_data(recset);
  if (ora_recset)
    {
      GList* node;

      /* free all fields */
      for (node = g_list_first(recset->fields); node != NULL; node = g_list_next(node))
	{
	  ORACLE_Field*    ora_field;
	  Gda_ServerField* field = (Gda_ServerField *) node->data;
	  if (field)
	    {
	      ora_field = (ORACLE_Field *) gda_server_field_get_user_data(field);
	      if (ora_field)
		{
		  if (ora_field->real_value) g_free(ora_field->real_value);
		  g_free((gpointer) ora_field);
		}
	    }
	}
      g_free((gpointer) ora_recset);
    }
}


