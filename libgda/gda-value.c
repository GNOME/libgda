/* GDA common library
 * Copyright (C) 1998 - 2007 The GNOME Foundation.
 *
 * AUTHORS:
 *	Michael Lausch <michael@lausch.at>
 *	Rodrigo Moya <rodrigo@gnome-db.org>
 *	Gonzalo Paniagua Javier <gonzalo@gnome-db.org>
 *	Juan-Mariano de Goyeneche <jmseyas@dit.upm.es> (BLOB issues)
 *      Daniel Espinosa Ortiz <esodan@gmail.com> (Port to GValue)
 *      Vivien Malerba <malerba@gnome-db.org>
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

#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <glib/gdate.h>
#include <glib/gmessages.h>
#include <glib/gstrfuncs.h>
#include <glib/gstring.h>
#include <glib/gi18n-lib.h>
#include <libgda/gda-value.h>
#include <libgda/gda-blob-op.h>
#include <libgda/gda-util.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

#define l_g_value_unset(val) if G_IS_VALUE (val) g_value_unset (val)



static gboolean
set_from_string (GValue *value, const gchar *as_string)
{
	gboolean retval;
	gchar *endptr [1];
	gdouble dvalue;
	glong lvalue;
        gulong ulvalue;
	GType type;

	g_return_val_if_fail (value, FALSE);
	if (! G_IS_VALUE (value)) {
		g_warning ("Can't determine the GType of a NULL GValue");
		return FALSE;
	}

	type = G_VALUE_TYPE (value);
	gda_value_reset_with_type (value, type);

	if (g_value_type_transformable (G_TYPE_STRING, type)) {
		/* use the GLib type transformation function */
		GValue *string;

                string = g_new0 (GValue, 1);
                g_value_init (string, G_TYPE_STRING);
                g_value_set_string (string, as_string);

                g_value_transform (string, value);
                gda_value_free (string);

                return TRUE;
	}

	/* custom transform function */
	retval = FALSE;
	if (type == G_TYPE_BOOLEAN) {
		if (g_ascii_strcasecmp (as_string, "true") == 0) {
			g_value_set_boolean (value, TRUE);
			retval = TRUE;
		}
		else {
			if (g_ascii_strcasecmp (as_string, "false") == 0) {
				g_value_set_boolean (value, FALSE);
				retval = TRUE;
			}
		}
	}
	else if (type == GDA_TYPE_BINARY) {
		GdaBinary binary;
		retval = gda_string_to_binary (as_string, &binary);
		if (retval)
			gda_value_set_binary (value, &binary);
	}
	else if (type == GDA_TYPE_BLOB) {
		GdaBlob blob;
		retval = gda_string_to_blob (as_string, &blob);
		if (retval)
			gda_value_set_blob (value, &blob);
	}
	else if (type == G_TYPE_INT64) {
		/* Use g_strtod instead of strtoll */
		dvalue = g_strtod (as_string, endptr);
		if (*as_string != '\0' && **endptr == '\0'){
			g_value_set_int64 (value, (gint64) dvalue);
			retval = TRUE;
		}
	}
	else if (type == G_TYPE_UINT64) {
 	        dvalue = g_strtod (as_string, endptr);
                if (*as_string!=0 && **endptr==0) {
			g_value_set_uint64(value,(guint64)dvalue);
			retval = TRUE;
                }
	}
	else if (type == G_TYPE_INT) {
		lvalue = strtol (as_string, endptr, 10);
		if (*as_string != '\0' && **endptr == '\0'){
			g_value_set_int (value, (gint32) lvalue);
			retval = TRUE;
		}
	}
	else if (type == G_TYPE_UINT) {
		ulvalue = strtoul (as_string, endptr, 10);
		if (*as_string!=0 && **endptr==0) {
			g_value_set_uint(value,(guint32)ulvalue);
			retval = TRUE;
		}
	}
	else if (type == GDA_TYPE_SHORT) {
		lvalue = strtol (as_string, endptr, 10);
		if (*as_string != '\0' && **endptr == '\0'){
			gda_value_set_short (value, (gint16) lvalue);
			retval = TRUE;
		}
	}
	else if (type == GDA_TYPE_USHORT) {
		ulvalue = strtoul (as_string, endptr, 10);
		if (*as_string!=0 && **endptr==0) {
			gda_value_set_ushort(value,(guint16)ulvalue);
			retval = TRUE;
		}
	}
	else if (type == G_TYPE_CHAR) {
		lvalue = strtol(as_string, endptr, 10);
		if (*as_string!=0 && **endptr==0) {
			g_value_set_char(value,(gchar)lvalue);
			retval = TRUE;
		}
	}
	else if (type == G_TYPE_UCHAR) {
		ulvalue = strtoul(as_string,endptr, 10);
		if (*as_string!=0 && **endptr==0) {
			g_value_set_uchar(value,(guchar)ulvalue);
			retval = TRUE;
		}
	}
	else if (type == G_TYPE_FLOAT) {
		dvalue = g_strtod (as_string, endptr);
		if (*as_string != '\0' && **endptr == '\0'){
			g_value_set_float (value, (gfloat) dvalue);
			retval = TRUE;
		}
	}
	else if (type == G_TYPE_DOUBLE) {
		dvalue = g_strtod (as_string, endptr);
		if (*as_string != '\0' && **endptr == '\0'){
			g_value_set_double (value, dvalue);
			retval = TRUE;
		}
	}
	else if (type == GDA_TYPE_NUMERIC) {
		GdaNumeric numeric;
		/* FIXME: what test whould i do for numeric? */
		numeric.number = g_strdup (as_string);
		numeric.precision = 0; /* FIXME */
		numeric.width = 0; /* FIXME */
		gda_value_set_numeric (value, &numeric);
		g_free (numeric.number);
		retval = TRUE;
	}
	else if (type == G_TYPE_DATE) {
		GDate *gdate;
		gdate = g_date_new ();
		g_date_set_parse (gdate, as_string);
		if (g_date_valid (gdate)) {
			g_value_take_boxed (value, gdate);
			retval = TRUE;
		}
		else
			g_date_free (gdate);
	}
	else if (type == GDA_TYPE_NULL)
		gda_value_set_null (value);
	else if (type == G_TYPE_ULONG)
		g_value_set_ulong (value, gda_g_type_from_string (as_string));

	return retval;
}

/* 
 * Register the GdaBinary type in the GType system 
 */

/* Transform a String GValue to a GdaBinary*/
static void 
string_to_binary (const GValue *src, GValue *dest) 
{
	/* FIXME: add more checks*/
	GdaBinary *binary;
	const gchar *as_string;
	
	g_return_if_fail (G_VALUE_HOLDS_STRING (src) &&
			  GDA_VALUE_HOLDS_BINARY (dest));
	
	as_string = g_value_get_string (src);
	
	binary = g_new0 (GdaBinary, 1);
	gda_string_to_binary (as_string, binary);
	gda_value_take_binary (dest, binary);
}

static void 
binary_to_string (const GValue *src, GValue *dest) 
{
	gchar *str;
	
	g_return_if_fail (G_VALUE_HOLDS_STRING (dest) &&
			  GDA_VALUE_HOLDS_BINARY (src));
	
	str = gda_binary_to_string (gda_value_get_binary ((GValue *) src), 0);
	
	g_value_set_string (dest, str);
}

GType
gda_binary_get_type (void)
{
	static GType type = 0;
	
	if (G_UNLIKELY (type == 0)) {
		type = g_boxed_type_register_static ("GdaBinary",
						     (GBoxedCopyFunc) gda_binary_copy,
						     (GBoxedFreeFunc) gda_binary_free);
		
		g_value_register_transform_func (G_TYPE_STRING,
						 type,
						 string_to_binary);
		
		g_value_register_transform_func (type, 
						 G_TYPE_STRING,
						 binary_to_string);
	}
	
	return type;
}

/**
 * gda_binary_copy
 * @boxed: source to get a copy from.
 *
 * Creates a new #GdaBinary structure from an existing one.

 * Returns: a newly allocated #GdaBinary which contains a copy of
 * information in @boxed.
 */
gpointer
gda_binary_copy (gpointer boxed)
{
	GdaBinary *src = (GdaBinary*) boxed;
	GdaBinary *copy = NULL;

	g_return_val_if_fail (src, NULL);

	copy = g_new0 (GdaBinary, 1);
	copy->data = g_memdup (src->data, src->binary_length);
	copy->binary_length = src->binary_length;
	
	return copy;
}

