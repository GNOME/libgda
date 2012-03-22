/*
 * Copyright (C) 2001 - 2005 Rodrigo Moya <rodrigo@gnome-db.org>
 * Copyright (C) 2002 Andrew Hill <andru@src.gnome.org>
 * Copyright (C) 2002 - 2003 Gonzalo Paniagua Javier <gonzalo@ximian.com>
 * Copyright (C) 2002 Holger Thon <holger.thon@gnome-db.org>
 * Copyright (C) 2002 Zbigniew Chyla <cyba@gnome.pl>
 * Copyright (C) 2003 Chris Silles <csilles@src.gnome.org>
 * Copyright (C) 2003 Laurent Sansonetti <lrz@gnome.org>
 * Copyright (C) 2003 Paisa Seeluangsawat <paisa@users.sf.net>
 * Copyright (C) 2004 - 2010 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2005 Alan Knowles <alan@akbkhome.com>
 * Copyright (C) 2005 - 2009 Bas Driessen <bas.driessen@xobas.com>
 * Copyright (C) 2005 Mike Fisk <mfisk@woozle.org>
 * Copyright (C) 2005 Álvaro Peña <alvaropg@telefonica.net>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#include <stdarg.h>
#include <string.h>
#include <glib/gi18n-lib.h>
#include <libgda/gda-util.h>
#include <libgda/gda-connection-private.h>
#include <libgda/providers-support/gda-data-select-priv.h>
#include "gda-mysql.h"
#include "gda-mysql-recordset.h"
#include "gda-mysql-provider.h"
#include "gda-mysql-util.h"
#include <libgda/libgda-global-variables.h>
#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif

#define _GDA_PSTMT(x) ((GdaPStmt*)(x))

enum
{
	PROP_0,
	PROP_CHUNK_SIZE,
	PROP_CHUNKS_READ
};

static void
gda_mysql_recordset_class_init (GdaMysqlRecordsetClass  *klass);
static void
gda_mysql_recordset_init (GdaMysqlRecordset       *recset,
			  GdaMysqlRecordsetClass  *klass);
static void
gda_mysql_recordset_dispose (GObject  *object);

/* virtual methods */
static gint
gda_mysql_recordset_fetch_nb_rows (GdaDataSelect  *model);
static gboolean
gda_mysql_recordset_fetch_random (GdaDataSelect  *model,
				  GdaRow        **row,
				  gint            rownum,
				  GError        **error);
static gboolean
gda_mysql_recordset_fetch_next (GdaDataSelect  *model,
				GdaRow        **row,
				gint            rownum,
				GError        **error);
static gboolean
gda_mysql_recordset_fetch_prev (GdaDataSelect  *model,
				GdaRow        **row,
				gint            rownum,
				GError        **error);
static gboolean
gda_mysql_recordset_fetch_at (GdaDataSelect  *model,
			      GdaRow        **row,
			      gint            rownum,
			      GError        **error);

struct _GdaMysqlRecordsetPrivate {
	GdaConnection  *cnc;
	
	MYSQL_STMT     *mysql_stmt;

	gint            chunk_size;    /* Number of rows to fetch at a time when iterating forward/backward. */
	gint            chunks_read;   /* Number of times that we've iterated forward/backward. */
	GdaRow         *tmp_row;       /* Used in cursor mode to store a reference to the latest #GdaRow. */

	/* if no prepared statement available */
	gint          ncols;
        GType        *types;
};
static GObjectClass *parent_class = NULL;

/*
 * Object init and finalize
 */
static void
gda_mysql_recordset_init (GdaMysqlRecordset       *recset,
			  G_GNUC_UNUSED GdaMysqlRecordsetClass  *klass)
{
	g_return_if_fail (GDA_IS_MYSQL_RECORDSET (recset));
	recset->priv = g_new0 (GdaMysqlRecordsetPrivate, 1);
	recset->priv->cnc = NULL;

	/* initialize specific information */
	recset->priv->chunk_size = 1;
	recset->priv->chunks_read = 0;

	recset->priv->ncols = 0;
	recset->priv->types = NULL;
}


gint
gda_mysql_recordset_get_chunk_size (GdaMysqlRecordset  *recset)
{
	g_return_val_if_fail (GDA_IS_MYSQL_RECORDSET (recset), -1);
	return recset->priv->chunk_size;
}

void
gda_mysql_recordset_set_chunk_size (GdaMysqlRecordset  *recset,
				    gint                chunk_size)
{
	g_return_if_fail (GDA_IS_MYSQL_RECORDSET (recset));

	if (recset->priv->mysql_stmt == NULL)  // Creation is in progress so it's not set.
		return;

#if MYSQL_VERSION_ID >= 50002
	const unsigned long prefetch_rows = chunk_size;
	if (mysql_stmt_attr_set (recset->priv->mysql_stmt, STMT_ATTR_PREFETCH_ROWS,
				 (void *) &prefetch_rows)) {
		g_warning ("%s: %s\n", __func__, mysql_stmt_error (recset->priv->mysql_stmt));
		return;
	}
	recset->priv->chunk_size = chunk_size;
	g_object_notify (G_OBJECT(recset), "chunk-size");
#else
	g_warning (_("Could not use CURSOR. Mysql version 5.0 at least is required. "
		     "Chunk size ignored."));
#endif
}

gint
gda_mysql_recordset_get_chunks_read (GdaMysqlRecordset  *recset)
{
	g_return_val_if_fail (GDA_IS_MYSQL_RECORDSET (recset), -1);
	return recset->priv->chunks_read;
}


