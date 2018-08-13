/*
 * Copyright (C) 2010 David King <davidk@openismus.com>
 * Copyright (C) 2010 - 2015 Vivien Malerba <malerba@gnome-db.org>
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

#include <glib/gi18n-lib.h>
#include <string.h>
#include "ui-spec-editor.h"
#include "data-source.h"
#include <libgda/libgda.h>
#include "../ui-support.h"
#include "data-source-editor.h"
#include <gdk/gdkkeysyms.h>

enum
{
	COLUMN_DATA_SOURCE,
	NUM_COLUMNS
};

struct _UiSpecEditorPrivate {
	DataSourceManager *mgr;
	GtkListStore *sources_model;
	GtkWidget *sources_tree;
	DataSourceEditor *propsedit;
	GtkWidget *popup_menu;

	/* warnings */
	GtkWidget  *info;
};

static void ui_spec_editor_class_init (UiSpecEditorClass *klass);
static void ui_spec_editor_init       (UiSpecEditor *sped, UiSpecEditorClass *klass);
static void ui_spec_editor_dispose    (GObject *object);

static GObjectClass *parent_class = NULL;

/*
 * UiSpecEditor class implementation
 */
static void
ui_spec_editor_class_init (UiSpecEditorClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->dispose = ui_spec_editor_dispose;
}

static void
cell_text_data_func (G_GNUC_UNUSED GtkTreeViewColumn *tree_column, GtkCellRenderer *cell,
		     GtkTreeModel *tree_model, GtkTreeIter *iter, G_GNUC_UNUSED gpointer data)
{
	DataSource *source;
	gtk_tree_model_get (tree_model, iter, COLUMN_DATA_SOURCE, &source, -1);
	g_assert (source);

	GString *mark;
	const gchar *id, *str;
	mark = g_string_new ("");
	/* FIXME: add source ID */
	id = NULL;

	str = data_source_get_id (source);
	if (str)
		g_string_append (mark, str);
	else
		g_string_append_c (mark, '-');
	str = data_source_get_title (source);
        if (str && *str) {
		if (!id || strcmp ((gchar*) id, (gchar*) str)) {
			gchar *tmp;
			tmp = g_markup_escape_text ((gchar*) str, -1);
			g_string_append_printf (mark, "\n<small><i>%s</i></small>", tmp);
			g_free (tmp);
		}
        }

	g_object_set (cell, "markup", mark->str, NULL);
	g_string_free (mark, TRUE);
}

static void
cell_pixbuf_data_func (G_GNUC_UNUSED GtkTreeViewColumn *tree_column, GtkCellRenderer *cell,
		       GtkTreeModel *tree_model, GtkTreeIter *iter, G_GNUC_UNUSED gpointer data)
{
	DataSource *source;
	gtk_tree_model_get (tree_model, iter, COLUMN_DATA_SOURCE, &source, -1);
	g_assert (source);

	DataSourceType stype;
	stype = data_source_get_source_type (source);
	switch (stype) {
	case DATA_SOURCE_TABLE:
		g_object_set (cell, "pixbuf", ui_get_pixbuf_icon (UI_ICON_TABLE), NULL);
		break;
	case DATA_SOURCE_SELECT:
		g_object_set (cell, "pixbuf", ui_get_pixbuf_icon (UI_ICON_QUERY), NULL);
		break;
	default:
		g_object_set (cell, "pixbuf", NULL, NULL);
		break;
	}
}

static void
data_source_selection_changed_cb (GtkTreeSelection *sel, UiSpecEditor *uied)
{
	GtkTreeIter iter;
	if (gtk_tree_selection_get_selected (sel, NULL, &iter)) {
		DataSource *source;
		gtk_tree_model_get (GTK_TREE_MODEL (uied->priv->sources_model), &iter,
				    COLUMN_DATA_SOURCE, &source, -1);
		data_source_editor_display_source (uied->priv->propsedit, source);
	}
	else
		data_source_editor_display_source (uied->priv->propsedit, NULL);
}

static void
popup_func_delete_cb (G_GNUC_UNUSED GtkMenuItem *mitem, UiSpecEditor *uied)
{
	GtkTreeModel *model;
	GtkTreeSelection *select;
	GtkTreeIter iter;
	
	select = gtk_tree_view_get_selection (GTK_TREE_VIEW (uied->priv->sources_tree));
	if (gtk_tree_selection_get_selected (select, &model, &iter)) {
		DataSource *source;
		gtk_tree_model_get (model, &iter, COLUMN_DATA_SOURCE, &source, -1);
		g_assert (source);
		data_source_manager_remove_source (uied->priv->mgr, source);
	}
}

