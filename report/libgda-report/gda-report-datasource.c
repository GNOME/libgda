/* GDA report libary
 * Copyright (C) 1998-2001 The Free Software Foundation
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

#include <libgda-report/gda-report-datasource.h>

#define PARENT_TYPE BONOBO_X_OBJECT_TYPE

static void gda_report_datasource_class_init (GdaReportDatasourceClass *klass);
static void gda_report_datasource_init       (GdaReportDatasource *source,
					      GdaReportDatasourceClass *klass);
static void gda_report_datasource_finalize   (GObject *object);

static GObjectClass *parent_class = NULL;

