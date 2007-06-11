/* GDA common library
 * Copyright (C) 1998 - 2007 The GNOME Foundation.
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
#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif

#include <libgda/binreloc/gda-binreloc.h>

/**
 * gda_g_type_to_string
 * @type: Type to convert from.
 *
 * Returns: the string representing the given #GType.
 */
const gchar *
gda_g_type_to_string (GType type)
{
	if (type == GDA_TYPE_NULL)
		return "null";
	else
		return g_type_name (type);
}

/**
 * gda_g_type_from_string
 * @str: the name of a #GType, as returned by gda_g_type_to_string().
 *
 * Returns: the #GType represented by the given @str.
 */
GType
gda_g_type_from_string (const gchar *str)
{
	GType type;
	g_return_val_if_fail (str != NULL, G_TYPE_INVALID);
	
	type = g_type_from_name (str);
	if (type == 0) {
		if (!strcmp (str, "null"))
			type = GDA_TYPE_NULL;
		else
			/* could not find a valid GType for @str */
			type = G_TYPE_INVALID;
	}

	return type;
}

/**
 * gda_default_escape_string
 * @string: string to escape
 *
 * Escapes @string to make it understandable by a DBMS. The escape method is very common and replaces any
 * occurence of "'" with "\'" and "\" with "\\".
 */
gchar *
gda_default_escape_string (const gchar *string)
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
 * gda_default_unescape_string
 * @string: string to unescape
 *
 * Do the reverse of gda_default_escape_string(): transforms any "\'" into "'" and any
 * "\\" into "\". 
 *
 * Returns: a new unescaped string, or %NULL in an error was found in @string
 */
gchar *
gda_default_unescape_string (const gchar *string)
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
 * gda_utility_table_field_attrs_stringify
 */
gchar *
gda_utility_table_field_attrs_stringify (GdaValueAttribute attributes)
{
	if (attributes & FIELD_AUTO_INCREMENT)
		return g_strdup ("AUTO_INCREMENT");

	return NULL;
}

/**
 * gda_utility_table_field_attrs_parse
 */
guint
gda_utility_table_field_attrs_parse (const gchar *str)
{
	if (!str)
		return 0;

	if (!strcmp (str, "AUTO_INCREMENT"))
		return FIELD_AUTO_INCREMENT;

	return 0;
}


/**
 * gda_utility_build_encoded_id
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
gda_utility_build_encoded_id (const gchar *prefix, const gchar *id)
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

/* This is the previous version:
 * It caused a warning: "comparison is always true due to limited range of data type"
 *
 * #define isbase64(x) ((to_uchar (x) <= 255) && (0 <= b64[to_uchar (x)]))
 *
 * murrayc
 */
#define isbase64(x) ((0 <= b64[to_uchar (x)]))


/**
 * gda_utility_build_decoded_id
 *
 * Reverse of gda_utility_build_encoded_id()
 */
gchar *
gda_utility_build_decoded_id (const gchar *prefix, const gchar *id)
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
 * gda_utility_check_data_model
 * @model: a #GdaDataModel object
 * @nbcols: the minimum requested number of columns
 * @Varargs: @nbcols arguments of type GType or -1 (if any data type is accepted)
 *
 * Check the column types of a GdaDataModel.
 *
 * Returns: TRUE if the data model's columns match the provided data types and number
 */
gboolean
gda_utility_check_data_model (GdaDataModel *model, gint nbcols, ...)
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
		GType mtype, rtype;
		gint argtype;
		va_list ap;

		va_start  (ap, nbcols);
		i = 0;
		while ((i<nbcols) && retval) {
			att = gda_data_model_describe_column (model, i);
			mtype = gda_column_get_g_type (att);
			
			argtype = va_arg (ap, GType);
			if (argtype >= 0) {
				rtype = (GType) argtype;
				if (mtype != rtype) {
					retval = FALSE;
#ifdef GDA_DEBUG
					g_print ("Column %d: Expected %s, got %s\n",
						 i, gda_g_type_to_string (rtype), gda_g_type_to_string (mtype));
#endif
				}
			}
			
			i++;
		}
		va_end (ap);
	}

	return retval;

}

/**
 * gda_utility_data_model_dump_data_to_xml
 * @model: 
 * @cols: an array containing which columns of @model will be exported, or %NULL for all columns
 * @nb_cols: the number of columns in @cols
 * @rows: an array containing which rows of @model will be exported, or %NULL for all rows
 * @nb_rows: the number of rows in @rows
 * @name: name to use for the XML resulting table.
 *
 * Dump the data in a #GdaDataModel into a xmlNodePtr (as used in libxml).
 */
