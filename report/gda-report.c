/* libgda library
 *
 * Copyright (C) 2001 Carlos Perelló Marín <carlos@gnome-db.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Library General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <gda-report.h>


enum {
	REPORT_ERROR,
	REPORT_WARNING,
	LAST_SIGNAL
};

static gint
gda_report_signals[LAST_SIGNAL] = {0, };


#ifdef HAVE_GOBJECT
static void    gda_report_class_init   (Gda_ReportClass *klass,
					gpointer data);
static void    gda_report_init         (Gda_Report *object,
					Gda_ReportClass *klass);
#else
static void    gda_report_class_init   (Gda_ReportClass* klass);
static void    gda_report_init         (Gda_Report* object);
#endif

static void    gda_report_real_error   (Gda_Report* object, GList* errors);
static void    gda_report_real_warning (Gda_Report* object, GList* errors);

static void
gda_report_real_error (Gda_Report* object, GList* errors)
{
  g_print("%s: %d: %s called\n", __FILE__, __LINE__, __PRETTY_FUNCTION__);
  object->errors_head = g_list_concat(object->errors_head, errors);
}

static void
gda_report_real_warning (Gda_Report* object, GList* warnings)
{
  g_print("%s: %d: %s called\n", __FILE__, __LINE__, __PRETTY_FUNCTION__);
}

#ifdef HAVE_GOBJECT
GType
gda_report_get_type (void)
{
	static GType type = 0;

	if (!type) {
		GTypeInfo info = {
			sizeof (Gda_ReportClass),		/* class_size */
			NULL,					/* base_init */
			NULL,					/* base_finalize */
			(GClassInitFunc) gda_report_class_init,	/* class_init */
			NULL,					/* class_finalize */
			NULL,					/* class_data */
			sizeof (Gda_Report),			/* instance_size */
			0,					/* n_preallocs */
			(GInstanceInitFunc) gda_report_init,	/* instance_init */
			NULL,					/* value_table */
		};
		type = g_type_register_static (G_TYPE_OBJECT, "Gda_Report", &info);
	}
	return type;
}
#else
GtkType
gda_report_get_type (void)
{
	static guint gda_report_type = 0;

	if (!gda_report_type) {
		GtkTypeInfo gda_report_info = {
			"Gda_Report",
			sizeof (Gda_Report),
			sizeof (Gda_ReportClass),
			(GtkClassInitFunc) gda_report_class_init,
			(GtkObjectInitFunc) gda_report_init,
			(GtkArgSetFunc)NULL,
			(GtkArgSetFunc)NULL,
		};
		gda_report_type = gtk_type_unique(gtk_object_get_type(),
						  &gda_report_info);
	}
	return gda_report_type;
}
#endif

#ifdef HAVE_GOBJECT
static void
gda_report_class_init (Gda_ReportClass *klass, gpointer data)
{
	/* FIXME: No GObject signals yet */
	klass->error = gda_report_real_error;
	klass->warning = gda_report_real_warning;
}
#else
static void
gda_report_class_init (Gda_ReportClass* klass)
{
	GtkObjectClass*   object_class = GTK_OBJECT_CLASS(klass);
  
	gda_report_signals[REPORT_ERROR] = \
				gtk_signal_new("error",
						GTK_RUN_FIRST,
						object_class->type,
						GTK_SIGNAL_OFFSET(Gda_ReportClass, error),
						gtk_marshal_NONE__POINTER,
						GTK_TYPE_NONE, 1, GTK_TYPE_POINTER);
	gda_report_signals[REPORT_WARNING] = \
				gtk_signal_new("warning",
						GTK_RUN_LAST,
						object_class->type,
						GTK_SIGNAL_OFFSET(Gda_ReportClass, warning),
						gtk_marshal_NONE__POINTER,
						GTK_TYPE_NONE, 1, GTK_TYPE_POINTER);
	gtk_object_class_add_signals(object_class, gda_report_signals, LAST_SIGNAL);
	klass->error   = gda_report_real_error;
	klass->warning = gda_report_real_warning;
}
#endif

