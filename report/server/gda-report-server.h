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

#if !defined(__gda_report_server_h__)
#  define __gda_report_server_h__

#include "config.h"
#include <gda-common.h>
#include <gda-report-defs.h>
#include <gda-report-server-cache.h>
#include <gda-report-server-db.h>

#include <GDA_Report.h>

#ifdef ENABLE_NLS
#  include <libintl.h>
#  define _(String) gettext (String)
#  define N_(String) (String)
#else
/* Stubs that do something close enough.  */
#  define textdomain(String)
#  define gettext(String) (String)
#  define dgettext(Domain,Message) (Message)
#  define dcgettext(Domain,Message,Type) (Message)
#  define bindtextdomain(Domain,Directory)
#  define _(String) (String)
#  define N_(String) (String)
#endif

/*
 * Report streams
 */
typedef struct
{
  POA_GDA_ReportStream servant;
  PortableServer_POA poa;
  
  /* added fields */
  GArray* stream_data;
} impl_POA_GDA_ReportStream;

GDA_ReportStream impl_GDA_ReportStream__create (PortableServer_POA poa, CORBA_Environment *ev);
void impl_GDA_ReportStream__destroy (impl_POA_GDA_ReportStream *servant, CORBA_Environment *ev);
GDA_ReportStreamChunk* impl_GDA_ReportStream_readChunk (impl_POA_GDA_ReportStream *servant,
                                                        CORBA_long start,
                                                        CORBA_long size,
                                                        CORBA_Environment *ev);
CORBA_long impl_GDA_ReportStream_writeChunk (impl_POA_GDA_ReportStream *servant,
                                             GDA_ReportStreamChunk *data,
                                             CORBA_long size,
                                             CORBA_Environment * ev);
CORBA_long impl_GDA_ReportStream_getLength (impl_POA_GDA_ReportStream *servant, CORBA_Environment *ev);

/*
 * Report elements
 */
typedef struct
{
  POA_GDA_ReportElement servant;
  PortableServer_POA poa;

  GDA_ReportAttributeList attr_attr_list;
  GDA_VE attr_contents;
  
  /* added fields */
  xmlNodePtr element_xmlnode;
} impl_POA_GDA_ReportElement;

GDA_ReportElement impl_GDA_ReportElement__create (PortableServer_POA poa,
                                                  xmlNodePtr xmlparent,
                                                  const gchar *name,
                                                  CORBA_Environment *ev);
void impl_GDA_ReportElement__destroy (impl_POA_GDA_ReportElement *servant, CORBA_Environment *ev);
CORBA_char* impl_GDA_ReportElement__get_name (impl_POA_GDA_ReportElement * servant, CORBA_Environment *ev);
GDA_ReportAttributeList* impl_GDA_ReportElement__get_attr_list (impl_POA_GDA_ReportElement *servant,
                                                                CORBA_Environment * ev);
GDA_VE *impl_GDA_ReportElement__get_contents (impl_POA_GDA_ReportElement *servant, CORBA_Environment *ev);
void impl_GDA_ReportElement_addAttribute (impl_POA_GDA_ReportElement *servant,
                                          CORBA_char * name,
                                          CORBA_char * value,
                                          CORBA_Environment * ev);
void impl_GDA_ReportElement_removeAttribute (impl_POA_GDA_ReportElement * servant,
                                             CORBA_char * name,
                                             CORBA_Environment * ev);
GDA_ReportAttribute* impl_GDA_ReportElement_getAttribute (impl_POA_GDA_ReportElement * servant,
                                                          CORBA_char * name,
                                                          CORBA_Environment * ev);
void impl_GDA_ReportElement_setAttribute (impl_POA_GDA_ReportElement * servant,
                                          CORBA_char * name,
                                          CORBA_char * value,
                                          CORBA_Environment * ev);
GDA_ReportElement impl_GDA_ReportElement_addChild (impl_POA_GDA_ReportElement * servant,
                                                   CORBA_char * name,
                                                   CORBA_Environment * ev);
