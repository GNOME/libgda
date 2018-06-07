/*
 * Copyright (C) 2001 - 2003 Rodrigo Moya <rodrigo@gnome-db.org>
 * Copyright (C) 2002 - 2003 Gonzalo Paniagua Javier <gonzalo@gnome-db.org>
 * Copyright (C) 2002 Holger Thon <holger.thon@gnome-db.org>
 * Copyright (C) 2002 - 2016 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2002 Zbigniew Chyla <cyba@gnome.pl>
 * Copyright (C) 2003 Akira TAGOH <tagoh@gnome-db.org>
 * Copyright (C) 2003 Danilo Schoeneberg <dschoene@src.gnome.org>
 * Copyright (C) 2003 Laurent Sansonetti <lrz@src.gnome.org>
 * Copyright (C) 2003 Philippe CHARLIER <p.charlier@chello.be>
 * Copyright (C) 2004 Andrew Hill <andru@src.gnome.org>
 * Copyright (C) 2004 Dani Baeyens <daniel.baeyens@hispalinux.es>
 * Copyright (C) 2004 - 2011 Murray Cumming <murrayc@murrayc.com>
 * Copyright (C) 2004 Paisa  Seeluangsawat <paisa@users.sf.net>
 * Copyright (C) 2004 Szalai Ferenc <szferi@einstein.ki.iif.hu>
 * Copyright (C) 2007 Armin Burgmeier <armin@openismus.com>
 * Copyright (C) 2007 Leonardo Boshell <lb@kmc.com.co>
 * Copyright (C) 2008 Przemysław Grzegorczyk <pgrzegorczyk@gmail.com>
 * Copyright (C) 2009 Bas Driessen <bas.driessen@xobas.com>
 * Copyright (C) 2010 David King <davidk@openismus.com>
 * Copyright (C) 2010 Jonh Wendell <jwendell@gnome.org>
 * Copyright (C) 2011 - 2017 Daniel Espinosa <esodan@gmail.com>
 * Copyright (C) 2013 Miguel Angel Cabrera Moya <madmac2501@gmail.com>
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

#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <locale.h>
#include <libgda/gda-value.h>
#include <libgda/gda-blob-op.h>
#include <libgda/gda-util.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <glib.h>
#define __GDA_INTERNAL__
#include "dir-blob-op.h"

#define l_g_value_unset(val) G_STMT_START{ if (G_IS_VALUE (val)) g_value_unset (val); }G_STMT_END
#ifdef G_OS_WIN32
#define bcmp(s1, s2, n) memcmp ((s1), (s2), (n))
#endif

/**
 * GdaNumeric: (set-value-func gda_value_set_numeric) (get-value-func gda_value_get_numeric)
 * @number: a string representing a number
 * @precision: precision to use when @number is converted (not implemented jet)
 * @width: not implemented jet
 *
 * Holds numbers represented as strings.
 *
 * This struct must be considered as opaque. Any access to its members must use its
 * accessors added since version 5.0.2.
 */
struct _GdaNumeric {
	gchar*   number; /* stored in the "C" locale, never NULL */
	glong    precision;
	glong    width;
	
	/*< private >*/
	gpointer reserved; /* reserved for future usage with GMP (http://gmplib.org/) */
};



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
		g_warning ("Can't set value for a G_TYPE_INVALID GValue");
		return FALSE;
	}
	else if (GDA_VALUE_HOLDS_NULL (value)) {
		g_warning ("Can't set value for a NULL GValue");
                return FALSE;
        }

	type = G_VALUE_TYPE (value);
	g_value_reset (value);

	/* custom transform function */
	retval = FALSE;
	if (type == G_TYPE_BOOLEAN) {
		if ((as_string[0] == 't') || (as_string[0] == 'T')) {
			g_value_set_boolean (value, TRUE);
			retval = TRUE;
		}
		else if ((as_string[0] == 'f') || (as_string[0] == 'F')) {
			g_value_set_boolean (value, FALSE);
			retval = TRUE;
		}
		else {
			gint i;
			i = atoi (as_string); /* Flawfinder: ignore */
			g_value_set_boolean (value, i ? TRUE : FALSE);
			retval = TRUE;
		}
	}
	else if (type == G_TYPE_INT64) {
		gint64 i64;
		i64 = g_ascii_strtoll (as_string, endptr, 10);
		if (*as_string != '\0' && **endptr == '\0'){
			g_value_set_int64 (value, i64);
			retval = TRUE;
		}
	}
	else if (type == G_TYPE_UINT64) {
		guint64 ui64;
		ui64 = g_ascii_strtoull (as_string, endptr, 10);
		if (*as_string != '\0' && **endptr == '\0'){
			g_value_set_uint64 (value, ui64);
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
		lvalue = strtol (as_string, endptr, 10);
		if (*as_string!=0 && **endptr==0) {
			g_value_set_schar(value,(gint)lvalue);
			retval = TRUE;
		}
	}
	else if (type == G_TYPE_UCHAR) {
		ulvalue = strtoul (as_string,endptr, 10);
		if (*as_string!=0 && **endptr==0) {
			g_value_set_uchar(value,(guchar)ulvalue);
			retval = TRUE;
		}
	}
	else if (type == G_TYPE_FLOAT) {
		dvalue = g_ascii_strtod (as_string, endptr);
		if (*as_string != '\0' && **endptr == '\0'){
			g_value_set_float (value, (gfloat) dvalue);
			retval = TRUE;
		}
	}
	else if (type == G_TYPE_DOUBLE) {
		dvalue = g_ascii_strtod (as_string, endptr);
		if (*as_string != '\0' && **endptr == '\0'){
			g_value_set_double (value, dvalue);
			retval = TRUE;
		}
	}
	else if (type == GDA_TYPE_NUMERIC) {
		GdaNumeric *numeric = gda_numeric_new ();
		gda_numeric_set_from_string (numeric, as_string);
		gda_value_set_numeric (value, numeric);
		gda_numeric_free (numeric);
		retval = TRUE;
	}
	else if (type == G_TYPE_DATE) {
		GDate *gdate;
		gdate = g_date_new ();

		if (gda_parse_iso8601_date (gdate, as_string)) {
			g_value_take_boxed (value, gdate);
			retval = TRUE;
		}
		else
			g_date_free (gdate);
	}
	else if (type == GDA_TYPE_TIME) {
		GdaTime* timegda = gda_time_new ();
		if (gda_parse_iso8601_time (timegda, as_string)) {
			gda_value_set_time (value, timegda);
			retval = TRUE;
		}
		gda_time_free (timegda);
	}
	else if (g_type_is_a (type, G_TYPE_DATE_TIME)) {
		GdaTimestamp* timestamp = gda_parse_iso8601_timestamp (as_string);
		if (timestamp) {
			g_value_set_boxed (value, timestamp);
			retval = TRUE;
		}
		gda_timestamp_free (timestamp);
	}
	else if (type == GDA_TYPE_NULL) {
		gda_value_set_null (value);
		retval = TRUE;
	}
	else if (type == G_TYPE_GTYPE) {
		GType gt;
		gt = gda_g_type_from_string (as_string);
		if (gt != G_TYPE_INVALID) {
			g_value_set_gtype (value, gt);
			retval = TRUE;
		}
	}
	else if (type == G_TYPE_ULONG)
	{
		gulong ulvalue;

		ulvalue = strtoul (as_string, endptr, 10);
		if (*as_string!=0 && **endptr==0) {
			g_value_set_ulong (value, ulvalue);
			retval = TRUE;
		}
	}
	else if (type == G_TYPE_LONG)
	{
		glong lvalue;

		lvalue = strtol (as_string, endptr, 10);
		if (*as_string!=0 && **endptr==0) {
			g_value_set_long (value, lvalue);
			retval = TRUE;
		}
	}
	else if (type == GDA_TYPE_BINARY) {
		GdaBinary *bin;
		bin = gda_string_to_binary (as_string);
		if (bin) {
			gda_value_take_binary (value, bin);
			retval = TRUE;
		}
	}
	else if (type == GDA_TYPE_BLOB) {
		GdaBlob *blob;
		blob = gda_string_to_blob (as_string);
		if (blob) {
			gda_value_take_blob (value, blob);
			retval = TRUE;
		}
	}
	else if (g_value_type_transformable (G_TYPE_STRING, type)) {
		/* use the GLib type transformation function */
		GValue *string;

                string = g_new0 (GValue, 1);
                g_value_init (string, G_TYPE_STRING);
                g_value_set_string (string, as_string);

                g_value_transform (string, value);
                gda_value_free (string);

                retval = TRUE;
	}

	return retval;
}

/*
 * Register the NULL type in the GType system
 */
static void
string_to_null (const GValue *src, GValue *dest)
{
       g_return_if_fail (G_VALUE_HOLDS_STRING (src) && GDA_VALUE_HOLDS_NULL (dest));
       /* Do nothing just a dummy function to register */
}

static void
null_to_string (const GValue *src, GValue *dest)
{
       g_return_if_fail (G_VALUE_HOLDS_STRING (dest) && GDA_VALUE_HOLDS_NULL (src));
       g_value_set_string (dest, "NULL");
}

static gpointer
gda_null_copy (G_GNUC_UNUSED gpointer boxed)
{
	return (gpointer) NULL;
}

static void
gda_null_free (G_GNUC_UNUSED gpointer boxed)
{
}

GType
gda_null_get_type (void)
{
       static GType type = 0;

       if (G_UNLIKELY (type == 0)) {
               type = g_boxed_type_register_static ("GdaNull",
                                                    (GBoxedCopyFunc) gda_null_copy,
                                                    (GBoxedFreeFunc) gda_null_free);

               g_value_register_transform_func (G_TYPE_STRING,
                                                type,
                                                string_to_null);

               g_value_register_transform_func (type,
                                                G_TYPE_STRING,
                                                null_to_string);
       }

       return type;
}

/*
 * Register the DEFAULT type in the GType system
 */
static void
string_to_default (const GValue *src, GValue *dest)
{
       g_return_if_fail (G_VALUE_HOLDS_STRING (src) && GDA_VALUE_HOLDS_DEFAULT (dest));
       g_value_set_boxed (dest, g_value_get_string (src));
}

static void
default_to_string (const GValue *src, GValue *dest)
{
	gchar *str;
	g_return_if_fail (G_VALUE_HOLDS_STRING (dest) && GDA_VALUE_HOLDS_DEFAULT (src));
	str = (gchar*) g_value_get_boxed (src);
	g_value_set_string (dest, str);
}

static gpointer
gda_default_copy (G_GNUC_UNUSED gpointer boxed)
{
	if (boxed)
		return (gpointer) g_strdup (boxed);
	else
		return (gpointer) NULL;
}

static void
gda_default_free (G_GNUC_UNUSED gpointer boxed)
{
	g_free (boxed);
}

GType
gda_default_get_type (void)
{
       static GType type = 0;

       if (G_UNLIKELY (type == 0)) {
               type = g_boxed_type_register_static ("GdaDefault",
                                                    (GBoxedCopyFunc) gda_default_copy,
                                                    (GBoxedFreeFunc) gda_default_free);

               g_value_register_transform_func (G_TYPE_STRING,
                                                type,
                                                string_to_default);

               g_value_register_transform_func (type,
                                                G_TYPE_STRING,
                                                default_to_string);
       }

       return type;
}


// GdaBinary

struct _GdaBinary {
	guchar *data;
	glong   binary_length;
};

/*
 * Register the GdaBinary type in the GType system
 */

