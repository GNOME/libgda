/* GDA provider
 * Copyright (C) 2009 The GNOME Foundation.
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

#include <stdarg.h>
#include <string.h>
#include <glib/gi18n-lib.h>
#include <libgda/gda-util.h>
#include <libgda/sql-parser/gda-sql-parser.h>
#include <libgda/gda-connection-private.h>
#include "gda-web.h"
#include "gda-web-recordset.h"
#include "gda-web-provider.h"

#define _GDA_PSTMT(x) ((GdaPStmt*)(x))

static void gda_web_recordset_class_init (GdaWebRecordsetClass *klass);
static void gda_web_recordset_init       (GdaWebRecordset *recset,
					     GdaWebRecordsetClass *klass);
static void gda_web_recordset_dispose   (GObject *object);

/* virtual methods */
static gint     gda_web_recordset_fetch_nb_rows (GdaDataSelect *model);
static gboolean gda_web_recordset_fetch_random (GdaDataSelect *model, GdaRow **prow, gint rownum, GError **error);
static gboolean gda_web_recordset_fetch_next (GdaDataSelect *model, GdaRow **prow, gint rownum, GError **error);
static gboolean gda_web_recordset_fetch_prev (GdaDataSelect *model, GdaRow **prow, gint rownum, GError **error);
static gboolean gda_web_recordset_fetch_at (GdaDataSelect *model, GdaRow **prow, gint rownum, GError **error);


struct _GdaWebRecordsetPrivate {
	GdaConnection *cnc;
	
	GdaConnection *rs_cnc; /* connection to store resultsets */
	gboolean table_created;
	
	GdaStatement *insert; /* adding new data to @rs_cnc */
	GdaStatement *select; /* final SELECT */
	GdaDataModel *real_model;
};
static GObjectClass *parent_class = NULL;

/*
 * Object init and finalize
 */
static void
gda_web_recordset_init (GdaWebRecordset *recset,
			   GdaWebRecordsetClass *klass)
{
	g_return_if_fail (GDA_IS_WEB_RECORDSET (recset));
	recset->priv = g_new0 (GdaWebRecordsetPrivate, 1);
	recset->priv->cnc = NULL;
	recset->priv->rs_cnc = NULL;
	recset->priv->table_created = FALSE;
	recset->priv->insert = NULL;
	recset->priv->select = NULL;
}

static void
gda_web_recordset_class_init (GdaWebRecordsetClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GdaDataSelectClass *pmodel_class = GDA_DATA_SELECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->dispose = gda_web_recordset_dispose;
	pmodel_class->fetch_nb_rows = gda_web_recordset_fetch_nb_rows;
	pmodel_class->fetch_random = gda_web_recordset_fetch_random;

	pmodel_class->fetch_next = gda_web_recordset_fetch_next;
	pmodel_class->fetch_prev = gda_web_recordset_fetch_prev;
	pmodel_class->fetch_at = gda_web_recordset_fetch_at;
}

static void
gda_web_recordset_dispose (GObject *object)
{
	GdaWebRecordset *recset = (GdaWebRecordset *) object;

	g_return_if_fail (GDA_IS_WEB_RECORDSET (recset));

	if (recset->priv) {
		if (recset->priv->cnc) 
			g_object_unref (recset->priv->cnc);
		if (recset->priv->real_model)
			g_object_unref (recset->priv->real_model);
		if (recset->priv->rs_cnc)
			g_object_unref (recset->priv->rs_cnc);

		if (recset->priv->insert)
			g_object_unref (recset->priv->insert);
		if (recset->priv->select)
			g_object_unref (recset->priv->select);

		g_free (recset->priv);
		recset->priv = NULL;
	}

	parent_class->dispose (object);
}

/*
 * Public functions
 */

GType
gda_web_recordset_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static GStaticMutex registering = G_STATIC_MUTEX_INIT;
		static const GTypeInfo info = {
			sizeof (GdaWebRecordsetClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_web_recordset_class_init,
			NULL,
			NULL,
			sizeof (GdaWebRecordset),
			0,
			(GInstanceInitFunc) gda_web_recordset_init
		};
		g_static_mutex_lock (&registering);
		if (type == 0)
			type = g_type_register_static (GDA_TYPE_DATA_SELECT, "GdaWebRecordset", &info, 0);
		g_static_mutex_unlock (&registering);
	}

	return type;
}

