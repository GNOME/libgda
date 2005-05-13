/* GDA client libary
 * Copyright (C) 1998-2002 The GNOME Foundation.
 *
 * AUTHORS:
 *	Rodrigo Moya <rodrigo@gnome-db.org>
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

#include <libgda/gda-export.h>
#include <libgda/gda-util.h>

struct _GdaExportPrivate {
	GdaConnection *cnc;
	GHashTable *selected_tables;

	gboolean running;
	GdaExportFlags tmp_flags;
	GList *tmp_tables;
	GdaXmlDatabase *tmp_xmldb;
};

static void gda_export_class_init (GdaExportClass *klass);
static void gda_export_init       (GdaExport *exp, GdaExportClass *klass);
static void gda_export_finalize   (GObject *object);

enum {
	OBJECT_SELECTED,
	OBJECT_UNSELECTED,
	FINISHED,
	CANCELLED,
	LAST_SIGNAL
};

static gint gda_export_signals[LAST_SIGNAL];
static GObjectClass *parent_class = NULL;

/*
 * Private functions
 */

static void
free_hash_entry (gpointer key, gpointer value, gpointer user_data)
{
	g_free (key);
	/* g_free (value); */
}

static void
destroy_hash_table (GHashTable **hash_table)
{
	g_return_if_fail (*hash_table != NULL);

	g_hash_table_foreach (*hash_table, (GHFunc) free_hash_entry, NULL);
	g_hash_table_destroy (*hash_table);
	*hash_table = NULL;
}

static GList *
get_object_list (GdaConnection *cnc, GdaConnectionSchema schema)
{
	GdaDataModel *recset;
	gint pos;
	GList *list = NULL;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	/* retrieve list of tables from connection */
	recset = gda_connection_get_schema (cnc, schema, NULL);

	for (pos = 0; pos < gda_data_model_get_n_rows (recset); pos++) {
		GdaValue *value;
		gchar *str;

		value = (GdaValue *) gda_data_model_get_value_at (recset, 0, pos);
		str = gda_value_stringify (value);
		list = g_list_append (list, str);
	}

	g_object_unref (G_OBJECT (recset));

	return list;
}

/*
 * Callbacks
 */
static gboolean
run_export_cb (gpointer user_data)
{
	GList *item;
	gint num_found = 0;
	GdaCommand *cmd;
	GdaDataModel *recset;
	GdaExport *exp = (GdaExport *) user_data;

	g_return_val_if_fail (GDA_IS_EXPORT (exp), FALSE);

	/* see if we've got tables to export */
	item = g_list_first (exp->priv->tmp_tables);
	if (item) {
		GdaTable *table;
		gchar *name = (gchar *) item->data;

		cmd = gda_command_new (name, GDA_COMMAND_TYPE_TABLE, 0);

		recset = gda_connection_execute_single_command (exp->priv->cnc, cmd, NULL);
		gda_command_free (cmd);
		if (!GDA_IS_DATA_MODEL (recset)) {
			gda_export_stop (exp);
			return FALSE;
		}

		/* add the table to the database */
		table = gda_xml_database_new_table_from_model (
			exp->priv->tmp_xmldb,
			(const gchar *) name,
			(const GdaDataModel *) recset,
			exp->priv->tmp_flags & GDA_EXPORT_FLAGS_TABLE_DATA ? TRUE : FALSE);
		if (!GDA_IS_TABLE (table)) {
			gda_export_stop (exp);
			return FALSE;
		}

		/* free memory */
		g_object_unref (G_OBJECT (recset));

		exp->priv->tmp_tables = g_list_remove (exp->priv->tmp_tables, name);
		g_free (name);

		num_found++;
	}

	/* if we didn't treat any object, we're finished */
	if (!num_found) {
		g_signal_emit (G_OBJECT (exp),
			       gda_export_signals[FINISHED],
			       0, exp->priv->tmp_xmldb);
		g_object_unref (G_OBJECT (exp->priv->tmp_xmldb));
		exp->priv->tmp_xmldb = NULL;

		return FALSE;
	}

	return TRUE;
}

/*
 * GdaExport class implementation
 */

GType
gda_export_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (GdaExportClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_export_class_init,
			NULL,
			NULL,
			sizeof (GdaExport),
			0,
			(GInstanceInitFunc) gda_export_init
		};
		type = g_type_register_static (G_TYPE_OBJECT, "GdaExport", &info, 0);
	}
	return type;
}

