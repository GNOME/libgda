/* GDA Report Engine
 * Copyright (C) 2000 Rodrigo Moya
 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <gda-report-server.h>

static GCache* report_cache = NULL;

/*
 * Private functions
 */
static void
destroy_key_func (gpointer value)
{
}

static void
destroy_value_func (gpointer value)
{
}

static gpointer
dup_key_func (gpointer value)
{
}

static guint
hash_key_func (gconstpointer key)
{
}

static guint hash_value_func (gconstpointer key)
{
}

static gint
key_compare_func (gconstpointer a, gconstpointer b)
{
}

static gpointer
new_value_func (gpointer key)
{
}

/*
 * Public functions
 */
void
report_server_cache_init (void)
{
  static gboolean initialized = FALSE;
  
  if (!initialized)
    {
      report_cache = g_cache_new((GCacheNewFunc) new_value_func,
                                 (GCacheDestroyFunc) destroy_value_func,
                                 (GCacheDupFunc) dup_key_func,
                                 (GCacheDestroyFunc) destroy_key_func,
                                 (GHashFunc) hash_key_func,
                                 (GHashFunc) hash_value_func,
                                 (GCompareFunc) key_compare_func);
    }
  initialized = TRUE;
}