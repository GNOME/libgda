/* gda-handler-time.c
 *
 * Copyright (C) 2003 - 2005 Vivien Malerba
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

#include "gda-handler-time.h"
#include <string.h>
#include <glib/gi18n-lib.h>

static void gda_handler_time_class_init (GdaHandlerTimeClass *class);
static void gda_handler_time_init (GdaHandlerTime *hdl);
static void gda_handler_time_dispose (GObject *object);
static void gnome_db_handler_compute_locale (GdaHandlerTime *hdl);

/* General notes:
 * about months representations:
 * -----------------------------
 * GtkCalendar gets months in [0-11]
 * GDate represents months in [1-12]
 * struct tm represents months in [0-11]
 * GdaDate represents months in [1-12]
 *
 * about date localization:
 * ------------------------
 * see how this aspect is handled in glib: function g_date_prepare_to_parse()
 * in file gdate.c
 */

/* GdaDataHandler interface */
static void         gda_handler_time_data_handler_init      (GdaDataHandlerIface *iface);
static gchar       *gda_handler_time_get_sql_from_value     (GdaDataHandler *dh, const GdaValue *value);
static gchar       *gda_handler_time_get_str_from_value     (GdaDataHandler *dh, const GdaValue *value);
static GdaValue    *gda_handler_time_get_value_from_sql     (GdaDataHandler *dh, const gchar *sql,
								  GdaValueType type);
static GdaValue    *gda_handler_time_get_value_from_str     (GdaDataHandler *dh, const gchar *sql,
								  GdaValueType type);

static GdaValue    *gda_handler_time_get_sane_init_value    (GdaDataHandler * dh, GdaValueType type);

static guint        gda_handler_time_get_nb_gda_types       (GdaDataHandler *dh);
static GdaValueType gda_handler_time_get_gda_type_index     (GdaDataHandler *dh, guint index);
static gboolean     gda_handler_time_accepts_gda_type       (GdaDataHandler * dh, GdaValueType type);

static const gchar *gda_handler_time_get_descr              (GdaDataHandler *dh);

typedef struct _LocaleSetting {
	GDateDMY        dmy_order[3];
	gboolean        twodigit_years;
	gint            current_offset; /* 1900, 2000, etc... */
	gchar           separator;	
} LocaleSetting;

struct  _GdaHandlerTimePriv {
	gchar          *detailled_descr;
	guint           nb_gda_types;
	GdaValueType   *valid_gda_types;

	/* for locale setting */
	LocaleSetting  *sql_locale;
	LocaleSetting  *str_locale;
};

/* get a pointer to the parents to be able to call their destructor */
static GObjectClass *parent_class = NULL;

GType
gda_handler_time_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (GdaHandlerTimeClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_handler_time_class_init,
			NULL,
			NULL,
			sizeof (GdaHandlerTime),
			0,
			(GInstanceInitFunc) gda_handler_time_init
		};		

		static const GInterfaceInfo data_entry_info = {
			(GInterfaceInitFunc) gda_handler_time_data_handler_init,
			NULL,
			NULL
		};

		type = g_type_register_static (GDA_TYPE_OBJECT, "GdaHandlerTime", &info, 0);
		g_type_add_interface_static (type, GDA_TYPE_DATA_HANDLER, &data_entry_info);
	}
	return type;
}

static void
gda_handler_time_data_handler_init (GdaDataHandlerIface *iface)
{
	iface->get_sql_from_value = gda_handler_time_get_sql_from_value;
	iface->get_str_from_value = gda_handler_time_get_str_from_value;
	iface->get_value_from_sql = gda_handler_time_get_value_from_sql;
	iface->get_value_from_str = gda_handler_time_get_value_from_str;
	iface->get_sane_init_value = gda_handler_time_get_sane_init_value;
	iface->get_nb_gda_types = gda_handler_time_get_nb_gda_types;
	iface->accepts_gda_type = gda_handler_time_accepts_gda_type;
	iface->get_gda_type_index = gda_handler_time_get_gda_type_index;
	iface->get_descr = gda_handler_time_get_descr;
}


static void
gda_handler_time_class_init (GdaHandlerTimeClass * class)
{
	GObjectClass   *object_class = G_OBJECT_CLASS (class);
	
	parent_class = g_type_class_peek_parent (class);

	object_class->dispose = gda_handler_time_dispose;
}

