/* GDA Report Engine
 * Copyright (C) 2000-2001 The Free Software Foundation
 * 
 * AUTHORS:
 * 	Rodrigo Moya <rodrigo@gnome-db.org>
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

#if !defined(__gda_report_engine_h__)
#  define __gda_report_engine_h__

#include <bonobo/bonobo-object.h>
#include <gda-common-defs.h>
#include <GNOME_Database_Report.h>

G_BEGIN_DECLS

typedef struct _GdaReportEngine        GdaReportEngine;
typedef struct _GdaReportEngineClass   GdaReportEngineClass;
typedef struct _GdaReportEnginePrivate GdaReportEnginePrivate;

#define GDA_TYPE_REPORT_ENGINE            (gda_report_engine_get_type ())
#define GDA_REPORT_ENGINE(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_REPORT_ENGINE, GdaReportEngine))
#define GDA_REPORT_ENGINE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_REPORT_ENGINE, GdaReportEngineClass))
#define GDA_IS_REPORT_ENGINE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE (obj, GDA_TYPE_REPORT_ENGINE))
#define GDA_IS_REPORT_ENGINE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GDA_TYPE_REPORT_ENGINE))

struct _GdaReportEngine {
	BonoboObject object;
	GdaReportEnginePrivate *priv;
};

struct _GdaReportEngineClass {
	BonoboObjectClass parent_class;

	POA_GNOME_Database_Report_Engine__epv epv;
};

GType            gda_report_engine_get_type (void);
GdaReportEngine *gda_report_engine_new (void);

G_END_DECLS

#endif
