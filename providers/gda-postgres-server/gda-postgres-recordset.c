/* GDA Postgres provider
 * Copyright (c) 1998 by Rodrigo Moya
 * Copyright (c) 2000 by Vivien Malerba
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "gda-postgres.h"
#include <stdlib.h>
#include <time.h>

/* private functions */
/* Converting MM-DD-YYYY to struct tm 
   or 
   MM/DD/YYYY to struct tm */
static struct tm *
str_to_tmstruct_date (gchar * date)
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
	month = atoi (ptr);
	if (!(ptr = (char *) strtok (NULL, "-/."))) {
		g_free (stm);
		return NULL;	/* Error */
	}
	day = atoi (ptr);
	if (!(ptr = (char *) strtok (NULL, "-/."))) {
		g_free (stm);
		return NULL;	/* Error */
	}
	year = atoi (ptr);

	stm->tm_mday = day;
	stm->tm_mon = month - 1;
	stm->tm_year = year - 1900;

	return stm;
}

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

/* Converting HH:MM:SS to struct tm 
   or 
   HH:MM:SS+/-FZ to struct tm */
static struct tm *
str_to_tmstruct_time (gchar * time)
{
	int hh, mm, ss;
	char *ptr;
	char mtime[9];
	struct tm *stm;

	stm = (struct tm *) g_malloc (sizeof (struct tm));
	if ((time == NULL) || (*time == '\0')) {
		g_free (stm);
		return NULL;
	}

	strncpy (mtime, time, 8);
	mtime[8] = '\0';
	ptr = (char *) strtok (mtime, "-/.:");
	hh = atoi (ptr);
	if (!(ptr = (char *) strtok (NULL, "-/.:"))) {
		g_free (stm);
		return NULL;	/* Error */
	}
	mm = atoi (ptr);
	if (!(ptr = (char *) strtok (NULL, "-/.:"))) {
		g_free (stm);
		return NULL;	/* Error */
	}
	ss = atoi (ptr);

	if ((hh > 23) || (mm > 60) || (ss > 60)) {
		g_free (stm);
		return NULL;	/* Error */
	}

	stm->tm_hour = hh;
	stm->tm_min = mm;
	stm->tm_sec = ss;

	return stm;
}

/* converts a abstime like '01/26/2000 15:42:32.00 CET' to a struct tm */
static struct tm *
str_to_tmstruct_abstime (gchar * time)
{
	int hh, mm, ss, frac;
	int day, month, year;
	char *ptr, *ptr1, *ptr2;
	char mtime[27];
	struct tm *stm;

	if ((time == NULL) || (*time == '\0'))
		return NULL;

	strncpy (mtime, time, 26);
	mtime[26] = '\0';
	/* warning: do all the fractionning here because str_to_tmstruct_*
	   functions use strtok as well! */
	ptr1 = (char *) strtok (mtime, " ");	/* the complete date */
	if (!(ptr2 = (char *) strtok (NULL, ".")))	/* the complete time */
		return NULL;
	if (!(ptr = (char *) strtok (NULL, " ")))	/* the fraction */
		return NULL;
	frac = atoi (ptr);


	stm = str_to_tmstruct_date (ptr1);
	if (!stm)
		return NULL;
	year = stm->tm_year;
	month = stm->tm_mon;
	day = stm->tm_mday;
	g_free (stm);

	stm = str_to_tmstruct_time (ptr2);
	if (!stm)
		return NULL;
	hh = stm->tm_hour;
	mm = stm->tm_min;
	ss = stm->tm_sec;
	g_free (stm);

	stm = (struct tm *) g_malloc (sizeof (struct tm));
	stm->tm_year = year;
	stm->tm_mon = month;
	stm->tm_mday = day;
	stm->tm_hour = hh;
	stm->tm_min = mm;
	stm->tm_sec = ss;
	stm->tm_isdst = frac;	/* use this field for the fraction! */

	return stm;
}

