/*
 * Copyright (C) 2010 David King <davidk@openismus.com>
 * Copyright (C) 2010 - 2013 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2011 Murray Cumming <murrayc@murrayc.com>
 * Copyright (C) 2017 Daniel Espinosa <esodan@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <libgda/libgda.h>
static gboolean test_parse_iso8601_date (void);
static gboolean test_parse_iso8601_time (void);
static gboolean test_parse_iso8601_timestamp (void);
static gboolean test_date_handler (void);
static gboolean test_time_handler (void);
static gboolean test_timestamp_handler (void);

int
main (int argc, char** argv)
{
	gint nfailed = 0;

	if (! test_parse_iso8601_date ())
		nfailed++;
	if (! test_parse_iso8601_time ())
		nfailed++;
	if (! test_parse_iso8601_timestamp ())
		nfailed++;

	gda_init ();
	if (! test_date_handler ())
		nfailed++;
	if (! test_time_handler ())
		nfailed++;
	if (! test_timestamp_handler ())
		nfailed++;

	if (nfailed > 0) {
		g_print ("FAILED: %d tests failed\n", nfailed);
		return EXIT_FAILURE;
	}
	else {
		g_print ("All tests passed\n");
		return EXIT_SUCCESS;
	}
}

typedef struct {
	gchar   *in_string;
	gboolean exp_retval;
	guint    exp_day;
	guint    exp_month;
	gint    exp_year;
} TestDate;

TestDate datedata[] = {
	{"1996-11-22", TRUE, 22, 11, 1996},
	{"1996-22-23", FALSE, 0, 0, 0},
	{"96-7-23", TRUE, 23, 7, 96},
	{"2050-12-31", TRUE, 31, 12, 2050},
	{"2050-11-31", FALSE, 0, 0, 0},
	{"1996-02-29", TRUE, 29, 2, 1996},
	{"1997-02-29", FALSE, 0, 0, 0},
	{"1900-5-22", TRUE, 22, 5, 1900},
	{"1900.05-22", FALSE, 0, 0, 0},
	{"1900-05.22", FALSE, 0, 0, 0},
	{"1900-05-22 ", FALSE, 0, 0, 0},
	{" 1900-05-22", FALSE, 0, 0, 0},
	{"1900 -05-22", FALSE, 0, 0, 0},
	{"1900- 05-22", FALSE, 0, 0, 0},
	{"1900-05 -22", FALSE, 0, 0, 0},
	{"1900-05- 22", FALSE, 0, 0, 0},
	{"65535-05-22", TRUE, 22, 5, 65535},
	{"1-05-22", TRUE, 22, 5, 1},
	{"65536-05-22", FALSE, 0, 0, 0},
};

static gboolean
test_parse_iso8601_date (void)
{
	guint i;

	for (i = 0; i < sizeof (datedata) / sizeof (TestDate); i++) {
		TestDate td = datedata[i];
		GDate date;
		/*g_print ("[%s]\n", td.in_string);*/
		if (gda_parse_iso8601_date (&date, td.in_string) != td.exp_retval) {
			g_print ("Wrong result for gda_parse_iso8601_date (\"%s\"): got %s\n",
				 td.in_string, td.exp_retval ? "FALSE" : "TRUE");
			return FALSE;
		}
		if (td.exp_retval &&
		    ((g_date_get_day (&date) != td.exp_day) ||
		     (g_date_get_month (&date) != td.exp_month) ||
		     (g_date_get_year (&date) != td.exp_year))) {
			g_print ("Wrong result for gda_parse_iso8601_date (\"%s\"):\n"
				 "   exp: DD=%d MM=%d YYYY=%d\n"
				 "   got: DD=%d MM=%d YYYY=%d\n",
				 td.in_string, td.exp_day, td.exp_month, td.exp_year,
				 g_date_get_day (&date), g_date_get_month (&date),
				 g_date_get_year (&date));
			return FALSE;
		}
	}
	g_print ("All %d iso8601 date parsing tests passed\n", i);

	return TRUE;
}

typedef struct {
	gchar   *in_string;
	gboolean exp_retval;
	gushort hour;
	gushort minute;
	gushort second;
	gulong fraction;
	glong timezone;
} TestTime;

