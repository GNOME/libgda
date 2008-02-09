/* GDA library
 * Copyright (C) 2007 The GNOME Foundation.
 *
 * AUTHORS:
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

#define GDA_PG_DEBUG
#undef GDA_PG_DEBUG

#include <glib/gi18n-lib.h>
#include <libgda/gda-data-model.h>
#include <libgda/gda-data-model-private.h>
#include <string.h>
#include "gda-postgres.h"
#include "gda-postgres-cursor-recordset.h"
#include <libgda/gda-data-model-iter.h>

struct _GdaPostgresCursorRecordsetPrivate {
        GdaConnection    *cnc;
	PGconn           *pconn;
	gchar            *cursor_name;
	gint              chunk_size; /* Number of rows to fetch at a time when iterating forward or backwards. */
	gint              chunks_read; /* Effectively equal to the number of times that we have iterated forwards or backwards. */

	GSList           *columns;
        GType            *column_types;
        gint              ncolumns;
	gint              nrows; /* -1 untill the total number of rows is known */
	
	/* Pg cursor's information */
	PGresult         *pg_res; /* The result for the current chunk of rows. */
	gint              pg_pos; /* from G_MININT to G_MAXINT */
	gint              pg_res_size; /* The number of rows in the current chunk - usually equal to chunk_size when iterating forward or backward. */
	gint              pg_res_inf; /* The row number of the first row in the current chunk. Don't use if (@pg_res_size <= 0). */

	/* Internal iterator's information */
	gint              iter_row; /* G_MININT if at start, G_MAXINT if at end */
	GdaDataModelIter *iter;

	/* Misc information */
	gchar            *command_text; 
        GdaCommandType    command_type;
};

/* properties */
enum
{
        PROP_0,
        PROP_CHUNCK_SIZE,
	PROP_CHUNCKS_READ,
	PROP_COMMAND_TEXT,
        PROP_COMMAND_TYPE,
};

static void gda_postgres_cursor_recordset_class_init (GdaPostgresCursorRecordsetClass *klass);
static void gda_postgres_cursor_recordset_init       (GdaPostgresCursorRecordset *model,
					      GdaPostgresCursorRecordsetClass *klass);
static void gda_postgres_cursor_recordset_dispose    (GObject *object);
static void gda_postgres_cursor_recordset_finalize   (GObject *object);

static void gda_postgres_cursor_recordset_set_property (GObject *object,
							guint param_id,
							const GValue *value,
							GParamSpec *pspec);
static void gda_postgres_cursor_recordset_get_property (GObject *object,
							guint param_id,
							GValue *value,
							GParamSpec *pspec);

static gboolean fetch_next (GdaPostgresCursorRecordset *model);
static gboolean fetch_prev (GdaPostgresCursorRecordset *model);

/* GdaDataModel interface */
static void                 gda_postgres_cursor_recordset_data_model_init (GdaDataModelClass *iface);
static gint                 gda_postgres_cursor_recordset_get_n_rows      (GdaDataModel *model);
static gint                 gda_postgres_cursor_recordset_get_n_columns   (GdaDataModel *model);
static GdaColumn           *gda_postgres_cursor_recordset_describe_column (GdaDataModel *model, gint col);
static GdaDataModelAccessFlags gda_postgres_cursor_recordset_get_access_flags(GdaDataModel *model);
static const GValue      *gda_postgres_cursor_recordset_get_value_at    (GdaDataModel *model, gint col, gint row);
static GdaValueAttribute    gda_postgres_cursor_recordset_get_attributes_at (GdaDataModel *model, gint col, gint row);
static GdaDataModelIter    *gda_postgres_cursor_recordset_create_iter      (GdaDataModel *model);
static gboolean             gda_postgres_cursor_recordset_iter_next       (GdaDataModel *model, GdaDataModelIter *iter);
static gboolean             gda_postgres_cursor_recordset_iter_prev       (GdaDataModel *model, GdaDataModelIter *iter);
static gboolean             gda_postgres_cursor_recordset_iter_at_row       (GdaDataModel *model, GdaDataModelIter *iter, gint row);

