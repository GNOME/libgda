/* GDA client library
 * Copyright (C) 1998,1999 Michael Lausch
 * Copyright (C) 1999 Rodrigo Moya
 * Copyright (C) 2000 Vivien Malerba
 *
 * This Library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this Library; see the file COPYING.LIB.  If not,
 * write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"
#include "gda-field.h"
#include <stdio.h>
#include <time.h>

#ifdef ENABLE_NLS
#  include <libintl.h>
#  define _(String) gettext (String)
#  define N_(String) (String)
#else
/* Stubs that do something close enough. */
#  define textdomain(String)
#  define gettext(String) (String)
#  define dgettext(Domain,Message) (Message)
#  define dcgettext(Domain,Message,Type) (Message)
#  define bindtextdomain(Domain,Directory)
#  define _(String) (String)
#  define N_(String) (String)
#endif

enum
{
	GDA_FIELD_LAST_SIGNAL
};

static guint gda_field_signals[GDA_FIELD_LAST_SIGNAL] = { 0, };

static void gda_field_class_init (GdaFieldClass * klass);
static void gda_field_init (GdaField * field);

#define ENUM_TO_STR(e) case (e): strncpy(bfr, #e, length); break
#define min(a,b)       (a) < (b) ? (a) : (b)

guint
gda_field_get_type (void)
{
	static guint gda_field_type = 0;

	if (!gda_field_type) {
		GtkTypeInfo gda_field_info = {
			"GdaField",
			sizeof (GdaField),
			sizeof (GdaFieldClass),
			(GtkClassInitFunc) gda_field_class_init,
			(GtkObjectInitFunc) gda_field_init,
			(GtkArgSetFunc) NULL,
			(GtkArgSetFunc) NULL,
		};
		gda_field_type =
			gtk_type_unique (gtk_object_get_type (),
					 &gda_field_info);
	}
	return gda_field_type;
}

static void
gda_field_class_init (GdaFieldClass * klass)
{
	GtkObjectClass *object_class = GTK_OBJECT_CLASS (klass);
}

static void
gda_field_init (GdaField * field)
{
}

/**
 * gda_fieldtype_2_string:
 * @bfr: bufferspace where the printed name of the type of the field
 * is placed into.
 * @length: the size of the buffer
 *
 * Returns: a pointer to the buffer.
 */
gchar *
gda_fieldtype_2_string (gchar * bfr, gint length, GDA_ValueType type)
{
	if (!bfr) {
		bfr = g_new0 (gchar, 20);
		length = 20;
	}
	switch (type) {
		ENUM_TO_STR (GDA_TypeNull);
		ENUM_TO_STR (GDA_TypeBigint);
		ENUM_TO_STR (GDA_TypeBinary);
		ENUM_TO_STR (GDA_TypeBoolean);
		ENUM_TO_STR (GDA_TypeBstr);
		ENUM_TO_STR (GDA_TypeChar);
		ENUM_TO_STR (GDA_TypeCurrency);
		ENUM_TO_STR (GDA_TypeDate);
		ENUM_TO_STR (GDA_TypeDbDate);
		ENUM_TO_STR (GDA_TypeDbTime);
		ENUM_TO_STR (GDA_TypeDbTimestamp);
		ENUM_TO_STR (GDA_TypeDecimal);
		ENUM_TO_STR (GDA_TypeDouble);
		ENUM_TO_STR (GDA_TypeError);
		ENUM_TO_STR (GDA_TypeInteger);
		ENUM_TO_STR (GDA_TypeLongvarbin);
		ENUM_TO_STR (GDA_TypeLongvarchar);
		ENUM_TO_STR (GDA_TypeLongvarwchar);
		ENUM_TO_STR (GDA_TypeNumeric);
		ENUM_TO_STR (GDA_TypeSingle);
		ENUM_TO_STR (GDA_TypeSmallint);
		ENUM_TO_STR (GDA_TypeTinyint);
		ENUM_TO_STR (GDA_TypeUBigint);
		ENUM_TO_STR (GDA_TypeUSmallint);
		ENUM_TO_STR (GDA_TypeVarchar);
		ENUM_TO_STR (GDA_TypeVarbin);
		ENUM_TO_STR (GDA_TypeVarwchar);
		ENUM_TO_STR (GDA_TypeFixchar);
		ENUM_TO_STR (GDA_TypeFixbin);
		ENUM_TO_STR (GDA_TypeFixwchar);
		ENUM_TO_STR (GDA_TypeLastValue);
	}
	return bfr;
}

