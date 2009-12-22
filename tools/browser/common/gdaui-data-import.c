/* gdaui-data-import.c
 *
 * Copyright (C) 2006 - 2008 Vivien Malerba
 *
 * This Library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

#include <string.h>
#include "gdaui-data-import.h"
#include <libgda/libgda.h>
#include <glib/gi18n-lib.h>
#include <libgda-ui/gdaui-combo.h>
#include <libgda-ui/gdaui-grid.h>
#include <libgda-ui/gdaui-raw-grid.h>
#include <libgda-ui/gdaui-data-proxy.h>
#include <libgda-ui/gdaui-data-proxy-info.h>
#include <libgda-ui/gdaui-data-selector.h>
#include <libgda/binreloc/gda-binreloc.h>

static void gdaui_data_import_class_init (GdauiDataImportClass *class);
static void gdaui_data_import_init (GdauiDataImport *wid);
static void gdaui_data_import_dispose (GObject *object);


enum {
	SEP_COMMA,
	SEP_SEMICOL,
	SEP_TAB,
	SEP_SPACE,
	SEP_PIPE,
	SEP_OTHER,
	SEP_LAST
};

struct _GdauiDataImportPriv
{
	GdaDataModel  *model;

	/* spec widgets */
	GtkWidget     *file_chooser;
	GtkWidget     *encoding_combo;
	GtkWidget     *first_line_check;
	GtkWidget     *sep_array [SEP_LAST];
	GtkWidget     *sep_other_entry;

	/* preview widgets */
	GtkWidget     *preview_box;
	GtkWidget     *no_data_label;
	GtkWidget     *preview_grid;
};

static void spec_changed_cb (GtkWidget *wid, GdauiDataImport *import);

/* get a pointer to the parents to be able to call their destructor */
static GObjectClass *parent_class = NULL;

GType
gdaui_data_import_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo info = {
			sizeof (GdauiDataImportClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gdaui_data_import_class_init,
			NULL,
			NULL,
			sizeof (GdauiDataImport),
			0,
			(GInstanceInitFunc) gdaui_data_import_init
		};		
		
		type = g_type_register_static (GTK_TYPE_VPANED, "GdauiDataImport", &info, 0);
	}

	return type;
}

static void
gdaui_data_import_class_init (GdauiDataImportClass * class)
{
	GObjectClass   *object_class = G_OBJECT_CLASS (class);
	
	parent_class = g_type_class_peek_parent (class);

	object_class->dispose = gdaui_data_import_dispose;
}

