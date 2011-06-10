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

#ifndef __GDAUI_LOGIN_DIALOG_H__
#define __GDAUI_LOGIN_DIALOG_H__

#include <libgda-ui/gdaui-login.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GDAUI_TYPE_LOGIN_DIALOG            (gdaui_login_dialog_get_type())
#define GDAUI_LOGIN_DIALOG(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDAUI_TYPE_LOGIN_DIALOG, GdauiLoginDialog))
#define GDAUI_LOGIN_DIALOG_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDAUI_TYPE_LOGIN_DIALOG, GdauiLoginDialogClass))
#define GDAUI_IS_LOGIN_DIALOG(obj)         (G_TYPE_CHECK_INSTANCE_TYPE (obj, GDAUI_TYPE_LOGIN_DIALOG))
#define GDAUI_IS_LOGIN_DIALOG_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GDAUI_TYPE_LOGIN_DIALOG))

typedef struct _GdauiLoginDialog        GdauiLoginDialog;
typedef struct _GdauiLoginDialogClass   GdauiLoginDialogClass;
typedef struct _GdauiLoginDialogPrivate GdauiLoginDialogPrivate;

struct _GdauiLoginDialog {
	GtkDialog dialog;
	GdauiLoginDialogPrivate *priv;
};

struct _GdauiLoginDialogClass {
	GtkDialogClass parent_class;
};

GType        gdaui_login_dialog_get_type         (void) G_GNUC_CONST;
GtkWidget   *gdaui_login_dialog_new              (const gchar *title, GtkWindow *parent);
gboolean     gdaui_login_dialog_run              (GdauiLoginDialog *dialog);

GdauiLogin   *gdaui_login_dialog_get_login_widget (GdauiLoginDialog *dialog);

G_END_DECLS

#endif
