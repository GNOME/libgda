/*
 * Copyright (C) 2009 - 2015 Vivien Malerba <malerba@gnome-db.org>
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

#ifndef __GDAUI_BAR_H__
#define __GDAUI_BAR_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

/*
 * Type checking and casting macros
 */
#define GDAUI_TYPE_BAR              (gdaui_bar_get_type())
#define GDAUI_BAR(obj)              (G_TYPE_CHECK_INSTANCE_CAST((obj), GDAUI_TYPE_BAR, GdauiBar))
#define GDAUI_BAR_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST((klass), GDAUI_TYPE_BAR, GdauiBarClass))
#define GDAUI_IS_BAR(obj)           (G_TYPE_CHECK_INSTANCE_TYPE((obj), GDAUI_TYPE_BAR))
#define GDAUI_IS_BAR_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GDAUI_TYPE_BAR))
#define GDAUI_BAR_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS((obj), GDAUI_TYPE_BAR, GdauiBarClass))


typedef struct _GdauiBarPrivate GdauiBarPrivate;
typedef struct _GdauiBarClass GdauiBarClass;
typedef struct _GdauiBar GdauiBar;

struct _GdauiBar
{
	GtkBox parent;

	/*< private > */
	GdauiBarPrivate *priv;
};


struct _GdauiBarClass
{
	GtkBoxClass parent_class;
};

GType        gdaui_bar_get_type                  (void) G_GNUC_CONST;
GtkWidget   *gdaui_bar_new                       (const gchar *text);
const gchar *gdaui_bar_get_text                  (GdauiBar *bar);
void         gdaui_bar_set_text                  (GdauiBar *bar, const gchar *text);
void         gdaui_bar_set_icon_from_file        (GdauiBar *bar, const gchar *file);
void         gdaui_bar_set_icon_from_resource    (GdauiBar *bar, const gchar *resource_name);
void         gdaui_bar_set_icon_from_pixbuf      (GdauiBar *bar, GdkPixbuf *pixbuf);
void         gdaui_bar_set_icon_from_icon_name   (GdauiBar *bar, const gchar *icon_name);
void         gdaui_bar_set_show_icon             (GdauiBar *bar, gboolean show);
gboolean     gdaui_bar_get_show_icon             (GdauiBar *bar);

GtkWidget   *gdaui_bar_add_search_entry          (GdauiBar *bar);
void         gdaui_bar_add_widget                (GdauiBar *bar, GtkWidget *widget);
GtkWidget   *gdaui_bar_add_button_from_icon_name (GdauiBar *bar, const gchar *icon_name);

G_END_DECLS

#endif