static void
gda_mysql_recordset_set_property (GObject       *object,
				  guint          param_id,
				  const GValue  *value,
				  GParamSpec    *pspec)
{
	GdaMysqlRecordset *recordset;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GDA_IS_MYSQL_RECORDSET(object));
	g_return_if_fail (GDA_MYSQL_RECORDSET(object)->priv != NULL);

	recordset = GDA_MYSQL_RECORDSET(object);

	switch (param_id) {
	case PROP_CHUNK_SIZE:
		gda_mysql_recordset_set_chunk_size (recordset,
						    g_value_get_int (value));
		break;
	case PROP_CHUNKS_READ:
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
	}
}

static void
gda_mysql_recordset_get_property (GObject     *object,
				  guint        param_id,
				  GValue      *value,
				  GParamSpec  *pspec)
{
	GdaMysqlRecordset *recordset;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GDA_IS_MYSQL_RECORDSET(object));
	g_return_if_fail (GDA_MYSQL_RECORDSET(object)->priv != NULL);

	recordset = GDA_MYSQL_RECORDSET(object);

	switch (param_id) {
	case PROP_CHUNK_SIZE:
		g_value_set_int (value, recordset->priv->chunk_size);
		break;
	case PROP_CHUNKS_READ:
		g_value_set_int (value, recordset->priv->chunks_read);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
	}
}


static void
gda_mysql_recordset_class_init (GdaMysqlRecordsetClass  *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GdaDataSelectClass *pmodel_class = GDA_DATA_SELECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->dispose = gda_mysql_recordset_dispose;
	pmodel_class->fetch_nb_rows = gda_mysql_recordset_fetch_nb_rows;
	pmodel_class->fetch_random = gda_mysql_recordset_fetch_random;

	pmodel_class->fetch_next = gda_mysql_recordset_fetch_next;
	pmodel_class->fetch_prev = gda_mysql_recordset_fetch_prev;
	pmodel_class->fetch_at = gda_mysql_recordset_fetch_at;
	
	/* Properties. */
	object_class->set_property = gda_mysql_recordset_set_property;
	object_class->get_property = gda_mysql_recordset_get_property;

	g_object_class_install_property
		(object_class,
		 PROP_CHUNK_SIZE,
		 g_param_spec_int ("chunk-size", _("Number of rows fetched at a time"),
				   NULL,
				   1, G_MAXINT - 1, 1,
				   (G_PARAM_CONSTRUCT | G_PARAM_WRITABLE | G_PARAM_READABLE)));

	g_object_class_install_property
		(object_class,
		 PROP_CHUNKS_READ,
		 g_param_spec_int ("chunks-read", _("Number of row chunks read since the object creation"),
				   NULL,
				   0, G_MAXINT - 1, 0,
				   (G_PARAM_CONSTRUCT | G_PARAM_WRITABLE | G_PARAM_READABLE)));
	
}

static void
gda_mysql_recordset_dispose (GObject  *object)
{
	GdaMysqlRecordset *recset = (GdaMysqlRecordset *) object;

	g_return_if_fail (GDA_IS_MYSQL_RECORDSET (recset));

	if (recset->priv) {
		GDA_MYSQL_PSTMT (GDA_DATA_SELECT (object)->prep_stmt)->stmt_used = FALSE;

		if (recset->priv->cnc) {
			g_object_unref (G_OBJECT(recset->priv->cnc));
			recset->priv->cnc = NULL;
		}
		if (recset->priv->tmp_row) {
			g_object_unref (G_OBJECT(recset->priv->tmp_row));
			recset->priv->tmp_row = NULL;
		}
		if (recset->priv->types)
			g_free (recset->priv->types);

		g_free (recset->priv);
		recset->priv = NULL;
	}

	parent_class->dispose (object);
}

/*
 * Public functions
 */

GType
gda_mysql_recordset_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static GStaticMutex registering = G_STATIC_MUTEX_INIT;
		static const GTypeInfo info = {
			sizeof (GdaMysqlRecordsetClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_mysql_recordset_class_init,
			NULL,
			NULL,
			sizeof (GdaMysqlRecordset),
			0,
			(GInstanceInitFunc) gda_mysql_recordset_init,
			NULL
		};
		g_static_mutex_lock (&registering);
		if (type == 0)
			type = g_type_register_static (GDA_TYPE_DATA_SELECT, "GdaMysqlRecordset", &info, 0);
		g_static_mutex_unlock (&registering);
	}

	return type;
}

