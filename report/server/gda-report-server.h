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
  POA_GDA_Report_Stream servant;
  PortableServer_POA poa;
  
  /* added fields */
  GArray* stream_data;
} impl_POA_GDA_Report_Stream;

GDA_Report_Stream impl_GDA_Report_Stream__create (PortableServer_POA poa, CORBA_Environment *ev);
void impl_GDA_Report_Stream__destroy (impl_POA_GDA_Report_Stream *servant, CORBA_Environment *ev);
GDA_Report_StreamChunk* impl_GDA_Report_Stream_readChunk (impl_POA_GDA_Report_Stream *servant,
                                                        CORBA_long start,
                                                        CORBA_long size,
                                                        CORBA_Environment *ev);
CORBA_long impl_GDA_Report_Stream_writeChunk (impl_POA_GDA_Report_Stream *servant,
                                             GDA_Report_StreamChunk *data,
                                             CORBA_long size,
                                             CORBA_Environment * ev);
CORBA_long impl_GDA_Report_Stream_getLength (impl_POA_GDA_Report_Stream *servant, CORBA_Environment *ev);

/*
 * Report elements
 */
typedef struct
{
  POA_GDA_Report_Element servant;
  PortableServer_POA poa;

  GDA_Report_AttributeList attr_attr_list;
  GDA_Report_VE attr_contents;
  
  /* added fields */
  xmlNodePtr element_xmlnode;
} impl_POA_GDA_Report_Element;

GDA_Report_Element impl_GDA_Report_Element__create (PortableServer_POA poa,
                                                  xmlNodePtr xmlparent,
                                                  const gchar *name,
                                                  CORBA_Environment *ev);
void impl_GDA_Report_Element__destroy (impl_POA_GDA_Report_Element *servant, CORBA_Environment *ev);
CORBA_char* impl_GDA_Report_Element__get_name (impl_POA_GDA_Report_Element * servant, CORBA_Environment *ev);
GDA_Report_AttributeList* impl_GDA_Report_Element__get_attr_list (impl_POA_GDA_Report_Element *servant,
                                                                CORBA_Environment * ev);
GDA_Report_VE *impl_GDA_Report_Element__get_contents (impl_POA_GDA_Report_Element *servant, CORBA_Environment *ev);
void impl_GDA_Report_Element_addAttribute (impl_POA_GDA_Report_Element *servant,
                                          CORBA_char * name,
                                          CORBA_char * value,
                                          CORBA_Environment * ev);
void impl_GDA_Report_Element_removeAttribute (impl_POA_GDA_Report_Element * servant,
                                             CORBA_char * name,
                                             CORBA_Environment * ev);
GDA_Report_Attribute* impl_GDA_Report_Element_getAttribute (impl_POA_GDA_Report_Element * servant,
                                                          CORBA_char * name,
                                                          CORBA_Environment * ev);
void impl_GDA_Report_Element_setAttribute (impl_POA_GDA_Report_Element * servant,
                                          CORBA_char * name,
                                          CORBA_char * value,
                                          CORBA_Environment * ev);
GDA_Report_Element impl_GDA_Report_Element_addChild (impl_POA_GDA_Report_Element * servant,
                                                   CORBA_char * name,
                                                   CORBA_Environment * ev);
void impl_GDA_Report_Element_removeChild (impl_POA_GDA_Report_Element * servant,
                                         GDA_Report_Element child,
                                         CORBA_Environment * ev);
GDA_Report_ElementList* impl_GDA_Report_Element_getChildren (impl_POA_GDA_Report_Element *servant,
                                                           CORBA_Environment *ev);
CORBA_char* impl_GDA_Report_Element_getName (impl_POA_GDA_Report_Element *servant, CORBA_Environment *ev);
void impl_GDA_Report_Element_setName (impl_POA_GDA_Report_Element *servant,
                                     CORBA_char *name,
                                     CORBA_Environment *ev);

/*
 * Report format
 */
typedef struct
{
  POA_GDA_Report_Format servant;
  PortableServer_POA poa;
  
  /* added fields */
  xmlDocPtr         format_xmldoc;
} impl_POA_GDA_Report_Format;

GDA_Report_Format impl_GDA_Report_Format__create (PortableServer_POA poa, CORBA_Environment *ev);
void impl_GDA_Report_Format__destroy (impl_POA_GDA_Report_Format *servant, CORBA_Environment *ev);
GDA_Report_Element impl_GDA_Report_Format_getRootElement (impl_POA_GDA_Report_Format *servant,
                                                     CORBA_Environment *ev);
GDA_Report_Stream impl_GDA_Report_Format_getStream (impl_POA_GDA_Report_Format *servant,
                                                  CORBA_Environment * ev);

/*
 * Output converters
 */
typedef struct
{
  POA_GDA_Report_Converter servant;
  PortableServer_POA poa;
  CORBA_char *attr_format;
} impl_POA_GDA_Report_Converter;

GDA_Report_Converter impl_GDA_Report_Converter__create (PortableServer_POA poa, CORBA_Environment *ev);
void impl_GDA_Report_Converter__destroy (impl_POA_GDA_Report_Converter *servant, CORBA_Environment * ev);
CORBA_char* impl_GDA_Report_Converter__get_format (impl_POA_GDA_Report_Converter *servant,
                                                  CORBA_Environment *ev);

