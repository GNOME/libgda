/*
 * Copyright (C) 2011 Vivien Malerba <malerba@gnome-db.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#ifndef __GDA_LDAP_CONNECTION_H__
#define __GDA_LDAP_CONNECTION_H__

#include <virtual/gda-vconnection-data-model.h>
#include <libgda/gda-data-model-ldap.h>

#define GDA_TYPE_LDAP_CONNECTION            (gda_ldap_connection_get_type())
#define GDA_LDAP_CONNECTION(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_LDAP_CONNECTION, GdaLdapConnection))
#define GDA_LDAP_CONNECTION_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_LDAP_CONNECTION, GdaLdapConnectionClass))
#define GDA_IS_LDAP_CONNECTION(obj)         (G_TYPE_CHECK_INSTANCE_TYPE (obj, GDA_TYPE_LDAP_CONNECTION))
#define GDA_IS_LDAP_CONNECTION_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GDA_TYPE_LDAP_CONNECTION))

G_BEGIN_DECLS

typedef struct _GdaLdapConnection      GdaLdapConnection;
typedef struct _GdaLdapConnectionClass GdaLdapConnectionClass;
typedef struct _GdaLdapConnectionPrivate GdaLdapConnectionPrivate;

struct _GdaLdapConnection {
	GdaVconnectionDataModel      parent;
	GdaLdapConnectionPrivate    *priv;
};

struct _GdaLdapConnectionClass {
	GdaVconnectionDataModelClass parent_class;

	/*< private >*/
	/* Padding for future expansion */
	void (*_gda_reserved1) (void);
	void (*_gda_reserved2) (void);
	void (*_gda_reserved3) (void);
	void (*_gda_reserved4) (void);
};

/**
 * SECTION:gda-ldap-connection
 * @short_description: LDAP connection objects
 * @title: GdaLdapConnection
 * @stability: Stable
 * @see_also: 
 *
 * This is a connection, as opened by the LDAP provider. Use
 * gda_connection_open_from_string() or gda_connection_open_from_dsn() to create
 * a #GdaLdapConnection connection.
 *
 * Warning: if you create a #GdaLdapConnection using g_object_new(), then the resulting
 * object won't be functionnal.
 *
 * A #GdaLdapConnection is a virtual connection which accepts any of SQLite's SQL dialect.
 * However, some SQL commands have been added to manipulate virtual tables mapped to
 * LDAP searches. These commands are:
 * <itemizedlist>
 *   <listitem>
 *      <para>The CREATE LDAP TABLE: <synopsis>CREATE LDAP TABLE &lt;table name&gt; [BASE=&lt;base DN&gt;] [FILTER=&lt;LDAP filter&gt;] [ATTRIBUTES=&lt;LDAP attributes&gt;] [SCOPE=&lt;search scope&gt;]</synopsis>
 *      </para>
 *      <para>Each of the BASE, FILTER, ATTRIBUTES and SCOPE specifications is optional. Use this command to declare a table, for example: <programlisting>CREATE LDAP TABLE users FILTER='(cn=*doe*)' SCOPE= 'SUBTREE';</programlisting>.
 *        The allowed SCOPE values are: 'BASE', 'ONELEVEL' and 'SUBTREE'.
 *      </para>
 *      <para>See the <link linkend="gda-ldap-connection-declare-table">gda_ldap_connection_declare_table()</link>
 *        for more information about the ATTRIBUTES syntax.
 *      </para>
 *   </listitem>
 *   <listitem>
 *      <para>The DROP LDAP TABLE: <synopsis>DROP LDAP TABLE &lt;table name&gt;</synopsis>
 *      </para>
 *      <para>Use this command to undeclare a table, for example: <programlisting>DROP LDAP TABLE users;</programlisting> Note that it is also possible to use the normal command to remove a table: <programlisting>DROP TABLE users;</programlisting>
 *      </para>
 *   </listitem>
 *   <listitem>
 *      <para>The ALTER LDAP TABLE: <synopsis>ALTER LDAP TABLE &lt;table name&gt;</synopsis> or
 *            <synopsis>ALTER LDAP TABLE &lt;table name&gt; [BASE=&lt;base DN&gt;] [FILTER=&lt;LDAP filter&gt;] [ATTRIBUTES=&lt;LDAP attributes&gt;] [SCOPE=&lt;search scope&gt;]</synopsis>
 *      </para>
 *      <para>Use this command to modify the definition of a virtual table, for example: <programlisting>ALTER LDAP TABLE users FILTER='(cn=*doe*)' SCOPE='BASE';</programlisting> If no argument is specified after the table name, then
 *        the definition of the virtual table is returned instead. When altering the virtual table, only the
 *        specified parameters are altered (ie. you don't need to repeat the parameters you don't want to
 *        be modified)
 *      </para>
 *   </listitem>
 *   <listitem>
 *      <para>The DESCRIBE LDAP TABLE: <synopsis>DESCRIBE LDAP TABLE &lt;table name&gt;</synopsis> 
 *      </para>
 *      <para>Use this command to get the definition of the virtual table.
 *      </para>
 *   </listitem>
 * </itemizedlist>
 *
 * Each "LDAP" table can then be used like any other table, but you should keep in mind that &LIBGDA;
 * can optimize the LDAP search command executed when the table's data is actually read if you use
 * a "WHERE" clause which involves a search criteria on an LDAP attribute. For example the following
 * SQL: <programlisting>SELECT * FROM users WHERE cn MATCH '%doe%';</programlisting> will actually
 * be optimized by requesting an LDAP search with the filter <programlisting>(cn=*doe*)</programlisting>
 * Optimizations can be done on MATCH, =, &lt;, &lt;=, &gt; and &gt;= operators, the LIKE operator
 * will not be optimized.
 *
 * However a command like <programlisting>SELECT * FROM users WHERE cn MATCH '%doe%' OR uid=123;</programlisting>
 * can't be optimized (because of the "OR") whereas the
 * <programlisting>SELECT * FROM users WHERE cn MATCH '%doe%' AND uid=123;</programlisting>
 * will be optimized because of the "AND".
 */

