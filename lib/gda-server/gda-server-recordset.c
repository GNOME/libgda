/* GDA Server Library
 * Copyright (C) 2000 Rodrigo Moya
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

#include "gda-server.h"
#include "gda-server-private.h"

#define PARENT_TYPE BONOBO_X_OBJECT_TYPE

static void gda_server_recordset_init       (GdaServerRecordset * recset, GdaServerRecordsetClass *klass);
static void gda_server_recordset_class_init (GdaServerRecordsetClass * klass);
static void gda_server_recordset_finalize   (GObject *object);

/*
 * Stub implementations
 */
CORBA_long
impl_Recordset_getRowCount (PortableServer_Servant servant,
			    CORBA_Environment * ev)
{
	return -1;
}

CORBA_long
impl_Recordset_close (PortableServer_Servant servant,
		      CORBA_Environment * ev)
{
	GdaServerRecordset *recset = (GdaServerRecordset *) bonobo_x_object (servant);

	g_return_val_if_fail (GDA_IS_SERVER_RECORDSET (recset), -1);
	return gda_server_recordset_close (recset);
}

CORBA_long
impl_Recordset_moveFirst (PortableServer_Servant servant,
			  CORBA_Environment * ev)
{
	gda_log_error (_("%s not implemented"), __PRETTY_FUNCTION__);
	return 0;
}

CORBA_long
impl_Recordset_moveLast (PortableServer_Servant servant,
			 CORBA_Environment * ev)
{
	gda_log_error (_("%s not implemented"), __PRETTY_FUNCTION__);
	return 0;
}

CORBA_long
impl_Recordset_moveNext (PortableServer_Servant servant,
			 CORBA_Environment *ev)
{
	CORBA_long ret;
	GdaServerRecordset *recset = (GdaServerRecordset *) bonobo_x_object (servant);

	g_return_val_if_fail (GDA_IS_SERVER_RECORDSET (recset), -1);

	ret = gda_server_recordset_move_next (recset);
	if (ret != 0)
		gda_error_list_to_exception (recset->cnc->errors, ev);

	return ret;
}

CORBA_long
impl_Recordset_movePrev (PortableServer_Servant servant,
			 CORBA_Environment *ev)
{
	CORBA_long ret;
	GdaServerRecordset *recset = (GdaServerRecordset *) bonobo_x_object (servant);

	g_return_val_if_fail (GDA_IS_SERVER_RECORDSET (recset), -1);

	ret = gda_server_recordset_move_prev (recset);
	if (ret != 0)
		gda_error_list_to_exception (recset->cnc->errors, ev);

	return ret;
}

CORBA_long
impl_Recordset_reQuery (PortableServer_Servant servant,
			CORBA_Environment * ev)
{
	gda_log_error (_("%s not implemented"), __PRETTY_FUNCTION__);
	return 0;
}

CORBA_boolean
impl_Recordset_supports (PortableServer_Servant servant,
			 GNOME_Database_Option what,
			 CORBA_Environment * ev)
{
	gda_log_error (_("%s not implemented"), __PRETTY_FUNCTION__);
	return 0;
}

GNOME_Database_Recordset_Chunk *
impl_Recordset_fetch (PortableServer_Servant servant,
		      CORBA_long count,
		      CORBA_Environment * ev)
{
	GdaServerRecordset *rs = GDA_SERVER_RECORDSET (bonobo_x_object (servant));
	GNOME_Database_Row *row;
	GNOME_Database_Recordset_Chunk *chunk;
	GNOME_Database_Field *field;
	GdaField *server_field;
	GList *ptr;
	gint rowidx = 0;
	gint colidx;
	gint rc;
	GList *tmp_rows;
	gint rowlength;

	if (!GDA_IS_SERVER_RECORDSET (rs))
		return CORBA_OBJECT_NIL;

	tmp_rows = 0;
	rowidx = 0;
	rowlength = g_list_length (rs->fields);

	chunk = GNOME_Database_Recordset_Chunk__alloc ();
	chunk->_buffer = 0;
	chunk->_length = 0;

	if (0 == rowlength)
		return (chunk);

	do {
		row = g_new0 (GNOME_Database_Row, 1);
		row->_buffer = CORBA_sequence_GNOME_Database_Field_allocbuf (rowlength);
		row->_length = rowlength;
		ptr = rs->fields;

		colidx = 0;

		while (ptr) {
			server_field = ptr->data;
			gda_field_copy_to_corba (server_field, &row->_buffer[colidx]);
			ptr = g_list_next (ptr);
			colidx++;
		}

		rc = gda_server_recordset_move_next (rs);

		if (rc != 0) {
			CORBA_free (row->_buffer);
			g_free (row);
			break;
		}
		tmp_rows = g_list_append (tmp_rows, row);

		ptr = rs->fields;
		field = &row->_buffer[0];
		colidx = 0;

		while (ptr) {
			server_field = ptr->data;
			gda_field_copy_to_corba (server_field, field);

			field = &row->_buffer[++colidx];
		}
		rowidx++;
	} while (rowidx < count);

	if (rc < 0) {
		gda_error_list_to_exception (rs->cnc->errors, ev);
		return chunk;
	}

	if (rowidx)
		chunk->_buffer = CORBA_sequence_GNOME_Database_Row_allocbuf (rowidx);

	chunk->_length = rowidx;
	for (ptr = tmp_rows, count = 0; count < rowidx;
	     ptr = g_list_next (ptr), count++) {
		row = ptr->data;
		chunk->_buffer[count]._length = row->_length;
		chunk->_buffer[count]._buffer = row->_buffer;
		g_free (row);
	}
	g_list_free (tmp_rows);

	return chunk;
}

