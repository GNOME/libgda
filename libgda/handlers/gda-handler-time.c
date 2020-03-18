/*
 * Copyright (C) 2006 - 2015 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2007 Murray Cumming <murrayc@murrayc.com>
 * Copyright (C) 2009 Bas Driessen <bas.driessen@xobas.com>
 * Copyright (C) 2010 David King <davidk@openismus.com>
 * Copyright (C) 2010 Jonh Wendell <jwendell@gnome.org>
 * Copyright (C) 2011-2017 Daniel Espinosa <esodan@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#include "gda-handler-time.h"
#include <gda-util.h>
#include <string.h>
#include <ctype.h>
#include <glib/gi18n-lib.h>
#include <libgda/gda-debug-macros.h>

typedef struct _LocaleSetting {
	GDateDMY        dmy_order[3];
	gboolean        twodigit_years;
	gint            current_offset; /* 1900, 2000, etc... */
	gchar           separator;	
} LocaleSetting;

struct _GdaHandlerTime
{
	GObject   parent_instance;

	guint   nb_g_types;
	GType  *valid_g_types;

	/* for locale setting */
	LocaleSetting  *sql_locale;
	LocaleSetting  *str_locale;
};

/* General notes:
 * about months representations:
 * -----------------------------
 * GtkCalendar gets months in [0-11]
 * GDate represents months in [1-12]
 * struct tm represents months in [0-11]
 *
 * about date localization:
 * ------------------------
 * see how this aspect is handled in glib: function g_date_prepare_to_parse()
 * in file gdate.c
 */

static void   data_handler_iface_init (GdaDataHandlerInterface *iface);
static gchar *render_date_locale      (const GDate *date, LocaleSetting *locale);
static void   handler_compute_locale  (GdaHandlerTime *hdl);

G_DEFINE_TYPE_EXTENDED (GdaHandlerTime, gda_handler_time, G_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (GDA_TYPE_DATA_HANDLER, data_handler_iface_init))

/**
 * gda_handler_time_new:
 *
 * Creates a data handler for time values
 *
 * Returns: (type GdaHandlerTime) (transfer full): the new object
 */
GdaDataHandler *
gda_handler_time_new (void)
{
	GObject *obj;

	obj = g_object_new (GDA_TYPE_HANDLER_TIME, NULL);
	handler_compute_locale (GDA_HANDLER_TIME (obj));

	return (GdaDataHandler *) obj;
}

/**
 * gda_handler_time_new_no_locale:
 *
 * Creates a data handler for time values, but using the default C locale
 * instead of the current user locale.
 *
 * Returns: (type GdaHandlerTime) (transfer full): the new object
 */
GdaDataHandler *
gda_handler_time_new_no_locale (void)
{
	GObject *obj;

	obj = g_object_new (GDA_TYPE_HANDLER_TIME, NULL);

	return (GdaDataHandler *) obj;
}

/**
 * gda_handler_time_set_sql_spec:
 * @dh: a #GdaHandlerTime object
 * @first: what comes first in the date representation
 * @sec: what comes second in the date representation
 * @third: what comes third in the date representation
 * @separator: separator character used between year, month and day
 * @twodigits_years: TRUE if year part of date must be rendered on 2 digits
 *
 * Specifies the SQL output style of the @dh data handler. The general format is "FIRSTsSECsTHIRD"
 * where FIRST, SEC and THIRD are specified by @first, @sec and @trird and 's' is the separator,
 * specified by @separator.
 *
 * The default implementation is @first=G_DATE_MONTH, @sec=G_DATE_DAY and @third=G_DATE_YEAR
 * (the year is rendered on 4 digits) and the separator is '-'
 */
void
gda_handler_time_set_sql_spec  (GdaHandlerTime *dh, GDateDMY first, GDateDMY sec,
				GDateDMY third, gchar separator, gboolean twodigits_years)
{
	g_return_if_fail (GDA_IS_HANDLER_TIME (dh));
	g_return_if_fail (first != sec);
	g_return_if_fail (sec != third);
	g_return_if_fail (first != third);

	dh->sql_locale->dmy_order[0] = first;
	dh->sql_locale->dmy_order[1] = sec;
	dh->sql_locale->dmy_order[2] = third;
	dh->sql_locale->twodigit_years = twodigits_years;
	dh->sql_locale->separator = separator;
}

