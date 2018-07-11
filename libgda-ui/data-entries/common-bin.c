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

#include <glib/gstdio.h>
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
show_and_clear_error (BinMenu *binmenu, const gchar *context, GError **error)
{
	if (!error)
		return;

	GtkWidget *msg, *relative_to;
	relative_to = gtk_popover_get_relative_to (GTK_POPOVER (binmenu->popover));
	msg = gtk_message_dialog_new_with_markup (relative_to ? GTK_WINDOW (gtk_widget_get_toplevel (relative_to)) : NULL,
						  GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR,
						  GTK_BUTTONS_CLOSE,
						  "<b>%s:</b>\n%s: %s",
						  _("Error"), context,
						  *error && (*error)->message ? (*error)->message : _("No detail"));
	gtk_dialog_run (GTK_DIALOG (msg));
	gtk_widget_destroy (msg);
	g_clear_error (error);
}

static void
file_load_cb (GtkWidget *button, BinMenu *menu)
{
	GtkWidget *dlg;

	gtk_widget_hide (menu->popover);
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
				bin = gda_binary_new ();
				gda_binary_set_data (bin, (guchar*) data, length);
				nvalue = gda_value_new (GDA_TYPE_BINARY);
				gda_value_take_binary (nvalue, bin);

				menu->loaded_value_cb (menu->loaded_value_cb_data, nvalue);
			}
			else {
				gtk_widget_destroy (dlg);
				dlg = NULL;

				gchar *tmp;
				tmp = g_strdup_printf (_("Could not load the contents of '%s'"), filename);
				show_and_clear_error (menu, tmp, &error);
				g_free (tmp);
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

static gboolean
export_data (BinMenu *menu, const gchar *filename, GError **error)
{
	gboolean allok;
	if (menu->entry_type == GDA_TYPE_BINARY) {
		GdaBinary *bin;
		bin = gda_value_get_binary (menu->tmpvalue);
		allok = g_file_set_contents (filename, (gchar *) gda_binary_get_data (bin),
					     gda_binary_get_size (bin), error);
	}
	else if (menu->entry_type == GDA_TYPE_BLOB) {
		GdaBlob *blob;
		blob = (GdaBlob*) gda_value_get_blob (menu->tmpvalue);
		if (gda_blob_get_op (blob)) {
			GValue *dest_value;
			GdaBlob *dest_blob;

			dest_value = gda_value_new_blob_from_file (filename);
			dest_blob = (GdaBlob*) gda_value_get_blob (dest_value);
			allok = gda_blob_op_write_all (gda_blob_get_op (dest_blob), (GdaBlob*) blob);
			gda_value_free (dest_value);
		}
		else
			allok = g_file_set_contents (filename, (gchar *) gda_binary_get_data (gda_blob_get_binary (blob)),
						     gda_binary_get_size (gda_blob_get_binary (blob)), error);
	}
	else
		g_assert_not_reached ();
	if (allok) {
#ifdef G_OS_WIN32
		SetFileAttributes (filename, FILE_ATTRIBUTE_NOT_CONTENT_INDEXED |
				   FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_TEMPORARY);
#else
		g_chmod (filename, 0400);
#endif
	}
	return allok;
}

static void
file_save_cb (GtkWidget *button, BinMenu *menu)
{
	GtkWidget *dlg;

	gtk_widget_hide (menu->popover);
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
                GError *error = NULL;

                filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dlg));
                if (! export_data (menu, filename, &error)) {
                        GtkWidget *msg;
			gchar *tmp;
			tmp = g_strdup_printf (_("Could not save data to '%s'"), filename);
			msg = gtk_message_dialog_new_with_markup (GTK_WINDOW (gtk_widget_get_toplevel (button)),
								  GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR,
								  GTK_BUTTONS_CLOSE,
								  "<b>%s:</b>\n%s: %s",
								  _("Error"), tmp,
								  error && error->message ? error->message : _("No detail"));
			g_free (tmp);
			g_clear_error (&error);
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

static void
clear_data_cb (GtkWidget *button, BinMenu *menu)
{
	menu->loaded_value_cb (menu->loaded_value_cb_data, NULL);
}

#ifdef HAVE_GIO
static void
open_data_cb (GtkWidget *button, BinMenu *menu)
{
	g_return_if_fail (menu->open_menu);
	gtk_menu_popup_at_pointer (GTK_MENU (menu->open_menu), NULL);
}
#endif

void
common_bin_create_menu (GtkWidget *relative_to, BinMenu *binmenu, GType entry_type,
			BinCallback loaded_value_cb, gpointer loaded_value_cb_data)
{
	GtkWidget *bbox, *button;

	binmenu->entry_type = entry_type;
	binmenu->loaded_value_cb = loaded_value_cb;
	binmenu->loaded_value_cb_data = loaded_value_cb_data;
	
	binmenu->popover = gtk_popover_new (relative_to);
	gtk_popover_set_modal (GTK_POPOVER (binmenu->popover), TRUE);

	bbox = gtk_button_box_new (GTK_ORIENTATION_HORIZONTAL);
	gtk_container_add (GTK_CONTAINER (binmenu->popover), bbox);

	button = gtk_button_new_from_icon_name ("document-open-symbolic", GTK_ICON_SIZE_MENU);
	gtk_widget_set_tooltip_text (button, _("Load data from file"));
	gtk_box_pack_start (GTK_BOX (bbox), button, FALSE, FALSE, 0);
	g_signal_connect (button, "clicked",
			  G_CALLBACK (file_load_cb), binmenu);
	binmenu->load_button = button;

	button = gtk_button_new_from_icon_name ("document-save-as-symbolic", GTK_ICON_SIZE_MENU);
	gtk_widget_set_tooltip_text (button, _("Save data to file"));
	gtk_box_pack_start (GTK_BOX (bbox), button, FALSE, FALSE, 0);
	g_signal_connect (button, "clicked",
			  G_CALLBACK (file_save_cb), binmenu);
	binmenu->save_button = button;

	button = gtk_button_new_from_icon_name ("edit-clear-symbolic", GTK_ICON_SIZE_MENU);
	gtk_widget_set_tooltip_text (button, _("Clear data"));
	gtk_box_pack_start (GTK_BOX (bbox), button, FALSE, FALSE, 0);
	g_signal_connect (button, "clicked",
			  G_CALLBACK (clear_data_cb), binmenu);
	binmenu->clear_button = button;

#ifdef HAVE_GIO
	button = gtk_button_new_from_icon_name ("open-menu-symbolic", GTK_ICON_SIZE_MENU);
	gtk_widget_set_tooltip_text (button, _("Open with..."));
	gtk_box_pack_start (GTK_BOX (bbox), button, FALSE, FALSE, 0);
	g_signal_connect (button, "clicked",
			  G_CALLBACK (open_data_cb), binmenu);
	binmenu->open_button = button;
#endif

	gtk_widget_show_all (bbox);
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

/**
 * common_bin_get_description:
 */
gchar *
common_bin_get_description (BinMenu *binmenu)
{
	gchar *size;
	GString *string = NULL;

	const GValue *value = binmenu->tmpvalue;
	if (value) {
		if (G_VALUE_TYPE (value) == GDA_TYPE_NULL)
			return NULL;

		string = g_string_new ("");
		if (G_VALUE_TYPE (value) == GDA_TYPE_BINARY) {
			const GdaBinary *bin;
			bin = gda_value_get_binary (value);
			size = format_size (gda_binary_get_size (bin));
			g_string_append (string, size);
			g_free (size);
		}
		else if (G_VALUE_TYPE (value) == GDA_TYPE_BLOB) {
			GdaBlob *blob;
			GdaBinary *bin;
			blob = (GdaBlob*)gda_value_get_blob (value);
			bin = gda_blob_get_binary (blob);
			if (gda_blob_get_op (blob)) {
				glong len;
				len = gda_blob_op_get_length (gda_blob_get_op (blob));
				if (len >= 0) {
					size = format_size (len);
					g_string_append (string, size);
					g_free (size);
				}
				else
					g_string_append (string, _("Unknown size"));
			}
			else {
				size = format_size (gda_binary_get_size (bin));
				g_string_append (string, size);
				g_free (size);
			}
		}
		else
			g_assert_not_reached ();
	}
	else
		return NULL;

#ifdef HAVE_GIO
	if (binmenu->ctype) {
		gchar *tmp;
		tmp = g_content_type_get_description (binmenu->ctype);
		g_string_append_printf (string, " (%s)", tmp);
		g_free (tmp);
	}
#endif
	return g_string_free (string, FALSE);
}

#ifdef HAVE_GIO

typedef struct {
	gchar *dirname;
	gchar *filename;
} LaunchData;

static gboolean
launch_cleanups (LaunchData *data)
{
	g_assert (data);

	if (data->filename) {
#ifndef G_OS_WIN32
		if (g_unlink (data->filename))
			g_warning ("Error removing temporary file %s", data->filename);
#endif
		g_free (data->filename);
	}

	if (data->dirname) {
#ifndef G_OS_WIN32
		if (g_rmdir (data->dirname))
			g_warning ("Error removing temporary directory %s", data->dirname);
#endif
		g_free (data->dirname);
	}
	g_free (data);

	return G_SOURCE_REMOVE;
}

static gchar *
get_file_extension_for_mime_type (const gchar *mime_type)
{
	g_return_val_if_fail (mime_type && *mime_type, NULL);
	size_t length;
	length = strlen (mime_type);
	g_return_val_if_fail (length > 0, NULL);
	GBytes *css_data;
        css_data = g_resources_lookup_data ("/gdaui/data/mime-types-extensions",
					    G_RESOURCE_LOOKUP_FLAGS_NONE, NULL);
	if (css_data) {
		gchar *ext = NULL;
		const gchar *data;
		const gchar *sol;
		data = (gchar*) g_bytes_get_data (css_data, NULL);
		for (sol = data; *sol; ) {
			const gchar *eol;
			for (eol = sol; *eol && (*eol != '\n'); eol++);
			if (((size_t) (eol - sol) > length) &&
			    (sol[length] == '.') &&
			    !strncmp (sol, mime_type, length)) {
				ext = g_strndup (sol + length + 1, eol - sol - length - 1);
				break;
			}
			sol = eol;
			if (*sol)
				sol++;
		}
		g_bytes_unref (css_data);
		return ext;
	}
	else
		return NULL;
}

static void
open_activate_cb (GtkMenuItem *mitem, BinMenu *binmenu)
{
	GAppInfo *ai;
	ai = g_object_get_data (G_OBJECT (mitem), "ai");
	g_print ("OPENING With %s\n", g_app_info_get_name (ai));

	GError *error = NULL;

	/* Save data to a TMP file */
	gchar *tmpdir;
	tmpdir = g_dir_make_tmp (NULL, &error);
	if (!tmpdir) {
		show_and_clear_error (binmenu, _("Could not create temporary directory"), &error);
		return;
	}

	/* filename */
#define BASE_EXP_NAME "exported-data"
	gchar *filename;
	gchar *ext;
	ext = get_file_extension_for_mime_type (binmenu->ctype);
	if (ext) {
		gchar *tmp;
		tmp = g_strdup_printf ("%s.%s", BASE_EXP_NAME, ext);
		filename = g_build_filename (tmpdir, tmp, NULL);
		g_free (tmp);
		g_free (ext);
	}
	else
		filename = g_build_filename (tmpdir, BASE_EXP_NAME, NULL);
	g_print ("TMP file is: %s\n", filename);

	LaunchData *data;
	data = g_new (LaunchData, 1);
	data->dirname = tmpdir;
	data->filename = filename;

	if (export_data (binmenu, filename, &error)) {
		g_print ("Exported, now runinng opener program\n");
		GList *files_list;
		GFile *file;
		file = g_file_new_for_path (filename);
		files_list = g_list_append (NULL, file);

		if (g_app_info_launch (ai, files_list, NULL, &error)) {
			g_print ("Ok, running...\n");
			g_timeout_add_seconds (30, (GSourceFunc) launch_cleanups, data);
		}
		else {
			show_and_clear_error (binmenu, _("Could not run selected application"), &error);
			launch_cleanups (data);
		}
		g_list_free (files_list);
		g_object_unref (file);
	}
	else {
		show_and_clear_error (binmenu, _("Could not export data"), &error);
		launch_cleanups (data);
	}
}

static void
adjust_ctype (BinMenu *binmenu)
{
	g_free (binmenu->ctype);
	binmenu->ctype = NULL;
	if (binmenu->open_menu) {
		gtk_widget_destroy (binmenu->open_menu);
		binmenu->open_menu = NULL;
	}
	if (! binmenu->tmpvalue)
		return;

	const GValue *value = binmenu->tmpvalue;
	if (value && (G_VALUE_TYPE (value) != GDA_TYPE_NULL)) {
		if (G_VALUE_TYPE (value) == GDA_TYPE_BINARY) {
			GdaBinary *bin;
			bin = gda_value_get_binary (value);
			binmenu->ctype = g_content_type_guess (NULL, gda_binary_get_data (bin), (gsize) gda_binary_get_size (bin), NULL);
		}
		else if (G_VALUE_TYPE (value) == GDA_TYPE_BLOB) {
			GdaBlob *blob;
			GdaBinary *bin;
			blob = (GdaBlob*)gda_value_get_blob (value);
			bin = gda_blob_get_binary (blob);
			if (gda_blob_get_op (blob)) {
				glong len;
				len = gda_blob_op_get_length (gda_blob_get_op (blob));
				if (len >= 0) {
					GdaBlob *blob2;
					blob2 = (GdaBlob*) gda_blob_copy ((gpointer) blob);
					gda_blob_op_read (gda_blob_get_op (blob2), blob2, 0, 1024);
					bin = gda_blob_get_binary (blob2);
					binmenu->ctype = g_content_type_guess (NULL, gda_binary_get_data (bin),
								      (gsize) gda_binary_get_size (bin), NULL);
					gda_blob_free ((gpointer) blob2);
				}
			}
			else
				binmenu->ctype = g_content_type_guess (NULL, gda_binary_get_data (bin),
								       (gsize) gda_binary_get_size (bin), NULL);
		}
		else
			g_assert_not_reached ();
	}
	if (binmenu->ctype) {
		GList *list;
		list = g_app_info_get_all_for_type (binmenu->ctype);
		for (; list; ) {
			GAppInfo *ai = (GAppInfo *) list->data;
			if (!binmenu->open_menu)
				binmenu->open_menu = gtk_menu_new ();

			gchar *tmp;
			tmp = g_strdup_printf ("%s %s", _("Open with"), g_app_info_get_name (ai));
			GtkWidget *mitem;
			mitem = gtk_menu_item_new_with_label (tmp);
			g_free (tmp);
			g_object_set_data_full (G_OBJECT (mitem), "ai", ai, g_object_unref);
			g_signal_connect (mitem, "activate",
					  G_CALLBACK (open_activate_cb), binmenu);
			gtk_menu_shell_append (GTK_MENU_SHELL (binmenu->open_menu), mitem);
			gtk_widget_show (mitem);

			list = g_list_delete_link (list, list);
		}
	}
}
#endif

/*
 * adjust the sensitiveness of the menu items in the popup menu
 */
void
common_bin_adjust (BinMenu *binmenu, gboolean editable, const GValue *value)
{
	if (!binmenu || !binmenu->popover)
		return;

	if (value != binmenu->tmpvalue) {
		if (binmenu->tmpvalue) {
			gda_value_free (binmenu->tmpvalue);
			binmenu->tmpvalue = NULL;
		}
		if (value)
			binmenu->tmpvalue = gda_value_copy (value);
	}
#ifdef HAVE_GIO
	adjust_ctype (binmenu);
#endif
	gtk_widget_set_sensitive (binmenu->load_button, editable);
	gtk_widget_set_sensitive (binmenu->save_button, (value && !gda_value_is_null (value)) ? TRUE : FALSE);
	gtk_widget_set_sensitive (binmenu->clear_button, (editable && value && !gda_value_is_null (value)) ? TRUE : FALSE);
#ifdef HAVE_GIO
	gtk_widget_set_sensitive (binmenu->open_button, binmenu->open_menu ? TRUE : FALSE);
#endif
}

/*
 * Initialize @binmenu's contents
 */
void
common_bin_init (BinMenu *binmenu)
{
	g_assert (binmenu);
	memset (binmenu, 0, sizeof (BinMenu));
}

/*
 * Reset @binmenu's contents
 */
void
common_bin_reset (BinMenu *binmenu)
{
	if (binmenu->tmpvalue) {
		gda_value_free (binmenu->tmpvalue);
		binmenu->tmpvalue = NULL;
	}
	if (binmenu->popover)
		gtk_widget_destroy (binmenu->popover);
	g_free (binmenu->current_folder);
#ifdef HAVE_GIO
	if (binmenu->open_menu)
		gtk_widget_destroy (binmenu->open_menu);
	g_free (binmenu->ctype);
#endif	

	memset (binmenu, 0, sizeof (BinMenu));
}
