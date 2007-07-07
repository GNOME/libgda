/* GDA Berkeley-DB data model
 * Copyright (C) 1998 - 2007 The GNOME Foundation
 *
 * AUTHORS:
 *      Laurent Sansonetti <lrz@gnome.org>
 *      Vivien Malerba <malerba@gnome-db.org>
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

#include <string.h>
#include <glib/gi18n-lib.h>
#include <libgda/gda-data-model.h>
#include <libgda/gda-data-model-bdb.h>

#define PARENT_TYPE GDA_TYPE_OBJECT
#define BDB_VERSION  (10000*DB_VERSION_MAJOR+100*DB_VERSION_MINOR+DB_VERSION_PATCH)

struct _GdaDataModelBdbPrivate {
	gchar  *filename;
	gchar  *db_name;

	DB     *dbp;
	DBC    *dbpc; /* cursor */

	GSList *errors; /* list of errors as GError structures*/

	GSList *columns;
	gint    n_columns; /* length of @columns */

	gint    n_key_columns; /* > 0 if custom number of columns */
	gint    n_data_columns;/* > 0 if custom number of columns */

	GSList *cursor_values; /* list of GValues for the current row */
};

/* properties */
enum
{
        PROP_0,
        PROP_FILENAME,
        PROP_DB_NAME
};

static void gda_data_model_bdb_class_init (GdaDataModelBdbClass *klass);
static void gda_data_model_bdb_init       (GdaDataModelBdb *model,
                                              GdaDataModelBdbClass *klass);
static void gda_data_model_bdb_dispose    (GObject *object);

static void gda_data_model_bdb_set_property (GObject *object,
                                                guint param_id,
                                                const GValue *value,
                                                GParamSpec *pspec);
static void gda_data_model_bdb_get_property (GObject *object,
                                                guint param_id,
                                                GValue *value,
                                                GParamSpec *pspec);

/* GdaDataModel interface */
static void                 gda_data_model_bdb_data_model_init (GdaDataModelClass *iface);
static gint                 gda_data_model_bdb_get_n_rows      (GdaDataModel *model);
static gint                 gda_data_model_bdb_get_n_columns   (GdaDataModel *model);
static GdaColumn           *gda_data_model_bdb_describe_column (GdaDataModel *model, gint col);
static GdaDataModelAccessFlags gda_data_model_bdb_get_access_flags(GdaDataModel *model);
static const GValue      *gda_data_model_bdb_get_value_at    (GdaDataModel *model, gint col, gint row);
static GdaValueAttribute    gda_data_model_bdb_get_attributes_at (GdaDataModel *model, gint col, gint row);

#ifdef GDA_DEBUG
static void gda_data_model_bdb_dump (GdaDataModelBdb *model, guint offset);
#endif
static void add_error (GdaDataModelBdb *model, const gchar *err);
static void free_current_cursor_values (GdaDataModelBdb *model);

static GObjectClass *parent_class = NULL;
#define CLASS(model) (GDA_DATA_MODEL_BDB_CLASS (G_OBJECT_GET_CLASS (model)))

/*
 * Object init and dispose
 */
static void
gda_data_model_bdb_data_model_init (GdaDataModelClass *iface)
{
        iface->i_get_n_rows = gda_data_model_bdb_get_n_rows;
        iface->i_get_n_columns = gda_data_model_bdb_get_n_columns;
        iface->i_describe_column = gda_data_model_bdb_describe_column;
        iface->i_get_access_flags = gda_data_model_bdb_get_access_flags;
        iface->i_get_value_at = gda_data_model_bdb_get_value_at;
        iface->i_get_attributes_at = gda_data_model_bdb_get_attributes_at;

        iface->i_create_iter = NULL;
        iface->i_iter_at_row = NULL;
        iface->i_iter_next = NULL;
        iface->i_iter_prev = NULL;

        iface->i_set_value_at = NULL;
        iface->i_set_values = NULL;
        iface->i_append_values = NULL;
        iface->i_append_row = NULL;
        iface->i_remove_row = NULL;
        iface->i_find_row = NULL;

        iface->i_set_notify = NULL;
        iface->i_get_notify = NULL;
        iface->i_send_hint = NULL;
}

