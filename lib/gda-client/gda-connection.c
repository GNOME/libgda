/* GDA client library
 * Copyright (C) 1998,1999 Michael Lausch
 * Copyright (C) 1999,2000 Rodrigo Moya
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

#include "config.h"
#include "gda-connection.h"
#include "gda-command.h"
#include "gda-config.h"
#include "gda-corba.h"

#ifndef HAVE_GOBJECT
#  include <gtk/gtksignal.h>
#endif

#ifdef ENABLE_NLS
#  include <libintl.h>
#  define _(String) gettext (String)
#  define N_(String) (String)
#else
/* Stubs that do something close enough. */
#  define textdomain(String)
#  define gettext(String) (String)
#  define dgettext(Domain,Message) (Message)
#  define dcgettext(Domain,Message,Type) (Message)
#  define bindtextdomain(Domain,Directory)
#  define _(String) (String)
#  define N_(String) (String)
#endif

static GHashTable* factories = NULL;
static GtkObjectClass *parent_class = NULL;

enum {
	CONNECTION_ERROR,
	CONNECTION_WARNING,
	CONNECTION_OPEN,
	CONNECTION_CLOSE,
	LAST_SIGNAL
};

static gint
gda_connection_signals[LAST_SIGNAL] = {0, };

#ifdef HAVE_GOBJECT
static void    gda_connection_class_init    (GdaConnectionClass *klass,
                                             gpointer data);
static void    gda_connection_init          (GdaConnection *cnc,
                                             GdaConnectionClass *klass);
#else
static void    gda_connection_class_init    (GdaConnectionClass* klass);
static void    gda_connection_init          (GdaConnection* cnc);
static void    gda_connection_destroy       (GtkObject *object, gpointer user_data);
#endif

static void    gda_connection_real_error    (GdaConnection* cnc, GList*);
static void    gda_connection_real_warning  (GdaConnection* cnc, GList*);
static int     get_corba_connection         (GdaConnection* cnc);

static void
gda_connection_real_error (GdaConnection* cnc, GList* errors)
{
	g_print("%s: %d: %s called\n", __FILE__, __LINE__, __PRETTY_FUNCTION__);
	cnc->errors_head = g_list_concat(cnc->errors_head, errors);
}

static void
gda_connection_real_warning (GdaConnection* cnc, GList* warnings)
{
	g_print("%s: %d: %s called\n", __FILE__, __LINE__, __PRETTY_FUNCTION__);
}

#ifdef HAVE_GOBJECT
GType
gda_connection_get_type (void)
{
	static GType type = 0;
	
	if (!type) {
		GTypeInfo info = {
			sizeof (GdaConnectionClass),               /* class_size */
			NULL,                                       /* base_init */
			NULL,                                       /* base_finalize */
			(GClassInitFunc) gda_connection_class_init, /* class_init */
			NULL,                                       /* class_finalize */
			NULL,                                       /* class_data */
			sizeof (GdaConnection),                    /* instance_size */
			0,                                          /* n_preallocs */
			(GInstanceInitFunc) gda_connection_init,    /* instance_init */
			NULL,                                       /* value_table */
		};
		type = g_type_register_static (G_TYPE_OBJECT, "GdaConnection", &info, 0);
	}
	return type;
}
#else
GtkType
gda_connection_get_type (void)
{
	static guint gda_connection_type = 0;
	
	if (!gda_connection_type) {
		GtkTypeInfo gda_connection_info = {
			"GdaConnection",
			sizeof (GdaConnection),
			sizeof (GdaConnectionClass),
			(GtkClassInitFunc) gda_connection_class_init,
			(GtkObjectInitFunc) gda_connection_init,
			(GtkArgSetFunc)NULL,
			(GtkArgSetFunc)NULL,
		};
		gda_connection_type = gtk_type_unique(gtk_object_get_type(),
		                                      &gda_connection_info);
	}
	return gda_connection_type;
}
#endif