static GObjectClass *parent_class = NULL;

/**
 * gda_postgres_cursor_recordset_get_type
 *
 * Returns: the #GType of GdaPostgresCursorRecordset.
 */
GType
gda_postgres_cursor_recordset_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo info = {
			sizeof (GdaPostgresCursorRecordsetClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_postgres_cursor_recordset_class_init,
			NULL,
			NULL,
			sizeof (GdaPostgresCursorRecordset),
			0,
			(GInstanceInitFunc) gda_postgres_cursor_recordset_init
		};

		static const GInterfaceInfo data_model_info = {
			(GInterfaceInitFunc) gda_postgres_cursor_recordset_data_model_init,
			NULL,
			NULL
		};

		type = g_type_register_static (GDA_TYPE_OBJECT, "GdaPostgresCursorRecordset", &info, 0);
		g_type_add_interface_static (type, GDA_TYPE_DATA_MODEL, &data_model_info);
	}
	return type;
}

static void 
gda_postgres_cursor_recordset_class_init (GdaPostgresCursorRecordsetClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	/* properties */
	object_class->set_property = gda_postgres_cursor_recordset_set_property;
        object_class->get_property = gda_postgres_cursor_recordset_get_property;
	g_object_class_install_property (object_class, PROP_CHUNCK_SIZE,
					 g_param_spec_int ("chunk_size", _("Number of rows fetched at a time"), NULL,
							   1, G_MAXINT - 1, 2, 
							   G_PARAM_CONSTRUCT | G_PARAM_READABLE | G_PARAM_WRITABLE));
	g_object_class_install_property (object_class, PROP_CHUNCKS_READ,
					 g_param_spec_int ("chunks_read", 
							   _("Number of rows chunks read since the object creation"), NULL,
							   0, G_MAXINT - 1, 0, 
							   G_PARAM_READABLE));
	g_object_class_install_property (object_class, PROP_COMMAND_TEXT,
                                         g_param_spec_string ("command_text", NULL, NULL,
                                                              NULL, G_PARAM_READABLE | G_PARAM_WRITABLE));
        g_object_class_install_property (object_class, PROP_COMMAND_TYPE,
                                         g_param_spec_int ("command_type", NULL, NULL,
							   GDA_COMMAND_TYPE_SQL, GDA_COMMAND_TYPE_INVALID,
                                                           GDA_COMMAND_TYPE_INVALID,
                                                           G_PARAM_READABLE | G_PARAM_WRITABLE));

	/* virtual functions */
	object_class->dispose = gda_postgres_cursor_recordset_dispose;
	object_class->finalize = gda_postgres_cursor_recordset_finalize;

	/* class attributes */
	GDA_OBJECT_CLASS (klass)->id_unique_enforced = FALSE;
}

