/* GDA Server Library
 * Copyright (C) 2000 Rodrigo Moya
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

#if !defined(__gda_server_impl_h__)
#  define __gda_server_impl_h__

#include <glib.h>

#ifdef HAVE_GOBJECT
#  include <glib-object.h>
#else
#  include <gtk/gtkobject.h>
#endif

#include <orb/orbit.h>
#include <GDA.h>
#include <gda-common.h>

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct _GdaServerImpl      GdaServerImpl;
typedef struct _GdaServerImplClass GdaServerImplClass;

#define GDA_TYPE_SERVER_IMPL            (gda_server_impl_get_type())

#ifdef HAVE_GOBJECT
#  define GDA_SERVER_IMPL(obj) \
         G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_SERVER_IMPL, GdaServerImpl)
#  define GDA_SERVER_IMPL_CLASS(klass) \
     G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_SERVER_IMPL, GdaServerImplClass)
#  define IS_GDA_SERVER_IMPL(obj) \
          G_TYPE_CHECK_INSTANCE_TYPE (obj, GDA_TYPE_SERVER_IMPL)
#  define IS_GDA_SERVER_IMPL_CLASS(klass) \
          G_TYPE_CHECK_CLASS_TYPE (klass, GDA_TYPE_SERVER_IMPL)
#else
#  define GDA_SERVER_IMPL(obj)            GTK_CHECK_CAST(obj, GDA_TYPE_SERVER_IMPL, GdaServerImpl)
#  define GDA_SERVER_IMPL_CLASS(klass)    GTK_CHECK_CLASS_CAST(klass, GDA_TYPE_SERVER_IMPL, GdaServerImplClass)
#  define IS_GDA_SERVER_IMPL(obj)         GTK_CHECK_TYPE(obj, GDA_TYPE_SERVER_IMPL)
#  define IS_GDA_SERVER_IMPL_CLASS(klass) (GTK_CHECK_CLASS_TYPE((klass), GDA_TYPE_SERVER_IMPL))
#endif

typedef struct _GdaServerImplFunctions GdaServerImplFunctions;

typedef struct {
	GdaServerImpl* server_impl;
	gchar*          dsn;
	gchar*          username;
	gchar*          password;
	GList*          commands;
	GList*          errors;
	gint            users;

	gpointer        user_data;
} GdaServerConnection;

typedef struct {
	GdaServerConnection* cnc;
	gchar*                text;
	GDA_CommandType       type;
	gint                  users;
	
	gpointer              user_data;
} GdaServerCommand;

typedef struct {
	GdaServerConnection* cnc;
	GList*                fields;
	gulong                position;
	gboolean              at_begin;
	gboolean              at_end;
	gint                  users;
	
	gpointer              user_data;
} GdaServerRecordset;

typedef struct {
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
} GdaServerField;

typedef struct {
	gchar* description;
	glong  number;
	gchar* source;
	gchar* helpfile;
	gchar* helpctxt;
	gchar* sqlstate;
	gchar* native;
} GdaServerError;

struct _GdaServerImplFunctions {
	/* Connection interface */
	gboolean             (*connection_new)(GdaServerConnection *cnc);
	gint                 (*connection_open)(GdaServerConnection *cnc,
	                                        const gchar *dsn,
	                                        const gchar *user,
	                                        const gchar *password);
	void                 (*connection_close)(GdaServerConnection *cnc);
	gint                 (*connection_begin_transaction)(GdaServerConnection *cnc);
	gint                 (*connection_commit_transaction)(GdaServerConnection *cnc);
	gint                 (*connection_rollback_transaction)(GdaServerConnection *cnc);
	GdaServerRecordset* (*connection_open_schema)(GdaServerConnection *cnc,
						      GdaServerError *error,
						      GDA_Connection_QType t,
						      GDA_Connection_Constraint *constraints,
						      gint length);
	glong                (*connection_modify_schema)(GdaServerConnection *cnc,
							 GDA_Connection_QType t,
							 GDA_Connection_Constraint *constraints,
							 gint length);
	gint                 (*connection_start_logging)(GdaServerConnection *cnc,
							 const gchar *filename);
	gint                 (*connection_stop_logging)(GdaServerConnection *cnc);
	gchar*               (*connection_create_table)(GdaServerConnection *cnc, GDA_RowAttributes *columns);
	gboolean             (*connection_supports)(GdaServerConnection *cnc, GDA_Connection_Feature feature);
	GDA_ValueType        (*connection_get_gda_type)(GdaServerConnection *cnc, gulong sql_type);
	gshort               (*connection_get_c_type)(GdaServerConnection *cnc, GDA_ValueType type);
	gchar*               (*connection_sql2xml)(GdaServerConnection *cnc,
                                                   const gchar *sql);
	gchar*               (*connection_xml2sql)(GdaServerConnection *cnc,
                                                   const gchar *xml);
	void                 (*connection_free)(GdaServerConnection *cnc);

	/* Command interface */
	gboolean             (*command_new)(GdaServerCommand *cmd);
	GdaServerRecordset* (*command_execute)(GdaServerCommand *cmd,
					       GdaServerError *error,
					       const GDA_CmdParameterSeq *params,
					       gulong *affected,
					       gulong options);
	void                 (*command_free)(GdaServerCommand *cmd);

	/* Recordset interface */
	gboolean (*recordset_new)      (GdaServerRecordset *recset);
	gint     (*recordset_move_next)(GdaServerRecordset *recset);
	gint     (*recordset_move_prev)(GdaServerRecordset *recset);
	gint     (*recordset_close)    (GdaServerRecordset *recset);
	void     (*recordset_free)     (GdaServerRecordset *recset);
	
	/* Error interface */
	void (*error_make)(GdaServerError *error,
	                   GdaServerRecordset *recset,
	                   GdaServerConnection *cnc,
	                   gchar *where);
};

