/* GDA client library
 * Copyright (C) 1998,1999 Michael Lausch
 * Copyright (C) 1999 Rodrigo Moya
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
#include "gda-error.h"
#include "GDA.h"

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

enum
{
  /* add signals here */
  GDA_ERROR_LAST_SIGNAL
};

static gint gda_error_signals[GDA_ERROR_LAST_SIGNAL] = { 0, };

#ifdef HAVE_GOBJECT
static void gda_error_class_init (Gda_ErrorClass *klass, gpointer data);
static void gda_error_init       (Gda_Error *error, Gda_ErrorClass *klass);
#else
static void gda_error_class_init (Gda_ErrorClass *klass);
static void gda_error_init       (Gda_Error *error);
#endif

#ifdef HAVE_GOBJECT
GType
gda_error_get_type (void)
{
  static GType type = 0;

  if (!type)
    {
      GTypeInfo info =
      {
        sizeof (Gda_ErrorClass),               /* class_size */
	NULL,                                  /* base_init */
	NULL,                                  /* base_finalize */
        (GClassInitFunc) gda_error_class_init, /* class_init */
	NULL,                                  /* class_finalize */
	NULL,                                  /* class_data */
        sizeof (Gda_Error),                    /* instance_size */
	0,                                     /* n_preallocs */
        (GInstanceInitFunc) gda_error_init,    /* instance_init */
	NULL,                                  /* value_table */
      };
      type = g_type_register_static (G_TYPE_OBJECT, "Gda_Error", &info, 0);
    }
  return type;
}
#else
guint
gda_error_get_type (void)
{
  static guint gda_error_type = 0;

  if (!gda_error_type)
    {
      GtkTypeInfo gda_error_info =
      {
        "GdaError",
        sizeof (Gda_Error),
        sizeof (Gda_ErrorClass),
        (GtkClassInitFunc) gda_error_class_init,
        (GtkObjectInitFunc) gda_error_init,
        (GtkArgSetFunc) NULL,
        (GtkArgSetFunc) NULL,
      };
      gda_error_type = gtk_type_unique(gtk_object_get_type(), &gda_error_info);
    }
  return gda_error_type;
}
#endif

#ifdef HAVE_GOBJECT
static void
gda_error_class_init (Gda_ErrorClass *klass, gpointer data)
{
}
#else
static void
gda_error_class_init (Gda_ErrorClass* klass)
{
  GtkObjectClass*   object_class;

  object_class = (GtkObjectClass*) klass;
}
#endif

static void
#ifdef HAVE_GOBJECT
gda_error_init (Gda_Error *error, Gda_ErrorClass *klass)
#else
gda_error_init (Gda_Error* error)
#endif
{
  g_return_if_fail(IS_GDA_ERROR(error));
  
  error->description = 0;
  error->number = 0;
  error->source = 0;
  error->helpurl = 0;
  error->sqlstate = 0;
  error->nativeMsg = 0;
  error->realcommand = 0;
}

/*
 * gda_error_new:
 * Creates a new unitialized error object.
 *
 * Returns: the error object.
 */
Gda_Error *
gda_error_new (void)
{
  Gda_Error* error;

#ifdef HAVE_GOBJECT
  error = GDA_ERROR (g_object_new (GDA_TYPE_ERROR, NULL));
#else
  error = GDA_ERROR(gtk_type_new(gda_error_get_type()));
#endif
  return error;
}

