/* GDA report engine
 * Copyright (C) 1998-2002 The GNOME Foundation.
 *
 * AUTHORS:
 *      Carlos Perelló <carlos@gnome-db.org>
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

#include <bonobo/bonobo-exception.h>
#include <bonobo/bonobo-i18n.h>
#include "gda-report-engine.h"

#define PARENT_TYPE BONOBO_X_OBJECT_TYPE

struct _GdaReportEnginePrivate {
};

static void gda_report_engine_class_init (GdaReportEngineClass *klass);
static void gda_report_engine_init       (GdaReportEngine *engine, GdaReportEngineClass *klass);
static void gda_report_engine_finalize   (GObject *object);

static GObjectClass *parent_class = NULL;
static GdaClient *db_client = NULL;

/*
 * CORBA methods implementation
 */

static GNOME_Database_Report_Output
impl_ReportEngine_runDocument (PortableServer_Servant servant,
			       const Bonobo_Stream xml,
			       const GNOME_Database_ParameterList *params,
			       CORBA_Environment *ev)
{
	CORBA_long res;
	gchar *body;
	GNOME_Database_Report_Output output;
	GdaParameterList *plist;
	GdaReportEngine *engine = bonobo_x_object (servant);

	bonobo_return_val_if_fail (GDA_IS_REPORT_ENGINE (engine), CORBA_OBJECT_NIL, ev);
	bonobo_return_val_if_fail (xml != CORBA_OBJECT_NIL, CORBA_OBJECT_NIL, ev);

	/* read the report file from the stream */
	res = bonobo_stream_client_read_string (xml, &body, ev);
	if (BONOBO_EX (ev) || res == -1) {
		gda_log_error (_("CORBA exception: %s"),
			       bonobo_exception_get_text (ev));
		return CORBA_OBJECT_NIL;
	}

	/* process the report file */
	plist = gda_parameter_list_new_from_corba (params);
	output = engine_job_process_report (engine, body, plist, ev);

	g_free (body);
	gda_parameter_list_free (plist);

	return BONOBO_IS_OBJECT (output) ? 
		bonobo_object_corba_objref (BONOBO_OBJECT (output)) : CORBA_OBJECT_NIL;
}

/*
 * GdaReportEngine class implementation
 */

static void
gda_report_engine_class_init (GdaReportEngineClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	POA_GNOME_Database_Report_Engine__epv *epv;

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = gda_report_engine_finalize;

	/* set the epv */
	epv = &klass->epv;
	epv->runDocument = impl_ReportEngine_runDocument;

	/* create the global GDA client */
	if (!db_client) {
		db_client = gda_client_new ();
		if (!GDA_IS_CLIENT (db_client)) {
			gda_log_error (_("Could not create global GDA client"));
			exit (-1);
		}
	}
}

static void
gda_report_engine_init (GdaReportEngine *engine, GdaReportEngineClass *klass)
{
	g_return_if_fail (GDA_IS_REPORT_ENGINE (engine));

	/* allocate private structure */
	engine->priv = g_new0 (GdaReportEnginePrivate, 1);
}

static void
gda_report_engine_finalize (GObject *object)
{
	GdaReportEngine *engine = (GdaReportEngine *) object;

	g_return_if_fail (GDA_IS_REPORT_ENGINE (engine));

	/* free memory */
	g_free (engine->priv);
	engine->priv = NULL;

	/* chain to parent class */
	parent_class->finalize (object);
}

BONOBO_X_TYPE_FUNC_FULL (GdaReportEngine,
			 GNOME_Database_Report_Engine,
			 PARENT_TYPE,
			 gda_report_engine)

GdaReportEngine *
gda_report_engine_new (void)
{
	GdaReportEngine *engine;

	engine = g_object_new (GDA_TYPE_REPORT_ENGINE, NULL);
	return engine;
}

GdaClient *
gda_report_engine_get_gda_client (void)
{
	return db_client;
}
