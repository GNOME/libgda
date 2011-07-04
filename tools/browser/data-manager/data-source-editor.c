/*
 * Copyright (C) 2010 David King <davidk@openismus.com>
 * Copyright (C) 2010 - 2011 Vivien Malerba <malerba@gnome-db.org>
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
#include "data-source-editor.h"

/* signals */
enum {
	CHANGED,
	LAST_SIGNAL
};

gint data_source_editor_signals [LAST_SIGNAL] = { 0 };

/* 
 * Main static functions 
 */
static void data_source_editor_class_init (DataSourceEditorClass *klass);
static void data_source_editor_init (DataSourceEditor *editor);
static void data_source_editor_dispose (GObject *object);

static void attribute_changed_cb (GdaSet *set, GdaHolder *holder, DataSourceEditor *editor);

/* get a pointer to the parents to be able to call their destructor */
static GObjectClass  *parent_class = NULL;

struct _DataSourceEditorPrivate {
	DataSource *source;
        GdaSet *attributes;
	GdauiBasicForm *form;
	GtkTextBuffer *tbuffer;
};

GType
data_source_editor_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static GStaticMutex registering = G_STATIC_MUTEX_INIT;
		static const GTypeInfo info = {
			sizeof (DataSourceEditorClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) data_source_editor_class_init,
			NULL,
			NULL,
			sizeof (DataSourceEditor),
			0,
			(GInstanceInitFunc) data_source_editor_init,
			0
		};

		
		g_static_mutex_lock (&registering);
		if (type == 0)
			type = g_type_register_static (GTK_TYPE_VBOX, "DataSourceEditor", &info, 0);
		g_static_mutex_unlock (&registering);
	}
	return type;
}

static void
data_source_editor_class_init (DataSourceEditorClass *klass)
{
	GObjectClass   *object_class = G_OBJECT_CLASS (klass);
	parent_class = g_type_class_peek_parent (klass);

	/* signals */
	/*data_source_editor_signals [CHANGED] =
                g_signal_new ("changed",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_FIRST,
                              G_STRUCT_OFFSET (DataSourceEditorClass, changed),
                              NULL, NULL,
                              _dm_marshal_VOID__VOID, G_TYPE_NONE, 0);*/

	object_class->dispose = data_source_editor_dispose;
}

static void
data_source_editor_init (DataSourceEditor *editor)
{
	GtkWidget *vpaned;
	editor->priv = g_new0 (DataSourceEditorPrivate, 1);
	editor->priv->attributes = gda_set_new_inline (4,
						       "id", G_TYPE_STRING, "",
						       "descr", G_TYPE_STRING, "",
						       "table", G_TYPE_STRING, "",
						       "sql", G_TYPE_STRING, "");
	g_signal_connect (editor->priv->attributes, "holder-changed",
			  G_CALLBACK (attribute_changed_cb), editor);

	vpaned = gtk_vpaned_new ();
	gtk_box_pack_start (GTK_BOX (editor), vpaned, TRUE, TRUE, 0);
	gtk_widget_show (vpaned);

	GtkWidget *form;
	form = gdaui_basic_form_new (editor->priv->attributes);
	editor->priv->form = GDAUI_BASIC_FORM (form);
	gtk_paned_add1 (GTK_PANED (vpaned), form);
	gtk_widget_show (form);

	GdaHolder *holder;
	GValue *value;
	holder = gda_set_get_holder (editor->priv->attributes, "id");
	g_object_set ((GObject*) holder, "name", _("Id"),
		      "description",
		      _("Data source's ID\n"
			"(as referenced by other data sources)"), NULL);
	gdaui_basic_form_entry_set_editable (GDAUI_BASIC_FORM (form), holder, FALSE);

	holder = gda_set_get_holder (editor->priv->attributes, "descr");
	g_object_set ((GObject*) holder, "name", _("Description"),
		      "description", _("Data source's description"), NULL);

	holder = gda_set_get_holder (editor->priv->attributes, "table");
	g_object_set ((GObject*) holder, "name", _("Table"),
		      "description", _("Table to display data from, leave empty\n"
				       "to specify a SELECT statement instead"), NULL);

	holder = gda_set_get_holder (editor->priv->attributes, "sql");
	g_object_set ((GObject*) holder, "name", _("SELECT\nSQL"),
		      "description", _("Actual SQL executed\nto select data\n"
				       "Can't be changed if a table name is set"), NULL);
	value = gda_value_new_from_string ("text:PROG_LANG=gda-sql", G_TYPE_STRING);
        gda_holder_set_attribute_static (holder, GDAUI_ATTRIBUTE_PLUGIN, value);
        gda_value_free (value);

#define SPACING 3
	GtkWidget *hbox, *label, *sw, *text;
	GtkSizeGroup *sg;
	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, SPACING);
	gtk_paned_add2 (GTK_PANED (vpaned), hbox);

	label = gtk_label_new (_("Dependencies:"));
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

	sg = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
	gtk_size_group_add_widget (sg, label);
	gdaui_basic_form_add_to_size_group (GDAUI_BASIC_FORM (form), sg, GDAUI_BASIC_FORM_LABELS);
	g_object_unref ((GObject*) sg);

	sw = gtk_scrolled_window_new (NULL, NULL);
        gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw), GTK_POLICY_AUTOMATIC,
                                        GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw), GTK_SHADOW_NONE);
        gtk_box_pack_start (GTK_BOX (hbox), sw, TRUE, TRUE, 0);

	editor->priv->tbuffer = gtk_text_buffer_new (NULL);
	text = gtk_text_view_new_with_buffer (editor->priv->tbuffer);
	sg = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
	gtk_size_group_add_widget (sg, sw);
	gdaui_basic_form_add_to_size_group (GDAUI_BASIC_FORM (form), sg, GDAUI_BASIC_FORM_ENTRIES);
	g_object_unref ((GObject*) sg);

	gtk_container_add (GTK_CONTAINER (sw), text);
	gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (text), GTK_WRAP_WORD);
        gtk_text_view_set_editable (GTK_TEXT_VIEW (text), FALSE);
        gtk_text_view_set_cursor_visible (GTK_TEXT_VIEW (text), FALSE);
	gtk_text_buffer_create_tag (editor->priv->tbuffer, "section",
                                    "weight", PANGO_WEIGHT_BOLD, NULL);

	gtk_widget_show_all (hbox);
}

