/*
 * Copyright (C) 2009 - 2014 Vivien Malerba <malerba@gnome-db.org>
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


#ifndef __AUTH_DIALOG_H_
#define __AUTH_DIALOG_H_

#include <libgda-ui/libgda-ui.h>
#include <gtk/gtk.h>
#include <common/t-connection.h>

G_BEGIN_DECLS

#define AUTH_TYPE_DIALOG          (auth_dialog_get_type())
#define AUTH_DIALOG(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, auth_dialog_get_type(), AuthDialog)
#define AUTH_DIALOG_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, auth_dialog_get_type (), AuthDialogClass)
#define AUTH_IS_DIALOG(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, auth_dialog_get_type ())

typedef struct _AuthDialog AuthDialog;
typedef struct _AuthDialogClass AuthDialogClass;
typedef struct _AuthDialogPrivate AuthDialogPrivate;

/* error reporting */
extern GQuark auth_dialog_error_quark (void);
#define AUTH_DIALOG_ERROR auth_dialog_error_quark ()

typedef enum {
	AUTH_DIALOG_CANCELLED_ERROR,
} AuthDialogError;

/* struct for the object's data */
struct _AuthDialog
{
	GtkDialog                 object;
	AuthDialogPrivate       *priv;
};


/* struct for the object's class */
struct _AuthDialogClass
{
	GtkDialogClass          parent_class;
};

GType               auth_dialog_get_type          (void) G_GNUC_CONST;
AuthDialog         *auth_dialog_new               (GtkWindow *parent);
gboolean            auth_dialog_add_cnc_string    (AuthDialog *dialog, const gchar *cnc_string, GError **error);

gboolean            auth_dialog_run               (AuthDialog *dialog);

G_END_DECLS

#endif