/* converts a abstime like '2000-05-12 16:43:29+02' to a struct tm */
static struct tm *
str_to_tmstruct_timestamp (gchar * time)
{
	int hh, mm, ss;
	int day, month, year;
	char *ptr1, *ptr2;
	char mtime[23];
	struct tm *stm;

	if ((time == NULL) || (*time == '\0'))
		return NULL;

	strncpy (mtime, time, 22);
	mtime[22] = '\0';
	/* warning: do all the fractionning here because str_to_tmstruct_*
	   functions use strtok as well! */
	ptr1 = (char *) strtok (mtime, " ");	/* the complete date */
	if (!(ptr2 = (char *) strtok (NULL, ".+-")))	/* the complete time */
		return NULL;

	stm = str_to_tmstruct_date2 (ptr1);
	if (!stm)
		return NULL;
	year = stm->tm_year;
	month = stm->tm_mon;
	day = stm->tm_mday;
	g_free (stm);

	stm = str_to_tmstruct_time (ptr2);
	if (!stm)
		return NULL;
	hh = stm->tm_hour;
	mm = stm->tm_min;
	ss = stm->tm_sec;
	g_free (stm);

	stm = (struct tm *) g_malloc (sizeof (struct tm));
	stm->tm_year = year;
	stm->tm_mon = month;
	stm->tm_mday = day;
	stm->tm_hour = hh;
	stm->tm_min = mm;
	stm->tm_sec = ss;

	return stm;
}

static struct tm *
str_to_tmstruct_tinterval_left (gchar * time)
{
	return NULL;
}

static struct tm *
str_to_tmstruct_tinterval_right (gchar * time)
{
	return NULL;
}

/*
 * takes the OID of the Postgres Type and returns the corresponding GdaType.
 * The returned string is allocated and needs to be freed.
 */
gchar *
replace_PROV_TYPES_with_gdatype (POSTGRES_Recordset * recset, gchar * oid)
{
	gchar *str;
	gint sqltype;

	sqltype = atoi (oid);
	str = g_strdup_printf ("%d",
			       gda_postgres_connection_get_gda_type_psql
			       (recset->cnc, sqltype));
	return str;
}


/*
 * takes the table name and returns the SQL statement used to create the
 * table.
 * The returned string is allocated and needs to be freed.
 */
gchar *
replace_TABLE_NAME_with_SQL (POSTGRES_Recordset * recset, gchar * table)
{
	GString *str;
	gchar *retval, *strptr;
	gboolean begin;
	PGresult *res, *res2;
	guint i, nrows;

	str = g_string_new ("CREATE TABLE ");
	g_string_sprintfa (str, "%s (", table);
	begin = TRUE;
	retval = g_strdup_printf
		("SELECT a.attname, a.attnum, t.typname, a.attlen, "
		 "a.atttypmod, a.attnotnull, a.atthasdef "
		 "FROM pg_class c, pg_attribute a , "
		 "pg_type t WHERE c.relname = '%s' and "
		 "a.attnum > 0 AND a.attrelid = c.oid and "
		 "a.atttypid = t.oid ORDER BY attnum", table);
	res = PQexec (recset->cnc->pq_conn, retval);
	g_free (retval);
	if (!res || (PQresultStatus (res) != PGRES_TUPLES_OK)) {
		if (res)
			PQclear (res);
		return NULL;
	}
	nrows = PQntuples (res);

	/* going through all the fields */
	for (i = 0; i < nrows; i++) {
		if (begin)
			begin = FALSE;
		else
			g_string_append (str, ", ");
		strptr = PQgetvalue (res, i, 2);
		g_string_sprintfa (str, "%s %s", PQgetvalue (res, i, 0),
				   strptr);
		if (!strcmp (strptr, "varchar") || !strcmp (strptr, "bpchar"))
			g_string_sprintfa (str, "(%d)",	/*4=sizeof(int32), VARHDRSZ in pg */
					   atoi (PQgetvalue (res, i, 4)) - 4);

		/* default value */
		if (*PQgetvalue (res, i, 6) == 't') {
			retval = g_strdup_printf
				("SELECT d.adsrc FROM pg_attrdef d, "
				 "pg_class c, pg_attribute a WHERE "
				 "c.relname ='%s' and c.oid = d.adrelid "
				 "and d.adnum = a.attnum and a.attname='%s' "
				 "and a.attrelid = c.oid", table,
				 PQgetvalue (res, i, 0));
			res2 = PQexec (recset->cnc->pq_conn, retval);
			g_free (retval);
			if (!res2
			    || (PQresultStatus (res2) != PGRES_TUPLES_OK)) {
				if (res2)
					PQclear (res2);
				PQclear (res);
				return NULL;
			}
			if (PQntuples (res2) > 0)
				g_string_sprintfa (str, " DEFAULT %s",
						   PQgetvalue (res2, 0, 0));
			else {
				PQclear (res2);
				PQclear (res);
				return NULL;
			}
			PQclear (res2);
		}

		/* Nullable */
		if (*PQgetvalue (res, i, 5) == 't')
			g_string_append (str, " NOT NULL");
	}
	PQclear (res);
	g_string_append (str, ")");

	/* final */
	retval = str->str;
	g_string_free (str, FALSE);
	return retval;
}

