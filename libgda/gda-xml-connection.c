/* GDA library
 * Copyright (C) 1998-2002 The GNOME Foundation.
 *
 * AUTHORS:
 * 	Rodrigo Moya <rodrigo@gnome-db.org>
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

#include <libxml/parser.h>
#include <libgda/gda-util.h>
#include <libgda/gda-xml-connection.h>
#include <string.h>

struct _GdaXmlConnectionPrivate {
	gchar *dsn;
	gchar *username;
	gchar *password;
};

#define PARENT_TYPE G_TYPE_OBJECT

static void gda_xml_connection_class_init (GdaXmlConnectionClass *klass);
static void gda_xml_connection_init       (GdaXmlConnection *xmlcnc,
					   GdaXmlConnectionClass *klass);
static void gda_xml_connection_finalize   (GObject *object);

static GObjectClass *parent_class = NULL;

/*
 * GdaXmlConnection class implementation
 */

static void
gda_xml_connection_class_init (GdaXmlConnectionClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);
	object_class->finalize = gda_xml_connection_finalize;
}

static void
gda_xml_connection_init (GdaXmlConnection *xmlcnc, GdaXmlConnectionClass *klass)
{
	g_return_if_fail (GDA_IS_XML_CONNECTION (xmlcnc));

	/* allocate private structure */
	xmlcnc->priv = g_new0 (GdaXmlConnectionPrivate, 1);
	xmlcnc->priv->dsn = NULL;
	xmlcnc->priv->username = NULL;
	xmlcnc->priv->password = NULL;
}

static void
gda_xml_connection_finalize (GObject *object)
{
	GdaXmlConnection *xmlcnc = (GdaXmlConnection *) object;

	g_return_if_fail (GDA_IS_XML_CONNECTION (xmlcnc));

	/* free memory */
	if (xmlcnc->priv->dsn) {
		g_free (xmlcnc->priv->dsn);
		xmlcnc->priv->dsn = NULL;
	}
	if (xmlcnc->priv->username) {
		g_free (xmlcnc->priv->username);
		xmlcnc->priv->username = NULL;
	}
	if (xmlcnc->priv->password) {
		g_free (xmlcnc->priv->password);
		xmlcnc->priv->password = NULL;
	}

	g_free (xmlcnc->priv);
	xmlcnc->priv = NULL;

	/* chain to parent class */
	parent_class->finalize (object);
}

GType
gda_xml_connection_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (GdaXmlConnectionClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_xml_connection_class_init,
			NULL,
			NULL,
			sizeof (GdaXmlConnection),
			0,
			(GInstanceInitFunc) gda_xml_connection_init
		};
		type = g_type_register_static (PARENT_TYPE,
					       "GdaXmlConnection",
					       &info, 0);
	}
	return type;
}

/**
 * gda_xml_connection_new
 *
 * Creates a new #GdaXmlConnection object, which lets you parse and/or
 * create .connection files, which are XML files used to specify all
 * parameters needed to open a database connection, and which can
 * be used to store user's connection preferences, or for automatic
 * connection to databases from unattended scripts and such.
 *
 * Returns: the newly created object.
 */
GdaXmlConnection *
gda_xml_connection_new (void)
{
	GdaXmlConnection *xmlcnc;

	xmlcnc = g_object_new (GDA_TYPE_XML_CONNECTION, NULL);
	return xmlcnc;
}

/**
 * gda_xml_connection_new_from_file
 * @filename: name of file to create the #GdaXmlConnection object from.
 *
 * Creates a #GdaXmlConnection object from the contents of @filename,
 * which must be a correct .connection file.
 *
 * Returns: the newly created object.
 */
GdaXmlConnection *
gda_xml_connection_new_from_file (const gchar *filename)
{
	GdaXmlConnection *xmlcnc;

	g_return_val_if_fail (filename != NULL, NULL);

	xmlcnc = gda_xml_connection_new ();
	if (!gda_xml_connection_set_from_file (xmlcnc, filename)) {
		g_object_unref (G_OBJECT (xmlcnc));
		return NULL;
	}

	return xmlcnc;
}

/**
 * gda_xml_connection_new_from_string
 * @string: XML string to create the #GdaXmlConnection object from.
 *
 * Creates a #GdaXmlConnection object from the given XML string.
 *
 * Returns: the newly created object.
 */
GdaXmlConnection *
gda_xml_connection_new_from_string (const gchar *string)
{
	GdaXmlConnection *xmlcnc;

	g_return_val_if_fail (string != NULL, NULL);

	xmlcnc = gda_xml_connection_new ();
	if (!gda_xml_connection_set_from_string (xmlcnc, string)) {
		g_object_unref (G_OBJECT (xmlcnc));
		return NULL;
	}

	return xmlcnc;
}

/**
 * gda_xml_connection_set_from_file
 * @xmlcnc: a #GdaXmlConnection object.
 * @filename: name of a XML file.
 *
 * Loads a XML file into the given #GdaXmlConnection object.
 *
 * Returns: %TRUE if successful, %FALSE otherwise.
 */
