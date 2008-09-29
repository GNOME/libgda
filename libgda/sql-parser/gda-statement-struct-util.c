/* 
 * Copyright (C) 2007 Vivien Malerba
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
#include <libgda/sql-parser/gda-statement-struct.h>

/*
 *
 * Utility functions
 *
 */
gchar *
gda_sql_value_stringify (const GValue *value)
{
        if (value && G_IS_VALUE (value)) {
                if (g_value_type_transformable (G_VALUE_TYPE (value), G_TYPE_STRING)) {
                        GValue *string;
                        gchar *str;

                        string = g_value_init (g_new0 (GValue, 1), G_TYPE_STRING);
                        g_value_transform (value, string);
                        str = g_value_dup_string (string);
                        g_value_unset (string);
			g_free (string);
                        return str;
                }
                else {
                        GType type = G_VALUE_TYPE (value);
                        if (type == G_TYPE_DATE) {
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
			else
                                return g_strdup ("<type not transformable to string>");
		}
	}
	else
		return g_strdup ("NULL");
}

/* Returns: @str */
gchar *
_remove_quotes (gchar *str)
{
        glong total;
        gchar *ptr;
        glong offset = 0;
	char delim;
	
	if (!str)
		return NULL;
	delim = *str;
	if ((delim != '\'') && (delim != '"'))
		return str;


        total = strlen (str);
        if (str[total-1] == delim) {
		/* string is correclty terminated by a double quote */
		g_memmove (str, str+1, total-2);
		total -=2;
	}
	else {
		/* string is _not_ correclty terminated by a double quote */
		g_memmove (str, str+1, total-1);
		total -=1;
	}
        str[total] = 0;

        ptr = (gchar *) str;
        while (offset < total) {
                /* we accept the "''" as a synonym of "\'" */
                if (*ptr == delim) {
                        if (*(ptr+1) == delim) {
                                g_memmove (ptr+1, ptr+2, total - offset);
                                offset += 2;
                        }
                        else {
                                *str = 0;
                                return str;
                        }
                }
                if (*ptr == '\\') {
                        if (*(ptr+1) == '\\') {
                                g_memmove (ptr+1, ptr+2, total - offset);
                                offset += 2;
                        }
                        else {
                                if (*(ptr+1) == delim) {
                                        *ptr = delim;
                                        g_memmove (ptr+1, ptr+2, total - offset);
                                        offset += 2;
                                }
                                else {
                                        *str = 0;
                                        return str;
                                }
                        }
                }
                else
                        offset ++;

                ptr++;
        }

        return str;
}

/**
 * gda_sql_identifier_add_quotes
 * @str: an SQL identifier
 *
 * Add double quotes around the @str identifier. Use the gda_sql_identifier_needs_quotes()
 * function to tell if an identifier needs to be quoted.
 *
 * Returns: a new string
 */
gchar *
gda_sql_identifier_add_quotes (const gchar *str)
{
	gchar *retval, *rptr;
	const gchar *sptr;
	gint len;

	if (!str)
		return NULL;

	len = strlen (str);
	retval = g_new (gchar, 2*len + 3);
	*retval = '"';
	for (rptr = retval+1, sptr = str; *sptr; sptr++, rptr++) {
		if (*sptr == '"') {
			*rptr = '\\';
			rptr++;
			*rptr = *sptr;
		}
		else
			*rptr = *sptr;
	}
	*rptr = '"';
	rptr++;
	*rptr = 0;
	return retval;
}

gchar *
_json_quote_string (const gchar *str)
{
	gchar *retval, *rptr;
	const gchar *sptr;
	gint len;

	if (!str)
		return g_strdup ("null");

	len = strlen (str);
	retval = g_new (gchar, 2*len + 3);
	*retval = '"';
	for (rptr = retval+1, sptr = str; *sptr; sptr++, rptr++) {
		switch (*sptr) {
		case '"':
			*rptr = '\\';
			rptr++;
			*rptr = *sptr;
			break;
		case '\\':
			*rptr = '\\';
			rptr++;
			*rptr = *sptr;
			break;
		case '/':
			*rptr = '\\';
			rptr++;
			*rptr = *sptr;
			break;
		case '\b':
			*rptr = '\\';
			rptr++;
			*rptr = 'b';
			break;
		case '\f':
			*rptr = '\\';
			rptr++;
			*rptr = 'f';
			break;
		case '\n':
			*rptr = '\\';
			rptr++;
			*rptr = 'n';
			break;
		case '\r':
			*rptr = '\\';
			rptr++;
			*rptr = 'r';
			break;
		case '\t':
			*rptr = '\\';
			rptr++;
			*rptr = 't';
			break;
		default:
			*rptr = *sptr;
			break;
		}
	}
	*rptr = '"';
	rptr++;
	*rptr = 0;
	return retval;
}

/*
** If X is a character that can be used in an identifier then
** IdChar(X) will be true.  Otherwise it is false.
**  
** For ASCII, any character with the high-order bit set is
** allowed in an identifier.  For 7-bit characters, 
** sqlite3IsIdChar[X] must be 1. 
*/
static const char AsciiIdChar[] = {
     /* x0 x1 x2 x3 x4 x5 x6 x7 x8 x9 xA xB xC xD xE xF */
	0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 2x */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0,  /* 3x */
	0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /* 4x */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1,  /* 5x */
	0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /* 6x */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0,  /* 7x */
};
#define IdChar(C) (((c=C)&0x80)!=0 || (c>0x1f && AsciiIdChar[c-0x20]))
gboolean
_string_is_identifier (const gchar *str)
{
	const gchar *ptr;
	gchar *endptr;
	gdouble d;
	gchar c;

	if (!str || !(*str)) 
		return FALSE;
	for (ptr = *str == '"' ? str + 1 : str; 
	     IdChar(*ptr) || (*ptr == '*') || (*ptr == '.') || ((*ptr == '"') && ptr[1] == 0); 
	     ptr++);
	if (*ptr) 
		return FALSE;
	if ((*str == '"') && (ptr[-1] == '"'))
		return TRUE;

	/* @str is composed only of character that can be used in an identifier */
	d = g_ascii_strtod (str, &endptr);
	if (!*endptr)
		/* @str is a number */
		return FALSE;
	return TRUE;
}

/**
 * gda_sql_identifier_needs_quotes
 * @str: an SQL identifier
 *
 * Tells if @str needs to be quoted before using it in an SQL statement. To actually add quotes,
 * use gda_sql_identifier_add_quotes().
 *
 * Returns: TRUE if @str needs some quotes
 */
gboolean
gda_sql_identifier_needs_quotes (const gchar *str)
{
	const gchar *ptr;

	g_return_val_if_fail (str, FALSE);
	for (ptr = str; *ptr; ptr++) {
		if (*ptr != g_ascii_tolower (*ptr))
			return TRUE;
	}
	return FALSE;
}

/**
 * gda_sql_identifier_remove_quotes
 * @str: a quoted string
 *
 * Prepares @str to be compared:
 * - if surrounded by double quotes, then just remove the quotes
 * - otherwise convert to lower case
 *
 * WARNING: @str must NOT be a composed identifier (<part1>."<part2>" for example)
 * 
 * Returns: @str
 */
gchar *
gda_sql_identifier_remove_quotes (gchar *str)
{
	if (!str)
		return NULL;
	if (*str == '"')
		return _remove_quotes (str);
	else {
		gchar *ptr;
		for (ptr = str; *ptr; ptr++)
			*ptr = g_ascii_tolower (*ptr);
		return str;
	}
}

/*
 * Reuses @str and fills in @remain and @last
 *
 * @str "swallowed" by the function and must be allocated memory, and @remain and @last are
 * also allocated memory.
 *
 * Accepted notations for each individual part:
 *   - aaa
 *   - "aaa"
 *
 * So "aaa".bbb, aaa.bbb, "aaa"."bbb", aaa."bbb" are Ok.
 *
 * Returns TRUE:
 * if @str has the <part1>.<part2> form, then @last will contain <part2> and @remain will contain <part1>
 * if @str has the <part1> form, then @last will contain <part1> and @remain will contain NULL
 * 
 * Returns FALSE (in any case @str is also freed)
 * if @str is NULL:
 * if @str is "":
 * if @str is malformed:
 *              @last and @remain will contain NULL
 */
gboolean
_split_identifier_string (gchar *str, gchar **remain, gchar **last)
{
	gchar *ptr;
	gboolean inq = FALSE;
	gint len;

	*remain = NULL;
	*last = NULL;
	if (!str)
		return FALSE;
	g_strchomp (str);
	if (!*str) {
		g_free (str);
		return FALSE;
	}

	len = strlen (str) - 1;
	if (len > 1) {
		if (((str[len] == '"') && (str[len-1] == '.')) ||
		    (str[len] == '.')) {
			g_free (str);
			return FALSE;
		}
	}

	if (((str[0] == '"') && (str[1] == '.')) ||
	    (str[0] == '.')) {
		g_free (str);
		return FALSE;
	}

	for (ptr = str + strlen (str) - 1; ptr >= str; ptr--) {
		if (*ptr == '"') 
			inq = !inq;
		else if ((*ptr == '.') && !inq) {
			*ptr = 0;
			*remain = str;
			*last = g_strdup (ptr+1);
			break;
		}
	}
	if (!(*last) && !(*remain))
		*last = str;

	if (*last && !_string_is_identifier (*last)) {
		g_free (*last);
		*last = NULL;
		g_free (*remain);
		*remain = NULL;
		return FALSE;
	}
	return TRUE;
}
