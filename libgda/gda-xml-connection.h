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

#if !defined(__gda_xml_connection_h__)
#  define __gda_xml_connection_h__

#include <libgda/gda-connection.h>

#define GDA_TYPE_XML_CONNECTION            (gda_xml_connection_get_type())
#define GDA_XML_CONNECTION(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_XML_CONNECTION, GdaXmlConnection))
#define GDA_XML_CONNECTION_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_XML_CONNECTION, GdaXmlConnectionClass))
#define GDA_IS_XML_CONNECTION(obj)         (G_TYPE_CHECK_INSTANCE_TYPE (obj, GDA_TYPE_XML_CONNECTION))
#define GDA_IS_XML_CONNECTION_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GDA_TYPE_XML_CONNECTION))

typedef struct _GdaXmlConnection        GdaXmlConnection;
typedef struct _GdaXmlConnectionClass   GdaXmlConnectionClass;
typedef struct _GdaXmlConnectionPrivate GdaXmlConnectionPrivate;

struct _GdaXmlConnection {
	GObject object;
	GdaXmlConnectionPrivate *priv;
};

struct _GdaXmlConnectionClass {
	GObjectClass parent_class;
};

GType             gda_xml_connection_get_type (void);
GdaXmlConnection *gda_xml_connection_new (void);
GdaXmlConnection *gda_xml_connection_new_from_file (const gchar *filename);
GdaXmlConnection *gda_xml_connection_new_from_string (const gchar *string);
gboolean          gda_xml_connection_set_from_file (GdaXmlConnection *xmlcnc,
						    const gchar *filename);
gboolean          gda_xml_connection_set_from_string (GdaXmlConnection *xmlcnc,
						      const gchar *string);
const gchar      *gda_xml_connection_get_dsn (GdaXmlConnection *xmlcnc);
void              gda_xml_connection_set_dsn (GdaXmlConnection *xmlcnc,
					      const gchar *dsn);
const gchar      *gda_xml_connection_get_username (GdaXmlConnection *xmlcnc);
void              gda_xml_connection_set_username (GdaXmlConnection *xmlcnc,
						   const gchar *username);
const gchar      *gda_xml_connection_get_password (GdaXmlConnection *xmlcnc);
void              gda_xml_connection_set_password (GdaXmlConnection *xmlcnc,
						   const gchar *password);

#endif
