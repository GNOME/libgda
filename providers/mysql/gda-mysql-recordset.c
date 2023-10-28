/*
 * Copyright (C) 2001 - 2005 Rodrigo Moya <rodrigo@gnome-db.org>
 * Copyright (C) 2002 Andrew Hill <andru@src.gnome.org>
 * Copyright (C) 2002 - 2003 Gonzalo Paniagua Javier <gonzalo@ximian.com>
 * Copyright (C) 2002 Holger Thon <holger.thon@gnome-db.org>
 * Copyright (C) 2002 Zbigniew Chyla <cyba@gnome.pl>
 * Copyright (C) 2003 Chris Silles <csilles@src.gnome.org>
 * Copyright (C) 2003 Laurent Sansonetti <lrz@gnome.org>
 * Copyright (C) 2003 Paisa Seeluangsawat <paisa@users.sf.net>
 * Copyright (C) 2004 - 2016 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2005 Alan Knowles <alan@akbkhome.com>
 * Copyright (C) 2005 - 2009 Bas Driessen <bas.driessen@xobas.com>
 * Copyright (C) 2005 Mike Fisk <mfisk@woozle.org>
 * Copyright (C) 2005 Álvaro Peña <alvaropg@telefonica.net>
 * Copyright (C) 2011 Murray Cumming <murrayc@murrayc.com>
 * Copyright (C) 2019 Daniel Espinosa <esodan@gmail.com>
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
#include <gda-data-select-private.h>
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
gda_mysql_recordset_init (GdaMysqlRecordset       *recset);
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

typedef struct {
	GdaConnection  *cnc;
	
	MYSQL_STMT     *mysql_stmt;

	gint            chunk_size;    /* Number of rows to fetch at a time when iterating forward/backward. */
	gint            chunks_read;   /* Number of times that we've iterated forward/backward. */
	GdaRow         *tmp_row;       /* Used in cursor mode to store a reference to the latest #GdaRow. */

	/* if no prepared statement available */
	gint          ncols;
        GType        *types;
} GdaMysqlRecordsetPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GdaMysqlRecordset, gda_mysql_recordset, GDA_TYPE_DATA_SELECT)
/*
 * Object init and finalize
 */
static void
gda_mysql_recordset_init (GdaMysqlRecordset       *recset)
{
	g_return_if_fail (GDA_IS_MYSQL_RECORDSET (recset));
  GdaMysqlRecordsetPrivate *priv = gda_mysql_recordset_get_instance_private (recset);

	priv->cnc = NULL;

	/* initialize specific information */
	priv->chunk_size = 1;
	priv->chunks_read = 0;

	priv->ncols = 0;
	priv->types = NULL;
}


gint
gda_mysql_recordset_get_chunk_size (GdaMysqlRecordset  *recset)
{
	g_return_val_if_fail (GDA_IS_MYSQL_RECORDSET (recset), -1);
  GdaMysqlRecordsetPrivate *priv = gda_mysql_recordset_get_instance_private (recset);
	return priv->chunk_size;
}