/**
 * gda_binary_free
 * @boxed: #GdaBinary to free.
 *
 * Deallocates all memory associated to the given #GdaBinary.
 */
void
gda_binary_free (gpointer boxed)
{
	GdaBinary *binary = (GdaBinary*) boxed;
	
	g_return_if_fail (binary);
		
	g_free (binary->data);
}

/* 
 * Register the GdaBlob type in the GType system 
 */
/* Transform a String GValue to a GdaBlob*/
static void 
string_to_blob (const GValue *src, GValue *dest) 
{
	/* FIXME: add more checks*/
	GdaBlob *blob;
	const gchar *as_string;
	
	g_return_if_fail (G_VALUE_HOLDS_STRING (src) &&
			  GDA_VALUE_HOLDS_BLOB (dest));
	
	as_string = g_value_get_string (src);
	
	blob = g_new0 (GdaBlob, 1);
	gda_string_to_blob (as_string, blob);
	gda_value_take_blob (dest, blob);
}

static void 
blob_to_string (const GValue *src, GValue *dest) 
{
	gchar *str;
	
	g_return_if_fail (G_VALUE_HOLDS_STRING (dest) &&
			  GDA_VALUE_HOLDS_BLOB (src));
	
	str = gda_blob_to_string ((GdaBlob *) gda_value_get_blob ((GValue *) src), 0);
	
	g_value_set_string (dest, str);
}

GType
gda_blob_get_type (void)
{
	static GType type = 0;
	
	if (G_UNLIKELY (type == 0)) {
		type = g_boxed_type_register_static ("GdaBlob",
						     (GBoxedCopyFunc) gda_blob_copy,
						     (GBoxedFreeFunc) gda_blob_free);
		
		g_value_register_transform_func (G_TYPE_STRING,
						 type,
						 string_to_blob);
		
		g_value_register_transform_func (type, 
						 G_TYPE_STRING,
						 blob_to_string);
	}
	
	return type;
}

/**
 * gda_blob_copy
 * @boxed: source to get a copy from.
 *
 * Creates a new #GdaBlob structure from an existing one.

 * Returns: a newly allocated #GdaBlob which contains a copy of
 * information in @boxed.
 */
gpointer
gda_blob_copy (gpointer boxed)
{
	GdaBlob *src = (GdaBlob*) boxed;
	GdaBlob *copy = NULL;

	g_return_val_if_fail (src, NULL);

	copy = g_new0 (GdaBlob, 1);
	((GdaBinary *)copy)->data = g_memdup (((GdaBinary *)src)->data, ((GdaBinary *)src)->binary_length);
	((GdaBinary *)copy)->binary_length = ((GdaBinary *)src)->binary_length;
	gda_blob_set_op (copy, src->op);
	
	return copy;
}

/**
 * gda_blob_free
 * @boxed: #GdaBlob to free.
 *
 * Deallocates all memory associated to the given #GdaBlob.
 */
void
gda_blob_free (gpointer boxed)
{
	GdaBlob *blob = (GdaBlob*) boxed;
	
	g_return_if_fail (blob);
		
	if (blob->op)
		g_object_unref (blob->op);
	gda_binary_free ((GdaBinary *) blob);
}

/**
 * gda_blob_set_op 
 * @blob: a #GdaBlob value
 * @op: a #GdaBlobOp object, or %NULL
 *
 * correctly assigns @op to @blob
 */
void
gda_blob_set_op (GdaBlob *blob, GdaBlobOp *op)
{
	if (blob->op) {
		g_object_unref (blob->op);
		blob->op = NULL;
	}
	if (op) {
		g_return_if_fail (GDA_IS_BLOB_OP (op));
		blob->op = op;
		g_object_ref (op);
	}
}

/* 
 * Register the GdaGeometricPoint type in the GType system 
 */
static void
geometric_point_to_string (const GValue *src, GValue *dest) 
{
	gchar *str;
	GdaGeometricPoint *point;
	
	g_return_if_fail (G_VALUE_HOLDS_STRING (dest) &&
			  GDA_VALUE_HOLDS_GEOMETRIC_POINT (src));
	
	point = (GdaGeometricPoint *) gda_value_get_geometric_point ((GValue *) src);
	
	str = g_strdup_printf ("(%.*g,%.*g)",
				  DBL_DIG,
				  point->x,
				  DBL_DIG,
				  point->y);
	
	g_value_set_string(dest, str);
}

/* Transform a String GValue to a GdaGeometricPoint from a string like "(3.2,5.6)" */
static void
string_to_geometricpoint (const GValue *src, GValue *dest)
{
	/* FIXME: add more checks*/
	GdaGeometricPoint *point;
	const gchar *as_string;
	
	g_return_if_fail (G_VALUE_HOLDS_STRING (src) &&
			  GDA_VALUE_HOLDS_GEOMETRIC_POINT (dest));
	
	as_string = g_value_get_string (src);
	point = g_new0 (GdaGeometricPoint, 1);
	
	as_string++;
	point->x = atof (as_string);
	as_string = strchr (as_string, ',');
	as_string++;
	point->y = atof (as_string);

	gda_value_set_geometric_point (dest, point);
	g_free (point);
}

GType
gda_geometricpoint_get_type (void)
{
	static GType type = 0;
	
	if (G_UNLIKELY (type == 0)) {
		type = g_boxed_type_register_static ("GdaGeometricPoint",
						     (GBoxedCopyFunc) gda_geometricpoint_copy,
						     (GBoxedFreeFunc) gda_geometricpoint_free);
		
		g_value_register_transform_func (G_TYPE_STRING,
						 type,
						 string_to_geometricpoint);
		
		g_value_register_transform_func (type, 
						 G_TYPE_STRING,
						 geometric_point_to_string);
	}
	
	return type;
}

gpointer 
gda_geometricpoint_copy (gpointer boxed)
{
	GdaGeometricPoint *val = (GdaGeometricPoint*) boxed;
	GdaGeometricPoint *copy;
	
	g_return_val_if_fail( val, NULL);
		
	copy = g_new0(GdaGeometricPoint, 1);
	copy->x = val->x;
	copy->y = val->y;

	return copy;
}

void gda_geometricpoint_free (gpointer boxed)
{
	
}




/* 
 * Register the GdaValueList type in the GType system 
 */
gpointer gda_value_list_copy (gpointer boxed);
void gda_value_list_free (gpointer boxed);

static void 
list_to_string (const GValue *src, GValue *dest) 
{
	gchar *str;
	const GdaValueList *list;
	GList *l;
	GString *gstr;
	
	g_return_if_fail (G_VALUE_HOLDS_STRING (dest) &&
			  GDA_VALUE_HOLDS_LIST (src));
	
	list = gda_value_get_list ((GValue *) src);
	
	for (l = (GList *) list; l != NULL; l = l->next) {
		gchar *s;

		s = gda_value_stringify ((GValue *) l->data);
		if (!str) {
			gstr = g_string_new ("{ \"");
			gstr = g_string_append (gstr, s);
			gstr = g_string_append (gstr, "\"");
		}
		else {
			gstr = g_string_append (gstr, ", \"");
			gstr = g_string_append (gstr, s);
			gstr = g_string_append (gstr, "\"");
		}
		g_free (s);
	}

	if (gstr) {
		g_string_append (gstr, " }");
		str = gstr->str;
		g_string_free (gstr, FALSE);
	}
	else
		str = g_strdup ("");
	
	g_value_take_string (dest, str);
}

GType
gda_value_list_get_type(void)
{
	static GType type = 0;
	
	if (G_UNLIKELY (type == 0)) {
		type = g_boxed_type_register_static ("GdaValueList",
						     (GBoxedCopyFunc) gda_value_list_copy,
						     (GBoxedFreeFunc) gda_value_list_free);
		
		g_value_register_transform_func (type, 
						 G_TYPE_STRING,
						 list_to_string);
		/* FIXME: No function to transform from string to a GdaValueList */
	}

	return type;
}

gpointer 
gda_value_list_copy (gpointer boxed)
{
	GList *list = NULL;
	const GList *values;
	
	values = (GList*) boxed;
	
	while (values) {
		list = g_list_append (list, gda_value_copy ((GValue *) (values->data)));
		values = values->next;
	}

	return list;
}

void gda_value_list_free (gpointer boxed)
{
	GList *l = (GList*) boxed;
	g_list_free(l);
}