static GType
_gda_mysql_type_to_gda (G_GNUC_UNUSED MysqlConnectionData *cdata,
			enum enum_field_types  mysql_type, unsigned int charsetnr)
{
	GType gtype = 0;
	switch (mysql_type) {
	case MYSQL_TYPE_TINY:
	case MYSQL_TYPE_SHORT:
	case MYSQL_TYPE_LONG:
	case MYSQL_TYPE_INT24:
	case MYSQL_TYPE_YEAR:
		gtype = G_TYPE_INT;
		break;
	case MYSQL_TYPE_LONGLONG:
		gtype = G_TYPE_LONG;
		break;
	case MYSQL_TYPE_FLOAT:
		gtype = G_TYPE_FLOAT;
		break;
	case MYSQL_TYPE_DECIMAL:
	case MYSQL_TYPE_NEWDECIMAL:
		gtype = GDA_TYPE_NUMERIC;
		break;
	case MYSQL_TYPE_DOUBLE:
		gtype = G_TYPE_DOUBLE;
		break;
	case MYSQL_TYPE_TIMESTAMP:
	case MYSQL_TYPE_DATETIME:
		gtype = GDA_TYPE_TIMESTAMP;
		break;
	case MYSQL_TYPE_DATE:
		gtype = G_TYPE_DATE;
		break;
	case MYSQL_TYPE_TIME:
		gtype = GDA_TYPE_TIME;
		break;
	case MYSQL_TYPE_NULL:
		gtype = GDA_TYPE_NULL;
		break;
	case MYSQL_TYPE_STRING:
	case MYSQL_TYPE_VAR_STRING:
	case MYSQL_TYPE_SET:
	case MYSQL_TYPE_ENUM:
	case MYSQL_TYPE_GEOMETRY:
	case MYSQL_TYPE_BIT:
	case MYSQL_TYPE_BLOB:
	default:
		if (charsetnr == 63)
			gtype = GDA_TYPE_BLOB;
		else
			gtype = G_TYPE_STRING;
		break;
	}

	/* g_print ("%s: ", __func__); */
	/* switch (mysql_type) { */
	/* case MYSQL_TYPE_TINY:  g_print ("MYSQL_TYPE_TINY");  break; */
	/* case MYSQL_TYPE_SHORT:  g_print ("MYSQL_TYPE_SHORT");  break; */
	/* case MYSQL_TYPE_LONG:  g_print ("MYSQL_TYPE_LONG");  break; */
	/* case MYSQL_TYPE_INT24:  g_print ("MYSQL_TYPE_INT24");  break; */
	/* case MYSQL_TYPE_YEAR:  g_print ("MYSQL_TYPE_YEAR");  break; */
	/* case MYSQL_TYPE_LONGLONG:  g_print ("MYSQL_TYPE_LONGLONG");  break; */
	/* case MYSQL_TYPE_FLOAT:  g_print ("MYSQL_TYPE_FLOAT");  break; */
	/* case MYSQL_TYPE_DECIMAL:  g_print ("MYSQL_TYPE_DECIMAL");  break; */
	/* case MYSQL_TYPE_NEWDECIMAL:  g_print ("MYSQL_TYPE_NEWDECIMAL");  break; */
	/* case MYSQL_TYPE_DOUBLE:  g_print ("MYSQL_TYPE_DOUBLE");  break; */
	/* case MYSQL_TYPE_BIT:  g_print ("MYSQL_TYPE_BIT");  break; */
	/* case MYSQL_TYPE_BLOB:  g_print ("MYSQL_TYPE_BLOB");  break; */
	/* case MYSQL_TYPE_TIMESTAMP:  g_print ("MYSQL_TYPE_TIMESTAMP");  break; */
	/* case MYSQL_TYPE_DATETIME:  g_print ("MYSQL_TYPE_DATETIME");  break; */
	/* case MYSQL_TYPE_DATE:  g_print ("MYSQL_TYPE_DATE");  break; */
	/* case MYSQL_TYPE_TIME:  g_print ("MYSQL_TYPE_TIME");  break; */
	/* case MYSQL_TYPE_NULL:  g_print ("MYSQL_TYPE_NULL");  break; */
	/* case MYSQL_TYPE_STRING:  g_print ("MYSQL_TYPE_STRING");  break; */
	/* case MYSQL_TYPE_VAR_STRING:  g_print ("MYSQL_TYPE_VAR_STRING");  break; */
	/* case MYSQL_TYPE_SET:  g_print ("MYSQL_TYPE_SET");  break; */
	/* case MYSQL_TYPE_ENUM:  g_print ("MYSQL_TYPE_ENUM");  break; */
	/* case MYSQL_TYPE_GEOMETRY:  g_print ("MYSQL_TYPE_GEOMETRY");  break; */
	/* default:  g_print ("UNKNOWN %d: MYSQL_TYPE_STRING", mysql_type);  break; */
	/* } */
	/* g_print ("\n"); */

	return gtype;
}

