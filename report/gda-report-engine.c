/* GDA Report Engine
 * Copyright (C) 2000 Rodrigo Moya <rodrigo@gnome-db.org>
 * Copyright (C) 2001 Carlos Perelló Marín <carlos@gnome-db.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <gda-report-defs.h>
#include <gda-report-engine.h>

enum {
	REPORT_ENGINE_ERROR,
	REPORT_ENGINE_WARNING,
	LAST_SIGNAL
};

static gint
gda_report_engine_signals[LAST_SIGNAL] = {0, };

#ifdef HAVE_GOBJECT
static void    gda_report_engine_class_init	(Gda_ReportEngineClass *klass,
						 gpointer data);
static void    gda_report_engine_init		(Gda_ReportEngine *object,
						 Gda_ReportEngineClass *klass);
#else
static void    gda_report_engine_class_init	(Gda_ReportEngineClass* klass);
static void    gda_report_engine_init		(Gda_ReportEngine* object);
#endif

static void    gda_report_engine_real_error	(Gda_ReportEngine* object, GList* errors);
static void    gda_report_engine_real_warning	(Gda_ReportEngine* object, GList* errors);

static void
gda_report_engine_real_error (Gda_ReportEngine* object, GList* errors)
{
	g_print("%s: %d: %s called\n", __FILE__, __LINE__, __PRETTY_FUNCTION__);
	object->errors_head = g_list_concat(object->errors_head, errors);
}

static void
gda_report_engine_real_warning (Gda_ReportEngine* object, GList* warnings)
{
	g_print("%s: %d: %s called\n", __FILE__, __LINE__, __PRETTY_FUNCTION__);
}

#ifdef HAVE_GOBJECT
GType
gda_report_engine_get_type (void)
{
	static GType type = 0;

	if (!type) {
		GTypeInfo info = {
			sizeof (Gda_ReportEngineClass),			/* class_size */
			NULL,						/* base_init */
			NULL,						/* base_finalize */
			(GClassInitFunc) gda_report_engine_class_init,	/* class_init */
			NULL,						/* class_finalize */
			NULL,						/* class_data */
			sizeof (Gda_ReportEngine),			/* instance_size */
			0,						/* n_preallocs */
			(GInstanceInitFunc) gda_report_engine_init,	/* instance_init */
			NULL,						/* value_table */
		};
		type = g_type_register_static (G_TYPE_OBJECT, "Gda_ReportEngine", &info);
	}
	return type;
}
#else
GtkType
gda_report_engine_get_type (void)
{
	static guint gda_report_engine_type = 0;

	if (!gda_report_engine_type) {
		GtkTypeInfo gda_report_engine_info = {
			"Gda_ReportEngine",
			sizeof (Gda_ReportEngine),
			sizeof (Gda_ReportEngineClass),
			(GtkClassInitFunc) gda_report_engine_class_init,
			(GtkObjectInitFunc) gda_report_engine_init,
			(GtkArgSetFunc)NULL,
			(GtkArgSetFunc)NULL,
		};
		gda_report_engine_type = gtk_type_unique(gtk_object_get_type(),
							 &gda_report_engine_info);
	}
	return gda_report_engine_type;
}
#endif

