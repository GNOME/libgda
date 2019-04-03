/*
 * Copyright (C) 2009 - 2014 Vivien Malerba <malerba@gnome-db.org>
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

#include "common-pict.h"
#include <string.h>
#include <libgda/gda-quark-list.h>
#include <libgda/gda-blob-op.h>
#include <libgda-ui/libgda-ui.h>
#include <libgda-ui/gdaui-data-entry.h>

/*
 * Fills in @bindata->data and @bindata->data_length with the contents of @value.
 *
 * Returns: TRUE if the data has been loaded correctly
 */
gboolean
common_pict_load_data (PictOptions *options, const GValue *value, PictBinData *bindata, 
		       const gchar **out_icon_name, GError **error)
{
	gboolean allok = TRUE;

	g_assert (out_icon_name);
	*out_icon_name = NULL;

	if (value) {
		if (gda_value_is_null ((GValue *) value)) {
			*out_icon_name = "image-missing";
			g_set_error (error, GDAUI_DATA_ENTRY_ERROR, GDAUI_DATA_ENTRY_INVALID_DATA_ERROR,
				     "%s", _("No data"));
			allok = FALSE;
		}
		else {
			if (G_VALUE_TYPE ((GValue *) value) == GDA_TYPE_BLOB) {
				GdaBlob *blob;
				GdaBinary *bin;

				blob = (GdaBlob *) gda_value_get_blob ((GValue *) value);
				g_assert (blob);
				bin = gda_blob_get_binary (blob);
				if (gda_blob_get_op (blob) &&
				    (gda_binary_get_size (bin) != gda_blob_op_get_length (gda_blob_get_op (blob))))
					gda_blob_op_read_all (gda_blob_get_op (blob), blob);
				if (gda_binary_get_size (bin) > 0) {
					bindata->data = g_new (guchar, gda_binary_get_size (bin));
					bindata->data_length = gda_binary_get_size (bin);
					memcpy (bindata->data, gda_binary_get_data (bin), gda_binary_get_size (bin));
				}
			}
			else if (G_VALUE_TYPE ((GValue *) value) == GDA_TYPE_BINARY) {
				GdaBinary *bin;

				bin = (GdaBinary *) gda_value_get_binary ((GValue *) value);
				if (bin && gda_binary_get_size (bin) > 0) {
					bindata->data = g_new (guchar, gda_binary_get_size (bin));
					bindata->data_length = gda_binary_get_size (bin);
					memcpy (bindata->data, gda_binary_get_data (bin), gda_binary_get_size (bin));
				}
				else {
					*out_icon_name = "dialog-error";
					g_set_error (error, GDAUI_DATA_ENTRY_ERROR, GDAUI_DATA_ENTRY_INVALID_DATA_ERROR,
						     "%s", _("No data"));
					allok = FALSE;
				}
			}
			else if (G_VALUE_TYPE ((GValue *) value) == G_TYPE_STRING) {
				const gchar *str;

				str = g_value_get_string (value);
				if (str) {
					switch (options->encoding) {
					case ENCODING_NONE:
						bindata->data = (guchar *) g_strdup (str);
						bindata->data_length = strlen ((gchar *) bindata->data);
						break;
					case ENCODING_BASE64: {
						gsize out_len;
						bindata->data = g_base64_decode (str, &out_len);
						if (out_len > 0)
							bindata->data_length = out_len;
						else {
							g_free (bindata->data);
							bindata->data = NULL;
							bindata->data_length = 0;
						}
						break;
					}
					}
				}
				else {
					*out_icon_name = "image-missing";
					g_set_error (error, GDAUI_DATA_ENTRY_ERROR, GDAUI_DATA_ENTRY_INVALID_DATA_ERROR,
						     "%s", _("Empty data"));
					allok = FALSE;
				}
			}
			else {
				*out_icon_name = "dialog-error";
				g_set_error (error, GDAUI_DATA_ENTRY_ERROR, GDAUI_DATA_ENTRY_INVALID_DATA_ERROR,
					     "%s", _("Unhandled type of data"));
				allok = FALSE;
			}
		}
	}
	else {
		*out_icon_name = "image-missing";
		g_set_error (error, GDAUI_DATA_ENTRY_ERROR, GDAUI_DATA_ENTRY_INVALID_DATA_ERROR,
			     "%s", _("Empty data"));
		allok = FALSE;
	}

	return allok;
}