/* 
 * Register the GdaNumeric type in the GType system 
 */
static void 
numeric_to_string (const GValue *src, GValue *dest) 
{
	const GdaNumeric *numeric;
	
	g_return_if_fail (G_VALUE_HOLDS_STRING (dest) &&
			  GDA_VALUE_HOLDS_NUMERIC (src));

	numeric = gda_value_get_numeric (src);
	if (numeric)
		g_value_set_string (dest, numeric->number);
	else
		g_value_set_string (dest, "");
}

static void 
numeric_to_int (const GValue *src, GValue *dest) 
{
	const GdaNumeric *numeric;
	
	g_return_if_fail (G_VALUE_HOLDS_INT (dest) &&
			  GDA_VALUE_HOLDS_NUMERIC (src));

	numeric = gda_value_get_numeric (src);
	if (numeric)
		g_value_set_int (dest, atol (numeric->number));
	else
		g_value_set_int (dest, 0);
}

static void 
numeric_to_uint (const GValue *src, GValue *dest) 
{
	const GdaNumeric *numeric;
	
	g_return_if_fail (G_VALUE_HOLDS_UINT (dest) &&
			  GDA_VALUE_HOLDS_NUMERIC (src));

	numeric = gda_value_get_numeric (src);
	if (numeric)
		g_value_set_uint (dest, atol (numeric->number));
	else
		g_value_set_uint (dest, 0);
}

static void 
numeric_to_boolean (const GValue *src, GValue *dest) 
{
	const GdaNumeric *numeric;
	
	g_return_if_fail (G_VALUE_HOLDS_BOOLEAN (dest) &&
			  GDA_VALUE_HOLDS_NUMERIC (src));

	numeric = gda_value_get_numeric (src);
	if (numeric)
		g_value_set_boolean (dest, atoi (numeric->number));
	else
		g_value_set_boolean (dest, 0);
}

GType
gda_numeric_get_type (void)
{
	static GType type = 0;
	
	if (G_UNLIKELY (type == 0)) {
		type = g_boxed_type_register_static ("GdaNumeric",
						     (GBoxedCopyFunc) gda_numeric_copy,
						     (GBoxedFreeFunc) gda_numeric_free);
	
		/* FIXME: No function to Transform String to from GdaNumeric */
	
		g_value_register_transform_func (type, G_TYPE_STRING, numeric_to_string);
		g_value_register_transform_func (type, G_TYPE_INT, numeric_to_int);
		g_value_register_transform_func (type, G_TYPE_UINT, numeric_to_uint);
		g_value_register_transform_func (type, G_TYPE_BOOLEAN, numeric_to_boolean);
	}
	
	return type;
}

/**
 * gda_numeric_copy
 * @boxed: source to get a copy from.
 *
 * Creates a new #GdaNumeric structure from an existing one.

 * Returns: a newly allocated #GdaNumeric which contains a copy of
 * information in @boxed.
 */

gpointer
gda_numeric_copy (gpointer boxed)
{
	GdaNumeric *src = (GdaNumeric*) boxed;
	GdaNumeric *copy = NULL;

	g_return_val_if_fail (src, NULL);

	copy = g_new0 (GdaNumeric, 1);
	copy->number = g_strdup (src->number);
	copy->precision = src->precision;
	copy->width = src->width;  

	return copy;
}

/**
 * gda_numeric_free
 * @boxed:
 *
 * Deallocates all memory associated to the given @boxed
 */
void
gda_numeric_free (gpointer boxed)
{
	GdaNumeric *numeric = (GdaNumeric*) boxed;
	g_return_if_fail (numeric);

	g_free (numeric->number);

	g_free (numeric);
}



/* 
 * Register the GdaTime type in the GType system 
 */

static void 
time_to_string (const GValue *src, GValue *dest) 
{
	GdaTime *gdatime;
	GString *string;
	
	g_return_if_fail (G_VALUE_HOLDS_STRING (dest) &&
			  GDA_VALUE_HOLDS_TIME (src));
	
	gdatime = (GdaTime *) gda_value_get_time ((GValue *) src);
	
	string = g_string_new ("");
	g_string_append_printf (string, "%02u:%02u:%02u",
				gdatime->hour,
				gdatime->minute,
				gdatime->second);
	if (gdatime->fraction != 0)
		g_string_append_printf (string, ".%lu", gdatime->fraction);
	
	if (gdatime->timezone != GDA_TIMEZONE_INVALID)
		g_string_append_printf (string, "%+02d", (int) gdatime->timezone / 3600);

	g_value_take_string (dest, string->str);
	g_string_free (string, FALSE);
}

/* Transform a String GValue to a GdaTime from a string like "12:30:15+01" */
static void
string_to_time (const GValue *src, GValue *dest)
{
	/* FIXME: add more checks*/
	GdaTime *timegda;
	const gchar *as_string;
	const gchar *ptr;
	
	g_return_if_fail (G_VALUE_HOLDS_STRING (src) &&
			  GDA_VALUE_HOLDS_TIME (dest));
       
	as_string = g_value_get_string (src);
	if (!as_string)
		return;
	timegda = g_new0 (GdaTime, 1);

	/* hour */
	timegda->timezone = GDA_TIMEZONE_INVALID;
	ptr = as_string;
	if ((*ptr >= '0') && (*ptr <= '9') &&
	    (*(ptr+1) >= '0') && (*(ptr+1) <= '9'))
		timegda->hour = (*ptr - '0') * 10 + *(ptr+1) - '0';
	else {
		g_free (timegda);
		return;
	}

	/* minute */
	ptr += 2;
	if (! *ptr) {
		g_free (timegda);
		return;
	}
	if (*ptr == ':')
		ptr++;
	if ((*ptr >= '0') && (*ptr <= '9') &&
	    (*(ptr+1) >= '0') && (*(ptr+1) <= '9'))
		timegda->minute = (*ptr - '0') * 10 + *(ptr+1) - '0';
	else{
		g_free (timegda);
		return;
	}

	/* second */
	ptr += 2;
	timegda->second = 0;
	if (! *ptr) {
		if ((timegda->hour <= 24) && (timegda->minute <= 60))
			gda_value_set_time (dest, timegda);
		g_free (timegda);
		return;
	}
	if (*ptr == ':')
		ptr++;
	if ((*ptr >= '0') && (*ptr <= '9') &&
	    (*(ptr+1) >= '0') && (*(ptr+1) <= '9'))
		timegda->second = (*ptr - '0') * 10 + *(ptr+1) - '0';
	
	/* extra */
	ptr += 2;
	if (! *ptr) {
		if ((timegda->hour <= 24) && (timegda->minute <= 60) && 
		    (timegda->second <= 60))
			gda_value_set_time (dest, timegda);
		g_free (timegda);
		return;
	}

	if (*ptr == '.') {
		gulong fraction = 0;

		ptr ++;
		while (*ptr && (*ptr >= '0') && (*ptr <= '9')) {
			fraction = fraction * 10 + *ptr - '0';
			ptr++;
		}
	}

	if ((*ptr == '+') || (*ptr == '-')) {
		glong sign = (*ptr == '+') ? 1 : -1;
		timegda->timezone = 0;
		while (*ptr && (*ptr >= '0') && (*ptr <= '9')) {
			timegda->timezone = timegda->timezone * 10 + sign * ((*ptr) - '0');
			ptr++;
		}
		timegda->timezone *= 3600;
	}
	
	/* checks */
	if ((timegda->hour <= 24) || (timegda->minute <= 60) || (timegda->second <= 60))
		gda_value_set_time (dest, timegda);
	g_free (timegda);
}

GType
gda_time_get_type(void)
{
	static GType type = 0;
	
	if (G_UNLIKELY (type == 0)) {
		type = g_boxed_type_register_static ("GdaTime",
						     (GBoxedCopyFunc) gda_time_copy,
						     (GBoxedFreeFunc) gda_time_free);
	
		g_value_register_transform_func (G_TYPE_STRING, type, string_to_time);
		g_value_register_transform_func (type, G_TYPE_STRING, time_to_string);
	}
	
	return type;
}

gpointer
gda_time_copy (gpointer boxed)
{
	
	GdaTime *src = (GdaTime*) boxed;
	GdaTime *copy = NULL;
	
	g_return_val_if_fail(src, NULL);
	
	copy = g_new0(GdaTime, 1);
	copy->hour = src->hour;
	copy->minute = src->minute;
	copy->second = src->second;
	copy->fraction = src->fraction;
	copy->timezone = src->timezone;

	return copy;
}

