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

#include "config.h"
#include "gda-server.h"
#include "gda-server-private.h"

static void gda_server_recordset_init (GdaServerRecordset * recset);
static void gda_server_recordset_class_init (GdaServerRecordsetClass * klass);
static void gda_server_recordset_destroy (GtkObject * object);

/*
 * Stub implementations
 */
CORBA_long
impl_GDA_Recordset_getRowCount (PortableServer_Servant servant,
				CORBA_Environment * ev)
{
	return -1;
}

CORBA_long
impl_GDA_Recordset_close (PortableServer_Servant servant,
			  CORBA_Environment * ev)
{
	GdaServerRecordset *recset =
		(GdaServerRecordset *) bonobo_x_object (servant);

	g_return_val_if_fail (GDA_IS_SERVER_RECORDSET (recset), -1);
	return gda_server_recordset_close (recset);
}

CORBA_long
impl_GDA_Recordset_move (PortableServer_Servant servant,
			 CORBA_long count,
			 CORBA_long bookmark, CORBA_Environment * ev)
{
	gda_log_error (_("%s not implemented"), __PRETTY_FUNCTION__);
	return 0;
}

CORBA_long
impl_GDA_Recordset_moveFirst (PortableServer_Servant servant,
			      CORBA_Environment * ev)
{
	gda_log_error (_("%s not implemented"), __PRETTY_FUNCTION__);
	return 0;
}

CORBA_long
impl_GDA_Recordset_moveLast (PortableServer_Servant servant,
			     CORBA_Environment * ev)
{
	gda_log_error (_("%s not implemented"), __PRETTY_FUNCTION__);
	return 0;
}

CORBA_long
impl_GDA_Recordset_reQuery (PortableServer_Servant servant,
			    CORBA_Environment * ev)
{
	gda_log_error (_("%s not implemented"), __PRETTY_FUNCTION__);
	return 0;
}

CORBA_boolean
impl_GDA_Recordset_supports (PortableServer_Servant servant,
			     GDA_Option what, CORBA_Environment * ev)
{
	gda_log_error (_("%s not implemented"), __PRETTY_FUNCTION__);
	return 0;
}