static void
do_popup_menu (G_GNUC_UNUSED GtkWidget *widget, GdkEventButton *event, UiSpecEditor *uied)
{
        if (! uied->priv->popup_menu) {
                GtkWidget *menu, *mitem;

                menu = gtk_menu_new ();
                g_signal_connect (menu, "deactivate",
                                  G_CALLBACK (gtk_widget_hide), NULL);

                mitem = gtk_menu_item_new_with_label (_("Remove"));
                gtk_menu_shell_append (GTK_MENU_SHELL (menu), mitem);
                gtk_widget_show (mitem);
                g_signal_connect (mitem, "activate",
				  G_CALLBACK (popup_func_delete_cb), uied);

                uied->priv->popup_menu = menu;
        }

        gtk_menu_popup_at_pointer (GTK_MENU (uied->priv->popup_menu), NULL);
}

static gboolean
popup_menu_cb (GtkWidget *widget, UiSpecEditor *uied)
{
        do_popup_menu (widget, NULL, uied);
        return TRUE;
}

static gboolean
button_press_event_cb (GtkTreeView *treeview, GdkEventButton *event, UiSpecEditor *uied)
{
        if (event->button == 3 && event->type == GDK_BUTTON_PRESS) {
                do_popup_menu ((GtkWidget*) treeview, event, uied);
                return TRUE;
        }

        return FALSE;
}

static gboolean
key_press_event_cb (GtkTreeView *treeview, GdkEventKey *event, UiSpecEditor *sped)
{
        if (event->keyval == GDK_KEY_Delete) {
		GtkTreeModel *model;
                GtkTreeSelection *select;
                GtkTreeIter iter;

                select = gtk_tree_view_get_selection (treeview);
                if (gtk_tree_selection_get_selected (select, &model, &iter)) {
			DataSource *source;
			gtk_tree_model_get (model, &iter, COLUMN_DATA_SOURCE, &source, -1);
			g_assert (source);
			data_source_manager_remove_source (sped->priv->mgr, source);
		}
		return TRUE;
	}
	return FALSE; /* not handled */
}

