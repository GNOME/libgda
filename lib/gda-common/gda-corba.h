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

#if !defined(__gda_corba_h__)
#  define __gda_corba_h__

#include <orb/orb.h>
#include <liboaf/liboaf.h>

#if defined(__cplusplus)
extern "C" {
#endif

CORBA_ORB    gda_corba_get_orb           (void);
CORBA_Object gda_corba_get_name_service  (void);
gchar*       gda_corba_get_oaf_attribute (CORBA_sequence_OAF_Property props, const gchar *name);

gboolean     gda_corba_handle_exception  (CORBA_Environment *ev);

#if defined(__cplusplus)
}
#endif

#endif
