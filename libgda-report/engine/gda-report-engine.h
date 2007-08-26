/* GDA 
 * Copyright (C) 2007 The GNOME Foundation.
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

#ifndef __GDA_REPORT_ENGINE_H__
#define __GDA_REPORT_ENGINE_H__

#include <glib-object.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libgda/gda-query.h>

#define GDA_TYPE_REPORT_ENGINE            (gda_report_engine_get_type())
#define GDA_REPORT_ENGINE(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_REPORT_ENGINE, GdaReportEngine))
#define GDA_REPORT_ENGINE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_REPORT_ENGINE, GdaReportEngineClass))
#define GDA_IS_REPORT_ENGINE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE (obj, GDA_TYPE_REPORT_ENGINE))
#define GDA_IS_REPORT_ENGINE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GDA_TYPE_REPORT_ENGINE))

G_BEGIN_DECLS

typedef struct _GdaReportEngine      GdaReportEngine;
typedef struct _GdaReportEngineClass GdaReportEngineClass;
typedef struct _GdaReportEnginePrivate GdaReportEnginePrivate;

struct _GdaReportEngine {
	GObject                 base;
	GdaReportEnginePrivate *priv;
};

struct _GdaReportEngineClass {
	GObjectClass            parent_class;
};

GType            gda_report_engine_get_type        (void) G_GNUC_CONST;

GdaReportEngine *gda_report_engine_new             (xmlNodePtr spec_node);
GdaReportEngine *gda_report_engine_new_from_string (const gchar *spec_string);
GdaReportEngine *gda_report_engine_new_from_file   (const gchar *spec_file_name);

void             gda_report_engine_declare_object  (GdaReportEngine *engine, GObject *object, const gchar *obj_name);
GObject         *gda_report_engine_find_declared_object (GdaReportEngine *engine, GType obj_type, const gchar *obj_name);

xmlNodePtr       gda_report_engine_run_as_node     (GdaReportEngine *engine, GError **error);
xmlDocPtr        gda_report_engine_run_as_doc      (GdaReportEngine *engine, GError **error);

G_END_DECLS

#endif
