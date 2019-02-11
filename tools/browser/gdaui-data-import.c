/*
 * Copyright (C) 2009 - 2015 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2010 David King <davidk@openismus.com>
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
#include "gdaui-data-import.h"
#include <libgda/libgda.h>
#include <glib/gi18n-lib.h>
#include <libgda-ui/libgda-ui.h>
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
			(GInstanceInitFunc) gdaui_data_import_init,
			0
		};		
		
		type = g_type_register_static (GTK_TYPE_PANED, "GdauiDataImport", &info, 0);
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
	GtkWidget *grid, *entry;
	GtkFileFilter *filter;
	GdaDataModel *encs;
	GSList *encs_errors;
	
	import->priv = g_new0 (GdauiDataImportPriv, 1);
	import->priv->model = NULL;

	gtk_orientable_set_orientation (GTK_ORIENTABLE (import), GTK_ORIENTATION_VERTICAL);

	/* 
	 * top part: import specs. 
	 */
	vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
        gtk_paned_pack1 (GTK_PANED (import), vbox, FALSE, FALSE);

	str = g_strdup_printf ("<b>%s:</b>", _("Import specifications"));
        label = gtk_label_new ("");
        gtk_label_set_markup (GTK_LABEL (label), str);
	g_free (str);
	gtk_widget_set_halign (label, GTK_ALIGN_START);
        gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

        hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0); /* HIG */
        gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, TRUE, 0);
        gtk_widget_show (hbox);
        label = gtk_label_new ("    ");
        gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

	grid = gtk_grid_new ();
	gtk_box_pack_start (GTK_BOX (hbox), grid, TRUE, TRUE, 0);
	gtk_grid_set_column_spacing (GTK_GRID (grid), 5);
	gtk_grid_set_row_spacing (GTK_GRID (grid), 5);

	/* file to import from */
	label = gtk_label_new (_("File to import from:"));
	gtk_widget_set_halign (label, GTK_ALIGN_START);
	gtk_grid_attach (GTK_GRID (grid), label, 0, 0, 1, 1);

	entry = gtk_file_chooser_button_new (_("File to import data from"), GTK_FILE_CHOOSER_ACTION_OPEN);
	gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (entry), gdaui_get_default_path ());
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
	gtk_grid_attach (GTK_GRID (grid), entry, 1, 0, 3, 1);
	g_signal_connect (G_OBJECT (entry), "selection-changed",
			  G_CALLBACK (spec_changed_cb), import);

	/* Encoding */ 
	label = gtk_label_new (_("Encoding:"));
	gtk_widget_set_halign (label, GTK_ALIGN_START);
	gtk_grid_attach (GTK_GRID (grid), label, 0, 1, 1, 1);

	gchar *fname = gda_gbr_get_file_path (GDA_DATA_DIR, LIBGDA_ABI_NAME, "import_encodings.xml", NULL);
	encs = gda_data_model_import_new_file (fname, TRUE, NULL);
	g_free (fname);
	encs_errors = gda_data_model_import_get_errors (GDA_DATA_MODEL_IMPORT (encs));
	if (encs_errors) {
		for (; encs_errors; encs_errors = g_slist_next (encs_errors)) {
			gda_log_message ("Error importing import_encodings.xml: %s\n",
					 encs_errors->data && ((GError *) encs_errors->data)->message ?
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
	gtk_grid_attach (GTK_GRID (grid), entry, 1, 1, 3, 1);
	g_signal_connect (G_OBJECT (entry), "changed",
			  G_CALLBACK (spec_changed_cb), import);

	/* first line as title */
	label = gtk_label_new (_("First line as title:"));
	gtk_widget_set_halign (label, GTK_ALIGN_START);
	gtk_grid_attach (GTK_GRID (grid), label, 0, 2, 1, 1);
	
	entry = gtk_check_button_new ();
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (entry), TRUE);
	import->priv->first_line_check = entry;
	gtk_grid_attach (GTK_GRID (grid), entry, 1, 2, 2, 1);
	g_signal_connect (G_OBJECT (entry), "toggled",
			  G_CALLBACK (spec_changed_cb), import);

	/* separator */
	label = gtk_label_new (_("Separator:"));
	gtk_widget_set_halign (label, GTK_ALIGN_START);
	gtk_grid_attach (GTK_GRID (grid), label, 0, 3, 1, 1);

	entry = gtk_radio_button_new_with_label (NULL, _("Comma"));
	import->priv->sep_array [SEP_COMMA] = entry;
	gtk_grid_attach (GTK_GRID (grid), entry, 1, 3, 1, 1);
	g_object_set_data (G_OBJECT (entry), "_sep", ",");
	g_signal_connect (G_OBJECT (entry), "toggled",
			  G_CALLBACK (spec_changed_cb), import);

	entry = gtk_radio_button_new_with_label_from_widget (GTK_RADIO_BUTTON (entry), _("Semi colon"));
	import->priv->sep_array [SEP_SEMICOL] = entry;
	gtk_grid_attach (GTK_GRID (grid), entry, 2, 3, 1, 1);
	g_object_set_data (G_OBJECT (entry), "_sep", ";");
	g_signal_connect (G_OBJECT (entry), "toggled",
			  G_CALLBACK (spec_changed_cb), import);

	entry = gtk_radio_button_new_with_label_from_widget (GTK_RADIO_BUTTON (entry), _("Tab"));
	import->priv->sep_array [SEP_TAB] = entry;
	gtk_grid_attach (GTK_GRID (grid), entry, 1, 4, 1, 1);
	g_object_set_data (G_OBJECT (entry), "_sep", "\t");
	g_signal_connect (G_OBJECT (entry), "toggled",
			  G_CALLBACK (spec_changed_cb), import);

	entry = gtk_radio_button_new_with_label_from_widget (GTK_RADIO_BUTTON (entry), _("Space"));
	import->priv->sep_array [SEP_SPACE] = entry;
	gtk_grid_attach (GTK_GRID (grid), entry, 2, 4, 1, 1);
	g_object_set_data (G_OBJECT (entry), "_sep", " ");
	g_signal_connect (G_OBJECT (entry), "toggled",
			  G_CALLBACK (spec_changed_cb), import);

	entry = gtk_radio_button_new_with_label_from_widget (GTK_RADIO_BUTTON (entry), _("Pipe"));
	import->priv->sep_array [SEP_PIPE] = entry;
	gtk_grid_attach (GTK_GRID (grid), entry, 1, 5, 1, 1);
	g_object_set_data (G_OBJECT (entry), "_sep", "|");
	g_signal_connect (G_OBJECT (entry), "toggled",
			  G_CALLBACK (spec_changed_cb), import);

	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_grid_attach (GTK_GRID (grid), hbox, 2, 5, 1, 1);
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
	vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
        gtk_paned_pack2 (GTK_PANED (import), vbox, TRUE, FALSE);

	str = g_strdup_printf ("<b>%s:</b>", _("Import preview"));
        label = gtk_label_new ("");
        gtk_label_set_markup (GTK_LABEL (label), str);
	g_free (str);
	gtk_widget_set_halign (label, GTK_ALIGN_START);
        gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

        hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0); /* HIG */
	import->priv->preview_box = hbox;

        gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, TRUE, 0);
        gtk_widget_show (hbox);
        label = gtk_label_new ("    ");
        gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

        label = gtk_label_new ("");
        gtk_label_set_markup (GTK_LABEL (label), _("No data."));
	gtk_widget_set_halign (label, GTK_ALIGN_START);
        gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, TRUE, 0);
	import->priv->no_data_label = label;

	gtk_widget_show_all (vbox);

	gtk_paned_set_position (GTK_PANED (import), 1);	
}

/**
 * gdaui_data_import_new
 *
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
			psep = gda_holder_new (G_TYPE_STRING, "SEPARATOR");
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

		gdaui_grid_set_sample_size (GDAUI_GRID (import->priv->preview_grid), 50);
		g_object_set (G_OBJECT (import->priv->preview_grid), "info-flags",
			      GDAUI_DATA_PROXY_INFO_CHUNK_CHANGE_BUTTONS | 
			      GDAUI_DATA_PROXY_INFO_CURRENT_ROW, NULL);
		gtk_box_pack_start (GTK_BOX (import->priv->preview_box), import->priv->preview_grid, TRUE, TRUE, 0);
		gtk_widget_show (import->priv->preview_grid);
		gdaui_set_default_path (gtk_file_chooser_get_current_folder (GTK_FILE_CHOOSER (import->priv->file_chooser)));
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
