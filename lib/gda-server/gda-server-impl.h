/* GDA Server Library
 * Copyright (C) 2000 Rodrigo Moya
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

#if !defined(__gda_server_impl_h__)
#  define __gda_server_impl_h__

#include <glib.h>

#ifdef HAVE_GOBJECT
#  include <glib-object.h>
#else
#  include <gtk/gtk.h>
#endif

#include <orb/orbit.h>
#include <GDA.h>
#include <gda-common.h>

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct _Gda_ServerImpl      Gda_ServerImpl;
typedef struct _Gda_ServerImplClass Gda_ServerImplClass;

#define GDA_TYPE_SERVER_IMPL            (gda_server_impl_get_type())

#ifdef HAVE_GOBJECT
#  define GDA_SERVER_IMPL(obj) \
         G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_SERVER_IMPL, Gda_ServerImpl)
#  define GDA_SERVER_IMPL_CLASS(klass) \
     G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_SERVER_IMPL, Gda_ServerImplClass)
#  define IS_GDA_SERVER_IMPL(obj) \
          G_TYPE_CHECK_INSTANCE_TYPE (obj, GDA_TYPE_SERVER_IMPL)
#  define IS_GDA_SERVER_IMPL_CLASS(klass) \
          G_TYPE_CHECK_CLASS_TYPE (klass, GDA_TYPE_SERVER_IMPL)
#else
#  define GDA_SERVER_IMPL(obj)            GTK_CHECK_CAST(obj, GDA_TYPE_SERVER_IMPL, Gda_ServerImpl)
#  define GDA_SERVER_IMPL_CLASS(klass)    GTK_CHECK_CLASS_CAST(klass, GDA_TYPE_SERVER_IMPL, Gda_ServerImplClass)
#  define IS_GDA_SERVER_IMPL(obj)         GTK_CHECK_TYPE(obj, GDA_TYPE_SERVER_IMPL)
#  define IS_GDA_SERVER_IMPL_CLASS(klass) (GTK_CHECK_CLASS_TYPE((klass), GDA_TYPE_SERVER_IMPL))
#endif

typedef struct _Gda_ServerImplFunctions Gda_ServerImplFunctions;

typedef struct
{
  Gda_ServerImpl* server_impl;
  gchar*          dsn;
  gchar*          username;
  gchar*          password;
  GList*          commands;
  GList*          errors;
  gint            users;

  gpointer        user_data;
} Gda_ServerConnection;

typedef struct
{
  Gda_ServerConnection* cnc;
  gchar*                text;
  gulong                type;
  gint                  users;

  gpointer              user_data;
} Gda_ServerCommand;

typedef struct
{
  Gda_ServerConnection* cnc;
  GList*                fields;
  gulong                position;
  gboolean              at_begin;
  gboolean              at_end;
  gint                  users;

  gpointer              user_data;
} Gda_ServerRecordset;

typedef struct
{
  gchar*     name;
  gulong     sql_type;
  gshort     c_type;
  gshort     nullable;
  GDA_Value* value;
  guchar     precision;
  gshort     num_scale;
  glong      defined_length;
  glong      actual_length;
  gint       malloced;

  gpointer   user_data;
} Gda_ServerField;

typedef struct
{
  gchar* description;
  glong  number;
  gchar* source;
  gchar* helpfile;
  gchar* helpctxt;
  gchar* sqlstate;
  gchar* native;
} Gda_ServerError;

struct _Gda_ServerImplFunctions
{
  /* Connection interface */
  gboolean             (*connection_new)(Gda_ServerConnection *cnc);
  gint                 (*connection_open)(Gda_ServerConnection *cnc,
                                          const gchar *dsn,
                                          const gchar *user,
                                          const gchar *password);
  void                 (*connection_close)(Gda_ServerConnection *cnc);
  gint                 (*connection_begin_transaction)(Gda_ServerConnection *cnc);
  gint                 (*connection_commit_transaction)(Gda_ServerConnection *cnc);
  gint                 (*connection_rollback_transaction)(Gda_ServerConnection *cnc);
  Gda_ServerRecordset* (*connection_open_schema)(Gda_ServerConnection *cnc,
                                                 Gda_ServerError *error,
                                                 GDA_Connection_QType t,
                                                 GDA_Connection_Constraint *constraints,
                                                 gint length);
  gint                 (*connection_start_logging)(Gda_ServerConnection *cnc,
                                                   const gchar *filename);
  gint                 (*connection_stop_logging)(Gda_ServerConnection *cnc);
  gchar*               (*connection_create_table)(Gda_ServerConnection *cnc, GDA_RowAttributes *columns);
  gboolean             (*connection_supports)(Gda_ServerConnection *cnc, GDA_Connection_Feature feature);
  GDA_ValueType        (*connection_get_gda_type)(Gda_ServerConnection *cnc, gulong sql_type);
  gshort               (*connection_get_c_type)(Gda_ServerConnection *cnc, GDA_ValueType type);
  void                 (*connection_free)(Gda_ServerConnection *cnc);

  /* Command interface */
  gboolean             (*command_new)(Gda_ServerCommand *cmd);
  Gda_ServerRecordset* (*command_execute)(Gda_ServerCommand *cmd,
                                          Gda_ServerError *error,
                                          const GDA_CmdParameterSeq *params,
                                          gulong *affected,
                                          gulong options);
  void                 (*command_free)(Gda_ServerCommand *cmd);

  /* Recordset interface */
  gboolean (*recordset_new)      (Gda_ServerRecordset *recset);
  gint     (*recordset_move_next)(Gda_ServerRecordset *recset);
  gint     (*recordset_move_prev)(Gda_ServerRecordset *recset);
  gint     (*recordset_close)    (Gda_ServerRecordset *recset);
  void     (*recordset_free)     (Gda_ServerRecordset *recset);

  /* Error interface */
  void (*error_make)(Gda_ServerError *error,
                     Gda_ServerRecordset *recset,
                     Gda_ServerConnection *cnc,
                     gchar *where);
};

