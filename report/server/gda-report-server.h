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

/*
 * Global variables
 */
extern CORBA_Object glb_engine;

#endif
