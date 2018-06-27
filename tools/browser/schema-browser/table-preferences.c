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
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <libgda/gda-tree.h>
#include "table-info.h"
#include "table-preferences.h"
#include <libgda-ui/gdaui-tree-store.h>
#include "../ui-support.h"
#include "../gdaui-bar.h"
#include "schema-browser-perspective.h"
#include "../query-exec/query-editor.h"
#include <libgda-ui/gdaui-plugin.h>
#include <libgda-ui/gdaui-basic-form.h>
#include <libgda-ui/gdaui-easy.h>
#include <libgda/gda-debug-macros.h>

struct _TablePreferencesPrivate {
	TConnection *tcnc;
	TableInfo *tinfo;
	GtkListStore *columns_store;
	GtkTreeView *columns_treeview;

	GdaMetaTable *current_table;
	GdaMetaTableColumn *current_column;

	/* field properties */
	GtkWidget *field_props;
	GtkTreeModel *plugins_model;
	gboolean save_plugin_changes;
	GtkWidget *plugins_combo;
	GtkWidget *options_vbox;
	GtkWidget *options_wid;
	GtkWidget *options_label;
	GtkWidget *preview_vbox;
	GtkWidget *preview_none;
	GtkWidget *preview_wid;
};

static void table_preferences_class_init (TablePreferencesClass *klass);
static void table_preferences_init       (TablePreferences *tpreferences, TablePreferencesClass *klass);
static void table_preferences_dispose   (GObject *object);

static GtkWidget *create_column_properties (TablePreferences *tpref);
static void       update_column_properties (TablePreferences *tpref);
static void meta_changed_cb (TConnection *tcnc, GdaMetaStruct *mstruct, TablePreferences *tpreferences);
static void plugins_combo_changed_cb (GtkComboBox *combo, TablePreferences *tpref);

static GObjectClass *parent_class = NULL;

#ifdef G_OS_WIN32
#define IMPORT __declspec(dllimport)
#else
#define IMPORT
#endif
extern IMPORT GHashTable *gdaui_plugins_hash;

/*
 * TablePreferences class implementation
 */

static void
table_preferences_class_init (TablePreferencesClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->dispose = table_preferences_dispose;
}


static void
table_preferences_init (TablePreferences *tpreferences, G_GNUC_UNUSED TablePreferencesClass *klass)
{
	tpreferences->priv = g_new0 (TablePreferencesPrivate, 1);

	gtk_orientable_set_orientation (GTK_ORIENTABLE (tpreferences), GTK_ORIENTATION_VERTICAL);
}

static void
table_preferences_dispose (GObject *object)
{
	TablePreferences *tpref = (TablePreferences *) object;

	/* free memory */
	if (tpref->priv) {
		if (tpref->priv->tcnc) {
			g_signal_handlers_disconnect_by_func (tpref->priv->tcnc,
							      G_CALLBACK (meta_changed_cb), tpref);
			g_object_unref (tpref->priv->tcnc);
		}

		if (tpref->priv->columns_store)
			g_object_unref (G_OBJECT (tpref->priv->columns_store));

		if (tpref->priv->plugins_model)
			g_object_unref (G_OBJECT (tpref->priv->plugins_model));

		g_free (tpref->priv);
		tpref->priv = NULL;
	}

	parent_class->dispose (object);
}

GType
table_preferences_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo preferences = {
			sizeof (TablePreferencesClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) table_preferences_class_init,
			NULL,
			NULL,
			sizeof (TablePreferences),
			0,
			(GInstanceInitFunc) table_preferences_init,
			0
		};
		type = g_type_register_static (GTK_TYPE_BOX, "TablePreferences", &preferences, 0);
	}
	return type;
}

enum
{
	COLUMN_POINTER,
	COLUMN_GTYPE,
	COLUMN_PLUGIN,
        NUM_COLUMNS
};