/**
 * gda_handler_time_set_str_spec:
 * @dh: a #GdaHandlerTime object
 * @first: what comes first in the date representation
 * @sec: what comes second in the date representation
 * @third: what comes third in the date representation
 * @separator: separator character used between year, month and day
 * @twodigits_years: TRUE if year part of date must be rendered on 2 digits
 *
 * Specifies the human readable output style of the @dh data handler.
 * The general format is "FIRSTsSECsTHIRD"
 * where FIRST, SEC and THIRD are specified by @first, @sec and @trird and 's' is the separator,
 * specified by @separator.
 *
 * The default implementation depends on the current locale, except if @dh was created
 * using gda_handler_time_new_no_locale().
 *
 * Since: 4.2.1
 */
void
gda_handler_time_set_str_spec  (GdaHandlerTime *dh, GDateDMY first, GDateDMY sec,
				GDateDMY third, gchar separator, gboolean twodigits_years)
{
	g_return_if_fail (GDA_IS_HANDLER_TIME (dh));
	g_return_if_fail (first != sec);
	g_return_if_fail (sec != third);
	g_return_if_fail (first != third);

	dh->str_locale->dmy_order[0] = first;
	dh->str_locale->dmy_order[1] = sec;
	dh->str_locale->dmy_order[2] = third;
	dh->str_locale->twodigit_years = twodigits_years;
	dh->str_locale->separator = separator;
}

static void
handler_compute_locale (GdaHandlerTime *hdl)
{
	GDate *date;
	gchar buf[128], *ptr, *numstart;
	gint nums[3];
	gboolean error = FALSE;

	date = g_date_new_dmy (4, 7, 1976); /* Same date used by GLib */
	g_date_strftime (buf, 127, "%x", date);
	g_date_free (date);

	/* 1st number */
	ptr = buf;
	numstart = ptr;
	while (*ptr && g_ascii_isdigit (*ptr))
		ptr++;
	if (*ptr) {
		hdl->str_locale->separator = *ptr;
		*ptr = 0;
		nums[0] = atoi (numstart); /* Flawfinder: ignore */
	}
	else
		error = TRUE;

	/* 2nd number */
	if (!error) {
		ptr++;
		numstart = ptr;
		while (*ptr && g_ascii_isdigit (*ptr))
			ptr++;
		if (*ptr) {
			*ptr = 0;
			nums[1] = atoi (numstart); /* Flawfinder: ignore */
		}
		else
			error = TRUE;
	}

	/* 3rd number */
	if (!error) {
		ptr++;
		numstart = ptr;
		while (*ptr && g_ascii_isdigit (*ptr))
			ptr++;
		*ptr = 0;
		nums[2] = atoi (numstart); /* Flawfinder: ignore */
	}
	
	/* computations */
	if (!error) {
#ifdef GDA_DEBUG_NO
		gchar *strings[3];
#endif
		gint i;
		time_t now;
		struct tm *now_tm;

		for (i=0; i < 3; i++) {
			switch (nums[i]) {
			case 7:
				hdl->str_locale->dmy_order[i] = G_DATE_MONTH;
				break;
			case 4:
				hdl->str_locale->dmy_order[i] = G_DATE_DAY;
				break;
			case 76:
				hdl->str_locale->twodigit_years = TRUE;
				hdl->str_locale->dmy_order[i] = G_DATE_YEAR;
                                break;
			case 1976:
				hdl->str_locale->dmy_order[i] = G_DATE_YEAR;
				break;
			default:
				break;
			}
		}
		
		now = time (NULL);
#ifdef HAVE_LOCALTIME_R
		struct tm tmpstm;
		now_tm = localtime_r (&now, &tmpstm);
#elif HAVE_LOCALTIME_S
		struct tm tmpstm;
		g_assert (localtime_s (&tmpstm, &now) == 0);
		now_tm = &tmpstm;
#else
                now_tm = localtime (&now);
#endif
		hdl->str_locale->current_offset = ((now_tm->tm_year + 1900) / 100) * 100;

#ifdef GDA_DEBUG_NO		
		for (i=0; i<3; i++) {
			switch (hdl->str_locale->dmy_order[i]) {
			case G_DATE_MONTH:
				strings[i] = "Month";
				break;
			case G_DATE_YEAR:
				strings[i] = "Year";
				break;
			case G_DATE_DAY:
				strings[i] = "Day";
				break;
			default:
				strings[i] = NULL;
				break;
			}
		}
		g_print ("GdaHandlerTime %p\n", hdl);
		g_print ("\tlocale order = %s %s %s, separator = %c\n", 
			 strings[0], strings[1], strings[2], hdl->str_locale->separator);
		if (hdl->str_locale->twodigit_years)
			g_print ("\tlocale has 2 digits year, using %d as offset\n", hdl->str_locale->current_offset);
		else
			g_print ("\tlocale has 4 digits year\n");
#endif
	}
	else {
		TO_IMPLEMENT;
	}
}

