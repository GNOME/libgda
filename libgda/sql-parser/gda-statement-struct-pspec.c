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
#include <libgda/sql-parser/gda-statement-struct-pspec.h>
#include <libgda/sql-parser/gda-statement-struct-parts.h>
#include <libgda/sql-parser/gda-statement-struct-util.h>
#include <libgda/gda-util.h>

/*
 *
 * Param specs
 *
 */
void
gda_sql_param_spec_take_name (GdaSqlParamSpec *pspec, GValue *value)
{
	if (pspec->name) {
		g_free (pspec->name);
		pspec->name = NULL;
	}
	if (value) {
		pspec->name = _remove_quotes (g_value_dup_string (value));
		g_value_unset (value);
		g_free (value);
	}
}

void
gda_sql_param_spec_take_descr (GdaSqlParamSpec *pspec, GValue *value)
{
	if (pspec->descr) {
		g_free (pspec->descr);
		pspec->descr = NULL;
	}
	if (value) {
		pspec->descr = _remove_quotes (g_value_dup_string (value));
		g_value_unset (value);
		g_free (value);
	}
}

void
gda_sql_param_spec_take_nullok (GdaSqlParamSpec *pspec, GValue *value)
{
	pspec->nullok = FALSE;
	if (value) {
		gchar *str = (gchar *) g_value_get_string (value);
		if (str) {
			_remove_quotes (str);
			if ((*str == 't') || (*str == 'T'))
				pspec->nullok = TRUE;
		}
		g_value_unset (value);
		g_free (value);
	}
}

void
gda_sql_param_spec_take_type (GdaSqlParamSpec *pspec, GValue *value)
{
	if (pspec->type) {
		g_free (pspec->type);
		pspec->type = NULL;
	}
	if (value) {
		pspec->type = _remove_quotes (g_value_dup_string (value));
		g_value_unset (value);
		g_free (value);

		pspec->g_type = gda_g_type_from_string (pspec->type);
	}
}

GdaSqlParamSpec *
gda_sql_param_spec_new (GValue *value)
{
	GdaSqlParamSpec *pspec;

	pspec =g_new0 (GdaSqlParamSpec, 1);
	pspec->is_param = TRUE;
	pspec->nullok = FALSE;
	pspec->type = NULL;

	if (value) {
		gchar *str = (gchar *) g_value_get_string (value);
		gchar *ptr;
		gint part; /* 0 for name, 1 for type and 2 for NULL */
		ptr = str;
		for (part = 0; *ptr && (part < 3); part++) {
			gchar old;
			for (; *ptr && ((*ptr != ':') || (*(ptr+1) != ':')); ptr++);
			old = *ptr;
			*ptr = 0;
			switch (part) {
			case 0:
				g_free (pspec->name);
				pspec->name = g_strdup (str);
				break;
			case 1:
				g_free (pspec->type);
				pspec->type = g_strdup (str);
				break;
			case 2:
				pspec->nullok = (*str == 'n') || (*str == 'N') ? TRUE : FALSE;
				break;
			}
			*ptr = old;
			if (*ptr) {
				ptr +=2;
				str = ptr;
			}
		}
		g_value_unset (value);
		g_free (value);
	}

	return pspec;
}

GdaSqlParamSpec *
gda_sql_param_spec_copy (GdaSqlParamSpec *pspec)
{
	GdaSqlParamSpec *copy;
	if (!pspec) return NULL;

	copy = g_new0 (GdaSqlParamSpec, 1);
	if (pspec->name)
		copy->name = g_strdup (pspec->name);
	if (pspec->descr)
		copy->descr = g_strdup (pspec->descr);
	if (pspec->type)
		copy->type = g_strdup (pspec->type);
	copy->is_param = pspec->is_param;
	copy->nullok = pspec->nullok;

	return copy;
}

void
gda_sql_param_spec_free (GdaSqlParamSpec *pspec)
{
	if (!pspec) return;

	g_free (pspec->name);
	g_free (pspec->descr);
	g_free (pspec->type);
	g_free (pspec);
}

gchar *
gda_sql_param_spec_serialize (GdaSqlParamSpec *pspec)
{
	GString *string;
	gchar *str;

	if (!pspec)
		return NULL;

	string = g_string_new ("{");
	str = _json_quote_string (pspec->name);
	g_string_append_printf (string, "\"name\":%s", str);
	g_free (str);


	str = _json_quote_string (pspec->descr);
	g_string_append_printf (string, ",\"descr\":%s", str);
	g_free (str);

	str = _json_quote_string (pspec->type);
	g_string_append_printf (string, ",\"type\":%s", str);
	g_free (str);

	g_string_append_printf (string, ",\"is_param\":%s", pspec->is_param ? "true" : "false");
	g_string_append_printf (string, ",\"nullok\":%s", pspec->nullok ? "true" : "false");
	g_string_append_c (string, '}');
	str = string->str;
	g_string_free (string, FALSE);
	return str;
}