static void
#ifdef HAVE_GOBJECT
gda_report_init (Gda_Report* object, Gda_ReportClass *klass)
#else
gda_report_init (Gda_Report *object)
#endif
{
	g_return_if_fail(GDA_IS_REPORT(object));
	object->corba_report = CORBA_OBJECT_NIL;
	object->engine = NULL;
	object->rep_name = NULL;
	object->description = NULL;
	object->errors_head = NULL;
}

/**
 * gda_report_new:
 * @rep_name: The Report's name.
 * @description: A descripiton.
 *
 * Allocates space for a client report object
 *
 * Returns: the pointer to the allocated object
 */
Gda_Report*
gda_report_new (gchar* rep_name, gchar* description)
{
	Gda_Report* object;
	
#ifdef HAVE_GOBJECT
	object = GDA_REPORT (g_object_new (GDA_TYPE_REPORT, NULL));
#else
	object = gtk_type_new(gda_report_get_type());
#endif
	object->engine = NULL;
	if (rep_name) {
		object->rep_name = g_strdup(rep_name);	/* FIXME: I don't know if we must use
						      gda_report_set_name */
	}
	if (description) {
		object->description = g_strdup(description); /* FIXME: I don't know if we must use
							  gda_report_set_description */
	}
	return object;
}

/**
 * gda_report_free:
 * @object: the report
 *
 * If exists a report object, the object is freed.
 *
 */
void
gda_report_free (Gda_Report* object)
{
	CORBA_Environment ev;
	
	g_return_if_fail(IS_GDA_REPORT(object));
	
	if (object->corba_report != CORBA_OBJECT_NIL) {
		CORBA_exception_init(&ev);
		CORBA_Object_release(object->corba_report, &ev);
		if (!gda_corba_handle_exception(&ev)) {
			gda_log_error(_("CORBA exception unloading report engine: %s"), CORBA_exception_id(&ev));
		}

		CORBA_exception_free(&ev);
	}
	g_free(object->engine);
	g_free(object->rep_name);
	g_free(object->description);
	
#ifdef HAVE_GOBJECT
	g_object_unref (G_OBJECT (object));
#else
	gtk_object_destroy (GTK_OBJECT (object));
#endif
}

/**
 * gda_report_get_name:
 * @object: The Report object.
 *
 * Returns: The name of the report, NULL if it doesn't have name. You must free the
 *	    string after use it.
 *
 */
gchar*
gda_report_get_name (Gda_Report* object)
{

	g_return_if_fail(IS_GDA_REPORT(object));
	
	g_return_val_if_fail(object->rep_name != NULL, NULL);

	return g_strdup(object->rep_name);
}

/**
 * gda_report_set_name:
 * @object: The Report object.
 * @name: The Report's name.
 *
 * Returns: -1 on error, 0 on success
 *
 */
gint
gda_report_set_name (Gda_Report* object, gchar* name)
{
	CORBA_Environment ev;
	
	g_return_val_if_fail(IS_GDA_REPORT(object), -1);
	g_return_val_if_fail(name != NULL, -1);

	if (object->rep_name) {
		g_free(object->rep_name);
	}
	object->rep_name = g_strdup(name);
	
	if(object->corba_report != CORBA_OBJECT_NIL) {
		GDA_Report__set_name(object->corba_report, name, &ev);
	}
	/*  if (gda_corba_exception(object, &ev))
	return -1;
	else*/
	CORBA_exception_free(&ev);
	return 0;
}

/**
 * gda_report_get_description:
 * @object: The Report object.
 *
 * Returns: The description of the report, NULL if it doesn't have name. You must free the
 *	    string after use it.
 *
 */
gchar*
gda_report_get_description (Gda_Report* object)
{

	g_return_if_fail(IS_GDA_REPORT(object));
	
	g_return_val_if_fail(object->description != NULL, NULL);

	return g_strdup(object->description);
}

/**
 * gda_report_set_description:
 * @object: The Report object.
 * @name: The Report's description.
 *
 * Returns: -1 on error, 0 on success
 *
 */
