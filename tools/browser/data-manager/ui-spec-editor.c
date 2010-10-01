/*
 * Copyright (C) 2010 The GNOME Foundation.
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

#include <glib/gi18n-lib.h>
#include <string.h>
#include "ui-spec-editor.h"
#include "data-source.h"
#include <libgda/libgda.h>
#include "../support.h"
#include "data-source-editor.h"

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
        if (str) {
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
		g_object_set (cell, "pixbuf", browser_get_pixbuf_icon (BROWSER_ICON_TABLE), NULL);
		break;
	case DATA_SOURCE_SELECT:
		g_object_set (cell, "pixbuf", browser_get_pixbuf_icon (BROWSER_ICON_QUERY), NULL);
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
}

static void
ui_spec_editor_init (UiSpecEditor *sped, G_GNUC_UNUSED UiSpecEditorClass *klass)
{
	g_return_if_fail (IS_UI_SPEC_EDITOR (sped));

	/* allocate private structure */
	sped->priv = g_new0 (UiSpecEditorPrivate, 1);
	
	GtkWidget *hpaned;
	hpaned = gtk_hpaned_new ();
	gtk_box_pack_start (GTK_BOX (sped), hpaned, TRUE, TRUE, 0);
	
	GtkWidget *vbox;
	vbox = gtk_vbox_new (FALSE, 0);
	gtk_paned_pack1 (GTK_PANED (hpaned), vbox, TRUE, FALSE);

	GtkWidget *label;
	gchar *str;
	label = gtk_label_new ("");
	str = g_strdup_printf ("<b>%s</b>", _("Data sources:"));
	gtk_label_set_markup (GTK_LABEL (label), str);
	g_free (str);
	gtk_misc_set_alignment (GTK_MISC (label), 0., -1);
	gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

	/* data sources model & view */
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;
	sped->priv = g_new0 (UiSpecEditorPrivate, 1);
	sped->priv->sources_model = gtk_list_store_new (NUM_COLUMNS,
							G_TYPE_POINTER);

	sped->priv->sources_tree = gtk_tree_view_new_with_model (GTK_TREE_MODEL (sped->priv->sources_model));
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

	vbox = gtk_vbox_new (FALSE, 0);
	gtk_paned_pack2 (GTK_PANED (hpaned), vbox, TRUE, FALSE);

	label = gtk_label_new ("");
	str = g_strdup_printf ("<b>%s</b>", _("Selected data source's properties:"));
	gtk_label_set_markup (GTK_LABEL (label), str);
	g_free (str);
	gtk_misc_set_alignment (GTK_MISC (label), 0., -1);
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
			(GInstanceInitFunc) ui_spec_editor_init
		};
		type = g_type_register_static (GTK_TYPE_VBOX, "UiSpecEditor", &info, 0);
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
	data_source_editor_display_source (sped->priv->propsedit, current_source);
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