/*
 * the @ps struct is modified and transferred to the new data model created in
 * this function
 */
GdaDataModel *
gda_web_recordset_new (GdaConnection *cnc, GdaWebPStmt *ps, GdaSet *exec_params,
		       GdaDataModelAccessFlags flags, GType *col_types,
		       const gchar *session_id, xmlNodePtr data_node, GError **error)
{
	GdaWebRecordset *model;
        gint i;
	GdaDataModelAccessFlags rflags;
	static guint counter = 0;

        g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);
        g_return_val_if_fail (ps != NULL, NULL);

	/* prepare internal connection which will be used to store
	 * the recordset's data
	 */
	GdaConnection *rs_cnc;
	gchar *fname, *tmp;
	
	for (fname = (gchar*) session_id; *fname && (*fname != '='); fname++);
	g_assert (*fname == '=');
	fname++;
	tmp = g_strdup_printf ("%s%u.db", fname, counter++);
	rs_cnc = gda_connection_open_sqlite (NULL, tmp, TRUE);
	if (!rs_cnc) {
		fname = g_build_filename (g_get_tmp_dir(), tmp, NULL);
		g_set_error (error, GDA_SERVER_PROVIDER_ERROR,
			     GDA_SERVER_PROVIDER_INTERNAL_ERROR,
			     _("Can't create temporary file '%s'"), fname);
		g_free (tmp);
		g_free (fname);
		return NULL;
	}
	g_free (tmp);

	/* make sure @ps reports the correct number of columns using the API*/
        if (_GDA_PSTMT (ps)->ncols < 0) {
		xmlNodePtr child;
		_GDA_PSTMT (ps)->ncols = 0;
		for (child = data_node->children; child; child = child->next) {
			if (!strcmp ((gchar*) child->name, "gda_array_field"))
				_GDA_PSTMT (ps)->ncols ++;
		}
	}

        /* completing @ps if not yet done */
        if (!_GDA_PSTMT (ps)->types && (_GDA_PSTMT (ps)->ncols > 0)) {
		/* create prepared statement's columns */
		GSList *list;
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
		
		/* fill GdaColumn's data */
		xmlNodePtr child;
		WebConnectionData *cdata;
		cdata = (WebConnectionData*) gda_connection_internal_get_provider_data (cnc);
		if (!cdata) 
			return FALSE;

		for (child = data_node->children, i = 0, list = _GDA_PSTMT (ps)->tmpl_columns; 
		     child && (i < GDA_PSTMT (ps)->ncols);
		     child = child->next, i++, list = list->next) {
			GdaColumn *column;
			xmlChar *prop;
			gboolean typeset = FALSE;

			while (strcmp ((gchar*) child->name, "gda_array_field"))
				child = child->next;
			column = GDA_COLUMN (list->data);

			if (_GDA_PSTMT (ps)->types [i] == 0) {
				if (cdata && cdata->reuseable && cdata->reuseable->operations->re_get_type) {
					prop = xmlGetProp (child, BAD_CAST "dbtype");
					if (prop) {
						GType type;
						type = cdata->reuseable->operations->re_get_type (cnc, cdata->reuseable,
												  (gchar*) prop);
						if (type != GDA_TYPE_NULL) {
							_GDA_PSTMT (ps)->types [i] = type;
							gda_column_set_g_type (column, type);
							typeset = TRUE;
						}
						xmlFree (prop);
					}
				}
				if (!typeset) {
					prop = xmlGetProp (child, BAD_CAST "gdatype");
					if (prop) {
						GType type;
						
						type = gda_g_type_from_string ((gchar*) prop);
						_GDA_PSTMT (ps)->types [i] = type;
						gda_column_set_g_type (column, type);
						xmlFree (prop);
					}
					else {
						_GDA_PSTMT (ps)->types [i] = G_TYPE_STRING;
						gda_column_set_g_type (column, G_TYPE_STRING);
					}
				}
			}
			else
				gda_column_set_g_type (column, _GDA_PSTMT (ps)->types [i]);
			prop = xmlGetProp (child, BAD_CAST "name");
			if (prop && *prop) {
				gda_column_set_name (column, (gchar*) prop);
				gda_column_set_description (column, (gchar*) prop);
			}
			else {
				gchar *tmp;
				tmp = g_strdup_printf ("col%d", i);
				gda_column_set_name (column, tmp);
				gda_column_set_description (column, tmp);
				g_free (tmp);
			}
			if (prop)
				xmlFree (prop);
		}
        }

	/* determine access mode: RANDOM or CURSOR FORWARD are the only supported */
	if (flags & GDA_DATA_MODEL_ACCESS_RANDOM)
		rflags = GDA_DATA_MODEL_ACCESS_RANDOM;
	else
		rflags = GDA_DATA_MODEL_ACCESS_CURSOR_FORWARD;

	/* create data model */
        model = g_object_new (GDA_TYPE_WEB_RECORDSET, 
			      "prepared-stmt", ps, 
			      "model-usage", rflags, 
			      "exec-params", exec_params, NULL);
        model->priv->cnc = cnc;
	model->priv->rs_cnc = rs_cnc;
	g_object_ref (cnc);

        return GDA_DATA_MODEL (model);
}


