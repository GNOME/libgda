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

#ifndef __gda_command_h__
#define __gda_command_h__ 1

#include <glib.h>

#ifdef HAVE_GOBJECT
#  include <glib-object.h>
#else
#  include <gtk/gtk.h>
#endif

#include <orb/orbit.h>
#include <GDA.h>

#if defined(__cplusplus)
extern "C" {
#endif

/* The command object. Holds and executes an SQL query or other,
 * datasource specific operations.
 */

typedef struct _Gda_Command      Gda_Command;
typedef struct _Gda_CommandClass Gda_CommandClass;

#include <gda-recordset.h>   /* these two need the definitions above */
#include <gda-connection.h>

#define GDA_TYPE_COMMAND            (gda_command_get_type())

#ifdef HAVE_GOBJECT
#  define GDA_COMMAND(obj) \
            G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_COMMAND, Gda_Command)
#  define GDA_COMMAND_CLASS(klass) \
            G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_COMMAND, Gda_CommandClass)
#  define IS_GDA_COMMAND(obj) \
            G_TYPE_CHECK_INSTANCE_TYPE (obj, GDA_TYPE_COMMAND)
#  define IS_GDA_COMMAND_CLASS(klass) \
            G_TYPE_CHECK_CLASS_TYPE (klass, GDA_TYPE_COMMAND)
#else
#  define GDA_COMMAND(obj)            GTK_CHECK_CAST(obj, GDA_TYPE_COMMAND, Gda_Command)
#  define GDA_COMMAND_CLASS(klass)    GTK_CHECK_CLASS_CAST(klass, GDA_TYPE_COMMAND, Gda_CommandClass)
#  define IS_GDA_COMMAND(obj)         GTK_CHECK_TYPE(obj, GDA_TYPE_COMMAND)
#  define IS_GDA_COMMAND_CLASS(klass) (GTK_CHECK_CLASS_TYPE((klass), GDA_TYPE_COMMAND))
#endif

struct _Gda_Command
{
#ifdef HAVE_GOBJECT
  GObject         object;
#else
  GtkObject       object;
#endif
  CORBA_Object    command;
  CORBA_ORB       orb;
  Gda_Connection* connection;
  gchar*          text;
  glong           type;
  GList*          parameters;
  gboolean        text_pending;
  gboolean        type_pending;
};

struct _Gda_CommandClass
{
#ifdef HAVE_GOBJECT
  GObjectClass   parent_class;
#else
  GtkObjectClass parent_class;
#endif
};

#ifdef HAVE_GOBJECT
GType           gda_command_get_type         (void);
#else
guint           gda_command_get_type         (void);
#endif

Gda_Command*    gda_command_new              (void);
void            gda_command_free             (Gda_Command* cmd);
Gda_Connection* gda_command_get_connection   (Gda_Command* cmd);
gint            gda_command_set_connection   (Gda_Command* cmd, Gda_Connection* cnc);
gchar*          gda_command_get_text         (Gda_Command* cmd);
void            gda_command_set_text         (Gda_Command* cmd, gchar* text);
gulong          gda_command_get_cmd_type     (Gda_Command* cmd);
void            gda_command_set_cmd_type     (Gda_Command* cmd, gulong flags);
Gda_Recordset*  gda_command_execute          (Gda_Command* cmd, gulong* reccount, gulong flags);
void            gda_command_create_parameter (Gda_Command* cmd,
                                              gchar* name,
                                              GDA_ParameterDirection inout,
                                              GDA_Value* value);
glong           gda_command_get_timeout      (Gda_Command* cmd);
void            gda_command_set_timeout      (Gda_Command* cmd, glong timeout);

#if defined(__cplusplus)
}
#endif

#endif