void gda_time_free (gpointer boxed)
{
	
}



/* 
 * Register the GdaTimestamp type in the GType system 
 */
/* Transform a String GValue to a GdaTimestamp from a string like "2003-12-13 13:12:01.12+01" */
static void
string_to_timestamp (const GValue *src, GValue *dest)
{
	/* FIXME: add more checks*/
	GdaTimestamp *timestamp;
	const gchar *as_string;
	
	g_return_if_fail (G_VALUE_HOLDS_STRING (src) &&
			  GDA_VALUE_HOLDS_TIMESTAMP (dest));
	
	timestamp = g_new0 (GdaTimestamp, 1);
	
	as_string = g_value_get_string (src);
	
	timestamp->year = atoi (as_string);
	as_string += 5;
	timestamp->month = atoi (as_string);
	as_string += 3;
	timestamp->day = atoi (as_string);
	as_string += 3;
	timestamp->hour = atoi (as_string);
	as_string += 3;
	timestamp->minute = atoi (as_string);
	as_string += 3;
	timestamp->second = atoi (as_string);
	if (strlen(as_string)>=3) {
		as_string += 3;
		timestamp->fraction = atol (as_string) * 10; /* I have only hundredths of second */
		if (strlen(as_string)>=3) {
			as_string += 3;
			timestamp->timezone = atol (as_string) * 60 * 60;
		}
	}
	gda_value_set_timestamp (dest, timestamp);
	g_free (timestamp);
}

static void 
timestamp_to_string (const GValue *src, GValue *dest) 
{
	GdaTimestamp *timestamp;
	GString *string;
	
	g_return_if_fail (G_VALUE_HOLDS_STRING (dest) &&
			  GDA_VALUE_HOLDS_TIMESTAMP (src));
	
	timestamp = (GdaTimestamp *) gda_value_get_timestamp ((GValue *) src);
	
	string = g_string_new ("");
	g_string_append_printf (string, "%04u-%02u-%02u %02u:%02u:%02u",
				timestamp->year,
				timestamp->month,
				timestamp->day,
				timestamp->hour,
				timestamp->minute,
				timestamp->second);
	if (timestamp->fraction > 0)
		g_string_append_printf (string, ".%lu", timestamp->fraction);
	if (timestamp->timezone != GDA_TIMEZONE_INVALID)
		g_string_append_printf (string, "%+02d",
					(int) timestamp->timezone/3600);
	g_value_take_string (dest, string->str);
	g_string_free (string, FALSE);
}

GType
gda_timestamp_get_type (void)
{
	static GType type = 0;
	
	if (G_UNLIKELY (type == 0)) {
		type = g_boxed_type_register_static ("GdaTimestamp",
						     (GBoxedCopyFunc) gda_timestamp_copy,
						     (GBoxedFreeFunc) gda_timestamp_free);
	
		g_value_register_transform_func (G_TYPE_STRING, type, string_to_timestamp);
		g_value_register_transform_func (type, G_TYPE_STRING, timestamp_to_string);
	}
	
	return type;
}

gpointer 
gda_timestamp_copy (gpointer boxed)
{
	GdaTimestamp *src = (GdaTimestamp*) boxed;
	GdaTimestamp *copy;
	
	g_return_val_if_fail(src, NULL);
	
	copy = g_new0(GdaTimestamp, 1);
	copy->year = src->year;
	copy->month = src->month;
	copy->day = src->day;
	copy->hour = src->hour;
	copy->minute = src->minute;
	copy->second = src->second;
	copy->fraction = src->fraction;
	copy->timezone = src->timezone;
	
	return copy;
}

void gda_timestamp_free (gpointer boxed)
{
	
}



/**
 * gda_value_new
 * @type: the new value type.
 *
 * Makes a new #GValue of type @type.
 *
 * Returns: the newly created #GValue with the specified @type. 
 *	You need to set the value in the returned GValue.
 */
GValue *
gda_value_new (GType type)
{
	GValue *value;

	value = g_new0 (GValue, 1);
	g_value_init (value, type);

	return value;
}

/**
 * gda_value_new_binary
 * @val: value to set for the new #GValue.
 * @size: the size of the memory pool pointer to by @val.
 *
 * Makes a new #GValue of type #GDA_TYPE_BINARY with value @val.
 *
 * Returns: the newly created #GValue.
 */
GValue *
gda_value_new_binary (const guchar *val, glong size)
{
	GValue *value;
	GdaBinary binary;

	/* We use the const on the function parameter to make this clearer, 
	 * but it would be awkward to keep the const in the struct.
         */
        binary.data = (guchar*)val;
        binary.binary_length = size;

        value = g_new0 (GValue, 1);
        gda_value_set_binary (value, &binary);

        return value;
}

/**
 * gda_value_new_blob
 * @val: value to set for the new #GValue.
 * @size: the size of the memory pool pointer to by @val.
 *
 * Makes a new #GValue of type #GDA_TYPE_BLOB with value @val.
 *
 * Returns: the newly created #GValue.
 */
GValue *
gda_value_new_blob (const guchar *val, glong size)
{
	GValue *value;
	GdaBlob blob;
	GdaBinary *bin;

	bin = (GdaBinary*)(&blob);

        /* We use the const on the function parameter to make this clearer, 
	 * but it would be awkward to keep the const in the struct.
         */
        bin->data = (guchar*)val;
        bin->binary_length = size;
	blob.op = NULL;

        value = g_new0 (GValue, 1);
        gda_value_set_blob (value, &blob);

        return value;
}

/**
 * gda_value_new_timestamp_from_timet
 * @val: value to set for the new #GValue.
 *
 * Makes a new #GValue of type #GDA_TYPE_TIMESTAMP with value @val
 * (of type time_t).
 *
 * Returns: the newly created #GValue.
 */
GValue *
gda_value_new_timestamp_from_timet (time_t val)
{
	GValue *value;

	struct tm *ltm;

        value = g_new0 (GValue, 1);
        ltm = localtime ((const time_t *) &val);
        if (ltm) {
                GdaTimestamp tstamp;
                tstamp.year = ltm->tm_year;
                tstamp.month = ltm->tm_mon;
                tstamp.day = ltm->tm_mday;
                tstamp.hour = ltm->tm_hour;
                tstamp.minute = ltm->tm_min;
                tstamp.second = ltm->tm_sec;
                tstamp.fraction = 0;
                tstamp.timezone = GDA_TIMEZONE_INVALID;
                gda_value_set_timestamp (value, (const GdaTimestamp *) &tstamp);
        }

        return value;
}

/**
 * gda_value_new_from_string
 * @as_string: stringified representation of the value.
 * @type: the new value type.
 *
 * Makes a new #GValue of type @type from its string representation.
 *
 * Returns: the newly created #GValue or %NULL if the string representation
 * cannot be converted to the specified @type.
 */
GValue *
gda_value_new_from_string (const gchar *as_string, GType type)
{
	GValue *value;
	
	g_return_val_if_fail (as_string, NULL);
	
	value = gda_value_new (type);
	if (set_from_string (value, as_string))
		return value;
	else {
		gda_value_free (value);
		return NULL;
	}
}

/**
 * gda_value_new_from_xml
 * @node: a XML node representing the value.
 *
 * Creates a GValue from a XML representation of it. That XML
 * node corresponds to the following string representation:
 *    &lt;value type="gdatype"&gt;value&lt;/value&gt;
 *
 * Returns:  the newly created #GValue.
 */
GValue *
gda_value_new_from_xml (const xmlNodePtr node)
{
	GValue *value;

	g_return_val_if_fail (node, NULL);

	/* parse the XML */
	if (!node || !(node->name) || (node && strcmp ((gchar*)node->name, "value"))) 
		return NULL;

	value = g_new0 (GValue, 1);
	if (!gda_value_set_from_string (value,
					(gchar*)xmlNodeGetContent (node),
					g_type_from_name ((gchar*)xmlGetProp(node, (xmlChar*)"gdatype")))) {
		g_free (value);
		value = NULL;
	}

	return value;
}

/**
 * gda_value_free
 * @value: the resource to free.
 *
 * Deallocates all memory associated to a #GValue.
 */
void
gda_value_free (GValue *value)
{
	g_return_if_fail (value);

	l_g_value_unset (value);
	g_free (value);
}