#ifdef HAVE_GOBJECT
static void
gda_connection_class_init (GdaConnectionClass *klass, gpointer data)
{
	/* FIXME: No GObject signals yet */
	klass->error = gda_connection_real_error;
	klass->warning = gda_connection_real_warning;
	klass->open = NULL;
	klass->close = NULL;
}
#else
static void
gda_connection_class_init (GdaConnectionClass* klass)
{
	GtkObjectClass*   object_class;

	parent_class = gtk_type_class(gtk_object_get_type());

	object_class = (GtkObjectClass*) klass;
	
	gda_connection_signals[CONNECTION_ERROR] =
		gtk_signal_new("error",
			       GTK_RUN_FIRST,
			       object_class->type,
			       GTK_SIGNAL_OFFSET(GdaConnectionClass, error),
			       gtk_marshal_NONE__POINTER,
			       GTK_TYPE_NONE, 1, GTK_TYPE_POINTER);
	gda_connection_signals[CONNECTION_WARNING] =
		gtk_signal_new("warning",
			       GTK_RUN_LAST,
			       object_class->type,
			       GTK_SIGNAL_OFFSET(GdaConnectionClass, warning),
			       gtk_marshal_NONE__POINTER,
			       GTK_TYPE_NONE, 1, GTK_TYPE_POINTER);
	gda_connection_signals[CONNECTION_OPEN] =
		gtk_signal_new("open",
			       GTK_RUN_LAST,
			       object_class->type,
			       GTK_SIGNAL_OFFSET(GdaConnectionClass, open),
			       gtk_signal_default_marshaller,
			       GTK_TYPE_NONE, 0);
	gda_connection_signals[CONNECTION_CLOSE] =
		gtk_signal_new("close",
			       GTK_RUN_LAST,
			       object_class->type,
			       GTK_SIGNAL_OFFSET(GdaConnectionClass, close),
			       gtk_signal_default_marshaller,
			       GTK_TYPE_NONE, 0);

	gtk_object_class_add_signals(object_class, gda_connection_signals, LAST_SIGNAL);

	object_class->destroy = gda_connection_destroy;
	klass->error   = gda_connection_real_error;
	klass->warning = gda_connection_real_warning;
	klass->close   = NULL;
	klass->open    = NULL;
}
#endif

static void
#ifdef HAVE_GOBJECT
gda_connection_init (GdaConnection *cnc, GdaConnectionClass *klass)
#else
gda_connection_init (GdaConnection* cnc)
#endif
{
	cnc->connection = CORBA_OBJECT_NIL;
	cnc->commands = NULL;
	cnc->recordsets = NULL;
	cnc->provider = NULL;
	cnc->default_db = NULL;
	cnc->database = NULL;
	cnc->user = NULL;
	cnc->passwd = NULL;
	cnc->errors_head = NULL;
	cnc->is_open = FALSE;
}

gint
gda_connection_corba_exception (GdaConnection* cnc, CORBA_Environment* ev)
{
	GdaError* error;
	
	g_return_val_if_fail(ev != 0, -1);
	
	switch (ev->_major) {
	case CORBA_NO_EXCEPTION:
		return 0;
	case CORBA_SYSTEM_EXCEPTION: {
		CORBA_SystemException* sysexc;
		
		sysexc = CORBA_exception_value(ev);
		error = gda_error_new();
		error->source = g_strdup("[CORBA System Exception]");
		switch(sysexc->minor) {
		case ex_CORBA_COMM_FAILURE:
			error->description = g_strdup_printf(_("%s: The server didn't respond."),
							     CORBA_exception_id(ev));
			break;
		default:
			error->description = g_strdup_printf(_("%s: An Error occured in the CORBA system."),
							     CORBA_exception_id(ev));
			break;
		}
		gda_connection_add_single_error(cnc, error);
		return -1;
	}
	case CORBA_USER_EXCEPTION:
		if (strcmp(CORBA_exception_id(ev), ex_GDA_NotSupported) == 0) {
			GDA_NotSupported* not_supported_error;
			
			not_supported_error = (GDA_NotSupported*)ev->_params;
			error = gda_error_new();
			error->source = g_strdup("[CORBA User Exception]");
			error->description = g_strdup(not_supported_error->errormsg);
			gda_connection_add_single_error(cnc, error);
			return -1;
		}
		if (strcmp(CORBA_exception_id(ev), ex_GDA_DriverError) == 0) {
			GDA_ErrorSeq  error_sequence = ((GDA_DriverError*)ev->_params)->errors;
			gint          idx;
				
			for (idx = 0; idx < error_sequence._length; idx++) {
				GDA_Error* gda_error = &error_sequence._buffer[idx];
				
				error = gda_error_new();
				if (gda_error->source)
					error->source = g_strdup(gda_error->source);
				error->number = gda_error->number;
				if (gda_error->sqlstate)
					error->sqlstate = g_strdup(gda_error->sqlstate);
				if (gda_error->nativeMsg)
					error->nativeMsg = g_strdup(gda_error->nativeMsg);
				if (gda_error->description)
					error->description = g_strdup(gda_error->description);
				gda_connection_add_single_error(cnc, error);
			}
		}
		return -1;
	default:
		g_error("Unknown CORBA exception for connection");
	}
	return 0;
}

static int
get_corba_connection (GdaConnection* cnc)
{
	CORBA_Environment     ev;
	GDA_ConnectionFactory factory;
	
	CORBA_exception_init(&ev);

	g_return_val_if_fail (IS_GDA_CONNECTION (cnc), -1);
	g_return_val_if_fail (cnc->provider != NULL, -1);

	/* get the connection factory */
	if (!factories)
		factories = g_hash_table_new(g_str_hash, g_str_equal);

	factory = g_hash_table_lookup(factories, cnc->provider);
	if (!factory) {
		factory = oaf_activate_from_id(cnc->provider, 0, NULL, &ev);
		if (factory == CORBA_OBJECT_NIL ||
		    gda_connection_corba_exception(cnc, &ev)) {
			return -1;
		}
		g_hash_table_insert(factories, cnc->provider, factory);
	}

	/* create the connection */
	cnc->connection = GDA_ConnectionFactory_create_connection(factory, cnc->provider, &ev);
	if (CORBA_Object_is_nil(cnc->connection, &ev) ||
	    gda_connection_corba_exception(cnc, &ev)) {
		return -1;
	}

	return 0;
}