GList*
gda_errors_from_exception (CORBA_Environment* ev)
{
  GList*     all_errors = 0;
  Gda_Error* error;

  g_return_val_if_fail(ev != 0, 0);
  
  switch(ev->_major)
    {
    case CORBA_NO_EXCEPTION:
      return 0;
    case CORBA_SYSTEM_EXCEPTION:
      {
        CORBA_SystemException* sysexc;
	
        sysexc = CORBA_exception_value(ev);
        error = gda_error_new();
        error->source = g_strdup("[CORBA System Exception]");
        switch(sysexc->minor)
          {
          case ex_CORBA_COMM_FAILURE:
            error->description = g_strdup_printf(_("%s: The server didn't respond."),
                                                 CORBA_exception_id(ev));
            break;
          default:
            error->description = g_strdup(_("%s: An Error occured in the CORBA system."));
            break;
          }
        all_errors = g_list_append(all_errors, error);
      }
    case CORBA_USER_EXCEPTION:
      {
        if (strcmp(CORBA_exception_id(ev), ex_GDA_NotSupported) == 0)
          {
            GDA_NotSupported* not_supported_error;
	    
            not_supported_error = (GDA_NotSupported*)ev->_params;
            error = gda_error_new();
            error->source = g_strdup("[CORBA User Exception]");
            error->description = g_strdup(not_supported_error->errormsg);
            all_errors = g_list_append(all_errors, error);
          }
        else if (strcmp(CORBA_exception_id(ev), ex_GDA_DriverError) == 0)
          {
            GDA_ErrorSeq  error_sequence = ((GDA_DriverError*)ev->_params)->errors;
            gint          idx;
	    
            for (idx = 0; idx < error_sequence._length; idx++)
              {
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
                all_errors = g_list_append(all_errors, error);
              }
          }
      }
      break;
    default:
      g_error("Unknown CORBA exception for connection");
    }
  return all_errors;
}

/**
 * gda_error_free:
 * @error: the error object.
 *
 * Frees the memory allocated by the error object.
 *
 */
void
gda_error_free (Gda_Error* error)
{
  g_return_if_fail(IS_GDA_ERROR(error));
  
  if (error->description)
    g_free(error->description);

  if (error->source)
    g_free(error->source);

  if (error->helpurl)
    g_free(error->helpurl);

  if (error->sqlstate)
    g_free(error->sqlstate);

  if (error->nativeMsg)
    g_free(error->nativeMsg);

#ifdef HAVE_GOBJECT
  g_object_unref (G_OBJECT (error));
#else
  gtk_object_destroy (GTK_OBJECT (error));
#endif
}

/**
 * gda_error_list_free:
 * @errors: a glist holding error objects.
 *
 * Frees all error objects in the list and the list itself.
 * After this function has been called, the @errors parameter doesn't point
 * to valid storage any more.
 */
void
gda_error_list_free (GList* errors)
{
  GList* ptr = errors;

  g_return_if_fail(errors != 0);
  
  while (ptr)
    {
      Gda_Error* error = ptr->data;
      gda_error_free(error);
      ptr = g_list_next(ptr);
    }
  g_list_free(errors);
}

 
const gchar*
gda_error_description (Gda_Error* error)
{
  g_return_val_if_fail(IS_GDA_ERROR(error), 0);
  return error->description;
}

const glong
gda_error_number (Gda_Error* error)
{
  g_return_val_if_fail(IS_GDA_ERROR(error), 0);
  return error->number;
}

const gchar*
gda_error_source (Gda_Error* error)
{
  g_return_val_if_fail(IS_GDA_ERROR(error), 0);
  return error->source;
}

const gchar*
gda_error_helpurl (Gda_Error* error)
{
  g_return_val_if_fail(IS_GDA_ERROR(error), 0);
  return error->helpurl;
}

const gchar*
gda_error_sqlstate (Gda_Error* error)
{
  g_return_val_if_fail(IS_GDA_ERROR(error), 0);
  return error->sqlstate;
}

const gchar*
gda_error_nativeMsg (Gda_Error* error)
{
  g_return_val_if_fail(IS_GDA_ERROR(error), 0);
  return error->nativeMsg;
}

const gchar*
gda_error_realcommand (Gda_Error* error)
{
  g_return_val_if_fail(IS_GDA_ERROR(error), 0);
  return error->realcommand;
}