/* Transform a String GValue to a GdaBinary*/
static void
string_to_binary (const GValue *src, GValue *dest)
{
	GdaBinary *bin;
	const gchar *as_string;

	g_return_if_fail (G_VALUE_HOLDS_STRING (src) &&
			  GDA_VALUE_HOLDS_BINARY (dest));

	as_string = g_value_get_string (src);

	bin = gda_string_to_binary (as_string);
	g_return_if_fail (bin);
	gda_value_take_binary (dest, bin);
}

static void
binary_to_string (const GValue *src, GValue *dest)
{
	gchar *str;

	g_return_if_fail (G_VALUE_HOLDS_STRING (dest) &&
			  GDA_VALUE_HOLDS_BINARY (src));

	str = gda_binary_to_string (gda_value_get_binary (src), 0);

	g_value_take_string (dest, str);
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


/*
 * Register the GdaBlob type in the GType system
 */
/* Transform a String GValue to a GdaBlob*/
static void
string_to_blob (const GValue *src, GValue *dest)
{
	GdaBlob *blob;
	const gchar *as_string;

	g_return_if_fail (G_VALUE_HOLDS_STRING (src) &&
			  GDA_VALUE_HOLDS_BLOB (dest));

	as_string = g_value_get_string (src);

	blob = gda_string_to_blob (as_string);
	g_return_if_fail (blob);
	gda_value_take_blob (dest, blob);
}

static void
blob_to_string (const GValue *src, GValue *dest)
{
	gchar *str;

	g_return_if_fail (G_VALUE_HOLDS_STRING (dest) &&
			  GDA_VALUE_HOLDS_BLOB (src));

	str = gda_blob_to_string ((GdaBlob *) gda_value_get_blob ((GValue *) src), 0);

	g_value_take_string (dest, str);
}

/**
 * gda_binary_new:
 *
 * Creates a new #GdaBinary coping data.
 *
 * Returns: (transfer full): the newly created #GdaBinary.
 *
 * Since: 6.0
 */
GdaBinary*
gda_binary_new (void)
{
	GdaBinary *binary = g_new0 (GdaBinary, 1);

	binary->data = NULL;
	binary->binary_length = 0;

	return (GdaBinary*) binary;
}

/**
 * gda_binary_set_data:
 * @binary: a #GdaBinary pointer
 * @val: (array length=size) (element-type guint8): value to be copied by #GdaBinary.
 * @size: the size of the memory pool pointer to by @val.
 *
 * Set binary data to a #GdaBinary, holding a copy of the data.
 *
 * Since: 6.0
 */
void
gda_binary_set_data (GdaBinary *binary, const guchar *val, glong size)
{
	g_return_if_fail (binary);
	g_return_if_fail (val);
	if (binary->data)
		g_free (binary->data);
	binary->data = g_memdup (val, size);
	binary->binary_length = size;
}

/**
 * gda_binary_take_data:
 * @val: (array length=size) (element-type guint8): value to be taken by #GdaBinary.
 * @size: the size of the memory pool pointer to by @val.
 *
 * Set binary data to a #GdaBinary, directly holding @val (no copy made).
 */
void
gda_binary_take_data (GdaBinary *binary, guchar *val, glong size)
{
	g_return_if_fail (binary);
	g_return_if_fail (val);
	if (binary->data)
		g_free (binary->data);
	binary->data = val;
	binary->binary_length = size;
}


/**
 * gda_binary_get_data:
 * @binary: a #GdaBinary pointer
 *
 * Returns: (transfer none): associated data to #GdaBinary.
 *
 * Since: 6.0
 */
gpointer
gda_binary_get_data (GdaBinary *binary)
{
	g_return_val_if_fail (binary, NULL);

	return binary->data;
}



/**
 * gda_binary_reset_data:
 * @binary: a #GdaBinary pointer
 *
 * Frees data referenced by #GdaBinary
 *
 * Since: 6.0
 */
void
gda_binary_reset_data (GdaBinary *binary)
{
	g_return_if_fail (binary);

	if (binary->data != NULL)
		g_free (binary->data);
	binary->data = NULL;
	binary->binary_length = 0;
}


/**
 * gda_binary_get_size:
 *
 * Returns: size of associated data to #GdaBinary or -1 in case of error.
 *
 * Since: 6.0
 */
glong
gda_binary_get_size (const GdaBinary *binary)
{
	g_return_val_if_fail (binary, -1);

	return binary->binary_length;
}


/**
 * gda_binary_copy:
 * @src: source to get a copy from.
 *
 * Creates a new #GdaBinary structure from an existing one.

 * Returns: (transfer full): a newly allocated #GdaBinary which contains a copy of information in @boxed.
 *
 */
GdaBinary*
gda_binary_copy (GdaBinary *src)
{
	GdaBinary *copy = NULL;

	g_return_val_if_fail (src, NULL);

	copy = g_new0 (GdaBinary, 1);
	copy->data = g_memdup (src->data, src->binary_length);
	copy->binary_length = src->binary_length;

	return copy;
}

/**
 * gda_binary_free:
 * @binary: (transfer full): #GdaBinary to free.
 *
 * Deallocates all memory associated to the given #GdaBinary.
 */
void
gda_binary_free (GdaBinary *binary)
{
	g_return_if_fail (binary);

	if (binary->data)
		g_free (binary->data);
	g_free (binary);
}

/**
 * gda_value_new_binary: (skip)
 * @val: (transfer full): value to set for the new #GValue.
 * @size: the size of the memory pool pointer to by @val.
 *
 * Makes a new #GValue of type #GDA_TYPE_BINARY with value @val.
 *
 * Returns: (transfer full): the newly created #GValue.
 *
 * Free-function: gda_value_free
 */
GValue *
gda_value_new_binary (const guchar *val, glong size)
{
	GValue *value;
	GdaBinary binary;

	/* We use the const on the function parameter to make this clearer,
	 * but it would be awkward to keep the const in the struct.
         */
        binary.data = (guchar*) val;
        binary.binary_length = size;

        value = g_new0 (GValue, 1);
        gda_value_set_binary (value, &binary);

        return value;
}



// GdaBlob

struct _GdaBlob {
	GdaBinary *data;
	GdaBlobOp *op;
};


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
 * gda_blob_new:
 *
 * Creates a new #GdaBlob.
 *
 * Returns: (transfer full): a newly allocated #GdaBlob.
 *
 * Since: 6.0
 */
GdaBlob*
gda_blob_new (void)
{
	GdaBlob* blob = g_new0 (GdaBlob, 1);
	blob->data = gda_binary_new ();

	return (GdaBlob*) blob;
}

/**
 * gda_blob_get_binary:
 * @blob: a #GdaBlob pointer
 *
 * Returns: (transfer none): associated #GdaBinary.
 *
 * Since: 6.0
 */
GdaBinary*
gda_blob_get_binary (GdaBlob *blob)
{
	g_return_val_if_fail (blob, NULL);

	return blob->data;
}

/**
 * gda_blob_get_op:
 * @blob: a #GdaBlob pointer
 *
 * Returns: (transfer none): associated #GdaBlobOp.
 *
 * Since: 6.0
 */
GdaBlobOp*
gda_blob_get_op (GdaBlob *blob)
{
	g_return_val_if_fail (blob, NULL);

	return blob->op;
}


/**
 * gda_blob_copy:
 * @src: source to get a copy from.
 *
 * Creates a new #GdaBlob structure from an existing one.

 * Returns: (transfer full): a newly allocated #GdaBlob which contains a copy of information in @boxed.
 *
 */
GdaBlob*
gda_blob_copy (GdaBlob *src)
{
	GdaBlob *copy = NULL;

	g_return_val_if_fail (src != NULL, NULL);

	copy = g_new0 (GdaBlob, 1);
	copy->data = gda_binary_new ();
	if (! src->data)
		return copy;

	if (src->data->data)
		gda_binary_set_data (copy->data,
				     gda_binary_get_data (src->data),
				     gda_binary_get_size (src->data));
	gda_blob_set_op (copy, src->op);

	return (GdaBlob*) copy;
}

/**
 * gda_blob_free:
 * @blob: (transfer full): #GdaBlob to free.
 *
 * Deallocates all memory associated to the given #GdaBlob.
 */
void
gda_blob_free (GdaBlob *blob)
{
	g_return_if_fail (blob);

	if (blob->op) {
		g_object_unref (blob->op);
		blob->op = NULL;
	}
	gda_binary_free (blob->data);
	g_free (blob);
}

/**
 * gda_blob_set_op:
 * @blob: a #GdaBlob value
 * @op: (allow-none): a #GdaBlobOp object, or %NULL
 *
 * Correctly assigns @op to @blob (increases @op's reference count)
 */
void
gda_blob_set_op (GdaBlob *blob, GdaBlobOp *op)
{
	g_return_if_fail (blob);

	if (blob->op) {
		g_object_unref (blob->op);
		blob->op = NULL;
	}
	if (op) {
		g_return_if_fail (GDA_IS_BLOB_OP (op));
		blob->op = g_object_ref (op);
	}
}

/**
 * GdaGeometricPoint:
 * @x:
 * @y:
 */
struct _GdaGeometricPoint {
	gdouble x;
	gdouble y;
};

/*
 * Register the GdaGeometricPoint type in the GType system
 */
static void
geometric_point_to_string (const GValue *src, GValue *dest)
{
	GdaGeometricPoint *point;
	gchar *str;
	g_return_if_fail (G_VALUE_HOLDS_STRING (dest) &&
			  GDA_VALUE_HOLDS_GEOMETRIC_POINT (src));

	point = (GdaGeometricPoint *) gda_value_get_geometric_point ((GValue *) src);
	if (point)
		str = g_strdup_printf ("(%.*g,%.*g)", DBL_DIG, point->x,
				       DBL_DIG, point->y);
	else
		str = g_strdup_printf ("(%.*g,%.*g)",
				       DBL_DIG, 0.,
				       DBL_DIG, 0.);
	g_value_take_string (dest, str);
}

/* Transform a String GValue to a GdaGeometricPoint from a string like "(3.2,5.6)" */
static void
string_to_geometric_point (const GValue *src, GValue *dest)
{
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
gda_geometric_point_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		type = g_boxed_type_register_static ("GdaGeometricPoint",
						     (GBoxedCopyFunc) gda_geometric_point_copy,
						     (GBoxedFreeFunc) gda_geometric_point_free);

		g_value_register_transform_func (G_TYPE_STRING,
						 type,
						 string_to_geometric_point);

		g_value_register_transform_func (type,
						 G_TYPE_STRING,
						 geometric_point_to_string);
	}

	return type;
}

/**
 * gda_geometric_point_copy:
 *
 * Returns: (transfer full):
 */
GdaGeometricPoint*
gda_geometric_point_copy (GdaGeometricPoint *gp)
{
	GdaGeometricPoint *copy;

	g_return_val_if_fail (gp, NULL);

	copy = g_new0 (GdaGeometricPoint, 1);
	copy->x = gp->x;
	copy->y = gp->y;

	return copy;
}

void
gda_geometric_point_free (GdaGeometricPoint *gp)
{
	g_free (gp);
}


/**
 * gda_geometric_point_new:
 *
 * Returns: (transfer full): a new #GdaGeometricPoint
 */
GdaGeometricPoint*
gda_geometric_point_new (void)
{
	return g_new0 (GdaGeometricPoint, 1);
}
/**
 * gda_geometric_point_get_x:
 *
 */
gdouble
gda_geometric_point_get_x (GdaGeometricPoint* gp)
{
	g_return_val_if_fail (gp != NULL, 0);
	return gp->x;
}
/**
 * gda_geometric_point_set_x:
 *
 */
void
gda_geometric_point_set_x (GdaGeometricPoint* gp, double x)
{
	g_return_if_fail (gp != NULL);
	gp->x = x;
}
/**
 * gda_geometric_point_get_y:
 *
 */
