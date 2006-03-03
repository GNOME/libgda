/* GDA common library
 * Copyright (C) 1998 - 2006 The GNOME Foundation.
 *
 * AUTHORS:
 *	Rodrigo Moya <rodrigo@gnome-db.org>
 *	Juan-Mariano de Goyeneche <jmseyas@dit.upm.es> (BLOB issues)
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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <glib/gfileutils.h>
#include <glib/gmessages.h>
#include <glib/gstrfuncs.h>
#include <glib/gprintf.h>
#include <libsql/sql_parser.h>
#include <glib/gi18n-lib.h>
#include <libgda/gda-log.h>
#include <libgda/gda-util.h>
#include <libgda/gda-column.h>
#include <libgda/gda-dict-field.h>

/**
 * gda_type_to_string
 * @type: Type to convert from.
 *
 * Returns: the string representing the given #GdaValueType.
 * This is not necessarily the same string used to describe the column type in a SQL statement.
 * Use gda_connection_get_schema() with GDA_CONNECTION_SCHEMA_TYPES to get the actual types supported by the provider.
 */
const gchar *
gda_type_to_string (GdaValueType type)
{
	switch (type) {
	case GDA_VALUE_TYPE_NULL : return "null";
	case GDA_VALUE_TYPE_BIGINT : return "bigint";
	case GDA_VALUE_TYPE_BIGUINT : return "biguint";
	case GDA_VALUE_TYPE_BLOB : return "blob";
	case GDA_VALUE_TYPE_BINARY : return "binary";
	case GDA_VALUE_TYPE_BOOLEAN : return "boolean";
	case GDA_VALUE_TYPE_DATE : return "date";
	case GDA_VALUE_TYPE_DOUBLE : return "double";
	case GDA_VALUE_TYPE_GEOMETRIC_POINT : return "point";
	case GDA_VALUE_TYPE_INTEGER : return "integer";
	case GDA_VALUE_TYPE_UINTEGER : return "uinteger";
	case GDA_VALUE_TYPE_LIST : return "list";
	case GDA_VALUE_TYPE_NUMERIC : return "numeric";
	case GDA_VALUE_TYPE_MONEY : return "money"; 
	case GDA_VALUE_TYPE_SINGLE : return "single";
	case GDA_VALUE_TYPE_SMALLINT : return "smallint";
	case GDA_VALUE_TYPE_SMALLUINT : return "smalluint";
	case GDA_VALUE_TYPE_STRING : return "string";
	case GDA_VALUE_TYPE_TIME : return "time";
	case GDA_VALUE_TYPE_TIMESTAMP : return "timestamp";
	case GDA_VALUE_TYPE_TINYINT : return "tinyint";
	case GDA_VALUE_TYPE_TINYUINT : return "tinyuint";
    
	case GDA_VALUE_TYPE_GOBJECT : return "gobject";
	case GDA_VALUE_TYPE_TYPE : return "type";
	case GDA_VALUE_TYPE_UNKNOWN : return "unknown";
	default: break;
	}

	return "unknown";
}

/**
 * gda_type_from_string
 * @str: the name of a #GdaValueType, as returned by gda_type_to_string().
 *
 * Returns: the #GdaValueType represented by the given @str.
 */
