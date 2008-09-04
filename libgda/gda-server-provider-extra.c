/* GDA common library
 * Copyright (C) 2005 - 2008 The GNOME Foundation.
 *
 * AUTHORS:
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
#include <libgda/gda-server-provider.h>
#include <libgda/gda-server-provider-extra.h>
#include <libgda/gda-server-provider-private.h>
#include <libgda/gda-data-handler.h>
#include <libgda/gda-util.h>
#include <glib/gi18n-lib.h>
#include <libgda/sql-parser/gda-sql-parser.h>

#include <libgda/handlers/gda-handler-numerical.h>
#include <libgda/handlers/gda-handler-boolean.h>
#include <libgda/handlers/gda-handler-time.h>
#include <libgda/handlers/gda-handler-string.h>
#include <libgda/handlers/gda-handler-type.h>

/**
 * gda_server_provider_internal_get_parser
 * @prov:
 *
 * This is a factory method to get a unique instance of a #GdaSqlParser object
 * for each #GdaServerProvider object
 * Don't unref() it.
 *
 * Returns: a #GdaSqlParser
 */
GdaSqlParser *
gda_server_provider_internal_get_parser (GdaServerProvider *prov)
{
	
	if (prov->priv->parser)
		return prov->priv->parser;
	prov->priv->parser = gda_server_provider_create_parser (prov, NULL);
	if (!prov->priv->parser)
		prov->priv->parser = gda_sql_parser_new ();
	return prov->priv->parser;
}

/**
 * gda_server_provider_perform_operation_default
 * @provider: a #GdaServerProvider object
 * @cnc: a #GdaConnection object which will be used to perform an action, or %NULL
 * @op: a #GdaServerOperation object
 * @error: a place to store an error, or %NULL
 *
 * Performs the operation described by @op, using the SQL from the rendering of the operation
 *
 * Returns: TRUE if no error occurred
 */
gboolean
gda_server_provider_perform_operation_default (GdaServerProvider *provider, GdaConnection *cnc,
					       GdaServerOperation *op, GError **error)
{
	gchar *sql;
	GdaBatch *batch;
	const GSList *list;
	gboolean retval = TRUE;

	sql = gda_server_provider_render_operation (provider, cnc, op, error);
	if (!sql)
		return FALSE;

	batch = gda_sql_parser_parse_string_as_batch (provider->priv->parser, sql, NULL, error);
	g_free (sql);
	if (!batch)
		return FALSE;

	for (list = gda_batch_get_statements (batch); list; list = list->next) {
		if (gda_connection_statement_execute_non_select (cnc, GDA_STATEMENT (list->data), NULL, NULL, error) == -1) {
			retval = FALSE;
			break;
		}
	}
	g_object_unref (batch);

	return retval;;
}

/**
 * gda_server_provider_get_data_handler_default
 * @provider: a server provider.
 * @cnc: a #GdaConnection object, or %NULL
 * @for_type: a #GType
 * @dbms_type: a DBMS type definition
 *
 * Provides the implementation when the default Libgda's data handlers must be used
 * 
 * Returns: a #GdaDataHandler, or %NULL
 */