gdouble
gda_geometric_point_get_y (GdaGeometricPoint* gp)
{
	g_return_val_if_fail (gp != NULL, 0);
	return gp->y;
}
/**
 * gda_geometric_point_set_y:
 *
 */
void
gda_geometric_point_set_y (GdaGeometricPoint* gp, double y)
{
	g_return_if_fail (gp != NULL);
	gp->y = y;
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
		g_value_set_string (dest, "0.0");
}

static void
numeric_to_int (const GValue *src, GValue *dest)
{
	const GdaNumeric *numeric;

	g_return_if_fail (G_VALUE_HOLDS_INT (dest) &&
			  GDA_VALUE_HOLDS_NUMERIC (src));

	numeric = gda_value_get_numeric (src);
	if (numeric) {
		glong tmp;
		tmp = atol (numeric->number); /* Flawfinder: ignore */
		if ((tmp < G_MININT) || (tmp > G_MAXINT))
			g_warning ("Integer overflow for value %ld", tmp);
		g_value_set_int (dest, tmp);
	}
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
	if (numeric) {
		glong tmp;
		tmp = atol (numeric->number); /* Flawfinder: ignore */
		if ((tmp < 0) || (tmp > (glong)G_MAXUINT))
			g_warning ("Unsigned integer overflow for value %ld", tmp);
		g_value_set_uint (dest, tmp);
	}
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
		g_value_set_boolean (dest, atoi (numeric->number)); /* Flawfinder: ignore */
	else
		g_value_set_boolean (dest, 0);
}

static void
numeric_to_double (const GValue *src, GValue *dest)
{
	const GdaNumeric *numeric;

	g_return_if_fail (G_VALUE_HOLDS_DOUBLE (dest) &&
			  GDA_VALUE_HOLDS_NUMERIC (src));

	numeric = gda_value_get_numeric (src);
	if (numeric)
		g_value_set_double (dest, gda_numeric_get_double (numeric));
	else
		g_value_set_double (dest, 0.0);
}

static void
numeric_to_float (const GValue *src, GValue *dest)
{
	const GdaNumeric *numeric;

	g_return_if_fail (G_VALUE_HOLDS_FLOAT (dest) &&
			  GDA_VALUE_HOLDS_NUMERIC (src));

	numeric = gda_value_get_numeric (src);
	if (numeric)
		g_value_set_float (dest, (float) gda_numeric_get_double (numeric));
	else
		g_value_set_float (dest, 0.0);
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
		g_value_register_transform_func (type, G_TYPE_DOUBLE, numeric_to_double);
		g_value_register_transform_func (type, G_TYPE_FLOAT, numeric_to_float);
	}

	return type;
}


/**
 * gda_numeric_copy:
 * @src: source to get a copy from.
 *
 * Creates a new #GdaNumeric structure from an existing one.

 * Returns: (transfer full): a newly allocated #GdaNumeric which contains a copy of information in @boxed.
 *
 * Free-function: gda_numeric_free
 */

GdaNumeric*
gda_numeric_copy (GdaNumeric *src)
{
	GdaNumeric *copy;
	gchar *str;

	g_return_val_if_fail (src, NULL);

	copy = gda_numeric_new ();

	str = gda_numeric_get_string (src);
	gda_numeric_set_from_string (copy, str);
	g_free (str);

	gda_numeric_set_width (copy, gda_numeric_get_width (src));
	gda_numeric_set_precision (copy, gda_numeric_get_precision (src));

	return copy;
}

/**
 * gda_numeric_free:
 * @numeric: (transfer full): a #GdaNumeric pointer
 *
 * Deallocates all memory associated to the given @boxed
 */
void
gda_numeric_free (GdaNumeric *numeric)
{
	g_return_if_fail (numeric);

	g_free (numeric->number);
	g_free (numeric);
}

static gchar*
gda_dtostr_dup (const double value)
{
	char buffer[G_ASCII_DTOSTR_BUF_SIZE];
	g_ascii_dtostr (buffer, sizeof (buffer), value);
	return g_strdup (buffer);
}

/**
 * gda_numeric_new:
 *
 * Creates a new #GdaNumeric with defaults.
 *
 * Returns: (transfer full): a new #GdaNumeric.
 * Since: 5.0.2
 */
GdaNumeric*
gda_numeric_new (void)
{
	GdaNumeric *n = g_new0 (GdaNumeric, 1);
	n->number = gda_dtostr_dup (0.0);
	return n;
}

/**
 * gda_numeric_set_from_string:
 * @numeric: a #GdaNumeric
 * @str: a string representing a number, in the C locale format
 *
 * Sets @numeric with a number represented by @str, in the C locale format (dot as a fraction separator).
 *
 * Since: 5.0.2
 */
void
gda_numeric_set_from_string (GdaNumeric *numeric, const gchar* str)
{
	g_return_if_fail (numeric);
	g_return_if_fail (str);
	g_free (numeric->number);

	gdouble number;
	gchar *endptr = NULL;
	number = g_ascii_strtod (str, &endptr);
	if (*endptr)
		numeric->number = gda_dtostr_dup (number);
	else
		numeric->number = g_strdup (str);
}

/**
 * gda_numeric_set_double:
 * @numeric: a #GdaNumeric
 * @number: a #gdouble
 *
 * Sets @numeric using a #gdouble represented by @number.
 *
 * Since: 5.0.2
 */
void
gda_numeric_set_double (GdaNumeric *numeric, gdouble number)
{
	g_return_if_fail (numeric);
	g_free (numeric->number);
	numeric->number = gda_dtostr_dup (number);
}

/**
 * gda_numeric_get_double:
 * @numeric: a #GdaNumeric
 *
 * Returns: a #gdouble representation of @numeric
 * Since: 5.0.2
 */
gdouble
gda_numeric_get_double (const GdaNumeric *numeric)
{
	g_return_val_if_fail (numeric, 0.0);
	return g_ascii_strtod (numeric->number, NULL);
}

/**
 * gda_numeric_set_width:
 * @numeric: a #GdaNumeric
 * @width: a #glong
 *
 * Sets the width of a #GdaNumeric. (Not yet implemented).
 *
 * Since: 5.0.2
 */
void
gda_numeric_set_width (GdaNumeric *numeric, glong width)
{
	g_return_if_fail (numeric);
	numeric->width = width;
}

/**
 * gda_numeric_get_width:
 * @numeric: a #GdaNumeric
 *
 * Gets the width of a #GdaNumeric. (Not yet implemented).
 *
 * Returns: an integer with the width of a #GdaNumeric. (Not jet implemented).
 *
 * Since: 5.0.2
 */
glong
gda_numeric_get_width (const GdaNumeric *numeric)
{
	g_return_val_if_fail (numeric, 0.0);
	return numeric->width;
}

/**
 * gda_numeric_set_precision:
 * @numeric: a #GdaNumeric
 * @precision: a #glong
 *
 * Sets the precision of a #GdaNumeric.
 *
 * Since: 5.0.2
 */
void
gda_numeric_set_precision (GdaNumeric *numeric, glong precision)
{
	g_return_if_fail (numeric);
	numeric->precision = precision;
}

/**
 * gda_numeric_get_precision:
 * @numeric: a #GdaNumeric
 *
 * Gets the precision of a #GdaNumeric.
 *
 * Returns: an integer with the precision of a #GdaNumeric.
 *
 * Since: 5.0.2
 */
glong
gda_numeric_get_precision (const GdaNumeric *numeric)
{
	g_return_val_if_fail (numeric, -1);
	return numeric->precision;
}
/**
 * gda_numeric_get_string:
 * @numeric: a #GdaNumeric
 *
 * Get the string representation of @numeric, in the C locale format (dot as a fraction separator).
 *
 * Returns: (transfer full) (allow-none): a new string representing the stored valued in @numeric
 *
 * Since: 5.0.2
 */
gchar*
gda_numeric_get_string (const GdaNumeric *numeric)
{
	if (numeric)
		return g_strdup (numeric->number);
	else
		return NULL;
}

/*
 * Register the GdaTime type in the GType system
 */

static void
time_to_string (const GValue *src, GValue *dest)
{
	GdaTime *gdatime;

	g_return_if_fail (G_VALUE_HOLDS_STRING (dest) &&
			  GDA_VALUE_HOLDS_TIME (src));

	gdatime = (GdaTime *) gda_value_get_time ((GValue *) src);

	if (gdatime) {
		GString *string;
		string = g_string_new ("");
		g_string_append_printf (string, "%02u:%02u:%02u",
					gda_time_get_hour (gdatime),
					gda_time_get_minute (gdatime),
					gda_time_get_second (gdatime));
		if (gda_time_get_fraction (gdatime) != 0)
			g_string_append_printf (string, ".%lu", gda_time_get_fraction (gdatime));

		if (gda_time_get_timezone (gdatime) != GDA_TIMEZONE_INVALID)
			g_string_append_printf (string, "%+02d", (int) gda_time_get_timezone (gdatime) / 3600);

		g_value_take_string (dest, string->str);
		g_string_free (string, FALSE);
	}
	else
		g_value_set_string (dest, "00:00:00");
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
	timegda = gda_time_new ();

	/* hour */
	gda_time_set_timezone (timegda, GDA_TIMEZONE_INVALID);
	ptr = as_string;
	if ((*ptr >= '0') && (*ptr <= '9') &&
	    (*(ptr+1) >= '0') && (*(ptr+1) <= '9'))
		gda_time_set_hour (timegda, (*ptr - '0') * 10 + *(ptr+1) - '0');
	else {
		gda_time_free (timegda);
		return;
	}

	/* minute */
	ptr += 2;
	if (! *ptr) {
		gda_time_free (timegda);
		return;
	}
	if (*ptr == ':')
		ptr++;
	if ((*ptr >= '0') && (*ptr <= '9') &&
	    (*(ptr+1) >= '0') && (*(ptr+1) <= '9'))
		gda_time_set_minute (timegda, (*ptr - '0') * 10 + *(ptr+1) - '0');
	else{
		gda_time_free (timegda);
		return;
	}

	/* second */
	ptr += 2;
	gda_time_set_second (timegda, 0);
	if (! *ptr) {
		if ((gda_time_get_hour (timegda) <= 24) && (gda_time_get_minute (timegda) <= 60))
			gda_value_set_time (dest, timegda);
		gda_time_free (timegda);
		return;
	}
	if (*ptr == ':')
		ptr++;
	if ((*ptr >= '0') && (*ptr <= '9') &&
	    (*(ptr+1) >= '0') && (*(ptr+1) <= '9'))
		gda_time_set_second (timegda, (*ptr - '0') * 10 + *(ptr+1) - '0');

	/* extra */
	ptr += 2;
	if (! *ptr) {
		if ((gda_time_get_hour (timegda) <= 24) && (gda_time_get_minute (timegda) <= 60) &&
		    (gda_time_get_second (timegda) <= 60))
			gda_value_set_time (dest, timegda);
		gda_time_free (timegda);
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
		gda_time_set_timezone (timegda, 0);
		while (*ptr && (*ptr >= '0') && (*ptr <= '9')) {
			gda_time_set_timezone (timegda, gda_time_get_timezone (timegda) * 10 + sign * ((*ptr) - '0'));
			ptr++;
		}
		gda_time_set_timezone (timegda, gda_time_get_timezone (timegda) * 3600);
	}

	/* checks */
	if ((gda_time_get_hour (timegda) <= 24) || (gda_time_get_minute (timegda) <= 60) || (gda_time_get_second (timegda) <= 60))
		gda_value_set_time (dest, timegda);
	gda_time_free (timegda);
}

