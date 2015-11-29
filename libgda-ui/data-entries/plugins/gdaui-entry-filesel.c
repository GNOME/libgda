/*
 * Copyright (C) 2009 - 2015 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2010 David King <davidk@openismus.com>
 * Copyright (C) 2011 Murray Cumming <murrayc@murrayc.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#include <glib/gi18n-lib.h>
#include "gdaui-entry-filesel.h"
#include <libgda/gda-data-handler.h>
#include <libgda-ui/libgda-ui.h>

/* 
 * Main static functions 
 */
static void gdaui_entry_filesel_class_init (GdauiEntryFileselClass * class);
static void gdaui_entry_filesel_init (GdauiEntryFilesel * srv);
static void gdaui_entry_filesel_dispose (GObject   * object);
static void gdaui_entry_filesel_finalize (GObject   * object);

/* virtual functions */
static GtkWidget *create_entry (GdauiEntryWrapper *mgwrap);
static void       real_set_value (GdauiEntryWrapper *mgwrap, const GValue *value);
static GValue    *real_get_value (GdauiEntryWrapper *mgwrap);
static void       connect_signals(GdauiEntryWrapper *mgwrap, GCallback modify_cb, GCallback activate_cb);
static void       set_editable (GdauiEntryWrapper *mgwrap, gboolean editable);

/* get a pointer to the parents to be able to call their destructor */
static GObjectClass  *parent_class = NULL;

/* private structure */
struct _GdauiEntryFileselPrivate
{
	GtkWidget            *entry;
	GtkWidget            *button;
	GtkFileChooserAction  mode;
};

GType
gdaui_entry_filesel_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo info = {
			sizeof (GdauiEntryFileselClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gdaui_entry_filesel_class_init,
			NULL,
			NULL,
			sizeof (GdauiEntryFilesel),
			0,
			(GInstanceInitFunc) gdaui_entry_filesel_init,
			0
		};
		
		type = g_type_register_static (GDAUI_TYPE_ENTRY_WRAPPER, "GdauiEntryFilesel", &info, 0);
	}
	return type;
}

static void
gdaui_entry_filesel_class_init (GdauiEntryFileselClass * class)
{
	GObjectClass   *object_class = G_OBJECT_CLASS (class);

	parent_class = g_type_class_peek_parent (class);

	object_class->dispose = gdaui_entry_filesel_dispose;
	object_class->finalize = gdaui_entry_filesel_finalize;

	GDAUI_ENTRY_WRAPPER_CLASS (class)->create_entry = create_entry;
	GDAUI_ENTRY_WRAPPER_CLASS (class)->real_set_value = real_set_value;
	GDAUI_ENTRY_WRAPPER_CLASS (class)->real_get_value = real_get_value;
	GDAUI_ENTRY_WRAPPER_CLASS (class)->connect_signals = connect_signals;
	GDAUI_ENTRY_WRAPPER_CLASS (class)->set_editable = set_editable;	
}

static void
gdaui_entry_filesel_init (GdauiEntryFilesel * gdaui_entry_filesel)
{
	gdaui_entry_filesel->priv = g_new0 (GdauiEntryFileselPrivate, 1);
	gdaui_entry_filesel->priv->entry = NULL;
	gdaui_entry_filesel->priv->button = NULL;
	gdaui_entry_filesel->priv->mode = GTK_FILE_CHOOSER_ACTION_OPEN;
}

/**
 * gdaui_entry_filesel_new
 * @dh: the data handler to be used by the new widget
 * @type: the requested data type (compatible with @dh)
 * @options: the options
 *
 * Creates a new widget which is mainly a GtkEntry
 *
 * Returns: the new widget
 */
