/*
 * Copyright (C) 2008 - 2011 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2009 Bas Driessen <bas.driessen@xobas.com>
 * Copyright (C) 2010 Murray Cumming <murrayc@murrayc.com>
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
#include <libgda/sql-parser/gda-statement-struct-pspec.h>
#include <libgda/sql-parser/gda-statement-struct-parts.h>
#include <libgda/sql-parser/gda-statement-struct-util.h>
#include <libgda/gda-util.h>

G_DEFINE_BOXED_TYPE(GdaSqlParamSpec, gda_sql_param_spec, gda_sql_param_spec_copy, gda_sql_param_spec_free)

/**
 * gda_sql_param_spec_take_name:
 * @pspec: a #GdaSqlParamSpec pointer
 * @value: (transfer full): a G_TYPE_STRING #GValue
 *
 * Sets @pspec's name. @value's ownership is transferred to
 * @pspec (which means @pspec is then responsible for freeing it when no longer needed).
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
		gda_value_free (value);
	}
}

/**
 * gda_sql_param_spec_take_descr:
 * @pspec: a #GdaSqlParamSpec pointer
 * @value: (transfer full): a G_TYPE_STRING #GValue
 *
 * Sets @pspec's description. @value's ownership is transferred to
 * @pspec (which means @pspec is then responsible for freeing it when no longer needed).
 */
void
gda_sql_param_spec_take_descr (GdaSqlParamSpec *pspec, GValue *value)
{
	if (pspec->descr) {
		g_free (pspec->descr);
		pspec->descr = NULL;
	}
	if (value) {
		pspec->descr = _remove_quotes (g_value_dup_string (value));
		gda_value_free (value);
	}
}

/**
 * gda_sql_param_spec_take_nullok:
 * @pspec: a #GdaSqlParamSpec pointer
 * @value: (transfer full): a G_TYPE_STRING #GValue. 
 *
 * Sets @pspec's ability of being NULL. @value's ownership is transferred to
 * @pspec (which means @pspec is then responsible for freeing it when no longer needed).
 *
 * If @value's string starts by 't' or 'T' then @pspec will be allowed to be %NULL
 */
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
		gda_value_free (value);
	}
}

/**
 * gda_sql_param_spec_take_type:
 * @pspec: a #GdaSqlParamSpec pointer
 * @value: (transfer full): a G_TYPE_STRING #GValue
 *
 * Sets @pspec's data type. @value's ownership is transferred to
 * @pspec (which means @pspec is then responsible for freeing it when no longer needed).
 *
 * @value must represent a data type, as understood by gda_g_type_from_string().
 */
void
gda_sql_param_spec_take_type (GdaSqlParamSpec *pspec, GValue *value)
{
	pspec->g_type = GDA_TYPE_NULL;
	if (value) {
		gchar *tmp;
		tmp = _remove_quotes (g_value_dup_string (value));
		gda_value_free (value);

		pspec->g_type = gda_g_type_from_string (tmp);
		g_free (tmp);
		if (pspec->g_type == G_TYPE_INVALID)
			pspec->g_type = GDA_TYPE_NULL;
	}
}

/**
 * gda_sql_param_spec_new:
 * @simple_spec: (transfer full): a G_TYPE_STRING #GValue
 *
 * @value must contain a string representing a variable, see the documentation associated to the
 * #GdaSqlParser object.
 *
 * @value is destroyed by this function.
 *
 * Returns: (transfer full): a new #GdaSqlParamSpec
 */
GdaSqlParamSpec *
gda_sql_param_spec_new (GValue *simple_spec)
{
	GdaSqlParamSpec *pspec;

	pspec =g_new0 (GdaSqlParamSpec, 1);
	pspec->is_param = TRUE;
	pspec->nullok = FALSE;
	pspec->g_type = GDA_TYPE_NULL;

	if (simple_spec) {
		gchar *str = (gchar *) g_value_get_string (simple_spec);
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
				pspec->g_type = gda_g_type_from_string (str);
				if (pspec->g_type == G_TYPE_INVALID)
					pspec->g_type = GDA_TYPE_NULL;
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
		gda_value_free (simple_spec);
	}

	return pspec;
}

/**
 * gda_sql_param_spec_copy:
 * @pspec: #GdaSqlParamSpec pointer
 *
 * Creates a copy of @pspec.
 *
 * Returns: a new #GdaSqlParamSpec
 */
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
	copy->g_type = pspec->g_type;
	copy->is_param = pspec->is_param;
	copy->nullok = pspec->nullok;

	return copy;
}

/**
 * gda_sql_param_spec_free:
 * @pspec: #GdaSqlParamSpec pointer
 *
 * Destroys @pspec.
 */
void
gda_sql_param_spec_free (GdaSqlParamSpec *pspec)
{
	if (!pspec) return;

	g_free (pspec->name);
	g_free (pspec->descr);
	g_free (pspec);
}

/**
 * gda_sql_param_spec_serialize:
 * @pspec: a #GdaSqlParamSpec pointer
 *
 * Creates a new string representing @pspec.
 *
 * Returns: a new string.
 */
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

	if (pspec->g_type != GDA_TYPE_NULL) {
		str = _json_quote_string (gda_g_type_to_string (pspec->g_type));
		g_string_append_printf (string, ",\"type\":%s", str);
		g_free (str);
	}
	else
		g_string_append_printf (string, ",\"type\":null");

	g_string_append_printf (string, ",\"is_param\":%s", pspec->is_param ? "true" : "false");
	g_string_append_printf (string, ",\"nullok\":%s", pspec->nullok ? "true" : "false");
	g_string_append_c (string, '}');
	str = string->str;
	g_string_free (string, FALSE);
	return str;
}