static void
compute_reduced_size (gint width, gint height, PictAllocation *allocation, 
		     gint *out_width, gint *out_height)
{
	gint reqw, reqh;

	reqw = allocation->width;
	reqh = allocation->height;

	if ((reqw < width) || (reqh < height)) {
		gint w, h;
	
		if ((double) height * (double) reqw > (double) width * (double) reqh) {
			w = 0.5 + (double) width * (double) reqh / (double) height;
			h =  reqh;
		} else {
			h = 0.5 + (double) height * (double) reqw / (double) width;
			w =  reqw;
		}
		*out_width = w;
		*out_height = h;
	}
	else {
		*out_width = width;
		*out_height = height;
	}
}

static void 
loader_size_prepared_cb (GdkPixbufLoader *loader, gint width, gint height, PictAllocation *allocation)
{
	gint w, h;

	compute_reduced_size (width, height, allocation, &w, &h);
	if ((w != width) || (h != height))
		gdk_pixbuf_loader_set_size (loader, w, h);

	/*
	gint reqw, reqh;

	reqw = allocation->width;
	reqh = allocation->height;

	if ((reqw < width) || (reqh < height)) {
		gint w, h;
	
		if ((double) height * (double) reqw > (double) width * (double) reqh) {
			w = 0.5 + (double) width * (double) reqh / (double) height;
			h =  reqh;
		} else {
			h = 0.5 + (double) height * (double) reqw / (double) width;
			w =  reqw;
		}
		
		gdk_pixbuf_loader_set_size (loader, w, h);
	}
	*/
}

/*
 * Creates a GdkPixbuf from @bindata and @options; returns NULL if an error occured.
 *
 * if @allocation is %NULL, then the GdaPixbuf will have the real size of the image.
 */
GdkPixbuf * 
common_pict_make_pixbuf (PictOptions *options, PictBinData *bindata, PictAllocation *allocation, 
			 const gchar **out_icon_name, GError **error)
{
	GdkPixbuf *retpixbuf = NULL;
	g_assert (out_icon_name);
	*out_icon_name = NULL;

	if (bindata->data) {
		GdkPixbufLoader *loader;
		GError *loc_error = NULL;

		loader = gdk_pixbuf_loader_new ();
		if (allocation)
			g_signal_connect (G_OBJECT (loader), "size-prepared",
					  G_CALLBACK (loader_size_prepared_cb), allocation);
		if (gdk_pixbuf_loader_write (loader, bindata->data, bindata->data_length, &loc_error) &&
		    gdk_pixbuf_loader_close (loader, &loc_error)) {
			retpixbuf = gdk_pixbuf_loader_get_pixbuf (loader);
			if (!retpixbuf) {
				if (loc_error)
					g_propagate_error (error, loc_error);
				*out_icon_name = "image-missing";
			}
			else
				g_object_ref (retpixbuf);
		}
		else {
			gchar *notice_msg;
			notice_msg = g_strdup_printf (_("Error while interpreting data as an image:\n%s"),
						      loc_error && loc_error->message ? loc_error->message : _("No detail"));
			*out_icon_name = "dialog-warning";
			g_set_error_literal (error, loc_error ? loc_error->domain : GDAUI_DATA_ENTRY_ERROR,
					     loc_error ? loc_error->code : GDAUI_DATA_ENTRY_INVALID_DATA_ERROR,
					     notice_msg);
			g_error_free (loc_error);
			g_free (notice_msg);
		}

		g_object_unref (loader);
	}

	return retpixbuf;
}

/*
 * Creates a new popup menu, attaches it to @ettach_to. The actions are in reference to @bindata,
 * and the @callback callback is called when the data in @bindata has been modified
 */