/**
 * _GdaTime:
 * @hour: hour representation of the time, as a number between 0 and 23
 * @minute: minute representation of the time, as a number between 0 and 59
 * @second: second representation of the time, as a number between 0 and 59
 * @fraction: fractionnal part of the seconds, in millionth' of second
 * @timezone: number of seconds added to the GMT timezone
 */
struct _GdaTime {
	gushort   hour;
	gushort   minute;
	gushort   second;
	gulong    fraction;
	GTimeSpan timezone;
};

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

/**
 * gda_time_copy:
 *
 * Returns: (transfer full): a pointer to a new #GdaTime struct
 */
GdaTime*
gda_time_copy (const GdaTime* time)
{

	GdaTime *src = (GdaTime*) time;
	GdaTime *copy = NULL;

	g_return_val_if_fail (src, NULL);

	copy = g_new0 (GdaTime, 1);
	copy->hour = src->hour;
	copy->minute = src->minute;
	copy->second = src->second;
	copy->fraction = src->fraction;
	copy->timezone = src->timezone;

	return copy;
}

/**
 * gda_time_free:
 * @time: a #GdaTime to free
 *
 * Since: 6.0
 */
void
gda_time_free (GdaTime* boxed)
{
	g_free (boxed);
}

/**
 * gda_value_take_time: (skip)
 * @value: a #GValue that will store @val.
 * @time: (transfer full): a #GdaTime structure with the time to be stored in @value.
 *
 * Stores @val into @value, but on the contrary to gda_value_set_data(), the @time
 * argument is not copied, but used as-is and it should be considered owned by @value.
 */
void
gda_value_take_time (GValue *value, GdaTime *time)
{
	g_return_if_fail (value);
	g_return_if_fail (time != NULL);

	l_g_value_unset (value);
	g_value_init (value, GDA_TYPE_TIME);
	g_value_take_boxed (value, time);
}


/**
 * gda_time_new:
 *
 * Creates a new #GdaTime structure.
 *
 * Returns: (transfer full): a new #GdaTime structure
 */
GdaTime*
gda_time_new (void)
{
	return g_new0 (GdaTime, 1);
}

/**
 * gda_time_new_from_values:
 * @hour: hours
 * @minute: minutes
 * @second: seconds
 * @fraction: fraction of seconds
 * @timezone: timezone used
 *
 * Returns: (transfer full): the a new value storing a time
 */
GdaTime*
gda_time_new_from_values (gushort hour, gushort minute, gushort second, gulong fraction, glong timezone)
{
	GdaTime* time = g_new0 (GdaTime, 1);
	time->hour = hour;
	time->minute = minute;
	time->second = second;
	time->fraction = fraction;
	time->timezone = timezone;
	return time;
}
/**
 * gda_time_get_hour:
 * @time: a #GdaTime value to get hours from
 *
 * Since: 6.0
 */
gushort
gda_time_get_hour (const GdaTime* time)
{
	g_return_val_if_fail (time != NULL, 0);
	return time->hour;
}
/**
 * gda_time_set_hour:
 * @time: a #GdaTime value to set hours to
 * @hour: new hours to set to
 *
 * Since: 6.0
 */
void
gda_time_set_hour (GdaTime* time, gushort hour)
{
	g_return_if_fail (time != NULL);
	time->hour = hour;
}
/**
 * gda_time_get_minute:
 * @time: a #GdaTime value to get minutes from
 *
 * Since: 6.0
 */
gushort
gda_time_get_minute (const GdaTime* time)
{
	g_return_val_if_fail (time != NULL, 0);
	return time->minute;
}
/**
 * gda_time_set_minute:
 * @time: a #GdaTime value to set hours to
 * @minute: new minutes to set to
 *
 * Since: 6.0
 */
void
gda_time_set_minute (GdaTime* time, gushort minute)
{
	g_return_if_fail (time != NULL);
	time->minute = minute;
}
/**
 * gda_time_get_second:
 * @time: a #GdaTime value to get seconds from
 *
 * Since: 6.0
 */
gushort
gda_time_get_second (const GdaTime* time)
{
	g_return_val_if_fail (time != NULL, 0);
	return time->second;
}
/**
 * gda_time_set_second:
 * @time: a #GdaTime value to set hours to
 * @second: new seconds to set to
 *
 * Since: 6.0
 */
void
gda_time_set_second (GdaTime* time, gushort second)
{
	g_return_if_fail (time != NULL);
	time->second = second;
}
/**
 * gda_time_get_fraction:
 * @time: a #GdaTime value to get fraction of seconds from
 *
 * Since: 6.0
 */
gulong
gda_time_get_fraction (const GdaTime* time)
{
	g_return_val_if_fail (time != NULL, 0);
	return time->fraction;
}
/**
 * gda_time_set_fraction:
 * @time: a #GdaTime value to set hours to
 * @fraction: new second fraction to set to.
 *
 * Since: 6.0
 */
void
gda_time_set_fraction (GdaTime* time, gulong fraction)
{
	g_return_if_fail (time != NULL);
	time->fraction = fraction;
}
/**
 * gda_time_get_timezone:
 * @time: a #GdaTime value to get time zone from
 *
 * Returns number of seconds to be added to UTC time.
 *
 * Since: 6.0
 */
glong
gda_time_get_timezone (const GdaTime* time)
{
	g_return_val_if_fail (time != NULL, 0);
	return time->timezone;
}
/**
 * gda_time_set_timezone:
 * @time: a #GdaTime value to set time zone to
 * @timezone: new time zone to set to. See #gda_time_change_timezone
 *
 * Since: 6.0
 */
void
gda_time_set_timezone (GdaTime* time, glong timezone)
{
	g_return_if_fail (time != NULL);
	time->timezone = timezone;
}

/**
 * gda_time_valid:
 * @time: a #GdaTime value to check if it is valid
 *
 * Returns: #TRUE if #GdaTime is valid; %FALSE otherwise.
 *
 * Since: 4.2
 */
gboolean
gda_time_valid (const GdaTime *time)
{
	g_return_val_if_fail (time, FALSE);

	if ((time->hour > 23) ||
	    (time->minute > 59) ||
	    (time->second > 59))
		return FALSE;
	if ((time->fraction >= 1000000) ||
	    (time->timezone <= -12 * 3600) ||
	    (time->timezone >= 12 * 3600))
		return FALSE;
	return TRUE;
}

/**
 * gda_time_change_timezone:
 * @time: a valid #GdaTime
 * @ntz: a new timezone to use, in seconds added to GMT
 *
 * Changes @time's timezone (for example to convert from GMT to another time zone).
 * If @time's current timezone is unset (i.e. equal to %GDA_TIMEZONE_INVALID), then this function simply sets
 * @time's timezone attribute; Otherwise, it adds or removes hours, minutes or seconds to reflect the time in the new timezone.
 *
 * Note: the resulting will always be a valid time.
 *
 * Since: 5.2
 */
void
gda_time_change_timezone (GdaTime *time, glong ntz)
{
	g_return_if_fail (time);
	g_return_if_fail (gda_time_valid (time));
	g_return_if_fail ((ntz > - 12 * 3600) && (ntz < 12 * 3600));

	if (time->timezone == ntz)
		return;

	if (time->timezone != GDA_TIMEZONE_INVALID) {
		glong nsec;
		nsec = time->hour * 3600 + time->minute * 60 + time->second - time->timezone + ntz;
		if (nsec < 0)
			nsec += 86400;
		else if (nsec >= 86400)
			nsec -= 86400;

		/* hours */
		gint n;
		n = nsec / 3600;
		time->hour = (gushort) n;

		/* minutes */
		nsec -= n * 3600;
		n = nsec / 60;
		time->minute = (gushort) n;

		/* seconds */
		nsec -= n * 60;
		time->second = (gushort) nsec;
	}

	time->timezone = ntz;
}

/**
 * gda_timestamp_new:
 *
 * A new timestamp initialized to the instantace of creation at UTC zone.
 *
 * Returns: (transfer full):
 */
GdaTimestamp*
gda_timestamp_new (void)
{
	return (GdaTimestamp*) g_date_time_new_now_utc ();
}

/**
 * gda_timestamp_new_from_values:
 * @year: year
 * @month: month
 * @hour: hours
 * @minute: minutes
 * @second: seconds
 * @fraction: fraction of seconds
 * @timezone: timezone used
 *
 * Returns: (transfer full): a new value storing a timestamp
 */
GdaTimestamp*
gda_timestamp_new_from_values (gushort year, gushort month, gushort day, gushort hour, gushort minute, gushort second, gulong fraction, glong timezone)
{
  GString* tzd;
  GString* sec;
  GTimeZone* tz;
  gchar* sign;
  gdouble seconds;

  tzd = g_string_new ("");
  sign = "+";
  if (timezone < 0)
    sign = "-";
  g_string_printf (tzd,"%s%ul", sign, (uint) timezone);
	tz = g_time_zone_new (tzd->str);

  sec = g_string_new ("");
  g_string_printf (sec,"%d.%lu", second, fraction);
  seconds = g_strtod (sec->str, NULL);

  GdaTimestamp* timestamp = (GdaTimestamp*) g_date_time_new (tz,
                                            (gint) year,
                                            (gint) month,
                                            (gint) day,
                                            (gint) hour,
                                            (gint) minute,
                                            (gint) seconds);
  g_string_free (tzd, TRUE);
  g_string_free (sec, TRUE);
	return timestamp;
}

/**
 * gda_timestamp_copy:
 *
 * Returns: (transfer full):
 */
GdaTimestamp*
gda_timestamp_copy (GdaTimestamp *ts)
{
	g_return_val_if_fail(ts != NULL, NULL);

	GDateTime *dts = (GDateTime*) ts;
	GTimeZone *tz;

	tz = g_time_zone_new (g_date_time_get_timezone_abbreviation (dts));

	return (GdaTimestamp*) g_date_time_new (tz,
																					g_date_time_get_year (dts),
																					g_date_time_get_month (dts),
																					g_date_time_get_day_of_month (dts),
																					g_date_time_get_hour (dts),
																					g_date_time_get_minute (dts),
																					g_date_time_get_seconds (dts));


}

void
gda_timestamp_free (GdaTimestamp *ts)
{
	g_date_time_unref ((GDateTime*) ts);
}

/**
 * gda_timestamp_valid:
 * @timestamp: a #GdaTimestamp value to check if it is valid
 *
 * Returns: #TRUE if #GdaTimestamp is valid; %FALSE otherwise.
 *
 * Since: 4.2
 */
gboolean
gda_timestamp_valid (const GdaTimestamp *timestamp)
{
	g_return_val_if_fail (timestamp, FALSE);

	GdaTimestamp *copy = gda_timestamp_copy ((GdaTimestamp*) timestamp);
  return copy != NULL ? TRUE : FALSE;
}

/**
 * gda_timestamp_change_timezone:
 * @ts: a valid #GdaTimestamp
 * @ntz: a new timezone to use, in seconds added to GMT
 *
 * This function is similar to gda_time_change_timezone() but operates on time stamps.
 *
 * Note: this method is no longer valid use gda_timestamp_new() to create a new
 * object with the required time zone.
 *
 * Deprecated: 6.0
 * Since: 5.2
 */
void
gda_timestamp_change_timezone (GdaTimestamp *ts, glong ntz)
{
	g_return_if_fail (ts);
	g_return_if_fail ((ntz > - 12 * 3600) && (ntz < 12 * 3600));
}