GdaDataHandler *
gda_server_provider_get_data_handler_default (GdaServerProvider *provider, GdaConnection *cnc,
					      GType type, const gchar *dbms_type)
{
	GdaDataHandler *dh;
	if ((type == G_TYPE_INT64) ||
	    (type == G_TYPE_UINT64) ||
	    (type == G_TYPE_DOUBLE) ||
	    (type == G_TYPE_INT) ||
	    (type == GDA_TYPE_NUMERIC) ||
	    (type == G_TYPE_FLOAT) ||
	    (type == GDA_TYPE_SHORT) ||
	    (type == GDA_TYPE_USHORT) ||
	    (type == G_TYPE_CHAR) ||
	    (type == G_TYPE_UCHAR) ||
	    (type == G_TYPE_UINT)) {
		dh = gda_server_provider_handler_find (provider, NULL, type, NULL);
		if (!dh) {
			dh = gda_handler_numerical_new ();
			gda_server_provider_handler_declare (provider, dh, NULL, G_TYPE_INT64, NULL);
			gda_server_provider_handler_declare (provider, dh, NULL, G_TYPE_UINT64, NULL);
			gda_server_provider_handler_declare (provider, dh, NULL, G_TYPE_DOUBLE, NULL);
			gda_server_provider_handler_declare (provider, dh, NULL, G_TYPE_INT, NULL);
			gda_server_provider_handler_declare (provider, dh, NULL, GDA_TYPE_NUMERIC, NULL);
			gda_server_provider_handler_declare (provider, dh, NULL, G_TYPE_FLOAT, NULL);
			gda_server_provider_handler_declare (provider, dh, NULL, GDA_TYPE_SHORT, NULL);
			gda_server_provider_handler_declare (provider, dh, NULL, GDA_TYPE_USHORT, NULL);
			gda_server_provider_handler_declare (provider, dh, NULL, G_TYPE_CHAR, NULL);
			gda_server_provider_handler_declare (provider, dh, NULL, G_TYPE_UCHAR, NULL);
			gda_server_provider_handler_declare (provider, dh, NULL, G_TYPE_UINT, NULL);
			g_object_unref (dh);
		}
	}
        else if ((type == GDA_TYPE_BINARY) ||
		 (type == GDA_TYPE_BLOB)) {
		/* no default binary data handler, it's too database specific */
		dh = NULL;
	}
        else if (type == G_TYPE_BOOLEAN) {
		dh = gda_server_provider_handler_find (provider, NULL, type, NULL);
		if (!dh) {
			dh = gda_handler_boolean_new ();
			gda_server_provider_handler_declare (provider, dh, NULL, G_TYPE_BOOLEAN, NULL);
			g_object_unref (dh);
		}
	}
	else if ((type == GDA_TYPE_TIME) ||
		 (type == GDA_TYPE_TIMESTAMP) ||
		 (type == G_TYPE_DATE)) {
		/* no default time related data handler, it's too database specific */
		dh = NULL;
	}
	else if (type == G_TYPE_STRING) {
		dh = gda_server_provider_handler_find (provider, NULL, type, NULL);
		if (!dh) {
			dh = gda_handler_string_new ();
			gda_server_provider_handler_declare (provider, dh, NULL, G_TYPE_STRING, NULL);
			g_object_unref (dh);
		}
	}
	else if (type == G_TYPE_ULONG) {
		dh = gda_server_provider_handler_find (provider, NULL, type, NULL);
		if (!dh) {
			dh = gda_handler_type_new ();
			gda_server_provider_handler_declare (provider, dh, NULL, G_TYPE_ULONG, NULL);
			g_object_unref (dh);
		}
	}

	return dh;
}


/**
 * gda_server_provider_get_schema_nb_columns
 * @schema:
 *
 * Returns: the number of columns the #GdaDataModel for the requested schema
 * must have
 */
gint
gda_server_provider_get_schema_nb_columns (GdaConnectionSchema schema)
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
	case GDA_CONNECTION_SCHEMA_CONSTRAINTS:
		return 5;
	default:
		g_assert_not_reached ();
	}
}

typedef struct {
        gchar        *col_name;
        GType  data_type;
} GdaSchemaColData;

GdaSchemaColData aggs_spec [] = {
	/* To translators: "Aggregate": the noun */
	{ N_("Aggregate"), G_TYPE_STRING},
	{ N_("Id"), G_TYPE_STRING},
	{ N_("Owner"), G_TYPE_STRING},
	{ N_("Comments"), G_TYPE_STRING},
	{ N_("OutType"), G_TYPE_STRING},
	{ N_("InType"), G_TYPE_STRING},
	{ N_("Definition"), G_TYPE_STRING}
};

GdaSchemaColData dbs_spec [] = {
	{ N_("Database"), G_TYPE_STRING}
};

GdaSchemaColData fields_spec [] = {
	{ N_("Field name"), G_TYPE_STRING},
	{ N_("Data type"), G_TYPE_STRING},
	{ N_("Size"), G_TYPE_INT},
	{ N_("Scale"), G_TYPE_INT},
	{ N_("Not null?"), G_TYPE_BOOLEAN},
	{ N_("Primary key?"), G_TYPE_BOOLEAN},
	{ N_("Unique index?"), G_TYPE_BOOLEAN},
	{ N_("References"), G_TYPE_STRING},
	{ N_("Default value"), G_TYPE_STRING},
	{ N_("Extra attributes"), G_TYPE_STRING}
};

GdaSchemaColData indexes_spec [] = {
	{ N_("Index"), G_TYPE_STRING}
};

GdaSchemaColData lang_spec [] = {
	{ N_("Language"), G_TYPE_STRING}
};

GdaSchemaColData ns_spec [] = {
	{ N_("Namespace"), G_TYPE_STRING}
};

GdaSchemaColData parent_spec [] = {
	{ N_("Table"), G_TYPE_STRING},
	{ N_("Sequence"), G_TYPE_INT}
};