static void
meta_changed_cb (G_GNUC_UNUSED TConnection *tcnc, GdaMetaStruct *mstruct, TablePreferences *tpref)
{
	gtk_list_store_clear (tpref->priv->columns_store);

	tpref->priv->current_table = NULL;
	tpref->priv->current_column = NULL;

	if (!mstruct)
		return;

	GdaMetaDbObject *dbo;
	GValue *schema_v = NULL, *name_v;
	const gchar *str;
	
	str = table_info_get_table_schema (tpref->priv->tinfo);
	if (str)
		g_value_set_string ((schema_v = gda_value_new (G_TYPE_STRING)), str);
	str = table_info_get_table_name (tpref->priv->tinfo);
	g_value_set_string ((name_v = gda_value_new (G_TYPE_STRING)), str);
	dbo = gda_meta_struct_get_db_object (mstruct, NULL, schema_v, name_v);
	if (schema_v)
		gda_value_free (schema_v);
	gda_value_free (name_v);

	if (dbo) {
		GdaMetaTable *mtable = GDA_META_TABLE (dbo);
		GSList *list;

		tpref->priv->current_table = mtable;
		for (list = mtable->columns; list; list = list->next) {
			GdaMetaTableColumn *column = GDA_META_TABLE_COLUMN (list->data);
			GtkTreeIter iter;
			gchar *eprops;
			GError *error = NULL;
			eprops = t_connection_get_table_column_attribute (tpref->priv->tcnc,
										tpref->priv->current_table,
										column,
										T_CONNECTION_COLUMN_PLUGIN,
										&error);
			if (error) {
				TO_IMPLEMENT; /* FIXME: add a notice somewhere in the UI */
				g_warning ("Error: %s\n", error->message);
				g_clear_error (&error);
			}

			gtk_list_store_append (tpref->priv->columns_store, &iter);
			gtk_list_store_set (tpref->priv->columns_store, &iter,
					    COLUMN_POINTER, column,
					    COLUMN_GTYPE, column->gtype,
					    COLUMN_PLUGIN, eprops,
					    -1);
			g_free (eprops);
		}
	}
}

static void
table_column_pref_changed_cb (G_GNUC_UNUSED TConnection *tcnc, G_GNUC_UNUSED GdaMetaTable *table,
			      GdaMetaTableColumn *column,
			      const gchar *attr_name, const gchar *value, TablePreferences *tpref)
{
	GtkTreeIter iter;
	GdaMetaTableColumn *mcol;
	gboolean valid;
	GtkTreeModel *model;
	
	if (strcmp (attr_name, T_CONNECTION_COLUMN_PLUGIN))
		return;

	model = GTK_TREE_MODEL (tpref->priv->columns_store);
	for (valid = gtk_tree_model_get_iter_first (model, &iter);
	     valid;
	     valid = gtk_tree_model_iter_next (model, &iter)) {
		gtk_tree_model_get (model, &iter, COLUMN_POINTER, &mcol, -1);
		if (mcol == column) {
			gtk_list_store_set (tpref->priv->columns_store, &iter, COLUMN_PLUGIN,
					    value, -1);
			return;
		}
	}
}

static void
cell_name_data_func (G_GNUC_UNUSED GtkTreeViewColumn *tree_column,
		     GtkCellRenderer *cell,
		     GtkTreeModel *tree_model,
		     GtkTreeIter *iter,
		     G_GNUC_UNUSED gpointer data)
{
	gchar *plugin;
	GdaMetaTableColumn *column;
        gchar *str;
        gchar *m1, *m2;

        gtk_tree_model_get (tree_model, iter, COLUMN_POINTER, &column, COLUMN_PLUGIN, &plugin, -1);
        m1 = g_markup_escape_text (column->column_name, -1);
        if (plugin) {
		for (str = plugin; *str && (*str != ':'); str++);
		*str = 0;

		m2 = g_markup_escape_text (plugin, -1);
                str = g_strdup_printf ("%s\n<small>%s</small>", m1, m2);
	}
        else {
		m2 = g_markup_escape_text (_("default"), -1);
                str = g_strdup_printf ("%s\n<small><i>%s</i></small>", m1, m2);
	}

        g_free (plugin);
        g_free (m1);
        g_free (m2);

        g_object_set ((GObject*) cell, "markup", str, NULL);
        g_free (str);
}

