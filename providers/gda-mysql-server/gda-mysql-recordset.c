/* GNOME DB MySQL Provider
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
#include <time.h>

/* private functions */
/* Converting YYYY-MM-DD to struct tm */
static struct tm *
str_to_tmstruct_date2 (gchar * date)
{
	int day, month, year;
	char *ptr;
	char mdate[11];
	struct tm *stm;

	stm = (struct tm *) g_malloc (sizeof (struct tm));
	if ((date == NULL) || (*date == '\0')) {
		g_free (stm);
		return NULL;
	}

	strncpy (mdate, date, 10);
	mdate[10] = '\0';
	ptr = (char *) strtok (mdate, "-/.");
	year = atoi (ptr);
	if (!(ptr = (char *) strtok (NULL, "-/."))) {
		g_free (stm);
		return NULL;	/* Error */
	}
	month = atoi (ptr);
	if (!(ptr = (char *) strtok (NULL, "-/."))) {
		g_free (stm);
		return NULL;	/* Error */
	}
	day = atoi (ptr);

	stm->tm_mday = day;
	stm->tm_mon = month - 1;
	stm->tm_year = year - 1900;

	return stm;
}

static void
fill_field_values (GdaServerRecordset * recset,
		   MYSQL_Recordset * mysql_recset)
{
	gint rowlength;
	gint fieldidx;
	GList *node;
	struct tm *stm;
	gulong row = mysql_recset->pos;

	g_return_if_fail (recset != NULL);
	g_return_if_fail (mysql_recset != NULL);

	if (mysql_recset->mysql_res) {
		/* call fix_fields callback */
		if (mysql_recset->fix_fields) {
			mysql_recset->array =
				mysql_recset->fix_fields (mysql_recset,
							  mysql_recset->
							  array);
		}
		mysql_recset->lengths =
			mysql_fetch_lengths (mysql_recset->mysql_res);
		rowlength = mysql_num_fields (mysql_recset->mysql_res);
	}
	else
		rowlength =
			GdaBuiltin_Result_get_nbfields (mysql_recset->
							btin_res);

	for (fieldidx = 0; fieldidx < rowlength; fieldidx++) {
		node = g_list_nth (gda_server_recordset_get_fields (recset),
				   fieldidx);
		if (node) {
			gchar *thevalue;

			if (mysql_recset->mysql_res)
				thevalue = mysql_recset->array[fieldidx];
			else
				thevalue =
					GdaBuiltin_Result_get_value
					(mysql_recset->btin_res, row,
					 fieldidx);
			if (thevalue) {
				GdaServerField *field =
					(GdaServerField *) node->data;
				switch (gda_server_field_get_sql_type (field)) {
				case FIELD_TYPE_TINY:
				case FIELD_TYPE_SHORT:
					gda_server_field_set_smallint (field,
								       atoi
								       (thevalue));
					break;
				case FIELD_TYPE_LONG:
					gda_server_field_set_integer (field,
								      atoi
								      (thevalue));
					break;
				case FIELD_TYPE_DECIMAL:
				case FIELD_TYPE_FLOAT:
					gda_server_field_set_single (field,
								     atof
								     (thevalue));
					break;
				case FIELD_TYPE_DOUBLE:
					gda_server_field_set_double (field,
								     atof
								     (thevalue));
					break;
				case FIELD_TYPE_DATE:
					stm = str_to_tmstruct_date2
						(thevalue);
					if (stm) {
						GDate *date;

						date = g_date_new_dmy (stm->
								       tm_mday,
								       stm->
								       tm_mon,
								       stm->
								       tm_year
								       +
								       1900);
						g_print ("Day %d, month %d, year %d\n", stm->tm_mday, stm->tm_mon, stm->tm_year + 1900);
						gda_server_field_set_date
							(field, date);
						g_date_free (date);
						g_free (stm);
					}
					else
						gda_server_field_set_date
							(field, NULL);

				case FIELD_TYPE_TINY_BLOB:
				case FIELD_TYPE_MEDIUM_BLOB:
				case FIELD_TYPE_LONG_BLOB:
				case FIELD_TYPE_BLOB:
					gda_server_field_set_varbin (field,
								     (gpointer)
								     thevalue,
								     field->
								     defined_length);
					break;
				case FIELD_TYPE_BOOL:
					if (thevalue) {
						if (*thevalue == 't')
							gda_server_field_set_boolean
								(field, TRUE);
						else
							gda_server_field_set_boolean
								(field,
								 FALSE);
					}
					else {
						gda_server_field_set_boolean
							(field, FALSE);
						/* tell it the bool value is not valid */
						gda_server_field_set_actual_length
							(field, 0);
					}
					break;
				case FIELD_TYPE_STRING:
				default:
					gda_server_field_set_varchar (field,
								      thevalue);
					break;
				}
			}
			else {	/* NULL value */

				gda_server_field_set_actual_length ((GdaServerField *) node->data, 0);
			}
		}
	}
}

gboolean
gda_mysql_recordset_new (GdaServerRecordset * recset)
{
	MYSQL_Recordset *mysql_recset;

	mysql_recset = g_new0 (MYSQL_Recordset, 1);
	gda_server_recordset_set_user_data (recset, (gpointer) mysql_recset);

	return TRUE;
}

gint
gda_mysql_recordset_move_next (GdaServerRecordset * recset)
{
	MYSQL_Recordset *mysql_recset;

	g_return_val_if_fail (recset != NULL, -1);

	mysql_recset =
		(MYSQL_Recordset *)
		gda_server_recordset_get_user_data (recset);
	if (mysql_recset) {
		if (mysql_recset->mysql_res) {
			mysql_recset->array =
				mysql_fetch_row (mysql_recset->mysql_res);
			if (mysql_recset->array) {
				fill_field_values (recset, mysql_recset);
				mysql_recset->pos++;
				return 0;
			}
			else {	/* end-of-file */

				gda_server_recordset_set_at_begin (recset,
								   FALSE);
				gda_server_recordset_set_at_end (recset,
								 TRUE);
				return 1;
			}
		}
		else {		/* from the builtin result */
			gint ntuples;
			ntuples =
				GdaBuiltin_Result_get_nbtuples (mysql_recset->
								btin_res);
			if (mysql_recset->pos < ntuples) {
				fill_field_values (recset, mysql_recset);
				mysql_recset->pos++;
				return (0);
			}
			else {
				gda_server_recordset_set_at_begin (recset,
								   FALSE);
				gda_server_recordset_set_at_end (recset,
								 TRUE);
				return (1);
			}
		}
	}
	return -1;
}

gint
gda_mysql_recordset_move_prev (GdaServerRecordset * recset)
{
	/* FIXME: user the mysql_row_seek() function */
	return -1;
}

gint
gda_mysql_recordset_close (GdaServerRecordset * recset)
{
	MYSQL_Recordset *mysql_recset;

	g_return_val_if_fail (recset != NULL, -1);

	mysql_recset =
		(MYSQL_Recordset *)
		gda_server_recordset_get_user_data (recset);
	if (mysql_recset) {
		mysql_free_result (mysql_recset->mysql_res);
		mysql_recset->mysql_res = NULL;
		return 0;
	}
	return -1;
}

void
gda_mysql_recordset_free (GdaServerRecordset * recset)
{
	MYSQL_Recordset *mysql_recset;

	g_return_if_fail (recset != NULL);

	mysql_recset =
		(MYSQL_Recordset *)
		gda_server_recordset_get_user_data (recset);
	if (mysql_recset) {
		g_free ((gpointer) mysql_recset);
		gda_server_recordset_set_user_data (recset, NULL);
	}
}
