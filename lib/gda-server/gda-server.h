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

#if !defined(__gda_server_h__)
#  define __gda_server_h__

#include <gtk/gtkobject.h>
#include <bonobo/bonobo-generic-factory.h>

/*
 * This is the main header file for the libgda-server library
 */

typedef struct _GdaServer GdaServer;
typedef struct _GdaServerClass GdaServerClass;
typedef struct _GdaServerImplFunctions GdaServerImplFunctions;

#include <GDA.h>
#include <gda-common-defs.h>
#include <gda-error.h>
#include <gda-server-connection.h>
#include <gda-server-command.h>
#include <gda-server-field.h>
#include <gda-server-recordset.h>
#include <gda-server-error.h>

G_BEGIN_DECLS

/*
 * The GdaServer class
 */

#define GDA_TYPE_SERVER            (gda_server_get_type())

#ifdef HAVE_GOBJECT
#define GDA_SERVER(obj)            G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_SERVER, GdaServer)
#define GDA_SERVER_CLASS(klass)    G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_SERVER, GdaServerClass)
#define GDA_IS_SERVER_(obj)        G_TYPE_CHECK_INSTANCE_TYPE (obj, GDA_TYPE_SERVER)
#define GDA_IS_SERVER_CLASS(klass) G_TYPE_CHECK_CLASS_TYPE (klass, GDA_TYPE_SERVER)
#else
#define GDA_SERVER(obj)            GTK_CHECK_CAST(obj, GDA_TYPE_SERVER, GdaServer)
#define GDA_SERVER_CLASS(klass)    GTK_CHECK_CLASS_CAST(klass, GDA_TYPE_SERVER, GdaServerClass)
#define GDA_IS_SERVER(obj)         GTK_CHECK_TYPE(obj, GDA_TYPE_SERVER)
#define GDA_IS_SERVER_CLASS(klass) (GTK_CHECK_CLASS_TYPE((klass), GDA_TYPE_SERVER))
#endif

struct _GdaServerImplFunctions {
	/* Connection interface */
	gboolean (*connection_new) (GdaServerConnection * cnc);
	gint (*connection_open) (GdaServerConnection * cnc,
				 const gchar * dsn,
				 const gchar * user,
				 const gchar * password);
	void (*connection_close) (GdaServerConnection * cnc);
	gint (*connection_begin_transaction) (GdaServerConnection *cnc);
	gint (*connection_commit_transaction) (GdaServerConnection *cnc);
	gint (*connection_rollback_transaction) (GdaServerConnection *cnc);
	GdaServerRecordset *(*connection_open_schema) (GdaServerConnection * cnc,
					    GdaError * error,
					    GDA_Connection_QType t,
					    GDA_Connection_Constraint *constraints,
					    gint length);
	glong (*connection_modify_schema) (GdaServerConnection *cnc,
					     GDA_Connection_QType t,
					     GDA_Connection_Constraint *constraints,
					     gint length);
	gint (*connection_start_logging) (GdaServerConnection * cnc,
					    const gchar * filename);
	gint (*connection_stop_logging) (GdaServerConnection * cnc);
	gchar *(*connection_create_table) (GdaServerConnection * cnc,
					   GDA_RowAttributes *columns);
	gboolean (*connection_supports) (GdaServerConnection * cnc,
					   GDA_Connection_Feature feature);
	GDA_ValueType (*connection_get_gda_type)
			(GdaServerConnection * cnc, gulong sql_type);
	gshort (*connection_get_c_type) (GdaServerConnection * cnc,
						   GDA_ValueType type);
	gchar *(*connection_sql2xml) (GdaServerConnection * cnc,
					      const gchar * sql);
	gchar *(*connection_xml2sql) (GdaServerConnection * cnc,
					      const gchar * xml);
	void (*connection_free) (GdaServerConnection * cnc);

	/* Command interface */
	gboolean (*command_new) (GdaServerCommand * cmd);
	GdaServerRecordset *(*command_execute) (GdaServerCommand *
							cmd, GdaError * error,
							const
							GDA_CmdParameterSeq *
							params,
							gulong * affected,
							gulong options);
	void (*command_free) (GdaServerCommand * cmd);

	/* Recordset interface */
	gboolean (*recordset_new) (GdaServerRecordset * recset);
	gint (*recordset_move_next) (GdaServerRecordset * recset);
	gint (*recordset_move_prev) (GdaServerRecordset * recset);
	gint (*recordset_close) (GdaServerRecordset * recset);
	void (*recordset_free) (GdaServerRecordset * recset);

	/* Error interface */
	void (*error_make) (GdaError * error,
			    GdaServerRecordset * recset,
			    GdaServerConnection * cnc, gchar * where);
};

struct _GdaServer {
#ifdef HAVE_GOBJECT
	GObject object;
#else
	GtkObject object;
#endif
	BonoboGenericFactory *connection_factory;
	gchar *name;
	GdaServerImplFunctions functions;
	GList *connections;
	gboolean is_running;
};

struct _GdaServerClass {
#ifdef HAVE_GOBJECT
	GObjectClass parent_class;
	GObjectClass *parent;
#else
	GtkObjectClass parent_class;
#endif
};

#ifdef HAVE_GOBJECT
GType gda_server_get_type (void);
#else
GtkType gda_server_get_type (void);
#endif

GdaServer *gda_server_new (const gchar * name,
			   GdaServerImplFunctions * functions);
GdaServer *gda_server_find (const gchar * id);
void gda_server_start (GdaServer * server_impl);
void gda_server_stop (GdaServer * server_impl);

#define    gda_server_is_running(_simpl_) ((_simpl_) ? (_simpl_)->is_running : FALSE)
#define    gda_server_get_name(_simpl_) ((_simpl_) ? (_simpl_)->name : NULL)
#define    gda_server_get_connections(_simpl_) ((_simpl_) ? (_simpl_)->connections : NULL)

/* for private use */
gint gda_server_exception (CORBA_Environment * ev);

/*
 * Initialization function
 */
void gda_server_init (const gchar * app_id, const gchar * version,
		      gint nargs, gchar * args[]);

G_END_DECLS

#endif
