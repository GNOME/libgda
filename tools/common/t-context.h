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

#ifndef __T_CONTEXT__
#define __T_CONTEXT__

#include <glib.h>
#include "t-connection.h"
#include "base/base-tool.h"
#include "t-errors.h"

/*
 * Object representing a context using a connection (AKA a console)
 */
#define T_TYPE_CONTEXT          (t_context_get_type())
#define T_CONTEXT(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, t_context_get_type(), TContext)
#define T_CONTEXT_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, t_context_get_type (), TContextClass)
#define T_IS_CONTEXT(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, t_context_get_type ())

typedef struct _TContextClass TContextClass;
typedef struct _TContextPrivate TContextPrivate;

/* struct for the object's data */
struct _TContext
{
        GObject          object;
        TContextPrivate *priv;
};

/* struct for the object's class */
struct _TContextClass
{
        GObjectClass     parent_class;
	void           (*run) (TContext *self);
};

GType              t_context_get_type (void);

GThread           *t_context_run (TContext *console);

ToolCommandResult *t_context_command_execute (TContext *console, const gchar *command,
					      GdaStatementModelUsage usage, GError **error);

const gchar       *t_context_get_id (TContext *console);
void               t_context_set_command_group (TContext *console, ToolCommandGroup *group);
ToolCommandGroup  *t_context_get_command_group (TContext *console);

void               t_context_set_output_format (TContext *console, ToolOutputFormat format);
ToolOutputFormat   t_context_get_output_format (TContext *console);

gboolean           t_context_set_output_file (TContext *console, const gchar *file, GError **error);
FILE              *t_context_get_output_stream (TContext *console, gboolean *out_is_pipe);

void               t_context_set_connection (TContext *console, TConnection *tcnc);
TConnection       *t_context_get_connection (TContext *console);

GDateTime         *t_context_get_last_time_used (TContext *console);

#endif