GdaValueType
gda_type_from_string (const gchar *str)
{
	g_return_val_if_fail (str != NULL, GDA_VALUE_TYPE_UNKNOWN);

	if (!g_strcasecmp (str, "null")) return GDA_VALUE_TYPE_NULL;
	else if (!g_strcasecmp (str, "bigint")) return GDA_VALUE_TYPE_BIGINT;
	else if (!g_strcasecmp (str, "biguint")) return GDA_VALUE_TYPE_BIGUINT;
	else if (!g_strcasecmp (str, "binary")) return GDA_VALUE_TYPE_BINARY;
	else if (!g_strcasecmp (str, "blob")) return GDA_VALUE_TYPE_BLOB;
	else if (!g_strcasecmp (str, "boolean")) return GDA_VALUE_TYPE_BOOLEAN;
	else if (!g_strcasecmp (str, "date")) return GDA_VALUE_TYPE_DATE;
	else if (!g_strcasecmp (str, "double")) return GDA_VALUE_TYPE_DOUBLE;
	else if (!g_strcasecmp (str, "point")) return GDA_VALUE_TYPE_GEOMETRIC_POINT;
	else if (!g_strcasecmp (str, "integer")) return GDA_VALUE_TYPE_INTEGER;
	else if (!g_strcasecmp (str, "uinteger")) return GDA_VALUE_TYPE_UINTEGER;
	else if (!g_strcasecmp (str, "list")) return GDA_VALUE_TYPE_LIST;
	else if (!g_strcasecmp (str, "numeric")) return GDA_VALUE_TYPE_NUMERIC;
	else if (!g_strcasecmp (str, "money")) return GDA_VALUE_TYPE_MONEY;
	else if (!g_strcasecmp (str, "single")) return GDA_VALUE_TYPE_SINGLE;
	else if (!g_strcasecmp (str, "smallint")) return GDA_VALUE_TYPE_SMALLINT;
	else if (!g_strcasecmp (str, "smalluint")) return GDA_VALUE_TYPE_SMALLUINT;  
	else if (!g_strcasecmp (str, "string")) return GDA_VALUE_TYPE_STRING;
	else if (!g_strcasecmp (str, "time")) return GDA_VALUE_TYPE_TIME;
	else if (!g_strcasecmp (str, "timestamp")) return GDA_VALUE_TYPE_TIMESTAMP;
	else if (!g_strcasecmp (str, "tinyint")) return GDA_VALUE_TYPE_TINYINT;
	else if (!g_strcasecmp (str, "tinyuint")) return GDA_VALUE_TYPE_TINYUINT;
  
	else if (!g_strcasecmp (str, "gobject")) return GDA_VALUE_TYPE_GOBJECT;
	else if (!g_strcasecmp (str, "type")) return GDA_VALUE_TYPE_TYPE;  
	else if (!g_strcasecmp (str, "unknown")) return GDA_VALUE_TYPE_UNKNOWN;      
  
	return GDA_VALUE_TYPE_UNKNOWN;
}

/**
 * gda_default_escape_chars
 * @string: string to escape
 *
 * Escapes @string to make it understandable by a DBMS. The escape method is very common and replaces any
 * occurence of "'" with "\'" and "\" with "\\".
 */
gchar *
gda_default_escape_chars (const gchar *string)
{
	gchar *ptr, *ret, *retptr;
	gint size;

	if (!string)
		return NULL;
	
	/* determination of the new string size */
	ptr = (gchar *) string;
	size = 1;
	while (*ptr) {
		if ((*ptr == '\'') ||(*ptr == '\\'))
			size += 2;
		else
			size += 1;
		ptr++;
	}

	ptr = (gchar *) string;
	ret = g_new0 (gchar, size);
	retptr = ret;
	while (*ptr) {
		if ((*ptr == '\'') || (*ptr == '\\')) {
			*retptr = '\\';
			*(retptr+1) = *ptr;
			retptr += 2;
		}
		else {
			*retptr = *ptr;
			retptr ++;
		}
		ptr++;
	}
	*retptr = '\0';

	return ret;
}

/**
 * gda_default_unescape_chars
 * @string: string to unescape
 *
 * Do the reverse of gda_default_escape_chars(): transforms any "\'" into "'" and any
 * "\\" into "\". 
 *
 * Returns: a new unescaped string, or %NULL in an error was found in @string
 */
