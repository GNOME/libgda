/*
 * Copyright (C) 2010 Claude Paroz <claude@2xlibre.net>
 * Copyright (C) 2010 David King <davidk@openismus.com>
 * Copyright (C) 2010 Murray Cumming <murrayc@murrayc.com>
 * Copyright (C) 2010 - 2015 Vivien Malerba <malerba@gnome-db.org>
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

#include <string.h>
#include <glib/gi18n-lib.h>
#include "../common/t-connection.h"
#include "../ui-support.h"
#include "data-manager-marshal.h"
#include <sql-parser/gda-sql-parser.h>
#include <libgda/gda-data-model-extra.h>
#include <libgda/gda-sql-builder.h>
#include "../ui-formgrid.h"
#include "common/t-errors.h"

#include "data-source.h"
#define DEFAULT_DATA_SOURCE_NAME "DataSource"
#define DEPENDENCY_SEPARATOR "<|>"

/* signals */
enum {
	CHANGED,
	EXEC_STARTED,
	EXEC_FINISHED,
	LAST_SIGNAL
};

gint data_source_signals [LAST_SIGNAL] = {0, 0, 0};

/* 
 * Main static functions 
 */
static void data_source_class_init (DataSourceClass *klass);
static void data_source_init (DataSource *source);
static void data_source_dispose (GObject *object);


static void update_export_information (DataSource *source);
static void compute_stmt_and_params (DataSource *source);
static void compute_import_params (DataSource *source);


/* get a pointer to the parents to be able to call their destructor */
static GObjectClass  *parent_class = NULL;

typedef struct {
	gchar *dep_id;
	gchar *dep_table;
	gchar *dep_columns; /* column names, separated by a |, sorted */
} Dependency;

static void
dependency_free (Dependency *dep)
{
	g_free (dep->dep_id);
	g_free (dep->dep_table);
	g_free (dep->dep_columns);
	g_free (dep);
}

/*
 * converts an array of column names into a single string in the format:
 * <colname>[SEP<colname>...] where SEP is DEPENDENCY_SEPARATOR
 * 
 * Returns: a new string, never %NULL
 */
static gchar *
column_names_to_string (gint size, const gchar **colnames)
{
	if (!colnames)
		return g_strdup ("");

	GString *string = NULL;
	GArray *colsarray;
	gint i;
	colsarray = g_array_new (FALSE, FALSE, sizeof (gchar*));
	for (i = 0; i < size; i++)
		g_array_append_val (colsarray, colnames[i]);
	g_array_sort (colsarray, (GCompareFunc) g_strcmp0);
	for (i = 0; i < size; i++) {
		gchar *tmp;
		tmp = g_array_index (colsarray, gchar *, i);
		if (!string)
			string = g_string_new (tmp);
		else {
			g_string_append (string, DEPENDENCY_SEPARATOR);
			g_string_append (string, tmp);
		}
	}
	g_array_free (colsarray, TRUE);
	return g_string_free (string, FALSE);
}

static Dependency *
dependency_find (GSList *dep_list, const gchar *id, const gchar *table, gint size, const gchar **colnames)
{
	GSList *list;
	gchar *colsstring = NULL;
	for (list = dep_list; list; list = list->next) {
		Dependency *dep = (Dependency*) list->data;
		if (strcmp (dep->dep_id, id) || strcmp (dep->dep_table, table))
			continue;

		if (!colsstring)
			colsstring = column_names_to_string (size, colnames);
		if (!strcmp (colsstring, dep->dep_columns)) {
			g_free (colsstring);
			return dep;
		}
	}
	g_free (colsstring);
	return NULL;
}

struct _DataSourcePrivate {
	TConnection *tcnc;
	gchar             *title;
	gchar             *impl_title;
	gchar             *id;
	DataSourceType     source_type;

	GError            *init_error;
	GArray            *export_names; /* array of strings, memory allocated in export_columns */
	GHashTable        *export_columns; /* key = export name, value = column number */

	guint              exec_id;
	gboolean           executing;
	gboolean           exec_again;

	gchar             *tablename;
	GdaSqlBuilder     *builder;
	GSList            *dependencies; /* list of Dependency pointers */

	GdaStatement      *stmt;
	GdaSet            *ext_params; /* "free" parameters */
	GdaSet            *params; /* all the params used when executing @stmt */
	gboolean           need_rerun; /* set to %TRUE if @params has changed since the last exec */

	GError            *exec_error;

	GdaDataModel      *model;
};

GType
data_source_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static GMutex registering;
		static const GTypeInfo info = {
			sizeof (DataSourceClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) data_source_class_init,
			NULL,
			NULL,
			sizeof (DataSource),
			0,
			(GInstanceInitFunc) data_source_init,
			0
		};

		
		g_mutex_lock (&registering);
		if (type == 0)
			type = g_type_register_static (G_TYPE_OBJECT, "DataSource", &info, 0);
		g_mutex_unlock (&registering);
	}
	return type;
}

