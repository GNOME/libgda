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
#include <libgda/gda-intl.h>

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

