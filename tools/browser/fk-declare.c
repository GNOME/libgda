/*
 * Copyright (C) 2011 - 2015 Vivien Malerba <malerba@gnome-db.org>
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
#include <sql-parser/gda-sql-parser.h>
#include "fk-declare.h"
#include "ui-support.h"
#include <t-utils.h>

/* 
 * Main static functions 
 */
static void fk_declare_class_init (FkDeclareClass * class);
static void fk_declare_init (FkDeclare *declare);
static void fk_declare_dispose (GObject *object);

enum {
	MODEL_COLUMNS_COLUMN_PIXBUF,
	MODEL_COLUMNS_COLUMN_STRING,
	MODEL_COLUMNS_COLUMN_META_COLUMN,
	MODEL_COLUMNS_COLUMN_LAST
};

enum {
	MODEL_TABLES_COLUMN_PIXBUF,
	MODEL_TABLES_COLUMN_STRING,
	MODEL_TABLES_COLUMN_META_TABLE,
	MODEL_TABLES_COLUMN_LAST
};
static GtkTreeModel *create_tables_model (GdaMetaStruct *mstruct);
static void update_reference_column_choices (FkDeclare *dec);
static void update_dialog_response_sensitiveness (FkDeclare *dec);

/* get a pointer to the parents to be able to call their destructor */
static GObjectClass  *parent_class = NULL;

typedef struct {
	GtkWidget *checkbox; /* column name chackbox */
	GtkComboBox *cbox; /* referenced column combo box */
	GdaMetaTableColumn *column;
} Assoc;

struct _FkDeclarePrivate
{
	GdaMetaStruct *mstruct;
	GdaMetaTable *mtable;
	GtkWidget *fk_name;
	GtkComboBox *ref_table_cbox;
	gint n_cols; /* length (@mtable->columns) */
	Assoc *associations; /* size is @n_cols */
	
	gboolean dialog_sensitive;
};

GType
fk_declare_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static GMutex registering;
		static const GTypeInfo info = {
			sizeof (FkDeclareClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) fk_declare_class_init,
			NULL,
			NULL,
			sizeof (FkDeclare),
			0,
			(GInstanceInitFunc) fk_declare_init,
			0
		};
		
		g_mutex_lock (&registering);
		if (type == 0)
			type = g_type_register_static (GTK_TYPE_DIALOG, "FkDeclare", &info, 0);
		g_mutex_unlock (&registering);
	}

	return type;
}


static void
fk_declare_class_init (FkDeclareClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);

	parent_class = g_type_class_peek_parent (class);

	/* virtual functions */
	object_class->dispose = fk_declare_dispose;
}

#ifdef HAVE_GDU
static void
help_clicked_cb (GtkButton *button, G_GNUC_UNUSED FkDeclare *declare)
{
	ui_show_help ((GtkWindow*) gtk_widget_get_toplevel ((GtkWidget*) button), "declared-fk");
	g_signal_stop_emission_by_name (button, "clicked");
}
#endif

static void
fk_declare_init (FkDeclare *declare)
{
	declare->priv = g_new0 (FkDeclarePrivate, 1);
	declare->priv->dialog_sensitive = FALSE;

	gtk_dialog_add_buttons (GTK_DIALOG (declare),
				_("_Add"), GTK_RESPONSE_ACCEPT,
				_("_Cancel"), GTK_RESPONSE_REJECT,
				NULL);
#ifdef HAVE_GDU
	GtkWidget *help_btn;
	help_btn = gtk_button_new_from_icon_name ("help-browser", GTK_ICON_SIZE_BUTTON);
	g_signal_connect (help_btn, "clicked",
			  G_CALLBACK (help_clicked_cb), declare);
	gtk_widget_show (help_btn);
	gtk_dialog_add_action_widget (GTK_DIALOG (declare), help_btn, GTK_RESPONSE_HELP);
#endif

	gtk_dialog_set_response_sensitive (GTK_DIALOG (declare), GTK_RESPONSE_ACCEPT, FALSE);
}

static void
fk_declare_dispose (GObject *object)
{
	FkDeclare *declare;
	declare = FK_DECLARE (object);
	if (declare->priv) {
		g_free (declare->priv->associations);
		if (declare->priv->mstruct)
			g_object_unref (declare->priv->mstruct);
		
		g_free (declare->priv);
		declare->priv = NULL;
	}

	/* parent class */
        parent_class->dispose (object);
}