static void
data_source_class_init (DataSourceClass *klass)
{
	GObjectClass   *object_class = G_OBJECT_CLASS (klass);
	parent_class = g_type_class_peek_parent (klass);

	/* signals */
	data_source_signals [CHANGED] =
                g_signal_new ("changed",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (DataSourceClass, changed),
                              NULL, NULL,
                              _dm_marshal_VOID__VOID, G_TYPE_NONE, 0);
	data_source_signals [EXEC_STARTED] =
                g_signal_new ("execution-started",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (DataSourceClass, execution_started),
                              NULL, NULL,
                              _dm_marshal_VOID__VOID, G_TYPE_NONE, 0);
	data_source_signals [EXEC_FINISHED] =
                g_signal_new ("execution-finished",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (DataSourceClass, execution_finished),
                              NULL, NULL,
                              _dm_marshal_VOID__POINTER, G_TYPE_NONE, 1, G_TYPE_POINTER);
        klass->changed = NULL;
        klass->execution_started = NULL;
        klass->execution_finished = NULL;

	object_class->dispose = data_source_dispose;
}

static void
data_source_init (DataSource *source)
{
	source->priv = g_new0 (DataSourcePrivate, 1);
	source->priv->id = g_strdup (DEFAULT_DATA_SOURCE_NAME);
	source->priv->tcnc = NULL;
	source->priv->source_type = DATA_SOURCE_UNKNOWN;
	source->priv->need_rerun = FALSE;
	source->priv->exec_id = 0;
	source->priv->executing = FALSE;
	source->priv->exec_again = FALSE;
}

static void
params_changed_cb (G_GNUC_UNUSED GdaSet *params, G_GNUC_UNUSED GdaHolder *holder, DataSource *source)
{
	source->priv->need_rerun = TRUE;
}

static void
ext_params_changed_cb (G_GNUC_UNUSED GdaSet *params, G_GNUC_UNUSED GdaHolder *holder, DataSource *source)
{
#ifdef DEBUG_SOURCE
	g_print ("  => data source [%s] should rerun\n",
		 data_source_get_title (source));
#endif
	source->priv->need_rerun = TRUE;
}

static void
data_source_reset (DataSource *source)
{
	source->priv->source_type = DATA_SOURCE_UNKNOWN;
	g_clear_error (& source->priv->init_error);

	if (source->priv->builder) {
		g_object_unref (source->priv->builder);
		source->priv->builder = NULL;
	}
	if (source->priv->stmt) {
		g_object_unref (source->priv->stmt);
		source->priv->stmt = NULL;
	}
	if (source->priv->params) {
		g_signal_handlers_disconnect_by_func (source->priv->params,
						      G_CALLBACK (params_changed_cb), source);
		g_object_unref (source->priv->params);
		source->priv->params = NULL;
	}
	if (source->priv->ext_params) {
		g_signal_handlers_disconnect_by_func (source->priv->ext_params,
						      G_CALLBACK (ext_params_changed_cb),
						      source);
		g_object_unref (source->priv->ext_params);
		source->priv->ext_params = NULL;
	}
	
	if (source->priv->tablename) {
		g_free (source->priv->tablename);
		source->priv->tablename = NULL;
	}
	
	if (source->priv->dependencies) {
		g_slist_foreach (source->priv->dependencies, (GFunc) dependency_free, NULL);
		g_slist_free (source->priv->dependencies);
		source->priv->dependencies = NULL;
	}
	
	if (source->priv->model) {
		g_object_unref (source->priv->model);
		source->priv->model = NULL;
	}
	
	if (source->priv->export_names) {
		g_array_free (source->priv->export_names, TRUE);
		source->priv->export_names = NULL;
	}
	
	if (source->priv->export_columns) {
		g_hash_table_destroy (source->priv->export_columns);
		source->priv->export_columns = NULL;
	}
}

static void
data_source_dispose (GObject *object)
{
	DataSource *source;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_DATA_SOURCE (object));

	source = DATA_SOURCE (object);
	if (source->priv) {
		if (source->priv->tcnc)
			g_object_unref (source->priv->tcnc);
		data_source_reset (source);
		g_free (source->priv->id);
		g_free (source->priv->title);
		g_free (source->priv->impl_title);

		g_free (source->priv);
		source->priv = NULL;
	}

	/* parent class */
	parent_class->dispose (object);
}

static void init_from_query (DataSource *source, xmlNodePtr node);
static gboolean init_from_table_node (DataSource *source, xmlNodePtr node, GError **error);

/**
 * data_source_new
 * @tcnc: a #TConnection
 * @type: the new data source's requested type
 *
 * Returns: a new #DataSource object
 */
DataSource *
data_source_new (TConnection *tcnc, DataSourceType type)
{
	DataSource *source;

	g_return_val_if_fail (T_IS_CONNECTION (tcnc), NULL);

	source = DATA_SOURCE (g_object_new (DATA_SOURCE_TYPE, NULL));
	source->priv->tcnc = g_object_ref (tcnc);
	source->priv->source_type = type;

	return source;
}

/**
 * data_source_new_from_xml_node
 * @tcnc: a #TConnection
 * @node:
 * @error:
 *
 * Creates a new #DataSource object
 *
 * Returns: a new object
 */