static void
gda_postgres_cursor_recordset_data_model_init (GdaDataModelClass *iface)
{
	iface->i_get_n_rows = gda_postgres_cursor_recordset_get_n_rows;
	iface->i_get_n_columns = gda_postgres_cursor_recordset_get_n_columns;
	iface->i_describe_column = gda_postgres_cursor_recordset_describe_column;
        iface->i_get_access_flags = gda_postgres_cursor_recordset_get_access_flags;
	iface->i_get_value_at = gda_postgres_cursor_recordset_get_value_at;
	iface->i_get_attributes_at = gda_postgres_cursor_recordset_get_attributes_at;

	iface->i_create_iter = gda_postgres_cursor_recordset_create_iter;
	iface->i_iter_at_row = gda_postgres_cursor_recordset_iter_at_row;
	iface->i_iter_next = gda_postgres_cursor_recordset_iter_next;
	iface->i_iter_prev = gda_postgres_cursor_recordset_iter_prev;

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
gda_postgres_cursor_recordset_init (GdaPostgresCursorRecordset *model, GdaPostgresCursorRecordsetClass *klass)
{
	g_return_if_fail (GDA_IS_POSTGRES_CURSOR_RECORDSET (model));
	model->priv = g_new0 (GdaPostgresCursorRecordsetPrivate, 1);
	model->priv->chunk_size = 10;
	model->priv->chunks_read = 0;

	model->priv->nrows = -1; /* total number of rows unknown */
	model->priv->pg_res = NULL;
	model->priv->pg_pos = G_MININT;
	model->priv->pg_res_size = 0;

	model->priv->iter_row = G_MININT;
	model->priv->iter = NULL;

	model->priv->command_text = NULL;
	model->priv->command_type = GDA_COMMAND_TYPE_INVALID;
}

static void
gda_postgres_cursor_recordset_dispose (GObject *object)
{
	GdaPostgresCursorRecordset *model = (GdaPostgresCursorRecordset *) object;

	g_return_if_fail (GDA_IS_POSTGRES_CURSOR_RECORDSET (model));

	/* free memory */
	if (model->priv) {
		if (model->priv->iter) {
                        g_object_unref (model->priv->iter);
                        model->priv->iter = NULL;
                }

	}

	/* chain to parent class */
	parent_class->dispose (object);
}

static void
gda_postgres_cursor_recordset_finalize (GObject *object)
{
	GdaPostgresCursorRecordset *model = (GdaPostgresCursorRecordset *) object;

	g_return_if_fail (GDA_IS_POSTGRES_CURSOR_RECORDSET (model));

	/* free memory */
	if (model->priv) {
		gchar *str;
		PGresult *pg_res;

		if (model->priv->pg_res != NULL) {
			PQclear (model->priv->pg_res);
			model->priv->pg_res = NULL;
		}

		str = g_strdup_printf ("CLOSE %s", model->priv->cursor_name);
		pg_res = PQexec (model->priv->pconn, str);
		g_free (str);
		PQclear (pg_res);

		g_free (model->priv->cursor_name);

		g_free (model->priv->command_text);

		g_free (model->priv);
		model->priv = NULL;
	}

	/* chain to parent class */
	parent_class->finalize (object);
}

static void
gda_postgres_cursor_recordset_set_property (GObject *object,
					    guint param_id,
					    const GValue *value,
					    GParamSpec *pspec)
{
	GdaPostgresCursorRecordset *model = (GdaPostgresCursorRecordset *) object;
	if (model->priv) {
		switch (param_id) {
		case PROP_CHUNCK_SIZE:
			model->priv->chunk_size = g_value_get_int (value);
			break;
		case PROP_COMMAND_TEXT:
                        if (model->priv->command_text) {
                                g_free (model->priv->command_text);
                                model->priv->command_text = NULL;
                        }

                        model->priv->command_text = g_strdup (g_value_get_string (value));
                        break;
                case PROP_COMMAND_TYPE:
                        model->priv->command_type = g_value_get_int (value);
                        break;
		default:
			break;
		}
	}
}

static void
gda_postgres_cursor_recordset_get_property (GObject *object,
					    guint param_id,
					    GValue *value,
					    GParamSpec *pspec)
{
	GdaPostgresCursorRecordset *model = (GdaPostgresCursorRecordset *) object;
	if (model->priv) {
		switch (param_id) {
		case PROP_CHUNCK_SIZE:
			g_value_set_int (value, model->priv->chunk_size);
			break;
		case PROP_CHUNCKS_READ:
			g_value_set_int (value, model->priv->chunks_read);
			break;
		case PROP_COMMAND_TEXT:
                        g_value_set_string (value, model->priv->command_text);
                        break;
                case PROP_COMMAND_TYPE:
                        g_value_set_int (value, model->priv->command_type);
                        break;
		default:
			break;
		}
	}
}

#ifdef GDA_PG_DEBUG
static void
dump_pg_res (PGresult *res)
{
	int nFields = PQnfields(res);
	int i, j;

        //PQntuples() returns the number of rows in the result:
	for (i = 0; i < PQntuples(res); i++) {
		printf (".......");
		for (j = 0; j < nFields; j++)
			printf("%-15s", PQgetvalue(res, i, j));
		printf("\n");
	}
}
#endif

/**
 * gda_postgres_cursor_recordset_new
 */
GdaDataModel *
gda_postgres_cursor_recordset_new (GdaConnection *cnc, const gchar *cursor_name, gint chunk_size)
{
	GdaPostgresCursorRecordset *model;
	GdaPostgresConnectionData *cnc_priv_data;
	gint i;

	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
	g_return_val_if_fail (cursor_name && *cursor_name, NULL);

	cnc_priv_data = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_POSTGRES_HANDLE);

	model = g_object_new (GDA_TYPE_POSTGRES_CURSOR_RECORDSET, 
			      "chunk_size", (chunk_size > 0) ? chunk_size : 1, NULL);
	model->priv->cnc = cnc;
	model->priv->pconn = cnc_priv_data->pconn;
	model->priv->pg_res = NULL;
	model->priv->cursor_name = g_strdup (cursor_name);
	if (fetch_next (model)) {
		PGresult *pg_res = model->priv->pg_res;
		model->priv->ncolumns = PQnfields (pg_res);
		model->priv->columns = NULL;
		model->priv->column_types = gda_postgres_get_column_types (pg_res, cnc_priv_data->type_data, 
									   cnc_priv_data->ntypes);
		/* GdaColumn: create and set attributes */
		for (i = 0; i < model->priv->ncolumns; i++) 
			model->priv->columns = g_slist_prepend (model->priv->columns,
								gda_column_new ());

		gchar *table_name = gda_postgres_guess_table_name (cnc, pg_res);
		for (i = 0; i < model->priv->ncolumns; i++)
			gda_postgres_recordset_describe_column (GDA_DATA_MODEL (model), cnc, pg_res, 
								cnc_priv_data->type_data, cnc_priv_data->ntypes,
								table_name, i);
		g_free (table_name);
	}

	return GDA_DATA_MODEL (model);
}

