/* 
 * $Id$
 *
 * GNOME DB tds Provider
 * Copyright (C) 2000 Holger Thon
 * Copyright (C) 2000 Stephan Heinze
 * Copyright (C) 2000 Rodrigo Moya
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
// Revision 1.1  2000/11/04 23:42:17  holger
// 2000-11-05  Holger Thon <gnome-dms@users.sourceforge.net>
//
//   * Added gda-tds-server
//

#include "gda-tds.h"

void gda_tds_field_fill_values(Gda_ServerRecordset *rec,
                                  tds_Recordset *srec)
{
  CS_INT i  = 0;
  GList  *entry = NULL;
  
//  g_return_if_fail(rec != NULL);
//  g_return_if_fail(srec != NULL);
//  g_return_if_fail(srec->data != NULL);
//  g_return_if_fail(srec->colscnt > 0);
  
  for (i = 0; i < srec->colscnt; i++) {
    entry = g_list_nth(gda_server_recordset_get_fields(rec), i);
    if (entry) {
      Gda_ServerField *field = (Gda_ServerField *) entry->data;
      if (field) {
        if (srec->data[i].data) {
          switch (gda_server_field_get_sql_type(field)) {
            case CS_IMAGE_TYPE:
            case CS_LONGBINARY_TYPE:
            case CS_BINARY_TYPE:
            case CS_VARBINARY_TYPE:
              gda_server_field_set_varbin(field,
                             srec->data[i].data, srec->data[i].datalen);
              break;
            case CS_LONGCHAR_TYPE:
              srec->data[i].data[srec->data[i].datalen] = '\0';
              gda_server_field_set_longvarchar(field,
                                               srec->data[i].data);
              gda_server_field_set_actual_length(field,
                                                 srec->data[i].datalen);
              break;
            case CS_CHAR_TYPE:
            case CS_VARCHAR_TYPE:
            case CS_TEXT_TYPE:
              srec->data[i].data[srec->data[i].datalen] = '\0';
              gda_server_field_set_varchar(field,
                                           (gchar *) srec->data[i].data);
              gda_server_field_set_actual_length(field,
                                                 srec->data[i].datalen);
              break;
            case CS_BIT_TYPE:
              gda_server_field_set_boolean(field,
                                           (gboolean) *srec->data[i].data);
              break;
            case CS_TINYINT_TYPE:
            case CS_SMALLINT_TYPE:
              gda_server_field_set_smallint(field,
                                            (gshort) *srec->data[i].data);
              break;
            case CS_INT_TYPE:
              gda_server_field_set_integer(field,
                                           (gint) *srec->data[i].data);
              break;
            case CS_REAL_TYPE:
              gda_server_field_set_single(field,
                                          (gdouble) *srec->data[i].data);
              break;
            case CS_FLOAT_TYPE:
              gda_server_field_set_double(field,
                                          (gdouble) *srec->data[i].data);
              break;
            case CS_DATETIME_TYPE:
              gda_tds_field_set_datetime(field,
                                         (CS_DATETIME *) srec->data[i].data);
              break;
            case CS_DATETIME4_TYPE:
              gda_tds_field_set_datetime4(field,
                                          (CS_DATETIME4 *) srec->data[i].data);
              break;
            default:
            // try cs_convert data to CS_CHAR_TYPE
              gda_tds_field_set_general(field, &srec->data[i],
                                        srec->scnc);
              break;
          }
        } else {
          gda_server_field_set_actual_length(field, 0);
        }
      }
    }
  }
}

gint
gda_tds_row_result(gboolean            forward,
                      Gda_ServerRecordset *recset,
                      tds_Recordset    *srecset,
                      CS_COMMAND          *cmd)
{
  CS_INT               rows_read = 0;
  // We are are just called within internal procedures and trust
  // them giving no null pointers

  // FIXME: check if previous row fetch is implementable

  if (srecset->failed == TRUE) {
    gda_server_recordset_set_at_begin(recset, FALSE);
    gda_server_recordset_set_at_end(recset, TRUE);
    return 1;
  }

  srecset->ret = ct_fetch(cmd, CS_UNUSED, CS_UNUSED, CS_UNUSED, &rows_read);
  switch (srecset->ret) {
  case CS_SUCCEED:
    gda_tds_field_fill_values(recset, srecset);
    srecset->rows_cnt += rows_read;
    return 0;
    break;
  case CS_ROW_FAIL:
    gda_log_error(_("fetch error at row %d"), srecset->rows_cnt);
    srecset->rows_cnt += rows_read;
    return 0;
    break;
  case CS_END_DATA:
    gda_server_recordset_set_at_begin(recset, FALSE);
    gda_server_recordset_set_at_end(recset, TRUE);
    gda_log_message(_("CS_END_DATA on row %d; canceling"),
                    srecset->rows_cnt);
    TDS_CHK((CS_INT *) &srecset->ret,
            ct_cancel(NULL, cmd, CS_CANCEL_ALL),
                      NULL, recset, srecset->cnc, srecset->cmd
           );
    return 1;
    break;
  default:
    gda_log_error(_("%s: Error (%d)"), __PRETTY_FUNCTION__,
                  srecset->ret);
    gda_server_recordset_set_at_begin(recset, FALSE);
    gda_server_recordset_set_at_end(recset, TRUE);
    srecset->ret = ct_cancel(NULL, cmd, CS_CANCEL_ALL);
    return -1;
    break;
  }
}

gboolean
gda_tds_recordset_new(Gda_ServerRecordset *recset)
{
  tds_Recordset *srecset = NULL;

  g_return_val_if_fail(recset != NULL, FALSE);
  srecset = g_new0(tds_Recordset, 1);
  g_return_val_if_fail(srecset != NULL, FALSE);

  srecset->colscnt = 0;
  srecset->failed = FALSE;
  srecset->scnc = (tds_Connection *) NULL;
  srecset->scmd = (tds_Command *) NULL;

  /* We don't know the rowcount yet, so we initialize with null pointer */
  srecset->datafmt = (CS_DATAFMT *) NULL;
  srecset->data = (tds_Field *) NULL;

  gda_server_recordset_set_user_data(recset, (gpointer) srecset);

  return TRUE;
}