/* gda_value_reset_with_type
 * @value: the #GValue to be reseted
 * @type:  the #GType to set to
 *
 * Resets the #GValue and set a new type to #GType.
*/
void
gda_value_reset_with_type (GValue *value, GType type)
{
	g_return_if_fail (value);

	l_g_value_unset (value);
	if (type == GDA_TYPE_NULL || type == G_TYPE_INVALID)
		return;
	else
		g_value_init (value, type);
}



/**
 * gda_value_is_null
 * @value: value to test.
 *
 * Tests if a given @value is of type #GDA_TYPE_NULL.
 *
 * Returns: a boolean that says whether or not @value is of type
 * #GDA_TYPE_NULL.
 */
gboolean
gda_value_is_null (const GValue *value)
{
	g_return_val_if_fail (value, FALSE);
	return !G_IS_VALUE (value);
}

/**
 * gda_value_is_number
 * @value: a #GValue.
 *
 * Gets whether the value stored in the given #GValue is of
 * numeric type or not.
 *
 * Returns: %TRUE if a number, %FALSE otherwise.
 */
gboolean
gda_value_is_number (const GValue *value)
{
	g_return_val_if_fail (value && G_IS_VALUE(value), FALSE);
	if(G_VALUE_HOLDS_INT(value) ||
		G_VALUE_HOLDS_INT64(value) ||
		G_VALUE_HOLDS_UINT(value) ||
		G_VALUE_HOLDS_UINT64(value) ||
		G_VALUE_HOLDS_CHAR(value) ||
		G_VALUE_HOLDS_UCHAR(value))
		return TRUE;
	else
		return FALSE;
}

/**
 * gda_value_copy
 * @value: value to get a copy from.
 *
 * Creates a new #GValue from an existing one.
 * 
 * Returns: a newly allocated #GValue with a copy of the data in @value.
 */
GValue *
gda_value_copy (const  GValue *value)
{
	GValue *copy;
	
	g_return_val_if_fail (value, NULL);

	copy = g_new0 (GValue, 1);

	if (G_IS_VALUE (value)) {
		g_value_init (copy, G_VALUE_TYPE (value));
		g_value_copy (value, copy);
	}

	return copy;
}

/**
 * gda_value_get_binary
 * @value: a #GValue whose value we want to get.
 *
 * Returns: the value stored in @value.
 */
G_CONST_RETURN GdaBinary *
gda_value_get_binary (const GValue *value)
{
	GdaBinary *val;

	g_return_val_if_fail (value && G_IS_VALUE (value), NULL);
	g_return_val_if_fail (gda_value_isa (value, GDA_TYPE_BINARY), NULL);

	val = (GdaBinary*) g_value_get_boxed (value);

	return val;
}


/**
 * gda_value_set_binary
 * @value: a #GValue that will store @val.
 * @binary: a #GdaBinary structure with the data and its size to be stored in @value.
 *
 * Stores @val into @value.
 */
void
gda_value_set_binary (GValue *value, const GdaBinary *binary)
{
	g_return_if_fail (value);
	g_return_if_fail (binary);
	
	l_g_value_unset (value);
	g_value_init (value, GDA_TYPE_BINARY);
	g_value_set_boxed (value, binary);
}

/**
 * gda_value_take_binary
 * @value: a #GValue that will store @val.
 * @binary: a #GdaBinary structure with the data and its size to be stored in @value.
 *
 * Stores @val into @value, but on the contrary to gda_value_set_binary(), the @binary
 * argument is not copied, but used as-is and it should be considered owned by @value.
 */
void
gda_value_take_binary (GValue *value, const GdaBinary *binary)
{
	g_return_if_fail (value);
	g_return_if_fail (binary);
	
	l_g_value_unset (value);
	g_value_init (value, GDA_TYPE_BINARY);
	g_value_take_boxed (value, binary);
}

/**
 * gda_value_set_blob
 * @value: a #GValue that will store @val.
 * @blob: a #GdaBlob structure with the data and its size to be stored in @value.
 *
 * Stores @val into @value.
 */
void
gda_value_set_blob (GValue *value, const GdaBlob *blob)
{
	g_return_if_fail (value);
	g_return_if_fail (blob);
	
	l_g_value_unset (value);
	g_value_init (value, GDA_TYPE_BLOB);
	g_value_set_boxed (value, blob);
}

/**
 * gda_value_get_blob
 * @value: a #GValue whose value we want to get.
 *
 * Returns: the value stored in @value.
 */
G_CONST_RETURN GdaBlob *
gda_value_get_blob (const GValue *value)
{
	GdaBlob *val;

	g_return_val_if_fail (value && G_IS_VALUE (value), NULL);
	g_return_val_if_fail (gda_value_isa (value, GDA_TYPE_BLOB), NULL);

	val = (GdaBlob*) g_value_get_boxed (value);

	return val;
}

/**
 * gda_value_take_blob
 * @value: a #GValue that will store @val.
 * @blob: a #GdaBlob structure with the data and its size to be stored in @value.
 *
 * Stores @val into @value, but on the contrary to gda_value_set_blob(), the @blob
 * argument is not copied, but used as-is and it should be considered owned by @value.
 */
void
gda_value_take_blob (GValue *value, const GdaBlob *blob)
{
	g_return_if_fail (value);
	g_return_if_fail (blob);
	
	l_g_value_unset (value);
	g_value_init (value, GDA_TYPE_BLOB);
	g_value_take_boxed (value, blob);
}

/**
 * gda_value_get_geometric_point
 * @value: a #GValue whose value we want to get.
 *
 * Returns: the value stored in @value.
 */
G_CONST_RETURN GdaGeometricPoint *
gda_value_get_geometric_point (const GValue *value)
{
	g_return_val_if_fail (value && G_IS_VALUE (value), NULL);
	g_return_val_if_fail (gda_value_isa (value, GDA_TYPE_GEOMETRIC_POINT), NULL);
	return (const GdaGeometricPoint *) g_value_get_boxed(value);
}

/**
 * gda_value_set_geometric_point
 * @value: a #GValue that will store @val.
 * @val: value to be stored in @value.
 *
 * Stores @val into @value.
 */
void
gda_value_set_geometric_point (GValue *value, const GdaGeometricPoint *val)
{
	g_return_if_fail (value);
	g_return_if_fail (val);

	l_g_value_unset (value);	
	g_value_init (value, GDA_TYPE_GEOMETRIC_POINT);
	g_value_set_boxed (value, val);
}

/**
 * gda_value_get_list
 * @value: a #GValue whose value we want to get.
 *
 * Returns: the value stored in @value.
 */
G_CONST_RETURN GdaValueList *
gda_value_get_list (const GValue *value)
{
	g_return_val_if_fail (value && G_IS_VALUE (value), NULL);
	g_return_val_if_fail (gda_value_isa (value, GDA_TYPE_LIST), NULL);
	return (const GdaValueList *) g_value_get_boxed(value);
}

/**
 * gda_value_set_list
 * @value: a #GValue that will store @val.
 * @val: value to be stored in @value.
 *
 * Stores @val into @value.
 */
void
gda_value_set_list (GValue *value, const GdaValueList *val)
{
	g_return_if_fail (value);
	g_return_if_fail (val);

	l_g_value_unset (value);
	g_value_init (value, GDA_TYPE_LIST);
	
	/* See the implementation of GdaValueList as a GBoxed for the Copy function used by GValue*/
	g_value_set_boxed (value, val);
}

/**
 * gda_value_set_null
 * @value: a #GValue that will store a value of type #GDA_TYPE_NULL.
 *
 * Sets the type of @value to #GDA_TYPE_NULL.
 */
void
gda_value_set_null (GValue *value)
{
	g_return_if_fail (value);

	l_g_value_unset (value);
}

/**
 * gda_value_get_numeric
 * @value: a #GValue whose value we want to get.
 *
 * Returns: the value stored in @value.
 */
G_CONST_RETURN GdaNumeric *
gda_value_get_numeric (const GValue *value)
{
	g_return_val_if_fail (value && G_IS_VALUE (value), NULL);
	g_return_val_if_fail (gda_value_isa (value, GDA_TYPE_NUMERIC), NULL);
	return (const GdaNumeric *) g_value_get_boxed(value);
}

/**
 * gda_value_set_numeric
 * @value: a #GValue that will store @val.
 * @val: value to be stored in @value.
 *
 * Stores @val into @value.
 */