static void
selection_changed_cb (GtkTreeSelection *select, TablePreferences *tpref)
{
        GtkTreeIter iter;
	GtkTreeModel *model;

        if (gtk_tree_selection_get_selected (select, &model, &iter)) {
		GdaMetaTableColumn *column;
		gtk_tree_model_get (model, &iter, COLUMN_POINTER, &column, -1);
		tpref->priv->current_column = column;
        }
	else
		tpref->priv->current_column = NULL;

	update_column_properties (tpref);
}

static void
columns_model_row_changed_cb (G_GNUC_UNUSED GtkTreeModel *tree_model, G_GNUC_UNUSED GtkTreePath *path,
			      G_GNUC_UNUSED GtkTreeIter *iter, TablePreferences *tpref)
{
	tpref->priv->save_plugin_changes = FALSE;
	plugins_combo_changed_cb (GTK_COMBO_BOX (tpref->priv->plugins_combo), tpref);
	tpref->priv->save_plugin_changes = TRUE;
}

/**
 * table_preferences_new
 *
 * Returns: a new #GtkWidget
 */
GtkWidget *
table_preferences_new (TableInfo *tinfo)
{
	TablePreferences *tpref;

	g_return_val_if_fail (IS_TABLE_INFO (tinfo), NULL);

	tpref = TABLE_PREFERENCES (g_object_new (TABLE_PREFERENCES_TYPE, NULL));

	tpref->priv->tinfo = tinfo;
	tpref->priv->tcnc = g_object_ref (table_info_get_connection (tinfo));
	g_signal_connect (tpref->priv->tcnc, "meta-changed",
			  G_CALLBACK (meta_changed_cb), tpref);
	g_signal_connect (tpref->priv->tcnc, "table-column-pref-changed",
			  G_CALLBACK (table_column_pref_changed_cb), tpref);

	GtkWidget *grid;
	grid = gtk_grid_new ();
	gtk_box_pack_start (GTK_BOX (tpref), grid, TRUE, TRUE, 0);
	gtk_grid_set_row_spacing (GTK_GRID (grid), 6);
	gtk_grid_set_column_spacing (GTK_GRID (grid), 6);
	gtk_container_set_border_width (GTK_CONTAINER (grid), 6);

	/* left column */
	GtkWidget *label;
	gchar *str;
	label = gtk_label_new ("");
	str = g_strdup_printf ("<b>%s:</b>", _("Table's fields"));
	gtk_label_set_markup (GTK_LABEL (label), str);
	g_free (str);
	gtk_widget_set_halign (label, GTK_ALIGN_START);
	gtk_grid_attach (GTK_GRID (grid), label, 0, 0, 1, 1);

	GtkWidget *sw, *treeview;
	sw = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw),
					     GTK_SHADOW_ETCHED_IN);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
					GTK_POLICY_NEVER,
					GTK_POLICY_AUTOMATIC);
	gtk_grid_attach (GTK_GRID (grid), sw, 0, 1, 1, 1);

	tpref->priv->columns_store = gtk_list_store_new (NUM_COLUMNS,
							 G_TYPE_POINTER, G_TYPE_GTYPE,
							 G_TYPE_STRING);
	treeview = ui_make_tree_view (GTK_TREE_MODEL (tpref->priv->columns_store));
	tpref->priv->columns_treeview = GTK_TREE_VIEW (treeview);
	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (treeview), FALSE);
	gtk_container_add (GTK_CONTAINER (sw), treeview);

	/* treeview's columns */
	GtkTreeViewColumn *col;
	GtkCellRenderer *cell;
	cell = gtk_cell_renderer_text_new ();
	col = gtk_tree_view_column_new ();
	gtk_tree_view_column_pack_start (col, cell, TRUE);
	gtk_tree_view_column_set_cell_data_func (col, cell,
						 (GtkTreeCellDataFunc) cell_name_data_func, NULL, NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), col);
	
	/* selection handling */
	GtkTreeSelection *select;
	select = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
	gtk_tree_selection_set_mode (select, GTK_SELECTION_SINGLE);
	g_signal_connect (G_OBJECT (select), "changed",
			  G_CALLBACK (selection_changed_cb), tpref);


	/* right column */
	label = gtk_label_new ("");
	str = g_strdup_printf ("<b>%s:</b>", _("Field's display preferences"));
	gtk_label_set_markup (GTK_LABEL (label), str);
	g_free (str);
	gtk_widget_set_halign (label, GTK_ALIGN_START);
	gtk_grid_attach (GTK_GRID (grid), label, 1, 0, 1, 1);

	tpref->priv->field_props = create_column_properties (tpref);
	gtk_grid_attach (GTK_GRID (grid), tpref->priv->field_props, 1, 1, 1, 1);

	/* show all */
	gtk_widget_show_all (grid);

	/*
	 * initial update
	 */
	GdaMetaStruct *mstruct;
	mstruct = t_connection_get_meta_struct (tpref->priv->tcnc);
	if (mstruct)
		meta_changed_cb (tpref->priv->tcnc, mstruct, tpref);
	selection_changed_cb (select, tpref);

	g_signal_connect (tpref->priv->columns_store, "row-changed",
			  G_CALLBACK (columns_model_row_changed_cb), tpref);

	return (GtkWidget*) tpref;
}