/*
 * takes the sequence name and returns the SQL statement used to create the
 * sequence.
 * The returned string is allocated and needs to be freed.
 */
gchar *
replace_SEQUENCE_NAME_with_SQL (POSTGRES_Recordset * recset, gchar * seq)
{
	GString *str;
	gchar *retval;
	gboolean begin;
	PGresult *res;

	str = g_string_new ("CREATE SEQUENCE ");
	g_string_sprintfa (str, "%s ", seq);
	begin = TRUE;
	retval = g_strdup_printf ("SELECT increment_by, min_value, "
				  "max_value, last_value, cache_value, is_cycled "
				  "FROM %s", seq);
	res = PQexec (recset->cnc->pq_conn, retval);
	g_free (retval);
	if (!res || (PQresultStatus (res) != PGRES_TUPLES_OK) ||
	    (PQntuples (res) < 1)) {
		if (res)
			PQclear (res);
		return NULL;
	}

	/* going through all the fields */
	g_string_sprintfa (str, "increment %s ", PQgetvalue (res, 0, 0));
	g_string_sprintfa (str, "minvalue %s ", PQgetvalue (res, 0, 1));
	g_string_sprintfa (str, "maxvalue %s ", PQgetvalue (res, 0, 2));
	g_string_sprintfa (str, "start %s ", PQgetvalue (res, 0, 3));
	g_string_sprintfa (str, "cache %s ", PQgetvalue (res, 0, 4));
	if (*PQgetvalue (res, 0, 5) == 't')
		g_string_append (str, "cycle");
	PQclear (res);

	/* final */
	retval = str->str;
	g_string_free (str, FALSE);
	return retval;
}

gchar *
replace_FUNCTION_OID_with_SQL (POSTGRES_Recordset * recset, gchar * fnoid)
{
	gchar *str;
	/* FIXME: for the given OID,
	 *  do a "select p.prosrc, l.lanname from pg_proc p, pg_language l "
	 *       "where p.oid=%s and l.oid=p.prolang;", fnoid
	 * if the second col is "internal", return NULL
	 * otherwise return the first col
	 */

	/* Warning: there is a difference in the way the IN types are returned in Postgres 6.5.x and 7.0.x,
	   see gda-postgres-connection.c for explanations */
	str = g_strdup_printf ("NYI(%s)", fnoid);
	return str;
}

gchar *
replace_AGGREGATE_OID_with_SQL (POSTGRES_Recordset * recset, gchar * oid)
{
	/* FIXME: find something like for the functions... */
	/* Note: there is no difference from Postgres versions */
	gchar *str;

	str = g_strdup_printf ("NYI(%s)", oid);
	return str;
}

gchar *
replace_TABLE_FIELD_with_length (POSTGRES_Recordset * recset, gchar * tf)
{
	/* the format is x/y */
	gchar *retval, *buf, *ptr;
	gint i = -1, j = -1;

	buf = g_strdup (tf);
	ptr = strtok (buf, "/");
	if (ptr)
		i = atoi (ptr);
	ptr = strtok (NULL, " ");
	if (ptr)
		j = atoi (ptr);
	g_free (buf);
	if (j > i)
		i = j - 4;	/* 4 is the sizeof(int32) under postgres */
	retval = g_strdup_printf ("%d", i);

	return retval;
}