GDA_Recordset_Chunk *
impl_GDA_Recordset_fetch (PortableServer_Servant servant,
			  CORBA_long count, CORBA_Environment * ev)
{
	GdaServerRecordset *rs =
		GDA_SERVER_RECORDSET (bonobo_x_object (servant));
	GDA_Row *row;
	GDA_Recordset_Chunk *chunk;
	GDA_Field *field;
	GdaServerField *server_field;
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

	chunk = GDA_Recordset_Chunk__alloc ();
	chunk->_buffer = 0;
	chunk->_length = 0;

	if (0 == rowlength)
		return (chunk);

	do {
		row = g_new0 (GDA_Row, 1);
		row->_buffer = CORBA_sequence_GDA_Field_allocbuf (rowlength);
		row->_length = rowlength;
		ptr = rs->fields;

		colidx = 0;

		while (ptr) {
			server_field = ptr->data;
			server_field->value =
				&row->_buffer[colidx].realValue._u.v;
			row->_buffer[colidx].realValue._d = 1;
			row->_buffer[colidx].shadowValue._d = 1;
			row->_buffer[colidx].originalValue._d = 1;
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

			field->actualSize = server_field->actual_length;
			field->realValue._d =
				server_field->actual_length == 0;
			field->shadowValue._d = 1;
			field->originalValue._d = 1;
			ptr = g_list_next (ptr);
			field = &row->_buffer[++colidx];
		}
		rowidx++;
	} while (rowidx < count);

	if (rc < 0) {
		gda_error_list_to_exception (rs->cnc->errors, ev);
		return chunk;
	}

	if (rowidx)
		chunk->_buffer = CORBA_sequence_GDA_Row_allocbuf (rowidx);

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

GDA_RowAttributes *
impl_GDA_Recordset_describe (PortableServer_Servant servant,
			     CORBA_Environment * ev)
{
	GdaServerRecordset *rs =
		(GdaServerRecordset *) bonobo_x_object (servant);
	GDA_RowAttributes *rc;
	GList *ptr;
	GdaServerField *server_field;
	GDA_FieldAttributes *field;
	gint idx;

	g_return_val_if_fail (GDA_IS_SERVER_RECORDSET (rs), NULL);

	rc = GDA_RowAttributes__alloc ();
	rc->_length = g_list_length (rs->fields);
	rc->_buffer =
		CORBA_sequence_GDA_FieldAttributes_allocbuf (rc->_length);
	rc->_maximum = 0;

	idx = 0;
	ptr = rs->fields;
	while (ptr) {
		field = &rc->_buffer[idx];

		server_field = (GdaServerField *) (ptr->data);

		field->name = CORBA_string_dup (server_field->name);
		field->definedSize = server_field->defined_length;
		field->scale = server_field->num_scale;
		field->gdaType =
			gda_server_connection_get_gda_type (rs->cnc,
							    server_field->
							    sql_type);
		field->nativeType = server_field->sql_type;
		field->cType =
			gda_server_connection_get_c_type (rs->cnc,
							  field->gdaType);
		ptr = g_list_next (ptr);
		idx++;
	}

	return rc;
}

/*
 * GdaServerRecordset class implementation
 */
static void
gda_server_recordset_class_init (GdaServerRecordsetClass * klass)
{
	POA_GDA_Recordset__epv *epv;
	GtkObjectClass *object_class = GTK_OBJECT_CLASS (klass);

	object_class->destroy = gda_server_recordset_destroy;

	/* set the epv */
	epv = &klass->epv;
	epv->getRowCount = impl_GDA_Recordset_getRowCount;
	epv->close = impl_GDA_Recordset_close;
	epv->move = impl_GDA_Recordset_move;
	epv->moveFirst = impl_GDA_Recordset_moveFirst;
	epv->moveLast = impl_GDA_Recordset_moveLast;
	epv->reQuery = impl_GDA_Recordset_reQuery;
	epv->supports = impl_GDA_Recordset_supports;
	epv->fetch = impl_GDA_Recordset_fetch;
	epv->describe = impl_GDA_Recordset_describe;
}

static void
gda_server_recordset_init (GdaServerRecordset * recset)
{
	recset->cnc = NULL;
	recset->fields = NULL;
	recset->position = -1;
	recset->at_begin = FALSE;
	recset->at_end = FALSE;
	recset->user_data = NULL;
}

static void
gda_server_recordset_destroy (GtkObject * object)
{
	GtkObjectClass *parent_class;
	GdaServerRecordset *recset = (GdaServerRecordset *) object;

	g_return_if_fail (GDA_IS_SERVER_RECORDSET (recset));

	if ((recset->cnc != NULL) &&
	    (recset->cnc->server_impl != NULL) &&
	    (recset->cnc->server_impl->functions.recordset_free != NULL))
		recset->cnc->server_impl->functions.recordset_free (recset);

	g_list_foreach (recset->fields, (GFunc) gda_server_field_free, NULL);

	parent_class = gtk_type_class (BONOBO_X_OBJECT_TYPE);
	if (parent_class && parent_class->destroy)
		parent_class->destroy (object);
}

GtkType
gda_server_recordset_get_type (void)
{
	static GtkType type = 0;

	if (!type) {
		GtkTypeInfo info = {
			"GdaServerRecordset",
			sizeof (GdaServerRecordset),
			sizeof (GdaServerRecordsetClass),
			(GtkClassInitFunc) gda_server_recordset_class_init,
			(GtkObjectInitFunc) gda_server_recordset_init,
			(GtkArgSetFunc) NULL,
			(GtkArgSetFunc) NULL
		};
		type = bonobo_x_type_unique (BONOBO_X_OBJECT_TYPE,
					     POA_GDA_Recordset__init, NULL,
					     GTK_STRUCT_OFFSET
					     (GdaServerRecordsetClass, epv),
					     &info);
	}
	return type;
}

/**
 * gda_server_recordset_new
 */
GdaServerRecordset *
gda_server_recordset_new (GdaServerConnection * cnc)
{
	GdaServerRecordset *recset;

	g_return_val_if_fail (GDA_IS_SERVER_CONNECTION (cnc), NULL);

	recset = GDA_SERVER_RECORDSET (gtk_type_new
				       (gda_server_recordset_get_type ()));
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
gda_server_recordset_add_field (GdaServerRecordset * recset,
				GdaServerField * field)
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
	g_return_val_if_fail (recset->cnc->server_impl->functions.
			      recordset_move_next != NULL, -1);

	return recset->cnc->server_impl->functions.
		recordset_move_next (recset);
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
	g_return_val_if_fail (recset->cnc->server_impl->functions.
			      recordset_move_prev != NULL, -1);

	return recset->cnc->server_impl->functions.
		recordset_move_prev (recset);
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
	g_return_val_if_fail (recset->cnc->server_impl->functions.
			      recordset_close != NULL, -1);

	return recset->cnc->server_impl->functions.recordset_close (recset);
}
