/* GDA FreeTDS Provider
 * Copyright (C) 2002 The GNOME Foundation
 *
 * AUTHORS: Holger Thon <holger.thon@gnome-db.org>
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

#if !defined(__gda_freetds_h__)
#  define __gda_freetds_h__

#if defined(HAVE_CONFIG_H)
#  include <config.h>
#endif

#include <glib/gmacros.h>
#include <libgda/gda-intl.h>
#include <libgda/gda-server-provider.h>
#include <tds.h>
#include "gda-freetds-provider.h"
#include "gda-freetds-recordset.h"
#include "gda-freetds-types.h"
#include "gda-freetds-message.h"
#include "gda-tds-schemas.h"

#define GDA_FREETDS_PROVIDER_ID          "GDA FreeTDS provider"

G_BEGIN_DECLS

/*
 * Utility functions
 */

GdaError *gda_freetds_make_error (TDSSOCKET *tds, const gchar *message);
gchar **gda_freetds_split_commandlist(const gchar *cmdlist);
	

G_END_DECLS

#endif

