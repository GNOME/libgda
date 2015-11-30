/*
 * Copyright (C) 2009 - 2014 Vivien Malerba <malerba@gnome-db.org>
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

#ifndef __COMMON_PICT_H__
#define __COMMON_PICT_H__

#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>
#include <gdk-pixbuf/gdk-pixdata.h>
#include <gdk/gdk.h>
#include <libgda/gda-data-handler.h>

typedef enum {
        ENCODING_NONE,
        ENCODING_BASE64
} PictEncodeType;

typedef struct {
	guchar    *data;
        glong      data_length;
} PictBinData;

typedef struct {
	PictEncodeType encoding;
	GHashTable    *pixbuf_hash; /* key = GValue pointer, value = a GdkPixbuf */
} PictOptions;

typedef struct {
	gint width;
	gint height;
} PictAllocation;

/*
 * Notice: when the PictCallback function is called, the bindata's contents are
 * given to the function, which means it is responsible to free @bindata->data
 */
typedef void (*PictCallback) (PictBinData *, gpointer);
typedef struct {
	GtkWidget    *menu; /* popup menu */
	GtkWidget    *load_mitem;
	GtkWidget    *save_mitem;
	GtkWidget    *copy_mitem;
} PictMenu;

void         common_pict_parse_options (PictOptions *options, const gchar *options_str);
void         common_pict_init_cache (PictOptions *options);
void         common_pict_add_cached_pixbuf (PictOptions *options, const GValue *value, GdkPixbuf *pixbuf);
GdkPixbuf   *common_pict_fetch_cached_pixbuf (PictOptions *options, const GValue *value);
void         common_pict_clear_pixbuf_cache (PictOptions *options);

gboolean     common_pict_load_data   (PictOptions *options, const GValue *value, PictBinData *bindata, 
				      const gchar **out_icon_name, GError **error);

GdkPixbuf   *common_pict_make_pixbuf (PictOptions *options, PictBinData *bindata, PictAllocation *allocation, 
				      const gchar **out_icon_name, GError **error);

void         common_pict_create_menu (PictMenu *pictmenu, GtkWidget *attach_to, PictBinData *bindata, PictOptions  *options,
				      PictCallback callback, gpointer data);

void         common_pict_adjust_menu_sensitiveness (PictMenu *pictmenu, gboolean editable, PictBinData *bindata);
GValue      *common_pict_get_value (PictBinData *bindata, PictOptions *options, GType gtype);

#endif