static void
data_source_editor_dispose (GObject *object)
{
	DataSourceEditor *editor;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_DATA_SOURCE_EDITOR (object));

	editor = DATA_SOURCE_EDITOR (object);
	if (editor->priv) {
		if (editor->priv->tbuffer)
			g_object_unref ((GObject*) editor->priv->tbuffer);
		if (editor->priv->source)
			g_object_unref (editor->priv->source);
		if (editor->priv->attributes) {
			g_signal_handlers_disconnect_by_func (editor->priv->attributes,
							      G_CALLBACK (attribute_changed_cb), editor);

			g_object_unref (editor->priv->attributes);
		}

		g_free (editor->priv);
		editor->priv = NULL;
	}

	/* parent class */
	parent_class->dispose (object);
}

/**
 * data_source_editor_new
 *
 * Creates a new #DataSourceEditor widget
 *
 * Returns: a new widget
 */
GtkWidget *
data_source_editor_new (void)
{
	DataSourceEditor *editor;

	editor = DATA_SOURCE_EDITOR (g_object_new (DATA_SOURCE_EDITOR_TYPE, NULL));

	return (GtkWidget*) editor;
}

static void
update_dependencies_display (DataSourceEditor *editor)
{
	GtkTextIter start, end;
	GtkTextBuffer *tbuffer;

	tbuffer = editor->priv->tbuffer;
        gtk_text_buffer_get_start_iter (tbuffer, &start);
        gtk_text_buffer_get_end_iter (tbuffer, &end);
        gtk_text_buffer_delete (tbuffer, &start, &end);
	gtk_text_buffer_get_start_iter (tbuffer, &start);

	if (editor->priv->source) {
		GdaSet *import;
		import = data_source_get_import ( editor->priv->source);
		gtk_text_buffer_insert_with_tags_by_name (tbuffer, &start,
							  _("Requires:"), -1,
							  "section", NULL);
		gtk_text_buffer_insert (tbuffer, &start, "\n", -1);
		if (import && import->holders) {
			GSList *list;
			for (list = import->holders; list; list = list->next) {
				gtk_text_buffer_insert (tbuffer, &start,
							gda_holder_get_id (GDA_HOLDER (list->data)), -1);
				gtk_text_buffer_insert (tbuffer, &start, "\n", -1);
			}
		}
		else 
			gtk_text_buffer_insert (tbuffer, &start, "--\n", -1);

		GArray *export;
		export = data_source_get_export_names ( editor->priv->source);
		gtk_text_buffer_insert_with_tags_by_name (tbuffer, &start,
							  _("Exports:"), -1,
							  "section", NULL);
		gtk_text_buffer_insert (tbuffer, &start, "\n", -1);
		if (export) {
			gsize i;
			for (i = 0; i < export->len ; i++) {
				gchar *tmp;
				tmp = g_array_index (export, gchar *, i);
				gtk_text_buffer_insert (tbuffer, &start, tmp, -1);
				gtk_text_buffer_insert (tbuffer, &start, "\n", -1);
			}
		}
		else
			gtk_text_buffer_insert (tbuffer, &start, "--\n", -1);
	}
}

/**
 * data_source_editor_display_source
 * @editor: a #DataSourceEditor widget
 * @source: the #DataSource for which properties are displayed, or %NULL
 *
 * Update @editor's display with @source's properties
 */