typedef struct {
	PictBinData  *bindata;
	PictOptions  *options;
	PictCallback  callback;
	gpointer      data;
} PictMenuData;
static void file_load_cb (GtkMenuItem *mitem, PictMenuData *menudata);
static void file_save_cb (GtkMenuItem *mitem, PictMenuData *menudata);
static void copy_cb (GtkMenuItem *mitem, PictMenuData *menudata);

static void
menudata_free (PictMenuData *menudata)
{
	if (menudata->bindata) {
		g_free (menudata->bindata->data);
		g_free (menudata->bindata);
	}
	g_free (menudata);
}

void
common_pict_create_menu (PictMenu *pictmenu, GtkWidget *attach_to, PictBinData *bindata, PictOptions *options,
			 PictCallback callback, gpointer data)
{
	GtkWidget *menu, *mitem;
	PictMenuData *menudata;
	
	menudata = g_new (PictMenuData, 1);
	menudata->bindata = g_new (PictBinData, 1);
	menudata->bindata->data = g_memdup (bindata->data, bindata->data_length);
	menudata->bindata->data_length = bindata->data_length;
	menudata->options = options;
	menudata->callback = callback;
	menudata->data = data;
	
	menu = gtk_menu_new ();
	g_object_set_data_full (G_OBJECT (menu), "menudata", menudata, (GDestroyNotify) menudata_free);
	g_signal_connect (menu, "deactivate", 
			  G_CALLBACK (gtk_widget_hide), NULL);
	pictmenu->menu = menu;

	mitem = gtk_menu_item_new_with_mnemonic (_("_Copy image"));
	gtk_widget_show (mitem);
	gtk_container_add (GTK_CONTAINER (menu), mitem);
	g_signal_connect (mitem, "activate",
			  G_CALLBACK (copy_cb), menudata);
	gtk_widget_set_sensitive (mitem, bindata->data ? TRUE : FALSE);
	pictmenu->copy_mitem = mitem;

	mitem = gtk_menu_item_new_with_mnemonic (_("_Load image from file"));
	gtk_widget_show (mitem);
	gtk_container_add (GTK_CONTAINER (menu), mitem);
	if (attach_to)
		g_object_set_data_full (G_OBJECT (mitem), "attach-to", g_object_ref (attach_to), g_object_unref);
	g_signal_connect (mitem, "activate",
			  G_CALLBACK (file_load_cb), menudata);
	pictmenu->load_mitem = mitem;
		
	mitem = gtk_menu_item_new_with_mnemonic (_("_Save image"));
	gtk_widget_show (mitem);
	gtk_container_add (GTK_CONTAINER (menu), mitem);
	if (attach_to)
		g_object_set_data_full (G_OBJECT (mitem), "attach-to", g_object_ref (attach_to), g_object_unref);
	g_signal_connect (mitem, "activate",
			  G_CALLBACK (file_save_cb), menudata);
	gtk_widget_set_sensitive (mitem, bindata->data ? TRUE : FALSE);
	pictmenu->save_mitem = mitem;

	gtk_menu_attach_to_widget (GTK_MENU (menu), attach_to, NULL);
}