/*
 * create tha table to store the actual data
 */
static gboolean
create_table (GdaWebRecordset *rs, GError **error)
{
#define TABLE_NAME "data"
	GString *string;
	gint i, ncols;
	gboolean retval = FALSE;

	GdaSqlBuilder *sb, *ib;

	ib = gda_sql_builder_new (GDA_SQL_STATEMENT_INSERT);
	sb = gda_sql_builder_new (GDA_SQL_STATEMENT_SELECT);

	gda_sql_builder_set_table (ib, TABLE_NAME);
	gda_sql_builder_select_add_target (sb, TABLE_NAME, NULL);

	string = g_string_new ("CREATE table " TABLE_NAME " (");
	ncols = gda_data_model_get_n_columns ((GdaDataModel*) rs);
	for (i = 0; i < ncols; i++) {
		GdaColumn *column;
		gchar *colname;

		column = gda_data_model_describe_column ((GdaDataModel*) rs, i);
		if (i > 0)
			g_string_append (string, ", ");
		colname = g_strdup_printf ("col%d", i);
		g_string_append_printf (string, "%s %s", colname,
					gda_g_type_to_string (gda_column_get_g_type (column)));

		gda_sql_builder_add_field_id (ib, gda_sql_builder_add_id (ib, 0, colname),
					   gda_sql_builder_add_param (ib, 0, colname,
								      gda_column_get_g_type (column), TRUE));
		gda_sql_builder_add_field_id (sb, gda_sql_builder_add_id (sb, 0, colname), 0);

		g_free (colname);
	}
	g_string_append (string, ")");
	/*g_print ("CREATE SQL: [%s]\n", string->str);*/

	GdaStatement *stmt;
	GdaSqlParser *parser;
	const gchar *remain;
	parser = gda_sql_parser_new ();
	stmt = gda_sql_parser_parse_string (parser, string->str, &remain, NULL);
	g_object_unref (parser);
	if (!stmt || remain) {
		g_set_error (error, GDA_SERVER_PROVIDER_ERROR,
			     GDA_SERVER_PROVIDER_INTERNAL_ERROR, "%s",
			     _("Can't create temporary table to store data from web server"));
		goto out;
	}
	if (gda_connection_statement_execute_non_select (rs->priv->rs_cnc, stmt, NULL, NULL, NULL)) {
		g_set_error (error, GDA_SERVER_PROVIDER_ERROR,
			     GDA_SERVER_PROVIDER_INTERNAL_ERROR, "%s",
			     _("Can't create temporary table to store data from web server"));
		goto out;
	}

	rs->priv->insert = gda_sql_builder_get_statement (ib, error);
	if (rs->priv->insert) 
		rs->priv->select = gda_sql_builder_get_statement (sb, NULL);
	if (! rs->priv->insert || ! rs->priv->select) {
		g_set_error (error, GDA_SERVER_PROVIDER_ERROR,
			     GDA_SERVER_PROVIDER_INTERNAL_ERROR, "%s",
			     _("Can't create temporary table to store data from web server"));
		goto out;
	}
	/*g_print ("INSERT: [%s]\n", gda_statement_to_sql (rs->priv->insert, NULL, NULL));
	  g_print ("SELECT: [%s]\n", gda_statement_to_sql (rs->priv->select, NULL, NULL));*/

	retval = TRUE;
 out:
	if (!retval) {
		if (rs->priv->insert)
			g_object_unref (rs->priv->insert);
		rs->priv->insert = NULL;
		if (rs->priv->select)
			g_object_unref (rs->priv->select);
		rs->priv->select = NULL;
	}
	g_object_unref (ib);
	g_object_unref (sb);
	if (stmt)
		g_object_unref (stmt);
	g_string_free (string, TRUE);
	return retval;
}