enum {
	PL_COLUMN_PLUGIN,
	PL_COLUMN_DESCR,
	PL_NUM_COLUMNS
};

enum {
	GT_COLUMN_GTYPE,
	GT_COLUMN_DESCR,
	GT_NUM_COLUMNS
};

typedef struct {
	GType type;
	GtkListStore *store;
} ForeachData;
static void plugin_hash_foreach_func (const gchar *plugin_name, GdauiPlugin *plugin, ForeachData *fdata);

static GtkWidget *
create_column_properties (TablePreferences *tpref)
{
	GtkWidget *combo, *label, *grid;
	GtkCellRenderer *renderer;

	grid = gtk_grid_new ();
	gtk_grid_set_row_spacing (GTK_GRID (grid), 5);
	gtk_grid_set_column_spacing (GTK_GRID (grid), 5);

	/* plugins combo */
	tpref->priv->plugins_model = GTK_TREE_MODEL (gtk_list_store_new (PL_NUM_COLUMNS,
									 G_TYPE_POINTER, G_TYPE_STRING));
	ForeachData data;
	data.type = 0;
	data.store = GTK_LIST_STORE (tpref->priv->plugins_model);
	g_hash_table_foreach (gdaui_plugins_hash, (GHFunc) plugin_hash_foreach_func, &data);

	combo = gtk_combo_box_new_with_model (tpref->priv->plugins_model);
	gtk_combo_box_set_active (GTK_COMBO_BOX (combo), 0);
	tpref->priv->plugins_combo = combo;
	renderer = gtk_cell_renderer_text_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo), renderer, FALSE);
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combo), renderer,
					"text", PL_COLUMN_DESCR, 
					NULL);
	g_signal_connect (G_OBJECT (combo), "changed",
			  G_CALLBACK (plugins_combo_changed_cb), tpref);

	gtk_grid_attach (GTK_GRID (grid), combo, 1, 0, 1, 1);
	
	label = gtk_label_new (_("Data entry type:"));
	gtk_widget_set_tooltip_text (label, _("Defines how data for the selected column\n"
					      "will be displayed in forms. Leave 'Default' to have\n"
					      "the default display"));
	gtk_widget_set_halign (label, GTK_ALIGN_START);
	gtk_grid_attach (GTK_GRID (grid), label, 0, 0, 1, 1);
	
	/* plugin options */
	tpref->priv->options_vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_grid_attach (GTK_GRID (grid), tpref->priv->options_vbox, 1, 1, 1, 1);

	label = gtk_label_new (_("Options:"));
	tpref->priv->options_label = label;
	gtk_widget_set_halign (label, GTK_ALIGN_START);
	gtk_grid_attach (GTK_GRID (grid), label, 0, 1, 1, 1);

	/* plugin preview */
	tpref->priv->preview_vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_grid_attach (GTK_GRID (grid), tpref->priv->preview_vbox, 1, 2, 1, 1);
	tpref->priv->preview_none = gtk_label_new (_("none"));
	gtk_widget_set_halign (tpref->priv->preview_none, GTK_ALIGN_START);
	gtk_box_pack_start (GTK_BOX (tpref->priv->preview_vbox), tpref->priv->preview_none, FALSE, FALSE, 0);

	label = gtk_label_new (_("Preview:"));
	gtk_widget_set_tooltip_text (label, _("Free form to test the configured\n"
					      "data entry"));
	gtk_widget_set_halign (label, GTK_ALIGN_START);
	gtk_grid_attach (GTK_GRID (grid), label, 0, 2, 1, 1);

	gtk_widget_show_all (grid);

	return grid;
}