/**
 * gda_handler_time_get_no_locale_str_from_value:
 * @dh: a #GdaHandlerTime object
 * @value: a #GValue value
 *
 * Returns: a new string representing @value without taking the current
 * locale into account (i.e. in the "C" locale)
 */
gchar *
gda_handler_time_get_no_locale_str_from_value (GdaHandlerTime *dh, const GValue *value)
{
	gchar *retval = NULL, *str;
	GType type;

	g_return_val_if_fail (GDA_IS_HANDLER_TIME (dh), NULL);
	type = G_VALUE_TYPE (value);

	if (type == G_TYPE_DATE) {
		const GDate *date;

		date = (GDate *) g_value_get_boxed (value);
		str = render_date_locale (date, dh->sql_locale);
		if (!str)
			retval = g_strdup ("NULL");
		else 
			retval = str;
	}
	else if (type == GDA_TYPE_TIME) {
		const GdaTime *tim;
		GString *string;
		string = g_string_new ("");
		g_string_append_c (string, '\'');
		tim = gda_value_get_time ((GValue *) value);
		g_string_append_printf (string, "%02d:%02d:%02d",
					gda_time_get_hour (tim),
					gda_time_get_minute (tim),
					gda_time_get_second (tim));
		if (gda_time_get_timezone (tim) != GDA_TIMEZONE_INVALID)
			g_string_append_printf (string, "%+02d",
						(int) gda_time_get_timezone (tim) / 3600);
		g_string_append_c (string, '\'');
		retval = g_string_free (string, FALSE);
	}
	else if (type == G_TYPE_DATE_TIME) {
		GDateTime *gdats;

		gdats = (GDateTime*) g_value_get_boxed ((GValue *) value);
		if (gdats != NULL)
			retval = g_date_time_format (gdats, "%FT%H:%M:%S%:::z");
		else
			retval = g_strdup ("NULL");
	}
	else if (type == G_TYPE_DATE_TIME) {
		GDateTime *ts;
		GDate *vdate;

		ts = g_value_get_boxed ((GValue *) value);
		if (ts) {
			gint y, m, d;
			g_date_time_get_ymd (ts, &y, &m, &d);
			vdate = g_date_new_dmy (d, m, y);
			str = render_date_locale (vdate, dh->sql_locale);
			g_date_free (vdate);

			if (str) {
				GString *string;
				string = g_string_new ("");
				g_string_append_printf (string, "%02u:%02u:%02u",
							g_date_time_get_hour (ts),
							g_date_time_get_minute (ts),
							g_date_time_get_second (ts));
				if (g_date_time_get_microsecond (ts) != 0)
					g_string_append_printf (string, ".%d", g_date_time_get_microsecond (ts));

				GTimeSpan span;
				span = g_date_time_get_utc_offset (ts);
				if (span > 0)
					g_string_append_printf (string, "+%02d",
								(int) (span / G_TIME_SPAN_HOUR));
				else
					g_string_append_printf (string, "-%02d",
								(int) (-span / G_TIME_SPAN_HOUR));

				retval = g_strdup_printf ("%s %s", str, string->str);
				g_free (str);
				g_string_free (string, TRUE);
			}
			else
				retval = g_strdup ("NULL");
		}
		else
			retval = g_strdup ("NULL");
	}
	else
		g_assert_not_reached ();

	return retval;
}

/**
 * gda_handler_time_get_format:
 * @dh: a #GdaHandlerTime object
 * @type: the type of data being handled
 *
 * Get a string representing the locale-dependent way to enter a date/time/datetime, using
 * a syntax suitable for the #GdauiFormatEntry widget
 *
 * Returns: a new string
 */