TestTime timedata[] = {
       {"11:22:56", TRUE, 11, 22, 56, 0, GDA_TIMEZONE_INVALID},
       {"1:22:56", TRUE, 1, 22, 56, 0, GDA_TIMEZONE_INVALID},
       {"1:22:60", FALSE, 1, 22, 0, 0, GDA_TIMEZONE_INVALID},
       {"1:60:45", FALSE, 1, 0, 0, 0, GDA_TIMEZONE_INVALID},
       {"24:23:45", FALSE, 0, 0, 0, 0, GDA_TIMEZONE_INVALID},
       {"23:59:59", TRUE, 23, 59, 59, 0, GDA_TIMEZONE_INVALID},
       {"0:0:00", TRUE, 0, 0, 0, 0, GDA_TIMEZONE_INVALID},
       {"12:1:0", TRUE, 12, 1, 0, 0, GDA_TIMEZONE_INVALID},
       {" 12:00:00", FALSE, 0, 0, 0, 0, GDA_TIMEZONE_INVALID},
       {"12 :00:00", FALSE, 12, 0, 0, 0, GDA_TIMEZONE_INVALID},
       {"12: 00:00", FALSE, 12, 0, 0, 0, GDA_TIMEZONE_INVALID},
       {"12: 00:00", FALSE, 12, 0, 0, 0, GDA_TIMEZONE_INVALID},
       {"12:1 :00", FALSE, 12, 1, 0, 0, GDA_TIMEZONE_INVALID},
       {"12:1:2 ", FALSE, 12, 1, 2, 0, GDA_TIMEZONE_INVALID},
       {"12:1:2.", FALSE, 12, 1, 2, 0, GDA_TIMEZONE_INVALID},
       {"12:1:2:", FALSE, 12, 1, 2, 0, GDA_TIMEZONE_INVALID},
       {"12:1:2.123", TRUE, 12, 1, 2, 123, GDA_TIMEZONE_INVALID},
       {"12:1:2-2", TRUE, 12, 1, 2, 0, -2*60*60},
       {"12:1:2+11", TRUE, 12, 1, 2, 0, 11*60*60},
       {"12:1:2.1234+11", TRUE, 12, 1, 2, 1234, 11*60*60},
       {"12:1:2.12345678-3", TRUE, 12, 1, 2, 12345678, -3*60*60},
       {"12:1:2.12345678 UTC", TRUE, 12, 1, 2, 12345678, 0},
       {"12:1:2.12345678 INVALID", FALSE, 12, 1, 2, 12345678, GDA_TIMEZONE_INVALID},
};

static gboolean
test_parse_iso8601_time (void)
{
	guint i;

	for (i = 0; i < sizeof (timedata) / sizeof (TestTime); i++) {
		TestTime td = timedata[i];
		GdaTime* time = gda_time_new ();
		/*g_print ("[%s]\n", td.in_string);*/
		if (gda_parse_iso8601_time (time, td.in_string) != td.exp_retval) {
			g_print ("Wrong result for gda_parse_iso8601_time (\"%s\"): got %s\n",
				 td.in_string, td.exp_retval ? "FALSE" : "TRUE");
			return FALSE;
		}
		if ((gda_time_get_hour (time) != td.hour) ||
		    (gda_time_get_minute (time) != td.minute) ||
		    (gda_time_get_second (time) != td.second) ||
		    (gda_time_get_fraction (time) != td.fraction) ||
		    (gda_time_get_timezone (time) != td.timezone)) {
			g_print ("Wrong result for gda_parse_iso8601_time (\"%s\"):\n"
				 "   exp: HH=%d MM=%d SS=%d FF=%ld TZ=%ld\n"
				 "   got: HH=%d MM=%d SS=%d FF=%ld TZ=%ld\n",
				 td.in_string,
				 td.hour, td.minute, td.second,
				 td.fraction, td.timezone,
				 gda_time_get_hour (time), gda_time_get_minute (time), gda_time_get_second (time),
				 gda_time_get_fraction (time), gda_time_get_timezone (time));
			return FALSE;
		}
	}
	g_print ("All %d iso8601 time parsing tests passed\n", i);

	return TRUE;
}

