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
// Revision 1.4  2001/07/18 23:05:43  vivien
// Ran indent -br -i8 on the files
//
// Revision 1.3  2001/04/07 08:49:34  rodrigo
// 2001-04-07  Rodrigo Moya <rodrigo@gnome-db.org>
//
//      * objects renaming (Gda_* to Gda*) to conform to the GNOME
//      naming standards
//
// Revision 1.2  2000/11/21 19:57:14  holger
// 2000-11-21 Holger Thon <holger@gidayu.max.uni-duisburg.de>
//
//      - Fixed missing parameter in gda-sybase-server
//      - Removed ct_diag/cs_diag stuff completly from tds server
//
// Revision 1.1  2000/11/04 23:42:17  holger
// 2000-11-05  Holger Thon <gnome-dms@users.sourceforge.net>
//
//   * Added gda-tds-server
//

#include "gda-tds.h"

void
gda_tds_field_fill_values (GdaServerRecordset * rec, tds_Recordset * srec)
{
	CS_INT i = 0;
	GList *entry = NULL;

//  g_return_if_fail(rec != NULL);
//  g_return_if_fail(srec != NULL);
//  g_return_if_fail(srec->data != NULL);
//  g_return_if_fail(srec->colscnt > 0);

	for (i = 0; i < srec->colscnt; i++) {
		entry = g_list_nth (gda_server_recordset_get_fields (rec), i);
		if (entry) {
			GdaServerField *field =
				(GdaServerField *) entry->data;
			if (field) {
				if (srec->data[i].data) {
					switch (gda_server_field_get_sql_type
						(field)) {
					case CS_IMAGE_TYPE:
					case CS_LONGBINARY_TYPE:
					case CS_BINARY_TYPE:
					case CS_VARBINARY_TYPE:
						gda_server_field_set_varbin
							(field,
							 srec->data[i].data,
							 srec->data[i].
							 datalen);
						break;
					case CS_LONGCHAR_TYPE:
						srec->data[i].data[srec->
								   data[i].
								   datalen] =
							'\0';
						gda_server_field_set_longvarchar
							(field,
							 srec->data[i].data);
						gda_server_field_set_actual_length
							(field,
							 srec->data[i].
							 datalen);
						break;
					case CS_CHAR_TYPE:
					case CS_VARCHAR_TYPE:
					case CS_TEXT_TYPE:
						srec->data[i].data[srec->
								   data[i].
								   datalen] =
							'\0';
						gda_server_field_set_varchar
							(field,
							 (gchar *) srec->
							 data[i].data);
						gda_server_field_set_actual_length
							(field,
							 srec->data[i].
							 datalen);
						break;
					case CS_BIT_TYPE:
						gda_server_field_set_boolean
							(field,
							 (gboolean) *
							 srec->data[i].data);
						break;
					case CS_TINYINT_TYPE:
					case CS_SMALLINT_TYPE:
						gda_server_field_set_smallint
							(field,
							 (gshort) *
							 srec->data[i].data);
						break;
					case CS_INT_TYPE:
						gda_server_field_set_integer
							(field,
							 (gint) *
							 srec->data[i].data);
						break;
					case CS_REAL_TYPE:
						gda_server_field_set_single
							(field,
							 (gdouble) *
							 srec->data[i].data);
						break;
					case CS_FLOAT_TYPE:
						gda_server_field_set_double
							(field,
							 (gdouble) *
							 srec->data[i].data);
						break;
					case CS_DATETIME_TYPE:
						gda_tds_field_set_datetime
							(field,
							 (CS_DATETIME *)
							 srec->data[i].data);
						break;
					case CS_DATETIME4_TYPE:
						gda_tds_field_set_datetime4
							(field,
							 (CS_DATETIME4 *)
							 srec->data[i].data);
						break;
					default:
						// try cs_convert data to CS_CHAR_TYPE
						gda_tds_field_set_general
							(field,
							 &srec->data[i],
							 srec->tcnc);
						break;
					}
				}
				else {
					gda_server_field_set_actual_length
						(field, 0);
				}
			}
		}
	}
}

