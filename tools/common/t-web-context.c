/*
 * Copyright (C) 2014 Vivien Malerba <malerba@gnome-db.org>
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

#include "t-web-context.h"
#include "t-utils.h"
#include "t-app.h"
#include <glib/gi18n-lib.h>
#include <sql-parser/gda-sql-parser.h>
#include <glib/gstdio.h>
#include <errno.h>

/*
 * Main static functions
 */
static void t_web_context_class_init (TWebContextClass *klass);
static void t_web_context_init (TWebContext *bcore);
static void t_web_context_dispose (GObject *object);

/* get a pointer to the parents to be able to call their destructor */
static GObjectClass  *parent_class = NULL;

struct _TWebContextPrivate {
	gint dummy;
};

GType
t_web_context_get_type (void)
{
        static GType type = 0;

        if (G_UNLIKELY (type == 0)) {
                static GMutex registering;
                static const GTypeInfo info = {
                        sizeof (TWebContextClass),
                        (GBaseInitFunc) NULL,
                        (GBaseFinalizeFunc) NULL,
                        (GClassInitFunc) t_web_context_class_init,
                        NULL,
                        NULL,
                        sizeof (TWebContext),
                        0,
                        (GInstanceInitFunc) t_web_context_init,
                        0
                };


                g_mutex_lock (&registering);
                if (type == 0)
                        type = g_type_register_static (T_TYPE_CONTEXT, "TWebContext", &info, 0);
                g_mutex_unlock (&registering);
        }
        return type;
}

static void
t_web_context_class_init (TWebContextClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        parent_class = g_type_class_peek_parent (klass);

        object_class->dispose = t_web_context_dispose;
}

static void
t_web_context_init (TWebContext *self)
{
        self->priv = g_new0 (TWebContextPrivate, 1);
}

static void
t_web_context_dispose (GObject *object)
{
	TWebContext *console = T_WEB_CONTEXT (object);

	if (console->priv) {
		g_free (console->priv);
		console->priv = NULL;
	}

	/* parent class */
        parent_class->dispose (object);
}



/**
 * t_web_context_new:
 *
 * Returns: (transfer none): a new #TWebContext
 */
TWebContext *
t_web_context_new (const gchar *id)
{
        TWebContext *console = NULL;
#ifdef HAVE_LIBSOUP
	TApp *main_data;
	main_data = t_app_get ();

	console = g_object_new (T_TYPE_WEB_CONTEXT, "id", id, NULL);

	t_context_set_command_group (T_CONTEXT (console), main_data->web_commands);
#endif
        return console;
}