/*
 * GdaDataModel interface implementation
 */
static gint
gda_postgres_cursor_recordset_get_n_rows (GdaDataModel *model)
{
	GdaPostgresCursorRecordset *imodel;
	g_return_val_if_fail (GDA_IS_POSTGRES_CURSOR_RECORDSET (model), 0);
	imodel = GDA_POSTGRES_CURSOR_RECORDSET (model);
	g_return_val_if_fail (imodel->priv, 0);

	if (imodel->priv->nrows >= 0)
		return imodel->priv->nrows;
	else
		/* number of rows is not known */
		return -1;
}

static gint
gda_postgres_cursor_recordset_get_n_columns (GdaDataModel *model)
{
	GdaPostgresCursorRecordset *imodel;
	g_return_val_if_fail (GDA_IS_POSTGRES_CURSOR_RECORDSET (model), 0);
	imodel = GDA_POSTGRES_CURSOR_RECORDSET (model);
	g_return_val_if_fail (imodel->priv, 0);
	
	return imodel->priv->ncolumns;
}

static GdaColumn *
gda_postgres_cursor_recordset_describe_column (GdaDataModel *model, gint col)
{
	GdaPostgresCursorRecordset *imodel;
	g_return_val_if_fail (GDA_IS_POSTGRES_CURSOR_RECORDSET (model), NULL);
	imodel = GDA_POSTGRES_CURSOR_RECORDSET (model);
	g_return_val_if_fail (imodel->priv, NULL);

	return g_slist_nth_data (imodel->priv->columns, col);
}

