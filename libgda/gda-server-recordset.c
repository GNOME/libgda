/* GDA server library
 * Copyright (C) 1998-2001 The Free Software Foundation
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

#include <bonobo/bonobo-exception.h>
#include <bonobo/bonobo-i18n.h>
#include <libgda/gda-server-recordset.h>

#define PARENT_TYPE BONOBO_X_OBJECT_TYPE

struct _GdaServerRecordsetPrivate {
	GdaServerConnection *cnc;
	GdaServerRecordsetFetchFunc fetch_func;
	GdaServerRecordsetDescribeFunc desc_func;

	glong current_pos;
	gboolean is_eof;
	GPtrArray *rows;
};

static void gda_server_recordset_class_init (GdaServerRecordsetClass *klass);
static void gda_server_recordset_init       (GdaServerRecordset *recset,
					     GdaServerRecordsetClass *klass);
static void gda_server_recordset_finalize   (GObject *object);

static GObjectClass *parent_class = NULL;

/*
 * Private functions
 */

static GdaRow *
get_row (GdaServerRecordset *recset, glong pos)
{
	gint i;
	GdaRow *row;

	g_return_val_if_fail (GDA_IS_SERVER_RECORDSET (recset), NULL);
	g_return_val_if_fail (pos >= 0, NULL);

	/* if we've got all the rows from the provider, don't fetch */
	if (recset->priv->is_eof && pos >= recset->priv->rows->len) {
		recset->priv->is_eof = TRUE;
		return NULL;
	}

	if (pos < recset->priv->rows->len) {
		/* we've got the row already */
		return g_ptr_array_index (recset->priv->rows, pos);
	}

	/* fetch all rows until the given one */
	for (i = recset->priv->rows->len - 1; i <= pos; i++) {
		if (recset->priv->fetch_func) {
			if (i < 0)
				i = 0;

			row = recset->priv->fetch_func (recset, i);
			if (!row) {
				recset->priv->is_eof = TRUE;
				return NULL;
			}
			g_ptr_array_add (recset->priv->rows, row);
		}
		else
			g_ptr_array_add (recset->priv->rows, gda_row_new (0));
	}

	return g_ptr_array_index (recset->priv->rows, pos);
}

/*
 * CORBA methods implementation
 */

static GNOME_Database_RowAttributes *
impl_Recordset_describe (PortableServer_Servant servant, CORBA_Environment *ev)
{
	GdaRowAttributes *attrs;
	GdaServerRecordset *recset = (GdaServerRecordset *) bonobo_x_object (servant);

	bonobo_return_val_if_fail (GDA_IS_SERVER_RECORDSET (recset), NULL, ev);

	if (recset->priv->desc_func)
		return recset->priv->desc_func (recset);

	/* return an empty GdaRowAttributes */
	attrs = gda_row_attributes_new (0);

	return attrs;
}

static CORBA_long
impl_Recordset_getRowCount (PortableServer_Servant servant, CORBA_Environment *ev)
{
	glong i;
	GdaRow *row;
	GdaServerRecordset *recset = (GdaServerRecordset *) bonobo_x_object (servant);

	bonobo_return_val_if_fail (GDA_IS_SERVER_RECORDSET (recset), 0, ev);

	if (recset->priv->is_eof)
		return recset->priv->rows->len;

	/* we don't have all rows, so get them */
	i = 0;
	do {
		row = get_row (recset, i);
		if (row)
			i++;
	} while (row != NULL);

	return i;
}

static CORBA_boolean
impl_Recordset_moveFirst (PortableServer_Servant servant, CORBA_Environment *ev)
{
	GdaServerRecordset *recset = (GdaServerRecordset *) bonobo_x_object (servant);

	bonobo_return_val_if_fail (GDA_IS_SERVER_RECORDSET (recset), FALSE, ev);

	recset->priv->current_pos = 0;
	return TRUE;
}

static CORBA_boolean
impl_Recordset_moveNext (PortableServer_Servant servant, CORBA_Environment *ev)
{
	GdaServerRecordset *recset = (GdaServerRecordset *) bonobo_x_object (servant);

	bonobo_return_val_if_fail (GDA_IS_SERVER_RECORDSET (recset), FALSE, ev);

	if (get_row (recset, recset->priv->current_pos + 1)) {
		recset->priv->current_pos++;
		return TRUE;
	}

	return FALSE;
}

static CORBA_boolean
impl_Recordset_movePrevious (PortableServer_Servant servant, CORBA_Environment *ev)
{
	GdaServerRecordset *recset = (GdaServerRecordset *) bonobo_x_object (servant);

	bonobo_return_val_if_fail (GDA_IS_SERVER_RECORDSET (recset), FALSE, ev);

	if (recset->priv->current_pos > 0) {
		recset->priv->current_pos--;
		return TRUE;
	}

	return FALSE;
}

static CORBA_boolean
impl_Recordset_moveLast (PortableServer_Servant servant, CORBA_Environment *ev)
{
	GdaServerRecordset *recset = (GdaServerRecordset *) bonobo_x_object (servant);

	bonobo_return_val_if_fail (GDA_IS_SERVER_RECORDSET (recset), FALSE, ev);

	while (get_row (recset, recset->priv->current_pos))
		recset->priv->current_pos++;

	return recset->priv->is_eof;
}

