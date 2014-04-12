/*
 * Copyright (C) 2009 - 2014 Vivien Malerba <malerba@gnome-db.org>
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

#include <libgda/gda-value.h>
#include "common-bin.h"
#include "../internal/popup-container.h"
#include <string.h>
#include <gtk/gtk.h>
#include <glib/gi18n-lib.h>
#include <libgda/gda-blob-op.h>

#ifdef HAVE_GIO
#include <gio/gio.h>
#endif

static void
file_load_cb (GtkWidget *button, BinMenu *menu)
{
	GtkWidget *dlg;

	gtk_widget_hide (menu->popup);
        dlg = gtk_file_chooser_dialog_new (_("Select file to load"),
                                           GTK_WINDOW (gtk_widget_get_toplevel (button)),
                                           GTK_FILE_CHOOSER_ACTION_OPEN,
                                           _("_Cancel"), GTK_RESPONSE_CANCEL,
                                           _("_Open"), GTK_RESPONSE_ACCEPT,
                                           NULL);
	if (menu->current_folder)
		gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (dlg), menu->current_folder);

	if (gtk_dialog_run (GTK_DIALOG (dlg)) == GTK_RESPONSE_ACCEPT) {
                char *filename;
                gsize length;
                GError *error = NULL;
                gchar *data;

                filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dlg));

		if (menu->entry_type == GDA_TYPE_BLOB) {
			menu->loaded_value_cb (menu->loaded_value_cb_data,
					       gda_value_new_blob_from_file (filename));
		}
		else if (menu->entry_type == GDA_TYPE_BINARY) {
			if (g_file_get_contents (filename, &data, &length, &error)) {
				GdaBinary *bin;
				GValue *nvalue;
				bin = g_new0 (GdaBinary, 1);
				bin->data = (guchar*) data;
				bin->binary_length = length;
				nvalue = gda_value_new (GDA_TYPE_BINARY);
				gda_value_take_binary (nvalue, bin);

				menu->loaded_value_cb (menu->loaded_value_cb_data, nvalue);
			}
			else {
				GtkWidget *msg;
				
				msg = gtk_message_dialog_new_with_markup (GTK_WINDOW (gtk_widget_get_toplevel (button)),
						   GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR,
						   GTK_BUTTONS_CLOSE,
						   _("Could not load the contents of '%s':\n %s"),
						   filename,
						   error && error->message ? error->message : _("No detail"));
				if (error)
					g_error_free (error);
				gtk_widget_destroy (dlg);
				dlg = NULL;
				
				gtk_dialog_run (GTK_DIALOG (msg));
				gtk_widget_destroy (msg);
			}
		}
		else
			g_assert_not_reached ();

                g_free (filename);
        }

        if (dlg) {
		g_free (menu->current_folder);
		menu->current_folder = gtk_file_chooser_get_current_folder (GTK_FILE_CHOOSER (dlg));
                gtk_widget_destroy (dlg);
	}
}

static void
file_save_cb (GtkWidget *button, BinMenu *menu)
{
	GtkWidget *dlg;

	gtk_widget_hide (menu->popup);
        dlg = gtk_file_chooser_dialog_new (_("Select a file to save data to"),
                                           GTK_WINDOW (gtk_widget_get_toplevel (button)),
                                           GTK_FILE_CHOOSER_ACTION_SAVE,
                                           _("_Cancel"), GTK_RESPONSE_CANCEL,
                                           _("_Open"), GTK_RESPONSE_ACCEPT,
                                           NULL);

	if (menu->current_folder)
		gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (dlg), menu->current_folder);

        if (gtk_dialog_run (GTK_DIALOG (dlg)) == GTK_RESPONSE_ACCEPT) {
                char *filename;
                gboolean allok = TRUE;
                GError *error = NULL;

                filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dlg));
		if (menu->entry_type == GDA_TYPE_BINARY) {
			const GdaBinary *bin;
			bin = gda_value_get_binary (menu->tmpvalue);
			allok = g_file_set_contents (filename, (gchar *) bin->data,
						     bin->binary_length, &error);
		}
		else if (menu->entry_type == GDA_TYPE_BLOB) {
			GdaBlob *blob;
			blob = (GdaBlob*) gda_value_get_blob (menu->tmpvalue);
			if (blob->op) {
			       
				GValue *dest_value;
				GdaBlob *dest_blob;

				dest_value = gda_value_new_blob_from_file (filename);
				dest_blob = (GdaBlob*) gda_value_get_blob (dest_value);
				allok = gda_blob_op_write_all (dest_blob->op, (GdaBlob*) blob);
				gda_value_free (dest_value);
			}
			else
				allok = g_file_set_contents (filename, (gchar *) ((GdaBinary*)blob)->data,
							     ((GdaBinary*)blob)->binary_length, &error);
		}
		else
			g_assert_not_reached ();
                if (!allok) {
                        GtkWidget *msg;
			msg = gtk_message_dialog_new_with_markup (GTK_WINDOW (gtk_widget_get_toplevel (button)),
                                                                  GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR,
                                                                  GTK_BUTTONS_CLOSE,
                                                                  _("Could not save data to '%s':\n %s"),
                                                                  filename,
                                                                  error && error->message ? error->message : _("No detail"));
                        if (error)
                                g_error_free (error);
                        gtk_widget_destroy (dlg);
                        dlg = NULL;

                        gtk_dialog_run (GTK_DIALOG (msg));
                        gtk_widget_destroy (msg);
                }
                g_free (filename);
        }

        if (dlg) {
		g_free (menu->current_folder);
		menu->current_folder = gtk_file_chooser_get_current_folder (GTK_FILE_CHOOSER (dlg));
                gtk_widget_destroy (dlg);

	}
}

void
common_bin_create_menu (BinMenu *binmenu, PopupContainerPositionFunc pos_func, GType entry_type,
			BinCallback loaded_value_cb, gpointer loaded_value_cb_data)
{
	GtkWidget *popup, *vbox, *hbox, *bbox, *button, *label;
	gchar *str;

	binmenu->entry_type = entry_type;
	binmenu->loaded_value_cb = loaded_value_cb;
	binmenu->loaded_value_cb_data = loaded_value_cb_data;
	
	popup = popup_container_new_with_func (pos_func);
	binmenu->popup = popup;
	
	vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_container_add (GTK_CONTAINER (popup), vbox);

	label = gtk_label_new ("");
	str = g_strdup_printf ("<b>%s:</b>", _("Properties"));
	gtk_label_set_markup (GTK_LABEL (label), str);
	g_free (str);
	gtk_misc_set_alignment (GTK_MISC (label), 0., -1);
	gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0); /* HIG */
        gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 5);
        gtk_widget_show (hbox);
        label = gtk_label_new ("    ");
        gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
        gtk_widget_show (label);

	label = gtk_label_new ("");
	gtk_misc_set_alignment (GTK_MISC (label), 0., -1);
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
	binmenu->props_label = label;

	bbox = gtk_button_box_new (GTK_ORIENTATION_HORIZONTAL);
	gtk_box_pack_start (GTK_BOX (vbox), bbox, FALSE, FALSE, 0);

	button = gtk_button_new_from_icon_name ("document-open", GTK_ICON_SIZE_BUTTON);
	gtk_box_pack_start (GTK_BOX (bbox), button, FALSE, FALSE, 0);
	g_signal_connect (button, "clicked",
			  G_CALLBACK (file_load_cb), binmenu);
	binmenu->load_button = button;

	button = gtk_button_new_from_icon_name ("document-save-as", GTK_ICON_SIZE_BUTTON);
	gtk_box_pack_start (GTK_BOX (bbox), button, FALSE, FALSE, 0);
	g_signal_connect (button, "clicked",
			  G_CALLBACK (file_save_cb), binmenu);
	binmenu->save_button = button;

	gtk_widget_show_all (vbox);
}

