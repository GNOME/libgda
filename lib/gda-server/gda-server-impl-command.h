/* GNOME DB Server Library
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

#if !defined(__gda_server_impl_command_h__)
#  define __gda_server_impl_command_h__

/*
 * App-specific servant structures
 */
typedef struct
{
  POA_GDA_Parameter  servant;
  PortableServer_POA poa;

} impl_POA_GDA_Parameter;

typedef struct
{
  POA_GDA_Command     servant;
  PortableServer_POA  poa;
  CORBA_long          attr_cmdTimeout;
  CORBA_boolean       attr_prepared;
  CORBA_long          attr_state;
  CORBA_char*         attr_text;
  CORBA_unsigned_long attr_type;

  Gda_ServerCommand*  cmd;
} impl_POA_GDA_Command;

/*
 * Implementation stub prototypes
 */
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
CORBA_unsigned_long impl_GDA_Command__get_type (impl_POA_GDA_Command *servant,
						CORBA_Environment *ev);
void impl_GDA_Command__set_type (impl_POA_GDA_Command *servant,
				 CORBA_unsigned_long value,
				 CORBA_Environment *ev);
GDA_Recordset impl_GDA_Command_open (impl_POA_GDA_Command *servant,
				     GDA_CmdParameterSeq *param,
				     GDA_CursorType ct,
				     GDA_LockType lt,
				     CORBA_unsigned_long *affected,
				     CORBA_Environment *ev);

#endif
