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

#if !defined(__gda_freetds_error_h__)
#  define __gda_freetds_error_h__

#if defined(HAVE_CONFIG_H)
#  include <config.h>
#endif

#include <glib/gmacros.h>
#include <libgda/gda-intl.h>
#include <libgda/gda-server-provider.h>
#include <tds.h>
#include "gda-freetds.h"
#include "gda-freetds-provider.h"
#include "gda-freetds-recordset.h"
#include "gda-freetds-types.h"

G_BEGIN_DECLS

typedef struct _GdaFreeTDSMessage GdaFreeTDSMessage;
struct _GdaFreeTDSMessage {
	gboolean    is_err_msg;

	TDSMSGINFO  msg;
};

GdaFreeTDSMessage *gda_freetds_message_new (GdaConnection *cnc,
                                            TDSMSGINFO *info,
                                            const gboolean is_err_msg);
GdaFreeTDSMessage *gda_freetds_message_add (GdaConnection *cnc,
                                            TDSMSGINFO *info,
                                            const gboolean is_err_msg);
void gda_freetds_message_free (GdaFreeTDSMessage *message);


G_END_DECLS

#endif

