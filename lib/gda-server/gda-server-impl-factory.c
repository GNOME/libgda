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
#include "gda-server-impl.h"
#include "gda-server-impl-factory.h"

/*
 * epv structures
 */
static PortableServer_ServantBase__epv impl_GDA_ConnectionFactory_base_epv = {
	NULL,                        /* _private data */
	(gpointer) & impl_GDA_ConnectionFactory__destroy,    /* finalize routine */
	NULL,                        /* default_POA routine */
};

static POA_GDA_ConnectionFactory__epv impl_GDA_ConnectionFactory_epv = {
	NULL,                        /* _private */
	(gpointer) & impl_GDA_ConnectionFactory_create_connection,
};

static POA_GNOME_ObjectFactory__epv impl_GDA_ConnectionFactory_GNOME_ObjectFactory_epv = {
	NULL,                        /* _private */
	(gpointer) & impl_GDA_ConnectionFactory_manufactures,
	(gpointer) & impl_GDA_ConnectionFactory_create_object,
};

/*
 * vepv structures
 */
static POA_GDA_ConnectionFactory__vepv impl_GDA_ConnectionFactory_vepv = {
	&impl_GDA_ConnectionFactory_base_epv,
	&impl_GDA_ConnectionFactory_GNOME_ObjectFactory_epv,
	&impl_GDA_ConnectionFactory_epv,
};

/*
 * Stub implementations
 */
GDA_ConnectionFactory
impl_GDA_ConnectionFactory__create(PortableServer_POA poa, CORBA_Environment * ev) {
	GDA_ConnectionFactory retval;
	impl_POA_GDA_ConnectionFactory *newservant;
	PortableServer_ObjectId *objid;
	
	newservant = g_new0(impl_POA_GDA_ConnectionFactory, 1);
	newservant->servant.vepv = &impl_GDA_ConnectionFactory_vepv;
	newservant->poa = poa;
	POA_GDA_ConnectionFactory__init((PortableServer_Servant) newservant, ev);
	objid = PortableServer_POA_activate_object(poa, newservant, ev);
	CORBA_free(objid);
	retval = PortableServer_POA_servant_to_reference(poa, newservant, ev);
	
	return retval;
}

/* You shouldn't call this routine directly without first deactivating the servant... */
void
impl_GDA_ConnectionFactory__destroy (impl_POA_GDA_ConnectionFactory * servant, CORBA_Environment * ev) {
	PortableServer_ObjectId* objid;
	
	objid = PortableServer_POA_servant_to_id(servant->poa, servant, ev);
	PortableServer_POA_deactivate_object(servant->poa, objid, ev);
	CORBA_free(objid);
	
	POA_GDA_ConnectionFactory__fini((PortableServer_Servant) servant, ev);
	g_free((gpointer) servant);
}

CORBA_Object
impl_GDA_ConnectionFactory_create_connection (impl_POA_GDA_ConnectionFactory * servant,
                                              CORBA_char * goad_id,
                                              CORBA_Environment * ev) {
	GDA_Connection new_connection;
	
	gda_log_message("%s: called\n", __PRETTY_FUNCTION__);
	new_connection = impl_GDA_Connection__create(servant->poa, goad_id, ev);
	gda_server_impl_exception(ev);
	gda_log_message("%s: left\n", __PRETTY_FUNCTION__);
	return new_connection;
}

CORBA_boolean
impl_GDA_ConnectionFactory_manufactures (impl_POA_GDA_ConnectionFactory *servant,
                                         CORBA_char *obj_id,
                                         CORBA_Environment *ev) {
	return FALSE;
}

CORBA_Object
impl_GDA_ConnectionFactory_create_object (impl_POA_GDA_ConnectionFactory * servant,
                                          CORBA_char * goad_id,
                                          GNOME_stringlist * params,
                                          CORBA_Environment * ev) {
	return impl_GDA_ConnectionFactory_create_connection(servant, goad_id, ev);
}
