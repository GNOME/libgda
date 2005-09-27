/* GDA common library
 * Copyright (C) 1998-2002 The GNOME Foundation.
 *
 * AUTHORS:
 *	Rodrigo Moya <rodrigo@gnome-db.org>
 *	Juan-Mariano de Goyeneche <jmseyas@dit.upm.es> (BLOB issues)
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
#include <glib/gfileutils.h>
#include <glib/gmessages.h>
#include <glib/gstrfuncs.h>
#include <libsql/sql_parser.h>
#include <libgda/gda-intl.h>
#include <libgda/gda-log.h>
#include <libgda/gda-util.h>

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
 * gda_provider_get_schema_nb_columns
 * @schema:
 *
 * Returns: the number of columns the #GdaDataModel for the requested schema
 * must have
 */
gint
gda_provider_get_schema_nb_columns (GdaConnectionSchema schema)
{
	switch (schema) {
	case GDA_CONNECTION_SCHEMA_AGGREGATES:
		return 7;
	case GDA_CONNECTION_SCHEMA_DATABASES:
		return 1;
	case GDA_CONNECTION_SCHEMA_FIELDS:
		return 10;
        case GDA_CONNECTION_SCHEMA_INDEXES:
		return 1;
        case GDA_CONNECTION_SCHEMA_LANGUAGES:
		return 1;
        case GDA_CONNECTION_SCHEMA_NAMESPACES:
		return 1;
        case GDA_CONNECTION_SCHEMA_PARENT_TABLES:
		return 2;
        case GDA_CONNECTION_SCHEMA_PROCEDURES:
		return 8;
        case GDA_CONNECTION_SCHEMA_SEQUENCES:
		return 4;
        case GDA_CONNECTION_SCHEMA_TABLES:
		return 4;
        case GDA_CONNECTION_SCHEMA_TRIGGERS:
		return 1;
        case GDA_CONNECTION_SCHEMA_TYPES:
		return 5;
        case GDA_CONNECTION_SCHEMA_USERS:
		return 1;
        case GDA_CONNECTION_SCHEMA_VIEWS:
		return 4;
	default:
		g_assert_not_reached ();
	}
}

typedef struct {
        gchar        *col_name;
        GdaValueType  data_type;
} GdaSchemaColData;

GdaSchemaColData aggs_spec [] = {
	{ N_("Aggregate"), GDA_VALUE_TYPE_STRING},
	{ N_("Id"), GDA_VALUE_TYPE_STRING},
	{ N_("Owner"), GDA_VALUE_TYPE_STRING},
	{ N_("Comments"), GDA_VALUE_TYPE_STRING},
	{ N_("OutType"), GDA_VALUE_TYPE_STRING},
	{ N_("InType"), GDA_VALUE_TYPE_STRING},
	{ N_("Definition"), GDA_VALUE_TYPE_STRING}
};

GdaSchemaColData dbs_spec [] = {
	{ N_("Database"), GDA_VALUE_TYPE_STRING}
};

GdaSchemaColData fields_spec [] = {
	{ N_("Field name"), GDA_VALUE_TYPE_STRING},
	{ N_("Data type"), GDA_VALUE_TYPE_STRING},
	{ N_("Size"), GDA_VALUE_TYPE_INTEGER},
	{ N_("Scale"), GDA_VALUE_TYPE_INTEGER},
	{ N_("Not null?"), GDA_VALUE_TYPE_BOOLEAN},
	{ N_("Primary key?"), GDA_VALUE_TYPE_BOOLEAN},
	{ N_("Unique index?"), GDA_VALUE_TYPE_BOOLEAN},
	{ N_("References"), GDA_VALUE_TYPE_STRING},
	{ N_("Default value"), GDA_VALUE_TYPE_STRING},
	{ N_("Extra attributes"), GDA_VALUE_TYPE_STRING}
};

GdaSchemaColData indexes_spec [] = {
	{ N_("Indexe"), GDA_VALUE_TYPE_STRING}
};

GdaSchemaColData lang_spec [] = {
	{ N_("Language"), GDA_VALUE_TYPE_STRING}
};

GdaSchemaColData ns_spec [] = {
	{ N_("Namespace"), GDA_VALUE_TYPE_STRING}
};

GdaSchemaColData parent_spec [] = {
	{ N_("Table"), GDA_VALUE_TYPE_STRING},
	{ N_("Sequence"), GDA_VALUE_TYPE_INTEGER}
};