/*
 * Report output
 */
typedef struct
{
  POA_GDA_Report_Output servant;
  PortableServer_POA poa;
} impl_POA_GDA_Report_Output;

GDA_Report_Output impl_GDA_Report_Output__create (PortableServer_POA poa, CORBA_Environment *ev);
void impl_GDA_Report_Output__destroy (impl_POA_GDA_Report_Output *servant, CORBA_Environment *ev);
GDA_Report_Stream impl_GDA_Report_Output_convert (impl_POA_GDA_Report_Output * servant,
                                                CORBA_char *format,
                                                CORBA_long flags,
                                                CORBA_Environment *ev);

/*
 * Report instances
 */
typedef struct
{
  POA_GDA_Report_Report servant;
  PortableServer_POA poa;
  CORBA_char *attr_name;
  CORBA_char *attr_description;
  GDA_Report_Format attr_format;
  CORBA_boolean attr_isLocked;
} impl_POA_GDA_Report_Report;

GDA_Report_Report impl_GDA_Report_Report__create (PortableServer_POA poa, CORBA_Environment *ev);
void impl_GDA_Report_Report__destroy (impl_POA_GDA_Report_Report * servant, CORBA_Environment * ev);
CORBA_char *impl_GDA_Report_Report__get_name (impl_POA_GDA_Report_Report * servant, CORBA_Environment * ev);
void impl_GDA_Report_Report__set_name (impl_POA_GDA_Report_Report *servant, CORBA_char *value, CORBA_Environment *ev);
CORBA_char *impl_GDA_Report_Report__get_description (impl_POA_GDA_Report_Report *servant, CORBA_Environment * ev);
void impl_GDA_Report_Report__set_description (impl_POA_GDA_Report_Report *servant, CORBA_char *value, CORBA_Environment *ev);
GDA_Report_Format impl_GDA_Report_Report__get_format (impl_POA_GDA_Report_Report * servant, CORBA_Environment * ev);
CORBA_boolean impl_GDA_Report_Report__get_isLocked (impl_POA_GDA_Report_Report * servant, CORBA_Environment * ev);
GDA_Report_Output impl_GDA_Report_Report_run (impl_POA_GDA_Report_Report * servant,
                                      GDA_Report_ParamList * params,
                                      CORBA_long flags,
                                      CORBA_Environment * ev);
void impl_GDA_Report_Report_lock (impl_POA_GDA_Report_Report * servant, CORBA_Environment * ev);
void impl_GDA_Report_Report_unlock (impl_POA_GDA_Report_Report * servant, CORBA_Environment * ev);

/*
 * The report engine
 */
typedef struct
{
  POA_GDA_Report_Engine    servant;
  PortableServer_POA      poa;
  GDA_Report_ConverterList attr_conv_list;
} impl_POA_GDA_Report_Engine;

GDA_Report_Engine impl_GDA_Report_Engine__create (PortableServer_POA poa, CORBA_Environment * ev);
void impl_GDA_Report_Engine__destroy (impl_POA_GDA_Report_Engine *servant,
                                     CORBA_Environment *ev);
GDA_Report_ConverterList* impl_GDA_Report_Engine__get_conv_list (impl_POA_GDA_Report_Engine *servant,
                                                        CORBA_Environment *ev);
GDA_Report_List* impl_GDA_Report_Engine_queryReports (impl_POA_GDA_Report_Engine *servant,
                                                    CORBA_char *condition,
                                                    CORBA_long flags,
                                                    CORBA_Environment *ev);
GDA_Report_Report impl_GDA_Report_Engine_openReport (impl_POA_GDA_Report_Engine *servant,
                                             CORBA_char *rep_name,
                                             CORBA_Environment *ev);
GDA_Report_Report impl_GDA_Report_Engine_addReport (impl_POA_GDA_Report_Engine *servant,
                                            CORBA_char *rep_name,
                                            CORBA_char *description,
                                            CORBA_Environment *ev);
void impl_GDA_Report_Engine_removeReport (impl_POA_GDA_Report_Engine *servant,
                                         CORBA_char *rep_name,
                                         CORBA_Environment *ev);
CORBA_boolean impl_GDA_Report_Engine_registerConverter (impl_POA_GDA_Report_Engine *servant,
                                                       CORBA_char *format,
                                                       GDA_Report_Converter converter,
                                                       CORBA_Environment *ev);
void impl_GDA_Report_Engine_unregisterConverter (impl_POA_GDA_Report_Engine *servant,
                                                GDA_Report_Converter converter,
                                                CORBA_Environment *ev);
GDA_Report_Converter impl_GDA_Report_Engine_findConverter (impl_POA_GDA_Report_Engine *servant,
                                                         CORBA_char *format,
                                                         CORBA_Environment *ev);
GDA_Report_Stream impl_GDA_Report_Engine_createStream (impl_POA_GDA_Report_Engine *servant,
                                                     CORBA_Environment *ev);

/*
 * Global variables
 */
extern CORBA_Object glb_engine;

#endif