GdaSchemaColData procs_spec [] = {
	{ N_("Procedure"), G_TYPE_STRING},
	{ N_("Id"), G_TYPE_STRING},
	{ N_("Owner"), G_TYPE_STRING},
	{ N_("Comments"), G_TYPE_STRING},
	{ N_("Return type"), G_TYPE_STRING},
	/* To translators: "Nb args": the procedure's number of arguments */
	{ N_("Nb args"), G_TYPE_INT},
	{ N_("Args types"), G_TYPE_STRING},
	{ N_("Definition"), G_TYPE_STRING}
};

GdaSchemaColData seq_spec [] = {
	{ N_("Sequence"), G_TYPE_STRING},
	{ N_("Owner"), G_TYPE_STRING},
	{ N_("Comments"), G_TYPE_STRING},
	{ N_("Definition"), G_TYPE_STRING}
};

GdaSchemaColData table_spec [] = {
	{ N_("Table"), G_TYPE_STRING},
	{ N_("Owner"), G_TYPE_STRING},
	{ N_("Description"), G_TYPE_STRING},
	{ N_("Definition"), G_TYPE_STRING}
};

GdaSchemaColData trigger_spec [] = {
	{ N_("Trigger"), G_TYPE_STRING}
};

GdaSchemaColData types_spec [] = {
	{ N_("Type"), G_TYPE_STRING},
	{ N_("Owner"), G_TYPE_STRING},
	{ N_("Comments"), G_TYPE_STRING},
	{ N_("GDA type"), G_TYPE_ULONG},
	{ N_("Synonyms"), G_TYPE_STRING}
};

GdaSchemaColData user_spec [] = {
	{ N_("User"), G_TYPE_STRING}
};

GdaSchemaColData view_spec [] = {
	{ N_("View"), G_TYPE_STRING},
	{ N_("Owner"), G_TYPE_STRING},
	{ N_("Description"), G_TYPE_STRING},
	{ N_("Definition"), G_TYPE_STRING}
};

GdaSchemaColData constraint_spec [] = {
	{ N_("Name"), G_TYPE_STRING},
	{ N_("Type"), G_TYPE_STRING},
	{ N_("Fields"), G_TYPE_STRING},
	{ N_("Definition"), G_TYPE_STRING},
	{ N_("Options"), G_TYPE_STRING}
};

static GdaSchemaColData *
schema_get_spec (GdaConnectionSchema schema)
{
	GdaSchemaColData *spec = NULL;

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
	case GDA_CONNECTION_SCHEMA_CONSTRAINTS:
		spec = constraint_spec;
		break;
	default:
		g_assert_not_reached ();
	}

	return spec;
}

/**
 * gda_server_provider_init_schema_model
 * @model:
 * @schema:
 *
 * Sets the column attributes of @model for the requested schema
 *
 * Returns: TRUE if there was no error
 */
gboolean
gda_server_provider_init_schema_model (GdaDataModel *model, GdaConnectionSchema schema)
{
	GdaSchemaColData *spec = NULL;
	gint nbcols, i;
	GdaColumn *column;

	g_return_val_if_fail (model && GDA_IS_DATA_MODEL (model), FALSE);
	spec = schema_get_spec (schema);

	nbcols = gda_server_provider_get_schema_nb_columns (schema);
	if (gda_data_model_get_n_columns (model) != nbcols)
		return FALSE;

	for (i = 0; i < nbcols; i++) {
                column = gda_data_model_describe_column (GDA_DATA_MODEL (model), i);

                gda_column_set_title (column, spec[i].col_name);
                gda_column_set_name (column, spec[i].col_name);
                gda_column_set_g_type (column, spec[i].data_type);
        }

	return TRUE;
}

/**
 * gda_server_provider_test_schema_model
 * @model: a #GdaDataModel to test
 * @schema:
 * @error:
 *
 * Test that the structure of @model is correct in regard with @schema
 *
 * Returns: TRUE if @model has the correct structure
 */