static gboolean
test_parse_iso8601_timestamp (void)
{
	guint idate, itime;
	for (idate = 0; idate < sizeof (datedata) / sizeof (TestTime); idate++) {
		TestDate td = datedata [idate];
		for (itime = 0; itime < sizeof (timedata) / sizeof (TestTime); itime++) {
			TestTime tt = timedata[itime];
			gchar *str;
			str = g_strdup_printf ("%s %s", td.in_string, tt.in_string);

			GdaTimestamp* timestamp = gda_timestamp_new ();
			gboolean exp_result = td.exp_retval && tt.exp_retval;
			/*g_print ("[%s]\n", str);*/
			if (gda_parse_iso8601_timestamp (timestamp, str) != exp_result) {
				g_print ("Wrong result for gda_parse_iso8601_timestamp (\"%s\"): got %s\n",
					 td.in_string, exp_result ? "FALSE" : "TRUE");
				return FALSE;
			}

			if ((td.exp_retval &&
			     ((gda_timestamp_get_year (timestamp) != td.exp_year) ||
			      (gda_timestamp_get_month (timestamp) != td.exp_month) ||
			      (gda_timestamp_get_day (timestamp) != td.exp_day))) &&
			    (((gda_timestamp_get_hour (timestamp) != tt.hour) ||
			      (gda_timestamp_get_minute (timestamp) != tt.minute) ||
			      (gda_timestamp_get_second (timestamp) != tt.second) ||
			      (gda_timestamp_get_fraction (timestamp) != tt.fraction) ||
			      (gda_timestamp_get_timezone (timestamp) != tt.timezone)))) {
				g_print ("Wrong result for gda_parse_iso8601_timestamp (\"%s\"):\n"
					 "   exp: DD=%d MM=%d YYYY=%d HH=%d MM=%d SS=%d FF=%ld TZ=%ld\n"
					 "   got: DD=%d MM=%d YYYY=%d HH=%d MM=%d SS=%d FF=%ld TZ=%ld\n",
					 str, td.exp_day, td.exp_month, td.exp_year,
					 tt.hour, tt.minute, tt.second, tt.fraction, tt.timezone,
					 gda_timestamp_get_year (timestamp), gda_timestamp_get_month (timestamp),
					 gda_timestamp_get_day (timestamp), gda_timestamp_get_hour (timestamp), gda_timestamp_get_minute (timestamp),
					 gda_timestamp_get_second (timestamp), gda_timestamp_get_fraction (timestamp),
					 gda_timestamp_get_timezone (timestamp));
					 
				g_free (str);
				return FALSE;
			}
			g_free (str);
			gda_timestamp_free (timestamp);
		}
	}
	g_print ("All %d iso8601 timestamp parsing tests passed\n", idate * itime);

	return TRUE;
}


static gboolean
test_date_handler (void)
{
	GdaDataHandler *dh;
	guint i;
	dh = gda_handler_time_new_no_locale ();
	gda_handler_time_set_str_spec (GDA_HANDLER_TIME (dh),
				       G_DATE_YEAR, G_DATE_MONTH, G_DATE_DAY, '-', FALSE);

	for (i = 0; i < sizeof (datedata) / sizeof (TestDate); i++) {
		TestDate td = datedata[i];
		GValue *value;
		/*g_print ("[%s]\n", td.in_string);*/

		value = gda_data_handler_get_value_from_str (dh, td.in_string, G_TYPE_DATE);
		if ((!value && td.exp_retval) ||
		    (value && !td.exp_retval)) {
			g_print ("Wrong result for gda_data_handler_get_value_from_str (\"%s\", G_TYPE_DATE): got %s\n",
				 td.in_string, td.exp_retval ? "FALSE" : "TRUE");
			g_object_unref (dh);
			return FALSE;
		}

		if (! td.exp_retval)
			continue;
		GDate *pdate, date;
		pdate = g_value_get_boxed (value);
		date = *pdate;
		gda_value_free (value);

		if ((g_date_get_day (&date) != td.exp_day) ||
		    (g_date_get_month (&date) != td.exp_month) ||
		    (g_date_get_year (&date) != td.exp_year)) {
			g_print ("Wrong result for gda_parse_iso8601_date (\"%s\"):\n"
				 "   exp: DD=%d MM=%d YYYY=%d\n"
				 "   got: DD=%d MM=%d YYYY=%d\n",
				 td.in_string, td.exp_day, td.exp_month, td.exp_year,
				 g_date_get_day (&date), g_date_get_month (&date),
				 g_date_get_year (&date));
			g_object_unref (dh);
			return FALSE;
		}
	}
	g_print ("All %d GdaDataHandler (G_TYPE_DATE) parsing tests passed\n", i);
	g_object_unref (dh);
	return TRUE;
}