gboolean
gda_xml_connection_set_from_file (GdaXmlConnection *xmlcnc, const gchar *filename)
{
	gchar *string;
	gboolean result;

	g_return_val_if_fail (GDA_IS_XML_CONNECTION (xmlcnc), FALSE);
	g_return_val_if_fail (filename != NULL, FALSE);

	string = gda_file_load (filename);
	result = gda_xml_connection_set_from_string (xmlcnc, string);
	g_free (string);

	return result;
}

/**
 * gda_xml_connection_set_from_string
 * @xmlcnc: a #GdaXmlConnection object.
 * @string: XML connection file contents.
 *
 * Loads a XML string into the given #GdaXmlConnection object.
 *
 * Returns: %TRUE if successful, %FALSE otherwise.
 */
gboolean
gda_xml_connection_set_from_string (GdaXmlConnection *xmlcnc, const gchar *string)
{
	xmlDocPtr doc;
	xmlNodePtr node;

	g_return_val_if_fail (GDA_IS_XML_CONNECTION (xmlcnc), FALSE);
	g_return_val_if_fail (string != NULL, FALSE);

	/* load the file */
	doc = xmlParseMemory (string, strlen (string));
	if (!doc)
		return FALSE;

	node = xmlDocGetRootElement (doc);
	if (!node || strcmp (node->name, "database-connection")) {
		xmlFreeDoc (doc);
		return FALSE;
	}

	/* parse the file */
	for (node = node->xmlChildrenNode; node != NULL; node = node->next) {
		gchar *content;

		content = xmlNodeGetContent (node);
		if (!strcmp (node->name, "dsn"))
			gda_xml_connection_set_dsn (xmlcnc, (const gchar *) content);
		else if (!strcmp (node->name, "username"))
			gda_xml_connection_set_username (xmlcnc, (const gchar *) content);
		else if (!strcmp (node->name, "password"))
			gda_xml_connection_set_password (xmlcnc, (const gchar *) content);

		if (content)
			free (content);
	}

	xmlFreeDoc (doc);

	return TRUE;
}

/**
 * gda_xml_connection_get_dsn
 * @xmlcnc: a #GdaXmlConnection object.
 *
 * Returns: the data source name for the given #GdaXmlConnection object.
 */
const gchar *
gda_xml_connection_get_dsn (GdaXmlConnection *xmlcnc)
{
	g_return_val_if_fail (GDA_IS_XML_CONNECTION (xmlcnc), NULL);
	return (const gchar *) xmlcnc->priv->dsn;
}

/**
 * gda_xml_connection_set_dsn
 * @xmlcnc: a #GdaXmlConnection object.
 * @dsn: data source name.
 *
 * Sets the data source name for the given #GdaXmlConnection object.
 */
void
gda_xml_connection_set_dsn (GdaXmlConnection *xmlcnc, const gchar *dsn)
{
	g_return_if_fail (GDA_IS_XML_CONNECTION (xmlcnc));

	if (xmlcnc->priv->dsn)
		g_free (xmlcnc->priv->dsn);
	xmlcnc->priv->dsn = g_strdup (dsn);
}

/**
 * gda_xml_connection_get_username
 * @xmlcnc: a #GdaXmlConnection object.
 *
 * Returns: the user name defined in the #GdaXmlConnection object.
 */
const gchar *
gda_xml_connection_get_username (GdaXmlConnection *xmlcnc)
{
	g_return_val_if_fail (GDA_IS_XML_CONNECTION (xmlcnc), NULL);
	return (const gchar *) xmlcnc->priv->username;
}

/**
 * gda_xml_connection_set_username
 * @xmlcnc: a #GdaXmlConnection object.
 * @username: new user name.
 *
 * Sets the user name for the given #GdaXmlConnection object.
 */
void
gda_xml_connection_set_username (GdaXmlConnection *xmlcnc, const gchar *username)
{
	g_return_if_fail (GDA_IS_XML_CONNECTION (xmlcnc));

	if (xmlcnc->priv->username)
		g_free (xmlcnc->priv->username);
	xmlcnc->priv->username = g_strdup (username);
}

/**
 * gda_xml_connection_get_password
 * @xmlcnc: a #GdaXmlConnection object.
 *
 * Returns: the password defined in the #GdaXmlConnection object.
 */
const gchar *
gda_xml_connection_get_password (GdaXmlConnection *xmlcnc)
{
	g_return_val_if_fail (GDA_IS_XML_CONNECTION (xmlcnc), NULL);
	return (const gchar *) xmlcnc->priv->password;
}

/**
 * gda_xml_connection_set_password
 * @xmlcnc: a #GdaXmlConnection object.
 * @password: new password.
 *
 * Sets the password for the given #GdaXmlConnection object.
 */
void
gda_xml_connection_set_password (GdaXmlConnection *xmlcnc, const gchar *password)
{
	g_return_if_fail (GDA_IS_XML_CONNECTION (xmlcnc));

	if (xmlcnc->priv->password)
		g_free (xmlcnc->priv->password);
	xmlcnc->priv->password = g_strdup (password);
}
