/*
 * Copyright (C) 2001 - 2002 Carlos Perelló Marín <carlos@gnome-db.org>
 * Copyright (C) 2001 - 2002 Rodrigo Moya <rodrigo@gnome-db.org>
 * Copyright (C) 2001 - 2014 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2002 Gonzalo Paniagua Javier <gonzalo@src.gnome.org>
 * Copyright (C) 2005 Denis Fortin <denis.fortin@free.fr>
 * Copyright (C) 2008 Przemysław Grzegorczyk <pgrzegorczyk@gmail.com>
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

#ifndef __GDA_SQLITE_H__
#define __GDA_SQLITE_H__

#include <glib.h>
#include <libgda/libgda.h>
#include <libgda/gda-data-handler.h>
#include <libgda/gda-connection-private.h>
#include <libgda/sqlite/gda-sqlite-provider.h>

#ifdef WITH_BDBSQLITE
  #include <dbsql.h>
  #include "gda-symbols-util.h"
  #define SQLITE3_CALL(x) (s3r->x)
#else
  #include <sqlite3.h>
  #define SQLITE3_CALL(p,x) (((Sqlite3ApiRoutines*) gda_sqlite_provider_get_api(p))->x)
  #if (SQLITE_VERSION_NUMBER < 3005000)
    typedef sqlite_int64 sqlite3_int64;
  #endif
#endif

/*
 * Provider's specific connection data
 */
typedef struct {
	GdaServerProviderConnectionData parent;
	sqlite3      *connection;
	GWeakRef     provider;
	gchar        *file;
	GHashTable   *types_hash; /* key = type name, value = pointer to a GType */
	GType        *types_array;/* holds GType values, pointed by @types_hash */
} SqliteConnectionData;

extern GHashTable *error_blobs_hash;


/**
 * API interface to SQLite derived providers
 */