gboolean
gda_server_provider_test_schema_model (GdaDataModel *model, GdaConnectionSchema schema, GError **error)
{
	gint i, nbcols;
	GdaSchemaColData *spec = NULL;

	g_return_val_if_fail (model && GDA_IS_DATA_MODEL (model), FALSE);

	nbcols = gda_data_model_get_n_columns (model);
	if (nbcols < gda_server_provider_get_schema_nb_columns (schema)) {
		g_set_error (error, 0, 0,
			     _("Data model for schema has a wrong number of columns"));
		return FALSE;
	}

	spec = schema_get_spec (schema);
	for (i = 0; i < nbcols; i++) {
		GdaColumn *column;

                column = gda_data_model_describe_column (GDA_DATA_MODEL (model), i);

		if (strcmp (gda_column_get_title (column), spec[i].col_name)) {
			g_set_error (error, 0, 0,
				     _("Data model for schema has a wrong column title: '%s' instead of '%s'"),
				     gda_column_get_title (column), spec[i].col_name);
			return FALSE;
		}

		if (strcmp (gda_column_get_name (column), spec[i].col_name)) {
			g_set_error (error, 0, 0,
				     _("Data model for schema has a wrong column name: '%s' instead of '%s'"),
				     gda_column_get_name (column), spec[i].col_name);
			return FALSE;
		}

		if (gda_column_get_g_type (column) != spec[i].data_type) {
			g_set_error (error, 0, 0,
				     _("Data model for schema has a wrong gda type: %s instead of %s"),
				     gda_g_type_to_string (gda_column_get_g_type (column)), 
				     gda_g_type_to_string (spec[i].data_type));
			return FALSE;
		}
        }
	return TRUE;
}

GdaDataHandler *
gda_server_provider_handler_find (GdaServerProvider *prov, GdaConnection *cnc, 
				  GType g_type, const gchar *dbms_type)
{
	GdaDataHandler *dh;
	GdaServerProviderHandlerInfo info;

	info.cnc = cnc;
	info.g_type = g_type;
	info.dbms_type = (gchar *) dbms_type;

	dh = g_hash_table_lookup (prov->priv->data_handlers, &info);
	return dh;
}

void
gda_server_provider_handler_declare (GdaServerProvider *prov, GdaDataHandler *dh,
				     GdaConnection *cnc, 
				     GType g_type, const gchar *dbms_type)
{
	GdaServerProviderHandlerInfo *info;
	g_return_if_fail (GDA_IS_DATA_HANDLER (dh));
	
	info = g_new (GdaServerProviderHandlerInfo, 1);
	info->cnc = cnc;
	info->g_type = g_type;
	info->dbms_type = dbms_type ? g_strdup (dbms_type) : NULL;
	
	g_hash_table_insert (prov->priv->data_handlers, info, dh);
	g_object_ref (dh);
}

/*
 * Tries to create a list of Key fields for table @table_name using @dict (and if @dict does not know about
 * that table, get the FIELDS schema from the provider)
 *
 * if @types_list is not %NULL, then it will contain a list of GType types of the same length as the returned list.
 */
static GSList *
get_key_field_names (GdaConnection *cnc, const gchar *table_name, GSList **types_list, GError **error)
{
	GSList *retfields = NULL;
	if (!table_name || !(*table_name))
		return NULL;

	TO_IMPLEMENT; /* using @cnc's GdaMetaStore */
	return retfields;
}

/*
 * Tries to create a list of blob fields for table @table_name using @dict (and if @dict does not know about
 * that table, get the FIELDS schema from the provider)
 */
static GSList *
get_blob_field_names (GdaConnection *cnc, const gchar *table_name, GError **error)
{
	GSList *retfields = NULL;
	if (!table_name || !(*table_name))
		return NULL;

	TO_IMPLEMENT; /* using @cnc's GdaMetaStore */
	return retfields;
}

/**
 * gda_server_provider_blob_list_for_update
 *
 * Create a SELECT query from an UPDATE query which lists all the BLOB fields in the
 * query which will be updated. This function is used by GdaServerProvider implementations
 * when dealing with BLOB updates.
 *
 * After execution, @out_select contains a new GdaQuery, or %NULL if the update query does not have any
 * BLOB to update.
 *
 * For example UPDATE blobs set name = ##/ *name:'name' type:gchararray* /, data = ##/ *name:'theblob' type:'GdaBlob'* / WHERE id= ##/ *name:'id' type:gint* /
 * will create:
 * SELECT t1.data FROM blobs AS t1 WHERE id= ##/ *name:'id' type:gint* /
 *
 * Returns: TRUE if no error occurred.
 */
gboolean
gda_server_provider_blob_list_for_update (GdaConnection *cnc, GdaStatement *query, 
					  GdaStatement **out_select, GError **error)
{
	gboolean retval = TRUE;
	
	g_return_val_if_fail (out_select, FALSE);
	g_return_val_if_fail (GDA_IS_STATEMENT (query), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);

	TO_IMPLEMENT; /* using @cnc's GdaMetaStore */
	return retval;
}

/**
 * gda_server_provider_blob_list_for_delete
 *
 * Create a SELECT query from a DELETE query which lists all the BLOB fields in the
 * query which will be deleted. This function is used by GdaServerProvider implementations
 * when dealing with BLOB deletions.
 *
 * After execution, @out_select contains a new GdaQuery, or %NULL if the delete query does not have any
 * BLOB to delete.
 *
 * For example DELETE FROM blobs WHERE id= ##/ *name:'id' type:gint* /
 * will create:
 * SELECT t1.data FROM blobs AS t1 WHERE id= ##/ *name:'id' type:gint* /
 *
 * Returns: TRUE if no error occurred.
 */
