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

#include "config.h"
#include "gda-server.h"

#ifdef ENABLE_NLS
#  include <libintl.h>
#  define _(String) gettext (String)
#  define N_(String) (String)
#else
/* Stubs that do something close enough.  */
#  define textdomain(String)
#  define gettext(String) (String)
#  define dgettext(Domain,Message) (Message)
#  define dcgettext(Domain,Message,Type) (Message)
#  define bindtextdomain(Domain,Directory)
#  define _(String) (String)
#  define N_(String) (String)
#endif

#include <liboaf/liboaf.h>

#ifdef HAVE_GOBJECT
static void gda_server_impl_finalize (GObject *object);
#else
static void gda_server_impl_destroy    (Gda_ServerImpl *server_impl);
#endif

static GList* server_list = NULL;

/*
 * Private functions
 */
#ifdef HAVE_GOBJECT
static void
gda_server_impl_class_init (Gda_ServerImplClass *klass, gpointer data)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = &gda_server_impl_finalize;
  klass->parent = g_type_class_peek_parent (klass);
}
#else
static void
gda_server_impl_class_init (Gda_ServerImplClass *klass)
{
  GtkObjectClass* object_class = (GtkObjectClass *) klass;

  object_class->destroy = gda_server_impl_destroy;
}
#endif

#ifdef HAVE_GOBJECT
static void
gda_server_impl_init (Gda_ServerImpl *server_impl, Gda_ServerImplClass *klass)
#else
static void
gda_server_impl_init (Gda_ServerImpl *server_impl)
#endif
{
  g_return_if_fail(IS_GDA_SERVER_IMPL(server_impl));

  server_impl->name = NULL;
  memset((void *) &server_impl->functions, 0, sizeof(Gda_ServerImplFunctions));
}

#ifdef HAVE_GOBJECT
static void
gda_server_impl_finalize (GObject *object)
{
  Gda_ServerImpl *server_impl = GDA_SERVER_IMPL (object);
  Gda_ServerImplClass *klass =
    G_TYPE_INSTANCE_GET_CLASS (object, GDA_SERVER_IMPL_CLASS,
                               Gda_ServerImplClass);

  if (server_impl->name)
    g_free ((gpointer) server_impl->name);
  klass->parent->finalize (object);
}
#else
static void
gda_server_impl_destroy (Gda_ServerImpl *server_impl)
{
  g_return_if_fail(IS_GDA_SERVER_IMPL(server_impl));

  if (server_impl->name) g_free((gpointer) server_impl->name);
}
#endif

#ifdef HAVE_GOBJECT
GType
gda_server_impl_get_type (void)
{
  static GType type = 0;
  if (!type)
    {
      GTypeInfo info =
      {
	sizeof (Gda_ServerImplClass),                /* class_size */
        NULL,                                        /* base_init */
        NULL,                                        /* base_finalize */
	(GClassInitFunc) gda_server_impl_class_init, /* class_init */
        NULL,                                        /* class_finalize */
        NULL,                                        /* class_data */
	sizeof (Gda_ServerImpl),                     /* instance_size */
        0,                                           /* n_preallocs */
	(GInstanceInitFunc) gda_server_impl_init,    /* instance_init */
        NULL,                                        /* value_table */
      };
      type = g_type_register_static (G_TYPE_OBJECT, "Gda_ServerImpl", &info);
    }
  return (type);
}
#else
GtkType
gda_server_impl_get_type (void)
{
  static guint type = 0;

  if (!type)
    {
      GtkTypeInfo info =
      {
	"Gda_ServerImpl",
	sizeof (Gda_ServerImpl),
	sizeof (Gda_ServerImplClass),
	(GtkClassInitFunc) gda_server_impl_class_init,
	(GtkObjectInitFunc) gda_server_impl_init,
	(GtkArgSetFunc)NULL,
        (GtkArgSetFunc)NULL,
      };
      type = gtk_type_unique(gtk_object_get_type(), &info);
    }
  return type;
}
#endif

/**
 * gda_server_impl_new
 * @name: name of the server
 * @functions: callback functions
 *
 * Create a new GDA provider implementation object. This function initializes
 * all the needed CORBA stuff, registers the server in the system's object
 * directory, and initializes all internal data. After successful return,
 * you've got a ready-to-go GDA provider. To start it, use
 * #gda_server_impl_start
 */