static void is_node_sensitive (GtkCellLayout *cell_layout, GtkCellRenderer *cell, GtkTreeModel *tree_model,
			       GtkTreeIter *iter, gpointer data);
static void fk_name_changed_cb (GtkWidget *entry, FkDeclare *decl);
static void table_selection_changed_cb (GtkComboBox *cbox, FkDeclare *decl);
static void column_toggled_cb (GtkToggleButton *toggle, FkDeclare *decl);
static void column_selection_changed_cb (GtkComboBox *cbox, FkDeclare *decl);

static void
create_internal_layout (FkDeclare *decl)
{
	GtkWidget *label, *grid, *cbox, *entry;
	char *markup, *str;
	GtkWidget *dcontents;
	gint i;
	GSList *list;

	dcontents = gtk_dialog_get_content_area (GTK_DIALOG (decl));
	gtk_box_set_spacing (GTK_BOX (dcontents), 5);

	/* label */
	label = gtk_label_new ("");
	gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
	str = g_strdup_printf (_("Declare a foreign key for table '%s'"),
			       GDA_META_DB_OBJECT (decl->priv->mtable)->obj_short_name);
	markup = g_markup_printf_escaped ("<big><b>%s:</b></big>\n%s\n", str,
					  _("define which table is referenced, which columns are "
					    "part of the foreign key, "
					    "and which column each one references"));
	g_free (str);
	gtk_label_set_markup (GTK_LABEL (label), markup);
	gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
	gtk_label_set_width_chars (GTK_LABEL (label), 50);
	g_free (markup);
	gtk_widget_set_halign (label, GTK_ALIGN_START);
	gtk_box_pack_start (GTK_BOX (dcontents), label, FALSE, FALSE, 0);
	gtk_widget_show (label);

	/* GtkTable to hold contents */
	grid = gtk_grid_new ();
	gtk_box_pack_start (GTK_BOX (dcontents), grid, TRUE, TRUE, 0);
	gtk_grid_set_column_spacing (GTK_GRID (grid), 5);
	gtk_grid_set_row_spacing (GTK_GRID (grid), 5);

	/* FK name */
	label = gtk_label_new (_("Foreign key name:"));
	gtk_widget_set_halign (label, GTK_ALIGN_START);
	gtk_grid_attach (GTK_GRID (grid), label, 0, 0, 1, 1);
	entry = gtk_entry_new ();
	decl->priv->fk_name = entry;
	gtk_grid_attach (GTK_GRID (grid), entry, 1, 0, 1, 1);
	g_signal_connect (entry, "changed",
			  G_CALLBACK (fk_name_changed_cb), decl);

	/* table to reference */
	label = gtk_label_new (_("Referenced table:"));
	gtk_widget_set_halign (label, GTK_ALIGN_START);
	gtk_grid_attach (GTK_GRID (grid), label, 0, 1, 1, 1);

	GtkTreeModel *model;
	GtkCellRenderer *renderer;
	model = create_tables_model (decl->priv->mstruct);
	cbox = gtk_combo_box_new_with_model (model);
	decl->priv->ref_table_cbox = GTK_COMBO_BOX (cbox);
	g_signal_connect (cbox, "changed",
			  G_CALLBACK (table_selection_changed_cb), decl);
	g_object_unref (G_OBJECT (model));
	renderer = gtk_cell_renderer_pixbuf_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (cbox), renderer, FALSE);
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (cbox), renderer,
					"pixbuf", MODEL_TABLES_COLUMN_PIXBUF,
					NULL);
	gtk_cell_layout_set_cell_data_func (GTK_CELL_LAYOUT (cbox),
					    renderer,
					    is_node_sensitive,
					    NULL, NULL);

	renderer = gtk_cell_renderer_text_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (cbox), renderer, TRUE);
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (cbox), renderer,
					"text", MODEL_TABLES_COLUMN_STRING,
					NULL);
	gtk_cell_layout_set_cell_data_func (GTK_CELL_LAYOUT (cbox),
					    renderer,
					    is_node_sensitive,
					    NULL, NULL);
	gtk_grid_attach (GTK_GRID (grid), cbox, 1, 1, 1, 1);

	/* more labels */
	label = gtk_label_new ("");
	markup = g_strdup_printf ("<b>%s:</b>", _("Columns"));
	gtk_label_set_markup (GTK_LABEL (label), markup);
	g_free (markup);
	gtk_widget_set_halign (label, GTK_ALIGN_START);
	gtk_grid_attach (GTK_GRID (grid), label, 0, 2, 1, 1);

	label = gtk_label_new ("");
	markup = g_strdup_printf ("<b>%s:</b>", _("Referenced column"));
	gtk_label_set_markup (GTK_LABEL (label), markup);
	g_free (markup);
	gtk_widget_set_halign (label, GTK_ALIGN_START);
	gtk_grid_attach (GTK_GRID (grid), label, 1, 2, 1, 1);

	/* columns */
	decl->priv->n_cols = g_slist_length (decl->priv->mtable->columns);
	decl->priv->associations = g_new0 (Assoc, decl->priv->n_cols);
	for (i = 3, list = decl->priv->mtable->columns; list; i++, list = list->next) {
		GtkCellRenderer *renderer;
		GdaMetaTableColumn *column = (GdaMetaTableColumn*) list->data;
		Assoc *assoc = &(decl->priv->associations [i-3]);
		assoc->column = column;

		label = gtk_check_button_new_with_label (column->column_name);
		gtk_grid_attach (GTK_GRID (grid), label, 0, i, 1, 1);
		assoc->checkbox = label;
		g_signal_connect (label, "toggled",
				  G_CALLBACK (column_toggled_cb), decl);

		cbox = gtk_combo_box_new ();
		g_object_set_data (G_OBJECT (label), "cbox", cbox);
		renderer = gtk_cell_renderer_pixbuf_new ();
		gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (cbox), renderer, FALSE);
		gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (cbox), renderer,
						"pixbuf", MODEL_COLUMNS_COLUMN_PIXBUF,
						NULL);		
		renderer = gtk_cell_renderer_text_new ();
		gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (cbox), renderer, TRUE);
		gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (cbox), renderer,
						"text", MODEL_COLUMNS_COLUMN_STRING,
						NULL);
		gtk_grid_attach (GTK_GRID (grid), cbox, 1, i, 1, 1);
		assoc->cbox = GTK_COMBO_BOX (cbox);
		g_signal_connect (cbox, "changed",
				  G_CALLBACK (column_selection_changed_cb), decl);
		gtk_widget_set_sensitive (cbox, FALSE);
	}
	gtk_widget_show_all (grid);
}

