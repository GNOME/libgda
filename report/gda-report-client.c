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

#include <GNOME_Database_Report.h>
#include "gda-report-client.h"
#include "gda-report-common.h"

struct _GdaReportClientPrivate {
	GNOME_Database_Report_DocumentFactory corba_engine;
	gchar *engine_id;
};

static void gda_report_client_class_init (GdaReportClientClass *klass);
static void gda_report_client_init       (GdaReportClient *client, GdaReportClientClass *klass);
static void gda_report_client_finalize   (GObject *object);

/*
 * GNOME::Database::Report::Client CORBA implementation
 */

/*
 * GdaReportClient class implementation
 */
static void
gda_report_client_class_init (GdaReportClientClass *klass)
{
	POA_GNOME_Database_Report_Client__epv *epv;
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = gda_report_client_finalize;

	/* set the epv */
	epv = &klass->epv;
}

static void
gda_report_client_init (GdaReportClient *client, GdaReportClientClass *klass)
{
	client->priv = g_new0 (GdaReportClientPrivate, 1);
}

static void
gda_report_client_finalize (GObject *object)
{
	GObjectClass *parent_class;
	CORBA_Environment ev;

	GdaReportClient *client = (GdaReportClient *) object;

	g_return_if_fail (GDA_IS_REPORT_CLIENT (client));

	/* free memory */
	CORBA_exception_init (&ev);
	CORBA_Object_release (client->priv->corba_engine, &ev);
	CORBA_exception_free (&ev);

	g_free (client->priv->engine_id);
	g_free (client->priv);
	client->priv = NULL;

	parent_class = g_type_class_peek (BONOBO_TYPE_OBJECT);
	if (parent_class && parent_class->finalize)
		parent_class->finalize (object);
}

BONOBO_TYPE_FUNC_FULL (GdaReportClient,
		       GNOME_Database_Report_Client,
		       BONOBO_TYPE_OBJECT,
		       gda_report_client);

/**
 * gda_report_client_construct
 */
GdaReportClient *
gda_report_client_construct (GdaReportClient *client, const gchar *engine_id)
{
	CORBA_Environment ev;

	g_return_val_if_fail (GDA_IS_REPORT_CLIENT (client));

	CORBA_exception_init (&ev);

	/* activate engine component */
	if (engine_id) {
		client->priv->corba_engine = bonobo_get_object (
			engine_id, "IDL:GNOME/Database/Report/Engine:1.0", &ev);
	}
	else {
		client->priv->corba_engine bonobo_get_object (
			GDA_COMPONENT_ID_REPORT,
			"IDL:GNOME/Database/Report/Engine:1.0",
			&ev);
	}
	client->priv->engine_id = g_strdup (engine_id);

	if (BONOBO_EX (&ev) || engine_id == CORBA_OBJECT_NIL) {
		gda_report_client_free (client);
		return NULL;
	}

	return client;
}

/**
 * gda_report_client_new
 */
GdaReportClient *
gda_report_client_new (void)
{
	GdaReportClient *client;

	client = GDA_REPORT_CLIENT (g_object_new (GDA_TYPE_REPORT_CLIENT, NULL));
	return gda_report_client_construct (client, NULL);
}

/**
 * gda_report_client_new_with_engine
 */
GdaReportClient *
gda_report_client_new_with_engine (const gchar *engine_id)
{
	GdaReportClient *client;

	client = GDA_REPORT_CLIENT (g_object_new (GDA_TYPE_REPORT_CLIENT, NULL));
	return gda_report_client_construct (client, engine_id);
}

/**
 * gda_report_client_free
 */
void
gda_report_client_free (GdaReportClient *client)
{
	g_return_if_fail (GDA_IS_REPORT_CLIENT (client));
	g_object_unref (G_OBJECT (client));
}