GtkWidget *
gdaui_entry_filesel_new (GdaDataHandler *dh, GType type, const gchar *options)
{
	GObject *obj;
	GdauiEntryFilesel *filesel;

	g_return_val_if_fail (dh && GDA_IS_DATA_HANDLER (dh), NULL);
	g_return_val_if_fail (gda_data_handler_accepts_g_type (dh, type), NULL);

	obj = g_object_new (GDAUI_TYPE_ENTRY_FILESEL, "handler", dh, NULL);
	filesel = GDAUI_ENTRY_FILESEL (obj);
	gdaui_data_entry_set_value_type (GDAUI_DATA_ENTRY (filesel), type);

	if (options && *options) {
		GdaQuarkList *params;
		const gchar *str;

		params = gda_quark_list_new_from_string (options);
		str = gda_quark_list_find (params, "MODE");
		if (str) {
			if ((*str == 'O') || (*str == 'o'))
				filesel->priv->mode = GTK_FILE_CHOOSER_ACTION_OPEN;
			else if ((*str == 'S') || (*str == 's'))
				filesel->priv->mode = GTK_FILE_CHOOSER_ACTION_SAVE;
			else if ((*str == 'P') || (*str == 'p'))
				filesel->priv->mode = GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER;
			else if ((*str == 'N') || (*str == 'n'))
				filesel->priv->mode = GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER;
		}
		gda_quark_list_free (params);
	}
	
	return GTK_WIDGET (obj);
}


static void
gdaui_entry_filesel_dispose (GObject   * object)
{
	GdauiEntryFilesel *gdaui_entry_filesel;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GDAUI_IS_ENTRY_FILESEL (object));

	gdaui_entry_filesel = GDAUI_ENTRY_FILESEL (object);
	if (gdaui_entry_filesel->priv) {

	}

	/* parent class */
	parent_class->dispose (object);
}

static void
gdaui_entry_filesel_finalize (GObject   * object)
{
	GdauiEntryFilesel *gdaui_entry_filesel;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GDAUI_IS_ENTRY_FILESEL (object));

	gdaui_entry_filesel = GDAUI_ENTRY_FILESEL (object);
	if (gdaui_entry_filesel->priv) {
		g_free (gdaui_entry_filesel->priv);
		gdaui_entry_filesel->priv = NULL;
	}

	/* parent class */
	parent_class->finalize (object);
}

static void
button_clicked_cb (G_GNUC_UNUSED GtkWidget *button, GdauiEntryFilesel *filesel)
{
	
	GtkWidget *dlg;
	gint result;
	gchar *title;

	if ((filesel->priv->mode == GTK_FILE_CHOOSER_ACTION_OPEN) ||
	    (filesel->priv->mode == GTK_FILE_CHOOSER_ACTION_SAVE))
		title = _("Choose a file");
	else
		title = _("Choose a directory");
	dlg = gtk_file_chooser_dialog_new (title,
					   (GtkWindow *) gtk_widget_get_ancestor (GTK_WIDGET (filesel), GTK_TYPE_WINDOW), 
					   filesel->priv->mode, 
					   _("_Cancel"), GTK_RESPONSE_CANCEL,
					   _("_Apply"), GTK_RESPONSE_ACCEPT,
					   NULL);
	gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (dlg),
					     gdaui_get_default_path ());
		
	result = gtk_dialog_run (GTK_DIALOG (dlg));
	if (result == GTK_RESPONSE_ACCEPT) {
		gchar *file;

		file = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dlg));
		gtk_entry_set_text (GTK_ENTRY (filesel->priv->entry), file);
		g_free (file);
		gdaui_set_default_path (gtk_file_chooser_get_current_folder (GTK_FILE_CHOOSER (dlg)));
	}
	gtk_widget_destroy (dlg);
}