static void
is_node_sensitive (G_GNUC_UNUSED GtkCellLayout *cell_layout,
		   GtkCellRenderer *cell, GtkTreeModel *tree_model,
		   GtkTreeIter *iter, G_GNUC_UNUSED gpointer data)
{
	gboolean sensitive;
	sensitive = !gtk_tree_model_iter_has_child (tree_model, iter);
	g_object_set (cell, "sensitive", sensitive, NULL);
}

static void
fk_name_changed_cb (G_GNUC_UNUSED GtkWidget *entry, FkDeclare *decl)
{
	update_dialog_response_sensitiveness (decl);
}

static void
table_selection_changed_cb (G_GNUC_UNUSED GtkComboBox *cbox, FkDeclare *decl)
{
	update_reference_column_choices (decl);
	update_dialog_response_sensitiveness (decl);
}

static void
column_toggled_cb (GtkToggleButton *toggle, FkDeclare *decl)
{
	GtkWidget *cbox;
	cbox = g_object_get_data (G_OBJECT (toggle), "cbox");
	gtk_widget_set_sensitive (cbox, gtk_toggle_button_get_active (toggle));
	update_dialog_response_sensitiveness (decl);
}

static void
column_selection_changed_cb (G_GNUC_UNUSED GtkComboBox *cbox, FkDeclare *decl)
{
	update_dialog_response_sensitiveness (decl);
}

