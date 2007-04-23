/* GDA common library
 * Copyright (C) 2005 - 2007 The GNOME Foundation.
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

#include <libgda/gda-renderer.h>
#include <libgda/gda-entity.h>
#include <libgda/gda-query.h>
#include <libgda/gda-query-target.h>
#include <libgda/gda-query-condition.h>
#include <libgda/gda-query-field-field.h>
#include <libgda/gda-query-field-value.h>


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

guint
gda_server_provider_handler_info_hash_func  (GdaServerProviderHandlerInfo *key)
{
	guint hash;

	hash = g_int_hash (&(key->g_type));
	if (key->dbms_type)
		hash += g_str_hash (key->dbms_type);
	hash += GPOINTER_TO_UINT (key->cnc);

	return hash;
}

gboolean
gda_server_provider_handler_info_equal_func (GdaServerProviderHandlerInfo *a, GdaServerProviderHandlerInfo *b)
{
	if ((a->g_type == b->g_type) &&
	    (a->cnc == b->cnc) &&
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

static gboolean
copy_condition (GdaQuery *source, GdaQuery *dest, GError **error)
{
	GdaQueryCondition *cond, *ncond;
	gchar *sql;

	cond = gda_query_get_condition (source);
	if (!cond)
		return TRUE;

	sql = gda_renderer_render_as_sql (GDA_RENDERER (cond), NULL, NULL, GDA_RENDERER_PARAMS_AS_DETAILED, error);
	if (!sql) 
		return FALSE;

	ncond = gda_query_condition_new_from_sql (dest, sql, NULL, error);
	g_free (sql);
	if (!ncond)
		return FALSE;
	
	gda_query_set_condition (dest, ncond);
	g_object_unref (ncond);

	return TRUE;
}

/*
 * Find the GType of a DBMS type using the TYPES schema array
 */
static GType
find_gtype (GdaDataModel *types, const gchar *name)
{
	GType gtype = G_TYPE_INVALID;
	gint row, nrows;
	nrows = gda_data_model_get_n_rows (types);
	for (row = 0; (row < nrows) && (gtype == G_TYPE_INVALID); row++) {
		const GValue *value;
		value = gda_data_model_get_value_at (types, 0, row);
		if (value && gda_value_isa (value, G_TYPE_STRING)) {
			const gchar *t = g_value_get_string ((GValue*) value);
			if (!strcmp (t, name)) {
				value = gda_data_model_get_value_at (types, 3, row);
				if (value && gda_value_isa (value, G_TYPE_ULONG))
					gtype = g_value_get_ulong (value);
			}
		}
	}
	
	return gtype;
}

/*
 * Tries to create a list of Key fields for table @table_name using @dict (and if @dict does not know about
 * that table, get the FIELDS schema from the provider)
 *
 * if @types_list is not %NULL, then it will contain a list of GType types of the same length as the returned list.
 */
