/* GDA client libary
 * Copyright (C) 1998,1999 Michael Lausch
 * Copyright (C) 2000,2001 Rodrigo Moya
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

#if !defined(__gda_connection_h__)
#  define __gda_connection_h__

#include <glib.h>

#ifdef HAVE_GOBJECT
#  include <glib-object.h>
#else
#  include <gtk/gtkobject.h>
#endif

#include <orb/orbit.h>
#include <GDA.h>
#include <gda-error.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * The connection object. The base of all acceess to data sources.
 */

typedef struct _GdaConnection       GdaConnection;
typedef struct _GdaConnectionClass  GdaConnectionClass;

#include <gda-recordset.h>    /* this one needs the definitions above */

#define GDA_TYPE_CONNECTION            (gda_connection_get_type())

#ifdef HAVE_GOBJECT
#define GDA_CONNECTION(obj)            G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_CONNECTION, GdaConnection)
#define GDA_CONNECTION_CLASS(klass)    G_TYPE_CHECK_CLASS_CAST (obj, GDA_TYPE_CONNECTION,  GdaConnectionClass)
#define GDA_IS_CONNECTION(obj)         G_TYPE_CHECK_INSTANCE_TYPE (obj, GDA_TYPE_CONNECTION)
#define GDA_IS_CONNECTION_CLASS(klass) G_TYPE_CHECK_CLASS_TYPE (klass, GDA_TYPE_CONNECTION)
#else
#define GDA_CONNECTION(obj)            GTK_CHECK_CAST(obj, GDA_TYPE_CONNECTION, GdaConnection)
#define GDA_CONNECTION_CLASS(klass)    GTK_CHECK_CLASS_CAST(klass, GDA_TYPE_CONNECTION, GdaConnectionClass)
#define GDA_IS_CONNECTION(obj)         GTK_CHECK_TYPE(obj, GDA_TYPE_CONNECTION)
#define GDA_IS_CONNECTION_CLASS(klass) (GTK_CHECK_CLASS_TYPE((klass), GDA_TYPE_CONNECTION))
#endif

struct _GdaConnection {
#ifdef HAVE_GOBJECT
	GObject       object;
#else
	GtkObject     object;
#endif
	CORBA_Object  connection;
	CORBA_ORB     orb;
	GList*        commands;
	GList*        recordsets;
	gchar*        provider;
	gchar*        default_db;
	gchar*        database;
	gchar*        user;
	gchar*        passwd;
	GList*        errors_head;
	gint          is_open;
};

struct _GdaConnectionClass {
#ifdef HAVE_GOBJECT
	GObjectClass   parent_class;
#else
	GtkObjectClass parent_class;
#endif
	void           (*error)      (GdaConnection*, GList*);
	void           (*warning)    (GdaConnection*, GList*);
	void           (*open)       (GdaConnection*);
	void           (*close)      (GdaConnection*);
};

typedef struct _GdaConstraint_Element {
	GDA_Connection_QType  type;
	gchar*                value;
} GdaConstraint_Element;

#ifdef HAVE_GOBJECT
GType              gda_connection_get_type            (void);
#else
guint              gda_connection_get_type            (void);
#endif

GdaConnection*     gda_connection_new                 (CORBA_ORB orb);
GdaConnection*     gda_connection_new_from_dsn        (const gchar *dsn_name,
                                                       const gchar *username,
                                                       const gchar *password);
void               gda_connection_free                (GdaConnection* cnc);
void               gda_connection_set_provider        (GdaConnection* cnc, gchar* name);
const gchar*       gda_connection_get_provider        (GdaConnection* cnc);
gboolean           gda_connection_supports            (GdaConnection* cnc, GDA_Connection_Feature feature);
void               gda_connection_set_default_db      (GdaConnection* cnc, gchar* dsn);
gint               gda_connection_open                (GdaConnection* cnc,
                                                       const gchar* dsn,
                                                       const gchar* user,
                                                       const gchar* pwd);
void               gda_connection_close               (GdaConnection* cnc);
GdaRecordset*      gda_connection_open_schema         (GdaConnection* cnc,
                                                       GDA_Connection_QType t, ...);
GdaRecordset*      gda_connection_open_schema_array   (GdaConnection* cnc,
                                                       GDA_Connection_QType t,
                                                       GdaConstraint_Element*);
glong              gda_connection_modify_schema       (GdaConnection *cnc,
                                                       GDA_Connection_QType t, ...);
GList*             gda_connection_get_errors          (GdaConnection* cnc);
gint               gda_connection_begin_transaction   (GdaConnection* cnc);
gint               gda_connection_commit_transaction  (GdaConnection* cnc);
gint               gda_connection_rollback_transaction (GdaConnection* cnc);
GdaRecordset*      gda_connection_execute             (GdaConnection* cnc, gchar* txt, gulong* reccount, gulong flags);
gint               gda_connection_start_logging       (GdaConnection* cnc, gchar* filename);
gint               gda_connection_stop_logging        (GdaConnection* cnc);
gchar*             gda_connection_create_recordset    (GdaConnection* cnc, GdaRecordset* rs);

gint               gda_connection_corba_exception     (GdaConnection* cnc, CORBA_Environment* ev);
void               gda_connection_add_single_error    (GdaConnection* cnc, GdaError* error);
void               gda_connection_add_error_list      (GdaConnection* cnc, GList* list);

#define            gda_connection_is_open(cnc)        ((cnc) ? GDA_CONNECTION(cnc)->is_open : FALSE)
#define            gda_connection_get_dsn(cnc)        ((cnc) ? GDA_CONNECTION(cnc)->database : 0)
#define            gda_connection_get_user(cnc)       ((cnc) ? GDA_CONNECTION(cnc)->user : 0)
#define            gda_connection_get_password(cnc)   ((cnc) ? GDA_CONNECTION(cnc)->passwd : 0)

gchar*             gda_connection_get_version         (GdaConnection *cnc);

/* conversion routines */
gchar*             gda_connection_sql2xml             (GdaConnection *cnc,
                                                       const gchar *sql);
gchar*             gda_connection_xml2sql             (GdaConnection *cnc,
                                                       const gchar *xml);

#ifdef __cplusplus
}
#endif

#endif