/**
 * gda_connection_new:
 * @orb: the ORB
 *
 * Allocates space for a client side connection object
 *
 * Returns: the pointer to the allocated object
 */
GdaConnection*
gda_connection_new (CORBA_ORB orb)
{
	GdaConnection* cnc;
	
	g_return_val_if_fail(orb != NULL, 0);

#ifdef HAVE_GOBJECT
	cnc = GDA_CONNECTION (g_object_new (GDA_TYPE_CONNECTION, NULL));
#else
	cnc = gtk_type_new(gda_connection_get_type());
#endif
	cnc->orb = orb;
	return cnc;
}

/**
 * gda_connection_new_from_dsn
 * @dsn_name: Data source name
 * @username: User name
 * @password: Password
 *
 * Creates a #GdaConnection object from the configuration stored for the
 * given data source.
 *
 * Returns: an #GdaConnection object already connected to the underlying
 * database.
 */
GdaConnection *
gda_connection_new_from_dsn (const gchar *dsn_name,
                             const gchar *username,
                             const gchar *password)
{
	GdaDsn *dsn;
	GdaConnection *cnc;

	g_return_val_if_fail (dsn_name != NULL, NULL);

	/* find the data source in the configuration */
	dsn = gda_dsn_find_by_name (dsn_name);
	if (!dsn)
		return NULL;

	cnc = gda_connection_new (gda_corba_get_orb ());
	gda_connection_set_provider (cnc, GDA_DSN_PROVIDER (dsn));
	if (gda_connection_open (cnc,
	                         GDA_DSN_DSN (dsn),
	                         username,
                                 password) != 0) {
		gda_connection_free (cnc);
		cnc = NULL;
	}

	gda_dsn_free (dsn);

	return cnc;
}

#ifndef HAVE_GOBJECT
static void
gda_connection_destroy (GtkObject *object, gpointer user_data)
{
	GdaConnection *cnc = (GdaConnection *) object;

	g_return_if_fail(IS_GDA_CONNECTION(cnc));

	/* be sure to emit "close" signal for listeners */
	if (gda_connection_is_open(cnc)) {
		if (cnc->commands)
			g_warning("Commands still associated with gda_connection");
		if (cnc->recordsets)
			g_warning("Recordsets still associated with gda_connection");
		if (cnc->errors_head)
			g_warning("Errors still associated with gda_connection");
		gda_connection_close(cnc);
	}
	else
		gtk_signal_emit(GTK_OBJECT(cnc), gda_connection_signals[CONNECTION_CLOSE]);

	/* free all memory */
	if (cnc->provider)
		g_free(cnc->provider);
	if (cnc->default_db)
		g_free(cnc->default_db);
	if (cnc->database)
		g_free(cnc->database);
	if (cnc->user)
		g_free(cnc->user);
	if (cnc->passwd)
		g_free(cnc->passwd);

	if (GTK_OBJECT_CLASS(parent_class)->destroy)
		GTK_OBJECT_CLASS(parent_class)->destroy(object);
}
#endif

/**
 * gda_connection_free:
 * @cnc: the connection
 *
 * If the connection is open the connection is closed and all
 * associated objects (commands, recordsets, errors) are deleted. The
 * memory is freed. The connection object cannot be used any more.
 *
 */
void
gda_connection_free (GdaConnection* cnc)
{
#ifdef HAVE_GOBJECT
	g_object_unref (G_OBJECT (cnc));
#else
	gtk_object_unref (GTK_OBJECT (cnc));
#endif
}  

/**
 * gda_connection_set_provider:
 * @cnc: connection object
 * @name: name of the provider
 *
 * Registers @name as the provider for this connection. This
 * should be the id of the CORBA server to
 * use
 *
 */
void
gda_connection_set_provider (GdaConnection* cnc, gchar* name)
{
	g_return_if_fail(IS_GDA_CONNECTION(cnc));
	g_return_if_fail(name != 0);
	
	if (cnc->provider) g_free(cnc->provider);
	cnc->provider = g_strdup(name);
}

/**
 * gda_connection_get_provider:
 * @cnc: Connection object
 *
 * Retuns the provider used for this connection
 *
 * Returns: a reference to the provider name
 */
 
const gchar*
gda_connection_get_provider (GdaConnection* cnc)
{
	g_return_val_if_fail(IS_GDA_CONNECTION(cnc), 0);
	return (g_strdup(cnc->provider));
}

/**
 * gda_connection_supports
 * @cnc: the connection object
 * @feature: feature to be tested
 *
 * Return information on features supported by the underlying database
 * system.
 *
 * Returns: whether the requested feature is supported or not
 */