GdaDataModel *
gda_mysql_recordset_new_direct (GdaConnection *cnc, GdaDataModelAccessFlags flags, 
				GType *col_types)
{
	GdaMysqlRecordset *model;
        MysqlConnectionData *cdata;
        gint i;
	GdaDataModelAccessFlags rflags;
	GSList *columns = NULL;

        g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	cdata = (MysqlConnectionData*) gda_connection_internal_get_provider_data (cnc);
	if (!cdata)
		return NULL;

	/* determine access mode: RANDOM or CURSOR FORWARD are the only supported */
	if (flags & GDA_DATA_MODEL_ACCESS_RANDOM)
		rflags = GDA_DATA_MODEL_ACCESS_RANDOM;
	else
		rflags = GDA_DATA_MODEL_ACCESS_CURSOR_FORWARD;

	/* create data model */
        model = g_object_new (GDA_TYPE_MYSQL_RECORDSET,
			      "connection", cnc,
			      "model-usage", rflags,
			      NULL);
        model->priv->cnc = cnc;
	g_object_ref (G_OBJECT(cnc));

	/* columns & types */	
	model->priv->ncols = mysql_field_count (cdata->mysql);
	model->priv->types = g_new0 (GType, model->priv->ncols);
	
	/* create columns */
	for (i = 0; i < model->priv->ncols; i++)
		columns = g_slist_prepend (columns, gda_column_new ());
	columns = g_slist_reverse (columns);

	if (col_types) {
		for (i = 0; ; i++) {
			if (col_types [i] > 0) {
				if (col_types [i] == G_TYPE_NONE)
					break;
				if (i >= model->priv->ncols) {
					g_warning (_("Column %d out of range (0-%d), ignoring its specified type"), i,
						   model->priv->ncols - 1);
					break;
				}
				else
					model->priv->types [i] = col_types [i];
			}
		}
	}

	/* fill bind result */
	MYSQL_RES *mysql_res = mysql_store_result (cdata->mysql);
	MYSQL_FIELD *mysql_fields = mysql_fetch_fields (mysql_res);
	GSList *list;

	((GdaDataSelect *) model)->advertized_nrows = mysql_affected_rows (cdata->mysql);
	for (i=0, list = columns; 
	     i < model->priv->ncols; 
	     i++, list = list->next) {
		GdaColumn *column = GDA_COLUMN (list->data);
		
		/* use C API to set columns' information using gda_column_set_*() */
		MYSQL_FIELD *field = &mysql_fields[i];
		
		GType gtype = model->priv->types [i];
		if (gtype == 0) {
			gtype = _gda_mysql_type_to_gda (cdata, field->type, field->charsetnr);
			model->priv->types [i] = gtype;
		}
		gda_column_set_g_type (column, gtype);
		gda_column_set_name (column, field->name);
		gda_column_set_description (column, field->name);
	}
	gda_data_select_set_columns (GDA_DATA_SELECT (model), columns);

	/* load ALL data */
	MYSQL_ROW mysql_row;
	gint rownum;
	GdaServerProvider *prov;
	prov = gda_connection_get_provider (cnc);
	for (mysql_row = mysql_fetch_row (mysql_res), rownum = 0;
	     mysql_row;
	     mysql_row = mysql_fetch_row (mysql_res), rownum++) {
		GdaRow *row = gda_row_new (model->priv->ncols);
		gint col;
		for (col = 0; col < model->priv->ncols; col++) {
			gint i = col;
		
			GValue *value = gda_row_get_value (row, i);
			GType type = model->priv->types[i];
			char *data = mysql_row[i];

			if (!data || (type == GDA_TYPE_NULL))
				continue;

			gda_value_reset_with_type (value, type);
			if (type == G_TYPE_STRING)
				g_value_set_string (value, data);
			else {
				GdaDataHandler *dh;
				gboolean valueset = FALSE;
				dh = gda_server_provider_get_data_handler_g_type (prov, cnc, type);
				if (dh) {
					GValue *tmpvalue;
					tmpvalue = gda_data_handler_get_value_from_str (dh, data, type);
					if (tmpvalue) {
						*value = *tmpvalue;
						g_free (tmpvalue);
						valueset = TRUE;
					}
				}
				if (!valueset)
					gda_row_invalidate_value (row, value);
			}
		}
		gda_data_select_take_row ((GdaDataSelect*) model, row, rownum);
	}
	mysql_free_result (mysql_res);

        return GDA_DATA_MODEL (model);
}


/*
 * the @ps struct is modified and transferred to the new data model created in
 * this function
 */
