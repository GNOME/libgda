/*
 * Copyright (C) 2009 - 2012 Vivien Malerba <malerba@gnome-db.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#include <string.h>
#include "reuse-all.h"

#include "postgres/gda-postgres-reuseable.h"
#include "mysql/gda-mysql-reuseable.h"

/**
 * _gda_provider_reuseable_new
 * @provider_name: a database provider name
 * @version_major: the major version number, or %NULL
 * @version_minor: the minor version number, or %NULL
 *
 * Entry point to get provider's reuseable features for a provider.
 *
 * Returns: a new GdaProviderReuseable pointer, or %NULL
 */
GdaProviderReuseable *
_gda_provider_reuseable_new (const gchar *provider_name)
{
	GdaProviderReuseable *reuseable = NULL;
	GdaProviderReuseableOperations *ops = NULL;
	g_return_val_if_fail (provider_name && *provider_name, NULL);
	if (!strcmp (provider_name, "PostgreSQL"))
		ops = _gda_postgres_reuseable_get_ops ();
	else if (!strcmp (provider_name, "MySQL"))
		ops = _gda_mysql_reuseable_get_ops ();

	if (ops) {
		reuseable = ops->re_new_data ();
		g_assert (reuseable->operations == ops);
	}

	return reuseable;
}
