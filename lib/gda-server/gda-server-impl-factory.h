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

#if !defined(__gda_server_impl_factory_h__)
#  define __gda_server_impl_factory_h__

#include "GDA.h"

/*
 * App-specific servant structures
 */
typedef struct
{
  POA_GDA_ConnectionFactory servant;
  PortableServer_POA        poa;

} impl_POA_GDA_ConnectionFactory;

/*
 * Implementation stub prototypes
 */
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

#endif


