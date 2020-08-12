/*
 * Copyright (C) 2000 Reinhard Müller <reinhard@src.gnome.org>
 * Copyright (C) 2000 - 2002 Rodrigo Moya <rodrigo@gnome-db.org>
 * Copyright (C) 2001 Carlos Perelló Marín <carlos@gnome-db.org>
 * Copyright (C) 2001 - 2014 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2002 Gonzalo Paniagua Javier <gonzalo@src.gnome.org>
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

#ifndef __GDA_DATA_SELECT_PRIV_H__
#define __GDA_DATA_SELECT_PRIV_H__

#include <glib-object.h>
#include <libgda/gda-row.h>
#include <libgda/providers-support/gda-pstmt.h>
#include <libgda/sql-parser/gda-sql-statement.h>
#include <libgda/gda-data-select.h>

G_BEGIN_DECLS

/**
 * SECTION:gda-data-select-priv
 * @short_description: Base class for all the data models returned by DBMS providers when a SELECT statement is executed
 * @title: Subclassing GdaDataSelect
 * @stability: Stable
 * @see_also: #GdaDataModel and #GdaDataSelect
 *
 * All database providers should subclass this class when returning a data model after the execution of a SELECT
 *  statement. Specifically it has the following features:
 *  <itemizedlist>
 *    <listitem><para>Manages its list of <link linkend="GdaColumn">GdaColumn</link> using the list exported by the prepared statement object (<link linkend="GdaPStmt">GdaPStmt</link>)</para></listitem>
 *    <listitem><para>Allows random or cursor based access</para></listitem>
 *    <listitem><para>Allows for efficient memory usage allowing the subclass to finely tune its memory usage</para></listitem>
 *    <listitem><para>Provides a generic mechanism for writable data models</para></listitem>
 *  </itemizedlist>
 *
 *  See the <link linkend="libgda-provider-recordset">Virtual methods for recordsets</link> section for more information
 *  about how to implement the virtual methods of the subclassed object.
 *
 *  This section documents the methods available for the database provider's implementations.
 */

GType          gda_data_select_get_type                     (void) G_GNUC_CONST;

/* API reserved to provider's implementations */
void           gda_data_select_take_row                     (GdaDataSelect *model, GdaRow *row, gint rownum);
GdaRow        *gda_data_select_get_stored_row               (GdaDataSelect *model, gint rownum);
GdaConnection *gda_data_select_get_connection               (GdaDataSelect *model);
void           gda_data_select_set_columns                  (GdaDataSelect *model, GSList *columns);

void           gda_data_select_add_exception                (GdaDataSelect *model, GError *error);

/* internal API */
void           _gda_data_select_share_private_data (GdaDataSelect *master, GdaDataSelect *slave);

G_END_DECLS

#endif