static void
gda_handler_time_init (GdaHandlerTime *hdl)
{
	/* Private structure */
	hdl->priv = g_new0 (GdaHandlerTimePriv, 1);
	hdl->priv->detailled_descr = _("Time and Date handler");
	hdl->priv->nb_gda_types = 3;
	hdl->priv->valid_gda_types = g_new0 (GdaValueType, 7);
	hdl->priv->valid_gda_types[0] = GDA_VALUE_TYPE_DATE;
	hdl->priv->valid_gda_types[1] = GDA_VALUE_TYPE_TIME;
	hdl->priv->valid_gda_types[2] = GDA_VALUE_TYPE_TIMESTAMP;

	/* taking into accout the locale */
	hdl->priv->sql_locale = g_new0 (LocaleSetting, 1);
	hdl->priv->sql_locale->dmy_order[0] = G_DATE_MONTH;
	hdl->priv->sql_locale->dmy_order[1] = G_DATE_DAY;
	hdl->priv->sql_locale->dmy_order[2] = G_DATE_YEAR;
	hdl->priv->sql_locale->twodigit_years = FALSE;
	hdl->priv->sql_locale->current_offset = 0;
	hdl->priv->sql_locale->separator = '-';

	hdl->priv->str_locale = g_new0 (LocaleSetting, 1);
	hdl->priv->str_locale->dmy_order[0] = G_DATE_DAY;
	hdl->priv->str_locale->dmy_order[1] = G_DATE_MONTH;
	hdl->priv->str_locale->dmy_order[2] = G_DATE_YEAR;
	hdl->priv->str_locale->twodigit_years = FALSE;
	hdl->priv->sql_locale->current_offset = 0;
	hdl->priv->str_locale->separator = '/';

	gda_object_set_name (GDA_OBJECT (hdl), _("InternalTime"));
	gda_object_set_description (GDA_OBJECT (hdl), _("Time, Date and TimeStamp representation"));
}

static void
gda_handler_time_dispose (GObject *object)
{
	GdaHandlerTime *hdl;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GDA_IS_HANDLER_TIME (object));

	hdl = GDA_HANDLER_TIME (object);

	if (hdl->priv) {
		gda_object_destroy_check (GDA_OBJECT (object));

		g_free (hdl->priv->valid_gda_types);
		hdl->priv->valid_gda_types = NULL;

		g_free (hdl->priv->str_locale);
		g_free (hdl->priv->sql_locale);

		g_free (hdl->priv);
		hdl->priv = NULL;
	}

	/* for the parent class */
	parent_class->dispose (object);
}

/**
 * gda_handler_time_new
 *
 * Creates a data handler for time values
 *
 * Returns: the new object
 */
GdaDataHandler *
gda_handler_time_new (void)
{
	GObject *obj;

	obj = g_object_new (GDA_TYPE_HANDLER_TIME, "dict", NULL, NULL);
	gnome_db_handler_compute_locale (GDA_HANDLER_TIME (obj));

	return (GdaDataHandler *) obj;
}

/**
 * gda_handler_time_new_no_locale
 *
 * Creates a data handler for time values, but using the default C locale
 * instead of the current user locale.
 *
 * Returns: the new object
 */
GdaDataHandler *
gda_handler_time_new_no_locale (void)
{
	GObject *obj;

	obj = g_object_new (GDA_TYPE_HANDLER_TIME, NULL);

	return (GdaDataHandler *) obj;
}