gint
gda_tds_row_result (gboolean forward,
		    GdaServerRecordset * recset,
		    tds_Recordset * trecset, CS_COMMAND * cmd)
{
	CS_INT rows_read = 0;
	// We are are just called within internal procedures and trust
	// them giving no null pointers

	// FIXME: check if previous row fetch is implementable

	if (trecset->failed == TRUE) {
		gda_server_recordset_set_at_begin (recset, FALSE);
		gda_server_recordset_set_at_end (recset, TRUE);
		return 1;
	}

	trecset->ret =
		ct_fetch (cmd, CS_UNUSED, CS_UNUSED, CS_UNUSED, &rows_read);
	switch (trecset->ret) {
	case CS_SUCCEED:
		gda_tds_field_fill_values (recset, trecset);
		trecset->rows_cnt += rows_read;
		return 0;
		break;
	case CS_ROW_FAIL:
		gda_log_error (_("fetch error at row %d"), trecset->rows_cnt);
		trecset->rows_cnt += rows_read;
		return 0;
		break;
	case CS_END_DATA:
		gda_server_recordset_set_at_begin (recset, FALSE);
		gda_server_recordset_set_at_end (recset, TRUE);
		gda_log_message (_("CS_END_DATA on row %d; canceling"),
				 trecset->rows_cnt);
		trecset->ret = ct_cancel (NULL, cmd, CS_CANCEL_ALL);
		return 1;

		break;
	default:
		gda_log_error (_("%s: Error (%d)"), __PRETTY_FUNCTION__,
			       trecset->ret);
		gda_server_recordset_set_at_begin (recset, FALSE);
		gda_server_recordset_set_at_end (recset, TRUE);
		trecset->ret = ct_cancel (NULL, cmd, CS_CANCEL_ALL);
		return -1;

		break;
	}
}

gboolean
gda_tds_recordset_new (GdaServerRecordset * recset)
{
	tds_Recordset *trecset = NULL;

	g_return_val_if_fail (recset != NULL, FALSE);
	trecset = g_new0 (tds_Recordset, 1);
	g_return_val_if_fail (trecset != NULL, FALSE);

	trecset->colscnt = 0;
	trecset->failed = FALSE;
	trecset->tcnc = (tds_Connection *) NULL;
	trecset->tcmd = (tds_Command *) NULL;

	/* We don't know the rowcount yet, so we initialize with null pointer */
	trecset->datafmt = (CS_DATAFMT *) NULL;
	trecset->data = (tds_Field *) NULL;

	gda_server_recordset_set_user_data (recset, (gpointer) trecset);

	return TRUE;
}

gint
gda_tds_recordset_move_next (GdaServerRecordset * recset)
{
	tds_Recordset *trecset = NULL;
	CS_COMMAND *cmd = NULL;
	gint ret = 0;

	g_return_val_if_fail (recset != NULL, -1);
	trecset =
		(tds_Recordset *) gda_server_recordset_get_user_data (recset);
	g_return_val_if_fail (trecset != NULL, -1);
	g_return_val_if_fail (trecset->tcmd != NULL, -1);
	cmd = trecset->tcmd->cmd;
	g_return_val_if_fail (cmd != NULL, -1);

	switch (trecset->result_type) {
	case CS_ROW_RESULT:
	case CS_CURSOR_RESULT:
		if ((ret =
		     gda_tds_row_result (TRUE, recset, trecset, cmd)) == 0) {
			gda_server_recordset_set_at_begin (recset, FALSE);

			return 0;
		}
		else {
			gda_server_recordset_set_at_end (recset, TRUE);

			return 1;
		}
		break;
	case CS_ROW_FAIL:
		return -1;

		break;
	default:
		gda_log_message ("%s: %d", __PRETTY_FUNCTION__, trecset->ret);
		trecset->tcnc->ret = ct_cancel (NULL, cmd, CS_CANCEL_ALL);
		gda_server_recordset_set_at_begin (recset, FALSE);
		gda_server_recordset_set_at_end (recset, TRUE);
		return -1;

		break;
	}

	gda_server_recordset_set_at_begin (recset, FALSE);
	gda_server_recordset_set_at_end (recset, TRUE);
	return -1;
}

