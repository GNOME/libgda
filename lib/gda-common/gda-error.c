/* GDA common library
 * Copyright (C) 1998,1999 Michael Lausch
 * Copyright (C) 1999,2000,2001 Rodrigo Moya
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

#include "config.h"
#include "gda-error.h"
#include "GDA.h"

#ifdef ENABLE_NLS
#  include <libintl.h>
#  define _(String) gettext (String)
#  define N_(String) (String)
#else
/* Stubs that do something close enough. */
#  define textdomain(String)
#  define gettext(String) (String)
#  define dgettext(Domain,Message) (Message)
#  define dcgettext(Domain,Message,Type) (Message)
#  define bindtextdomain(Domain,Directory)
#  define _(String) (String)
#  define N_(String) (String)
#endif

enum {
	/* add signals here */
	GDA_ERROR_LAST_SIGNAL
};

static gint gda_error_signals[GDA_ERROR_LAST_SIGNAL] = { 0, };

#ifdef HAVE_GOBJECT
static void gda_error_class_init (GdaErrorClass *klass, gpointer data);
static void gda_error_init       (GdaError *error, GdaErrorClass *klass);
#else
static void gda_error_class_init (GdaErrorClass *klass);
static void gda_error_init       (GdaError *error);
static void gda_error_destroy    (GtkObject *object);
#endif

#ifdef HAVE_GOBJECT
GType
gda_error_get_type (void)
{
	static GType type = 0;

	if (!type) {
		GTypeInfo info = {
			sizeof (GdaErrorClass),
			NULL,
			NULL,
			(GClassInitFunc) gda_error_class_init,
			NULL,
			NULL,
			sizeof (GdaError),
			0,
			(GInstanceInitFunc) gda_error_init,
			NULL,
		};
		type = g_type_register_static (G_TYPE_OBJECT, "GdaError", &info, 0);
	}
	return type;
}
#else
guint
gda_error_get_type (void)
{
	static guint type = 0;

	if (!type) {
		GtkTypeInfo info = {
			"GdaError",
			sizeof (GdaError),
			sizeof (GdaErrorClass),
			(GtkClassInitFunc) gda_error_class_init,
			(GtkObjectInitFunc) gda_error_init,
			(GtkArgSetFunc) NULL,
			(GtkArgSetFunc) NULL,
		};
		type = gtk_type_unique(gtk_object_get_type(), &info);
	}
	return type;
}
#endif

#ifdef HAVE_GOBJECT
static void
gda_error_class_init (GdaErrorClass *klass, gpointer data)
{
}
#else
static void
gda_error_class_init (GdaErrorClass* klass)
{
	GtkObjectClass*   object_class;

	object_class = (GtkObjectClass*) klass;
	object_class->destroy = gda_error_destroy;
}
#endif

static void
#ifdef HAVE_GOBJECT
gda_error_init (GdaError *error, GdaErrorClass *klass)
#else
gda_error_init (GdaError* error)
#endif
{
	g_return_if_fail(GDA_IS_ERROR(error));
  
	error->description = NULL;
	error->number = -1;
	error->source = NULL;
	error->helpurl = NULL;
	error->helpctxt = NULL;
	error->sqlstate = NULL;
	error->native = NULL;
	error->realcommand = NULL;
}

/*
 * gda_error_new:
 * Creates a new unitialized error object.
 *
 * Returns: the error object.
 */
GdaError *
gda_error_new (void)
{
	GdaError* error;

#ifdef HAVE_GOBJECT
	error = GDA_ERROR (g_object_new (GDA_TYPE_ERROR, NULL));
#else
	error = GDA_ERROR(gtk_type_new(gda_error_get_type()));
#endif
	return error;
}