gboolean
gda_connection_supports (GdaConnection *cnc, GDA_Connection_Feature feature)
{
	CORBA_Environment ev;
	gboolean          rc;
	
	g_return_val_if_fail(IS_GDA_CONNECTION(cnc), FALSE);
	
	CORBA_exception_init(&ev);
	rc = GDA_Connection_supports(cnc->connection, feature, &ev);
	if (gda_connection_corba_exception(cnc, &ev) < 0)
		return FALSE;

	return rc;
}

/**
 * gda_connection_set_default_db:
 * @cnc: the connection object
 * @dsn: the DSN of the default database
 *
 * Registers the DSN as the name of the default DB to access when the
 * connection is opened
 *
 */
void
gda_connection_set_default_db (GdaConnection* cnc, gchar* dsn)
{
	g_return_if_fail(IS_GDA_CONNECTION(cnc));
	g_return_if_fail(dsn != 0);
	
	if (cnc->default_db)
		g_free(cnc->default_db);
	cnc->default_db = g_strdup(dsn);
}

/**
 * gda_connection_open:
 * @cnc: the connection object which describes the server and
 *        the database name to which the connection shuld be opened
 * @dsn: The DSN of the database. Can be NULL.
 * @user: The username for authentication to the database. Can be NULL.
 * @pwd: The password for authentication to the database. Can be NULL.
 * 
 * The function activates the correct CORBA server (defined with the
 * #gda_connection_set_provider() function. Then it
 * tries to open the database using the DSN or the default database
 * as a data source. If @user or @pwd is not NULL, it will overwrite the
 * appropriate entry in the DSN passed as @par2. Entries in the DSN have the form
 * <key> = <value> seperated from the database name . Currently the DSN is not
 * parsed.
 *
 * Returns: 0 on success, -1 on error
 */
gint
gda_connection_open (GdaConnection* cnc, const gchar* dsn, const gchar* user, const gchar* pwd)
{
	gchar*            db_to_use;
	gint              rc;
	CORBA_Environment ev;
	
	g_return_val_if_fail(IS_GDA_CONNECTION(cnc), -1);
	g_return_val_if_fail(cnc->connection == CORBA_OBJECT_NIL, -1);
	g_return_val_if_fail(cnc->is_open == 0, -1);
	
	if (!dsn)
		db_to_use = cnc->default_db;
	else
		db_to_use = (gchar *) dsn;
	if (!db_to_use) {
		GdaError* e = gda_error_new();
		
		e->description = g_strdup_printf(_("Database '%s' not found in system configuration"), dsn);
		e->source = g_strdup("[GDA Client Library]");
		e->nativeMsg = g_strdup(e->description);
		gda_connection_add_single_error(cnc, e);
		return -1;
	}

	cnc->database = g_strdup(db_to_use);
	cnc->user = g_strdup(user);
	cnc->passwd = g_strdup(pwd);

	/* start CORBA connection */
	if (get_corba_connection(cnc) < 0) {
		GdaError* e = gda_error_new();
		
		e->description = g_strdup(_("Could not open CORBA factory"));
		e->source = g_strdup("[GDA Client Library]");
		e->nativeMsg = g_strdup(e->description);
		gda_connection_add_single_error(cnc, e);
		return -1;
	}
	
	CORBA_exception_init(&ev);
	
	rc = GDA_Connection_open(cnc->connection, cnc->database, cnc->user, cnc->passwd, &ev);
	g_print("GDA_Connection_open returns %d\n", rc);
	if (gda_connection_corba_exception(cnc, &ev) < 0 || rc < 0) {
		CORBA_SystemException* sysexc = CORBA_exception_value(&ev);
		if (sysexc && sysexc->completed != CORBA_COMPLETED_NO) {
			GDA_Connection_close(cnc->connection, &ev);
			CORBA_Object_release(cnc->connection, &ev);
		}
		cnc->connection = CORBA_OBJECT_NIL;
		return -1;
	}

	cnc->is_open = 1;
#ifndef HAVE_GOBJECT /* FIXME */
	gtk_signal_emit(GTK_OBJECT(cnc), gda_connection_signals[CONNECTION_OPEN]);
#endif

	return 0;
}

/**
 * gda_connection_close:
 * @cnc: The connection object which should be closed
 *
 * Closes the connection object and all associated #GdaRecordset objects.
 * #GdaCommand objects are not closed, but can't be used anymore. Transactions
 * pending on this objects are aborted and an error is returned.
 *
 * Returns: 0 on success, -1 on error
 */
void
gda_connection_close (GdaConnection* cnc)
{
	CORBA_Environment     ev;
	
	g_return_if_fail(cnc != NULL);
	g_return_if_fail(gda_connection_is_open(cnc));
	g_return_if_fail(cnc->connection != NULL);
	
	CORBA_exception_init(&ev);
	GDA_Connection_close(cnc->connection, &ev);
	gda_connection_corba_exception(cnc, &ev);
	CORBA_Object_release(cnc->connection, &ev);
	cnc->is_open = 0;
	cnc->connection = CORBA_OBJECT_NIL;

#ifndef HAVE_GOBJECT /* FIXME */
	gtk_signal_emit(GTK_OBJECT(cnc), gda_connection_signals[CONNECTION_CLOSE]);
#endif
}


