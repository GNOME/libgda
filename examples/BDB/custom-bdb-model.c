/*
 * Copyright (C) 2007 - 2013 Vivien Malerba <malerba@gnome-db.org>
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

#include <string.h>
#include "custom-bdb-model.h"
#include <glib.h>
#include "common.h"
#include <libgda/libgda.h>

#define PARENT_TYPE GDA_TYPE_DATA_MODEL_BDB
#define CLASS(obj) (GDA_DATA_MODEL_BDB_CLASS (G_OBJECT_GET_CLASS (obj)))

struct _CustomDataModelPrivate {
	
};

static void custom_data_model_class_init (CustomDataModelClass *klass);
static void custom_data_model_init       (CustomDataModel *prov, CustomDataModelClass *klass);
static GObjectClass  *parent_class = NULL;

/* virtual methods */
static GSList *custom_create_key_columns  (GdaDataModelBdb *model);
static GSList *custom_create_data_columns (GdaDataModelBdb *model);
static GValue *custom_get_key_part        (GdaDataModelBdb *model, gpointer data, gint length, gint part);
static GValue *custom_get_data_part       (GdaDataModelBdb *model, gpointer data, gint length, gint part);

/*
 * CustomDataModel class implementation
 */
static void
custom_data_model_class_init (CustomDataModelClass *klass)
{
	GdaDataModelBdbClass *bdb_class = GDA_DATA_MODEL_BDB_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);
	bdb_class->create_key_columns = custom_create_key_columns;
	bdb_class->create_data_columns = custom_create_data_columns;
	bdb_class->get_key_part = custom_get_key_part;
	bdb_class->get_data_part = custom_get_data_part;
}

static void
custom_data_model_init (CustomDataModel *prov, CustomDataModelClass *klass)
{
	prov->priv = g_new (CustomDataModelPrivate, 1);
}


GType
custom_data_model_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static GMutex registering;
		static GTypeInfo info = {
			sizeof (CustomDataModelClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) custom_data_model_class_init,
			NULL, NULL,
			sizeof (CustomDataModel),
			0,
			(GInstanceInitFunc) custom_data_model_init
		};
		
		g_mutex_lock (&registering);
		if (type == 0)
			type = g_type_register_static (PARENT_TYPE, "CustomDataModel", &info, 0);
		g_mutex_unlock (&registering);
	}

	return type;
}

GdaDataModel *
custom_data_model_new (void)
{
	GdaDataModel *model;

        model = g_object_new (custom_data_model_get_type (), NULL);
        return model;
}

static GSList *
custom_create_key_columns (GdaDataModelBdb *model)
{
	GSList *list = NULL;
	GdaColumn *column;

	column = gda_column_new ();
	list = g_slist_append (list , column);
	gda_column_set_name (column, "color");
	gda_column_set_description (column, "color");
	gda_column_set_g_type (column, G_TYPE_STRING);

	column = gda_column_new ();
	list = g_slist_append (list , column);
	gda_column_set_name (column, "type");
	gda_column_set_description (column, "type");
	gda_column_set_g_type (column, G_TYPE_INT);

	return list;
}

static GSList *
custom_create_data_columns (GdaDataModelBdb *model)
{
	GSList *list = NULL;
	GdaColumn *column;

	column = gda_column_new ();
	list = g_slist_append (list , column);
	gda_column_set_name (column, "size");
	gda_column_set_description (column, "size");
	gda_column_set_g_type (column, G_TYPE_FLOAT);

	column = gda_column_new ();
	list = g_slist_append (list , column);
	gda_column_set_name (column, "name");
	gda_column_set_description (column, "name");
	gda_column_set_g_type (column, G_TYPE_STRING);

	return list;
}

static GValue *
custom_get_key_part (GdaDataModelBdb *model, gpointer data, gint length, gint part)
{
	GValue *value;
	Key *key = (Key*) data;
	
	switch (part) {
	case 0:
		value = gda_value_new (G_TYPE_STRING);
		g_value_set_string (value, key->color);
		break;
	case 1:
		value = gda_value_new (G_TYPE_INT);
		g_value_set_int (value, key->type);
		break;
	default:
		g_assert_not_reached ();
	}

	return value;
}

static GValue *
custom_get_data_part (GdaDataModelBdb *model, gpointer data, gint length, gint part)
{
	GValue *value;
	Value *val = (Value*) data;
	
	switch (part) {
	case 0:
		value = gda_value_new (G_TYPE_FLOAT);
		g_value_set_float (value, val->size);
		break;
	case 1:
		value = gda_value_new (G_TYPE_STRING);
		g_value_set_string (value, val->name);
		break;
	default:
		g_assert_not_reached ();
	}

	return value;
}