gchar *
gda_handler_time_get_format (GdaHandlerTime *dh, GType type)
{
	gchar *str;
	GString *string;
	gint i;

	g_return_val_if_fail (GDA_IS_HANDLER_TIME (dh), NULL);

	string = g_string_new ("");
	if ((type == G_TYPE_DATE) || (type == G_TYPE_DATE_TIME) || (type == G_TYPE_DATE_TIME)) {
		for (i=0; i<3; i++) {
			if (i > 0)
				g_string_append_c (string, dh->str_locale->separator);
			switch (dh->str_locale->dmy_order[i]) {
			case G_DATE_DAY:
			case G_DATE_MONTH:
				g_string_append (string, "00");
				break;
			case G_DATE_YEAR:
				if (dh->str_locale->twodigit_years)
					g_string_append (string, "00");
				else
					g_string_append (string, "0000");
				break;
			default:
				g_assert_not_reached ();
				break;
			}
		}
	}
	if (type == G_TYPE_DATE_TIME)
		g_string_append_c (string, ' ');

	if ((type == GDA_TYPE_TIME) || (type == G_TYPE_DATE_TIME) || (type == G_TYPE_DATE_TIME))
		g_string_append (string, "00:00:00");

	str = string->str;
	g_string_free (string, FALSE);
	return str;
}

/**
 * gda_handler_time_get_hint:
 * @dh: a #GdaHandlerTime object
 * @type: the type of data being handled
 *
 * Get a string giving the user a hint about the locale-dependent requested format.
 *
 * Returns: a new string
 *
 * Since: 6.0
 */
gchar *
gda_handler_time_get_hint (GdaHandlerTime *dh, GType type)
{
	gchar *str;
	GString *string;
	gint i;

	g_return_val_if_fail (GDA_IS_HANDLER_TIME (dh), NULL);

	string = g_string_new ("");
	if ((type == G_TYPE_DATE) || (type == G_TYPE_DATE_TIME) || (type == G_TYPE_DATE_TIME)) {
		for (i=0; i<3; i++) {
			if (i > 0)
				g_string_append_c (string, dh->str_locale->separator);
			switch (dh->str_locale->dmy_order[i]) {
			case G_DATE_DAY:
				/* To translators: DD represents a place holder in a date string. For example in the "YYYY-MM-DD" format, one knows that she has to replace DD by a day number */
				g_string_append (string, _("DD"));
				break;
			case G_DATE_MONTH:
				/* To translators: MM represents a place holder in a date string. For example in the "YYYY-MM-DD" format, one knows that she has to replace MM by a month number */
				g_string_append (string, _("MM"));
				break;
			case G_DATE_YEAR:
				if (dh->str_locale->twodigit_years)
					/* To translators: YY represents a place holder in a date string. For example in the "YY-MM-DD" format, one knows that she has to replace YY by a year number, on 2 digits */
					g_string_append (string, _("YY"));
				else
					/* To translators: YYYY represents a place holder in a date string. For example in the "YYYY-MM-DD" format, one knows that she has to replace YYYY by a year number, on 4 digits */
					g_string_append (string, _("YYYY"));
				break;
			default:
				g_assert_not_reached ();
				break;
			}
		}
	}
	if (type == G_TYPE_DATE_TIME)
		g_string_append_c (string, ' ');

	if ((type == GDA_TYPE_TIME) || (type == G_TYPE_DATE_TIME))
		/* To translators: HH:MM:SS represents a time format. For example in the "HH:MM:SS" format, one knows that she has to replace HH by a number of hours, and so on */
		g_string_append (string, "HH:MM:SS");

	str = string->str;
	g_string_free (string, FALSE);
	return str;
}

/* Interface implementation */

/* REM: SQL date format is always returned using the MM-DD-YYY format, it's up to the
 * provider to be correctly set up to accept this format.
 */
static gchar *
gda_handler_time_get_sql_from_value (GdaDataHandler *iface, const GValue *value)
{
	g_assert (value);

	gchar *retval = NULL, *str;
	GdaHandlerTime *hdl;
	GType type;

	g_return_val_if_fail (GDA_IS_HANDLER_TIME (iface), NULL);
	hdl = (GdaHandlerTime*) (iface);
	type = G_VALUE_TYPE (value);

	if (type == G_TYPE_DATE) {
		const GDate *date;

		date = (GDate *) g_value_get_boxed (value);
		str = render_date_locale (date, hdl->sql_locale);
		if (!str)
			retval = g_strdup ("NULL");
		else {
			retval = g_strdup_printf ("'%s'", str);
			g_free (str);
		}
	}
	else if (type == GDA_TYPE_TIME) {
		GdaTime *gdat;
		gchar *str;

		gdat = (GdaTime*) g_value_get_boxed ((GValue *) value);
		str = gda_time_to_string_utc (gdat);
		retval = g_strdup_printf ("'%s'", str);
		g_free (str);
	}
	else if (g_type_is_a (type, G_TYPE_DATE_TIME)) {
		GDateTime *gdats;

		gdats = (GDateTime*) g_value_get_boxed ((GValue *) value);
		if (gdats != NULL)
			retval = g_date_time_format (gdats, "'%FT%H:%M:%S%:::z'");
		else
			retval = g_strdup ("NULL");
	}
	else {
		g_warning (_("Time data handler can create an SQL representation of a value not holding time type."));
		retval = g_strdup ("NULL");
	}

	return retval;
}

