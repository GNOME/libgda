/* GDA common library
 * Copyright (C) 1998,1999 Michael Lausch
 * Copyright (C) 1999-2001 Rodrigo Moya
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

#include "gda-common-private.h"
#include "gda-error.h"
#include "GNOME_Database.h"

struct _GdaErrorPrivate {
	gchar *description;
	glong number;
	gchar *source;
	gchar *helpurl;
	gchar *helpctxt;
	gchar *sqlstate;
	gchar *native;
	gchar *realcommand;
};

static void gda_error_class_init (GdaErrorClass * klass);
static void gda_error_init (GdaError * error, GdaErrorClass *klass);
static void gda_error_finalize (GObject * object);

GType
gda_error_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (GdaErrorClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_error_class_init,
			NULL,
			NULL,
			sizeof (GdaError),
			0,
			(GInstanceInitFunc) gda_error_init
		};
		type = g_type_register_static (G_TYPE_OBJECT, "GdaError", &info, 0);
	}
	return type;
}

static void
gda_error_class_init (GdaErrorClass * klass)
{
	GObjectClass *object_class;

	object_class = (GObjectClass *) klass;
	object_class->finalize = gda_error_finalize;
}

static void
gda_error_init (GdaError * error, GdaErrorClass *klass)
{
	error->priv = g_new0 (GdaErrorPrivate, 1);
}

/*
 * gda_error_new:
 *
 * Creates a new unitialized error object. This class is used for communicating
 * errors from the different providers to the clients.
 *
 * Returns: the error object.
 */
GdaError *
gda_error_new (void)
{
	GdaError *error;

	error = GDA_ERROR (g_object_new (GDA_TYPE_ERROR, NULL));
	return error;
}

/**
 * gda_error_list_from_exception
 * @ev: a CORBA_Environment structure
 *
 * Creates a list of #GdaError's from a CORBA_Environment structure. This
 * is the standard way of informing of errors. 
 *
 * Returns: a list of #GdaError structures.
 */
GList *
gda_error_list_from_exception (CORBA_Environment * ev)
{
	GList *all_errors = 0;
	GdaError *error;

	g_return_val_if_fail (ev != 0, 0);

	switch (ev->_major) {
	case CORBA_NO_EXCEPTION:
		return NULL;
	case CORBA_SYSTEM_EXCEPTION:{
		CORBA_SystemException *sysexc;

		sysexc = CORBA_exception_value (ev);
		error = gda_error_new ();
		gda_error_set_source (error, "[CORBA System Exception]");
		gda_error_set_description (error,
					   _("%s: An Error occured in the CORBA system."));
		all_errors = g_list_append (all_errors, error);
		break;
	}
	case CORBA_USER_EXCEPTION:{
		if (strcmp (CORBA_exception_id (ev), ex_GNOME_Database_NotSupported) == 0) {
			GNOME_Database_NotSupported *not_supported_error;

			not_supported_error = (GNOME_Database_NotSupported *) &ev->_any;
			error = gda_error_new ();
			gda_error_set_source (error,
					      _("[CORBA User Exception]"));
			gda_error_set_description (
				error, not_supported_error->errormsg);
			all_errors = g_list_append (all_errors, error);
		}
		else if (strcmp (CORBA_exception_id (ev), ex_GNOME_Database_DriverError) == 0) {
			GNOME_Database_ErrorSeq error_sequence =
				((GNOME_Database_DriverError *) &ev->_any)->errors;
			gint idx;

			for (idx = 0; idx < error_sequence._length; idx++) {
				GNOME_Database_Error *gda_error =
					&error_sequence._buffer[idx];

				error = gda_error_new ();
				if (gda_error->source)
					gda_error_set_source (error,
							      gda_error->source);
				gda_error_set_number (error,
						      gda_error->number);
				if (gda_error->sqlstate)
					gda_error_set_sqlstate (error,
								gda_error->sqlstate);
				if (gda_error->nativeMsg)
					gda_error_set_native (error,
							      gda_error->nativeMsg);
				if (gda_error->description)
					gda_error_set_description (error,
								   gda_error->description);
				all_errors = g_list_append (all_errors, error);
			}
		}
		break;
	}
	default:
		g_error (_("Unknown CORBA exception for connection"));
	}

	return all_errors;
}