gchar *
replace_TABLE_FIELD_with_defaultval (POSTGRES_Recordset * recset, gchar * tf)
{
	gchar *retval, *buf;
	gchar *table, *field;
	PGresult *res;

	buf = g_strdup (tf);
	table = strtok (buf, " ");
	field = strtok (NULL, " ");
	if (table && field) {
		retval = g_strdup_printf
			("SELECT d.adsrc FROM pg_attrdef d, pg_class c, "
			 "pg_attribute a WHERE c.relname = '%s' and "
			 "c.oid = d.adrelid and d.adnum = a.attnum and "
			 "a.attname='%s' and a.attrelid = c.oid", table,
			 field);
		g_free (buf);
		res = PQexec (recset->cnc->pq_conn, retval);
		g_free (retval);
		if (res && (PQresultStatus (res) == PGRES_TUPLES_OK)) {
			if (PQntuples (res) > 0)
				retval = g_strdup (PQgetvalue (res, 0, 0));
			else
				retval = NULL;
			PQclear (res);
			return retval;
		}
		else {
			if (res)
				PQclear (res);
			return NULL;
		}
	}

	g_free (buf);
	return NULL;
}

/* creates a GSList with each node pointing to a string which appears
   in the tabular (space separated values).
   the created list will have to be freed, as well as its strings contents.
   The input is a string like:
    -> "12 56" and returns "12"--"56"
    -> "12 56 0 0 0" and returns "12"--"56"
*/
GSList *
convert_tabular_to_list (gchar * tab)
{
	GSList *list;
	gchar *wtab, *ptr, *ptr2;

	list = NULL;
	wtab = g_strdup (tab);
	ptr = strtok (wtab, " ");
	while (ptr) {
		if (*ptr != '0') {
			ptr2 = g_strdup (ptr);
			list = g_slist_append (list, ptr2);
		}
		ptr = strtok (NULL, " ");
	}
	g_free (wtab);

	return list;
}


gchar *
replace_TABLE_FIELD_with_iskey (POSTGRES_Recordset * recset, gchar * tf)
{
	gchar *retval, *buf;
	gchar *table, *field;
	gint pof;		/* position of field */
	PGresult *res;

	buf = g_strdup (tf);
	table = strtok (buf, " ");
	field = strtok (NULL, " ");
	if (table && field) {
		pof = atoi (field);
		retval = g_strdup_printf
			("SELECT indkey, indisunique FROM pg_index "
			 "WHERE indrelid=pg_class.oid AND "
			 "pg_class.relname='%s'", table);

		res = PQexec (recset->cnc->pq_conn, retval);
		g_free (retval);
		if (res && (PQresultStatus (res) == PGRES_TUPLES_OK)) {
			gint i, j = 0;
			GSList *list;

			i = PQntuples (res);
			retval = NULL;
			while ((j < i) && !retval) {
				list = convert_tabular_to_list (PQgetvalue
								(res, j, 0));
				while (list) {
					if (pof ==
					    atoi ((gchar *) (list->data)))
						retval = g_strdup ("t");
					if (list->data)
						g_free (list->data);
					list = g_slist_remove_link (list,
								    list);
				}
				j++;
			}
			if (!retval)
				retval = g_strdup ("f");
			PQclear (res);
		}
		else {
			if (res)
				PQclear (res);
			retval = g_strdup ("f");
		}
	}
	else
		retval = g_strdup ("f");

	g_free (buf);
	return retval;
}


