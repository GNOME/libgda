/*
 * Copyright (C) 2019 Daniel Espinosa <esodan@gmail.com>
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

#include <glib/gi18n-lib.h>
#include <providers/sqlcipher/gda-sqlcipher-provider.h>
#include <libgda/sqlite/gda-sqlite.h>

// API routines from library
Sqlite3ApiRoutines *libapi;

/**
 * call it if you are implementing a new derived provider using
 * a different SQLite library, like SQLCipher
 */
static gpointer
gda_sqlcipher_provider_get_api_internal (GdaSqliteProvider *prov) {
  return libapi;
}

G_DEFINE_TYPE (GdaSqlcipherProvider, gda_sqlcipher_provider, GDA_TYPE_SQLITE_PROVIDER)

static void
gda_sqlcipher_provider_class_init (GdaSqlcipherProviderClass *klass)
{
  GModule *module2;

  module2 = gda_sqlite_find_library ("sqlcipher");
  if (module2)
    gda_sqlite_load_symbols (module2, &libapi);
  if (libapi == NULL) {
    g_warning (_("Can't find sqlcipher." G_MODULE_SUFFIX " file."));
  }
  GDA_SQLITE_PROVIDER_CLASS (gda_sqlcipher_provider_parent_class)->get_api = gda_sqlcipher_provider_get_api_internal;
}

static void
gda_sqlcipher_provider_init (GdaSqlcipherProvider *object) {
  g_object_set (G_OBJECT (object), "is-default", TRUE, NULL);
}