void
gda_value_set_numeric (GValue *value, const GdaNumeric *val)
{
	g_return_if_fail (value);
	g_return_if_fail (val);

	l_g_value_unset (value);
	g_value_init (value, GDA_TYPE_NUMERIC);
	g_value_set_boxed (value, val);
}

/**
 * gda_value_get_short
 * @value: a #GValue whose value we want to get.
 *
 * Returns: the value stored in @value.
 */
gshort
gda_value_get_short (const GValue *value)
{
	g_return_val_if_fail (value && G_IS_VALUE (value), -1);
	g_return_val_if_fail (gda_value_isa (value, GDA_TYPE_SHORT), -1);
	return (gshort) value->data[0].v_int;
}

/**
 * gda_value_set_short
 * @value: a #GValue that will store @val.
 * @val: value to be stored in @value.
 *
 * Stores @val into @value.
 */
void
gda_value_set_short (GValue *value, gshort val)
{
	g_return_if_fail (value);

	l_g_value_unset (value);
	g_value_init (value, GDA_TYPE_SHORT);
	value->data[0].v_int = val;
}

/**
 * gda_value_get_ushort
 * @value: a #GValue whose value we want to get.
 *
 * Returns: the value stored in @value.
 */
gushort
gda_value_get_ushort (const GValue *value)
{
	g_return_val_if_fail (value && G_IS_VALUE (value), -1);
	g_return_val_if_fail (gda_value_isa (value, GDA_TYPE_USHORT), -1);
	return (gushort) value->data[0].v_uint;
}

/**
 * gda_value_set_ushort
 * @value: a #GValue that will store @val.
 * @val: value to be stored in @value.
 *
 * Stores @val into @value.
 */
void
gda_value_set_ushort (GValue *value, gushort val)
{
	g_return_if_fail (value);

	l_g_value_unset (value);
	g_value_init (value, GDA_TYPE_USHORT);
	value->data[0].v_uint = val;
}


/**
 * gda_value_get_time
 * @value: a #GValue whose value we want to get.
 *
 * Returns: the value stored in @value.
 */
const GdaTime *
gda_value_get_time (const GValue *value)
{
	g_return_val_if_fail (value && G_IS_VALUE (value), NULL);
	g_return_val_if_fail (gda_value_isa (value, GDA_TYPE_TIME), NULL);
	return (const GdaTime *) g_value_get_boxed(value);
}

/**
 * gda_value_set_time
 * @value: a #GValue that will store @val.
 * @val: value to be stored in @value.
 *
 * Stores @val into @value.
 */
void
gda_value_set_time (GValue *value, const GdaTime *val)
{
	g_return_if_fail (value);
	g_return_if_fail (val);

	l_g_value_unset (value);
	g_value_init (value, GDA_TYPE_TIME);
	g_value_set_boxed (value, val);
}

/**
 * gda_value_get_timestamp
 * @value: a #GValue whose value we want to get.
 *
 * Returns: the value stored in @value.
 */
const GdaTimestamp *
gda_value_get_timestamp (const GValue *value)
{
	g_return_val_if_fail (value && G_IS_VALUE (value), NULL);
	g_return_val_if_fail (gda_value_isa (value, GDA_TYPE_TIMESTAMP), NULL);
	return (const GdaTimestamp *) g_value_get_boxed(value);
}

/**
 * gda_value_set_timestamp
 * @value: a #GValue that will store @val.
 * @val: value to be stored in @value.
 *
 * Stores @val into @value.
 */
void
gda_value_set_timestamp (GValue *value, const GdaTimestamp *val)
{
	g_return_if_fail (value);
	g_return_if_fail (val);

	l_g_value_unset (value);
	g_value_init (value, GDA_TYPE_TIMESTAMP);
	g_value_set_boxed (value, val);
}

/**
 * gda_value_set_from_string
 * @value: a #GValue that will store @val.
 * @as_string: the stringified representation of the value.
 * @type: the type of the value
 *
 * Stores the value data from its string representation as @type.
 *
 * Returns: %TRUE if the value has been converted to @type from
 * its string representation; it not means that the value is converted 
 * successfully, just that the transformation is avairable. %FALSE otherwise.
 */
gboolean
gda_value_set_from_string (GValue *value, 
			   const gchar *as_string,
			   GType type)
{
	g_return_val_if_fail (value, FALSE);
	g_return_val_if_fail (as_string, FALSE);
		
	/* REM: glib does not register any transform function from G_TYPE_STRING to any other
	 * type except to a G_TYPE_STRING, so we can't use g_value_type_transformable (G_TYPE_STRING, type) */
	gda_value_reset_with_type (value, type);
        return set_from_string (value, as_string);
}

/**
 * gda_value_set_from_value
 * @value: a #GValue.
 * @from: the value to copy from.
 *
 * Sets the value of a #GValue from another #GValue. This
 * is different from #gda_value_copy, which creates a new #GValue.
 * #gda_value_set_from_value, on the other hand, copies the contents
 * of @copy into @value, which must already be allocated.
 *
 * Returns: %TRUE if successful, %FALSE otherwise.
 */
gboolean
gda_value_set_from_value (GValue *value, const GValue *from)
{
	g_return_val_if_fail (value, FALSE);
	g_return_val_if_fail (from, FALSE);

	if (G_IS_VALUE (from)) {
		g_value_copy (from, value);
		return g_value_type_compatible (G_VALUE_TYPE (from), G_VALUE_TYPE (value));
	}
	else {
		l_g_value_unset (value);
		return TRUE;
	}
}

/**
 * gda_value_stringify
 * @value: a #GValue.
 *
 * Converts a GValue to its string representation which is a human readable value. Note that the
 * returned string does not take into account the current locale of the user (on the contrary to the
 * #GdaDataHandler objects). Using this function should be limited to debugging and values serialization
 * purposes.
 *
 * Dates are converted in a YYYY-MM-DD format.
 *
 * Returns: a new string, or %NULL if the conversion cannot be done. Free the value with a g_free() when you've finished
 * using it. 
 */
gchar *
gda_value_stringify (const GValue *value)
{
	if (value && G_IS_VALUE (value)) {
		if (g_value_type_transformable (G_VALUE_TYPE (value), G_TYPE_STRING)) {
			GValue *string;
			gchar *str;
			
			string = g_value_init (g_new0 (GValue, 1), G_TYPE_STRING);
			g_value_transform (value, string);
			str = g_value_dup_string (string);
			gda_value_free (string);
			return str;
		}
		else {
			GType type = G_VALUE_TYPE (value);
			if (type == G_TYPE_DATE) {
				GDate *date;
				date = (GDate *) g_value_get_boxed (value);
				return g_strdup_printf ("%04u-%02u-%02u",
							g_date_get_year (date),
							g_date_get_month (date),
							g_date_get_day (date));
			}
			else
				return g_strdup ("");
		}
	}
	else
		return g_strdup ("NULL");
}
	
/**
 * gda_value_compare
 * @value1: a #GValue to compare.
 * @value2: the other #GValue to be compared to @value1.
 *
 * Compares two values of the same type.
 *
 * Returns: if both values have the same type, returns 0 if both contain
 * the same value, an integer less than 0 if @value1 is less than @value2 or
 * an integer greater than 0 if @value1 is greater than @value2.
 */