gboolean
gda_server_provider_blob_list_for_delete (GdaConnection *cnc, GdaStatement *query, 
					  GdaStatement **out_stmt, GError **error)
{
	gboolean retval = TRUE;

	g_return_val_if_fail (out_stmt, FALSE);
	g_return_val_if_fail (GDA_IS_STATEMENT (query), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);

	TO_IMPLEMENT; /* using @cnc's GdaMetaStore */
	return retval;
}

/**
 * gda_server_provider_split_update_query
 *
 * When an update query will affect N>1 rows, it can be refined into a new update query which can be executed N times wich
 * will affect one row at a time. This is usefull for providers implementations when dealing with BLOBs where updating
 * rows with a blob can be done only one row at a time.
 *
 * After execution, @out_select contains a new GdaQuery, or %NULL if it is not possible to create the update query.
 *
 * For example UPDATE blobs set name = ##/ *name:'name' type:gchararray* /, data = ##/ *name:'theblob' type:'GdaBlob'* / WHERE name= ##/ *name:'oname' type:gchararray* /
 * will create (if 'id' is a PK of the table to update):
 * UPDATE blobs set name = ##/ *name:'name' type:gchararray* /, data = ##/ *name:'theblob' type:'GdaBlob'* / WHERE id= ##/ *name:'oid' type:gint* /
 *
 * Returns: TRUE if no error occurred.
 */
gboolean
gda_server_provider_split_update_query (GdaConnection *cnc, GdaStatement *query, 
					GdaStatement **out_stmt, GError **error)
{
	gboolean retval = TRUE;

	g_return_val_if_fail (out_stmt, FALSE);
	g_return_val_if_fail (GDA_IS_STATEMENT (query), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);

	TO_IMPLEMENT; /* using @cnc's GdaMetaStore */
	return retval;
}

/**
 * gda_server_provider_select_query_has_blobs
 * 
 * Determines if @query (which must be a SELECT query) returns a data model with some BLOB data
 */
gboolean
gda_server_provider_select_query_has_blobs (GdaConnection *cnc, GdaStatement *stmt, GError **error)
{
	gboolean has_blobs = FALSE;

	g_return_val_if_fail (GDA_IS_STATEMENT (stmt), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);

	TO_IMPLEMENT; /* using @cnc's GdaMetaStore */
	return has_blobs;
}

/*
 * finds the location of a @filename
 */
gchar *
gda_server_provider_find_file (GdaServerProvider *prov, const gchar *inst_dir, const gchar *filename)
{
	gchar *file = NULL;
	const gchar *dirname;

	dirname = g_object_get_data (G_OBJECT (prov), "GDA_PROVIDER_DIR");
	if (dirname)
		file = g_build_filename (dirname, filename, NULL);

	if (!file ||
	    (file && !g_file_test (file, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_REGULAR))) {
		g_free (file);
		file = g_build_filename (inst_dir, filename, NULL);
		if (! g_file_test (file, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_REGULAR)) {
			g_free (file);
			file = NULL;
			if (dirname) {
				/* look in the parent dir, to handle the case where the lib is in a .libs dir */
				file = g_build_filename (dirname, "..", filename, NULL);
				if (! g_file_test (file, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_REGULAR)) {
					g_free (file);
					file = NULL;
				}
			}
		}
	}

	return file;
}

/*
 * Loads and returns the contents of @filename.
 * @filename is searched in several places
 */
gchar *
gda_server_provider_load_file_contents (const gchar *inst_dir, const gchar *data_dir, const gchar *filename)
{
	gchar *contents, *file;

	file = g_build_filename (inst_dir, filename, NULL);

	if (g_file_get_contents (file, &contents, NULL, NULL))
		goto theend;

	g_free (file);
	file = g_build_filename (inst_dir, "..", filename, NULL);
	if (g_file_get_contents (file, &contents, NULL, NULL))
		goto theend;

	g_free (file);
	file = g_build_filename (data_dir, filename, NULL);
	if (g_file_get_contents (file, &contents, NULL, NULL))
		goto theend;
	
	g_free (file);
	file = g_build_filename (inst_dir, "..", "..", "..", "share", "libgda-3.0", filename, NULL);
	if (g_file_get_contents (file, &contents, NULL, NULL))
		goto theend;
	contents = NULL;

 theend:
	g_free (file);
	return contents;
}
