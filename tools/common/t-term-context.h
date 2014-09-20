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

#ifndef __T_TERM_CONTEXT__
#define __T_TERM_CONTEXT__

#include <glib.h>
#include "t-context.h"

/*
 * Object representing a context using a connection (AKA a console)
 */
#define T_TYPE_TERM_CONTEXT          (t_term_context_get_type())
#define T_TERM_CONTEXT(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, t_term_context_get_type(), TTermContext)
#define T_TERM_CONTEXT_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, t_term_context_get_type (), TTermContextClass)
#define T_IS_TERM_CONTEXT(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, t_term_context_get_type ())

typedef struct _TTermContextClass TTermContextClass;
typedef struct _TTermContextPrivate TTermContextPrivate;

/* struct for the object's data */
typedef struct
{
        TContext             object;
        TTermContextPrivate *priv;
} TTermContext;

/* struct for the object's class */
struct _TTermContextClass
{
        TContextClass        parent_class;
};

GType              t_term_context_get_type (void);

TTermContext      *t_term_context_new (const gchar *id);

void               t_term_context_set_interactive (TTermContext *term_console, gboolean interactive);
gboolean           t_term_context_set_input_file (TTermContext *term_console, const gchar *filename, GError **error);
void               t_term_context_set_input_stream (TTermContext *term_console, FILE *stream);
FILE              *t_term_context_get_input_stream (TTermContext *term_console);
void               t_term_context_treat_single_line (TTermContext *term_console, const gchar *cmde);

#endif
