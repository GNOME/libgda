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

#include <libgda-report/gda-report-client.h>
#include <libgda-report/GNOME_Database_Report.h>
#include <bonobo/bonobo-exception.h>
#include <bonobo/bonobo-i18n.h>

#define PARENT_TYPE G_TYPE_OBJECT

struct _GdaReportClientPrivate {
	GNOME_Database_Report_Engine corba_engine;
};

static void gda_report_client_class_init (GdaReportClientClass *klass);
static void gda_report_client_init       (GdaReportClient *client, GdaReportClientClass *klass);
static void gda_report_client_finalize   (GObject *object);

static GObjectClass *parent_class = NULL;

/*
 * GdaReportClient class implementation
 */

static void
gda_report_client_class_init (GdaReportClientClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = gda_report_client_finalize;
}

static void
gda_report_client_init (GdaReportClient *client, GdaReportClientClass *klass)
{
	g_return_if_fail (GDA_IS_REPORT_CLIENT (client));

	/* allocate private structure */
	client->priv = g_new0 (GdaReportClientPrivate, 1);
	client->priv->corba_engine = CORBA_OBJECT_NIL;
}

static void
gda_report_client_finalize (GObject *object)
{
	GdaReportClient *client = (GdaReportClient *) object;

	g_return_if_fail (GDA_IS_REPORT_CLIENT (client));

	/* free memory */
	if (client->priv->corba_engine != CORBA_OBJECT_NIL) {
		CORBA_Environment ev;

		CORBA_exception_init (&ev);
		CORBA_Object_release (client->priv->corba_engine, &ev);
		CORBA_exception_free (&ev);

		client->priv->corba_engine = CORBA_OBJECT_NIL;
	}

	g_free (client->priv);
	client->priv = NULL;

	/* chain to parent class */
	parent_class->finalize (object);
}

GType
gda_report_client_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (GdaReportClientClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_report_client_class_init,
			NULL,
			NULL,
			sizeof (GdaReportClient),
			0,
			(GInstanceInitFunc) gda_report_client_init
		};
		type = g_type_register_static (PARENT_TYPE, "GdaReportClient", &info, 0);
	}
	return type;
}

/**
 * gda_report_client_new
 *
 * Create a new #GdaReportClient object, which lets you connect
 * to the GDA report engine.
 *
 * Returns: the newly created object.
 */
GdaReportClient *
gda_report_client_new (void)
{
	GdaReportClient *client;
	CORBA_Environment ev;

	client = g_object_new (GDA_TYPE_REPORT_CLIENT, NULL);

	/* load the GDA report engine */
	CORBA_exception_init (&ev);

	client->priv->corba_engine = bonobo_get_object (
		"OAFIID:GNOME_Database_Report_Engine",
		"IDL:GNOME/Database/Report/Engine:1.0",
		&ev);
	if (BONOBO_EX (&ev)) {
		gda_log_error (_("Could not activate report engine"));
		CORBA_exception_free (&ev);
		g_object_unref (G_OBJECT (client));
		return NULL;
	}

	CORBA_exception_free (&ev);

	return client;
}
