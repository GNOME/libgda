/*
 * Copyright (C) 2009 - 2011 Vivien Malerba <malerba@gnome-db.org>
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

#ifndef __CC_GRAY_BAR_H__
#define __CC_GRAY_BAR_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define CC_TYPE_GRAY_BAR            (cc_gray_bar_get_type())
#define CC_GRAY_BAR(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, CC_TYPE_GRAY_BAR, CcGrayBar))
#define CC_GRAY_BAR_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, CC_TYPE_GRAY_BAR, CcGrayBarClass))
#define CC_IS_GRAY_BAR(obj)         (G_TYPE_CHECK_INSTANCE_TYPE (obj, CC_TYPE_GRAY_BAR))
#define CC_IS_GRAY_BAR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), CC_TYPE_GRAY_BAR))

typedef struct _CcGrayBar        CcGrayBar;
typedef struct _CcGrayBarClass   CcGrayBarClass;
typedef struct _CcGrayBarPrivate CcGrayBarPrivate;

struct _CcGrayBar {
	GtkBin bin;
	CcGrayBarPrivate *priv;
};

struct _CcGrayBarClass {
	GtkBinClass parent_class;
};

/**
 * SECTION:cc-gray-bar
 * @short_description: A title bar
 * @title: CcGrayBar
 * @stability: Stable
 * @see_also:
 *
 * The #CcGrayBar widget is a styled title bar
 */

GType        cc_gray_bar_get_type (void) G_GNUC_CONST;
GtkWidget   *cc_gray_bar_new (const gchar *label);
const gchar *cc_gray_bar_get_text (CcGrayBar *bar);
void         cc_gray_bar_set_text (CcGrayBar *bar, const gchar *text);
void         cc_gray_bar_set_icon_from_pixbuf  (CcGrayBar *bar, GdkPixbuf *pixbuf);
void         cc_gray_bar_set_icon_from_file  (CcGrayBar *bar, const gchar *file);
void         cc_gray_bar_set_icon_from_stock (CcGrayBar *bar, const gchar *stock_id, GtkIconSize size);
void         cc_gray_bar_set_show_icon       (CcGrayBar *bar, gboolean show);
gboolean     cc_gray_bar_get_show_icon       (CcGrayBar *bar);

G_END_DECLS

#endif