/**
 * gda_web_recordset_store
 *
 * Store some more rows coming from the @data_node array
 */
gboolean
gda_web_recordset_store (GdaWebRecordset *rs, xmlNodePtr data_node, GError **error)
{
	GdaSet *params, *iter;
	GdaDataModel *data;
	GSList *plist, *ilist;
	gboolean retval = FALSE;
	gint i, ncols;
	xmlNodePtr node;

	g_return_val_if_fail (GDA_IS_WEB_RECORDSET (rs), FALSE);
	g_return_val_if_fail (data_node, FALSE);
	g_return_val_if_fail (!strcmp ((gchar*) data_node->name, "gda_array"), FALSE);
	if (! rs->priv->table_created && ! create_table (rs, error))
		return FALSE;

	/* modify the @data_node tree to set the correct data types */
	ncols = gda_data_model_get_n_columns ((GdaDataModel*) rs);
	for (node = data_node->children, i = 0;
	     (i < ncols) && node;
	     node = node->next) {
		if (strcmp ((gchar*) node->name, "gda_array_field"))
			continue;
		GdaColumn *column;

		column = gda_data_model_describe_column ((GdaDataModel*) rs, i);
		i++;
		xmlSetProp (node, BAD_CAST "gdatype",
			    BAD_CAST gda_g_type_to_string (gda_column_get_g_type (column)));
	}

	/* for each row in @data_mode, insert the row in @rs->priv->rs_cnc */
	g_assert (rs->priv->insert);

	data = gda_data_model_import_new_xml_node (data_node);
	if (!data) {
		g_set_error (error, GDA_SERVER_PROVIDER_ERROR,
			     GDA_SERVER_PROVIDER_INTERNAL_ERROR, "%s",
			     _("Can't import data from web server"));
		return FALSE;
	}

	g_assert (gda_statement_get_parameters (rs->priv->insert, &params, NULL));
	iter = GDA_SET (gda_data_model_create_iter (data));
	for (plist = params->holders, ilist = iter->holders;
	     plist && ilist;
	     plist = plist->next, ilist = ilist->next) {
		GdaHolder *ph, *ih;
		ph = GDA_HOLDER (plist->data);
		ih = GDA_HOLDER (ilist->data);
		g_assert (gda_holder_set_bind (ph, ih, NULL));
	}
	g_assert (!plist && !ilist);

	for (; gda_data_model_iter_move_next ((GdaDataModelIter*) iter); ) {
		if (gda_connection_statement_execute_non_select (rs->priv->rs_cnc, rs->priv->insert, 
								 params, NULL, NULL) == -1) {
			g_set_error (error, GDA_SERVER_PROVIDER_ERROR,
			     GDA_SERVER_PROVIDER_INTERNAL_ERROR, "%s",
			     _("Can't import data from web server"));
			goto out;
		}
	}

	retval = TRUE;

 out:
	g_object_unref (data);
	g_object_unref (iter);
	g_object_unref (params);

	return retval;
}

static void
create_real_model (GdaWebRecordset *rs)
{
	if (rs->priv->real_model)
		return;
	rs->priv->real_model = gda_connection_statement_execute_select (rs->priv->rs_cnc, rs->priv->select,
									NULL, NULL);
}

/*
 * Get the number of rows in @model, if possible
 */
static gint
gda_web_recordset_fetch_nb_rows (GdaDataSelect *model)
{
	GdaWebRecordset *imodel;

	imodel = GDA_WEB_RECORDSET (model);
	if (model->advertized_nrows >= 0)
		return model->advertized_nrows;

	create_real_model (imodel);
	if (imodel->priv->real_model)
		model->advertized_nrows = gda_data_model_get_n_rows (imodel->priv->real_model);

	return model->advertized_nrows;
}

/*
 * Create a new filled #GdaRow object for the row at position @rownum, and put it into *prow.
 *
 * WARNING: @prow will NOT be NULL, but *prow may or may not be NULL:
 *  -  If *prow is NULL then a new #GdaRow object has to be created, 
 *  -  and otherwise *prow contains a #GdaRow object which has already been created 
 *     (through a call to this very function), and in this case it should not be modified
 *     but the function may return FALSE if an error occurred.
 *
 * Memory management for that new GdaRow object is left to the implementation, which
 * can use gda_data_select_take_row(). If new row objects are "given" to the GdaDataSelect implemantation
 * using that method, then this method should detect when all the data model rows have been analyzed
 * (when model->nb_stored_rows == model->advertized_nrows) and then possibly discard the API handle
 * as it won't be used anymore to fetch rows.
 */