/**
 * gda_error_to_exception
 * @error: a #GdaError object
 * @ev: a CORBA exception
 *
 */
void
gda_error_to_exception (GdaError * error, CORBA_Environment * ev)
{
	GList *error_list = NULL;

	g_return_if_fail (error != NULL);
	g_return_if_fail (ev != NULL);

	error_list = g_list_append (error_list, error);
	gda_error_list_to_exception (error_list, ev);
	g_list_free (error_list);
}

void
gda_error_list_to_exception (GList * error_list, CORBA_Environment * ev)
{
	GNOME_Database_DriverError *exception;
	GNOME_Database_ErrorSeq *corba_errors;
	gint count;

	g_return_if_fail (ev != NULL);

	exception = GNOME_Database_DriverError__alloc ();
	corba_errors = gda_error_list_to_corba_seq (error_list);
	memcpy (&exception->errors, corba_errors, sizeof (GNOME_Database_ErrorSeq));
	exception->realcommand = CORBA_string_dup (g_get_prgname ());
	CORBA_exception_set (ev, CORBA_USER_EXCEPTION,
			     ex_GNOME_Database_DriverError,
			     exception);
}

GNOME_Database_ErrorSeq *
gda_error_list_to_corba_seq (GList * error_list)
{
	GList *l;
	gint count;
	gint i;
	GNOME_Database_ErrorSeq *rc;

	count = error_list != NULL ? g_list_length (error_list) : 0;

	rc = GNOME_Database_ErrorSeq__alloc ();
	CORBA_sequence_set_release (rc, TRUE);
	rc->_length = count;
	rc->_buffer = CORBA_sequence_GNOME_Database_Error_allocbuf (count);

	for (l = g_list_first (error_list), i = 0;
	     l != NULL; l = g_list_next (l), i++) {
		GdaError *error = GDA_ERROR (l->data);

		if (GDA_IS_ERROR (error)) {
			gchar *desc, *source, *state, *native;

			desc = (gchar *) gda_error_get_description (error);
			source = (gchar *) gda_error_get_source (error);
			state = (gchar *) gda_error_get_sqlstate (error);
			native = (gchar *) gda_error_get_native (error);

			rc->_buffer[i].description = CORBA_string_dup (desc ? desc : "<Null>");
			rc->_buffer[i].number = error->priv->number;
			rc->_buffer[i].source = CORBA_string_dup (source ? source : "<Null>");
			rc->_buffer[i].sqlstate = CORBA_string_dup (state ? state : "<Null>");
			rc->_buffer[i].nativeMsg = CORBA_string_dup (native ? native : "<Null>");
		}
	}

	return rc;
}

static void
gda_error_finalize (GObject * object)
{
	GObjectClass *parent_class;
	GdaError *error = (GdaError *) object;

	g_return_if_fail (GDA_IS_ERROR (error));

	if (error->priv->description)
		g_free (error->priv->description);
	if (error->priv->source)
		g_free (error->priv->source);
	if (error->priv->helpurl)
		g_free (error->priv->helpurl);
	if (error->priv->helpctxt)
		g_free (error->priv->helpctxt);
	if (error->priv->sqlstate)
		g_free (error->priv->sqlstate);
	if (error->priv->native)
		g_free (error->priv->native);
	if (error->priv->realcommand)
		g_free (error->priv->realcommand);

	g_free (error->priv);
	error->priv = NULL;

	parent_class = g_type_class_peek (G_TYPE_OBJECT);
	if (parent_class && parent_class->finalize)
		parent_class->finalize (object);
}

/**
 * gda_error_free:
 * @error: the error object.
 *
 * Frees the memory allocated by the error object.
 *
 */