DataSource*
data_source_new_from_xml_node (TConnection *tcnc, xmlNodePtr node, GError **error)
{
	DataSource *source;

	g_return_val_if_fail (T_IS_CONNECTION (tcnc), NULL);
	g_return_val_if_fail (node, NULL);

	source = DATA_SOURCE (g_object_new (DATA_SOURCE_TYPE, NULL));
	source->priv->tcnc = g_object_ref (tcnc);
	xmlChar *prop;
	prop = xmlGetProp (node, BAD_CAST "title");
	if (prop) {
		g_free (source->priv->title);
		source->priv->title = g_strdup ((gchar*) prop);
		xmlFree (prop);
	}
	prop = xmlGetProp (node, BAD_CAST "id");
	if (prop) {
		g_free (source->priv->id);
		source->priv->id = g_strdup ((gchar*) prop);
		xmlFree (prop);
	}

	if (!strcmp ((gchar*)node->name, "table")) {
		if (! init_from_table_node (source, node, error)) {
			g_object_unref (source);
			source = NULL;
		}
	}
	else if (!strcmp ((gchar*)node->name, "query")) {
		init_from_query (source, node);
	}
	else {
		g_set_error (error, T_ERROR, T_COMMAND_ARGUMENTS_ERROR,
			     /* Translators: Do not translate "table" nor "query" */
			     _("Node must be \"table\" or \"query\", and is \"%s\""), (gchar*)node->name);
		g_object_unref (source);
		source = NULL;
	}
	
	return source;
}

static void
init_from_query (DataSource *source, xmlNodePtr node)
{
	xmlChar *contents;

#ifdef DEBUG_SOURCE
	g_print ("%s(%s [%s])\n", __FUNCTION__, source->priv->id, source->priv->title);
#endif
	contents = xmlNodeGetContent (node);
	g_clear_error (& source->priv->init_error);
	data_source_set_query (source, (gchar*) contents, &source->priv->init_error);
}

static GdaMetaTable *
get_meta_table (DataSource *source, const gchar *table_name, GError **error)
{
	GdaMetaStruct *mstruct;
	GdaMetaDbObject *dbo;
	GValue *vname[3] = {NULL, NULL, NULL};
	gchar **split;
	gint len;

	mstruct = t_connection_get_meta_struct (source->priv->tcnc);
	if (! mstruct) {
		g_set_error (error, T_ERROR, T_STORED_DATA_ERROR,
			     "%s", _("Not ready"));
		return NULL;
	}

	split = gda_sql_identifier_split (table_name);
	if (! split) {
		g_set_error (error, T_ERROR, T_COMMAND_ARGUMENTS_ERROR,
			     _("Malformed table name \"%s\""), table_name);
		return NULL;
	}
	len = g_strv_length (split);
	g_value_set_string ((vname[2] = gda_value_new (G_TYPE_STRING)), split[len - 1]);
	if (len > 1)
		g_value_set_string ((vname[1] = gda_value_new (G_TYPE_STRING)), split[len -2]);
	if (len > 2)
		g_value_set_string ((vname[0] = gda_value_new (G_TYPE_STRING)), split[len - 3]);

	dbo = gda_meta_struct_get_db_object (mstruct, vname[0], vname[1], vname[2]);
	if (vname[0]) gda_value_free (vname[0]);
	if (vname[1]) gda_value_free (vname[1]);
	if (vname[2]) gda_value_free (vname[2]);

	if (! dbo) {
		g_set_error (error, T_ERROR, T_INTERNAL_COMMAND_ERROR,
			     _("Could not find the \"%s\" table"), table_name);
		return NULL;
	}
	if ((dbo->obj_type != GDA_META_DB_TABLE) && (dbo->obj_type != GDA_META_DB_VIEW)) {
		g_set_error (error, T_ERROR, T_INTERNAL_COMMAND_ERROR,
			     _("The \"%s\" object is not a table"), table_name);
		return NULL;
	}
	return GDA_META_TABLE (dbo);
}

static gboolean
init_from_table_node (DataSource *source, xmlNodePtr node, GError **error)
{
	xmlChar *tname;
	gboolean retval;

#ifdef DEBUG_SOURCE
	g_print ("%s(%s [%s])\n", __FUNCTION__, source->priv->id, source->priv->title);
#endif
	tname = xmlGetProp (node, BAD_CAST "name");
	if (!tname) {
		g_set_error (error, T_ERROR, T_COMMAND_ARGUMENTS_ERROR,
			     /* Translators: Do not translate "name" */
			     "%s", _("Missing attribute \"name\" for table"));
		return FALSE;
	}

	retval = data_source_set_table (source, (gchar*) tname, error);
	xmlFree (tname);

	/* linking */
	xmlNodePtr subnode;
	for (subnode = node->children; subnode; subnode = subnode->next) {
		if (!strcmp ((gchar*)subnode->name, "depend")) {
			xmlChar *fk_table, *id;
			GArray *cols_array = NULL;
			xmlNodePtr chnode;
			
			fk_table = xmlGetProp (subnode, BAD_CAST "foreign_key_table");
			id = xmlGetProp (subnode, BAD_CAST "id");
			for (chnode = subnode->children; chnode; chnode = chnode->next) {
				xmlChar *colname;
				if (strcmp ((gchar*)chnode->name, "column"))
					continue;
				colname = xmlNodeGetContent (chnode);
				if (colname) {
					if (! cols_array)
						cols_array = g_array_new (FALSE, FALSE, sizeof (gchar*));
					g_array_append_val (cols_array, colname);
				}
			}

			if (fk_table &&
			    ! data_source_add_dependency (source, (gchar *) fk_table, (gchar*) id,
							  cols_array ? cols_array->len : 0,
							  (const gchar **) (cols_array ? cols_array->data : NULL),
							  error))
				retval = FALSE;
			if (fk_table)
				xmlFree (fk_table);
			if (id)
				xmlFree (id);
			if (cols_array) {
				gsize i;
				for (i = 0; i < cols_array->len; i++) {
					xmlChar *colname;
					colname = g_array_index (cols_array, xmlChar*, i);
					xmlFree (colname);
				}
				g_array_free (cols_array, TRUE);
			}
			break;
		}
	}

	return retval;
}

