/* GDA Report Engine
 * Copyright (C) 2000 Rodrigo Moya
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#if !defined(__gda_report_server_h__)
#  define __gda_report_server_h__

#include <gda-common.h>
#include <gda-report-defs.h>
#include <GDA_Report.h>

/*
 * The report engine
 */
typedef struct
{
  POA_GDA_ReportEngine servant;
  PortableServer_POA poa;
} impl_POA_GDA_ReportEngine;

extern POA_GDA_ReportEngine__vepv impl_GDA_ReportEngine_vepv;

GDA_ReportEngine server_engine__create(PortableServer_POA poa, CORBA_Environment *ev);
void report_server_engine__destroy (impl_POA_GDA_ReportEngine *servant, CORBA_Environment *ev);
GDA_ReportList *server_engine_queryReports (impl_POA_GDA_ReportEngine *servant,
                                            CORBA_char * condition,
                                            CORBA_long flags,
                                            CORBA_Environment * ev);
GDA_Report server_engine_openReport (impl_POA_GDA_ReportEngine * servant,
                                     CORBA_char * rep_name,
                                     CORBA_Environment * ev);
GDA_Report server_engine_addReport (impl_POA_GDA_ReportEngine * servant,
                                    CORBA_char * rep_name,
                                    CORBA_char * description,
                                    CORBA_Environment * ev);
void server_engine_removeReport (impl_POA_GDA_ReportEngine * servant,
                                 CORBA_char * rep_name,
                                 CORBA_Environment * ev);

/*
 * Global variables
 */
extern PortableServer_POA glb_the_poa;
extern GDA_ReportEngine   glb_engine;

#endif
