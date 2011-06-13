/*
 * Copyright (C) 2006 - 2010 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2007 Murray Cumming <murrayc@murrayc.com>
 * Copyright (C) 2010 David King <davidk@openismus.com>
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
#define GDA_DATA_HANDLER(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_DATA_HANDLER, GdaDataHandler)
#define GDA_IS_DATA_HANDLER(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, GDA_TYPE_DATA_HANDLER)
#define GDA_DATA_HANDLER_GET_IFACE(obj) (G_TYPE_INSTANCE_GET_INTERFACE ((obj), GDA_TYPE_DATA_HANDLER, GdaDataHandlerIface))


/* struct for the interface */
struct _GdaDataHandlerIface
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




GType        gda_data_handler_get_type               (void) G_GNUC_CONST;

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