gint
gda_report_set_description (Gda_Report* object, gchar* description)
{
	CORBA_Environment ev;
	
	g_return_val_if_fail(IS_GDA_REPORT(object), -1);
	g_return_val_if_fail(description != NULL, -1);

	if (object->description) {
		g_free(object->description);
	}
	object->description = g_strdup(description);
	
	if(object->corba_report != CORBA_OBJECT_NIL) {
		GDA_Report__set_description(object->corba_report, description, &ev);
	}
	/*  if (gda_corba_exception(object, &ev))
	return -1;
	else*/
	CORBA_exception_free(&ev);
	return 0;

}

/**
 * gda_report_get_elements:
 * @object: The Report object.
 *
 * Return: The main ReportElement
 *
 */
Gda_ReportElement*
gda_report_get_elements (Gda_Report* object)
{
	CORBA_Environment ev;
	Gda_ReportElement *element;
	gchar* elementName;
	
	g_return_val_if_fail(IS_GDA_REPORT(object), NULL);

	element = gda_report_element_new("");
	
	CORBA_exception_init(&ev);
	element->corba_report_element = GDA_Report__get_elements(object->corba_report, &ev);
	if (gda_corba_handle_exception(&ev)) {

	}
	CORBA_exception_init(&ev);
	elementName = (gchar *) GDA_ReportElement__get_name(element->corba_report_element, &ev);
	if (gda_corba_handle_exception(&ev)) {

	}
	element->name = g_strdup (elementName);
	CORBA_exception_free(&ev);
	return element;

}

/**
 * gda_report_set_elements:
 * @object: The Report object.
 * @element: The main ReportElement
 *
 * Return: -1 on error, 0 on success.
 *
 */
gint
gda_report_set_elements (Gda_Report* object, Gda_ReportElement* element)
{
	CORBA_Environment ev;
	
	g_return_val_if_fail(IS_GDA_REPORT(object), -1);
	g_return_val_if_fail(IS_GDA_REPORT_ELEMENT(element), -1);

	CORBA_exception_init(&ev);
	GDA_Report__set_elements(object->corba_report, element->corba_report_element, &ev);
	if (gda_corba_handle_exception(&ev)) {

	}
	CORBA_exception_free(&ev);
	return 0;

}

/**
 * gda_report_get_format:
 * @object: The Report object.
 *
 * Return: The format of the Report Object.
 *
 */
Gda_ReportFormat*
gda_report_get_format (Gda_Report* object)
{
	/* FIXME: Implement me, please!!! */
	return NULL;
}

/**
 * gda_report_isLocked:
 * @object: The Report object.
 *
 * Return: If the Report Object is locked ot not
 *
 */
gboolean
gda_report_isLocked (Gda_Report* object)
{
	CORBA_Environment ev;
	gboolean locked;
	
	g_return_val_if_fail(IS_GDA_REPORT(object), FALSE);

	CORBA_exception_init(&ev);
	locked = GDA_Report__get_isLocked(object->corba_report, &ev);
	if (gda_corba_handle_exception(&ev)) {

	}
	CORBA_exception_free(&ev);
	return locked;

}

/**
 * gda_report_run:
 * @object: The Report object.
 * @param:
 * @flags:
 *
 * Return: The result of running the report.
 *
 */
Gda_ReportOutput*
gda_report_run (Gda_Report* object, GList param, gint32 flags)
{
	/* FIXME: Implement me, please!!! */
	return NULL;
}

/**
 * gda_report_lock:
 * @object: The Report object.
 *
 *
 */
void
gda_report_lock (Gda_Report* object)
{
	CORBA_Environment ev;
	
	g_return_if_fail(IS_GDA_REPORT(object));

	CORBA_exception_init(&ev);
	GDA_Report_lock(object->corba_report, &ev);
	if (gda_corba_handle_exception(&ev)) {

	}
	CORBA_exception_free(&ev);
}

/**
 * gda_report_unlock:
 * @object: The Report object.
 *
 *
 */
void
gda_report_unlock (Gda_Report* object)
{
	CORBA_Environment ev;
	
	g_return_if_fail(IS_GDA_REPORT(object));

	CORBA_exception_init(&ev);
	GDA_Report_unlock(object->corba_report, &ev);
	if (gda_corba_handle_exception(&ev)) {

	}
	CORBA_exception_free(&ev);
}