static void
gda_data_model_bdb_init (GdaDataModelBdb *model,
			 GdaDataModelBdbClass *klass)
{
	g_return_if_fail (GDA_IS_DATA_MODEL_BDB (model));

	model->priv = g_new0 (GdaDataModelBdbPrivate, 1);
	model->priv->filename = NULL;
	model->priv->db_name = NULL;
	model->priv->columns = NULL;
	model->priv->n_columns = 0;
	model->priv->cursor_values = NULL;
}

static void
gda_data_model_bdb_class_init (GdaDataModelBdbClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	/* properties */
        object_class->set_property = gda_data_model_bdb_set_property;
        object_class->get_property = gda_data_model_bdb_get_property;
        g_object_class_install_property (object_class, PROP_FILENAME,
                                         g_param_spec_string ("filename", "DB file", NULL, NULL,
                                                              G_PARAM_READABLE | G_PARAM_WRITABLE |
                                                              G_PARAM_CONSTRUCT_ONLY));
        g_object_class_install_property (object_class, PROP_DB_NAME,
                                         g_param_spec_string ("db_name", "Name of the database", NULL, NULL,
                                                              G_PARAM_READABLE | G_PARAM_WRITABLE |
                                                              G_PARAM_CONSTRUCT_ONLY));

	/* virtual functions */
	object_class->dispose = gda_data_model_bdb_dispose;
#ifdef GDA_DEBUG
        GDA_OBJECT_CLASS (klass)->dump = (void (*)(GdaObject *, guint)) gda_data_model_bdb_dump;
#endif

        /* class attributes */
        GDA_OBJECT_CLASS (klass)->id_unique_enforced = FALSE;
}

static void
gda_data_model_bdb_dispose (GObject * object)
{
	GdaDataModelBdb *model = (GdaDataModelBdb *) object;

	g_return_if_fail (GDA_IS_DATA_MODEL_BDB (model));

	if (model->priv) {
		if (model->priv->dbp) {
			model->priv->dbp->close (model->priv->dbp, 0);
			model->priv->dbp = NULL;
		}

		if (model->priv->errors) {
                        g_slist_foreach (model->priv->errors, (GFunc) g_error_free, NULL);
                        g_slist_free (model->priv->errors);
                }
		if (model->priv->columns) {
                        g_slist_foreach (model->priv->columns, (GFunc) g_object_unref, NULL);
                        g_slist_free (model->priv->columns);
                        model->priv->columns = NULL;
                }
		if (model->priv->cursor_values) 
			free_current_cursor_values (model);
		g_free (model->priv->filename);
		g_free (model->priv->db_name);
		g_free (model->priv);
		model->priv = NULL;
	}

	parent_class->dispose (object);
}

static void
free_current_cursor_values (GdaDataModelBdb *model)
{
	g_slist_foreach (model->priv->cursor_values, (GFunc) gda_value_free, NULL);
	g_slist_free (model->priv->cursor_values);
	model->priv->cursor_values = NULL;
}

static void
add_error (GdaDataModelBdb *model, const gchar *err)
{
	GError *error = NULL;

        g_set_error (&error, 0, 0, err);
        model->priv->errors = g_slist_append (model->priv->errors, error);
}

/*
 * Public functions
 */

GType
gda_data_model_bdb_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (GdaDataModelBdbClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_data_model_bdb_class_init,
			NULL,
			NULL,
			sizeof (GdaDataModelBdb),
			0,
			(GInstanceInitFunc) gda_data_model_bdb_init
		};
		static const GInterfaceInfo data_model_info = {
                        (GInterfaceInitFunc) gda_data_model_bdb_data_model_init,
                        NULL,
                        NULL
                };

		type = g_type_register_static (PARENT_TYPE, "GdaDataModelBdb", &info, 0);
		g_type_add_interface_static (type, GDA_TYPE_DATA_MODEL, &data_model_info);
	}
	return type;
}