TestTime timedata2[] = {
	{"112256", TRUE, 11, 22, 56, 0, GDA_TIMEZONE_INVALID},
	{"012256", TRUE, 1, 22, 56, 0, GDA_TIMEZONE_INVALID},
	{"012260", FALSE, 1, 22, 0, 0, GDA_TIMEZONE_INVALID},
	{"016045", FALSE, 1, 0, 0, 0, GDA_TIMEZONE_INVALID},
	{"242345", FALSE, 0, 0, 0, 0, GDA_TIMEZONE_INVALID},
	{"235959", TRUE, 23, 59, 59, 0, GDA_TIMEZONE_INVALID},
	{"000000", TRUE, 0, 0, 0, 0, GDA_TIMEZONE_INVALID},
	{"120100", TRUE, 12, 1, 0, 0, GDA_TIMEZONE_INVALID},
	{" 120000", FALSE, 0, 0, 0, 0, GDA_TIMEZONE_INVALID},
	{"12 0000", FALSE, 12, 0, 0, 0, GDA_TIMEZONE_INVALID},
	{"12 0000", FALSE, 12, 0, 0, 0, GDA_TIMEZONE_INVALID},
	{"12 0000", FALSE, 12, 0, 0, 0, GDA_TIMEZONE_INVALID},
	{"1201 00", FALSE, 12, 1, 0, 0, GDA_TIMEZONE_INVALID},
	{"120102 ", FALSE, 12, 1, 2, 0, GDA_TIMEZONE_INVALID},
	{"120102.", FALSE, 12, 1, 2, 0, GDA_TIMEZONE_INVALID},
	{"120102:", FALSE, 12, 1, 2, 0, GDA_TIMEZONE_INVALID},
	{"120102.123", TRUE, 12, 1, 2, 123, GDA_TIMEZONE_INVALID},
	{"120102-2", TRUE, 12, 1, 2, 0, -2*60*60},
	{"120102+11", TRUE, 12, 1, 2, 0, 11*60*60},
	{"120102.1234+11", TRUE, 12, 1, 2, 1234, 11*60*60},
	{"120102.12345678-3", TRUE, 12, 1, 2, 12345678, -3*60*60},
};

static gboolean
test_time_handler (void)
{
	GdaDataHandler *dh;
	guint i, j;
	dh = gda_data_handler_get_default (GDA_TYPE_TIME);
	g_assert (dh);

	for (i = 0; i < sizeof (timedata) / sizeof (TestTime); i++) {
		TestTime td = timedata[i];
		GValue *value;
		/*g_print ("[%s]\n", td.in_string);*/

		value = gda_data_handler_get_value_from_str (dh, td.in_string, GDA_TYPE_TIME);
		if ((!value && td.exp_retval) ||
		    (value && !td.exp_retval)) {
			g_print ("Wrong result for gda_data_handler_get_value_from_str (\"%s\", GDA_TYPE_TIME): got %s\n",
				 td.in_string, td.exp_retval ? "FALSE" : "TRUE");
			g_object_unref (dh);
			return FALSE;
		}

		if (! td.exp_retval)
			continue;
		const GdaTime *ptime;
		ptime = gda_value_get_time (value);

		if ((gda_time_get_hour (ptime) != td.hour) ||
		    (gda_time_get_minute (ptime) != td.minute) ||
		    (gda_time_get_second (ptime) != td.second) ||
		    (gda_time_get_fraction (ptime) != td.fraction) ||
		    (gda_time_get_timezone (ptime) != td.timezone)) {
			g_print ("Wrong result for gda_data_handler_get_value_from_str (\"%s\", GDA_TYPE_TIME):\n"
				 "   exp: HH=%d MM=%d SS=%d FF=%ld TZ=%ld\n"
				 "   got: HH=%d MM=%d SS=%d FF=%ld TZ=%ld\n",
				 td.in_string,
				 td.hour, td.minute, td.second,
				 td.fraction, td.timezone,
				 gda_time_get_hour (ptime), gda_time_get_minute (ptime), gda_time_get_second (ptime),
				 gda_time_get_fraction (ptime), gda_time_get_timezone (ptime));
			return FALSE;
		}
		gda_value_free (value);
	}

	for (j = 0; j < sizeof (timedata2) / sizeof (TestTime); j++) {
		TestTime td = timedata2[j];
		GValue *value;
		/*g_print ("[%s]\n", td.in_string);*/

		value = gda_data_handler_get_value_from_str (dh, td.in_string, GDA_TYPE_TIME);
		if ((!value && td.exp_retval) ||
		    (value && !td.exp_retval)) {
			g_print ("Wrong result for gda_data_handler_get_value_from_str (\"%s\", GDA_TYPE_TIME): got %s\n",
				 td.in_string, td.exp_retval ? "FALSE" : "TRUE");
			g_object_unref (dh);
			return FALSE;
		}

		if (! td.exp_retval)
			continue;
		const GdaTime *ptime;
		ptime = gda_value_get_time (value);

		if ((gda_time_get_hour (ptime) != td.hour) ||
		    (gda_time_get_minute (ptime) != td.minute) ||
		    (gda_time_get_second (ptime) != td.second) ||
		    (gda_time_get_fraction (ptime) != td.fraction) ||
		    (gda_time_get_timezone (ptime) != td.timezone)) {
			g_print ("Wrong result forgda_data_handler_get_value_from_str (\"%s\", GDA_TYPE_TIME):\n"
				 "   exp: HH=%d MM=%d SS=%d FF=%ld TZ=%ld\n"
				 "   got: HH=%d MM=%d SS=%d FF=%ld TZ=%ld\n",
				 td.in_string,
				 td.hour, td.minute, td.second,
				 td.fraction, td.timezone,
				 gda_time_get_hour (ptime), gda_time_get_minute (ptime), gda_time_get_second (ptime),
				 gda_time_get_fraction (ptime), gda_time_get_timezone (ptime));
			return FALSE;
		}
		gda_value_free (value);
	}

	g_print ("All %d GdaDataHandler (GDA_TYPE_TIME) parsing tests passed\n", i + j);
	g_object_unref (dh);
	return TRUE;
}