static gchar *
format_size (gulong size)
{
	if (size < 1024)
		return g_strdup_printf (ngettext ("%lu Byte", "%lu Bytes", size), size);
	else if (size < 1048576)
		return g_strdup_printf ("%.1f Kio", (gfloat) (size / 1024));
	else if (size < 1073741824)
		return g_strdup_printf ("%.1f Mio", (gfloat) (size / 1048576));
	else
		return g_strdup_printf ("%.1f Gio", (gfloat) (size / 1073741824));
}

/* 
 * adjust the sensitiveness of the menu items in the popup menu
 */
void
common_bin_adjust_menu (BinMenu *binmenu, gboolean editable, const GValue *value)
{
	gchar *size;
	GString *string;
#ifdef HAVE_GIO
	gchar *ctype = NULL;
#endif

	if (!binmenu || !binmenu->popup)
		return;

	if (binmenu->tmpvalue) {
		gda_value_free (binmenu->tmpvalue);
		binmenu->tmpvalue = NULL;
	}
	string = g_string_new ("");
	if (value) {
		binmenu->tmpvalue = gda_value_copy (value);
		if (G_VALUE_TYPE (value) == GDA_TYPE_NULL)
			g_string_append_printf (string, "<i>%s</i>", _("No data"));
		else if (G_VALUE_TYPE (value) == GDA_TYPE_BINARY) {
			const GdaBinary *bin;
			bin = gda_value_get_binary (value);
			size = format_size (bin->binary_length);
			g_string_append_printf (string, "%s: %s", _("Data size"), size);
			g_free (size);
#ifdef HAVE_GIO
			ctype = g_content_type_guess (NULL, bin->data, (gsize) bin->binary_length, NULL);
#endif
		}
		else if (G_VALUE_TYPE (value) == GDA_TYPE_BLOB) {
			const GdaBlob *blob;
			GdaBinary *bin;
			blob = gda_value_get_blob (value);
			bin = (GdaBinary *) blob;
			if (blob->op) {
				glong len;
				len = gda_blob_op_get_length (blob->op);
				if (len >= 0) {
					size = format_size (len);
					g_string_append_printf (string, "%s: %s", _("Data size"), size);
					g_free (size);
#ifdef HAVE_GIO
					GdaBlob *blob2;
					blob2 = (GdaBlob*) gda_blob_copy ((gpointer) blob);
					gda_blob_op_read (blob2->op, blob2, 0, 1024);
					bin = (GdaBinary *) blob2;
					ctype = g_content_type_guess (NULL, bin->data,
								      (gsize) bin->binary_length, NULL);
					gda_blob_free ((gpointer) blob2);
#endif
				}
				else
					g_string_append_printf (string, "%s: %s", _("Data size"), _("Unknown"));
			}
			else {
				size = format_size (bin->binary_length);
				g_string_append_printf (string, "%s: %s", _("Data size"), size);
				g_free (size);
#ifdef HAVE_GIO
				ctype = g_content_type_guess (NULL, bin->data, (gsize) bin->binary_length, NULL);
#endif
			}
		}
		else
			g_assert_not_reached ();
	}
	else
		g_string_append_printf (string, "<i>%s</i>", _("No data"));

#ifdef HAVE_GIO
	if (ctype) {
		GList *list;
		gchar *descr, *tmp;
		descr = g_content_type_get_description (ctype);
		tmp = g_markup_escape_text (descr, -1);
		g_free (descr);
		g_string_append_printf (string, "\n%s: %s", _("Data type"), tmp);
		g_free (tmp);

		list = g_app_info_get_all_for_type (ctype);
		for (; list; list = list->next) {
			GAppInfo *ai;
			ai = (GAppInfo*) list->data;
			g_print ("\t open with %s (%s)\n", g_app_info_get_name (ai),
				 g_app_info_get_executable (ai));
		}
		g_free (ctype);
	}
#endif


	gtk_label_set_markup (GTK_LABEL (binmenu->props_label), string->str);
	g_string_free (string, TRUE);

	gtk_widget_set_sensitive (binmenu->load_button, editable);
	gtk_widget_set_sensitive (binmenu->save_button, (value && !gda_value_is_null (value)) ? TRUE : FALSE);
}

/*
 * Reset @bonmenu's contents
 */
void
common_bin_reset (BinMenu *binmenu)
{
	if (binmenu->tmpvalue) {
		gda_value_free (binmenu->tmpvalue);
		binmenu->tmpvalue = NULL;
	}
	if (binmenu->popup)
		gtk_widget_destroy (binmenu->popup);
	g_free (binmenu->current_folder);

	memset (binmenu, 0, sizeof (BinMenu));
}