static GSList *
get_key_field_names (GdaConnection *cnc, GdaDict *dict, const gchar *table_name, GSList **types_list, GError **error)
{
	GSList *retfields = NULL;
	GSList *rettypes = NULL;
	gboolean allok = TRUE;
	if (!table_name || !(*table_name))
		return NULL;

	/* try to use the dictionary */
	if (dict) {
		GdaDictDatabase *db = gda_dict_get_database (dict);
		if (db) {
			GdaDictTable *table = gda_dict_database_get_table_by_name (db, table_name);
			if (table) {
				GdaDictConstraint *pk;
				GSList *fields = NULL, *list;

				pk = gda_dict_table_get_pk_constraint (table);
				if (pk)
					fields = gda_dict_constraint_pkey_get_fields (pk);
				else
					fields = gda_entity_get_fields (GDA_ENTITY (table));

				for (list = fields; list; list = list->next) {
					GdaEntityField *field = GDA_ENTITY_FIELD (list->data);
					if (gda_entity_field_get_g_type (field) == GDA_TYPE_BLOB) {
						g_set_error (error, 0, 0,
							     _("Can't handle Primary Key fields which are BLOBs"));
						allok = FALSE;
					}
					else {
						GType *type;
						retfields = g_slist_append (retfields, 
									    g_strdup (gda_entity_field_get_name  (field)));
						type = g_new (GType, 1);
						*type = gda_entity_field_get_g_type (field);
						rettypes = g_slist_append (rettypes, type);
					}
				}
				g_slist_free (fields);
			}
		}
	}

	/* try to use the FIELDS schema */
	if (!retfields && allok) {
		GdaDataModel *model, *types = NULL;
		GdaParameterList *plist;

		plist = gda_parameter_list_new_inline (dict, "name", G_TYPE_STRING, table_name, NULL);
		model = gda_server_provider_get_schema (gda_connection_get_provider_obj (cnc), cnc,
							GDA_CONNECTION_SCHEMA_FIELDS,
							plist, error);
		g_object_unref (plist);
		if (!model)
			allok = FALSE;
		else {
			types = gda_server_provider_get_schema (gda_connection_get_provider_obj (cnc), cnc,
								GDA_CONNECTION_SCHEMA_TYPES, NULL, error);
			if (!types)
				allok = FALSE;
			else {
				gint row, nrows;
				nrows = gda_data_model_get_n_rows (model);
				
				/* try to get PK fields */
				for (row = 0; (row < nrows) && allok; row++) {
					const GValue *value, *tvalue;
					GType gtype;
					
					value = gda_data_model_get_value_at (model, 0, row);
					tvalue = gda_data_model_get_value_at (model, 1, row);
					if (!value || !gda_value_isa (value, G_TYPE_STRING) ||
					    !tvalue || !gda_value_isa (tvalue, G_TYPE_STRING))
						allok = FALSE;
					else {
						const gchar *fname = g_value_get_string (value);
						const gchar *tname = g_value_get_string (tvalue);
						
						value = gda_data_model_get_value_at (model, 5, row);
						if (!value || !gda_value_isa (value, G_TYPE_BOOLEAN))
							allok = FALSE;
						else {
							if (g_value_get_boolean (value)) {
								gtype = find_gtype (types, tname);
								if (gtype == GDA_TYPE_BLOB) {
									g_set_error (error, 0, 0,
										     _("Can't handle Primary Key fields which are BLOBs"));
									allok = FALSE;
								}
								else {
									GType *type;
									retfields = g_slist_append (retfields, g_strdup (fname));
									type = g_new (GType, 1);
									*type = gtype;
									rettypes = g_slist_append (rettypes, type);
								}
							}
						}
					}
				}

				if (allok && !retfields) {
					/* now use all fields */
					for (row = 0; (row < nrows) && allok; row++) {
						const GValue *value, *tvalue;
						GType gtype;
						
						value = gda_data_model_get_value_at (model, 0, row);
						tvalue = gda_data_model_get_value_at (model, 1, row);
						if (!value || !gda_value_isa (value, G_TYPE_STRING) ||
						    !tvalue || !gda_value_isa (tvalue, G_TYPE_STRING))
							allok = FALSE;
						else {
							const gchar *fname = g_value_get_string (value);
							const gchar *tname = g_value_get_string (tvalue);

							gtype = find_gtype (types, tname);
							if (gtype != GDA_TYPE_BLOB) {
								GType *type;
								retfields = g_slist_append (retfields, g_strdup (fname));
								type = g_new (GType, 1);
								*type = gtype;
								rettypes = g_slist_append (rettypes, type);
							}
						}
					}
				}
			}

			g_object_unref (model);
			if (types)
				g_object_unref (types);
		}
	}

	if (!allok) {
		g_slist_foreach (retfields, (GFunc) g_free, NULL);
		g_slist_free (retfields);
		g_slist_foreach (rettypes, (GFunc) g_free, NULL);
		g_slist_free (rettypes);
		return NULL;
	}

	if (types_list)
		*types_list = rettypes;
	else {
		g_slist_foreach (rettypes, (GFunc) g_free, NULL);
		g_slist_free (rettypes);
	}
	return retfields;
}