GdaDataModel *
gda_mysql_recordset_new (GdaConnection            *cnc,
			 GdaMysqlPStmt            *ps,
			 GdaSet                   *exec_params,
			 GdaDataModelAccessFlags   flags,
			 GType                    *col_types)
{
	GdaMysqlRecordset *model;
        MysqlConnectionData *cdata;
        gint i;
	GdaDataModelAccessFlags rflags;

        g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
        g_return_val_if_fail (ps != NULL, NULL);

	cdata = (MysqlConnectionData*) gda_connection_internal_get_provider_data (cnc);
	if (!cdata)
		return NULL;

	g_assert (ps->mysql_stmt);

	/* make sure @ps reports the correct number of columns using the API*/
        if (_GDA_PSTMT (ps)->ncols < 0)
		_GDA_PSTMT(ps)->ncols = mysql_stmt_field_count (ps->mysql_stmt);

        /* completing @ps if not yet done */
	g_assert (! ps->stmt_used);
        ps->stmt_used = TRUE;
        if (!_GDA_PSTMT (ps)->types && (_GDA_PSTMT (ps)->ncols > 0)) {
		/* create prepared statement's columns */
		for (i = 0; i < _GDA_PSTMT (ps)->ncols; i++)
			_GDA_PSTMT (ps)->tmpl_columns = g_slist_prepend (_GDA_PSTMT (ps)->tmpl_columns, 
									 gda_column_new ());
		_GDA_PSTMT (ps)->tmpl_columns = g_slist_reverse (_GDA_PSTMT (ps)->tmpl_columns);

		/* create prepared statement's types */
		_GDA_PSTMT (ps)->types = g_new0 (GType, _GDA_PSTMT (ps)->ncols); /* all types are initialized to GDA_TYPE_NULL */
		if (col_types) {
			for (i = 0; ; i++) {
				if (col_types [i] > 0) {
					if (col_types [i] == G_TYPE_NONE)
						break;
					if (i >= _GDA_PSTMT (ps)->ncols) {
						g_warning (_("Column %d out of range (0-%d), ignoring its specified type"), i,
							   _GDA_PSTMT (ps)->ncols - 1);
						break;
					}
					else
						_GDA_PSTMT (ps)->types [i] = col_types [i];
				}
			}
		}
	}

	/* get rid of old bound result if any */
	if (ps->mysql_bind_result) {
		gint i;
		for (i = 0; i < ((GdaPStmt *) ps)->ncols; ++i) {
			g_free (ps->mysql_bind_result[i].buffer);
			g_free (ps->mysql_bind_result[i].is_null);
			g_free (ps->mysql_bind_result[i].length);
		}
		g_free (ps->mysql_bind_result);
		ps->mysql_bind_result = NULL;
	}

	/* fill bind result */
	MYSQL_RES *mysql_res = mysql_stmt_result_metadata (ps->mysql_stmt);
	MYSQL_FIELD *mysql_fields = mysql_fetch_fields (mysql_res);
	
	MYSQL_BIND *mysql_bind_result = g_new0 (MYSQL_BIND, GDA_PSTMT (ps)->ncols);
	GSList *list;

	for (i=0, list = _GDA_PSTMT (ps)->tmpl_columns; 
	     i < GDA_PSTMT (ps)->ncols; 
	     i++, list = list->next) {
		GdaColumn *column = GDA_COLUMN (list->data);
		
		/* use C API to set columns' information using gda_column_set_*() */
		MYSQL_FIELD *field = &mysql_fields[i];
		
		GType gtype = _GDA_PSTMT(ps)->types[i];
		if (gtype == 0) {
			gtype = _gda_mysql_type_to_gda (cdata, field->type, field->charsetnr);
			_GDA_PSTMT(ps)->types[i] = gtype;
		}
		gda_column_set_g_type (column, gtype);
		gda_column_set_name (column, field->name);
		gda_column_set_description (column, field->name);
		
		/* binding results with types */
		mysql_bind_result[i].buffer_type = field->type;
		mysql_bind_result[i].is_unsigned = field->flags & UNSIGNED_FLAG ? TRUE : FALSE;
		mysql_bind_result[i].is_null = g_malloc0 (sizeof(my_bool));
		
		switch (mysql_bind_result[i].buffer_type) {
		case MYSQL_TYPE_TINY:
		case MYSQL_TYPE_SHORT:
		case MYSQL_TYPE_INT24:
		case MYSQL_TYPE_LONG:
		case MYSQL_TYPE_YEAR:
			mysql_bind_result[i].buffer = g_malloc0 (sizeof(int));
			break;
		case MYSQL_TYPE_LONGLONG:
			mysql_bind_result[i].buffer = g_malloc0 (sizeof(long long));
			break;
		case MYSQL_TYPE_NULL:
			break;
		case MYSQL_TYPE_TIME:
		case MYSQL_TYPE_DATE:
		case MYSQL_TYPE_DATETIME:
		case MYSQL_TYPE_TIMESTAMP:
			mysql_bind_result[i].buffer = g_malloc0 (sizeof(MYSQL_TIME));
			break;
		case MYSQL_TYPE_FLOAT:
		case MYSQL_TYPE_DOUBLE:
			mysql_bind_result[i].buffer = g_malloc0 (sizeof(double));
			break;
		case MYSQL_TYPE_STRING:
		case MYSQL_TYPE_VAR_STRING:
		case MYSQL_TYPE_BLOB:
		case MYSQL_TYPE_TINY_BLOB:
		case MYSQL_TYPE_MEDIUM_BLOB:
		case MYSQL_TYPE_LONG_BLOB:
		case MYSQL_TYPE_DECIMAL:
		case MYSQL_TYPE_NEWDECIMAL:
		case MYSQL_TYPE_BIT:
			mysql_bind_result[i].buffer = g_malloc0 (field->max_length + 1);
			mysql_bind_result[i].buffer_length = field->max_length + 1;
			mysql_bind_result[i].length = g_malloc0 (sizeof(unsigned long));
			break;
		default:
			g_warning (_("Invalid column bind data type. %d\n"),
				   mysql_bind_result[i].buffer_type);
		}
		/*g_print ("%s(): NAME=%s, TYPE=%d, GTYPE=%s\n", 
		  __FUNCTION__, field->name, field->type, g_type_name (gtype));*/
	}
	
	if (mysql_stmt_bind_result (ps->mysql_stmt, mysql_bind_result)) {
		g_warning ("mysql_stmt_bind_result failed: %s\n",
			   mysql_stmt_error (ps->mysql_stmt));
	}
	
	mysql_free_result (mysql_res);
	ps->mysql_bind_result = mysql_bind_result;

	/* determine access mode: RANDOM or CURSOR FORWARD are the only supported */
	if (flags & GDA_DATA_MODEL_ACCESS_RANDOM)
		rflags = GDA_DATA_MODEL_ACCESS_RANDOM;
	else
		rflags = GDA_DATA_MODEL_ACCESS_CURSOR_FORWARD;

	/* create data model */
        model = g_object_new (GDA_TYPE_MYSQL_RECORDSET,
			      "connection", cnc,
			      "prepared-stmt", ps,
			      "model-usage", rflags,
			      "exec-params", exec_params, 
			      NULL);
        model->priv->cnc = cnc;
	g_object_ref (G_OBJECT(cnc));

	model->priv->mysql_stmt = ps->mysql_stmt;

	((GdaDataSelect *) model)->advertized_nrows = mysql_stmt_affected_rows (ps->mysql_stmt);

        return GDA_DATA_MODEL (model);
}


/*
 * Get the number of rows in @model, if possible
 */