/**
 * gda_string_2_fieldtype
 * @type GDA type string
 *
 * Returns the GDA type identifier for the given string (returned by
 * gda_fieldtype_2_string
 */
GDA_ValueType
gda_string_2_fieldtype (gchar * type)
{
	g_return_val_if_fail (type != 0, -1);

	/* FIXME: is there a better way of doing this? */
	if (!strcmp (type, "GDA_TypeNull"))
		return (GDA_TypeNull);
	else if (!strcmp (type, "GDA_TypeBigint"))
		return (GDA_TypeBigint);
	else if (!strcmp (type, "GDA_TypeBinary"))
		return (GDA_TypeBinary);
	else if (!strcmp (type, "GDA_TypeBoolean"))
		return (GDA_TypeBoolean);
	else if (!strcmp (type, "GDA_TypeBstr"))
		return (GDA_TypeBstr);
	else if (!strcmp (type, "GDA_TypeChar"))
		return (GDA_TypeChar);
	else if (!strcmp (type, "GDA_TypeCurrency"))
		return (GDA_TypeCurrency);
	else if (!strcmp (type, "GDA_TypeDate"))
		return (GDA_TypeDate);
	else if (!strcmp (type, "GDA_TypeDbDate"))
		return (GDA_TypeDbDate);
	else if (!strcmp (type, "GDA_TypeDbTime"))
		return (GDA_TypeDbTime);
	else if (!strcmp (type, "GDA_TypeDbTimestamp"))
		return (GDA_TypeDbTimestamp);
	else if (!strcmp (type, "GDA_TypeDecimal"))
		return (GDA_TypeDecimal);
	else if (!strcmp (type, "GDA_TypeDouble"))
		return (GDA_TypeDouble);
	else if (!strcmp (type, "GDA_TypeError"))
		return (GDA_TypeError);
	else if (!strcmp (type, "GDA_TypeInteger"))
		return (GDA_TypeInteger);
	else if (!strcmp (type, "GDA_TypeLongvarbin"))
		return (GDA_TypeLongvarbin);
	else if (!strcmp (type, "GDA_TypeLongvarchar"))
		return (GDA_TypeLongvarchar);
	else if (!strcmp (type, "GDA_TypeLongvarwchar"))
		return (GDA_TypeLongvarwchar);
	else if (!strcmp (type, "GDA_TypeNumeric"))
		return (GDA_TypeNumeric);
	else if (!strcmp (type, "GDA_TypeSingle"))
		return (GDA_TypeSingle);
	else if (!strcmp (type, "GDA_TypeSmallint"))
		return (GDA_TypeSmallint);
	else if (!strcmp (type, "GDA_TypeTinyint"))
		return (GDA_TypeTinyint);
	else if (!strcmp (type, "GDA_TypeUBigint"))
		return (GDA_TypeUBigint);
	else if (!strcmp (type, "GDA_TypeUSmallint"))
		return (GDA_TypeUSmallint);
	else if (!strcmp (type, "GDA_TypeVarchar"))
		return (GDA_TypeVarchar);
	else if (!strcmp (type, "GDA_TypeVarbin"))
		return (GDA_TypeVarbin);
	else if (!strcmp (type, "GDA_TypeVarwchar"))
		return (GDA_TypeVarwchar);
	else if (!strcmp (type, "GDA_TypeFixchar"))
		return (GDA_TypeFixchar);
	else if (!strcmp (type, "GDA_TypeFixbin"))
		return (GDA_TypeFixbin);
	else if (!strcmp (type, "GDA_TypeFixwchar"))
		return (GDA_TypeFixwchar);
	return (-1);
}