static void
fill_field_values (GdaServerRecordset * recset, POSTGRES_Recordset * prc)
{
	register gint cnt = 0;
	gulong row = prc->pos;
	GList *node;
	GSList *repl_list;
	POSTGRES_Recordset_Replacement *repl;
	guint valtype = GNOME_Database_TypeUnknown;
	/* need to know about the postgres version. */
	POSTGRES_Connection *cnc;

	/* check parameters */
	g_return_if_fail (recset != NULL);
	g_return_if_fail ((prc->pq_data != NULL) || (prc->btin_res != NULL));

	cnc = (POSTGRES_Connection *)
		gda_server_connection_get_user_data (recset->cnc);
	g_return_if_fail (cnc != NULL);

	/* traverse all recordset values */
	node = g_list_first (recset->fields);

	fprintf (stderr, "getting data from row %ld\n", row);
	while (node != NULL) {
		GdaField *field = (GdaField *) node->data;
		struct tm *stm;

		if (field != 0) {
			/*
			 * field value (GNOME_Database_Value)
			 */
			gchar *native_value = NULL, *tmpstr;

			/* is there a replacement for that col? */
			repl_list = prc->replacements;
			repl = NULL;
			while (repl_list && !repl) {
				if (((POSTGRES_Recordset_Replacement
				      *) (repl_list->data))
				    ->colnum == cnt)
					repl = (POSTGRES_Recordset_Replacement
						*)
						(repl_list->data);
				else
					repl_list = g_slist_next (repl_list);
			}

			if (prc->cnc) {
				if (repl)
					valtype = repl->newtype;
				else
					valtype =
						gda_postgres_connection_get_gda_type_psql
						(prc->cnc, field->sql_type);
			}

			/* get field's value from server */
			if (prc->pq_data) {
				if (PQgetisnull (prc->pq_data, row, cnt))
					tmpstr = NULL;
				else
					tmpstr = PQgetvalue (prc->pq_data,
							     row, cnt);
			}
			else
				tmpstr = GdaBuiltin_Result_get_value (prc->
								      btin_res,
								      row,
								      cnt);
			if (tmpstr) {
				if (repl)
					native_value =
						(*repl->trans_func) (prc,
								     tmpstr);
				else
					native_value = g_strdup (tmpstr);
			}
			else
				native_value = NULL;

			switch (valtype) {
			case GNOME_Database_TypeBoolean:
				if (native_value) {
					if (*native_value == 't')
						gda_field_set_boolean_value
							(field, TRUE);
					else
						gda_field_set_boolean_value
							(field, FALSE);
				}
				else {
					gda_field_set_boolean_value (field,
								      FALSE);
					/* tell it the bool value is not valid */
					gda_field_set_actual_size (field, 0);
				}
				break;

			case GNOME_Database_TypeDbDate:
				/* the date is DD-MM-YYYY, as set at connection opening */
				stm = NULL;
				if (native_value) {
					stm = str_to_tmstruct_date
						(native_value);
					if (stm) {
						GDate *date;

						date = g_date_new_dmy (stm->
								       tm_mday,
								       stm->
								       tm_mon
								       + 1,
								       stm->
								       tm_year
								       +
								       1900);
						gda_field_set_date_value (field, date);
						g_date_free (date);
					}
					else
						gda_field_set_date_value (field, NULL);
				}
				if (!native_value || !stm)
					gda_field_set_date_value (field, NULL);

				if (stm)
					g_free (stm);
				break;

			case GNOME_Database_TypeTime:
				stm = NULL;
				if (native_value) {
					GTime mtime;
					stm = str_to_tmstruct_time
						(native_value);
					if (stm) {
						mtime = mktime (stm);
						gda_field_set_time_value (field, mtime);
					}
					else
						gda_field_set_time_value (field, 0);
				}
				if (!native_value || !stm)
					gda_field_set_time_value (field, 0);


				if (stm)
					g_free (stm);
				break;

			case GNOME_Database_TypeTimestamp:	/* FIXME */
				/* binded to datetime, timestamp, abstime, interval/timespan, 
				   tinterval, reltime */

				/* We don't use gda_server_field_set_timestamp
				   ((GdaServerField *field, GDate *dat, GTime tim); 
				   we use direct modifications in memory.
				 */

				/* the output format for abstime and datetime will be
				   for ex: 05/27/2000 15:26:36.00 CET
				   and the one of timestamp for ex:
				   2000-05-12 16:43:29+02
				   because the DATESTYLE is set in gda-postgres-connection.c
				   to SQL with US (NonEuropean) conventions. */

				gda_field_set_timestamp_value (field, 0);
				if (!native_value) {
					gda_field_set_actual_size (field, 0);
					break;
				}
				field->actual_length =
					sizeof (GNOME_Database_Timestamp);

				if (((field->sql_type ==
				      gda_postgres_connection_get_sql_type
				      (prc->cnc, "abstime"))
				     || (field->sql_type ==
					 gda_postgres_connection_get_sql_type
					 (prc->cnc, "datetime")))
				    || ((cnc->version > 7.0)
					&& (field->sql_type ==
					    gda_postgres_connection_get_sql_type
					    (prc->cnc, "timestamp")))) {
					stm = str_to_tmstruct_abstime
						(native_value);

					if (!stm)
						break;
					field->value->_u.dbtstamp.year =
						stm->tm_year + 1900;
					field->value->_u.dbtstamp.month =
						stm->tm_mon + 1;
					field->value->_u.dbtstamp.day =
						stm->tm_mday;
					field->value->_u.dbtstamp.hour =
						stm->tm_hour;
					field->value->_u.dbtstamp.minute =
						stm->tm_min;
					field->value->_u.dbtstamp.second =
						stm->tm_sec;
					field->value->_u.dbtstamp.fraction = stm->tm_isdst;	/*! */
					g_free (stm);
					break;
				}
				if ((cnc->version < 7.0) &&
				    (field->sql_type ==
				     gda_postgres_connection_get_sql_type
				     (prc->cnc, "timestamp"))) {
					stm = str_to_tmstruct_timestamp
						(native_value);

					if (!stm)
						break;
					field->value->_u.dbtstamp.year =
						stm->tm_year + 1900;
					field->value->_u.dbtstamp.month =
						stm->tm_mon + 1;
					field->value->_u.dbtstamp.day =
						stm->tm_mday;
					field->value->_u.dbtstamp.hour =
						stm->tm_hour;
					field->value->_u.dbtstamp.minute =
						stm->tm_min;
					field->value->_u.dbtstamp.second =
						stm->tm_sec;
					g_free (stm);
					break;
				}
				if ((field->sql_type ==
				     gda_postgres_connection_get_sql_type
				     (prc->cnc, "tinterval"))) {
					/* FIXME */
					/* use str_to_tmstruct_tinterval_left and
					   str_to_tmstruct_tinterval_right */
					field->actual_length = 0;
					break;
				}
				if (((cnc->version < 7.0) &&
				     (field->sql_type ==
				      gda_postgres_connection_get_sql_type
				      (prc->cnc, "timespan")))
				    || ((cnc->version > 7.0)
					&& (field->sql_type ==
					    gda_postgres_connection_get_sql_type
					    (prc->cnc, "interval")))
				    ||
				    ((field->sql_type ==
				      gda_postgres_connection_get_sql_type
				      (prc->cnc, "tinterval")))) {
					/* FIXME */
					/* this one will be quite complicated! */
					field->actual_length = 0;
					break;
				}

				break;

			case GNOME_Database_TypeSmallint:
				if (native_value)
					gda_server_field_set_smallint (field,
								       atoi
								       (native_value));
				else {
					gda_server_field_set_smallint (field,
								       0);
					gda_server_field_set_actual_length
						(field, 0);
				}
				break;

			case GNOME_Database_TypeInteger:
				if (native_value)
					gda_server_field_set_integer (field,
								      atol
								      (native_value));
				else {
					gda_server_field_set_integer (field,
								      0);
					gda_server_field_set_actual_length
						(field, 0);
				}
				break;

			case GNOME_Database_TypeLongvarchar:
				gda_server_field_set_longvarchar (field,
								  native_value);
				break;

			case GNOME_Database_TypeChar:
				gda_server_field_set_char (field,
							   native_value);
				break;

			default:	/* FIXME */
			case GNOME_Database_TypeVarchar:
				gda_server_field_set_varchar (field,
							      native_value);
				break;

			case GNOME_Database_TypeSingle:
				if (native_value)
					gda_server_field_set_single (field,
								     atof
								     (native_value));
				else {
					gda_server_field_set_single (field,
								     0);
					gda_server_field_set_actual_length
						(field, 0);
				}
				break;

			case GNOME_Database_TypeDouble:
				if (native_value)
					gda_server_field_set_double (field,
								     atof
								     (native_value));
				else {
					gda_server_field_set_double (field,
								     0);
					gda_server_field_set_actual_length
						(field, 0);
				}
				break;

			case GNOME_Database_TypeVarbin:
				/* FIXME:
				   the IDL is typedef sequence<octet> VarBinString;
				   and the struct:
				   typedef struct {
				   CORBA_unsigned_long _maximum,
				   _length;
				   CORBA_octet *_buffer;
				   CORBA_boolean _release;
				   } CORBA_sequence_CORBA_octet;
				 */
				gda_server_field_set_varbin (field,
							     native_value, 0);
				gda_server_field_set_actual_length (field, 0);
				break;
			}
			if (native_value)
				g_free (native_value);
			/*
			 * c_type
			 */
			field->c_type =
				gda_postgres_connection_get_c_type_psql (prc->
									 cnc,
									 field->
									 value->
									 _d);
			/* FIXME:
			   What about the nullable, precision, num_scale ? */
		}
		node = g_list_next (node);
		cnt++;
	}
}

