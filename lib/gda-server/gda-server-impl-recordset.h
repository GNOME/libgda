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

#if !defined(__gda_server_impl_recordset_h__)
#  define __gda_server_impl_recordset_h__

/*
 * App-specific servant structures
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

  Gda_ServerRecordset* recset;
} impl_POA_GDA_Recordset;

/*
 * Implementation stub prototypes
 */
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