/**
 * data_source_add_dependency
 * @source: a #DataSource
 * @table: the name of the referenced table
 * @id: (nullable): the ID of the referenced data source, or %NULL if its ID is the same as the table name
 * @col_name_size: the size of @col_names
 * @col_names: (nullable): names of the FK columns involved in the foreign key, or %NULL
 * @error: a place to store errors, or %NULL
 *
 * Adds a dependency on the @table table, only for DATA_SOURCE_TABLE sources
 */
gboolean
data_source_add_dependency (DataSource *source, const gchar *table,
			    const char *id, gint col_name_size, const gchar **col_names,
			    GError **error)
{
	g_return_val_if_fail (IS_DATA_SOURCE (source), FALSE);
	g_return_val_if_fail (table && *table, FALSE);
	g_return_val_if_fail (source->priv->source_type == DATA_SOURCE_TABLE, FALSE);
	g_return_val_if_fail (source->priv->builder, FALSE);

	if (dependency_find (source->priv->dependencies, id ? id : table, table, col_name_size, col_names))
		return TRUE;

	GdaMetaTable *mtable, *mlinked;
	mtable = get_meta_table (source, source->priv->tablename, error);
	if (!mtable)
		return FALSE;

	mlinked = get_meta_table (source, table, error);
	if (!mlinked)
		return FALSE;
	
	/* find foreign key to linked table */
	GdaMetaTableForeignKey *fk = NULL;
	GSList *list;
	gboolean reverse = FALSE;
	for (list = mtable->fk_list; list; list = list->next) {
		if (GDA_META_TABLE_FOREIGN_KEY (list->data)->depend_on == GDA_META_DB_OBJECT (mlinked)) {
			fk = GDA_META_TABLE_FOREIGN_KEY (list->data);
			if (col_names && (col_name_size == fk->cols_nb)) {
				gint i;
				for (i = 0; i < col_name_size; i++) {
					gint j;
					for (j = 0; j < col_name_size; j++) {
						if (!strcmp (col_names [i], fk->fk_names_array [j]))
							break;
					}
					if (j == col_name_size) {
						fk = NULL; /* not this FK */
						break;
					}
				}
			}
			if (fk)
				break;
		}
	}
	if (!fk) {
		for (list = mlinked->fk_list; list; list = list->next) {
			if (GDA_META_TABLE_FOREIGN_KEY (list->data)->depend_on == GDA_META_DB_OBJECT (mtable)) {
				fk = GDA_META_TABLE_FOREIGN_KEY (list->data);
				reverse = TRUE;
				if (col_names && (col_name_size == fk->cols_nb)) {
					gint i;
					for (i = 0; i < col_name_size; i++) {
						gint j;
						for (j = 0; j < col_name_size; j++) {
							if (!strcmp (col_names [i],
								     fk->fk_names_array [j]))
								break;
						}
						if (j == col_name_size) {
							fk = NULL; /* not this FK */
							break;
						}
					}
				}
				if (fk)
					break;
			}
		}
	}
	if (!fk) {
		g_set_error (error, T_ERROR, T_INTERNAL_COMMAND_ERROR,
			     _("Could not find any foreign key to \"%s\""), table);
		return FALSE;
	}
	else if (fk->cols_nb <= 0) {
		g_set_error (error, T_ERROR, T_INTERNAL_COMMAND_ERROR,
			     _("The fields involved in the foreign key to \"%s\" are not known"),
			     table);
		return FALSE;
	}
	else if (fk->cols_nb == 1) {
		gchar *tmp;
		GdaMetaTableColumn *col;
		GdaSqlBuilderId id1, id2, id_cond;
		if (reverse) {
			id1 = gda_sql_builder_add_id (source->priv->builder, fk->ref_pk_names_array [0]);
			tmp = g_strdup_printf ("%s@%s", id ? id : table, fk->fk_names_array [0]);

			col = GDA_META_TABLE_COLUMN (g_slist_nth_data (mlinked->columns,
								       fk->fk_cols_array [0] - 1));
			g_assert (col);
			id2 = gda_sql_builder_add_param (source->priv->builder, tmp, col->gtype, FALSE);
			g_free (tmp);
		}
		else {
			id1 = gda_sql_builder_add_id (source->priv->builder, fk->fk_names_array [0]);
			tmp = g_strdup_printf ("%s@%s", id ? id : table, fk->ref_pk_names_array [0]);
			
			col = GDA_META_TABLE_COLUMN (g_slist_nth_data (mlinked->columns,
								       fk->ref_pk_cols_array [0] - 1));
			g_assert (col);
			id2 = gda_sql_builder_add_param (source->priv->builder, tmp, col->gtype, FALSE);
			g_free (tmp);
			id_cond = gda_sql_builder_add_cond (source->priv->builder,
							    GDA_SQL_OPERATOR_TYPE_EQ,
							    id1, id2, 0);
		}
		id_cond = gda_sql_builder_add_cond (source->priv->builder,
						    GDA_SQL_OPERATOR_TYPE_EQ,
						    id1, id2, 0);
		gda_sql_builder_set_where (source->priv->builder, id_cond);
	}
	else {
		gchar *tmp;
		gint i;
		GdaMetaTableColumn *col;
		GdaSqlBuilderId andid;
		GdaSqlBuilderId *op_ids;
		GdaSqlBuilderId id1, id2;
		op_ids = g_new (GdaSqlBuilderId, fk->cols_nb);
		
		for (i = 0; i < fk->cols_nb; i++) {
			if (reverse) {
				id1 = gda_sql_builder_add_id (source->priv->builder, fk->ref_pk_names_array [i]);
				tmp = g_strdup_printf ("%s@%s", id ? id : table, fk->fk_names_array [i]);

				col = GDA_META_TABLE_COLUMN (g_slist_nth_data (mlinked->columns,
									       fk->fk_cols_array [i] - 1));
				g_assert (col);
				id2 = gda_sql_builder_add_param (source->priv->builder, tmp, col->gtype, FALSE);
				g_free (tmp);
			}
			else {
				id1 = gda_sql_builder_add_id (source->priv->builder, fk->fk_names_array [i]);
				tmp = g_strdup_printf ("%s@%s", id ? id : table, fk->ref_pk_names_array [i]);
				
				col = GDA_META_TABLE_COLUMN (g_slist_nth_data (mlinked->columns,
									       fk->ref_pk_cols_array [i] - 1));
				g_assert (col);
				id2 = gda_sql_builder_add_param (source->priv->builder, tmp, col->gtype, FALSE);
				g_free (tmp);
			}
			op_ids [i] = gda_sql_builder_add_cond (source->priv->builder,
							       GDA_SQL_OPERATOR_TYPE_EQ,
							       id1, id2, 0);
		}
		andid = gda_sql_builder_add_cond_v (source->priv->builder, GDA_SQL_OPERATOR_TYPE_AND,
						    op_ids, fk->cols_nb);
		g_free (op_ids);
		gda_sql_builder_set_where (source->priv->builder, andid);
	}

	Dependency *dep = g_new0 (Dependency, 1);
	dep->dep_id = g_strdup (id ? id : table);
	dep->dep_table = g_strdup (table);
	dep->dep_columns = column_names_to_string (col_name_size, col_names);
	source->priv->dependencies = g_slist_append (source->priv->dependencies, dep);

	compute_stmt_and_params (source);
	return TRUE;
}


