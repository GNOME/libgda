/* GDA library
 * Copyright (C) 1998-2002 The GNOME Foundation.
 *
 * AUTHORS:
 * 	Rodrigo Moya <rodrigo@gnome-db.org>
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

#if !defined(__gda_table_h__)
#  define __gda_table_h__

#include <libgda/gda-data-model-array.h>
#include <libgda/gda-row.h>

G_BEGIN_DECLS

#define GDA_TYPE_TABLE            (gda_table_get_type())
#define GDA_TABLE(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_TABLE, GdaTable))
#define GDA_TABLE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_TABLE, GdaTableClass))
#define GDA_IS_TABLE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE (obj, GDA_TYPE_TABLE))
#define GDA_IS_TABLE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GDA_TYPE_TABLE))

typedef struct _GdaTable        GdaTable;
typedef struct _GdaTableClass   GdaTableClass;
typedef struct _GdaTablePrivate GdaTablePrivate;

struct _GdaTable {
	GdaDataModelArray model;
	GdaTablePrivate *priv;
};

struct _GdaTableClass {
	GdaDataModelArrayClass parent_class;

	/* signals */
	void (* name_changed) (GdaTable *table, const gchar *old_name);
};

GType        gda_table_get_type (void);
GdaTable    *gda_table_new (const gchar *name);
GdaTable    *gda_table_new_from_model (const gchar *name,
				       const GdaDataModel *model,
				       gboolean add_data);

const gchar *gda_table_get_name (GdaTable *table);
void         gda_table_set_name (GdaTable *table, const gchar *name);

void         gda_table_add_field (GdaTable *table, const GdaColumn *column);
void         gda_table_add_data_from_model (GdaTable *table, const GdaDataModel *model);

GList       *gda_table_get_columns (GdaTable *table);

GdaColumn *gda_table_find_column (GdaTable *table, const gchar *name);

G_END_DECLS

#endif
