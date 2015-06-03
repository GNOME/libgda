/*
 * Copyright (C) 2011 - 2014 Vivien Malerba <malerba@gnome-db.org>
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

#ifndef __VTABLE_DIALOG_H__
#define __VTABLE_DIALOG_H__

#include "common/t-connection.h"

G_BEGIN_DECLS

#define VTABLE_DIALOG_TYPE            (vtable_dialog_get_type())
#define VTABLE_DIALOG(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, VTABLE_DIALOG_TYPE, VtableDialog))
#define VTABLE_DIALOG_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, VTABLE_DIALOG_TYPE, VtableDialogClass))
#define IS_VTABLE_DIALOG(obj)         (G_TYPE_CHECK_INSTANCE_TYPE (obj, VTABLE_DIALOG_TYPE))
#define IS_VTABLE_DIALOG_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), VTABLE_DIALOG_TYPE))

typedef struct _VtableDialog        VtableDialog;
typedef struct _VtableDialogClass   VtableDialogClass;
typedef struct _VtableDialogPrivate VtableDialogPrivate;

struct _VtableDialog {
	GtkDialog            parent;
	VtableDialogPrivate *priv;
};

struct _VtableDialogClass {
	GtkDialogClass       parent_class;
};

GType           vtable_dialog_get_type       (void) G_GNUC_CONST;

GtkWidget      *vtable_dialog_new            (GtkWindow *parent, TConnection *tcnc);
const gchar    *vtable_dialog_get_table_name (VtableDialog *dlg);
gboolean        vtable_dialog_get_replace_if_exists (VtableDialog *dlg);

G_END_DECLS

#endif