/*
 * Gda_ServerConnection management
 */
Gda_ServerConnection* gda_server_connection_new  (Gda_ServerImpl *server_impl);
gchar*                gda_server_connection_get_dsn (Gda_ServerConnection *cnc);
void                  gda_server_connection_set_dsn (Gda_ServerConnection *cnc, const gchar *dsn);
gchar*                gda_server_connection_get_username (Gda_ServerConnection *cnc);
void                  gda_server_connection_set_username (Gda_ServerConnection *cnc,
                                                          const gchar *username);
gchar*                gda_server_connection_get_password (Gda_ServerConnection *cnc);
void                  gda_server_connection_set_password (Gda_ServerConnection *cnc,
                                                          const gchar *password);
void                  gda_server_connection_add_error (Gda_ServerConnection *cnc,
                                                       Gda_ServerError *error);
void                  gda_server_connection_add_error_string (Gda_ServerConnection *cnc,
                                                              const gchar *msg);
gpointer              gda_server_connection_get_user_data (Gda_ServerConnection *cnc);
void                  gda_server_connection_set_user_data (Gda_ServerConnection *cnc, gpointer user_data);
void                  gda_server_connection_free (Gda_ServerConnection *cnc);

gint                  gda_server_connection_open (Gda_ServerConnection *cnc,
                                                  const gchar *dsn,
                                                  const gchar *user,
                                                  const gchar *password);
void                  gda_server_connection_close (Gda_ServerConnection *cnc);
gint                  gda_server_connection_begin_transaction (Gda_ServerConnection *cnc);
gint                  gda_server_connection_commit_transaction (Gda_ServerConnection *cnc);
gint                  gda_server_connection_rollback_transaction (Gda_ServerConnection *cnc);
Gda_ServerRecordset*  gda_server_connection_open_schema (Gda_ServerConnection *cnc,
                                                         Gda_ServerError *error,
                                                         GDA_Connection_QType t,
                                                         GDA_Connection_Constraint *constraints,
                                                         gint length);
