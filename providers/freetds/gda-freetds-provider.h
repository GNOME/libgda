/* GNOME DB FreeTDS Provider
 * Copyright (C) 2002 - 2005 The GNOME Foundation
 *
 * AUTHORS: 
 *         Holger Thon <holger.thon@gnome-db.org>
 *         Vivien Malerba <malerba@gnome-db.org>
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

#if !defined(__gda_freetds_provider_h__)
#  define __gda_freetds_provider_h__

#if defined(HAVE_CONFIG_H)
#endif

#include <libgda/gda-server-provider.h>
#include <tds.h>
#include "gda-freetds-defs.h"

G_BEGIN_DECLS

/* Compatibility macros */
#ifdef HAVE_FREETDS_VER0_63

#define TDSCONNECTINFO TDSCONNECTION
#define TDSCOLINFO     TDSCOLUMN
#define TDSMSGINFO     TDSMESSAGE

#endif  /* HAVE_FREETDS_VER0_63 */

#define GDA_TYPE_FREETDS_PROVIDER            (gda_freetds_provider_get_type())
#define GDA_FREETDS_PROVIDER(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_FREETDS_PROVIDER, GdaFreeTDSProvider))
#define GDA_FREETDS_PROVIDER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_FREETDS_PROVIDER, GdaFreeTDSProviderClass))
#define GDA_IS_FREETDS_PROVIDER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE (obj, GDA_TYPE_FREETDS_PROVIDER))
#define GDA_IS_FREETDS_PROVIDER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GDA_TYPE_FREETDS_PROVIDER))

#define PARENT_TYPE GDA_TYPE_SERVER_PROVIDER
#define OBJECT_DATA_FREETDS_HANDLE "GDA_FreeTDS_FreeTDSHandle"


typedef struct _GdaFreeTDSProvider      GdaFreeTDSProvider;
typedef struct _GdaFreeTDSProviderClass GdaFreeTDSProviderClass;

struct _GdaFreeTDSProvider {
	GdaServerProvider provider;
};

struct _GdaFreeTDSProviderClass {
	GdaServerProviderClass parent_class;
};

typedef struct _GdaFreeTDSConnectionData GdaFreeTDSConnectionData;
struct _GdaFreeTDSConnectionData {
	gint            rc;         /* rc code of last operation */
	GPtrArray       *msg_arr;   /* array containing msgs from server */
	GPtrArray       *err_arr;   /* array containing error msgs from server */
	gchar           *database;  /* database we are connected to */
	
	TDSLOGIN        *login;     /* tds login struct */
#if defined(HAVE_FREETDS_VER0_63) || defined(HAVE_FREETDS_VER0_61_62) || defined(HAVE_FREETDS_VER0_60)
	TDSCONTEXT      *ctx;       /* tds context */
#endif
	TDSSOCKET       *tds;       /* connection handle */
	_TDSCONNECTINFO *config;    /* tds connect struct */

	/* Data just got at connection beginning */
	gchar          *server_id; /* Server identifier/version string */
	gboolean       is_sybase;  /* true if cnc to ASE, false for mssql */
	gulong         srv_ver;    /* Server version */
	gint           result_type;
};

GType              gda_freetds_provider_get_type (void);
GdaServerProvider *gda_freetds_provider_new (void);

G_END_DECLS

#endif