/**
 * data_source_to_xml_node
 */
xmlNodePtr
data_source_to_xml_node (DataSource *source)
{
	xmlNodePtr node = NULL;
	g_return_val_if_fail (IS_DATA_SOURCE (source), NULL);
	switch (source->priv->source_type) {
	case DATA_SOURCE_TABLE:
		node = xmlNewNode (NULL, BAD_CAST "table");
		if (g_strcmp0 (source->priv->id, source->priv->tablename))
			xmlSetProp (node, BAD_CAST "id", BAD_CAST source->priv->id);
		if (source->priv->title && g_strcmp0 (source->priv->title, source->priv->tablename))
			xmlSetProp (node, BAD_CAST "title", BAD_CAST source->priv->title);
		xmlSetProp (node, BAD_CAST "name",
			    BAD_CAST (source->priv->tablename ? source->priv->tablename : ""));

		if (source->priv->dependencies) {
			GSList *list;
			for (list = source->priv->dependencies; list; list = list->next) {
				Dependency *dep = (Dependency*) list->data;
				xmlNodePtr depnode;
				depnode = xmlNewChild (node, NULL, BAD_CAST "depend", NULL);
				xmlSetProp (depnode, BAD_CAST "foreign_key_table",
					    BAD_CAST (dep->dep_table));
				xmlSetProp (depnode, BAD_CAST "id",
					    BAD_CAST (dep->dep_id));

				gchar **array;
				gint i;
				array = g_strsplit (dep->dep_columns, DEPENDENCY_SEPARATOR, 0);
				for (i = 0; array[i]; i++)
					xmlNewChild (depnode, NULL, BAD_CAST "column", BAD_CAST (array[i]));
				g_strfreev (array);
			}
		}
		break;
	case DATA_SOURCE_SELECT: {
		node = xmlNewNode (NULL, BAD_CAST "query");
		xmlSetProp (node, BAD_CAST "id", BAD_CAST source->priv->id);
		if (source->priv->title)
			xmlSetProp (node, BAD_CAST "title", BAD_CAST source->priv->title);

		if (source->priv->stmt) {
			gchar *sql;
			sql = gda_statement_to_sql_extended (source->priv->stmt, NULL, NULL,
							     GDA_STATEMENT_SQL_PRETTY |
							     GDA_STATEMENT_SQL_PARAMS_SHORT, NULL, NULL);
			if (sql) {
				xmlNodeSetContent (node, BAD_CAST sql);
				g_free (sql);
			}
		}
		break;
	}
	default:
		break;
	}

	if (node) {
	}
	return node;
}

/**
 *data_source_get_statement
 */
GdaStatement *
data_source_get_statement (DataSource *source)
{
	g_return_val_if_fail (IS_DATA_SOURCE (source), NULL);
	return source->priv->stmt;
}

/**
 * data_source_execution_going_on
 */
gboolean
data_source_execution_going_on (DataSource *source)
{
	g_return_val_if_fail (IS_DATA_SOURCE (source), FALSE);
	return source->priv->executing || (source->priv->exec_id > 0);
}

/**
 * data_source_get_import
 *
 * Returns: a pointer to a read-only #GdaSet, or %NULL (must not be modified)
 */
GdaSet *
data_source_get_import (DataSource *source)
{
	g_return_val_if_fail (IS_DATA_SOURCE (source), NULL);
	return source->priv->params;
}