/**
 * gda_stringify_value
 * @bfr: buffer where the string will be copied.
 * @maxlen: maximum length of @bfr.
 * @f: a #GdaField object.
 *
 * Converts the value stored on the given #GdaField object
 * into a human-readable string. You can either pass it an
 * existing buffer (@bfr) or pass it NULL as the first
 * parameter, in which case it will return a newly allocated
 * string containing the value.
 *
 * Returns: a pointer to @bfr or to a newly allocated string,
 * which should be g_free's when no longer needed.
 */
gchar *
gda_stringify_value (gchar * bfr, gint maxlen, GdaField * f)
{
	gchar *retval = NULL;
	gchar tmp[40];
	struct tm stm;

	g_return_val_if_fail (GDA_IS_FIELD (f), 0);

	if (bfr)
		retval = bfr;

	if (gda_field_is_null (f))
		return g_strdup (_("<NULL>"));

	switch (f->attributes->gdaType) {
	case GDA_TypeNull:
		if (bfr)
			retval = strncpy (bfr, _("<Unknown GDA Type(NULL)>"),
					  maxlen);
		else
			retval = g_strdup (_("<Unknown GDA Type(NULL)>"));
		break;
	case GDA_TypeBigint:
		if (!bfr) {
			retval = g_new (gchar, 20);
			maxlen = 20;
		}
		g_snprintf (retval, maxlen, "%Ld", gda_field_get_bigint_value (f));
		break;
	case GDA_TypeBoolean:
		{
			gchar *retstr;
			if (gda_field_get_boolean_value (f))
				retstr = _("true");
			else
				retstr = _("false");

			if (bfr)
				retval = strncpy (bfr, retstr, maxlen);
			else
				retval = g_strdup (retstr);
		}
		break;
	case GDA_TypeVarbin:
		{
			gint len = maxlen - 1;
			gint copylen;

			if (!bfr) {
				retval = g_new0 (char,
						 f->real_value->_u.v._u.lvb.
						 _length + 1);
				len = f->real_value->_u.v._u.lvb._length;
			}
			copylen = min (len, f->real_value->_u.v._u.lvb._length);
			gda_log_message ("GDA_TypeVarbin: Copying %d bytes\n", copylen);
			memcpy (retval, f->real_value->_u.v._u.lvb._buffer, copylen);
			retval[len] = '\0';
		}
		break;
	case GDA_TypeLongvarchar :
	case GDA_TypeVarchar :
	case GDA_TypeChar :
		if (!bfr) {
			if (f->real_value->_u.v._u.lvc != NULL)
				retval = g_strdup (f->real_value->_u.v._u.lvc);
			else
				retval = g_strdup (_("<NULL>"));
		}
		else {
			if (f->real_value->_u.v._u.lvc != NULL) {
				strncpy (retval, f->real_value->_u.v._u.lvc,
					 min (maxlen, strlen (f->real_value->_u.v._u.lvc)));
				retval[min (maxlen, strlen (f->real_value->_u.v._u.lvc))] = '\0';
			}
			else
				retval = g_strdup (_("<NULL>"));
		}
		break;
	case GDA_TypeInteger:
		if (!bfr) {
			retval = g_new0 (gchar, 20);
			maxlen = 20;
		}
		g_snprintf (retval, maxlen, "%d", f->real_value->_u.v._u.i);
		break;
	case GDA_TypeSmallint:
		if (!bfr) {
			retval = g_new0 (gchar, 20);
			maxlen = 20;
		}
		g_snprintf (retval, maxlen, "%d", f->real_value->_u.v._u.si);
		break;
	case GDA_TypeDecimal:
	case GDA_TypeNumeric:
		if (!bfr)
			retval = g_strdup (f->real_value->_u.v._u.lvc);
		else {
			strncpy (retval, f->real_value->_u.v._u.lvc,
				 min (maxlen,
				      strlen (f->real_value->_u.v._u.lvc)));
			retval[min
			       (maxlen,
				strlen (f->real_value->_u.v._u.lvc))] = '\0';
		}
		break;
	case GDA_TypeSingle:
		if (!bfr) {
			retval = g_new0 (gchar, 20);
			maxlen = 20;
		}
		g_snprintf (retval, maxlen, "%g", f->real_value->_u.v._u.f);
		break;
	case GDA_TypeDouble:
		if (!bfr) {
			retval = g_new0 (gchar, 20);
			maxlen = 20;
		}
		g_snprintf (retval, maxlen, "%g", f->real_value->_u.v._u.dp);
		break;
	case GDA_TypeDbTime:
		memset (&stm, 0, sizeof (stm));
		stm.tm_sec = f->real_value->_u.v._u.dbt.second;
		stm.tm_min = f->real_value->_u.v._u.dbt.minute;
		stm.tm_hour = f->real_value->_u.v._u.dbt.hour;
		strftime (tmp, 30, "%X", &stm);
		if (!bfr)
			retval = g_strdup (tmp);
		else {
			strncpy (retval, tmp, min (maxlen, strlen (tmp)));
			retval[min (maxlen, strlen (tmp))] = '\0';
		}
		break;
	case GDA_TypeDbDate:
		memset (&stm, 0, sizeof (stm));
		stm.tm_year = f->real_value->_u.v._u.dbd.year - 1900;
		stm.tm_mon = f->real_value->_u.v._u.dbd.month - 1;
		stm.tm_mday = f->real_value->_u.v._u.dbd.day;
		strftime (tmp, 30, "%x", &stm);
		if (!bfr)
			retval = g_strdup (tmp);
		else {
			strncpy (retval, tmp, min (maxlen, strlen (tmp)));
			retval[min (maxlen, strlen (tmp))] = '\0';
		}
		break;
	case GDA_TypeDbTimestamp:
		memset (&stm, 0, sizeof (stm));
		stm.tm_sec = f->real_value->_u.v._u.dbtstamp.second;
		stm.tm_min = f->real_value->_u.v._u.dbtstamp.minute;
		stm.tm_hour = f->real_value->_u.v._u.dbtstamp.hour;
		stm.tm_year = f->real_value->_u.v._u.dbtstamp.year - 1900;
		stm.tm_mon = f->real_value->_u.v._u.dbtstamp.month - 1;
		stm.tm_mday = f->real_value->_u.v._u.dbtstamp.day;
		strftime (tmp, 40, "%x %X", &stm);	/* the %c does not seem to work! */
		if (!bfr)
			retval = g_strdup (tmp);
		else {
			strncpy (retval, tmp, min (maxlen, strlen (tmp)));
			retval[min (maxlen, strlen (tmp))] = '\0';
		}
		break;
	case GDA_TypeBinary:
	case GDA_TypeBstr:
	case GDA_TypeCurrency:
	case GDA_TypeDate:
	case GDA_TypeError:
	case GDA_TypeLongvarbin:
	case GDA_TypeLongvarwchar:
	case GDA_TypeTinyint:
	case GDA_TypeUBigint:
	case GDA_TypeUSmallint:
	case GDA_TypeVarwchar:
	case GDA_TypeFixchar:
	case GDA_TypeFixbin:
	case GDA_TypeLastValue:
	case GDA_TypeFixwchar:
		if (!bfr) {
			retval = g_new0 (gchar, 128);
			maxlen = 128;
		}
		g_print ("stringify for valuetype [%d]'%s' NYI\n",
			 f->attributes->gdaType,
			 gda_fieldtype_2_string (0, 20, gda_field_get_gdatype (f)));
		retval[0] = '\0';
		break;
	}
	return retval;
}

