/*
 * Copyright (C) 2011 - 2014 Vivien Malerba <malerba@gnome-db.org>
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

#include <glib/gi18n-lib.h>
#include "analyser.h"
#include "../browser-window.h"

static gchar *compute_fk_dependency (GdaMetaTableForeignKey *fkey, GSList *selfields, gboolean reverse,
				     DataSource *source, xmlNodePtr *out_sourcespec);

static void add_data_source_mitem_activated_cb (GtkMenuItem *mitem, DataSourceManager *mgr);

GSList *
data_manager_add_proposals_to_menu (GtkWidget *menu,
				    DataSourceManager *mgr, DataSource *source,
				    GtkWidget *error_attach_widget)
{
	g_return_val_if_fail (GTK_IS_MENU (menu), NULL);
	g_return_val_if_fail (IS_DATA_SOURCE (source), NULL);
	g_return_val_if_fail (GTK_IS_WIDGET (error_attach_widget), NULL);

	GtkWidget *mitem;
	gboolean add_separator = TRUE;
	GSList *added_list = NULL;

	GdaStatement *stmt;
	stmt = data_source_get_statement (source);
	if (gda_statement_get_statement_type (stmt) == GDA_SQL_STATEMENT_SELECT) {
		GSList *fields = NULL, *flist;
		GdaSqlStatement *sql_statement;
		TConnection *tcnc;
		GHashTable *hash; /* key = a menu string, value= 0x1 */
		
		g_object_get (G_OBJECT (stmt), "structure", &sql_statement, NULL);
		
		tcnc = data_source_manager_get_browser_cnc (mgr);
		if (t_connection_check_sql_statement_validify (tcnc, sql_statement, NULL))
			fields = ((GdaSqlStatementSelect *) sql_statement->contents)->expr_list;
		
		hash = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
		for (flist = fields; flist; flist = flist->next) {
			GdaSqlSelectField *select_field = (GdaSqlSelectField*) flist->data;
			if (!select_field->validity_meta_table_column)
				continue;
			GdaMetaDbObject *dbo;
			dbo = select_field->validity_meta_object;
			if (dbo->obj_type == GDA_META_DB_TABLE) {
				GdaMetaTable *mtable;
				mtable = GDA_META_TABLE (dbo);
				
				GSList *fklist;
				for (fklist = mtable->reverse_fk_list; fklist; fklist = fklist->next) {
					GdaMetaTableForeignKey *fkey;
					fkey = (GdaMetaTableForeignKey *) fklist->data;
					gchar *tmp;
					xmlNodePtr sourcespec = NULL;
					if (fkey->meta_table->obj_type != GDA_META_DB_TABLE)
						continue;
					tmp = compute_fk_dependency (fkey, fields, FALSE,
								     source, &sourcespec);
					if (!tmp)
						continue;
					if (g_hash_table_lookup (hash, tmp)) {
						g_free (tmp);
						continue;
					}
					
					if (add_separator) {
						mitem = gtk_separator_menu_item_new ();
						gtk_widget_show (mitem);
						gtk_menu_shell_append (GTK_MENU_SHELL (menu), mitem);
						add_separator = FALSE;
					}
					
					mitem = gtk_menu_item_new_with_label (tmp);
					added_list = g_slist_prepend (added_list, mitem);
					g_object_set_data_full ((GObject*) mitem, "xml", sourcespec,
								(GDestroyNotify) xmlFreeNode);
					g_object_set_data_full (G_OBJECT (mitem), "attachwidget",
								g_object_ref (error_attach_widget),
								g_object_unref);
					g_signal_connect (mitem, "activate",
							  G_CALLBACK (add_data_source_mitem_activated_cb),
							  mgr);
					gtk_widget_show (mitem);
					gtk_menu_shell_append (GTK_MENU_SHELL (menu), mitem);
					g_hash_table_insert (hash, tmp, (gpointer) 0x1);
				}
				for (fklist = mtable->fk_list; fklist; fklist = fklist->next) {
					GdaMetaTableForeignKey *fkey;
					fkey = (GdaMetaTableForeignKey *) fklist->data;
					gchar *tmp;
					xmlNodePtr sourcespec = NULL;
					if (fkey->depend_on->obj_type != GDA_META_DB_TABLE)
						continue;
					tmp = compute_fk_dependency (fkey, fields, TRUE,
								     source, &sourcespec);
					if (!tmp)
						continue;
					if (g_hash_table_lookup (hash, tmp)) {
						g_free (tmp);
						continue;
					}
					
					if (add_separator) {
						mitem = gtk_separator_menu_item_new ();
						gtk_widget_show (mitem);
						gtk_menu_shell_append (GTK_MENU_SHELL (menu), mitem);
						add_separator = FALSE;
					}
					
					mitem = gtk_menu_item_new_with_label (tmp);
					added_list = g_slist_prepend (added_list, mitem);
					g_object_set_data_full ((GObject*) mitem, "xml", sourcespec,
								(GDestroyNotify) xmlFreeNode);
					g_object_set_data_full (G_OBJECT (mitem), "attachwidget",
								g_object_ref (error_attach_widget),
								g_object_unref);
					g_signal_connect (mitem, "activate",
							  G_CALLBACK (add_data_source_mitem_activated_cb),
							  mgr);
					gtk_widget_show (mitem);
					gtk_menu_shell_append (GTK_MENU_SHELL (menu), mitem);
					g_hash_table_insert (hash, tmp, (gpointer) 0x1);
				}
			}
		}
		g_hash_table_destroy (hash);
		gda_sql_statement_free (sql_statement);
	}

	return added_list;
}