#ifdef HAVE_GOBJECT
static void
gda_report_engine_class_init (Gda_ReportEngineClass *klass, gpointer data)
{
	/* FIXME: No GObject signals yet */
	klass->error = gda_report_engine_real_error;
	klass->warning = gda_report_engine_real_warning;
}
#else
static void
gda_report_engine_class_init (Gda_ReportEngineClass* klass)
{
	GtkObjectClass*   object_class = GTK_OBJECT_CLASS(klass);

	gda_report_engine_signals[REPORT_ENGINE_ERROR] = \
					gtk_signal_new("error",
							GTK_RUN_FIRST,
							object_class->type,
							GTK_SIGNAL_OFFSET(Gda_ReportEngineClass, error),
							gtk_marshal_NONE__POINTER,
							GTK_TYPE_NONE, 1, GTK_TYPE_POINTER);
	gda_report_engine_signals[REPORT_ENGINE_WARNING] = \
					gtk_signal_new("warning",
							GTK_RUN_LAST,
							object_class->type,
							GTK_SIGNAL_OFFSET(Gda_ReportEngineClass, warning),
							gtk_marshal_NONE__POINTER,
							GTK_TYPE_NONE, 1, GTK_TYPE_POINTER);
	gtk_object_class_add_signals(object_class, gda_report_engine_signals, LAST_SIGNAL);
	klass->error   = gda_report_engine_real_error;
	klass->warning = gda_report_engine_real_warning;
}
#endif

static void
#ifdef HAVE_GOBJECT
gda_report_engine_init (Gda_ReportEngine* object, Gda_ReportEngineClass *klass)
#else
gda_report_engine_init (Gda_ReportEngine *object)
#endif
{
	g_return_if_fail(GDA_IS_REPORT_ENGINE(object));
}

 
/**
 * gda_report_engine_new
 *
 * Connect a client application to the report engine, as defined in the
 * system
 *
 * Returns: the pointer to the allocated object
 */
Gda_ReportEngine *
gda_report_engine_new (void)
{
	Gda_ReportEngine* engine;
	CORBA_Environment ev;

#ifdef HAVE_GOBJECT
	engine = GDA_REPORT_ENGINE (g_object_new (GDA_TYPE_REPORT_ENGINE, NULL));
#else
	engine = gtk_type_new(gda_report_engine_get_type());
#endif

	/* activate CORBA object */
	CORBA_exception_init(&ev);
	engine->corba_engine = oaf_activate_from_id(GDA_REPORT_OAFIID, 0, NULL, &ev);
	if (!CORBA_Object_is_nil(engine->corba_engine, &ev)) {
		/* FIXME: add authentication? */
		CORBA_exception_free(&ev);
		return engine;
	} else {
		/*FIXME: we must catch the exceptions*/
		CORBA_exception_free(&ev);
		/* free memory on error */
#ifdef HAVE_GOBJECT
		g_object_unref (G_OBJECT (engine));
#else
		gtk_object_destroy (GTK_OBJECT (engine));
#endif
		return NULL;
	}

}

/**
 * gda_report_engine_free
 * @object: the report engine
 *
 * If exists a report engine object, the object is freed.
 *
 */
void
gda_report_engine_free (Gda_ReportEngine *engine)
{
	CORBA_Environment ev;

	g_return_if_fail(engine != NULL);
	g_return_if_fail(engine->corba_engine != CORBA_OBJECT_NIL);

	/* deactivates CORBA server */
	CORBA_exception_init(&ev);
	CORBA_Object_release(engine->corba_engine, &ev);
	if (!gda_corba_handle_exception(&ev)) {
		gda_log_error(_("CORBA exception unloading report engine: %s"), CORBA_exception_id(&ev));
	}

	CORBA_exception_free(&ev);

#ifdef HAVE_GOBJECT
	g_object_unref (G_OBJECT (engine));
#else
	gtk_object_destroy (GTK_OBJECT (engine));
#endif
}

/**
 * gda_report_engine_query_reports
 */
GList *
gda_report_engine_query_reports (Gda_ReportEngine *engine, const gchar *condition, Gda_ReportFlags flags)
{
	GList*            list = NULL;
	CORBA_Environment ev;
	GDA_ReportList*   report_list;

	g_return_val_if_fail(engine != NULL, NULL);

	CORBA_exception_init(&ev);
	report_list = GDA_ReportEngine_queryReports(engine->corba_engine,
						    (CORBA_char *) condition,
						    flags,
						    &ev);
	if (gda_corba_handle_exception(&ev)) {

	}
	CORBA_exception_free(&ev);
	return list;
}
