/* GDA mysql provider
 * Copyright (C) 2008 The GNOME Foundation.
 *
 * AUTHORS:
 *      Carlos Savoretti <csavoretti@gmail.com>
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

#ifndef __GDA_MYSQL_PROVIDER_H__
#define __GDA_MYSQL_PROVIDER_H__

#include <libgda/gda-server-provider.h>

#define GDA_TYPE_MYSQL_PROVIDER            (gda_mysql_provider_get_type())
#define GDA_MYSQL_PROVIDER(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_MYSQL_PROVIDER, GdaMysqlProvider))
#define GDA_MYSQL_PROVIDER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_MYSQL_PROVIDER, GdaMysqlProviderClass))
#define GDA_IS_MYSQL_PROVIDER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE (obj, GDA_TYPE_MYSQL_PROVIDER))
#define GDA_IS_MYSQL_PROVIDER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GDA_TYPE_MYSQL_PROVIDER))

typedef struct _GdaMysqlProvider      GdaMysqlProvider;
typedef struct _GdaMysqlProviderClass GdaMysqlProviderClass;

struct _GdaMysqlProvider {
	GdaServerProvider      provider;
	gboolean               test_mode; /* for tests ONLY */
	gboolean               test_identifiers_case_sensitive; /* for tests ONLY */
};

struct _GdaMysqlProviderClass {
	GdaServerProviderClass parent_class;
};

G_BEGIN_DECLS

GType gda_mysql_provider_get_type (void) G_GNUC_CONST;

G_END_DECLS

#endif