static void
plugin_hash_foreach_func (G_GNUC_UNUSED const gchar *plugin_name, GdauiPlugin *plugin, ForeachData *fdata)
{
	gboolean add_it = FALSE;
	if ((plugin->nb_g_types == 0) || (!plugin->plugin_file && !plugin->options_xml_spec))
		return;

	if (! fdata->type)
		add_it = TRUE;
	else {
		gsize i;
		for (i = 0; i < plugin->nb_g_types; i++) {
			if (fdata->type == plugin->valid_g_types[i]) {
				add_it = TRUE;
				break;
			}
		}
	}

	if (add_it) {
		GtkTreeIter iter;
		gtk_list_store_append (fdata->store, &iter);
		gtk_list_store_set (fdata->store, &iter,
				    PL_COLUMN_PLUGIN, plugin,
				    PL_COLUMN_DESCR, plugin->plugin_descr,
				    -1);
	}
}

static void
update_column_properties (TablePreferences *tpref)
{
	GtkTreeIter iter;
	GtkListStore *store;

	store = GTK_LIST_STORE (tpref->priv->plugins_model);
	gtk_list_store_clear (store);
	gtk_list_store_append (store, &iter);
	gtk_list_store_set (store, &iter,
			    PL_COLUMN_PLUGIN, NULL,
			    PL_COLUMN_DESCR, "Default",
			    -1);

	if (tpref->priv->current_column) {
		ForeachData data;
		data.type = tpref->priv->current_column->gtype;
		data.store = store;
		g_hash_table_foreach (gdaui_plugins_hash, (GHFunc) plugin_hash_foreach_func, &data);
		gtk_widget_set_sensitive (tpref->priv->field_props, TRUE);
	}
	else
		gtk_widget_set_sensitive (tpref->priv->field_props, FALSE);

	if (! tpref->priv->current_table ||
	    ! tpref->priv->current_column) {
		gtk_combo_box_set_active (GTK_COMBO_BOX (tpref->priv->plugins_combo), 0);
		return;
	}

	/* load existing preference */
	gchar *eprops;
	GError *error = NULL;
	tpref->priv->save_plugin_changes = FALSE;
	eprops = t_connection_get_table_column_attribute (tpref->priv->tcnc,
								tpref->priv->current_table,
								tpref->priv->current_column,
								T_CONNECTION_COLUMN_PLUGIN, &error);
	if (error) {
		TO_IMPLEMENT; /* FIXME: add a notice somewhere in the UI */
		g_warning ("Error: %s\n", error->message);
		g_clear_error (&error);
		gtk_combo_box_set_active (GTK_COMBO_BOX (tpref->priv->plugins_combo), 0);
	}
	else if (eprops) {
		gchar *options;
		for (options = eprops; *options && (*options != ':'); options++);
		if (*options) {
			*options = 0;
			options++;
		}
		else
			options = NULL;

		GtkTreeIter iter;
		gboolean valid;
		for (valid = gtk_tree_model_get_iter_first (tpref->priv->plugins_model, &iter);
		     valid;
		     valid = gtk_tree_model_iter_next (tpref->priv->plugins_model, &iter)) {
			GdauiPlugin *plugin;
			gtk_tree_model_get (tpref->priv->plugins_model, &iter, PL_COLUMN_PLUGIN, &plugin, -1);
			if (plugin && !strcmp (plugin->plugin_name, eprops)) {
				gtk_combo_box_set_active_iter (GTK_COMBO_BOX (tpref->priv->plugins_combo), &iter);
				break;
			}
		}

		g_free (eprops);
	}
	else
		gtk_combo_box_set_active (GTK_COMBO_BOX (tpref->priv->plugins_combo), 0);
	tpref->priv->save_plugin_changes = TRUE;
}

