/* GDA server library
 * Copyright (C) 1998-2002 The GNOME Foundation.
 *
 * AUTHORS:
 *      Michael Lausch <michael@lausch.at>
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

#include <config.h>
#include <libgda/gda-error.h>
#include <libgda/GNOME_Database.h>
#include <bonobo/bonobo-exception.h>
#include <bonobo/bonobo-i18n.h>

struct _GdaErrorPrivate {
	gchar *description;
	glong number;
	gchar *source;
	gchar *sqlstate;
};

static void gda_error_class_init (GdaErrorClass *klass);
static void gda_error_init       (GdaError *error, GdaErrorClass *klass);
static void gda_error_finalize   (GObject *object);

static GObjectClass *parent_class = NULL;

/*
 * GdaError class implementation
 */

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
gda_error_class_init (GdaErrorClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = gda_error_finalize;
}

static void
gda_error_init (GdaError *error, GdaErrorClass *klass)
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
gda_error_list_from_exception (CORBA_Environment *ev)
{
	GList *all_errors = 0;
	GdaError *error;

	g_return_val_if_fail (ev != 0, 0);

	switch (ev->_major) {
	case CORBA_NO_EXCEPTION:
		return NULL;
	case CORBA_SYSTEM_EXCEPTION: {
		gchar *corba_message;
		gchar *bonobo_message;

		bonobo_message = bonobo_exception_get_text (ev);
		corba_message = g_strdup_printf (
				_("%s: An Error occured in the CORBA system."),
				bonobo_message);
		error = gda_error_new ();
		gda_error_set_source (error, "[CORBA System Exception]");
		gda_error_set_description (error, corba_message);
		all_errors = g_list_append (all_errors, error);
		g_free (bonobo_message);
		g_free (corba_message);
		break;
	}
	case CORBA_USER_EXCEPTION: {
		if (strcmp (CORBA_exception_id (ev), ex_GNOME_Database_DriverError) == 0) {
			GNOME_Database_ErrorSeq *error_sequence;
			gint idx;

			error_sequence = CORBA_exception_value (ev);
			for (idx = 0; idx < error_sequence->_length; idx++) {
				GNOME_Database_Error *gda_error;

				gda_error = &error_sequence->_buffer[idx];

				error = gda_error_new ();
				if (gda_error->source)
					gda_error_set_source (error, gda_error->source);
				gda_error_set_number (error, gda_error->number);
				if (gda_error->sqlstate)
					gda_error_set_sqlstate (error, gda_error->sqlstate);
				if (gda_error->description)
					gda_error_set_description (error, gda_error->description);
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
gda_error_to_exception (GdaError *error, CORBA_Environment *ev)
{
	GList *error_list = NULL;

	g_return_if_fail (error != NULL);
	g_return_if_fail (ev != NULL);

	error_list = g_list_append (error_list, error);
	gda_error_list_to_exception (error_list, ev);
	g_list_free (error_list);
}

/**
 * gda_error_list_to_exception
 */
void
gda_error_list_to_exception (GList *error_list, CORBA_Environment *ev)
{
	GNOME_Database_DriverError *exception;
	GNOME_Database_ErrorSeq *corba_errors;
	gint i;

	g_return_if_fail (ev != NULL);

	corba_errors = gda_error_list_to_corba_seq (error_list);

	exception = GNOME_Database_DriverError__alloc ();
	exception->realcommand = CORBA_string_dup (g_get_prgname ());

	exception->errors._buffer = CORBA_sequence_GNOME_Database_Error_allocbuf (corba_errors->_length);
	exception->errors._length = corba_errors->_length;
	CORBA_sequence_set_release (&exception->errors, TRUE);
	for (i = 0; i < corba_errors->_length; i++) {
		exception->errors._buffer[i].description = CORBA_string_dup (corba_errors->_buffer[i].description);
		exception->errors._buffer[i].number = corba_errors->_buffer[i].number;
		exception->errors._buffer[i].source = CORBA_string_dup (corba_errors->_buffer[i].source);
		exception->errors._buffer[i].sqlstate = CORBA_string_dup (corba_errors->_buffer[i].sqlstate);
	}

	CORBA_exception_set (ev, CORBA_USER_EXCEPTION,
			     ex_GNOME_Database_DriverError,
			     exception);

	CORBA_free (corba_errors);
}

/**
 * gda_error_list_to_corba_seq
 */
GNOME_Database_ErrorSeq *
gda_error_list_to_corba_seq (GList *error_list)
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
			gchar *desc, *source, *state;

			desc = (gchar *) gda_error_get_description (error);
			source = (gchar *) gda_error_get_source (error);
			state = (gchar *) gda_error_get_sqlstate (error);

			rc->_buffer[i].description = CORBA_string_dup (desc ? desc : "<Null>");
			rc->_buffer[i].number = error->priv->number;
			rc->_buffer[i].source = CORBA_string_dup (source ? source : "<Null>");
			rc->_buffer[i].sqlstate = CORBA_string_dup (state ? state : "<Null>");
		}
	}

	return rc;
}

static void
gda_error_finalize (GObject *object)
{
	GdaError *error = (GdaError *) object;

	g_return_if_fail (GDA_IS_ERROR (error));

	/* free memory */
	if (error->priv->description)
		g_free (error->priv->description);
	if (error->priv->source)
		g_free (error->priv->source);
	if (error->priv->sqlstate)
		g_free (error->priv->sqlstate);

	g_free (error->priv);
	error->priv = NULL;

	/* chain to parent class */
	parent_class->finalize (object);
}

/**
 * gda_error_free:
 * @error: the error object.
 *
 * Frees the memory allocated by the error object.
 */
void
gda_error_free (GdaError *error)
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
gda_error_get_description (GdaError *error)
{
	g_return_val_if_fail (GDA_IS_ERROR (error), NULL);
	return error->priv->description;
}

/**
 * gda_error_get_description
 */
void
gda_error_set_description (GdaError *error, const gchar *description)
{
	g_return_if_fail (GDA_IS_ERROR (error));

	if (error->priv->description)
		g_free (error->priv->description);
	error->priv->description = g_strdup (description);
}

/**
 * gda_error_get_number
 */
const glong
gda_error_get_number (GdaError * error)
{
	g_return_val_if_fail (GDA_IS_ERROR (error), -1);
	return error->priv->number;
}

/**
 * gda_error_set_number
 */
void
gda_error_set_number (GdaError *error, glong number)
{
	g_return_if_fail (GDA_IS_ERROR (error));
	error->priv->number = number;
}

/**
 * gda_error_get_source
 */
const gchar *
gda_error_get_source (GdaError *error)
{
	g_return_val_if_fail (GDA_IS_ERROR (error), 0);
	return error->priv->source;
}

/**
 * gda_error_set_source
 */
void
gda_error_set_source (GdaError *error, const gchar *source)
{
	g_return_if_fail (GDA_IS_ERROR (error));

	if (error->priv->source)
		g_free (error->priv->source);
	error->priv->source = g_strdup (source);
}

/**
 * gda_error_get_sqlstate
 */
const gchar *
gda_error_get_sqlstate (GdaError *error)
{
	g_return_val_if_fail (GDA_IS_ERROR (error), NULL);
	return error->priv->sqlstate;
}

/**
 * gda_error_set_sqlstate
 */
void
gda_error_set_sqlstate (GdaError *error, const gchar *sqlstate)
{
	g_return_if_fail (GDA_IS_ERROR (error));

	if (error->priv->sqlstate)
		g_free (error->priv->sqlstate);
	error->priv->sqlstate = g_strdup (sqlstate);
}