static GdaRecordset*
open_schema (GdaConnection* cnc,
             GDA_Connection_QType t,
             GDA_Connection_ConstraintSeq* constraints)
{
	CORBA_Environment ev;
	CORBA_Object      corba_rs;
	GdaRecordset*    rs;
	
	CORBA_exception_init(&ev);
	
	corba_rs = GDA_Connection_openSchema(cnc->connection, t, constraints, &ev);
	if (gda_connection_corba_exception(cnc, &ev) < 0
	    || CORBA_Object_is_nil(corba_rs, &ev))
		return 0;

	rs = GDA_RECORDSET(gda_recordset_new());
	rs->open = 1;
	rs->corba_rs = corba_rs;
	rs->field_attributes = GDA_Recordset_describe(corba_rs, &ev);
	if (gda_connection_corba_exception(cnc, &ev)
	    || CORBA_Object_is_nil(rs->field_attributes, &ev)) {
		g_print(" Cannot get recordset description\n");
	}
	return rs;
}

/**
 * gda_connection_open_schema_array:
 * @cnc: Connection object
 * @t: Query type
 * @elems: Constraint values 
 * 
 * Retrieves meta data about the data source to which the
 * connection object is connected.
 * The @elems is an array of Constraint_Element item. The last item
 * must have it's type element set to GDA_Connection_no_CONSTRAINT
 * If @elems is NULL, no constraints are used.
 *
 * Returns: The recordset which holds the results.
 */
GdaRecordset*
gda_connection_open_schema_array (GdaConnection* cnc,
                                  GDA_Connection_QType t,
                                  GdaConstraint_Element* elems)
{
	CORBA_Environment             ev;
	GDA_Connection_ConstraintSeq* constraints;
	gint                          count = 0;
	gint                          index;
	GdaConstraint_Element*       current;
	GdaRecordset*                rc;
	
	g_return_val_if_fail(IS_GDA_CONNECTION(cnc), NULL);
	g_return_val_if_fail(gda_connection_is_open(cnc), NULL);
	g_return_val_if_fail(cnc->connection != NULL, NULL);
	
	count = 0;
	current = elems;
	while (current) {
		if (current->type == GDA_Connection_no_CONSTRAINT)
			break;
		current++;
		count++;
	}
	constraints =  GDA_Connection_ConstraintSeq__alloc();
	constraints->_buffer = CORBA_sequence_GDA_Connection_Constraint_allocbuf(count);
	constraints->_length = count;
	
	index = 0;
	current = elems;
	while (count) {
		constraints->_buffer[index].ctype = current->type;
		constraints->_buffer[index].value = CORBA_string_dup(current->value);
		index++;
		count--;
		current++;
	}
	CORBA_exception_init(&ev);
	g_print ("client: gda_connection_open_schema: constraints._maximum = %d\n", constraints->_maximum);
	g_print ("                                    constraints._length  = %d\n", constraints->_length);

	rc = open_schema(cnc, t, constraints);
	CORBA_free(constraints);
	
	return (rc);
}

/**
 * gda_connection_open_schema:
 * @cnc: Connection object
 * @t: Query type
 * @Varargs: Constraint values 
 * 
 * Retrieves meta data about the data source to which the
 * connection object is connected.
 * The @constraints is a list of enum/string pairs.
 * The end of the list must be marked by the special value
 * GDA_Connection_no_CONSTRAINT
 *
 * Returns: The recordset which holds the results.
 */
GdaRecordset*
gda_connection_open_schema (GdaConnection* cnc, GDA_Connection_QType t, ...)
{
	va_list ap;
	CORBA_Environment ev;
	GDA_Connection_ConstraintSeq* constraints;
	GDA_Connection_ConstraintType constraint_type;
	GDA_Connection_Constraint*    c;
	GList*                        tmp_constraints = 0;
	GList*                        ptr;
	gint                          count = 0;
	gint                          index;
	GdaRecordset*                rc;
	
	g_return_val_if_fail(IS_GDA_CONNECTION(cnc), NULL);
	g_return_val_if_fail(gda_connection_is_open(cnc), NULL);
	g_return_val_if_fail(cnc->connection != NULL, NULL);
	
	va_start(ap, t);
	while (1) {
		gchar* constraint_value;
		
		constraint_type = va_arg(ap, int);
		if (constraint_type == GDA_Connection_no_CONSTRAINT)
			break;
		constraint_value = va_arg(ap, char*);
		g_print("gda_connection_open_schema: constraint value = '%s'\n", constraint_value);
		c = g_new0(GDA_Connection_Constraint, 1);
		c->ctype = constraint_type;
		c->value = CORBA_string_dup(constraint_value);
		tmp_constraints = g_list_append(tmp_constraints, c);
		count++;
	}
	
	constraints =  GDA_Connection_ConstraintSeq__alloc();
	constraints->_buffer = CORBA_sequence_GDA_Connection_Constraint_allocbuf(count);
	constraints->_length = count;
	
	ptr = tmp_constraints;
	index = 0;
	
	while (count) {
		memcpy(&constraints->_buffer[index], ptr->data, sizeof(GDA_Connection_Constraint));
		g_print("CORBA seq: constraint->value = '%s'\n", constraints->_buffer[index].value);
		index++;
		count--;
		c = ptr->data;
		g_free(c);
		ptr = g_list_next(ptr);
	}
	g_list_free(tmp_constraints);
	
	CORBA_exception_init(&ev);
	rc = open_schema(cnc, t, constraints);
	CORBA_free(constraints);
	
	return rc;
}

