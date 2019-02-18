/*
 * Copyright (C) 2010 David King <davidk@openismus.com>
 * Copyright (C) 2010 - 2014 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2011 Murray Cumming <murrayc@murrayc.com>
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
#include "../widget-overlay.h"

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
};

GType
data_source_editor_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static GMutex registering;
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

		
		g_mutex_lock (&registering);
		if (type == 0)
			type = g_type_register_static (GTK_TYPE_BOX, "DataSourceEditor", &info, 0);
		g_mutex_unlock (&registering);
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
zoom_mitem_cb (GtkCheckMenuItem *checkmenuitem, WidgetOverlay *ovl)
{
	g_object_set (G_OBJECT (ovl), "add-scale",
		      gtk_check_menu_item_get_active (checkmenuitem), NULL);
}

static void
form_populate_popup_cb (GtkWidget *wid, GtkMenu *menu, WidgetOverlay *ovl)
{
	GtkWidget *mitem;
	gboolean add_scale;
	g_object_get (G_OBJECT (ovl), "add-scale", &add_scale, NULL);
	mitem = gtk_check_menu_item_new_with_label (_("Zoom..."));
	gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (mitem), add_scale);
	gtk_widget_show (mitem);
	gtk_menu_shell_prepend (GTK_MENU_SHELL (menu), mitem);
	g_signal_connect (mitem, "toggled",
			  G_CALLBACK (zoom_mitem_cb), ovl);
}

static void
data_source_editor_init (DataSourceEditor *editor)
{
	GtkWidget *ovl;

	gtk_orientable_set_orientation (GTK_ORIENTABLE (editor), GTK_ORIENTATION_VERTICAL);

	editor->priv = g_new0 (DataSourceEditorPrivate, 1);
	editor->priv->attributes = gda_set_new_inline (5,
						       "id", G_TYPE_STRING, "",
						       "descr", G_TYPE_STRING, "",
						       "table", G_TYPE_STRING, "",
						       "sql", G_TYPE_STRING, "",
						       "depend", G_TYPE_STRING, "");
	g_signal_connect (editor->priv->attributes, "holder-changed",
			  G_CALLBACK (attribute_changed_cb), editor);

	ovl = widget_overlay_new ();
	gtk_box_pack_start (GTK_BOX (editor), ovl, TRUE, TRUE, 0);

	GtkWidget *form;
	form = gdaui_basic_form_new (editor->priv->attributes);
	editor->priv->form = GDAUI_BASIC_FORM (form);
	g_signal_connect (form, "populate-popup",
			  G_CALLBACK (form_populate_popup_cb), ovl);

	gtk_container_add (GTK_CONTAINER (ovl), form);
	widget_overlay_set_child_props (WIDGET_OVERLAY (ovl), form,
					WIDGET_OVERLAY_CHILD_VALIGN, WIDGET_OVERLAY_ALIGN_FILL,
					WIDGET_OVERLAY_CHILD_HALIGN, WIDGET_OVERLAY_ALIGN_FILL,
					WIDGET_OVERLAY_CHILD_SCALE, .9,
					-1);
	g_object_set (G_OBJECT (ovl), "add-scale", TRUE, NULL);
	g_object_set (G_OBJECT (ovl), "add-scale", FALSE, NULL);
	gtk_widget_show_all (ovl);

	GdaHolder *holder;
	holder = gda_set_get_holder (editor->priv->attributes, "id");
	if (holder) {
		g_object_set ((GObject*) holder, "name", _("Id"),
		      "description",
		      _("Data source's ID\n(as referenced by other data sources)"), NULL);
		gdaui_basic_form_entry_set_editable (GDAUI_BASIC_FORM (form), holder, FALSE);
	}

	holder = gda_set_get_holder (editor->priv->attributes, "descr");
	if (holder != NULL) {
		g_object_set ((GObject*) holder, "name", _("Description"),
		      "description", _("Data source's description"), NULL);
	}

	holder = gda_set_get_holder (editor->priv->attributes, "table");

	if (holder != NULL) {
		g_object_set ((GObject*) holder, "name", _("Table"),
		      "description", _("Table to display data from, leave empty\nto specify a SELECT statement instead"),
		      NULL);
	}

	holder = gda_set_get_holder (editor->priv->attributes, "sql");
	if (holder != NULL) {
		g_object_set ((GObject*) holder, "name", _("SELECT\nSQL"),
		      "description", _("Actual SQL executed\nto select data\nCan't be changed if a table name is set"),
		       "plugin", "text:PROG_LANG=gda-sql",
		       NULL);
	}


	holder = gda_set_get_holder (editor->priv->attributes, "depend");
	if (holder != NULL) {
		g_object_set ((GObject*) holder, "name", _("Dependencies"),
		      "description", _("Required and provided named parameters"),
		      "plugin", "rtext", NULL);
		gdaui_basic_form_entry_set_editable (GDAUI_BASIC_FORM (form), holder, FALSE);
	}
}

static void
data_source_editor_dispose (GObject *object)
{
	DataSourceEditor *editor;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_DATA_SOURCE_EDITOR (object));

	editor = DATA_SOURCE_EDITOR (object);
	if (editor->priv) {
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
	GString *string;

	string = g_string_new ("");
	if (editor->priv->source) {
		GdaSet *import;
		import = data_source_get_import ( editor->priv->source);
		g_string_append_printf (string, "**%s**\n", _("Requires:"));
		if (import && gda_set_get_holders (import)) {
			GSList *list;
			for (list = gda_set_get_holders (import); list; list = list->next)
				g_string_append_printf (string, "%s\n",
							gda_holder_get_id (GDA_HOLDER (list->data)));
		}
		else
			g_string_append (string, "--\n");

		GArray *export;
		export = data_source_get_export_names ( editor->priv->source);
		g_string_append_printf (string, "\n**%s**\n", _("Exports:"));
		if (export) {
			gsize i;
			for (i = 0; i < export->len ; i++) {
				gchar *tmp;
				tmp = g_array_index (export, gchar *, i);
				g_string_append_printf (string, "%s\n", tmp);
			}
		}
		else
			g_string_append (string, "--\n");
	}

	GdaHolder *holder;
	holder = gda_set_get_holder (editor->priv->attributes, "depend");
	g_assert (gda_holder_set_value_str (holder, NULL, string->str, NULL));
	g_string_free (string, TRUE);
}

/**
 * data_source_editor_display_source
 * @editor: a #DataSourceEditor widget
 * @source: (nullable): the #DataSource for which properties are displayed, or %NULL
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
	else if (strcmp (id, "depend"))
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