void
gda_error_free (GdaError * error)
{
	g_object_unref (G_OBJECT (error));
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
gda_error_list_free (GList * errors)
{
	GList *ptr = errors;

	g_return_if_fail (errors != 0);

	while (ptr) {
		GdaError *error = ptr->data;
		gda_error_free (error);
		ptr = g_list_next (ptr);
	}
	g_list_free (errors);
}

/**
 * gda_error_get_description
 */
const gchar *
gda_error_get_description (GdaError * error)
{
	g_return_val_if_fail (GDA_IS_ERROR (error), NULL);
	return error->priv->description;
}

void
gda_error_set_description (GdaError * error, const gchar * description)
{
	g_return_if_fail (GDA_IS_ERROR (error));

	if (error->priv->description)
		g_free (error->priv->description);
	error->priv->description = g_strdup (description);
}

const glong
gda_error_get_number (GdaError * error)
{
	g_return_val_if_fail (GDA_IS_ERROR (error), -1);
	return error->priv->number;
}

void
gda_error_set_number (GdaError * error, glong number)
{
	g_return_if_fail (GDA_IS_ERROR (error));
	error->priv->number = number;
}

const gchar *
gda_error_get_source (GdaError * error)
{
	g_return_val_if_fail (GDA_IS_ERROR (error), 0);
	return error->priv->source;
}

void
gda_error_set_source (GdaError * error, const gchar * source)
{
	g_return_if_fail (GDA_IS_ERROR (error));

	if (error->priv->source)
		g_free (error->priv->source);
	error->priv->source = g_strdup (source);
}

const gchar *
gda_error_get_help_url (GdaError * error)
{
	g_return_val_if_fail (GDA_IS_ERROR (error), 0);
	return error->priv->helpurl;
}

void
gda_error_set_help_url (GdaError * error, const gchar * helpurl)
{
	g_return_if_fail (GDA_IS_ERROR (error));

	if (error->priv->helpurl)
		g_free (error->priv->helpurl);
	error->priv->helpurl = g_strdup (helpurl);
}

const gchar *
gda_error_get_help_context (GdaError * error)
{
	g_return_val_if_fail (GDA_IS_ERROR (error), NULL);
	return (const gchar *) error->priv->helpctxt;
}

void
gda_error_set_help_context (GdaError * error, const gchar * helpctxt)
{
	g_return_if_fail (GDA_IS_ERROR (error));

	if (error->priv->helpctxt)
		g_free (error->priv->helpctxt);
	error->priv->helpctxt = g_strdup (helpctxt);
}

const gchar *
gda_error_get_sqlstate (GdaError * error)
{
	g_return_val_if_fail (GDA_IS_ERROR (error), NULL);
	return error->priv->sqlstate;
}

void
gda_error_set_sqlstate (GdaError * error, const gchar * sqlstate)
{
	g_return_if_fail (GDA_IS_ERROR (error));

	if (error->priv->sqlstate)
		g_free (error->priv->sqlstate);
	error->priv->sqlstate = g_strdup (sqlstate);
}

const gchar *
gda_error_get_native (GdaError * error)
{
	g_return_val_if_fail (GDA_IS_ERROR (error), NULL);
	return error->priv->native;
}

void
gda_error_set_native (GdaError * error, const gchar * native)
{
	g_return_if_fail (GDA_IS_ERROR (error));

	if (error->priv->native)
		g_free (error->priv->native);
	error->priv->native = g_strdup (native);
}

const gchar *
gda_error_get_real_command (GdaError * error)
{
	g_return_val_if_fail (GDA_IS_ERROR (error), NULL);
	return error->priv->realcommand;
}

void
gda_error_set_real_command (GdaError * error, const gchar * realcommand)
{
	g_return_if_fail (GDA_IS_ERROR (error));

	if (error->priv->realcommand)
		g_free (error->priv->realcommand);
	error->priv->realcommand = g_strdup (realcommand);
}
