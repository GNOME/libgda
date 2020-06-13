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
#include <glib.h>
static gboolean test_parse_iso8601_date (void);
static gboolean test_parse_iso8601_time (void);
static gboolean test_parse_iso8601_timestamp (void);
static gboolean test_date_handler (void);
static gboolean test_time_handler (void);
static gboolean test_timestamp_handler (void);

int
main (G_GNUC_UNUSED int argc, G_GNUC_UNUSED char** argv)
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
	{"0096-07-23", TRUE, 23, 7, 96},
	{"2050-12-31", TRUE, 31, 12, 2050},
	{"2050-11-31", FALSE, 0, 0, 0},
	{"1996-02-29", TRUE, 29, 2, 1996},
	{"1997-02-29", FALSE, 0, 0, 0},
	{"1900-05-22", TRUE, 22, 5, 1900},
	{"1900.05-22", FALSE, 0, 0, 0},
	{"1900-05.22", FALSE, 0, 0, 0},
	{"1900-05-22 ", FALSE, 0, 0, 0},
	{" 1900-05-22", FALSE, 0, 0, 0},
	{"1900 -05-22", FALSE, 0, 0, 0},
	{"1900- 05-22", FALSE, 0, 0, 0},
	{"1900-05 -22", FALSE, 0, 0, 0},
	{"1900-05- 22", FALSE, 0, 0, 0},
	{"0001-05-22", TRUE, 22, 5, 1},
	{"65536-05-22", FALSE, 0, 0, 0},
};

