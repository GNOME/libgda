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

#if !defined(__gda_server_impl_factory_h__)
#  define __gda_server_impl_factory_h__

/*
 * App-specific servant structures
 */
typedef struct
{
  POA_GNOME_GenericFactory servant;
  PortableServer_POA       poa;

} impl_POA_GNOME_GenericFactory;

typedef struct
{
  POA_GNOME_GenericFactory servant;
  PortableServer_POA       poa;

} impl_POA_GDA_GenericFactory;

typedef struct
{
  POA_GDA_ConnectionFactory servant;
  PortableServer_POA        poa;

} impl_POA_GDA_ConnectionFactory;

/*
 * Implementation stub prototypes
 */
CORBA_boolean impl_GNOME_GenericFactory_supports (impl_POA_GNOME_GenericFactory *servant,
						  CORBA_char *obj_goad_id,
						  CORBA_Environment *ev);
void impl_GNOME_GenericFactory__destroy (impl_POA_GNOME_GenericFactory * servant,
					 CORBA_Environment * ev);
CORBA_Object impl_GNOME_GenericFactory_create_object (impl_POA_GNOME_GenericFactory *servant,
						       CORBA_char *goad_id,
						       GNOME_stringlist *params,
						       CORBA_Environment *ev);

void impl_GDA_ConnectionFactory__destroy (impl_POA_GDA_ConnectionFactory *servant,
					  CORBA_Environment *ev);
CORBA_Object impl_GDA_ConnectionFactory_create_connection (impl_POA_GDA_ConnectionFactory *servant,
							   CORBA_char *goad_id,
							   CORBA_Environment *ev);
CORBA_boolean impl_GDA_ConnectionFactory_supports (impl_POA_GDA_ConnectionFactory *servant,
						   CORBA_char *obj_goad_id,
						   CORBA_Environment *ev);
CORBA_Object impl_GDA_ConnectionFactory_create_object (impl_POA_GDA_ConnectionFactory *servant,
						       CORBA_char *goad_id,
						       GNOME_stringlist *params,
						       CORBA_Environment *ev);

#endif


