/* GDA Report Engine
 * Copyright (C) 2000 Rodrigo Moya
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <gda-report-server.h>

static PortableServer_ServantBase__epv impl_GDA_ReportEngine_base_epv = {
	NULL, /* _private data */
	(gpointer) &impl_GDA_ReportEngine__destroy, /* finalize routine */
	NULL, /* default_POA routine */
};
static POA_GDA_ReportEngine__epv impl_GDA_ReportEngine_epv = {
	NULL,                        /* _private */
	(gpointer) & impl_GDA_ReportEngine__get_conv_list,
	(gpointer) & impl_GDA_ReportEngine_queryReports,
	(gpointer) & impl_GDA_ReportEngine_openReport,
	(gpointer) & impl_GDA_ReportEngine_addReport,
	(gpointer) & impl_GDA_ReportEngine_removeReport,
	(gpointer) & impl_GDA_ReportEngine_registerConverter,
	(gpointer) & impl_GDA_ReportEngine_unregisterConverter,
	(gpointer) & impl_GDA_ReportEngine_findConverter,
	(gpointer) & impl_GDA_ReportEngine_createStream,
};
static POA_GDA_ReportEngine__vepv impl_GDA_ReportEngine_vepv = {
	&impl_GDA_ReportEngine_base_epv,
	&impl_GDA_ReportEngine_epv,
};

/*
 * Stub implementations
 */
GDA_ReportEngine
impl_GDA_ReportEngine__create (PortableServer_POA poa, CORBA_Environment * ev) {
	GDA_ReportEngine retval;
	impl_POA_GDA_ReportEngine *newservant;
	PortableServer_ObjectId *objid;
	
	newservant = g_new0(impl_POA_GDA_ReportEngine, 1);
	newservant->servant.vepv = &impl_GDA_ReportEngine_vepv;
	newservant->poa = poa;
	POA_GDA_ReportEngine__init((PortableServer_Servant) newservant, ev);
	objid = PortableServer_POA_activate_object(poa, newservant, ev);
	CORBA_free(objid);
	retval = PortableServer_POA_servant_to_reference(poa, newservant, ev);
	
	return retval;
}

void
impl_GDA_ReportEngine__destroy (impl_POA_GDA_ReportEngine *servant,
                                CORBA_Environment * ev) {
	PortableServer_ObjectId *objid;
	
	objid = PortableServer_POA_servant_to_id(servant->poa, servant, ev);
	PortableServer_POA_deactivate_object(servant->poa, objid, ev);
	CORBA_free(objid);
	
	POA_GDA_ReportEngine__fini((PortableServer_Servant) servant, ev);
	g_free(servant);
}

GDA_ReportConverterList *
impl_GDA_ReportEngine__get_conv_list (impl_POA_GDA_ReportEngine *servant,
                                      CORBA_Environment * ev) {
	GDA_ReportConverterList *retval;
	return retval;
}

GDA_ReportList *
impl_GDA_ReportEngine_queryReports (impl_POA_GDA_ReportEngine * servant,
                                    CORBA_char * condition,
                                    CORBA_long flags,
                                    CORBA_Environment * ev) {
	GDA_ReportList *retval;
	return retval;
}

GDA_Report
impl_GDA_ReportEngine_openReport (impl_POA_GDA_ReportEngine * servant,
                                  CORBA_char * rep_name,
                                  CORBA_Environment * ev) {
	GDA_Report retval;
	return retval;
}

GDA_Report
impl_GDA_ReportEngine_addReport (impl_POA_GDA_ReportEngine * servant,
                                 CORBA_char * rep_name,
                                 CORBA_char * description,
                                 CORBA_Environment * ev) {
	GDA_Report retval;
	return retval;
}

void
impl_GDA_ReportEngine_removeReport (impl_POA_GDA_ReportEngine * servant,
                                    CORBA_char * rep_name,
                                    CORBA_Environment * ev) {
}

CORBA_boolean
impl_GDA_ReportEngine_registerConverter (impl_POA_GDA_ReportEngine * servant,
                                         CORBA_char * format,
                                         GDA_ReportConverter converter,
                                         CORBA_Environment * ev) {
	CORBA_boolean retval;
	return retval;
}

void
impl_GDA_ReportEngine_unregisterConverter (impl_POA_GDA_ReportEngine * servant,
                                           GDA_ReportConverter converter,
                                           CORBA_Environment * ev) {
}

GDA_ReportConverter
impl_GDA_ReportEngine_findConverter (impl_POA_GDA_ReportEngine *servant,
                                     CORBA_char *format,
                                     CORBA_Environment *ev) {
}

GDA_ReportStream
impl_GDA_ReportEngine_createStream (impl_POA_GDA_ReportEngine *servant, CORBA_Environment *ev) {
	GDA_ReportStream new_stream;
	
	g_return_val_if_fail(servant != NULL, CORBA_OBJECT_NIL);
	
	new_stream = impl_GDA_ReportStream__create(servant->poa, ev);
	gda_corba_handle_exception(ev);
	return new_stream;
}