/*
 * GdaServerConnection management
 */
GdaServerConnection* gda_server_connection_new  (GdaServerImpl *server_impl);
gchar*               gda_server_connection_get_dsn (GdaServerConnection *cnc);
void                 gda_server_connection_set_dsn (GdaServerConnection *cnc, const gchar *dsn);
gchar*               gda_server_connection_get_username (GdaServerConnection *cnc);
void                 gda_server_connection_set_username (GdaServerConnection *cnc,
							 const gchar *username);
gchar*               gda_server_connection_get_password (GdaServerConnection *cnc);
void                 gda_server_connection_set_password (GdaServerConnection *cnc,
                                                          const gchar *password);
void                 gda_server_connection_add_error (GdaServerConnection *cnc,
                                                       GdaServerError *error);
void                 gda_server_connection_add_error_string (GdaServerConnection *cnc,
                                                              const gchar *msg);
gpointer             gda_server_connection_get_user_data (GdaServerConnection *cnc);
void                 gda_server_connection_set_user_data (GdaServerConnection *cnc, gpointer user_data);
void                 gda_server_connection_free (GdaServerConnection *cnc);

gint                 gda_server_connection_open (GdaServerConnection *cnc,
                                                  const gchar *dsn,
                                                  const gchar *user,
                                                  const gchar *password);
void                 gda_server_connection_close (GdaServerConnection *cnc);
gint                 gda_server_connection_begin_transaction (GdaServerConnection *cnc);
gint                 gda_server_connection_commit_transaction (GdaServerConnection *cnc);
gint                 gda_server_connection_rollback_transaction (GdaServerConnection *cnc);
GdaServerRecordset*  gda_server_connection_open_schema (GdaServerConnection *cnc,
                                                         GdaServerError *error,
                                                         GDA_Connection_QType t,
                                                         GDA_Connection_Constraint *constraints,
                                                         gint length);
gint                 gda_server_connection_start_logging (GdaServerConnection *cnc,
                                                           const gchar *filename);
gint                 gda_server_connection_stop_logging (GdaServerConnection *cnc);
gchar*               gda_server_connection_create_table (GdaServerConnection *cnc,
                                                          GDA_RowAttributes *columns);
gboolean             gda_server_connection_supports (GdaServerConnection *cnc,
                                                      GDA_Connection_Feature feature);
GDA_ValueType        gda_server_connection_get_gda_type (GdaServerConnection *cnc,
                                                          gulong sql_type);
gshort               gda_server_connection_get_c_type (GdaServerConnection *cnc,
                                                        GDA_ValueType type);
gchar*               gda_server_connection_sql2xml (GdaServerConnection *cnc,
                                                     const gchar *sql);
gchar*               gda_server_connection_xml2sql (GdaServerConnection *cnc,
                                                     const gchar *xml);

/*
 * GdaServerCommand management
 */
GdaServerCommand*    gda_server_command_new  (GdaServerConnection *cnc);
GdaServerConnection* gda_server_command_get_connection (GdaServerCommand *cmd);
gchar*               gda_server_command_get_text (GdaServerCommand *cmd);
void                 gda_server_command_set_text (GdaServerCommand *cmd,
                                                   const gchar *text);
GDA_CommandType      gda_server_command_get_type (GdaServerCommand *cmd);
void                 gda_server_command_set_type (GdaServerCommand *cmd,
                                                   GDA_CommandType type);
gpointer             gda_server_command_get_user_data (GdaServerCommand *cmd);
void                 gda_server_command_set_user_data (GdaServerCommand *cmd,
                                                        gpointer user_data);
void                 gda_server_command_free (GdaServerCommand *cmd);
GdaServerRecordset*  gda_server_command_execute (GdaServerCommand *cmd,
                                                  GdaServerError *error,
                                                  const GDA_CmdParameterSeq *params,
                                                  gulong *affected,
                                                  gulong options);

/*
 * GdaServerRecordset management
 */
GdaServerRecordset*  gda_server_recordset_new  (GdaServerConnection *cnc);
GdaServerConnection* gda_server_recordset_get_connection (GdaServerRecordset *recset);
void                 gda_server_recordset_add_field (GdaServerRecordset *recset,
                                                      GdaServerField *field);