/**
 * data_source_set_params
 */
void
data_source_set_params (DataSource *source, GdaSet *params)
{
	gboolean bound = FALSE;
	g_return_if_fail (IS_DATA_SOURCE (source));
	g_return_if_fail (!params || GDA_IS_SET (params));

	if (source->priv->ext_params) {
		g_signal_handlers_disconnect_by_func (source->priv->ext_params,
						      G_CALLBACK (ext_params_changed_cb), source);
		g_object_unref (source->priv->ext_params);
		source->priv->ext_params = NULL;
	}

	if (source->priv->params) {
		GSList *list;
		for (list = gda_set_get_holders (source->priv->params); list; list = list->next) {
			GdaHolder *holder = GDA_HOLDER (list->data);
			GdaHolder *bind = NULL;
			if (params)
				bind = gda_set_get_holder (params, gda_holder_get_id (holder));
			if ((holder != bind) && gda_holder_set_bind (holder, bind, NULL))
				bound = TRUE;
		}
	}

	if (params && bound) {
		source->priv->ext_params = g_object_ref (params);
		g_signal_connect (params, "holder-changed",
				  G_CALLBACK (ext_params_changed_cb), source);
	}
}

/**
 * data_source_get_export_names
 *
 * Returns: an array of strings (don't modify) or %NULL
 */
GArray *
data_source_get_export_names (DataSource *source)
{
	g_return_val_if_fail (IS_DATA_SOURCE (source), NULL);
	return source->priv->export_names;
}

/**
 * data_source_get_export_columns
 *
 * Returns: a #GHashTable where key is an export name and value is its column number (use GPOINTER_TO_INT)
 */
GHashTable *
data_source_get_export_columns (DataSource *source)
{
	g_return_val_if_fail (IS_DATA_SOURCE (source), NULL);
	return source->priv->export_columns;	
}

/**
 * data_source_execute
 */
void
data_source_execute (DataSource *source, GError **error)
{
	GError *lerror = NULL;
	g_return_if_fail (IS_DATA_SOURCE (source));

	if (source->priv->exec_again || source->priv->executing) {
		source->priv->exec_again = TRUE;
		return;
	}

	source->priv->executing = TRUE;
	if (! source->priv->stmt) {
		if (source->priv->init_error)
			g_propagate_error (error, source->priv->init_error);
		else
			g_set_error (error, T_ERROR, T_COMMAND_ARGUMENTS_ERROR,
				     "%s", _("No SELECT statement to execute"));
	}

	if (source->priv->model) {
		if (source->priv->need_rerun) {
			source->priv->need_rerun = FALSE;
			g_signal_emit (source, data_source_signals [EXEC_STARTED], 0);
      // FIXME: Use a GdaDataModelSelect instead
			gda_data_model_dump (source->priv->model, NULL);
			g_signal_emit (source, data_source_signals [EXEC_FINISHED], 0, lerror);
		}
	}
	else {
		GObject *result;
		g_signal_emit (source, data_source_signals [EXEC_STARTED], 0);
		result = t_connection_execute_statement (source->priv->tcnc,
							       source->priv->stmt,
							       source->priv->params,
							       GDA_STATEMENT_MODEL_RANDOM_ACCESS |
							       GDA_STATEMENT_MODEL_ALLOW_NOPARAM,
							       NULL, &lerror);
		if (result) {
			if (GDA_IS_DATA_MODEL (result)) {
				if (source->priv->model != GDA_DATA_MODEL (result)) {
					if (source->priv->model)
						g_object_unref (source->priv->model);
					source->priv->model = GDA_DATA_MODEL (result);
				}
				gda_data_model_dump (source->priv->model, NULL);
			}
			else {
				g_object_unref (result);
				g_set_error (&lerror, T_ERROR, T_COMMAND_ARGUMENTS_ERROR,
					     "%s", _("Statement to execute is not a selection statement"));
			}
			g_signal_emit (source, data_source_signals [EXEC_FINISHED], 0, lerror);
		}

		g_signal_emit (source, data_source_signals [EXEC_FINISHED], 0, lerror);
	}

	if (source->priv->exec_again) {
		source->priv->exec_again = FALSE;
		data_source_execute (source, NULL);
	}

	source->priv->executing = FALSE;
}

static void
action_refresh_cb (GtkButton *button, DataSource *source)
{
	source->priv->need_rerun = TRUE;
	data_source_execute (source, NULL);
}

/**
 * data_source_create_grid
 *
 * Returns: a new #GdauiRawGrid, or %NULL if an error occurred
 */
GtkWidget *
data_source_create_grid (DataSource *source)
{
	g_return_val_if_fail (IS_DATA_SOURCE (source), NULL);

	if (! source->priv->model)
		return NULL;

	GtkWidget *fg;
	fg = ui_formgrid_new (source->priv->model, FALSE, GDAUI_DATA_PROXY_INFO_ROW_MODIFY_BUTTONS);

	ui_formgrid_set_refresh_func (UI_FORMGRID (fg), G_CALLBACK (action_refresh_cb), source);

	return fg;
}

/**
 * data_source_set_id
 * @source: a #DataSource
 * @id: the new source's ID, not %NULL
 *
 * @source MUST NOT be executed when calling this method.
 */
