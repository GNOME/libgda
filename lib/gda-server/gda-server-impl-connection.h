/* GDA Server Library
 * Copyright (C) 2000, Rodrigo Moya <rmoya@chez.com>
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

#if !defined(__gda_server_impl_connection_h__)
#  define __gda_server_impl_connection_h__

#include <GDA.h>

/*
 * App-specific servant structures
 */
typedef struct
{
  POA_GDA_Connection    servant;
  PortableServer_POA    poa;
  CORBA_long            attr_flags;
  CORBA_long            attr_cmdTimeout;
  CORBA_long            attr_connectTimeout;
  GDA_CursorLocation    attr_cursor;
  CORBA_char*           attr_version;
  GDA_ErrorSeq          attr_errors;

  Gda_ServerConnection* cnc;
} impl_POA_GDA_Connection;

/*
 * Implementation stub prototypes
 */
void impl_GDA_Connection__destroy (impl_POA_GDA_Connection *servant,
                                   CORBA_Environment *ev);
CORBA_long impl_GDA_Connection__get_flags (impl_POA_GDA_Connection *servant,
                                           CORBA_Environment *ev);
void impl_GDA_Connection__set_flags (impl_POA_GDA_Connection *servant,
                                     CORBA_long value,
                                     CORBA_Environment *ev);
CORBA_long impl_GDA_Connection__get_cmdTimeout (impl_POA_GDA_Connection *servant,
                                                CORBA_Environment *ev);
void impl_GDA_Connection__set_cmdTimeout (impl_POA_GDA_Connection *servant,
                                          CORBA_long value,
                                          CORBA_Environment *ev);
CORBA_long impl_GDA_Connection__get_connectTimeout (impl_POA_GDA_Connection *servant,
                                                    CORBA_Environment *ev);
void impl_GDA_Connection__set_connectTimeout (impl_POA_GDA_Connection *servant,
                                              CORBA_long value,
                                              CORBA_Environment *ev);
GDA_CursorLocation impl_GDA_Connection__get_cursor (impl_POA_GDA_Connection *servant,
                                                    CORBA_Environment *ev);
void impl_GDA_Connection__set_cursor (impl_POA_GDA_Connection *servant,
                                      GDA_CursorLocation value,
                                      CORBA_Environment *ev);
CORBA_char* impl_GDA_Connection__get_version (impl_POA_GDA_Connection *servant,
                                              CORBA_Environment *ev);
GDA_ErrorSeq* impl_GDA_Connection__get_errors (impl_POA_GDA_Connection *servant,
                                               CORBA_Environment *ev);

CORBA_long impl_GDA_Connection_beginTransaction (impl_POA_GDA_Connection *servant,
                                                 CORBA_Environment *ev);
CORBA_long impl_GDA_Connection_commitTransaction (impl_POA_GDA_Connection *servant,
                                                  CORBA_Environment *ev);
CORBA_long impl_GDA_Connection_rollbackTransaction (impl_POA_GDA_Connection *servant,
                                                    CORBA_Environment *ev);
CORBA_long impl_GDA_Connection_close (impl_POA_GDA_Connection *servant,
                                      CORBA_Environment *ev);
CORBA_long impl_GDA_Connection_open (impl_POA_GDA_Connection *servant,
                                     CORBA_char *dsn,
                                     CORBA_char *user,
                                     CORBA_char *pwd,
                                     CORBA_Environment *ev);
GDA_Recordset impl_GDA_Connection_openSchema (impl_POA_GDA_Connection *servant,
                                              GDA_Connection_QType t,
                                              GDA_Connection_ConstraintSeq *constraints,
                                              CORBA_Environment *ev);
CORBA_long impl_GDA_Connection_modifySchema (impl_POA_GDA_Connection *servant,
                                             GDA_Connection_QType t,
                                             GDA_Connection_ConstraintSeq *constraints,
                                             CORBA_Environment *ev);
GDA_Command impl_GDA_Connection_createCommand (impl_POA_GDA_Connection *servant,
                                               CORBA_Environment *ev);
GDA_Recordset impl_GDA_Connection_createRecordset (impl_POA_GDA_Connection *servant,
                                                   CORBA_Environment *ev);
GDA_Connection_DSNlist* impl_GDA_Connection_listSources (impl_POA_GDA_Connection *servant,
                                                         CORBA_Environment *ev);
CORBA_long impl_GDA_Connection_startLogging (impl_POA_GDA_Connection *servant,
                                             CORBA_char *filename,
                                             CORBA_Environment *ev);
CORBA_long impl_GDA_Connection_stopLogging (impl_POA_GDA_Connection *servant,
                                            CORBA_Environment *ev);
CORBA_char* impl_GDA_Connection_createTable (impl_POA_GDA_Connection *servant,
                                             CORBA_char *name,
                                             GDA_RowAttributes *columns,
                                             CORBA_Environment *ev);
CORBA_boolean impl_GDA_Connection_supports (impl_POA_GDA_Connection *servant,
                                            GDA_Connection_Feature feature,
                                            CORBA_Environment *ev);

#endif