static gint
gda_mysql_recordset_fetch_nb_rows (GdaDataSelect *model)
{
	GdaMysqlRecordset *imodel;

	imodel = GDA_MYSQL_RECORDSET (model);
	if (model->advertized_nrows >= 0)
		return model->advertized_nrows;

	model->advertized_nrows = mysql_stmt_affected_rows (imodel->priv->mysql_stmt);
	
	return model->advertized_nrows;
}

static GdaRow *
new_row_from_mysql_stmt (GdaMysqlRecordset *imodel, G_GNUC_UNUSED gint rownum, GError **error)
{
	//g_print ("%s(): NCOLS=%d  ROWNUM=%d\n", __func__, ((GdaDataSelect *) imodel)->prep_stmt->ncols, rownum);
	int res;
	MYSQL_BIND *mysql_bind_result;
	g_return_val_if_fail (imodel->priv->mysql_stmt != NULL, NULL);

	mysql_bind_result = ((GdaMysqlPStmt *) ((GdaDataSelect *) imodel)->prep_stmt)->mysql_bind_result;
	g_assert (mysql_bind_result);

	res = mysql_stmt_fetch (imodel->priv->mysql_stmt);
	if (res == MYSQL_NO_DATA) {
		/* should not happen */
		g_set_error (error, GDA_DATA_MODEL_ERROR, GDA_DATA_MODEL_ACCESS_ERROR,
			     "No more data, please report this bug to "
			     "http://bugzilla.gnome.org/ for the \"libgda\" product and the MySQL provider.");
	}
	else if (res == MYSQL_DATA_TRUNCATED) {
		GString *string;

		string = g_string_new ("Truncated data, please report this bug to "
				       "http://bugzilla.gnome.org/ for the \"libgda\" product and the MySQL provider.");

		gint col;
		for (col = 0; col < ((GdaDataSelect *) imodel)->prep_stmt->ncols; ++col) {
			my_bool truncated;
			mysql_bind_result[col].error = &truncated;
			mysql_stmt_fetch_column (imodel->priv->mysql_stmt, &(mysql_bind_result[col]),
						 (unsigned int)col, 0);
			if (truncated)
				g_string_append_printf (string, "\n  column %d is truncated\n", col);
			mysql_bind_result[col].error = NULL;
		}
		g_set_error (error, GDA_DATA_MODEL_ERROR, GDA_DATA_MODEL_ACCESS_ERROR, "%s", string->str);
		g_string_free (string, TRUE);

		return NULL;
	}
	else if (res) {
		_gda_mysql_make_error (imodel->priv->cnc, NULL, imodel->priv->mysql_stmt, error);
		return NULL;
	}

	/* g_print ("%s: SQL=%s\n", __func__, ((GdaDataSelect *) imodel)->prep_stmt->sql); */

	
	GdaRow *row = gda_row_new (((GdaDataSelect *) imodel)->prep_stmt->ncols);
	gint col;
	for (col = 0; col < ((GdaDataSelect *) imodel)->prep_stmt->ncols; ++col) {
		gint i = col;
		
		GValue *value = gda_row_get_value (row, i);
		GType type = ((GdaDataSelect *) imodel)->prep_stmt->types[i];
		
		/* g_print ("%s: #%d : TYPE=%d, GTYPE=%s\n", __func__, i, mysql_bind_result[i].buffer_type, g_type_name (type)); */
		
		
		int intvalue = 0;
		long long longlongvalue = 0;
		double doublevalue = 0.0;
		float floatvalue = 0.;
		MYSQL_TIME timevalue = { 0, 0, 0, 0, 0, 0, 0, 0, 0 };
		my_bool is_null = FALSE;
		unsigned long length;
		
		g_memmove (&is_null, mysql_bind_result[i].is_null, sizeof(my_bool));
		if (is_null) {
			gda_value_set_null (value);
			continue;
		}
		else
			gda_value_reset_with_type (value, type);

		switch (mysql_bind_result[i].buffer_type) {
		case MYSQL_TYPE_TINY:
		case MYSQL_TYPE_SHORT:
		case MYSQL_TYPE_INT24:
		case MYSQL_TYPE_LONG:
		case MYSQL_TYPE_YEAR:
			g_memmove (&intvalue, mysql_bind_result[i].buffer, sizeof(int));
			
			if (type == G_TYPE_INT)
				g_value_set_int (value, intvalue);
			else if (type == G_TYPE_LONG)
				g_value_set_long (value, (long) intvalue);
			else if (type == G_TYPE_BOOLEAN)
				g_value_set_boolean (value, intvalue ? TRUE : FALSE);
			else {
				gda_row_invalidate_value (row, value);
				g_set_error (error, GDA_SERVER_PROVIDER_ERROR,
					     GDA_SERVER_PROVIDER_DATA_ERROR,
					     _("Type %s not mapped for value %d"),
					     g_type_name (type), intvalue);
			}
			break;
		case MYSQL_TYPE_LONGLONG:
			g_memmove (&longlongvalue, mysql_bind_result[i].buffer, sizeof(long long));

			if (type == G_TYPE_BOOLEAN)
				g_value_set_boolean (value, longlongvalue ? TRUE : FALSE);
			else if (type == G_TYPE_INT)
				g_value_set_int (value, longlongvalue);
			else if (type == G_TYPE_LONG)
				g_value_set_long (value, longlongvalue);
			else {
				gda_row_invalidate_value (row, value);
				g_set_error (error, GDA_SERVER_PROVIDER_ERROR,
					     GDA_SERVER_PROVIDER_DATA_ERROR,
					     _("Type %s not mapped for value %lld"),
					     g_type_name (type), longlongvalue);
			}
			break;
		case MYSQL_TYPE_NULL:
			if (G_IS_VALUE(value))
				g_value_unset (value);
			break;
		case MYSQL_TYPE_TIME:
		case MYSQL_TYPE_DATE:
		case MYSQL_TYPE_DATETIME:
		case MYSQL_TYPE_TIMESTAMP:
			g_memmove (&timevalue, mysql_bind_result[i].buffer, sizeof(MYSQL_TIME));

			if (type == GDA_TYPE_TIME) {
				GdaTime time = {
					.hour = timevalue.hour,
					.minute = timevalue.minute,
					.second = timevalue.second,
					.fraction = timevalue.second_part,
				};
				gda_value_set_time (value, &time);
			}
			else if (type == G_TYPE_DATE) {
				GDate *date = g_date_new_dmy
					((timevalue.day != 0) ? timevalue.day : 1,
					 (timevalue.month != 0) ? timevalue.month : 1,
					 (timevalue.year != 0) ? timevalue.year : 1970);
				g_value_take_boxed (value, date);
			}
			else if (type == GDA_TYPE_TIMESTAMP) {
				GdaTimestamp timestamp = {
					.year = timevalue.year,
					.month = timevalue.month,
					.day = timevalue.day,
					.hour = timevalue.hour,
					.minute = timevalue.minute,
					.second = timevalue.second,
					.fraction = timevalue.second_part,
				};
				gda_value_set_timestamp (value, &timestamp);
			}
			else {
				gda_row_invalidate_value (row, value);
				g_set_error (error, GDA_SERVER_PROVIDER_ERROR,
					     GDA_SERVER_PROVIDER_DATA_ERROR,
					     _("Type %s not mapped for value %d/%d/%d %d:%d:%d.%lu"),
					     g_type_name (type), timevalue.year, timevalue.month, 
					     timevalue.day, timevalue.hour, timevalue.minute, 
					     timevalue.second, timevalue.second_part);
			}

			break;
		case MYSQL_TYPE_FLOAT: {
			g_memmove (&floatvalue, mysql_bind_result[i].buffer, sizeof(float));
			
			if (type == G_TYPE_FLOAT)
				g_value_set_float (value, (float) floatvalue);
			else {
				gda_row_invalidate_value (row, value);
				g_set_error (error, GDA_SERVER_PROVIDER_ERROR,
					     GDA_SERVER_PROVIDER_DATA_ERROR,
					     _("Type %s not mapped for value %f"),
					     g_type_name (type), floatvalue);
			}			
			break;
		}
		case MYSQL_TYPE_DOUBLE: {
			g_memmove (&doublevalue, mysql_bind_result[i].buffer, sizeof(double));
			
			if (type == G_TYPE_DOUBLE)
				g_value_set_double (value, doublevalue);
			else {
				gda_row_invalidate_value (row, value);
				g_set_error (error, GDA_SERVER_PROVIDER_ERROR,
					     GDA_SERVER_PROVIDER_DATA_ERROR,
					     _("Type %s not mapped for value %f"),
					     g_type_name (type), doublevalue);
			}
			break;
		}
		case MYSQL_TYPE_STRING:
		case MYSQL_TYPE_VAR_STRING:
		case MYSQL_TYPE_BLOB:
		case MYSQL_TYPE_TINY_BLOB:
		case MYSQL_TYPE_MEDIUM_BLOB:
		case MYSQL_TYPE_LONG_BLOB:
		case MYSQL_TYPE_NEWDECIMAL:
		case MYSQL_TYPE_DECIMAL:
		case MYSQL_TYPE_BIT: {
			char *strvalue = NULL;
			g_memmove (&length, mysql_bind_result[i].length, sizeof(unsigned long));
			if (length > 0) {
				strvalue = g_malloc (length + 1);
				memcpy (strvalue, mysql_bind_result[i].buffer, length);
				strvalue [length] = 0;
			}
			
			if (type == G_TYPE_STRING)
				g_value_set_string (value, strvalue);
			else if (type == GDA_TYPE_BINARY) {
				GdaBinary binary = {
					.data = (guchar*) strvalue,
					.binary_length = length
				};
				gda_value_set_binary (value, &binary);
			}
			else if (type == GDA_TYPE_BLOB) {
				/* we don't use GdaMysqlBlobOp because it looks like the MySQL
				 * API does not support BLOBs accessed in a random way,
				 * so we return the whole BLOB at once */
				GdaBlob blob = { {(guchar*) strvalue, length}, NULL };
				gda_value_set_blob (value, &blob);
			}
			else if (type == GDA_TYPE_NUMERIC) {
				setlocale (LC_NUMERIC, "C");
				if (length > 0) {
					GdaNumeric *numeric;
					numeric = g_new0 (GdaNumeric, 1);
					numeric->number = g_strdup (strvalue);
					numeric->width = length;
					gda_value_set_numeric (value, numeric);
					gda_numeric_free (numeric);
				}
				setlocale (LC_NUMERIC, gda_numeric_locale);
			}
			else if (type == G_TYPE_DOUBLE) {
				if (length > 0) {
					setlocale (LC_NUMERIC, "C");
					g_value_set_double (value, atof (strvalue));
					setlocale (LC_NUMERIC, gda_numeric_locale);
				}
				else {
					/* error: wrong column type */
					gda_row_invalidate_value (row, value);
					g_set_error (error, GDA_SERVER_PROVIDER_ERROR,
						     GDA_SERVER_PROVIDER_DATA_ERROR,
						     _("Invalid column bind data type. %d\n"),
						     mysql_bind_result[i].buffer_type);
					break;
				}
			}
			else if (type == G_TYPE_INT) {
				if (length > 0)
					g_value_set_int (value, atoi (strvalue));
				else {
					/* error: wrong column type */
					gda_row_invalidate_value (row, value);
					g_set_error (error, GDA_SERVER_PROVIDER_ERROR,
						     GDA_SERVER_PROVIDER_DATA_ERROR,
						     _("Invalid column bind data type. %d\n"),
						     mysql_bind_result[i].buffer_type);
					break;
				}
			}
			else if (type == G_TYPE_BOOLEAN) {
				if (length > 0)
					g_value_set_boolean (value, atoi (strvalue));
				else {
					/* error: wrong column type */
					gda_row_invalidate_value (row, value);
					g_set_error (error, GDA_SERVER_PROVIDER_ERROR,
						     GDA_SERVER_PROVIDER_DATA_ERROR,
						     _("Invalid column bind data type. %d\n"),
						     mysql_bind_result[i].buffer_type);
					break;
				}
			}
			else {
				gda_row_invalidate_value (row, value);
				g_set_error (error, GDA_SERVER_PROVIDER_ERROR,
					     GDA_SERVER_PROVIDER_DATA_ERROR,
					     _("Type %s not mapped for value %s"),
					     g_type_name (type), strvalue);
			}
			g_free (strvalue);
			break;
		}
		default:
			gda_row_invalidate_value (row, value);
			g_set_error (error, GDA_SERVER_PROVIDER_ERROR,
				     GDA_SERVER_PROVIDER_DATA_ERROR,
				     _("Invalid column bind data type. %d\n"),
				     mysql_bind_result[i].buffer_type);
		}

	}
	return row;
}


