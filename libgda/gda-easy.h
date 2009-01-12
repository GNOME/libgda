/* GDA library
 * Copyright (C) 2008 The GNOME Foundation.
 *
 * AUTHORS:
 *      Vivien Malerba <malerba@gnome-db.org>
 *      Daniel Espinosa <esodan@gmail.com>
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

#ifndef __GDA_EASY_H__
#define __GDA_EASY_H__

#include <libgda/gda-connection.h>
#include <libgda/gda-server-operation.h>
#include <libgda/handlers/gda-handler-bin.h>
#include <libgda/handlers/gda-handler-boolean.h>
#include <libgda/handlers/gda-handler-numerical.h>
#include <libgda/handlers/gda-handler-string.h>
#include <libgda/handlers/gda-handler-time.h>
#include <libgda/handlers/gda-handler-type.h>

G_BEGIN_DECLS

extern GQuark gda_easy_error_quark (void);
#define GDA_EASY_ERROR gda_easy_error_quark ()

typedef enum  {
    GDA_EASY_OBJECT_NAME_ERROR,
    GDA_EASY_INCORRECT_VALUE_ERROR,
    GDA_EASY_OPERATION_ERROR
} GdaEasyError;

typedef enum
{
	GDA_EASY_CREATE_TABLE_NOTHING_FLAG   = 1 << 0,
	GDA_EASY_CREATE_TABLE_PKEY_FLAG      = 1 << 1,
	GDA_EASY_CREATE_TABLE_NOT_NULL_FLAG  = 1 << 2,
	GDA_EASY_CREATE_TABLE_UNIQUE_FLAG    = 1 << 3,
	GDA_EASY_CREATE_TABLE_AUTOINC_FLAG   = 1 << 4,
	GDA_EASY_CREATE_TABLE_FKEY_FLAG      = 1 << 5,
	/* Flags combinations */
	GDA_EASY_CREATE_TABLE_PKEY_AUTOINC_FLAG = GDA_EASY_CREATE_TABLE_PKEY_FLAG | GDA_EASY_CREATE_TABLE_AUTOINC_FLAG
} GdaEasyCreateTableFlag;

/* 
 * Convenient Functions
 */
GdaDataHandler     *gda_get_default_handler           (GType for_type);

/*
 * Quick commands execution
 */
GdaDataModel*       gda_execute_select_command        (GdaConnection *cnc, const gchar *sql, GError **error);
gint                gda_execute_non_select_command    (GdaConnection *cnc, const gchar *sql, GError **error);


/*
 * Database creation and destruction
 */
GdaServerOperation *gda_prepare_create_database       (const gchar *provider, const gchar *db_name, GError **error);
gboolean            gda_perform_create_database       (const gchar *provider, GdaServerOperation *op, GError **error);
GdaServerOperation *gda_prepare_drop_database         (const gchar *provider, const gchar *db_name, GError **error);
gboolean            gda_perform_drop_database         (const gchar *provider, GdaServerOperation *op, GError **error);

/*
 * Tables creation and destruction
 */
GdaServerOperation *gda_prepare_create_table	      (GdaConnection *cnc, const gchar *table_name, GError **error, ...);
gboolean            gda_perform_create_table          (GdaServerOperation *op, GError **error);
GdaServerOperation *gda_prepare_drop_table            (GdaConnection *cnc, const gchar *table_name, GError **error);
gboolean            gda_perform_drop_table            (GdaServerOperation *op, GError **error);

/*
 * Data in tables manipulation
 */
gboolean            gda_insert_row_into_table        (GdaConnection *cnc, const gchar *table, GError **error, ...);
gboolean            gda_update_row_in_table          (GdaConnection *cnc, const gchar *table, 
						      const gchar *condition_column_name, 
						      GValue *condition_value, GError **error, ...);
gboolean            gda_delete_row_from_table        (GdaConnection *cnc, const gchar *table, 
						      const gchar *condition_column_name, 
						      GValue *condition_value, GError **error);


G_END_DECLS

#endif