gchar *
gda_default_unescape_chars (const gchar *string)
{
	glong total;
	gchar *ptr;
	gchar *retval;
	glong offset = 0;
	
	if (!string) 
		return NULL;
	
	total = strlen (string);
	retval = g_memdup (string, total+1);
	ptr = (gchar *) retval;
	while (offset < total) {
		/* we accept the "''" as a synonym of "\'" */
		if (*ptr == '\'') {
			if (*(ptr+1) == '\'') {
				g_memmove (ptr+1, ptr+2, total - offset);
				offset += 2;
			}
			else {
				g_free (retval);
				return NULL;
			}
		}
		if (*ptr == '\\') {
			if (*(ptr+1) == '\\') {
				g_memmove (ptr+1, ptr+2, total - offset);
				offset += 2;
			}
			else {
				if (*(ptr+1) == '\'') {
					*ptr = '\'';
					g_memmove (ptr+1, ptr+2, total - offset);
					offset += 2;
				}
				else {
					g_free (retval);
					return NULL;
				}
			}
		}
		else
			offset ++;

		ptr++;
	}

	return retval;	
}

/* function called by g_hash_table_foreach to add items to a GList */
static void
add_string_key_to_list (gpointer key, gpointer value, gpointer user_data)
{
        GList **list = (GList **) user_data;

        *list = g_list_append (*list, g_strdup (key));
}

/**
 * gda_string_hash_to_list
 * @hash_table: a hash table.
 *
 * Creates a new list of strings, which contains all keys of a given hash 
 * table. After using it, you should free this list by calling g_list_free.
 * 
 * Returns: a new GList.
 */
GList *
gda_string_hash_to_list (GHashTable *hash_table)
{
	GList *list = NULL;

        g_return_val_if_fail (hash_table != NULL, NULL);

        g_hash_table_foreach (hash_table, (GHFunc) add_string_key_to_list, &list);
        return list;
}

/**
 * gda_sql_replace_placeholders
 * @sql: a SQL command containing placeholders for values.
 * @params: a list of values for the placeholders.
 *
 * Replaces the placeholders (:name) in the given SQL command with
 * the values from the #GdaParameterList specified as the @params
 * argument.
 *
 * Returns: the SQL string with all placeholders replaced, or %NULL
 * on error. On success, the returned string must be freed by the caller
 * when no longer needed.
 */
gchar *
gda_sql_replace_placeholders (const gchar *sql, GdaParameterList *params)
{
	sql_statement *sql_stmt;

	g_return_val_if_fail (sql != NULL, NULL);

	/* parse the string */
	sql_stmt = sql_parse (sql);
	if (!sql_stmt) {
		gda_log_error (_("Could not parse SQL command '%s'"), sql);
		return NULL;
	}

	/* FIXME */

	return NULL;
}

/**
 * gda_file_load
 * @filename: path for the file to be loaded.
 *
 * Loads a file, specified by the given @uri, and returns the file
 * contents as a string.
 *
 * It is the caller's responsibility to free the returned value.
 *
 * Returns: the file contents as a newly-allocated string, or %NULL
 * if there is an error.
 */
gchar *
gda_file_load (const gchar *filename)
{
	gchar *retval = NULL;
	gsize length = 0;
	GError *error = NULL;

	g_return_val_if_fail (filename != NULL, NULL);

	if (g_file_get_contents (filename, &retval, &length, &error))
		return retval;

	gda_log_error (_("Error while reading %s: %s"), filename, error->message);
	g_error_free (error);

	return NULL;
}

/**
 * gda_file_save
 * @filename: path for the file to be saved.
 * @buffer: contents of the file.
 * @len: size of @buffer.
 *
 * Saves a chunk of data into a file.
 *
 * Returns: %TRUE if successful, %FALSE on error.
 */
gboolean
gda_file_save (const gchar *filename, const gchar *buffer, gint len)
{
	gint fd;
	gint res;
	
	g_return_val_if_fail (filename != NULL, FALSE);

	fd = open (filename, O_RDWR | O_CREAT | O_TRUNC, 0644);
	if (fd == -1) {
		gda_log_error (_("Could not create file %s"), filename);
		return FALSE;
	}

	res = write (fd, (const void *) buffer, len);
	close (fd);

	return res == -1 ? FALSE : TRUE;
}

/**
 * utility_table_field_attrs_stringify
 */
gchar *
utility_table_field_attrs_stringify (guint attributes)
{
	if (attributes & FIELD_AUTO_INCREMENT)
		return g_strdup ("AUTO_INCREMENT");

	return NULL;
}