void impl_GDA_ReportElement_removeChild (impl_POA_GDA_ReportElement * servant,
                                         GDA_ReportElement child,
                                         CORBA_Environment * ev);
GDA_ReportElementList* impl_GDA_ReportElement_getChildren (impl_POA_GDA_ReportElement *servant,
                                                           CORBA_Environment *ev);
CORBA_char* impl_GDA_ReportElement_getName (impl_POA_GDA_ReportElement *servant, CORBA_Environment *ev);
void impl_GDA_ReportElement_setName (impl_POA_GDA_ReportElement *servant,
                                     CORBA_char *name,
                                     CORBA_Environment *ev);

/*
 * Report format
 */
typedef struct
{
  POA_GDA_ReportFormat servant;
  PortableServer_POA poa;
} impl_POA_GDA_ReportFormat;

GDA_ReportFormat impl_GDA_ReportFormat__create (PortableServer_POA poa, CORBA_Environment *ev);
void impl_GDA_ReportFormat__destroy (impl_POA_GDA_ReportFormat *servant, CORBA_Environment *ev);
GDA_ReportElementList* impl_GDA_ReportFormat_getStructure (impl_POA_GDA_ReportFormat *servant,
                                                           CORBA_Environment *ev);
GDA_ReportStream impl_GDA_ReportFormat_getStream (impl_POA_GDA_ReportFormat *servant,
                                                  CORBA_Environment * ev);

/*
 * The report engine
 */
typedef struct
{
  POA_GDA_ReportEngine    servant;
  PortableServer_POA      poa;
  GDA_ReportConverterList attr_conv_list;
} impl_POA_GDA_ReportEngine;

GDA_ReportEngine impl_GDA_ReportEngine__create (PortableServer_POA poa, CORBA_Environment * ev);
void impl_GDA_ReportEngine__destroy (impl_POA_GDA_ReportEngine *servant,
                                     CORBA_Environment *ev);
GDA_ReportConverterList* impl_GDA_ReportEngine__get_conv_list (impl_POA_GDA_ReportEngine *servant,
                                                        CORBA_Environment *ev);
GDA_ReportList* impl_GDA_ReportEngine_queryReports (impl_POA_GDA_ReportEngine *servant,
                                                    CORBA_char *condition,
                                                    CORBA_long flags,
                                                    CORBA_Environment *ev);
GDA_Report impl_GDA_ReportEngine_openReport (impl_POA_GDA_ReportEngine *servant,
                                             CORBA_char *rep_name,
                                             CORBA_Environment *ev);
GDA_Report impl_GDA_ReportEngine_addReport (impl_POA_GDA_ReportEngine *servant,
                                            CORBA_char *rep_name,
                                            CORBA_char *description,
                                            CORBA_Environment *ev);
void impl_GDA_ReportEngine_removeReport (impl_POA_GDA_ReportEngine *servant,
                                         CORBA_char *rep_name,
                                         CORBA_Environment *ev);
CORBA_boolean impl_GDA_ReportEngine_registerConverter (impl_POA_GDA_ReportEngine *servant,
                                                       CORBA_char *format,
                                                       GDA_ReportConverter converter,
                                                       CORBA_Environment *ev);
void impl_GDA_ReportEngine_unregisterConverter (impl_POA_GDA_ReportEngine *servant,
                                                GDA_ReportConverter converter,
                                                CORBA_Environment *ev);
GDA_ReportConverter impl_GDA_ReportEngine_findConverter (impl_POA_GDA_ReportEngine *servant,
                                                         CORBA_char *format,
                                                         CORBA_Environment *ev);
GDA_ReportStream impl_GDA_ReportEngine_createStream (impl_POA_GDA_ReportEngine *servant,
                                                     CORBA_Environment *ev);

/*
 * Global variables
 */
extern CORBA_Object glb_engine;

#endif