/*
 * Create a new filled #GdaRow object for the row at position @rownum, and put it into *row.
 *
 * Each new GdaRow is given to @model using gda_data_select_take_row().
 */
static gboolean 
gda_mysql_recordset_fetch_random (GdaDataSelect  *model,
				  GdaRow        **row,
				  gint            rownum,
				  GError        **error)
{
	GdaMysqlRecordset *imodel;

	imodel = GDA_MYSQL_RECORDSET (model);

	*row = new_row_from_mysql_stmt (imodel, rownum, error);
	if (!*row)
		return TRUE;

	gda_data_select_take_row (model, *row, rownum);
	
	if (model->nb_stored_rows == model->advertized_nrows) {
		/* All the row have been converted.  We could free result now */
		/* but it was done before provided no field-based API functions */
		/* that process result set meta data was needed in the middle. */
	}

	return TRUE;
}

/*
 * Create a new filled #GdaRow object for the next cursor row, and put it into *row.
 *
 * Each new #GdaRow created is referenced only by imodel->priv->tmp_row (the #GdaDataSelect implementation
 * never keeps a reference to it).
 * Before a new #GdaRow gets created, the previous one, if set, is discarded.
 */
static gboolean 
gda_mysql_recordset_fetch_next (GdaDataSelect  *model,
				GdaRow        **row,
				gint            rownum,
				GError        **error)
{
	GdaMysqlRecordset *imodel = (GdaMysqlRecordset*) model;

	if (imodel->priv->tmp_row)
		g_object_unref (G_OBJECT(imodel->priv->tmp_row));
	*row = new_row_from_mysql_stmt (imodel, rownum, error);
	imodel->priv->tmp_row = *row;

	return TRUE;
}