Gda_ServerImpl *
gda_server_impl_new (const gchar *name, Gda_ServerImplFunctions *functions)
{
  Gda_ServerImpl*        server_impl;
  PortableServer_POA     root_poa;
  CORBA_char*            objref;
  CORBA_ORB              orb;
  OAF_RegistrationResult result;

  g_return_val_if_fail(name != NULL, NULL);

  /* look for an already running instance */
  server_impl = gda_server_impl_find(name);
  if (server_impl) return server_impl;

  /* create provider instance */
#ifdef HAVE_GOBJECT
  server_impl = GDA_SERVER_IMPL (g_object_new (GDA_TYPE_SERVER_IMPL, NULL));
#else
  server_impl = GDA_SERVER_IMPL(gtk_type_new(gda_server_impl_get_type()));
#endif
  server_impl->name = g_strdup(name);
  g_set_prgname(server_impl->name);
  if (functions)
    {
      memcpy((void *) &server_impl->functions,
             (const void *) functions,
             sizeof(Gda_ServerImplFunctions));
    }
  else gda_log_message(_("Starting provider %s with no implementation functions"), name);

  server_impl->connections = NULL;
  server_impl->is_running = FALSE;

  /* create CORBA connection factory */
  orb = gda_corba_get_orb();
  server_impl->ev = g_new0(CORBA_Environment, 1);
  server_impl->root_poa = (PortableServer_POA)
    CORBA_ORB_resolve_initial_references(orb, "RootPOA", server_impl->ev);
  gda_server_impl_exception(server_impl->ev);

  server_impl->connection_factory_obj = impl_GDA_ConnectionFactory__create(server_impl->root_poa,
									   server_impl->ev);
  gda_server_impl_exception(server_impl->ev);
  objref = CORBA_ORB_object_to_string(orb,
				      server_impl->connection_factory_obj,
				      server_impl->ev);
  gda_server_impl_exception(server_impl->ev);

  /* register server with OAF */
  result = oaf_active_server_register(server_impl->name, server_impl->connection_factory_obj);
  if (result != OAF_REG_SUCCESS)
    {
      switch (result)
        {
        case OAF_REG_NOT_LISTED :
          gda_log_error(_("OAF doesn't know about our IID; indicates broken installation. Exiting..."));
          break;
        case OAF_REG_ALREADY_ACTIVE :
          gda_log_error(_("Another instance of this provider is already registered with OAF. Exiting..."));
          break;
        default :
          gda_log_error(_("Unknown error registering this provider with OAF. Exiting"));
        }
      exit(-1);
    }
  gda_log_message(_("Registered with ID = %s"), objref);
  
  server_list = g_list_append(server_list, (gpointer) server_impl);
  return server_impl;
}

/**
 * gda_server_impl_find
 * @id: object id
 *
 * Searches the list of loaded server implementations by object activation
 * identification
 */
Gda_ServerImpl *
gda_server_impl_find (const gchar *id)
{
  GList* node;

  g_return_val_if_fail(id != NULL, NULL);

  node = g_list_first(server_list);
  while (node)
    {
      Gda_ServerImpl* server_impl = GDA_SERVER_IMPL(node->data);
      if (server_impl && !strcmp(server_impl->name, id))
	return server_impl;
      node = g_list_next(node);
    }
  return NULL;
}

/**
 * gda_server_impl_start
 * @server_impl: server implementation
 *
 * Starts the given GDA provider
 */
void
gda_server_impl_start (Gda_ServerImpl *server_impl)
{
  PortableServer_POAManager pm;

  g_return_if_fail(server_impl != NULL);
  g_return_if_fail(server_impl->is_running == FALSE);

  pm = PortableServer_POA__get_the_POAManager(server_impl->root_poa, server_impl->ev);
  gda_server_impl_exception(server_impl->ev);
  PortableServer_POAManager_activate(pm, server_impl->ev);
  gda_server_impl_exception(server_impl->ev);

  server_impl->is_running = TRUE;
  CORBA_ORB_run(oaf_orb_get(), server_impl->ev);
}

/**
 * gda_server_impl_stop
 * @server_impl: server implementation
 *
 * Stops the given server implementation
 */
void
gda_server_impl_stop (Gda_ServerImpl *server_impl)
{
  g_return_if_fail(IS_GDA_SERVER_IMPL(server_impl));
  g_return_if_fail(server_impl->is_running);

  CORBA_ORB_shutdown(oaf_orb_get(), TRUE, server_impl->ev);
  oaf_active_server_unregister("IID", server_impl->connection_factory_obj);
  server_impl->is_running = FALSE;
}

/*
 * Convenience functions
 */
gint
gda_server_impl_exception (CORBA_Environment *ev)
{
  g_return_val_if_fail(ev != NULL, -1);

  switch (ev->_major)
    {
    case CORBA_SYSTEM_EXCEPTION :
      gda_log_error(_("CORBA system exception %s"), CORBA_exception_id(ev));
      return -1;
    case CORBA_USER_EXCEPTION :
      gda_log_error(_("CORBA user exception: %s"), CORBA_exception_id( ev ));
      return -1;
    default:
      break;
    }
  return 0;
}

GDA_Error *
gda_server_impl_make_error_buffer (Gda_ServerConnection *cnc)
{
  gint       idx;
  GList*     ptr;
  GDA_Error* rc;

  g_return_val_if_fail(cnc != NULL, CORBA_OBJECT_NIL);

  rc  = CORBA_sequence_GDA_Error_allocbuf(g_list_length(cnc->errors));
  idx = 0;
  ptr = cnc->errors;

  while (ptr)
    {
      Gda_ServerError* e = (Gda_ServerError *) ptr->data;

      rc[idx].description = CORBA_string_dup(e->description);
      rc[idx].number = e->number;
      rc[idx].source = CORBA_string_dup(e->source);
#if 0
      rc->_buffer[idx].helpfile = CORBA_string_dup(e->helpfile);
      rc->_buffer[idx].helpctxt = CORBA_string_dup(e->helpctxt);
#endif
      rc[idx].sqlstate = CORBA_string_dup(e->sqlstate);
      rc[idx].nativeMsg   = CORBA_string_dup(e->native);
      gda_server_error_free(e);
      ptr = g_list_next(ptr);
      idx++;
    }
  g_list_free(cnc->errors);
  cnc->errors = NULL;
  return rc;
}


