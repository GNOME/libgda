/*
 * Copyright (C) 2008 - 2013 Vivien Malerba <malerba@gnome-db.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */
#include "common.h"
#ifdef HAVE_JSON_GLIB
#include <json-glib/json-glib.h>
#endif

void
tests_common_display_value (const gchar *prefix, const GValue *value)
{
	gchar *str;

	str = gda_value_stringify ((GValue*) value);
	g_print ("*** %s: %s\n", prefix, str);
	g_free (str);
}

gchar *
tests_common_holder_serialize (GdaHolder *h)
{
	GString *string;
	gchar *str, *json;
	const GValue *value;

	string = g_string_new ("{");

	g_string_append (string, "\"type\":");
	g_string_append_printf (string, "\"%s\"", g_type_name (gda_holder_get_g_type (h)));

	g_string_append (string, ",\"id\":");
	json = _json_quote_string (gda_holder_get_id (h));
	g_string_append (string, json);
	g_free (json);

	g_object_get (G_OBJECT (h), "name", &str, NULL);
	if (str) {
		g_string_append (string, ",\"name\":");
		json = _json_quote_string (str);
		g_string_append (string, json);
		g_free (json);
		g_free (str);
	}

	g_object_get (G_OBJECT (h), "description", &str, NULL);
	if (str) {
		g_string_append (string, ",\"descr\":");
		json = _json_quote_string (str);
		g_string_append (string, json);
		g_free (json);
		g_free (str);
	}

	g_string_append (string, ",\"value\":");
	value = gda_holder_get_value (h);
	str = gda_value_stringify (value);
	json = _json_quote_string (str);
	g_free (str);
	g_string_append (string, json);
	g_free (json);

	g_string_append (string, ",\"default_value\":");
	value = gda_holder_get_default_value (h);
	str = gda_value_stringify (value);
	json = _json_quote_string (str);
	g_free (str);
	g_string_append (string, json);
	g_free (json);

	g_string_append (string, ",\"is_default\":");
	g_string_append_printf (string, gda_holder_value_is_default (h) ? "\"TRUE\"" : "\"FALSE\"");

	g_string_append (string, ",\"is_valid\":");
	g_string_append_printf (string, gda_holder_is_valid (h) ? "\"TRUE\"" : "\"FALSE\"");

	g_string_append (string, ",\"not_null\":");
	g_string_append_printf (string, gda_holder_get_not_null (h) ? "\"TRUE\"" : "\"FALSE\"");

	g_string_append_c (string, '}');
	str = g_string_free (string, FALSE);
	return str;
}

