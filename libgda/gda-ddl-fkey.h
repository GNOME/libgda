/* gda-ddl-fkey.h
 *
 * Copyright © 2018 Pavlo Solntsev <pavlo.solntsev@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef GDA_DDL_FKEY_H
#define GDA_DDL_FKEY_H

#include <glib-object.h>
#include <gmodule.h>
#include <glib.h>
#include <libxml/parser.h>
#include <libxml/xmlwriter.h>

G_BEGIN_DECLS

#define GDA_TYPE_DDL_FKEY (gda_ddl_fkey_get_type())

G_DECLARE_DERIVABLE_TYPE (GdaDdlFkey, gda_ddl_fkey, GDA, DDL_FKEY, GObject)

struct _GdaDdlFkeyClass {
	GObjectClass parent_class;
};

typedef enum {
	GDA_DDL_FKEY_NO_ACTION,
	GDA_DDL_FKEY_SET_NULL,
	GDA_DDL_FKEY_RESTRICT,
	GDA_DDL_FKEY_SET_DEFAULT,
	GDA_DDL_FKEY_CASCADE
} GdaDdlFkeyReferenceAction;

typedef enum {
	GDA_DDL_FKEY_ERROR_START_ELEMENT,
	GDA_DDL_FKEY_ERROR_ATTRIBUTE,
	GDA_DDL_FKEY_ERROR_END_ELEMENT
}GdaDdlFkeyError;

#define GDA_DDL_FKEY_ERROR gda_ddl_fkey_error_quark()
GQuark gda_ddl_fkey_error_quark (void);

/**
 * SECTION:gda-ddl-fkey
 * @short_description: Object to hold information for foregn key.
 * @stability: Stable
 * @include: libgda.h
 *
 * For generating database from xml file or for mapping
 * database to an xml file #GdaDdlFkey holds information about
 * foregn keys with a convenient set of methods to manipulate them.
 */

GdaDdlFkey*		gda_ddl_fkey_new 				(void);

const GList*	gda_ddl_fkey_get_field_name 	(GdaDdlFkey *self);
const GList*	gda_ddl_fkey_get_ref_field  	(GdaDdlFkey *self);

void			gda_ddl_fkey_set_field			(GdaDdlFkey  *self,
												 const gchar *field,
												 const gchar *reffield);

const gchar*	gda_ddl_fkey_get_ref_table  	(GdaDdlFkey *self);
void			gda_ddl_fkey_set_ref_table		(GdaDdlFkey  *self,
												 const gchar *rtable);

const gchar*	gda_ddl_fkey_get_ondelete   	(GdaDdlFkey *self);
GdaDdlFkeyReferenceAction gda_ddl_fkey_get_ondelete_id (GdaDdlFkey *self);
void			gda_ddl_fkey_set_ondelete		(GdaDdlFkey *self,
												 GdaDdlFkeyReferenceAction id);

const gchar*	gda_ddl_fkey_get_onupdate   	(GdaDdlFkey *self);
GdaDdlFkeyReferenceAction gda_ddl_fkey_get_onupdate_id (GdaDdlFkey *self);
void			gda_ddl_fkey_set_onupdate		(GdaDdlFkey *self,
												 GdaDdlFkeyReferenceAction id);
gboolean		gda_ddl_fkey_parse_node     	(GdaDdlFkey  *self,
												 xmlNodePtr node,
												 GError **error);

gboolean		gda_ddl_fkey_write_xml			(GdaDdlFkey  *self,
												 xmlTextWriterPtr writer,
												 GError     **error);

void			gda_ddl_fkey_free         		(GdaDdlFkey *self);

G_END_DECLS

#endif /* GDA_DDL_FKEY_H */