/**
 * gda_field_new:
 * Allocate space for a new field object.
 *
 * Returns: the pointer to the new field object.
 */
GdaField *
gda_field_new (void)
{
	return GDA_FIELD (gtk_type_new (gda_field_get_type ()));
}

/**
 * gda_field_free:
 * @f: a pointer to a GdaField object
 *
 * Free the memory allocated for this field object.
 *
 */
void
gda_field_free (GdaField * f)
{
	g_return_if_fail (GDA_IS_FIELD (f));
	gtk_object_unref (GTK_OBJECT (f));
}

/**
 * gda_field_get_date_value
 */
GDate *
gda_field_get_date_value (GdaField *field)
{
	GDate *dt = NULL;

	g_return_val_if_fail (GDA_IS_FIELD (field), NULL);

	if (field->attributes->gdaType == GDA_TypeDate) {
		struct tm *stm;

		stm = localtime ((time_t *) &field->real_value->_u.v._u.d);
		if (stm != NULL) {
			dt = g_date_new_dmy (stm->tm_mday,
					     stm->tm_mon,
					     stm->tm_year);
		}
	}
	else if (field->attributes->gdaType == GDA_TypeDbDate) {
		dt = g_date_new_dmy (field->real_value->_u.v._u.dbd.day,
				     field->real_value->_u.v._u.dbd.month,
				     field->real_value->_u.v._u.dbd.year);
	}

	return dt;
}

