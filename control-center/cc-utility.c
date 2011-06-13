/*
 * Copyright (C) 2009 Vivien Malerba <malerba@gnome-db.org>
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

#include "cc-utility.h"

GtkWidget *
cc_new_menu_item (const gchar *label,
		 	gboolean pixmap,
		        GCallback cb_func,
			gpointer user_data)
{
	GtkWidget *item;

	if (pixmap)
		item = gtk_image_menu_item_new_from_stock (label, NULL);
	else
		item = gtk_menu_item_new_with_mnemonic (label);
	
	g_signal_connect (G_OBJECT (item), "activate",
			  G_CALLBACK (cb_func), user_data);

	return item;
}

GtkWidget *
cc_new_vbox_widget (gboolean homogenous, gint spacing)
{
	GtkWidget *vbox;

	vbox = gtk_vbox_new (homogenous, spacing);
	gtk_widget_show (vbox);

	return vbox;
}

GtkWidget *
cc_new_hbox_widget (gboolean homogenous, gint spacing)
{
	GtkWidget *hbox;

	hbox = gtk_hbox_new (homogenous, spacing);
	gtk_widget_show (hbox);

	return hbox;
}

GtkWidget *
cc_new_table_widget (gint rows, gint cols, gboolean homogenous)
{
	GtkWidget *table;

	table = gtk_table_new (rows, cols, homogenous);
	gtk_table_set_row_spacings (GTK_TABLE (table), 6);
        gtk_table_set_col_spacings (GTK_TABLE (table), 6);

	gtk_widget_show (table);

	return table;
}

GtkWidget *
cc_new_label_widget (const gchar *text)
{
	GtkWidget *label;

	label = gtk_label_new_with_mnemonic (text);
	gtk_label_set_selectable (GTK_LABEL (label), TRUE);
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_widget_show (label);

	return label;
}
