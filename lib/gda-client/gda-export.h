/* GDA client libary
 * Copyright (C) 2001 The Free Software Foundation
 *
 * AUTHORS:
 *	Rodrigo Moya <rodrigo@gnome-db.org>
 *
 * This Library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this Library; see the file COPYING.LIB.  If not,
 * write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#if !defined(__gda_export_h__)
#  define __gda_export_h__ 1

#include <glib.h>
#include <gtk/gtkobject.h>
#include <gda-common-defs.h>
#include <gda-connection.h>

G_BEGIN_DECLS

/*
 * The export object. Makes it easy to perform an export operation
 */

typedef struct _GdaExport        GdaExport;
typedef struct _GdaExportClass   GdaExportClass;
typedef struct _GdaExportPrivate GdaExportPrivate;

#define GDA_TYPE_EXPORT            (gda_export_get_type())
#define GDA_EXPORT(obj)            GTK_CHECK_CAST(obj, GDA_TYPE_EXPORT, GdaExport)
#define GDA_EXPORT_CLASS(klass)    GTK_CHECK_CLASS_CAST(klass, GDA_TYPE_EXPORT, GdaExportClass)
#define GDA_IS_EXPORT(obj)         GTK_CHECK_TYPE(obj, GDA_TYPE_EXPORT)
#define GDA_IS_EXPORT_CLASS(klass) (GTK_CHECK_CLASS_TYPE((klass), GDA_TYPE_EXPORT))

struct _GdaExport {
	GtkObject object;
	GdaExportPrivate *priv;
};

struct _GdaExportClass {
	GtkObjectClass parent_class;

	void (*object_selected) (GdaExport * exp,
				 GDA_Connection_QType qtype,
				 const gchar * name);
	void (*object_unselected) (GdaExport * exp,
				   GDA_Connection_QType qtype,
				   const gchar * name);
};

GtkType        gda_export_get_type (void);

GdaExport     *gda_export_new (GdaConnection * cnc);
void           gda_export_free (GdaExport * exp);

GList         *gda_export_get_tables (GdaExport * exp);
GList         *gda_export_get_selected_tables (GdaExport * exp);
void           gda_export_select_table (GdaExport * exp, const gchar * table);
void           gda_export_unselect_table (GdaExport * exp, const gchar * table);

void           gda_export_run (GdaExport * exp);
void           gda_export_stop (GdaExport * exp);

GdaConnection *gda_export_get_connection (GdaExport * exp);
void           gda_export_set_connection (GdaExport * exp, GdaConnection * cnc);

G_END_DECLS

#endif
