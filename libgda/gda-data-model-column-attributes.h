/* GDA library
 * Copyright (C) 1998 - 2002 The GNOME Foundation.
 *
 * AUTHORS:
 *      Michael Lausch <michael@lausch.at>
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

#if !defined(__gda_data_column_attributes_h__)
#  define __gda_data_column_attributes_h__

#include <libgda/gda-value.h>
#include <glib/gmacros.h>
#include <libgda/global-decl.h>

G_BEGIN_DECLS


struct _GdaDataModelColumnAttributes {
	gint         defined_size;
	gchar       *name;
	gchar       *table;
	gchar       *caption;
	gint         scale;

	gchar       *dbms_type;
	GdaValueType gda_type;

	gboolean     allow_null;
	gboolean     primary_key;
	gboolean     unique_key;
	gchar       *references;

	gboolean     auto_increment;
	glong        auto_increment_start;
	glong        auto_increment_step;
	gint         position;
	GdaValue    *default_value;
};

#define GDA_TYPE_DATA_MODEL_COLUMN_ATTRIVUTES (gda_data_model_column_attributes_get_type ())

GType               gda_data_model_column_attributes_get_type (void);
GdaDataModelColumnAttributes *gda_data_model_column_attributes_new (void);
GdaDataModelColumnAttributes *gda_data_model_column_attributes_copy (GdaDataModelColumnAttributes *fa);
void                gda_data_model_column_attributes_free (GdaDataModelColumnAttributes *fa);
gboolean            gda_data_model_column_attributes_equal (const GdaDataModelColumnAttributes *lhs, 
							    const GdaDataModelColumnAttributes *rhs);
glong               gda_data_model_column_attributes_get_defined_size (GdaDataModelColumnAttributes *fa);
void                gda_data_model_column_attributes_set_defined_size (GdaDataModelColumnAttributes *fa, glong size);
const gchar        *gda_data_model_column_attributes_get_name (GdaDataModelColumnAttributes *fa);
void                gda_data_model_column_attributes_set_name (GdaDataModelColumnAttributes *fa, const gchar *name);
const gchar        *gda_data_model_column_attributes_get_table (GdaDataModelColumnAttributes *fa);
void                gda_data_model_column_attributes_set_table (GdaDataModelColumnAttributes *fa, const gchar *table);
const gchar        *gda_data_model_column_attributes_get_caption (GdaDataModelColumnAttributes *fa);
void                gda_data_model_column_attributes_set_caption (GdaDataModelColumnAttributes *fa, const gchar *caption);
glong               gda_data_model_column_attributes_get_scale (GdaDataModelColumnAttributes *fa);
void                gda_data_model_column_attributes_set_scale (GdaDataModelColumnAttributes *fa, glong scale);
GdaValueType        gda_data_model_column_attributes_get_gdatype (GdaDataModelColumnAttributes *fa);
void                gda_data_model_column_attributes_set_gdatype (GdaDataModelColumnAttributes *fa, GdaValueType type);
gboolean            gda_data_model_column_attributes_get_allow_null (GdaDataModelColumnAttributes *fa);
void                gda_data_model_column_attributes_set_allow_null (GdaDataModelColumnAttributes *fa, gboolean allow);
gboolean            gda_data_model_column_attributes_get_primary_key (GdaDataModelColumnAttributes *fa);
void                gda_data_model_column_attributes_set_primary_key (GdaDataModelColumnAttributes *fa, gboolean pk);
gboolean            gda_data_model_column_attributes_get_unique_key (GdaDataModelColumnAttributes *fa);
void                gda_data_model_column_attributes_set_unique_key (GdaDataModelColumnAttributes *fa, gboolean uk);
const gchar        *gda_data_model_column_attributes_get_references (GdaDataModelColumnAttributes *fa);
void                gda_data_model_column_attributes_set_references (GdaDataModelColumnAttributes *fa, const gchar *ref);
gboolean            gda_data_model_column_attributes_get_auto_increment (GdaDataModelColumnAttributes *fa);
void                gda_data_model_column_attributes_set_auto_increment (GdaDataModelColumnAttributes *fa,
							     gboolean is_auto);
gint                gda_data_model_column_attributes_get_position (GdaDataModelColumnAttributes *fa);
void                gda_data_model_column_attributes_set_position (GdaDataModelColumnAttributes *fa, gint position);

const GdaValue     *gda_data_model_column_attributes_get_default_value (GdaDataModelColumnAttributes *fa);
void                gda_data_model_column_attributes_set_default_value (GdaDataModelColumnAttributes *fa, const GdaValue *default_value);

G_END_DECLS

#endif