static gchar *strip_quotes (const gchar *str);
static gchar *
gda_handler_time_get_str_from_value (GdaDataHandler *iface, const GValue *value)
{
	g_assert (value);

	GdaHandlerTime *hdl;
	gchar *retval = NULL, *str;
	GType type;

	g_return_val_if_fail (GDA_IS_HANDLER_TIME (iface), NULL);
	hdl = (GdaHandlerTime*) (iface);
	type = G_VALUE_TYPE (value);

	if (type == G_TYPE_DATE) {
		const GDate *date;
		GTimeZone *tz = g_time_zone_new_utc ();
		GDateTime *dt;
		date = (GDate *) g_value_get_boxed (value);
		dt = g_date_time_new (tz, g_date_get_year (date),
		                      g_date_get_month (date),
		                      g_date_get_day (date),
		                      0, 0, 0.0);
		retval = g_date_time_format (dt, "%F");
		if (!retval)
			retval = g_strdup ("");
	}
	else if (type == GDA_TYPE_TIME) {
		str = gda_handler_time_get_sql_from_value (iface, value);
		retval = strip_quotes (str);
		g_free (str);
	}
	else if (type == G_TYPE_DATE_TIME) {
		GDateTime *gdats;

		gdats = (GDateTime*) g_value_get_boxed ((GValue *) value);
		if (gdats != NULL)
			retval = g_date_time_format (gdats, "%FT%H:%M:%S%:::z");
		else
			retval = g_strdup ("");
	}
	else if (type == G_TYPE_DATE_TIME) { // FIXME: Remove
		GDateTime *ts;
		GDate *vdate;

		ts = g_value_get_boxed ((GValue *) value);
		if (ts) {
			gint y, m, d;
			g_date_time_get_ymd (ts, &y, &m, &d);
			vdate = g_date_new_dmy (d, m, y);
			str = render_date_locale (vdate, hdl->str_locale);
			g_date_free (vdate);

			if (str) {
				GString *string;
				string = g_string_new ("");
				g_string_append_printf (string, "%02u:%02u:%02u",
							g_date_time_get_hour (ts),
							g_date_time_get_minute (ts),
							g_date_time_get_second (ts));
				if (g_date_time_get_microsecond (ts) != 0)
					g_string_append_printf (string, ".%d", g_date_time_get_microsecond (ts));

				GTimeSpan span;
				span = g_date_time_get_utc_offset (ts);
				if (span > 0)
					g_string_append_printf (string, "+%02d",
								(int) (span / G_TIME_SPAN_HOUR));
				else
					g_string_append_printf (string, "-%02d",
								(int) (-span / G_TIME_SPAN_HOUR));

				retval = g_strdup_printf ("%sT%s", str, string->str);
				g_free (str);
				g_string_free (string, TRUE);
			}
			else
				retval = g_strdup ("NULL");
		}
		else
			retval = g_strdup ("NULL");
	}
	else
		g_assert_not_reached ();
       
	return retval;
}

static gchar *
render_date_locale (const GDate *date, LocaleSetting *locale)
{
	GString *string;
	gchar *retval;
	gint i;

	if (!date)
		return NULL;

	string = g_string_new ("");
	for (i=0; i<3; i++) {
		if (i)
			g_string_append_c (string, locale->separator);

		switch (locale->dmy_order[i]) {
		case G_DATE_DAY:
			g_string_append_printf (string, "%02d", g_date_get_day (date));
			break;
		case G_DATE_MONTH:
			g_string_append_printf (string, "%02d", g_date_get_month (date));
			break;
		case G_DATE_YEAR:
			if (locale->twodigit_years) {
				GDateYear year = g_date_get_year (date);
				if ((year >= locale->current_offset) && (year < locale->current_offset + 100))
					g_string_append_printf (string, "%02d", year - locale->current_offset);
				else
					g_string_append_printf (string, "%04d", year);
			}
			else
				g_string_append_printf (string, "%04d", g_date_get_year (date));
			break;
		}
	}

	retval = string->str;
	g_string_free (string, FALSE);
	return retval;
}

