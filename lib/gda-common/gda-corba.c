/* GNOME DB Common Library
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

#include "gda-common.h"

/**
 * gda_corba_get_orb
 *
 * Return the CORBA ORB being used by the current program
 */
CORBA_ORB
gda_corba_get_orb (void)
{
#if defined(USING_OAF)
  return oaf_orb_get();
#else
  return gnome_CORBA_ORB();
#endif
}

/**
 * gda_corba_get_name_service
 *
 * Return a reference to the CORBA name service
 */
CORBA_Object
gda_corba_get_name_service (void)
{
#if defined(USING_OAF)
  CORBA_Environment ev;
  return oaf_name_service_get(&ev);
#else
  return gnome_name_service_get();
#endif
}