GNOME_Database_RowAttributes *
impl_Recordset_describe (PortableServer_Servant servant,
			 CORBA_Environment * ev)
{
	GdaServerRecordset *rs = (GdaServerRecordset *) bonobo_x_object (servant);
	GNOME_Database_RowAttributes *rc;
	GList *ptr;
	GdaField *server_field;
	GNOME_Database_FieldAttributes *attrs;
	gint idx;

	g_return_val_if_fail (GDA_IS_SERVER_RECORDSET (rs), NULL);

	rc = GNOME_Database_RowAttributes__alloc ();
	rc->_length = g_list_length (rs->fields);
	rc->_buffer = CORBA_sequence_GNOME_Database_FieldAttributes_allocbuf (rc->_length);
	rc->_maximum = 0;

	idx = 0;
	ptr = rs->fields;
	while (ptr) {
		attrs = &rc->_buffer[idx];

		server_field = GDA_FIELD (ptr->data);
		gda_field_copy_to_corba_attributes (server_field, attrs);
		
		ptr = g_list_next (ptr);
		idx++;
	}

	return rc;
}

/*
 * GdaServerRecordset class implementation
 */

BONOBO_TYPE_FUNC_FULL (GdaServerRecordset,
		       GNOME_Database_Recordset,
		       PARENT_TYPE,
		       gda_server_recordset);

static void
gda_server_recordset_class_init (GdaServerRecordsetClass * klass)
{
	POA_GNOME_Database_Recordset__epv *epv;
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = gda_server_recordset_finalize;

	/* set the epv */
	epv = &klass->epv;
	epv->getRowCount = impl_Recordset_getRowCount;
	epv->close = impl_Recordset_close;
	epv->moveFirst = impl_Recordset_moveFirst;
	epv->moveLast = impl_Recordset_moveLast;
	epv->moveNext = impl_Recordset_moveNext;
	epv->movePrev = impl_Recordset_movePrev;
	epv->reQuery = impl_Recordset_reQuery;
	epv->supports = impl_Recordset_supports;
	epv->fetch = impl_Recordset_fetch;
	epv->describe = impl_Recordset_describe;
}

static void
gda_server_recordset_init (GdaServerRecordset * recset, GdaServerRecordsetClass *klass)
{
	recset->cnc = NULL;
	recset->fields = NULL;
	recset->position = -1;
	recset->at_begin = FALSE;
	recset->at_end = FALSE;
	recset->user_data = NULL;
}

static void
gda_server_recordset_finalize (GObject *object)
{
	GObjectClass *parent_class;
	GdaServerRecordset *recset = (GdaServerRecordset *) object;

	g_return_if_fail (GDA_IS_SERVER_RECORDSET (recset));

	if ((recset->cnc != NULL) &&
	    (recset->cnc->server_impl != NULL) &&
	    (recset->cnc->server_impl->functions.recordset_free != NULL))
		recset->cnc->server_impl->functions.recordset_free (recset);

	g_list_foreach (recset->fields, (GFunc) gda_field_free, NULL);

	parent_class = g_type_class_peek (PARENT_TYPE);
	if (parent_class && parent_class->finalize)
		parent_class->finalize (object);
}

/**
 * gda_server_recordset_new
 */