void
gda_utility_data_model_dump_data_to_xml (GdaDataModel *model, xmlNodePtr parent, 
					 const gint *cols, gint nb_cols, 
					 const gint *rows, gint nb_rows,
					 gboolean use_col_ids)
{
	gint nrows, i;
	gint *rcols, rnb_cols;
	gchar **col_ids = NULL;

	/* compute columns if not provided */
	if (!cols) {
		rnb_cols = gda_data_model_get_n_columns (model);
		rcols = g_new (gint, rnb_cols);
		for (i = 0; i < rnb_cols; i++)
			rcols [i] = i;
	}
	else {
		rcols = (gint *) cols;
		rnb_cols = nb_cols;
	}

	if (use_col_ids) {
		gint c;
		col_ids = g_new0 (gchar *, rnb_cols);
		for (c = 0; c < rnb_cols; c++) {
			GdaColumn *column;
			const gchar *id;
		
			column = gda_data_model_describe_column (model, rcols [c]);
			g_object_get (G_OBJECT (column), "id", &id, NULL);
			if (id && *id)
				col_ids [c] = g_strdup (id);
			else
				col_ids [c] = g_strdup_printf ("_%d", c);
		}
	}

	/* add the model data to the XML output */
	if (!rows)
		nrows = gda_data_model_get_n_rows (model);
	else
		nrows = nb_rows;
	if (nrows > 0) {
		xmlNodePtr row, data, field;
		gint r, c;

		data = xmlNewChild (parent, NULL, (xmlChar*)"gda_array_data", NULL);
		for (r = 0; r < nrows; r++) {
			row = xmlNewChild (data, NULL,  (xmlChar*)"gda_array_row", NULL);
			for (c = 0; c < rnb_cols; c++) {
				GValue *value;
				gchar *str = NULL;
				gboolean isnull = FALSE;

				value = (GValue *) gda_data_model_get_value_at (model, rcols [c], rows ? rows [r] : r);
				if (!value || gda_value_is_null ((GValue *) value)) 
					isnull = TRUE;
				else {
					if (G_VALUE_TYPE (value) == G_TYPE_BOOLEAN)
						str = g_strdup (g_value_get_boolean (value) ? "TRUE" : "FALSE");
					else if (G_VALUE_TYPE (value) == G_TYPE_STRING) {
						if (!g_value_get_string (value))
							isnull = TRUE;
						else
							str = gda_value_stringify (value);	
					}
					else
						str = gda_value_stringify (value);
				}
				if (!use_col_ids) {
					if (! str || (str && (*str == 0)))
						field = xmlNewChild (row, NULL,  (xmlChar*)"gda_value", NULL);
					else
						field = xmlNewChild (row, NULL,  (xmlChar*)"gda_value", (xmlChar*)str);
				}
				else {
					field = xmlNewChild (row, NULL,  (xmlChar*)"gda_array_value", (xmlChar*)str);
					xmlSetProp(field, (xmlChar*)"colid",  (xmlChar*)col_ids [c]);
				}
				if (isnull)
					xmlSetProp(field,  (xmlChar*)"isnull", (xmlChar*)"t");

				g_free (str);
			}
		}
	}

	if (use_col_ids) {
		gint c;
		for (c = 0; c < rnb_cols; c++) 
			g_free (col_ids [c]);
		g_free (col_ids);
	}
}

/**
 * gda_utility_parameter_load_attributes
 * @param:
 * @node: an xmlNodePtr with a &lt;parameter&gt; tag
 * @sources: a list of #GdaDataModel
 *
 * WARNING: may set the "source" custom string property 
 */