/*
 * Returns: a new list of #GdaSqlSelectField (which are already listed in @selfields), or %NULL
 */
static gchar *
compute_fk_dependency (GdaMetaTableForeignKey *fkey, GSList *selfields, gboolean reverse,
		       DataSource *source, xmlNodePtr *out_sourcespec)
{
	GString *string = NULL;
	gint i;
	gint *index_array;
	GdaMetaTable *table;

	*out_sourcespec = NULL;

	if (reverse) {
		table = GDA_META_TABLE (fkey->meta_table);
		index_array = fkey->fk_cols_array;
	}
	else {
		table = GDA_META_TABLE (fkey->depend_on);
		index_array = fkey->ref_pk_cols_array;
	}
	for (i = 0; i < fkey->cols_nb; i++) {
		gint pos;
		GdaMetaTableColumn *col;
		pos = index_array[i] - 1;
		col = g_slist_nth_data (table->columns, pos);

		/* make sure @col is among @selfields */
		GSList *flist;
		gboolean found = FALSE;
		for (flist = selfields; flist; flist = flist->next) {
			GdaSqlSelectField *select_field = (GdaSqlSelectField*) flist->data;
			if (!select_field->validity_meta_object ||
			    !select_field->validity_meta_table_column)
				continue;
			GdaMetaTableColumn *field;
			field = select_field->validity_meta_table_column;
			if (field == col) {
				found = TRUE;
				if (reverse) {
					if (!string) {
						string = g_string_new ("");
						g_string_printf (string, _("Obtain referenced data in table %s from "),
									fkey->depend_on->obj_short_name);
					}
					else
						g_string_append (string, ", ");
					g_string_append_printf (string, _("column number %d ("),
								g_slist_position (selfields, flist) + 1);
					if (select_field->as)
						g_string_append (string, select_field->as);
					else
						g_string_append (string,
								 select_field->validity_meta_table_column->column_name);
					g_string_append_c (string, ')');
				}
				else {
					if (!string) {
						string = g_string_new ("");
						g_string_printf (string, _("List referencing data in %s."),
									fkey->meta_table->obj_short_name);
					}
					else
						g_string_append (string, ", ");
					g_string_append (string, fkey->fk_names_array [i]);
				}
				break;
			}
		}

		if (! found) {
			if (string) {
				g_string_free (string, TRUE);
				string = NULL;
			}
			break;
		}
	}

	if (string) {
		xmlNodePtr node, snode;
		node = xmlNewNode (NULL, BAD_CAST "table");
		xmlSetProp (node, BAD_CAST "name",
 			    BAD_CAST (reverse ? GDA_META_DB_OBJECT (fkey->depend_on)->obj_short_name : 
				      GDA_META_DB_OBJECT (fkey->meta_table)->obj_short_name));

		snode = xmlNewChild (node, NULL, BAD_CAST "depend", NULL);
		xmlSetProp (snode, BAD_CAST "foreign_key_table",
			    BAD_CAST GDA_META_DB_OBJECT (table)->obj_short_name);

		xmlSetProp (snode, BAD_CAST "id", BAD_CAST data_source_get_id (source));

		gint i;
		for (i = 0; i < fkey->cols_nb; i++)
			xmlNewChild (snode, NULL, BAD_CAST "column", BAD_CAST (fkey->fk_names_array[i]));

		*out_sourcespec = node;
		return g_string_free (string, FALSE);
	}
	else
		return NULL;
}

static void
add_data_source_mitem_activated_cb (GtkMenuItem *mitem, DataSourceManager *mgr)
{
	DataSource *source;
	GError *lerror = NULL;
	xmlNodePtr sourcespec;
	TConnection *tcnc;

	tcnc = data_source_manager_get_browser_cnc (mgr);
	sourcespec = (xmlNodePtr) g_object_get_data ((GObject*) mitem, "xml");
	source = data_source_new_from_xml_node (tcnc, sourcespec, &lerror);
	if (source) {
		data_source_manager_add_source (mgr, source);
		g_object_unref (source);
	}
	else {
		BrowserWindow *bwin;
		GtkWidget *attachwidget;
		attachwidget = g_object_get_data (G_OBJECT (mitem), "attachwidget");
		g_assert (attachwidget);
		bwin = BROWSER_WINDOW (gtk_widget_get_toplevel (attachwidget));
		browser_window_show_notice_printf (bwin, GTK_MESSAGE_ERROR, "data-widget-add-new-source",
						   _("Error adding new data source: %s"),
						   lerror && lerror->message ? lerror->message :
						   _("No detail"));
		g_clear_error (&lerror);
	}

#ifdef GDA_DEBUG_NO
	xmlBufferPtr buffer;
	buffer = xmlBufferCreate ();
	xmlNodeDump (buffer, NULL, sourcespec, 0, 1);
	g_print ("Source to ADD: [%s]\n", (gchar *) xmlBufferContent (buffer));
	xmlBufferFree (buffer);
#endif
}