void
data_source_set_id (DataSource *source, const gchar * id)
{
	g_return_if_fail (IS_DATA_SOURCE (source));
	g_return_if_fail (! data_source_execution_going_on (source));
	g_return_if_fail (id && *id);

	g_free (source->priv->id);
	source->priv->id = g_strdup (id);
	update_export_information (source);
	g_signal_emit (source, data_source_signals [CHANGED], 0);
}


/**
 * data_source_get_id
 * @source: a #DataSource
 *
 * Returns: the ID, or %NULL if no ID has been defined
 */
const gchar *
data_source_get_id (DataSource *source)
{
	g_return_val_if_fail (IS_DATA_SOURCE (source), NULL);

	return source->priv->id;
}

/**
 * data_source_set_title
 * @source: a #DataSource
 * @title: the new source's TITLE
 *
 * @source MUST NOT be executed when calling this method.
 */
void
data_source_set_title (DataSource *source, const gchar * title)
{
	g_return_if_fail (IS_DATA_SOURCE (source));
	g_return_if_fail (! data_source_execution_going_on (source));

	g_free (source->priv->title);
	if (title)
		source->priv->title = g_strdup (title);
	else
		source->priv->title = NULL;
	g_signal_emit (source, data_source_signals [CHANGED], 0);
}

/**
 * data_source_get_title
 * @source: a #DataSource
 */
const gchar *
data_source_get_title (DataSource *source)
{
	g_return_val_if_fail (IS_DATA_SOURCE (source), NULL);

	if (source->priv->title)
		return source->priv->title;
	else if (source->priv->impl_title)
		return source->priv->impl_title;
	else
		return source->priv->id;
}

static void
update_export_information (DataSource *source)
{
	g_assert (source->priv->id);

	/* clear previous information */
	if (source->priv->export_names) {
		g_array_free (source->priv->export_names, TRUE);
		source->priv->export_names = NULL;
	}
	if (source->priv->export_columns) {
		g_hash_table_destroy (source->priv->export_columns);
		source->priv->export_columns = NULL;
	}

	if (! source->priv->stmt)
		return;

	/* Get GdaSqlStatement */
	GdaSqlStatement *sqlst;
	g_object_get ((GObject*) source->priv->stmt, "structure", &sqlst, NULL);
	if (t_connection_check_sql_statement_validify (source->priv->tcnc, sqlst, NULL))
		g_object_set ((GObject*) source->priv->stmt, "structure", sqlst, NULL);
	if (! sqlst)
		return;

	/* compute exported data */
	if (sqlst->stmt_type == GDA_SQL_STATEMENT_SELECT) {
		GdaSqlStatementSelect *selst;
		selst = (GdaSqlStatementSelect*) sqlst->contents;
		GSList *list;
		gint i;
		for (i = 0, list = selst->expr_list; list; i++, list = list->next) {
			gchar *tmp;
			if (! source->priv->export_names)
				source->priv->export_names = g_array_new (FALSE, FALSE,
									  sizeof (gchar*));
			if (! source->priv->export_columns)
				source->priv->export_columns =
					g_hash_table_new_full (g_str_hash, g_str_equal,
							       g_free, NULL);
			
			tmp = g_strdup_printf ("%s@%d", source->priv->id, i+1);
			g_array_append_val (source->priv->export_names, tmp);
			g_hash_table_insert (source->priv->export_columns, tmp,
					     GINT_TO_POINTER (i + 1));
#ifdef DEBUG_SOURCE
			g_print ("\tEXPORT [%s]\n", tmp);
#endif
			
			GdaSqlSelectField *sf = (GdaSqlSelectField *) list->data;
			if (sf->validity_meta_table_column) {
				tmp = g_strdup_printf ("%s@%s", source->priv->id,
						       sf->validity_meta_table_column->column_name);
				g_array_append_val (source->priv->export_names, tmp);
				g_hash_table_insert (source->priv->export_columns, tmp,
						     GINT_TO_POINTER (i + 1));
#ifdef DEBUG_SOURCE
				g_print ("\tEXPORT [%s]\n", tmp);
#endif
			}
		}
	}

	gda_sql_statement_free (sqlst);
}

/**
 * data_source_set_table
 *
 * @source MUST NOT be executed when calling this method.
 */
gboolean
data_source_set_table (DataSource *source, const gchar *table, GError **error)
{
	g_return_val_if_fail (IS_DATA_SOURCE (source), FALSE);
	g_return_val_if_fail (! data_source_execution_going_on (source), FALSE);

	data_source_reset (source);
	if (!table)
		return FALSE;

	/* locate table */
	GdaMetaTable *mtable;
	mtable = get_meta_table (source, table, error);
	if (!mtable)
		return FALSE;

	source->priv->source_type = DATA_SOURCE_TABLE;
	source->priv->tablename = g_strdup (table);

	if (! strcmp (source->priv->id, DEFAULT_DATA_SOURCE_NAME)) {
		g_free (source->priv->id);
		source->priv->id = g_strdup (table);
	}

	g_free (source->priv->impl_title);
	source->priv->impl_title = g_strdup_printf (_("Contents of '%s'"), table);

	/* build statement */
	GdaSqlBuilder *b;
	gint i;
	GSList *list;

	b = gda_sql_builder_new (GDA_SQL_STATEMENT_SELECT);
	source->priv->builder = b;
	if (! gda_sql_builder_select_add_target (b, table, NULL)) {
		g_set_error (error, T_ERROR, T_INTERNAL_COMMAND_ERROR,
			     "%s", _("Could not build SELECT statement"));
		return FALSE;
	}
	gda_sql_builder_select_set_limit (b,
					  gda_sql_builder_add_expr (b, NULL,
								    G_TYPE_INT, DEFAULT_DATA_SELECT_LIMIT),
					  0);
	for (i = 0, list = mtable->columns; list; i++, list = list->next) {
		GdaMetaTableColumn *mcol;
		mcol = GDA_META_TABLE_COLUMN (list->data);
		gda_sql_builder_select_add_field (b, mcol->column_name, NULL, NULL);

		if (mcol->pkey) {
			/* ORDER BY */
			gda_sql_builder_select_order_by (b,
							 gda_sql_builder_add_id (b,
										 mcol->column_name),
							 FALSE, NULL);
		}
	}

	/* compute statement & parameters */
	compute_stmt_and_params (source);
	/*g_print ("SQL [%s]\n", gda_statement_to_sql (source->priv->stmt, NULL, NULL));*/

	update_export_information (source);

#ifdef DEBUG_SOURCE
	g_print ("\n");
#endif

	g_signal_emit (source, data_source_signals [CHANGED], 0);
	return source->priv->stmt ? TRUE : FALSE;
}