static void
file_load_cb (GtkMenuItem *mitem, PictMenuData *menudata)
{
	GtkWidget *dlg;
	GtkFileFilter *filter;

	GtkWidget *attach_to;
	attach_to = g_object_get_data (G_OBJECT (mitem), "attach-to");
	if (!attach_to)
		attach_to = GTK_WIDGET (mitem);
	dlg = gtk_file_chooser_dialog_new (_("Select image to load"), 
					   GTK_WINDOW (gtk_widget_get_toplevel (attach_to)),
					   GTK_FILE_CHOOSER_ACTION_OPEN, 
					   _("_Cancel"), GTK_RESPONSE_CANCEL,
					   _("_Open"), GTK_RESPONSE_ACCEPT,
					   NULL);
	filter = gtk_file_filter_new ();
	gtk_file_filter_add_pixbuf_formats (filter);
	gtk_file_chooser_set_filter (GTK_FILE_CHOOSER (dlg), filter);
	gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (dlg),
					     gdaui_get_default_path ());	

	if (gtk_dialog_run (GTK_DIALOG (dlg)) == GTK_RESPONSE_ACCEPT) {
		char *filename;
		gsize length;
		GError *error = NULL;
		gchar *data;

		filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dlg));
		gdaui_set_default_path (gtk_file_chooser_get_current_folder (GTK_FILE_CHOOSER (dlg)));

		if (g_file_get_contents (filename, &data, &length, &error)) {
			g_free (menudata->bindata->data);
			menudata->bindata->data = (guchar *) data;
			menudata->bindata->data_length = length;

			/* call the callback */
			if (menudata->callback)
				(menudata->callback) (menudata->bindata, menudata->data);
			menudata->bindata->data = NULL;
			menudata->bindata->data_length = 0;
		}
		else {
			GtkWidget *msg;
			gchar *tmp;
			tmp = g_strdup_printf (_("Could not load the contents of '%s'"), filename);
			msg = gtk_message_dialog_new_with_markup (GTK_WINDOW (gtk_widget_get_toplevel (attach_to)),
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
	
	if (dlg)
		gtk_widget_destroy (dlg);
}

typedef struct {
	GtkComboBox *combo;
	GSList      *formats;
} PictFormat;

static void
add_if_writable (GdkPixbufFormat *data, PictFormat *format)
{
	if (gdk_pixbuf_format_is_writable (data)) {
		gchar *str;

		str= g_strdup_printf ("%s (%s)", gdk_pixbuf_format_get_name (data),
				      gdk_pixbuf_format_get_description (data));
		gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (format->combo), str);
		g_free (str);
		format->formats = g_slist_append (format->formats, g_strdup (gdk_pixbuf_format_get_name (data)));
	}
}