/**
 * gda_connection_modify_schema
 */
glong
gda_connection_modify_schema (GdaConnection *cnc, GDA_Connection_QType t, ...)
{
	g_return_val_if_fail(IS_GDA_CONNECTION(cnc), -1);
	g_return_val_if_fail(gda_connection_is_open(cnc), -1);
	g_return_val_if_fail(cnc->connection != NULL, -1);
}

/**
 * gda_connection_get_errors:
 * @cnc: the connection object
 *
 * Returns a list of all errors for this connection object. This
 * function also clears the error list. The errors are stored in LIFO
 * order, so the last error which happend is stored as the first
 * element in the list. Advancing the list means to get back in time and 
 * retrieve earlier errors.
 *
 * The farther away from the client the error happens, the later it
 * is in the queue. If an error happens on the client and then in the
 * CORBA layer and then on the server, you will get the server error
 * first.
 *
 * Returns: a #Glist holding the error objects.
 */
GList*
gda_connection_get_errors (GdaConnection* cnc)
{
	GList*             retval;
	GDA_ErrorSeq*      remote_errors;
	CORBA_Environment  ev;
	gint               idx;
	
	g_return_val_if_fail(IS_GDA_CONNECTION(cnc), 0);
	CORBA_exception_init(&ev);
	
	if (cnc->connection) {
		remote_errors = GDA_Connection__get_errors(cnc->connection, &ev);
		gda_connection_corba_exception(cnc, &ev);
		for (idx = 0; idx < remote_errors->_length; idx++) {
			GdaError* e;
			e = gda_error_new();
			e->description = g_strdup(remote_errors->_buffer[idx].description);
			e->number      = remote_errors->_buffer[idx].number;
			e->source      = g_strdup(remote_errors->_buffer[idx].source);
			e->helpurl = 0;
			e->sqlstate    = g_strdup(remote_errors->_buffer[idx].sqlstate);
			e->nativeMsg   = g_strdup(remote_errors->_buffer[idx].nativeMsg);
			gda_connection_add_single_error(cnc, e);
		}
		CORBA_free(remote_errors);
	}
	
	retval = cnc->errors_head;
	cnc->errors_head = 0;
	return retval;
}

/**
 * gda_connection_begin_transaction:
 * @cnc: the connection object
 *
 * Start a new transaction on @cnc
 *
 * Returns: 0 if everything is okay, -1 on error
 */
gint
gda_connection_begin_transaction (GdaConnection* cnc)
{
	CORBA_Environment ev;
	
	g_return_val_if_fail(IS_GDA_CONNECTION(cnc), -1);
	g_return_val_if_fail(gda_connection_is_open(cnc), -1);
	
	CORBA_exception_init(&ev);
	
	GDA_Connection_beginTransaction(cnc->connection, &ev);
	if (gda_connection_corba_exception(cnc, &ev) < 0)
		return -1;
	return 0;
}

/**
 * gda_connection_commit_transaction:
 * @cnc: the connection object
 *
 * Commit a pending transaction on @cnc
 *
 * Returns: 0 if everything is okay, -1 on error
 */
gint
gda_connection_commit_transaction (GdaConnection* cnc)
{
	CORBA_Environment ev;
	
	g_return_val_if_fail(IS_GDA_CONNECTION(cnc), -1);
	g_return_val_if_fail(gda_connection_is_open(cnc), -1);
	
	CORBA_exception_init(&ev);
	
	GDA_Connection_commitTransaction(cnc->connection, &ev);
	if (gda_connection_corba_exception(cnc, &ev) < 0)
		return -1;
	return 0;
}

/**
 * gda_connection_rollback_transaction:
 * @cnc: the connection object
 *
 * Abort and rollback a pending transaction on @cnc
 *
 * Returns: 0 if everything is okay, -1 on error
 */
gint
gda_connection_rollback_transaction (GdaConnection* cnc)
{
	CORBA_Environment ev;
	
	g_return_val_if_fail(IS_GDA_CONNECTION(cnc), -1);
	g_return_val_if_fail(gda_connection_is_open(cnc), -1);
	
	CORBA_exception_init(&ev);
	
	GDA_Connection_rollbackTransaction(cnc->connection, &ev);
	if (gda_connection_corba_exception(cnc, &ev) < 0)
		return -1;
	return 0;
}