/* create a new result set object */
gboolean
gda_postgres_recordset_new (GdaServerRecordset * recset)
{
	POSTGRES_Recordset *rc;
	POSTGRES_Connection *pc;

	pc = (POSTGRES_Connection *)
		gda_server_connection_get_user_data (recset->cnc);

	rc = g_new0 (POSTGRES_Recordset, 1);
	rc->replacements = NULL;
	rc->cnc = pc;
	gda_server_recordset_set_user_data (recset, (gpointer) rc);
	return TRUE;
}

gint
gda_postgres_recordset_move_next (GdaServerRecordset * recset)
{
	POSTGRES_Recordset *prc;
	gint ntuples;

	g_return_val_if_fail (recset != NULL, -1);
	prc = (POSTGRES_Recordset *)
		gda_server_recordset_get_user_data (recset);

	if (prc) {
		if (prc->pq_data)
			ntuples = PQntuples (prc->pq_data);
		else
			ntuples =
				GdaBuiltin_Result_get_nbtuples (prc->
								btin_res);

		/* check if we can fetch forward */
		if (prc->pos < ntuples) {
			if (prc->pq_data || prc->btin_res) {
				fill_field_values (recset, prc);
				prc->pos++;
				return (0);
			}
		}
		else {		/* end of file */
			gda_server_recordset_set_at_begin (recset, FALSE);
			gda_server_recordset_set_at_end (recset, TRUE);
			return (1);
		}
	}
	return (-1);
}