/**
 * data_source_set_query
 *
 * @source MUST NOT be executed when calling this method.
 */
void
data_source_set_query (DataSource *source, const gchar *sql, GError **warning)
{
	g_return_if_fail (IS_DATA_SOURCE (source));
	g_return_if_fail (! data_source_execution_going_on (source));

	data_source_reset (source);

	source->priv->source_type = DATA_SOURCE_SELECT;
	if (!sql) {
		g_signal_emit (source, data_source_signals [CHANGED], 0);
		return;
	}

	GdaSqlParser *parser;
	const gchar *remain;
	parser = t_connection_create_parser (source->priv->tcnc);
	source->priv->stmt = gda_sql_parser_parse_string (parser, sql,
							  &remain, warning);
	g_object_unref (parser);
	if (!source->priv->stmt) {
		g_signal_emit (source, data_source_signals [CHANGED], 0);
		return;
	}

	if (remain)
		g_set_error (warning, T_ERROR, T_COMMAND_ARGUMENTS_ERROR,
			     "%s", _("Multiple statements detected, only the first will be used"));

	/* try to normalize the statement */
	GdaSqlStatement *sqlst;
	g_object_get ((GObject*) source->priv->stmt, "structure", &sqlst, NULL);
	if (t_connection_normalize_sql_statement (source->priv->tcnc, sqlst, NULL))
		g_object_set ((GObject*) source->priv->stmt, "structure", sqlst, NULL);
	gda_sql_statement_free (sqlst);
	
	update_export_information (source);
	
	/* compute parameters */
	source->priv->need_rerun = FALSE;
	compute_import_params (source);

#ifdef DEBUG_SOURCE
	g_print ("\n");
#endif

	g_signal_emit (source, data_source_signals [CHANGED], 0);
}

static void
compute_stmt_and_params (DataSource *source)
{
	g_assert (source->priv->builder);
	if (source->priv->stmt)
		g_object_unref (source->priv->stmt);
	source->priv->stmt = gda_sql_builder_get_statement (source->priv->builder, NULL);
	compute_import_params (source);

#ifdef DEBUG_SOURCE
	gchar *sql;
	sql = gda_statement_to_sql (source->priv->stmt, NULL, NULL);
	g_print ("[%s]\n", sql);
	g_free (sql);
#endif
}

static void
compute_import_params (DataSource *source)
{
	if (source->priv->params) {
		g_signal_handlers_disconnect_by_func (source->priv->params,
						      G_CALLBACK (params_changed_cb), source);
		g_object_unref (source->priv->params);
		source->priv->params = NULL;
	}
	g_clear_error (& source->priv->init_error);

	gda_statement_get_parameters (source->priv->stmt, &source->priv->params,
				      &source->priv->init_error);
	if (source->priv->params) {
		GSList *list;
		for (list = gda_set_get_holders (source->priv->params); list; list = list->next) {
			gda_holder_set_not_null (GDA_HOLDER (list->data), FALSE);
#ifdef DEBUG_SOURCE
			g_print ("\tIMPORT [%s]\n", gda_holder_get_id (GDA_HOLDER (list->data)));
#endif
		}

		t_connection_define_ui_plugins_for_stmt (source->priv->tcnc, source->priv->stmt,
							       source->priv->params);

		g_signal_connect (source->priv->params, "holder-changed",
				  G_CALLBACK (params_changed_cb), source);
	}
}

/**
 * data_source_get_table
 * @source: a #DataSource
 *
 * Returns: the name of the table used by @source, if its type is %DATA_SOURCE_TABLE
 */
const gchar *
data_source_get_table (DataSource *source)
{
	g_return_val_if_fail (IS_DATA_SOURCE (source), NULL);
	return source->priv->tablename;	
}

/**
 * data_source_get_source_type
 */
DataSourceType
data_source_get_source_type (DataSource *source)
{
	g_return_val_if_fail (IS_DATA_SOURCE (source), DATA_SOURCE_UNKNOWN);
	return source->priv->source_type;
}

/**
 * data_source_should_rerun
 *
 * The SELECT statement will be re-executed the next time
 * data_source_execute() is called 
 */
void
data_source_should_rerun (DataSource *source)
{
	g_return_if_fail (IS_DATA_SOURCE (source));
	source->priv->need_rerun = TRUE;
}