gint                  gda_server_connection_start_logging (Gda_ServerConnection *cnc,
                                                           const gchar *filename);
gint                  gda_server_connection_stop_logging (Gda_ServerConnection *cnc);
gchar*                gda_server_connection_create_table (Gda_ServerConnection *cnc,
                                                          GDA_RowAttributes *columns);
gboolean              gda_server_connection_supports (Gda_ServerConnection *cnc,
                                                      GDA_Connection_Feature feature);
GDA_ValueType         gda_server_connection_get_gda_type (Gda_ServerConnection *cnc,
                                                          gulong sql_type);
gshort                gda_server_connection_get_c_type (Gda_ServerConnection *cnc,
                                                        GDA_ValueType type);

/*
 * Gda_ServerCommand management
 */
Gda_ServerCommand*    gda_server_command_new  (Gda_ServerConnection *cnc);
Gda_ServerConnection* gda_server_command_get_connection (Gda_ServerCommand *cmd);
gchar*                gda_server_command_get_text (Gda_ServerCommand *cmd);
void                  gda_server_command_set_text (Gda_ServerCommand *cmd,
                                                   const gchar *text);
gulong                gda_server_command_get_type (Gda_ServerCommand *cmd);
void                  gda_server_command_set_type (Gda_ServerCommand *cmd,
                                                   gulong type);
gpointer              gda_server_command_get_user_data (Gda_ServerCommand *cmd);
void                  gda_server_command_set_user_data (Gda_ServerCommand *cmd,
                                                        gpointer user_data);
void                  gda_server_command_free (Gda_ServerCommand *cmd);
Gda_ServerRecordset*  gda_server_command_execute (Gda_ServerCommand *cmd,
                                                  Gda_ServerError *error,
                                                  const GDA_CmdParameterSeq *params,
                                                  gulong *affected,
                                                  gulong options);

/*
 * Gda_ServerRecordset management
 */
Gda_ServerRecordset*  gda_server_recordset_new  (Gda_ServerConnection *cnc);
Gda_ServerConnection* gda_server_recordset_get_connection (Gda_ServerRecordset *recset);
void                  gda_server_recordset_add_field (Gda_ServerRecordset *recset,
                                                      Gda_ServerField *field);
GList*                gda_server_recordset_get_fields (Gda_ServerRecordset *recset);
gboolean              gda_server_recordset_is_at_begin (Gda_ServerRecordset *recset);
void                  gda_server_recordset_set_at_begin (Gda_ServerRecordset *recset,
                                                         gboolean at_begin);
gboolean              gda_server_recordset_is_at_end (Gda_ServerRecordset *recset);
void                  gda_server_recordset_set_at_end (Gda_ServerRecordset *recset,
                                                       gboolean at_end);
gpointer              gda_server_recordset_get_user_data (Gda_ServerRecordset *recset);
void                  gda_server_recordset_set_user_data (Gda_ServerRecordset *recset,
                                                          gpointer user_data);
void                  gda_server_recordset_free (Gda_ServerRecordset *recset);

gint                  gda_server_recordset_move_next (Gda_ServerRecordset *recset);
gint                  gda_server_recordset_move_prev (Gda_ServerRecordset *recset);
gint                  gda_server_recordset_close (Gda_ServerRecordset *recset);

/*
 * Gda_ServerField management
 */
Gda_ServerField* gda_server_field_new  (void);
void             gda_server_field_set_name (Gda_ServerField *field, const gchar *name);
gulong           gda_server_field_get_sql_type (Gda_ServerField *field);
void             gda_server_field_set_sql_type (Gda_ServerField *field, gulong sql_type);
void             gda_server_field_set_defined_length (Gda_ServerField *field, glong length);
void             gda_server_field_set_actual_length (Gda_ServerField *field, glong length);
void             gda_server_field_set_scale (Gda_ServerField *field, gshort scale);
gpointer         gda_server_field_get_user_data (Gda_ServerField *field);
void             gda_server_field_set_user_data (Gda_ServerField *field, gpointer user_data);
void             gda_server_field_free (Gda_ServerField *field);