gint
gda_tds_recordset_move_next (Gda_ServerRecordset *recset)
{
  tds_Recordset *srecset  = NULL;
  CS_COMMAND       *cmd      = NULL;
  gint             ret       = 0;

  g_return_val_if_fail(recset != NULL, -1);
  srecset = (tds_Recordset *) gda_server_recordset_get_user_data(recset);
  g_return_val_if_fail(srecset != NULL, -1);
  g_return_val_if_fail(srecset->scmd != NULL, -1);
  cmd = srecset->scmd->cmd;
  g_return_val_if_fail(cmd != NULL, -1);

  switch (srecset->result_type) {
  case CS_ROW_RESULT:
  case CS_CURSOR_RESULT:
    if ((ret = gda_tds_row_result(TRUE, recset, srecset, cmd)) == 0) {
      gda_server_recordset_set_at_begin(recset, FALSE);
      return 0;
    } else {
      gda_server_recordset_set_at_end(recset, TRUE);
      return 1;
    }
    break;
  case CS_ROW_FAIL:
    return -1;
    break;
  default:
    gda_log_message("%s: %d", __PRETTY_FUNCTION__, srecset->ret);
    TDS_CHK((CS_INT *) &srecset->scnc->ret,
            ct_cancel(NULL, cmd, CS_CANCEL_ALL),
            NULL, recset, srecset->cnc, srecset->cmd
           );
    gda_server_recordset_set_at_begin(recset, FALSE);
    gda_server_recordset_set_at_end(recset, TRUE);
    return -1;
    break;
  }

  gda_server_recordset_set_at_begin(recset, FALSE);
  gda_server_recordset_set_at_end(recset, TRUE);
  return -1;
}

gint
gda_tds_recordset_move_prev (Gda_ServerRecordset *recset)
{
  g_return_val_if_fail(recset != NULL, -1);
  gda_server_recordset_set_at_begin(recset, FALSE);
  gda_server_recordset_set_at_end(recset, TRUE);

  return 1;
}

gint
gda_tds_recordset_close (Gda_ServerRecordset *recset)
{
  tds_Recordset *srecset = NULL;
  CS_INT           colnr = 0;

  g_return_val_if_fail(recset != NULL, -1);
  srecset = gda_server_recordset_get_user_data(recset);
  g_return_val_if_fail(srecset != NULL, -1);

  if (srecset->data) {
    while (colnr < srecset->colscnt) {
      if (srecset->data[colnr].data) {
        g_free((gpointer) srecset->data[colnr].data);
      }
      colnr++;
    }
    g_free((gpointer) srecset->data);
  }

  if (srecset->datafmt != NULL) {
    g_free((gpointer) srecset->datafmt);
  }

  return 0;
}

void
gda_tds_recordset_free (Gda_ServerRecordset *recset)
{
  tds_Recordset *srecset = NULL;
  CS_INT           colnr = 0;

  g_return_if_fail(recset != NULL);
  srecset = gda_server_recordset_get_user_data(recset);
  g_return_if_fail(srecset != NULL);

  if (srecset->data) {
    while (colnr < srecset->colscnt) {
      if (srecset->data[colnr].data) {
        g_free((gpointer) srecset->data[colnr].data);
      }
      colnr++;
    }
    g_free((gpointer) srecset->data);
  }

  if (srecset->datafmt != NULL) {
    g_free((gpointer) srecset->datafmt);
  }
  g_free((gpointer) srecset);

  gda_server_recordset_set_user_data(recset, (gpointer) NULL);
}