void
gda_mysql_recordset_set_chunk_size (GdaMysqlRecordset  *recset,
				    gint                chunk_size)
{
	g_return_if_fail (GDA_IS_MYSQL_RECORDSET (recset));
  GdaMysqlRecordsetPrivate *priv = gda_mysql_recordset_get_instance_private (recset);

	if (priv->mysql_stmt == NULL)  // Creation is in progress so it's not set.
		return;

#if MYSQL_VERSION_ID >= 50002
	const unsigned long prefetch_rows = chunk_size;
	if (mysql_stmt_attr_set (priv->mysql_stmt, STMT_ATTR_PREFETCH_ROWS,
				 (void *) &prefetch_rows)) {
		g_warning ("%s: %s\n", __func__, mysql_stmt_error (priv->mysql_stmt));
		return;
	}
	priv->chunk_size = chunk_size;
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
  GdaMysqlRecordsetPrivate *priv = gda_mysql_recordset_get_instance_private (recset);

	return priv->chunks_read;
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

	recordset = GDA_MYSQL_RECORDSET(object);
  GdaMysqlRecordsetPrivate *priv = gda_mysql_recordset_get_instance_private (recordset);

	switch (param_id) {
	case PROP_CHUNK_SIZE:
		g_value_set_int (value, priv->chunk_size);
		break;
	case PROP_CHUNKS_READ:
		g_value_set_int (value, priv->chunks_read);
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
  GdaMysqlRecordsetPrivate *priv = gda_mysql_recordset_get_instance_private (recset);

	gda_mysql_pstmt_set_stmt_used (GDA_MYSQL_PSTMT (gda_data_select_get_prep_stmt (GDA_DATA_SELECT (object))), FALSE);

	if (priv->cnc) {
		g_object_unref (G_OBJECT(priv->cnc));
		priv->cnc = NULL;
	}
	if (priv->tmp_row) {
		g_object_unref (G_OBJECT(priv->tmp_row));
		priv->tmp_row = NULL;
	}
	if (priv->types) {
		g_free (priv->types);
    priv->types = NULL;
  }

	G_OBJECT_CLASS (gda_mysql_recordset_parent_class)->dispose (object);
}

/**
 * Read a long from a byte buffer in big endian format.
 * @param buf must be 8 bytes
 */
static gint64 read_long_from_bytes_big_endian(const guchar* buf) {
	return  (gint64) (buf[0] & 0xff) << 56
			| (gint64) (buf[1] & 0xff) << 48
			| (gint64) (buf[2] & 0xff) << 40
			| (gint64) (buf[3] & 0xff) << 32
			| (gint64) (buf[4] & 0xff) << 24
			| (gint64) (buf[5] & 0xff) << 16
			| (gint64) (buf[6] & 0xff) <<  8
			| (gint64) (buf[7] & 0xff);
}
/**
 * Read a long from a byte buffer in little endian format.
 * @param buf must be 8 bytes
 */
static gint64 read_long_from_bytes_little_endian(const guchar* buf) {
	return  (gint64) (buf[7] & 0xff) << 56
			| (gint64) (buf[6] & 0xff) << 48
			| (gint64) (buf[5] & 0xff) << 40
			| (gint64) (buf[4] & 0xff) << 32
			| (gint64) (buf[3] & 0xff) << 24
			| (gint64) (buf[2] & 0xff) << 16
			| (gint64) (buf[1] & 0xff) <<  8
			| (gint64) (buf[0] & 0xff);
}
/**
 * Read a long from a byte buffer in big or little endian format.
 * @param bigEndian true for big endian, false for little endian
 * @param buf must be 8 bytes
 */
static gdouble read_double_from_bytes(const guchar* buf, gboolean big_endian) {
    gint64 long_val =  big_endian ? read_long_from_bytes_big_endian (buf)
            : read_long_from_bytes_little_endian (buf);
	union {
        gint64 val_long;
        gdouble val_double;
    } val_union;
    val_union.val_long = long_val;

    return (gdouble)val_union.val_double;
}

/**
* Read a long from a byte buffer in big or little endian format.
* @param bigEndian true for big endian, false for little endian
* @param buf must be 8 bytes length after offset
*/
static double read_double_from_bytes_offset(const guchar* buf, gint offset, gboolean big_endian) {
	return read_double_from_bytes(buf + offset, big_endian);
}

/*
 * Public functions
 */

static GType
_gda_mysql_type_to_gda (G_GNUC_UNUSED MysqlConnectionData *cdata,
			enum enum_field_types  mysql_type, unsigned int charsetnr)
{
	GType gtype;
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
	case MYSQL_TYPE_GEOMETRY:
		gtype = GDA_TYPE_GEOMETRIC_POINT;
		break;
	case MYSQL_TYPE_DOUBLE:
		gtype = G_TYPE_DOUBLE;
		break;
	case MYSQL_TYPE_TIMESTAMP:
	case MYSQL_TYPE_DATETIME:
		gtype = G_TYPE_DATE_TIME;
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

	cdata = (MysqlConnectionData*) gda_connection_internal_get_provider_data_error (cnc, NULL);
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
  GdaMysqlRecordsetPrivate *priv = gda_mysql_recordset_get_instance_private (model);
  priv->cnc = cnc;
	g_object_ref (G_OBJECT(cnc));

	/* columns & types */	
	priv->ncols = mysql_field_count (cdata->mysql);
	priv->types = g_new0 (GType, priv->ncols);
	
	/* create columns */
	for (i = 0; i < priv->ncols; i++)
		columns = g_slist_prepend (columns, gda_column_new ());
	columns = g_slist_reverse (columns);

	if (col_types) {
		for (i = 0; ; i++) {
			if (col_types [i] > 0) {
				if (col_types [i] == G_TYPE_NONE)
					break;
				if (i >= priv->ncols) {
					g_warning (_("Column %d out of range (0-%d), ignoring its specified type"), i,
						   priv->ncols - 1);
					break;
				}
				else
					priv->types [i] = col_types [i];
			}
		}
	}

	/* fill bind result */
	MYSQL_RES *mysql_res = mysql_store_result (cdata->mysql);
	MYSQL_FIELD *mysql_fields = mysql_fetch_fields (mysql_res);
	GSList *list;

	gda_data_select_set_advertized_nrows ((GdaDataSelect *) model, mysql_affected_rows (cdata->mysql));
	for (i=0, list = columns; 
	     i < priv->ncols;
	     i++, list = list->next) {
		GdaColumn *column = GDA_COLUMN (list->data);
		
		/* use C API to set columns' information using gda_column_set_*() */
		MYSQL_FIELD *field = &mysql_fields[i];
		
		GType gtype = priv->types [i];
		if (gtype == GDA_TYPE_NULL) {
			gtype = _gda_mysql_type_to_gda (cdata, field->type, field->charsetnr);
			priv->types [i] = gtype;
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
		GdaRow *row = gda_row_new (priv->ncols);
		gint col;
		for (col = 0; col < priv->ncols; col++) {
			gint i = col;
		
			GValue *value = gda_row_get_value (row, i);
			GType type = priv->types[i];
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
 *
 * See MySQL's documentation "C API Prepared Statement Type Codes":
 *     http://docs.oracle.com/cd/E17952_01/refman-5.5-en/c-api-prepared-statement-type-codes.html
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

	cdata = (MysqlConnectionData*) gda_connection_internal_get_provider_data_error (cnc, NULL);
	if (!cdata)
		return NULL;

	g_assert (gda_mysql_pstmt_get_mysql_stmt(ps));

	/* make sure @ps reports the correct number of columns using the API*/
	if (gda_pstmt_get_ncols (_GDA_PSTMT (ps)) < 0)
		gda_pstmt_set_cols (_GDA_PSTMT(ps), mysql_stmt_field_count (gda_mysql_pstmt_get_mysql_stmt(ps)), gda_pstmt_get_types (_GDA_PSTMT(ps)));

        /* completing @ps if not yet done */
	g_assert (! gda_mysql_pstmt_get_stmt_used (ps));
        gda_mysql_pstmt_set_stmt_used (ps, TRUE);
        if (!gda_pstmt_get_types (_GDA_PSTMT (ps)) && (gda_pstmt_get_ncols (_GDA_PSTMT (ps)) > 0)) {
		/* create prepared statement's columns */
		for (i = 0; i < gda_pstmt_get_ncols (_GDA_PSTMT (ps)); i++)
			gda_pstmt_set_tmpl_columns (_GDA_PSTMT (ps), g_slist_prepend (gda_pstmt_get_tmpl_columns (_GDA_PSTMT (ps)),
									 gda_column_new ()));
		gda_pstmt_set_tmpl_columns (_GDA_PSTMT (ps), g_slist_reverse (gda_pstmt_get_tmpl_columns (_GDA_PSTMT (ps))));

		/* create prepared statement's types, all types are initialized to GDA_TYPE_NULL */
		gda_pstmt_set_cols (_GDA_PSTMT (ps), gda_pstmt_get_ncols (_GDA_PSTMT (ps)), g_new (GType,gda_pstmt_get_ncols (_GDA_PSTMT (ps))));
		for (i = 0; i < gda_pstmt_get_ncols (_GDA_PSTMT (ps)); i++)
			gda_pstmt_get_types (_GDA_PSTMT (ps)) [i] = GDA_TYPE_NULL;

		if (col_types) {
			for (i = 0; ; i++) {
				if (col_types [i] > 0) {
					if (col_types [i] == G_TYPE_NONE)
						break;
					if (i >= gda_pstmt_get_ncols (_GDA_PSTMT (ps))) {
						g_warning (_("Column %d out of range (0-%d), ignoring its specified type"), i,
							   gda_pstmt_get_ncols (_GDA_PSTMT (ps)) - 1);
						break;
					}
					else
						gda_pstmt_get_types (_GDA_PSTMT (ps)) [i] = col_types [i];
				}
			}
		}
	}

	/* get rid of old bound result if any */
	if (gda_mysql_pstmt_get_mysql_bind_result (ps)) {
    gda_mysql_pstmt_free_mysql_bind_result (ps);
	}

	/* fill bind result */
	MYSQL_RES *mysql_res = mysql_stmt_result_metadata (gda_mysql_pstmt_get_mysql_stmt(ps));
	MYSQL_FIELD *mysql_fields = mysql_fetch_fields (mysql_res);
	
	MYSQL_BIND *mysql_bind_result = g_new0 (MYSQL_BIND, gda_pstmt_get_ncols (_GDA_PSTMT (ps)));
	GSList *list;

	for (i=0, list = gda_pstmt_get_tmpl_columns (_GDA_PSTMT (ps));
	     i < gda_pstmt_get_ncols (_GDA_PSTMT (ps));
	     i++, list = list->next) {
		GdaColumn *column = GDA_COLUMN (list->data);
		
		/* use C API to set columns' information using gda_column_set_*() */
		MYSQL_FIELD *field = &mysql_fields[i];
		
		GType gtype = gda_pstmt_get_types (_GDA_PSTMT (ps))[i];
		if (gtype == GDA_TYPE_NULL) {
			gtype = _gda_mysql_type_to_gda (cdata, field->type, field->charsetnr);
			gda_pstmt_get_types (_GDA_PSTMT (ps))[i] = gtype;
		}
		gda_column_set_g_type (column, gtype);
		gda_column_set_name (column, field->name);
		gda_column_set_description (column, field->name);
		
		/* binding results with types */
		mysql_bind_result[i].buffer_type = field->type;
		mysql_bind_result[i].is_unsigned = field->flags & UNSIGNED_FLAG ? TRUE : FALSE;
		mysql_bind_result[i].is_null = g_malloc0 (sizeof (my_bool));
		
		switch (mysql_bind_result[i].buffer_type) {
		case MYSQL_TYPE_TINY:
			mysql_bind_result[i].buffer = g_malloc0 (sizeof (signed char));
			break;
		case MYSQL_TYPE_SHORT:
			mysql_bind_result[i].buffer = g_malloc0 (sizeof (short int));
			break;
		case MYSQL_TYPE_INT24:
		case MYSQL_TYPE_LONG:
		case MYSQL_TYPE_YEAR:
			mysql_bind_result[i].buffer = g_malloc0 (sizeof (int));
			break;
		case MYSQL_TYPE_LONGLONG:
			mysql_bind_result[i].buffer = g_malloc0 (sizeof (long long));
			break;
		case MYSQL_TYPE_NULL:
			break;
		case MYSQL_TYPE_TIME:
		case MYSQL_TYPE_DATE:
		case MYSQL_TYPE_DATETIME:
		case MYSQL_TYPE_TIMESTAMP:
			mysql_bind_result[i].buffer = g_malloc0 (sizeof (MYSQL_TIME));
			break;
		case MYSQL_TYPE_FLOAT:
		case MYSQL_TYPE_DOUBLE:
			mysql_bind_result[i].buffer = g_malloc0 (sizeof (double));
			break;
		case MYSQL_TYPE_STRING:
		case MYSQL_TYPE_VAR_STRING:
		case MYSQL_TYPE_BLOB:
		case MYSQL_TYPE_TINY_BLOB:
		case MYSQL_TYPE_MEDIUM_BLOB:
		case MYSQL_TYPE_LONG_BLOB:
		case MYSQL_TYPE_DECIMAL:
		case MYSQL_TYPE_NEWDECIMAL:
		case MYSQL_TYPE_GEOMETRY:
		case MYSQL_TYPE_BIT:
			mysql_bind_result[i].buffer = g_malloc0 (field->max_length + 1);
			mysql_bind_result[i].buffer_length = field->max_length + 1;
			mysql_bind_result[i].length = g_malloc0 (sizeof (unsigned long));
			break;
		default:
			g_warning (_("Invalid column bind data type. %d\n"),
				   mysql_bind_result[i].buffer_type);
		}
		/*g_print ("%s(): NAME=%s, TYPE=%d, GTYPE=%s, unsigned: %d\n",
			 __FUNCTION__, field->name, field->type, g_type_name (gtype),
			 field->flags & UNSIGNED_FLAG);*/
	}
	
	if (mysql_stmt_bind_result (gda_mysql_pstmt_get_mysql_stmt(ps), mysql_bind_result)) {
		g_warning ("mysql_stmt_bind_result failed: %s\n",
			   mysql_stmt_error (gda_mysql_pstmt_get_mysql_stmt(ps)));
	}
	
	mysql_free_result (mysql_res);
	gda_mysql_pstmt_set_mysql_bind_result (ps, mysql_bind_result);

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
  GdaMysqlRecordsetPrivate *priv = gda_mysql_recordset_get_instance_private (model);
  priv->cnc = cnc;
	g_object_ref (G_OBJECT(cnc));

	priv->mysql_stmt = gda_mysql_pstmt_get_mysql_stmt(ps);

	gda_data_select_set_advertized_nrows ((GdaDataSelect *) model, mysql_stmt_affected_rows (gda_mysql_pstmt_get_mysql_stmt(ps)));

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
  GdaMysqlRecordsetPrivate *priv = gda_mysql_recordset_get_instance_private (imodel);

	if (gda_data_select_get_advertized_nrows (model) >= 0)
		return gda_data_select_get_advertized_nrows (model);

	gda_data_select_set_advertized_nrows (model, mysql_stmt_affected_rows (priv->mysql_stmt));
	
	return gda_data_select_get_advertized_nrows (model);
}

static GdaRow *
new_row_from_mysql_stmt (GdaMysqlRecordset *imodel, G_GNUC_UNUSED gint rownum, GError **error)
{
	//g_print ("%s(): NCOLS=%d  ROWNUM=%d\n", __func__, ((GdaDataSelect *) imodel)->prep_stmt->ncols, rownum);
	int res;
	MYSQL_BIND *mysql_bind_result;
  GdaMysqlRecordsetPrivate *priv = gda_mysql_recordset_get_instance_private (imodel);
	g_return_val_if_fail (priv->mysql_stmt != NULL, NULL);

	mysql_bind_result = gda_mysql_pstmt_get_mysql_bind_result ((GdaMysqlPStmt *) gda_data_select_get_prep_stmt ((GdaDataSelect *) imodel));
	g_assert (mysql_bind_result);

	res = mysql_stmt_fetch (priv->mysql_stmt);
	if (res == MYSQL_NO_DATA) {
		/* should not happen */
		g_set_error (error, GDA_DATA_MODEL_ERROR, GDA_DATA_MODEL_ACCESS_ERROR,
			     "%s", "No more data, please report this bug to "
			     "http://gitlab.gnome.org/GNOME/libgda/issues");
	}
	else if (res == MYSQL_DATA_TRUNCATED) {
		GString *string;

		string = g_string_new ("Truncated data, please report this bug to "
				       "http://gitlab.gnome.org/GNOME/libgda/issues");

		gint col;
		for (col = 0; col < gda_pstmt_get_ncols (gda_data_select_get_prep_stmt ((GdaDataSelect *) imodel)); ++col) {
			my_bool truncated;
			mysql_bind_result[col].error = &truncated;
			mysql_stmt_fetch_column (priv->mysql_stmt, &(mysql_bind_result[col]),
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
		_gda_mysql_make_error (priv->cnc, NULL, priv->mysql_stmt, error);
		return NULL;
	}

	/* g_print ("%s: SQL=%s\n", __func__, ((GdaDataSelect *) imodel)->prep_stmt->sql); */

	
	GdaRow *row = gda_row_new (gda_pstmt_get_ncols (gda_data_select_get_prep_stmt ((GdaDataSelect *) imodel)));
	gint col;
	for (col = 0; col < gda_pstmt_get_ncols (gda_data_select_get_prep_stmt ((GdaDataSelect *) imodel)); ++col) {
		gint i = col;
		
		GValue *value = gda_row_get_value (row, i);
		GType type = gda_pstmt_get_types (gda_data_select_get_prep_stmt ((GdaDataSelect *) imodel))[i];
		
		/*g_print ("%s: #%d : TYPE=%d, GTYPE=%s\n", __func__, i, mysql_bind_result[i].buffer_type, g_type_name (type));*/

		my_bool is_null = FALSE;
		unsigned long length;
		
		memmove (&is_null, mysql_bind_result[i].is_null, sizeof (my_bool));
		if (is_null) {
			gda_value_set_null (value);
			continue;
		}
		else
			gda_value_reset_with_type (value, type);

		switch (mysql_bind_result[i].buffer_type) {
		case MYSQL_TYPE_SHORT: {
			short int bvalue = 0;
			memmove (&bvalue, mysql_bind_result[i].buffer, sizeof (bvalue));
			g_value_set_int (value, bvalue);
			break;
		}
		case MYSQL_TYPE_TINY: {
			signed char bvalue = 0;
			memmove (&bvalue, mysql_bind_result[i].buffer, sizeof (bvalue));
			g_value_set_int (value, bvalue);
			break;
		}
		case MYSQL_TYPE_INT24:
		case MYSQL_TYPE_LONG:
		case MYSQL_TYPE_YEAR: {
			int bvalue = 0;
			memmove (&bvalue, mysql_bind_result[i].buffer, sizeof (bvalue));
			
			if (type == G_TYPE_INT)
				g_value_set_int (value, bvalue);
			else if (type == G_TYPE_LONG)
				g_value_set_long (value, (long) bvalue);
			else if (type == G_TYPE_BOOLEAN)
				g_value_set_boolean (value, bvalue ? TRUE : FALSE);
			else {
				gda_row_invalidate_value (row, value);
				g_set_error (error, GDA_SERVER_PROVIDER_ERROR,
					     GDA_SERVER_PROVIDER_DATA_ERROR,
					     _("Type %s not mapped for value %d"),
					     g_type_name (type), bvalue);
			}
			break;
		}
		case MYSQL_TYPE_LONGLONG: {
			long long bvalue = 0;
			memmove (&bvalue, mysql_bind_result[i].buffer, sizeof (bvalue));

			if (type == G_TYPE_BOOLEAN)
				g_value_set_boolean (value, bvalue ? TRUE : FALSE);
			else if (type == G_TYPE_INT)
				g_value_set_int (value, bvalue);
			else if (type == G_TYPE_LONG)
				g_value_set_long (value, bvalue);
			else {
				gda_row_invalidate_value (row, value);
				g_set_error (error, GDA_SERVER_PROVIDER_ERROR,
					     GDA_SERVER_PROVIDER_DATA_ERROR,
					     _("Type %s not mapped for value %lld"),
					     g_type_name (type), bvalue);
			}
			break;
		}
		case MYSQL_TYPE_NULL:
			gda_value_set_null (value);
			break;
		case MYSQL_TYPE_TIME:
		case MYSQL_TYPE_DATE:
		case MYSQL_TYPE_DATETIME:
		case MYSQL_TYPE_TIMESTAMP: {
			MYSQL_TIME bvalue = { 0, 0, 0, 0, 0, 0, 0, 0, 0 };
			memmove (&bvalue, mysql_bind_result[i].buffer, sizeof (bvalue));

			if (type == GDA_TYPE_TIME) {
				GdaTime *time = gda_time_new_from_values (bvalue.hour,
					                                      bvalue.minute,
					                                      bvalue.second,
					                                      bvalue.second_part,
					                                      0); /* GMT */
				gda_value_set_time (value, time);
                gda_time_free (time);
			}
			else if (type == G_TYPE_DATE) {
				GDate *date = g_date_new_dmy
					((bvalue.day != 0) ? bvalue.day : 1,
					 (bvalue.month != 0) ? bvalue.month : 1,
					 (bvalue.year != 0) ? bvalue.year : 1970);
				g_value_take_boxed (value, date);
			}
			else if (type == G_TYPE_DATE_TIME) {
				GTimeZone *tz = g_time_zone_new_identifier ("Z"); /* UTC */
				GDateTime *timestamp = g_date_time_new (tz,
																								bvalue.year,
																								bvalue.month,
																								bvalue.day,
																								bvalue.hour,
																								bvalue.minute,
																								(gdouble) bvalue.second + bvalue.second_part / 1000000.0);
				g_value_set_boxed (value, timestamp);
				g_date_time_unref (timestamp);
			}
			else {
				gda_row_invalidate_value (row, value);
				g_set_error (error, GDA_SERVER_PROVIDER_ERROR,
					     GDA_SERVER_PROVIDER_DATA_ERROR,
					     _("Type %s not mapped for value %d/%d/%d %d:%d:%d.%lu"),
					     g_type_name (type), bvalue.year, bvalue.month,
					     bvalue.day, bvalue.hour, bvalue.minute,
					     bvalue.second, bvalue.second_part);
			}
			break;
		}
		case MYSQL_TYPE_FLOAT: {
			float bvalue = 0.;
			memmove (&bvalue, mysql_bind_result[i].buffer, sizeof (bvalue));
			
			if (type == G_TYPE_FLOAT)
				g_value_set_float (value, (float) bvalue);
			else {
				gda_row_invalidate_value (row, value);
				g_set_error (error, GDA_SERVER_PROVIDER_ERROR,
					     GDA_SERVER_PROVIDER_DATA_ERROR,
					     _("Type %s not mapped for value %f"),
					     g_type_name (type), bvalue);
			}			
			break;
		}
		case MYSQL_TYPE_DOUBLE: {
			double bvalue = 0.0;
			memmove (&bvalue, mysql_bind_result[i].buffer, sizeof (bvalue));
			
			if (type == G_TYPE_DOUBLE)
				g_value_set_double (value, bvalue);
			else {
				gda_row_invalidate_value (row, value);
				g_set_error (error, GDA_SERVER_PROVIDER_ERROR,
					     GDA_SERVER_PROVIDER_DATA_ERROR,
					     _("Type %s not mapped for value %f"),
					     g_type_name (type), bvalue);
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
		case MYSQL_TYPE_GEOMETRY:
		case MYSQL_TYPE_BIT: {
			char *bvalue = NULL;
			memmove (&length, mysql_bind_result[i].length, sizeof (unsigned long));
			if (length > 0) {
				bvalue = g_malloc (length + 1);
				memcpy (bvalue, mysql_bind_result[i].buffer, length);
				bvalue [length] = 0;
			}
			
			if (type == G_TYPE_STRING) {
				if (length == 0) {
					bvalue = "\0";
				}
				g_value_set_string (value, bvalue);
				bvalue = NULL;
			}
			else if (type == GDA_TYPE_TEXT) {
				if (length == 0) {
					bvalue = "\0";
				}
				GdaText *text = gda_text_new ();
				gda_text_set_string (text, bvalue);
				g_value_take_boxed (value, text);
				bvalue = NULL;
			}
			else if (type == GDA_TYPE_BINARY) {
				GdaBinary *bin;
				bin = gda_binary_new ();
				gda_binary_set_data (bin, (guchar*) bvalue, length);
				gda_value_take_binary (value, bin);
			}
			else if (type == GDA_TYPE_BLOB) {
				/* we don't use GdaMysqlBlobOp because it looks like the MySQL
				 * API does not support BLOBs accessed in a random way,
				 * so we return the whole BLOB at once */
				GdaBlob *blob = gda_blob_new ();
				GdaBinary *bin = gda_blob_get_binary (blob);
				gda_binary_set_data (bin, (guchar*) bvalue, length);
				gda_value_take_blob (value, blob);
			}
			else if (type == GDA_TYPE_NUMERIC) {
				if (length > 0) {
					GdaNumeric *numeric;
					numeric = gda_numeric_new ();
					gda_numeric_set_from_string (numeric, bvalue);
					gda_numeric_set_precision (numeric, 6);
					gda_numeric_set_width (numeric, length);
					gda_value_set_numeric (value, numeric);
					gda_numeric_free (numeric);
				}
			}
			else if (type == GDA_TYPE_GEOMETRIC_POINT) {
				if (length > 0) {
					GdaGeometricPoint* point = gda_geometric_point_new ();

					guchar bytes = (guchar)(*(bvalue + 4));
					gboolean is_big_endian = (0 == bytes);
					gdouble x = read_double_from_bytes_offset(bvalue, 9, is_big_endian);
					gdouble y = read_double_from_bytes_offset(bvalue, 17, is_big_endian);
					gda_geometric_point_set_x (point, x);
					gda_geometric_point_set_y (point, y);
					gda_value_set_geometric_point (value, point);
					// g_warning("Point: %s", gda_value_stringify(value));
					gda_geometric_point_free (point);
				}
			}
			else if (type == G_TYPE_DOUBLE) {
				if (length > 0)
					g_value_set_double (value, g_ascii_strtod (bvalue, NULL));
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
					g_value_set_int (value, atoi (bvalue));
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
					g_value_set_boolean (value, atoi (bvalue));
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
					     g_type_name (type), bvalue);
			}
			g_free (bvalue);
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
	
	if (gda_data_select_get_nb_stored_rows (model) == gda_data_select_get_advertized_nrows (model)) {
		/* All the row have been converted.  We could free result now */
		/* but it was done before provided no field-based API functions */
		/* that process result set meta data was needed in the middle. */
	}

	return TRUE;
}

/*
 * Create a new filled #GdaRow object for the next cursor row, and put it into *row.
 *
 * Each new #GdaRow created is referenced only by priv->tmp_row (the #GdaDataSelect implementation
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
  GdaMysqlRecordsetPrivate *priv = gda_mysql_recordset_get_instance_private (imodel);

	if (priv->tmp_row)
		g_object_unref (G_OBJECT(priv->tmp_row));
	*row = new_row_from_mysql_stmt (imodel, rownum, error);
	priv->tmp_row = *row;

	return TRUE;
}

/*
 * Create a new filled #GdaRow object for the previous cursor row, and put it into *prow.
 *
 * Each new #GdaRow created is referenced only by priv->tmp_row (the #GdaDataSelect implementation
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
  GdaMysqlRecordsetPrivate *priv = gda_mysql_recordset_get_instance_private (imodel);

	if (priv->tmp_row)
		g_object_unref (G_OBJECT(priv->tmp_row));
	*row = new_row_from_mysql_stmt (imodel, rownum, error);
	priv->tmp_row = *row;

	return TRUE;
}

/*
 * Create a new filled #GdaRow object for the cursor row at position @rownum, and put it into *row.
 *
 * Each new #GdaRow created is referenced only by priv->tmp_row (the #GdaDataSelect implementation
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
  GdaMysqlRecordsetPrivate *priv = gda_mysql_recordset_get_instance_private (imodel);

	if (priv->tmp_row)
		g_object_unref (G_OBJECT(priv->tmp_row));
	*row = new_row_from_mysql_stmt (imodel, rownum, error);
	priv->tmp_row = *row;

	return TRUE;
}