static void set_preview_widget (TablePreferences *tpref);
static void options_form_param_changed_cb (GdauiBasicForm *form, GdaHolder *param, gboolean is_user_modif,
					   TablePreferences *tpref);

static void
plugins_combo_changed_cb (GtkComboBox *combo, TablePreferences *tpref)
{
	GtkTreeIter iter;
	GtkWidget *old_options = NULL;

	if (tpref->priv->options_wid) {
		old_options = tpref->priv->options_wid;
		tpref->priv->options_wid = NULL;
	}
	
	if (gtk_combo_box_get_active_iter (combo, &iter)) {
		GdauiPlugin *plugin;
		GtkTreeModel *model;
		GError *error = NULL;

		model = gtk_combo_box_get_model (combo);
		gtk_tree_model_get (model, &iter, PL_COLUMN_PLUGIN, &plugin, -1);
		if (plugin && plugin->options_xml_spec) {
			GdaSet *plist;
			
			plist = gda_set_new_from_spec_string (plugin->options_xml_spec, &error);
			if (!plist) {
				g_warning ("Cannot parse XML spec for plugin options: %s",
					   error && error->message ? error->message : "No detail");
				g_clear_error (&error);
			}
			else {
				if (!old_options ||
				    (g_object_get_data (G_OBJECT (old_options), "plugin") != plugin)) {
					tpref->priv->options_wid = gdaui_basic_form_new (plist);
					g_object_set_data (G_OBJECT (tpref->priv->options_wid), "plugin", plugin);
					g_signal_connect (G_OBJECT (tpref->priv->options_wid), "holder-changed",
							  G_CALLBACK (options_form_param_changed_cb), tpref);

					gtk_box_pack_start (GTK_BOX (tpref->priv->options_vbox),
							    tpref->priv->options_wid, TRUE, TRUE, 0);
				}
				else {
					tpref->priv->options_wid = old_options;
					old_options = NULL;
				}
				g_object_unref (plist);
			}

			if (tpref->priv->options_wid) {
				plist = gdaui_basic_form_get_data_set (GDAUI_BASIC_FORM (tpref->priv->options_wid));
				gtk_widget_show (tpref->priv->options_wid);
				gtk_widget_show (tpref->priv->options_label);

				if (plist && !tpref->priv->save_plugin_changes) {
					/* load plugin options */
					GtkTreeSelection *select;
					GtkTreeIter citer;

					select = gtk_tree_view_get_selection (tpref->priv->columns_treeview);
					if (gtk_tree_selection_get_selected (select, NULL, &citer)) {
						gchar *plugin_str;

						gtk_tree_model_get (GTK_TREE_MODEL (tpref->priv->columns_store),
								    &citer, COLUMN_PLUGIN, &plugin_str, -1);
						/*g_print ("%p PLUGIN_STR:[%s]\n", tpref, plugin_str);*/
						if (plugin_str) {
							GdaQuarkList *ql;
							GSList *list;
							gchar *tmp;
							for (tmp = plugin_str; *tmp && (*tmp != ':'); tmp++);
							if (*tmp == ':') {
								ql = gda_quark_list_new_from_string (tmp+1);
								for (list = gda_set_get_holders (plist); list; list = list->next) {
									GdaHolder *holder = GDA_HOLDER (list->data);
									const gchar *cstr;
									cstr = gda_quark_list_find (ql, gda_holder_get_id (holder));
									if (cstr)
										gda_holder_set_value_str (holder, NULL, cstr, NULL);
									else
										gda_holder_set_value (holder, NULL, NULL);
								}
								gda_quark_list_free (ql);
							}
							g_free (plugin_str);
						}
					}
				}
			}
		}

		if (tpref->priv->save_plugin_changes &&
		    tpref->priv->current_table &&
		    tpref->priv->current_column &&
		    ! t_connection_set_table_column_attribute (tpref->priv->tcnc,
								     tpref->priv->current_table,
								     tpref->priv->current_column,
								     T_CONNECTION_COLUMN_PLUGIN,
								     plugin ? plugin->plugin_name : NULL,
								     &error)) {
			TO_IMPLEMENT; /* FIXME: add a notice somewhere in the UI */
			g_warning ("Error: %s\n", error && error->message ? error->message : _("No detail"));
			g_clear_error (&error);
		}

		set_preview_widget (tpref);
	}

	if (old_options)
		gtk_widget_destroy (old_options);

	if (! tpref->priv->options_wid)
		gtk_widget_hide (tpref->priv->options_label);
}

