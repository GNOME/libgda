/* GDA Default provider
 * Copyright (C) 2001 The Free Software Foundation, Inc.
 *
 * AUTHORS:
 *	Rodrigo Moya <rodrigo@gnome-db.org>
 *      Carlos Perelló Marín <carlos@gnome-db.org>
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

#include "gda-default.h"

#define PARENT_TYPE GDA_TYPE_SERVER_PROVIDER

#define OBJECT_DATA_DEFAULT_HANDLE "GDA_Default_DefaultHandle"

static void gda_default_provider_class_init (GdaDefaultProviderClass *klass);
static void gda_default_provider_init       (GdaDefaultProvider *provider,
					   GdaDefaultProviderClass *klass);
static void gda_default_provider_finalize   (GObject *object);

static gboolean gda_default_provider_open_connection (GdaServerProvider *provider,
						    GdaServerConnection *cnc,
						    GdaQuarkList *params,
						    const gchar *username,
						    const gchar *password);
static gboolean gda_default_provider_close_connection (GdaServerProvider *provider,
						     GdaServerConnection *cnc);

static GObjectClass *parent_class = NULL;

/*
 * GdaDefaultProvider class implementation
 */

static void
gda_default_provider_class_init (GdaDefaultProviderClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GdaServerProviderClass *provider_class = GDA_SERVER_PROVIDER_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = gda_default_provider_finalize;
	provider_class->open_connection = gda_default_provider_open_connection;
	provider_class->close_connection = gda_default_provider_close_connection;
	provider_class->begin_transaction = NULL;
	provider_class->commit_transaction = NULL;
	provider_class->rollback_transaction = NULL;
}

static void
gda_default_provider_init (GdaDefaultProvider *myprv, GdaDefaultProviderClass *klass)
{
}

static void
gda_default_provider_finalize (GObject *object)
{
	GdaDefaultProvider *dfprv = (GdaDefaultProvider *) object;

	g_return_if_fail (GDA_IS_DEFAULT_PROVIDER (dfprv));

	/* chain to parent class */
	parent_class->finalize (object);
}

GType
gda_default_provider_get_type (void)
{
	static GType type = 0;

	if (!type) {
		if (type == 0) {
			static GTypeInfo info = {
				sizeof (GdaDefaultProviderClass),
				(GBaseInitFunc) NULL,
				(GBaseFinalizeFunc) NULL,
				(GClassInitFunc) gda_default_provider_class_init,
				NULL, NULL,
				sizeof (GdaDefaultProvider),
				0,
				(GInstanceInitFunc) gda_default_provider_init
			};
			type = g_type_register_static (PARENT_TYPE,
						       "GdaDefaultProvider",
						       &info, 0);
		}
	}

	return type;
}

/* open_connection handler for the GdaDefaultProvider class */
static gboolean
gda_default_provider_open_connection (GdaServerProvider *provider,
				    GdaServerConnection *cnc,
				    GdaQuarkList *params,
				    const gchar *username,
				    const gchar *password)
{
	const gchar *t_filename = NULL;
	gchar *errmsg = NULL;

	sqlite *sqlite;
	
	GdaError *error;
	GdaDefaultProvider *dfprv = (GdaDefaultProvider *) provider;

	g_return_val_if_fail (GDA_IS_DEFAULT_PROVIDER (dfprv), FALSE);
	g_return_val_if_fail (GDA_IS_SERVER_CONNECTION (cnc), FALSE);

	/* get all parameters received */
	t_filename = gda_quark_list_find (params, "FILENAME");

	if (!t_filename || *t_filename != '/') {
		gda_server_connection_add_error_string ( cnc,
				_("A full path must be specified on the "
				"connection string to open a database."));
		return FALSE;
	} else {

		sqlite = sqlite_open (t_filename, 0666, &errmsg);

		/* free memory */
		g_free ((gpointer) t_filename);
		
		if (!sqlite) {
			gda_server_connection_add_error_string (cnc, errmsg);
			
			free (errmsg); /* must use free () for this pointer */
			
			return FALSE;
		} else {

			g_object_set_data (G_OBJECT (cnc),
					   OBJECT_DATA_DEFAULT_HANDLE, sqlite);

			return TRUE;
		}
	}
}

/* close_connection handler for the GdaDefaultProvider class */
static gboolean
gda_default_provider_close_connection (GdaServerProvider *provider, GdaServerConnection *cnc)
{
	sqlite *sqlite;
	
	GdaDefaultProvider *dfprv = (GdaDefaultProvider *) provider;

	g_return_val_if_fail (GDA_IS_DEFAULT_PROVIDER (dfprv), FALSE);
	g_return_val_if_fail (GDA_IS_SERVER_CONNECTION (cnc), FALSE);

	sqlite = g_object_get_data (G_OBJECT (cnc), OBJECT_DATA_DEFAULT_HANDLE);
	if (!sqlite)
		return FALSE;

	sqlite_close (sqlite);
	g_object_set_data (G_OBJECT (cnc), OBJECT_DATA_DEFAULT_HANDLE, NULL);

	return TRUE;
}