GList*
gda_error_list_from_exception (CORBA_Environment* ev)
{
	GList*    all_errors = 0;
	GdaError* error;

	g_return_val_if_fail(ev != 0, 0);

	switch(ev->_major) {
	case CORBA_NO_EXCEPTION:
		return NULL;
	case CORBA_SYSTEM_EXCEPTION: {
		CORBA_SystemException* sysexc;

		sysexc = CORBA_exception_value(ev);
		error = gda_error_new();
		error->source = g_strdup("[CORBA System Exception]");
		switch(sysexc->minor) {
		case ex_CORBA_COMM_FAILURE:
			error->description = g_strdup_printf(_("%s: The server didn't respond."),
							     CORBA_exception_id(ev));
			break;
		default:
			error->description = g_strdup(_("%s: An Error occured in the CORBA system."));
			break;
		}
		all_errors = g_list_append(all_errors, error);
	}
	case CORBA_USER_EXCEPTION: {
		if (strcmp(CORBA_exception_id(ev), ex_GDA_NotSupported) == 0) {
			GDA_NotSupported* not_supported_error;

			not_supported_error = (GDA_NotSupported*)ev->_params;
			error = gda_error_new ();
			gda_error_set_source (error, _("[CORBA User Exception]"));
			gda_error_set_description (error, not_supported_error->errormsg);
			all_errors = g_list_append (all_errors, error);
		}
		else if (strcmp(CORBA_exception_id(ev), ex_GDA_DriverError) == 0) {
			GDA_ErrorSeq  error_sequence = ((GDA_DriverError*)ev->_params)->errors;
			gint          idx;
	    
			for (idx = 0; idx < error_sequence._length; idx++) {
				GDA_Error* gda_error = &error_sequence._buffer[idx];

				error = gda_error_new();
				if (gda_error->source)
					gda_error_set_source (error, gda_error->source);
				gda_error_set_number (error, gda_error->number);
				if (gda_error->sqlstate)
					gda_error_set_sqlstate (error, gda_error->sqlstate);
				if (gda_error->nativeMsg)
					gda_error_set_native (error, gda_error->nativeMsg);
				if (gda_error->description)
					gda_error_set_description (error, gda_error->description);
				all_errors = g_list_append(all_errors, error);
			}
		}
		break;
	}
	default:
		g_error("Unknown CORBA exception for connection");
	}

	return all_errors;
}

void
gda_error_to_exception (GdaError *error, CORBA_Environment *ev)
{
	GList *error_list = NULL;

	g_return_if_fail (error != NULL);
	g_return_if_fail (ev != NULL);

	error_list = g_list_append (error_list, error);
	gda_error_list_to_exception (error_list, ev);
	g_list_free (error_list);
}

void
gda_error_list_to_exception (GList *error_list, CORBA_Environment *ev)
{
	GDA_DriverError* exception;
	gint count;

	g_return_if_fail (error_list != NULL);
	g_return_if_fail (ev != NULL);

	count = g_list_length (error_list);

	exception = GDA_DriverError__alloc ();
	exception->errors._length = count;
	exception->errors._buffer = gda_error_list_to_corba_seq (error_list);
	exception->realcommand = CORBA_string_dup (g_get_prgname ());
	CORBA_exception_set (ev, CORBA_USER_EXCEPTION, ex_GDA_DriverError, exception);
}

GDA_ErrorSeq *
gda_error_list_to_corba_seq (GList *error_list)
{
	GList *l;
	gint count;
	gint i;
	GDA_ErrorSeq* rc;

	count = error_list != NULL ? g_list_length (error_list) : 0;

	rc = GDA_ErrorSeq__alloc();
	CORBA_sequence_set_release (rc, TRUE);
	rc->_length = count;
	rc->_buffer = CORBA_sequence_GDA_Error_allocbuf (count);

	for (l = g_list_first (error_list), i = 0;
	     l != NULL;
	     l = g_list_next (l), i++) {
		GdaError *error = GDA_ERROR (l->data);

		if (GDA_IS_ERROR (error)) {
			rc->_buffer[i].description = CORBA_string_dup (gda_error_get_description (error));
			rc->_buffer[i].number = error->number;
			rc->_buffer[i].source = CORBA_string_dup (gda_error_get_source (error));
			rc->_buffer[i].sqlstate = CORBA_string_dup (gda_error_get_sqlstate (error));
			rc->_buffer[i].nativeMsg = CORBA_string_dup (gda_error_get_native (error));
		}
	}

	return rc;
}

static void
gda_error_destroy (GtkObject *object)
{
	GtkObjectClass *parent_class;
	GdaError *error = (GdaError *) object;

	g_return_if_fail(GDA_IS_ERROR(error));
  
	if (error->description)
		g_free (error->description);
	if (error->source)
		g_free (error->source);
	if (error->helpurl)
		g_free (error->helpurl);
	if (error->helpctxt)
		g_free (error->helpctxt);
	if (error->sqlstate)
		g_free (error->sqlstate);
	if (error->native)
		g_free (error->native);
	if (error->realcommand)
		g_free (error->realcommand);

	parent_class = gtk_type_class (gtk_object_get_type ());
	parent_class->destroy (object);
}

