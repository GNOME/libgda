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
#include <gda-common-defs.h>
#include <gda-connection.h>

G_BEGIN_DECLS

/* The batch job object. Provides a way of managing a series
 * of commands as a simple object, allowing the execution
 * of what is known as "transactions".
 */

typedef struct _GdaBatch GdaBatch;
typedef struct _GdaBatchClass GdaBatchClass;

#define GDA_TYPE_BATCH            (gda_batch_get_type())

#ifdef HAVE_GOBJECT
#  define GDA_BATCH(obj) \
            G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_BATCH, GdaBatch)
#  define GDA_BATCH_CLASS(klass) \
            G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_BATCH, GdaBatchClass)
#  define GDA_IS_BATCH(obj) \
            G_TYPE_CHECK_INSTANCE_TYPE (obj, GDA_TYPE_BATCH)
#  define GDA_IS_BATCH_CLASS(klass) \
            G_TYPE_CHECK_CLASS_TYPE (klass, GDA_TYPE_BATCH)
#else
#define GDA_BATCH(obj)            GTK_CHECK_CAST(obj, GDA_TYPE_BATCH, GdaBatch)
#define GDA_BATCH_CLASS(klass)    GTK_CHECK_CLASS_CAST(klass, GDA_TYPE_BATCH, GdaBatchClass)
#define GDA_IS_BATCH(obj)         GTK_CHECK_TYPE(obj, GDA_TYPE_BATCH)
#define GDA_IS_BATCH_CLASS(klass) (GTK_CHECK_CLASS_TYPE((klass), GDA_TYPE_BATCH))
#endif

struct _GdaBatch {
#ifdef HAVE_GOBJECT
	GObject object;
#else
	GtkObject object;
#endif
	GdaConnection *cnc;
	gboolean transaction_mode;
	gboolean is_running;
	GList *commands;
};

struct _GdaBatchClass {
#ifdef HAVE_GOBJECT
	GObjectClass parent_class;
#else
	GtkObjectClass parent_class;
#endif
	void (*begin_transaction) (GdaBatch * job);
	void (*commit_transaction) (GdaBatch * job);
	void (*rollback_transaction) (GdaBatch * job);
	void (*execute_command) (GdaBatch * job, const gchar * cmd);
};

#ifdef HAVE_GOBJECT
GType gda_batch_get_type (void);
#else
guint gda_batch_get_type (void);
#endif

GdaBatch *gda_batch_new (void);
void gda_batch_free (GdaBatch * job);

gboolean gda_batch_load_file (GdaBatch * job, const gchar * filename,
			      gboolean clean);
void gda_batch_add_command (GdaBatch * job, const gchar * cmd);
void gda_batch_clear (GdaBatch * job);

gboolean gda_batch_start (GdaBatch * job);
void gda_batch_stop (GdaBatch * job);
gboolean gda_batch_is_running (GdaBatch * job);

GdaConnection *gda_batch_get_connection (GdaBatch * job);
void gda_batch_set_connection (GdaBatch * job, GdaConnection * cnc);
gboolean gda_batch_get_transaction_mode (GdaBatch * job);
void gda_batch_set_transaction_mode (GdaBatch * job, gboolean mode);

G_END_DECLS

#endif