static GNOME_Database_Row *
impl_Recordset_fetch (PortableServer_Servant servant, CORBA_Environment *ev)
{
	GdaRow *row;
	GdaServerRecordset *recset = (GdaServerRecordset *) bonobo_x_object (servant);

	bonobo_return_val_if_fail (GDA_IS_SERVER_RECORDSET (recset), NULL, ev);

	if (recset->priv->fetch_func) {
		row = get_row (recset, recset->priv->current_pos);
		if (!row)
			return NULL;
	}
	else {
		gda_server_connection_add_error_string (
			gda_server_recordset_get_connection (recset),
			_("No implementation for the fetch() method found"));
		row = NULL;
	}

	/* duplicate the object to be returned */

	return row;
}

/*
 * GdaServerRecordset class implementation
 */

static void
gda_server_recordset_class_init (GdaServerRecordsetClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	POA_GNOME_Database_Recordset__epv *epv;

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = gda_server_recordset_finalize;

	/* set the epv */
	epv = &klass->epv;
	epv->describe = impl_Recordset_describe;
	epv->getRowCount = impl_Recordset_getRowCount;
	epv->moveFirst = impl_Recordset_moveFirst;
	epv->moveNext = impl_Recordset_moveNext;
	epv->movePrevious = impl_Recordset_movePrevious;
	epv->moveLast = impl_Recordset_moveLast;
	epv->fetch = impl_Recordset_fetch;
}

static void
gda_server_recordset_init (GdaServerRecordset *recset, GdaServerRecordsetClass *klass)
{
	g_return_if_fail (GDA_IS_SERVER_RECORDSET (recset));

	recset->priv = g_new0 (GdaServerRecordsetPrivate, 1);
	recset->priv->cnc = NULL;
	recset->priv->fetch_func = NULL;
	recset->priv->desc_func = NULL;
	recset->priv->current_pos = 0;
	recset->priv->is_eof = FALSE;
	recset->priv->rows = g_ptr_array_new ();
}

static void
gda_server_recordset_finalize (GObject *object)
{
	gint i;
	GdaServerRecordset *recset = (GdaServerRecordset *) object;

	g_return_if_fail (GDA_IS_SERVER_RECORDSET (recset));

	/* free memory */
	for (i = 0; i < recset->priv->rows->len; i++) {
		GdaRow *row;

		row = g_ptr_array_index (recset->priv->rows, i);
		g_ptr_array_remove_index (recset->priv->rows, i);
		gda_row_free (row);
	}
	g_ptr_array_free (recset->priv->rows, TRUE);
	
	g_free (recset->priv);
	recset->priv = NULL;

	/* chain to parent class */
	parent_class->finalize (object);
}

BONOBO_X_TYPE_FUNC_FULL (GdaServerRecordset,
			 GNOME_Database_Recordset,
			 PARENT_TYPE,
			 gda_server_recordset)

/**
 * gda_server_recordset_new
 */
GdaServerRecordset *
gda_server_recordset_new (GdaServerConnection *cnc,
			  GdaServerRecordsetFetchFunc fetch_func,
			  GdaServerRecordsetDescribeFunc desc_func)
{
	GdaServerRecordset *recset;

	g_return_val_if_fail (GDA_IS_SERVER_CONNECTION (cnc), NULL);

	recset = g_object_new (GDA_TYPE_SERVER_RECORDSET, NULL);
	recset->priv->cnc = cnc;
	recset->priv->fetch_func = fetch_func;
	recset->priv->desc_func = desc_func;

	return recset;
}

/**
 * gda_server_recordset_get_connection
 */
GdaServerConnection *
gda_server_recordset_get_connection (GdaServerRecordset *recset)
{
	g_return_val_if_fail (GDA_IS_SERVER_RECORDSET (recset), NULL);
	return recset->priv->cnc;
}

/**
 * gda_server_recordset_set_connection
 */
void
gda_server_recordset_set_connection (GdaServerRecordset *recset, GdaServerConnection *cnc)
{
	g_return_if_fail (GDA_IS_SERVER_RECORDSET (recset));
	g_return_if_fail (GDA_IS_SERVER_CONNECTION (cnc));

	recset->priv->cnc = cnc;
}

/**
 * gda_server_recordset_set_fetch_func
 */
void
gda_server_recordset_set_fetch_func (GdaServerRecordset *recset,
				     GdaServerRecordsetFetchFunc func)
{
	g_return_if_fail (GDA_IS_SERVER_RECORDSET (recset));
	recset->priv->fetch_func = func;
}

/**
 * gda_server_recordset_set_describe_func
 */
void
gda_server_recordset_set_describe_func (GdaServerRecordset *recset,
					GdaServerRecordsetDescribeFunc func)
{
	g_return_if_fail (GDA_IS_SERVER_RECORDSET (recset));
	recset->priv->desc_func = func;
}
