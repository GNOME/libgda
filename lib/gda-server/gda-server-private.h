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

#if !defined(__gda_server_private_h__)
#  define __gda_server_private_h__

#include "config.h"
#include "GDA.h"

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
 * Definitions for the GDA::Command interface
 */
typedef struct {
	POA_GDA_Parameter  servant;
	PortableServer_POA poa;
} impl_POA_GDA_Parameter;

typedef struct {
	POA_GDA_Command     servant;
	PortableServer_POA  poa;
	CORBA_long          attr_cmdTimeout;
	CORBA_boolean       attr_prepared;
	CORBA_long          attr_state;
	CORBA_char*         attr_text;
	GDA_CommandType     attr_type;
	
	/* added fields */
	GdaServerCommand*  cmd;
} impl_POA_GDA_Command;

void impl_GDA_Parameter__destroy (impl_POA_GDA_Parameter *servant,
                                  CORBA_Environment *ev);
CORBA_long impl_GDA_Parameter_appendChunk (impl_POA_GDA_Parameter *servant,
					   GDA_Parameter_VarBinString *data,
					   CORBA_Environment *ev);

void impl_GDA_Command__destroy (impl_POA_GDA_Command *servant,
				CORBA_Environment *ev);
CORBA_long impl_GDA_Command__get_cmdTimeout (impl_POA_GDA_Command *servant,
					     CORBA_Environment *ev);
void impl_GDA_Command__set_cmdTimeout (impl_POA_GDA_Command *servant,
				       CORBA_long value,
				       CORBA_Environment *ev);
CORBA_boolean impl_GDA_Command__get_prepared (impl_POA_GDA_Command *servant,
					      CORBA_Environment *ev);
CORBA_long impl_GDA_Command__get_state (impl_POA_GDA_Command *servant,
					CORBA_Environment *ev);
void impl_GDA_Command__set_state (impl_POA_GDA_Command *servant,
				  CORBA_long value,
				  CORBA_Environment *ev);
CORBA_char* impl_GDA_Command__get_text (impl_POA_GDA_Command *servant,
					CORBA_Environment *ev);
void impl_GDA_Command__set_text (impl_POA_GDA_Command *servant,
				 CORBA_char *value,
				 CORBA_Environment *ev);
GDA_CommandType impl_GDA_Command__get_type (impl_POA_GDA_Command *servant,
                                            CORBA_Environment *ev);
void impl_GDA_Command__set_type (impl_POA_GDA_Command *servant,
                                 GDA_CommandType value,
                                 CORBA_Environment *ev);
GDA_Recordset impl_GDA_Command_open (impl_POA_GDA_Command *servant,
                                     GDA_CmdParameterSeq *param,
                                     GDA_CursorType ct,
                                     GDA_LockType lt,
                                     CORBA_unsigned_long *affected,
                                     CORBA_Environment *ev);

/*
 * Definitions for the GDA::Connection interface
 */
typedef struct {
	POA_GDA_Connection   servant;
	PortableServer_POA   poa;
	CORBA_long           attr_flags;
	CORBA_long           attr_cmdTimeout;
	CORBA_long           attr_connectTimeout;
	GDA_CursorLocation   attr_cursor;
	CORBA_char*          attr_version;
	GDA_ErrorSeq         attr_errors;

	GdaServerConnection* cnc;
	gchar*               id;
} impl_POA_GDA_Connection;

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
CORBA_char* impl_GDA_Connection_sql2xml (impl_POA_GDA_Connection *servant,
                                         CORBA_char *sql,
                                         CORBA_Environment *ev);
CORBA_char* impl_GDA_Connection_xml2sql (impl_POA_GDA_Connection *servant,
                                         CORBA_char *xml,
                                         CORBA_Environment *ev);

/*
 * Definitions for the GDA::ConnectionFactory interface
 */
typedef struct
{
  POA_GDA_ConnectionFactory servant;
  PortableServer_POA        poa;

} impl_POA_GDA_ConnectionFactory;

void impl_GDA_ConnectionFactory__destroy (impl_POA_GDA_ConnectionFactory *servant,
                                          CORBA_Environment *ev);
CORBA_Object impl_GDA_ConnectionFactory_create_connection (impl_POA_GDA_ConnectionFactory *servant,
                                                           CORBA_char *goad_id,
                                                           CORBA_Environment *ev);
CORBA_boolean impl_GDA_ConnectionFactory_manufactures (impl_POA_GDA_ConnectionFactory *servant,
                                                       CORBA_char *obj_id,
                                                       CORBA_Environment *ev);
CORBA_Object impl_GDA_ConnectionFactory_create_object (impl_POA_GDA_ConnectionFactory *servant,
                                                       CORBA_char *goad_id,
                                                       GNOME_stringlist *params,
                                                       CORBA_Environment *ev);
void impl_GDA_ConnectionFactory_ref (impl_POA_GDA_ConnectionFactory *servant,
									 CORBA_Environment *ev);