gshort
gda_timestamp_get_year (const GdaTimestamp* timestamp)
{
	g_return_val_if_fail (timestamp != NULL, 0);
	return g_date_time_get_year ((GDateTime*) timestamp);
}
void
gda_timestamp_set_year (GdaTimestamp* timestamp, gshort year)
{
	g_return_if_fail (timestamp != NULL);
}
gushort
gda_timestamp_get_month (const GdaTimestamp* timestamp)
{
	g_return_val_if_fail (timestamp != NULL, 0);
	return g_date_time_get_month ((GDateTime*) timestamp);
}
void
gda_timestamp_set_month (GdaTimestamp* timestamp, gushort month)
{
	g_return_if_fail (timestamp != NULL);
}
gushort
gda_timestamp_get_day (const GdaTimestamp* timestamp)
{
	g_return_val_if_fail (timestamp != NULL, 0);
	return g_date_time_get_day_of_month ((GDateTime*) timestamp);
}
void
gda_timestamp_set_day (GdaTimestamp* timestamp, gushort day)
{
	g_return_if_fail (timestamp != NULL);
}
gushort
gda_timestamp_get_hour (const GdaTimestamp* timestamp)
{
	g_return_val_if_fail (timestamp != NULL, 0);
	return g_date_time_get_hour ((GDateTime*) timestamp);
}
void
gda_timestamp_set_hour (GdaTimestamp* timestamp, gushort hour)
{
	g_return_if_fail (timestamp != NULL);
}
gushort
gda_timestamp_get_minute (const GdaTimestamp* timestamp)
{
	g_return_val_if_fail (timestamp != NULL, 0);
	return g_date_time_get_minute ((GDateTime*) timestamp);
}
void
gda_timestamp_set_minute (GdaTimestamp* timestamp, gushort minute)
{
	g_return_if_fail (timestamp != NULL);
}
gdouble
gda_timestamp_get_seconds (const GdaTimestamp* timestamp)
{
	g_return_val_if_fail (timestamp != NULL, 0);
	return (gushort) g_date_time_get_seconds ((GDateTime*) timestamp);
}
gushort
gda_timestamp_get_second (const GdaTimestamp* timestamp)
{
	g_return_val_if_fail (timestamp != NULL, 0);
	return (gushort) g_date_time_get_second ((GDateTime*) timestamp);
}
void
gda_timestamp_set_second (GdaTimestamp* timestamp, gushort second)
{
	g_return_if_fail (timestamp != NULL);
}
/**
 * gda_timestamp_get_fraction:
 *
 * Microseconds.
 */
gulong
gda_timestamp_get_fraction (const GdaTimestamp* timestamp)
{
	g_return_val_if_fail (timestamp != NULL, 0);
	return (gulong) ((g_date_time_get_seconds ((GDateTime*) timestamp)
		- g_date_time_get_second ((GDateTime*) timestamp)) * 1000000.0);
}
void
gda_timestamp_set_fraction (GdaTimestamp* timestamp, gulong fraction)
{
	g_return_if_fail (timestamp != NULL);
}
/**
 * gda_timestamp_get_timezone:
 *
 * Seconds from UTC.
 */
glong
gda_timestamp_get_timezone (const GdaTimestamp* timestamp)
{
	g_return_val_if_fail (timestamp != NULL, 0);
	return (glong) (g_date_time_get_utc_offset ((GDateTime*) timestamp) / 1000000);
}
void
gda_timestamp_set_timezone (GdaTimestamp* timestamp, glong timezone)
{
	g_return_if_fail (timestamp != NULL);
}

/**
 * gda_value_new:
 * @type: the new value type.
 *
 * Creates a new #GValue of type @type, left in the same state as when g_value_init() is called.
 *
 * Returns: (transfer full): the newly created #GValue with the specified @type. You need to set the value in the returned GValue.
 *
 * Free-function: gda_value_free
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
 * gda_value_new_null:
 *
 * Creates a new #GValue initiated to a #GdaNull structure with a #GDA_TYPE_NULL, to
 * represent a NULL in the database.
 *
 * Returns: (transfer full): a new #GValue of the type #GDA_TYPE_NULL
 */
GValue*
gda_value_new_null (void)
{
	return gda_value_new (GDA_TYPE_NULL);
}

/**
 * gda_value_new_default:
 * @default_val: (allow-none): the default value as a string, or %NULL
 *
 * Creates a new default value.
 *
 * Returns: (transfer full): a new #GValue of the type #GDA_TYPE_DEFAULT
 *
 * Since: 4.2.9
 */
GValue *
gda_value_new_default (const gchar *default_val)
{
	GValue *value;
	value = gda_value_new (GDA_TYPE_DEFAULT);
	g_value_set_boxed (value, default_val);
	return value;
}



/**
 * gda_value_new_blob:
 * @val: value to set for the new #GValue.
 * @size: the size of the memory pool pointer to by @val.
 *
 * Makes a new #GValue of type #GDA_TYPE_BLOB with the data contained by @val.
 *
 * Returns: (transfer full): the newly created #GValue.
 *
 * Free-function: gda_value_free
 */
GValue *
gda_value_new_blob (const guchar *val, glong size)
{
	GValue *value;
	GdaBlob *blob;
	GdaBinary *bin;

	blob = gda_blob_new ();
	bin = gda_blob_get_binary (blob);
	gda_binary_set_data (bin, val, size);
	value = g_new0 (GValue, 1);
	g_value_init (value, GDA_TYPE_BLOB);
	g_value_take_boxed (value, blob);

	return value;
}

/**
 * gda_value_new_blob_from_file:
 * @filename: name of the file to manipulate
 *
 * Makes a new #GValue of type #GDA_TYPE_BLOB interfacing with the contents of the file
 * named @filename
 *
 * Returns: (transfer full): the newly created #GValue.
 *
 * Free-function: gda_value_free
 */
GValue *
gda_value_new_blob_from_file (const gchar *filename)
{
	GValue *value;
	GdaBlob *blob;

	blob = g_new0 (GdaBlob, 1);
	blob->op = _gda_dir_blob_op_new (filename);

        value = g_new0 (GValue, 1);
	g_value_init (value, GDA_TYPE_BLOB);
        g_value_take_boxed (value, blob);

        return value;
}

/*
 * Warning: DOES NOT modify @gmttm and loctm
 *
 * Returns: the offset, or G_MAXLONG in case of error
 */
static glong
compute_tz_offset (struct tm *gmttm, struct tm *loctm)
{
	if (! gmttm || !loctm)
		return G_MAXLONG;

	struct tm cgmttm, cloctm;
	cgmttm = *gmttm;
	cloctm = *loctm;

	time_t lt, gt;
	cgmttm.tm_isdst = 0;
	cloctm.tm_isdst = 0;

	lt = mktime (&cloctm);
	if (lt == -1)
		return G_MAXLONG;
	gt = mktime (&cgmttm);
	if (gt == -1)
		return G_MAXLONG;
	glong off;
	off = lt - gt;
	/*g_print ("%s(): %02d:%02d:%02d %d\n", __FUNCTION__,
	  loctm->tm_hour, loctm->tm_min, loctm->tm_sec, (gint) (off / 3600));*/

	if ((off >= 24 * 3600) || (off <= - 24 * 3600))
		return G_MAXLONG;
	else
		return off;
}

/**
 * gda_value_new_timestamp_from_timet:
 * @val: value to set for the new #GValue.
 *
 * Makes a new #GValue of type #GDA_TYPE_TIMESTAMP with value @val
 * (of type time_t). The returned timestamp's value is relative to the current
 * timezone (i.e. is localtime).
 *
 * For example, to get a time stamp representing the current date and time, use:
 *
 * <code>
 * ts = gda_value_new_timestamp_from_timet (time (NULL));
 * </code>
 *
 * Returns: (transfer full): the newly created #GValue, or %NULL in case of error
 *
 * Free-function: gda_value_free
 */
GValue *
gda_value_new_timestamp_from_timet (time_t val)
{
	GValue *value = NULL;
	struct tm *ltm = NULL;
	glong tz = 0;

#ifdef HAVE_LOCALTIME_R
	struct tm gmttm, loctm;
	tzset ();
	ltm = localtime_r ((const time_t *) &val, &loctm);
	tz = compute_tz_offset (gmtime_r ((const time_t *) &val, &gmttm), &loctm);
	if (tz == G_MAXLONG)
		ltm = NULL;
#elif HAVE_LOCALTIME_S
	struct tm gmttm, loctm;
	if ((localtime_s (&loctm, (const time_t *) &val) == 0) &&
	    (gmtime_s (&gmttm, (const time_t *) &val) == 0)) {
		tz = compute_tz_offset (&gmttm, &loctm);
		if (tz != G_MAXLONG)
			ltm = &loctm;
	}
#else
	struct tm gmttm, loctm;
	ltm = gmtime ((const time_t *) &val);
	if (ltm) {
		gmttm = *ltm;
		ltm = localtime ((const time_t *) &val);
		if (ltm) {
			loctm = *ltm;
			tz = compute_tz_offset (&gmttm, &loctm);
			if (tz == G_MAXLONG)
				ltm = NULL;
		}
	}
	
#endif

        if (ltm) {
                GdaTimestamp* tstamp = gda_timestamp_new_from_values (ltm->tm_year + 1900,
                                                                      ltm->tm_mon + 1,
                                                                      ltm->tm_mday,
                                                                      ltm->tm_hour,
                                                                      ltm->tm_min,
                                                                      ltm->tm_sec,
                                                                      0,
                                                                      tz);

		value = g_new0 (GValue, 1);
                gda_value_set_timestamp (value, (const GdaTimestamp *) tstamp);
		gda_timestamp_free (tstamp);
        }

        return value;
}

/**
 * gda_value_new_time_from_timet:
 * @val: value to set for the new #GValue.
 *
 * Makes a new #GValue of type #GDA_TYPE_TIME with value @val
 * (of type time_t). The returned times's value is relative to the current
 * timezone (i.e. is localtime).
 *
 * For example, to get a time representing the current time, use:
 *
 * <code>
 * ts = gda_value_new_time_from_timet (time (NULL));
 * </code>
 *
 * Returns: (transfer full): the newly created #GValue, or %NULL in case of error
 *
 * Free-function: gda_value_free
 *
 * Since: 6.0
 */
GValue *
gda_value_new_time_from_timet (time_t val)
{
	GValue *value = NULL;
	struct tm *ltm = NULL;
	glong tz = 0;

#ifdef HAVE_LOCALTIME_R
	struct tm gmttm, loctm;
	tzset ();
	ltm = localtime_r ((const time_t *) &val, &loctm);
	tz = compute_tz_offset (gmtime_r ((const time_t *) &val, &gmttm), &loctm);
	if (tz == G_MAXLONG)
		ltm = NULL;
#elif HAVE_LOCALTIME_S
	struct tm gmttm, loctm;
	if ((localtime_s (&loctm, (const time_t *) &val) == 0) &&
	    (gmtime_s (&gmttm, (const time_t *) &val) == 0)) {
		tz = compute_tz_offset (&gmttm, &loctm);
		if (tz != G_MAXLONG)
			ltm = &loctm;
	}
#else
	struct tm gmttm, loctm;
	ltm = gmtime ((const time_t *) &val);
	if (ltm) {
		gmttm = *ltm;
		ltm = localtime ((const time_t *) &val);
		if (ltm) {
			loctm = *ltm;
			tz = compute_tz_offset (&gmttm, &loctm);
			if (tz == G_MAXLONG)
				ltm = NULL;
		}
	}
	
#endif

        if (ltm) {
                GdaTime tim;
                tim.hour = ltm->tm_hour;
                tim.minute = ltm->tm_min;
                tim.second = ltm->tm_sec;
                tim.fraction = 0;
                tim.timezone = tz;

		value = g_new0 (GValue, 1);
                gda_value_set_time (value, (const GdaTime *) &tim);
        }

        return value;
}