static void
gda_data_model_bdb_set_property (GObject *object,
                                    guint param_id,
                                    const GValue *value,
                                    GParamSpec *pspec)
{
        GdaDataModelBdb *model;
        const gchar *string;
	static gboolean db_name_set = FALSE, db_file_set = FALSE;

        model = GDA_DATA_MODEL_BDB (object);
        if (model->priv) {
                switch (param_id) {
                case PROP_FILENAME:
			if (model->priv->filename) {
				g_free (model->priv->filename);
				model->priv->filename = NULL;
			}
			string = g_value_get_string (value);
			if (string) {
				model->priv->filename = g_strdup (string);
				db_file_set = TRUE;
			}
			break;
                case PROP_DB_NAME:
			if (model->priv->db_name) {
				g_free (model->priv->db_name);
				model->priv->db_name = NULL;
			}
			string = g_value_get_string (value);
			if (string) 
				model->priv->db_name = g_strdup (string);
			db_name_set = TRUE;
			break;
		}
	}

	if (db_name_set && db_file_set) {
		/* open the DB file */
		int ret;
		DBC *dbpc;
		DB *dbp;

		/* open database */
		ret = db_create (&dbp, NULL, 0);
		if (ret) {
			add_error (model, db_strerror (ret));
			goto out;
		}
		
		model->priv->dbp = dbp;
		ret = dbp->open (dbp,
#if BDB_VERSION >= 40124 
				 NULL,
#endif
				 model->priv->filename,
				 model->priv->db_name,
				 DB_UNKNOWN, /* autodetect DBTYPE */
				 0, 0);
		if (ret) {
			add_error (model, db_strerror (ret));
			goto out;
		}

		/* get cursor */
		ret = dbp->cursor (dbp, NULL, &dbpc, 0);
		if (ret) {
			add_error (model, db_strerror (ret));
			goto out;
		}
		model->priv->dbpc = dbpc;

		model->priv->columns = NULL;

		model->priv->n_key_columns = 0;
		if (CLASS (model)->create_key_columns) {
			model->priv->columns = CLASS (model)->create_key_columns (model);
			model->priv->n_key_columns = g_slist_length (model->priv->columns);
		}
		if (model->priv->n_key_columns == 0) {
			GdaColumn *column;
			
                        column = gda_column_new ();
                        model->priv->columns = g_slist_append (model->priv->columns , column);
			gda_column_set_name (column, "key");
			gda_column_set_title (column, "key");
			gda_column_set_g_type (column, GDA_TYPE_BINARY);
		}

		model->priv->n_data_columns = 0;
		if (CLASS (model)->create_data_columns) {
			GSList *list;
			list = CLASS (model)->create_data_columns (model);
			model->priv->columns = g_slist_concat (model->priv->columns, list);
			model->priv->n_data_columns = g_slist_length (list);
		}
		if (model->priv->n_data_columns == 0) {
			GdaColumn *column;

                        column = gda_column_new ();
                        model->priv->columns = g_slist_append (model->priv->columns , column);
			gda_column_set_name (column, "data");
			gda_column_set_title (column, "data");
			gda_column_set_g_type (column, GDA_TYPE_BINARY);
		}
		model->priv->n_columns = g_slist_length (model->priv->columns);
	}

 out:
	return;
}

static void
gda_data_model_bdb_get_property (GObject *object,
                                    guint param_id,
                                    GValue *value,
                                    GParamSpec *pspec)
{
        GdaDataModelBdb *model;

        model = GDA_DATA_MODEL_BDB (object);
        if (model->priv) {
                switch (param_id) {
                case PROP_FILENAME:
			g_value_set_string (value, model->priv->filename);
			break;
                case PROP_DB_NAME:
			g_value_set_string (value, model->priv->db_name);
			break;
		}
	}
}