static void
update_reference_column_choices (FkDeclare *decl)
{
	gint i;
	GtkTreeIter iter;
	GdaMetaTable *mtable = NULL;
	
	if (gtk_combo_box_get_active_iter (decl->priv->ref_table_cbox, &iter))
		gtk_tree_model_get (gtk_combo_box_get_model (decl->priv->ref_table_cbox), &iter,
				    MODEL_TABLES_COLUMN_META_TABLE, &mtable, -1);

	for (i = 0; i < decl->priv->n_cols; i++) {
		Assoc *assoc = &(decl->priv->associations [i]);
		GtkListStore *lstore;
		lstore = (GtkListStore*) gtk_combo_box_get_model (assoc->cbox);
		if (! lstore) {
			lstore = gtk_list_store_new (3, GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_POINTER);
			gtk_combo_box_set_model (assoc->cbox, GTK_TREE_MODEL (lstore));
			g_object_unref (lstore);
		}
		else
			gtk_list_store_clear (lstore);

		if (!mtable)
			continue;

		/* add columns */
		GSList *list;
		for (list = mtable->columns; list; list = list->next) {
			GdaMetaTableColumn *col = (GdaMetaTableColumn*) list->data;
			GdkPixbuf *pix;
			gtk_list_store_append (lstore, &iter);
			pix = ui_get_pixbuf_icon (UI_ICON_COLUMN);
			gtk_list_store_set (lstore, &iter,
					    MODEL_COLUMNS_COLUMN_PIXBUF, pix,
					    MODEL_COLUMNS_COLUMN_STRING, col->column_name,
					    MODEL_COLUMNS_COLUMN_META_COLUMN, col, -1);
		}
	}
}

/**
 * fk_declare_new
 */
GtkWidget  *
fk_declare_new (GtkWindow *parent, GdaMetaStruct *mstruct, GdaMetaTable *table)
{
	GtkWidget *wid;
	FkDeclare *decl;
	gchar *str;

	g_return_val_if_fail (GDA_IS_META_STRUCT (mstruct), NULL);
	g_return_val_if_fail (table, NULL);
	g_return_val_if_fail (GDA_META_DB_OBJECT (table)->obj_type == GDA_META_DB_TABLE, NULL);
	g_return_val_if_fail (table->columns, NULL);

	str = g_strdup_printf (_("Declare a foreign key for table '%s'"),
			       GDA_META_DB_OBJECT (table)->obj_short_name);
	wid = (GtkWidget*) g_object_new (FK_DECLARE_TYPE, "title", str,
					 "transient-for", parent,
					 "border-width", 10, NULL);
	g_free (str);

	decl = FK_DECLARE (wid);
	decl->priv->mstruct = GDA_META_STRUCT (g_object_ref ((GObject*) mstruct));
	decl->priv->mtable = table;

	create_internal_layout (decl);

	return wid;
}

static gint
dbo_sort_func (GdaMetaDbObject *dbo1, GdaMetaDbObject *dbo2)
{
	const gchar *n1, *n2;
	g_assert (dbo1);
	g_assert (dbo2);
	if (dbo1->obj_name[0] ==  '"')
		n1 = dbo1->obj_name + 1;
	else
		n1 = dbo1->obj_name;
	if (dbo2->obj_name[0] ==  '"')
		n2 = dbo2->obj_name + 1;
	else
		n2 = dbo2->obj_name;
	return strcmp (n2, n1);
}