static void
gda_export_class_init (GdaExportClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = gda_export_finalize;

	/* create class signals */
	gda_export_signals[OBJECT_SELECTED] =
		g_signal_new ("object_selected",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GdaExportClass, object_selected),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__UINT_POINTER,
			      G_TYPE_NONE, 2,
			      G_TYPE_INT, G_TYPE_STRING);
	gda_export_signals[OBJECT_UNSELECTED] =
		g_signal_new ("object_unselected",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GdaExportClass, object_unselected),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__UINT_POINTER,
			      G_TYPE_NONE, 2,
			      G_TYPE_INT, G_TYPE_STRING);
	gda_export_signals[CANCELLED] =
		g_signal_new ("cancelled",
				G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GdaExportClass, cancelled),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);
	gda_export_signals[FINISHED] =
		g_signal_new ("finished",
				G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GdaExportClass, finished),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__POINTER,
			      G_TYPE_NONE, 1,
			      G_TYPE_POINTER);
}

static void
gda_export_init (GdaExport *exp, GdaExportClass *klass)
{
	g_return_if_fail (GDA_IS_EXPORT (exp));

	exp->priv = g_new0 (GdaExportPrivate, 1);
	exp->priv->selected_tables = g_hash_table_new (g_str_hash, g_str_equal);
}

static void
gda_export_finalize (GObject *object)
{
	GdaExport *exp = (GdaExport *) object;

	g_return_if_fail (GDA_IS_EXPORT (exp));

	destroy_hash_table (&exp->priv->selected_tables);
	if (GDA_IS_CONNECTION (exp->priv->cnc)) {
		g_object_unref (G_OBJECT (exp->priv->cnc));
		exp->priv->cnc = NULL;
	}

	g_free (exp->priv);
	exp->priv = NULL;

	parent_class->finalize (object);
}

/**
 * gda_export_new
 * @cnc: a #GdaConnection object.
 *
 * Creates a new #GdaExport object, which allows you to easily add
 * exporting functionality to your programs.
 *
 * It works by first having a #GdaConnection object associated
 * to it, and then allowing you to retrieve information about all
 * the objects present in the database, and also to add/remove
 * those objects from a list of selected objects.
 *
 * When you're done, you just run the export (#gda_export_run), first
 * connecting to the different signals that will let you be
 * informed of the export process progress.
 *
 * Returns: a newly allocated #GdaExport object.
 */
GdaExport *
gda_export_new (GdaConnection * cnc)
{
	GdaExport *exp;

	exp = g_object_new (GDA_TYPE_EXPORT, NULL);
	if (GDA_IS_CONNECTION (cnc))
		gda_export_set_connection (exp, cnc);

	return exp;
}

/**
 * gda_export_get_tables
 * @exp: a #GdaExport object.
 *
 * Returns a list of all tables that exist in the #GdaConnection
 * being used by the given #GdaExport object. This function is 
 * useful when you're building, for example, a list for the user 
 * to select which tables he/she wants in the export process.
 *
 * You are responsible to free the returned value yourself.
 *
 * Returns: a GList containing the names of all the tables.
 */
GList *
gda_export_get_tables (GdaExport * exp)
{
	g_return_val_if_fail (GDA_IS_EXPORT (exp), NULL);
	g_return_val_if_fail (exp->priv != NULL, NULL);

	return get_object_list (exp->priv->cnc, GDA_CONNECTION_SCHEMA_TABLES);
}

/**
 * gda_export_get_selected_tables
 * @exp: a #GdaExport object.
 *
 * Returns a list with the names of all the currently selected objects
 * in the given #GdaExport object.
 *
 * You are responsible to free the returned value yourself.
 *
 * Returns: a #GList containing the names of the selected tables.
 */
GList *
gda_export_get_selected_tables (GdaExport * exp)
{
	g_return_val_if_fail (GDA_IS_EXPORT (exp), NULL);
	g_return_val_if_fail (exp->priv != NULL, NULL);

	return gda_string_hash_to_list (exp->priv->selected_tables);
}

/**
 * gda_export_select_table
 * @exp: a #GdaExport object.
 * @table: name of the table.
 *
 * Adds the given @table to the list of selected tables.
 */
void
gda_export_select_table (GdaExport *exp, const gchar *table)
{
	gchar *data;

	g_return_if_fail (GDA_IS_EXPORT (exp));
	g_return_if_fail (table != NULL);

	data = g_hash_table_lookup (exp->priv->selected_tables,
				    (gconstpointer) table);
	if (!data) {
		data = (gpointer) g_strdup (table);

		g_hash_table_insert (exp->priv->selected_tables, data, data);
		g_signal_emit (G_OBJECT (exp),
			       gda_export_signals[OBJECT_SELECTED], 0,
			       GDA_CONNECTION_SCHEMA_TABLES,
			       table);
	}
}