static gboolean
test_parse_iso8601_date (void)
{
	guint i;

	for (i = 0; i < G_N_ELEMENTS (datedata); i++) {
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
	GTimeSpan timezone;
} TestTime;

TestTime timedata[] = {
       {"11:22:56Z",TRUE, 11, 22, 56, 0, 0},
       {"01:22:56Z",TRUE, 1, 22, 56, 0, 0},
       {"1:22:60Z",FALSE, 1, 22, 0, 0, 0},
       {"1:60:45Z",FALSE, 1, 0, 0, 0, 0},
       {"24:23:45Z",FALSE, 0, 0, 0, 0, 0},
       {"23:59:59Z",TRUE, 23, 59, 59, 0, 0},
       {"00:00:00Z",TRUE, 0, 0, 0, 0, 0},
       {"12:01:00Z",TRUE, 12, 1, 0, 0, 0},
       {" 12:00:00Z",FALSE, 0, 0, 0, 0, 0},
       {"12 :00:00Z",FALSE, 12, 0, 0, 0, 0},
       {"12: 00:00Z",FALSE, 12, 0, 0, 0, 0},
       {"12: 00:00Z",FALSE, 12, 0, 0, 0, 0},
       {"12:1 :00Z",FALSE, 12, 1, 0, 0, 0},
       {"12:01:02Z",TRUE, 12, 1, 2, 0, 0},
       {"12:01:02.123Z",TRUE, 12, 1, 2, 123000, 0},
       {"12:01:02-02",TRUE, 12, 1, 2, 0, -2l*60*60},
       {"12:01:02+11",TRUE, 12, 1, 2, 0, 11l*60*60},
       {"12:01:02.1234+11",TRUE, 12, 1, 2, 123400, 11l*60*60},
       {"12:01:02.123456-03",TRUE, 12, 1, 2, 123456, -3l*60*60},
       {"12:01:02.123456Z", TRUE, 12, 1, 2, 123456, 0},
};

static gboolean
test_parse_iso8601_time (void)
{
	guint i;

	for (i = 0; i < G_N_ELEMENTS (timedata); i++) {
		TestTime td = timedata[i];
		GdaTime* time = NULL;
    g_print ("Time to parse: %s\n", td.in_string);
    time = gda_parse_iso8601_time (td.in_string);
    g_assert ((time != NULL) == td.exp_retval);
    if (!td.exp_retval)
      continue;
		g_print ("test_parse_iso8601_time: result for gda_parse_iso8601_time (\"%s\"):\n"
				 "   exp: HH=%d MM=%d SS=%d FF=%ld TZ=%ld\n"
				 "   got: HH=%d MM=%d SS=%d FF=%ld TZ=%ld\n",
				 td.in_string,
				 td.hour, td.minute, td.second,
				 td.fraction, td.timezone,
				 gda_time_get_hour (time), gda_time_get_minute (time), gda_time_get_second (time),
				 gda_time_get_fraction (time), gda_time_get_timezone (time));
		g_assert (gda_time_get_hour (time) == td.hour);
    g_assert (gda_time_get_minute (time) == td.minute);
    g_assert (gda_time_get_second (time) == td.second);
    g_assert (gda_time_get_fraction (time) == td.fraction);
    g_assert (gda_time_get_timezone (time) == td.timezone);
	}
	g_print ("All %d iso8601 time parsing tests passed\n", i);

	return TRUE;
}

static gboolean
test_parse_iso8601_timestamp (void)
{
	guint idate, itime;
	for (idate = 0; idate < G_N_ELEMENTS (datedata); idate++) {
		TestDate td = datedata [idate];
		for (itime = 0; itime < G_N_ELEMENTS (timedata); itime++) {
			TestTime tt = timedata[itime];
			gchar *str;
			str = g_strdup_printf ("%sT%s", td.in_string, tt.in_string);

			GDateTime* timestamp = g_date_time_new_from_iso8601 (str, NULL);
			if (timestamp == NULL && td.exp_retval && tt.exp_retval) {
				g_print ("test_parse_iso8601: Wrong result for g_date_time_new_from_iso8601(\"%s\"): for valid timestamp\n",
					 str);
				return FALSE;
			}
			if (timestamp == NULL) {
				g_free (str);
				continue;
			}

			if ((td.exp_retval &&
			     ((g_date_time_get_year (timestamp) != td.exp_year) ||
			      (g_date_time_get_month (timestamp) != (gint) td.exp_month) ||
			      (g_date_time_get_day_of_month (timestamp) != (gint) td.exp_day))) &&
			    (((g_date_time_get_hour (timestamp) != tt.hour) ||
			      (g_date_time_get_minute (timestamp) != tt.minute) ||
			      (g_date_time_get_second (timestamp) != tt.second) ||
			      (((gint) ((g_date_time_get_seconds (timestamp) - g_date_time_get_second (timestamp)) * 1000000.0))
											!= (gint) tt.fraction) ||
			      ((g_date_time_get_utc_offset (timestamp) / 1000000) != tt.timezone)))) {
				g_print ("test_parse_iso8601_timestamp: Wrong result for g_date_time_new_from_iso8601(\"%s\"):\n"
					 "   exp: DD=%d MM=%d YYYY=%d HH=%d MM=%d SS=%d FF=%ld TZ=%ld\n"
					 "   got: DD=%d MM=%d YYYY=%d HH=%d MM=%d SS=%d FF=%ld TZ=%ld\n",
					 str, td.exp_day, td.exp_month, td.exp_year,
					 tt.hour, tt.minute, tt.second, tt.fraction, tt.timezone,
					 g_date_time_get_year (timestamp), g_date_time_get_month (timestamp),
					 g_date_time_get_day_of_month (timestamp), g_date_time_get_hour (timestamp), g_date_time_get_minute (timestamp),
					 g_date_time_get_second (timestamp),
					 (glong) (g_date_time_get_seconds (timestamp) - g_date_time_get_second (timestamp)) / 1000000l,
					 g_date_time_get_utc_offset (timestamp)/1000000);
					 
				g_free (str);
				return FALSE;
			}
			g_free (str);
			g_date_time_unref (timestamp);
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

	for (i = 0; i < G_N_ELEMENTS (datedata); i++) {
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
	{"112256Z",TRUE, 11, 22, 56, 0, 0},
	{"012256Z",TRUE, 1, 22, 56, 0, 0},
//	{"012260Z",FALSE, 1, 22, 59, 0, 0}, FIXME: this is failing on GLIB 2.62, but working on above
	{"016045Z",FALSE, 1, 0, 0, 0, 0},
	{"242345Z",FALSE, 0, 0, 0, 0, 0},
	{"235959Z",TRUE, 23, 59, 59, 0, 0},
	{"000000Z",TRUE, 0, 0, 0, 0, 0},
	{"120100Z",TRUE, 12, 1, 0, 0, 0},
	{" 120000Z",FALSE, 0, 0, 0, 0, 0},
	{"12 0000Z",FALSE, 12, 0, 0, 0, 0},
	{"12 0000Z",FALSE, 12, 0, 0, 0, 0},
	{"12 0000Z",FALSE, 12, 0, 0, 0, 0},
	{"1201 00Z",FALSE, 12, 1, 0, 0, 0},
	{"120102Z",TRUE, 12, 1, 2, 0, 0},
	{"120102.123Z",TRUE, 12, 1, 2, 123000, 0},
	{"120102-02",TRUE, 12, 1, 2, 0, -2l*60*60},
	{"120102+11",TRUE, 12, 1, 2, 0, 11l*60*60},
	{"120102.1234+11",TRUE, 12, 1, 2, 123400, 11l*60*60},
	{"120102.123456-03",TRUE, 12, 1, 2, 123456, -3l*60*60},
};

static gboolean
test_time_handler (void)
{
	GdaDataHandler *dh;
	guint i, j;
	dh = gda_data_handler_get_default (GDA_TYPE_TIME);
	g_assert (dh);

	for (i = 0; i < G_N_ELEMENTS (timedata); i++) {
		TestTime td = timedata[i];
		GValue *value;
		g_print ("Time to parse: [%s]. Is Valid? %s\n", td.in_string, td.exp_retval ? "TRUE" : "FALSE");

		value = gda_data_handler_get_value_from_str (dh, td.in_string, GDA_TYPE_TIME);
		if (value != NULL)
			g_print ("Returned Value: %s\n", gda_value_stringify (value));
		g_assert ((value != NULL && td.exp_retval) || (value == NULL && !td.exp_retval));

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

	for (j = 0; j < G_N_ELEMENTS (timedata2); j++) {
		TestTime td = timedata2[j];
		GValue *value;
		g_print ("Time Simplified format, to parse: [%s]\n", td.in_string);

		value = gda_data_handler_get_value_from_str (dh, td.in_string, GDA_TYPE_TIME);
		if (value != NULL)
			g_print ("Result for gda_data_handler_get_value_from_str (\"%s\", GDA_TYPE_TIME): got %s\n",
				 td.in_string, gda_value_stringify (value));
		g_assert ((value != NULL && td.exp_retval) || (value == NULL && !td.exp_retval));

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

	for (idate = 0; idate < G_N_ELEMENTS (datedata); idate++) {
		TestDate td = datedata [idate];
		for (itime = 0; itime < G_N_ELEMENTS (timedata); itime++) {
			TestTime tt = timedata[itime];
			gchar *str;
			str = g_strdup_printf ("%sT%s", td.in_string, tt.in_string);
			g_print ("Timestamp to parse: %s\n", str);

			GValue *value;
			value = gda_data_handler_get_value_from_str (dh, str, G_TYPE_DATE_TIME);
			if (value != NULL)
				g_print ("Result with DateValid %d, TimeValid %d, Res= %s\n",
								 td.exp_retval, tt.exp_retval, gda_value_stringify (value));
			g_assert ((value != NULL && td.exp_retval && tt.exp_retval)
								|| (value == NULL && (!td.exp_retval || !tt.exp_retval)));
			if (value == NULL) {
				g_free (str);
				continue;
			}

			GDateTime *ptimestamp;
			GDateTime *timestamp;
			ptimestamp = g_value_get_boxed (value);
			if (ptimestamp != NULL) {
				timestamp = gda_date_time_copy (ptimestamp);
				gda_value_free (value);

				g_print ("test_timestamp_handler: Result for gda_data_handler_get_value_from_str ():\n"
						 "   exp: DD=%d MM=%d YYYY=%d HH=%d MM=%d SS=%d FF=%ld TZ=%ld\n"
						 "   got: DD=%d MM=%d YYYY=%d HH=%d MM=%d SS=%d FF=%ld (SF=%f) TZ=%ld\n",
						 td.exp_day, td.exp_month, td.exp_year,
						 tt.hour, tt.minute, tt.second, tt.fraction, tt.timezone,
						 g_date_time_get_day_of_month (timestamp),
						 g_date_time_get_month (timestamp),
						 g_date_time_get_year (timestamp),
					 g_date_time_get_hour (timestamp), g_date_time_get_minute (timestamp),
					 g_date_time_get_second (timestamp),
					 (glong) ((g_date_time_get_seconds (timestamp) - g_date_time_get_second (timestamp)) * 1000000.0),
					 g_date_time_get_seconds (timestamp),
					 g_date_time_get_utc_offset (timestamp)/1000000);

				g_assert (g_date_time_get_year (timestamp) == td.exp_year);
				g_assert (g_date_time_get_month (timestamp) == (gint) td.exp_month);
				g_assert (g_date_time_get_day_of_month (timestamp) == (gint) td.exp_day);
				g_assert (g_date_time_get_hour (timestamp) == tt.hour);
				g_assert (g_date_time_get_minute (timestamp) == tt.minute);
				g_assert (g_date_time_get_second (timestamp) == tt.second);
				g_assert ((gulong) ((g_date_time_get_seconds (timestamp) - g_date_time_get_second (timestamp)) * 1000000.0) == tt.fraction);
				g_assert (g_date_time_get_utc_offset (timestamp)/1000000 == tt.timezone);
				g_date_time_unref (timestamp);
			}
			g_free (str);
		}
	}

	for (idate = 0; idate < G_N_ELEMENTS (datedata); idate++) {
		TestDate td = datedata [idate];
		for (itime2 = 0; itime2 < G_N_ELEMENTS (timedata2); itime2++) {
			TestTime tt = timedata2[itime2];
			gchar *str;
			str = g_strdup_printf ("%sT%s", td.in_string, tt.in_string);
			g_print ("Time to Parse: %s\n", str);

			GValue *value;
			value = gda_data_handler_get_value_from_str (dh, str, G_TYPE_DATE_TIME);
			if (value != NULL)
				g_print ("Result to parse compact time format: %s\n", gda_value_stringify (value));
			g_assert ((value != NULL && td.exp_retval && tt.exp_retval)
								|| (value == NULL && (!td.exp_retval || !tt.exp_retval)));
			if (value == NULL) {
				g_free (str);
				continue;
			}

			GDateTime *ptimestamp;
			GDateTime *timestamp;
			ptimestamp = g_value_get_boxed (value);
			if (ptimestamp != NULL) {
				timestamp = gda_date_time_copy (ptimestamp);
				gda_value_free (value);
				if ((g_date_time_get_year (timestamp) != (gint) td.exp_year) ||
			    (g_date_time_get_month (timestamp) != (gint) td.exp_month) ||
			    (g_date_time_get_day_of_month (timestamp) != (gint) td.exp_day) ||
			    (g_date_time_get_hour (timestamp) != tt.hour) ||
			    (g_date_time_get_minute (timestamp) != tt.minute) ||
			    (g_date_time_get_second (timestamp) != (gint) tt.second) ||
			    ((gulong) ((g_date_time_get_seconds (timestamp) - g_date_time_get_second (timestamp)) * 1000000.0) != tt.fraction) ||
			    (g_date_time_get_utc_offset (timestamp)/1000000 != tt.timezone)) {
					g_print ("test_timestamp_handler: Compact Time Format: Wrong result for gda_data_handler_get_value_from_str (\"%s\", G_TYPE_DATE_TIME):\n"
						 "   exp: DD=%d MM=%d YYYY=%d HH=%d MM=%d SS=%d FF=%ld TZ=%ld\\n"
						 "   got: DD=%d MM=%d YYYY=%d HH=%d MM=%d SS=%d FF=%ld TZ=%ld\\n",
						 str, td.exp_day, td.exp_month, td.exp_year,
						 tt.hour, tt.minute, tt.second, tt.fraction, tt.timezone,
						 g_date_time_get_year (timestamp), g_date_time_get_month (timestamp),
						 g_date_time_get_day_of_month (timestamp), g_date_time_get_hour (timestamp), g_date_time_get_minute (timestamp),
						 g_date_time_get_second (timestamp),
						 (glong) ((g_date_time_get_seconds (timestamp) - g_date_time_get_second (timestamp)) * 1000000.0),
						 g_date_time_get_utc_offset (timestamp)/1000000);

					g_object_unref (dh);
					g_free (str);
					return FALSE;
				}
				g_date_time_unref (timestamp);
				g_free (str);
			} else {
				return FALSE;
			}
		}
	}
	
	g_print ("All %d GdaDataHandler (G_TYPE_DATE_TIME) parsing tests passed\n", idate * (itime + itime2));
	g_object_unref (dh);
	return TRUE;
}
