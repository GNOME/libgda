/*
 * Copyright (C) 2014 - 2015 Vivien Malerba <malerba@gnome-db.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef __T_APP__
#define __T_APP__

#include <glib.h>
#include <gio/gio.h>
#include "t-connection.h"
#include "t-decl.h"
#include "base-tool.h"
#include "t-errors.h"
#include "web-server.h"
#ifdef HAVE_GTK_CLASSES
  #include <gtk/gtk.h>
#endif

#define T_APP_TYPE          (t_app_get_type())
#define T_APP(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, t_app_get_type(), TApp)
#define T_APP_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, t_app_get_type (), TAppClass)
#define IS_T_APP(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, t_app_get_type ())

typedef struct _TAppClass TAppClass;
typedef struct _TAppPrivate TAppPrivate;
typedef GSList *(*TAppInitFactories) (void);

/* struct for the object's data */
struct _TApp
{
#ifdef HAVE_GTK_CLASSES
	GtkApplication parent;
#else
	GApplication parent;
#endif
        TAppPrivate *priv;

	ToolCommandGroup *term_commands;
#ifdef HAVE_LIBSOUP
	ToolCommandGroup *web_commands;
#endif

};

/* struct for the object's class */
struct _TAppClass
{
#ifdef HAVE_GTK_CLASSES
	GtkApplicationClass parent_class;
#else
	GApplicationClass parent_class;
#endif

        /* signals */
        void    (*connection_added) (TApp *self, TConnection *tcnc);
        void    (*connection_removed) (TApp *self, TConnection *tcnc);
	void    (*quit_requested) (TApp *self);
};

typedef enum {
	T_APP_NO_FEATURE   = 0,
	T_APP_TERM_CONSOLE = 1 << 0,
	T_APP_BROWSER      = 1 << 1,
	T_APP_WEB_SERVER   = 1 << 2,
} TAppFeatures;

void           t_app_setup               (TAppFeatures features);
void           t_app_add_feature         (TAppFeatures feature);
void           t_app_remove_feature      (TAppFeatures feature);

#ifdef HAVE_GTK_CLASSES
GMenu         *t_app_get_app_menu        (void);
GMenu         *t_app_get_win_menu        (void);
#endif

void           t_app_cleanup             (void);
TApp          *t_app_get                 (void);
void           t_app_request_quit        (void);

void           t_app_lock                (void);
void           t_app_unlock              (void);

gboolean       t_app_open_connections    (gint argc, const gchar *argv[], GError **error);

GSList        *t_app_get_all_connections (void);
GdaDataModel  *t_app_get_all_connections_m (void);

void           t_app_add_tcnc            (TConnection *tcnc);
GValue        *t_app_get_parameter_value (const gchar *name);
TContext      *t_app_get_term_console    (void);
GdaSet        *t_app_get_options         (void);
gboolean       t_app_start_http_server   (gint port, const gchar *auth_token, GError **error);

void           t_app_store_data_model    (GdaDataModel *model, const gchar *name);
GdaDataModel  *t_app_fetch_data_model    (const gchar *name);

/* internal API */
void          _t_app_add_context         (TContext *console);
void          _t_app_remove_context      (TContext *console);

#endif