#ifdef GDA_DEBUG
static void
gda_data_model_bdb_dump (GdaDataModelBdb *model, guint offset)
{
	gchar *stroff;

	stroff = g_new0 (gchar, offset+1);
	memset (stroff, ' ', offset);

	if (model->priv) {
		g_print ("%s" D_COL_H1 "GdaDataModelBdb %p (name=%s, id=%s)\n" D_COL_NOR,
			 stroff, model, gda_object_get_name (GDA_OBJECT (model)), 
			 gda_object_get_id (GDA_OBJECT (model)));
		
        }
        else
                g_print ("%s" D_COL_ERR "Using finalized object %p" D_COL_NOR, stroff, model);

	g_free (stroff);
}
#endif


/**
 * gda_data_model_bdb_new
 * @filename:
 * @db_name: the name of the database within @filename, or %NULL
 *
 * Creates a new #GdaDataModel object to access the contents of the Berkeley DB file @file,
 * for the database @db_name if not %NULL
 *
 * Returns: a new #GdaDataModel
 */
GdaDataModel *
gda_data_model_bdb_new (const gchar *filename, const gchar *db_name)
{
	GdaDataModel *model;

	g_return_val_if_fail (filename && *filename, NULL);

	model = (GdaDataModel *) g_object_new (GDA_TYPE_DATA_MODEL_BDB, "db_name", db_name, 
					       "filename", filename, NULL);

	return model;
}



static gint
gda_data_model_bdb_get_n_rows (GdaDataModel *model)
{
	GdaDataModelBdb *imodel = (GdaDataModelBdb *) model;
	int ret;
	DB_BTREE_STAT *statp;

	g_return_val_if_fail (GDA_IS_DATA_MODEL_BDB (imodel), 0);
	g_return_val_if_fail (imodel->priv != NULL, 0);
	if (!imodel->priv->dbp)
		return 0;
	
	ret = imodel->priv->dbp->stat (imodel->priv->dbp,
#if BDB_VERSION > 40300
			 NULL,
#endif
			 &statp,

#if BDB_VERSION < 40000
			 NULL,
#endif
			 0);
	if (ret) {
		add_error (imodel, db_strerror (ret));
		return 0;
	}
	else {
		ret = (int) statp->bt_ndata;
		free (statp);
		return ret;
	}
}

static gint
gda_data_model_bdb_get_n_columns (GdaDataModel *model)
{
	GdaDataModelBdb *imodel;
        g_return_val_if_fail (GDA_IS_DATA_MODEL_BDB (model), 0);
        imodel = GDA_DATA_MODEL_BDB (model);
        g_return_val_if_fail (imodel->priv, 0);

        if (imodel->priv->columns)
                return imodel->priv->n_columns;
        else
                return 0;
}

static GdaColumn *
gda_data_model_bdb_describe_column (GdaDataModel *model, gint col)
{
	GdaDataModelBdb *imodel;
        g_return_val_if_fail (GDA_IS_DATA_MODEL_BDB (model), NULL);
        imodel = GDA_DATA_MODEL_BDB (model);
        g_return_val_if_fail (imodel->priv, NULL);

        if (imodel->priv->columns)
                return g_slist_nth_data (imodel->priv->columns, col);
        else
                return NULL;
}

static GdaDataModelAccessFlags
gda_data_model_bdb_get_access_flags(GdaDataModel *model)
{
	GdaDataModelBdb *imodel;
        GdaDataModelAccessFlags flags;

        g_return_val_if_fail (GDA_IS_DATA_MODEL_BDB (model), 0);
        imodel = GDA_DATA_MODEL_BDB (model);
        g_return_val_if_fail (imodel->priv, 0);

	flags = GDA_DATA_MODEL_ACCESS_CURSOR_FORWARD | 
		GDA_DATA_MODEL_ACCESS_CURSOR_BACKWARD |
		GDA_DATA_MODEL_ACCESS_RANDOM |
		GDA_DATA_MODEL_ACCESS_WRITE;

        return flags;
}

