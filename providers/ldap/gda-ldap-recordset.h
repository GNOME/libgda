/* GDA LDAP provider
 * Copyright (C) 1998-2003 The GNOME Foundation.
 *
 * AUTHORS:
 *      Michael Lausch <michael@lausch.at>
 *	    Rodrigo Moya <rodrigo@gnome-db.org>
 *      Vivien Malerba <malerba@gnome-db.org>
 *      German Poo-Caaman~o <gpoo@ubiobio.cl>
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

#if !defined(__gda_ldap_recordset_h__)
#  define __gda_ldap_recordset_h__

#include <libgda/gda-connection.h>
#include <libgda/gda-data-model.h>
#include <ldap.h>

G_BEGIN_DECLS

#define GDA_TYPE_LDAP_RECORDSET            (gda_ldap_recordset_get_type())
#define GDA_LDAP_RECORDSET(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_LDAP_RECORDSET, GdaLdapRecordset))
#define GDA_LDAP_RECORDSET_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_LDAP_RECORDSET, GdaLdapRecordsetClass))
#define GDA_IS_LDAP_RECORDSET(obj)         (G_TYPE_CHECK_INSTANCE_TYPE (obj, GDA_TYPE_LDAP_RECORDSET))
#define GDA_IS_LDAP_RECORDSET_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GDA_TYPE_LDAP_RECORDSET))

typedef struct _GdaLdapRecordset      GdaLdapRecordset;
typedef struct _GdaLdapRecordsetClass GdaLdapRecordsetClass;

struct _GdaLdapRecordset {
	GdaDataModel model;
	GPtrArray *rows;
	GdaConnection *cnc;
	LDAPMessage *ldap_res;
};

struct _GdaLdapRecordsetClass {
	GdaDataModelClass parent_class;
};

GType              gda_ldap_recordset_get_type (void);
GdaLdapRecordset *gda_ldap_recordset_new (GdaConnection *cnc, LDAPMessage *ldap_res);

G_END_DECLS

#endif