void             gda_server_field_set_boolean (Gda_ServerField *field, gboolean val);
void             gda_server_field_set_date (Gda_ServerField *field, GDate *val);
void             gda_server_field_set_time (Gda_ServerField *field, GTime val);
void             gda_server_field_set_timestamp (Gda_ServerField *field, GDate *dat, GTime tim);
void             gda_server_field_set_smallint (Gda_ServerField *field, gshort val);
void             gda_server_field_set_integer (Gda_ServerField *field, gint val);
void             gda_server_field_set_longvarchar (Gda_ServerField *field, gchar *val);
void             gda_server_field_set_char (Gda_ServerField *field, gchar *val);
void             gda_server_field_set_varchar (Gda_ServerField *field, gchar *val);
void             gda_server_field_set_single (Gda_ServerField *field, gfloat val);
void             gda_server_field_set_double (Gda_ServerField *field, gdouble val);
void             gda_server_field_set_varbin (Gda_ServerField *field, gpointer val, glong size);

/*
 * Gda_ServerError management
 */
Gda_ServerError* gda_server_error_new              (void);
gchar*           gda_server_error_get_description  (Gda_ServerError *error);
void             gda_server_error_set_description  (Gda_ServerError *error, const gchar *description);
glong            gda_server_error_get_number       (Gda_ServerError *error);
void             gda_server_error_set_number       (Gda_ServerError *error, glong number);
void             gda_server_error_set_source       (Gda_ServerError *error, const gchar *source);
void             gda_server_error_set_help_file    (Gda_ServerError *error, const gchar *helpfile);
void             gda_server_error_set_help_context (Gda_ServerError *error, const gchar *helpctxt);
void             gda_server_error_set_sqlstate     (Gda_ServerError *error, const gchar *sqlstate);
void             gda_server_error_set_native       (Gda_ServerError *error, const gchar *native);
void             gda_server_error_free             (Gda_ServerError *error);

void             gda_server_error_make             (Gda_ServerError *error,
                                                    Gda_ServerRecordset *recset,
                                                    Gda_ServerConnection *cnc,
                                                    gchar *where);

/*
 * Gda_ServerImpl object - interface for applications
 */
struct _Gda_ServerImpl
{
#ifdef HAVE_GOBJECT
  GObject                 object;
#else
  GtkObject               object;
#endif
  CORBA_Object            connection_factory_obj;
  PortableServer_POA      root_poa;
  CORBA_Environment*      ev;
  gchar*                  name;
  Gda_ServerImplFunctions functions;
  GList*                  connections;
  gboolean                is_running;
};

struct _Gda_ServerImplClass
{
#ifdef HAVE_GOBJECT
  GObjectClass  parent_class;
  GObjectClass *parent;
#else
  GtkObjectClass parent_class;
#endif
};

#ifdef HAVE_GOBJECT
GType           gda_server_impl_get_type (void);
#else
GtkType         gda_server_impl_get_type (void);
#endif

Gda_ServerImpl* gda_server_impl_new      (const gchar *name, Gda_ServerImplFunctions *functions);
Gda_ServerImpl* gda_server_impl_find     (const gchar *id);
void            gda_server_impl_start    (Gda_ServerImpl *server_impl);
void            gda_server_impl_stop     (Gda_ServerImpl *server_impl);

#define         gda_server_impl_is_running(_simpl_) ((_simpl_) ? (_simpl_)->is_running : FALSE)

#define         gda_server_impl_get_name(_simpl_) ((_simpl_) ? (_simpl_)->name : NULL)
#define         gda_server_impl_get_connections(_simpl_) ((_simpl_) ? (_simpl_)->connections : NULL)

/* for private use */
gint       gda_server_impl_exception         (CORBA_Environment *ev);
GDA_Error* gda_server_impl_make_error_buffer (Gda_ServerConnection *cnc);

#if defined(__cplusplus)
}
#endif

#endif