static gboolean
test_timestamp_handler (void)
{
	GdaDataHandler *dh;
	guint idate, itime, itime2;
	dh = gda_handler_time_new_no_locale ();
	gda_handler_time_set_str_spec (GDA_HANDLER_TIME (dh),
				       G_DATE_YEAR, G_DATE_MONTH, G_DATE_DAY, '-', FALSE);

	for (idate = 0; idate < sizeof (datedata) / sizeof (TestTime); idate++) {
		TestDate td = datedata [idate];
		for (itime = 0; itime < sizeof (timedata) / sizeof (TestTime); itime++) {
			TestTime tt = timedata[itime];
			gchar *str;
			str = g_strdup_printf ("%s %s", td.in_string, tt.in_string);

			GValue *value;
			gboolean exp_result = td.exp_retval && tt.exp_retval;
			/*g_print ("[%s]\n", str);*/

			value = gda_data_handler_get_value_from_str (dh, str, GDA_TYPE_TIMESTAMP);
			if ((!value && exp_result) ||
			    (value && !exp_result)) {
				g_print ("Wrong result for gda_data_handler_get_value_from_str (\"%s\", GDA_TYPE_TIMESTAMP): got %s\n",
					 str, exp_result ? "FALSE" : "TRUE");
				g_object_unref (dh);
				return FALSE;
			}

			if (! exp_result) {
				g_free (str);
				continue;
			}
			GdaTimestamp *ptimestamp;
			GdaTimestamp *timestamp;
			ptimestamp = (GdaTimestamp*) gda_value_get_timestamp (value);
			timestamp = gda_timestamp_copy (ptimestamp);
			gda_value_free (value);

			if ((td.exp_retval &&
			     ((gda_timestamp_get_year (timestamp) != td.exp_year) ||
			      (gda_timestamp_get_month (timestamp) != td.exp_month) ||
			      (gda_timestamp_get_day (timestamp) != td.exp_day))) &&
			    ((tt.exp_retval) &&
			     ((gda_timestamp_get_hour (timestamp) != tt.hour) ||
			      (gda_timestamp_get_minute (timestamp) != tt.minute) ||
			      (gda_timestamp_get_second (timestamp) != tt.second) ||
			      (gda_timestamp_get_fraction (timestamp) != tt.fraction) ||
			      (gda_timestamp_get_timezone (timestamp) != tt.timezone)))) {
				g_print ("Wrong result for gda_data_handler_get_value_from_str (\"%s\", GDA_TYPE_TIMESTAMP):\n"
					 "   exp: DD=%d MM=%d YYYY=%d HH=%d MM=%d SS=%d FF=%ld TZ=%ld\\n"
					 "   got: DD=%d MM=%d YYYY=%d HH=%d MM=%d SS=%d FF=%ld TZ=%ld\\n",
					 str, td.exp_day, td.exp_month, td.exp_year,
					 tt.hour, tt.minute, tt.second, tt.fraction, tt.timezone,
					 gda_timestamp_get_year (timestamp), gda_timestamp_get_month (timestamp), gda_timestamp_get_day (timestamp),
					 gda_timestamp_get_hour (timestamp), gda_timestamp_get_minute (timestamp),
					 gda_timestamp_get_second (timestamp), gda_timestamp_get_fraction (timestamp), gda_timestamp_get_timezone (timestamp));
					 
				g_object_unref (dh);
				g_free (str);
				return FALSE;
			}
			g_free (str);
			gda_timestamp_free (timestamp);
		}
	}

	for (idate = 0; idate < sizeof (datedata) / sizeof (TestTime); idate++) {
		TestDate td = datedata [idate];
		for (itime2 = 0; itime2 < sizeof (timedata2) / sizeof (TestTime); itime2++) {
			TestTime tt = timedata2[itime2];
			gchar *str;
			str = g_strdup_printf ("%s %s", td.in_string, tt.in_string);

			GValue *value;
			gboolean exp_result = td.exp_retval && tt.exp_retval;
			/*g_print ("[%s]\n", str);*/

			value = gda_data_handler_get_value_from_str (dh, str, GDA_TYPE_TIMESTAMP);
			if ((!value && exp_result) ||
			    (value && !exp_result)) {
				g_print ("Wrong result for gda_data_handler_get_value_from_str (\"%s\", GDA_TYPE_TIMESTAMP): got %s\n",
					 str, exp_result ? "FALSE" : "TRUE");
				g_object_unref (dh);
				return FALSE;
			}

			if (! exp_result) {
				g_free (str);
				continue;
			}
			GdaTimestamp *ptimestamp;
			GdaTimestamp *timestamp;
			ptimestamp = (GdaTimestamp*) gda_value_get_timestamp (value);
			timestamp = gda_timestamp_copy (ptimestamp);
			gda_value_free (value);

			if ((gda_timestamp_get_year (timestamp) != td.exp_year) ||
			    (gda_timestamp_get_month (timestamp) != td.exp_month) ||
			    (gda_timestamp_get_day (timestamp) != td.exp_day) ||
			    (gda_timestamp_get_hour (timestamp) != tt.hour) ||
			    (gda_timestamp_get_minute (timestamp) != tt.minute) ||
			    (gda_timestamp_get_second (timestamp) != tt.second) ||
			    (gda_timestamp_get_fraction (timestamp) != tt.fraction) ||
			    (gda_timestamp_get_timezone (timestamp) != tt.timezone)) {
				g_print ("Wrong result for gda_data_handler_get_value_from_str (\"%s\", GDA_TYPE_TIMESTAMP):\n"
					 "   exp: DD=%d MM=%d YYYY=%d HH=%d MM=%d SS=%d FF=%ld TZ=%ld\\n"
					 "   got: DD=%d MM=%d YYYY=%d HH=%d MM=%d SS=%d FF=%ld TZ=%ld\\n",
					 str, td.exp_day, td.exp_month, td.exp_year,
					 tt.hour, tt.minute, tt.second, tt.fraction, tt.timezone,
					 gda_timestamp_get_year (timestamp), gda_timestamp_get_month (timestamp),
					 gda_timestamp_get_day (timestamp), gda_timestamp_get_hour (timestamp),
					 gda_timestamp_get_minute (timestamp), gda_timestamp_get_second (timestamp),
					 gda_timestamp_get_fraction (timestamp), gda_timestamp_get_timezone (timestamp));
					 
				g_object_unref (dh);
				g_free (str);
				return FALSE;
			}
			g_free (str);
			gda_timestamp_free (timestamp);
		}
	}
	
	g_print ("All %d GdaDataHandler (GDA_TYPE_TIMESTAMP) parsing tests passed\n", idate * (itime + itime2));
	g_object_unref (dh);
	return TRUE;
}
