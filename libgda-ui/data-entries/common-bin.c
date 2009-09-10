/* common-pict.c
 * Copyright (C) 2009 Vivien Malerba <malerba@gnome-db.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <libgda/gda-value.h>
#include "common-bin.h"
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

        dlg = gtk_file_chooser_dialog_new (_("Select file to load"),
                                           GTK_WINDOW (gtk_widget_get_toplevel (button)),
                                           GTK_FILE_CHOOSER_ACTION_OPEN,
                                           GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                           GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
                                           NULL);
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
				bin->data = data;
				bin->binary_length = length;
				nvalue = gda_value_new (GDA_TYPE_BINARY);
				gda_value_take_binary (nvalue, bin);

				menu->loaded_value_cb (menu->loaded_value_cb_data, nvalue);

#ifdef HAVE_GIO
				/* Open with... using GIO */
				gchar *tmp;
				tmp = g_content_type_guess (NULL, bin->data, (gsize) bin->binary_length, NULL);
				g_print ("Content type: %s\n", tmp);

				GList *list;
				list = g_app_info_get_all_for_type (tmp);
				for (; list; list = list->next) {
					GAppInfo *ai;
					ai = (GAppInfo*) list->data;
					g_print ("\t open with %s (%s)\n", g_app_info_get_name (ai),
						 g_app_info_get_executable (ai));
				}
				g_free (tmp);
#endif
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

        if (dlg)
                gtk_widget_destroy (dlg);
}

static void
file_save_cb (GtkWidget *button, BinMenu *menu)
{
	GtkWidget *dlg;

        dlg = gtk_file_chooser_dialog_new (_("Select a file to save data to"),
                                           GTK_WINDOW (gtk_widget_get_toplevel (button)),
                                           GTK_FILE_CHOOSER_ACTION_SAVE,
                                           GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                           GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
                                           NULL);

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

        if (dlg)
                gtk_widget_destroy (dlg);
}

void
common_bin_create_menu (BinMenu *binmenu, GtkWidget *attach_to, GType entry_type,
			BinCallback loaded_value_cb, gpointer loaded_value_cb_data)
{
	GtkWidget *menu, *mitem;

	binmenu->entry_type = entry_type;
	binmenu->loaded_value_cb = loaded_value_cb;
	binmenu->loaded_value_cb_data = loaded_value_cb_data;
	
	menu = gtk_menu_new ();
	g_signal_connect (menu, "deactivate", 
			  G_CALLBACK (gtk_widget_hide), NULL);
	binmenu->menu = menu;
	
	mitem = gtk_menu_item_new_with_mnemonic (_("_Load from file"));
	gtk_container_add (GTK_CONTAINER (menu), mitem);
	g_signal_connect (mitem, "activate",
			  G_CALLBACK (file_load_cb), binmenu);
	binmenu->load_mitem = mitem;
	
	mitem = gtk_menu_item_new_with_mnemonic (_("_Save to file"));
	gtk_container_add (GTK_CONTAINER (menu), mitem);
	g_signal_connect (mitem, "activate",
			  G_CALLBACK (file_save_cb), binmenu);
	binmenu->save_mitem = mitem;
	
	gtk_menu_attach_to_widget (GTK_MENU (menu), attach_to, NULL);
	gtk_widget_show_all (menu);
}

/* 
 * adjust the sensitiveness of the menu items in the popup menu
 */
void
common_bin_adjust_menu (BinMenu *binmenu, gboolean editable, const GValue *value)
{
	if (!binmenu || !binmenu->menu)
		return;

	binmenu->tmpvalue = value;
	gtk_widget_set_sensitive (binmenu->load_mitem, editable);
	gtk_widget_set_sensitive (binmenu->save_mitem, (value && !gda_value_is_null (value)) ? TRUE : FALSE);
}

