/* GDA client libary
 * Copyright (C) 2001 The Free Software Foundation
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

#include "config.h"
#include "gda-export.h"
#include "gda-util.h"
#ifndef HAVE_GOBJECT
#  include <gtk/gtksignal.h>
#endif

struct _GdaExportPrivate {
	GdaConnection *cnc;
	GHashTable *selected_tables;
};

enum {
	OBJECT_SELECTED,
	OBJECT_UNSELECTED,
	LAST_SIGNAL
};

static gint gda_export_signals[LAST_SIGNAL] = { 0, };

static void gda_export_class_init (GdaExportClass *klass);
static void gda_export_init       (GdaExport *export);
static void gda_export_destroy    (GdaExport *destroy);

/*
 * Private functions
 */

static void
free_hash_entry (gpointer key, gpointer value, gpointer user_data)
{
	g_free (key);
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
get_object_list (GdaConnection *cnc, GDA_Connection_QType qtype)
{
	GdaRecordset *recset;
	gint pos;
	GList *list = NULL;

	g_return_val_if_fail (IS_GDA_CONNECTION (cnc), NULL);

	/* retrieve list of tables from connection */
	recset = gda_connection_open_schema (cnc,
					     qtype,
					     GDA_Connection_no_CONSTRAINT);
	pos = gda_recordset_move (recset, 1, 0);
	while (pos != GDA_RECORDSET_INVALID_POSITION &&
	       !gda_recordset_eof (recset)) {
		GdaField *field;

		field = gda_recordset_field_idx (recset, 0);
		list = g_list_append (list, gda_stringify_value (0, 0, field));

		pos = gda_recordset_move (recset, 1, 0);
	}

	gda_recordset_free (recset);
}

/*
 * GdaExport class implementation
 */

/**
 * gda_export_get_type
 */
GtkType
gda_export_get_type (void)
{
	GtkType type = 0;

	if (!type) {
		GtkTypeInfo info = {
			"GdaExport",
			sizeof (GdaExport),
			sizeof (GdaExportClass),
			(GtkClassInitFunc) gda_export_class_init,
			(GtkObjectInitFunc) gda_export_init,
			(GtkArgSetFunc) NULL,
			(GtkArgSetFunc) NULL,
		};
		type = gtk_type_unique (gtk_object_get_type (), &info);
	}
	return type;
}

static void
gda_export_class_init (GdaExportClass *klass)
{
	GtkObjectClass *object_class = GTK_OBJECT_CLASS (klass);

	gda_export_signals[OBJECT_SELECTED] =
		gtk_signal_new ("object_selected",
				GTK_RUN_FIRST,
				object_class->type,
				GTK_SIGNAL_OFFSET (GdaExportClass, object_selected),
				gtk_marshal_NONE__INT_POINTER,
				GTK_TYPE_NONE, 2,
				GTK_TYPE_INT, GTK_TYPE_STRING);
	gda_export_signals[OBJECT_UNSELECTED] =
		gtk_signal_new ("object_unselected",
				GTK_RUN_FIRST,
				object_class->type,
				GTK_SIGNAL_OFFSET (GdaExportClass, object_unselected),
				gtk_marshal_NONE__INT_POINTER,
				GTK_TYPE_NONE, 2,
				GTK_TYPE_INT, GTK_TYPE_STRING);
	gtk_object_class_add_signals (object_class, gda_export_signals, LAST_SIGNAL);

	object_class->destroy = gda_export_destroy;
}

static void
gda_export_init (GdaExport *export)
{
	export->priv = g_new0 (GdaExportPrivate, 1);
}

static void
gda_export_destroy (GdaExport *export)
{
	GtkObjectClass *parent_class;

	destroy_hash_table (&export->priv->selected_tables);

	g_free (export->priv);

	parent_class = gtk_type_class (gtk_object_get_type ());
	if (parent_class && parent_class->destroy)
		parent_class->destroy (GTK_OBJECT (export));
}

/**
 * gda_export_new
 * @cnc: a #GdaConnection object
 *
 * Create a new #GdaExport object, which allows you to easily add
 * exporting functionality to your programs.
 *
 * It works by first having a #GdaConnection object associated
 * to it, and then allowing you to retrieve information about all
 * the objects present in the database, and also to add/remove
 * those objects from a list of selected objects.
 *
 * When you're done, you just run the export (#gda_export_run), first
 * connecting to the different signals that will let you be
 * informed of the export process progress
 *
 * Returns: a newly allocated #GdaExport object
 */
GdaExport *
gda_export_new (GdaConnection *cnc)
{
	GdaExport *export;

	export = GDA_EXPORT (gtk_type_new (gda_export_get_type ()));
	if (IS_GDA_CONNECTION (cnc))
		gda_export_set_connection (export, cnc);

	return export;
}

/**
 * gda_export_get_tables
 * @cnc: a #GdaConnection object
 *
 * Return a list of all tables that exist in the #GdaConnection
 * being used. This function is useful when you're building,
 * for example, a list for the user to select which tables he/she
 * wants in the export process.
 *
 * You are responsible to free the returned value yourself.
 *
 * Returns: a GList containing the names of all the tables
 */
GList *
gda_export_get_tables (GdaExport *export)
{
	g_return_val_if_fail (IS_GDA_EXPORT (export), NULL);
	g_return_val_if_fail (export->priv != NULL, NULL);

	return get_object_list (export->priv->cnc, GDA_Connection_GDCN_SCHEMA_TABLES);
}

/**
 * gda_export_get_selected_tables
 * @export: a #GdaExport object
 *
 * Return a list with the names of all the currently selected objects
 * in the given #GdaExport object.
 *
 * You are responsible to free the returned value yourself.
 *
 * Returns: a #GList containing the names of the selected tables
 */
GList *
gda_export_get_selected_tables (GdaExport *export)
{
	g_return_val_if_fail (IS_GDA_EXPORT (export), NULL);
	g_return_val_if_fail (export->priv != NULL, NULL);

	return gda_util_hash_to_list (export->priv->selected_tables);
}

/**
 * gda_export_select_table
 * @export: a #GdaExport object
 * @table: name of the table
 *
 * Add the given table to the list of selected tables
 */
void
gda_export_select_table (GdaExport *export, const gchar *table)
{
	gchar *data;

	g_return_if_fail (IS_GDA_EXPORT (export));
	g_return_if_fail (table != NULL);

	data = g_hash_table_lookup (export->priv->selected_tables, (gconstpointer) table);
	if (!data) {
		data = (gpointer) g_strdup (table);

		g_hash_table_insert (export->priv->selected_tables, data, data);
		gtk_signal_emit (GTK_OBJECT (export),
				 gda_export_signals[OBJECT_SELECTED],
				 GDA_Connection_GDCN_SCHEMA_TABLES,
				 table);
	}
}

/**
 * gda_export_unselect_table
 * @export: a #GdaExport object
 * @table: name of the table
 *
 * Remove the given table name from the list of selected tables
 */
void
gda_export_unselect_table (GdaExport *export, const gchar *table)
{
	gchar *data;

	g_return_if_fail (IS_GDA_EXPORT (export));
	g_return_if_fail (table != NULL);

	data = g_hash_table_lookup (export->priv->selected_tables, (gconstpointer) table);
	if (data) {
		g_hash_table_remove (export->priv->selected_tables, table);
		gtk_signal_emit (GTK_OBJECT (export),
				 gda_export_signals[OBJECT_UNSELECTED],
				 GDA_Connection_GDCN_SCHEMA_TABLES,
				 table);
	}
}

/**
 * gda_export_get_connection
 * @export: a #GdaExport object
 *
 * Return the #GdaConnection object associated with the given
 * #GdaExport
 *
 * Returns: the #GdaConnection object being used
 */
GdaConnection *
gda_export_get_connection (GdaExport *export)
{
	g_return_val_if_fail (IS_GDA_EXPORT (export), NULL);
	return export->priv->cnc;
}

/**
 * gda_export_set_connection
 */
void
gda_export_set_connection (GdaExport *export, GdaConnection *cnc)
{
	g_return_if_fail (IS_GDA_EXPORT (export));

	/* unref the old GdaConnection */
	if (IS_GDA_CONNECTION (export->priv->cnc)) {
		gda_connection_free (export->priv->cnc);
		export->priv->cnc = NULL;
	}

	if (IS_GDA_CONNECTION (cnc)) {
		export->priv->cnc = cnc;
		gtk_object_ref (GTK_OBJECT (export->priv->cnc));
	}
}