GdaServerRecordset *
gda_server_recordset_new (GdaServerConnection * cnc)
{
	GdaServerRecordset *recset;

	g_return_val_if_fail (GDA_IS_SERVER_CONNECTION (cnc), NULL);

	recset = GDA_SERVER_RECORDSET (g_object_new (GDA_TYPE_SERVER_RECORDSET, NULL));
	recset->cnc = cnc;

	if ((recset->cnc->server_impl != NULL) &&
	    (recset->cnc->server_impl->functions.recordset_new != NULL))
		recset->cnc->server_impl->functions.recordset_new (recset);

	return recset;
}

/**
 * gda_server_recordset_get_connection
 */
GdaServerConnection *
gda_server_recordset_get_connection (GdaServerRecordset * recset)
{
	g_return_val_if_fail (GDA_IS_SERVER_RECORDSET (recset), NULL);
	return recset->cnc;
}

/**
 * gda_server_recordset_add_field
 */
void
gda_server_recordset_add_field (GdaServerRecordset *recset,
				GdaField *field)
{
	g_return_if_fail (GDA_IS_SERVER_RECORDSET (recset));
	g_return_if_fail (field != NULL);

	recset->fields = g_list_append (recset->fields, (gpointer) field);
}

/**
 * gda_server_recordset_get_fields
 */
GList *
gda_server_recordset_get_fields (GdaServerRecordset * recset)
{
	g_return_val_if_fail (GDA_IS_SERVER_RECORDSET (recset), NULL);
	return recset->fields;
}

/**
 * gda_server_recordset_is_at_begin
 */
gboolean
gda_server_recordset_is_at_begin (GdaServerRecordset * recset)
{
	g_return_val_if_fail (recset != NULL, FALSE);
	return recset->at_begin;
}

/**
 * gda_server_recordset_set_at_begin
 */
void
gda_server_recordset_set_at_begin (GdaServerRecordset * recset,
				   gboolean at_begin)
{
	g_return_if_fail (recset != NULL);
	recset->at_begin = at_begin;
}

/**
 * gda_server_recordset_is_at_end
 */
gboolean
gda_server_recordset_is_at_end (GdaServerRecordset * recset)
{
	g_return_val_if_fail (recset != NULL, FALSE);
	return recset->at_end;
}

/**
 * gda_server_recordset_set_at_end
 */
void
gda_server_recordset_set_at_end (GdaServerRecordset * recset, gboolean at_end)
{
	g_return_if_fail (recset != NULL);
	recset->at_end = at_end;
}

/**
 * gda_server_recordset_get_user_data
 */
gpointer
gda_server_recordset_get_user_data (GdaServerRecordset * recset)
{
	g_return_val_if_fail (recset != NULL, NULL);
	return recset->user_data;
}

/**
 * gda_server_recordset_set_user_data
 */
void
gda_server_recordset_set_user_data (GdaServerRecordset * recset,
				    gpointer user_data)
{
	g_return_if_fail (recset != NULL);
	recset->user_data = user_data;
}

/**
 * gda_server_recordset_free
 */
void
gda_server_recordset_free (GdaServerRecordset * recset)
{
	bonobo_object_unref (BONOBO_OBJECT (recset));
}

/**
 * gda_server_recordset_move_next
 */
gint
gda_server_recordset_move_next (GdaServerRecordset * recset)
{
	g_return_val_if_fail (recset != NULL, -1);
	g_return_val_if_fail (recset->cnc != NULL, -1);
	g_return_val_if_fail (recset->cnc->server_impl != NULL, -1);
	g_return_val_if_fail (recset->cnc->server_impl->functions.recordset_move_next != NULL, -1);

	return recset->cnc->server_impl->functions.recordset_move_next (recset);
}

/**
 * gda_server_recordset_move_prev
 */
gint
gda_server_recordset_move_prev (GdaServerRecordset * recset)
{
	g_return_val_if_fail (recset != NULL, -1);
	g_return_val_if_fail (recset->cnc != NULL, -1);
	g_return_val_if_fail (recset->cnc->server_impl != NULL, -1);
	g_return_val_if_fail (recset->cnc->server_impl->functions.recordset_move_prev != NULL, -1);

	return recset->cnc->server_impl->functions.recordset_move_prev (recset);
}

/**
 * gda_server_recordset_close
 */
gint
gda_server_recordset_close (GdaServerRecordset * recset)
{
	g_return_val_if_fail (recset != NULL, -1);
	g_return_val_if_fail (recset->cnc != NULL, -1);
	g_return_val_if_fail (recset->cnc->server_impl != NULL, -1);
	g_return_val_if_fail (recset->cnc->server_impl->functions.recordset_close != NULL, -1);

	return recset->cnc->server_impl->functions.recordset_close (recset);
}