/**
 * gda_connection_execute:
 * @cnc: the connection
 * @txt: the statement to execute
 * @reccount: a pointer to a gulong. The number of affected records is 
 * stored in the space where the pointer points to
 * @flags: falgs (currently unused
 *
 * Executes @txt. An internal GdaCommand object is used for
 * execution. This command object is destroyed automatically after the 
 * commadn has finished.
 *
 * Returns: a recordset on success or NULL on error.
 */
GdaRecordset*
gda_connection_execute (GdaConnection* cnc, gchar* txt, gulong* reccount, gulong flags)
{
	GdaCommand*   cmd;
	GdaRecordset* rs;
	
	g_return_val_if_fail(IS_GDA_CONNECTION(cnc), 0);
	g_return_val_if_fail(gda_connection_is_open(cnc), 0);
	g_return_val_if_fail(txt != 0, 0);
	
	cmd = gda_command_new();
	gda_command_set_connection(cmd, cnc);
	gda_command_set_text(cmd, txt);
	rs = gda_command_execute(cmd, reccount, flags);
	gda_command_free(cmd);
	return rs;
}

/**
 * gda_connection_start_logging:
 * @cnc: the connection
 * @filename: the name of the logfile
 *
 * This function causes the server to log the statements
 * it executes in @filename.
 *
 * Returns: -1 on error and 0 if everything is okay
 */
gint
gda_connection_start_logging (GdaConnection* cnc, gchar* filename)
{
	CORBA_Environment ev;
	gint              rc;
	
	g_return_val_if_fail(IS_GDA_CONNECTION(cnc), -1);
	g_return_val_if_fail(gda_connection_is_open(cnc), -1);
	g_return_val_if_fail(filename, -1);
	g_return_val_if_fail(cnc->connection != 0, -1);
	
	CORBA_exception_init(&ev);
	rc = GDA_Connection_startLogging(cnc->connection, filename, &ev);
	if (gda_connection_corba_exception(cnc, &ev) || rc < 0) {
		return -1;
	}
	return 0;
}

/**
 * gda_connection_stop_logging:
 * @cnc: the connection
 *
 * This function stops the database server logs
 *
 * Returns: -1 on error and 0 if everything is okay
 */
gint
gda_connection_stop_logging (GdaConnection* cnc)
{
	CORBA_Environment ev;
	gint              rc;
	
	g_return_val_if_fail(IS_GDA_CONNECTION(cnc), -1);
	g_return_val_if_fail(gda_connection_is_open(cnc), -1);
	g_return_val_if_fail(cnc->connection != 0, -1);
	
	CORBA_exception_init(&ev);
	rc = GDA_Connection_stopLogging(cnc->connection, &ev);
	if (gda_connection_corba_exception(cnc, &ev) || rc < 0) {
		return -1;
	}
	return 0;
}
   
gchar*
gda_connection_create_recordset (GdaConnection* cnc, GdaRecordset* rs)
{
	gchar*            retval;
	CORBA_Environment ev;
	
	g_return_val_if_fail(IS_GDA_CONNECTION(cnc), 0);
	g_return_val_if_fail(gda_connection_is_open(cnc), 0);
	g_return_val_if_fail(IS_GDA_RECORDSET(rs), 0);
	
	CORBA_exception_init(&ev);
	retval = GDA_Connection_createTable(cnc->connection,
	                                    rs->name,
	                                    rs->field_attributes,
	                                    &ev);
	if (gda_connection_corba_exception(cnc, &ev))
		return 0;
	return retval;
}

/**
 * gda_connection_add_single_error
 * @cnc: a connection
 * @error: error object
 */
void
gda_connection_add_single_error (GdaConnection* cnc, GdaError* error)
{
	GList* error_list = 0;
	
	g_return_if_fail(IS_GDA_CONNECTION(cnc));
	g_return_if_fail(error != 0);
	
	error_list = g_list_append(error_list, error);
#ifndef HAVE_GOBJECT /* FIXME */
	gtk_signal_emit(GTK_OBJECT(cnc),
	                gda_connection_signals[CONNECTION_ERROR], error_list);
#endif
}

void
gda_connection_add_errorlist (GdaConnection* cnc, GList* errors)
{
	g_return_if_fail(IS_GDA_CONNECTION(cnc));
	g_return_if_fail(errors != 0);

#ifndef HAVE_GOBJECT /* FIXME */
	gtk_signal_emit(GTK_OBJECT(cnc), gda_connection_signals[CONNECTION_ERROR], errors);
#endif
}

glong
gda_connection_get_flags (GdaConnection *cnc)
{
	CORBA_Environment ev;
	glong ret;
	
	g_return_val_if_fail(IS_GDA_CONNECTION(cnc), -1);
	
	CORBA_exception_init(&ev);
	ret = GDA_Connection__get_flags(cnc->connection, &ev);
	if (gda_connection_corba_exception(cnc, &ev) == 0)
		return (ret);
	return (-1);
}