GList*               gda_server_recordset_get_fields (GdaServerRecordset *recset);
gboolean             gda_server_recordset_is_at_begin (GdaServerRecordset *recset);
void                 gda_server_recordset_set_at_begin (GdaServerRecordset *recset,
                                                         gboolean at_begin);
gboolean             gda_server_recordset_is_at_end (GdaServerRecordset *recset);
void                 gda_server_recordset_set_at_end (GdaServerRecordset *recset,
                                                       gboolean at_end);
gpointer             gda_server_recordset_get_user_data (GdaServerRecordset *recset);
void                 gda_server_recordset_set_user_data (GdaServerRecordset *recset,
                                                          gpointer user_data);
void                 gda_server_recordset_free (GdaServerRecordset *recset);

gint                 gda_server_recordset_move_next (GdaServerRecordset *recset);
gint                 gda_server_recordset_move_prev (GdaServerRecordset *recset);
gint                 gda_server_recordset_close (GdaServerRecordset *recset);

/*
 * GdaServerField management
 */
GdaServerField* gda_server_field_new  (void);
void            gda_server_field_set_name (GdaServerField *field, const gchar *name);
gulong          gda_server_field_get_sql_type (GdaServerField *field);
void            gda_server_field_set_sql_type (GdaServerField *field, gulong sql_type);
void            gda_server_field_set_defined_length (GdaServerField *field, glong length);
void            gda_server_field_set_actual_length (GdaServerField *field, glong length);
void            gda_server_field_set_scale (GdaServerField *field, gshort scale);
gpointer        gda_server_field_get_user_data (GdaServerField *field);
void            gda_server_field_set_user_data (GdaServerField *field, gpointer user_data);
void            gda_server_field_free (GdaServerField *field);

void            gda_server_field_set_boolean (GdaServerField *field, gboolean val);
void            gda_server_field_set_date (GdaServerField *field, GDate *val);
void            gda_server_field_set_time (GdaServerField *field, GTime val);
void            gda_server_field_set_timestamp (GdaServerField *field, GDate *dat, GTime tim);
void            gda_server_field_set_smallint (GdaServerField *field, gshort val);
void            gda_server_field_set_integer (GdaServerField *field, gint val);
void            gda_server_field_set_longvarchar (GdaServerField *field, gchar *val);
void            gda_server_field_set_char (GdaServerField *field, gchar *val);
void            gda_server_field_set_varchar (GdaServerField *field, gchar *val);
void            gda_server_field_set_single (GdaServerField *field, gfloat val);
void            gda_server_field_set_double (GdaServerField *field, gdouble val);
void            gda_server_field_set_varbin (GdaServerField *field, gpointer val, glong size);

/*
 * GdaServerError management
 */
GdaServerError* gda_server_error_new              (void);
gchar*          gda_server_error_get_description  (GdaServerError *error);
void            gda_server_error_set_description  (GdaServerError *error, const gchar *description);
glong           gda_server_error_get_number       (GdaServerError *error);
void            gda_server_error_set_number       (GdaServerError *error, glong number);
void            gda_server_error_set_source       (GdaServerError *error, const gchar *source);
void            gda_server_error_set_help_file    (GdaServerError *error, const gchar *helpfile);
void            gda_server_error_set_help_context (GdaServerError *error, const gchar *helpctxt);
void            gda_server_error_set_sqlstate     (GdaServerError *error, const gchar *sqlstate);
void            gda_server_error_set_native       (GdaServerError *error, const gchar *native);
void            gda_server_error_free             (GdaServerError *error);

void            gda_server_error_make             (GdaServerError *error,
						   GdaServerRecordset *recset,
						   GdaServerConnection *cnc,
						   gchar *where);

/*
 * GdaServerImpl object - interface for providers implementations
 */
struct _GdaServerImpl {
#ifdef HAVE_GOBJECT
	GObject                 object;
#else
	GtkObject               object;
#endif
	CORBA_Object            connection_factory_obj;
	PortableServer_POA      root_poa;
	CORBA_Environment*      ev;
	gchar*                  name;
	GdaServerImplFunctions functions;
	GList*                  connections;
	gboolean                is_running;
};

struct _GdaServerImplClass {
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

GdaServerImpl* gda_server_impl_new      (const gchar *name, GdaServerImplFunctions *functions);
GdaServerImpl* gda_server_impl_find     (const gchar *id);
void            gda_server_impl_start    (GdaServerImpl *server_impl);
void            gda_server_impl_stop     (GdaServerImpl *server_impl);

#define         gda_server_impl_is_running(_simpl_) ((_simpl_) ? (_simpl_)->is_running : FALSE)

#define         gda_server_impl_get_name(_simpl_) ((_simpl_) ? (_simpl_)->name : NULL)
#define         gda_server_impl_get_connections(_simpl_) ((_simpl_) ? (_simpl_)->connections : NULL)

/* for private use */
gint       gda_server_impl_exception         (CORBA_Environment *ev);
GDA_Error* gda_server_impl_make_error_buffer (GdaServerConnection *cnc);

#if defined(__cplusplus)
}
#endif

#endif
