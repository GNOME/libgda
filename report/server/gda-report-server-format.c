/* GDA Report Engine
 * Copyright (C) 2000 Rodrigo Moya
 
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

static PortableServer_ServantBase__epv impl_GDA_ReportFormat_base_epv = {
	NULL,			/* _private data */
	NULL,			/* finalize routine */
	NULL,			/* default_POA routine */
};
static POA_GDA_ReportFormat__epv impl_GDA_ReportFormat_epv = {
	NULL,			/* _private */
	(gpointer) & impl_GDA_ReportFormat_getRootElement,
	(gpointer) & impl_GDA_ReportFormat_getStream,
};
static POA_GDA_ReportFormat__vepv impl_GDA_ReportFormat_vepv = {
	&impl_GDA_ReportFormat_base_epv,
	&impl_GDA_ReportFormat_epv,
};

/*
 * Stub implementations
 */
GDA_ReportFormat
impl_GDA_ReportFormat__create (PortableServer_POA poa, CORBA_Environment * ev) {
	GDA_ReportFormat retval;
	impl_POA_GDA_ReportFormat *newservant;
	PortableServer_ObjectId *objid;
	
	newservant = g_new0(impl_POA_GDA_ReportFormat, 1);
	newservant->servant.vepv = &impl_GDA_ReportFormat_vepv;
	newservant->poa = poa;
	
	/* create XML root node */
	newservant->format_xmldoc = xmlNewDoc("1.0");
	newservant->format_xmldoc->root = xmlNewDocNode(newservant->format_xmldoc, NULL, "report", NULL);
	
	/* activate CORBA object */
	POA_GDA_ReportFormat__init((PortableServer_Servant) newservant, ev);
	objid = PortableServer_POA_activate_object(poa, newservant, ev);
	CORBA_free(objid);
	retval = PortableServer_POA_servant_to_reference(poa, newservant, ev);
	
	return retval;
}

void
impl_GDA_ReportFormat__destroy (impl_POA_GDA_ReportFormat *servant, CORBA_Environment *ev) {
	PortableServer_ObjectId *objid;
	
	g_return_if_fail(!CORBA_Object_is_nil(servant, ev));
	
	/* free memory */
	xmlFreeDoc(servant->format_xmldoc);
	
	/* deactivate CORBA object */
	objid = PortableServer_POA_servant_to_id(servant->poa, servant, ev);
	PortableServer_POA_deactivate_object(servant->poa, objid, ev);
	CORBA_free(objid);
	
	POA_GDA_ReportFormat__fini((PortableServer_Servant) servant, ev);
	g_free(servant);
}

GDA_ReportElement
impl_GDA_ReportFormat_getRootElement (impl_POA_GDA_ReportFormat *servant, CORBA_Environment *ev) {
	GDA_ReportElement retval;
	
	g_return_val_if_fail(!CORBA_Object_is_nil(servant, ev), CORBA_OBJECT_NIL);
	
	retval = impl_GDA_ReportElement__create(servant->poa, NULL, NULL, ev);
	if (gda_corba_handle_exception(ev)) {
		//retval->element_xmlnode = servant->format_xmldoc->root;
		return retval;
	}
	return CORBA_OBJECT_NIL;
}

GDA_ReportStream
impl_GDA_ReportFormat_getStream (impl_POA_GDA_ReportFormat *servant, CORBA_Environment *ev) {
	GDA_ReportStream retval;
	xmlChar*         xml = NULL;
	gint             size;
	
	g_return_val_if_fail(!CORBA_Object_is_nil(servant, ev), CORBA_OBJECT_NIL);
	
	/* create the stream to be returned */
	retval = impl_GDA_ReportEngine_createStream(servant->poa, ev);
	xmlDocDumpMemory(servant->format_xmldoc, &xml, &size);
	if (xml) {
		GDA_ReportStreamChunk* chunk;
		
		/* create the ReportStreamChunk object */
		chunk = GDA_ReportStreamChunk__alloc();
		chunk->_buffer = g_memdup((gpointer) xml, size);
		chunk->_length = size;
		if (impl_GDA_ReportStream_writeChunk(retval, chunk, size, ev) < 0) {
			gda_log_error(_("Error while writing to stream %p"), retval);
			GDA_ReportStreamChunk__free(chunk, chunk->_buffer, TRUE);
			CORBA_Object_release(retval, ev);
			return CORBA_OBJECT_NIL;
		}
		gda_log_message(_("Wrote %d bytes to stream %p"), size, retval);
		GDA_ReportStreamChunk__free(chunk, chunk->_buffer, TRUE);
	}
	return retval;
}