GdaSchemaColData procs_spec [] = {
	{ N_("Procedure"), GDA_VALUE_TYPE_STRING},
	{ N_("Id"), GDA_VALUE_TYPE_STRING},
	{ N_("Owner"), GDA_VALUE_TYPE_STRING},
	{ N_("Comments"), GDA_VALUE_TYPE_STRING},
	{ N_("Return type"), GDA_VALUE_TYPE_STRING},
	{ N_("Nb args"), GDA_VALUE_TYPE_INTEGER},
	{ N_("Args types"), GDA_VALUE_TYPE_STRING},
	{ N_("Definition"), GDA_VALUE_TYPE_STRING}
};

GdaSchemaColData seq_spec [] = {
	{ N_("Sequence"), GDA_VALUE_TYPE_STRING},
	{ N_("Owner"), GDA_VALUE_TYPE_STRING},
	{ N_("Comments"), GDA_VALUE_TYPE_STRING},
	{ N_("Definition"), GDA_VALUE_TYPE_STRING}
};

GdaSchemaColData table_spec [] = {
	{ N_("Table"), GDA_VALUE_TYPE_STRING},
	{ N_("Owner"), GDA_VALUE_TYPE_STRING},
	{ N_("Description"), GDA_VALUE_TYPE_STRING},
	{ N_("Definition"), GDA_VALUE_TYPE_STRING}
};

GdaSchemaColData trigger_spec [] = {
	{ N_("Trigger"), GDA_VALUE_TYPE_STRING}
};

GdaSchemaColData types_spec [] = {
	{ N_("Type"), GDA_VALUE_TYPE_STRING},
	{ N_("Owner"), GDA_VALUE_TYPE_STRING},
	{ N_("Comments"), GDA_VALUE_TYPE_STRING},
	{ N_("GDA type"), GDA_VALUE_TYPE_TYPE},
	{ N_("Synonyms"), GDA_VALUE_TYPE_STRING}
};

GdaSchemaColData user_spec [] = {
	{ N_("User"), GDA_VALUE_TYPE_STRING}
};

GdaSchemaColData view_spec [] = {
	{ N_("View"), GDA_VALUE_TYPE_STRING},
	{ N_("Owner"), GDA_VALUE_TYPE_STRING},
	{ N_("Description"), GDA_VALUE_TYPE_STRING},
	{ N_("Definition"), GDA_VALUE_TYPE_STRING}
};

/**
 * gda_provider_init_schema_model
 * @model:
 * @schema:
 *
 * Sets the column attributes of @model for the requested schema
 *
 * Returns: TRUE if there was no error
 */
gboolean
gda_provider_init_schema_model (GdaDataModel *model, GdaConnectionSchema schema)
{
	GdaSchemaColData *spec = NULL;
	gint nbcols, i;
	GdaColumn *column;

	g_return_val_if_fail (model && GDA_IS_DATA_MODEL (model), FALSE);

	switch (schema) {
	case GDA_CONNECTION_SCHEMA_AGGREGATES:
		spec = aggs_spec;
		break;
	case GDA_CONNECTION_SCHEMA_DATABASES:
		spec = dbs_spec;
		break;
	case GDA_CONNECTION_SCHEMA_FIELDS:
		spec = fields_spec;
 		break;
	case GDA_CONNECTION_SCHEMA_INDEXES:
		spec = indexes_spec;
 		break;
	case GDA_CONNECTION_SCHEMA_LANGUAGES:
		spec = lang_spec;
 		break;
	case GDA_CONNECTION_SCHEMA_NAMESPACES:
		spec = ns_spec;
 		break;
	case GDA_CONNECTION_SCHEMA_PARENT_TABLES:
		spec = parent_spec;
 		break;
	case GDA_CONNECTION_SCHEMA_PROCEDURES:
		spec = procs_spec;
 		break;
	case GDA_CONNECTION_SCHEMA_SEQUENCES:
		spec = seq_spec;
 		break;
	case GDA_CONNECTION_SCHEMA_TABLES:
		spec = table_spec;
 		break;
	case GDA_CONNECTION_SCHEMA_TRIGGERS:
		spec = trigger_spec;
  		break;
	case GDA_CONNECTION_SCHEMA_TYPES:
		spec = types_spec;
   		break;
	case GDA_CONNECTION_SCHEMA_USERS:
		spec = user_spec;
   		break;
	case GDA_CONNECTION_SCHEMA_VIEWS:
		spec = view_spec;
		break;
	default:
		g_assert_not_reached ();
	}

	nbcols = gda_provider_get_schema_nb_columns (schema);
	if (gda_data_model_get_n_columns (model) != nbcols)
		return FALSE;

	for (i = 0; i < nbcols; i++) {
                column = gda_data_model_describe_column (GDA_DATA_MODEL (model), i);

                gda_column_set_title (column, spec[i].col_name);
                gda_column_set_name (column, spec[i].col_name);
                gda_column_set_gdatype (column, spec[i].data_type);
        }

	return TRUE;
}