static gchar *
strip_quotes (const gchar *str)
{
	gchar *ptr = g_strdup (str);
	gchar *to_free = ptr, *retval;

        if (*ptr == '\'') {
                ptr++;
        }

        if (*(ptr+(strlen (ptr)-1)) == '\'') {
                *(ptr+(strlen (ptr)-1)) = 0;
        }

	retval = g_strdup (ptr);
	g_free (to_free);
        return retval;
}

static GValue *gda_handler_time_get_value_from_locale (GdaDataHandler *iface, const gchar *sql, 
							GType type, LocaleSetting *locale);

static GValue *
gda_handler_time_get_value_from_sql (GdaDataHandler *iface, const gchar *sql, GType type)
{
	g_assert (sql);

	GdaHandlerTime *hdl;
	GValue *value = NULL;

	g_return_val_if_fail (GDA_IS_HANDLER_TIME (iface), NULL);
	hdl = (GdaHandlerTime*) (iface);

	if (*sql) {
		gint i = strlen (sql);
		if ((i>=2) && (*sql=='\'') && (sql[i-1]=='\'')) {
			gchar *str = g_strdup (sql);
			str[i-1] = 0;
			value = gda_handler_time_get_value_from_locale (iface, str+1, type, hdl->sql_locale);
			g_free (str);
		}
	}
	else
		value = gda_value_new_null ();

	return value;
}

static GValue *
gda_handler_time_get_value_from_str (GdaDataHandler *iface, const gchar *str, GType type)
{
	g_assert (str);
	GdaHandlerTime *hdl;

	g_return_val_if_fail (GDA_IS_HANDLER_TIME (iface), NULL);
	hdl = (GdaHandlerTime*) (iface);

	if (*str == '\'')
		return NULL;
	else
		return gda_handler_time_get_value_from_locale (iface, str, type, hdl->str_locale);
}



static GDateTime * make_timestamp (GdaHandlerTime *hdl,
				const gchar *value, LocaleSetting *locale);
static gboolean make_date (GdaHandlerTime *hdl, GDate *date, const gchar *value,
			   LocaleSetting *locale, const gchar **out_endptr);
static GdaTime * make_time (GdaHandlerTime *hdl, const gchar *value);
static GValue *
gda_handler_time_get_value_from_locale (GdaDataHandler *iface, const gchar *sql, 
					GType type, LocaleSetting *locale)
{
	GdaHandlerTime *hdl;
	GValue *value = NULL;

	g_return_val_if_fail (GDA_IS_HANDLER_TIME (iface), NULL);
	hdl = (GdaHandlerTime*) (iface);

	if (type == G_TYPE_DATE) {
		GDate date;
		if (make_date (hdl, &date, sql, locale, NULL)) {
			value = g_value_init (g_new0 (GValue, 1), G_TYPE_DATE);
			g_value_set_boxed (value, (gconstpointer) &date);
		}
	}
	else if (type == GDA_TYPE_TIME) {
		GdaTime* timegda =make_time (hdl, sql);
		if (timegda != NULL) {
			value = g_value_init (g_new0 (GValue, 1), GDA_TYPE_TIME);
			g_value_take_boxed (value, timegda);
		}
	}
	else if (g_type_is_a (type, G_TYPE_DATE_TIME)) {
		GDateTime* timestamp = make_timestamp (hdl, sql, locale);
		if (timestamp != NULL) {
			value = g_value_init (g_new0 (GValue, 1), G_TYPE_DATE_TIME);
			g_value_set_boxed (value, timestamp);
			g_date_time_unref (timestamp);
		}
	}
	else
		return NULL;

	return value;
}


/* Makes a GDateTime from a string like "24-12-2003T13:12:01.12+01",
 * taken from libgda/gda-value.h
 * with a modification for the date format
 */
static GDateTime*
make_timestamp (GdaHandlerTime *hdl, const gchar *value, LocaleSetting *locale)
{
  GTimeZone *default_tz = g_time_zone_new_local ();
	GDateTime* dt = g_date_time_new_from_iso8601 (value, default_tz);
  g_time_zone_unref (default_tz);
  return dt;
}