static void
options_form_param_changed_cb (G_GNUC_UNUSED GdauiBasicForm *form, G_GNUC_UNUSED GdaHolder *param,
			       G_GNUC_UNUSED gboolean is_user_modif, TablePreferences *tpref)
{
	GtkTreeIter iter;

	if (tpref->priv->save_plugin_changes &&
	    gtk_combo_box_get_active_iter (GTK_COMBO_BOX (tpref->priv->plugins_combo), &iter)) {
		GdauiPlugin *plugin;
		GError *error = NULL;
		GString *plugin_all = NULL;

		gtk_tree_model_get (tpref->priv->plugins_model, &iter, PL_COLUMN_PLUGIN, &plugin, -1);
		if (plugin) {
			plugin_all = g_string_new (plugin->plugin_name);
			if (tpref->priv->options_wid) {
				GdaSet *plist;
				GSList *list;
				gboolean first = TRUE;
				plist = gdaui_basic_form_get_data_set (GDAUI_BASIC_FORM (tpref->priv->options_wid));
				for (list = gda_set_get_holders (plist); list; list = list->next) {
					GdaHolder *holder;
					const GValue *cvalue;
					gchar *str, *r1, *r2;
					holder = GDA_HOLDER (list->data);
					if (! gda_holder_is_valid (holder))
						continue;

					cvalue = gda_holder_get_value (holder);
					if (G_VALUE_TYPE (cvalue) == GDA_TYPE_NULL)
						continue;
					if (first) {
						g_string_append_c (plugin_all, ':');
						first = FALSE;
					}
					else
						g_string_append_c (plugin_all, ';');
					str = gda_value_stringify (cvalue);
					r1 = gda_rfc1738_encode (str);
					g_free (str);
					r2 = gda_rfc1738_encode (gda_holder_get_id (holder));
					g_string_append_printf (plugin_all, "%s=%s", r2, r1);
					g_free (r1);
					g_free (r2);
				}
			}
		}
		
		g_signal_handlers_block_by_func (tpref->priv->columns_store,
						 G_CALLBACK (columns_model_row_changed_cb), tpref);
		if (tpref->priv->current_table &&
		    tpref->priv->current_column &&
		    ! t_connection_set_table_column_attribute (tpref->priv->tcnc,
								     tpref->priv->current_table,
								     tpref->priv->current_column,
								     T_CONNECTION_COLUMN_PLUGIN,
								     plugin_all ? plugin_all->str : NULL,
								     &error)) {
			TO_IMPLEMENT; /* FIXME: add a notice somewhere in the UI */
			g_warning ("Error: %s\n", error && error->message ? error->message : _("No detail"));
			g_clear_error (&error);
		}
		g_signal_handlers_unblock_by_func (tpref->priv->columns_store,
						   G_CALLBACK (columns_model_row_changed_cb), tpref);

		if (plugin_all)
			g_string_free (plugin_all, TRUE);
	}
	set_preview_widget (tpref);
}