static GtkTreeModel *
create_tables_model (GdaMetaStruct *mstruct)
{
	GtkTreeStore *tstore;
	GSList *all_dbo, *list;
	GHashTable *schemas = NULL; /* key = schema name, value = a #GtkTreeRowReference as parent */

	tstore = gtk_tree_store_new (MODEL_TABLES_COLUMN_LAST, GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_POINTER);
	schemas = g_hash_table_new_full (g_str_hash, g_str_equal,
					 NULL, (GDestroyNotify) gtk_tree_row_reference_free);
	all_dbo = gda_meta_struct_get_all_db_objects (mstruct);
	all_dbo = g_slist_sort (all_dbo, (GCompareFunc) dbo_sort_func);
	for (list = all_dbo; list; list = list->next) {
		GdaMetaDbObject *dbo = GDA_META_DB_OBJECT (list->data);
		GtkTreeIter iter;
		GdkPixbuf *pix;
		if (dbo->obj_type != GDA_META_DB_TABLE)
			continue;
		
		if (strcmp (dbo->obj_short_name, dbo->obj_full_name)) {
			gtk_tree_store_prepend (tstore, &iter, NULL);
			pix = ui_get_pixbuf_icon (UI_ICON_TABLE);
			gtk_tree_store_set (tstore, &iter,
					    MODEL_TABLES_COLUMN_PIXBUF, pix,
					    MODEL_TABLES_COLUMN_STRING, dbo->obj_short_name,
					    MODEL_TABLES_COLUMN_META_TABLE, GDA_META_TABLE (dbo), -1);
		}

		GtkTreePath *path;
		GtkTreeRowReference *rref;
		GtkTreeIter parent;
		rref = g_hash_table_lookup (schemas, dbo->obj_schema);
		if (!rref) {
			GtkTreeIter iter;
			GtkTreePath *path;
			GdkPixbuf *pix;
			gtk_tree_store_append (tstore, &iter, NULL);
			pix = ui_get_pixbuf_icon (UI_ICON_SCHEMA);
			gtk_tree_store_set (tstore, &iter,
					    MODEL_TABLES_COLUMN_PIXBUF, pix,
					    MODEL_TABLES_COLUMN_STRING, dbo->obj_schema,
					    MODEL_TABLES_COLUMN_META_TABLE, NULL, -1);
			path = gtk_tree_model_get_path ((GtkTreeModel*) tstore, &iter);
			rref = gtk_tree_row_reference_new ((GtkTreeModel*) tstore, path);
			gtk_tree_path_free (path);
			g_hash_table_insert (schemas, dbo->obj_schema, rref);
		}
		
		path = gtk_tree_row_reference_get_path (rref);
		g_assert (gtk_tree_model_get_iter ((GtkTreeModel*) tstore, &parent, path));
		gtk_tree_path_free (path);
		gtk_tree_store_prepend (tstore, &iter, &parent);
		pix = ui_get_pixbuf_icon (UI_ICON_TABLE);
		gtk_tree_store_set (tstore, &iter,
				    MODEL_TABLES_COLUMN_PIXBUF, pix,
				    MODEL_TABLES_COLUMN_STRING, dbo->obj_short_name,
				    MODEL_TABLES_COLUMN_META_TABLE, GDA_META_TABLE (dbo), -1);
	}
	g_slist_free (all_dbo);
	g_hash_table_destroy (schemas);

	return GTK_TREE_MODEL (tstore);
}

/*
 * Sets the dialog's sensitiveness for the GTK_RESPONSE_ACCEPT response
 *
 * It is sensitive if:
 *  - the FK is named
 *  - a reference table is selected
 *  - at least one column is checked
 *  - for each checked column, there is a selected reference column
 */
static void
update_dialog_response_sensitiveness (FkDeclare *decl)
{
	gboolean sensitive = FALSE;
	gint i;
	gboolean onechecked = FALSE;
	const gchar *fkname;

	fkname = gtk_entry_get_text (GTK_ENTRY (decl->priv->fk_name));
	if (!fkname || !*fkname)
		goto out;

	if (gtk_combo_box_get_active (decl->priv->ref_table_cbox) == -1)
		goto out;

	sensitive = TRUE;
	for (i = 0; i < decl->priv->n_cols; i++) {
		Assoc *assoc = &(decl->priv->associations [i]);
		if (! gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (assoc->checkbox)))
			continue;

		onechecked = TRUE;
		if (gtk_combo_box_get_active (assoc->cbox) == -1)
			sensitive = FALSE;
	}

	if (! onechecked) {
		sensitive = FALSE;
		goto out;
	}

 out:
	decl->priv->dialog_sensitive = sensitive;
	gtk_dialog_set_response_sensitive (GTK_DIALOG (decl), GTK_RESPONSE_ACCEPT, sensitive);
}

/**
 * fk_declare_write
 * @decl: a #FkDeclare widget
 * @bwin: (nullable): a #BrowserWindow, or %NULL
 * @error: a place to store errors or %NULL
 *
 * Actually declares the new foreign key in the meta store
 *
 * Returns: %TRUE if no error occurred
 */ 