static gboolean
get_uint_from_string (const gchar *str, guint16 *out_int)
{
	long int li;
	char *endptr = NULL;
	li = strtol (str, &endptr, 10);
	if (!*endptr && (li >= 0) && (li <= G_MAXUINT16)) {
		*out_int = (guint16) li;
		return TRUE;
	}
	else {
		*out_int = 0;
		return FALSE;
	}
}

/* Makes a GDate from a string like "24-12-2003"
 * If @out_endptr is %NULL, then all the @value has to be consumed and there must not
 * be any character left. If it's not %NULL, then it will point on the first unused character
 * of @value.
 */
static gboolean
make_date (G_GNUC_UNUSED GdaHandlerTime *hdl, GDate *date, const gchar *value,
	   LocaleSetting *locale, const gchar **out_endptr)
{
	gboolean retval = TRUE;
	guint16 nums[3];
	gboolean error = FALSE;
	gchar *ptr, *numstart, *tofree;
	gint i;

	if (out_endptr)
		*out_endptr = NULL;

	if (!value)
		return FALSE;

	g_date_clear (date, 1);

	/* 1st number */
	ptr = g_strdup (value);
	tofree = ptr;
	numstart = ptr;
	while (*ptr && g_ascii_isdigit (*ptr))
		ptr++;
	if ((ptr != numstart) && *ptr) {
		*ptr = 0;
		if (! get_uint_from_string (numstart, &(nums[0])))
			error = TRUE;
	}
	else
		error = TRUE;

	/* 2nd number */
	if (!error) {
		if (value [ptr-tofree] != locale->separator)
			error = TRUE;
		else {
			ptr++;
			numstart = ptr;
			while (*ptr && g_ascii_isdigit (*ptr))
				ptr++;
			if ((ptr != numstart) && *ptr) {
				*ptr = 0;
				if (! get_uint_from_string (numstart, &(nums[1])))
					error = TRUE;
			}
			else
				error = TRUE;
		}
	}

	/* 3rd number */
	if (!error) {
		if (value [ptr-tofree] != locale->separator)
			error = TRUE;
		else {
			ptr++;
			numstart = ptr;
			while (*ptr && g_ascii_isdigit (*ptr))
				ptr++;
			*ptr = 0;
			if (ptr != numstart) {
				if (! get_uint_from_string (numstart, &(nums[2])))
					error = TRUE;
			}
			else
				error = TRUE;
		}
	}

	/* test if there are some characters left */
	if (!error) {
		if (out_endptr)
			*out_endptr = value + (ptr-tofree);
		else if (value [ptr-tofree])
			error = TRUE;
	}
	GDateDay day = 1;
	GDateMonth month = 1;
	GDateYear year = 1;

	/* analyse what's parsed */
	if (!error) {
		for (i=0; i<3; i++) {
			switch (locale->dmy_order[i]) {
			case G_DATE_DAY:
					day = nums[i];
				break;
			case G_DATE_MONTH:
				month = nums[i];
				break;
			case G_DATE_YEAR:
				year = nums[i] < 100 ? nums[i] + locale->current_offset : nums[i];
				break;
			}
		}
		if (!g_date_valid_dmy (day, month, year)) {
			retval = FALSE;
		} else {
			g_date_set_dmy (date, day, month, year);
			retval = TRUE;
		}
	}
	else
		retval = FALSE;

	g_free (tofree);

	return retval;
}

/* Makes a GdaTime from a string like:
 * 12:30:15+01
 * 12:30:15-02
 * 12:30:15.123
 * 123015+01
 * 123015-02
 * 123015.123
 * taken from libgda/gda-value.h
 *
 * Also works if there is only 0 or 1 digit instead of 2
 */
static GdaTime *
make_time (G_GNUC_UNUSED GdaHandlerTime *hdl, const gchar *value)
{
  GdaTime *ret = NULL;
	ret = gda_parse_iso8601_time (value);
  if (ret == NULL)
		return gda_parse_formatted_time (value, 0);
  return ret;
}