static void
ui_spec_editor_init (UiSpecEditor *sped, G_GNUC_UNUSED UiSpecEditorClass *klass)
{
	g_return_if_fail (IS_UI_SPEC_EDITOR (sped));

	/* allocate private structure */
	sped->priv = g_new0 (UiSpecEditorPrivate, 1);

	gtk_orientable_set_orientation (GTK_ORIENTABLE (sped), GTK_ORIENTATION_VERTICAL);
	
	GtkWidget *hpaned;
	hpaned = gtk_paned_new (GTK_ORIENTATION_HORIZONTAL);
	gtk_box_pack_start (GTK_BOX (sped), hpaned, TRUE, TRUE, 0);
	
	GtkWidget *vbox;
	vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_paned_pack1 (GTK_PANED (hpaned), vbox, TRUE, FALSE);

	GtkWidget *label;
	gchar *str;
	label = gtk_label_new ("");
	str = g_strdup_printf ("<b>%s</b>", _("Data sources:"));
	gtk_label_set_markup (GTK_LABEL (label), str);
	g_free (str);
	gtk_widget_set_halign (label, GTK_ALIGN_START);
	gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

	/* data sources model & view */
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;
	sped->priv = g_new0 (UiSpecEditorPrivate, 1);
	sped->priv->sources_model = gtk_list_store_new (NUM_COLUMNS,
							G_TYPE_POINTER);

	sped->priv->sources_tree = ui_make_tree_view (GTK_TREE_MODEL (sped->priv->sources_model));
	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (sped->priv->sources_tree), FALSE);
	gtk_widget_set_size_request (sped->priv->sources_tree, 170, -1);

	renderer = gtk_cell_renderer_pixbuf_new ();
	column = gtk_tree_view_column_new ();
	gtk_tree_view_column_pack_start (column, renderer, FALSE);
        gtk_tree_view_column_set_cell_data_func (column, renderer,
						 (GtkTreeCellDataFunc) cell_pixbuf_data_func,
                                                 NULL, NULL);
	
	renderer = gtk_cell_renderer_text_new ();	
	gtk_tree_view_column_pack_start (column, renderer, TRUE);
        gtk_tree_view_column_set_cell_data_func (column, renderer, (GtkTreeCellDataFunc) cell_text_data_func,
                                                 NULL, NULL);

        gtk_tree_view_append_column (GTK_TREE_VIEW (sped->priv->sources_tree), column);
	
	g_signal_connect (G_OBJECT (sped->priv->sources_tree), "key-press-event",
                          G_CALLBACK (key_press_event_cb), sped);
	g_signal_connect (G_OBJECT (sped->priv->sources_tree), "popup-menu",
                          G_CALLBACK (popup_menu_cb), sped);
	g_signal_connect (G_OBJECT (sped->priv->sources_tree), "button-press-event",
                          G_CALLBACK (button_press_event_cb), sped);

	GtkTreeSelection *sel;
	sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (sped->priv->sources_tree));
	g_signal_connect (sel, "changed",
			  G_CALLBACK (data_source_selection_changed_cb), sped);

	/* data sources tree */
	GtkWidget *sw;
	sw = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw),
					     GTK_SHADOW_NONE);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);
	gtk_container_add (GTK_CONTAINER (sw), sped->priv->sources_tree);
	gtk_box_pack_start (GTK_BOX (vbox), sw, TRUE, TRUE, 0);

	vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_paned_pack2 (GTK_PANED (hpaned), vbox, TRUE, FALSE);

	label = gtk_label_new ("");
	str = g_strdup_printf ("<b>%s</b>", _("Selected data source's properties:"));
	gtk_label_set_markup (GTK_LABEL (label), str);
	g_free (str);
	gtk_widget_set_halign (label, GTK_ALIGN_START);
	gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

	GtkWidget *pe;
	pe = data_source_editor_new ();
	gtk_box_pack_start (GTK_BOX (vbox), pe, TRUE, TRUE, 0);
	sped->priv->propsedit = DATA_SOURCE_EDITOR (pe);

 	/* warning, not shown */
	sped->priv->info = NULL;

	gtk_widget_show_all (hpaned);
}

GType
ui_spec_editor_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo info = {
			sizeof (UiSpecEditorClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) ui_spec_editor_class_init,
			NULL,
			NULL,
			sizeof (UiSpecEditor),
			0,
			(GInstanceInitFunc) ui_spec_editor_init,
			0
		};
		type = g_type_register_static (GTK_TYPE_BOX, "UiSpecEditor", &info, 0);
	}
	return type;
}

static void
ui_spec_editor_dispose (GObject *object)
{
	UiSpecEditor *sped = (UiSpecEditor*) object;
	if (sped->priv) {
		if (sped->priv->mgr)
			g_object_unref (sped->priv->mgr);
		if (sped->priv->popup_menu)
                        gtk_widget_destroy (sped->priv->popup_menu);

		g_free (sped->priv);
		sped->priv = NULL;
	}
	parent_class->dispose (object);
}

static void
mgr_changed_cb (DataSourceManager *mgr, UiSpecEditor *sped)
{
	const GSList *list;

	/* keep selected source, if any */
	GtkTreeIter iter;
	GtkTreeSelection *sel;
	DataSource *current_source = NULL;
	GtkTreePath *current_path = NULL;

	sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (sped->priv->sources_tree));
	if (gtk_tree_selection_get_selected (sel, NULL, &iter)) {
		gtk_tree_model_get (GTK_TREE_MODEL (sped->priv->sources_model),
				    &iter, COLUMN_DATA_SOURCE, &current_source, -1);
		current_path = gtk_tree_model_get_path (GTK_TREE_MODEL (sped->priv->sources_model),
							&iter);
	}

	/* reset sources list */
	gtk_list_store_clear (sped->priv->sources_model);
	for (list = data_source_manager_get_sources (mgr); list; list = list->next) {
		DataSource *source = DATA_SOURCE (list->data);

		gtk_list_store_append (sped->priv->sources_model, &iter);
		gtk_list_store_set (sped->priv->sources_model, &iter,
				    COLUMN_DATA_SOURCE, source,
				    -1);
	}

	/* reset selected source */
	if (! g_slist_find ((GSList*) data_source_manager_get_sources (sped->priv->mgr),
			    current_source))
		current_source = NULL;
	data_source_editor_display_source (sped->priv->propsedit, NULL);
	if (current_path) {
		gtk_tree_selection_select_path (sel, current_path);
		gtk_tree_path_free (current_path);
	}
}

