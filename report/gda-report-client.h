/* GDA Report client library
 * Copyright (C) 2001 The Free Software Foundation
 *
 * AUTHORS:
 *	Rodrigo Moya <rodrigo@gnome-db.org>
 *
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

#if !defined(__gda_report_client_h__)
#  define __gda_report_client_h__

#include <gda-common-defs.h>

G_BEGIN_DECLS

typedef struct _GdaReportClient        GdaReportClient;
typedef struct _GdaReportClientClass   GdaReportClientClass;
typedef struct _GdaReportClientPrivate GdaReportClientPrivate;

#define GDA_TYPE_REPORT_CLIENT            (gda_report_client_get_type ())
#define GDA_REPORT_CLIENT(obj)            GTK_CHECK_CAST(obj, GDA_TYPE_REPORT_CLIENT, GdaReportClient)
#define GDA_REPORT_CLIENT_CLASS(klass)    GTK_CHECK_CLASS_CAST(klass, GDA_TYPE_REPORT_CLIENT, GdaReportClientClass)
#define GDA_IS_REPORT_CLIENT(obj)         GTK_CHECK_TYPE(obj, GDA_TYPE_REPORT_CLIENT)
#define GDA_IS_REPORT_CLIENT_CLASS(klass) (GTK_CHECK_CLASS_TYPE((klass), GDA_TYPE_REPORT_CLIENT))

struct _GdaReportClient {
	GtkObject object;
	GdaReportClientPrivate *priv;
};

struct _GdaReportClientClass {
	GtkObjectClass parent_class;
};

GtkType          gda_report_client_get_type (void);
GdaReportClient *gda_report_client_construct (GdaReportClient *client,
					      const gchar *engine_id);
GdaReportClient *gda_report_client_new (void);
GdaReportClient *gda_report_client_new_with_engine (const gchar *engine_id);
void             gda_report_client_free (GdaReportClient *client);

G_END_DECLS

#endif