/**
 * utility_table_field_attrs_parse
 */
guint
utility_table_field_attrs_parse (const gchar *str)
{
	if (!str)
		return 0;

	if (!strcmp (str, "AUTO_INCREMENT"))
		return FIELD_AUTO_INCREMENT;

	return 0;
}


/**
 * utility_build_encoded_id
 *
 * Creates a BASE64 kind encoded string. It's not really a BASE64 because:
 * - the characters + and / of BASE64 are replaced with - and _
 * - no padding is done using the = character
 *
 * The created string is a valid NCName XML token.
 */
static inline unsigned char
to_uchar (gchar ch)
{
	return ch;
}

gchar *
utility_build_encoded_id (const gchar *prefix, const gchar *id)
{
	const gchar conv[64] = 
		"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
	gchar *str, *out;
	const gchar *in;
	int ln = 0, pln = 0;
	int offset;

	/* size computation */
	if (prefix) {
		pln = strlen (prefix);
		ln = pln;
	}

	ln += (strlen (id) * 4 + 2) / 3 + 1;
	str = g_new0 (char, ln);

	/* copy prefix */
	out = str;
	if (prefix) {
		strcpy (str, prefix);
		out += pln;
	}

	/* create data */
	offset = 4;
	for (in = id; offset == 4; in += 3) {
		offset = 0;
		
		if (in[0]) {
			offset = 2;
			
			out[0] = conv [to_uchar (in[0]) >> 2];

			if (in[1]) {
				offset = 3;
				out[1] = conv [((to_uchar (in[0]) << 4) + 
						(to_uchar (in[1]) >> 4)) & 0x3f];

				if (in[2]) {
					offset = 4;

					out[2] = conv [((to_uchar (in[1]) << 2) + 
							(to_uchar (in[2]) >> 6)) & 0x3f];
					out[3] = conv [to_uchar (in[2]) & 0x3f];
				}
				else
					out[2] = conv [(to_uchar (in[1]) << 2) & 0x3f];
			}
			else
				out[1] = conv [(to_uchar (in[0]) << 4) & 0x3f];


			out += offset;
		}
        }

	return str;
}

#define B64(x)	     \
  ((x) == 'A' ? 0    \
   : (x) == 'B' ? 1  \
   : (x) == 'C' ? 2  \
   : (x) == 'D' ? 3  \
   : (x) == 'E' ? 4  \
   : (x) == 'F' ? 5  \
   : (x) == 'G' ? 6  \
   : (x) == 'H' ? 7  \
   : (x) == 'I' ? 8  \
   : (x) == 'J' ? 9  \
   : (x) == 'K' ? 10 \
   : (x) == 'L' ? 11 \
   : (x) == 'M' ? 12 \
   : (x) == 'N' ? 13 \
   : (x) == 'O' ? 14 \
   : (x) == 'P' ? 15 \
   : (x) == 'Q' ? 16 \
   : (x) == 'R' ? 17 \
   : (x) == 'S' ? 18 \
   : (x) == 'T' ? 19 \
   : (x) == 'U' ? 20 \
   : (x) == 'V' ? 21 \
   : (x) == 'W' ? 22 \
   : (x) == 'X' ? 23 \
   : (x) == 'Y' ? 24 \
   : (x) == 'Z' ? 25 \
   : (x) == 'a' ? 26 \
   : (x) == 'b' ? 27 \
   : (x) == 'c' ? 28 \
   : (x) == 'd' ? 29 \
   : (x) == 'e' ? 30 \
   : (x) == 'f' ? 31 \
   : (x) == 'g' ? 32 \
   : (x) == 'h' ? 33 \
   : (x) == 'i' ? 34 \
   : (x) == 'j' ? 35 \
   : (x) == 'k' ? 36 \
   : (x) == 'l' ? 37 \
   : (x) == 'm' ? 38 \
   : (x) == 'n' ? 39 \
   : (x) == 'o' ? 40 \
   : (x) == 'p' ? 41 \
   : (x) == 'q' ? 42 \
   : (x) == 'r' ? 43 \
   : (x) == 's' ? 44 \
   : (x) == 't' ? 45 \
   : (x) == 'u' ? 46 \
   : (x) == 'v' ? 47 \
   : (x) == 'w' ? 48 \
   : (x) == 'x' ? 49 \
   : (x) == 'y' ? 50 \
   : (x) == 'z' ? 51 \
   : (x) == '0' ? 52 \
   : (x) == '1' ? 53 \
   : (x) == '2' ? 54 \
   : (x) == '3' ? 55 \
   : (x) == '4' ? 56 \
   : (x) == '5' ? 57 \
   : (x) == '6' ? 58 \
   : (x) == '7' ? 59 \
   : (x) == '8' ? 60 \
   : (x) == '9' ? 61 \
   : (x) == '-' ? 62 \
   : (x) == '_' ? 63 \
   : -1)

