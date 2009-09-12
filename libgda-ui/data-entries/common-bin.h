/* common-bin.h
 * Copyright (C) 2009  Vivien Malerba <malerba@gnome-db.org>
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

#ifndef __COMMON_BIN_H__
#define __COMMON_BIN_H__

#include <gtk/gtk.h>
#include "../internal/popup-container.h"

typedef void (*BinCallback) (gpointer, GValue *);
typedef struct {
        GtkWidget    *popup; /* PopupContainer popup window */
        GtkWidget    *load_button;
        GtkWidget    *save_button;
	GtkWidget    *props_label;

	GType         entry_type;
	GValue       *tmpvalue;

	BinCallback   loaded_value_cb;
	gpointer      loaded_value_cb_data;
} BinMenu;


void         common_bin_create_menu (BinMenu *binmenu, PopupContainerPositionFunc pos_func, GType entry_type,
				     BinCallback loaded_value_cb, gpointer loaded_value_cb_data);
void         common_bin_adjust_menu (BinMenu *binmenu, gboolean editable, const GValue *value);
void         common_bin_reset (BinMenu *binmenu);

#endif