gint
gda_value_compare (const GValue *value1, const GValue *value2)
{
	GList *l1, *l2;
	gint retval;
	GType type;

	g_return_val_if_fail (value1 && value2, -1);
	g_return_val_if_fail (G_VALUE_TYPE (value1) == G_VALUE_TYPE (value2), -1);

	type = G_VALUE_TYPE(value1);
	
	if (value1 == value2)
		return 0;

	else if (type == G_TYPE_INT64) 
		return (g_value_get_int64 (value1) > g_value_get_int64 (value2)) ? 1 : 
			((g_value_get_int64 (value1) == g_value_get_int64 (value2)) ? 0 : -1);
		
	else if (type == G_TYPE_UINT64)
		return (g_value_get_uint64 (value1) > g_value_get_uint64 (value2)) ? 1 : 
			((g_value_get_uint64 (value1) == g_value_get_uint64 (value2)) ? 0 : -1);

	else if (type == GDA_TYPE_BINARY) {
		const GdaBinary *binary1 = gda_value_get_binary (value1);
		const GdaBinary *binary2 = gda_value_get_binary (value2);
		if (binary1 && binary2 && (binary1->binary_length == binary2->binary_length))
			return memcmp (binary1->data, binary2->data, binary1->binary_length) ;
		else
			return -1;
	}

	else if (type == G_TYPE_BOOLEAN)
		return g_value_get_boolean (value1) - g_value_get_boolean (value2);

	else if (type == GDA_TYPE_BLOB) {
		const GdaBlob *blob1 = gda_value_get_blob (value1);
		const GdaBlob *blob2 = gda_value_get_blob (value2);
		if (blob1 && blob2 && (((GdaBinary *)blob1)->binary_length == ((GdaBinary *)blob2)->binary_length))
			return memcmp (((GdaBinary *)blob1)->data, ((GdaBinary *)blob2)->data, 
				       ((GdaBinary *)blob1)->binary_length) ;
		else
			return -1;
	}

	else if (type == G_TYPE_DATE) {
		GDate *d1, *d2;

		d1 = (GDate *) g_value_get_boxed (value1);
		d2 = (GDate *) g_value_get_boxed (value2);
		if (d1 && d2)
			return g_date_compare (d1, d2);
		else {
			if (d1)
				return 1;
			else {
				if (d2)
					return -1;
				else
					return 0;
			}
		}
	}

	else if (type == G_TYPE_DOUBLE) {
		gdouble v1, v2;

		v1 = g_value_get_double (value1);
		v2 = g_value_get_double (value2);
		
		if (v1 == v2)
			return 0;
		else
			return (v1 > v2) ? 1 : -1;
	}

	else if (type == GDA_TYPE_GEOMETRIC_POINT)
		return memcmp (gda_value_get_geometric_point(value1) , 
			       gda_value_get_geometric_point(value2),
			       sizeof (GdaGeometricPoint));

	else if (type == G_TYPE_OBJECT) {
		if (g_value_get_object (value1) == g_value_get_object (value2))
			return 0;
		else
			return -1;
	}

	else if (type == G_TYPE_INT)
		return (g_value_get_int (value1) > g_value_get_int (value2)) ? 1 : 
			((g_value_get_int (value1) == g_value_get_int (value2)) ? 0 : -1);

	else if (type == GDA_TYPE_LIST) {
		retval = 0;
		for (l1 = (GList*) gda_value_get_list (value1), l2 = (GList*) gda_value_get_list (value2); 
		     l1 != NULL && l2 != NULL; l1 = l1->next, l2 = l2->next){
			retval = gda_value_compare ((GValue *) l1->data,
						    (GValue *) l2->data);
			if (retval != 0) 
				return retval;
		}
		if (retval == 0 && (l1 == NULL || l2 == NULL) && l1 != l2)
			retval = (l1 == NULL) ? -1 : 1;
		
		return retval;
	}

	else if (type == GDA_TYPE_NUMERIC) {
		const GdaNumeric *num1, *num2;
		num1= gda_value_get_numeric (value1);
		num2 = gda_value_get_numeric (value2);
                if (num1) {
			if (num2)
				retval = strcmp (num1->number, num2->number);
			else
				retval = 1;
		}
		else {
			if (num2->number)
				retval = -1;
			else
				retval = 0;
		}
		return retval;
	}

	else if (type == G_TYPE_FLOAT)
		return (g_value_get_float (value1) > g_value_get_float (value2)) ? 1 : 
			((g_value_get_float (value1) == g_value_get_float (value2)) ? 0 : -1);

	else if (type == GDA_TYPE_SHORT)
		return (gda_value_get_short (value1) > gda_value_get_short (value2)) ? 1 : 
			((gda_value_get_short (value1) == gda_value_get_short (value2)) ? 0 : -1);

	else if (type == G_TYPE_ULONG)
		return (g_value_get_ulong (value1) > g_value_get_ulong (value2)) ? 1 : 
			((g_value_get_ulong (value1) == g_value_get_ulong (value2)) ? 0 : -1);

	else if (type == G_TYPE_LONG)
		return (g_value_get_long (value1) > g_value_get_long (value2)) ? 1 : 
			((g_value_get_long (value1) == g_value_get_long (value2)) ? 0 : -1);
	
	else if (type == GDA_TYPE_USHORT)
		return (gda_value_get_ushort (value1) > gda_value_get_ushort (value2)) ? 1 : 
			((gda_value_get_ushort (value1) == gda_value_get_ushort (value2)) ? 0 : -1);
	
	else if (type == G_TYPE_STRING)	{
		const gchar *str1, *str2;
		str1 = g_value_get_string (value1);
		str2 = g_value_get_string (value2);
		if (str1 && str2)
			retval = strcmp (str1, str2);
		else {
			if (str1)
				return 1;
			else {
				if (str2)
					return -1;
				else
					return 0;
			}
		}
		
		return retval;
	}
	
	else if (type == GDA_TYPE_TIME)
		return memcmp (gda_value_get_time(value1), gda_value_get_time(value2),
			       sizeof (GdaTime));

	else if (type == GDA_TYPE_TIMESTAMP)
		return memcmp (gda_value_get_timestamp(value1), 
			       gda_value_get_timestamp(value2),
			       sizeof (GdaTimestamp));
	
	else if (type == G_TYPE_CHAR)
		return (g_value_get_char (value1) > g_value_get_char (value2)) ? 1 : 
			((g_value_get_char (value1) == g_value_get_char (value2)) ? 0 : -1);

	else if (type == G_TYPE_UCHAR)
		return (g_value_get_uchar (value1) > g_value_get_uchar (value2)) ? 1 : 
			((g_value_get_uchar (value1) == g_value_get_uchar (value2)) ? 0 : -1);

	else if (type == G_TYPE_UINT)
		return (g_value_get_uint (value1) > g_value_get_uint (value2)) ? 1 : 
			((g_value_get_uint (value1) == g_value_get_uint (value2)) ? 0 : -1);


	g_warning ("%s() cannot handle values of type %s", __FUNCTION__, g_type_name (G_VALUE_TYPE (value1)));

	return 0;
}

/**
 * gda_value_compare_ext
 * @value1: a #GValue to compare.
 * @value2: the other #GValue to be compared to @value1.
 *
 * Like gda_value_compare(), compares two values of the same type, except that NULL values and values
 * of type GDA_TYPE_NULL are considered equals
 *
 * Returns: 0 if both contain the same value, an integer less than 0 if @value1 is less than @value2 or
 * an integer greater than 0 if @value1 is greater than @value2.
 */
gint
gda_value_compare_ext (const GValue *value1, const GValue *value2)
{
	if (value1 == value2)
		return 0;

	if (!value1 || (G_VALUE_TYPE (value1) == GDA_TYPE_NULL)) {
		/* value1 represents a NULL value */
		if (! value2 || (G_VALUE_TYPE (value2) == GDA_TYPE_NULL))
			return 0;
		else
			return 1;
	}
	else {
		/* value1 does not represents a NULL value */
		if (! value2 || (G_VALUE_TYPE (value2) == GDA_TYPE_NULL))
			return -1;
		else
			return gda_value_compare (value1, value2);
	}
}

/*
 * to_string
 * 
 * The exact reverse process of set_from_string(), almost the same as gda_value_stingify ()
 * because of some localization with gda_value_stingify ().
 */
static gchar *
to_string (const GValue *value)
{
	gchar *retval = NULL;

	g_return_val_if_fail (value && G_IS_VALUE (value), NULL);

	if (G_VALUE_TYPE (value) == G_TYPE_BOOLEAN) {
		if (g_value_get_boolean (value))
			retval = g_strdup ("true");
		else
			retval = g_strdup ("false");
	}
	else
		retval = gda_value_stringify (value);
        	
	return retval;
}


/**
 * gda_value_to_xml
 * @value: a #GValue.
 *
 * Serializes the given #GValue to a XML node string.
 *
 * Returns: the XML node. Once not needed anymore, you should free it.
 */
xmlNodePtr
gda_value_to_xml (const GValue *value)
{
	gchar *valstr;
	xmlNodePtr retval;

	g_return_val_if_fail (value && G_IS_VALUE (value), NULL);

	valstr = to_string (value);

	retval = xmlNewNode (NULL, (xmlChar*)"value");
	xmlSetProp(retval, (xmlChar*)"type", (xmlChar*)g_type_name (G_VALUE_TYPE (value)));
	xmlNodeSetContent (retval, (xmlChar*)valstr);

	g_free (valstr);

	return retval;
}