static const signed char b64[0x100] = {
  B64 (0), B64 (1), B64 (2), B64 (3),
  B64 (4), B64 (5), B64 (6), B64 (7),
  B64 (8), B64 (9), B64 (10), B64 (11),
  B64 (12), B64 (13), B64 (14), B64 (15),
  B64 (16), B64 (17), B64 (18), B64 (19),
  B64 (20), B64 (21), B64 (22), B64 (23),
  B64 (24), B64 (25), B64 (26), B64 (27),
  B64 (28), B64 (29), B64 (30), B64 (31),
  B64 (32), B64 (33), B64 (34), B64 (35),
  B64 (36), B64 (37), B64 (38), B64 (39),
  B64 (40), B64 (41), B64 (42), B64 (43),
  B64 (44), B64 (45), B64 (46), B64 (47),
  B64 (48), B64 (49), B64 (50), B64 (51),
  B64 (52), B64 (53), B64 (54), B64 (55),
  B64 (56), B64 (57), B64 (58), B64 (59),
  B64 (60), B64 (61), B64 (62), B64 (63),
  B64 (64), B64 (65), B64 (66), B64 (67),
  B64 (68), B64 (69), B64 (70), B64 (71),
  B64 (72), B64 (73), B64 (74), B64 (75),
  B64 (76), B64 (77), B64 (78), B64 (79),
  B64 (80), B64 (81), B64 (82), B64 (83),
  B64 (84), B64 (85), B64 (86), B64 (87),
  B64 (88), B64 (89), B64 (90), B64 (91),
  B64 (92), B64 (93), B64 (94), B64 (95),
  B64 (96), B64 (97), B64 (98), B64 (99),
  B64 (100), B64 (101), B64 (102), B64 (103),
  B64 (104), B64 (105), B64 (106), B64 (107),
  B64 (108), B64 (109), B64 (110), B64 (111),
  B64 (112), B64 (113), B64 (114), B64 (115),
  B64 (116), B64 (117), B64 (118), B64 (119),
  B64 (120), B64 (121), B64 (122), B64 (123),
  B64 (124), B64 (125), B64 (126), B64 (127),
  B64 (128), B64 (129), B64 (130), B64 (131),
  B64 (132), B64 (133), B64 (134), B64 (135),
  B64 (136), B64 (137), B64 (138), B64 (139),
  B64 (140), B64 (141), B64 (142), B64 (143),
  B64 (144), B64 (145), B64 (146), B64 (147),
  B64 (148), B64 (149), B64 (150), B64 (151),
  B64 (152), B64 (153), B64 (154), B64 (155),
  B64 (156), B64 (157), B64 (158), B64 (159),
  B64 (160), B64 (161), B64 (162), B64 (163),
  B64 (164), B64 (165), B64 (166), B64 (167),
  B64 (168), B64 (169), B64 (170), B64 (171),
  B64 (172), B64 (173), B64 (174), B64 (175),
  B64 (176), B64 (177), B64 (178), B64 (179),
  B64 (180), B64 (181), B64 (182), B64 (183),
  B64 (184), B64 (185), B64 (186), B64 (187),
  B64 (188), B64 (189), B64 (190), B64 (191),
  B64 (192), B64 (193), B64 (194), B64 (195),
  B64 (196), B64 (197), B64 (198), B64 (199),
  B64 (200), B64 (201), B64 (202), B64 (203),
  B64 (204), B64 (205), B64 (206), B64 (207),
  B64 (208), B64 (209), B64 (210), B64 (211),
  B64 (212), B64 (213), B64 (214), B64 (215),
  B64 (216), B64 (217), B64 (218), B64 (219),
  B64 (220), B64 (221), B64 (222), B64 (223),
  B64 (224), B64 (225), B64 (226), B64 (227),
  B64 (228), B64 (229), B64 (230), B64 (231),
  B64 (232), B64 (233), B64 (234), B64 (235),
  B64 (236), B64 (237), B64 (238), B64 (239),
  B64 (240), B64 (241), B64 (242), B64 (243),
  B64 (244), B64 (245), B64 (246), B64 (247),
  B64 (248), B64 (249), B64 (250), B64 (251),
  B64 (252), B64 (253), B64 (254), B64 (255)
};