void impl_GDA_ConnectionFactory_unref (impl_POA_GDA_ConnectionFactory *servant,
									   CORBA_Environment *ev);

/*
 * Definitions for the GDA::Recordset interface
 */
typedef struct
{
  POA_GDA_Recordset    servant;
  PortableServer_POA   poa;
  CORBA_long           attr_currentBookmark;
  CORBA_long           attr_cachesize;
  GDA_CursorType       attr_currentCursorType;
  GDA_LockType         attr_lockingMode;
  CORBA_long           attr_maxrecords;
  CORBA_long           attr_pagecount;
  CORBA_long           attr_pagesize;
  CORBA_long           attr_recCount;
  CORBA_char*          attr_source;
  CORBA_long           attr_status;

  GdaServerRecordset* recset;
} impl_POA_GDA_Recordset;

void impl_GDA_Recordset__destroy (impl_POA_GDA_Recordset *servant, CORBA_Environment *ev);
CORBA_long impl_GDA_Recordset__get_currentBookmark (impl_POA_GDA_Recordset *servant,
                                                    CORBA_Environment *ev);
void impl_GDA_Recordset__set_currentBookmark (impl_POA_GDA_Recordset *servant,
                                              CORBA_long value,
                                              CORBA_Environment *ev);
CORBA_long impl_GDA_Recordset__get_cachesize (impl_POA_GDA_Recordset *servant,
					      CORBA_Environment *ev);
void impl_GDA_Recordset__set_cachesize (impl_POA_GDA_Recordset *servant,
					CORBA_long value,
					CORBA_Environment *ev);
GDA_CursorType impl_GDA_Recordset__get_currentCursorType (impl_POA_GDA_Recordset *servant,
							  CORBA_Environment *ev);
void impl_GDA_Recordset__set_currentCursorType (impl_POA_GDA_Recordset *servant,
						GDA_CursorType value,
						CORBA_Environment *ev);
GDA_LockType impl_GDA_Recordset__get_lockingMode (impl_POA_GDA_Recordset *servant,
						  CORBA_Environment *ev);
void impl_GDA_Recordset__set_lockingMode (impl_POA_GDA_Recordset *servant,
					  GDA_LockType value,
					  CORBA_Environment *ev);
CORBA_long impl_GDA_Recordset__get_maxrecords (impl_POA_GDA_Recordset *servant,
					       CORBA_Environment *ev);
void impl_GDA_Recordset__set_maxrecords (impl_POA_GDA_Recordset *servant,
					 CORBA_long value,
					 CORBA_Environment *ev);
CORBA_long impl_GDA_Recordset__get_pagecount (impl_POA_GDA_Recordset *servant,
					      CORBA_Environment *ev);
CORBA_long impl_GDA_Recordset__get_pagesize (impl_POA_GDA_Recordset *servant,
					     CORBA_Environment *ev);
void impl_GDA_Recordset__set_pagesize (impl_POA_GDA_Recordset *servant,
				       CORBA_long value,
				       CORBA_Environment *ev);
CORBA_long impl_GDA_Recordset__get_recCount (impl_POA_GDA_Recordset *servant,
					     CORBA_Environment *ev);
CORBA_char* impl_GDA_Recordset__get_source (impl_POA_GDA_Recordset *servant,
					    CORBA_Environment *ev);
CORBA_long impl_GDA_Recordset__get_status (impl_POA_GDA_Recordset *servant,
					   CORBA_Environment *ev);
CORBA_long impl_GDA_Recordset_close (impl_POA_GDA_Recordset *servant,
				     CORBA_Environment *ev);
CORBA_long impl_GDA_Recordset_move (impl_POA_GDA_Recordset *servant,
				    CORBA_long count,
				    CORBA_long bookmark,
				    CORBA_Environment *ev);
CORBA_long impl_GDA_Recordset_moveFirst (impl_POA_GDA_Recordset *servant,
					 CORBA_Environment *ev);
CORBA_long impl_GDA_Recordset_moveLast (impl_POA_GDA_Recordset *servant,
					CORBA_Environment *ev);
CORBA_long impl_GDA_Recordset_reQuery (impl_POA_GDA_Recordset *servant,
				       CORBA_Environment *ev);
CORBA_long impl_GDA_Recordset_reSync (impl_POA_GDA_Recordset *servant,
				      CORBA_Environment *ev);
CORBA_boolean impl_GDA_Recordset_supports (impl_POA_GDA_Recordset *servant,
					   GDA_Option what,
					   CORBA_Environment *ev);
GDA_Recordset_Chunk* impl_GDA_Recordset_fetch (impl_POA_GDA_Recordset *servant,
					       CORBA_long count,
					       CORBA_Environment *ev);
GDA_RowAttributes* impl_GDA_Recordset_describe (impl_POA_GDA_Recordset *servant,
						CORBA_Environment *ev);

#endif