GType          gda_ldap_connection_get_type        (void) G_GNUC_CONST;

const gchar   *gda_ldap_connection_get_base_dn     (GdaLdapConnection *cnc);
gboolean       gda_ldap_connection_declare_table   (GdaLdapConnection *cnc, const gchar *table_name,
						    const gchar *base_dn, const gchar *filter,
						    const gchar *attributes, GdaLdapSearchScope scope,
						    GError **error);
gboolean       gda_ldap_connection_undeclare_table (GdaLdapConnection *cnc, const gchar *table_name, GError **error);

gboolean       gda_ldap_connection_describe_table  (GdaLdapConnection *cnc, const gchar *table_name,
						    const gchar **out_base_dn, const gchar **out_filter,
						    const gchar **out_attributes,
						    GdaLdapSearchScope *out_scope, GError **error);


/**
 * GdaLdapAttribute:
 * @attr_name: the name of the attribute
 * @nb_values: the number of values in @values, or %0
 * @values: (allow-none) (array length=nb_values) (array zero-terminated=1): the attribute' values as #GValue values, (terminated by a %NULL)
 *
 * This structure holds information about the values of a single attribute (of a single LDAP entry).
 */
typedef struct {
	gchar   *attr_name;
	guint    nb_values;
	GValue **values;
} GdaLdapAttribute;

/**
 * GdaLdapEntry:
 * @dn: the Distinguished Name of the entry
 * @nb_attributes: the number of attributes in @attributes, or %0
 * @attributes: (allow-none) (array length=nb_attributes) (array zero-terminated=1): the entry's attributes, (terminated by a %NULL)
 * @attributes_hash: a hash table where the key is an attribute name, and the value the correcponding #GdaLdapAttribute
 *
 * This structure holds information about the attributes of a single LDAP entry.
 */
typedef struct {
	gchar             *dn;
	guint              nb_attributes;
	GdaLdapAttribute **attributes;
	GHashTable        *attributes_hash;
} GdaLdapEntry;

void           gda_ldap_entry_free                 (GdaLdapEntry *entry);
GdaLdapEntry  *gda_ldap_describe_entry             (GdaLdapConnection *cnc, const gchar *dn, GError **error);
GdaLdapEntry **gda_ldap_get_entry_children         (GdaLdapConnection *cnc, const gchar *dn,
						    gchar **attributes, GError **error);

gchar        **gda_ldap_dn_split                   (const gchar *dn, gboolean all);
gboolean       gda_ldap_is_dn                      (const gchar *dn);


/**
 * GdaLdapClassKind:
 * @GDA_LDAP_CLASS_KIND_ABSTRACT: the LDAP class is an abstract class
 * @GDA_LDAP_CLASS_KIND_STRUTURAL: the LDAP class is a structural class
 * @GDA_LDAP_CLASS_KIND_AUXILIARY: the LDAP class is auxilliary
 * @GDA_LDAP_CLASS_KIND_UNKNOWN: the LDAP class type is not known
 *
 * Defines the LDAP class type
 */
typedef enum {
	GDA_LDAP_CLASS_KIND_ABSTRACT  = 1,
	GDA_LDAP_CLASS_KIND_STRUTURAL = 2,
	GDA_LDAP_CLASS_KIND_AUXILIARY = 3,
	GDA_LDAP_CLASS_KIND_UNKNOWN   = 4
} GdaLdapClassKind;

/**
 * GdaLdapClass:
 * @oid: the OID of the class
 * @nb_names: the number of values in @values (always &gt;= 1)
 * @names: all the class names
 * @description: (allow-none): the class's description, or %NULL
 * @kind: the class kind
 * @obsolete: defines is LDAP class is obsolete
 * @nb_req_attributes: the number of values in @req_attributes
 * @req_attributes: names of required attributes in class
 * @nb_opt_attributes: the number of values in @opt_attributes
 * @opt_attributes: names of optional attributes in class
 * @parents: #GSList of the parent classes (pointers to #GdaLdapClass)
 * @children: #GSList of the children classes (pointers to #GdaLdapClass)
 *
 * Represents an LDAP declared class.
 */
typedef struct {
	gchar            *oid;
	guint             nb_names; /* always >= 1 */
	gchar           **names;
	gchar            *description;
	GdaLdapClassKind  kind;
	gboolean          obsolete;

	guint             nb_req_attributes;
	gchar           **req_attributes;
	guint             nb_opt_attributes;
	gchar           **opt_attributes;

	GSList           *parents; /* list of #LdapClass */
	GSList           *children; /* list of #LdapClass */
} GdaLdapClass;

GdaLdapClass   *gda_ldap_get_class_info (GdaLdapConnection *cnc, const gchar *classname);
const GSList   *gda_ldap_get_top_classes (GdaLdapConnection *cnc);

G_END_DECLS

#endif
