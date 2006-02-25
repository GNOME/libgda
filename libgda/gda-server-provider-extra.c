/* GDA common library
 * Copyright (C) 2005 The GNOME Foundation.
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
	{ N_("Index"), GDA_VALUE_TYPE_STRING}
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

GdaSchemaColData constraint_spec [] = {
	{ N_("Name"), GDA_VALUE_TYPE_STRING},
	{ N_("Type"), GDA_VALUE_TYPE_STRING},
	{ N_("Fields"), GDA_VALUE_TYPE_STRING},
	{ N_("Definition"), GDA_VALUE_TYPE_STRING},
	{ N_("Options"), GDA_VALUE_TYPE_STRING}
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
                gda_column_set_gda_type (column, spec[i].data_type);
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

		if (gda_column_get_gda_type (column) != spec[i].data_type) {
			g_set_error (error, 0, 0,
				     _("Data model for schema has a wrong gda type: %s instead of %s"),
				     gda_type_to_string (gda_column_get_gda_type (column)), 
				     gda_type_to_string (spec[i].data_type));
			return FALSE;
		}
        }
	return TRUE;
}

guint
gda_server_provider_handler_info_hash_func  (GdaServerProviderHandlerInfo *key)
{
	guint hash;

	hash = g_int_hash (&(key->gda_type));
	if (key->dbms_type)
		hash += g_str_hash (key->dbms_type);
	hash += GPOINTER_TO_UINT (key->cnc);

	return hash;
}

gboolean
gda_server_provider_handler_info_equal_func (GdaServerProviderHandlerInfo *a, GdaServerProviderHandlerInfo *b)
{
	if ((a->gda_type == b->gda_type) &&
	    (a->cnc = b->cnc) &&
	    ((!a->dbms_type && !b->dbms_type) || !strcmp (a->dbms_type, b->dbms_type)))
		return TRUE;
	else
		return FALSE;
}

void
gda_server_provider_handler_info_free (GdaServerProviderHandlerInfo *info)
{
	g_free (info->dbms_type);
	g_free (info);
}

GdaDataHandler *
gda_server_provider_handler_find (GdaServerProvider *prov, GdaConnection *cnc, 
				  GdaValueType gda_type, const gchar *dbms_type)
{
	GdaDataHandler *dh;
	GdaServerProviderHandlerInfo info;

	info.cnc = cnc;
	info.gda_type = gda_type;
	info.dbms_type = (gchar *) dbms_type;

	dh = g_hash_table_lookup (prov->priv->data_handlers, &info);
	return dh;
}

void
gda_server_provider_handler_declare (GdaServerProvider *prov, GdaDataHandler *dh,
				     GdaConnection *cnc, 
				     GdaValueType gda_type, const gchar *dbms_type)
{
	GdaServerProviderHandlerInfo *info;
	g_return_if_fail (GDA_IS_DATA_HANDLER (dh));
	
	info = g_new (GdaServerProviderHandlerInfo, 1);
	info->cnc = cnc;
	info->gda_type = gda_type;
	info->dbms_type = dbms_type ? g_strdup (dbms_type) : NULL;
	
	g_hash_table_insert (prov->priv->data_handlers, info, dh);
	g_object_ref (dh);
}