void
data_source_editor_display_source (DataSourceEditor *editor, DataSource *source)
{
	g_return_if_fail (IS_DATA_SOURCE_EDITOR (editor));
	g_return_if_fail (! source || IS_DATA_SOURCE (source));
	GdaStatement *stmt;
	gchar *sql = NULL;

	g_signal_handlers_block_by_func (editor->priv->attributes,
					 G_CALLBACK (attribute_changed_cb), editor);

	/* other variables */
	if (editor->priv->source)
		g_object_unref (editor->priv->source);
	if (source) {
		editor->priv->source = g_object_ref (source);
		g_assert (gda_set_set_holder_value (editor->priv->attributes, NULL,
						    "id", data_source_get_id (source)));
		g_assert (gda_set_set_holder_value (editor->priv->attributes, NULL,
						    "descr", data_source_get_title (source)));
		g_assert (gda_set_set_holder_value (editor->priv->attributes, NULL,
						    "table", data_source_get_table (source)));
		stmt = data_source_get_statement (source);
		if (stmt)
			sql = gda_statement_to_sql_extended (stmt, NULL, NULL,
							     GDA_STATEMENT_SQL_PRETTY | GDA_STATEMENT_SQL_PARAMS_SHORT,
							     NULL, NULL);
		g_assert (gda_set_set_holder_value (editor->priv->attributes, NULL, "sql", sql));
		g_free (sql);
		
		switch (data_source_get_source_type (source)) {
		case DATA_SOURCE_TABLE:
			gdaui_basic_form_entry_set_editable (editor->priv->form,
							     gda_set_get_holder (editor->priv->attributes, "sql"),
							     FALSE);
			break;
		case DATA_SOURCE_SELECT:
			gdaui_basic_form_entry_set_editable (editor->priv->form,
							     gda_set_get_holder (editor->priv->attributes, "sql"),
							     TRUE);
			break;
		default:
			g_assert_not_reached ();
			break;
		}
		
	}
	else {
		editor->priv->source = NULL;
		g_assert (gda_set_set_holder_value (editor->priv->attributes, NULL,
						    "id", NULL));
		g_assert (gda_set_set_holder_value (editor->priv->attributes, NULL,
						    "descr", NULL));
		g_assert (gda_set_set_holder_value (editor->priv->attributes, NULL,
						    "table", NULL));
		g_assert (gda_set_set_holder_value (editor->priv->attributes, NULL,
						    "sql", NULL));
	}
	gtk_widget_set_sensitive (GTK_WIDGET (editor), source ? TRUE : FALSE);

	g_signal_handlers_unblock_by_func (editor->priv->attributes,
					   G_CALLBACK (attribute_changed_cb), editor);

	/* dependencies */
	update_dependencies_display (editor);
}

static void
attribute_changed_cb (G_GNUC_UNUSED GdaSet *set, GdaHolder *holder, DataSourceEditor *editor)
{
	const gchar *id, *str = NULL;
	const GValue *cvalue;

	if (! editor->priv->source)
		return;

	g_signal_handlers_block_by_func (editor->priv->attributes,
					 G_CALLBACK (attribute_changed_cb), editor);

	id = gda_holder_get_id (holder);
	cvalue = gda_holder_get_value (holder);
	if (G_VALUE_TYPE (cvalue) == G_TYPE_STRING)
		str = g_value_get_string (cvalue);

	g_assert (id);
	if (!strcmp (id, "id")) {
		data_source_set_id (editor->priv->source, str);
	}
	else if (!strcmp (id, "descr")) {
		data_source_set_title (editor->priv->source, str);
	}
	else if (!strcmp (id, "table")) {
		GdaHolder *holder;
		holder = gda_set_get_holder (editor->priv->attributes, "sql");

		if (!str || !*str) {
			GdaStatement *stmt;
			gchar *sql = NULL;

			stmt = data_source_get_statement (editor->priv->source);
			if (stmt)
				sql = gda_statement_to_sql_extended (stmt, NULL, NULL,
								     GDA_STATEMENT_SQL_PRETTY |
								     GDA_STATEMENT_SQL_PARAMS_SHORT,
								     NULL, NULL);
			data_source_set_query (editor->priv->source, sql, NULL);
			g_free (sql);
			gdaui_basic_form_entry_set_editable (editor->priv->form, holder, TRUE);
		}
		else {
			data_source_set_table (editor->priv->source, str, NULL);
			gdaui_basic_form_entry_set_editable (editor->priv->form, holder, FALSE);

			GdaStatement *stmt;
			gchar *sql = NULL;

			stmt = data_source_get_statement (editor->priv->source);
			if (stmt)
				sql = gda_statement_to_sql_extended (stmt, NULL, NULL,
								     GDA_STATEMENT_SQL_PRETTY |
								     GDA_STATEMENT_SQL_PARAMS_SHORT,
								     NULL, NULL);
			g_assert (gda_holder_set_value_str (holder, NULL, sql, NULL));
			g_free (sql);
		}
	}
	else if (!strcmp (id, "sql")) {
		data_source_set_query (editor->priv->source, str, NULL);
		update_dependencies_display (editor);
	}
	else
		g_assert_not_reached ();

	g_signal_handlers_unblock_by_func (editor->priv->attributes,
					   G_CALLBACK (attribute_changed_cb), editor);
}

/**
 * data_source_editor_set_editable
 */
void
data_source_editor_set_readonly (DataSourceEditor *editor)
{
	gdaui_basic_form_entry_set_editable (editor->priv->form, NULL,
					     FALSE);
}