/**
 * gda_error_free:
 * @error: the error object.
 *
 * Frees the memory allocated by the error object.
 *
 */
void
gda_error_free (GdaError* error)
{
#ifdef HAVE_GOBJECT
	g_object_unref (G_OBJECT (error));
#else
	gtk_object_destroy (GTK_OBJECT (error));
#endif
}

/**
 * gda_error_list_free:
 * @errors: a glist holding error objects.
 *
 * Frees all error objects in the list and the list itself.
 * After this function has been called, the @errors parameter doesn't point
 * to valid storage any more.
 */
void
gda_error_list_free (GList* errors)
{
	GList* ptr = errors;

	g_return_if_fail(errors != 0);
  
	while (ptr) {
		GdaError* error = ptr->data;
		gda_error_free(error);
		ptr = g_list_next(ptr);
	}
	g_list_free(errors);
}

/**
 * gda_error_get_description
 */
const gchar*
gda_error_get_description (GdaError* error)
{
	g_return_val_if_fail (GDA_IS_ERROR(error), NULL);
	return error->description;
}

void
gda_error_set_description (GdaError *error, const gchar *description)
{
	g_return_if_fail (GDA_IS_ERROR (error));

	if (error->description)
		g_free (error->description);
	error->description = g_strdup (description);
}

const glong
gda_error_get_number (GdaError* error)
{
	g_return_val_if_fail(GDA_IS_ERROR(error), -1);
	return error->number;
}

void
gda_error_set_number (GdaError *error, glong number)
{
	g_return_if_fail (GDA_IS_ERROR (error));
	error->number = number;
}

const gchar*
gda_error_get_source (GdaError* error)
{
	g_return_val_if_fail (GDA_IS_ERROR (error), 0);
	return error->source;
}

void
gda_error_set_source (GdaError *error, const gchar *source)
{
	g_return_if_fail (GDA_IS_ERROR (error));

	if (error->source)
		g_free (error->source);
	error->source = g_strdup (source);
}

const gchar*
gda_error_get_help_url (GdaError* error)
{
	g_return_val_if_fail (GDA_IS_ERROR (error), 0);
	return error->helpurl;
}

void
gda_error_set_help_url (GdaError *error, const gchar *helpurl)
{
	g_return_if_fail (GDA_IS_ERROR (error));

	if (error->helpurl)
		g_free (error->helpurl);
	error->helpurl = g_strdup (helpurl);
}

const gchar *
gda_error_get_help_context (GdaError *error)
{
	g_return_val_if_fail (GDA_IS_ERROR (error), NULL);
	return (const gchar *) error->helpctxt;
}

void
gda_error_set_help_context (GdaError *error, const gchar *helpctxt)
{
	g_return_if_fail (GDA_IS_ERROR (error));

	if (error->helpctxt)
		g_free (error->helpctxt);
	error->helpctxt = g_strdup (helpctxt);
}

const gchar*
gda_error_get_sqlstate (GdaError* error)
{
	g_return_val_if_fail (GDA_IS_ERROR (error), NULL);
	return error->sqlstate;
}

void
gda_error_set_sqlstate (GdaError *error, const gchar *sqlstate)
{
	g_return_if_fail (GDA_IS_ERROR (error));

	if (error->sqlstate)
		g_free (error->sqlstate);
	error->sqlstate = g_strdup (sqlstate);
}

const gchar*
gda_error_get_native (GdaError* error)
{
	g_return_val_if_fail (GDA_IS_ERROR (error), NULL);
	return error->native;
}

void
gda_error_set_native (GdaError *error, const gchar *native)
{
	g_return_if_fail (GDA_IS_ERROR (error));

	if (error->native)
		g_free (error->native);
	error->native = g_strdup (native);
}

const gchar*
gda_error_get_real_command (GdaError* error)
{
	g_return_val_if_fail (GDA_IS_ERROR (error), NULL);
	return error->realcommand;
}

void
gda_error_set_real_command (GdaError *error, const gchar *realcommand)
{
	g_return_if_fail (GDA_IS_ERROR (error));

	if (error->realcommand)
		g_free (error->realcommand);
	error->realcommand = g_strdup (realcommand);
}