/* Register the GDA Types */

/* Gda gshort type */
/* Transform a String GValue to a gshort*/
static void 
string_to_short(const GValue *src, GValue *dest)
{
	
	const gchar *as_string;
	long int lvalue;
	gchar *endptr;

	g_return_if_fail (G_VALUE_HOLDS_STRING (src) &&
			  (GDA_VALUE_HOLDS_SHORT (dest) || GDA_VALUE_HOLDS_USHORT (dest)));
	
	as_string = g_value_get_string ((GValue *) src);
	
	lvalue = strtol (as_string, &endptr, 10);
	
	if (*as_string != '\0' && *endptr == '\0') {
		if (GDA_VALUE_HOLDS_SHORT (dest))
			gda_value_set_short (dest, (gshort) lvalue);
		else
			gda_value_set_ushort (dest, (gushort) lvalue);
	}
}

static void 
short_to_string (const GValue *src, GValue *dest) 
{
	gchar *str;
	
	g_return_if_fail (G_VALUE_HOLDS_STRING (dest) &&
			  (GDA_VALUE_HOLDS_SHORT (src) || GDA_VALUE_HOLDS_USHORT (src)));
	
	if (GDA_VALUE_HOLDS_SHORT (src))
		str = g_strdup_printf ("%d", gda_value_get_short ((GValue *) src));
	else
		str = g_strdup_printf ("%d", gda_value_get_ushort ((GValue *) src));
	
	g_value_take_string (dest, str);
}


GType
gda_short_get_type (void) 
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo type_info = {
    			0,			/* class_size */
    			NULL,		/* base_init */
    			NULL,		/* base_finalize */
    			NULL,		/* class_init */
    			NULL,		/* class_finalize */
    			NULL,		/* class_data */
    			0,			/* instance_size */
    			0,			/* n_preallocs */
    			NULL,		/* instance_init */
    			NULL		/* value_table */
  		};
  
  		type = g_type_register_static (G_TYPE_INT, "GdaShort", &type_info, 0);		

		g_value_register_transform_func(G_TYPE_STRING,
						type,
						string_to_short);
		
		g_value_register_transform_func(type, 
						G_TYPE_STRING,
						short_to_string);  
	}
	  
	
	
  	return type;
}

GType
gda_ushort_get_type (void) {
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo type_info = {
    			0,			/* class_size */
    			NULL,		/* base_init */
    			NULL,		/* base_finalize */
    			NULL,		/* class_init */
    			NULL,		/* class_finalize */
    			NULL,		/* class_data */
    			0,			/* instance_size */
    			0,			/* n_preallocs */
    			NULL,		/* instance_init */
    			NULL		/* value_table */
  		};
  
  		type = g_type_register_static (G_TYPE_UINT, "GdaUShort", &type_info, 0);		
		
  		g_value_register_transform_func (G_TYPE_STRING,
						 type,
						 string_to_short);
		
		g_value_register_transform_func (type, 
						 G_TYPE_STRING,
						 short_to_string);
	}
	
  	return type;
}



#define MYMIN(a,b) (((b) > 0) ? ((a) < (b) ? (a) : (b)) : (a))

/**
 * gda_binary_to_string
 * @bin: a correctly filled @GdaBinary structure
 * @maxlen: a maximum len used to truncate, or 0 for no maximum length
 *
 * Converts all the non printable characters of bin->data into the \xxx representation
 * where xxx is the octal representation of the byte, and the '\' (backslash) character
 * is converted to "\\".
 *
 * Returns: a new string from @bin
 */
gchar *
gda_binary_to_string (const GdaBinary *bin, guint maxlen)
{
	gint nb_rewrites = 0;
	gchar *ptr, *hold;
	gulong realsize = MYMIN (bin->binary_length, maxlen);

	gchar *retval;
	glong offset = 0;
	gunichar unichar;

	if (!bin->data || (bin->binary_length == 0))
		return g_strdup ("");

	/* compute number of char rewrites */
	ptr = (gchar*)bin->data;
	while (offset < realsize) {
		unichar = g_utf8_get_char_validated (ptr, -1);
		if ((*ptr == '\n') ||
		    ((*ptr != '\\') && (unichar > 0) && g_unichar_isprint (unichar))) {
			hold = ptr;
			ptr = g_utf8_next_char (ptr);
			offset += ptr - hold;
		}
		else {
			ptr++;
			nb_rewrites++;
			offset ++;
		}
	}
	
	/* mem allocation and copy */
	retval = g_malloc0 (realsize + nb_rewrites * 4 + 1);
	memcpy (retval, bin->data, realsize);
	ptr = retval;
	offset = 0;
	while (offset < realsize) {
		unichar = g_utf8_get_char_validated (ptr, -1);
		/* g_print (">%x<\n", (guchar) *ptr); */
		if ((*ptr == '\n') ||
		    ((*ptr != '\\') && (unichar > 0) && g_unichar_isprint (unichar))) {
			hold = ptr;
			ptr = g_utf8_next_char (ptr);
			offset += ptr - hold;
		}
		else {
			if (*ptr == '\\') {
				g_memmove (ptr+2, ptr+1, realsize - offset);

				*(ptr+1) = '\\';
				ptr += 2;
				offset ++;
			}
			else {
				guchar val = *ptr;
				g_memmove (ptr+4, ptr+1, realsize - offset);
				*ptr = '\\';
				*(ptr+1) = val / 49 + '0';
				val = val % 49;
				*(ptr+2) = val / 7 + '0';
				val = val % 7;
				*(ptr+3) = val + '0';
				
				ptr += 4;
				offset ++;
			}
		}
	}

	return retval;
}

/**
 * gda_string_to_binary
 * @str: a string to convert
 * @bin: a non filled @GdaBinary structure
 *
 * Performs the reverse of gda_binary_to_string().
 *
 * Returns: TRUE if no error were found in @str, or FALSE otherwise
 */
gboolean
gda_string_to_binary (const gchar *str, GdaBinary *bin)
{
	glong len = 0, total;
	gchar *ptr;
	gchar *retval;
	glong offset = 0;

	if (!str) {
		bin->data = NULL;
		bin->binary_length = 0;
		return TRUE;
	}

	total = strlen (str);
	retval = g_memdup (str, total);
	ptr = (gchar *) retval;
	while (offset < total) {
		if (*ptr == '\\') {
			if (*(ptr+1) == '\\') {
				g_memmove (ptr+1, ptr+2, total - offset);
				offset += 2;
			}
			else {
				if ((*(ptr+1) >= '0') && (*(ptr+1) <= '9') &&
				    (*(ptr+2) >= '0') && (*(ptr+2) <= '9') &&
				    (*(ptr+3) >= '0') && (*(ptr+3) <= '9')) {
					*ptr = (*(ptr+1) - '0') * 49 +
						(*(ptr+2) - '0') * 7 +
						*(ptr+3) - '0';
					g_memmove (ptr+1, ptr+4, total - offset);
					offset += 4;
				}
				else {
					g_free (retval);
					return FALSE;
				}
			}
		}
		else
			offset ++;

		ptr++;
		len ++;
	}

	bin->data = (guchar*)retval;
	bin->binary_length = len;

	return TRUE;
}

/**
 * gda_blob_to_string
 * @blob: a correctly filled @GdaBlob structure
 * @maxlen: a maximum len used to truncate, or 0 for no maximum length
 *
 * Converts all the non printable characters of blob->data into the \xxx representation
 * where xxx is the octal representation of the byte, and the '\' (backslash) character
 * is converted to "\\".
 *
 * Returns: a new string from @blob
 */
gchar *
gda_blob_to_string (GdaBlob *blob, guint maxlen)
{
	if (!((GdaBinary *)blob)->data && blob->op) {
		/* fetch some data first (limited amount of data) */
		gda_blob_op_read (blob->op, blob, 0, 40);
	}
	return gda_binary_to_string ((GdaBinary*) blob, maxlen);
}

/**
 * gda_string_to_blob
 * @str: a string to convert
 * @blob: a non filled @GdaBlob structure
 *
 * Performs the reverse of gda_blob_to_string().
 *
 * Returns: TRUE if no error were found in @str, or FALSE otherwise
 */
gboolean
gda_string_to_blob (const gchar *str, GdaBlob *blob)
{
	return gda_string_to_binary (str, (GdaBinary*) blob);
}