typedef struct {
	int  (*sqlite3_bind_blob)(sqlite3_stmt*,int,const void*,int n,void(*)(void*));
	int  (*sqlite3_bind_double)(sqlite3_stmt*,int,double);
	int  (*sqlite3_bind_int)(sqlite3_stmt*,int,int);
	int  (*sqlite3_bind_int64)(sqlite3_stmt*,int,sqlite_int64);
	int  (*sqlite3_bind_null)(sqlite3_stmt*,int);
	int  (*sqlite3_bind_text)(sqlite3_stmt*,int,const char*,int n,void(*)(void*));
	int (*sqlite3_bind_zeroblob)(sqlite3_stmt*,int,int);
	int (*sqlite3_blob_bytes)(sqlite3_blob*);
	int (*sqlite3_blob_close)(sqlite3_blob*);
	int (*sqlite3_blob_open)(sqlite3*,const char*,const char*,const char*,sqlite3_int64,int,sqlite3_blob**);
	int (*sqlite3_blob_read)(sqlite3_blob*,void*,int,int);
	int (*sqlite3_blob_write)(sqlite3_blob*,const void*,int,int);

	int  (*sqlite3_busy_timeout)(sqlite3*,int ms);
	int  (*sqlite3_changes)(sqlite3*);
	int (*sqlite3_clear_bindings)(sqlite3_stmt*);
	int  (*sqlite3_close)(sqlite3*);
	int  (*sqlite3_close_v2)(sqlite3*);

	const void * (*sqlite3_column_blob)(sqlite3_stmt*,int iCol);
	int  (*sqlite3_column_bytes)(sqlite3_stmt*,int iCol);
	int  (*sqlite3_column_count)(sqlite3_stmt*pStmt);
	const char * (*sqlite3_column_database_name)(sqlite3_stmt*,int);
	const char * (*sqlite3_column_decltype)(sqlite3_stmt*,int i);
	double  (*sqlite3_column_double)(sqlite3_stmt*,int iCol);
	int  (*sqlite3_column_int)(sqlite3_stmt*,int iCol);
	sqlite_int64  (*sqlite3_column_int64)(sqlite3_stmt*,int iCol);
	const char * (*sqlite3_column_name)(sqlite3_stmt*,int);
	const char * (*sqlite3_column_origin_name)(sqlite3_stmt*,int);
	const char * (*sqlite3_column_table_name)(sqlite3_stmt*,int);
	const unsigned char * (*sqlite3_column_text)(sqlite3_stmt*,int iCol);
	int  (*sqlite3_column_type)(sqlite3_stmt*,int iCol);

	int (*sqlite3_config) (int, ...);

	int  (*sqlite3_create_function)(sqlite3*,const char*,int,int,void*,void (*xFunc)(sqlite3_context*,int,sqlite3_value**),void (*xStep)(sqlite3_context*,int,sqlite3_value**),void (*xFinal)(sqlite3_context*));
  int  (*sqlite3_create_function_v2)(sqlite3*,const char*,int,int,void*,void (*xFunc)(sqlite3_context*,int,sqlite3_value**),void (*xStep)(sqlite3_context*,int,sqlite3_value**),void (*xFinal)(sqlite3_context*),void(*xDestroy)(void*));
	int (*sqlite3_create_module)(sqlite3*,const char*,const sqlite3_module*,void*);
	sqlite3 * (*sqlite3_db_handle)(sqlite3_stmt*);
	int (*sqlite3_declare_vtab)(sqlite3*,const char*);

	int  (*sqlite3_errcode)(sqlite3*db);
	const char * (*sqlite3_errmsg)(sqlite3*);

	int  (*sqlite3_exec)(sqlite3*,const char*,sqlite3_callback,void*,char**);
	int (*sqlite3_extended_result_codes)(sqlite3*,int);
	int  (*sqlite3_finalize)(sqlite3_stmt*pStmt);
	void  (*sqlite3_free)(void*);
	void  (*sqlite3_free_table)(char**result);
	int  (*sqlite3_get_table)(sqlite3*,const char*,char***,int*,int*,char**);
	sqlite_int64  (*sqlite3_last_insert_rowid)(sqlite3*);

	void *(*sqlite3_malloc)(int);
	char * (*sqlite3_mprintf)(const char*,...);
	int  (*sqlite3_open)(const char*,sqlite3**);
	int  (*sqlite3_open_v2)(const char *filename, sqlite3 **ppDb, int flags, const char *zVfs);
	int  (*sqlite3_prepare)(sqlite3*,const char*,int,sqlite3_stmt**,const char**);
	int (*sqlite3_prepare_v2)(sqlite3*,const char*,int,sqlite3_stmt**,const char**);

	int  (*sqlite3_reset)(sqlite3_stmt*pStmt);
	void  (*sqlite3_result_blob)(sqlite3_context*,const void*,int,void(*)(void*));
	void  (*sqlite3_result_double)(sqlite3_context*,double);
	void  (*sqlite3_result_error)(sqlite3_context*,const char*,int);
	void  (*sqlite3_result_int)(sqlite3_context*,int);
	void  (*sqlite3_result_int64)(sqlite3_context*,sqlite_int64);
	void  (*sqlite3_result_null)(sqlite3_context*);
	void  (*sqlite3_result_text)(sqlite3_context*,const char*,int,void(*)(void*));

	int  (*sqlite3_step)(sqlite3_stmt*);
	int  (*sqlite3_table_column_metadata)(sqlite3*,const char*,const char*,const char*,char const**,char const**,int*,int*,int*);
	int (*sqlite3_threadsafe)(void);

	const void * (*sqlite3_value_blob)(sqlite3_value*);
	int  (*sqlite3_value_bytes)(sqlite3_value*);
	int  (*sqlite3_value_int)(sqlite3_value*);
	double (*sqlite3_value_double)(sqlite3_value*);
	sqlite3_int64 (*sqlite3_value_int64)(sqlite3_value*);
	const unsigned char * (*sqlite3_value_text)(sqlite3_value*);
	int  (*sqlite3_value_type)(sqlite3_value*);

	int  (*sqlite3_key)(sqlite3 *, const void *, int);
	int  (*sqlite3_key_v2)(sqlite3 *, const char *, const void *, int);
	int  (*sqlite3_rekey)(sqlite3 *, const void *, int);

	int  (*sqlite3_create_collation) (sqlite3*, const char *, int, void*, int(*xCompare)(void*,int,const void*,int,const void*));

	int (*sqlite3_enable_load_extension) (sqlite3 *, int);
} Sqlite3ApiRoutines;

extern Sqlite3ApiRoutines *s3r;

GModule *gda_sqlite_find_library (const gchar *name_part);
void     gda_sqlite_load_symbols (GModule *module, Sqlite3ApiRoutines **apilib);

#endif