gint
gda_tds_recordset_move_prev (GdaServerRecordset * recset)
{
	g_return_val_if_fail (recset != NULL, -1);
	gda_server_recordset_set_at_begin (recset, FALSE);
	gda_server_recordset_set_at_end (recset, TRUE);

	return 1;
}

gint
gda_tds_recordset_close (GdaServerRecordset * recset)
{
	tds_Recordset *trecset = NULL;
	CS_INT colnr = 0;

	g_return_val_if_fail (recset != NULL, -1);
	trecset = gda_server_recordset_get_user_data (recset);
	g_return_val_if_fail (trecset != NULL, -1);

	if (trecset->data) {
		while (colnr < trecset->colscnt) {
			if (trecset->data[colnr].data) {
				g_free ((gpointer) trecset->data[colnr].data);
			}
			colnr++;
		}
		g_free ((gpointer) trecset->data);
	}

	if (trecset->datafmt != NULL) {
		g_free ((gpointer) trecset->datafmt);
	}

	return 0;
}

void
gda_tds_recordset_free (GdaServerRecordset * recset)
{
	tds_Recordset *trecset = NULL;
	CS_INT colnr = 0;

	g_return_if_fail (recset != NULL);
	trecset = gda_server_recordset_get_user_data (recset);
	g_return_if_fail (trecset != NULL);

	if (trecset->data) {
		while (colnr < trecset->colscnt) {
			if (trecset->data[colnr].data) {
				g_free ((gpointer) trecset->data[colnr].data);
			}
			colnr++;
		}
		g_free ((gpointer) trecset->data);
	}

	if (trecset->datafmt != NULL) {
		g_free ((gpointer) trecset->datafmt);
	}
	g_free ((gpointer) trecset);

	gda_server_recordset_set_user_data (recset, (gpointer) NULL);
}

