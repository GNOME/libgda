/* GDA client libary
 * Copyright (C) 1998,1999 Michael Lausch
 * Copyright (C) 2000 Rodrigo Moya
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef __gda_connection_h__
#define __gda_connection_h__ 1

#include <glib.h>

#ifdef HAVE_GOBJECT
#  include <glib-object.h>
#else
#  include <gtk/gtk.h>
#endif

#include <orb/orbit.h>
#include <gda.h>
#include <gda-error.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * The connection object. The base of all acceess to data sources.
 */

typedef struct _Gda_Connection       Gda_Connection;
typedef struct _Gda_ConnectionClass  Gda_ConnectionClass;

#include <gda-recordset.h>    /* this one needs the definitions above */

#define GDA_TYPE_CONNECTION            (gda_connection_get_type())

#ifdef HAVE_GOBJECT
#  define GDA_CONNECTION(obj) \
            G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_CONNECTION, \
                                        Gda_Connection)
#  define GDA_CONNECTION_CLASS(klass) \
            G_TYPE_CHECK_CLASS_CAST (obj, GDA_TYPE_CONNECTION, \
                                     Gda_ConnectionClass)
#  define IS_GDA_CONNECTION(obj) \
            G_TYPE_CHECK_INSTANCE_TYPE (obj, GDA_TYPE_CONNECTION)
#  define IS_GDA_CONNECTION_CLASS(klass) \
            G_TYPE_CHECK_CLASS_TYPE (klass, GDA_TYPE_CONNECTION)
#else
#  define GDA_CONNECTION(obj)            GTK_CHECK_CAST(obj, GDA_TYPE_CONNECTION, Gda_Connection)
#  define GDA_CONNECTION_CLASS(klass)    GTK_CHECK_CLASS_CAST(klass, GDA_TYPE_CONNECTION, GdaConnectionClass)
#  define IS_GDA_CONNECTION(obj)         GTK_CHECK_TYPE(obj, GDA_TYPE_CONNECTION)
#  define IS_GDA_CONNECTION_CLASS(klass) (GTK_CHECK_CLASS_TYPE((klass), GDA_TYPE_CONNECTION))
#endif

struct _Gda_Connection
{
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

struct _Gda_ConnectionClass
{
#ifdef HAVE_GOBJECT
  GObjectClass   parent_class;
#else
  GtkObjectClass parent_class;
#endif
  void           (*error)      (Gda_Connection*, GList*);
  void           (*warning)    (Gda_Connection*, GList*);
  void           (*open)       (Gda_Connection*);
  void           (*close)      (Gda_Connection*);
};

  

typedef struct _Gda_Constraint_Element
{
  GDA_Connection_QType  type;
  gchar*                value;
} Gda_Constraint_Element;

#ifdef HAVE_GOBJECT
GType              gda_connection_get_type            (void);
#else
guint              gda_connection_get_type            (void);
#endif

Gda_Connection*    gda_connection_new                 (CORBA_ORB orb);
void               gda_connection_free                (Gda_Connection* cnc);
GList*             gda_connection_list_providers      (void);
void               gda_connection_set_provider        (Gda_Connection* cnc, gchar* name);
const gchar*       gda_connection_get_provider        (Gda_Connection* cnc);
gboolean           gda_connection_supports            (Gda_Connection* cnc, GDA_Connection_Feature feature);
void               gda_connection_set_default_db      (Gda_Connection* cnc, gchar* dsn);
gint               gda_connection_open                (Gda_Connection* cnc, gchar* dsn, gchar* user,gchar* pwd );
void               gda_connection_close               (Gda_Connection* cnc);
Gda_Recordset*     gda_connection_open_schema         (Gda_Connection* cnc,
                                                       GDA_Connection_QType t, ...);
Gda_Recordset*     gda_connection_open_schema_array   (Gda_Connection* cnc, GDA_Connection_QType t, Gda_Constraint_Element*);
GList*             gda_connection_get_errors          (Gda_Connection* cnc);
GList*             gda_connection_list_datasources    (Gda_Connection* cnc);
gint               gda_connection_begin_transaction   (Gda_Connection* cnc);
gint               gda_connection_commit_transaction  (Gda_Connection* cnc);
gint               gda_connection_rollback_transaction (Gda_Connection* cnc);
Gda_Recordset*     gda_connection_execute             (Gda_Connection* cnc, gchar* txt, gulong* reccount, gulong flags);
gint               gda_connection_start_logging       (Gda_Connection* cnc, gchar* filename);
gint               gda_connection_stop_logging        (Gda_Connection* cnc);
gchar*             gda_connection_create_recordset    (Gda_Connection* cnc, Gda_Recordset* rs);

gint               gda_connection_corba_exception     (Gda_Connection* cnc, CORBA_Environment* ev);
void               gda_connection_add_single_error    (Gda_Connection* cnc, Gda_Error* error);
void               gda_connection_add_errorlist       (Gda_Connection* cnc, GList* list);

#define            gda_connection_is_open(cnc)        ((cnc) ? GDA_CONNECTION(cnc)->is_open : FALSE)
#define            gda_connection_get_dsn(cnc)        ((cnc) ? GDA_CONNECTION(cnc)->database : 0)
#define            gda_connection_get_user(cnc)       ((cnc) ? GDA_CONNECTION(cnc)->user : 0)
#define            gda_connection_get_password(cnc)   ((cnc) ? GDA_CONNECTION(cnc)->passwd : 0)

glong              gda_connection_get_flags           (Gda_Connection* cnc);
void               gda_connection_set_flags           (Gda_Connection* cnc, glong flags);
glong              gda_connection_get_cmd_timeout     (Gda_Connection* cnc);
void               gda_connection_set_cmd_timeout     (Gda_Connection* cnc, glong cmd_timeout);
glong              gda_connection_get_connect_timeout (Gda_Connection* cnc);
void               gda_connection_set_connect_timeout (Gda_Connection* cnc, glong timeout);

GDA_CursorLocation gda_connection_get_cursor_location (Gda_Connection* cnc);
void               gda_connection_set_cursor_location (Gda_Connection* cnc, GDA_CursorLocation cursor);

gchar*             gda_connection_get_version         (Gda_Connection *cnc);

#ifdef __cplusplus
}
#endif

#endif