void
gda_connection_set_flags (GdaConnection *cnc, glong flags)
{
	CORBA_Environment ev;
	
	g_return_if_fail(IS_GDA_CONNECTION(cnc));
	
	CORBA_exception_init(&ev);
	GDA_Connection__set_flags(cnc->connection, (CORBA_long) flags, &ev);
	gda_connection_corba_exception(cnc, &ev);
}

glong
gda_connection_get_cmd_timeout (GdaConnection *cnc)
{
	CORBA_Environment ev;
	glong ret;
	
	g_return_val_if_fail(IS_GDA_CONNECTION(cnc), -1);
	
	CORBA_exception_init(&ev);
	ret = GDA_Connection__get_cmdTimeout(cnc->connection, &ev);
	if (gda_connection_corba_exception(cnc, &ev) == 0)
		return (ret);
	return (-1);
}

void
gda_connection_set_cmd_timeout (GdaConnection *cnc, glong cmd_timeout)
{
	CORBA_Environment ev;
	
	g_return_if_fail(IS_GDA_CONNECTION(cnc));
	
	CORBA_exception_init(&ev);
	GDA_Connection__set_cmdTimeout(cnc->connection, (CORBA_long) cmd_timeout, &ev);
	gda_connection_corba_exception(cnc, &ev);
}

glong
gda_connection_get_connect_timeout (GdaConnection *cnc)
{
	CORBA_Environment ev;
	glong             ret;
	
	g_return_val_if_fail(IS_GDA_CONNECTION(cnc), -1);
	
	CORBA_exception_init(&ev);
	ret = GDA_Connection__get_connectTimeout(cnc->connection, &ev);
	if (gda_connection_corba_exception(cnc, &ev) == 0)
		return (ret);
	return (-1);
}

void
gda_connection_set_connect_timeout (GdaConnection *cnc, glong timeout)
{
	CORBA_Environment ev;
	
	g_return_if_fail(IS_GDA_CONNECTION(cnc));
	
	CORBA_exception_init(&ev);
	GDA_Connection__set_connectTimeout(cnc->connection, (CORBA_long) timeout, &ev);
	gda_connection_corba_exception(cnc, &ev);
}

GDA_CursorLocation
gda_connection_get_cursor_location (GdaConnection *cnc)
{
	CORBA_Environment ev;
	GDA_CursorLocation cursor;
	
	g_return_val_if_fail(IS_GDA_CONNECTION(cnc), -1);
	
	CORBA_exception_init(&ev);
	cursor = GDA_Connection__get_cursor(cnc->connection, &ev);
	if (gda_connection_corba_exception(cnc, &ev) == 0)
		return (cursor);
	return -1;
}

void
gda_connection_set_cursor_location (GdaConnection *cnc, GDA_CursorLocation cursor)
{
	CORBA_Environment ev;
	
	g_return_if_fail(IS_GDA_CONNECTION(cnc));
	
	CORBA_exception_init(&ev);
	GDA_Connection__set_cursor(cnc->connection, cursor, &ev);
	gda_connection_corba_exception(cnc, &ev);
}

/**
 * gda_connection_get_version
 * @cnc: connection
 *
 * Get the version number of the underlying provider
 */
gchar *
gda_connection_get_version (GdaConnection *cnc)
{
	CORBA_char*       version;
	CORBA_Environment ev;

	g_return_val_if_fail(IS_GDA_CONNECTION(cnc), 0);

	CORBA_exception_init(&ev);
	version = GDA_Connection__get_version(cnc->connection, &ev);
	if (gda_connection_corba_exception(cnc, &ev) == 0)
		return (gchar *) version;
	return NULL;
}

/**
 * gda_connection_sql2xml
 * @cnc: connection
 * @sql: SQL string
 *
 * Converts the given SQL command to a #GdaXmlQuery
 */
gchar *
gda_connection_sql2xml (GdaConnection *cnc, const gchar *sql)
{
	CORBA_char* xml;
	CORBA_Environment ev;
	
	g_return_val_if_fail(IS_GDA_CONNECTION(cnc), 0);
	
	CORBA_exception_init(&ev);
	xml = GDA_Connection_sql2xml(cnc->connection, sql, &ev);
	if (gda_connection_corba_exception(cnc, &ev) == 0)
		return (gchar *) xml;
	return NULL;
}

/**
 * gda_connection_xml2sql
 * @cnc: connection
 * @sql: XML string
 *
 * Converts the given #GdaXmlQuery to a SQL string for the underlying provider
 */
gchar *
gda_connection_xml2sql (GdaConnection *cnc, const gchar *xml)
{
	CORBA_char* sql;
	CORBA_Environment ev;
	
	g_return_val_if_fail(IS_GDA_CONNECTION(cnc), 0);
	
	CORBA_exception_init(&ev);
	sql = GDA_Connection_xml2sql(cnc->connection, xml, &ev);
	if (gda_connection_corba_exception(cnc, &ev) == 0)
		return (gchar *) sql;
	return NULL;
}