gboolean
fk_declare_write (FkDeclare *decl, BrowserWindow *bwin, GError **error)
{
	gboolean retval = FALSE;

	g_return_val_if_fail (IS_FK_DECLARE (decl), FALSE);
	g_return_val_if_fail (!bwin || BROWSER_IS_WINDOW (bwin), FALSE);

	if (! decl->priv->dialog_sensitive) {
		g_set_error (error, T_ERROR, T_COMMAND_ARGUMENTS_ERROR,
			     "%s", _("Missing information to declare foreign key"));
		return FALSE;
	}

	GdaMetaTable *mtable = NULL;
	GtkTreeIter iter;
	g_assert (gtk_combo_box_get_active_iter (decl->priv->ref_table_cbox, &iter));
	gtk_tree_model_get (gtk_combo_box_get_model (decl->priv->ref_table_cbox), &iter,
			    MODEL_TABLES_COLUMN_META_TABLE, &mtable, -1);

	GdaMetaStore *mstore;
	gchar **colnames, **ref_colnames;
	colnames = g_new0 (gchar *, decl->priv->n_cols);
	ref_colnames = g_new0 (gchar *, decl->priv->n_cols);
	gint i, j;
	for (i = 0, j = 0; i < decl->priv->n_cols; i++) {
		Assoc *assoc = &(decl->priv->associations [i]);
		if (! gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (assoc->checkbox)))
			continue;
		colnames [j] = assoc->column->column_name;
		g_assert (gtk_combo_box_get_active_iter (assoc->cbox, &iter));
		GdaMetaTableColumn *ref_column;
		gtk_tree_model_get (gtk_combo_box_get_model (assoc->cbox), &iter,
				    MODEL_TABLES_COLUMN_META_TABLE, &ref_column, -1);
		g_assert (ref_column);
		ref_colnames [j] = ref_column->column_name;
		j++;
	}

	g_object_get (G_OBJECT (decl->priv->mstruct), "meta-store", &mstore, NULL);
	retval = gda_meta_store_declare_foreign_key (mstore, NULL,
						     gtk_entry_get_text (GTK_ENTRY (decl->priv->fk_name)),
						     GDA_META_DB_OBJECT (decl->priv->mtable)->obj_catalog,
						     GDA_META_DB_OBJECT (decl->priv->mtable)->obj_schema,
						     GDA_META_DB_OBJECT (decl->priv->mtable)->obj_name,
						     GDA_META_DB_OBJECT (mtable)->obj_catalog,
						     GDA_META_DB_OBJECT (mtable)->obj_schema,
						     GDA_META_DB_OBJECT (mtable)->obj_name,
						     j, colnames, ref_colnames, error);
	g_free (colnames);
	g_free (ref_colnames);

	if (retval && bwin) {
		TConnection *tcnc;
		tcnc = browser_window_get_connection (bwin);
		t_connection_meta_data_changed (tcnc);
	}

	g_object_unref (mstore);
	return retval;
}

/**
 * fk_declare_undeclare:
 * @mstruct: the #GdaMEtaStruct to delete the FK from
 * @bwin: (nullable): a #BrowserWindow, or %NULL
 * @decl_fk: the #GdaMetaTableForeignKey fk to delete
 * @error: a place to store errors, or %NULL
 *
 * Deletes a declared FK.
 *
 * Returns: %TRUE if no error occurred
 */
gboolean
fk_declare_undeclare (GdaMetaStruct *mstruct, BrowserWindow *bwin, GdaMetaTableForeignKey *decl_fk,
		      GError **error)
{
	gboolean retval = FALSE;
	GdaMetaStore *mstore;

	g_return_val_if_fail (GDA_IS_META_STRUCT (mstruct), FALSE);
	g_return_val_if_fail (!bwin || BROWSER_IS_WINDOW (bwin), FALSE);
	g_return_val_if_fail (decl_fk, FALSE);
	if (!decl_fk->meta_table ||
	    !decl_fk->meta_table->obj_catalog ||
	    !decl_fk->meta_table->obj_schema ||
	    !decl_fk->meta_table->obj_name ||
	    !decl_fk->depend_on ||
	    !decl_fk->depend_on->obj_catalog ||
	    !decl_fk->depend_on->obj_schema ||
	    !decl_fk->depend_on->obj_name) {
		g_set_error (error, T_ERROR, T_COMMAND_ARGUMENTS_ERROR,
			     "%s", _("Missing information to undeclare foreign key"));
		return FALSE;
	}

	g_object_get (G_OBJECT (mstruct), "meta-store", &mstore, NULL);
	retval = gda_meta_store_undeclare_foreign_key (mstore, NULL,
						       decl_fk->fk_name,
						       decl_fk->meta_table->obj_catalog,
						       decl_fk->meta_table->obj_schema,
						       decl_fk->meta_table->obj_name,
						       decl_fk->depend_on->obj_catalog,
						       decl_fk->depend_on->obj_schema,
						       decl_fk->depend_on->obj_name,
						       error);
	if (retval && bwin) {
		TConnection *tcnc;
		tcnc = browser_window_get_connection (bwin);
		t_connection_meta_data_changed (tcnc);
	}

	g_object_unref (mstore);
	return retval;
}