/**
 * gda_field_get_timestamp_value
 */
time_t
gda_field_get_timestamp_value (GdaField *field)
{
	struct tm stm;

	g_return_val_if_fail (GDA_IS_FIELD (field), -1);

	stm.tm_year = field->real_value->_u.v._u.dbtstamp.year;
	stm.tm_mon = field->real_value->_u.v._u.dbtstamp.month;
	stm.tm_mday = field->real_value->_u.v._u.dbtstamp.day;
	stm.tm_hour = field->real_value->_u.v._u.dbtstamp.hour;
	stm.tm_min = field->real_value->_u.v._u.dbtstamp.minute;
	stm.tm_sec = field->real_value->_u.v._u.dbtstamp.second;

	return mktime (&stm);
}

/**
 * gda_field_actual_size:
 * @f: a pointer to the field.
 *
 * Calculates the number of bytes the value fo the field needs.
 *
 * Returns: the number of bytes the value for the field needs or 0 if
 * the field has a NULL value.
 */
gint
gda_field_actual_size (GdaField * f)
{
	g_return_val_if_fail (GDA_IS_FIELD (f), 0);

	if (gda_field_is_null (f))
		return 0;

	switch (gda_field_get_gdatype (f)) {
	case GDA_TypeTinyint:
		return sizeof (CORBA_char);
	case GDA_TypeBigint:
		return sizeof (CORBA_long_long);
	case GDA_TypeBoolean:
		return sizeof (CORBA_boolean);
	case GDA_TypeDate:
		return sizeof (GDA_Date);
	case GDA_TypeDbDate:
		return sizeof (GDA_DbDate);
	case GDA_TypeDbTime:
		return sizeof (GDA_DbTime);
	case GDA_TypeDbTimestamp:
		return sizeof (GDA_DbTimestamp);
	case GDA_TypeCurrency:
	case GDA_TypeDecimal:
	case GDA_TypeNumeric:
		return strlen (gda_field_get_string_value (f));
	case GDA_TypeDouble:
		return sizeof (CORBA_double);
		/* case GDA_TypeError: NYI */
	case GDA_TypeInteger:
		return sizeof (CORBA_long);
	case GDA_TypeVarbin:
	case GDA_TypeVarwchar:
	case GDA_TypeLongvarwchar:
	case GDA_TypeLongvarbin:
		return -1;
	case GDA_TypeFixbin:
	case GDA_TypeFixwchar:
	case GDA_TypeFixchar:
		return -1;
	case GDA_TypeChar:
	case GDA_TypeVarchar:
	case GDA_TypeLongvarchar:
		return strlen (gda_field_get_string_value (f));
	case GDA_TypeSingle:
		return sizeof (CORBA_float);
	case GDA_TypeSmallint:
		return sizeof (CORBA_short);
	case GDA_TypeUBigint:
		return sizeof (CORBA_unsigned_long_long);
	case GDA_TypeUSmallint:
		return sizeof (CORBA_unsigned_short);
	default:
		break;
	}
	g_warning ("gda_field_actual_size: unknown GDA Type %d\n",
		   gda_field_get_gdatype (f));
	return -1;
}