/**
 * gda_value_new_from_string:
 * @as_string: stringified representation of the value.
 * @type: the new value type.
 *
 * Makes a new #GValue of type @type from its string representation.
 *
 * For more information
 * about the string format, see the gda_value_set_from_string() function.
 * This function is typically used when reading configuration files or other non-user input that should be locale
 * independent.
 *
 * Returns: (transfer full): the newly created #GValue or %NULL if the string representation cannot be converted to the specified @type.
 *
 * Free-function: gda_value_free
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
 * gda_value_new_from_xml: (skip)
 * @node: an XML node representing the value.
 *
 * Creates a GValue from an XML representation of it. That XML
 * node corresponds to the following string representation:
 * &lt;value type="gdatype"&gt;value&lt;/value&gt;
 *
 * For more information
 * about the string format, see the gda_value_set_from_string() function.
 * This function is typically used when reading configuration files or other non-user input that should be locale
 * independent.
 *
 * Returns: (transfer full): the newly created #GValue.
 *
 * Free-function: gda_value_free
 */
GValue *
gda_value_new_from_xml (const xmlNodePtr node)
{
	GValue *value;
	xmlChar *prop;

	g_return_val_if_fail (node, NULL);

	/* parse the XML */
	if (!node || !(node->name) || (node && strcmp ((gchar*)node->name, "value")))
		return NULL;

	value = g_new0 (GValue, 1);
	prop = xmlGetProp (node, (xmlChar*) "gdatype");
	if (prop && !gda_value_set_from_string (value,
						(gchar*)xmlNodeGetContent (node),
						gda_g_type_from_string ((gchar*) prop))) {
		g_free (value);
		value = NULL;
	}
	if (prop)
		xmlFree (prop);

	return value;
}

/**
 * gda_value_free:
 * @value: (transfer full) (allow-none): the resource to free (or %NULL)
 *
 * Deallocates all memory associated to a #GValue.
 */
void
gda_value_free (GValue *value)
{
	if (!value)
		return;
	l_g_value_unset (value);
	g_free (value);
}

/**
 * gda_value_reset_with_type:
 * @value: the #GValue to be reseted
 * @type:  the #GType to set to
 *
 * Resets the #GValue and set a new type to #GType.
*/
void
gda_value_reset_with_type (GValue *value, GType type)
{
	g_return_if_fail (value);

	if (G_IS_VALUE (value) && (G_VALUE_TYPE (value) == type))
		g_value_reset (value);
	else {
		l_g_value_unset (value);
		if (type == G_TYPE_INVALID)
			return;
		else
			g_value_init (value, type);
	}
}



/**
 * gda_value_is_null:
 * @value: value to test.
 *
 * Tests if a given @value is of type #GDA_TYPE_NULL.
 *
 * Returns: a boolean that says whether or not @value is of type #GDA_TYPE_NULL.
 */
gboolean
gda_value_is_null (const GValue *value)
{
	g_return_val_if_fail (value, FALSE);
	return gda_value_isa (value, GDA_TYPE_NULL);
}

/**
 * gda_value_is_number:
 * @value: a #GValue.
 *
 * Gets whether the value stored in the given #GValue is of numeric type or not.
 *
 * Returns: %TRUE if a number, %FALSE otherwise.
 */
