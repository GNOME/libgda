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

#if !defined(__gda_common_h__)
#  define __gda_common_h__

/*
 * This is the main header file for the libgda-common library
 */

#if defined(HAVE_CONFIG_H)
#  include <config.h>
#endif
#include <orb/orb.h>
#include <gnome.h>
#include <liboaf/liboaf.h>
#include <gtk/gtk.h>

#include <gda-corba.h>
#include <gda-log.h>
#include <gda-config.h>
#include <gda-thread.h>
#include <gda-xml-file.h>
#include <gda-xml-database.h>
#include <gda-xml-query.h>

#endif