static void
gnome_db_handler_compute_locale (GdaHandlerTime *hdl)
{
	GDate *date;
	gchar buf[128], *ptr, *numstart;
	gint nums[3];
	gboolean error = FALSE;

	date = g_date_new_dmy (4, 7, 1976); /* Same date used by GLib */
	g_date_strftime (buf, 127, "%x", date);

	/* 1st number */
	ptr = buf;
	numstart = ptr;
	while (*ptr && g_ascii_isdigit (*ptr))
		ptr++;
	if (*ptr) {
		hdl->priv->str_locale->separator = *ptr;
		*ptr = 0;
		nums[0] = atoi (numstart);
	}
	else error = TRUE;

	/* 2nd number */
	if (!error) {
		ptr++;
		numstart = ptr;
		while (*ptr && g_ascii_isdigit (*ptr))
			ptr++;
		if (*ptr) {
			*ptr = 0;
			nums[1] = atoi (numstart);
		}
		else error = TRUE;
	}

	/* 3rd number */
	if (!error) {
		ptr++;
		numstart = ptr;
		while (*ptr && g_ascii_isdigit (*ptr))
			ptr++;
		*ptr = 0;
		nums[2] = atoi (numstart);
	}
	
	/* computations */
	if (!error) {
#ifdef GDA_DEBUG
		gchar *strings[3];
#endif
		gint i;
		time_t now;
		struct tm *now_tm;

		for (i=0; i<3; i++) {
			switch (nums[i]) {
			case 7:
				hdl->priv->str_locale->dmy_order[i] = G_DATE_MONTH;
				break;
			case 4:
				hdl->priv->str_locale->dmy_order[i] = G_DATE_DAY;
				break;
			case 76:
				hdl->priv->str_locale->twodigit_years = TRUE;
			case 1976:
				hdl->priv->str_locale->dmy_order[i] = G_DATE_YEAR;
				break;
			default:
				break;
			}
		}
		
		now = time (NULL);
		now_tm = localtime (&now);
		hdl->priv->str_locale->current_offset = ((now_tm->tm_year + 1900) / 100) * 100;

#ifdef GDA_DEBUG		
		for (i=0; i<3; i++) {
			switch (hdl->priv->str_locale->dmy_order[i]) {
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
		g_print ("GdaHandlerTime\n");
		g_print ("\tlocale order = %s %s %s, separator = %c\n", 
			 strings[0], strings[1], strings[2], hdl->priv->str_locale->separator);
		if (hdl->priv->str_locale->twodigit_years)
			g_print ("\tlocale has 2 digits year, using %d as offset\n", hdl->priv->str_locale->current_offset);
		else
			g_print ("\tlocale has 4 digits year\n");
#endif
	}
	else {
		TO_IMPLEMENT;
	}
}

/* Interface implementation */
static gchar *render_date_locale (const GdaDate *date, LocaleSetting *locale);

/* REM: SQL date format is always returned using the MM-DD-YYY format, it's up to the
 * provider to be correctly set up to accept this format.
 */
static gchar *
gda_handler_time_get_sql_from_value (GdaDataHandler *iface, const GdaValue *value)
{
	gchar *retval = NULL, *str;
	GdaHandlerTime *hdl;
	const GdaDate *date;
	GdaDate vdate;
	const GdaTime *tim;
	GdaTime vtim;
	const GdaTimestamp *gdats;

	g_return_val_if_fail (iface && GDA_IS_HANDLER_TIME (iface), NULL);
	hdl = GDA_HANDLER_TIME (iface);
	g_return_val_if_fail (hdl->priv, NULL);

	switch (gda_value_get_type (value)) {
	case GDA_VALUE_TYPE_DATE:
		date = gda_value_get_date (value);
		str = render_date_locale (date, hdl->priv->sql_locale);
		retval = g_strdup_printf ("'%s'", str);
		g_free (str);
		break;
	case GDA_VALUE_TYPE_TIME:
		tim = gda_value_get_time (value);
		retval = g_strdup_printf ("'%02d:%02d:%02d'",
					  tim->hour,
					  tim->minute,
					  tim->second);
		break;
	case GDA_VALUE_TYPE_TIMESTAMP:
		gdats = gda_value_get_timestamp (value);
		vdate.year = gdats->year;
		vdate.month = gdats->month;
		vdate.day = gdats->day;
		str = render_date_locale (&vdate, hdl->priv->sql_locale);
		vtim.hour = gdats->hour;
		vtim.minute = gdats->minute;
		vtim.second = gdats->second;
		retval = g_strdup_printf ("'%s %02d:%02d:%02d'",
					  str,
					  vtim.hour,
					  vtim.minute,
					  vtim.second);
		g_free (str);
		break;
	default:
		g_assert_not_reached ();
		break;
	}

	return retval;
}

static gchar *strip_quotes (const gchar *str);
static gchar *
gda_handler_time_get_str_from_value (GdaDataHandler *iface, const GdaValue *value)
{
	GdaHandlerTime *hdl;
	gchar *retval = NULL, *str;
	const GdaDate *date;
	const GdaTimestamp *gdats;
	GdaDate vdate;
	GdaTime vtime;

	g_return_val_if_fail (iface && GDA_IS_HANDLER_TIME (iface), NULL);
	hdl = GDA_HANDLER_TIME (iface);
	g_return_val_if_fail (hdl->priv, NULL);
	
	switch (gda_value_get_type (value)) {
	case GDA_VALUE_TYPE_DATE:
		date = gda_value_get_date (value);
		retval = render_date_locale (date, hdl->priv->str_locale);
		break;
	case GDA_VALUE_TYPE_TIME:
		str = gda_handler_time_get_sql_from_value (iface, value);
		retval = strip_quotes (str);
		g_free (str);
		break;
	case GDA_VALUE_TYPE_TIMESTAMP:
		gdats = gda_value_get_timestamp (value);
		vdate.year = gdats->year;
		vdate.month = gdats->month;
		vdate.day = gdats->day;
		str = render_date_locale (&vdate, hdl->priv->str_locale);
		vtime.hour = gdats->hour;
		vtime.minute = gdats->minute;
		vtime.second = gdats->second;
		retval = g_strdup_printf ("%s %02d:%02d:%02d",
					  str,
					  vtime.hour,
					  vtime.minute,
					  vtime.second);
		g_free (str);
		break;
	default:
		g_assert_not_reached ();
		break;
	}
       
	return retval;
}

static gchar *
render_date_locale (const GdaDate *date, LocaleSetting *locale)
{
	GString *string;
	gchar *retval;
	gint i;

	string = g_string_new ("");
	for (i=0; i<3; i++) {
		if (i)
			g_string_append_c (string, locale->separator);

		switch (locale->dmy_order[i]) {
		case G_DATE_DAY:
			g_string_append_printf (string, "%02d", date->day);
			break;
		case G_DATE_MONTH:
			g_string_append_printf (string, "%02d", date->month);
			break;
		case G_DATE_YEAR:
			if (locale->twodigit_years) {
				if ((date->year >= locale->current_offset) && (date->year < locale->current_offset + 100))
					g_string_append_printf (string, "%02d", date->year - locale->current_offset);
				else
					g_string_append_printf (string, "%04d", date->year);
			}
			else
				g_string_append_printf (string, "%04d", date->year);
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

static GdaValue *gda_handler_time_get_value_from_locale (GdaDataHandler *iface, const gchar *sql, 
							GdaValueType type, LocaleSetting *locale);

static GdaValue *
gda_handler_time_get_value_from_sql (GdaDataHandler *iface, const gchar *sql, GdaValueType type)
{
	GdaHandlerTime *hdl;

	g_return_val_if_fail (iface && GDA_IS_HANDLER_TIME (iface), NULL);
	hdl = GDA_HANDLER_TIME (iface);
	g_return_val_if_fail (hdl->priv, NULL);

	return gda_handler_time_get_value_from_locale (iface, sql, type, hdl->priv->sql_locale);
}

static GdaValue *
gda_handler_time_get_value_from_str (GdaDataHandler *iface, const gchar *sql, GdaValueType type)
{
	GdaHandlerTime *hdl;

	g_return_val_if_fail (iface && GDA_IS_HANDLER_TIME (iface), NULL);
	hdl = GDA_HANDLER_TIME (iface);
	g_return_val_if_fail (hdl->priv, NULL);

	return gda_handler_time_get_value_from_locale (iface, sql, type, hdl->priv->str_locale);
}



static gboolean make_timestamp (GdaHandlerTime *hdl, GdaTimestamp *timestamp, 
				const gchar *value, LocaleSetting *locale);
static gboolean make_date (GdaHandlerTime *hdl, GdaDate *date, const gchar *value, LocaleSetting *locale);
static gboolean make_time (GdaHandlerTime *hdl, GdaTime *timegda, const gchar *value);
static GdaValue *
gda_handler_time_get_value_from_locale (GdaDataHandler *iface, const gchar *sql, 
					GdaValueType type, LocaleSetting *locale)
{
	GdaHandlerTime *hdl;
	GdaValue *value = NULL;

	GdaDate date;
        GdaTime timegda;
        GdaTimestamp timestamp;

	g_return_val_if_fail (iface && GDA_IS_HANDLER_TIME (iface), NULL);
	hdl = GDA_HANDLER_TIME (iface);
	g_return_val_if_fail (hdl->priv, NULL);

	switch (type) {
	case GDA_VALUE_TYPE_DATE:
		if (make_date (hdl, &date, sql, locale))
			value = gda_value_new_date (&date);
		else
			value = NULL;
		break;
	case GDA_VALUE_TYPE_TIME:
		if (make_time (hdl, &timegda, sql))
			value = gda_value_new_time (&timegda);
		else
			value = NULL;
		break;
	case GDA_VALUE_TYPE_TIMESTAMP:
		if (make_timestamp (hdl, &timestamp, sql, locale)) 
			value = gda_value_new_timestamp (&timestamp);
		else
			value = NULL;
		break;
	default:
		g_assert_not_reached ();
		break;
	}

	return value;
}

/* Makes a GdaTimestamp from a string like "24-12-2003 13:12:01.12+01",
 * taken from libgda/gda-value.h
 * with a modification for the date format
 */
static gboolean
make_timestamp (GdaHandlerTime *hdl, GdaTimestamp *timestamp, const gchar *value, LocaleSetting *locale)
{
	gboolean retval;
	gchar *str, *ptr;
	GdaDate vdate;
	GdaTime vtime;

	str = g_strdup (value);
	ptr = strtok (str, " ");
	retval = make_date (hdl, &vdate, ptr, locale);
	if (retval) {
		ptr = strtok (NULL, " ");
		retval = make_time (hdl, &vtime, ptr);
		if (retval) {
			timestamp->day = vdate.day;
			timestamp->month = vdate.month;
			timestamp->year = vdate.year;

			timestamp->hour = vtime.hour;
			timestamp->minute = vtime.minute;
			timestamp->second = vtime.second;
			timestamp->fraction = 0;
			timestamp->timezone = 0;
		}
	}
	g_free (str);

	return retval;
}


/* Makes a GdaDate from a string like "24-12-2003" */
static gboolean
make_date (GdaHandlerTime *hdl, GdaDate *date, const gchar *value, LocaleSetting *locale)
{
	GDate *gdate;
	gboolean retval = TRUE;
	gushort nums[3];
	gboolean error = FALSE;
	gchar *ptr, *numstart, *tofree;
	gint i;
	
	/* 1st number */
	ptr = g_strdup (value);
	tofree = ptr;
	numstart = ptr;
	while (ptr && *ptr && g_ascii_isdigit (*ptr))
		ptr++;
	if (ptr && *ptr) {
		*ptr = 0;
		nums[0] = atoi (numstart);
	}
	else error = TRUE;

	/* 2nd number */
	if (!error) {
		ptr++;
		numstart = ptr;
		while (*ptr && g_ascii_isdigit (*ptr))
			ptr++;
		if (*ptr) {
			*ptr = 0;
			nums[1] = atoi (numstart);
		}
		else error = TRUE;
	}

	/* 3rd number */
	if (!error) {
		ptr++;
		numstart = ptr;
		while (*ptr && g_ascii_isdigit (*ptr))
			ptr++;
		*ptr = 0;
		nums[2] = atoi (numstart);
	}

	if (!error) {
		for (i=0; i<3; i++) 
			switch (locale->dmy_order[i]) {
			case G_DATE_DAY:
				date->day = nums[i];
				break;
			case G_DATE_MONTH:
				date->month = nums[i];
				break;
			case G_DATE_YEAR:
				date->year = nums[i];
				if (date->year < 100)
					date->year += locale->current_offset;
				break;
			}
		
		/* checks */
		if (g_date_valid_day (date->day) && g_date_valid_month (date->month) && 
		    g_date_valid_year (date->year)) {
			gdate = g_date_new ();
			g_date_set_day (gdate, date->day);
			g_date_set_month (gdate, date->month);
			g_date_set_year (gdate, date->year);
			retval = g_date_valid (gdate);
			g_date_free (gdate);
		}
		else
			retval = FALSE;
	}
	else
		retval = FALSE;

	return retval;
}

/* Makes a GdaTime from a string like "12:30:15+01",
 * taken from libgda/gda-value.h
 */
static gboolean
make_time (GdaHandlerTime *hdl, GdaTime *timegda, const gchar *value)
{
	gboolean retval = TRUE;

	if (!value)
		return FALSE;

        timegda->hour = atoi (value);
        value += 3;
        timegda->minute = atoi (value);
        value += 3;
        timegda->second = atoi (value);
        value += 2;
        if (*value)
                timegda->timezone = atoi (value);
        else
                timegda->timezone = 0;
	timegda->timezone = 0;


	/* checks */
	if ((timegda->hour > 24) || (timegda->minute > 60) || (timegda->second > 60))
		retval = FALSE;

	return retval;
}


static GdaValue *
gda_handler_time_get_sane_init_value (GdaDataHandler *iface, GdaValueType type)
{
	GdaHandlerTime *hdl;
	GdaValue *value = NULL;
	
	time_t now;
	struct tm *stm;
	GdaDate gdate;
	GdaTime gtime;
	GdaTimestamp gts;

	g_return_val_if_fail (iface && GDA_IS_HANDLER_TIME (iface), NULL);
	hdl = GDA_HANDLER_TIME (iface);
	g_return_val_if_fail (hdl->priv, NULL);

	now = time (NULL);
	stm = localtime (&now);
	switch (type) {
	case GDA_VALUE_TYPE_DATE:
		gdate.year = stm->tm_year + 1900;
		gdate.month = stm->tm_mon + 1;
		gdate.day = stm->tm_mday;
		value = gda_value_new_date (&gdate);
		break;
	case GDA_VALUE_TYPE_TIME:
                gtime.hour = stm->tm_hour;
		gtime.minute = stm->tm_min;
		gtime.second = stm->tm_sec;
		gtime.timezone = 0;
		value = gda_value_new_time (&gtime);
		break;
	case GDA_VALUE_TYPE_TIMESTAMP:
		gts.year = stm->tm_year + 1900;
		gts.month = stm->tm_mon + 1;
		gts.day = stm->tm_mday;
                gts.hour = stm->tm_hour;
		gts.minute = stm->tm_min;
		gts.second = stm->tm_sec;
		gts.fraction = 0;
		gts.timezone = 0;
		value = gda_value_new_timestamp (&gts);
		break;
	default:
		g_assert_not_reached ();
		break;
	}

	return value;
}


static guint
gda_handler_time_get_nb_gda_types (GdaDataHandler *iface)
{
	GdaHandlerTime *hdl;

	g_return_val_if_fail (iface && GDA_IS_HANDLER_TIME (iface), 0);
	hdl = GDA_HANDLER_TIME (iface);
	g_return_val_if_fail (hdl->priv, 0);

	return hdl->priv->nb_gda_types;
}


static gboolean
gda_handler_time_accepts_gda_type (GdaDataHandler *iface, GdaValueType type)
{
	GdaHandlerTime *hdl;
	guint i = 0;
	gboolean found = FALSE;

	g_return_val_if_fail (iface && GDA_IS_HANDLER_TIME (iface), FALSE);
	g_return_val_if_fail (type != GDA_VALUE_TYPE_UNKNOWN, FALSE);
	hdl = GDA_HANDLER_TIME (iface);
	g_return_val_if_fail (hdl->priv, 0);

	while (!found && (i < hdl->priv->nb_gda_types)) {
		if (hdl->priv->valid_gda_types [i] == type)
			found = TRUE;
		i++;
	}

	return found;
}

static GdaValueType
gda_handler_time_get_gda_type_index (GdaDataHandler *iface, guint index)
{
	GdaHandlerTime *hdl;

	g_return_val_if_fail (iface && GDA_IS_HANDLER_TIME (iface), GDA_VALUE_TYPE_UNKNOWN);
	hdl = GDA_HANDLER_TIME (iface);
	g_return_val_if_fail (hdl->priv, GDA_VALUE_TYPE_UNKNOWN);
	g_return_val_if_fail (index < hdl->priv->nb_gda_types, GDA_VALUE_TYPE_UNKNOWN);

	return hdl->priv->valid_gda_types[index];
}

static const gchar *
gda_handler_time_get_descr (GdaDataHandler *iface)
{
	GdaHandlerTime *hdl;

	g_return_val_if_fail (iface && GDA_IS_HANDLER_TIME (iface), NULL);
	hdl = GDA_HANDLER_TIME (iface);
	g_return_val_if_fail (hdl->priv, NULL);

	return gda_object_get_description (GDA_OBJECT (hdl));
}