static void
file_save_cb (GtkMenuItem *mitem, PictMenuData *menudata)
{
	GtkWidget *dlg;
	GtkWidget *combo, *expander, *hbox, *label;
	GSList *formats;
	PictFormat pictformat;

	GtkWidget *attach_to;
	attach_to = g_object_get_data (G_OBJECT (mitem), "attach-to");
	if (!attach_to)
		attach_to = GTK_WIDGET (mitem);

	/* determine writable formats */
	expander = gtk_expander_new (_("Image format"));
	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_container_add (GTK_CONTAINER (expander), hbox);

	label = gtk_label_new (_("Format image as:"));
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
	combo = gtk_combo_box_text_new ();
	gtk_box_pack_start (GTK_BOX (hbox), combo, TRUE, TRUE, 0);
	gtk_widget_show_all (hbox);

	formats = gdk_pixbuf_get_formats ();
	pictformat.combo = (GtkComboBox*) combo;
	pictformat.formats = NULL;
	g_slist_foreach (formats, (GFunc) add_if_writable, &pictformat);
	g_slist_free (formats);

	gtk_combo_box_text_prepend_text (GTK_COMBO_BOX_TEXT (combo), _("Current format"));
	gtk_combo_box_set_active (GTK_COMBO_BOX (combo), 0);

	dlg = gtk_file_chooser_dialog_new (_("Select a file to save the image to"), 
					   GTK_WINDOW (gtk_widget_get_toplevel (attach_to)),
					   GTK_FILE_CHOOSER_ACTION_SAVE, 
					   _("_Cancel"), GTK_RESPONSE_CANCEL,
					   _("_Save"), GTK_RESPONSE_ACCEPT,
					   NULL);
	gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (dlg),
					     gdaui_get_default_path ());

	gtk_file_chooser_set_extra_widget (GTK_FILE_CHOOSER (dlg), expander);
	if (gtk_dialog_run (GTK_DIALOG (dlg)) == GTK_RESPONSE_ACCEPT) {
		char *filename;
		gboolean allok = TRUE;
		GError *error = NULL;
		gint format;

		format = gtk_combo_box_get_active (GTK_COMBO_BOX (combo));
		filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dlg));
		gdaui_set_default_path (gtk_file_chooser_get_current_folder (GTK_FILE_CHOOSER (dlg)));

		if (format == 0) {
			/* save data AS IS */
			allok = g_file_set_contents (filename, (gchar *) menudata->bindata->data, 
						     menudata->bindata->data_length, &error);
		}
		else {
			/* export data to another format */
			GdkPixbuf *pixbuf;
			gchar *format_str;
			const gchar *stock;

			format_str = g_slist_nth_data (pictformat.formats, format - 1);
			pixbuf = common_pict_make_pixbuf (menudata->options, menudata->bindata, NULL, &stock, &error);
			if (pixbuf) {
				allok = gdk_pixbuf_save (pixbuf, filename, format_str, &error, NULL);
				g_object_unref (pixbuf);
			}
			else 
				allok = FALSE;
		}

		if (!allok) {
			GtkWidget *msg;
			gchar *tmp;
			tmp = g_strdup_printf (_("Could not save the image to '%s'"), filename);
			msg = gtk_message_dialog_new_with_markup (GTK_WINDOW (gtk_widget_get_toplevel (attach_to)),
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
	
	if (dlg)
		gtk_widget_destroy (dlg);

	g_slist_free_full (pictformat.formats, (GDestroyNotify) g_free);
}

static void
copy_cb (G_GNUC_UNUSED GtkMenuItem *mitem, PictMenuData *menudata)
{
	GtkClipboard *cp;
	cp = gtk_clipboard_get_default (gdk_display_get_default ());
	if (!cp)
		return;

	GdkPixbufLoader *loader;
	GdkPixbuf *pixbuf = NULL;
	loader = gdk_pixbuf_loader_new ();
	if (gdk_pixbuf_loader_write (loader, menudata->bindata->data,
				     menudata->bindata->data_length, NULL)) {
		if (gdk_pixbuf_loader_close (loader, NULL)) {
			pixbuf = gdk_pixbuf_loader_get_pixbuf (loader);
			g_object_ref (pixbuf);
		}
		else
			gdk_pixbuf_loader_close (loader, NULL);
	}
	else
		gdk_pixbuf_loader_close (loader, NULL);
	g_object_unref (loader);
	
	if (pixbuf) {
		gtk_clipboard_set_image (cp, pixbuf);
		g_object_unref (pixbuf);
	}
	else
		gtk_clipboard_set_image (cp, NULL);
}

/* 
 * adjust the sensitiveness of the menu items in the popup menu
 */
void
common_pict_adjust_menu_sensitiveness (PictMenu *pictmenu, gboolean editable, PictBinData *bindata)
{
	if (!pictmenu || !pictmenu->menu)
		return;
	gtk_widget_set_sensitive (pictmenu->load_mitem, editable);
	gtk_widget_set_sensitive (pictmenu->save_mitem, bindata->data ? TRUE : FALSE);
	gtk_widget_set_sensitive (pictmenu->copy_mitem, bindata->data ? TRUE : FALSE);
}

/*
 * Inits the hash table in @options
 */
void
common_pict_init_cache (PictOptions *options)
{
	g_assert (!options->pixbuf_hash);
	options->pixbuf_hash = g_hash_table_new_full (g_int_hash, g_int_equal,
						      g_free, g_object_unref);
}

/* 
 * computes a "hash" of the binary data 
 */
static gint
compute_hash (guchar *data, glong data_length)
{
	gint result = 0;
	guchar *ptr;

	if (!data)
		return 0;
	for (ptr = data; ptr <= data + data_length - 1; ptr++)
		result += *ptr;

	return result;
}

/*
 * Adds @pixbuf in the cache
 */
void
common_pict_add_cached_pixbuf (PictOptions *options, const GValue *value, GdkPixbuf *pixbuf)
{
	gint *hash;
	g_return_if_fail (pixbuf);

	if (!options->pixbuf_hash || !value)
		return;
	else if (GDA_VALUE_HOLDS_BINARY (value)) {
		GdaBinary *bin;
		bin = g_value_get_boxed (value);
		hash = g_new (gint, 1);
		*hash = compute_hash (gda_binary_get_data (bin), gda_binary_get_size (bin));
		g_hash_table_insert (options->pixbuf_hash, hash, g_object_ref (pixbuf));
	}
	else if (GDA_VALUE_HOLDS_BLOB (value)) {
		GdaBinary *bin;
		GdaBlob *blob;
		blob = g_value_get_boxed (value);
		bin = (GdaBinary*) gda_blob_get_binary (blob);
		if (bin) {
			if (!gda_binary_get_data (bin) && gda_blob_get_op (blob))
				gda_blob_op_read_all (gda_blob_get_op (blob), (GdaBlob*) blob);
			hash = g_new (gint, 1);
			*hash = compute_hash (gda_binary_get_data (bin), gda_binary_get_size (bin));
			g_hash_table_insert (options->pixbuf_hash, hash, g_object_ref (pixbuf));
		}
	}
}

/*
 * Tries to find a cached pixbuf
 */
GdkPixbuf *
common_pict_fetch_cached_pixbuf (PictOptions *options, const GValue *value)
{
	GdkPixbuf *pixbuf = NULL;
	gint hash;

	if (!options->pixbuf_hash)
		return NULL;
	if (!value)
		return NULL;
	else if (GDA_VALUE_HOLDS_BINARY (value)) {
		GdaBinary *bin;
		bin = g_value_get_boxed (value);
		if (bin) {
			hash = compute_hash (gda_binary_get_data (bin), gda_binary_get_size (bin));
			pixbuf = g_hash_table_lookup (options->pixbuf_hash, &hash);
		}
	}
	else if (GDA_VALUE_HOLDS_BLOB (value)) {
		GdaBinary *bin;
		GdaBlob *blob;
		blob = g_value_get_boxed (value);
		bin = (GdaBinary*) gda_blob_get_binary (blob);
		if (bin) {
			if (!gda_binary_get_data (bin) && gda_blob_get_op (blob))
				gda_blob_op_read_all (gda_blob_get_op (blob), (GdaBlob*) blob);
			hash = compute_hash (gda_binary_get_data (bin), gda_binary_get_size (bin));
			pixbuf = g_hash_table_lookup (options->pixbuf_hash, &hash);
		}
	}

	return pixbuf;
}

/*
 * clears all the cached pixbuf objects
 */
void
common_pict_clear_pixbuf_cache (PictOptions *options)
{
	if (!options->pixbuf_hash)
		return;
	g_hash_table_remove_all (options->pixbuf_hash);
}

/*
 * Fills @options with the correct values parsed from @options_str
 */
void
common_pict_parse_options (PictOptions *options, const gchar *options_str)
{
	if (options_str && *options_str) {
		GdaQuarkList *params;
		const gchar *str;

		params = gda_quark_list_new_from_string (options_str);
		str = gda_quark_list_find (params, "ENCODING");
		if (str) {
			if (!strcmp (str, "base64")) 
				options->encoding = ENCODING_BASE64;
		}
		gda_quark_list_free (params);
	}
}

/*
 * Creates a new GValue from the data in @bindata, using @options, of type @gtype
 */
GValue *
common_pict_get_value (PictBinData *bindata, PictOptions *options, GType gtype)
{
	GValue *value = NULL;

	if (bindata->data) {
		if (gtype == GDA_TYPE_BLOB) 
			value = gda_value_new_blob (bindata->data, bindata->data_length);
		else if (gtype == GDA_TYPE_BINARY) 
			value = gda_value_new_binary (bindata->data, bindata->data_length);
		else if (gtype == G_TYPE_STRING) {
			gchar *str = NULL;

			switch (options->encoding) {
			case ENCODING_NONE:
				str = g_strndup ((gchar *) bindata->data, 
						 bindata->data_length);
				break;
			case ENCODING_BASE64: 
				str = g_base64_encode (bindata->data, bindata->data_length);
				break;
			}
			
			value = gda_value_new (G_TYPE_STRING);
			g_value_take_string (value, str);
		}
		else
			g_assert_not_reached ();
	}

	if (!value) {
		/* in case the gda_data_handler_get_value_from_sql() returned an error because
		   the contents of the GtkEntry cannot be interpreted as a GValue */
		value = gda_value_new_null ();
	}

	return value;
}