static void
gdaui_data_import_init (GdauiDataImport * import)
{
	GtkWidget *label, *vbox, *hbox;
	gchar *str;
	GtkWidget *table, *entry;
	GtkFileFilter *filter;
	GdaDataModel *encs;
	GSList *encs_errors;
	
	import->priv = g_new0 (GdauiDataImportPriv, 1);
	import->priv->model = NULL;

	/* 
	 * top part: import specs. 
	 */
	vbox = gtk_vbox_new (FALSE, 0);
        gtk_paned_pack1 (GTK_PANED (import), vbox, FALSE, FALSE);

	str = g_strdup_printf ("<b>%s:</b>", _("Import specifications"));
        label = gtk_label_new ("");
        gtk_label_set_markup (GTK_LABEL (label), str);
        gtk_misc_set_alignment (GTK_MISC (label), 0., -1);
        g_free (str);
        gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

        hbox = gtk_hbox_new (FALSE, 0); /* HIG */
        gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, TRUE, 0);
        gtk_widget_show (hbox);
        label = gtk_label_new ("    ");
        gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

	table = gtk_table_new (7, 4, FALSE);
	gtk_box_pack_start (GTK_BOX (hbox), table, TRUE, TRUE, 0);
	gtk_table_set_col_spacings (GTK_TABLE (table), 5);
	gtk_table_set_row_spacings (GTK_TABLE (table), 5);

	/* file to import from */
	label = gtk_label_new (_("File to import from:"));
	gtk_misc_set_alignment (GTK_MISC (label), 0., -1);
	gtk_table_attach (GTK_TABLE (table), label, 0, 1, 0, 1, GTK_SHRINK | GTK_FILL, 0, 0, 0);

	entry = gtk_file_chooser_button_new (_("File to import data from"), GTK_FILE_CHOOSER_ACTION_OPEN);
	import->priv->file_chooser = entry;
	filter = gtk_file_filter_new ();
	gtk_file_filter_set_name (filter, _("Comma separated values"));
	gtk_file_filter_add_pattern (filter, "*.csv");
	gtk_file_filter_add_pattern (filter, "*.txt");
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (entry), filter);
	filter = gtk_file_filter_new ();
	gtk_file_filter_set_name (filter, _("XML exported"));
	gtk_file_filter_add_pattern (filter, "*.xml");
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (entry), filter);
	filter = gtk_file_filter_new ();
	gtk_file_filter_set_name (filter, _("All files"));
	gtk_file_filter_add_pattern (filter, "*");
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (entry), filter);
	gtk_table_attach (GTK_TABLE (table), entry, 1, 4, 0, 1, GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
	g_signal_connect (G_OBJECT (entry), "selection-changed",
			  G_CALLBACK (spec_changed_cb), import);

	/* Encoding */ 
	label = gtk_label_new (_("Encoding:"));
	gtk_misc_set_alignment (GTK_MISC (label), 0., -1);
	gtk_table_attach (GTK_TABLE (table), label, 0, 1, 1, 2, GTK_SHRINK | GTK_FILL, 0, 0, 0);

	gchar *fname = gda_gbr_get_file_path (GDA_DATA_DIR, LIBGDA_ABI_NAME, "import_encodings.xml", NULL);
	encs = gda_data_model_import_new_file (fname, TRUE, NULL);
	g_free (fname);
	encs_errors = gda_data_model_import_get_errors (GDA_DATA_MODEL_IMPORT (encs));
	if (encs_errors) {
		for (; encs_errors; encs_errors = g_slist_next (encs_errors)) {
			g_print ("Error: %s\n", encs_errors->data && ((GError *) encs_errors->data)->message ?
				 ((GError *) encs_errors->data)->message : _("no detail"));
		}
		import->priv->encoding_combo = NULL;
	}
	else {
		gint cols[] = {0};
		entry = gdaui_combo_new_with_model (encs, 1, cols);
		import->priv->encoding_combo = entry;
	}
	g_object_unref (encs);
	gtk_table_attach (GTK_TABLE (table), entry, 1, 4, 1, 2, GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
	g_signal_connect (G_OBJECT (entry), "changed",
			  G_CALLBACK (spec_changed_cb), import);

	/* first line as title */
	label = gtk_label_new (_("First line as title:"));
	gtk_misc_set_alignment (GTK_MISC (label), 0., -1);
	gtk_table_attach (GTK_TABLE (table), label, 0, 1, 2, 3, GTK_SHRINK | GTK_FILL, 0, 0, 0);
	
	entry = gtk_check_button_new ();
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (entry), TRUE);
	import->priv->first_line_check = entry,
	gtk_table_attach (GTK_TABLE (table), entry, 1, 3, 2, 3, GTK_SHRINK | GTK_FILL, 0, 0, 0);
	g_signal_connect (G_OBJECT (entry), "toggled",
			  G_CALLBACK (spec_changed_cb), import);

	/* separator */
	label = gtk_label_new (_("Separator:"));
	gtk_misc_set_alignment (GTK_MISC (label), 0., -1);
	gtk_table_attach (GTK_TABLE (table), label, 0, 1, 3, 4, GTK_SHRINK | GTK_FILL, 0, 0, 0);

	entry = gtk_radio_button_new_with_label (NULL, _("Comma"));
	import->priv->sep_array [SEP_COMMA] = entry;
	gtk_table_attach (GTK_TABLE (table), entry, 1, 2, 3, 4, GTK_FILL, 0, 0, 0);
	g_object_set_data (G_OBJECT (entry), "_sep", ",");
	g_signal_connect (G_OBJECT (entry), "toggled",
			  G_CALLBACK (spec_changed_cb), import);

	entry = gtk_radio_button_new_with_label_from_widget (GTK_RADIO_BUTTON (entry), _("Semi column"));
	import->priv->sep_array [SEP_SEMICOL] = entry;
	gtk_table_attach (GTK_TABLE (table), entry, 2, 3, 3, 4, GTK_FILL, 0, 0, 0);
	g_object_set_data (G_OBJECT (entry), "_sep", ";");
	g_signal_connect (G_OBJECT (entry), "toggled",
			  G_CALLBACK (spec_changed_cb), import);

	entry = gtk_radio_button_new_with_label_from_widget (GTK_RADIO_BUTTON (entry), _("Tab"));
	import->priv->sep_array [SEP_TAB] = entry;
	gtk_table_attach (GTK_TABLE (table), entry, 1, 2, 4, 5, GTK_FILL, 0, 0, 0);
	g_object_set_data (G_OBJECT (entry), "_sep", "\t");
	g_signal_connect (G_OBJECT (entry), "toggled",
			  G_CALLBACK (spec_changed_cb), import);

	entry = gtk_radio_button_new_with_label_from_widget (GTK_RADIO_BUTTON (entry), _("Space"));
	import->priv->sep_array [SEP_SPACE] = entry;
	gtk_table_attach (GTK_TABLE (table), entry, 2, 3, 4, 5, GTK_FILL, 0, 0, 0);
	g_object_set_data (G_OBJECT (entry), "_sep", " ");
	g_signal_connect (G_OBJECT (entry), "toggled",
			  G_CALLBACK (spec_changed_cb), import);

	entry = gtk_radio_button_new_with_label_from_widget (GTK_RADIO_BUTTON (entry), _("Pipe"));
	import->priv->sep_array [SEP_PIPE] = entry;
	gtk_table_attach (GTK_TABLE (table), entry, 1, 2, 5, 6, GTK_FILL, 0, 0, 0);
	g_object_set_data (G_OBJECT (entry), "_sep", "|");
	g_signal_connect (G_OBJECT (entry), "toggled",
			  G_CALLBACK (spec_changed_cb), import);

	hbox = gtk_hbox_new (FALSE, 0);
	gtk_table_attach (GTK_TABLE (table), hbox, 2, 3, 5, 6, GTK_FILL, 0, 0, 0);
	entry = gtk_radio_button_new_with_label_from_widget (GTK_RADIO_BUTTON (entry), _("Other:"));
	import->priv->sep_array [SEP_OTHER] = entry;
	gtk_box_pack_start (GTK_BOX (hbox), entry, FALSE, FALSE, 0);
	g_object_set_data (G_OBJECT (entry), "_sep", "");
	g_signal_connect (G_OBJECT (entry), "toggled",
			  G_CALLBACK (spec_changed_cb), import);

	entry = gtk_entry_new ();
	import->priv->sep_other_entry = entry;
	gtk_entry_set_max_length (GTK_ENTRY (entry), 1);
	gtk_box_pack_start (GTK_BOX (hbox), entry, TRUE, TRUE, 0);
	gtk_widget_set_sensitive (entry, FALSE);
	g_signal_connect (G_OBJECT (entry), "changed",
			  G_CALLBACK (spec_changed_cb), import);

        gtk_widget_show_all (vbox);
	

	/* 
	 * bottom part: import preview 
	 */
	vbox = gtk_vbox_new (FALSE, 0);
        gtk_paned_pack2 (GTK_PANED (import), vbox, TRUE, FALSE);

	str = g_strdup_printf ("<b>%s:</b>", _("Import preview"));
        label = gtk_label_new ("");
        gtk_label_set_markup (GTK_LABEL (label), str);
        gtk_misc_set_alignment (GTK_MISC (label), 0., -1);
        g_free (str);
        gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

        hbox = gtk_hbox_new (FALSE, 0); /* HIG */
        gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, TRUE, 0);
        gtk_widget_show (hbox);
        label = gtk_label_new ("    ");
        gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
	import->priv->preview_box = hbox;

        label = gtk_label_new ("");
        gtk_label_set_markup (GTK_LABEL (label), _("No data."));
        gtk_misc_set_alignment (GTK_MISC (label), 0., 0.);
        gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, TRUE, 0);
	import->priv->no_data_label = label;

	gtk_widget_show_all (vbox);

	gtk_paned_set_position (GTK_PANED (import), 1);
	
}