gint
gda_postgres_recordset_move_prev (GdaServerRecordset * recset)
{
	POSTGRES_Recordset *prc;

	g_return_val_if_fail (recset != NULL, -1);

	prc = (POSTGRES_Recordset *)
		gda_server_recordset_get_user_data (recset);

	if (prc) {
		if (prc->pos > 0) {
			if (prc->pq_data || prc->btin_res) {
				prc->pos--;
				fill_field_values (recset, prc);
				return (0);
			}
		}
		else {
			gda_server_recordset_set_at_begin (recset, TRUE);
			return (1);
		}
	}
	return (-1);
}


gint
gda_postgres_recordset_close (GdaServerRecordset * recset)
{
	POSTGRES_Recordset *prc;

	g_return_val_if_fail (recset != NULL, -1);

	prc = (POSTGRES_Recordset *)
		gda_server_recordset_get_user_data (recset);
	if (prc) {
		/* free associated command object */
		if (prc->pq_data != NULL) {
			PQclear (prc->pq_data);
			prc->pq_data = 0;
		}

		/* if there is a builtin result, free it */
		if (prc->btin_res)
			GdaBuiltin_Result_free (prc->btin_res);

		/* free the list of replacements */
		while (prc->replacements) {
			g_free (prc->replacements->data);
			prc->replacements =
				g_slist_remove_link (prc->replacements,
						     prc->replacements);
		}
		return (0);
	}
	return -1;
}


void
gda_postgres_recordset_free (GdaServerRecordset * recset)
{
	POSTGRES_Recordset *prc;

	g_return_if_fail (recset != NULL);
	prc = (POSTGRES_Recordset *)
		gda_server_recordset_get_user_data (recset);
	gda_postgres_recordset_close (recset);	/* just in case */
	if (prc)
		g_free ((gpointer) prc);
}