static GdaDataModelAccessFlags
gda_postgres_cursor_recordset_get_access_flags (GdaDataModel *model)
{
	g_return_val_if_fail (GDA_IS_POSTGRES_CURSOR_RECORDSET (model), 0);

	return GDA_DATA_MODEL_ACCESS_CURSOR_FORWARD | GDA_DATA_MODEL_ACCESS_CURSOR_BACKWARD;
}

static const GValue *
gda_postgres_cursor_recordset_get_value_at (GdaDataModel *model, gint col, gint row)
{
	g_return_val_if_fail (GDA_IS_POSTGRES_CURSOR_RECORDSET (model), NULL);

	return NULL;
}

static GdaValueAttribute
gda_postgres_cursor_recordset_get_attributes_at (GdaDataModel *model, gint col, gint row)
{
	GdaValueAttribute flags = 0;
	GdaPostgresCursorRecordset *imodel;

	g_return_val_if_fail (GDA_IS_POSTGRES_CURSOR_RECORDSET (model), 0);
	imodel = (GdaPostgresCursorRecordset *) model;
	g_return_val_if_fail (imodel->priv, 0);
	
	flags = GDA_VALUE_ATTR_NO_MODIF;
	
	return flags;
}

static GdaDataModelIter *
gda_postgres_cursor_recordset_create_iter (GdaDataModel *model)
{
	GdaPostgresCursorRecordset *imodel;

	g_return_val_if_fail (GDA_IS_POSTGRES_CURSOR_RECORDSET (model), NULL);
	imodel = (GdaPostgresCursorRecordset *) model;
	g_return_val_if_fail (imodel->priv, NULL);
	
	/* Create the iter if necessary, or just return the existing iter: */
	if (! imodel->priv->iter) {
		imodel->priv->iter = (GdaDataModelIter *) g_object_new (GDA_TYPE_DATA_MODEL_ITER, 
									"data_model", model, NULL);

		imodel->priv->iter_row = -1;
	}

	g_object_ref (imodel->priv->iter);
	return imodel->priv->iter;
}

static void update_iter (GdaPostgresCursorRecordset *imodel);
static gboolean
row_is_in_current_pg_res (GdaPostgresCursorRecordset *model, gint row)
{
	if ((model->priv->pg_res) && (model->priv->pg_res_size > 0) && 
	    (row >= model->priv->pg_res_inf) && (row < model->priv->pg_res_inf + model->priv->pg_res_size))
		return TRUE;
	else
		return FALSE;
}

static gboolean
fetch_next (GdaPostgresCursorRecordset *model)
{
	if (model->priv->pg_res) {
		PQclear (model->priv->pg_res);
		model->priv->pg_res = NULL;
	}

	if (model->priv->pg_pos == G_MAXINT) 
		return FALSE;

	gchar *str;
	gboolean retval = TRUE;
	int status;

	str = g_strdup_printf ("FETCH FORWARD %d FROM %s;",
			       model->priv->chunk_size, model->priv->cursor_name);
#ifdef GDA_PG_DEBUG
	g_print ("QUERY: %s\n", str);
#endif
        model->priv->pg_res = PQexec (model->priv->pconn, str);
        g_free (str);
        status = PQresultStatus (model->priv->pg_res);
	model->priv->chunks_read ++;
        if (status != PGRES_TUPLES_OK) {
                PQclear (model->priv->pg_res);
                model->priv->pg_res = NULL;
		model->priv->pg_res_size = 0;
                retval = FALSE;
        }
	else {
#ifdef GDA_PG_DEBUG
		dump_pg_res (model->priv->pg_res);
#endif

                //PQntuples() returns the number of rows in the result:
                const gint nbtuples = PQntuples (model->priv->pg_res);
		model->priv->pg_res_size = nbtuples;

                if (nbtuples > 0) {
			/* model->priv->pg_res_inf */
			if (model->priv->pg_pos == G_MININT)
				model->priv->pg_res_inf = 0;
			else
				model->priv->pg_res_inf = model->priv->pg_pos + 1;

			/* model->priv->nrows and model->priv->pg_pos */
			if (nbtuples < model->priv->chunk_size) {
				if (model->priv->pg_pos == G_MININT) 
					model->priv->nrows = nbtuples;
				else
					model->priv->nrows = model->priv->pg_pos + nbtuples + 1;

				model->priv->pg_pos = G_MAXINT;				
			}
			else {
				if (model->priv->pg_pos == G_MININT)
					model->priv->pg_pos = nbtuples - 1;
				else
					model->priv->pg_pos += nbtuples;
			}
		}
		else {
			if (model->priv->pg_pos == G_MININT)
				model->priv->nrows = 0;
			else
				model->priv->nrows = model->priv->pg_pos + 1; /* total number of rows */
			model->priv->pg_pos = G_MAXINT;
			retval = FALSE;
		}
	}

#ifdef GDA_PG_DEBUG
	g_print ("--> SIZE = %d (inf = %d) nrows = %d, pg_pos = %d\n", model->priv->pg_res_size, model->priv->pg_res_inf,
		 model->priv->nrows, model->priv->pg_pos);
#endif

	return retval;
}