static const GValue *
gda_data_model_bdb_get_value_at (GdaDataModel *model, gint col, gint row)
{
	GdaDataModelBdb *imodel;
	DBT key, data;
	DBC *dbpc;
	GdaBinary bin;
	GValue *value;
	int ret, i;

        g_return_val_if_fail (GDA_IS_DATA_MODEL_BDB (model), NULL);
        imodel = GDA_DATA_MODEL_BDB (model);
        g_return_val_if_fail (imodel->priv, NULL);

	if ((col < 0) || (col > imodel->priv->n_columns)) {
		add_error (imodel, _("Column number out of range"));
		return NULL;
	}

	dbpc = imodel->priv->dbpc;
	memset (&key, 0, sizeof key);
        memset (&data, 0, sizeof data);
        ret = dbpc->c_get (dbpc, &key, &data, DB_FIRST);
        if (ret) {
		add_error (imodel, db_strerror (ret));
                return NULL;
        }
	for (i = 0; i < row; i++) {
                memset (&key, 0, sizeof key);
                memset (&data, 0, sizeof data);
                ret = dbpc->c_get (dbpc, &key, &data, DB_NEXT);
                if (ret) {
			add_error (imodel, db_strerror (ret));
                        return NULL;
                }
        }

	if (imodel->priv->cursor_values)
		free_current_cursor_values (imodel);
	
	/* key part */
	if (imodel->priv->n_key_columns > 0) {
		int c;
		for (c = 0; c < imodel->priv->n_key_columns; c++) {
			if (CLASS (model)->get_key_part) {
				value = CLASS (model)->get_key_part (imodel, key.data, key.size, c);
				if (!value)
					value = gda_value_new_null ();
				imodel->priv->cursor_values = g_slist_append (imodel->priv->cursor_values, 
									      value);
			}
			else {
				if (c == 0)
					g_warning (_("Custom BDB model implementation is not complete: "
						     "the 'get_key_part' method is missing"));
				value = gda_value_new_null ();
				imodel->priv->cursor_values = g_slist_append (imodel->priv->cursor_values, 
									      value);
			}
		}
	}
	else {
		value = gda_value_new (GDA_TYPE_BINARY);
		bin.data = key.data;
		bin.binary_length = key.size;
		gda_value_set_binary (value, &bin);
		imodel->priv->cursor_values = g_slist_append (imodel->priv->cursor_values, value);
	}

	/* data part */
	if (imodel->priv->n_data_columns > 0) {
		int c;
		for (c = 0; c < imodel->priv->n_data_columns; c++) {
			if (CLASS (model)->get_data_part) {
				value = CLASS (model)->get_data_part (imodel, data.data, data.size, c);
				if (!value)
					value = gda_value_new_null ();
				imodel->priv->cursor_values = g_slist_append (imodel->priv->cursor_values, 
									      value);
			}
			else {
				if (c == 0)
					g_warning (_("Custom BDB model implementation is not complete: "
						     "the 'get_data_part' method is missing"));
				value = gda_value_new_null ();
				imodel->priv->cursor_values = g_slist_append (imodel->priv->cursor_values, 
									      value);
			}
		}
	}
	else {
		value = gda_value_new (GDA_TYPE_BINARY);
		bin.data = data.data;
		bin.binary_length = data.size;
		gda_value_set_binary (value, &bin);
		imodel->priv->cursor_values = g_slist_append (imodel->priv->cursor_values, value);
	}

	return g_slist_nth_data (imodel->priv->cursor_values, col);
}

static GdaValueAttribute
gda_data_model_bdb_get_attributes_at (GdaDataModel *model, gint col, gint row)
{
	GdaDataModelBdb *imodel;
	g_return_val_if_fail (GDA_IS_DATA_MODEL_BDB (model), 0);
        imodel = GDA_DATA_MODEL_BDB (model);
        g_return_val_if_fail (imodel->priv, 0);

	if ((col < 0) || (col > 1)) {
		add_error (imodel, _("Column number out of range"));
		return 0;
	}

	return GDA_VALUE_ATTR_CAN_BE_NULL;
}
