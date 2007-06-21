/* GDA FireBird Provider
 * Copyright (C) 1998-2002 The GNOME Foundation
 *
 * AUTHORS:
 *         Albi Jeronimo <jeronimoalbi@yahoo.com.ar>
 *         Rodrigo Moya <rodrigo@gnome-db.org>
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

#if !defined(__gda_firebird_provider_h__)
#  define __gda_firebird_provider_h__

#include <libgda/gda-server-provider.h>
#include <ibase.h>


G_BEGIN_DECLS

#define CONNECTION_DATA "GDA_Firebird_ConnectionData"
#define TRANSACTION_DATA "GDA_Firebird_TransactionData"
#define STATEMENT_DATA "GDA_Firebird_StatementData"

#define GDA_TYPE_FIREBIRD_PROVIDER            (gda_firebird_provider_get_type())
#define GDA_FIREBIRD_PROVIDER(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_FIREBIRD_PROVIDER, GdaFirebirdProvider))
#define GDA_FIREBIRD_PROVIDER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_FIREBIRD_PROVIDER, GdaFirebirdProviderClass))
#define GDA_IS_FIREBIRD_PROVIDER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE (obj, GDA_TYPE_FIREBIRD_PROVIDER))
#define GDA_IS_FIREBIRD_PROVIDER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GDA_TYPE_FIREBIRD_PROVIDER))

#define PARENT_TYPE GDA_TYPE_SERVER_PROVIDER

typedef struct _GdaFirebirdProvider      GdaFirebirdProvider;
typedef struct _GdaFirebirdProviderClass GdaFirebirdProviderClass;

struct _GdaFirebirdProvider {
	GdaServerProvider provider;
};

struct _GdaFirebirdProviderClass {
	GdaServerProviderClass parent_class;
};

GType              gda_firebird_provider_get_type (void) G_GNUC_CONST;
GdaServerProvider *gda_firebird_provider_new (void);

void               gda_firebird_connection_make_error (GdaConnection *cnc, 
						       const gint statement_type);

G_END_DECLS

#endif