static gboolean
fetch_prev (GdaPostgresCursorRecordset *model)
{
	if (model->priv->pg_res) {
		PQclear (model->priv->pg_res);
		model->priv->pg_res = NULL;
	}

	if (model->priv->pg_pos == G_MININT) 
		return FALSE;
	else if (model->priv->pg_pos == G_MAXINT) 
		g_assert (model->priv->nrows >= 0); /* total number of rows MUST be known at this point */

	gchar *str;
	gboolean retval = TRUE;
	int status;
	gint noffset;
	
	if (model->priv->pg_pos == G_MAXINT)
		noffset = model->priv->chunk_size + 1;
	else
		noffset = model->priv->pg_res_size + model->priv->chunk_size;
	str = g_strdup_printf ("MOVE BACKWARD %d FROM %s; FETCH FORWARD %d FROM %s;",
			       noffset, model->priv->cursor_name,
			       model->priv->chunk_size, model->priv->cursor_name);
#ifdef GDA_PG_DEBUG
	g_print ("QUERY: %s\n", str);
#endif
        model->priv->pg_res = PQexec (model->priv->pconn, str);
        g_free (str);
        status = PQresultStatus (model->priv->pg_res);
	model->priv->chunks_read ++;
        if (status != PGRES_TUPLES_OK) {
                PQclear (model->priv->pg_res);
                model->priv->pg_res = NULL;
		model->priv->pg_res_size = 0;
                retval = FALSE;
        }
	else {
#ifdef GDA_PG_DEBUG
		dump_pg_res (model->priv->pg_res);
#endif

                //PQntuples() returns the number of rows in the result:
                const gint nbtuples = PQntuples (model->priv->pg_res);
		model->priv->pg_res_size = nbtuples;

                if (nbtuples > 0) {
			/* model->priv->pg_res_inf */
			if (model->priv->pg_pos == G_MAXINT)
				model->priv->pg_res_inf = model->priv->nrows - nbtuples;
			else
				model->priv->pg_res_inf = 
					MAX (model->priv->pg_res_inf - (noffset - model->priv->chunk_size), 0);

			/* model->priv->pg_pos */
			if (nbtuples < model->priv->chunk_size) {
				model->priv->pg_pos = G_MAXINT;
			}
			else {
				if (model->priv->pg_pos == G_MAXINT)
					model->priv->pg_pos = model->priv->nrows - 1;
				else
					model->priv->pg_pos = MAX (model->priv->pg_pos - noffset, -1) + nbtuples;
			}
		}
		else {
			model->priv->pg_pos = G_MAXINT;
			retval = FALSE;
		}
	}

#ifdef GDA_PG_DEBUG
	g_print ("<-- SIZE = %d (inf = %d) nrows = %d, pg_pos = %d\n", model->priv->pg_res_size, model->priv->pg_res_inf,
		 model->priv->nrows, model->priv->pg_pos);
#endif

	return retval;
}

