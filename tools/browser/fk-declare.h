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


#ifndef __FK_DECLARE_H_
#define __FK_DECLARE_H_

#include <libgda-ui/libgda-ui.h>
#include <gtk/gtk.h>
#include "browser-window.h"

G_BEGIN_DECLS

#define FK_DECLARE_TYPE          (fk_declare_get_type())
#define FK_DECLARE(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, fk_declare_get_type(), FkDeclare)
#define FK_DECLARE_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, fk_declare_get_type (), FkDeclareClass)
#define IS_FK_DECLARE(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, fk_declare_get_type ())

typedef struct _FkDeclare FkDeclare;
typedef struct _FkDeclareClass FkDeclareClass;
typedef struct _FkDeclarePrivate FkDeclarePrivate;

/* struct for the object's data */
struct _FkDeclare
{
	GtkDialog               object;
	FkDeclarePrivate       *priv;
};

/* struct for the object's class */
struct _FkDeclareClass
{
	GtkDialogClass          parent_class;
};


GType       fk_declare_get_type (void) G_GNUC_CONST;
GtkWidget  *fk_declare_new      (GtkWindow *parent, GdaMetaStruct *mstruct, GdaMetaTable *table);
gboolean    fk_declare_write    (FkDeclare *decl, BrowserWindow *bwin, GError **error);

gboolean    fk_declare_undeclare (GdaMetaStruct *mstruct, BrowserWindow *bwin,
				  GdaMetaTableForeignKey *decl_fk, GError **error);

G_END_DECLS

#endif
