/* GDA report libary
 * Copyright (C) 1998-2002 The GNOME Foundation.
 *
 * AUTHORS:
 *	Rodrigo Moya <rodrigo@gnome-db.org>
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

#if !defined(__gda_report_datasource_h__)
#  define __gda_report_datasource_h__

#include <bonobo/bonobo-xobject.h>
#include <libgda-report/GNOME_Database_Report.h>

G_BEGIN_DECLS

#define GDA_TYPE_REPORT_DATASOURCE            (gda_report_datasource_get_type())
#define GDA_REPORT_DATASOURCE(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_REPORT_DATASOURCE, GdaReportDatasource))
#define GDA_REPORT_DATASOURCE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_REPORT_DATASOURCE, GdaReportDatasourceClass))
#define GDA_IS_REPORT_DATASOURCE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE(obj, GDA_TYPE_REPORT_DATASOURCE))
#define GDA_IS_REPORT_DATASOURCE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GDA_TYPE_REPORT_DATASOURCE))

typedef struct _GdaReportDatasource        GdaReportDatasource;
typedef struct _GdaReportDatasourceClass   GdaReportDatasourceClass;
typedef struct _GdaReportDatasourcePrivate GdaReportDatasourcePrivate;

struct _GdaReportDatasource {
	BonoboXObject object;
	GdaReportDatasourcePrivate *priv;
};

struct _GdaReportDatasourceClass {
	BonoboXObjectClass parent_class;
	POA_GNOME_Database_Report_DataSource__epv epv;
};

GType                gda_report_datasource_get_type (void);
GdaReportDatasource *gda_report_datasource_new (void);

G_END_DECLS

#endif