static gboolean
fetch_row_number (GdaPostgresCursorRecordset *model, int row_index)
{
	if (model->priv->pg_res) {
		PQclear (model->priv->pg_res);
		model->priv->pg_res = NULL;
	}

	gchar *str;
	gboolean retval = TRUE;
	int status;

        /* Postgres's FETCH ABSOLUTE seems to use a 1-based index: */
	str = g_strdup_printf ("FETCH ABSOLUTE %d FROM %s;",
			       row_index + 1, model->priv->cursor_name);
#ifdef GDA_PG_DEBUG
	g_print ("QUERY: %s\n", str);
#endif
        model->priv->pg_res = PQexec (model->priv->pconn, str);
        g_free (str);
        status = PQresultStatus (model->priv->pg_res);
	model->priv->chunks_read ++; /* Not really correct, because we are only fetching 1 row, not a whole chunk of rows. */
        if (status != PGRES_TUPLES_OK) {
                PQclear (model->priv->pg_res);
                model->priv->pg_res = NULL;
		model->priv->pg_res_size = 0;
                retval = FALSE;
        }
	else {
#ifdef GDA_PG_DEBUG
		dump_pg_res (model->priv->pg_res);
#endif

                //PQntuples() returns the number of rows in the result:
                const gint nbtuples = PQntuples (model->priv->pg_res);
		model->priv->pg_res_size = nbtuples;

                if (nbtuples > 0) {
                        /* Remember the row number for the start of this chunk:
                         * (actually a chunk of just 1 record in this case.) */ 
			model->priv->pg_res_inf = row_index;
		
			/* don't change model->priv->nrows because we can't know if we have reached the end */
			model->priv->pg_pos = row_index;
		}
		else {
			model->priv->pg_pos = G_MAXINT;
			retval = FALSE;
		}
	}

#ifdef GDA_PG_DEBUG
	g_print ("--> SIZE = %d (inf = %d) nrows = %d, pg_pos = %d\n", model->priv->pg_res_size, model->priv->pg_res_inf,
		 model->priv->nrows, model->priv->pg_pos);
#endif

	return retval;
}


static gboolean
gda_postgres_cursor_recordset_iter_next (GdaDataModel *model, GdaDataModelIter *iter)
{
	GdaPostgresCursorRecordset *imodel;
	gint target_iter_row;

	g_return_val_if_fail (GDA_IS_POSTGRES_CURSOR_RECORDSET (model), FALSE);
	imodel = (GdaPostgresCursorRecordset *) model;
	g_return_val_if_fail (imodel->priv, FALSE);
	g_return_val_if_fail (iter, FALSE);
	g_return_val_if_fail (imodel->priv->iter == iter, FALSE);

	if (imodel->priv->iter_row == G_MAXINT)
		return FALSE;
	else if (imodel->priv->iter_row == G_MININT) 
		target_iter_row = 0;
	else 
		target_iter_row = imodel->priv->iter_row + 1;

	if (row_is_in_current_pg_res (imodel, target_iter_row) ||
	    fetch_next (imodel)) {
		imodel->priv->iter_row = target_iter_row;
		update_iter (imodel);
		return TRUE;
	}
	else {
		g_signal_emit_by_name (iter, "end_of_data");
		g_object_set (G_OBJECT (iter), "current-row", -1, NULL);
		imodel->priv->iter_row = G_MAXINT;
		return FALSE;
	}
}