/**
 * gdaui_data_import_new
 * Creates a new #GdauiDataImport widget. After import, a #GdaDataModel will be created.
 *
 * Returns: the new widget
 */
GtkWidget *
gdaui_data_import_new (void)
{
	GdauiDataImport *import;
		
	import = GDAUI_DATA_IMPORT (g_object_new (GDAUI_TYPE_DATA_IMPORT, NULL));
	
	return GTK_WIDGET (import);
}


static void
gdaui_data_import_dispose (GObject *object)
{
	GdauiDataImport *import;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GDAUI_IS_DATA_IMPORT (object));
	import = GDAUI_DATA_IMPORT (object);

	if (import->priv) {
		if (import->priv->model) {
			g_object_unref (import->priv->model);
			import->priv->model = NULL;
		}

		/* the private area itself */
		g_free (import->priv);
		import->priv = NULL;
	}

	/* for the parent class */
	parent_class->dispose (object);
}

static void
spec_changed_cb (GtkWidget *wid, GdauiDataImport *import)
{
	gchar *file;
	GdaSet *options;
	gchar *sep;
	GdaHolder *psep = NULL;
	gint sepno;

	if (import->priv->preview_grid) {
		gtk_widget_destroy (import->priv->preview_grid);
		import->priv->preview_grid = NULL;
	}
	if (import->priv->model) {
		g_object_unref (import->priv->model);
		import->priv->model = NULL;
	}

	sep = g_object_get_data (G_OBJECT (wid), "_sep");
	if (sep) {
		if (*sep == 0)
			gtk_widget_set_sensitive (import->priv->sep_other_entry,  
						  gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (wid)));
		
		if (! gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (wid)))
			return;
	}

	for (sepno = SEP_COMMA; sepno < SEP_LAST; sepno++) {
		if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (import->priv->sep_array [sepno]))) {
			sep = g_object_get_data (G_OBJECT (import->priv->sep_array [sepno]), "_sep");
			psep = gda_holder_new (G_TYPE_STRING);
			g_object_set (G_OBJECT (psep), "id", "SEPARATOR", NULL);
			if (sepno != SEP_OTHER)
				gda_holder_set_value_str (psep, NULL, sep, NULL);
			else
				gda_holder_set_value_str (psep, NULL,
							  gtk_entry_get_text (GTK_ENTRY (import->priv->sep_other_entry)),
							  NULL);
			break;
		}
	}

	options = gda_set_new (NULL);
	if (psep) {
		gda_set_add_holder (options, psep);
		g_object_unref (psep);
	}

	if (import->priv->encoding_combo) {
		GdaDataModelIter *iter;
		
		iter = gdaui_data_selector_get_data_set (GDAUI_DATA_SELECTOR (import->priv->encoding_combo));;
		if (iter) {
			GdaHolder *h;
			h = g_object_new (GDA_TYPE_HOLDER, "id", "ENCODING", "g-type", G_TYPE_STRING, NULL);
			
			gda_holder_set_value (h, (GValue *) gda_data_model_iter_get_value_at (iter, 0), NULL);
			gda_set_add_holder (options, h);
			g_object_unref (h);
		}
	}

	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (import->priv->first_line_check))) {
			GdaHolder *h;
			h = gda_holder_new_inline (G_TYPE_BOOLEAN, "TITLE_AS_FIRST_LINE", TRUE);
			gda_set_add_holder (options, h);
			g_object_unref (h);
	}

	file = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (import->priv->file_chooser));
	if (file) {
		import->priv->model = gda_data_model_import_new_file (file, TRUE, options);
		g_free (file);
	}

	if (options)
		g_object_unref (options);

	if (import->priv->model) {
		gtk_widget_hide (import->priv->no_data_label);
		import->priv->preview_grid = gdaui_grid_new (import->priv->model);

		gdaui_data_proxy_column_show_actions (GDAUI_DATA_PROXY (import->priv->preview_grid), -1, FALSE);
		gdaui_grid_set_sample_size (GDAUI_GRID (import->priv->preview_grid), 50);
		g_object_set (G_OBJECT (import->priv->preview_grid), "info-flags",
			      GDAUI_DATA_PROXY_INFO_CHUNCK_CHANGE_BUTTONS | 
			      GDAUI_DATA_PROXY_INFO_CURRENT_ROW, NULL);
		gtk_box_pack_start (GTK_BOX (import->priv->preview_box), import->priv->preview_grid, TRUE, TRUE, 0);
		gtk_widget_show (import->priv->preview_grid);
	}
	else 
		gtk_widget_show (import->priv->no_data_label);
}

/**
 * gdaui_data_import_get_model
 * @import: a #GdauiDataImport widget
 *
 * Get the current imported data model. The caller has to reference it if needed.
 *
 * Returns: the #GdaDataModel, or %NULL
 */
GdaDataModel *
gdaui_data_import_get_model (GdauiDataImport *import)
{
	g_return_val_if_fail (GDAUI_IS_DATA_IMPORT (import), NULL);
	return import->priv->model;
}