gboolean
gda_value_is_number (const GValue *value)
{
	g_return_val_if_fail (value, FALSE);
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
 * gda_value_copy:
 * @value: value to get a copy from.
 *
 * Creates a new #GValue from an existing one.
 *
 * Returns: (transfer full): a newly allocated #GValue with a copy of the data in @value.
 *
 * Free-function: gda_value_free
 */
GValue *
gda_value_copy (const GValue *value)
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
 * gda_value_get_binary:
 * @value: a #GValue whose value we want to get.
 *
 * Returns: (transfer none): the value stored in @value.
 */
GdaBinary *
gda_value_get_binary (const GValue *value)
{
	GdaBinary *val;

	g_return_val_if_fail (value, NULL);
	g_return_val_if_fail (gda_value_isa (value, GDA_TYPE_BINARY), NULL);

	val = (GdaBinary*) g_value_get_boxed (value);

	return val;
}


/**
 * gda_value_set_binary:
 * @value: a #GValue that will store @val.
 * @binary: a #GdaBinary structure with the data and its size to be stored in @value.
 *
 * Stores @val into @value.
 */
void
gda_value_set_binary (GValue *value, GdaBinary *binary)
{
	g_return_if_fail (value);

	l_g_value_unset (value);
	g_value_init (value, GDA_TYPE_BINARY);
	if (binary)
		g_value_set_boxed (value, binary);
	else
		g_value_set_boxed (value, gda_binary_new ());
}

/**
 * gda_value_take_binary:
 * @value: a #GValue that will store @val.
 * @binary: (transfer full): a #GdaBinary structure with the data and its size to be stored in @value.
 *
 * Stores @val into @value, but on the contrary to gda_value_set_binary(), the @binary
 * argument is not copied, but used as-is and it should be considered owned by @value.
 */
void
gda_value_take_binary (GValue *value, GdaBinary *binary)
{
	g_return_if_fail (value);
	g_return_if_fail (binary);

	l_g_value_unset (value);
	g_value_init (value, GDA_TYPE_BINARY);
	g_value_take_boxed (value, binary);
}

/**
 * gda_value_set_blob:
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
 * gda_value_get_blob:
 * @value: a #GValue whose value we want to get.
 *
 * Returns: (transfer none): the value stored in @value.
 */
const GdaBlob *
gda_value_get_blob (const GValue *value)
{
	GdaBlob *val;

	g_return_val_if_fail (value, NULL);
	g_return_val_if_fail (gda_value_isa (value, GDA_TYPE_BLOB), NULL);

	val = (GdaBlob*) g_value_get_boxed (value);

	return val;
}

/**
 * gda_value_take_blob:
 * @value: a #GValue that will store @val.
 * @blob: (transfer full): a #GdaBlob structure with the data and its size to be stored in @value.
 *
 * Stores @val into @value, but on the contrary to gda_value_set_blob(), the @blob
 * argument is not copied, but used as-is and it should be considered owned by @value.
 */
void
gda_value_take_blob (GValue *value, GdaBlob *blob)
{
	g_return_if_fail (value);
	g_return_if_fail (blob);

	l_g_value_unset (value);
	g_value_init (value, GDA_TYPE_BLOB);
	g_value_take_boxed (value, blob);
}

/**
 * gda_value_get_geometric_point:
 * @value: a #GValue whose value we want to get.
 *
 * Returns: (transfer none): the value stored in @value.
 */
const GdaGeometricPoint *
gda_value_get_geometric_point (const GValue *value)
{
	g_return_val_if_fail (value, NULL);
	g_return_val_if_fail (gda_value_isa (value, GDA_TYPE_GEOMETRIC_POINT), NULL);
	return (const GdaGeometricPoint *) g_value_get_boxed(value);
}

/**
 * gda_value_set_geometric_point: (skip)
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
 * gda_value_set_null: (skip)
 * @value: a #GValue that will store a value of type #GDA_TYPE_NULL.
 *
 * Sets the type of @value to #GDA_TYPE_NULL.
 */
void
gda_value_set_null (GValue *value)
{
	g_return_if_fail (value);
	gda_value_reset_with_type (value, GDA_TYPE_NULL);
}

/**
 * gda_value_get_numeric: (skip)
 * @value: a #GValue whose value we want to get.
 *
 * Returns: (transfer none): the value stored in @value.
 */
const GdaNumeric *
gda_value_get_numeric (const GValue *value)
{
	g_return_val_if_fail (value, NULL);
	g_return_val_if_fail (gda_value_isa (value, GDA_TYPE_NUMERIC), NULL);
	return (const GdaNumeric *) g_value_get_boxed(value);
}

/**
 * gda_value_set_numeric: (skip)
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
 * gda_value_get_short: (skip)
 * @value: a #GValue whose value we want to get.
 *
 * Returns: the value stored in @value.
 */
gshort
gda_value_get_short (const GValue *value)
{
	g_return_val_if_fail (value, -1);
	g_return_val_if_fail (gda_value_isa (value, GDA_TYPE_SHORT), -1);
	return (gshort) value->data[0].v_int;
}

/**
 * gda_value_set_short: (skip)
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
 * gda_value_get_ushort: (skip)
 * @value: a #GValue whose value we want to get.
 *
 * Returns: the value stored in @value.
 */
gushort
gda_value_get_ushort (const GValue *value)
{
	g_return_val_if_fail (value, -1);
	g_return_val_if_fail (gda_value_isa (value, GDA_TYPE_USHORT), -1);
	return (gushort) value->data[0].v_uint;
}

/**
 * gda_value_set_ushort: (skip)
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
 * gda_value_get_time: (skip)
 * @value: a #GValue whose value we want to get.
 *
 * Returns: (transfer none): the value stored in @value.
 */
const GdaTime *
gda_value_get_time (const GValue *value)
{
	g_return_val_if_fail (value, NULL);
	g_return_val_if_fail (gda_value_isa (value, GDA_TYPE_TIME), NULL);
	return (const GdaTime *) g_value_get_boxed(value);
}

/**
 * gda_value_set_time: (skip)
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
 * gda_value_get_timestamp: (skip)
 * @value: a #GValue whose value we want to get.
 *
 * Returns: (transfer none): the value stored in @value.
 */
const GdaTimestamp *
gda_value_get_timestamp (const GValue *value)
{
	g_return_val_if_fail (value, NULL);
	g_return_val_if_fail (gda_value_isa (value, GDA_TYPE_TIMESTAMP), NULL);
	return (const GdaTimestamp *) g_value_get_boxed(value);
}

/**
 * gda_value_set_timestamp: (skip)
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
 * gda_value_set_from_string: (skip)
 * @value: a #GValue that will store @val.
 * @as_string: the stringified representation of the value.
 * @type: the type of the value
 *
 * Stores the value data from its string representation as @type.
 *
 * The accepted formats are:
 * <itemizedlist>
 *   <listitem><para>G_TYPE_BOOLEAN: a caseless comparison is made with "true" or "false"</para></listitem>
 *   <listitem><para>numerical types: C locale format (dot as a fraction separator)</para></listitem>
 *   <listitem><para>G_TYPE_DATE: see <link linkend="gda-parse-iso8601-date">gda_parse_iso8601_date()</link></para></listitem>
 *   <listitem><para>GDA_TYPE_TIME: see <link linkend="gda-parse-iso8601-time">gda_parse_iso8601_time()</link></para></listitem>
 *   <listitem><para>GDA_TYPE_TIMESTAMP: see <link linkend="gda-parse-iso8601-timestamp">gda_parse_iso8601_timestamp()</link></para></listitem>
 * </itemizedlist>
 *
 * This function is typically used when reading configuration files or other non-user input that should be locale
 * independent.
 *
 * Returns: %TRUE if the value has been converted to @type from
 * its string representation; it not means that the value is converted
 * successfully, just that the transformation is available. %FALSE otherwise.
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
 * gda_value_set_from_value: (skip)
 * @value: a #GValue.
 * @from: the value to copy from.
 *
 * Sets the value of a #GValue from another #GValue. This
 * is different from #gda_value_copy, which creates a new #GValue.
 * #gda_value_set_from_value, on the other hand, copies the contents
 * of @copy into @value, which must already be allocated.
 *
 * If values are incompatible (see @g_value_type_compatible) then @value is set to a
 * #GDA_TYPE_NULL, and %FALSE is returned.
 *
 * Returns: %TRUE if successful, %FALSE otherwise.
 */
gboolean
gda_value_set_from_value (GValue *value, const GValue *from)
{
	g_return_val_if_fail (value, FALSE);
	g_return_val_if_fail (from, FALSE);

	if (G_IS_VALUE (from)) {
		if (g_value_type_compatible (G_VALUE_TYPE (from), G_VALUE_TYPE (value))) {
			g_value_reset (value);
			g_value_copy (from, value);
			return TRUE;
		}
		else {
			gda_value_set_null (value);
			return FALSE;
		}
	}
	else {
		l_g_value_unset (value);
		return TRUE;
	}
}

/**
 * gda_value_stringify:
 * @value: a #GValue.
 *
 * Converts a GValue to its string representation which is a human readable value. Note that the
 * returned string does not take into account the current locale of the user (on the contrary to the
 * #GdaDataHandler objects). Using this function should be limited to debugging and values serialization
 * purposes.
 *
 * Output is in the "C" locale for numbers, and dates are converted in a YYYY-MM-DD format.
 *
 * Returns: (transfer full): a new string, or %NULL if the conversion cannot be done. Free the value with a g_free() when you've finished using it.
 */
gchar *
gda_value_stringify (const GValue *value)
{
	if (!value)
		return g_strdup ("NULL");

	GType type = G_VALUE_TYPE (value);
	if (type == G_TYPE_FLOAT) {
		char buffer[G_ASCII_DTOSTR_BUF_SIZE];
		g_ascii_formatd (buffer, sizeof (buffer), "%f", g_value_get_float (value));
		return g_strdup (buffer);
	}
	else if (type == G_TYPE_DOUBLE) {
		char buffer[G_ASCII_DTOSTR_BUF_SIZE];
		g_ascii_formatd (buffer, sizeof (buffer), "%f", g_value_get_double (value));
		return g_strdup (buffer);
	}
	else if (type == GDA_TYPE_NUMERIC)
		return gda_numeric_get_string (gda_value_get_numeric (value));
	else if (type == G_TYPE_DATE) {
		GDate *date;
		date = (GDate *) g_value_get_boxed (value);
		if (date) {
			if (g_date_valid (date))
				return g_strdup_printf ("%04u-%02u-%02u",
							g_date_get_year (date),
							g_date_get_month (date),
							g_date_get_day (date));
			else
				return g_strdup_printf ("%04u-%02u-%02u",
							date->year, date->month, date->day);
		}
		else
			return g_strdup ("0000-00-00");
	}
  else if (g_type_is_a (type, G_TYPE_DATE_TIME)) {
    GDateTime *ts;
    ts = (GDateTime*) g_value_get_boxed (value);
    return g_date_time_format (ts, "%FT%H:%M:%S%:::z");
  }
	else if (g_value_type_transformable (G_VALUE_TYPE (value), G_TYPE_STRING)) {
		GValue *string;
		gchar *str;

		string = g_value_init (g_new0 (GValue, 1), G_TYPE_STRING);
		g_value_transform (value, string);
		str = g_value_dup_string (string);
		gda_value_free (string);
		return str;
	}
	else if (G_TYPE_IS_OBJECT (type)) {
		GObject *obj;
		obj = g_value_get_object (value);
		return g_strdup_printf ("%p (%s)", obj, G_OBJECT_TYPE_NAME (obj));
	}
	else
		return g_strdup ("");
}

/**
 * gda_value_differ:
 * @value1: a #GValue to compare.
 * @value2: the other #GValue to be compared to @value1.
 *
 * Tells if two values are equal or not, by comparing memory representations. Unlike gda_value_compare(),
 * the returned value is boolean, and gives no idea about ordering.
 *
 * The two values must be of the same type, with the exception that a value of any type can be
 * compared to a GDA_TYPE_NULL value, specifically:
 * <itemizedlist>
 *   <listitem><para>if @value1 and @value2 are both GDA_TYPE_NULL values then the returned value is 0</para></listitem>
 *   <listitem><para>if @value1 is a GDA_TYPE_NULL value and @value2 is of another type then the returned value is 1</para></listitem>
 *   <listitem><para>if @value1 is of another type and @value2 is a GDA_TYPE_NULL value then the returned value is 1</para></listitem>
 *   <listitem><para>in all other cases, @value1 and @value2 must be of the same type and their values are compared</para></listitem>
 * </itemizedlist>
 *
 * Returns: a non 0 value if @value1 and @value2 differ, and 0 if they are equal
 */
gint
gda_value_differ (const GValue *value1, const GValue *value2)
{
	GType type;
	g_return_val_if_fail (value1 && value2, FALSE);

	/* blind value comparison */
	type = G_VALUE_TYPE (value1);
	if (!bcmp (value1, value2, sizeof (GValue)))
		return 0;

	/* handle GDA_TYPE_NULL comparisons with other types */
	else if (type == GDA_TYPE_NULL) {
		if (G_VALUE_TYPE (value2) == GDA_TYPE_NULL)
			return 0;
		else
			return 1;
	}

	else if (G_VALUE_TYPE (value2) == GDA_TYPE_NULL)
		return 1;

	g_return_val_if_fail (G_VALUE_TYPE (value1) == G_VALUE_TYPE (value2), 1);

	/* general case */
	if (type == GDA_TYPE_BINARY) {
		const GdaBinary *binary1 = gda_value_get_binary ((GValue*) value1);
		const GdaBinary *binary2 = gda_value_get_binary ((GValue*) value2);
		if (binary1 && binary2 && (binary1->binary_length == binary2->binary_length))
			return bcmp (binary1->data, binary2->data, binary1->binary_length);
		else
			return 1;
	}

	else if (type == GDA_TYPE_BLOB) {
		const GdaBlob *blob1 = gda_value_get_blob (value1);
		const GdaBlob *blob2 = gda_value_get_blob (value2);
		if (blob1 && blob2 && (((GdaBinary *)blob1)->binary_length == ((GdaBinary *)blob2)->binary_length)) {
			if (blob1->op == blob2->op)
				return bcmp (((GdaBinary *)blob1)->data, ((GdaBinary *)blob2)->data,
					     ((GdaBinary *)blob1)->binary_length);
		}
		return 1;
	}

	else if (type == G_TYPE_DATE) {
		GDate *d1, *d2;

		d1 = (GDate *) g_value_get_boxed (value1);
		d2 = (GDate *) g_value_get_boxed (value2);
		if (d1 && d2)
			return g_date_compare (d1, d2);
		return 1;
	}

	else if (type == GDA_TYPE_GEOMETRIC_POINT) {
		const GdaGeometricPoint *p1, *p2;
		p1 = gda_value_get_geometric_point (value1);
		p2 = gda_value_get_geometric_point (value2);
		if (p1 && p2)
			return bcmp (p1, p2, sizeof (GdaGeometricPoint));
		return 1;
	}

	else if (type == G_TYPE_OBJECT) {
		if (g_value_get_object (value1) == g_value_get_object (value2))
			return 0;
		else
			return -1;
	}

	else if (type == GDA_TYPE_NUMERIC) {
		const GdaNumeric *num1, *num2;
		num1= gda_value_get_numeric (value1);
		num2 = gda_value_get_numeric (value2);
                if (num1 && num2)
			return strcmp (num1->number, num2->number);
		return 1;
	}

	else if (type == G_TYPE_STRING)	{
		const gchar *str1, *str2;
		str1 = g_value_get_string (value1);
		str2 = g_value_get_string (value2);
		if (str1 && str2)
			return strcmp (str1, str2);
		return 1;
	}

	else if (type == GDA_TYPE_TIME) {
		const GdaTime *t1, *t2;
		t1 = gda_value_get_time (value1);
		t2 = gda_value_get_time (value2);
		if (t1 && t2) {
			gchar *stz = g_strdup_printf ("%ld", gda_time_get_timezone (t1));
			gchar *sec = g_strdup_printf ("%u.%ld", gda_time_get_minute (t1), gda_time_get_fraction (t1));
			gdouble seconds = g_strtod (sec, NULL);
			GTimeZone *tz = g_time_zone_new (stz);
			GDateTime *dt1 = g_date_time_new (tz, 1970, 1, 1,
																				gda_time_get_hour (t1),
																				gda_time_get_minute (t1),
																				seconds);
			g_free (stz);
			g_free (sec);
			g_time_zone_unref (tz);
			stz = g_strdup_printf ("%ld", gda_time_get_timezone (t1));
			sec = g_strdup_printf ("%u.%ld", gda_time_get_minute (t1), gda_time_get_fraction (t1));
			seconds = g_strtod (sec, NULL);
			tz = g_time_zone_new (stz);
			GDateTime *dt2 = g_date_time_new (tz, 1970, 1, 1,
																				gda_time_get_hour (t1),
																				gda_time_get_minute (t1),
																				seconds);
			g_free (stz);
			g_free (sec);
			g_time_zone_unref (tz);
			gint res = (gint) g_date_time_difference (dt1, dt2);
			g_date_time_unref (dt1);
			g_date_time_unref (dt2);
			return res;
    }
			return bcmp (t1, t2, sizeof (GdaTime));
		return 1;
	}

	else if (g_type_is_a (type, G_TYPE_DATE_TIME)) {
		GDateTime *ts1, *ts2;
		ts1 = (GDateTime*) g_value_get_boxed (value1);
		ts2 = (GDateTime*) g_value_get_boxed (value2);
		if (ts1 && ts2)
			return (gint) g_date_time_difference (ts1, ts2);
		return 1;
	}

	else if ((type == G_TYPE_INT) ||
		 (type == G_TYPE_UINT) ||
		 (type == G_TYPE_INT64) ||
		 (type == G_TYPE_UINT64) ||
		 (type == GDA_TYPE_SHORT) ||
		 (type == GDA_TYPE_USHORT) ||
		 (type == G_TYPE_FLOAT) ||
		 (type == G_TYPE_DOUBLE) ||
		 (type == G_TYPE_BOOLEAN) ||
		 (type == G_TYPE_CHAR) ||
		 (type == G_TYPE_UCHAR) ||
		 (type == G_TYPE_LONG) ||
		 (type == G_TYPE_ULONG) ||
		 (type == G_TYPE_GTYPE))
		/* values here ARE different because otherwise the bcmp() at the beginning would
		 * already have retruned */
		return 1;

	else if (g_type_is_a (type, G_TYPE_OBJECT)) {
		if (g_value_get_object (value1) == g_value_get_object (value2))
			return 0;
		else
			return -1;
	}

	g_warning ("%s() cannot handle values of type %s", __FUNCTION__, g_type_name (G_VALUE_TYPE (value1)));

	return 1;
}

/**
 * gda_value_compare:
 * @value1: a #GValue to compare (not %NULL)
 * @value2: the other #GValue to be compared to @value1 (not %NULL)
 *
 * Compares two values of the same type, with the exception that a value of any type can be
 * compared to a GDA_TYPE_NULL value, specifically:
 * <itemizedlist>
 *   <listitem><para>if @value1 and @value2 are both GDA_TYPE_NULL values then the returned value is 0</para></listitem>
 *   <listitem><para>if @value1 is a GDA_TYPE_NULL value and @value2 is of another type then the returned value is -1</para></listitem>
 *   <listitem><para>if @value1 is of another type and @value2 is a GDA_TYPE_NULL value then the returned value is 1</para></listitem>
 *   <listitem><para>in all other cases, @value1 and @value2 must be of the same type and their values are compared</para></listitem>
 * </itemizedlist>
 *
 * Returns: if both values have the same type, returns 0 if both contain
 * the same value, an integer less than 0 if @value1 is less than @value2 or
 * an integer greater than 0 if @value1 is greater than @value2.
 */
gint
gda_value_compare (const GValue *value1, const GValue *value2)
{
	gint retval;
	GType type;

	g_return_val_if_fail (value1 && value2, -1);

	type = G_VALUE_TYPE (value1);

	if (value1 == value2)
		return 0;

	/* handle GDA_TYPE_NULL comparisons with other types */
	else if (type == GDA_TYPE_NULL) {
		if (G_VALUE_TYPE (value2) == GDA_TYPE_NULL)
			return 0;
		else
			return -1;
	}

	else if (G_VALUE_TYPE (value2) == GDA_TYPE_NULL)
		return 1;

	/* general case */
	g_return_val_if_fail (G_VALUE_TYPE (value1) == G_VALUE_TYPE (value2), -1);

	if (type == G_TYPE_INT64) {
		gint64 i1 = g_value_get_int64 (value1);
		gint64 i2 = g_value_get_int64 (value2);
		return (i1 > i2) ? 1 : ((i1 == i2) ? 0 : -1);
	}

	else if (type == G_TYPE_UINT64) {
		guint64 i1 = g_value_get_uint64 (value1);
		guint64 i2 = g_value_get_uint64 (value2);
		return (i1 > i2) ? 1 : ((i1 == i2) ? 0 : -1);
	}

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
		if (blob1 && blob2 && (((GdaBinary *)blob1)->binary_length == ((GdaBinary *)blob2)->binary_length)) {
			if (blob1->op == blob2->op)
				return memcmp (((GdaBinary *)blob1)->data, ((GdaBinary *)blob2)->data,
					       ((GdaBinary *)blob1)->binary_length);
			else
				return -1;
		}
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

	else if (type == GDA_TYPE_GEOMETRIC_POINT) {
		const GdaGeometricPoint *p1, *p2;
		p1 = gda_value_get_geometric_point (value1);
		p2 = gda_value_get_geometric_point (value2);
		if (p1 && p2)
			return memcmp (p1, p2, sizeof (GdaGeometricPoint));
		else if (p1)
			return 1;
		else if (p2)
			return -1;
		else
			return 0;
	}

	else if (type == G_TYPE_OBJECT) {
		if (g_value_get_object (value1) == g_value_get_object (value2))
			return 0;
		else
			return -1;
	}

	else if (type == G_TYPE_INT)
		return g_value_get_int (value1) - g_value_get_int (value2);

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
		else
			retval = -1;
		return retval;
	}

	else if (type == G_TYPE_FLOAT) {
		gfloat f1 = g_value_get_float (value1);
		gfloat f2 = g_value_get_float (value2);
		return (f1 > f2) ? 1 : ((f1 == f2) ? 0 : -1);
	}

	else if (type == GDA_TYPE_SHORT) {
		gshort i1 = gda_value_get_short (value1);
		gshort i2 = gda_value_get_short (value2);
		return (i1 > i2) ? 1 : ((i1 == i2) ? 0 : -1);
	}

	else if (type == G_TYPE_ULONG) {
		gulong i1 = g_value_get_ulong (value1);
		gulong i2 = g_value_get_ulong (value2);
		return (i1 > i2) ? 1 : ((i1 == i2) ? 0 : -1);
	}

	else if (type == G_TYPE_LONG) {
		glong i1 = g_value_get_long (value1);
		glong i2 = g_value_get_long (value2);
		return (i1 > i2) ? 1 : ((i1 == i2) ? 0 : -1);
	}

	else if (type == GDA_TYPE_USHORT) {
		gushort i1 = gda_value_get_ushort (value1);
		gushort i2 = gda_value_get_ushort (value2);
		return (i1 > i2) ? 1 : ((i1 == i2) ? 0 : -1);
	}

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

	else if (type == GDA_TYPE_TIME) {
		const GdaTime *t1, *t2;
		t1 = gda_value_get_time (value1);
		t2 = gda_value_get_time (value2);
		if (t1 && t2)
			return memcmp (t1, t2, sizeof (GdaTime));
		else if (t1)
			return 1;
		else if (t2)
			return -1;
		else
			return 0;
	}

	else if (type == GDA_TYPE_TIMESTAMP) {
		const GdaTimestamp *ts1, *ts2;
		ts1 = gda_value_get_timestamp (value1);
		ts2 = gda_value_get_timestamp (value2);
		if (ts1 && ts2)
			return g_date_time_compare ((GDateTime*) ts1, (GDateTime*) ts2);
		else if (ts1)
			return 1;
		else if (ts2)
			return -1;
		else
			return 0;
	}

	else if (type == G_TYPE_CHAR) {
		gint8 c1 = g_value_get_schar (value1);
		gint8 c2 = g_value_get_schar (value2);
		return (c1 > c2) ? 1 : ((c1 == c2) ? 0 : -1);
	}

	else if (type == G_TYPE_UCHAR) {
		guchar c1 = g_value_get_uchar (value1);
		guchar c2 = g_value_get_uchar (value2);
		return (c1 > c2) ? 1 : ((c1 == c2) ? 0 : -1);
	}

	else if (type == G_TYPE_UINT) {
		guint i1 = g_value_get_uint (value1);
		guint i2 = g_value_get_uint (value2);
		return (i1 > i2) ? 1 : ((i1 == i2) ? 0 : -1);
	}

	else if (type == G_TYPE_GTYPE) {
		GType t1 = g_value_get_gtype (value1);
		GType t2 = g_value_get_gtype (value2);
		return (t1 > t2) ? 1 : ((t1 == t2) ? 0: -1);
	}

	else if (g_type_is_a (type, G_TYPE_OBJECT)) {
		if (g_value_get_object (value1) == g_value_get_object (value2))
			return 0;
		else
			return -1;
	}

	g_warning ("%s() cannot handle values of type %s", __FUNCTION__, g_type_name (G_VALUE_TYPE (value1)));

	return 0;
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

	g_return_val_if_fail (value, NULL);

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
 * gda_value_to_xml: (skip)
 * @value: a #GValue.
 *
 * Serializes the given #GValue to an XML node string.
 *
 * Returns: (transfer full): the XML node. Once not needed anymore, you should free it.
 */
xmlNodePtr
gda_value_to_xml (const GValue *value)
{
	gchar *valstr;
	xmlNodePtr retval;

	g_return_val_if_fail (value, NULL);

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
 * gda_binary_to_string:
 * @bin: a correctly filled @GdaBinary structure
 * @maxlen: a maximum len used to truncate, or %0 for no maximum length
 *
 * Converts all the non printable characters of bin->data into the "\xyz" representation
 * where "xyz" is the octal representation of the byte, and the '\' (backslash) character
 * is converted to "\\". Printable characters (defined by g_ascii_isprint()) as well as newline
 * character are not converted.
 *
 * Note that the backslash and newline characters are considered as printable characters and
 * will not be represented by the "\xyz" representation.
 *
 * Use this function to get a representation as much readable by humans as possible of a binary
 * chunk. Note that this function is internally called when transforming a binary value to
 * a string for example when using g_value_transform() or gda_value_stringify().
 *
 * Returns: (transfer full): a new string from @bin
 */
gchar *
gda_binary_to_string (const GdaBinary *bin, guint maxlen)
{
	gint nb_rewrites = 0;
	gchar *sptr, *rptr;
	gulong realsize = MYMIN ((gulong)(bin->binary_length), maxlen);

	gchar *retval;
	gulong offset = 0;

	if (!bin->data || (bin->binary_length == 0))
		return g_strdup ("");

	/* compute number of char rewrites */
	for (offset = 0, sptr = (gchar*) bin->data;
	     offset < realsize;
	     offset ++, sptr++) {
		if ((*sptr != '\n') && ((*sptr == '\\') || !g_ascii_isprint (*sptr)))
			nb_rewrites++;
	}

	/* mem allocation and copy */
	retval = g_malloc0 (realsize + nb_rewrites * 4 + 1);
	rptr = retval;
	sptr = (gchar*) bin->data;
	offset = 0;
	while (offset < realsize) {
		/* g_print (">%x<\n", (guchar) *ptr); */
		if ((*sptr == '\n') || ((*sptr != '\\') && g_ascii_isprint (*sptr))) {
			*rptr = *sptr;
			rptr ++;
		}
		else {
			if (*sptr == '\\') {
				*rptr = '\\';
				rptr++;
				*rptr = '\\';
				rptr++;
			}
			else {
				guchar val = *sptr;

				*rptr = '\\';
				rptr++;

				*rptr = val / 64 + '0';
				rptr++;
				val = val % 64;

				*rptr = val / 8 + '0';
				val = val % 8;
				rptr++;

				*rptr = val + '0';
				rptr++;
			}
		}

		sptr++;
		offset ++;
	}

	return retval;
}

/**
 * gda_string_to_binary:
 * @str: (allow-none): a string to convert, or %NULL
 *
 * Performs the reverse of gda_binary_to_string() (note that for any "\xyz" succession
 * of 4 characters where "xyz" represents a valid octal value, the resulting read value will
 * be modulo 256).
 *
 * I @str is %NULL, then an empty (i.e. where the @data part is %NULL) #GdaBinary is created and returned.
 *
 * Returns: (transfer full): a new #GdaBinary if no error were found in @str, or %NULL otherwise
 */
GdaBinary *
gda_string_to_binary (const gchar *str)
{
	GdaBinary *bin;
	glong len = 0, total;
	const guchar *sptr;
	guchar *rptr, *retval;

	if (!str) {
		bin = g_new0 (GdaBinary, 1);
		bin->data = NULL;
		bin->binary_length = 0;
		return bin;
	}

	total = strlen (str);
	retval = g_new0 (guchar, total + 1);
	sptr = (guchar*) str;
	rptr = retval;

	while (*sptr) {
		if (*sptr == '\\') {
			if (*(sptr+1) == '\\') {
				*rptr = '\\';
				sptr += 2;
			}
			else {
				guint tmp;
				if ((*(sptr+1) >= '0') && (*(sptr+1) <= '7') &&
				    (*(sptr+2) >= '0') && (*(sptr+2) <= '7') &&
				    (*(sptr+3) >= '0') && (*(sptr+3) <= '7')) {
					tmp = (*(sptr+1) - '0') * 64 +
						(*(sptr+2) - '0') * 8 +
						(*(sptr+3) - '0');
					sptr += 4;
					*rptr = tmp % 256;
				}
				else {
					g_free (retval);
					return NULL;
				}
			}
		}
		else {
			*rptr = *sptr;
			sptr++;
		}

		rptr++;
		len ++;
	}

	bin = g_new0 (GdaBinary, 1);
	bin->data = retval;
	bin->binary_length = len;

	return bin;
}

/**
 * gda_blob_to_string:
 * @blob: a correctly filled @GdaBlob structure
 * @maxlen: a maximum len used to truncate, or 0 for no maximum length
 *
 * Converts all the non printable characters of blob->data into the \xxx representation
 * where xxx is the octal representation of the byte, and the '\' (backslash) character
 * is converted to "\\".
 *
 * Returns: (transfer full): a new string from @blob
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
 * gda_string_to_blob:
 * @str: a string to convert
 *
 * Performs the reverse of gda_blob_to_string().
 *
 * Returns: (transfer full): a new #gdaBlob if no error were found in @str, or NULL otherwise
 */
GdaBlob *
gda_string_to_blob (const gchar *str)
{
	GdaBinary *bin;
	bin = gda_string_to_binary (str);
	if (bin) {
		GdaBlob *blob;
		blob = g_new0 (GdaBlob, 1);
		((GdaBinary*) blob)->data = bin->data;
		((GdaBinary*) blob)->binary_length = bin->binary_length;
		blob->op = NULL;
		g_free (bin);
		return blob;
	}
	else
		return NULL;
}
