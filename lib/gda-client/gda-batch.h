/* GDA client libary
 * Copyright (C) 1998,1999 Michael Lausch
 * Copyright (C) 2000 Rodrigo Moya
 *
 * This Library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 *  published by the Free Software Foundation; either version 2 of the
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

#ifndef __gda_batch_h__
#define __gda_batch_h__ 1

#include <glib.h>

#ifdef HAVE_GOBJECT
#  include <glib-object.h>
#else
#  include <gtk/gtkobject.h>
#endif

#include <orb/orbit.h>
#include <gda-connection.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The batch job object. Provides a way of managing a series
 * of commands as a simple object, allowing the execution
 * of what is known as "transactions".
 */

typedef struct _Gda_Batch      Gda_Batch;
typedef struct _Gda_BatchClass Gda_BatchClass;

#define GDA_TYPE_BATCH            (gda_batch_get_type())

#ifdef HAVE_GOBJECT
#  define GDA_BATCH(obj) \
            G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_BATCH, Gda_Batch)
#  define GDA_BATCH_CLASS(klass) \
            G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_BATCH, Gda_BatchClass)
#  define IS_GDA_BATCH(obj) \
            G_TYPE_CHECK_INSTANCE_TYPE (obj, GDA_TYPE_BATCH)
#  define IS_GDA_BATCH_CLASS(klass) \
            G_TYPE_CHECK_CLASS_TYPE (klass, GDA_TYPE_BATCH)
#else
#define GDA_BATCH(obj)            GTK_CHECK_CAST(obj, GDA_TYPE_BATCH, Gda_Batch)
#define GDA_BATCH_CLASS(klass)    GTK_CHECK_CLASS_CAST(klass, GDA_TYPE_BATCH, Gda_BatchClass)
#define IS_GDA_BATCH(obj)         GTK_CHECK_TYPE(obj, GDA_TYPE_BATCH)
#define IS_GDA_BATCH_CLASS(klass) (GTK_CHECK_CLASS_TYPE((klass), GDA_TYPE_BATCH))
#endif

struct _Gda_Batch
{
#ifdef HAVE_GOBJECT
  GObject         object;
#else
  GtkObject       object;
#endif
  Gda_Connection* cnc;
  gboolean        transaction_mode;
  gboolean        is_running;
  GList*          commands;
};

struct _Gda_BatchClass
{
#ifdef HAVE_GOBJECT
  GObjectClass   parent_class;
#else
  GtkObjectClass parent_class;
#endif
  void           (*begin_transaction)   (Gda_Batch *job);
  void           (*commit_transaction)  (Gda_Batch *job);
  void           (*rollback_transaction)(Gda_Batch *job);
  void           (*execute_command)     (Gda_Batch *job, const gchar *cmd);
};

#ifdef HAVE_GOBJECT
GType           gda_batch_get_type             (void);
#else
guint           gda_batch_get_type             (void);
#endif

Gda_Batch*      gda_batch_new                  (void);
void            gda_batch_free                 (Gda_Batch *job);

gboolean        gda_batch_load_file            (Gda_Batch *job, const gchar *filename, gboolean clean);
void            gda_batch_add_command          (Gda_Batch *job, const gchar *cmd);
void            gda_batch_clear                (Gda_Batch *job);

gboolean        gda_batch_start                (Gda_Batch *job);
void            gda_batch_stop                 (Gda_Batch *job);
gboolean        gda_batch_is_running           (Gda_Batch *job);

Gda_Connection* gda_batch_get_connection       (Gda_Batch *job);
void            gda_batch_set_connection       (Gda_Batch *job, Gda_Connection *cnc);
gboolean        gda_batch_get_transaction_mode (Gda_Batch *job);
void            gda_batch_set_transaction_mode (Gda_Batch *job, gboolean mode);

#ifdef __cplusplus
}
#endif

#endif