void
gda_tds_init_recset_fields (Gda_ServerError *err,
                            Gda_ServerRecordset *recset,
                            tds_Recordset *srecset,
                            CS_RETCODE result_type)
{
  gint       colnr;
  CS_RETCODE ret = CS_SUCCEED;
  CS_DATAFMT *datafmt = NULL;

  g_return_if_fail(recset != NULL);
  g_return_if_fail(srecset != NULL);
  g_return_if_fail(srecset->scmd != NULL);
  g_return_if_fail(srecset->scmd->cmd != NULL);
  g_return_if_fail(srecset->scnc != NULL);
  g_return_if_fail(srecset->scnc->cnc != NULL);
  /* Yes we mean ==, because we don't want to overwrite any existing data */
  g_return_if_fail(srecset->data == NULL);

  srecset->ret = result_type;
  srecset->rows_cnt = 0;

  // Count columns
  if (TDS_CHK((CS_INT *) &srecset->scnc->ret,
              ct_res_info(srecset->scmd->cmd, CS_NUMDATA,
                          &srecset->colscnt, CS_UNUSED, NULL
                         ),
              err, recset, srecset->cnc, srecset->cmd
             ) != CS_SUCCEED
     ) {
    gda_log_error(_("Failed fetching # of columns"));
    gda_server_recordset_free(recset);
    recset = NULL;
    return;
#ifdef TDS_DEBUG
  } else {
    gda_log_message(_("Counted %d columns"), srecset->colscnt);
#endif
  }

  // allocate datafmt structure for colscnt columns
  srecset->datafmt = g_new0(CS_DATAFMT, srecset->colscnt);
  if (!srecset->datafmt) {
    gda_log_error(_("%s could not allocate datafmt info"),
                  __PRETTY_FUNCTION__);
    gda_server_recordset_free(recset);
    return;
  }

  // allocate data structure for colscnt columns
  if (!srecset->data) {
    srecset->data = g_new0(tds_Field, srecset->colscnt);
    if (!srecset->data) {
      gda_log_error(_("%s could not allocate data fields"),
                    __PRETTY_FUNCTION__);
      gda_server_recordset_free(recset);
      return;
    }
  }

  /* set tds's resulttype */
  srecset->result_type = result_type;
  srecset->failed = FALSE;
  datafmt = srecset->datafmt;

  for (colnr = 0; colnr < srecset->colscnt; colnr++) {
    Gda_ServerField* field;

    // Initialize user data fmt pointer to datafmt pointer for colnr
    srecset->data[colnr].fmt = &srecset->datafmt[colnr];
#ifdef TDS_DEBUG
    gda_log_message(_("ct_describe on col # %d"), colnr + 1);
#endif

    // get datafmt info for colnr
    ret = ct_describe(srecset->scmd->cmd, colnr + 1,
                      &srecset->datafmt[colnr]);
    if (ret != CS_SUCCEED) {
      gda_log_error(_("ct_describe on col # %d failed"), colnr + 1);
      srecset->failed = TRUE;
      return;
    }

#ifdef TDS_DEBUG
    gda_log_message(_("ct_describe(%d): '%s', %d (%s)"), colnr + 1,
                    datafmt[colnr].name, datafmt[colnr].datatype,
                    tds_get_type_name(datafmt[colnr].datatype));
#endif

    // allocate enough memory for column colnr
    srecset->data[colnr].data = g_new0(gchar,
                                       srecset->datafmt[colnr].maxlength + 1);
    if (!srecset->data[colnr].data) {
      gda_log_error(_("could not allocate data holder"));
      srecset->failed = TRUE;
      return;
    }

    // bind column data placeholder to column colnr
    ret = ct_bind(srecset->scmd->cmd, (colnr + 1), &datafmt[colnr],
                  srecset->data[colnr].data, &srecset->data[colnr].datalen,
                  (CS_SMALLINT *)&srecset->data[colnr].indicator);
    if (ret != CS_SUCCEED) {
      gda_log_error(_("could not ct_bind data holder to column"));
      srecset->failed = TRUE;
      return;
    }

    // check if we got an illegal sql type, exit in that case
    if (datafmt[colnr].datatype == CS_ILLEGAL_TYPE) {
      gda_log_error(_("illegal type detected, aborting"));
      srecset->failed = TRUE;
      return;
    } else { /* well, let's allocate and fill in fielddata of column colnr */
      field = gda_server_field_new();
      if (!field) {
        gda_log_error(_("could not allocate field"));
        srecset->failed = TRUE;
        return;
      }

      gda_server_field_set_name(field, datafmt[colnr].name);
      gda_server_field_set_sql_type(field, datafmt[colnr].datatype);
      gda_server_field_set_actual_length(field,
                                         srecset->data[colnr].datalen);
      gda_server_field_set_defined_length(field, datafmt[colnr].maxlength);
      gda_server_field_set_user_data(field,
                                     (gpointer) &srecset->data[colnr]);
      gda_server_field_set_scale(field, datafmt[colnr].scale);
      // FIXME:
      // datafmt[colnr].precision: no corresponding function

      // and add it to the recordset
      gda_server_recordset_add_field(recset, field);
    }
  }

//  srecset->ret = ct_fetch(srecset->scmd->cmd, CS_UNUSED, CS_UNUSED, CS_NULLTERM,
//                          &srecset->rows_cnt);
//  gda_tds_field_fill_values(recset, srecset);
}