/*
 * Create a new filled #GdaRow object for the previous cursor row, and put it into *prow.
 *
 * Each new #GdaRow created is referenced only by imodel->priv->tmp_row (the #GdaDataSelect implementation
 * never keeps a reference to it).
 * Before a new #GdaRow gets created, the previous one, if set, is discarded.
 */
static gboolean 
gda_mysql_recordset_fetch_prev (GdaDataSelect  *model,
				GdaRow        **row,
				gint            rownum,
				GError        **error)
{
	GdaMysqlRecordset *imodel = (GdaMysqlRecordset*) model;

	if (imodel->priv->tmp_row)
		g_object_unref (G_OBJECT(imodel->priv->tmp_row));
	*row = new_row_from_mysql_stmt (imodel, rownum, error);
	imodel->priv->tmp_row = *row;

	return TRUE;
}

/*
 * Create a new filled #GdaRow object for the cursor row at position @rownum, and put it into *row.
 *
 * Each new #GdaRow created is referenced only by imodel->priv->tmp_row (the #GdaDataSelect implementation
 * never keeps a reference to it).
 * Before a new #GdaRow gets created, the previous one, if set, is discarded.
 */
static gboolean 
gda_mysql_recordset_fetch_at (GdaDataSelect  *model,
			      GdaRow        **row,
			      gint            rownum,
			      GError        **error)
{
	GdaMysqlRecordset *imodel = (GdaMysqlRecordset*) model;

	if (imodel->priv->tmp_row)
		g_object_unref (G_OBJECT(imodel->priv->tmp_row));
	*row = new_row_from_mysql_stmt (imodel, rownum, error);
	imodel->priv->tmp_row = *row;

	return TRUE;
}