static GtkWidget *
create_entry (GdauiEntryWrapper *mgwrap)
{
	GdauiEntryFilesel *filesel;
	GtkWidget *wid, *hbox;

	g_return_val_if_fail (GDAUI_IS_ENTRY_FILESEL (mgwrap), NULL);
	filesel = GDAUI_ENTRY_FILESEL (mgwrap);
	g_return_val_if_fail (filesel->priv, NULL);

	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);

	wid = gtk_entry_new ();
	gtk_box_pack_start (GTK_BOX (hbox), wid, TRUE, TRUE, 0);
	gtk_widget_show (wid);
	filesel->priv->entry = wid;

	wid = gtk_button_new_with_label (_("Choose"));

	filesel->priv->button = wid;
	gtk_box_pack_start (GTK_BOX (hbox), wid, FALSE, TRUE, 5);
	gtk_widget_show (wid);
	g_signal_connect (G_OBJECT (wid), "clicked",
			  G_CALLBACK (button_clicked_cb), filesel);
	
	return hbox;
}

static void
real_set_value (GdauiEntryWrapper *mgwrap, const GValue *value)
{
	GdauiEntryFilesel *filesel;
	gboolean sensitive = FALSE;

	g_return_if_fail (GDAUI_IS_ENTRY_FILESEL (mgwrap));
	filesel = GDAUI_ENTRY_FILESEL (mgwrap);
	g_return_if_fail (filesel->priv);

	if (value) {
		if (gda_value_is_null ((GValue *) value)) 
			sensitive = FALSE;
		else {
			GdaDataHandler *dh;		
			gchar *str;

			dh = gdaui_data_entry_get_handler (GDAUI_DATA_ENTRY (mgwrap));
			str = gda_data_handler_get_str_from_value (dh, value);
			if (str) {
				gtk_entry_set_text (GTK_ENTRY (filesel->priv->entry), str);
				g_free (str);
				sensitive = TRUE;
			}
		}
	}
	else 
		sensitive = FALSE;

	if (!sensitive) 
		gtk_entry_set_text (GTK_ENTRY (filesel->priv->entry), "");
}

static GValue *
real_get_value (GdauiEntryWrapper *mgwrap)
{
	GValue *value;
	GdauiEntryFilesel *filesel;
	GdaDataHandler *dh;
	const gchar *str;

	g_return_val_if_fail (GDAUI_IS_ENTRY_FILESEL (mgwrap), NULL);
	filesel = GDAUI_ENTRY_FILESEL (mgwrap);
	g_return_val_if_fail (filesel->priv, NULL);

	dh = gdaui_data_entry_get_handler (GDAUI_DATA_ENTRY (mgwrap));
	str = gtk_entry_get_text (GTK_ENTRY (filesel->priv->entry));
	value = gda_data_handler_get_value_from_str (dh, str, 
						     gdaui_data_entry_get_value_type (GDAUI_DATA_ENTRY (mgwrap)));
	if (!value) {
		/* in case the gda_data_handler_get_value_from_sql() returned an error because
		   the contents of the GtkFileChooser cannot be interpreted as a GValue */
		value = gda_value_new_null ();
	}

	return value;
}

static void
connect_signals(GdauiEntryWrapper *mgwrap, GCallback modify_cb, GCallback activate_cb)
{
	GdauiEntryFilesel *filesel;

	g_return_if_fail (GDAUI_IS_ENTRY_FILESEL (mgwrap));
	filesel = GDAUI_ENTRY_FILESEL (mgwrap);
	g_return_if_fail (filesel->priv);

	g_signal_connect_swapped (G_OBJECT (filesel->priv->entry), "changed",
				  modify_cb, mgwrap);
	g_signal_connect_swapped (G_OBJECT (filesel->priv->entry), "activate",
				  activate_cb, mgwrap);
}

static void
set_editable (GdauiEntryWrapper *mgwrap, gboolean editable)
{
	GdauiEntryFilesel *filesel;

	g_return_if_fail (GDAUI_IS_ENTRY_FILESEL (mgwrap));
	filesel = GDAUI_ENTRY_FILESEL (mgwrap);
	g_return_if_fail (filesel->priv);

	gtk_editable_set_editable (GTK_EDITABLE (filesel->priv->entry), editable);
	gtk_widget_set_sensitive (filesel->priv->button, editable);
	gtk_widget_set_visible (filesel->priv->button, editable);
}