static gboolean
gda_postgres_cursor_recordset_iter_prev (GdaDataModel *model, GdaDataModelIter *iter)
{
	GdaPostgresCursorRecordset *imodel;
	gint target_iter_row;

	g_return_val_if_fail (GDA_IS_POSTGRES_CURSOR_RECORDSET (model), FALSE);
	imodel = (GdaPostgresCursorRecordset *) model;
	g_return_val_if_fail (imodel->priv, FALSE);

	g_return_val_if_fail (iter, FALSE);
	g_return_val_if_fail (imodel->priv->iter == iter, FALSE);

	if (imodel->priv->iter_row <= 0)
		goto prev_error;
	
	else if (imodel->priv->iter_row == G_MAXINT) {
		g_assert (imodel->priv->nrows >= 0);
		target_iter_row = imodel->priv->nrows - 1;
	}
	else
		target_iter_row = imodel->priv->iter_row - 1;

	if (row_is_in_current_pg_res (imodel, target_iter_row) ||
	    fetch_prev (imodel)) {
		imodel->priv->iter_row = target_iter_row;
		update_iter (imodel);
		return TRUE;
	}
	else 
		goto prev_error;
 prev_error:
	g_object_set (G_OBJECT (iter), "current-row", -1, NULL);
	imodel->priv->iter_row = G_MININT;
	return FALSE;
}

static gboolean
gda_postgres_cursor_recordset_iter_at_row (GdaDataModel *model, GdaDataModelIter *iter, gint row)
{
	GdaPostgresCursorRecordset *imodel;

	g_return_val_if_fail (GDA_IS_POSTGRES_CURSOR_RECORDSET (model), FALSE);
	imodel = (GdaPostgresCursorRecordset *) model;
	g_return_val_if_fail (imodel->priv, FALSE);

	g_return_val_if_fail (iter, FALSE);
	g_return_val_if_fail (imodel->priv->iter == iter, FALSE);
	
	if (row_is_in_current_pg_res (imodel, row) ||
	    fetch_row_number (imodel, row)) {
		imodel->priv->iter_row = row;
		update_iter (imodel);
		return TRUE;
	}
	else 
		goto prev_error;
 prev_error:
	g_object_set (G_OBJECT (iter), "current-row", -1, NULL);
	imodel->priv->iter_row = G_MININT;
	return FALSE;
}


static void
update_iter (GdaPostgresCursorRecordset *imodel)
{
	gchar *thevalue;
        GType ftype;
        gboolean isNull;
        GValue *value;
        gint i, rownum;
	gint length;
	GdaDataModelIter *iter = imodel->priv->iter;
	GSList *plist;
	gboolean update_model;
	
#ifdef GDA_PG_DEBUG
	g_print ("Updating iter at row %d\n", imodel->priv->iter_row);
#endif
	g_object_get (G_OBJECT (iter), "update_model", &update_model, NULL);
	g_object_set (G_OBJECT (iter), "update_model", FALSE, NULL);
	
	rownum = imodel->priv->iter_row - imodel->priv->pg_res_inf;
	for (i = 0, plist = ((GdaParameterList *) iter)->parameters; 
	     i < imodel->priv->ncolumns; 
	     i++, plist = plist->next) {
                thevalue = PQgetvalue (imodel->priv->pg_res, rownum, i);
                length = PQgetlength (imodel->priv->pg_res, rownum, i);
                ftype = imodel->priv->column_types [i];
                isNull = thevalue && *thevalue != '\0' ? FALSE : PQgetisnull (imodel->priv->pg_res, rownum, i);
                value = gda_value_new_null ();
                gda_postgres_set_value (imodel->priv->cnc, value, ftype, thevalue, isNull, length);
		
		gda_parameter_set_value (GDA_PARAMETER (plist->data), value);
		gda_value_free (value);
        }

	g_object_set (G_OBJECT (iter), "current-row", imodel->priv->iter_row, NULL);
	g_object_set (G_OBJECT (iter), "update_model", update_model, NULL);
}