/*
 * Tries to create a list of blob fields for table @table_name using @dict (and if @dict does not know about
 * that table, get the FIELDS schema from the provider)
 */
static GSList *
get_blob_field_names (GdaConnection *cnc, GdaDict *dict, const gchar *table_name, GError **error)
{
	GSList *retfields = NULL;
	gboolean allok = TRUE;
	if (!table_name || !(*table_name))
		return NULL;

	/* try to use the dictionary */
	if (dict) {
		GdaDictDatabase *db = gda_dict_get_database (dict);
		if (db) {
			GdaDictTable *table = gda_dict_database_get_table_by_name (db, table_name);
			if (table) {
				GSList *fields = NULL, *list;

				fields = gda_entity_get_fields (GDA_ENTITY (table));

				for (list = fields; list; list = list->next) {
					GdaEntityField *field = GDA_ENTITY_FIELD (list->data);
					if (gda_entity_field_get_g_type (field) == GDA_TYPE_BLOB) {
						retfields = g_slist_append (retfields, 
									    g_strdup (gda_entity_field_get_name  (field)));
					}
				}
				g_slist_free (fields);
			}
		}
	}

	/* try to use the FIELDS schema */
	if (!retfields && allok) {
		GdaDataModel *model, *types = NULL;
		GdaParameterList *plist;

		plist = gda_parameter_list_new_inline (dict, "name", G_TYPE_STRING, table_name, NULL);
		model = gda_server_provider_get_schema (gda_connection_get_provider_obj (cnc), cnc,
							GDA_CONNECTION_SCHEMA_FIELDS,
							plist, error);
		g_object_unref (plist);
		if (!model)
			allok = FALSE;
		else {
			types = gda_server_provider_get_schema (gda_connection_get_provider_obj (cnc), cnc,
								GDA_CONNECTION_SCHEMA_TYPES, NULL, error);
			if (!types)
				allok = FALSE;
			else {
				gint row, nrows;
				nrows = gda_data_model_get_n_rows (model);
				
				for (row = 0; (row < nrows) && allok; row++) {
					const GValue *value, *tvalue;
					GType gtype;
					
					value = gda_data_model_get_value_at (model, 0, row);
					tvalue = gda_data_model_get_value_at (model, 1, row);
					if (!value || !gda_value_isa (value, G_TYPE_STRING) ||
					    !tvalue || !gda_value_isa (tvalue, G_TYPE_STRING))
						allok = FALSE;
					else {
						const gchar *fname = g_value_get_string (value);
						const gchar *tname = g_value_get_string (tvalue);
						
						gtype = find_gtype (types, tname);
						if (gtype == GDA_TYPE_BLOB)
							retfields = g_slist_append (retfields, g_strdup (fname));
					}
				}
			}

			g_object_unref (model);
			if (types)
				g_object_unref (types);
		}
	}

	if (!allok) {
		g_slist_foreach (retfields, (GFunc) g_free, NULL);
		g_slist_free (retfields);
		return NULL;
	}

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
gda_server_provider_blob_list_for_update (GdaConnection *cnc, GdaQuery *query, GdaQuery **out_select, GError **error)
{
	GdaQuery *select = NULL;
	GSList *ufields, *list;
	GSList *blob_value_prov_fields = NULL;
	gboolean retval = TRUE;
	
	g_return_val_if_fail (out_select, FALSE);
	g_return_val_if_fail (GDA_IS_QUERY (query), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);

	if (!gda_query_is_update_query (query)) {
		*out_select = NULL;
		return TRUE;
	}
	if (!gda_query_is_well_formed (query, NULL, error))
		return FALSE;

	/* create a list of GdaQueryFieldField fields which have a "value_provider" property refering to
	   a GdaQueryFieldValue of type GDA_TYPE_BLOB */
	ufields = gda_entity_get_fields (GDA_ENTITY (query));
	for (list = ufields; list; list=list->next) {
		GdaQueryFieldValue *fvalue;
		
		g_object_get (G_OBJECT (list->data), "value_provider", &fvalue, NULL);
		if (GDA_IS_QUERY_FIELD_VALUE (fvalue)) {
			if (gda_entity_field_get_g_type (GDA_ENTITY_FIELD (fvalue)) == GDA_TYPE_BLOB)
				blob_value_prov_fields = g_slist_append (blob_value_prov_fields, list->data);
		}
		g_object_unref (fvalue);
	}
	g_slist_free (ufields);

	if (blob_value_prov_fields) {
		/* create a SELECT query with all those fields in it */
		GdaDict *dict = gda_object_get_dict (GDA_OBJECT (query));
		GdaQueryTarget *target;
		GSList *key_fields_names;
			
		select = gda_query_new (dict);
		gda_query_set_query_type (select, GDA_QUERY_TYPE_SELECT);

		/* target */
		list = gda_query_get_targets (query);
		g_assert (list && list->data && !list->next);
		target = gda_query_target_new (select, 
					       gda_query_target_get_represented_table_name (GDA_QUERY_TARGET (list->data)));
		g_slist_free (list);
		gda_query_add_target (select, target, NULL);
		g_object_unref (target);

		/* BLOB fields first */
		for (list = blob_value_prov_fields; list; list = list->next) {
			GdaQueryField *nfield;
			GdaQueryFieldField *sfield = GDA_QUERY_FIELD_FIELD (list->data);

			nfield = (GdaQueryField *) g_object_new (GDA_TYPE_QUERY_FIELD_FIELD, "dict", dict,
								 "query", select, NULL);
			g_object_set (G_OBJECT (nfield), "target", target, 
				      "field-name", gda_query_field_field_get_ref_field_name (sfield), NULL);
			gda_entity_add_field (GDA_ENTITY (select), (GdaEntityField *) nfield);
			g_object_unref (nfield);
		}
		g_slist_free (blob_value_prov_fields);

		/* PK fields or all fields except BLOB fields if no PK key exists */
		key_fields_names = get_key_field_names (cnc, dict, gda_query_target_get_represented_table_name (target), 
							NULL, NULL);
		if (key_fields_names) {
			for (list = key_fields_names; list; list = list->next) {
				GdaQueryField *nfield;
				nfield = (GdaQueryField *) g_object_new (GDA_TYPE_QUERY_FIELD_FIELD, "dict", dict,
									 "query", select, NULL);
				g_object_set (G_OBJECT (nfield), "target", target, 
					      "field-name", (gchar *)(list->data), NULL);
				gda_entity_add_field (GDA_ENTITY (select), (GdaEntityField *) nfield);
				g_object_unref (nfield);
			}
			g_object_set_data (G_OBJECT (select), "_gda_nb_key_fields", 
					   GINT_TO_POINTER (g_slist_length (key_fields_names)));
			g_slist_foreach (key_fields_names, (GFunc) g_free, NULL);
			g_slist_free (key_fields_names);
		}

		/* WHERE condition */
		if (!copy_condition (query, select, error))
			retval = FALSE;
	}

	if (!retval) {
		g_object_unref (select);
		select = NULL;
	}

	*out_select = select;
	
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
gda_server_provider_blob_list_for_delete (GdaConnection *cnc, GdaQuery *query, GdaQuery **out_select, GError **error)
{
	GdaQuery *select = NULL;
	GSList *list;
	GSList *blob_fields = NULL;
	gboolean retval = TRUE;
	const gchar *table_name;
	GdaDict *dict;

	g_return_val_if_fail (out_select, FALSE);
	g_return_val_if_fail (GDA_IS_QUERY (query), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);

	if (!gda_query_is_delete_query (query)) {
		*out_select = NULL;
		return TRUE;
	}
	if (!gda_query_is_well_formed (query, NULL, error))
		return FALSE;

	/* table to delete from */
	list = gda_query_get_targets (query);
	g_assert (list && list->data && !list->next);
	table_name = gda_query_target_get_represented_table_name (GDA_QUERY_TARGET (list->data));
	g_slist_free (list);

	/* get a list of blob fields in the table to delete from */
	dict = gda_object_get_dict (GDA_OBJECT (query));
	blob_fields = get_blob_field_names (cnc, dict, table_name, error);

	if (blob_fields) {
		/* create a SELECT query with all those fields in it */
		GdaQueryTarget *target;
			
		select = gda_query_new (dict);
		gda_query_set_query_type (select, GDA_QUERY_TYPE_SELECT);

		/* target */
		target = gda_query_target_new (select, table_name);
		gda_query_add_target (select, target, NULL);
		g_object_unref (target);

		/* BLOB first */
		for (list = blob_fields; list; list = list->next) {
			GdaQueryField *nfield;
			
			nfield = (GdaQueryField *) g_object_new (GDA_TYPE_QUERY_FIELD_FIELD, "dict", dict,
								 "query", select, NULL);
			g_object_set (G_OBJECT (nfield), "target", target, 
				      "field-name", (gchar *)(list->data), NULL);
			gda_entity_add_field (GDA_ENTITY (select), (GdaEntityField *) nfield);
			g_object_unref (nfield);
		}

		/* WHERE condition */
		if (!copy_condition (query, select, error))
			retval = FALSE;

		g_slist_foreach (blob_fields, (GFunc) g_free, NULL);
		g_slist_free (blob_fields);
	}

	if (!retval) {
		g_object_unref (select);
		select = NULL;
	}

	*out_select = select;
	
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
gda_server_provider_split_update_query (GdaConnection *cnc, GdaQuery *query, GdaQuery **out_query, GError **error)
{
	GSList *key_fields_names;
	GSList *key_fields_types;
	GdaQuery *nquery = NULL;
	gboolean retval = TRUE;
	GdaDict *dict;
	GdaQueryTarget *target;
	GSList *list;

	g_return_val_if_fail (out_query, FALSE);
	g_return_val_if_fail (GDA_IS_QUERY (query), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_query_is_update_query (query), FALSE);

	dict = gda_object_get_dict (GDA_OBJECT (query));
	list = gda_query_get_targets (query);
	g_assert (list && list->data && !list->next);
	target = GDA_QUERY_TARGET (list->data);
	g_slist_free (list);

	key_fields_names = get_key_field_names (cnc, dict, gda_query_target_get_represented_table_name (target), 
						&key_fields_types, error);
	if (key_fields_names) {
		GSList *tlist;
		GdaQueryCondition *top, *cond;
		gint index;
		nquery = gda_query_new_copy (query, NULL);

		/* remove existing query condition, and create a new one using PK fields */
		gda_query_set_condition (nquery, NULL);

		top = gda_query_condition_new (nquery, GDA_QUERY_CONDITION_NODE_AND);
		gda_query_set_condition (nquery, top);
		g_object_unref (top);
		
		for (index = 0, list = key_fields_names, tlist = key_fields_types; 
		     list && tlist; 
		     list = list->next, tlist = tlist->next, index++) {
			GdaQueryField *ffield, *vfield;
			gchar *fname = (gchar *) (list->data);
			GType gtype = *((GType *) (tlist->data));
			gchar *str;
			
			ffield = (GdaQueryField *) g_object_new (GDA_TYPE_QUERY_FIELD_FIELD, "dict", dict,
								 "query", nquery, NULL);
			g_object_set (G_OBJECT (ffield), "target-name", gda_query_target_get_represented_table_name (target),
				      "field-name", fname, NULL);
			gda_entity_add_field (GDA_ENTITY (nquery), (GdaEntityField *) ffield);
			gda_query_field_set_visible (ffield, FALSE);
			g_object_unref (ffield);

			vfield = gda_query_field_value_new (nquery, gtype);
			str = g_strdup_printf ("_prov_EXTRA%d", index);
			gda_object_set_name (GDA_OBJECT (vfield), str);
			g_free (str);
			gda_entity_add_field (GDA_ENTITY (nquery), (GdaEntityField *) vfield);
			gda_query_field_set_visible (vfield, FALSE);
			gda_query_field_value_set_is_parameter ((GdaQueryFieldValue *) vfield, TRUE);
			g_object_unref (vfield);

			cond = gda_query_condition_new (nquery, GDA_QUERY_CONDITION_LEAF_EQUAL);
			gda_query_condition_leaf_set_operator (cond, GDA_QUERY_CONDITION_OP_LEFT, ffield);
			gda_query_condition_leaf_set_operator (cond, GDA_QUERY_CONDITION_OP_RIGHT, vfield);
			g_assert (gda_query_condition_node_add_child (top, cond, NULL));
			g_object_unref (cond);
		}
		g_assert (!list && !tlist);
		g_object_set_data (G_OBJECT (nquery), "_gda_nb_key_fields", 
				   GINT_TO_POINTER (g_slist_length (key_fields_names)));

		g_slist_foreach (key_fields_names, (GFunc) g_free, NULL);
		g_slist_free (key_fields_names);
		g_slist_foreach (key_fields_types, (GFunc) g_free, NULL);
		g_slist_free (key_fields_types);
	}
	else
		retval = FALSE;

	if (!retval) {
		if (nquery)
			g_object_unref (nquery);
		nquery = NULL;
	}

	*out_query = nquery;
	return retval;
}

/**
 * gda_server_provider_select_query_has_blobs
 * 
 * Determines if @query (which must be a SELECT query) returns a data model with some BLOB data
 */
gboolean
gda_server_provider_select_query_has_blobs (GdaConnection *cnc, GdaQuery *query, GError **error)
{
	gboolean has_blobs = FALSE;
	GSList *targets, *tlist;

	g_return_val_if_fail (GDA_IS_QUERY (query), FALSE);
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), FALSE);
	g_return_val_if_fail (gda_query_is_select_query (query), FALSE);

	targets = gda_query_get_targets (query);
	for (tlist = targets; tlist && !has_blobs; tlist=tlist->next) {
		GdaQueryTarget *target = GDA_QUERY_TARGET (tlist->data);
		const gchar *tname;

		tname = gda_query_target_get_represented_table_name (target);
		if (tname) {
			GSList *fields;

			fields = gda_query_get_fields_by_target (query, target, TRUE);
			if (fields) {
				/* there is at least one field of the tname table which is visible */
				GSList *vis_names;

				vis_names = get_blob_field_names (cnc, gda_object_get_dict (GDA_OBJECT (query)), 
								  tname, error);
				if (vis_names) {
					GSList *names, *list;
					for (names = vis_names; names && !has_blobs; names = names->next) {
						for (list = fields; list && !has_blobs; list = list->next) {
							if (GDA_IS_QUERY_FIELD_FIELD (list->data)) {
								gchar *fname = NULL;
								GdaObject *fobj = NULL;

								g_object_get (G_OBJECT (list->data), 
									      "field", &fobj, NULL);
								if (fobj) {
									fname = g_strdup (gda_object_get_name (fobj));
									g_object_unref (fobj);
								}
								else {
									g_object_get (G_OBJECT (list->data), 
										      "field_name", &fname, NULL);
								}

								if (!strcmp (fname, (gchar *) (names->data)))
									has_blobs = TRUE;
								
								g_free (fname);
							}
							else if (GDA_IS_QUERY_FIELD_ALL (list->data))
								has_blobs = TRUE;
						}
					}
					g_slist_foreach (vis_names, (GFunc) g_free, NULL);
					g_slist_free (vis_names);
				}
				g_slist_free (fields);
			}
		}
	}
	g_slist_free (targets);

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

	if (!g_file_get_contents (file, &contents, NULL, NULL)) {
		/* look in @data_dir */
		g_free (file);
		file = g_build_filename (inst_dir, "..", filename, NULL);
		if (!g_file_get_contents (file, &contents, NULL, NULL) && data_dir) {
			/* look in the parent dir, to handle the case where the lib is in a .libs dir */
			g_free (file);
			file = g_build_filename (data_dir, filename, NULL);
			g_file_get_contents (file, &contents, NULL, NULL);
		}
	}
	g_free (file);

	return contents;
}