gchar *
tests_common_set_serialize (GdaSet *set)
{
	GString *string;
	gchar *str, *json;
	GSList *list;

	string = g_string_new ("{");

	/* holders */
	if (gda_set_get_holders (set)) {
		g_string_append (string, "\"holders\":[");
		for (list = gda_set_get_holders (set); list; list = list->next) {
			if (list != gda_set_get_holders (set))
				g_string_append_c (string, ',');
			str = tests_common_holder_serialize (GDA_HOLDER (list->data));
			g_string_append (string, str);
			g_free (str);
		}
		g_string_append_c (string, ']');
	}

	/* set description */
	g_object_get (G_OBJECT (set), "id", &str, NULL);
	if (str) {
		g_string_append (string, "\"id\":");
		json = _json_quote_string (str);
		g_string_append (string, json);
		g_free (json);
		g_free (str);
	}

	g_object_get (G_OBJECT (set), "name", &str, NULL);
	if (str) {
		g_string_append (string, ",\"name\":");
		json = _json_quote_string (str);
		g_string_append (string, json);
		g_free (json);
		g_free (str);
	}

	g_object_get (G_OBJECT (set), "description", &str, NULL);
	if (str) {
		g_string_append (string, ",\"descr\":");
		json = _json_quote_string (str);
		g_string_append (string, json);
		g_free (json);
		g_free (str);
	}

	/* public data */
	if (gda_set_get_nodes (set)) {
		g_string_append (string, ",\"nodes\":[");
		for (list = gda_set_get_nodes (set); list; list = list->next) {
			GdaSetNode *node = (GdaSetNode*) list->data;
			if (list != gda_set_get_nodes (set))
				g_string_append_c (string, ',');

			g_string_append_c (string, '{');
			g_string_append_printf (string, "\"holder\":%d", g_slist_index (gda_set_get_holders (set), gda_set_node_get_holder (node)));

			GdaDataModel *source_model;
			source_model = gda_set_node_get_data_model (node);
			if (source_model) {
				g_string_append (string, ",\"source_model\":");
				if (g_object_get_data (G_OBJECT (source_model), "name"))
					json = _json_quote_string (g_object_get_data (G_OBJECT (source_model), "name"));
				else {
					str = gda_data_model_export_to_string (source_model,
									       GDA_DATA_MODEL_IO_TEXT_SEPARATED,
									       NULL, 0, NULL, 0, NULL);
					json = _json_quote_string (str);
					g_free (str);
				}
				g_string_append (string, json);
				g_free (json);

				g_string_append (string, ",\"source_column\":");
				g_string_append_printf (string, "\"%d\"", gda_set_node_get_source_column (node));
				/* FIXME: node->hint */
			}
			g_string_append_c (string, '}');
		}
		g_string_append_c (string, ']');
	}

	if (gda_set_get_sources (set)) {
		g_string_append (string, ",\"sources\":[");
		for (list = gda_set_get_sources (set); list; list = list->next) {
			GdaSetSource *source = (GdaSetSource*) list->data;
			if (list != gda_set_get_sources (set))
				g_string_append_c (string, ',');
			g_string_append_c (string, '{');
			g_string_append (string, "\"model\":");
			GdaDataModel *data_model;
			data_model = gda_set_source_get_data_model (source);
			if (g_object_get_data (G_OBJECT (data_model), "name"))
				json = _json_quote_string (g_object_get_data (G_OBJECT (data_model), "name"));
			else {
				str = gda_data_model_export_to_string (data_model,
								       GDA_DATA_MODEL_IO_TEXT_SEPARATED,
								       NULL, 0, NULL, 0, NULL);
				json = _json_quote_string (str);
				g_free (str);
			}
			g_string_append (string, json);
			g_free (json);

			g_string_append (string, ",\"nodes\":[");
			GSList *nodes;
			for (nodes = gda_set_source_get_nodes (source); nodes; nodes = nodes->next) {
				if (nodes != gda_set_source_get_nodes (source))
					g_string_append_c (string, ',');
				g_string_append_printf (string, "%d", g_slist_index (gda_set_get_nodes (set), nodes->data));
			}
			g_string_append_c (string, ']');

			g_string_append_c (string, '}');
		}
		g_string_append_c (string, ']');
	}

	g_string_append_c (string, '}');
	str = g_string_free (string, FALSE);
	return str;
}


/*
 * Loads @filename where each line (terminated by a '\n') is in the form <id>|<text>, and lines starting
 * with a '#' will be ignored. The new hash table is indexed on the <id> part.
 */
GHashTable *
tests_common_load_data (const gchar *filename)
{
	GHashTable *table;
	gchar *contents;
	GError *error = NULL;
	gchar *ptr;
	gchar *line_start, *line_end;
	gboolean stop = FALSE;

	ptr = g_build_filename (ROOT_DIR, "tests", "value-holders", filename, NULL);
	if (!g_file_get_contents (ptr, &contents, NULL, &error)) 
		g_error ("Could not load file '%s': %s", ptr,
			 error->message);

	table = g_hash_table_new (g_str_hash, g_str_equal);

	line_start = contents;
	line_end = line_start;
	while (!stop) {
		for (line_end = line_start; *line_end && (*line_end != '\n'); line_end++);
		if (! *line_end) {
			stop = TRUE;
			if (line_end == line_start)
				break;
		}

		*line_end = 0;
		if (*line_start != '#') {
			for (ptr = line_start; *ptr && (*ptr != '|'); ptr++);
			*ptr = 0;
			if (ptr == line_end)
				g_warning ("Invalid line in data file: %s", line_start);
			else {
				g_hash_table_insert (table, line_start, ptr+1);
				/*g_print ("ADDED (%s): %s\n", line_start, ptr+1);*/
			}
		}

		if (!stop)
			line_start = line_end + 1;
	}
	
	return table;
}