static gboolean 
gda_web_recordset_fetch_random (GdaDataSelect *model, GdaRow **prow, gint rownum, GError **error)
{
	GdaWebRecordset *imodel;

	imodel = GDA_WEB_RECORDSET (model);

	if (*prow)
                return TRUE;

	create_real_model (imodel);
	if (imodel->priv->real_model)
		return GDA_DATA_SELECT_CLASS (G_OBJECT_GET_CLASS (imodel->priv->real_model))->fetch_random
			((GdaDataSelect*) imodel->priv->real_model, prow, rownum, error);

	return FALSE;
}

/*
 * Create a new filled #GdaRow object for the next cursor row, and put it into *prow.
 *
 * WARNING: @prow will NOT be NULL, but *prow may or may not be NULL:
 *  -  If *prow is NULL then a new #GdaRow object has to be created, 
 *  -  and otherwise *prow contains a #GdaRow object which has already been created 
 *     (through a call to this very function), and in this case it should not be modified
 *     but the function may return FALSE if an error occurred.
 *
 * Memory management for that new GdaRow object is left to the implementation, which
 * can use gda_data_select_take_row().
 */
static gboolean 
gda_web_recordset_fetch_next (GdaDataSelect *model, GdaRow **prow, gint rownum, GError **error)
{
	GdaWebRecordset *imodel;

	imodel = GDA_WEB_RECORDSET (model);

	if (*prow)
                return TRUE;

	create_real_model (imodel);
	if (imodel->priv->real_model)
		return GDA_DATA_SELECT_CLASS (G_OBJECT_GET_CLASS (imodel->priv->real_model))->fetch_next
			((GdaDataSelect*) imodel->priv->real_model, prow, rownum, error);

	return FALSE;
}

/*
 * Create a new filled #GdaRow object for the previous cursor row, and put it into *prow.
 *
 * WARNING: @prow will NOT be NULL, but *prow may or may not be NULL:
 *  -  If *prow is NULL then a new #GdaRow object has to be created, 
 *  -  and otherwise *prow contains a #GdaRow object which has already been created 
 *     (through a call to this very function), and in this case it should not be modified
 *     but the function may return FALSE if an error occurred.
 *
 * Memory management for that new GdaRow object is left to the implementation, which
 * can use gda_data_select_take_row().
 */
static gboolean 
gda_web_recordset_fetch_prev (GdaDataSelect *model, GdaRow **prow, gint rownum, GError **error)
{
	GdaWebRecordset *imodel;

	imodel = GDA_WEB_RECORDSET (model);

	if (*prow)
                return TRUE;

	create_real_model (imodel);
	if (imodel->priv->real_model)
		return GDA_DATA_SELECT_CLASS (G_OBJECT_GET_CLASS (imodel->priv->real_model))->fetch_prev
			((GdaDataSelect*) imodel->priv->real_model, prow, rownum, error);

	return FALSE;
}

/*
 * Create a new filled #GdaRow object for the cursor row at position @rownum, and put it into *prow.
 *
 * WARNING: @prow will NOT be NULL, but *prow may or may not be NULL:
 *  -  If *prow is NULL then a new #GdaRow object has to be created, 
 *  -  and otherwise *prow contains a #GdaRow object which has already been created 
 *     (through a call to this very function), and in this case it should not be modified
 *     but the function may return FALSE if an error occurred.
 *
 * Memory management for that new GdaRow object is left to the implementation, which
 * can use gda_data_select_take_row().
 */
static gboolean 
gda_web_recordset_fetch_at (GdaDataSelect *model, GdaRow **prow, gint rownum, GError **error)
{
	GdaWebRecordset *imodel;

	imodel = GDA_WEB_RECORDSET (model);

	if (*prow)
                return TRUE;

	create_real_model (imodel);
	if (imodel->priv->real_model)
		return GDA_DATA_SELECT_CLASS (G_OBJECT_GET_CLASS (imodel->priv->real_model))->fetch_at
			((GdaDataSelect*) imodel->priv->real_model, prow, rownum, error);

	return FALSE;
}