#define isbase64(x) ((to_uchar (x) <= 255) && (0 <= b64[to_uchar (x)]))


/**
 * utility_build_decoded_id
 *
 * Reverse of utility_build_encoded_id()
 */
gchar *
utility_build_decoded_id (const gchar *prefix, const gchar *id)
{
	gchar *str;
	gchar *out;
	const gchar *in;
	int ln = 0;
	int offset;

	/* go to the first byte of actual data */
	in = id;
	if (prefix) {
		const gchar *tmp;
		tmp = prefix;
		while (*tmp) {
			in++;
			tmp++;
		}
	}

	ln = (strlen (in) * 3) / 4 + 3;
	str = g_new0 (char, ln);
	out = str;

	/* create data */
	offset = 3;
	for (; offset == 3; in += 4) {
		offset = 0;
		
		if (isbase64 (in[0]) && isbase64 (in[1])) {
			out[0] = ((b64 [to_uchar (in[0])] << 2) | 
				  (b64 [to_uchar (in[1])] >> 4));
			
			if (isbase64 (in[2])) {
				out[1] = (((b64 [to_uchar (in[1])] << 4) & 0xf0) |
					  (b64 [to_uchar (in[2])] >> 2));

				if (isbase64 (in[3])) {
					out[2] = (((b64 [to_uchar (in[2])] << 6) & 0xc0) |
						  b64 [to_uchar (in[3])]);
					offset = 3;
				}
			}
		}

		out += offset;
        }

	return str;
}


/**
 * utility_check_data_model
 * @model: a #GdaDataModel object
 * @nbcols: the minimum requested number of columns
 * @Varargs: @nbcols arguments of type GdaValueType or -1 (if any data type is accepted)
 *
 * Check the column types of a GdaDataModel.
 *
 * Returns: TRUE if the data model's columns match the provided data types and number
 */
gboolean
utility_check_data_model (GdaDataModel *model, gint nbcols, ...)
{
	gboolean retval = TRUE;
	gint i;

	g_return_val_if_fail (model && GDA_IS_DATA_MODEL (model), FALSE);
	
	/* number of columns */
	if (gda_data_model_get_n_columns (model) < nbcols)
		return FALSE;

	/* type of each column */
	if (nbcols > 0) {
		GdaColumn *att;
		GdaValueType mtype, rtype;
		gint argtype;
		va_list ap;

		va_start  (ap, nbcols);
		i = 0;
		while ((i<nbcols) && retval) {
			att = gda_data_model_describe_column (model, i);
			mtype = gda_column_get_gda_type (att);
			
			argtype = va_arg (ap, GdaValueType);
			if (argtype >= 0) {
				rtype = (GdaValueType) argtype;
				if (mtype != rtype) {
					retval = FALSE;
#ifdef GDA_DEBUG
					g_print ("Column %d: Expected %s, got %s\n",
						 i, gda_type_to_string (rtype), gda_type_to_string (mtype));
#endif
				}
			}
			
			i++;
		}
		va_end (ap);
	}

	return retval;

}