gboolean
tests_common_check_holder (GHashTable *data, const gchar *id, GdaHolder *h, GError **error)
{
	gchar *s;
	const gchar *got;

	s = tests_common_holder_serialize (h);
	got = g_hash_table_lookup (data, id);
	if (!got) {
#ifdef HAVE_JSON_GLIB
		JsonParser *jparser;
		jparser = json_parser_new ();
		if (!json_parser_load_from_data (jparser, s, -1, NULL)) 
			g_set_error (error, TEST_ERROR, TEST_ERROR_GENERIC,
				     "Unknown ID '%s', GdaHolder is: %s (JSON INVALID)\n", id, s);
		else {
			JsonGenerator *jgen;
			gchar *out;
			jgen = json_generator_new ();
			g_object_set (G_OBJECT (jgen), "pretty", TRUE, "indent", 5, NULL);
			json_generator_set_root (jgen, json_parser_get_root (jparser));
			out = json_generator_to_data (jgen, NULL);
			g_set_error (error, TEST_ERROR, TEST_ERROR_GENERIC,
				     "Unknown ID '%s', GdaHolder is: %s\nJSON: %s\n", id, s, out);
			g_print ("%s\n", out);
			g_free (out);
			g_object_unref (jgen);
		}
		g_object_unref (jparser);
#else
		g_set_error (error, TEST_ERROR, TEST_ERROR_GENERIC,
			     "Unknown ID '%s', GdaHolder is: %s\n", id, s);
#endif

		g_free (s);
		g_object_unref (h);
		return FALSE;
	}

	if (strcmp (got, s)) {
		g_set_error (error, TEST_ERROR, TEST_ERROR_GENERIC,
			     "GdaHolder error:\nexp: %s\ngot: %s\n", got, s);
		g_free (s);
		g_object_unref (h);
		return FALSE;
	}
	g_free (s);
	return TRUE;
}

gboolean
tests_common_check_set (GHashTable *data, const gchar *id, GdaSet *set, GError **error)
{
	gchar *s;
	const gchar *got = NULL;

	s = tests_common_set_serialize (set);
	
	if (id)
		got = g_hash_table_lookup (data, id);
	if (!got) {
#ifdef HAVE_JSON_GLIB
		JsonParser *jparser;
		jparser = json_parser_new ();
		if (!json_parser_load_from_data (jparser, s, -1, NULL)) 
			g_set_error (error, TEST_ERROR, TEST_ERROR_GENERIC,
				     "Unknown ID '%s', GdaSet is: %s (JSON INVALID)\n", id, s);
		else {
			JsonGenerator *jgen;
			gchar *out;
			jgen = json_generator_new ();
			g_object_set (G_OBJECT (jgen), "pretty", TRUE, "indent", 5, NULL);
			json_generator_set_root (jgen, json_parser_get_root (jparser));
			out = json_generator_to_data (jgen, NULL);
			g_set_error (error, TEST_ERROR, TEST_ERROR_GENERIC,
				     "Unknown ID '%s', GdaSet is: %s\nJSON: %s\n", id, s, out);
			g_free (out);
			g_object_unref (jgen);
		}
		g_object_unref (jparser);
#else
		g_set_error (error, TEST_ERROR, TEST_ERROR_GENERIC,
			     "Unknown ID '%s', GdaSet is: %s\n", id, s);
#endif

		g_free (s);
		g_object_unref (set);
		return FALSE;
	}

	if (strcmp (got, s)) {
		g_set_error (error, TEST_ERROR, TEST_ERROR_GENERIC,
			     "GdaSet error:\nexp: %s\ngot: %s\n", got, s);
		g_free (s);
		g_object_unref (set);
		return FALSE;
	}
	g_free (s);
	return TRUE;
}