void
gda_tds_init_recset_fields (GdaServerError * err,
			    GdaServerRecordset * recset,
			    tds_Recordset * trecset, CS_RETCODE result_type)
{
	gint colnr;
	CS_RETCODE ret = CS_SUCCEED;
	CS_DATAFMT *datafmt = NULL;

	g_return_if_fail (recset != NULL);
	g_return_if_fail (trecset != NULL);
	g_return_if_fail (trecset->tcmd != NULL);
	g_return_if_fail (trecset->tcmd->cmd != NULL);
	g_return_if_fail (trecset->tcnc != NULL);
	g_return_if_fail (trecset->tcnc->cnc != NULL);
	/* Yes we mean ==, because we don't want to overwrite any existing data */
	g_return_if_fail (trecset->data == NULL);

	trecset->ret = result_type;
	trecset->rows_cnt = 0;

	// Count columns
	if ((trecset->tcnc->ret = ct_res_info (trecset->tcmd->cmd, CS_NUMDATA,
					       &trecset->colscnt, CS_UNUSED,
					       NULL)
	    ) != CS_SUCCEED) {
		gda_log_error (_("Failed fetching # of columns"));
		gda_server_recordset_free (recset);
		recset = NULL;

		return;
#ifdef TDS_DEBUG
	}
	else {
		gda_log_message (_("Counted %d columns"), trecset->colscnt);
#endif
	}

	// allocate datafmt structure for colscnt columns
	trecset->datafmt = g_new0 (CS_DATAFMT, trecset->colscnt);
	if (!trecset->datafmt) {
		gda_log_error (_("%s could not allocate datafmt info"),
			       __PRETTY_FUNCTION__);
		gda_server_recordset_free (recset);
		return;
	}

	// allocate data structure for colscnt columns
	if (!trecset->data) {
		trecset->data = g_new0 (tds_Field, trecset->colscnt);
		if (!trecset->data) {
			gda_log_error (_("%s could not allocate data fields"),
				       __PRETTY_FUNCTION__);
			gda_server_recordset_free (recset);
			return;
		}
	}

	/* set tds's resulttype */
	trecset->result_type = result_type;
	trecset->failed = FALSE;
	datafmt = trecset->datafmt;

	for (colnr = 0; colnr < trecset->colscnt; colnr++) {
		GdaServerField *field;

		// Initialize user data fmt pointer to datafmt pointer for colnr
		trecset->data[colnr].fmt = &trecset->datafmt[colnr];
#ifdef TDS_DEBUG
		gda_log_message (_("ct_describe on col # %d"), colnr + 1);
#endif

		// get datafmt info for colnr
		ret = ct_describe (trecset->tcmd->cmd, colnr + 1,
				   &trecset->datafmt[colnr]);
		if (ret != CS_SUCCEED) {
			gda_log_error (_("ct_describe on col # %d failed"),
				       colnr + 1);
			trecset->failed = TRUE;
			return;
		}

#ifdef TDS_DEBUG
		gda_log_message (_("ct_describe(%d): '%s', %d (%s)"),
				 colnr + 1, datafmt[colnr].name,
				 datafmt[colnr].datatype,
				 tds_get_type_name (datafmt[colnr].datatype));
#endif

		// allocate enough memory for column colnr
		trecset->data[colnr].data = g_new0 (gchar,
						    trecset->datafmt[colnr].
						    maxlength + 1);
		if (!trecset->data[colnr].data) {
			gda_log_error (_("could not allocate data holder"));
			trecset->failed = TRUE;
			return;
		}

		// bind column data placeholder to column colnr
		ret = ct_bind (trecset->tcmd->cmd, (colnr + 1),
			       &datafmt[colnr], trecset->data[colnr].data,
			       &trecset->data[colnr].datalen,
			       (CS_SMALLINT *) & trecset->data[colnr].
			       indicator);
		if (ret != CS_SUCCEED) {
			gda_log_error (_
				       ("could not ct_bind data holder to column"));
			trecset->failed = TRUE;
			return;
		}

		// check if we got an illegal sql type, exit in that case
		if (datafmt[colnr].datatype == CS_ILLEGAL_TYPE) {
			gda_log_error (_("illegal type detected, aborting"));
			trecset->failed = TRUE;
			return;
		}
		else {		/* well, let's allocate and fill in fielddata of column colnr */
			field = gda_server_field_new ();
			if (!field) {
				gda_log_error (_("could not allocate field"));
				trecset->failed = TRUE;
				return;
			}

			gda_server_field_set_name (field,
						   datafmt[colnr].name);
			gda_server_field_set_sql_type (field,
						       datafmt[colnr].
						       datatype);
			gda_server_field_set_actual_length (field,
							    trecset->
							    data[colnr].
							    datalen);
			gda_server_field_set_defined_length (field,
							     datafmt[colnr].
							     maxlength);
			gda_server_field_set_user_data (field,
							(gpointer) & trecset->
							data[colnr]);
			gda_server_field_set_scale (field,
						    datafmt[colnr].scale);
			// FIXME:
			// datafmt[colnr].precision: no corresponding function

			// and add it to the recordset
			gda_server_recordset_add_field (recset, field);
		}
	}

//  trecset->ret = ct_fetch(trecset->tcmd->cmd, CS_UNUSED, CS_UNUSED, CS_NULLTERM,
//                          &trecset->rows_cnt);
//  gda_tds_field_fill_values(recset, trecset);
}
