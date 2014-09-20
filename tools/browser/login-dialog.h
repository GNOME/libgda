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


#ifndef __LOGIN_DIALOG_H_
#define __LOGIN_DIALOG_H_

#include <libgda-ui/libgda-ui.h>
#include <gtk/gtk.h>
#include <common/t-connection.h>

G_BEGIN_DECLS

#define LOGIN_TYPE_DIALOG          (login_dialog_get_type())
#define LOGIN_DIALOG(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, login_dialog_get_type(), LoginDialog)
#define LOGIN_DIALOG_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, login_dialog_get_type (), LoginDialogClass)
#define LOGIN_IS_DIALOG(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, login_dialog_get_type ())

typedef struct _LoginDialog LoginDialog;
typedef struct _LoginDialogClass LoginDialogClass;
typedef struct _LoginDialogPrivate LoginDialogPrivate;

/* error reporting */
extern GQuark login_dialog_error_quark (void);
#define LOGIN_DIALOG_ERROR login_dialog_error_quark ()

typedef enum {
	LOGIN_DIALOG_CANCELLED_ERROR,
} LoginDialogError;

/* struct for the object's data */
struct _LoginDialog
{
	GtkDialog                 object;
	LoginDialogPrivate       *priv;
};


/* struct for the object's class */
struct _LoginDialogClass
{
	GtkDialogClass          parent_class;
};

GType               login_dialog_get_type            (void) G_GNUC_CONST;
LoginDialog        *login_dialog_new                 (GtkWindow *parent);
TConnection        *login_dialog_run_open_connection (LoginDialog *dialog, gboolean retry, GError **error);

G_END_DECLS

#endif