static void
mgr_source_changed_cb (G_GNUC_UNUSED DataSourceManager *mgr, DataSource *source, UiSpecEditor *sped)
{
	GtkTreeIter iter;
	GtkTreeModel *tree_model = GTK_TREE_MODEL (sped->priv->sources_model);
	DataSource *msource;

	if (gtk_tree_model_get_iter_first (tree_model, &iter)) {
		gtk_tree_model_get (tree_model, &iter, COLUMN_DATA_SOURCE, &msource, -1);
		if (source == msource) {
			GtkTreePath *path;
			path = gtk_tree_path_new_from_indices (0, -1);
			gtk_tree_model_row_changed (tree_model, path, &iter);
			gtk_tree_path_free (path);
			return;
		}

		gint pos;
		for (pos = 1; gtk_tree_model_iter_next (tree_model, &iter); pos++) {
			gtk_tree_model_get (tree_model, &iter, COLUMN_DATA_SOURCE, &msource, -1);
			if (source == msource) {
				GtkTreePath *path;
				path = gtk_tree_path_new_from_indices (pos, -1);
				gtk_tree_model_row_changed (tree_model, path, &iter);
				gtk_tree_path_free (path);
				return;
			}
		}
	}

	g_assert_not_reached ();
}


/**
 * ui_spec_editor_new
 *
 * Returns: the newly created editor.
 */
GtkWidget *
ui_spec_editor_new (DataSourceManager *mgr)
{
	UiSpecEditor *sped;

	g_return_val_if_fail (IS_DATA_SOURCE_MANAGER (mgr), NULL);

	sped = g_object_new (UI_SPEC_EDITOR_TYPE, NULL);
	sped->priv->mgr = g_object_ref (mgr);
	g_signal_connect (mgr, "list-changed",
			  G_CALLBACK (mgr_changed_cb), sped);
	g_signal_connect (mgr, "source-changed",
			  G_CALLBACK (mgr_source_changed_cb), sped);
	mgr_changed_cb (mgr, sped);

	/* warning */
	sped->priv->info = NULL;

	return (GtkWidget*) sped;
}

/**
 * ui_spec_editor_select_source
 * @editor: a #UiSpecEditor widget
 * @source: a #DataSource
 *
 * Selects and displays @source's propserties
 */
void
ui_spec_editor_select_source (UiSpecEditor *editor, DataSource *source)
{
	GtkTreeSelection *sel;
	GtkTreeIter iter;
	GtkTreeModel *tree_model = GTK_TREE_MODEL (editor->priv->sources_model);
	DataSource *msource;

	sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (editor->priv->sources_tree));
	if (gtk_tree_model_get_iter_first (tree_model, &iter)) {
		gtk_tree_model_get (tree_model, &iter, COLUMN_DATA_SOURCE, &msource, -1);
		if (source == msource) {
			gtk_tree_selection_select_iter (sel, &iter);
			return;
		}

		gint pos;
		for (pos = 1; gtk_tree_model_iter_next (tree_model, &iter); pos++) {
			gtk_tree_model_get (tree_model, &iter, COLUMN_DATA_SOURCE, &msource, -1);
			if (source == msource) {
				gtk_tree_selection_select_iter (sel, &iter);
				return;
			}
		}
	}
}

/**
 * ui_spec_editor_get_selected_source
 * @sped: a #UiSpecEditor widget
 *
 * Get the currently selected data source for edition
 *
 * Returns: (transfer none): the #DataSource, or %NULL
 */
DataSource *
ui_spec_editor_get_selected_source (UiSpecEditor *sped)
{
	g_return_val_if_fail (IS_UI_SPEC_EDITOR (sped), NULL);
	GtkTreeModel *model;
	GtkTreeSelection *select;
	GtkTreeIter iter;

	select = gtk_tree_view_get_selection (GTK_TREE_VIEW (sped->priv->sources_tree));
	if (gtk_tree_selection_get_selected (select, &model, &iter)) {
		DataSource *source;
		gtk_tree_model_get (model, &iter, COLUMN_DATA_SOURCE, &source, -1);
		return source;
	}
	else
		return NULL;
}
