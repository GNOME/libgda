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

#include "gda-server-recordset.h"

#define PARENT_TYPE BONOBO_X_OBJECT_TYPE

struct _GdaServerRecordsetPrivate {
	GdaServerConnection *cnc;
	GdaServerRecordsetFetchFunc fetch_func;
	GdaServerRecordsetDescribeFunc desc_func;
	glong current_pos;
};

static void gda_server_recordset_class_init (GdaServerRecordsetClass *klass);
static void gda_server_recordset_init       (GdaServerRecordset *recset,
					     GdaServerRecordsetClass *klass);
static void gda_server_recordset_finalize   (GObject *object);

static GObjectClass *parent_class = NULL;

/*
 * CORBA methods implementation
 */

static GNOME_Database_RowAttributes *
impl_Recordset_describe (PortableServer_Servant servant, CORBA_Environment *ev)
{
	GdaServerRecordset *recset = (GdaServerRecordset *) bonobo_x_object (servant);

	g_return_val_if_fail (GDA_IS_SERVER_RECORDSET (recset), NULL);
}

static CORBA_boolean
impl_Recordset_moveFirst (PortableServer_Servant servant, CORBA_Environment *ev)
{
}

static CORBA_boolean
impl_Recordset_moveNext (PortableServer_Servant servant, CORBA_Environment *ev)
{
}

static CORBA_boolean
impl_Recordset_movePrevious (PortableServer_Servant servant, CORBA_Environment *ev)
{
}

static CORBA_boolean
impl_Recordset_moveLast (PortableServer_Servant servant, CORBA_Environment *ev)
{
}

static GNOME_Database_Row *
impl_Recordset_fetch (PortableServer_Servant servant, CORBA_Environment *ev)
{
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
	recset->priv->current_pos = 0;
}

static void
gda_server_recordset_finalize (GObject *object)
{
	GdaServerRecordset *recset = (GdaServerRecordset *) object;

	g_return_if_fail (GDA_IS_SERVER_RECORDSET (recset));

	/* free memory */
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