static void
set_preview_widget (TablePreferences *tpref)
{
	GtkWidget *preview = NULL;
	GtkTreeIter iter;

	if (!tpref->priv->current_column)
		return;

	if (gtk_combo_box_get_active_iter (GTK_COMBO_BOX (tpref->priv->plugins_combo), &iter)) {
		GdauiPlugin *plugin;
		GtkTreeModel *model;
		GType gtype;

		gtype = tpref->priv->current_column->gtype;
		
		model = tpref->priv->plugins_model;
		gtk_tree_model_get (model, &iter, PL_COLUMN_PLUGIN, &plugin, -1);
		if (plugin) {
			GString *string = NULL;
			if (tpref->priv->options_wid) {
				GdaSet *plist;
				GSList *list;
				
				plist = gdaui_basic_form_get_data_set (GDAUI_BASIC_FORM (tpref->priv->options_wid));
				for (list = gda_set_get_holders (plist); list; list = list->next) {
					GdaHolder *holder;
					holder = GDA_HOLDER (list->data);
					if (gda_holder_is_valid (holder)) {
						const GValue *cvalue;
						cvalue = gda_holder_get_value (holder);
						if (cvalue && (G_VALUE_TYPE (cvalue) != GDA_TYPE_NULL)) {
							gchar *str = gda_value_stringify (cvalue);
							gchar *r1, *r2;
							if (!string)
								string = g_string_new ("");
							else
								g_string_append_c (string, ';');
							r1 = gda_rfc1738_encode (gda_holder_get_id (holder));
							r2 = gda_rfc1738_encode (str);
							g_free (str);
							g_string_append_printf (string, "%s=%s", r1, r2);
							g_free (r1);
							g_free (r2);
						}
					}
				}
			}
			if (string) {
				g_string_prepend_c (string, ':');
				g_string_prepend (string, plugin->plugin_name);
				preview = GTK_WIDGET (gdaui_new_data_entry (gtype, string->str));
				g_string_free (string, TRUE);
			}
			else
				preview = GTK_WIDGET (gdaui_new_data_entry (gtype, plugin->plugin_name));
		}
		else
			preview = GTK_WIDGET (gdaui_new_data_entry (gtype, NULL));
	}

	GValue *prev_value = NULL;
	if (tpref->priv->preview_wid) {
		prev_value = gdaui_data_entry_get_value (GDAUI_DATA_ENTRY (tpref->priv->preview_wid));
		gtk_widget_destroy (tpref->priv->preview_wid);
		gtk_widget_show (tpref->priv->preview_none);
		tpref->priv->preview_wid = NULL;
	}
	if (preview) {
		if (prev_value &&
		    (G_VALUE_TYPE (prev_value) == gdaui_data_entry_get_value_type (GDAUI_DATA_ENTRY (preview))))
			gdaui_data_entry_set_value (GDAUI_DATA_ENTRY (preview), prev_value);
		tpref->priv->preview_wid = preview;
		gtk_box_pack_start (GTK_BOX (tpref->priv->preview_vbox), preview, TRUE, TRUE, 0);
		gtk_widget_hide (tpref->priv->preview_none);
		gtk_widget_show (tpref->priv->preview_wid);
	}
	if (prev_value)
		gda_value_free (prev_value);
}
