/* GDA library
 * Copyright (C) 2009 The GNOME Foundation.
 *
 * AUTHORS:
 *      Vivien Malerba <malerba@gnome-db.org>
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

#ifndef __GDA_THREAD_WRAPPER_H__
#define __GDA_THREAD_WRAPPER_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define GDA_TYPE_THREAD_WRAPPER            (gda_thread_wrapper_get_type())
#define GDA_THREAD_WRAPPER(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_THREAD_WRAPPER, GdaThreadWrapper))
#define GDA_THREAD_WRAPPER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_THREAD_WRAPPER, GdaThreadWrapperClass))
#define GDA_IS_THREAD_WRAPPER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE(obj, GDA_TYPE_THREAD_WRAPPER))
#define GDA_IS_THREAD_WRAPPER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GDA_TYPE_THREAD_WRAPPER))
#define GDA_THREAD_WRAPPER_GET_CLASS(o)    (G_TYPE_INSTANCE_GET_CLASS ((o), GDA_TYPE_THREAD_WRAPPER, GdaThreadWrapperClass))

typedef struct _GdaThreadWrapper GdaThreadWrapper;
typedef struct _GdaThreadWrapperClass GdaThreadWrapperClass;
typedef struct _GdaThreadWrapperPrivate GdaThreadWrapperPrivate;

/* error reporting */
extern GQuark gda_thread_wrapper_error_quark (void);
#define GDA_THREAD_WRAPPER_ERROR gda_thread_wrapper_error_quark ()

typedef enum {
	GDA_THREAD_WRAPPER_UNKNOWN_ERROR
} GdaThreadWrapperError;

struct _GdaThreadWrapper {
	GObject            object;
	GdaThreadWrapperPrivate *priv;
};

struct _GdaThreadWrapperClass {
	GObjectClass       object_class;

	/* Padding for future expansion */
	void (*_gda_reserved1) (void);
	void (*_gda_reserved2) (void);
	void (*_gda_reserved3) (void);
	void (*_gda_reserved4) (void);
};

typedef gpointer (*GdaThreadWrapperFunc) (gpointer arg, GError **error);
typedef void (*GdaThreadWrapperVoidFunc) (gpointer arg, GError **error);
typedef void (*GdaThreadWrapperCallback) (GdaThreadWrapper *wrapper, gpointer instance, const gchar *signame,
					  gint n_param_values, const GValue *param_values, gpointer gda_reserved,
					  gpointer data);

GType                  gda_thread_wrapper_get_type          (void) G_GNUC_CONST;
GdaThreadWrapper      *gda_thread_wrapper_new               (void);

guint                  gda_thread_wrapper_execute           (GdaThreadWrapper *wrapper, GdaThreadWrapperFunc func,
							     gpointer arg, GDestroyNotify arg_destroy_func, GError **error);
guint                  gda_thread_wrapper_execute_void      (GdaThreadWrapper *wrapper, GdaThreadWrapperVoidFunc func,
							     gpointer arg, GDestroyNotify arg_destroy_func, GError **error);
gboolean               gda_thread_wrapper_cancel            (GdaThreadWrapper *wrapper, guint id);
void                   gda_thread_wrapper_iterate           (GdaThreadWrapper *wrapper, gboolean may_block);
gpointer               gda_thread_wrapper_fetch_result      (GdaThreadWrapper *wrapper, gboolean may_lock, 
							     guint exp_id, GError **error);

gint                   gda_thread_wrapper_get_waiting_size (GdaThreadWrapper *wrapper);

gulong                 gda_thread_wrapper_connect_raw       (GdaThreadWrapper *wrapper,
							     gpointer instance,
							     const gchar *sig_name, gboolean private,
							     GdaThreadWrapperCallback callback,
							     gpointer data);
void                   gda_thread_wrapper_disconnect       (GdaThreadWrapper *wrapper, gulong id);
void                   gda_thread_wrapper_steal_signal     (GdaThreadWrapper *wrapper, gulong id);
G_END_DECLS

#endif