/**
 * gda_export_select_table_list
 * @exp: a #GdaExport object.
 * @list: list of tables to be selected.
 *
 * Adds all the tables contained in the given @list to the list of
 * selected tables.
 */
void
gda_export_select_table_list (GdaExport *exp, GList *list)
{
	GList *l;

	g_return_if_fail (GDA_IS_EXPORT (exp));
	g_return_if_fail (list != NULL);

	for (l = g_list_first (list); l != NULL; l = g_list_next (l))
		gda_export_select_table (exp, (const gchar *) l->data);
}

/**
 * gda_export_unselect_table
 * @exp: a #GdaExport object.
 * @table: name of the table.
 *
 * Removes the given table name from the list of selected tables.
 */
void
gda_export_unselect_table (GdaExport *exp, const gchar *table)
{
	gchar *data;

	g_return_if_fail (GDA_IS_EXPORT (exp));
	g_return_if_fail (table != NULL);

	data = g_hash_table_lookup (exp->priv->selected_tables,
				    (gconstpointer) table);
	if (data) {
		g_hash_table_remove (exp->priv->selected_tables, table);
		g_free (data);
		g_signal_emit (G_OBJECT (exp),
			       gda_export_signals[OBJECT_UNSELECTED], 0,
			       GDA_CONNECTION_SCHEMA_TABLES,
			       table);
	}
}

/**
 * gda_export_run
 * @exp: a #GdaExport object.
 * @flags: execution flags.
 *
 * Starts the execution of the given export object. This means that, after
 * calling this function, your application will lose control about the export
 * process and will only receive notifications via the class signals.
 */
void
gda_export_run (GdaExport *exp, GdaExportFlags flags)
{
	g_return_if_fail (GDA_IS_EXPORT (exp));
	g_return_if_fail (exp->priv->running == FALSE);
	g_return_if_fail (gda_connection_is_open (exp->priv->cnc));

	exp->priv->running = TRUE;
	exp->priv->tmp_flags = flags;
	exp->priv->tmp_tables = gda_string_hash_to_list (exp->priv->selected_tables);
	exp->priv->tmp_xmldb = gda_xml_database_new ();

	g_idle_add ((GSourceFunc) run_export_cb, exp);
}

/**
 * gda_export_stop
 * @exp: a #GdaExport object.
 *
 * Stops execution of the given export object.
 */
void
gda_export_stop (GdaExport *exp)
{
	g_return_if_fail (GDA_IS_EXPORT (exp));

	exp->priv->running = FALSE;
	if (exp->priv->tmp_tables != NULL) {
		g_list_foreach (exp->priv->tmp_tables, (GFunc) g_free, NULL);
		g_list_free (exp->priv->tmp_tables);
		exp->priv->tmp_tables = NULL;
	}

	g_object_unref (G_OBJECT (exp->priv->tmp_xmldb));
	exp->priv->tmp_xmldb = NULL;
	g_idle_remove_by_data (exp);

	g_signal_emit (G_OBJECT (exp), gda_export_signals[CANCELLED], 0);
}

/**
 * gda_export_get_connection
 * @exp: a #GdaExport object.
 *
 * Returns: the #GdaConnection object associated with the given #GdaExport.
 */
GdaConnection *
gda_export_get_connection (GdaExport * exp)
{
	g_return_val_if_fail (GDA_IS_EXPORT (exp), NULL);
	return exp->priv->cnc;
}

/**
 * gda_export_set_connection
 * @exp: a #GdaExport object.
 * @cnc: a #GdaConnection object.
 * 
 * Associates the given #GdaConnection with the given #GdaExport. 
 */
void
gda_export_set_connection (GdaExport *exp, GdaConnection * cnc)
{
	g_return_if_fail (GDA_IS_EXPORT (exp));

	/* unref the old GdaConnection */
	if (GDA_IS_CONNECTION (exp->priv->cnc)) {
		g_object_unref (G_OBJECT (exp->priv->cnc));
		exp->priv->cnc = NULL;
	}

	destroy_hash_table (&exp->priv->selected_tables);
	exp->priv->selected_tables = g_hash_table_new (g_str_hash, g_str_equal);

	if (GDA_IS_CONNECTION (cnc)) {
		exp->priv->cnc = cnc;
		g_object_ref (G_OBJECT (exp->priv->cnc));
	}
}