void
gda_utility_parameter_load_attributes (GdaParameter *param, xmlNodePtr node, GSList *sources)
{
	xmlChar *str;
	xmlNodePtr vnode;

	/* set properties from the XML spec */
	str = xmlGetProp (node, BAD_CAST "id");
	if (str) {
		g_object_set (G_OBJECT (param), "string_id", (gchar*)str, NULL);
		xmlFree (str);
	}	

	str = xmlGetProp (node, BAD_CAST "name");
	if (str) {
		gda_object_set_name (GDA_OBJECT (param), (gchar*)str);
		xmlFree (str);
	}
	str = xmlGetProp (node, BAD_CAST "descr");
	if (str) {
		gda_object_set_description (GDA_OBJECT (param), (gchar*)str);
		xmlFree (str);
	}
	str = xmlGetProp (node, BAD_CAST "nullok");
	if (str) {
		gda_parameter_set_not_null (param, (*str == 'T') || (*str == 't') ? FALSE : TRUE);
		xmlFree (str);
	}
	else
		gda_parameter_set_not_null (param, FALSE);
	str = xmlGetProp (node, BAD_CAST "plugin");
	if (str) {
		g_object_set (G_OBJECT (param), "entry_plugin", (gchar*)str, NULL);
		xmlFree (str);
	}
	
	str = xmlGetProp (node, BAD_CAST "source");
	if (str) 
		g_object_set_data_full (G_OBJECT (param), "source", (gchar*)str, g_free);

	/* set restricting source if specified */
	if (str && sources) {
		gchar *ptr1, *ptr2 = NULL, *tok;
		gchar *source;
			
		source = g_strdup ((gchar*)str);
		ptr1 = strtok_r (source, ":", &tok);
		if (ptr1)
			ptr2 = strtok_r (NULL, ":", &tok);
		
		if (ptr1 && ptr2) {
			GSList *tmp = sources;
			GdaDataModel *model = NULL;
			while (tmp && !model) {
				if (!strcmp (gda_object_get_name (GDA_OBJECT (tmp->data)), ptr1))
					model = GDA_DATA_MODEL (tmp->data);
				tmp = g_slist_next (tmp);
			}
			
			if (model) {
				gint fno;
				
				fno = atoi (ptr2);
				if ((fno < 0) ||
				    (fno >= gda_data_model_get_n_columns (model))) 
					g_warning (_("Field number %d not found in source named '%s'"), fno, ptr1); 
				else {
					if (gda_parameter_restrict_values (param, model, fno, NULL)) {
						/* rename the wrapper with the current param's name */
						g_object_set_data_full (G_OBJECT (model), "newname", 
									g_strdup ((gchar *)gda_object_get_name (GDA_OBJECT (param))),
									g_free);
						g_object_set_data_full (G_OBJECT (model), "newdescr", 
									g_strdup ((gchar *)gda_object_get_description (GDA_OBJECT (param))),
									g_free);
					}
				}
			}
		}
	}

	/* specified value */
	vnode = node->children;
	if (vnode) {
		xmlChar *this_lang, *isnull;
		const gchar *lang;
		GType gdatype;

		gdatype = gda_parameter_get_g_type (param);
#ifdef HAVE_LC_MESSAGES
		lang = setlocale (LC_MESSAGES, NULL);
#else
		lang = setlocale (LC_CTYPE, NULL);
#endif
		while (vnode) {
			if (xmlNodeIsText (vnode)) {
				vnode = vnode->next;
				continue;
			}

			if (strcmp ((gchar*)vnode->name, "gda_value")) {
				vnode = vnode->next;
				continue;
			}

			/* don't care about entries for the wrong locale */
			this_lang = xmlGetProp(vnode, (xmlChar*)"lang");
			if (this_lang && strncmp ((gchar*)this_lang, lang, strlen ((gchar*)this_lang))) {
				g_free (this_lang);
				vnode = vnode->next;
				continue;
			}
			
			isnull = xmlGetProp(vnode, (xmlChar*)"isnull");
			if (isnull) {
				if ((*isnull == 'f') || (*isnull == 'F')) {
					xmlFree (isnull);
					isnull = NULL;
				}
			}
			
			if (!isnull) {
				GValue *value;

				value = g_new0 (GValue, 1);
				if (! gda_value_set_from_string (value, (gchar*)xmlNodeGetContent (vnode), gdatype)) {
					/* error */
					g_free (value);
				}
				else {
					gda_parameter_set_value (param, value);
					gda_value_free (value);
				}
			}
			else {
				gda_parameter_set_value (param, NULL);
				xmlFree (isnull);
			}
			
			vnode = vnode->next;
		}
	}
}

/**
 * @dict:
 * @prov:
 * @cnc:
 * @dbms_type:
 * @g_type:
 * @created: a place to return if the data type has been created and is thus considered as a custom
 *           data type. Can't be %NULL!
 *
 * Finds or creates anew GdaDictType if possible. if @created is returned as TRUE, then the
 * caller of this function _does_ have a reference on the returned object.
 *
 * Returns: a #GdaDictType, or %NULL if it was not possible to find and create one
 */
GdaDictType *
gda_utility_find_or_create_data_type (GdaDict *dict, GdaServerProvider *prov, GdaConnection *cnc, 
				  const gchar *dbms_type, const gchar *g_type, gboolean *created)
{
	GdaDictType *dtype = NULL;

	g_return_val_if_fail (created, NULL);
	g_return_val_if_fail (!dict || GDA_IS_DICT (dict), NULL);
	g_return_val_if_fail (!prov || GDA_IS_SERVER_PROVIDER (prov), NULL);
	g_return_val_if_fail (!cnc || GDA_IS_CONNECTION (cnc), NULL);

	*created = FALSE;
	if (dbms_type)
		dtype = gda_dict_get_dict_type_by_name (ASSERT_DICT (dict), dbms_type);

	if (!dtype) {
		if (g_type) {
			GType gtype;
			
			gtype = gda_g_type_from_string (g_type);
			if (prov) {
				const gchar *deftype;
				
				deftype = gda_server_provider_get_default_dbms_type (prov, cnc, gtype);
				if (deftype)
					dtype = gda_dict_get_dict_type_by_name (ASSERT_DICT (dict), deftype);
			}	
			
			if (!dtype) {
				/* create a GdaDictType for that 'gda-type' */
				dtype = GDA_DICT_TYPE (gda_dict_type_new (ASSERT_DICT (dict)));
				gda_dict_type_set_sqlname (dtype, g_type);
				gda_dict_type_set_g_type (dtype, gtype);
				gda_dict_declare_object (ASSERT_DICT (dict), (GdaObject *) dtype);
				*created = TRUE;
			}
		}
	}

	return dtype;
}
