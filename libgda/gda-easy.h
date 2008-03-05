/* GDA library
 * Copyright (C) 2008 The GNOME Foundation.
 *
 * AUTHORS:
 *      Vivien Malerba <malerba@gnome-db.org>
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

extern GQuark gda_general_error_quark (void);
#define GDA_GENERAL_ERROR gda_general_error_quark ()

typedef enum  {
    GDA_GENERAL_OBJECT_NAME_ERROR,
    GDA_GENERAL_INCORRECT_VALUE_ERROR,
    GDA_GENERAL_OPERATION_ERROR
} GdaGeneralError;

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
gboolean            gda_perform_create_database       (GdaServerOperation *op, GError **error);
GdaServerOperation *gda_prepare_drop_database         (const gchar *provider, const gchar *db_name, GError **error);
gboolean            gda_perform_drop_database         (GdaServerOperation *op, GError **error);

/*
 * Tables creation and destruction
 */
gboolean            gda_create_table                  (GdaConnection *cnc, const gchar *table_name, GError **error, ...);
gboolean            gda_drop_table                    (GdaConnection *cnc, const gchar *table_name, GError **error);

/*
 * Data in tables manipulation
 */
gboolean            gda_insert_row_into_table         (GdaConnection *cnc, const gchar *table_name, GError **error, ...);
gboolean            gda_insert_row_into_table_from_string  (GdaConnection *cnc, const gchar *table_name, GError **error, ...);
gboolean            gda_update_value_in_table         (GdaConnection *cnc, const gchar *table_name, 
						       const gchar *search_for_column, 
						       const GValue *condition, 
						       const gchar *column_name, 
						       const GValue *new_value, GError **error);
gboolean            gda_update_values_in_table        (GdaConnection *cnc, const gchar *table_name, 
						       const gchar *condition_column_name, 
						       const GValue *condition, 
						       GError **error, ...);
gboolean            gda_delete_row_from_table         (GdaConnection *cnc, const gchar *table_name, 
						       const gchar *condition_column_name, const GValue *condition, 
						       GError **error);


G_END_DECLS

#endif
