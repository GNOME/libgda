/* GDA mSQL Provider
 * Copyright (C) 1998-2002 The GNOME Foundation
 *
 * AUTHORS:
 * 	   Danilo Schoeneberg <dj@starfire-programming.net
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

#ifndef __gda_msql_h__
#define __gda_msql_h__

#include <glib/gmacros.h>
#include <libgda/gda-connection.h>
#include <libgda/gda-server-provider.h>
#include <libgda/gda-intl.h>
#include "gda-msql-provider.h"
#include <msql.h>

#define GDA_MSQL_PROVIDER_ID "GDA Hughes Technologies mSQL provider"

G_BEGIN_DECLS

GdaError     *gda_msql_make_error(int sock);
GdaValueType  gda_msql_type_to_gda(int msql_type);
gchar        *gda_msql_value_to_sql_string(GdaValue *val);

G_END_DECLS 

#endif
