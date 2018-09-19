/*
 * Copyright (C) 2006 - 2013 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2007 Murray Cumming <murrayc@murrayc.com>
 * Copyright (C) 2010 David King <davidk@openismus.com>
 * Copyright (C) 2018 Daniel Espinosa <esodan@gmail.com>
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


#ifndef __GDA_DATA_HANDLER_H_
#define __GDA_DATA_HANDLER_H_

#include <glib-object.h>
#include "gda-decl.h"
#include "gda-value.h"

G_BEGIN_DECLS

#define GDA_TYPE_DATA_HANDLER          (gda_data_handler_get_type())
G_DECLARE_INTERFACE(GdaDataHandler, gda_data_handler, GDA, DATA_HANDLER, GObject)

/* struct for the interface */
struct _GdaDataHandlerInterface
{
	GTypeInterface           g_iface;

	/* virtual table */
	gchar         *(* get_sql_from_value)   (GdaDataHandler *dh, const GValue *value);
	gchar         *(* get_str_from_value)   (GdaDataHandler *dh, const GValue *value);
	GValue        *(* get_value_from_sql)   (GdaDataHandler *dh, const gchar *sql, GType type);
	GValue        *(* get_value_from_str)   (GdaDataHandler *dh, const gchar *str, GType type);
	GValue        *(* get_sane_init_value)  (GdaDataHandler *dh, GType type);

	gboolean       (* accepts_g_type)       (GdaDataHandler *dh, GType type);
	const gchar   *(* get_descr)            (GdaDataHandler *dh);
};


/**
 * SECTION:gda-data-handler
 * @short_description: Interface which provides data handling (conversions) capabilities
 * @title: GdaDataHandler
 * @stability: Stable
 * @see_also:
 *
 * Because data types vary a lot from a DBMS to another, the #GdaDataHandler interface helps
 * managing data in its various representations, and converting from one to another:
 * <itemizedlist>
 *   <listitem><para>as a #GValue which is a generic value container for the C language</para></listitem>
 *   <listitem><para>as a human readable string (in the user defined locale)</para></listitem>
 *   <listitem><para>as an SQL string (a string which can be used in SQL statements)</para></listitem>
 * </itemizedlist>
 *
 * For each data type, a corresponding #GdaDataHandler object can be requested using the
 * <link linkend="gda-data-handler-get-default">gda_data_handler_get_default()</link> function. However, when working
 * with a specific database provider, it's better to use a #GdaDataHandler which may be specific to the
 * database provider which will correctly handle each database specifics using
 * <link linkend="gda-server-provider-get-data-handler-g-type">gda_server_provider_get_data_handler_g_type()</link> or
 * <link linkend="gda-server-provider-get-data-handler-dbms">gda_server_provider_get_data_handler_dbms()</link>.
 */


/* Simple data manipulation */
gchar         *gda_data_handler_get_sql_from_value     (GdaDataHandler *dh, const GValue *value);
gchar         *gda_data_handler_get_str_from_value     (GdaDataHandler *dh, const GValue *value);
GValue        *gda_data_handler_get_value_from_sql     (GdaDataHandler *dh, const gchar *sql, GType type);
GValue        *gda_data_handler_get_value_from_str     (GdaDataHandler *dh, const gchar *str, GType type);
GValue        *gda_data_handler_get_sane_init_value    (GdaDataHandler *dh, GType type);

/* information about the data handler itself */
gboolean       gda_data_handler_accepts_g_type         (GdaDataHandler *dh, GType type);
const gchar   *gda_data_handler_get_descr              (GdaDataHandler *dh);
GdaDataHandler *gda_data_handler_get_default           (GType for_type);

G_END_DECLS

#endif