static GValue *
gda_handler_time_get_sane_init_value (G_GNUC_UNUSED GdaDataHandler *iface, GType type)
{
	GValue *value = NULL;
	GDateTime *gdate;

	gdate = g_date_time_new_now_utc ();

	if (type == G_TYPE_DATE) {
		GDate *date = g_date_new_dmy ((GDateDay) g_date_time_get_day_of_month (gdate),
		                             (GDateMonth) g_date_time_get_month (gdate),
		                             (GDateYear) g_date_time_get_year (gdate));
		value = g_value_init (g_new0 (GValue, 1), G_TYPE_DATE);
		g_value_take_boxed (value, date);
	}
	else if (type == GDA_TYPE_TIME) {
		value = g_value_init (g_new0 (GValue, 1), GDA_TYPE_TIME);
		g_value_set_boxed (value, gdate);
	}
	else if (g_type_is_a (type, G_TYPE_DATE_TIME)) {
    value = g_value_init (g_new0 (GValue, 1), G_TYPE_DATE_TIME);
	  g_value_set_boxed (value, gdate);
	}
	else
		value = gda_value_new_null ();

  g_date_time_unref (gdate);

	return value;
}

static gboolean
gda_handler_time_accepts_g_type (GdaDataHandler *iface, GType type)
{
	GdaHandlerTime *hdl;
	guint i;

	g_assert (iface);
	hdl = (GdaHandlerTime*) (iface);

	for (i = 0; i < hdl->nb_g_types; i++) {
		if (hdl->valid_g_types [i] == type)
			return TRUE;
	}

	return FALSE;
}

static const gchar *
gda_handler_time_get_descr (GdaDataHandler *iface)
{
	g_return_val_if_fail (GDA_IS_HANDLER_TIME (iface), NULL);
	return g_object_get_data (G_OBJECT (iface), "descr");
}

static void
data_handler_iface_init (GdaDataHandlerInterface *iface)
{
	iface->get_sql_from_value = gda_handler_time_get_sql_from_value;
	iface->get_str_from_value = gda_handler_time_get_str_from_value;
	iface->get_value_from_sql = gda_handler_time_get_value_from_sql;
	iface->get_value_from_str = gda_handler_time_get_value_from_str;
	iface->get_sane_init_value = gda_handler_time_get_sane_init_value;
	iface->accepts_g_type = gda_handler_time_accepts_g_type;
	iface->get_descr = gda_handler_time_get_descr;
}

static void
gda_handler_time_init (GdaHandlerTime *hdl)
{
	/* Private structure */
	hdl->nb_g_types = 4;
	hdl->valid_g_types = g_new0 (GType, 7);
	hdl->valid_g_types[0] = G_TYPE_DATE;
	hdl->valid_g_types[1] = GDA_TYPE_TIME;
	hdl->valid_g_types[2] = G_TYPE_DATE_TIME;
	hdl->valid_g_types[3] = G_TYPE_DATE_TIME;

	/* taking into accout the locale */
	hdl->sql_locale = g_new0 (LocaleSetting, 1);
	hdl->sql_locale->dmy_order[0] = G_DATE_MONTH;
	hdl->sql_locale->dmy_order[1] = G_DATE_DAY;
	hdl->sql_locale->dmy_order[2] = G_DATE_YEAR;
	hdl->sql_locale->twodigit_years = FALSE;
	hdl->sql_locale->current_offset = 0;
	hdl->sql_locale->separator = '-';

	hdl->str_locale = g_new0 (LocaleSetting, 1);
	hdl->str_locale->dmy_order[0] = G_DATE_MONTH;
	hdl->str_locale->dmy_order[1] = G_DATE_DAY;
	hdl->str_locale->dmy_order[2] = G_DATE_YEAR;
	hdl->str_locale->twodigit_years = FALSE;
	hdl->str_locale->current_offset = 0;
	hdl->str_locale->separator = '-';

	g_object_set_data (G_OBJECT (hdl), "name", "InternalTime");
	g_object_set_data (G_OBJECT (hdl), "descr", _("Time, Date and TimeStamp representation"));
}

static void
gda_handler_time_dispose (GObject *object)
{
	GdaHandlerTime *hdl;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GDA_IS_HANDLER_TIME (object));

	hdl = GDA_HANDLER_TIME (object);

	g_clear_pointer(&hdl->valid_g_types, g_free);
	g_clear_pointer(&hdl->str_locale, g_free);
	g_clear_pointer(&hdl->sql_locale, g_free);

	/* for the parent class */
	G_OBJECT_CLASS (gda_handler_time_parent_class)->dispose (object);
}

static void
gda_handler_time_class_init (GdaHandlerTimeClass * class)
{
	GObjectClass   *object_class = G_OBJECT_CLASS (class);

	object_class->dispose = gda_handler_time_dispose;
}
